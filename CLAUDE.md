# Cereka — Visual Novel Engine

Goal: a Ren'Py rival where game authors write only `.crka` scripts — no C++ needed.

## Build (zero system dependencies beyond a C++ compiler)
```bash
git clone --recursive <repo>
cd cereka && mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -j12
```
Produces in `build/`: `CerekaLauncher.exe`, `CerekaGame.exe`, all vendor DLLs.  
Linux: same commands, no DLLs needed.  
Everything (SDL3, SDL3_image, SDL3_ttf, SDL3_mixer, Lua 5.4.7, sol2, imgui) is vendored.

## Architecture

```
src/          — Cereka static library (engine)
runner/       — CerekaGame executable (game loop)
launcher/     — CerekaLauncher executable (ImGui project manager)
scripts/      — compiler.lua (Lua → Instruction[] compiler, embedded at build time)
vendor/       — SDL3, SDL3_image, SDL3_ttf, SDL3_mixer, sol2, imgui (all submodules)
include/      — public API: Cereka/Cereka.hpp
```

### Engine files (src/)
| File | Responsibility |
|---|---|
| `Cereka.cpp` | Init/shutdown, SDL helpers, scene helpers (ShowBackground, ShowCharacter…), public API wrappers |
| `script_vm.cpp` | TickScript (VM dispatch), Update (typewriter+fade), HandleEvent, LoadCompiledScript |
| `draw.cpp` | Every-frame rendering: bg, chars, menu buttons, dialogue box, save/load overlay |
| `audio.cpp` | PlayBGM, StopBGM, PlaySFX via SDL3_mixer |
| `save.cpp` | SaveGame, LoadGame (text key=value format), DrawSaveLoadOverlay, HitTestSaveSlot |
| `ui_config.cpp` | ApplyUiSet, LoadFont |
| `engine_impl.hpp` | Private CerekaImpl class — all state lives here |
| `compiler/vn_instruction.hpp` | Op enum + Instruction struct |
| `compiler/vn_instruction.cpp` | CompileVNScript (calls embedded compiler.lua via sol2/Lua) |

### State machine (CerekaState)
`Running → WaitingForInput → Running` (normal dialogue)  
`Running → InMenu` (choice presented)  
`Running → Fading → Running` (bg crossfade)  
`WaitingForInput/Running → SaveMenu/LoadMenu → (restored state)` (save overlay)  
Any state → `Finished`

## .crka Script Language

```
; comment

; Scene
bg <file>                        — instant background (assets/bg/)
bg <file> fade <sec>             — crossfade background
char <id> [left|center|right] <file>  — show sprite (assets/characters/)
hide char <id>

; Dialogue
narrate "text"                   — narrator (no name box)
say <id> "text"                  — character dialogue

; Audio
bgm <file>                       — loop music (assets/sounds/)
stop_bgm
sfx <file>                       — one-shot sfx

; Flow
label <name>
jump <label>
include <file>                   — compile-time inline
call <file>                      — runtime subroutine (CALL/RETURN stack, max 32)
end

; Choices
menu
    bg <file> [fade <sec>]       — optional bg swap inside menu
    button "Text" goto <label>
    button "Text" exit

; Variables (string only)
set <var> <value>
if <var> == <value>
if <var> != <value>
endif

; Save / Load
save_menu                        — show 10-slot save overlay (ESC cancels)
load_menu                        — show 10-slot load overlay (ESC cancels)
save <1-10>                      — silent save to slot N
load <1-10>                      — silent load from slot N

; UI theming (usually in ui.crka, included at top of main.crka)
ui textbox
    color r g b a
    y <percent%|pixels>
    h <percent%|pixels>
    text_margin_x <px>
    text_color r g b a
    image <path>
ui namebox
    color / x / y_offset / w / h / text_color / image
ui button
    color / w / h / text_color / image / hover_image
ui font
    size <px>
```

## Save format
Plain text `key=value`, stored in `{projectRoot}/saves/slot{N}.sav`.  
First line is always `timestamp=YYYY-MM-DD HH:MM`.  
Fields: `pc`, `callstack` (comma-separated), `var.<key>`, `bg`, `char.<id>` (`file:pos`), `bgm`, `state`, `speaker`, `name`, `text`, `displayedChars`, `skipMode`, `skipDepth`.

## Project layout (game projects)
```
game.cfg              — title, width, height, fullscreen, entry
assets/scripts/       — .crka files
assets/bg/            — backgrounds
assets/characters/    — sprites
assets/sounds/        — bgm + sfx
assets/fonts/         — .ttf / .otf
assets/ui/            — optional UI images
saves/                — auto-created by save system
```

## Launcher templates
Live in `launcher/templates.hpp` (split from main.cpp to keep it manageable).  
Scaffolded files: `game.cfg`, `ui.crka`, `main.crka`, `scene_two.crka`, placeholder assets.

## Key conventions
- All SDL vendor targets use namespaced form: `SDL3_ttf::SDL3_ttf` etc.
- Lua 5.4 found via `find_package(Lua 5.4 EXACT REQUIRED)` in root CMakeLists.txt.
- On Windows, vendor DLLs are routed to `CMAKE_BINARY_DIR` via `RUNTIME_OUTPUT_DIRECTORY`.
- `engine_impl.hpp` is private — never include from public headers.
- `save.cpp` / `audio.cpp` etc. are picked up automatically by `file(GLOB_RECURSE)` in `src/CMakeLists.txt`.

## Implemented features
bg, char, hide char, narrate, say, bgm, stop_bgm, sfx, label, jump, call, include, end, menu+button, set, if==, if!=, endif, bg fade, ui theming, save/load (10 slots, ESC shortcut), cross-platform Windows+Linux build.

## Next / planned
- Ship Game: launcher "Package" button → zip/tarball with exe + assets + DLLs
- Variable arithmetic (integers, numeric comparisons)
- Text markup (inline color/bold/italic)
- Audio fade in/out
