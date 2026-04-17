cmake_minimum_required(VERSION 3.24)
project(Cereka)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(vendor)
add_subdirectory(src)
add_subdirectory(runner)
add_subdirectory(launcher)
add_subdirectory(tests)

