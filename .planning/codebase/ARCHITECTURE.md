# Architecture

**Analysis Date:** 2026-05-07

## System Overview

Cereka is a visual novel engine split into three independent executables: `CerekaGame` (the runtime that plays a game project), `CerekaLauncher` (a Qt6 project manager), and `cereka_test` (unit tests). The engine itself is a static library (`Cereka`) that exposes a minimal pImpl-based public API and compiles `.crka` scripts via an embedded Lua compiler into a flat `Instruction[]` array that a C++ VM dispatches in a per-frame tick.

## Components

| Component | Location | Responsibility |
|-----------|----------|----------------|
| Public API | `include/Cereka/Cereka.hpp` | `CerekaEngine` class — the only type game runners instantiate |
| Engine library | `src/` | Static library; compiled into both `CerekaGame` and `cereka_test` |
| `CerekaImpl` | `src/engine_impl.hpp` | Private implementation class (pImpl); owns all engine subsystems |
| `ScriptInterpreter` | `src/script_interpreter.hpp` | Execution state: program array, PC, call stack, variables, label map |
| Script VM | `src/script_vm.cpp` | `TickScript` dispatch loop; `Update` (typewriter + fade); `HandleEvent` |
| Compiler bridge | `src/compiler/vn_instruction.cpp` | Spawns sol2 Lua state, runs embedded `compiler.lua`, maps string ops to `Op` enum; resolves `include`/`call` recursively |
| Lua compiler | `scripts/compiler.lua` | Five-pass compiler: line splitter → tokenizer → parser → lowerer → instruction table |
| `Op` / `Instruction` | `src/compiler/vn_instruction.hpp` | Instruction set; each instruction carries `srcLine`/`srcCol` |
| `SceneManager` | `src/scene_manager.hpp` / `.cpp` | Background and character textures; crossfade state machine |
| `DialogueSystem` | `src/dialogue_system.hpp` / `.cpp` | Speaker/text state; typewriter timer at 60 chars/sec |
| `MenuSystem` | `src/menu_system.hpp` / `.cpp` | Choice button data; hit-testing |
| `AudioManager` | `src/audio_manager.hpp` / `.cpp` | BGM looping + SFX one-shots via SDL3_mixer |
| `UiConfig` | `src/ui_config.hpp` | Theme structs (`Textbox`, `Namebox`, `Button`); `Dim` for %-or-px values |
| `ConfigManager` | `src/config/config_manager.hpp` / `.cpp` | Property Map Pattern; maps string keys to typed properties with apply functions |
| Draw | `src/draw.cpp` | Every-frame SDL rendering: bg, characters, dialogue box, menus, save/load overlay |
| Save / Load | `src/save.cpp` + `src/save_data.hpp` | Glaze JSON serialization of `SerializableSaveData`; 10 save slots |
| State machine | `src/state/cereka_state.hpp` | `IVNState` interface, CRTP `VNState<T>`, `CerekaStateMachine` with overlay stack |
| Concrete states | `src/state/cereka_states.hpp` / `.cpp` | `DialogueState`, `MenuState`, `FadeState`, `SaveMenuState`, `LoadMenuState`, `FinishedState`, `QuitState` |
| Video | `src/video.hpp` / `.cpp` | SDL window + renderer creation; global `video::window`/`width`/`height` |
| Text renderer | `src/text_renderer.hpp` / `.cpp` | SDL_ttf init/deinit wrapper |
| `CerekaGame` runner | `runner/main.cpp` | Reads `game.cfg`, calls `CompileVNScript`, runs the engine loop |
| `CerekaLauncher` | `launcher/` | Qt6 app: project creation, dev-run output piping, packaging |
| `cereka_test` | `tests/` | GoogleTest unit tests for config and save-data; Lua snapshot tests |

## Design Patterns

**pImpl (Pointer to Implementation)**
- `CerekaEngine` in `include/Cereka/Cereka.hpp` holds only `CerekaImpl *pImplementation`
- Full `CerekaImpl` definition is in `src/engine_impl.hpp` — never included from public headers
- Prevents SDL/Mixer/ttf types from leaking into the public API

**CRTP State Machine**
- `VNState<CerekaState T>` in `src/state/cereka_state.hpp` provides compile-time type dispatch via `type()` override
- `CerekaStateMachine` holds an `unordered_map<CerekaState, unique_ptr<IVNState>>` and an `overlayStack_` for non-destructive save/load overlays
- States communicate back to the engine only via `IVNStateContext` (interface) — loose coupling

**Property Map Pattern**
- `config::ConfigManager` in `src/config/config_manager.hpp` registers all UI properties in a static `PROPERTY_TABLE`
- Adding a new property requires only one entry in the table — no additional code changes
- `ApplyContext` carries raw engine pointers needed by apply functions in `src/config/property_handlers.cpp`

**Embedded Lua Compiler**
- `scripts/compiler.lua` is converted to a C++ `const char[]` header (`compiler_lua_embed.hpp`) at build time by `src/compiler/embed_lua.cmake`
- `CompileVNScript` in `src/compiler/vn_instruction.cpp` creates a fresh `sol::state` per compilation, calls the Lua `compile()` function, and maps string-tagged ops to the `Op` enum

**Flat Instruction Array + PC**
- Scripts compile to `vector<Instruction>` stored in `ScriptInterpreter::program`
- Execution is a simple index (`ScriptInterpreter::pc`) advanced by `TickScript`
- `JUMP` moves the PC; `CALL` pushes `pc+1` onto `ScriptInterpreter::callStack` (max 32 deep); `RETURN` pops it

## Data Flow

### Script Compilation (startup)

```
game.cfg (entry field)
  → CompileVNScript()          src/compiler/vn_instruction.cpp
      → reads .crka file
      → RunLuaCompiler()       spawns sol::state, runs embedded compiler.lua
          → Lua 5-pass compiler (scripts/compiler.lua)
              returns Lua table of {op, a, b, c, line, col, choices}
      → maps string ops → Op enum, builds vector<Instruction>
      → resolves INCLUDE (inline) and CALL (subroutine append) recursively
  → vector<Instruction>        returned to runner/main.cpp
```

### Runtime Game Loop (runner/main.cpp)

```
while (!IsGameFinished()) {
    PollEvent → HandleEvent    // input; state transitions (ESC, click, advance)
    Update(1/60)               // typewriter tick; fade tick
    TickScript()               // dispatch current PC instruction; advance PC
    Draw()                     // SDL rendering
    Present()                  // SDL_RenderPresent
}
```

### TickScript Dispatch (src/script_vm.cpp)

```
ScriptInterpreter::pc
  → program[pc].op switch
      SAY / NARRATE  → DialogueSystem::Show()  → state = WaitingForInput
      BG             → SceneManager::ShowBackground()
      CHAR           → SceneManager::ShowCharacter()
      FADE           → SceneManager::StartFade()  → state = Fading
      PLAY_BGM       → AudioManager::PlayBGM()
      MENU           → MenuSystem::Open()  → state = InMenu
      JUMP           → pc = labelMap[target]
      CALL           → callStack.push(pc+1); pc = labelMap[subroutine_label]
      RETURN         → pc = callStack.pop()
      IF_*/ENDIF/ELSE → skipMode / skipDepth bookkeeping
      UI_SET         → ConfigManager::apply()
      SAVE_MENU      → stateBeforeSaveMenu = state; state = SaveMenuState
      SAVE           → SaveGame(slot)
      END            → state = Finished
```

### Save / Load Round-Trip

```
SaveGame(slot):
  ScriptInterpreter → SerializableSaveData (pc, callStack, variables, skipMode)
  SceneManager      → SerializableSaveData (background path, character paths/positions)
  DialogueSystem    → SerializableSaveData (speaker, name, text, displayedChars)
  AudioManager      → SerializableSaveData (bgm path)
  glz::write_json() → {projectRoot}/saves/slot{N}.json

LoadGame(slot):
  glz::read_json()  → SerializableSaveData
  restore all subsystems from struct fields
```

## State Management

`CerekaImpl` holds a plain `CerekaState` enum (`state`) that the VM and event handler mutate directly. A separate `CerekaStateMachine` (defined in `src/state/cereka_state.hpp`) exists and is fully implemented but is not yet wired into `CerekaImpl` — `state` is still the authoritative runtime state.

**State transitions (active today):**
- `Running` → `WaitingForInput` when SAY/NARRATE executes
- `WaitingForInput` → `Running` when an advance key or click is received
- `Running` → `InMenu` when MENU executes
- `InMenu` → `Running` when a button choice is selected (jumps to label)
- `Running` → `Fading` when FADE executes; `Fading` → `Running` when `SceneManager::TickFade` returns true
- Any state → `SaveMenuState` / `LoadMenuState` via `SAVE_MENU` / `LOAD_MENU`; saved state restored to `stateBeforeSaveMenu` on ESC
- Any state → `Finished` on END; `Quit` on window close

**Overlay stack (CerekaStateMachine, not yet active):**
- `pushOverlay` / `popOverlay` preserve the underlying state when save/load overlays are shown

## Public API

**Header:** `include/Cereka/Cereka.hpp`

```cpp
namespace cereka {

struct CerekaEvent { enum Type { Quit, KeyDown, MouseDown, Unknown }; ... };
enum class CerekaState { Running, WaitingForInput, InMenu, Fading, Finished, Quit,
                         SaveMenuState, LoadMenuState };

class CerekaEngine {
public:
    bool InitGame(const char *title, int w, int h, bool fullscreen = false);
    void ShutDown();
    bool PollEvent(CerekaEvent &e);
    void Present();
    int Width() const;  int Height() const;
    void LoadCompiledScript(const vector<scenario::Instruction> &compiled);
    void LoadScript(const string &filename);
    void TickScript();
    void Reset();
    void HandleEvent(const CerekaEvent &e);
    void Update(float dt);
    void Draw();
    bool InMenu() const;
    const string &CurrentText() const;
    size_t ButtonCount() const;
    size_t ProgramCounter() const;
    bool IsGameFinished() const;  bool IsGameQuit() const;  bool IsFinished() const;
    bool SaveGame(int slot);  bool LoadGame(int slot);
private:
    CerekaImpl *pImplementation;
};

} // namespace cereka
```

**Compiler entry point** (also public, included transitively):
```cpp
// include/Cereka/Cereka.hpp → compiler/vn_instruction.hpp
namespace cereka::scenario {
    vector<Instruction> CompileVNScript(const string &filename);
}
```

**Exceptions:** `include/Cereka/exceptions.hpp` — `cereka::engine::Error` (also aliased as `cereka::engine::error`)

## Module Boundaries

**Public (safe to include from outside `src/`):**
- `include/Cereka/Cereka.hpp` — engine API, `CerekaEvent`, `CerekaState`
- `include/Cereka/exceptions.hpp` — `cereka::engine::Error`
- `src/compiler/vn_instruction.hpp` — `Op`, `Instruction`, `CompileVNScript` (included transitively via `Cereka.hpp`)

**Internal (never include from public headers or outside `src/`):**
- `src/engine_impl.hpp` — `CerekaImpl`; includes SDL3, SDL3_image, SDL3_ttf directly
- `src/scene_manager.hpp`, `src/audio_manager.hpp`, `src/dialogue_system.hpp`, `src/menu_system.hpp`, `src/script_interpreter.hpp` — subsystem headers
- `src/save_data.hpp` — glaze meta; only used by `save.cpp` and `tests/save_data_test.cpp`
- `src/ui_config.hpp`, `src/config/` — UI theme and config; depend on SDL types
- `src/state/` — state machine; depends on `Cereka.hpp` for `CerekaState` enum

**Launcher is fully separate:**
- `launcher/` links only `Qt6::Widgets`; has no dependency on the `Cereka` static library
- Spawns `CerekaGame` as a child process for dev-run

## Anti-Patterns

### State enum split

**What happens:** `CerekaImpl::state` (plain `CerekaState` enum, `src/engine_impl.hpp:58`) is mutated directly by `script_vm.cpp` and `HandleEvent`. The fully-implemented `CerekaStateMachine` in `src/state/` is not yet connected to `CerekaImpl`.

**Why it's wrong:** Two representations of the same concept exist in parallel; the state machine's `onEnter`/`onExit` lifecycle is never called during actual gameplay.

**Do this instead:** Replace `CerekaImpl::state` with a `CerekaStateMachine` member and route all state reads/writes through it (planned in Phase 0.2 / PLAN.md).

---

*Architecture analysis: 2026-05-07*
