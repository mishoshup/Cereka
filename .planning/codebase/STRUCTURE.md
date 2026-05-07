# Directory Structure

## Tree

```
cereka/
├── include/Cereka/          # Public API (no SDL/internal leaks)
│   ├── Cereka.hpp           # Main public header
│   └── exceptions.hpp       # Public exception types
├── src/                     # Engine static library (cereka)
│   ├── Cereka.cpp           # Init/shutdown, SDL helpers, public API wrappers
│   ├── script_vm.cpp        # TickScript (VM dispatch), Update, HandleEvent, EvalExpr
│   ├── script_interpreter.cpp/hpp  # ScriptInterpreter (extracted from CerekaImpl)
│   ├── draw.cpp             # Every-frame rendering (bg, chars, menu, dialogue, overlays)
│   ├── audio.cpp            # BGM/SFX via SDL3_mixer (deprecated - see audio_manager)
│   ├── audio_manager.cpp/hpp       # AudioManager (extracted from CerekaImpl)
│   ├── save.cpp             # SaveGame, LoadGame, DrawSaveLoadOverlay, HitTestSaveSlot
│   ├── save_data.hpp        # SerializableSaveData struct with glaze meta (save schema)
│   ├── ui_config.cpp/hpp    # ApplyUiSet, LoadFont — delegates to ConfigManager
│   ├── video.cpp/hpp        # Video/display helpers
│   ├── text_renderer.cpp/hpp       # Text rendering abstraction
│   ├── engine_impl.hpp      # Private CerekaImpl class (being split across phases)
│   ├── scene_manager.cpp/hpp       # SceneManager (extracted from CerekaImpl)
│   ├── dialogue_system.cpp/hpp     # DialogueSystem (extracted from CerekaImpl)
│   ├── menu_system.cpp/hpp         # MenuSystem (extracted from CerekaImpl)
│   ├── compiler/
│   │   ├── vn_instruction.hpp      # Op enum + Instruction struct (srcLine/srcCol)
│   │   ├── vn_instruction.cpp      # CompileVNScript: invokes compiler.lua via sol2
│   │   └── embed_lua.cmake         # Embeds compiler.lua as C++ header at build time
│   ├── config/
│   │   ├── config_manager.cpp/hpp  # Property Map Pattern — typed properties
│   │   ├── property_handlers.cpp   # Per-property apply functions
│   │   └── property_types.hpp      # Property type definitions
│   └── state/
│       ├── cereka_state.hpp        # IVNState, CRTP VNState<T>, CerekaStateMachine
│       ├── cereka_states.hpp       # Concrete state declarations
│       └── cereka_states.cpp       # Dialogue, Menu, Fade, SaveMenu, LoadMenu, etc.
├── runner/                  # CerekaGame executable
│   ├── CMakeLists.txt
│   └── main.cpp             # Game loop entry point
├── launcher/                # CerekaLauncher (Qt6) — project manager
│   ├── CMakeLists.txt
│   ├── main.cpp             # Window + dialogs
│   ├── project_manager.cpp/hpp     # Project lifecycle, dev-run, packaging
│   ├── config.cpp/hpp       # game.cfg reader/writer
│   ├── templates.hpp        # Scaffolded project files (game.cfg, ui.crka, main.crka)
│   ├── theme.hpp            # Qt palette
│   └── template_assets/     # Placeholder assets embedded into new projects
├── scripts/
│   └── compiler.lua         # .crka → Instruction[] compiler (5 passes, embedded at build)
├── tests/
│   ├── CMakeLists.txt
│   ├── main.cpp             # GoogleTest entry (wmain on Windows, main elsewhere)
│   ├── config_test.cpp      # ConfigManager unit tests
│   ├── save_data_test.cpp   # SerializableSaveData JSON round-trip tests
│   └── compile/             # Lua snapshot tests
│       ├── harness.lua      # Test runner (diff inputs vs expected)
│       ├── inputs/          # .crka input files (8 scenarios)
│       └── expected/        # Expected compiler output (snapshot files)
├── vendor/                  # Git submodules (SDL3*, sol2, glaze)
├── cmake/toolchains/
│   └── ucrt64.cmake         # llvm-mingw Linux→Windows cross-compile toolchain
├── CMakeLists.txt           # Root build config
├── .clang-format            # Clang-format style config
├── CLAUDE.md                # AI assistant codebase instructions
├── PLAN.md                  # Development roadmap
└── release.sh               # Release packaging script
```

## Key Files

| File | Role |
|------|------|
| `src/engine_impl.hpp` | Private CerekaImpl — central state holder (actively being split) |
| `src/script_vm.cpp` | VM instruction dispatch loop and expression evaluator |
| `src/compiler/vn_instruction.cpp` | C++ bridge: invokes Lua compiler, resolves includes/calls |
| `src/compiler/vn_instruction.hpp` | Op enum and Instruction struct — core data types |
| `scripts/compiler.lua` | The entire .crka compiler (5 passes: split→tokenize→parse→lower→bridge) |
| `src/state/cereka_state.hpp` | State machine: CRTP base, overlay stack |
| `src/state/cereka_states.cpp` | All concrete VN states |
| `src/config/config_manager.hpp` | Property Map config system |
| `src/save_data.hpp` | Glaze JSON save schema (single source of truth) |
| `include/Cereka/Cereka.hpp` | Public API surface |
| `launcher/templates.hpp` | New project scaffold templates |

## Entry Points

| Executable | Main File | Purpose |
|-----------|-----------|---------|
| `CerekaGame[.exe]` | `runner/main.cpp` | Runs a VN game (requires game.cfg) |
| `CerekaLauncher[.exe]` | `launcher/main.cpp` | Qt6 project manager and dev runner |
| `cereka_test[.exe]` | `tests/main.cpp` | GoogleTest suite (config + save_data) |

## Build Outputs

| Output | Path |
|--------|------|
| Game runner | `build/runtimes/<linux|windows>/CerekaGame[.exe]` |
| Launcher | `build/launcher/CerekaLauncher[.exe]` |
| Test binary | `build/tests/cereka_test[.exe]` |
| Engine lib | `build/src/libcereka.a` (static, linked into runner) |

## Where to Add New Code

| Task | Location |
|------|----------|
| New .crka keyword | `scripts/compiler.lua` (parser+lowerer) + `src/compiler/vn_instruction.hpp` (Op enum) + `src/script_vm.cpp` (dispatch) — see `/add-op` skill |
| New engine state | `src/state/cereka_states.hpp` + `src/state/cereka_states.cpp` |
| New UI property | `src/config/property_types.hpp` + `src/config/property_handlers.cpp` |
| New save field | `src/save_data.hpp` (glaze meta) |
| New C++ unit test | `tests/<area>_test.cpp` (re-run cmake after adding) |
| New snapshot test | `tests/compile/inputs/<name>.crka` + run `lua harness.lua --update` |
