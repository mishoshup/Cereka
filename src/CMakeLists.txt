file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

set(COMPILER_LUA       "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/compiler.lua")
set(COMPILER_LUA_EMBED "${CMAKE_CURRENT_BINARY_DIR}/compiler_lua_embed.hpp")

# Re-embed compiler.lua at build time whenever the .lua file changes.
add_custom_command(
    OUTPUT  "${COMPILER_LUA_EMBED}"
    COMMAND ${CMAKE_COMMAND}
            -DSRC="${COMPILER_LUA}"
            -DDST="${COMPILER_LUA_EMBED}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/embed_lua.cmake"
    DEPENDS "${COMPILER_LUA}"
    COMMENT "Embedding scripts/compiler.lua"
)

add_library(Cereka STATIC ${SRC} "${COMPILER_LUA_EMBED}")

target_include_directories(Cereka
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(Cereka PUBLIC vendor)

target_include_directories(Cereka
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/sol2/include
)
