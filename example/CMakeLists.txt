# A standalone project consuming an installed rapidyyjson.
#
#   cmake -S example -B example/build -DCMAKE_PREFIX_PATH=/where/you/installed
#   cmake --build example/build && ./example/build/basic
#
# The CI workflow builds this against a freshly staged install, so it doubles as
# a regression test for the exported CMake package.
cmake_minimum_required(VERSION 3.16)
project(rapidyyjson-example LANGUAGES CXX)

find_package(rapidyyjson REQUIRED)

add_executable(basic basic.cc)
target_link_libraries(basic PRIVATE rapidyyjson::rapidyyjson)
