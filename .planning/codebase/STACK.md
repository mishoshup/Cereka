# Technology Stack

**Analysis Date:** 2026-05-07

## Languages

**Primary:**
- C++23 — Engine (`src/`), game runner (`runner/`), tests (`tests/`)
- C++17 — Launcher (`launcher/`) — uses Qt6 which constrains the standard

**Secondary:**
- Lua 5.4 — Script compiler (`scripts/compiler.lua`), embedded in the engine binary at build time

## Build System

- **CMake** 3.24+ — root build orchestrator; minimum 3.16 for launcher
- **Ninja** — preferred generator (documented in CLAUDE.md build commands)
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` — generates `compile_commands.json` for IDE/tooling
- Cross-compile toolchain: `cmake/toolchains/ucrt64.cmake` (llvm-mingw/UCRT64 targeting Windows x86_64 from Linux)

## Core Libraries

| Library | Version | Purpose | Integration |
|---------|---------|---------|-------------|
| SDL3 | 3.3.7 | Window, rendering, input, event loop | Git submodule `vendor/SDL`, built as shared lib |
| SDL3_image | 3.3.0 | PNG/image loading for backgrounds and sprites | Git submodule `vendor/SDL_image`, shared |
| SDL3_ttf | 3.3.0 | TrueType font rendering (dialogue text, UI labels) | Git submodule `vendor/SDL_ttf`, shared |
| SDL3_mixer | 3.1.0 | BGM looping + SFX one-shot audio playback | Git submodule `vendor/SDL_mixer`, shared |
| Lua 5.4.7 | 5.4.7 | Script compiler runtime; only static lib built | Git submodule `vendor/lua` (walterschell/Lua), static |
| sol2 | 4.0.0 | C++/Lua binding layer; exposes Lua state to C++ | Git submodule `vendor/sol2`, header-only |
| glaze | 7.3.3 | JSON serialization for save files | Git submodule `vendor/glaze`, header-only |
| GoogleTest | v1.14.0 | C++ unit test runner + mock framework | CMake FetchContent (not vendored as submodule) |
| Qt6 | 6.8.3 | GUI framework for the launcher application | System install (via aqtinstall); `find_package(Qt6 COMPONENTS Widgets)` |

**Note:** `vendor/imgui` submodule is present but not wired into any CMakeLists.txt target — unused at this time.

## Runtime Dependencies

**CerekaGame (`build/runtimes/<linux|windows>/`):**
- SDL3 shared library (`.so` on Linux, `.dll` on Windows — bundled on Windows via vendor DLL routing)
- SDL3_image shared library
- SDL3_ttf shared library
- SDL3_mixer shared library
- A valid `game.cfg` and `assets/` directory tree in the working directory

**CerekaLauncher (`build/launcher/`):**
- Qt6::Widgets (system Qt6 install; `windeployqt` bundles DLLs on Windows post-build)
- No SDL dependency — the launcher forks/`CreateProcess`es CerekaGame separately

**cereka_test (`build/tests/`):**
- Cereka static library + all SDL shared libs (linked transitively via `vendor` interface target)

## Dev Tooling

- **Compiler (Linux native):** System GCC or Clang supporting C++23
- **Compiler (Windows cross-compile):** llvm-mingw UCRT64 (`x86_64-w64-ucrt-clang++`) — toolchain at `cmake/toolchains/ucrt64.cmake`
- **Lua interpreter:** Host Lua 5.4 required to run snapshot tests (`tests/compile/harness.lua`)
- **aqtinstall:** Used to install Qt6 on CI/dev machines (no system package manager assumption)
- **windeployqt:** Qt6 tool invoked as a post-build custom command in `launcher/CMakeLists.txt` to bundle Qt DLLs alongside the launcher binary on Windows
- **embed_lua.cmake:** Custom CMake script that xxd-encodes `scripts/compiler.lua` into a C++ header (`compiler_lua_embed.hpp`) at build time; re-runs whenever `compiler.lua` changes

## Platform Targets

| Target | OS | Architecture | Compiler |
|--------|-----|-------------|---------|
| Linux native | Linux | x86_64 | System GCC/Clang |
| Windows native | Windows 10+ (UCRT) | x86_64 | MSVC or MinGW |
| Linux→Windows cross | Windows (from Linux host) | x86_64 | llvm-mingw UCRT64 |

Output binaries:
- `build/runtimes/linux/CerekaGame`
- `build/runtimes/windows/CerekaGame.exe`
- `build/launcher/CerekaLauncher[.exe]`
- `build/tests/cereka_test[.exe]`

---

*Stack analysis: 2026-05-07*
