# Called by add_custom_command to re-embed cereka_compiler.lua into a C++ header.
# Usage: cmake -DSRC=<path/cereka_compiler.lua> -DDST=<path/compiler_lua_embed.hpp> -P embed_lua.cmake
file(READ "${SRC}" LUA_SOURCE)
file(WRITE "${DST}" [=[#pragma once
// Auto-generated from scripts/cereka_compiler.lua — do not edit directly.
namespace cereka {
static const char *COMPILER_LUA_SOURCE = R"LUA_DELIM(
]=])
file(APPEND "${DST}" "${LUA_SOURCE}")
file(APPEND "${DST}" [=[
)LUA_DELIM";
}
]=])
