cmake_minimum_required(VERSION 3.15)
project(scheme2c)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/fetch-libs.cmake)

add_subdirectory(src)
add_subdirectory(tests)