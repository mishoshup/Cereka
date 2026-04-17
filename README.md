# Cereka

A visual novel engine built in C++ with SDL3. Game projects are purely script-based — no C++ required. Write `.crka` scripts, run your game.

---

## Download

Pre-built binaries are on the [Releases](https://github.com/mishoshup/cereka/releases) page.

| Archive | Platform |
|---|---|
| `cereka-vX.X.X-linux.tar.gz` | Linux — launcher + both runtimes |
| `cereka-vX.X.X-windows.zip` | Windows — launcher + Windows runtime |

The Linux archive includes both the Linux and Windows game runners, so you can package your game for both platforms from a single Linux machine.

---

## Getting started (pre-built)

**Linux**
```bash
tar xzf cereka-*-linux.tar.gz
cd cereka-linux
./CerekaLauncher
```

**Windows** — unzip `cereka-*-windows.zip`, run `CerekaLauncher.exe`.

---

## Building from source

### Prerequisites

SDL3, sol2, Lua, and ImGui are all vendored. The only external requirement is **Qt6** (used by the launcher) — install it once via [aqtinstall](https://github.com/miurahr/aqtinstall) and CMake will find it automatically.

#### 1. Compiler, CMake, Ninja

**Linux**
```
g++ or clang++ (C++17+)   cmake (3.24+)   ninja   git
```
On Arch: `sudo pacman -S base-devel cmake ninja`  
On Ubuntu/Debian: `sudo apt install build-essential cmake ninja-build`

**Windows (native)** — install [MSYS2](https://www.msys2.org/), then from a UCRT64 shell:
```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

#### 2. Qt6

**Linux**
```bash
# Install pipx if needed: sudo pacman -S python-pipx  (Arch)
#                          sudo apt install pipx       (Ubuntu/Debian)
pipx install aqtinstall
aqt install-qt linux desktop 6.8.3 linux_gcc_64 -O ~/Qt
```

**Windows** — from a regular Command Prompt or PowerShell:
```bat
pip install aqtinstall
aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O C:\Qt
```

CMake defaults to these install paths. If you installed Qt elsewhere, pass `-DCMAKE_PREFIX_PATH=/your/qt/path` when configuring.

### Build

```bash
git clone --recursive https://github.com/mishoshup/cereka.git
cd cereka
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Produces in `build/`:
| Output | Location |
|---|---|
| `CerekaLauncher` | `build/` |
| `CerekaGame` | `build/runtimes/linux/` |
| Windows DLLs | `build/runtimes/windows/` (if cross-compiling) |

### Cross-compile Windows from Linux

Requires [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) with UCRT64 support.

```bash
cmake -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ucrt64.cmake
cmake --build build-win

# Copy Windows runtime alongside the Linux launcher
cp -r build-win/runtimes/windows build/runtimes/
```

### Release archives

```bash
./release.sh v0.0.2
```

Builds both platforms, creates `cereka-v0.0.2-linux.tar.gz` and `cereka-v0.0.2-windows.zip`, then optionally publishes to GitHub via `gh`.

---

## The launcher

Run `CerekaLauncher` (or `CerekaLauncher.exe` on Windows). No arguments needed.

- **Open** an existing game folder or pick from recent projects
- **Create Game** — scaffolds a complete starter project with a tutorial script
- **Edit title** — click the title field in the project header and type a new name; saves to `game.cfg` automatically
- **Rename folder** — rename the project directory on disk from inside the launcher
- **Launch Game** — runs your game
- **Package** — produces a distributable archive. Click the `▼` arrow to choose a target platform:
  - *All platforms* — one archive per available runtime
  - *Linux only* — `.tar.gz` with `launch.sh`
  - *Windows only* — `.zip` with `launch.bat`

The packaged archive contains the game runner, all required DLLs (Windows), your project files (saves/ excluded), and a launch script. Share it and players need nothing else installed.

**Scaffolded project layout:**
```
assets/scripts/main.crka       — full tutorial script (every command with comments)
assets/scripts/ui.crka         — starter UI theme
assets/scripts/scene_two.crka  — example subroutine scene
assets/bg/placeholder_bg.png
assets/characters/placeholder_char.png
assets/sounds/placeholder_{bgm,sfx}.wav
assets/fonts/Montserrat-Medium.ttf
game.cfg
```

---

## Project layout

```
my-game/
  game.cfg
  assets/
    scripts/    — .crka script files
    bg/         — background images
    characters/ — character sprites
    sounds/     — bgm + sfx
    fonts/      — .ttf / .otf
    ui/         — optional UI images (textbox, buttons…)
  saves/        — auto-created at runtime
```

**game.cfg**
```ini
title      = My Game
width      = 1280
height     = 720
fullscreen = false
entry      = assets/scripts/main.crka
```

Run without the launcher:
```bash
./build/runtimes/linux/CerekaGame /path/to/my-game
```

---

## Script reference (.crka)

```
; comment

; ---------- scene ----------
bg filename.jpg
bg filename.jpg fade 0.5        ; dissolve over N seconds

char id left   sprite.png       ; position: left | center | right
char id center sprite.png
char id right  sprite.png
hide char id

; ---------- dialogue ----------
say CharacterId "Text here."
narrate "Narration text."

; ---------- flow ----------
label scene_name
jump scene_name
end

include other.crka              ; compile-time inline
call scene.crka                 ; runtime subroutine (returns on end)

; ---------- menus ----------
menu
    bg background.jpg           ; optional background swap inside menu
    button "Option A" goto label_a
    button "Option B" goto label_b
    button "Exit"     exit

; ---------- variables ----------
set flag_name value

; ---------- conditionals ----------
if flag_name == value
    say Alice "This runs if flag matches."
endif

if flag_name != value
    say Alice "This runs if it doesn't."
endif

; ---------- audio ----------
bgm music.ogg
sfx sound.wav
stop_bgm

; ---------- save / load ----------
save_menu                       ; show 10-slot save overlay (ESC cancels)
load_menu                       ; show 10-slot load overlay
save 1                          ; silent save to slot N (1-10)
load 1                          ; silent load from slot N

; ---------- UI theming ----------
ui textbox
    color 0 0 0 160             ; r g b a
    y 75%                       ; % = screen-relative, plain number = pixels
    h 25%
    text_margin_x 80
    text_color 255 255 255 255
    image assets/ui/textbox.png ; overrides solid color if set

ui namebox
    color 30 30 100 255
    x 50   y_offset -65   w 260   h 52
    text_color 255 220 120 255

ui button
    color 20 80 120 255
    w 560   h 72
    text_color 255 255 255 255
    image assets/ui/button.png
    hover_image assets/ui/button_hover.png

ui font
    size 36
```

All UI properties have defaults — only override what you need. Put `include ui.crka` at the top of your entry script.

### Multi-file scripts

| Command | When | Use for |
|---|---|---|
| `include file.crka` | Compile time | UI themes, shared label banks |
| `call file.crka` | Runtime | Self-contained scenes, returns on `end` |

Paths are relative to the including file. Max depth: 32.

---

## Engine source layout

```
cereka/
  src/         — Cereka engine library (C++)
  runner/      — CerekaGame executable
  launcher/    — CerekaLauncher (Qt6 project manager)
  scripts/     — compiler.lua (.crka → bytecode, embedded at build time)
  vendor/      — SDL3, SDL3_ttf, SDL3_mixer, SDL3_image, sol2, ImGui, Lua 5.4
  include/     — public API headers
```
