---
phase: 02-distribution
plan: 03
subsystem: engine
tags: [state-machine, refactor, architecture, cpp]

requires:
  - phase: 02-distribution
    plan: 02
    provides: Fixed VM (nested if/else skipDepth) + word-wrap

provides:
  - CerekaImpl delegates main loop control to CerekaStateMachine
  - Game logic (dialogue, menus, fades, save/load overlays) encapsulated in concrete state classes
  - State transitions are traceable via stdout logging
  - Stack underflow protection for overlay operations

affects:
  - 02-distribution-04 (save system modernization — state machine sync after LoadGame)

tech-stack:
  added: []
  patterns:
    - State Machine pattern with CRTP (VNState<T>) for compile-time dispatch
    - IVNStateContext interface for loose coupling between states and engine
    - Overlay stack pattern for save/load UI on top of gameplay

key-files:
  created: []
  modified:
    - src/engine_impl.hpp: IVNStateContext inheritance, CerekaStateMachine member
    - src/Cereka.cpp: State registration in InitGame, IVNStateContext implementations, HandleEvent delegation
    - src/script_vm.cpp: Update delegates to state machine, TickScript uses changeState/pushOverlay
    - src/draw.cpp: Draw delegates to state machine for menus/fades/overlays, dialogue skip on overlay
    - src/state/cereka_state.hpp: State transition logging, isInitialized(), clearOverlays()
    - src/state/cereka_states.hpp: Draw method declarations for MenuState, FadeState, SaveMenuState, LoadMenuState
    - src/state/cereka_states.cpp: Full state implementations for all 7 concrete states

key-decisions:
  - States cast IVNStateContext& to Impl& (static_cast) to access engine internals — pragmatic for this tightly-coupled codebase, can be refactored to interface methods later
  - Fallback to direct state assignment when the state machine is uninitialized (supports VM unit tests that create Impl directly)
  - dialogue.Tick() kept in engine-level Update (not moved to DialogueState) because typewriter animation must run during both Running and WaitingForInput states
  - clearOverlays() added to CerekaStateMachine for LoadGame post-load state machine sync

patterns-established:
  - State pattern: each game mode (dialogue, menu, fade, save, load) encapsulated in its own class with enter/exit/update/draw/handleEvent lifecycle
  - Overlay stack: save/load menus push an overlay on top of gameplay state and pop back on ESC or action
  - Logging convention: "[STATE] <from> -> <to>" for all transitions

requirements-completed: [ARCH-01]

duration: 32 min
completed: 2026-05-07
---

# Phase 02 Plan 03: State Machine "Big Bang" Migration Summary

**Decoupled CerekaImpl game logic into 7 concrete state classes driven by CerekaStateMachine, with stdout traceability for all state transitions**

## Performance

- **Duration:** 32 min
- **Started:** 2026-05-07
- **Completed:** 2026-05-07
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments

- CerekaImpl now inherits IVNStateContext and delegates HandleEvent/Update/Draw to CerekaStateMachine
- All 7 states (Dialogue, Menu, Fade, SaveMenu, LoadMenu, Finished, Quit) registered and initialized in InitGame
- DialogueState::update runs TickScript() for VM dispatch during Running state
- MenuState handles button click events and renders menu buttons
- FadeState manages background fade transitions (alpha animation + auto-transition)
- SaveMenuState/LoadMenuState handle slot selection, save/load execution, and overlay rendering
- State transitions print to stdout with format `[STATE] <from> -> <to>`
- popOverlay guards against stack underflow (empty check before access)
- isInitialized() check provides graceful fallback for unit tests that create Impl without InitGame

## Task Commits

Each task was committed atomically:

1. **Task 1: Wire CerekaImpl to StateMachine** - `7921dc9` (feat)
2. **Task 2: Implement Concrete States** - `8f02bf2` (feat)
3. **Task 3: Add state transition observability** - `9989435` (feat)
4. **Fix: Handle uninitialized state machine** - `d36f8ac` (fix)

## Files Created/Modified

- `src/engine_impl.hpp` — IVNStateContext inheritance, CerekaStateMachine member, interface method declarations
- `src/Cereka.cpp` — State registration, IVNStateContext impls, HandleEvent with delegation + global event handling
- `src/script_vm.cpp` — Update delegates to m_stateMachine, TickScript routes transitions through changeState/pushOverlay
- `src/draw.cpp` — m_stateMachine.draw() for state-specific rendering, dialogue box skip when overlay active
- `src/state/cereka_state.hpp` — isInitialized(), clearOverlays(), stateLabel(), std::cout logging in transitions
- `src/state/cereka_states.hpp` — draw() method declarations for Menu/Fade/SaveMenu/LoadMenu states
- `src/state/cereka_states.cpp` — Full implementations for all 7 concrete states

## Decisions Made

- **Static cast from ctx:** States access the engine via `static_cast<Impl&>(ctx)` — pragmatic for current tightly-coupled codebase. Clean interface extraction deferred to a future refactoring phase.
- **dialogue.Tick stays in Update:** Typewriter animation must run during both `Running` and `WaitingForInput` states, so it remains in the engine-level Update() call rather than in a specific state.
- **Fallback for uninitialized state machine:** When `Impl` is used directly (e.g., VM unit tests), `changeState`/`pushOverlay`/`popOverlay` fall back to direct `state` member assignment instead of routing through the (uninitialized) state machine.
- **clearOverlays() for LoadGame:** After loading a save file restores `state` from disk, the overlay stack and state machine internal state are re-synced via `clearOverlays()` + `changeState()`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Uninitialized state machine breaks VM unit tests**
- **Found during:** Task 3 (post-build test run)
- **Issue:** VM tests create `Impl` directly and set `engine.state = Running` then call `engine.TickScript()`. When TickScript processes a SAY instruction and calls `changeState(WaitingForInput)`, the state machine is uninitialized (no setContext/setInitialState called) and returns without changing state. The legacy `state` member stayed as `Running`.
- **Fix:** Added `CerekaStateMachine::isInitialized()` accessor. In `Impl::changeState`/`pushOverlay`/`popOverlay`, check if the state machine is initialized. If not, update the legacy `state` member directly instead of routing through the state machine.
- **Files modified:** `src/Cereka.cpp`, `src/state/cereka_state.hpp`
- **Verification:** All 21 unit tests pass
- **Committed in:** `d36f8ac` (part of fix commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Necessary for backward compatibility with existing unit tests. No scope creep.

## Issues Encountered

- **popOverlay in LoadMenuState:** After `LoadGame()` restores `state` from the save file, the state machine's overlay stack still contains the LoadMenuState entry. `clearOverlays()` + `changeState()` sync the state machine after load. This may be revisited in the save system modernization phase (02-04) when the save format gains a `version` field.

## Threat Surface Scan

No new security-relevant surface introduced — changes are limited to internal engine architecture (state machine routing, no new network/file/auth endpoints).

## Threat Model Compliance

| Threat ID | Requirement | Status |
|-----------|-------------|--------|
| T-02-03 | popOverlay cannot underflow the stack | ✓ — empty check before access |

## Next Phase Readiness

- State machine is fully wired and ready for the save system modernization (02-04)
- LoadGame state machine sync is a known area needing cleanup in 02-04 when save format is updated
- Ready for Phase 3 architectural cleanup (CerekaImpl split, renderer abstraction, UIManager extraction)

---
*Phase: 02-distribution*
*Completed: 2026-05-07*
