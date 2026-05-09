---
phase: full-engine-review
reviewed: 2026-05-09T20:00:00Z
depth: deep
subreports:
  - REVIEW-CORE.md (7 critical, 10 warning, 5 info)
  - REVIEW-STATE.md (4 critical, 6 warning, 3 info)
  - REVIEW-RENDERER.md (6 critical, 11 warning, 4 info)
  - REVIEW-AUDIO-CONFIG.md (5 critical, 12 warning, 6 info)
  - REVIEW-TESTS.md (10 critical, 14 warning, 6 info)
  - REVIEW-LAUNCHER.md (2 critical, 13 warning, 4 info)
aggregate:
  critical: 34
  warning: 66
  info: 28
  total: 128
status: issues_found
---

# Cereka Engine — Full Code Review

**Reviewed:** 2026-05-09
**Depth:** deep (6 parallel subagent reviews, cross-file call chains)
**Files Reviewed:** 97 across 6 subsystems
**Status:** issues_found — **34 critical, 66 warning, 28 info**

---

## Aggregate Summary

Full engine deep code review across 6 parallel subsystem reviews. Found **128 issues total** (34 critical, 66 warning, 28 info). The most severe findings fall into three categories:

1. **Silent data loss / silent failures** — instructions dropped by C++ bridge, ELSE branches that never execute, tests that don't test anything
2. **Infinite loops** — LOAD/JUMP with invalid state never advances PC
3. **Security** — command injection in launcher packaging, path traversal in asset loading, sol2 all-libraries-open

---

## Top Critical Findings (must fix before shipping)

### CR-01: C++ bridge silently drops 5 instruction types
**File:** `src/compiler/cereka_instruction.cpp:68-136`
The string→enum mapping in `RunLuaCompiler` is missing entries for `PLAY_BGM_FADE`, `STOP_BGM_FADE`, `BGM_CROSSFADE`, `SG_SET`, `SG_REMOVE`. The Lua compiler emits them, the VM has switch cases for them, but the bridge rejects them as "Unknown op". BGM fades and scene graph ops compile but **never execute** — completely silent failure.

### CR-02: ELSE branch never executes
**File:** `src/state/cereka_states.cpp:26-38`
The skip-mode handler has no case for `ELSE`. When an IF condition is false, both THEN and ELSE bodies are skipped. Every `else` in every `.crka` script is dead code.

### CR-03: LOAD/JUMP to invalid state causes infinite loop
**Files:** `src/state/cereka_states.cpp:314-323`, `cereka_states.cpp:91,101,389`
`Op::LOAD` with out-of-range slot (or missing save file) returns without advancing PC — same instruction fires every frame forever. `labelMap[missingLabel]` uses `operator[]` which default-constructs an entry at PC 0 instead of reporting an error, causing infinite JMP-to-self loops.

### CR-04: Command injection in launcher packaging
**File:** `launcher/main.cpp:926-952`
Game title from `game.cfg` is concatenated into `system()` calls for tar/zip archiving without escaping. A malicious project can execute arbitrary shell commands on the packager's machine.

### CR-05: sol2 Lua opens all standard libraries
**File:** `src/cereka_script_interpreter.hpp:26`
`sol::state` opens `os.execute`, `io.open`, `loadfile`, `dofile` — malicious `.crka` scripts can read/write arbitrary files and execute system commands.

### CR-06: CTest integration disabled
**File:** `tests/CMakeLists.txt:52`
`gtest_discover_tests` is commented out — `ninja test` silently skips all Cereka tests. Test suite is non-functional from the build system perspective.

### CR-07: RollbackManagerTest tests never call capture()
**File:** `tests/rollback_manager_test.cpp:29,49`
Two test functions (`CanRollbackAfterCapture`, `CountIncrementsOnCapture`) have names claiming what they test, but neither ever calls `rm.capture()`. They only check initial state.

### CR-08: Scene graph position doesn't accumulate from parent
**File:** `src/scene_graph.cpp:73-74`
Child nodes ignore parent's world position — children don't follow parent movement, breaking the fundamental scene graph invariant.

### CR-09: Opacity computed but never applied
**File:** `src/ui/ui_manager.cpp:50-66`
Scene graph accumulates opacity but rendering never reads it. `DrawTexture` has no alpha parameter.

### CR-10: AudioManager destroys BGM before new load succeeds
**File:** `src/cereka_audio_manager.cpp`
Current BGM handle freed before `Mix_LoadAudio` — if the new load fails, old BGM is gone with no recovery.

### CR-11: Use-after-free in detached threads
**File:** `launcher/main.cpp:682,792`
`doLaunch()` and `doPackage()` spawn `std::thread([this]{...}).detach()`. If the window closes during an operation, the thread accesses a dangling `this`.

### CR-12: RollbackManager goTo index formula wrong
**File:** `src/cereka_rollback_manager.cpp:78-80`
Circular buffer index formula maps 0→newest and capacity-1→oldest instead of the correct direction.

### CR-13: DrawRichText infinite loop
**File:** `src/renderer/sdl_render_context.cpp:196-200`
When no glyph fits `maxWidth`, extent=0 causes perpetual wrap with no offset advance.

---

## Findings by Subsystem

### Core (22: 7C, 10W, 5I)
- CR: LOAD infinite loop, JUMP garbage label, ELSE dead code, sol2 all libs, path traversal in save assets, LoadGame infinite loop, `(int)float` UB on INT_MAX
- WR: sscanf without range clamping, MenuSystem no bounds checks, typewriter timer bias, CerekaImpl god class, dead coroutine

### State Machine & Managers (13: 4C, 6W, 3I)
- CR: ELSE never executes, overlay ESC re-push loop, LOAD infinite loop, missing label inserts garbage
- WR: unregistered state corruption, clearOverlays inconsistency, typewriter half-speed, path traversal, raw new/delete

### Renderer & Scene Graph (21: 6C, 11W, 4I)
- CR: no parent position accumulation, opacity never applied, DrawRichText infinite loop, SdlRenderContext double-free, rich_text_renderer dead code, raw ITexture* dangling pointer
- WR: TTF_Font* leaks through abstraction, SDL_Texture* exposed via RawTexture(), filename typo (irecture.hpp), parseFloatSafe silent error, public world struct, no caching

### Audio, Config, Compiler (23: 5C, 12W, 6I)
- CR: 5 dropped instruction types, RollbackManager goTo wrong, BGM destroy-before-load, sol2 unprotected result cast, rollback doesn't capture lua state
- WR: unchecked Mix_*, sscanf in asColor, duplicate serializeDim, O(capacity) resize per capture, empty catch blocks, no include cycle detection

### Tests (30: 10C, 14W, 6I)
- CR: rollback tests don't test, audio tests test copy not production, CTest disabled, GLOB_RECURSE, numVariables zero coverage, 30+ ops untested, scene graph no parent-child tests, no JSON error tests, no error-case snapshots, display_test tests SDL not Cereka
- WR: save_data slot bounds, stringVar not tested, missing 0-op test, const methods not verified, no fade/stop_bgm coverage

### Launcher (19: 2C, 13W, 4I)
- CR: command injection in system(), use-after-free in detached threads
- WR: path traversal in project creation, fragile config parsing, unchecked I/O, god class main.cpp, non-portable archiving, truncation, race conditions

---

## Individual Reports

- `REVIEW-CORE.md` — Core engine (Cereka.cpp, script dispatch, save/load, video, text renderer, public API, runner)
- `REVIEW-STATE.md` — State machine, overlay stack, menu/dialogue/scene managers
- `REVIEW-RENDERER.md` — Renderer abstraction, scene graph, rich text, markup parser, UI manager
- `REVIEW-AUDIO-CONFIG.md` — Audio, rollback, config, compiler bridge, Lua compiler
- `REVIEW-TESTS.md` — Test suite, compile snapshots, CMake build system
- `REVIEW-LAUNCHER.md` — Qt6 launcher, project manager, packaging, config
