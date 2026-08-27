# RapidJSON's own unit tests, run against rapidyyjson.
#
# RapidJSON and googletest are fetched at configure time (both pinned), the test
# sources are put through port.py, and the result is built and registered as the
# `rapidjson-suite` ctest test. See README.md in this directory for what the
# suite covers and which tests are excluded.

include(FetchContent)

set(RAPIDYYJSON_RAPIDJSON_TAG "v1.1.0" CACHE STRING
    "RapidJSON tag whose unit tests are run. This is the release whose API rapidyyjson reproduces.")
set(RAPIDYYJSON_GOOGLETEST_TAG "v1.14.0" CACHE STRING
    "googletest tag used to build RapidJSON's unit tests.")

FetchContent_Declare(rapidjson_upstream
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG ${RAPIDYYJSON_RAPIDJSON_TAG}
    GIT_SHALLOW TRUE
    # RapidJSON's own CMakeLists builds its tests and examples; we only want the sources.
    SOURCE_SUBDIR nonexistent-to-skip-configure)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG ${RAPIDYYJSON_GOOGLETEST_TAG}
    GIT_SHALLOW TRUE)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(rapidjson_upstream googletest)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(ported_dir "${CMAKE_CURRENT_BINARY_DIR}/ported")
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/port.py"
            "${rapidjson_upstream_SOURCE_DIR}" "${ported_dir}"
    RESULT_VARIABLE port_result
    OUTPUT_VARIABLE port_output)
if(NOT port_result EQUAL 0)
    message(FATAL_ERROR "port.py failed:\n${port_output}")
endif()
message(STATUS "rapidjson-suite: ${port_output}")

file(GLOB ported_sources "${ported_dir}/*.cpp")

add_executable(rapidjson-suite ${ported_sources})
target_include_directories(rapidjson-suite PRIVATE "${ported_dir}")
target_link_libraries(rapidjson-suite PRIVATE rapidyyjson::rapidyyjson gtest)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # RapidJSON's tests are from 2016 and trip newer warnings that say nothing
    # about rapidyyjson; they are not ours to fix.
    # -Wexceptions fires because the suite redefines RAPIDYYJSON_ASSERT to throw,
    # which makes noexcept functions look throwing. That is the harness, not the library.
    target_compile_options(rapidjson-suite PRIVATE
        -Wno-deprecated-declarations -Wno-unused-parameter -Wno-exceptions)
endif()

# Tests whose failure is a documented divergence rather than a defect. Each entry
# is justified in README.md next to the divergence it belongs to. Keeping them as
# an explicit exclusion list -- rather than not running them -- means any *other*
# regression fails the build.
set(RAPIDYYJSON_KNOWN_DIVERGENCES
    # The specific ParseErrorCode is derived from yyjson's error, so malformed
    # input can report a neighbouring code (divergence 1).
    Reader.ParseValue_Error
    Reader.ParseNumber_Error
    Reader.ParseString_Error
    Reader.ParseArray_Error
    Reader.ParseObject_Error
    Reader.ParseDocument_Error
    Reader.EmptyExceptForCommaErrors
    Reader.EmptyExceptForCommaErrorsIterative
    Reader.MultipleTrailingCommaErrors
    Reader.MultipleTrailingCommaErrorsIterative
    Reader.EofAfterOneLineComment
    Reader.UnrecognizedComment
    Reader.IterativeParsing_ErrorHandling
    # A handler cannot stop a parse of input that never scanned (divergence 7).
    Reader.ParseTerminationByHandler
    Reader.TrailingCommaHandlerTermination
    Reader.TrailingCommaHandlerTerminationIterative
    Reader.IterativeParsing_ShortCircuit
    # yyjson accepts NaN/Infinity spellings RapidJSON rejects (divergence 8).
    Reader.ParseNanAndInfinity
    # StringRef(0, 0) is tolerated, following RapidJSON master (divergence 9).
    Value.SetStringNullException
)
list(JOIN RAPIDYYJSON_KNOWN_DIVERGENCES ":" excluded)

# The suite reads fixtures (jsonchecker, encodings) by paths relative to bin/.
add_test(NAME rapidjson-suite
         COMMAND rapidjson-suite --gtest_filter=-${excluded}
         WORKING_DIRECTORY "${rapidjson_upstream_SOURCE_DIR}/bin")

# The same binary with no filter, for looking at the divergences on purpose.
# Not registered with ctest: it is expected to fail.
add_custom_target(rapidjson-suite-full
    COMMAND rapidjson-suite
    WORKING_DIRECTORY "${rapidjson_upstream_SOURCE_DIR}/bin"
    USES_TERMINAL)
