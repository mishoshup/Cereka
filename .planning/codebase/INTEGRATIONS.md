# External Integrations

**Analysis Date:** 2026-05-07

## Vendored Libraries

All vendored libraries live under `vendor/` as Git submodules (except GoogleTest which uses FetchContent).

| Library | Version | Integration Method | Used For |
|---------|---------|-------------------|----------|
| SDL3 | 3.3.7 | Git submodule + `add_subdirectory` | Window creation, hardware-accelerated 2D rendering, event loop, input |
| SDL3_image | 3.3.0 | Git submodule + `add_subdirectory` | Loading PNG assets (backgrounds, character sprites, UI images) |
| SDL3_ttf | 3.3.0 | Git submodule + `add_subdirectory` | Rendering TrueType fonts for dialogue text, name boxes, buttons |
| SDL3_mixer | 3.1.0 | Git submodule + `add_subdirectory` | Looping BGM and one-shot SFX playback via `audio_manager.cpp` |
| Lua 5.4.7 | 5.4.7 | Git submodule + `add_subdirectory`; static only | Host runtime for the `.crka` compiler (`scripts/compiler.lua`) |
| sol2 | 4.0.0 | Git submodule + `add_subdirectory`; header-only | C++ ↔ Lua FFI: calling `compiler.lua` from `src/compiler/vn_instruction.cpp` |
| glaze | 7.3.3 | Git submodule + `add_subdirectory`; header-only | JSON read/write for save slots (`src/save_data.hpp`, `src/save.cpp`) |
| GoogleTest | v1.14.0 | CMake `FetchContent_Declare` from github.com/google/googletest | C++ unit test runner + gmock for `tests/` |
| imgui | (submodule present) | Not wired — submodule exists in `vendor/imgui` but no CMakeLists target | Unused; reserved for future in-engine debug UI |

**Namespaced CMake targets in use:**
- `SDL3::SDL3`, `SDL3_image::SDL3_image`, `SDL3_ttf::SDL3_ttf`, `SDL3_mixer::SDL3_mixer`
- `Lua::Library` (from the lua submodule CMake package)
- `sol2` (target name from sol2's CMakeLists)
- `glaze::glaze`
- `Qt6::Widgets` (system)
- `gtest`, `gmock` (FetchContent)

All SDL, Lua, sol2, and glaze targets are collected into a single `INTERFACE` library target named `vendor` in `vendor/CMakeLists.txt`. The `Cereka` static lib links `vendor` publicly, so `CerekaGame` and `cereka_test` inherit all transitive dependencies.

## System Dependencies

| Dependency | Required By | Notes |
|-----------|-------------|-------|
| Qt6 6.8.3 | `launcher/` only | Installed via aqtinstall; `find_package(Qt6 COMPONENTS Widgets REQUIRED)`. Path: `~/Qt/6.8.3/linux_gcc_64` (Linux) or `C:/Qt/6.8.3/win64_msvc2022_64` (Windows). |
| Lua 5.4 interpreter | Snapshot tests only | `lua tests/compile/harness.lua` — host Lua needed to run compile-output snapshot tests; the engine embeds its own Lua and does not need a system install at runtime. |
| llvm-mingw (UCRT64) | Cross-compile only | Searched in `/opt/llvm-mingw-ucrt/bin` and siblings; only needed when building with `cmake/toolchains/ucrt64.cmake`. |
| C++23-capable compiler | Engine (`src/`, `runner/`, `tests/`) | GCC or Clang on Linux; MSVC or MinGW-w64 on Windows. |
| C++17-capable compiler | Launcher (`launcher/`) | Qt6 AUTOMOC requires at minimum C++17. |

## File Formats

| Format | Extension / Path | Read | Write | Handler |
|--------|-----------------|------|-------|---------|
| CRKA script | `assets/scripts/*.crka` | Yes | No | `scripts/compiler.lua` → `src/compiler/vn_instruction.cpp` |
| Game config | `game.cfg` | Yes | No (runner) | `runner/main.cpp` (`parseConfig` — plain `key=value` text) |
| Launcher config | `~/.cereka/cereka.cfg` | Yes | Yes | `launcher/config.cpp` (plain `key=value` text) |
| Save data | `saves/slot{1-10}.sav` | Yes | Yes | `src/save.cpp` + `src/save_data.hpp` (glaze JSON) |
| PNG images | `assets/bg/`, `assets/characters/`, `assets/ui/` | Yes | No | SDL3_image |
| TTF/OTF fonts | `assets/fonts/` | Yes | No | SDL3_ttf |
| WAV/OGG audio | `assets/sounds/` | Yes | No | SDL3_mixer |
| Embedded assets | `launcher/template_assets/` | Yes (at build time) | No | CMake `embed_file()` macro hex-encodes them into `embedded_assets.h` |

**Save file detail:** glaze JSON; file extension is `.sav` (not `.json`). Schema defined via `glz::meta` specializations in `src/save_data.hpp`. Fields: `timestamp`, `programCounter`, `callStack`, `variables`, `background`, `characters`, `bgm`, `state`, `speaker`, `name`, `text`, `displayedChars`, `skipMode`, `skipDepth`.

**game.cfg format:** Plain text `key=value` per line. Keys: `title`, `width`, `height`, `fullscreen`, `entry`.

**Compiler pipeline:** `compiler.lua` is embedded at build time as a C++ byte-array header (`compiler_lua_embed.hpp`) via `src/compiler/embed_lua.cmake`. At runtime, `CompileVNScript` in `src/compiler/vn_instruction.cpp` loads this embedded string into a `sol::state` and calls the Lua compiler function. No Lua files are read from disk at game runtime.

## IPC / Subprocess

**Launcher → CerekaGame (dev-run):**
The launcher spawns CerekaGame as a child process to preview a project in development. Implementation is platform-branched in `launcher/main.cpp`:

- **Linux:** `fork()` + `execlp()` with a `pipe()`/`dup2()` redirecting child stdout/stderr back to the launcher's log panel (around line 747–770).
- **Windows:** `CreateProcessA()` (around line 731–743). Output piping is not implemented on Windows in the current code.

No socket, shared memory, or named-pipe IPC is used. Communication is one-way: child output is streamed into the launcher's `QTextEdit` log widget. The launcher has no way to send commands to the running game process.

**Packaging:**
The launcher copies project files + built binaries into a distribution directory using `std::filesystem`. On Windows, `windeployqt` is run as a CMake post-build step (not at packaging time) to bundle Qt6 DLLs alongside the launcher binary itself.

---

*Integration audit: 2026-05-07*
