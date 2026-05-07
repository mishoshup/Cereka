# Phase 3: Architectural Cleanup — Research

**Researched:** 2026-05-07
**Domain:** C++ engine architecture, state machine migration, renderer abstraction, crash safety
**Confidence:** HIGH

## Summary

Phase 3 completes the CerekaImpl god-object split and adds a renderer abstraction boundary. The codebase has matured significantly since earlier concerns were documented: the state machine concrete states are no longer empty stubs, save format has been unified to Glaze JSON, and the nested-if VM bug is fixed. Four workstreams remain (plus one CI prerequisite), each with well-understood scope and verified integration points.

**Primary recommendation:** Execute in dependency order: (1) CI Fix → (2) Crash Safety → (3) IRenderContext + ITexture → (4) UIManager extraction → (5) State Machine Unification. Workstreams 2 and 3/4/5 are independent and can be parallelized, but 4 depends on 3, and 5's draw path integration depends on 4.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CI-01 | Fix Linux CI: add libxtst-dev | Verified: SDL cmake/macros.cmake:431-444 checks for X11/Xlib.h/Xresource.h; cmake runs find_package(X11) which needs libxtst-dev |
| CI-02 | Fix macOS CI: brew install qt | Verified: launcher/CMakeLists.txt:11 does find_package(Qt6 COMPONENTS Widgets REQUIRED); macOS runner has no Qt installed |
| CI-03 | Fix cross-compile: guard launcher subdirectory | Verified: root CMakeLists.txt:11 unconditionally add_subdirectory(launcher); cross-compile toolchain finds no Qt6 and fails |
| SM-01 | Move CerekaScriptTick dispatch into DialogueState | Verified: DialogueState::update() (cereka_states.cpp:14-19) already calls impl.CerekaScriptTick(); the 240-line switch at cereka_script.cpp:90-298 is the target |
| SM-02 | Remove CerekaImpl::state raw enum | 8 reads across Cereka.cpp, cereka_script.cpp; state is currently synced from m_stateMachine after every transition |
| SM-03 | Remove CerekaImpl::stateBeforeSaveMenu | 6 references across Cereka.cpp, save.cpp; fully replaced by m_stateMachine overlay stack |
| SM-04 | Fix HandleEvent to use m_stateMachine.currentType() | 3 raw state reads at Cereka.cpp:296,303, and advance-key check at 304-306 |
| UI-01 | Extract UIManager class with IRenderContext dependency | ~350 lines of SDL-drenched rendering across cereka_draw.cpp, state draw methods, save overlay, UI config — all move behind interface |
| RR-01 | Define IRenderContext + ITexture interfaces | SDL render types leak into 5+ classes: CerekaImpl, SceneManager, ConfigManager, UiConfig, text_renderer |
| RR-02 | Move SDL_Renderer* behind IRenderContext | Found in: CerekaImpl.renderer, SceneManager.renderer, ApplyContext.renderer, text_renderer functions |
| RR-03 | Move SDL_Texture* behind ITexture | Found in: SceneManager (bg/characters), UiConfig.textbox/namebox/button.images, CerekaImpl font textures |
| CS-01 | Safe numeric parser returning std::expected | 5 unguarded stoi/stof calls: cereka_script.cpp:271,281; property_handlers.cpp:47,56; config_manager.cpp:106,111 |
| CS-02 | Bounds-check CALL stack at runtime | cereka_script.cpp:145 has no guard; compiler has MAX_DEPTH=32 for includes but not for runtime CALL |
| CS-03 | Clamp restored pc after LoadGame | cereka_save.cpp:163 assigns data.programCounter directly; no [0, program.size()) check |
| CS-04 | Derive save path from game.cfg location | cereka_save.cpp:24 uses relative "saves/slot{N}.json"; runner sets cwd but save path should be absolute |
| CS-05 | Compile error propagation | cereka_instruction.cpp:240-243 returns empty vector on error; runner only checks empty, loses error message |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| State unification | Engine core (CerekaImpl / ICerekaStateContext) | Concrete state classes | The machine lives at engine level, states receive dispatch via ICerekaStateContext |
| UI rendering | UIManager | State draw() methods | UIManager owns all draw + layout + config; states call UIManager methods |
| Renderer abstraction | IRenderContext / ITexture | UIManager, SceneManager | Interface boundary keeps SDL types behind it; SceneManager consumes IRenderContext for texture operations |
| Crash safety | ScriptInterpreter, save.cpp, instruction.cpp | State machine validation | Runtime safety fixes localized to specific utility functions and the save/load pipeline |
| CI infrastructure | GitHub Actions workflow | CMakeLists.txt tweaks | Infra prerequisite; minimal changes to .yml and CMake guards |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++23 | N/A | Language standard | Already set in CMakeLists.txt:4 — `set(CMAKE_CXX_STANDARD 23)` |
| std::expected | C++23 stdlib | Error propagation | Already imported in cereka_state.hpp:28; no functional usage yet — this phase adopts it for crash safety |
| Glaze | v4+ (vendored) | JSON serialization | Already used for save format; stable, header-only, round-trip tested in save_data_test.cpp |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|--------------|
| SDL3 | vendored | Existing renderer implementation | Stays behind IRenderContext; SdlRenderContext implements the interface |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Dedicated IRenderContext | Continue using SDL_Renderer* everywhere | Blocks testing, future renderer swap, and scene graph (Phase 4) |
| std::expected for safe parsing | try/catch wrappers | std::expected composes better, forces caller to handle errors, typed error propagation |

**Version verification:** Standard library — no npm/pip equivalent to verify. std::expected is C++23, confirmed available via CMakeLists.txt requiring C++23.

## Architecture Patterns

### System Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                         CerekaEngine (pImpl)                      │
│  CerekaEngine → CerekaImpl                                        │
└──────────────────────────┬───────────────────────────────────────┘
                           │ owns
           ┌───────────────┼────────────────┬──────────────────┐
           ▼               ▼                ▼                  ▼
   ┌───────────────┐ ┌──────────┐  ┌──────────────┐  ┌──────────────┐
   │  StateMachine  │ │UIManager │  │ ScriptInterp  │  │  AudioMgr   │
   │  (overlay      │ │ (D-03)   │  │ er            │  │ (reference  │
   │   stack)       │ │ ┌──────┐ │  │              │  │  pattern)   │
   │  ┌──────────┐  │ │ │IRender│ │  │ program[]    │  │             │
   │  │Dialogue  │  │ │ │Context│ │  │ pc, callStack │  │             │
   │  │ Menu     │  │ │ │(D-04) │ │  │ variables     │  │             │
   │  │ Fade     │  │ │ └──────┘ │  │ skipMode      │  │             │
   │  │ SaveLoad │  │ │ UiConfig │  │              │  │             │
   │  └──────────┘  │ │ ConfigMgr │  │              │  │             │
   └───────┬───────┘ └──────────┘  └──────┬───────┘  └──────────────┘
           │                              │
           │ delegates update/draw/event  │ dispatch loop (D-02)
           ▼                              ▼
   ┌──────────────────┐     ┌────────────────────────┐
   │ SceneManager     │     │ CerekaScriptTick()     │
   │ (bg/char tex)    │     │ →op switch             │
   │ - ITexture* bg   │     │   SAY→WaitingForInput  │
   │ - ITexture* chars│     │   BG→SceneManager      │
   └──────────────────┘     │   CALL→push pc+1       │
                            │   JUMP→pc=labelMap     │
                            └────────────────────────┘

   Data flow (primary: script execution):
   .crka → Lua compiler → Instruction[] → CerekaScriptTick dispatch
   → SceneManager/AudioManager/DialogueSystem state changes
   → StateMachine transitions → UIManager rendering via IRenderContext
```

### Recommended Project Structure
```
src/
├── renderer/                    # NEW — renderer abstraction
│   ├── irender_context.hpp      # IRenderContext interface
│   ├── irecture.hpp             # ITexture interface
│   └── sdl_render_context.hpp   # SdlRenderContext implementation
├── ui/                          # NEW — UIManager (extracted from CerekaImpl)
│   ├── ui_manager.hpp           # UIManager class declaration
│   ├── ui_manager.cpp           # UIManager implementation (from cereka_draw.cpp + state draw)
│   ├── ui_config.hpp            # moved from src/cereka_ui_config.hpp
│   └── ui_config.cpp            # moved from src/cereka_ui_config.cpp
├── state/                       # EXISTING — state machine (unchanged structure)
│   ├── cereka_state.hpp         # ICerekaStateContext gets new methods? Check D-02 needs
│   ├── cereka_states.hpp
│   └── cereka_states.cpp        # MODIFIED — state draw methods delegate to UIManager
├── config/                      # EXISTING — property map system
│   ├── config_manager.hpp       # MODIFIED — ApplyContext uses IRenderContext& instead of SDL_Renderer*
│   ├── config_manager.cpp
│   ├── property_handlers.cpp    # MODIFIED — SDL_Color → Color (or keep behind compat), Texture via IRenderContext
│   └── property_types.hpp       # MODIFIED — SDL types behind interface
├── cereka_engine_impl.hpp       # MODIFIED — remove state/stateBeforeSaveMenu, replace SDL members with UIManager+IRenderContext
├── Cereka.cpp                   # MODIFIED — state bridging removed, init creates UIManager
├── cereka_script.cpp            # MODIFIED — CerekaScriptTick body moves to DialogueState
├── cereka_draw.cpp              # MODIFIED → thin delegate to UIManager, or removed entirely
├── cereka_save.cpp              # MODIFIED — use absolute path, clamp pc, parseState correctly
├── cereka_scene_manager.hpp     # MODIFIED — SDL_Texture* → ITexture*
├── cereka_scene_manager.cpp
├── cereka_text_renderer.hpp     # MODIFIED — return ITexture*, take IRenderContext&
├── cereka_text_renderer.cpp
├── ... (other files unchanged)
```

### Pattern 1: Subsystem Extraction (AudioManager Pattern)
**What:** The AudioManager (`cereka_audio_manager.hpp/.cpp`) is the reference for subsystem extraction. It's a standalone class with Init/Shutdown lifecycle, no SDL types in its `.hpp` public surface (except vendor MIX types), and is used via direct `.` calls from CerekaScriptTick.

**When to use:** For UIManager extraction. Follow the same pattern: standalone .hpp/.cpp pair, Init() takes IRenderContext& dependency, used via `ui.DrawBackground(ctx)` from state draw methods.

**Existing reference:**
```cpp
// cereka_audio_manager.hpp — clean standalone class
class AudioManager {
public:
    bool Init();
    void Shutdown();
    void PlayBGM(const std::string &filename);
    // ...
private:
    MIX_Mixer *mixer = nullptr;  // vendor types OK in private impl
};
```
[VERIFIED: src/cereka_audio_manager.hpp]

### Pattern 2: CRTP State Machine with ICerekaStateContext
**What:** States communicate with the engine only through `ICerekaStateContext` interface. Concrete states use `static_cast<Impl&>(ctx)` to access engine internals for dispatch.

**When to use:** For moving CerekaScriptTick dispatch into DialogueState. The existing pattern of ctx → Impl cast is already used by all concrete states.

**Existing example (cereka_states.cpp:14-19):**
```cpp
void DialogueState::update(float dt, ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.CerekaScriptTick();
}
```

**Target pattern:**
```cpp
void DialogueState::update(float dt, ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    auto &si = impl.scriptInterpreter;
    // ~240-line switch body moves HERE from cereka_script.cpp
    // changeState() calls ctx.changeState() instead of impl.changeState()
    // No guard on state enum — the machine only calls update() when Running
}
```
[VERIFIED: src/state/cereka_state.hpp, src/state/cereka_states.cpp]

### Pattern 3: pImpl Renderer Abstraction
**What:** `CerekaEngine → CerekaImpl` follows the pImpl pattern. IRenderContext should follow the same: `CerekaEngine → CerekaImpl → UIManager → IRenderContext*` where the context knows about SDL3 but the engine doesn't.

**When to use:** For IRenderContext/ITexture design. The concrete SdlRenderContext stays in `cereka_video.cpp` (or a new `src/renderer/` directory), while the interface lives in `src/renderer/irender_context.hpp`.

### Anti-Patterns to Avoid
- **Synchronous state sync:** Current Cereka.cpp:90-132 syncs `state = m_stateMachine.currentType()` after every transition. After removing `state`, this sync disappears entirely — m_stateMachine is the source of truth.
- **SDL_Texture* in UiConfig:** Textures as raw SDL pointers inside theme structs prevents mock testing. After IRenderContext, UiConfig stores ITexture* (or texture handles).
- **Global using Impl:** The `using Impl = cereka::CerekaImpl` alias at engine_impl.hpp:131 makes cross-file refactoring harder. New files (UIManager, renderer) should not use this alias pattern.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Error propagation from numeric parsing | Custom if-else branching for every parse site | `std::expected<T, std::string>` safe parser | Already available (C++23, imported in cereka_state.hpp) — single factory function for all parse sites |
| Texture lifetime management | Manual destroy at every callsite | RAII wrapper or ITexture::destroy() in IRenderContext | SceneManager, UiConfig, draw.cpp all destroy textures manually — one pattern to rule them all |
| Error reporting from compiler | Return empty vector + stderr | `std::expected<std::vector<Instruction>, std::string>` | Runner currently loses error messages — expected propagates the error string to the caller |
| Save path derivation | Hardcoded relative path | `fs::absolute()` or store project root | Runner already computes projectRoot at startup — pass it to Impl/engine for save path construction |

**Key insight:** std::expected is already in the include graph (cereka_state.hpp:28) but has zero functional usage. This phase should be the one that puts it to work across all 5 crash-safety sites. A single `safe_stof` / `safe_stoi` utility returning `std::expected` eliminates the inconsistent try/catch pattern currently spread across the codebase.

## Common Pitfalls

### Pitfall 1: State Machine Test Breakage
**What goes wrong:** Tests in `cereka_script_test.cpp` create `Impl` directly and set `engine.state = CerekaState::Running` before calling `engine.CerekaScriptTick()`. After removing `state`, these assignments won't compile.

**Root cause:** Tests directly manipulate the raw enum that's being removed. The state machine was never initialized (no setContext/setInitialState) in the test.

**How to avoid:** After unification, tests should call `engine.m_stateMachine.setInitialState(CerekaState::Running)` instead of `engine.state = CerekaState::Running`. Or better, have CerekaImpl's constructor initialize the state machine automatically.

**Warning signs:** 5 test references to `engine.state` at cereka_script_test.cpp:41,53,73,83,113,117.

### Pitfall 2: DialogueState::update() Resumption Semantics
**What goes wrong:** Moving the 240-line dispatch loop into DialogueState must preserve the "return mid-tick, resume next frame" behavior for SAY/NARRATE/FADE/MENU.

**Root cause:** The dispatch loop currently returns early (after `changeState(WaitingForInput)`), and the next frame's Update() calls DialgueState::update() which calls back into CerekaScriptTick. This works because the guard `if (state != CerekaState::Running) return;` stops re-entry until advance key transitions back to Running.

**How to avoid:** The guard must move from checking `state` to checking `m_stateMachine.currentType() == CerekaState::Running`. Since DialogueState::update() is only called when the machine is in Dialogue (Running), the state machine's lifecycle already enforces this. BUT — after advance key (which transitions from WaitingForInput → Running), the next frame's dialogueState::update() fires again. This is correct and already works.

**Warning signs:** Tick dispatch that doesn't advance but changes state — verify that the state machine transition happens, then DialogueState won't be called again until the state returns to Running.

### Pitfall 3: ConfigManager ApplyContext Still Holds SDL_Renderer*
**What goes wrong:** After IRenderContext abstraction, `config::ApplyContext` still holds `SDL_Renderer *renderer` (property_types.hpp:49). If texture-related properties (textbox.image, etc.) are applied after the migration, they crash or bypass the abstraction.

**Root cause:** The ApplyContext struct passes raw SDL pointers to property handlers. This must be updated to use IRenderContext.

**How to avoid:** Replace `SDL_Renderer *renderer` in ApplyContext with `IRenderContext *renderCtx`. Update `applyTexture` handler in property_handlers.cpp to call `renderCtx->createTexture()` instead of `IMG_LoadTexture(renderer, ...)`.

**Warning signs:** Any reference to `SDL_Renderer*` in config/ directory or property_handlers.cpp.

## Code Examples

### Workstream 1: State Machine Unification — Dispatch Move

**Current pattern** (cereka_script.cpp:62-68):
```cpp
void Impl::CerekaScriptTick()
{
    if (state != CerekaState::Running)
        return;
    // ... 240-line switch ...
}
```

**Target pattern** (in DialogueState::update):
```cpp
void DialogueState::update(float dt, ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    auto &si = impl.scriptInterpreter;

    // The guard is now implicit — machine only calls update() when in Running state
    while (si.pc < si.program.size()) {
        const auto &ins = si.program[si.pc];
        switch (ins.op) {
            case compiler::Op::SAY:
                impl.Say(ins.a, ins.a, ins.b);
                ctx.changeState(CerekaState::WaitingForInput);
                si.pc++;
                return;  // return mid-tick — machine transitions away

            case compiler::Op::FADE: {
                float dur = 0.5f;
                if (!ins.b.empty()) {
                    auto parsed = safe_stof(ins.b);  // D-05: safe parser
                    if (parsed) dur = *parsed;
                }
                impl.scene.StartFade(ins.a, dur);
                ctx.changeState(CerekaState::Fading);
                si.pc++;
                return;
            }
            // ... rest of op cases ...
        }
    }
}
```
[VERIFIED: src/cereka_script.cpp, src/state/cereka_states.cpp]

### Workstream 2: UIManager Extraction — IRenderContext Dependency Injection

**Target pattern:**
```cpp
// ui/ui_manager.hpp
class UIManager {
public:
    UIManager(IRenderContext &renderCtx);

    void DrawBackground(const SceneManager &scene);
    void DrawCharacters(const SceneManager &scene);
    void DrawDialogueBox(const DialogueSystem &dialogue,
                         const UiConfig &uiCfg,
                         int screenW, int screenH);
    void DrawMenuButtons(const MenuSystem &menu,
                         const UiConfig &uiCfg,
                         int screenW, int screenH);
    void DrawSaveLoadOverlay(bool isSaving,
                             const std::string &timestamps[10],
                             int screenW, int screenH);
    void DrawFade(const SceneManager &scene,
                  int screenW, int screenH);

private:
    IRenderContext &m_renderCtx;
};
```

**Key insight:** UIManager does NOT hold UiConfig or ConfigManager as owned members — it takes them as parameters or references. This keeps UIManager a rendering orchestrator while ownership of config/theme stays at the engine level (or is delegated per D-03's flexibility). [CITED: D-03 says "UiConfig and ConfigManager live under UIManager ownership" — this part needs confirmation during planning.]

### Workstream 3: IRenderContext Interface

```cpp
// renderer/irender_context.hpp
struct Color { uint8_t r, g, b, a; };
struct Rect { float x, y, w, h; };

class ITexture {
public:
    virtual ~ITexture() = default;
    virtual float Width() const = 0;
    virtual float Height() const = 0;
};

class IRenderContext {
public:
    virtual ~IRenderContext() = default;

    // Lifecycle
    virtual void Clear(Color c) = 0;
    virtual void Present() = 0;

    // Drawing
    virtual void FillRect(Rect rect, Color c) = 0;
    virtual void FillScreen(Color c) = 0;  // nullptr rect
    virtual void DrawTexture(ITexture &tex,
                             Rect *srcRect,
                             Rect *dstRect) = 0;

    // Texture management
    virtual std::unique_ptr<ITexture> CreateTexture(
        const std::string &filepath) = 0;
    virtual std::unique_ptr<ITexture> CreateTextTexture(
        TTF_Font *font,
        const std::string &text,
        Color color) = 0;
    virtual std::unique_ptr<ITexture> CreateTextTextureWrapped(
        TTF_Font *font,
        const std::string &text,
        Color color,
        int wrapWidth) = 0;

    // Font
    virtual TTF_Font *GetFont() const = 0;

    // Blend mode
    virtual void SetBlendMode(BlendMode mode) = 0;

    // Dimensions
    virtual int Width() const = 0;
    virtual int Height() const = 0;
};
```
[ASSUMED] — Interface design patterns for renderer abstraction, not verified against external documentation. Adjust method signatures during planning to match actual usage in cereka_draw.cpp and scene_manager.cpp.

### Workstream 4: Crash Safety — Safe Numeric Parser

```cpp
// src/<utility header or in existing safe_parse.hpp>
#include <expected>
#include <string>

namespace cereka {

std::expected<float, std::string> safe_stof(const std::string &s) noexcept;
std::expected<int, std::string> safe_stoi(const std::string &s) noexcept;
std::expected<unsigned long long, std::string> safe_stoull(const std::string &s) noexcept;

}  // namespace cereka
```

**Usage at each crash site:**
```cpp
// cereka_script.cpp:271 — Before:  int slot = ins.a.empty() ? 0 : std::stoi(ins.a);
// After:
int slot = 0;
if (!ins.a.empty()) {
    auto parsed = safe_stoi(ins.a);
    slot = parsed.value_or(0);
}

// cereka_save.cpp:163 — Before:  scriptInterpreter.pc = data.programCounter;
// After:
scriptInterpreter.pc = std::min(data.programCounter,
                                scriptInterpreter.program.empty()
                                    ? 0
                                    : scriptInterpreter.program.size() - 1);

// cereka_script.cpp:145 — Before:  si.callStack.push_back(si.pc + 1);
// After:
if (si.callStack.size() >= 32) {
    std::cerr << "[CEREKA] Call stack overflow\n";
    ctx.changeState(CerekaState::Finished);
    return;
}
si.callStack.push_back(si.pc + 1);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| State machine states are empty stubs (CONCERNS.md:8) | MenuState, FadeState, SaveMenuState, LoadMenuState all have real logic | Phase 2 (02-03-PLAN.md) | RESEARCH MUST UPDATE: the state machine consolidation path is shorter than originally anticipated |
| Save format split: .sav + .json | Glaze JSON unified | Phase 2 (02-04-PLAN.md) | UB enum cast from old integer format is no longer a concern — `parseState()` handles all save reads |
| Nested if/else VM bug (`skipDepth` reset) | Fixed | Phase 2 (02-02-PLAN.md) | One fewer crash risk to fix in D-05 |

### Deprecated/outdated:
- **CONCERNS.md §7:** "All 7 concrete state methods in cereka_states.cpp are empty stubs" — OUTDATED. Only FinishedState and QuitState have no overrides.
- **CONCERNS.md §10:** "Save format split" — OUTDATED. Unification was completed in Phase 2.
- **CONCERNS.md §8:** "Nested if/else VM bug" — OUTDATED. Fixed in Phase 2.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | std::expected is importable and usable across all C++23 toolchains (GCC, Clang, MSVC) | Crash Safety | Must verify with vendored toolchain — if not, use std::optional + error string instead |
| A2 | TTF_Font* can remain as a raw SDL type behind IRenderContext (not wrapped in ITexture) | Renderer Abstraction | Low risk — font is an SDL_ttf construct, not a renderer construct. The text rendering functions create/destroy textures via IRenderContext but keep TTF_Font* separate |
| A3 | IRenderContext can be fully defined without pulling in SDL headers | Renderer Abstraction | Must verify that Color, Rect structs don't collide with SDL names; SDL3 types used in current ApplyContext (SDL_Renderer*, SDL_Texture*) must be completely removed from the interface |

## Open Questions

1. **UiConfig ownership after UIManager extraction**
   - What we know: D-03 says "UIManager owns all draw + layout + config" and "UiConfig and ConfigManager live under UIManager ownership."
   - What's unclear: ConfigManager currently lives on CerekaImpl and is initialized in InitGame. If it moves to UIManager, does CerekaImpl push UI config changes through UIManager? Or does UIManager expose ConfigManager as a public member?
   - Recommendation: Keep ConfigManager on CerekaImpl for now (it's also used by ApplyUiSet from dispatch), have UIManager receive UiConfig as a const& parameter in draw methods. Full ownership transfer can be deferred to a cleanup pass.

2. **ApplyContext SDL types — replace or wrap?**
   - What we know: ApplyContext holds `SDL_Renderer*`, `SDL_Texture*&` in callbacks. SDL_Color is used throughout property_types.hpp and property_handlers.cpp.
   - What's unclear: Should ApplyContext use a custom Color struct (no SDL dependency) or keep SDL_Color as it's purely internal to the config system? Should IRenderContext methods take custom Color or SDL_Color?
   - Recommendation: Create a custom `cereka::Color{uint8_t r,g,b,a}` struct for the interface boundary. Use it in IRenderContext methods. Internally, SdlRenderContext converts to SDL_Color. This keeps SDL types out of the interface. The property handlers can use the custom Color too.

3. **SceneManager's SDL_Renderer* dependency**
   - What we know: SceneManager::Init(SDL_Renderer*) stores renderer for texture loading. SceneManager::loadBg() calls `IMG_LoadTexture(renderer, ...)`.
   - What's unclear: Does SceneManager take IRenderContext& instead, or does it store IRenderContext* separately? The textures it manages (ITexture*) already change with the interface.
   - Recommendation: SceneManager takes IRenderContext& in Init(). Texture loading goes through `m_renderCtx.CreateTexture(...)`.

4. **Text rendering — where does TTF_Font* live?**
   - What we know: CerekaImpl holds TTF_Font* font. RenderText/RenderTextWrapped take TTF_Font* + SDL_Renderer* and return SDL_Texture*.
   - What's unclear: After IRenderContext, the text rendering functions still need a TTF_Font* (which is SDL_ttf type, not SDL_render type). Should UIManager hold the font? Should the text renderer functions be methods on IRenderContext?
   - Recommendation: Add CreateTextTexture / CreateTextTextureWrapped to IRenderContext that take TTF_Font*, text, color, wrapWidth. The font is stored in CerekaImpl (or UIManager) and passed to IRenderContext's text methods. SdlRenderContext internally calls SDL_ttf to create surfaces and SDL3 to create textures from them.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| C++23 compiler | All workstreams | ✓ | Clang 16+ (macOS) | — |
| CMake | Build system | ✓ | 3.24+ | — |
| Git | Version control | ✓ | — | — |
| Ninja | Build system | ✓ | — | — |
| SDL3 (vendored) | Renderer, SdlRenderContext | ✓ | vendored | — |
| SDL3_ttf (vendored) | Text rendering via IRenderContext | ✓ | vendored | — |
| SDL3_image (vendored) | Texture loading via IRenderContext | ✓ | vendored | — |
| GoogleTest (FetchContent) | Tests | ✓ | v1.14.0 | — |
| Glaze (vendored) | Save serialization | ✓ | vendored | — |
| Qt6 | Launcher (CI only) | ✗ on macOS CI | — | Add `brew install qt` step |
| libxtst-dev | SDL3 on Linux CI | ✗ | — | Add to apt-get install |
| llvm-mingw | Cross-compile CI | ✓ on CI runner | — | — |

**Missing dependencies with no fallback:**
- None. All missing deps (Qt6 on macOS, libxtst on Linux) are fixed in D-01.

**Missing dependencies with fallback:**
- Cross-compile Qt6: Guard `add_subdirectory(launcher)` with `if(NOT CMAKE_CROSSCOMPILING)` in root CMakeLists.txt.

## Validation Architecture

> `workflow.nyquist_validation` key absent from .planning/config.json — treating as enabled per instructions.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.14.0 (FetchContent) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `ninja -C build cereka_test && ./build/tests/cereka_test` |
| Full suite command | `ninja -C build cereka_test && ./build/tests/cereka_test && lua tests/compile/harness.lua` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CI-01/02/03 | CI green on all 3 platforms | manual (CI run) | Push trigger on PR | ✅ .github/workflows/ci.yml |
| SM-01 | Dispatch loop in DialogueState | unit | `cereka_script_test` — refactored | ❌ Wave 0 |
| SM-02 | No raw `state` reads | compile-time | Ensure tests compile without `engine.state` | ❌ Wave 0 |
| UI-01 | UIManager renders correctly | integration | Manual (no mock IRenderContext yet) | ❌ Wave 0 |
| RR-01 | IRenderContext interface compiles | unit | Compile check | ❌ Wave 0 |
| CS-01 | Safe parser rejects bad input | unit | New test file or added to existing | ❌ Wave 0 |
| CS-02 | CALL stack overflow handled | unit | New test in cereka_script_test.cpp | ❌ Wave 0 |
| CS-03 | Corrupted save pc clamped | unit | New test in save_data_test.cpp | ❌ Wave 0 |
| CS-04 | Save path absolute | unit | Check savePath returns absolute | ❌ Wave 0 |
| CS-05 | Compile errors propagate | unit | Check expected error string | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `ninja -C build cereka_test 2>&1 | tail -5`
- **Per wave merge:** `ninja -C build cereka_test && ./build/tests/cereka_test`
- **Phase gate:** Full suite green + 3 CI jobs green

### Wave 0 Gaps
- [ ] `tests/cereka_script_test.cpp` — tests reference `engine.state` which must be removed; SM-01 refactoring breaks these tests without updates
- [ ] `tests/save_data_test.cpp` — could add CS-03 test for pc clamping
- [ ] No mock IRenderContext exists for UIManager testing — Wave 0 could add a minimal mock
- [ ] Compile-output snapshot tests (`tests/compile/`) — not affected by Phase 3 work (compiler unchanged)

## Security Domain

> No security enforcement is required for this phase. The engine does not handle user data, authentication, or network I/O. Crash safety (D-05) is an operational reliability concern, not a security concern per ASVS.

## Sources

### Primary (HIGH confidence)
- **All source files read and verified** — every code excerpt cited in this document has been read in full from the working directory.
- `src/state/cereka_state.hpp` — State machine design, ICerekaStateContext interface, overlay stack
- `src/state/cereka_states.hpp` — Concrete state declarations
- `src/state/cereka_states.cpp` — Concrete state implementations (not empty stubs)
- `src/cereka_engine_impl.hpp` — CerekaImpl god object members (state, stateBeforeSaveMenu, SDL types)
- `src/cereka_script.cpp` — 240-line CerekaScriptTick dispatch, crash sites
- `src/Cereka.cpp` — HandleEvent, state bridging, test references
- `src/cereka_draw.cpp` — All per-frame rendering (108 lines of SDL code)
- `src/cereka_save.cpp` — Save/load with crash risks (3 unguarded locations)
- `src/cereka_ui_config.hpp` — UiConfig with SDL_Texture* members
- `src/config/config_manager.hpp` — Property Map pattern
- `src/config/property_types.hpp` — ApplyContext with SDL_Renderer* and SDL_Color
- `src/config/property_handlers.cpp` — stoi/stof without try/catch
- `src/config/config_manager.cpp` — stoi/stof without try/catch
- `src/cereka_scene_manager.hpp` / `.cpp` — SDL_Renderer* + SDL_Texture* usage
- `src/cereka_audio_manager.hpp` — Reference extraction pattern
- `src/cereka_text_renderer.hpp` / `.cpp` — SDL_ttf text rendering returning SDL_Texture*
- `src/cereka_video.hpp` / `.cpp` — Window/renderer creation
- `src/compiler/cereka_instruction.hpp` / `.cpp` — MAX_DEPTH=32, compile errors swallowed
- `tests/cereka_script_test.cpp` — 5 references to `engine.state` that will break
- `.github/workflows/ci.yml` — CI configuration (missing deps)

### Secondary (MEDIUM confidence)
- `src/CMakeLists.txt` — `file(GLOB_RECURSE)` means new files in new directories (renderer/, ui/) will be picked up automatically
- `cmake/toolchains/ucrt64.cmake` — Cross-compile toolchain; no Qt6 available
- `launcher/CMakeLists.txt` — `find_package(Qt6 ... REQUIRED)` — fails on cross-compile
- `.planning/codebase/CONCERNS.md` — Outdated claims noted in State of the Art section
- `.planning/codebase/ARCHITECTURE.md` — System overview

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — All libraries verified against source files
- Architecture: HIGH — All code patterns verified by reading actual implementations
- Pitfalls: HIGH — Test breakage and resumption semantics verified by reading tests and dispatch logic
- Crash risks: HIGH — All 6 risks verified by reading exact line numbers and code paths
- CI: HIGH — CI config, launcher CMakeLists, and toolchain all read and verified

**Research date:** 2026-05-07
**Valid until:** 2026-06-07 (30-day estimate — C++23 compiler/stdlib and engine code are stable)
