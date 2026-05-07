# Phase 3: Architectural Cleanup - Context

**Gathered:** 2026-05-07
**Status:** Ready for planning

<domain>
## Phase Boundary

Complete the CerekaImpl god-object split and add renderer abstraction. Four workstreams:
1. Wire CerekaStateMachine overlay push/pop as single source of truth
2. Extract UIManager from CerekaImpl
3. Renderer abstraction (stop leaking SDL types into engine logic)
4. Fix crash/safety risks (unguarded stoi, UB enum cast, unbounded CALL stack, and adjacent)

Plus one infra prerequisite: fix CI failures from Phase 2.

</domain>

<decisions>
## Implementation Decisions

### D-01: CI Fix (Prerequisite)
- **Fix all 3 CI failures before any architectural work.**
- Linux: add `libxtst-dev` to apt dependencies in `.github/workflows/ci.yml`
- macOS: add `brew install qt` step before CMake configure
- Windows cross-compile: wrap `add_subdirectory(launcher)` in CMakeLists.txt with a guard so it's skipped when cross-compiling (launcher is host-platform only), OR add `-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON` to the cross-compile cmake args
- Verification: all three CI jobs green on push

### D-02: State Machine Unification — Full State Encapsulation
- **Move CerekaScriptTick() dispatch loop into DialogueState::update().**
- DialogueState owns the ~240-line switch statement. The machine's lifecycle delegates to the current state.
- Remove `CerekaImpl::state` (plain enum) — all reads replaced with `m_stateMachine.currentType()`
- Remove `CerekaImpl::stateBeforeSaveMenu` — overlay stack (`pushOverlay`/`popOverlay`) is the source of truth
- Save/load serializes via `m_stateMachine.currentType()`. Load restores machine state directly instead of writing to `Impl::state` and syncing post-facto.
- `HandleEvent()` queries machine state for ESC/advance-key logic
- This is the enterprise-grade pattern (Unity/Unreal encapsulate behavior per-state, no global switch)

### D-03: UIManager — Owns All Draw + Layout + Config
- **UIManager is the single source of truth for all visuals.**
- Scope: background rendering, character rendering, dialogue box, menu buttons, save/load overlay, all layout calculation, and the config/theme system
- `cereka_draw.cpp` becomes a thin delegate that calls UIManager
- Per-state draw methods in `cereka_states.cpp` (MenuState::draw, FadeState::draw, etc.) move into UIManager methods
- `UiConfig` and `ConfigManager` live under UIManager ownership
- UIManager depends on `IRenderContext` interface (D-04), NOT on SDL directly
- Future scene graph (Phase 4) extends UIManager's visual tree

### D-04: Renderer Abstraction — Full IRenderContext
- **IRenderContext interface** covering: draw ops (clear, texture, rect, present, blend mode), window dimensions (width/height), texture factory (`createTexture`), and render target management
- `ITexture` wraps `SDL_Texture*` behind the interface
- `video.cpp` creates the concrete `SdlRenderContext` impl
- UIManager gets `IRenderContext&` as an injected dependency — no `impl.renderer`, no `impl.screenWidth`/`screenHeight` globals
- `RenderText()` and `RenderTextWrapped()` take `IRenderContext&` and return `ITexture*`
- All SDL render types (`SDL_Renderer*`, `SDL_Texture*`, `SDL_FRect`, `SDL_Color`) stay behind the interface boundary
- Enables testing with mock IRenderContext, no SDL needed

### D-05: Crash Safety — All 6 Risks, Uniform Error Handling
- **All 6 documented risks fixed with `std::expected`/bounds-check/uniform pattern.**
  - Unguarded `stoi`/`stoull`/`stof` → safe numeric parser returning `std::expected<T, error>`
  - UB enum cast → `parseState()` fallback already exists; ensure the integer-cast path from old `.sav` format is fully dead after glaze migration
  - Unbounded CALL stack → runtime bounds check against `MAX_CALL_DEPTH` (32) in CerekaScriptTick
  - Unvalidated restored pc → after `LoadGame()`, clamp pc to `[0, program.size())`
  - Relative save path → derive from `game.cfg` location or use `fs::absolute()`
  - Compile errors swallowed → `CompileCerekaScript` propagates errors to caller via return value or error callback
- `std::expected` is already available in the codebase (used in `cereka_state.hpp`)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Scope
- `.planning/ROADMAP.md` §Phase 3 — scope anchor and goals
- `.planning/codebase/ARCHITECTURE.md` — system overview, anti-patterns (state enum split)
- `.planning/codebase/CONCERNS.md` — documented crash risks and architectural risks

### State Machine
- `src/state/cereka_state.hpp` — CerekaStateMachine, ICerekaStateContext, overlay stack
- `src/state/cereka_states.hpp` — concrete state declarations
- `src/state/cereka_states.cpp` — concrete state implementations (partial: Menu, Fade, Save, Load have real logic)

### Engine Core
- `src/cereka_engine_impl.hpp` — CerekaImpl god object (state enum + m_stateMachine coexist)
- `src/cereka_script.cpp` — CerekaScriptTick dispatch loop (240-line switch), CALL stack, label handling
- `src/Cereka.cpp` §HandleEvent — ESC/advance-key uses raw state
- `src/cereka_draw.cpp` — all rendering to split into UIManager
- `src/cereka_save.cpp` — save/load with crash risks (stoi, enum cast, unvalidated pc, relative path)
- `src/cereka_ui_config.hpp` — current theme structs (Textbox, Namebox, Button)
- `src/config/config_manager.hpp` — Property Map pattern

### CI
- `.github/workflows/ci.yml` — broken CI configuration
- `launcher/CMakeLists.txt` — Qt6 find_package (fails on cross-compile and macOS)
- `vendor/SDL/cmake/macros.cmake` §433 — XTEST missing on Linux

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `CerekaStateMachine` in `src/state/cereka_state.hpp` — fully implemented with overlay push/pop, lifecycle hooks, ICerekaStateContext interface. The machine exists, is wired for Update/Draw/HandleEvent, but CerekaScriptTick() bypasses it.
- `ICerekaStateContext` interface — CerekaImpl already inherits and implements `changeState`, `pushOverlay`, `popOverlay`.
- Concrete states already have real implementations in `cereka_states.cpp` (CONCERNS.md says "empty stubs" but this is outdated — MenuState, FadeState, SaveMenuState, LoadMenuState all have working logic)
- `std::expected` already imported in `cereka_state.hpp` — ready for error propagation pattern

### Established Patterns
- **CRTP State Machine**: `CerekaStateBase<T>` provides compile-time type dispatch. All concrete states follow this.
- **pImpl**: CerekaEngine → CerekaImpl. New abstractions (UIManager, IRenderContext) should follow this pattern for public API hygiene.
- **Property Map**: ConfigManager maps string keys to typed properties. UIManager should extend this, not replace it.
- **Cereka prefix**: All source files use `cereka_` prefix (Phase 2 branding). New files must follow.

### Integration Points
- **The dispatch loop** in `CerekaScriptTick()` returns mid-tick for SAY/NARRATE/FADE/MENU (sets state, returns). Moving it into DialogueState::update() must preserve this semantics — the machine transitions to WaitingForInput, and the next frame's DialogueState::update() resumes dispatch.
- **HandleEvent** has a global ESC handler (opens save menu) and advance-key handler. These currently read `state` — after unification they read `m_stateMachine.currentType()`.
- **Save serialization** writes `stateToString(stateBeforeSaveMenu)` — after removing `stateBeforeSaveMenu`, use overlay stack to determine saved state.
- **Draw path** delegates to `m_stateMachine.draw()` already. UIManager extraction means per-state draw() methods call UIManager instead of SDL directly.

</code_context>

<specifics>
## Specific Ideas

- "Enterprise-grade rivaling Unity, Unreal, Ren'Py" — state encapsulation pattern must match industry standard game engines
- "Foundation of the greatest VN engine in the world" — UI/renderer split must be clean enough to support future features (scene graph, ATL, text markup) without refactoring

</specifics>

<deferred>
## Deferred Ideas

- Scene graph + transform tree — Phase 4 (requires UIManager + IRenderContext foundation from Phase 3)
- Audio fade in/out — Phase 4
- Text markup (`{b}`, color spans) — Phase 4

</deferred>

---

*Phase: 3-Architectural Cleanup*
*Context gathered: 2026-05-07*
