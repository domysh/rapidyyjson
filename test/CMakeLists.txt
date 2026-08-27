add_executable(rapidyyjson-conformance conformance.cc)
target_link_libraries(rapidyyjson-conformance PRIVATE rapidyyjson::rapidyyjson)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rapidyyjson-conformance PRIVATE -Wall -Wextra)
endif()

add_test(NAME conformance COMMAND rapidyyjson-conformance)
