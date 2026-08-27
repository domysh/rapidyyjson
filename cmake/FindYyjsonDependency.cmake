# Locate yyjson and expose it as the imported target `yyjson::yyjson`.
#
# Three sources are tried, in order:
#   1. yyjson's own CMake package (installed by yyjson >= 0.6 and by vcpkg);
#   2. pkg-config (`yyjson.pc`, shipped by most distribution packages);
#   3. a plain header/library search, for hand-rolled installations.
#
# The file is included both by the top-level build and by the installed
# rapidyyjsonConfig.cmake, so consumers resolve yyjson the same way we do.

if(TARGET yyjson::yyjson)
    return()
endif()

find_package(yyjson QUIET CONFIG)
if(TARGET yyjson::yyjson)
    return()
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_YYJSON QUIET IMPORTED_TARGET yyjson)
    if(TARGET PkgConfig::PC_YYJSON)
        add_library(yyjson::yyjson INTERFACE IMPORTED)
        target_link_libraries(yyjson::yyjson INTERFACE PkgConfig::PC_YYJSON)
        return()
    endif()
endif()

find_path(YYJSON_INCLUDE_DIR NAMES yyjson.h)
find_library(YYJSON_LIBRARY NAMES yyjson)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Yyjson
    REQUIRED_VARS YYJSON_LIBRARY YYJSON_INCLUDE_DIR
    FAIL_MESSAGE "yyjson not found. Install it (apt: libyyjson-dev, brew: yyjson, dnf: yyjson-devel, vcpkg: yyjson) or point CMAKE_PREFIX_PATH at your build of https://github.com/ibireme/yyjson"
)

add_library(yyjson::yyjson UNKNOWN IMPORTED)
set_target_properties(yyjson::yyjson PROPERTIES
    IMPORTED_LOCATION "${YYJSON_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${YYJSON_INCLUDE_DIR}"
)
mark_as_advanced(YYJSON_INCLUDE_DIR YYJSON_LIBRARY)
