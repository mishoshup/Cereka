add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(SDL_mixer)
add_subdirectory(sol2)

find_package(Lua REQUIRED)

add_library(vendor INTERFACE)
target_link_libraries(vendor INTERFACE SDL3::SDL3 SDL3_image SDL3_ttf SDL3_mixer ${LUA_LIBRARIES} sol2)
