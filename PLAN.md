# Cereka — Visual Novel Engine Roadmap

> Goal: a Ren'Py-class VN engine where authors write only `.crka` scripts. Custom script, sensible defaults, power-user overrides. Long-term: screen/widget DSL, ATL transforms, rollback, minigames.

---

## Current State (2026-04-18, post `cac2b9e`)

**What's built and working:**

- `.crka` → `Instruction[]` pipeline: tokenizer + AST + lowerer in `scripts/compiler.lua`, bridged via `sol2`. Emits `line`/`col` per instruction for error reporting.
- Scene commands: `bg`, `bg ... fade`, `char <id> [pos] <file>`, `hide char`, `narrate`, `say`.
- Flow: `label`, `jump`, `call`/`return` (32-deep), `include` (compile-time inline), `end`.
- Choice menus: `menu` blocks with `button "..." goto <label> | exit`, per-menu `bg` swap + fade.
- Variables: string `set`, numeric `$ x = expr` with `+ - * /`, comparisons `== != > < >= <=`, `{var}` substitution in text.
- Audio: `bgm` (looping), `stop_bgm`, `sfx` one-shots.
- Save/load: 10 slots, glaze-based JSON serialization (`src/save_data.hpp`), overlay UI (`save_menu`/`load_menu`), silent `save N`/`load N`.
- UI theming via `ui.crka`: data-driven `ConfigManager` + Property Map Pattern (`src/config/`) — textbox, namebox, button, font, configurable `advance_keys`.
- State machine extracted to `src/state/` — `IVNState` interface, CRTP `VNState<T>`, `CerekaStateMachine` with overlay push/pop stack.
- Cross-platform build: Linux native + Linux→Windows cross-compile via llvm-mingw toolchain.
- Qt6 launcher with project scaffolding, dev-run output piping, game packaging (tar.gz/zip).
- C++ test suite (`tests/config_test.cpp`, `tests/save_data_test.cpp`) + Lua compile-output snapshot suite (`tests/compile/`, 8 cases, green).

**What's good — keep the bones:**

- Tech stack: SDL3 + SDL3_mixer/ttf/image, Lua 5.4 + sol2, Qt6, glaze. Modern, portable, ages well.
- Zero-dep build (`git clone --recursive && cmake && ninja`). Vendoring was the right call.
- Public API (`include/Cereka/Cereka.hpp`) is narrow.
- Windows cross-compile toolchain is genuinely well-designed.
- State machine design (context-interface + CRTP + overlay stack) is enterprise-grade — the prior "event-driven input + overlay stack" concern from the review is resolved.
- Property Map Pattern for config eliminates the prior string-dispatch concern in `ApplyUiSet`.
- Glaze JSON save format replaces the old fragile `key=value` text format.

**Architectural ceiling — what still blocks Ren'Py-parity feature work:**

Recent refactors knocked down several review concerns; two structural blockers remain for the sprints below:

1. **`CerekaImpl` is still a partial god class.** Despite the new `state/` and `config/` modules, `src/engine_impl.hpp` still holds window/renderer, font, characters map, audio handles, ScriptVM state, dialogue state, fade transition state, and menu state all in one place. `TickScript()` in `src/script_vm.cpp` (526 lines) is still one large switch mutating it freely. Rollback, dialogue history, and minigames need *narrative state* cleanly extractable from rendering state — not done yet.
2. **No scene graph, no renderer abstraction.** Flat `std::unordered_map<id, CharacterEntry>`; `src/draw.cpp` (146 lines) calls `SDL_*` directly. `src/video.cpp` handles window init but is not a drawing abstraction. ATL transforms (rotate/scale/zoom/dissolve as a unified animation system) can't land cleanly on this.

Parser concern (review item #1) is closed by Phase 0.1 below. State-machine/input concerns from the review are closed by the recent refactor. Save-format concern is largely closed (still needs explicit version field + migration hook — tracked in Phase 6).

---

## Philosophy

- **Custom script only** (`.crka`) — no external languages.
- **Sensible defaults**, but power users can override everything.
- As powerful as Ren'Py, but with a script designed for VNs from the ground up.

---

## Core Architecture Decisions

### Script language

- Indent-based blocks (Ren'Py-style).
- Compile-time `include`, runtime `call`/`return`.
- Hyprland-like import model for screens/widgets.

### Screen system (first-class)

- Screens are hierarchical layout trees.
- Built-in screens auto-available; user override allowed.
- ATL transforms work on screens.

### Widget system

- Indent-based DSL, sensible defaults, full override.
- Defined with the same syntax as screens.

### ATL (animation & transformation language)

- Ren'Py-equivalent: `dissolve`, `fade`, `move`, `zoom`, `rotate`, `shake`, `vpunch`, `hpunch`, `flash`.
- Applies to: characters, backgrounds, screens.
- Ease: `linear`, `ease-in`, `ease-out`, `ease-in-out`.

### Audio

- 4-channel mixer: `bgm`, `voice`, `sfx`, `master`.
- Per-channel volume, fade in/out.

---

## Phase 0: Refactor foundation (BLOCKS every phase below)

### 0.1 Real parser + AST for the compiler — 🔜 IN PROGRESS

**Why:** feature gating — indent-sensitive blocks, nested widget trees, `with <transform>` modifiers, text markup, and arithmetic expressions all need a real grammar.

- ✅ `scripts/compiler.lua` rewritten as tokenizer → recursive-descent parser → AST → lowerer. All 8 compile-output snapshot tests green.
- ✅ `line`/`col` emitted on every AST node and every lowered instruction.
- ✅ Errors raised as `line L col C: message` via `error(...)` — sol2 surfaces these to C++ naturally.
- 🔜 Remaining: plumb `srcLine`/`srcCol` through `src/compiler/vn_instruction.{hpp,cpp}` so runtime errors (undefined label, missing asset) can also cite the source location.
- 🔜 Remaining: verify full C++ build + game runs unchanged end-to-end.

### 0.2 Finish splitting the `CerekaImpl` god class

**Why:** rollback (Phase 5) needs snapshottable *narrative state* separated from *rendering state*; minigames need swap-in/swap-out of input+draw; dialogue history needs observable VM state.

Already done by `cac2b9e` / earlier commits (do **not** redo):

- ✅ State machine extracted to `src/state/` (`IVNState`, `CerekaStateMachine`, overlay stack).
- ✅ Config extracted to `src/config/` (`ConfigManager` with Property Map Pattern replaces string-dispatch).
- ✅ Video init extracted to `src/video.{cpp,hpp}` (`cereka::video` namespace).
- ✅ Text rendering extracted to `src/text_renderer.{cpp,hpp}`.

Still to extract from `CerekaImpl` (`src/engine_impl.hpp`):

- `SceneManager` — bg texture + path, `characters` + `charPaths` map, `pendingBg`, `fadePhase`, `fadeTimer`, `fadePhaseDuration`.
- `DialogueSystem` — `currentSpeaker`, `currentName`, `currentText`, `typewriterTimer`, `displayedChars`, `CHARS_PER_SECOND`.
- `AudioManager` — `mixer`, `bgmAudio`, `bgmTrack`, `bgmPath`, `sfxCache`, `audioInitialized`.
- `ScriptVM` — `lua`, `program`, `labelMap`, `variables`, `numVariables`, `callStack`, `pc`, `skipMode`, `skipDepth`, `scriptFinished`.
- `MenuSystem` — `inMenu`, `buttonTexts`/`buttonTargets`/`buttonExits`, `menuEndPC`.

Once extracted, `CerekaImpl` becomes a thin facade owning `SDL_Window`/`SDL_Renderer`/`TTF_Font` and wiring the subsystems + the existing `CerekaStateMachine`. Do **not** introduce an `IRenderer` interface in this step (that's Phase 2 foundation).

### 0.3 Compile-output snapshot tests — ✅ DONE

- `tests/compile/inputs/*.crka` — 8 cases covering every current opcode.
- `tests/compile/expected/*.txt` — canonical instruction output, one instruction per line, fields alphabetized.
- `tests/compile/harness.lua` — standalone runner: `lua tests/compile/harness.lua` (add `--update` to regenerate after an intentional behavior change).
- TODO: wire into `release.sh` / eventual CI so it runs on every push.

---

## Phase 1: Audio & voice (Sprint 1 continued)

### 1.1 Variables: numeric + arithmetic ✅ DONE

### 1.2 Four-channel mixer

- Channels: `bgm`, `voice`, `sfx`, `master`.
- Per-channel volume 0.0–1.0.
- Preferences persist across sessions (new `prefs.sav` or similar).
- `SetVolume("channel", v)` as a screen action (depends on Phase 3).

### 1.3 Audio fade in/out

```
bgm music.ogg fade 2.0
stop_bgm fade 1.0
SetVolume("bgm", 0.5, fade=1.0)
```

### 1.4 Voice lines

```
voice "alice_01.ogg"
say alice "Hello!"
```

Auto-plays with dialogue via the `voice` channel.

---

## Phase 2: ATL core (needs 0.2 done + scene graph + renderer abstraction)

### 2.0 Scene graph + renderer abstraction (foundation for this phase)

- `struct Displayable { Texture tex; Transform target; Transform current; vector<Tween> tweens; }`, owned by `SceneManager`.
- `Update(dt)` advances tweens with easing functions.
- `Renderer` class wrapping SDL: `DrawSprite(tex, rect, transform)`, `DrawRect`, `DrawText`. Remove `SDL_Texture*` from public headers.
- Fade reduces to a tween on `alpha` — not a special state machine branch.

### 2.1 Character transforms

```
char eileen happy center with dissolve
char alice surprised left with fade
char bob thinking right with ease-in move
char cathy happy center with zoom 1.5
char dave neutral left with rotate 15
```

Default duration 0.5s, easing options: `linear`, `ease-in`, `ease-out`, `ease-in-out`.

### 2.2 Background transforms

```
bg park with dissolve
bg beach with flash
bg city with fade
```

Also: `vpunch`, `hpunch`, `shake`.

### 2.3 Screen transforms

```
show screen quick_menu with slideawayleft
hide screen settings with slideawayright
```

---

## Phase 3: Screens & widgets (needs 0.1 done — hard block)

### 3.1 Text markup

```
say alice "{b}Bold{/b}"
say bob "{i}Italic{/i}"
say cathy "{color=ff0000}Red{/color}"
say dave "Click to continue.{nw}"
say eve "{cps=30}Slow{/cps}"
```

`{var}` already works ✅.

### 3.2 Screen system core

```
screen settings:
    frame:
        align (0.5, 0.5)
        padding 20
        vbox:
            label "Settings"
            hbox:
                text "BGM"
                slider value Preference("bgm_volume")
```

### 3.3 Widgets

`frame`, `vbox`, `hbox`, `label`, `textbutton`, `slider`, `toggle`, `text`.

### 3.4 Layout

```
align (0.5, 0.5)
xpos 100          ; pixels
ypos 0.3          ; or 30%
padding 20
spacing 10
```

### 3.5 Actions

```
ShowScreen / HideScreen / Jump / Call / Return
SetVolume / ToggleSetting / SetPreference
SaveGame / LoadGame / QuickSave / QuickLoad
Pause / Quit / Skip / Rollback
action UnlockGallery("beach_scene")  ; custom
```

### 3.6 State binding

```
slider value Preference("bgm_volume")
toggle value Preference("auto_forward")
```

Auto-syncs with engine preferences. React-like.

### 3.7 Events

```
textbutton "Save":
    action ShowScreen("save")
    hovered PlaySound("hover.wav")
    clicked PlaySound("click.wav")
```

---

## Phase 4: Built-in screens (needs Phase 3)

### 4.1 `textbutton` customization — per-button overrides, idle/hover images, sounds

### 4.2 Built-in `quick_menu` — Save / Load / Settings / History. Auto-shown during dialogue

### 4.3 Built-in `settings` — BGM / Voice / SFX sliders, text-speed, auto-forward, skip-unseen

### 4.4 Built-in `history` — scrollable dialogue log

### 4.5 Built-in `save` / `load` — 10 slots, thumbnails, timestamps

---

## Phase 5: Polish

### 5.1 Rollback (needs 0.2 — snapshottable narrative state)

- Click history → rewind `pc`, variables, bg, characters, text position.
- Essential player expectation.

### 5.2 `wait` / `pause`

```
wait 2.0
wait for voice
```

### 5.3 Subpositions

```
char eileen center at 0.3
char alice left at 0.1
```

Arbitrary `x ∈ [0, 1]`, not just left/center/right.

---

## Phase 6: Hardening (before v1.0)

- ✅ **Event-driven input / overlay stack.** Landed in `src/state/` — `CerekaStateMachine` with `pushOverlay`/`popOverlay` replaces the old `inMenu`/`stateBeforeSaveMenu` booleans. Multiple concurrent overlays now possible.
- 🟡 **Versioned save format.** Glaze JSON save already landed (`src/save_data.hpp`). Still missing: explicit `version` field + migration hook so future schema changes don't silently corrupt old saves.
- 🟡 **Compiler error surfacing at runtime.** 0.1 emits `line:col` on every instruction. Remaining: thread `srcLine/srcCol` through `Instruction` so runtime errors (undefined label, missing asset, type mismatch) can cite the source location.
- **Launcher refactor.** Extract `PackageManager`, `ProjectLifecycle` from `launcher/main.cpp` (1010 lines → per-concern files).
- **CMake hygiene.** Replace `file(GLOB_RECURSE)` in `src/CMakeLists.txt` with explicit source lists.
- **CI.** GitHub Actions: Linux-native + Linux→Windows cross-compile + `lua tests/compile/harness.lua` + the C++ test suite on every push.
- **Resolution independence.** Unified virtual coordinate space, scale at renderer boundary.
- **Runner path sandboxing.** Reject `game.cfg` entries that escape project root.
- **Per-game metadata.** `game.cfg` gains `version`, `author`, `icon`, `license`.

---

## Ticket index

| #   | Phase | Ticket                             | Status         |
| --- | ----- | ---------------------------------- | -------------- |
| 0.1 | 0     | Real parser + AST                  | 🔜 In progress (Lua done, C++ srcLine/srcCol pending) |
| 0.2 | 0     | Finish `CerekaImpl` split (scene, dialogue, audio, VM, menu — state + config already extracted) | Pending |
| 0.3 | 0     | Compile-output snapshot tests      | ✅ Done         |
| 1.1 | 1     | Numeric variables + arithmetic     | ✅ Done        |
| 1.2 | 1     | 4-channel mixer                    | Pending        |
| 1.3 | 1     | Audio fade in/out                  | Pending        |
| 1.4 | 1     | Voice lines                        | Pending        |
| 2.0 | 2     | Scene graph + renderer abstraction | Pending        |
| 2.1 | 2     | ATL: characters                    | Pending        |
| 2.2 | 2     | ATL: backgrounds                   | Pending        |
| 2.3 | 2     | ATL: screens                       | Pending        |
| 3.1 | 3     | Text markup                        | Pending        |
| 3.2 | 3     | Screen system core                 | Pending        |
| 3.3 | 3     | Widgets                            | Pending        |
| 3.4 | 3     | Layout                             | Pending        |
| 3.5 | 3     | Actions                            | Pending        |
| 3.6 | 3     | State binding                      | Pending        |
| 3.7 | 3     | Events                             | Pending        |
| 4.1 | 4     | textbutton customization           | Pending        |
| 4.2 | 4     | Built-in quick_menu                | Pending        |
| 4.3 | 4     | Built-in settings                  | Pending        |
| 4.4 | 4     | Built-in history                   | Pending        |
| 4.5 | 4     | Built-in save/load                 | Pending        |
| 5.1 | 5     | Rollback                           | Pending        |
| 5.2 | 5     | wait/pause                         | Pending        |
| 5.3 | 5     | Subpositions                       | Pending        |
| 6.x | 6     | Hardening pack                     | Pending        |

---

## Technical notes

### Implementation order

**0.3 (snapshot tests) ✅ → 0.1 (parser, Lua done + C++ srcLine/srcCol) → 0.2 (finish god-class split) → Phase 1 → 2.0 → 2.1–2.3 → Phase 3 → Phase 4 → Phase 5 → Phase 6.**

### Screen rendering model

- Screens are renderable nodes — same base as sprite/bg/character.
- ATL transforms apply universally.
- Z-order: backgrounds < characters < screens < overlays.

### File structure (game projects)

```
game/
├── game.cfg            ; title, width, height, fullscreen, entry
├── ui.crka             ; UI theming, include at top of main
├── main.crka           ; entry script
├── screens/
│   ├── settings.crka
│   ├── confirm.crka
│   └── ...
├── assets/
│   ├── bg/ characters/ sounds/ fonts/ ui/
│   └── scripts/
└── saves/              ; auto-created
```

### Include system

```
include screens/settings.crka
include screens/confirm.crka
```

Compile-time inline, post-parse expansion. No namespaces — labels are global.
