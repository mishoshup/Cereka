# Phase 1: Distribution & Packaging — Research

**Researched:** 2026-05-07
**Domain:** CMake build system, SDL3 static linking, Linux RPATH, Windows runtime DLL deployment, AppImage packaging
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Windows — CerekaGame (player distribution)**
- Static link all SDL3 libraries (`BUILD_SHARED_LIBS OFF` forced before SDL subdirectories)
- Result: single `CerekaGame.exe` with zero DLL dependencies
- Launcher packager ZIPs: `CerekaGame.exe` + `assets/` + `game.cfg` only
- Remove the `RUNTIME_OUTPUT_DIRECTORY` DLL-routing loop from `vendor/CMakeLists.txt`

**Linux — CerekaGame (player distribution)**
- Set `INSTALL_RPATH "$ORIGIN"` + `BUILD_WITH_INSTALL_RPATH ON` on `CerekaGame` target
- Linux packager copies SDL `.so` files alongside binary — no wrapper script
- ZIP layout: `CerekaGame` + `libSDL3.so.0` + `libSDL3_image.so.0` + `libSDL3_ttf.so.0` + `libSDL3_mixer.so.0` + `assets/` + `game.cfg`

**Windows — CerekaLauncher (dev tool distribution)**
- Fix windeployqt to capture UCRT/C++ runtime DLLs
- Launcher ships as directory with `CerekaLauncher.exe` + Qt6 DLLs + runtime DLLs

**Linux — CerekaLauncher (dev tool distribution)**
- Package CerekaLauncher as an AppImage using `linuxdeploy` + `linuxdeploy-plugin-qt`
- Single `.AppImage` file, no install needed

**Launcher Packager**
- Windows package: ZIP contains `CerekaGame.exe` (statically linked) + `assets/` + `game.cfg`
- Linux package: ZIP contains `CerekaGame` (RPATH set) + SDL `.so` files + `assets/` + `game.cfg`
- Platform detection in `launcher/main.cpp` (already exists in `doPackage()`)

### Claude's Discretion
(None specified)

### Deferred Ideas (OUT OF SCOPE)
- Flatpak / Snap packaging
- Windows installer (NSIS/WiX)
- macOS support
- Code signing / notarization
</user_constraints>

---

## Summary

This phase makes Cereka games and the CerekaLauncher distributable on fresh Linux and Windows machines. The work spans four independent sub-problems: (1) make `CerekaGame.exe` a zero-dependency single binary on Windows via SDL3 static linking, (2) make `CerekaGame` on Linux find its bundled `.so` files via `$ORIGIN` RPATH instead of relying on system-installed SDL, (3) fix the launcher packager (`doPackage()` in `main.cpp`) to include the correct file set per platform, and (4) package `CerekaLauncher` itself for distribution — fixing missing runtime DLLs on Windows and producing an AppImage on Linux.

All four sub-problems are localized to CMake build scripts and the launcher's packaging logic. No engine code changes are needed. The SDL3 libraries already support both static and shared build modes natively through `SDL_STATIC`/`SDL_SHARED` CMake options and `BUILD_SHARED_LIBS`. The `SDL3::SDL3`, `SDL3_image::SDL3_image`, etc. target aliases automatically redirect to the static variants when `BUILD_SHARED_LIBS=OFF` is set before the `add_subdirectory` calls — so `target_link_libraries` calls in `vendor/CMakeLists.txt` need no changes.

**Primary recommendation:** Gate all four changes behind CMake platform conditions (`if(WIN32)`, `if(UNIX AND NOT APPLE)`). The Windows static-link gate can use the existing toolchain-file pattern. Keep the Linux build shared to avoid glibc version coupling across distros. The AppImage build step is a post-build shell script triggered manually, not wired into the main CMake target, because it requires `linuxdeploy` to be present on the packaging machine.

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| SDL3 static link (Windows) | Build System (CMake) | — | `BUILD_SHARED_LIBS OFF` and `SDL_SHARED OFF` set in `vendor/CMakeLists.txt` before `add_subdirectory` |
| RPATH `$ORIGIN` (Linux game) | Build System (CMake) | — | `INSTALL_RPATH` target property on `CerekaGame` in `runner/CMakeLists.txt` |
| SDL `.so` routing to runtimes dir (Linux) | Build System (CMake) | — | `RUNTIME_OUTPUT_DIRECTORY` for SDL shared targets, mirroring the existing Windows pattern |
| Launcher ZIP packaging (both platforms) | Launcher C++ (`main.cpp`) | — | `doPackage()` function already exists; needs Linux `.so` copy and Windows DLL-only ZIP |
| UCRT/C++ runtime DLLs (Windows launcher) | Build System (CMake) | Launcher install step | `windeployqt --compiler-runtime` flag or explicit CMake `file(GET_RUNTIME_DEPENDENCIES)` |
| AppImage creation (Linux launcher) | Shell script / CMake custom target | linuxdeploy tool | `linuxdeploy` + `linuxdeploy-plugin-qt` run against the launcher binary |

---

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| CMake | 3.24+ (project min) | Build orchestration, RPATH properties, target-property-based SDL mode switching | Already in use; all SDL, RPATH, and runtime-copy patterns are native CMake |
| SDL3 `SDL_STATIC`/`SDL_SHARED` options | vendor SDL 3.3.7 | Gate whether static or shared SDL builds | SDL3's own CMake mechanism; `SDL3::SDL3` alias auto-redirects to static when `BUILD_SHARED_LIBS=OFF` |
| `linuxdeploy` + `linuxdeploy-plugin-qt` | latest AppImage releases | Bundle Qt6 into an AppImage for Linux | Standard tool for Qt Linux deployment; successor to `linuxdeployqt`; supports Qt6 |
| `windeployqt` | Qt 6.8.3 (already wired) | Bundle Qt6 DLLs for Windows launcher | Already in `launcher/CMakeLists.txt` as POST_BUILD |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `cmake/toolchains/ucrt64.cmake` | existing | Cross-compile Linux→Windows; already forces `SDLTTF_VENDORED`/`SDLIMAGE_VENDORED` | Already used for cross-compile; `BUILD_SHARED_LIBS OFF` and `SDL_SHARED OFF` must also be set here for the Windows static build |
| `windeployqt --compiler-runtime` | Qt 6.8.3 | Tells windeployqt to include MinGW/UCRT runtime DLLs alongside Qt DLLs | Add this flag to the existing POST_BUILD command in `launcher/CMakeLists.txt` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `linuxdeploy` + plugin-qt | `appimage-builder` | `appimage-builder` bundles glibc itself (more portable); heavier, needs YAML recipe; overkill for a dev tool |
| `linuxdeploy` + plugin-qt | `linuxdeployqt` (old) | `linuxdeployqt` is unmaintained and Qt6-incompatible |
| Manual DLL copy for launcher runtime | `InstallRequiredSystemLibraries` CMake module | CMake module is MSVC-centric; `--compiler-runtime` flag is the correct path for MinGW/UCRT builds |

**Installation (linuxdeploy — download at packaging time, not vendored):**
```bash
wget -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
wget -c "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage
```

---

## Architecture Patterns

### System Architecture Diagram

```
Build time (CMake configure)
  ├─ WIN32? ──► SDL_SHARED=OFF, SDL_STATIC=ON, BUILD_SHARED_LIBS=OFF
  │              → SDL3::SDL3 aliases to SDL3-static
  │              → SDL3_image::SDL3_image aliases to SDL3_image-static (etc.)
  │              → CerekaGame.exe links in all SDL code statically → zero DLL deps
  │
  └─ UNIX?  ──► SDL builds as shared (default), BUILD_SHARED_LIBS unchanged
                → RUNTIME_OUTPUT_DIRECTORY for SDL .so → build/runtimes/linux/
                → CerekaGame INSTALL_RPATH "$ORIGIN", BUILD_WITH_INSTALL_RPATH ON
                → CerekaGame finds .so files in its own directory at runtime

Launcher packaging (doPackage() in main.cpp)
  ├─ Windows ──► ZIP: CerekaGame.exe (static, no DLLs) + assets/ + game.cfg
  └─ Linux   ──► ZIP: CerekaGame + libSDL3.so.0 + libSDL3_image.so.0
                       + libSDL3_ttf.so.0 + libSDL3_mixer.so.0 + assets/ + game.cfg
                  (source: build/runtimes/linux/ — must be routed there by CMake)

CerekaLauncher distribution
  ├─ Windows ──► windeployqt --compiler-runtime → Qt6 DLLs + libc++/libunwind DLLs
  └─ Linux   ──► linuxdeploy + linuxdeploy-plugin-qt → CerekaLauncher.AppImage
```

### Recommended Project Structure
No new directories needed. All changes are within existing files:
```
vendor/CMakeLists.txt        — add WIN32 gate for BUILD_SHARED_LIBS OFF + SDL_SHARED/SDL_STATIC
runner/CMakeLists.txt        — add INSTALL_RPATH + BUILD_WITH_INSTALL_RPATH for Linux
launcher/CMakeLists.txt      — add --compiler-runtime to windeployqt args
launcher/main.cpp            — fix doPackage() to copy .so files for Linux
scripts/make-appimage.sh     — NEW: packaging script for Linux launcher AppImage
```

### Pattern 1: SDL3 Static on Windows via BUILD_SHARED_LIBS Gate

**What:** Setting `BUILD_SHARED_LIBS OFF`, `SDL_SHARED OFF`, `SDL_STATIC ON` before the SDL `add_subdirectory` calls causes SDL3 (and SDL3_image, SDL3_ttf, SDL3_mixer) to build only their static variants. Their `::SDL3_xxx` aliases automatically point to the static targets, so `target_link_libraries` in `vendor/CMakeLists.txt` requires no changes.

**When to use:** Win32 builds only. Linux keeps shared libraries to avoid glibc version coupling.

**Example:**
```cmake
# vendor/CMakeLists.txt — add before add_subdirectory(SDL)
if(WIN32)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED        OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC        ON  CACHE BOOL "" FORCE)
endif()

add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(SDL_mixer)

# Remove or guard the existing DLL-routing block:
# if(WIN32) foreach(_t SDL3-shared ...) → this block becomes dead code; remove it
```

**Verified:** Confirmed in SDL3 `CMakeLists.txt` lines 219–229 (vendor/SDL/CMakeLists.txt): when `BUILD_SHARED_LIBS=OFF`, `SDL_STATIC_DEFAULT` is ON and `SDL_SHARED_DEFAULT` is OFF. The `SDL3::SDL3` alias points to `SDL3-static` (line 501). The same aliasing pattern exists in `SDL_image` (line 291), `SDL_ttf` (line 150), and `SDL_mixer` (line 231). [VERIFIED: codebase grep vendor/SDL/CMakeLists.txt]

### Pattern 2: Linux RPATH `$ORIGIN`

**What:** Set `INSTALL_RPATH` to `"$ORIGIN"` and `BUILD_WITH_INSTALL_RPATH ON` so the binary finds `.so` files in its own directory at runtime — both during development (build dir) and after distribution (ZIP). No `LD_LIBRARY_PATH`, no wrapper script.

**When to use:** Linux CerekaGame only. macOS uses `@rpath` differently; Windows uses the binary's directory by default without RPATH.

**Example:**
```cmake
# runner/CMakeLists.txt — add inside the existing else() branch (Linux)
if(WIN32)
    set_target_properties(CerekaGame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/windows")
else()
    set_target_properties(CerekaGame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/linux"
        INSTALL_RPATH             "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH  ON)
endif()
```

**Note on escaping:** In CMake, `$ORIGIN` does NOT need escaping when passed as a string literal to `INSTALL_RPATH`. CMake does not expand `ORIGIN` as a CMake variable in this context. Using `\$ORIGIN` is also accepted and produces the same result. [VERIFIED: cmake.org INSTALL_RPATH docs, BUILD_WITH_INSTALL_RPATH docs] [CITED: https://cmake.org/cmake/help/latest/prop_tgt/INSTALL_RPATH.html]

### Pattern 3: Route SDL `.so` Files to `build/runtimes/linux/`

**What:** Mirror the existing Windows `RUNTIME_OUTPUT_DIRECTORY` DLL-routing pattern for Linux shared libraries. This puts `libSDL3.so.0` etc. alongside `CerekaGame` in the same directory, satisfying the `$ORIGIN` RPATH lookup and giving the packager a single source directory to copy from.

**Example:**
```cmake
# vendor/CMakeLists.txt — extend the existing WIN32 block or add a separate UNIX block
if(UNIX AND NOT APPLE)
    foreach(_t SDL3-shared SDL3_image-shared SDL3_ttf-shared SDL3_mixer-shared)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/linux")
        endif()
    endforeach()
endif()
```

**Important:** On Linux, shared libraries use `LIBRARY_OUTPUT_DIRECTORY`, not `RUNTIME_OUTPUT_DIRECTORY`. On Windows, DLLs use `RUNTIME_OUTPUT_DIRECTORY`. CMake distinguishes these. [VERIFIED: codebase — existing WIN32 block uses RUNTIME_OUTPUT_DIRECTORY for `.dll`; on Linux `.so` files are `LIBRARY` type, not `RUNTIME` type]

### Pattern 4: Fix `doPackage()` Linux Branch to Include SDL `.so` Files

**What:** The existing `doPackage()` in `main.cpp` (lines 858–862) sets executable permission for the Linux binary but does **not** copy any `.so` files. This must be fixed to copy the four SDL shared libraries from `runtimeDir("linux")` alongside the binary in the staging directory.

**Confirmed gap:** Lines 858–871 in `launcher/main.cpp` — the `if (plat.name == "linux")` branch only calls `fs::permissions(...)`. The `.dll` copy loop (`for (auto &e : fs::directory_iterator(runtimeDir("windows"), ec))`) has no Linux equivalent. [VERIFIED: codebase grep launcher/main.cpp]

**Example fix (conceptual — actual task will write the code):**
```cpp
if (plat.name == "linux") {
    fs::permissions(stagingDir / gameExe, /* exec bits */, fs::perm_options::add, ec);
    // Copy SDL .so files
    static const char* soLibs[] = {
        "libSDL3.so.0", "libSDL3_image.so.0",
        "libSDL3_ttf.so.0", "libSDL3_mixer.so.0"
    };
    for (auto* lib : soLibs) {
        fs::path src = runtimeDir("linux") / lib;
        if (fs::exists(src)) {
            fs::copy_file(src, stagingDir / lib,
                          fs::copy_options::overwrite_existing, ec);
            if (!ec) appendLog("Copied " + std::string(lib));
        }
    }
}
```

### Pattern 5: windeployqt `--compiler-runtime` Flag

**What:** Adding `--compiler-runtime` to the existing `windeployqt` POST_BUILD command instructs it to include the compiler's runtime DLLs (libwinpthread, libgcc_s_seh / libc++, libunwind depending on toolchain) alongside the Qt DLLs.

**Example:**
```cmake
# launcher/CMakeLists.txt
add_custom_command(TARGET CerekaLauncher POST_BUILD
    COMMAND Qt6::windeployqt
    ARGS --compiler-runtime $<TARGET_FILE:CerekaLauncher>
    COMMENT "Bundling Qt6 DLLs + compiler runtime for distribution"
)
```

**UCRT note:** The project uses llvm-mingw/UCRT64 toolchain. UCRT (`ucrtbase.dll`) is pre-installed on Windows 10+ and does not need bundling. The missing DLLs are the **C++ standard library** and **unwinder** from llvm-mingw: `libc++.dll` and `libunwind-1.dll`. The `--compiler-runtime` flag should capture these when windeployqt can find the compiler. [VERIFIED: mstorsjo/llvm-mingw README — UCRT is pre-installed on Win10+; llvm-mingw wraps clang with `-stdlib=libc++ -unwindlib=libunwind`] [CITED: https://github.com/mstorsjo/llvm-mingw]

**Fallback if `--compiler-runtime` insufficient:** Explicitly copy `libc++.dll` and `libunwind-1.dll` from the llvm-mingw toolchain sysroot via a second `add_custom_command` or `file(COPY ...)` step. The sysroot path is available from `CMAKE_SYSROOT` when the ucrt64 toolchain is active. [ASSUMED: --compiler-runtime reliably finds runtime DLLs when PATH includes the toolchain bin]

### Pattern 6: AppImage Build Script for Linux Launcher

**What:** A shell script (not a CMake target) that downloads linuxdeploy + its Qt plugin and builds the AppImage. Run manually when distributing the launcher — not wired into the default build to avoid requiring linuxdeploy on every developer machine.

**Required artifacts:**
- A `.desktop` file for `CerekaLauncher` (AppImage requirement)
- An icon file (PNG, 256x256 preferred)
- The built `CerekaLauncher` binary

**Example script (`scripts/make-appimage.sh`):**
```bash
#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
QMAKE="${2:-$(which qmake6 || which qmake)}"

# Download tools if not present
[ -f linuxdeploy-x86_64.AppImage ] || \
    wget -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
[ -f linuxdeploy-plugin-qt-x86_64.AppImage ] || \
    wget -c "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

mkdir -p AppDir/usr/bin AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/256x256/apps

cp "$BUILD_DIR/CerekaLauncher" AppDir/usr/bin/
cp launcher/cereka-launcher.desktop AppDir/usr/share/applications/
cp launcher/cereka-launcher.png AppDir/usr/share/icons/hicolor/256x256/apps/

QMAKE="$QMAKE" ./linuxdeploy-x86_64.AppImage \
    --appdir AppDir \
    --plugin qt \
    --output appimage
```

**Required new files:** `launcher/cereka-launcher.desktop` (standard XDG .desktop format) and `launcher/cereka-launcher.png` (icon). [CITED: https://docs.appimage.org/packaging-guide/from-source/native-binaries.html]

### Anti-Patterns to Avoid

- **Setting `BUILD_SHARED_LIBS OFF` globally without a `WIN32` guard:** This would break Linux builds. Static SDL on Linux causes glibc version coupling — a statically-linked binary built against glibc 2.35 may crash on an older distro. Always gate `BUILD_SHARED_LIBS OFF` behind `if(WIN32)`.
- **Using `\${ORIGIN}` when not necessary:** Some sources escape it; CMake handles `$ORIGIN` as a literal string in `INSTALL_RPATH` without escaping. Both work; pick one and be consistent.
- **Relying on `RUNTIME_OUTPUT_DIRECTORY` for Linux `.so` files:** On Linux, shared libraries are `LIBRARY` type, not `RUNTIME` type. Use `LIBRARY_OUTPUT_DIRECTORY`. Using `RUNTIME_OUTPUT_DIRECTORY` on Linux for `.so` files has no effect.
- **Hardcoding SDL `.so` filenames without the `.0` version suffix:** The actual symlink players run against is `libSDL3.so.0` (soname version), not `libSDL3.so` (development symlink). The packager should look for the `.so.0` file or copy all `.so*` symlinks.
- **Wiring AppImage creation into the default CMake build:** `linuxdeploy` is a packaging-time tool, not a build-time tool. Requiring it on every dev machine creates unnecessary friction. A manual script is the right pattern.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Discovering Qt DLLs for Windows distribution | Manual DLL enumeration | `windeployqt --compiler-runtime` | windeployqt knows Qt's plugin architecture; manual lists miss platform plugins, image format plugins, TLS backends |
| Bundling Qt6 + its plugins into Linux AppImage | Custom `ldd` + copy script | `linuxdeploy` + `linuxdeploy-plugin-qt` | Qt has ~40 plugins across imageformats, platforms, tls, xcb, etc.; the plugin handles all of them including QML/translations |
| SDL Windows system library list | Manual `-lwinmm -limm32 -lversion ...` | CMake static link via `SDL3::SDL3` target | SDL3's CMake target (`SDL3-static`) carries `INTERFACE_LINK_LIBRARIES` with `kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32` — these propagate automatically |

**Key insight:** The SDL3 static target propagates all its Windows system library dependencies via CMake `INTERFACE_LINK_LIBRARIES`. Consumers linking `SDL3::SDL3` (which aliases to `SDL3-static` when static) automatically get all required `-l` flags. No manual `-lwinmm` etc. needed. [VERIFIED: vendor/SDL/CMakeLists.txt line 2359: `sdl_link_dependency(base LIBS kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32)`]

---

## Common Pitfalls

### Pitfall 1: SDL `.so` Files Not in `build/runtimes/linux/`

**What goes wrong:** The `doPackage()` Linux packager looks for SDL `.so` files in `runtimeDir("linux")` which is `<launcher-dir>/runtimes/linux/`. But SDL shared libraries currently land in `build/vendor/SDL/`, `build/vendor/SDL_image/`, etc. because only Windows DLLs have `RUNTIME_OUTPUT_DIRECTORY` set. The packager silently skips missing `.so` files or packages a binary that can't find its libraries at runtime.

**Why it happens:** The `vendor/CMakeLists.txt` DLL-routing block is `if(WIN32)` only (line 24–29). Linux SDL `.so` files go to their default CMake output location.

**How to avoid:** Add a `LIBRARY_OUTPUT_DIRECTORY` routing block for Linux (mirroring the Windows block but using `LIBRARY_OUTPUT_DIRECTORY` and targeting `SDL3-shared`, `SDL3_image-shared`, `SDL3_ttf-shared`, `SDL3_mixer-shared`).

**Warning signs:** Packager log shows "Runtime: .../runtimes/linux/CerekaGame" (found) but no "Copied libSDL3.so.0" lines.

### Pitfall 2: `BUILD_SHARED_LIBS OFF` Breaks Linux SDL Build

**What goes wrong:** If `BUILD_SHARED_LIBS OFF` is set without a `WIN32` guard, Linux builds produce static SDL. CerekaGame links fine, but the binary is harder to distribute across distros (glibc ABI coupling), and the Linux packaging path (which expects `.so` files) breaks.

**Why it happens:** `BUILD_SHARED_LIBS` is a global CMake variable; if set unconditionally before `add_subdirectory`, it affects all platforms.

**How to avoid:** Always wrap `BUILD_SHARED_LIBS OFF` in `if(WIN32)`. [VERIFIED: CONTEXT.md note — Linux should stay shared to avoid glibc coupling]

### Pitfall 3: SDL3 Cache Poison on Re-configure

**What goes wrong:** On first configure with `BUILD_SHARED_LIBS=OFF`, CMake writes `SDL_SHARED=OFF` and `SDL_STATIC=ON` into `CMakeCache.txt`. If the cache is not cleared before re-configuring for Linux (without `BUILD_SHARED_LIBS=OFF`), the cached values persist and SDL continues building static even on Linux.

**Why it happens:** SDL3 uses `cmake_dependent_option` which writes its result to cache. A cached `SDL_SHARED=OFF` overrides the default on subsequent configures even if `BUILD_SHARED_LIBS` is no longer set.

**How to avoid:** Use separate build directories per platform (`build/` for Linux, `build-windows/` for cross-compile). Document this in the build instructions.

### Pitfall 4: `--compiler-runtime` Requires Toolchain in PATH at Build Time

**What goes wrong:** `windeployqt --compiler-runtime` finds the compiler's runtime DLLs by locating the compiler binary (e.g., `clang++.exe`) in `PATH`. In CI environments or when Qt was installed to a non-standard location, the MinGW/UCRT toolchain bin directory may not be in PATH, causing windeployqt to skip runtime DLLs silently.

**Why it happens:** windeployqt locates g++/clang++ via PATH to identify which runtime DLLs to deploy. [CITED: https://github.com/bow-simulation/virtualbow/issues/172]

**How to avoid:** If building natively on Windows with llvm-mingw installed, ensure the toolchain `bin/` directory is in `PATH` when CMake runs the POST_BUILD step. As a fallback, add an explicit `file(COPY ...)` step to copy `libc++.dll` and `libunwind-1.dll` from `CMAKE_SYSROOT` when `WIN32` and the ucrt64 toolchain is active.

### Pitfall 5: AppImage Requires a `.desktop` File and Icon

**What goes wrong:** `linuxdeploy` refuses to create an AppImage if no `.desktop` file is provided. The `.desktop` file must reference the binary name and a valid icon. Missing either causes a hard failure.

**Why it happens:** AppImage spec requires desktop integration metadata.

**How to avoid:** Create `launcher/cereka-launcher.desktop` and a 256x256 PNG icon as part of this phase. These are new tracked files. [CITED: https://docs.appimage.org/packaging-guide/from-source/native-binaries.html]

---

## Code Examples

### SDL3 Static on Windows — Minimal CMake
```cmake
# vendor/CMakeLists.txt (before add_subdirectory calls)
# Source: vendor/SDL/CMakeLists.txt lines 219-229 (verified behavior)
if(WIN32)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED        OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC        ON  CACHE BOOL "" FORCE)
endif()
```

### Linux RPATH
```cmake
# runner/CMakeLists.txt
# Source: cmake.org/cmake/help/latest/prop_tgt/INSTALL_RPATH.html
set_target_properties(CerekaGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_BINARY_DIR}/runtimes/linux"
    INSTALL_RPATH             "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH  ON)
```

### Linux SDL `.so` Routing
```cmake
# vendor/CMakeLists.txt
if(UNIX AND NOT APPLE)
    foreach(_t SDL3-shared SDL3_image-shared SDL3_ttf-shared SDL3_mixer-shared)
        if(TARGET ${_t})
            set_target_properties(${_t} PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/linux")
        endif()
    endforeach()
endif()
```

### windeployqt with Runtime DLLs
```cmake
# launcher/CMakeLists.txt
add_custom_command(TARGET CerekaLauncher POST_BUILD
    COMMAND Qt6::windeployqt
    ARGS --compiler-runtime $<TARGET_FILE:CerekaLauncher>
    COMMENT "Bundling Qt6 DLLs + compiler runtime for distribution"
)
```

### .desktop File for AppImage
```ini
# launcher/cereka-launcher.desktop
[Desktop Entry]
Type=Application
Name=Cereka Launcher
Exec=CerekaLauncher
Icon=cereka-launcher
Categories=Development;Game;
Comment=Visual Novel Engine — project manager and packager
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `linuxdeployqt` for Qt Linux distribution | `linuxdeploy` + `linuxdeploy-plugin-qt` | ~2019 | `linuxdeployqt` unmaintained, Qt6-incompatible; linuxdeploy is the maintained successor |
| Manual Windows DLL list | `windeployqt` | SDL2 era | windeployqt handles Qt plugin architecture; manual lists miss transient plugins |
| `LD_LIBRARY_PATH` wrapper script for Linux | `$ORIGIN` RPATH | Long-established CMake practice | Binary finds libs in its own directory; no script needed |

**Deprecated/outdated:**
- `linuxdeployqt`: not updated for Qt6; use `linuxdeploy` + `linuxdeploy-plugin-qt` instead
- Bundling `ucrtbase.dll`: UCRT is pre-installed on Windows 10+; do not bundle it

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `windeployqt --compiler-runtime` reliably finds and copies `libc++.dll` + `libunwind-1.dll` when llvm-mingw toolchain is in PATH during build | Pattern 5 | Launcher fails on fresh Windows — need explicit DLL copy fallback |
| A2 | Existing `doPackage()` source path `runtimeDir("linux")` equals `build/runtimes/linux/` (based on `selfExeDir()/runtimes/linux`) | Pattern 4 | SDL `.so` copy path incorrect; packaging silently skips libs |
| A3 | SDL `.so` soname on Linux is `libSDL3.so.0` (not `libSDL3.so.0.3.0`) for the player-facing symlink | Pattern 4 | Package contains wrong filename; dynamic linker can't find the library |

---

## Open Questions

1. **Where does the launcher binary live relative to `runtimes/`?**
   - What we know: `selfExeDir()` returns the launcher's directory. On Linux, the launcher is at `build/CerekaLauncher`. `runtimeDir("linux")` would then be `build/runtimes/linux/` — which is where SDL `.so` files will land after the routing fix.
   - What's unclear: When the launcher is distributed as an AppImage, `selfExeDir()` resolves to the AppImage's mount point. The packager runs from the installed launcher, not the build tree. The packager's "find SDL .so files" assumption (`runtimeDir("linux")`) breaks in the AppImage context because the AppImage does not contain the SDK's runtimes directory.
   - Recommendation: The packager is intended to be run from within the build tree, not from a distributed AppImage. Document this constraint. For an AppImage-resident packager, the runtime path would need a different discovery mechanism. Out of scope for this phase.

2. **Does `--compiler-runtime` work with the cross-compile scenario (Linux→Windows via ucrt64.cmake)?**
   - What we know: Cross-compilation produces `CerekaLauncher.exe` on Linux. `windeployqt` is a Windows-native tool that runs on Windows. It cannot run during a Linux cross-compile build.
   - What's unclear: Whether the windeployqt POST_BUILD step is skipped automatically when cross-compiling, or whether it errors.
   - Recommendation: The `windeployqt` POST_BUILD step should be guarded so it only runs on native Windows builds. Cross-compile workflow for the launcher is likely out of scope for this phase (the launcher is a dev tool; devs on Linux would use the AppImage, devs on Windows build natively).

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Build system | Yes | 4.3.2 (project min 3.24) | — |
| Ninja | Build | Yes | 1.13.2 | Make |
| zip | Linux packaging in `doPackage()` | Yes | /usr/bin/zip | Already guarded with error in code |
| tar | Linux archive creation | Yes | /usr/bin/tar | — |
| linuxdeploy | AppImage creation | No (dev machine is macOS) | — | Download at packaging time on Linux |
| linuxdeploy-plugin-qt | AppImage Qt bundling | No | — | Download at packaging time on Linux |
| windeployqt | Windows Qt deployment | No (this is macOS) | — | Available on Windows build machine |
| Qt6 | Launcher | Yes (macOS only) | 6.8.3, 6.11.0 at ~/Qt | Available on target platforms separately |

**Missing dependencies with no fallback on this machine:**
- `linuxdeploy`, `linuxdeploy-plugin-qt`: macOS dev machine. These are Linux-only packaging tools. Tasks involving AppImage creation must be verified on a Linux machine or CI.
- `windeployqt`: macOS dev machine. Tasks involving windeployqt changes must be verified on Windows.

**Missing dependencies with fallback:**
- None that block the CMake/C++ changes (vendor/CMakeLists.txt, runner/CMakeLists.txt, launcher/CMakeLists.txt, launcher/main.cpp).

---

## Validation Architecture

No automated test infrastructure applies to CMake configuration changes or binary packaging. Validation is smoke-test based:

### Phase Requirements → Test Map
| Behavior | Test Type | Command | Notes |
|----------|-----------|---------|-------|
| Windows CerekaGame.exe has zero DLL dependencies | Manual smoke test | Run on a fresh Windows VM; check with `dumpbin /dependents CerekaGame.exe` | Cannot automate on macOS dev machine |
| Linux CerekaGame finds SDL `.so` from its own dir | Manual smoke test | Move binary to temp dir with `.so` files, run `ldd` and execute | Run on Linux |
| Linux CerekaGame package ZIP extracts and runs | Manual smoke test | Unzip into temp dir, run `./CerekaGame` | Run on Linux |
| Windows launcher ZIP runs on fresh machine | Manual smoke test | Test on machine without Qt6 installed | Run on Windows VM |
| AppImage runs on stock Ubuntu LTS | Manual smoke test | `chmod +x CerekaLauncher.AppImage && ./CerekaLauncher.AppImage` | Run on target distro |

### Wave 0 Gaps
No new test files needed. This phase has no unit-testable logic — it is entirely build system and packaging configuration.

---

## Security Domain

This phase has no user-facing authentication, network communication, or data handling. The only security-relevant consideration is DLL loading:

- Static linking eliminates DLL hijacking attack surface for CerekaGame on Windows (no DLLs to hijack in the game bundle)
- `$ORIGIN` RPATH on Linux prevents loading attacker-controlled `.so` files from arbitrary `LD_LIBRARY_PATH` entries (RPATH takes precedence over `LD_LIBRARY_PATH`)
- AppImage uses its own isolated environment; the launcher accesses the filesystem via standard Qt APIs

No ASVS categories apply — this is a build/packaging phase with no runtime security logic.

---

## Sources

### Primary (HIGH confidence)
- `vendor/SDL/CMakeLists.txt` lines 193–232, 392–393, 461–501 — SDL3 `BUILD_SHARED_LIBS` handling, `SDL3::SDL3` alias logic [VERIFIED: codebase grep]
- `vendor/SDL/CMakeLists.txt` line 2359 — Windows system library deps for SDL3 static [VERIFIED: codebase grep]
- `vendor/SDL_image/CMakeLists.txt` lines 237, 291 — SDL3_image static target aliasing [VERIFIED: codebase grep]
- `vendor/SDL_ttf/CMakeLists.txt` lines 117–150 — SDL3_ttf static target aliasing [VERIFIED: codebase grep]
- `vendor/SDL_mixer/CMakeLists.txt` lines 184–231 — SDL3_mixer static target aliasing [VERIFIED: codebase grep]
- `launcher/main.cpp` lines 858–871 — existing `doPackage()` Linux branch confirms missing `.so` copy [VERIFIED: codebase grep]
- `vendor/CMakeLists.txt` — confirms `RUNTIME_OUTPUT_DIRECTORY` is WIN32-only, no Linux `.so` routing [VERIFIED: codebase grep]
- [cmake.org INSTALL_RPATH docs](https://cmake.org/cmake/help/latest/prop_tgt/INSTALL_RPATH.html) — RPATH syntax
- [cmake.org BUILD_RPATH_USE_ORIGIN](https://cmake.org/cmake/help/latest/prop_tgt/BUILD_RPATH_USE_ORIGIN.html) — BUILD_WITH_INSTALL_RPATH behavior

### Secondary (MEDIUM confidence)
- [SDL3 README-cmake wiki](https://wiki.libsdl.org/SDL3/README-cmake) — SDL_STATIC/SDL_SHARED options confirmed, SDL3::SDL3 alias guarantee [CITED]
- [linuxdeploy-plugin-qt GitHub](https://github.com/linuxdeploy/linuxdeploy-plugin-qt) — `--plugin qt` flag, `$QMAKE` env var [CITED]
- [AppImage packaging guide](https://docs.appimage.org/packaging-guide/from-source/native-binaries.html) — AppDir structure, `.desktop` file requirement [CITED]
- [mstorsjo/llvm-mingw](https://github.com/mstorsjo/llvm-mingw) — UCRT pre-installed on Windows 10+; llvm-mingw uses `libc++`/`libunwind` [CITED]
- [virtualbow issue #172](https://github.com/bow-simulation/virtualbow/issues/172) — windeployqt misses MinGW runtime DLLs in CI; `--compiler-runtime` flag behavior

### Tertiary (LOW confidence)
- WebSearch result re: `windeployqt --compiler-runtime` needing compiler in PATH — not directly verified with Qt docs

---

## Metadata

**Confidence breakdown:**
- SDL3 static linking CMake mechanics: HIGH — verified directly in vendor source
- Linux RPATH pattern: HIGH — verified via cmake.org docs and existing code structure
- `doPackage()` gap (missing `.so` copy): HIGH — verified by reading launcher/main.cpp
- SDL `.so` routing gap (no LIBRARY_OUTPUT_DIRECTORY for Linux): HIGH — verified by reading vendor/CMakeLists.txt
- windeployqt `--compiler-runtime` reliability: MEDIUM — behavior confirmed conceptually but not tested on this machine
- AppImage workflow: MEDIUM — linuxdeploy-plugin-qt docs verified; not runnable on macOS dev machine

**Research date:** 2026-05-07
**Valid until:** 2026-08-07 (SDL3/linuxdeploy are stable; CMake RPATH is long-stable)
