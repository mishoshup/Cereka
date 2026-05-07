---
phase: 03-architectural-cleanup
plan: 02
subsystem: renderer
tags: [irrendercontext, itexture, sdl-abstraction, migration]

# Dependency graph
requires:
  - phase: 02-engine-modularization
    provides: State machine, scene manager, config system
provides:
  - IRenderContext pure virtual interface (Clear, Present, FillRect, FillScreen, DrawTexture, texture factories)
  - ITexture pure virtual interface (Width, Height)
  - Color (uint8_t RGBA) and Rect (float xywh) POD structs, avoiding SDL type leaks
  - SdlRenderContext — full SDL3 implementation of IRenderContext
  - SceneManager updated to use IRenderContext& and ITexture* instead of SDL_Renderer*/SDL_Texture*
  - Config system (ApplyContext, ApplyValue, handlers, serializers) using IRenderContext/Color/ITexture
  - UiConfig using ITexture* and Color instead of SDL_Texture* and SDL_Color
  - CerekaImpl::m_renderCtx (unique_ptr<IRenderContext>) replacing raw SDL_Renderer*
  - NativeRenderer() escape hatch for existing draw code during migration
affects: [03-03-ui-manager, 04-scene-graph]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "interface-based render abstraction (IRenderContext/ITexture)"
    - "SDL types behind implementation boundary"
    - "inner SdlTexture class wrapping SDL_Texture* behind ITexture"

key-files:
  created:
    - src/renderer/irender_context.hpp
    - src/renderer/irecture.hpp
    - src/renderer/sdl_render_context.hpp
    - src/renderer/sdl_render_context.cpp
  modified:
    - src/cereka_scene_manager.hpp/cpp
    - src/cereka_text_renderer.hpp/cpp
    - src/config/property_types.hpp
    - src/config/property_handlers.cpp
    - src/config/config_manager.hpp
    - src/config/config_manager.cpp
    - src/cereka_ui_config.hpp
    - src/cereka_engine_impl.hpp
    - src/Cereka.cpp
    - src/cereka_ui_config.cpp
    - src/cereka_draw.cpp
    - src/state/cereka_states.cpp
    - src/cereka_save.cpp
    - tests/config_test.cpp

key-decisions:
  - "IRenderContext interface uses Color/Rect PODs instead of SDL_Color/SDL_FRect to avoid type leakage"
  - "NativeRenderer() escape hatch left in IRenderContext for existing draw code that uses raw SDL_Renderer*"
  - "RawTexture() escape hatch added to ITexture for existing SDL_RenderTexture calls in draw code"
  - "TTF_Font* stays as raw SDL type (not wrapped) — font is a parameter to CreateTextTexture methods"
  - "RenderText/RenderTextWrapped on CerekaImpl kept returning SDL_Texture* for backwards compat with draw code (will change in 03-03)"
  - "renderer member kept in CerekaImpl for existing draw code; populated from NativeRenderer() at InitGame"

patterns-established:
  - "Render abstraction boundary: engine code sees only IRenderContext/ITexture, never SDL types"
  - "Texture ownership via unique_ptr/shared_ptr in SceneManager, raw ITexture* in UiConfig (managed by engine lifecycle)"
  - "Font rendering pipeline moved from text_renderer namespace functions to IRenderContext methods"

requirements-completed: [RR-01, RR-02, RR-03]

# Metrics
duration: 19 min
completed: 2026-05-07
---

# Phase 03 Plan 02: IRenderContext Abstraction Boundary Summary

**Create IRenderContext/ITexture interface boundary separating engine logic from SDL3 render types, replacing SDL_Renderer* and SDL_Texture* in SceneManager, config system, and UiConfig**

## Performance

- **Duration:** 19 min
- **Started:** 2026-05-07T08:23:45Z
- **Completed:** 2026-05-07T08:42:52Z
- **Tasks:** 3
- **Files modified:** 20 (4 created, 16 modified)

## Accomplishments
- Created `src/renderer/` directory with IRenderContext, ITexture, SdlRenderContext, and supporting POD types (Color, Rect)
- SceneManager no longer references SDL_Renderer* or SDL_Texture* — uses IRenderContext& and ITexture*
- Text renderer stripped to init_ttf()/OpenFont() only; RenderText/RenderTextWrapped moved to IRenderContext methods
- Config system (ApplyContext, handlers, serializers) uses IRenderContext* and ITexture* instead of SDL types
- UiConfig uses ITexture* for images and Color for RGBA values — no SDL_Texture* or SDL_Color in configurable theme
- CerekaImpl::m_renderCtx (unique_ptr) replaces raw SDL_Renderer*, with renderer kept as backwards-compat alias
- InitGame creates SdlRenderContext and passes it to SceneManager and ConfigManager
- All 21 unit tests pass, build succeeds

## Task Commits

Each task was committed atomically:

1. **Task 1: Define IRenderContext, ITexture interfaces + SdlRenderContext impl** - `fdd4397` (feat)
2. **Task 2: Update SceneManager + text_renderer to use IRenderContext** - `b04864d` (refactor)
3. **Task 3: Update config system, UiConfig, CerekaImpl for IRenderContext** - `21dd0b3` (refactor)

**Plan metadata:** (committed below)

## Files Created/Modified
- `src/renderer/irecture.hpp` - ITexture pure virtual interface (Width, Height, RawTexture)
- `src/renderer/irender_context.hpp` - IRenderContext interface + Color + Rect POD structs
- `src/renderer/sdl_render_context.hpp` - SdlRenderContext declaration with inner SdlTexture
- `src/renderer/sdl_render_context.cpp` - Full SDL3 implementation delegating to SDL3, SDL3_ttf, SDL3_image
- `src/cereka_scene_manager.hpp/cpp` - Now uses IRenderContext& and shared_ptr<ITexture>
- `src/cereka_text_renderer.hpp/cpp` - Only init_ttf/OpenFont remain; RenderText functions removed
- `src/config/property_types.hpp` - ApplyContext uses IRenderContext*, Color, ITexture*
- `src/config/property_handlers.cpp` - Updated applyColor/applyTexture/serializeColor signatures
- `src/config/config_manager.hpp/cpp` - asColor() returns Color
- `src/cereka_ui_config.hpp` - ITexture* and Color replace SDL types
- `src/cereka_engine_impl.hpp` - m_renderCtx + renderer (backwards compat)
- `src/Cereka.cpp` - InitGame creates SdlRenderContext, ShutDown via NativeRenderer()
- `src/cereka_ui_config.cpp` - InitConfigManager uses m_renderCtx
- `src/cereka_draw.cpp` - RawTexture() calls for existing draw code
- `src/state/cereka_states.cpp` - RawTexture() for button.image
- `src/cereka_save.cpp` - cereka::Color for RenderText calls
- `tests/config_test.cpp` - asColor() return type updated

## Decisions Made
- **SdlTexture as private inner class**: Cleanly encapsulates SDL_Texture* management within SdlRenderContext. The enclosing class accesses it via RawTexture() accessor rather than direct member access (C++ nested class access rules).
- **TTF_Font* stays unwrapped**: Font pointers are pure SDL_ttf types with no renderer dependency. They're passed as parameters to CreateTextTexture methods on IRenderContext rather than being stored in the interface.
- **RenderText kept returning SDL_Texture***: The existing draw code (cereka_draw.cpp, cereka_states.cpp, cereka_save.cpp) creates textures for immediate render-and-destroy patterns. Changing the return type to unique_ptr<ITexture> would require rewriting all callers, which is deferred to 03-03 (UIManager extraction).
- **renderer member kept in CerekaImpl**: Existing draw code references `impl.renderer` extensively for SDL_RenderFillRect, SDL_RenderTexture, etc. Removing it would break 5+ files. It's populated from `m_renderCtx->NativeRenderer()` at InitGame.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added RawTexture() to ITexture for existing draw code compatibility**
- **Found during:** Task 3
- **Issue:** UiConfig now stores ITexture* for images, but existing draw code (cereka_draw.cpp, cereka_states.cpp) passes these directly to SDL_RenderTexture() which expects SDL_Texture*
- **Fix:** Added `virtual SDL_Texture *RawTexture() const` to ITexture interface and implemented in SdlTexture. Updated draw code to call `image->RawTexture()` before passing to SDL functions.
- **Files modified:** src/renderer/irecture.hpp, src/renderer/sdl_render_context.hpp, src/renderer/sdl_render_context.cpp, src/cereka_draw.cpp, src/state/cereka_states.cpp
- **Verification:** Build succeeds, tests pass
- **Committed in:** 21dd0b3 (Task 3 commit)

**2. [Rule 3 - Blocking] Kept RenderText returning SDL_Texture* for draw code backwards compat**
- **Found during:** Task 3
- **Issue:** The plan specifies changing RenderText return type from SDL_Texture* to unique_ptr<ITexture>, but existing callers in draw.cpp, save.cpp, and states.cpp create textures for immediate one-frame use (render then destroy). Changing the return type would require rewriting all callers.
- **Fix:** Kept SDL_Texture* return type but changed parameter type from SDL_Color to Color. Implementation moved from text_renderer namespace functions to inline SDL_ttf calls in Cereka.cpp.
- **Files modified:** src/cereka_engine_impl.hpp, src/Cereka.cpp
- **Verification:** Build succeeds, tests pass
- **Committed in:** 21dd0b3 (Task 3 commit)

**3. [Rule 3 - Blocking] Kept renderer member in CerekaImpl**
- **Found during:** Task 3
- **Issue:** The plan specifies removing `SDL_Renderer *renderer` from CerekaImpl. However, cereka_draw.cpp, cereka_save.cpp, and cereka_states.cpp reference `impl.renderer` extensively for SDL_RenderFillRect, SDL_SetRenderDrawColor, etc.
- **Fix:** Added `renderer` back alongside `m_renderCtx`, populated from NativeRenderer() at InitGame. Marked with TODO(03-03) for removal.
- **Files modified:** src/cereka_engine_impl.hpp, src/Cereka.cpp
- **Verification:** Build succeeds, tests pass
- **Committed in:** 21dd0b3 (Task 3 commit)

**4. [Rule 3 - Blocking] Updated config_test.cpp for Color return type**
- **Found during:** Task 3
- **Issue:** PropertyValue::asColor() now returns Color instead of SDL_Color, but the unit test assigned it to SDL_Color
- **Fix:** Changed variable to `auto` (deduces Color)
- **Files modified:** tests/config_test.cpp
- **Verification:** All 21 tests pass
- **Committed in:** 21dd0b3 (Task 3 commit)

---

**Total deviations:** 4 auto-fixed (all Rule 3 - blocking)
**Impact on plan:** All auto-fixes necessary for build compatibility with existing draw code that was outside the plan's scope. The real migration (removing RawTexture() usage and the renderer field) will happen in 03-03 when UIManager extraction replaces all direct SDL draw calls.

## Issues Encountered
- **cereka_ui_config.hpp namespace mismatch:** UiConfig and Dim are at global scope, but ITexture/Color are in `cereka::`. Required qualified names (`cereka::ITexture`, `cereka::Color`) in the struct declarations.
- **Existing draw code outside plan scope:** Three draw files (cereka_draw.cpp, cereka_save.cpp, cereka_states.cpp) use `impl.renderer` and directly call SDL_RenderTexture with types that changed (UiConfig images, SceneManager characters). Required adding RawTexture() escape hatch to keep code compiling.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- IRenderContext boundary complete — SDL_Renderer* and SDL_Texture* are behind the interface in SceneManager, config system, and UiConfig
- Ready for Plan 03-03 (UIManager extraction) which will remove remaining direct SDL calls from draw code and eliminate the RawTexture()/NativeRenderer() escape hatches
- Draw code (cereka_draw.cpp, cereka_save.cpp, cereka_states.cpp) still uses raw SDL_Renderer* via backwards-compat fields — flagged for 03-03 removal

## Self-Check: PASSED
- Verified: Build succeeds (`ninja -C build -j12`)
- Verified: All 21 unit tests pass
- Verified: No SDL_Renderer* or SDL_Texture* in SceneManager, config system, or UiConfig headers
- Verified: NativeRenderer() escape hatch exists in irender_context.hpp
- Verified: 4 renderer files in src/renderer/

---
*Phase: 03-architectural-cleanup*
*Completed: 2026-05-07*
