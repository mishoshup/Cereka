---
phase: state-machine-review
reviewed: 2026-05-09T20:00:00Z
depth: deep
files_reviewed: 9
files_reviewed_list:
  - src/state/cereka_state.hpp
  - src/state/cereka_states.hpp
  - src/state/cereka_states.cpp
  - src/cereka_menu_system.hpp
  - src/cereka_menu_system.cpp
  - src/cereka_dialogue_system.hpp
  - src/cereka_dialogue_system.cpp
  - src/cereka_scene_manager.hpp
  - src/cereka_scene_manager.cpp
  - src/Cereka.cpp
  - src/cereka_ui_config.hpp
  - src/cereka_engine_impl.hpp
  - src/cereka_save.cpp
  - src/cereka_script.cpp
  - src/cereka_script_interpreter.hpp
  - src/cereka_script_interpreter.cpp
  - src/renderer/irender_context.hpp
  - src/renderer/irecture.hpp
  - src/cereka_safe_parse.hpp
  - src/config/config_manager.cpp
  - src/config/property_types.hpp
  - src/config/property_handlers.cpp
  - src/cereka_ui_config.cpp
  - src/cereka_draw.cpp
findings:
  critical: 4
  warning: 6
  info: 3
  total: 13
status: issues_found
---

# State Machine & Managers: Code Review Report

**Reviewed:** 2026-05-09T20:00:00Z
**Depth:** deep (cross-file)
**Files Reviewed:** 9 primary files, 14 supporting files
**Status:** issues_found

## Summary

Deep review of the Cereka state machine subsystem (CRTP base, concrete states, overlay stack), menu system, dialogue system, and scene manager. The architecture is well-structured with clean separation of concerns. However, four critical defects were found: (1) the `ELSE` branch in script conditionals never executes when the `IF` condition is false due to skip-mode handling missing an `ELSE` case, (2) pressing Escape in an overlay (save/load/history) instantly re-pushes the overlay because engine-level event handling runs after state dispatch without checking whether an overlay was just popped, (3) the `LOAD` instruction with an out-of-range slot causes an infinite loop, and (4) missing label lookups use `unordered_map::operator[]` which silently inserts garbage entries.

---

## Critical Issues

### CR-01: ELSE branch never executes when IF condition is false

**File:** `src/state/cereka_states.cpp:26-38`
**Issue:** The skip-mode handler (lines 26-38) only tracks `IF*` and `ENDIF` for nesting depth. When `ELSE` is encountered while in skip mode (IF condition was false), it falls through to `si.pc++; continue;` and is silently skipped — the skip-mode handler has no case for it. The `ELSE` case in the `switch` (lines 216-222) is dead code when `skipMode` is true because the switch is never reached (the skip-mode handler does `continue`).

**Consequence:** Every `else` branch in the script language is dead code. If an IF condition is false, both the THEN body AND the ELSE body are skipped. The ELSE body is **never** executed at runtime.

**Trace:**
```
if x == y        ; false → skipMode=true, skipDepth=1
    say a "then" ; skipped (in skipMode)
else             ; NOT handled by skip-mode handler → pc++;continue (still skipMode!)
    say b "else" ; skipped (in skipMode) ← BUG: should NOT be skipped
endif            ; skipDepth-- → 0, skipMode=false
```

**Fix:** Add an ELSE case to the skip-mode handler that ends skip mode when `skipDepth == 1`:
```cpp
if (si.skipMode) {
    if (ins.op == compiler::Op::IF_EQ || ins.op == compiler::Op::IF_NEQ ||
        ins.op == compiler::Op::IF_GT || ins.op == compiler::Op::IF_LT ||
        ins.op == compiler::Op::IF_GE || ins.op == compiler::Op::IF_LE) {
        si.skipDepth++;
    } else if (ins.op == compiler::Op::ELSE && si.skipDepth == 1) {
        // This ELSE matches the outermost skipped IF — stop skipping to execute the ELSE body
        si.skipMode = false;
        si.skipDepth = 0;
    } else if (ins.op == compiler::Op::ENDIF) {
        si.skipDepth--;
        if (si.skipDepth == 0)
            si.skipMode = false;
    }
    si.pc++;
    continue;
}
```

---

### CR-02: Escape in overlay triggers immediate re-push (user trapped)

**File:** `src/Cereka.cpp:277-315`
**Issue:** `Impl::HandleEvent` always delegates to the state machine first, then processes engine-level events. When SaveMenuState (or LoadMenuState or HistoryState) handles an ESC key by calling `popOverlay()`, the state machine's current state reverts to the underlying gameplay state (e.g., `WaitingForInput`). The engine-level ESC handler then checks `currentType()` — now `WaitingForInput` — and pushes a **new** SaveMenu overlay.

**Consequence:** The user can never dismiss the save/load/history overlay with Escape. Every pop is immediately followed by a re-push, trapping the user in an infinite loop.

**Trace:**
```
User presses ESC in SaveMenuState overlay
  → state machine: SaveMenuState::handleEvent → popOverlay()
  → currentType() is now WaitingForInput (restored from stack)
  → engine-level: "ESC key + WaitingForInput → push SaveMenu overlay"
  → overlay is back → user is trapped
```

**Fix (choose one):**
1. Make `handleEvent` return `bool` (true = consumed). Engine-level handlers only fire if the state did NOT consume the event.
2. Process engine-level events BEFORE state machine dispatch (reorder).
3. Track a "just-popped" flag: after `popOverlay` in a state handler, suppress engine-level re-push for the same event.

**Recommended fix (option 1):**
```cpp
// ICerekaState:
[[nodiscard]] virtual bool handleEvent(const CerekaEvent &event, ICerekaStateContext &) { return false; }

// CerekaStateMachine:
bool handleEvent(const CerekaEvent &event)
{
    if (currentState_ && ctx_) {
        return currentState_->handleEvent(event, *ctx_);
    }
    return false;
}

// Impl::HandleEvent:
void Impl::HandleEvent(const CerekaEvent &e)
{
    bool consumed = m_stateMachine.handleEvent(e);
    if (consumed) return;

    // Engine-level events only fire if no state consumed the event
    if (e.type == CerekaEvent::KeyDown && e.key == SDLK_ESCAPE) { ... }
    ...
}
```

---

### CR-03: LOAD with invalid slot causes infinite loop

**File:** `src/state/cereka_states.cpp:314-323`
**Issue:** The `LOAD` instruction handler does NOT increment `si.pc` when the slot is out of range (not 1–10), and `LoadGame` is not called. The function `return`s without advancing the program counter. On the next `Update` tick, the same `LOAD` instruction is processed again, producing the same result — infinite loop.

Compare with the `SAVE` handler (lines 302-312) which correctly does `si.pc++; continue;` regardless of whether the slot was valid.

**Consequence:** A script with `load 0` or `load 99` or `load ""` will hang the engine permanently.

**Trace:**
```
pc → LOAD(ins.a="0")
  slot=0, 0 < 1 → LoadGame not called, pc not advanced
  return
Next update: pc still at LOAD → same thing → infinite loop
```

**Fix:** Always advance (or at minimum always advance when no load happens):
```cpp
case compiler::Op::LOAD: {
    int slot = 0;
    if (!ins.a.empty()) {
        auto r = safe_stoi(ins.a);
        if (r) slot = *r;
    }
    if (slot >= 1 && slot <= 10)
        impl.LoadGame(slot);
    si.pc++;
    return;  // or continue, but return is fine since LoadGame may change state
}
```

Note: The `si.pc++` before `return` is harmless when `LoadGame` succeeds because `LoadGame` calls `m_stateMachine.changeState()` which will re-enter some state and execution will use the restored PC. The increment is consumed by the state change and is a no-op in that case.

---

### CR-04: Missing JUMP/Target label silently inserts entry at PC=0

**File:** `src/state/cereka_states.cpp:91-92` (JUMP), `src/state/cereka_states.cpp:388-389` (MenuState::handleEvent)
**Issue:** Both `JUMP` handler and `MenuState::handleEvent` use `labelMap[target]` (via `operator[]`), which default-constructs a `size_t(0)` entry into the map when the key doesn't exist. This means jumping to a non-existent label silently resets the program counter to the start of the script, and pollutes `labelMap` with a spurious entry.

**Consequence:** A typo in a label name causes the script to restart from PC 0 with no diagnostic. The spurious map entry persists for the lifetime of the script.

**Fix:** Use `labelMap.at(target)` for a thrown exception, or `labelMap.find(target)` with explicit error handling:
```cpp
case compiler::Op::JUMP: {
    auto it = si.labelMap.find(ins.a);
    if (it != si.labelMap.end()) {
        si.pc = it->second;
    } else {
        std::cerr << "[CEREKA] Unknown label: " << ins.a << "\n";
        ctx.changeState(CerekaState::Finished);
    }
    continue;
}
```

---

## Warnings

### WR-01: State machine corruption when transitioning to unregistered state

**File:** `src/state/cereka_state.hpp:160-178`
**Issue:** `changeState()` calls `currentState_->onExit()` and sets `currentState_ = nullptr` before looking up the new state. If the new state type is not in `states_` (e.g., registration was missed), `currentState_` remains `nullptr` while `currentType_` retains the **old** type. Every subsequent dispatch (`update`, `draw`, `handleEvent`) checks `currentState_` and does nothing — the machine is dead but `currentType()` lies about the state.

The same issue exists in `pushOverlay()` (line 182-199): the overlay is pushed to the stack but `currentState_` is never set if the overlay type is not found, yet `popOverlay()` will later restore `{prevType, prevState}` from the stack entry.

**Fix:** Guard against missing state — abort or return early:
```cpp
void changeState(CerekaState newType) {
    if (!currentState_ || !ctx_) return;
    auto it = states_.find(newType);
    if (it == states_.end()) {
        std::cerr << "[STATE] Unknown state: " << stateLabel(newType) << "\n";
        return;  // stay in current state
    }
    // ... proceed with transition
}
```

---

### WR-02: clearOverlays leaves machine in inconsistent state

**File:** `src/state/cereka_state.hpp:266-274`
**Issue:** `clearOverlays()` calls `onExit` on the current (overlay) state, then clears the stack — but does NOT restore the underlying state or call its `onEnter`. After this call, `currentState_` still points to the overlay, and `currentType_` is still the overlay type, but the stack is empty. The machine is in a corrupted state — `effectiveState()` returns the overlay type instead of the gameplay state underneath.

**Current usage is safe** — every caller (`LoadGame`, `rollbackManager.goTo`) follows up with `changeState()`. But the API is fragile and violates the principle of least surprise. Any new caller that doesn't call `changeState()` immediately after will get silently corrupted state.

**Fix:** Either restore the parent state after clearing, or document the precondition clearly and rename to `clearOverlaysAndTransitionTo()`:
```cpp
// Option: restore the underlying state from the stack bottom before clearing
void clearOverlays() {
    if (currentState_) currentState_->onExit(*ctx_);
    if (!overlayStack_.empty()) {
        currentType_ = overlayStack_.front().first;  // bottom of stack = original state
        currentState_ = overlayStack_.front().second;
    }
    overlayStack_.clear();
}
```

---

### WR-03: Typewriter advances at approximately half configured speed

**File:** `src/cereka_dialogue_system.cpp:14-26`
**Issue:** The typewriter timer uses `int charsToAdd = (int)(typewriterTimer * CHARS_PER_SECOND);`. Integer truncation discards fractional time each frame. At 60 FPS with `CHARS_PER_SECOND = 60`, each frame accumulates ~0.0167s but needs 1/60 ≈ 0.0167s per character. Due to truncation (`int(0.0167 * 60) = int(1.0002) = 1` sometimes, `int(0.9996) = 0` other times), characters appear at roughly half the configured rate.

The root cause: `charsToAdd / CHARS_PER_SECOND` is used for timer subtraction but is floating-point division (`int / float`). The truncation in `(int)(timer * rate)` loses sub-character time, causing drift.

**Fix:** Use a while-loop accumulator pattern:
```cpp
void DialogueSystem::Tick(float dt) {
    if (text.empty()) return;
    typewriterTimer += dt;
    float charTime = 1.0f / CHARS_PER_SECOND;
    while (typewriterTimer >= charTime && displayedChars < (int)text.length()) {
        displayedChars++;
        typewriterTimer -= charTime;
    }
}
```

---

### WR-04: Path traversal via unsanitized asset filenames

**File:** `src/cereka_scene_manager.cpp:30` (`loadBg`), `src/cereka_scene_manager.cpp:50` (`ShowCharacter`)
**Issue:** Asset filenames from `.crka` scripts are concatenated directly into a path:
```cpp
auto tex = m_renderCtx->CreateTexture("assets/bg/" + filename);
auto tex = m_renderCtx->CreateTexture("assets/characters/" + filename);
```
A filename containing `../` (e.g., `../../etc/passwd`) resolves outside the intended directory. This also affects `LoadCerekaScript` in `cereka_script.cpp:32` where `lua.load_file(filename)` is called with a user-supplied path.

**Risk:** Low for first-party scripts, but any third-party `.crka` content can read arbitrary files.

**Fix:** Reject filenames containing `..` or `/`:
```cpp
if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos) {
    std::cerr << "[CEREKA] Invalid filename (path components not allowed): " << filename << "\n";
    return nullptr;
}
```

---

### WR-05: CerekaEngine pImpl uses raw new/delete instead of unique_ptr

**File:** `src/Cereka.cpp:352-356`
**Issue:** The pImpl pointer is a raw `CerekaImpl*` managed with `new`/`delete`. If the `CerekaEngine` constructor body throws after `new Impl()` completes (none currently does, but the code is not exception-safe going forward), the Impl is leaked since the destructor won't run on a partially-constructed object.

```cpp
cereka::CerekaEngine::CerekaEngine() : pImplementation(new Impl()) {}
cereka::CerekaEngine::~CerekaEngine() { delete pImplementation; }
```

Additionally, `ShutDown` (line 68-71) uses a lambda `destroyTex(ITexture *&t)` that does `delete t; t = nullptr;` on raw pointers. These pointers are created in `cereka_ui_config.cpp:58-59` via `CreateTexture(path).release()` — the ownership transfer is correct but fragile. If `release()` were ever changed to `get()`, the `delete` in `destroyTex` would cause a double-free.

**Fix:**
```cpp
class CerekaEngine {
    std::unique_ptr<CerekaImpl> pImplementation;
};
cereka::CerekaEngine::CerekaEngine() : pImplementation(std::make_unique<Impl>()) {}
~CerekaEngine() = default;  // unique_ptr handles cleanup
```

---

### WR-06: MenuSystem::Target/IsExit has no bounds checking

**File:** `src/cereka_menu_system.hpp:22-23`, `src/cereka_menu_system.cpp:32-40`
**Issue:** `Target(i)` and `IsExit(i)` access `targets[i]` and `exits[i]` without bounds checking. While callers (`MenuState::handleEvent`, `SelectMenuOption`) validate the index from `HitTest()`, this is a fragile invariant — if `Open()` is ever called with mismatched vector sizes (the `texts`, `targets`, and `exits` vectors are not validated to be the same length), `Target()` will index out of bounds.

**Fix:** Validate vector sizes in `Open()`:
```cpp
void MenuSystem::Open(std::vector<std::string> t, std::vector<std::string> tg,
                      std::vector<bool> ex, size_t end)
{
    if (t.size() != tg.size() || t.size() != ex.size()) {
        std::cerr << "[MENU] Mismatched button data sizes\n";
        return;
    }
    // ...
}
```

---

## Info

### IN-01: Buzzword comments in state machine header

**File:** `src/state/cereka_state.hpp:7-11`
**Issue:** The block comment still contains "Enterprise patterns", "Self-documenting code", "std::expected for error handling" — boilerplate that doesn't describe the actual design. `std::expected` is notably NOT used anywhere in this file.

**Fix:** Remove the buzzword block. Replace with a concise description of the state machine architecture.

---

### IN-02: cereal state machines tateLabel missing default case

**File:** `src/state/cereka_state.hpp:123-135`
**Issue:** The `switch` on `CerekaState` enumerates all current enum values but has no `default:` case. If new states are added without updating `stateLabel()`, the function silently returns `"?"`. A `default:` with a `static_assert` or explicit handling would make the compiler catch omissions.

**Fix:** Add `default: return "?";` (explicit) and update when adding new enum values.

---

### IN-03: LoadMenuState comment implies LoadGame handles overlay cleanup, but the call is conditional

**File:** `src/state/cereka_states.cpp:463-465`
**Issue:** The comment `// No overlay cleanup needed — LoadGame already called clearOverlays() + changeState() on m_stateMachine.` is accurate when `LoadGame` returns true. But if `LoadGame` returns false (e.g., corrupt save file, missing file), `clearOverlays()` is NOT called, the overlay stays open, and the user gets no failure feedback. The comment creates a false sense of safety.

**Fix:** Add error handling for failed `LoadGame` — either show feedback or just pop the overlay on failure:
```cpp
if (slot >= 1 && slot <= 10) {
    if (!impl.LoadGame(slot)) {
        std::cerr << "[CEREKA] Load failed for slot " << slot << "\n";
        // Stay on load screen so user can try again
    }
}
```

---

_Reviewed: 2026-05-09T20:00:00Z_
_Reviewer: gsd-code-reviewer (deep)_
_Depth: deep (cross-file, call-chain tracing)_
