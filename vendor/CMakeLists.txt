add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(SDL_mixer)
add_subdirectory(sol2)

# Lua 5.4.7 — built from source, no system package needed.
# Disable tests and standalone binaries; we only need the static library.
set(LUA_ENABLE_TESTING  OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_BINARY    OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_COMPILER  OFF CACHE BOOL "" FORCE)
set(LUA_ENABLE_SHARED   OFF CACHE BOOL "" FORCE)
add_subdirectory(lua)

# Route all vendor DLL outputs to the build root so executables find them
# without PATH tricks or manual copying.
if(WIN32)
    foreach(_t SDL3-shared SDL3_image-shared SDL3_ttf-shared SDL3_mixer-shared)
        set_target_properties(${_t} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    endforeach()
endif()

add_library(vendor INTERFACE)
# Lua::Library provides include dirs transitively — no need to list them separately.
target_link_libraries(vendor INTERFACE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    SDL3_mixer::SDL3_mixer
    Lua::Library
    sol2)
