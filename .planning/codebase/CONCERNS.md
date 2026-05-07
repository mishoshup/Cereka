# Technical Concerns

## Technical Debt

| Area | Description | Severity | Location |
|------|-------------|----------|----------|
| State machine dead code | `CerekaStateMachine` exists with full overlay logic but `CerekaImpl` uses a raw plain enum. All 7 concrete state methods in `cereka_states.cpp` are empty stubs. | HIGH | `src/state/cereka_states.cpp`, `src/engine_impl.hpp` |
| Nested if/else VM bug | `ELSE` handler in skip mode resets `skipDepth` to 0 unconditionally — breaks nested conditionals at depth > 1 | HIGH | `src/script_vm.cpp:154` |
| No text word-wrap | `draw.cpp` renders dialogue as one texture and scales it down on overflow — long lines become compressed and unreadable | HIGH | `src/draw.cpp:133-145` |
| Save format split | `save_data.hpp` defines glaze JSON schema but `save.cpp` uses a bespoke `key=value .sav` format and never includes `save_data.hpp`. Tests only cover the glaze path. CLAUDE.md claims `.json` format but actual saves are `.sav`. | HIGH | `src/save.cpp`, `src/save_data.hpp` |
| Lua VM per-include | A new `sol::state` is spun for each included file — no shared state across the compile chain | MED | `src/compiler/vn_instruction.cpp` |
| Unused hover image | `button.hoverImage` is loaded/destroyed in config but never read in `draw.cpp` | MED | `src/draw.cpp`, `src/config/property_handlers.cpp` |
| Silent label miss | `labelMap[]` operator silently inserts on missed label — missed jumps go to pc=0 instead of erroring | MED | `src/script_vm.cpp` |
| Window resolution ignored | `video::create_window()` ignores the `width`/`height` from `game.cfg`, always uses monitor native resolution | MED | `src/video.cpp` |
| Compiler internals leak | Public header `include/Cereka/Cereka.hpp` includes `compiler/vn_instruction.hpp`, exposing compiler internals to game authors | MED | `include/Cereka/Cereka.hpp` |
| Global type alias | `using Impl = cereka::CerekaImpl` declared at global scope in `engine_impl.hpp` | LOW | `src/engine_impl.hpp` |

## Crash / Safety Risks

| Risk | Description | Location |
|------|-------------|----------|
| Unguarded stoi on save data | `LoadGame()` calls `std::stoi`, `stoull`, `stof` on raw disk values with no try/catch — corrupted save crashes the engine | `src/save.cpp` |
| UB state cast from save | `(CerekaState)std::stoi(val)` casts an integer from disk — out-of-range gives undefined behavior | `src/save.cpp` |
| No VM call stack depth limit | Compiler enforces MAX_DEPTH=32 for nested includes, but the VM runtime `CALL` stack has no bounds check | `src/script_vm.cpp` |
| Unvalidated restored pc | No bounds check on restored `pc` before first `TickScript()` after `LoadGame()` | `src/save.cpp`, `src/script_vm.cpp` |
| Relative save path | Save path is `"saves/"` (relative) — depends on working directory matching project root | `src/save.cpp` |
| Compile errors swallowed | Compiler errors return an empty instruction vector with no error propagation to the caller | `src/compiler/vn_instruction.cpp` |

## Architectural Risks

- **CerekaImpl god object**: Still holds most state despite extraction of SceneManager, AudioManager, MenuSystem, DialogueSystem. The split is partially complete — tight coupling between extracted classes and CerekaImpl's private fields likely remains.
- **No renderer abstraction**: SDL types (`SDL_Renderer*`, `SDL_Texture*`) flow directly through engine logic rather than behind an interface. This blocks any future renderer swap or testing.
- **No scene graph**: bg/char/overlay rendering is done with flat arrays and manual z-order. Adding ATL (dissolve/zoom/rotate) requires a scene graph first.
- **State machine wired around the outside**: `CerekaStateMachine` overlay push/pop is the right design, but `CerekaImpl` still switches behavior via a plain enum — the two systems coexist without integration.

## Performance Concerns

| Issue | Impact | Location |
|-------|--------|----------|
| Per-frame texture recreation | `RenderText()` creates+destroys a GPU texture every frame for all visible text | `src/draw.cpp` / `src/text_renderer.cpp` |
| Per-frame file I/O in save overlay | `GetSlotTimestamp()` opens a file per slot on every frame the save overlay is visible | `src/save.cpp` |

## Known TODOs

From CLAUDE.md / PLAN.md (no explicit TODO/FIXME comments found in source):
- Wire `CerekaStateMachine` into `CerekaImpl` (currently holds plain enum)
- Save format `version` field + migration hook
- Scene graph + transform tree (blocks ATL)
- Renderer abstraction (stop leaking SDL types)
- Text markup (`{b}`, color spans)
- Audio fade in/out
- Rollback, dialogue history, minigames

## Platform Fragility

- `wmain` vs `main` split in `tests/main.cpp` — Windows requires `wmain` for Unicode argv; verified cross-platform but requires maintenance discipline.
- Lua found via `find_package(Lua 5.4 EXACT REQUIRED)` — exact version pinning means distro Lua 5.4.x updates with minor version bumps will break the find.
- Linux→Windows cross-compile via `ucrt64.cmake` uses llvm-mingw — Qt6 is a system dependency that must be installed separately and is not vendored.
- `file(GLOB_RECURSE)` in `src/CMakeLists.txt` means adding/removing `.cpp` files requires re-running cmake manually.

## Missing Safety Nets

- No integration tests — rendering, audio, save I/O, and VM execution are entirely untested
- No fuzzing or property-based testing for the .crka compiler or save format parser
- No input validation on `game.cfg` values before use (width/height treated as integers without bounds)
- No diagnostic output for runtime VM errors — execution silently stops
