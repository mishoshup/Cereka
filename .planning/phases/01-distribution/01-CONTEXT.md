# Phase 1 — Distribution & Packaging: Context

**Date:** 2026-05-07
**Status:** Ready for planning

---

<domain>
Cereka is a game engine. Game devs write `.crka` scripts, build with CerekaLauncher, and ship a ZIP to players. The ZIP must be completely self-contained — players unzip and run, no installs, no DLL hunting. CerekaLauncher itself must also run on a fresh machine without Qt6 pre-installed.

Two audiences:
- **Players** — download a ZIP of a game built with Cereka; must "just work"
- **Game devs** — use CerekaLauncher to build and package; launcher must run on fresh machine
</domain>

---

<decisions>

## Windows — CerekaGame (player distribution)

**Decision: Static link all SDL3 libraries**
- Build SDL3, SDL3_image, SDL3_ttf, SDL3_mixer as static libraries on Windows
- `BUILD_SHARED_LIBS OFF` forced before SDL subdirectories in `vendor/CMakeLists.txt`
- Result: single `CerekaGame.exe` with zero DLL dependencies
- Launcher packager ZIPs: `CerekaGame.exe` + `assets/` + `game.cfg` only
- Remove the `RUNTIME_OUTPUT_DIRECTORY` DLL-routing loop (dead code after this change)

**Rationale:** SDL3, SDL3_image, SDL3_ttf, SDL3_mixer are all zlib licensed — static linking is legally clean. Binary grows ~5-10MB which is negligible compared to game assets. Eliminates the entire class of DLL hell problems for players.

---

## Linux — CerekaGame (player distribution)

**Decision: `$ORIGIN` RPATH — no wrapper script**
- Set `INSTALL_RPATH "$ORIGIN"` + `BUILD_WITH_INSTALL_RPATH ON` on `CerekaGame` target in `runner/CMakeLists.txt`
- Linux dynamic linker resolves `.so` files from the binary's own directory
- Player runs `./CerekaGame` directly — no shell script, no `LD_LIBRARY_PATH` export
- Launcher packager copies the SDL `.so` files from `build/runtimes/linux/` alongside the binary

**ZIP layout:**
```
game_name/
├── CerekaGame
├── libSDL3.so.0
├── libSDL3_image.so.0
├── libSDL3_ttf.so.0
├── libSDL3_mixer.so.0
├── assets/
└── game.cfg
```

**Rationale:** Standard approach used by Godot exports, most Steam Linux titles. Professional and clean — user just runs the binary. `run.sh` wrappers are not acceptable for an enterprise-grade engine.

**Note:** Keep SDL3* as shared libs on Linux (static linking on Linux has glibc version coupling issues across distros). The `$ORIGIN` RPATH is the correct Linux answer.

---

## Windows — CerekaLauncher (dev tool distribution)

**Decision: Fix windeployqt to capture full runtime**
- `windeployqt` is already wired as a POST_BUILD step in `launcher/CMakeLists.txt`
- Gap: UCRT/MinGW C++ runtime DLLs not included — causes failure on fresh Windows VMs
- Fix: add `CMAKE_INSTALL_UCRT_LIBRARIES` logic or explicitly copy MinGW runtime DLLs alongside the launcher
- Launcher ships as a directory with `CerekaLauncher.exe` + all Qt6 DLLs + runtime DLLs

---

## Linux — CerekaLauncher (dev tool distribution)

**Decision: AppImage**
- Package CerekaLauncher as an AppImage so devs can run it on any Linux distro without Qt6 installed
- Single `.AppImage` file, `chmod +x`, run — no install needed
- Use `linuxdeploy` + `linuxdeploy-plugin-qt` in the build/package step
- AppImage bundles Qt6 Widgets and all transitive dependencies

**Rationale:** AppImage is the standard for distributing Qt apps on Linux to users without system Qt. This is what KDE apps, kdenlive, and similar projects use for portable distribution.

---

## Launcher Packager (`project_manager.cpp`)

**Decision: Platform-aware ZIP production**
- Windows package: ZIP contains `CerekaGame.exe` (statically linked, no DLLs) + `assets/` + `game.cfg`
- Linux package: ZIP contains `CerekaGame` (with RPATH set) + SDL `.so` files + `assets/` + `game.cfg`
- Launcher must detect current platform and apply the correct file list
- SDL `.so` source paths: `build/runtimes/linux/` (already routed there via `RUNTIME_OUTPUT_DIRECTORY`)

</decisions>

---

<canonical_refs>
- `vendor/CMakeLists.txt` — SDL subdirectory setup + DLL routing (modify for static Windows build)
- `runner/CMakeLists.txt` — CerekaGame target (add RPATH for Linux)
- `launcher/CMakeLists.txt` — windeployqt POST_BUILD + AppImage step (fix runtime DLL gap)
- `launcher/project_manager.cpp` — packaging logic (update file list per platform)
- `.planning/codebase/STACK.md` — full dependency list and current shared/static status
- `.planning/codebase/CONCERNS.md` — platform fragility notes
</canonical_refs>

---

<deferred>
- Flatpak / Snap packaging — a future distribution phase after AppImage is stable
- Windows installer (NSIS/WiX) — not needed at this stage; ZIP is sufficient
- macOS support — not in scope yet
- Code signing / notarization — future phase
</deferred>
