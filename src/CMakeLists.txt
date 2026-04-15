file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Embed compiler.lua as a C++ string at configure time
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/compiler.lua" COMPILER_LUA_RAW)
configure_file(
    compiler_lua_embed.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/compiler_lua_embed.hpp"
    @ONLY
)

add_library(Cereka STATIC ${SRC})

target_include_directories(Cereka
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

# CMake strips /usr/* paths from -I flags as "implicit system paths".
# Force Lua 5.4 headers before system Lua 5.5 by injecting the -I flag directly.
target_compile_options(Cereka PUBLIC "-I/usr/include/lua5.4")
target_include_directories(Cereka
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(Cereka PUBLIC vendor)

target_include_directories(Cereka
  PUBLIC vendor/sol2/include
)

