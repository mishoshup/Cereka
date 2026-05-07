---
phase: 03-architectural-cleanup
plan: 04
subsystem: engine
tags: [state-machine, architecture, cpp, refactoring]

requires:
  - phase: 03-architectural-cleanup
    provides: Safe numeric parsers (safe_parse.hpp), Lua audio helpers, SceneManager/DialogueSystem/UIManager extraction
provides:
  - Dispatch loop moved from raw CerekaScriptTick into DialogueState::update()
  - Dual state representation eliminated (CerekaImpl::state + CerekaStateMachine)
  - CerekaStateMachine is the single source of truth for all state transitions
  - ICerekaStateContext interface simplified (getSavedState/setSavedState removed)
  - WaitingForInputState added for state machine completeness

tech-stack:
  added: []
  patterns:
    - State machine drives dispatch: DialogueState::update() contains the instruction dispatch loop
    - No raw state guard — machine lifecycle enforces correct state
    - effectiveState() for save serialization respects overlay stack

key-files:
  created: []
  modified:
    - src/state/cereka_states.hpp — Added WaitingForInputState
    - src/state/cereka_states.cpp — Dispatch loop moved here; LoadMenuState simplified
    - src/cereka_script.cpp — CerekaScriptTick removed
    - src/Cereka.cpp — changeState/pushOverlay/popOverlay simplified; HandleEvent uses machine; public API updated
    - src/cereka_engine_impl.hpp — state/stateBeforeSaveMenu removed; CerekaScriptTick removed; getSavedState/setSavedState removed
    - src/cereka_save.cpp — Save/Load use m_stateMachine
    - src/state/cereka_state.hpp — ICerekaStateContext simplified; effectiveState() added
    - include/Cereka/Cereka.hpp — CerekaScriptTick removed from public API
    - tests/cereka_script_test.cpp — Tests use m_stateMachine instead of raw state
    - runner/main.cpp — CerekaScriptTick call removed (Update handles dispatch)

key-decisions:
  - "Dispatch loop moves into DialogueState::update() — machine lifecycle enforces Running state, no guard needed"
  - "changeState/pushOverlay/popOverlay on CerekaImpl now delegate purely to m_stateMachine — no raw state fallback"
  - "effectiveState() added to CerekaStateMachine — returns overlay origin if overlays active, else currentType()"
  - "WaitingForInputState added (not in plan) — machine requires a registered state for every enum value used in changeState()"

requirements-completed: [SM-01, SM-02, SM-03, SM-04]

duration: 18 min
completed: 2026-05-07
---

# Phase 03 Plan 04: State Machine Unification Summary

**Dispatch loop moved into DialogueState::update(), dual state representation eliminated, CerekaStateMachine is now the single source of truth for all state transitions**

## Performance

- **Duration:** 18 min
- **Started:** 2026-05-07T09:08:00Z
- **Completed:** 2026-05-07T09:26:25Z
- **Tasks:** 3 (+2 deviation fixes)
- **Files modified:** 10

## Accomplishments

- CerekaScriptTick (~250-line dispatch switch) moved from cereka_script.cpp into DialogueState::update()
- CerekaImpl::state and CerekaImpl::stateBeforeSaveMenu removed — all state reads use m_stateMachine
- CerekaImpl::changeState/pushOverlay/popOverlay simplified — no raw state fallback, pure machine delegation
- HandleEvent uses m_stateMachine.currentType() for ESC/advance-key logic
- IsGameFinished/IsGameQuit use m_stateMachine.currentType() instead of raw pImplementation->state
- SaveGame serializes state from m_stateMachine.effectiveState() (respects overlay stack)
- LoadGame restores via m_stateMachine.clearOverlays() + changeState()
- ICerekaStateContext interface simplified — getSavedState/setSavedState removed
- CerekaStateMachine::effectiveState() added for save serialization
- WaitingForInputState added (idle state for the machine to find when dispatch pauses)
- All 3 VM tests updated to use m_stateMachine instead of raw engine.state
- Runner updated — CerekaScriptTick call removed (Update() handles dispatch)
- Full build + all 21 tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Move CerekaScriptTick dispatch into DialogueState::update()** - `16f8787` (feat)
2. **Task 2: Remove raw state/stateBeforeSaveMenu, unify on state machine** - `ad6c27c` (feat)
3. **Task 3: Update ICerekaStateContext interface + fix unit tests** - `79f5f3b` (refactor)

**Deviation fixes:**

4. **Add WaitingForInputState for machine completeness** - `193b9e8` (fix)
5. **Remove CerekaScriptTick call from runner** - `f2694e0` (fix)

## Files Created/Modified

- `src/state/cereka_states.hpp` - Added WaitingForInputState declaration
- `src/state/cereka_states.cpp` - Dispatch loop in DialogueState::update(); LoadMenuState simplified
- `src/cereka_script.cpp` - CerekaScriptTick removed from Impl (method + includes cleaned up)
- `src/Cereka.cpp` - ICerekaStateContext methods simplified; HandleEvent uses machine; public API uses machine
- `src/cereka_engine_impl.hpp` - state/stateBeforeSaveMenu/getSavedState/setSavedState/CerekaScriptTick removed
- `src/cereka_save.cpp` - Save/Load use m_stateMachine.effectiveState() and clearOverlays()+changeState()
- `src/state/cereka_state.hpp` - ICerekaStateContext simplified; effectiveState() on CerekaStateMachine
- `include/Cereka/Cereka.hpp` - CerekaScriptTick removed from public API
- `tests/cereka_script_test.cpp` - All 3 VM tests use m_stateMachine instead of raw engine.state
- `runner/main.cpp` - CerekaScriptTick call removed (Update() handles dispatch)

## Decisions Made

- **Dispatch in DialogueState::update()** — The machine lifecycle (currentType() check) acts as the guard, replacing the old `if (state != Running) return;` in CerekaScriptTick. Cleaner: no duplicated guard, the machine already knows.
- **No raw state fallback** — changeState/pushOverlay/popOverlay on CerekaImpl now call m_stateMachine directly without updating a shadow `state` member. The machine is the source of truth.
- **effectiveState() on machine** — When an overlay is active (e.g., save menu), the serialized state should be the gameplay state below the overlay, not the overlay state itself. The overlay stack's `.first` element captures this.
- **WaitingForInputState** — The machine requires a registered state for every enum value passed to changeState(). Without it, changeState(WaitingForInput) would leave currentState_=nullptr and break the advance-key flow. This wasn't in the plan but is structurally required.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added WaitingForInputState**
- **Found during:** Task 3 verification (tests failed)
- **Issue:** CerekaStateMachine requires a registered state for every enum value used in changeState(). Without WaitingForInputState, calling changeState(WaitingForInput) left currentState_=nullptr and currentType_ unchanged (stuck on Running), breaking the advance-key flow and all 3 VM tests.
- **Fix:** Added `WaitingForInputState` (empty CRTP state) to cereka_states.hpp; registered it in both Impl::InitGame() and test SetUp().
- **Files modified:** src/state/cereka_states.hpp, src/Cereka.cpp, tests/cereka_script_test.cpp
- **Verification:** All 21 tests pass including all 3 VM tests
- **Committed in:** 193b9e8 (Task 3 follow-up)

**2. [Rule 2 - Missing Critical] Removed CerekaScriptTick call from runner/main.cpp**
- **Found during:** Post-task audit
- **Issue:** runner/main.cpp still called engine.CerekaScriptTick() which was removed from the public API. The runner would fail to compile.
- **Fix:** Removed the CerekaScriptTick call — engine.Update() now encompasses the dispatch loop via DialogueState::update().
- **Files modified:** runner/main.cpp
- **Verification:** ninja CerekaGame target builds successfully
- **Committed in:** f2694e0 (post-Task 3)

---

**Total deviations:** 2 auto-fixed (2 missing critical)
**Impact on plan:** Both fixes were structurally required — the first for the state machine to function at all, the second for the runner to compile. No scope creep.

## Issues Encountered

- **CerekaStateMachine needs states for all enum values** — The machine's changeState() method calls `states_.find(newType)` and skips the transition if no matching state is registered. This means every CerekaState enum value passed to changeState() needs a registered state. The plan didn't account for this requirement for WaitingForInput.
- **runner/main.cpp still referenced deprecated API** — The runner wasn't in the plan's files_modified list but needed updating since CerekaScriptTick was removed from the public API.

## Threat Flags

None — no new security-relevant surface introduced.

## Next Phase Readiness

- State machine is now the single source of truth — ready for further architectural work
- No more dual state representation (raw enum + machine)
- The state machine `changeState()` now properly tracks transitions via registered states
- WaitingForInputState exists as a minimal idle state (no overrides needed)

## Self-Check: PASSED

All 10 modified files confirmed on disk. All 5 commits (3 planned + 2 deviation fixes) confirmed in git log.

---
*Phase: 03-architectural-cleanup*
*Completed: 2026-05-07*
