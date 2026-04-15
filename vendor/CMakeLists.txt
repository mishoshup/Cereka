add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(SDL_mixer)
add_subdirectory(sol2)

# Route all vendor DLL outputs to the build root so executables find them
# without PATH tricks or manual copying.
if(WIN32)
    foreach(_t SDL3-shared SDL3_image-shared SDL3_ttf-shared SDL3_mixer-shared)
        set_target_properties(${_t} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    endforeach()
endif()

# Lua 5.4 is found once in the root CMakeLists.txt via find_package(Lua 5.4 EXACT REQUIRED).
# LUA_LIBRARIES and LUA_INCLUDE_DIR are already set — just use them here.
add_library(vendor INTERFACE)
target_link_libraries(vendor INTERFACE SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf SDL3_mixer::SDL3_mixer ${LUA_LIBRARIES} sol2)
target_include_directories(vendor INTERFACE ${LUA_INCLUDE_DIR})
