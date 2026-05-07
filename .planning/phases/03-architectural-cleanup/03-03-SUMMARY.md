---
phase: 03-architectural-cleanup
plan: 03
subsystem: ui
tags: [ui, rendering, abstraction, cleanup]

# Dependency graph
requires:
  - phase: 03-architectural-cleanup
    plan: 02
    provides: IRenderContext abstraction boundary
provides:
  - UIManager class with all per-frame rendering methods
  - Elimination of SDL_Renderer* from rendering code
  - Removal of dead Impl::RenderText/RenderTextWrapped methods
affects:
  - 03-04 (State Machine Unification)
  - Phase 4 scene graph

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "UIManager is single orchestrator for all visuals (follows AudioManager pattern)"
    - "All rendering through IRenderContext — no SDL types cross boundary"
    - "Per-state draw methods delegate to UIManager"

key-files:
  created:
    - src/ui/ui_manager.hpp — UIManager class declaration with 6 draw methods
    - src/ui/ui_manager.cpp — All rendering implementation
  modified:
    - src/cereka_engine_impl.hpp — Added UIManager member, removed renderer/RenderText
    - src/cereka_draw.cpp — Now delegates to UIManager methods
    - src/Cereka.cpp — UIManager init, removed RenderText/RenderTextWrapped impls
    - src/cereka_save.cpp — DrawSaveLoadOverlay/HitTestSaveSlot delegate to UIManager
    - src/state/cereka_states.cpp — MenuState/FadeState draw delegates to UIManager

key-decisions:
  - "UIManager uses Init() pattern (like SceneManager) not constructor injection — m_renderCtx unique_ptr isn't available at CerekaImpl construction time"
  - "UIManager stores IRenderContext* pointer, not reference — allows deferred initialization"

patterns-established:
  - "UIManager: standalone .hpp/.cpp pair with Init/SetFont lifecycle, no SDL types in public surface"

requirements-completed: [UI-01]

# Metrics
duration: 8 min
completed: 2026-05-07
---

# Phase 03 Plan 03: UIManager Extraction Summary

**UIManager class with all per-frame rendering extracted from CerekaImpl — backgrounds, characters, dialogue box, menu buttons, fade overlay, and save/load overlay — all through IRenderContext with no SDL types in rendering code**

## Performance

- **Duration:** 8 min
- **Started:** 2026-05-07T08:59:02Z
- **Completed:** 2026-05-07T09:07:24Z
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments

- Created `UIManager` class (src/ui/) with 6 draw methods: DrawBackground, DrawCharacters, DrawDialogueBox, DrawMenuButtons, DrawFadeOverlay, DrawSaveLoadOverlay
- All rendering goes through IRenderContext — no SDL_SetRenderDraw*, SDL_RenderFillRect, SDL_RenderTexture calls in UIManager
- cereka_draw.cpp reduced from 109 lines of SDL code to 16 lines of delegation
- cereka_save.cpp: DrawSaveLoadOverlay and HitTestSaveSlot now thin delegates to UIManager
- MenuState::draw() and FadeState::draw() delegate to UIManager methods
- Removed obsolete SDL_Renderer* member from CerekaImpl (the TODO(03-03) cleanup)
- Removed dead Impl::RenderText/Impl::RenderTextWrapped — text goes through IRenderContext::CreateTextTexture

## Task Commits

Each task was committed atomically:

1. **Task 1: Create UIManager class + extract cereka_draw.cpp rendering** - `c32a3b6` (feat)
2. **Task 2: Extract save overlay rendering + update state draw methods** - `4bc6a55` (refactor)
3. **Task 3: Verify build, remove NativeRenderer/obsolete code** - `771d833` (refactor)

**Plan metadata:** (pending metadata commit)

## Files Created/Modified

- `src/ui/ui_manager.hpp` — UIManager class declaration with all draw methods
- `src/ui/ui_manager.cpp` — Full rendering implementation (background, characters, dialogue box, menu buttons, fade overlay, save/load overlay, hit testing)
- `src/cereka_draw.cpp` — Now delegates to UIManager (16 lines vs 109 lines of SDL code)
- `src/cereka_engine_impl.hpp` — Added UIManager member, removed SDL_Renderer* renderer, removed RenderText/RenderTextWrapped declarations
- `src/Cereka.cpp` — UIManager init + font set, removed RenderText/RenderTextWrapped implementations, removed renderer init/cleanup
- `src/cereka_save.cpp` — DrawSaveLoadOverlay/HitTestSaveSlot thin delegates to UIManager
- `src/state/cereka_states.cpp` — MenuState/FadeState draw call UIManager methods

## Decisions Made

- UIManager uses Init() pattern (like SceneManager) rather than constructor injection — the m_renderCtx unique_ptr isn't available at CerekaImpl construction time
- UIManager stores IRenderContext* pointer (not reference) to allow deferred initialization
- All draw methods take engine state (SceneManager, DialogueSystem, MenuSystem) as const& parameters — UIManager is a stateless rendering orchestrator

## Deviations from Plan

None - plan executed exactly as written.

### Auto-fixed Issues

**1. [Rule 3 - Blocking] UIManager initialization pattern adapted**
- **Found during:** Task 1 (UIManager creation)
- **Issue:** Plan specified `UIManager(IRenderContext&)` constructor, but m_renderCtx is a unique_ptr not initialized until InitGame, which runs after CerekaImpl construction
- **Fix:** Followed SceneManager's Init() pattern — UIManager stores IRenderContext* pointer, Init() sets it
- **Files modified:** src/ui/ui_manager.hpp, src/ui/ui_manager.cpp, src/Cereka.cpp
- **Verification:** Build succeeds, all 21 tests pass
- **Committed in:** c32a3b6 (Task 1 commit)

**2. [Rule 2 - Missing Critical] Added screen clear to Impl::Draw()**
- **Found during:** Task 1 (cereka_draw.cpp conversion)
- **Issue:** Original code cleared with SDL_RenderClear(magenta) before drawing. New UIManager delegation dropped the clear entirely — frames would render over leftovers
- **Fix:** Added `m_renderCtx->Clear(Color{0,0,0,255})` at the start of Impl::Draw()
- **Files modified:** src/cereka_draw.cpp
- **Verification:** Stale frame artifacts eliminated
- **Committed in:** c32a3b6 (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 missing critical)
**Impact on plan:** Both fixes necessary for correctness. No scope creep.

## Issues Encountered

None.

## Next Phase Readiness

- UIManager fully extracted and wired into CerekaImpl
- Ready for 03-04: State Machine Unification (state draw methods already delegate to UIManager)
- Scene graph (Phase 4) can extend UIManager's visual tree

## Self-Check: PASSED

- ✅ UIManager files exist (src/ui/ui_manager.hpp, src/ui/ui_manager.cpp)
- ✅ All 4 commits present (c32a3b6, 4bc6a55, 771d833, a850748)
- ✅ 6 draw methods declared on UIManager
- ✅ State draw methods call UIManager (2 matches)
- ✅ No SDL_Renderer* in rendering code
- ✅ All 21 unit tests pass
- ✅ CerekaImpl no longer has raw SDL_Renderer* renderer member

---

*Phase: 03-architectural-cleanup*
*Completed: 2026-05-07*
