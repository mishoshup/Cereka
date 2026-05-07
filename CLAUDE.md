# Cereka — Cereka Game Engine

Goal: game authors write only `.crka` scripts — no C++ needed.

Developed on **both Linux and Windows** as first-class targets. Linux→Windows cross-compile is also supported via the `cmake/toolchains/ucrt64.cmake` llvm-mingw toolchain.

## Build (zero system dependencies beyond a C++ compiler)
```bash
git clone --recursive <repo>
cd cereka
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build -j12
```
Outputs:
- `build/runtimes/<linux|windows>/CerekaGame[.exe]`
- `build/launcher/CerekaLauncher[.exe]`
- `build/tests/cereka_test[.exe]`

Vendored: SDL3, SDL3_image, SDL3_ttf, SDL3_mixer, Lua 5.4.7, sol2, glaze, GoogleTest (FetchContent), Qt6 (system).

## Architecture

```
src/                 — Cereka static library (engine)
  state/             — CRTP state machine + overlay stack (cereka_state.hpp, cereka_states.{hpp,cpp})
  config/            — data-driven Property Map config (config_manager + property_handlers + property_types)
  compiler/          — Op enum, Instruction struct, Lua compiler bridge
runner/              — CerekaGame executable (game loop)
launcher/            — CerekaLauncher (Qt6) — project manager, dev-runner, packaging
scripts/             — cereka_compiler.lua (Lua → Instruction[] compiler, embedded at build time)
tests/               — GoogleTest C++ unit tests + Lua compile-output snapshots (tests/compile/)
vendor/              — SDL3*, sol2, glaze (submodules)
include/             — public API: Cereka/Cereka.hpp
.claude/skills/      — project skills (build, test, add-op, snapshot, try-crka, review, no-buzzwords, phase-status)
```

### Engine files (src/)
| File | Responsibility |
|---|---|
| `Cereka.cpp` | Init/shutdown, SDL helpers, scene helpers (ShowBackground, ShowCharacter…), public API wrappers |
| `cereka_script.cpp` | CerekaScriptTick (Cereka Script dispatch), Update (typewriter+fade), HandleEvent, LoadCompiledCerekaScript, EvalExpr (RHS expr parser) |
| `cereka_draw.cpp` | Every-frame rendering: bg, chars, menu buttons, dialogue box, save/load overlay |
| `cereka_audio_manager.cpp` | PlayBGM, StopBGM, PlaySFX via SDL3_mixer |
| `cereka_save.cpp` | SaveGame, LoadGame (glaze JSON), DrawSaveLoadOverlay, HitTestSaveSlot |
| `cereka_save_data.hpp` | `SerializableSaveData` struct (glaze meta) — single source of truth for save schema |
| `cereka_ui_config.cpp` | ApplyUiSet, LoadFont — delegates to ConfigManager |
| `cereka_engine_impl.hpp` | Private CerekaImpl class — currently still holds most state (Phase 0.2 splits this) |
| `state/cereka_state.hpp` | `ICerekaState`, CRTP `CerekaStateBase<T>`, `CerekaStateMachine` with overlay push/pop stack |
| `state/cereka_states.{hpp,cpp}` | Concrete states: Dialogue, Menu, Fade, SaveMenu, LoadMenu, Finished, Quit |
| `config/config_manager.{hpp,cpp}` | Property Map Pattern — typed properties registered once, looked up via key |
| `config/property_handlers.cpp` | Per-property apply functions (textbox.color, button.image, …) |
| `compiler/cereka_instruction.hpp` | Op enum + `Instruction` struct (carries `srcLine`/`srcCol` for diagnostics) |
| `compiler/cereka_instruction.cpp` | CompileCerekaScript: invokes embedded cereka_compiler.lua via sol2, resolves include/call recursively |

### State machine (CerekaState)
`Running → WaitingForInput → Running` (normal dialogue)
`Running → InMenu` (choice presented)
`Running → Fading → Running` (bg crossfade)
`WaitingForInput/Running → SaveMenuState/LoadMenuState → (restored state)` (save overlay)
Any state → `Finished`

Overlay stack (`pushOverlay`/`popOverlay` on `CerekaStateMachine`) lets save/load overlays sit on top of the active gameplay state without losing it.

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
                                   `{var}` is substituted with the variable's current value

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

; Variables — strings
set <var> <value>                — set a string variable

; Variables — numeric (with arithmetic + expression RHS)
$ <var> = <expr>                 — assignment    (overwrites)
$ <var> += <expr>                — add
$ <var> -= <expr>                — subtract
$ <var> *= <expr>                — multiply
$ <var> /= <expr>                — divide
                                   <expr> supports numbers, variables (bare names),
                                   and `+ - * /` with `* /` precedence over `+ -`
                                   e.g.  $ damage = base + bonus * 2

; Conditionals
if <var> == <value>
if <var> != <value>
if <var> > <expr>                — numeric comparisons accept variables / arithmetic in RHS
if <var> < <expr>
if <var> >= <expr>
if <var> <= <expr>
else                             — optional, runs when the if was false
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
ui advance_keys <key1> <key2> ...   — keys that advance dialogue (default: space enter)
```

## Compile pipeline (`scripts/cereka_compiler.lua`)
Five passes:
1. **Line splitter** — preserves source line numbers (incl. blank lines).
2. **Tokenizer** — per line, after indent stripped. Tokens: IDENT / STRING / NUMBER / OP. Two-char ops: `== != >= <= += -= *= /=`.
3. **Parser** — recursive descent; produces an AST with `kind`, kind-specific fields, plus `line`/`col`. Block statements (`menu`, `ui <element>`) collect indented children.
4. **Lowerer** — walks AST, emits `Instruction` records. Every emitted instruction carries `line`/`col`.
5. **C++ bridge** (`src/compiler/cereka_instruction.cpp`) — string-tagged op → `Op` enum; resolves `include` (compile-time inline) and `call` (runtime label + RETURN op) recursively.

Compiler errors raise `error("line L col C: message")` — sol2 surfaces these to the C++ caller as `protected_function_result` failures.

## Save format
**glaze JSON**, stored at `{projectRoot}/saves/slot{N}.json`. Schema lives in `src/cereka_save_data.hpp` as `SerializableSaveData` with `glz::meta`. Round-trip tested in `tests/save_data_test.cpp`.

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

## Launcher
Qt6 app (`launcher/`). Main pieces: `main.cpp` (window + dialogs), `project_manager.{cpp,hpp}` (project lifecycle + dev-run output piping + packaging), `config.{cpp,hpp}` (game.cfg reader/writer), `theme.hpp` (Qt palette), `templates.hpp` (scaffolded project files: `game.cfg`, `ui.crka`, `main.crka`, `scene_two.crka`).

## Testing
Two independent suites:

**Compile-output snapshot tests** (`tests/compile/`)
- `lua tests/compile/harness.lua` — diff each `inputs/*.crka` against `expected/*.txt`.
- `lua tests/compile/harness.lua --update` — regenerate expected after intentional compiler changes.
- Naming: never call these "golden" — call them "snapshot" or "compile" tests.

**C++ unit tests** (`tests/cereka_test`, GoogleTest via FetchContent)
- `ninja -C build cereka_test && ./build/tests/cereka_test`
- Currently covers `tests/config_test.cpp`, `tests/save_data_test.cpp`. Entry point `tests/main.cpp` is cross-platform (`wmain` on Windows, `main` elsewhere).

## Key conventions
- All SDL vendor targets use namespaced form: `SDL3_ttf::SDL3_ttf` etc.
- Lua 5.4 found via `find_package(Lua 5.4 EXACT REQUIRED)` in root CMakeLists.txt.
- On Windows, vendor DLLs are routed to `CMAKE_BINARY_DIR` via `RUNTIME_OUTPUT_DIRECTORY`.
- `cereka_engine_impl.hpp` is private — never include from public headers.
- `src/` uses `file(GLOB_RECURSE)` — re-run cmake after adding/removing `.cpp` files.
- Public surface (`include/Cereka/*.hpp`) must not leak SDL/Mixer types.
- Comments explain *why*, not *what*. No "Enterprise-grade" / "production-ready" header banners — see `/no-buzzwords` skill.
- Quality bar: code must be defensible against the "rivals Cereka" yardstick — see `/review` skill.

## Project skills (`.claude/skills/`)
- `/build` — cross-platform build commands and gotchas
- `/test` — run both test suites
- `/add-op` — add a new VN instruction end-to-end (compiler → enum → bridge → VM)
- `/snapshot` — manage compile snapshots
- `/try-crka` — compile a `.crka` snippet inline without booting the engine
- `/review` — quality-bar audit of changes
- `/no-buzzwords` — strip marketing-speak from headers
- `/phase-status` — read PLAN.md, cross-check against repo state, report

## Implemented features
bg, char, hide char, narrate, say, bgm, stop_bgm, sfx, label, jump, call, include, end, menu+button, set, numeric `$` arithmetic with expression RHS, if `== != > < >= <=`, else, endif, `{var}` substitution, bg fade, ui theming (Property Map config), configurable `advance_keys`, save/load (10 slots, glaze JSON, ESC overlay), source-locations on every instruction (`line`/`col`), state machine + overlay stack, cross-platform Linux+Windows native build + Linux→Windows cross-compile, Lua compile snapshots + GoogleTest unit tests.

## Next / planned (see PLAN.md for full roadmap)
- Phase 0.2: split `CerekaImpl` into SceneManager / DialogueSystem / AudioManager / ScriptVM / UIManager
- Wire `CerekaStateMachine` into `CerekaImpl` (currently holds plain enum)
- Save format `version` field + migration hook
- Scene graph + transform tree (unblocks ATL: dissolve/zoom/rotate)
- Renderer abstraction (stop leaking SDL types into engine logic)
- Text markup (`{b}...{/b}`, color spans)
- Audio fade in/out
- Rollback, dialogue history, minigames
