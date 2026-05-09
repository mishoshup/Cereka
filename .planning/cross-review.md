# Bugfix Sprint — Cross-Review Report

**Reviewed:** 2026-05-09T07:45:00Z
**Depth:** deep (cross-file, full call-chain tracing)
**Files Reviewed:** 25 source files across engine, compiler, renderer, state, UI, scripts, tests
**Status:** issues_found

---

## Summary Table

| Area | Files Changed | Verdict | Key Concerns |
|---|---|---|---|
| **ELSE skip handler** | `cereka_states.cpp` | **PASS** | Logic correct; nested-ELSE handling and skipDepth accounting are sound |
| **C++ bridge 5 ops** | `cereka_instruction.cpp`, `cereka_states.cpp` | **PASS** | PLAY_BGM_FADE, STOP_BGM_FADE, BGM_CROSSFADE, SG_SET, SG_REMOVE all wired correctly |
| **LOAD/JUMP infinite loops** | `cereka_states.cpp` | **FLAG** | LOAD and JUMP/CALL fixes are correct (find() not operator[]). But **MenuState::activateButton** still uses `labelMap[target]` with operator[] — inconsistent |
| **Scene graph position** | `scene_graph.cpp` | **PASS** | Parent position + child position * parent scale is correct hierarchical transform |
| **DrawRichText infinite loop** | `sdl_render_context.cpp` | **PASS** | UTF-8 codepoint skip handles continuation bytes correctly |
| **CTest re-enabled** | `tests/CMakeLists.txt` | **PASS** | Platform-conditional with VS_DEBUGGER_ENVIRONMENT for Windows DLL paths |
| **Word-wrap** | `sdl_render_context.cpp` | **PASS** | Word-boundary rewind handles edge cases; space-skip on line start is correct |
| **Menu/button overhaul** | `cereka_menu_system.*`, `cereka_states.cpp`, `ui_manager.*`, `config_manager.cpp` | **FLAG** | Hover, pagination, keyboard nav all logic-correct. Sentinels (+1/+2) for page nav are fragile but consistent |
| **Settings + pause menu** | `cereka_settings_manager.*`, `cereka_states.cpp`, `cereka_states.hpp`, `ui_manager.*`, `Cereka.cpp`, `cereka_engine_impl.hpp` | **FAIL** | **CRITICAL: BGM/SFX volume wrap-around broken** — volume stuck at 1.0 after reaching max |
| **Tree-sitter automation** | `ops.json`, `gen_tree_sitter_grammar.js`, `update_grammar.sh` | **PASS** | Well-structured, graceful degradation when grammar file missing |
| **Scripting enhancements** | `cereka_compiler.lua` | **PASS** | else-if chaining, &&/|| lowering all correct. Precedence: AND > OR |
| **Save/load UX** | `cereka_save.cpp`, `cereka_save_data.hpp`, `cereka_states.cpp` | **PASS** | Scene description, slot metadata, confirm-overwrite dialog all correct |

---

## CRITICAL Issues

### CR-01: BGM/SFX volume cycling never wraps back to 0 — stuck at max forever

**File:** `src/state/cereka_states.cpp:748-755`
**Issue:** The volume cycling logic uses `std::min` before the wrap check, making the wrap condition unreachable.

```cpp
// Line 748-750 (BGM Volume)
s.bgmVolume = std::min(1.0f, s.bgmVolume + 0.25f);
if (s.bgmVolume > 1.0f + 0.01f) s.bgmVolume = 0.0f;

// Line 753-755 (SFX Volume) — identical pattern
s.sfxVolume = std::min(1.0f, s.sfxVolume + 0.25f);
if (s.sfxVolume > 1.0f + 0.01f) s.sfxVolume = 0.0f;
```

The `std::min(1.0f, ...)` clamps the value to exactly 1.0f when `bgmVolume + 0.25f >= 1.0f`. After that, the condition `> 1.01f` can **never** be true. Result: once volume reaches 1.0 (100%), any further clicks leave it at 100%. It can never cycle back to 0%.

**Fix:** Rearrange to check before clamping, or use pre-increment value:

```cpp
// Option A: check pre-increment value
float next = s.bgmVolume + 0.25f;
s.bgmVolume = (next > 1.0f) ? 0.0f : next;

// Option B: check after increment by removing std::min
s.bgmVolume = s.bgmVolume + 0.25f;
if (s.bgmVolume > 1.0f + 0.01f) s.bgmVolume = 0.0f;
```

---

## WARNING Issues

### WR-01: MenuState::activateButton uses unsafe `labelMap[target]` (operator[])

**File:** `src/state/cereka_states.cpp:545-546`
**Issue:** The fix applied to JUMP and CALL (using `find()` instead of `operator[]`) was not applied to `MenuState::activateButton`.

```cpp
impl.scriptInterpreter.pc =
    target.empty() ? menu.EndPC() : impl.scriptInterpreter.labelMap[target];
```

If for any reason a menu button targets a label that doesn't exist in `labelMap` (corrupted save, miscompiled script, manual edit), `operator[]` **inserts a default entry** with `pc=0`, making the program counter jump to the beginning of the script. On well-formed scripts this won't trigger, but it's inconsistent with the pattern used for JUMP/CALL and masks errors silently.

**Fix:**
```cpp
if (!target.empty()) {
    auto it = impl.scriptInterpreter.labelMap.find(target);
    if (it != impl.scriptInterpreter.labelMap.end()) {
        impl.scriptInterpreter.pc = it->second;
    } else {
        std::cerr << "[CEREKA] Menu button target unknown: " << target << "\n";
        impl.scriptInterpreter.pc = menu.EndPC();
    }
} else {
    impl.scriptInterpreter.pc = menu.EndPC();
}
```

### WR-02: Per-frame texture allocation churn in overlay draw functions

**File:** `src/state/cereka_states.cpp:830-893` (SettingsMenuState::draw), `src/ui/ui_manager.cpp:502-556` (DrawPauseOverlay), `src/ui/ui_manager.cpp:598-651` (DrawConfirmOverwriteDialog)

**Issue:** All three overlay draw functions create and destroy 5–12 `SDL_Texture` objects **every frame** via `CreateTextTexture()` returning `unique_ptr<ITexture>`. While unique_ptr correctly destroys them when they go out of scope, this allocates and frees textures (with associated GPU round-trips) at 60fps. For simple text that changes rarely (titles, labels, hints), this is wasteful.

Settings draw per frame: 12 textures (title + 5 labels + 5 values + hint)  
Pause draw per frame:  7 textures (title + 5 buttons + hint)  
Confirm draw per frame: 4 textures (prompt + sub + yes + no)

**Fix:** Cache static textures (title, labels, hints) and only recreate when text/values change. At minimum, the title text ("SETTINGS", "PAUSED") never changes and could be cached at state-onEnter time.

### WR-03: `pendingConfirmSlot_` not reset on ESC dismiss

**File:** `src/state/cereka_states.cpp:699`
**Issue:** When the confirm-overwrite dialog is dismissed via ESC:

```cpp
if (event.type == CerekaEvent::KeyDown && event.key == SDLK_ESCAPE) {
    ctx.popOverlay(); // cancel confirm, back to save menu
    return;
}
```

The `impl.pendingConfirmSlot_` is **not** reset to -1. While it's overwritten before the next use (in SaveMenuState::handleEvent), this leaves a stale value in the field between the ESC and the next save-click. If any future code path reads `pendingConfirmSlot_` without first setting it, it would use the stale slot number. Defensive reset is cheap.

**Fix:** Add `impl.pendingConfirmSlot_ = -1;` before `ctx.popOverlay()`.

### WR-04: Save slot click silently ignored for empty slots in LoadMenu

**File:** `src/state/cereka_states.cpp:625-632`
**Issue:** Clicking an empty save slot in the Load menu does nothing — no feedback, no error message. The conditional block:

```cpp
auto meta = impl.GetSlotMetadata(slot);
if (!meta.timestamp.empty()) {
    impl.LoadGame(slot);
}
```

If the user clicks an empty slot, the condition fails silently. Consider showing a brief "No save data in this slot" indicator or playing an invalid-action sound.

**Fix:** Either add a toast/indicator message, or simply maintain the behavior but consider it for future UX improvements.

### WR-05: `effectiveState()` has O(n) linear scan with per-call vector construction

**File:** `src/state/cereka_state.hpp:272-283`
**Issue:** `effectiveState()` constructs a `std::vector<CerekaState>` skip-list **every call** and does a linear scan. This is called during save serialization (typically once per save action), so performance impact is negligible. However, the vector is rebuilt from a `std::initializer_list` each time, which is wasteful for a fixed set of skip states.

**Fix:** Use a `static const std::unordered_set<CerekaState>` or a `switch` statement inside `isSkip`. For only 6 states, a switch would be the most efficient and constexpr-friendly.

---

## INFO Issues

### IN-01: DrawPauseOverlay and DrawConfirmOverwriteDialog accept `uiCfg` but don't use it

**File:** `src/ui/ui_manager.cpp:502,598`
`DrawPauseOverlay(const UiConfig &uiCfg)` accepts the config but uses only hardcoded colors and dimensions. The `uiCfg` parameter is unused. Same for `DrawConfirmOverwriteDialog`. Either wire the config for theming or remove the parameter.

### IN-02: `SettingsMenuState::cycleSetting` text speed lookup has edge-case ambiguity

**File:** `src/state/cereka_states.cpp:738-746`
The text speed find-next loop uses `<= speeds[i] + 1.0f` — a 1 cps tolerance band. A speed of 60.5 cps maps to the 60 slot rather than the 90 slot. This is a minor UX surprise. Consider snapping to nearest or using exact match.

### IN-03: `fs::absolute("settings.json")` resolves relative to CWD

**File:** `src/cereka_settings_manager.cpp:22`
Works correctly for the game runner but would be surprising in library/embedded use. Consider using the project root (via config) rather than CWD.

### IN-04: `clearOverlays()` leaves `currentType_` referencing the overlay state

**File:** `src/state/cereka_state.hpp:301-309`
After `clearOverlays()`, `currentType_` still holds the overlay state's enum value and `currentState_` points to the exited overlay state. This is safe because every call site immediately follows with `changeState()`. But a standalone call would leave the state machine in an inconsistent state (currentType_ is an overlay but overlayStack_ is empty).

### IN-05: RETURN op with empty callStack transitions to Finished without logging

**File:** `src/state/cereka_states.cpp:134`
```cpp
case compiler::Op::RETURN:
    if (!si.callStack.empty()) {
        si.pc = si.callStack.back();
        si.callStack.pop_back();
    } else {
        ctx.changeState(CerekaState::Finished);
    }
    continue;
```
When RETURN is called with an empty call stack, the engine silently transitions to Finished. Consider emitting a warning via `std::cerr` to help debug miscompiled scripts.

---

## Integration Assessment

### Overlay Stack Interactions

The three new states (PauseMenu, ConfirmOverwrite, SettingsMenu) integrate correctly with the existing overlay stack:

- **PauseMenu → SaveMenu** (pushOverlay): stack grows correctly
- **SaveMenu → ConfirmOverwrite** (pushOverlay): tracks pendingSlot correctly
- **ConfirmOverwrite → Yes** (double popOverlay): correctly pops back to gameplay after SaveGame
- **ConfirmOverwrite → ESC** (single popOverlay): returns to SaveMenu; slot state preserved
- **LoadGame from any overlay**: calls `clearOverlays() + changeState()` which properly resets the stack

**No stack corruption risk.** All overlay push/pop pairs are balanced.

### State Machine Consistency

All new states are registered in `CerekaImpl::InitGame()`. The `effectiveState()` method properly walks past all overlay-only states. `stateToString()`/`parseState()` round-trip correctly. No gaps in enum-to-label mappings.

### Thread Safety

No threading concerns identified. All SDL operations are single-threaded. Qt6 launcher changes were not in scope for this review.

### Security

Settings file I/O (`settings.json`) uses `fs::absolute()` which is path-traversal safe for basic use. The path is hardcoded to CWD, so no injection vector. Save files use known slot names (slot1.json..slot10.json) — no arbitrary path construction from user input. No new user-facing APIs with injection potential.

---

## Overall Assessment

**Code quality: Good** — The majority of the sprint is solid, well-structured, and correctly implements the intended features. The else-if lowering with `&&`/`||` operators is particularly well done (correct precedence handling, correct lowering to nested IF/ENDIF pairs).

**One CRITICAL must-fix** — the volume wrap-around bug (CR-01) will cause user frustration in the settings menu.

**Several WARNING items** — the operator[] inconsistency in MenuState::activateButton (WR-01) is a copy-paste gap from the JUMP/CALL fix. The texture allocation churn (WR-02) is a performance concern that gets worse as more overlay features are added.

**No security vulnerabilities or data-loss risks identified.**

---

_Reviewed: 2026-05-09T07:45:00Z_
_Reviewer: gsd-code-reviewer (deep cross-file analysis)_
