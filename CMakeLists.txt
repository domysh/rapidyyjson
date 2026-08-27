cmake_minimum_required(VERSION 3.16)

project(rapidyyjson
    VERSION 0.1.0
    DESCRIPTION "A drop-in reimplementation of the RapidJSON 1.1.0 API on top of yyjson"
    HOMEPAGE_URL "https://github.com/domysh/rapidyyjson"
    LANGUAGES CXX
)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(RAPIDYYJSON_IS_TOP_LEVEL ON)
else()
    set(RAPIDYYJSON_IS_TOP_LEVEL OFF)
endif()

option(RAPIDYYJSON_BUILD_TESTS "Build the conformance test suite" ${RAPIDYYJSON_IS_TOP_LEVEL})
option(RAPIDYYJSON_BUILD_RAPIDJSON_SUITE
       "Also build RapidJSON's own unit tests against rapidyyjson (fetches RapidJSON and googletest)"
       OFF)
option(RAPIDYYJSON_INSTALL "Generate the install target" ${RAPIDYYJSON_IS_TOP_LEVEL})

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(FindYyjsonDependency)

# The library is header-only; the only compiled code it needs is libyyjson.
add_library(rapidyyjson INTERFACE)
add_library(rapidyyjson::rapidyyjson ALIAS rapidyyjson)

target_compile_features(rapidyyjson INTERFACE cxx_std_17)
target_link_libraries(rapidyyjson INTERFACE yyjson::yyjson)
target_include_directories(rapidyyjson INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(RAPIDYYJSON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(RAPIDYYJSON_INSTALL)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY include/rapidyyjson
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(TARGETS rapidyyjson EXPORT rapidyyjsonTargets)

    install(EXPORT rapidyyjsonTargets
            FILE rapidyyjsonTargets.cmake
            NAMESPACE rapidyyjson::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rapidyyjson)

    configure_package_config_file(
        cmake/rapidyyjsonConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/rapidyyjsonConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rapidyyjson)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/rapidyyjsonConfigVersion.cmake"
        COMPATIBILITY SameMinorVersion
        ARCH_INDEPENDENT)

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/rapidyyjsonConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/rapidyyjsonConfigVersion.cmake"
        "${CMAKE_CURRENT_LIST_DIR}/cmake/FindYyjsonDependency.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rapidyyjson)
endif()
