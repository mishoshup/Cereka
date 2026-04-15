cmake_minimum_required(VERSION 3.24)
project(Cereka)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# sol2 does not support Lua 5.5 yet — require exactly 5.4
find_package(Lua 5.4 EXACT REQUIRED)

add_subdirectory(vendor)
add_subdirectory(src)
add_subdirectory(runner)
add_subdirectory(launcher)

