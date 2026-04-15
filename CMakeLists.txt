cmake_minimum_required(VERSION 3.24)
project(Cereka)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force Lua 5.4 — sol2 does not support Lua 5.5 yet
set(LUA_INCLUDE_DIR "/usr/include/lua5.4")
set(LUA_LIBRARY "/usr/lib/liblua5.4.so")
include_directories(BEFORE /usr/include/lua5.4)

add_subdirectory(vendor)
add_subdirectory(src)

