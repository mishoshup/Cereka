# Phase 1: Distribution & Packaging — Pattern Map

**Mapped:** 2026-05-07
**Files analyzed:** 5 (4 modified, 1 new)
**Analogs found:** 5 / 5

---

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `vendor/CMakeLists.txt` | config (build) | transform | `cmake/toolchains/ucrt64.cmake` | role-match |
| `runner/CMakeLists.txt` | config (build) | transform | `runner/CMakeLists.txt` itself (existing WIN32 branch) | self-analog |
| `launcher/CMakeLists.txt` | config (build) | transform | `launcher/CMakeLists.txt` itself (existing POST_BUILD command) | self-analog |
| `launcher/main.cpp` (`doPackage`) | utility | file-I/O | `launcher/main.cpp` Windows DLL branch (lines 863–872) | self-analog |
| `scripts/make-appimage.sh` | utility (script) | batch | `scripts/build_all.sh` | role-match |

---

## Pattern Assignments

### `vendor/CMakeLists.txt` (config, transform)

**Change:** Add a `WIN32` gate that sets `BUILD_SHARED_LIBS OFF`, `SDL_SHARED OFF`, `SDL_STATIC ON` before any `add_subdirectory` call for SDL. Then add a `UNIX AND NOT APPLE` gate for `LIBRARY_OUTPUT_DIRECTORY` routing of SDL shared libraries to `build/runtimes/linux/`. Remove (or guard) the existing `WIN32` DLL-routing `foreach` loop once Windows static linking makes those DLL targets absent.

**Analog — existing `WIN32` DLL-routing block** (`vendor/CMakeLists.txt` lines 22–29):
```cmake
# Route vendor DLLs alongside CerekaGame so they are found at runtime
# and packaged together when distributing.
if(WIN32)
    foreach(_t SDL3-shared SDL3_image-shared SDL3_ttf-shared SDL3_mixer-shared)
        set_target_properties(${_t} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/windows")
    endforeach()
endif()
```

**Pattern to copy for static-link gate** — mirror the `set(... CACHE BOOL "" FORCE)` idiom already used in `cmake/toolchains/ucrt64.cmake` lines 7–12:
```cmake
set(SDLTTF_VENDORED   ON  CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED ON  CACHE BOOL "" FORCE)
```

**Pattern to copy for Linux `.so` routing** — mirror the WIN32 foreach above but change property name and target platform:
```cmake
if(UNIX AND NOT APPLE)
    foreach(_t SDL3-shared SDL3_image-shared SDL3_ttf-shared SDL3_mixer-shared)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/linux")
        endif()
    endforeach()
endif()
```
Note: `if(TARGET ${_t})` guard is required because the static build (`WIN32`) drops the `-shared` targets; the Linux foreach must not error when those targets don't exist.

**Key constraint:** The static-link block must appear **before** the `add_subdirectory(SDL)` call (line 1). The Linux routing block must appear **after** all four `add_subdirectory` calls so the targets exist.

---

### `runner/CMakeLists.txt` (config, transform)

**Change:** Add `INSTALL_RPATH "$ORIGIN"` and `BUILD_WITH_INSTALL_RPATH ON` to the existing `else()` branch that already sets `RUNTIME_OUTPUT_DIRECTORY` for Linux.

**Analog — existing WIN32/else target properties** (`runner/CMakeLists.txt` lines 9–15):
```cmake
if(WIN32)
    set_target_properties(CerekaGame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/windows")
else()
    set_target_properties(CerekaGame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/linux")
endif()
```

**Pattern to apply — extend the existing `else()` `set_target_properties` call:**
```cmake
else()
    set_target_properties(CerekaGame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/linux"
        INSTALL_RPATH             "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH  ON)
endif()
```
This is a pure additive change inside the existing `else()` block — same `set_target_properties` call, two extra properties. No new `if()` blocks needed.

---

### `launcher/CMakeLists.txt` (config, transform)

**Change:** Add `--compiler-runtime` to the existing `windeployqt` POST_BUILD command, and guard it so it only runs on native Windows (not during Linux→Windows cross-compile).

**Analog — existing POST_BUILD command** (`launcher/CMakeLists.txt` lines 64–68):
```cmake
add_custom_command(TARGET CerekaLauncher POST_BUILD
    COMMAND Qt6::windeployqt
    ARGS $<TARGET_FILE:CerekaLauncher>
    COMMENT "Bundling Qt6 DLLs for distribution"
)
```

**Pattern to apply — add flag and WIN32 guard:**
```cmake
if(WIN32)
    add_custom_command(TARGET CerekaLauncher POST_BUILD
        COMMAND Qt6::windeployqt
        ARGS --compiler-runtime $<TARGET_FILE:CerekaLauncher>
        COMMENT "Bundling Qt6 DLLs + compiler runtime for distribution"
    )
endif()
```
The existing command has no `WIN32` guard; on Linux `Qt6::windeployqt` may not be found, causing configure errors. The guard fixes that, and `--compiler-runtime` captures `libc++.dll` and `libunwind-1.dll` from the llvm-mingw toolchain.

---

### `launcher/main.cpp` — `doPackage()` Linux branch (utility, file-I/O)

**Change:** Inside the `if (plat.name == "linux")` branch (currently lines 858–862), add SDL `.so` file copying after the `fs::permissions` call, mirroring the Windows DLL copy loop in the `else` branch.

**Analog — existing Windows DLL copy loop** (`launcher/main.cpp` lines 863–872):
```cpp
} else {
    for (auto &e : fs::directory_iterator(runtimeDir("windows"), ec)) {
        if (e.path().extension() == ".dll") {
            fs::copy_file(e.path(), stagingDir / e.path().filename(),
                          fs::copy_options::overwrite_existing, ec);
            if (!ec)
                appendLog("Copied " + e.path().filename().string());
        }
    }
}
```

**Pattern to copy for Linux `.so` copy** — use a named-list approach instead of directory iteration (SDL `.so` files have known sonames, and directory iteration risks picking up `.so` dev symlinks or build artifacts):
```cpp
if (plat.name == "linux") {
    fs::permissions(stagingDir / gameExe,
                    fs::perms::owner_exec | fs::perms::group_exec |
                        fs::perms::others_exec,
                    fs::perm_options::add, ec);
    static const char* soLibs[] = {
        "libSDL3.so.0", "libSDL3_image.so.0",
        "libSDL3_ttf.so.0", "libSDL3_mixer.so.0"
    };
    for (auto* lib : soLibs) {
        fs::path src = runtimeDir("linux") / lib;
        if (fs::exists(src)) {
            fs::copy_file(src, stagingDir / lib,
                          fs::copy_options::overwrite_existing, ec);
            if (!ec)
                appendLog("Copied " + std::string(lib));
        }
    }
}
```

**Supporting patterns from the same function:**
- Error pattern (lines 838–843): `if (ec) { appendLog("[ERROR] ..."); continue; }` — use same structure for any `fs::copy_file` failure check if needed.
- Log pattern (lines 845–846): `appendLog("...")` followed immediately by `QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection)` — use same pattern after the SDL copy log calls to flush to the UI.
- `runtimeDir()` helper (lines 83–86): `return selfExeDir() / "runtimes" / platform;` — already returns the correct path; use `runtimeDir("linux")` as the source directory.

---

### `scripts/make-appimage.sh` (utility script, batch)

**Analog:** `scripts/build_all.sh`

**Shell structure pattern** (`scripts/build_all.sh` lines 1–16):
```bash
#!/usr/bin/env bash
# build_all.sh — Build Linux + Windows runtimes and collect them under one tree.
#
# Usage:
#   ./scripts/build_all.sh
#
# Produces:
#   build/CerekaLauncher
#   ...

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
```

**Pattern to copy for `make-appimage.sh`:**
- Header comment block: one-line description, `Usage:` section, `Produces:` section
- `set -euo pipefail` — mandatory; all scripts in this repo use it
- `ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"` — resolve project root relative to script location, not CWD
- Download tools with `[ -f <tool> ] || wget -c <url>` guard (don't re-download on re-run)
- Variables for configurable inputs (`BUILD_DIR`, `QMAKE`) with defaults via `${VAR:-default}`

**Concrete script structure (based on analog + AppImage requirements):**
```bash
#!/usr/bin/env bash
# make-appimage.sh — Package CerekaLauncher as an AppImage for Linux distribution.
#
# Usage:
#   ./scripts/make-appimage.sh [build-dir] [qmake-path]
#
# Produces:
#   CerekaLauncher-x86_64.AppImage (in the project root)
#
# Requires: wget, linuxdeploy, linuxdeploy-plugin-qt (downloaded if absent)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
QMAKE="${2:-$(which qmake6 2>/dev/null || which qmake 2>/dev/null || echo qmake)}"

# ... download linuxdeploy tools if absent ...
# ... create AppDir structure ...
# ... run linuxdeploy ...
```

**New required files referenced by the script:**
- `launcher/cereka-launcher.desktop` — XDG desktop entry (AppImage requirement; `linuxdeploy` refuses without it)
- `launcher/cereka-launcher.png` — 256x256 PNG icon (AppImage requirement)

`.desktop` file format (from AppImage packaging spec):
```ini
[Desktop Entry]
Type=Application
Name=Cereka Launcher
Exec=CerekaLauncher
Icon=cereka-launcher
Categories=Development;Game;
Comment=Visual novel engine — project manager and packager
```

---

## Shared Patterns

### `CACHE BOOL "" FORCE` Option Override
**Source:** `cmake/toolchains/ucrt64.cmake` lines 7–12
**Apply to:** `vendor/CMakeLists.txt` static-link gate
```cmake
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
```
This is the project-established idiom for overriding SDL CMake options so they survive CMake cache re-reads. Use `FORCE` — without it, a stale cache value from a previous configure wins.

### Platform-Conditional `set_target_properties`
**Source:** `runner/CMakeLists.txt` lines 9–15 and `vendor/CMakeLists.txt` lines 24–29
**Apply to:** `vendor/CMakeLists.txt` Linux routing block, `runner/CMakeLists.txt` RPATH addition
The project pattern is: wrap target property mutations in `if(WIN32) ... else() ... endif()` or `if(UNIX AND NOT APPLE)`. Always use the `if(TARGET ${_t})` guard inside `foreach` loops to avoid errors when a target doesn't exist under a given build configuration.

### `appendLog` + `QMetaObject::invokeMethod` flush
**Source:** `launcher/main.cpp` lines 845–846, 873–875, etc.
**Apply to:** All new log lines in `doPackage()`
```cpp
appendLog("Some status...");
QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);
```
Every `appendLog` that should appear in the UI promptly must be followed by this `invokeMethod` call. Calls at end of loop iteration are already there; new intermediate log calls inside the Linux `.so` copy block need the same treatment only if they should appear before the next existing flush.

### `fs::copy_file` Error Check Pattern
**Source:** `launcher/main.cpp` lines 848–856
**Apply to:** New `.so` copy calls in `doPackage()` Linux branch
```cpp
fs::copy_file(src, dst / filename, fs::copy_options::overwrite_existing, ec);
if (ec) {
    appendLog("[ERROR] Cannot copy ...: " + ec.message());
    // decide: continue or abort
}
```
The project uses the `std::error_code` overload (not the throwing overload) throughout `doPackage()`. Maintain this pattern for the `.so` copy calls.

### Shell Script Safety Header
**Source:** `scripts/build_all.sh` lines 14–16
**Apply to:** `scripts/make-appimage.sh`
```bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
```
All scripts use `set -euo pipefail` and resolve `ROOT` from the script's own location. Follow this without exception.

---

## No Analog Found

All files have close analogs in the codebase. No files require falling back to RESEARCH.md patterns exclusively. The RESEARCH.md code examples (Patterns 1–6) are consistent with the codebase analogs identified above and may be used as secondary reference for the AppImage script, which has only a partial analog in `build_all.sh`.

---

## Metadata

**Analog search scope:** `vendor/`, `runner/`, `launcher/`, `cmake/`, `scripts/`
**Files read:** `vendor/CMakeLists.txt`, `runner/CMakeLists.txt`, `launcher/CMakeLists.txt`, `launcher/main.cpp` (lines 68–103, 783–947), `cmake/toolchains/ucrt64.cmake`, `scripts/build_all.sh`
**Pattern extraction date:** 2026-05-07
