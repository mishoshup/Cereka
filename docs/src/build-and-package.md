# Build & Package

This guide explains how to build a Cereka game for distribution.

## Building from Source

### Prerequisites

- C++ compiler (GCC, Clang, or MSVC)
- CMake 3.20+
- Ninja build system
- SDL3 development libraries
- Qt6 development libraries (for Launcher only)

### Clone and Build

```bash
git clone --recursive https://github.com/mishoshup/Cereka
cd Cereka
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build -j$(nproc)
```

### Build Outputs

```
build/
├── runtimes/
│   ├── linux/
│   │   └── CerekaGame        # Linux engine binary
│   └── windows/
│       └── CerekaGame.exe    # Windows engine binary
├── launcher/
│   └── CerekaLauncher        # Project manager GUI
└── tests/
    └── cereka_test           # Test suite
```

---

## Creating a Distributable Game

### Using CerekaLauncher

1. Open CerekaLauncher
2. Click "New Project" or "Open Project" to select your game folder
3. Click "Build & Package"
4. Choose target platform (Linux or Windows)

CerekaLauncher creates a self-contained ZIP archive in the output directory.

### Manual Packaging

To create a distributable game manually:

**Linux:**

```bash
# Create package directory
mkdir my-game-linux
cp CerekaGame my-game-linux/
cp -r /path/to/SDL3/lib/*.so* my-game-linux/

# Copy project assets
cp -r my-game/* my-game-linux/

# Create ZIP
cd my-game-linux
zip -r ../my-game-linux.zip .
```

**Windows:**

```bash
# Create package directory
mkdir my-game-windows
copy CerekaGame.exe my-game-windows\

# Copy project assets
xcopy /E my-game my-game-windows\

# Create ZIP
powershell Compress-Archive -Path my-game-windows -DestinationPath my-game-windows.zip
```

---

## Running a Game

### Direct Execution

```bash
./CerekaGame /path/to/game/project/
```

The game project directory must contain:
- `game.cfg` — project configuration
- `assets/` — game assets

### Using CerekaLauncher

CerekaLauncher provides a graphical interface for:
- Creating new projects
- Opening existing projects
- Running games during development
- Packaging games for distribution

---

## Cross-Compilation

Cereka supports Linux → Windows cross-compilation using the llvm-mingw toolchain:

```bash
cmake -S . -B build-win -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ucrt64.cmake
```

This produces a Windows `CerekaGame.exe` that runs on Windows systems.

---

## Test Suite

```bash
# Build tests
ninja -C build cereka_test

# Run C++ unit tests
./build/tests/cereka_test

# Run Lua compile snapshot tests
lua tests/compile/harness.lua
```

Compile snapshot tests compare compiler output against expected results. After intentional compiler changes, update snapshots:

```bash
lua tests/compile/harness.lua --update
```
