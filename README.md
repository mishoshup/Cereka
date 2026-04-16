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

The launcher is a GUI project manager. Use it to create new game projects, open existing ones, and launch your game without touching the terminal.

**Create Game** scaffolds a complete starter project on disk:

```
assets/scripts/main.crka       — full tutorial script (every command with comments)
assets/scripts/ui.crka         — starter UI theme (ready to customise)
assets/scripts/scene_two.crka  — example called scene
assets/bg/placeholder_bg.png
assets/characters/placeholder_char.png
assets/sounds/placeholder_{bgm,sfx}.wav
assets/fonts/Montserrat-Medium.ttf
assets/ui/                     — empty, ready for custom UI sprites
game.cfg
```

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
    ui/
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

; ---------- scene ----------
bg filename.jpg
bg filename.jpg fade 0.5        ; dissolve over N seconds

char id left sprite.png         ; position: left | center | right
char id center sprite.png
char id right sprite.png
hide char id

; ---------- dialogue ----------
say CharacterId "Text here."
narrate "Narration text."

; ---------- flow ----------
label scene_name
jump scene_name
end

include other.crka              ; compile-time inline (strips its `end`)
call scene.crka                 ; runtime subroutine (returns on `end`)

; ---------- menus ----------
menu
    bg background.jpg           ; optional background swap
    button "Option A" goto label_a
    button "Option B" goto label_b
    button "Exit"     exit

; ---------- variables ----------
set flag_name value
set counter 0

; ---------- conditionals ----------
if flag_name == value
    say Alice "This runs if flag matches."
endif

if flag_name != value
    say Alice "This runs if it doesn't."
endif

; ---------- audio ----------
bgm music.mp3
sfx sound.wav
stop_bgm

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
    image assets/ui/namebox.png

ui button
    color 20 80 120 255
    w 560   h 72
    text_color 255 255 255 255
    image assets/ui/button.png
    hover_image assets/ui/button_hover.png

ui font
    size 36
```

All UI properties have defaults — only override what you need. Put `include ui.crka` at the top of your entry script to apply a theme before any scene runs.

### Multi-file scripts

- `include <file>` — compile-time inline. The target file's instructions are pasted in place; its `end` is stripped. Good for UI themes and shared label banks.
- `call <file>` — runtime subroutine. Pushes a return address, jumps to the file, and resumes on `end`. Good for self-contained scenes and reusable sequences.
- Paths are relative to the including file's directory.
- Max include/call depth: 32 (prevents infinite loops).

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
