---
name: build
description: Build Cereka (engine + runner + launcher + tests). Cross-platform — works on Linux and Windows native. Use when the user says "build", "compile", "ninja", or after edits that need verification.
allowed-tools: Bash
---

# Build Cereka

This project is developed on **both Linux and Windows** — pick the right path.

## Linux native
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug    # only if build/ missing
ninja -C build -j12
```
Outputs: `build/runtimes/linux/CerekaGame`, `build/launcher/CerekaLauncher`, `build/tests/cereka_test`.

## Windows native (MSYS2 UCRT64 / native ninja)
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build -j12
```
Outputs: `build/runtimes/windows/CerekaGame.exe`, `build/launcher/CerekaLauncher.exe`, `build/tests/cereka_test.exe`. Vendor DLLs land next to the exes.

## Linux → Windows cross-compile (llvm-mingw)
```bash
cmake -S . -B build-win -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ucrt64.cmake \
  -DCMAKE_BUILD_TYPE=Release
ninja -C build-win -j12
```

## Detecting platform
- `uname -s` → `Linux` / `MINGW*` / `MSYS*` for Windows under MSYS, or check `$OS == Windows_NT`.
- If `build/` already exists, prefer the existing configuration — don't re-run `cmake` unless something changed.

## Common gotchas
- `src/CMakeLists.txt` uses `file(GLOB_RECURSE)` — **re-run cmake** after adding or removing `.cpp` files.
- `scripts/compiler.lua` is embedded at build time via `compiler_lua_embed.hpp`; edits trigger a rebuild.
- Empty `vendor/<name>` dir → `git submodule update --init --recursive vendor/<name>`.
- On Windows, `tests/main.cpp` uses a `wmain` branch + `-municode` linker flag. Cross-platform main is required (already in place).
- Never pass `--no-verify`, `--no-gpg-sign`, or skip hooks unless the user asks.

Report only the failing tail of output — not the whole log.
