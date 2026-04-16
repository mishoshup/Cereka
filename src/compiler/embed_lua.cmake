# Called by add_custom_command to re-embed compiler.lua into a C++ header.
# Usage: cmake -DSRC=<path/compiler.lua> -DDST=<path/compiler_lua_embed.hpp> -P embed_lua.cmake
file(READ "${SRC}" LUA_SOURCE)
file(WRITE "${DST}" [=[#pragma once
// Auto-generated from scripts/compiler.lua — do not edit directly.
namespace cereka {
static const char *COMPILER_LUA_SOURCE = R"LUA_DELIM(
]=])
file(APPEND "${DST}" "${LUA_SOURCE}")
file(APPEND "${DST}" [=[
)LUA_DELIM";
}
]=])
