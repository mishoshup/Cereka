add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(SDL_mixer)
add_subdirectory(sol2)

# Force Lua 5.4 — sol2 does not yet support Lua 5.5
set(LUA_INCLUDE_DIR "/usr/include/lua5.4")
set(LUA_LIBRARY "/usr/lib/liblua5.4.so")
find_package(Lua 5.4 REQUIRED)

add_library(vendor INTERFACE)
target_link_libraries(vendor INTERFACE SDL3::SDL3 SDL3_image SDL3_ttf SDL3_mixer ${LUA_LIBRARIES} sol2)
target_include_directories(vendor INTERFACE ${LUA_INCLUDE_DIR})
