# Phase 01 Plan 01 Summary — SDL Static Gate & Linux RPATH

## Summary of Changes
- Modified `vendor/CMakeLists.txt` to enable static linking for SDL3 on Windows using `BUILD_SHARED_LIBS OFF`, `SDL_SHARED OFF`, and `SDL_STATIC ON`.
- Added library routing for Linux shared libraries in `vendor/CMakeLists.txt` to `build/runtimes/linux/`.
- Updated `runner/CMakeLists.txt` to set `INSTALL_RPATH` to `$ORIGIN` and enabled `BUILD_WITH_INSTALL_RPATH` for the Linux CerekaGame target.
- Removed obsolete Windows DLL routing block from `vendor/CMakeLists.txt`.

## Verification Results
- `cmake --build build` succeeded on macOS.
- `vendor/CMakeLists.txt` contains the WIN32 static gate and Linux library routing.
- `runner/CMakeLists.txt` contains the `$ORIGIN` RPATH configuration.
- The build produces `CerekaGame` in the platform-correct runtime directory.
