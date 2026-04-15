# Cereka

A visual novel engine built in C++ with SDL3. Game projects are purely script-based — no C++ required. Write `.crka` scripts, run your game.

---

## Prerequisites

**Linux**
```
g++ (C++23)   cmake (3.24+)   lua5.4   git
```

On Arch: `sudo pacman -S base-devel cmake lua54`  
On Ubuntu/Debian: `sudo apt install build-essential cmake liblua5.4-dev`

**Windows** — coming soon.

---

## Getting the engine

```bash
git clone --recursive https://github.com/mishoshup/cereka.git
cd cereka
```

The `--recursive` flag is required — it pulls SDL3, SDL_mixer, SDL_ttf, ImGui and other vendored dependencies.

---

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

This produces two binaries in `build/`:
| Binary | Purpose |
|---|---|
| `CerekaGame` | Runs a game project |
| `CerekaLauncher` | GUI launcher / dev tool |

---

## Running the launcher

Point it at any game project directory:

```bash
./build/CerekaLauncher /path/to/your/game
```

Or run it from inside the game directory:

```bash
cd /path/to/your/game
/path/to/cereka/build/CerekaLauncher
```

The launcher lets you build the engine and launch your game from a GUI without touching the terminal.

---

## Creating a game project

A game project is just a folder with a `game.cfg` and an `assets/` directory. No C++ needed.

```
my-game/
  game.cfg
  assets/
    scripts/
      main.crka
    bg/
    characters/
    fonts/
    sounds/
```

**game.cfg**
```ini
title      = My Game
width      = 1280
height     = 720
fullscreen = false
entry      = assets/scripts/main.crka
```

Run it directly without the launcher:
```bash
./build/CerekaGame /path/to/my-game
```

---

## Script reference (.crka)

```
; comment

; backgrounds and characters
bg filename.jpg
char id sprite.png
hide char id

; dialogue
say CharacterId "Text here."
narrate "Narration text."

; flow
label scene_name
jump scene_name
end

; menus (indented block)
menu
    bg background.jpg
    button "Option A" goto label_a
    button "Option B" goto label_b
    button "Exit"     exit

; variables
set flag_name value
set counter 0

; conditionals
if flag_name == value
    say Alice "This runs if flag matches."
endif

if flag_name != value
    say Alice "This runs if it doesn't."
endif

; audio
bgm music.mp3
sfx sound.wav
stop_bgm
```

---

## Project structure

```
cereka/
  src/         Cereka engine library (C++)
  runner/      CerekaGame — generic runner that reads game.cfg
  launcher/    CerekaLauncher — ImGui GUI dev tool
  scripts/     compiler.lua — .crka script compiler (embedded at build time)
  vendor/      SDL3, SDL_ttf, SDL_mixer, SDL_image, sol2, ImGui
  include/     Public API headers
```
