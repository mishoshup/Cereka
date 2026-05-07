# On Windows: build SDL3* as static libraries → CerekaGame.exe has zero DLL dependencies.
# Use FORCE so the SDL CMake options override any stale cache values from a prior configure.
# Keep Linux as shared (glibc ABI coupling makes static SDL impractical across distros).
if(WIN32)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED        OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC        ON  CACHE BOOL "" FORCE)
endif()

add_subdirectory(SDL)

# Vendored SDL flags moved to cmake/toolchains/ucrt64.cmake for cross-compiles
# (keeps vendor files untouched; toolchain forces these when targeting UCRT)

add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(SDL_mixer)

# On Linux: route SDL shared libraries into build/runtimes/linux/ alongside CerekaGame.
# This satisfies the $ORIGIN RPATH lookup (set in runner/CMakeLists.txt) and gives the
# packager a single source directory to copy .so files from.
# Note: shared libs use LIBRARY_OUTPUT_DIRECTORY, not RUNTIME_OUTPUT_DIRECTORY (which
# applies to executables and DLLs only).
if(UNIX AND NOT APPLE)
    foreach(_t SDL3-shared SDL3_image-shared SDL3_ttf-shared SDL3_mixer-shared)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/linux")
        endif()
    endforeach()
endif()

add_subdirectory(sol2)

# Lua 5.4.7 — built from source, no system package needed.
# Disable tests and standalone binaries; we only need the static library.
set(LUA_ENABLE_TESTING  OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_BINARY    OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_COMPILER  OFF CACHE BOOL "" FORCE)
set(LUA_ENABLE_SHARED   OFF CACHE BOOL "" FORCE)
add_subdirectory(lua)

# Glaze — header-only JSON/TOML serialization library
add_subdirectory(glaze)

add_library(vendor INTERFACE)
# Lua::Library provides include dirs transitively — no need to list them separately.
target_link_libraries(vendor INTERFACE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    SDL3_mixer::SDL3_mixer
    Lua::Library
    sol2
    glaze::glaze)
