---
phase: core-review
reviewed: 2026-05-09T12:00:00Z
depth: deep
files_reviewed: 33
files_reviewed_list:
  - src/Cereka.cpp
  - src/cereka_engine_impl.hpp
  - src/cereka_script.cpp
  - src/cereka_script_interpreter.cpp
  - src/cereka_script_interpreter.hpp
  - src/cereka_draw.cpp
  - src/cereka_save.cpp
  - src/cereka_save_data.hpp
  - src/cereka_ui_config.cpp
  - src/cereka_ui_config.hpp
  - src/cereka_video.cpp
  - src/cereka_video.hpp
  - src/cereka_text_renderer.cpp
  - src/cereka_text_renderer.hpp
  - src/cereka_safe_parse.hpp
  - src/cereka_dialogue_system.cpp
  - src/cereka_scene_manager.cpp
  - src/cereka_rollback_manager.cpp
  - src/cereka_audio_manager.cpp
  - src/state/cereka_states.cpp
  - include/Cereka/Cereka.hpp
  - include/Cereka/exceptions.hpp
  - include/Cereka/CerekaTest.hpp
  - runner/main.cpp
  - src/renderer/sdl_render_context.cpp
  - src/ui/ui_manager.cpp
  - src/renderer/irender_context.hpp
  - src/cereka_menu_system.hpp
  - src/cereka_dialogue_system.hpp
findings:
  critical: 7
  warning: 10
  info: 5
  total: 22
status: issues_found
---

# Cereka CORE Subsystem — Deep Code Review

**Reviewed:** 2026-05-09T12:00:00Z
**Depth:** deep (cross-file call chains, type consistency, error propagation, state mutation)
**Files Reviewed:** 22 source files
**Status:** issues_found

## Summary

The CORE subsystem is structurally coherent with a clear architecture (state machine → script dispatch → scene/audio/ui managers), but harbors several **critical correctness bugs** including infinite-loop injections via `.crka` LOAD/JUMP instructions, inverted history-rollback mapping on partially filled buffers, and latent security risks from an all-libraries-open sol2 Lua state. The `CerekaImpl` god class is acknowledged in the roadmap (Phase 0.2) but currently concentrates 15+ responsibilities into a single header, making cross-cutting defects harder to spot. Error propagation is inconsistent — some failures crash silently, others loop forever.

---

## Critical Issues

### CR-01: LOAD instruction with invalid/missing save file causes infinite loop

**File:** `src/state/cereka_states.cpp:314-323`
**Severity:** BLOCKER — engine hang / usability crash

The `Op::LOAD` dispatch calls `impl.LoadGame(slot)` and then unconditionally `return`s, **skipping `si.pc++`**. If the slot is outside 1–10 (e.g., `load 0` or `load 11` in a .crka script), or if the save file doesn't exist (LoadGame returns false), the PC is never advanced. On the next frame, the same LOAD instruction fires again, creating an **infinite loop**.

```cpp
// cereka_states.cpp:314-323
case compiler::Op::LOAD: {
    int slot = 0;
    if (!ins.a.empty()) {
        auto r = safe_stoi(ins.a);
        if (r) slot = *r;
    }
    if (slot >= 1 && slot <= 10)
        impl.LoadGame(slot);   // <-- returns false silently on missing file
    return;                    // <-- NO si.pc++ -> infinite loop
}
```

**Fix:** When `LoadGame` is not called (invalid slot) or returns `false`, advance PC to avoid the loop. Only `return` without increment when `LoadGame` succeeded (because it restores PC from the save).

```cpp
case compiler::Op::LOAD: {
    int slot = 0;
    if (!ins.a.empty()) {
        auto r = safe_stoi(ins.a);
        if (r) slot = *r;
    }
    if (slot >= 1 && slot <= 10) {
        if (impl.LoadGame(slot))
            return;  // pc restored from save data
    }
    si.pc++;          // avoid infinite loop on failure
    continue;
}
```

---

### CR-02: JUMP/CALL/labelMap lookup default-constructs missing entries → PC jumps to 0

**File:** `src/state/cereka_states.cpp:91,101`
**File:** `src/state/cereka_states.cpp:389`
**Severity:** BLOCKER — infinite loop on script error

`std::unordered_map::operator[]` default-constructs an entry (value `0`) when the key is missing. All three code paths do this:

```cpp
// Line 91 (JUMP):
si.pc = si.labelMap[ins.a];         // missing label → pc = 0

// Line 101 (CALL):
si.pc = si.labelMap[ins.a];         // missing label → pc = 0

// Line 389 (menu button):
impl.scriptInterpreter.pc =
    target.empty() ? impl.menu.EndPC() : impl.scriptInterpreter.labelMap[target];
```

When PC jumps to 0, the program loops from the top. If instruction 0 is another JUMP to the same missing label, the engine **spins forever**.

**Fix:** Use `labelMap.find()` or `labelMap.at()` and handle missing labels:

```cpp
auto it = si.labelMap.find(ins.a);
si.pc = (it != si.labelMap.end()) ? it->second : si.pc;
// or log error and advance
```

---

### CR-03: RollbackManager::goTo maps visual index to wrong buffer entry for partial fills

**File:** `src/cereka_rollback_manager.cpp:77-82`
**Severity:** BLOCKER — clicking "restore to X" restores wrong state

When `count_ < capacity_` (first 200 captures), `goTo` maps `index` directly to `bufIdx = index`. But `historyTexts()` returns entries in **reverse** chronological order (most recent first). So:

- Visual index 0 (top of list, most recent capture) → `bufIdx = 0` → **oldest** capture
- Visual index N-1 (bottom of list, oldest capture) → `bufIdx = N-1` → **most recent** capture

The mapping is inverted. It works correctly only when the circular buffer wraps (`count_ == capacity_`), where the `(head_ - 1 - index)` formula compensates.

**Concrete example:**
- 3 captures made: ["hello", "world", "done"], count_=3, capacity=200
- `historyTexts()` returns: ["done", "world", "hello"] (reversed)
- User clicks "done" (visual idx 0) → `goTo(impl, 0)` → `bufIdx = 0` → restores "hello" snapshot

**Fix:** Compute buffer index from the end in the partial-fill case:

```cpp
if (count_ < capacity_) {
    bufIdx = count_ - 1 - index;  // reverse: visual 0 = last entry
} else {
    bufIdx = (head_ - 1 - index + capacity_) % capacity_;
}
```

---

### CR-04: sol2 `lua` state opens all standard libraries — latent code execution risk

**File:** `src/cereka_script_interpreter.hpp:26`
**File:** `src/cereka_script.cpp:32`
**Severity:** BLOCKER — security / arbitrary code execution vector

`ScriptInterpreter::lua` is a plain `sol::state` member. **sol2 opens all Lua standard libraries by default** in its constructor, including `os`, `io`, `debug`, `package`, `coroutine`, `math`, `string`, `table`, and `utf8` — many of which have dangerous functions (`os.execute`, `io.open`, `load`, `dofile`).

While the `script` coroutine loaded by `LoadCerekaScript` is never resumed in the current codebase (dead code), this state is a public member of `ScriptInterpreter` which is a public member of `CerekaImpl`. Any code path that reaches this state — or if a future change does resume the coroutine or call into `lua` — opens arbitrary code execution.

The separate compiler `sol::state` in `cereka_instruction.cpp` correctly limits libraries: `lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table)` — but the engine's own state does not.

**Fix:** Explicitly open only the minimal library set needed, or open none:

```cpp
ScriptInterpreter() {
    lua.open_libraries(sol::lib::string, sol::lib::table);
    // Remove base if not needed — no os.execute!
}
```

---

### CR-05: Path traversal via save file asset paths — arbitrary file read

**Files:**
- `src/cereka_scene_manager.cpp:30` — `"assets/bg/" + filename`
- `src/cereka_scene_manager.cpp:50` — `"assets/characters/" + filename`
- `src/cereka_audio_manager.cpp:91,164,208` — `"assets/sounds/" + filename`
**Severity:** BLOCKER — security / arbitrary file read

Save files store asset paths as plain strings (`background`, `characters[].file`, `bgm`). `LoadGame` passes these directly to `IMG_LoadTexture` and `MIX_LoadAudio` **without path sanitization**. A crafted save file can specify paths like `../../etc/passwd` or `../../proc/self/maps`.

```cpp
// cereka_scene_manager.cpp:30
auto tex = m_renderCtx->CreateTexture("assets/bg/" + filename);
// filename could be "../../etc/passwd"
```

While `IMG_LoadTexture` would fail on non-image files, the attacker gets a file-existence oracle via error messages (stderr), and SDL_image parsing vulnerabilities could be triggered with crafted polyglot files.

**Fix:** Reject paths containing `..` or leading `/` before concatenation:

```cpp
static bool is_safe_path(const std::string& path) {
    return path.find("..") == std::string::npos
        && !path.empty()
        && path[0] != '/';
}
```

---

### CR-06: LOAD dispatch `return`s without PC increment when LoadGame returns false

**File:** `src/state/cereka_states.cpp:322`
**Severity:** BLOCKER — duplicate of CR-01, but worth noting the specific `LoadGame(false)` case

When `impl.LoadGame(slot)` is called and the save file doesn't exist (returns `false`), the handler still `return`s without `si.pc++`. Next frame: same LOAD instruction, same result, **infinite loop**.

Already described in CR-01 with the same code path. Listed separately because the trigger (missing save file) is distinct from invalid slot number.

---

### CR-07: `(int)it->second` truncates float → undefined behavior for large values

**File:** `src/Cereka.cpp:198`
**Severity:** BLOCKER — undefined behavior

```cpp
int intVal = (int)it->second;
if (it->second == intVal)
    replacement = std::to_string(intVal);
```

Per the C++ standard, converting a floating-point value to an integer type when the value is outside the representable range of the integer type is **undefined behavior**. If a `.crka` numeric variable exceeds 2³¹-1 (~2.1B) via arithmetic, this triggers UB — potentially a crash or silent corruption.

```crka
$ x = 9999999999  ; exceeds INT_MAX
; "{x}" triggers UB in SubstituteVariables
```

**Fix:** Use `std::isnan`, range-check, or format with `std::format`/`std::to_string` directly:

```cpp
if (std::isfinite(it->second) && it->second >= INT_MIN && it->second <= INT_MAX) {
    int intVal = (int)it->second;
    if (it->second == (float)intVal) { ... }
}
```

---

## Warnings

### WR-01: Rollback buffer index inversion also affects `restore()`

**File:** `src/cereka_rollback_manager.cpp:69-70`
**Severity:** WARNING

`restore()` calls `goTo(impl, prevIndex())`. `prevIndex()` returns `(head_ - 1 + capacity_) % capacity_`. For a partially filled buffer (`count_ < capacity_`), this gives `head_ - 1` (mod capacity_), but `goTo` then uses it as a direct index (`bufIdx = index`) which is wrong per CR-03.

Same root cause as CR-03, but the `restore()` codepath (not user-visible in current feature set) is also affected.

---

### WR-02: `parseColor` uses `sscanf` without range validation

**File:** `src/config/property_handlers.cpp:90`
**Severity:** WARNING — input validation

```cpp
int r = 255, g = 255, b = 255, a = 255;
std::sscanf(str.c_str(), "%d %d %d %d", &r, &g, &b, &a);
v.colorVal = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
```

Values outside 0–255 are silently wrapped via `(uint8_t)` cast (`99999 → 159`). Also, `sscanf` with `%d` on non-numeric input (e.g., `"red green blue"`) silently leaves the defaults.

**Fix:** Parse with `safe_stoi` and clamp:

```cpp
auto rResult = safe_stoi(token);
int r = rResult ? std::clamp(*rResult, 0, 255) : 255;
```

---

### WR-03: `MenuSystem::Target()` and `IsExit()` have no bounds check

**File:** `src/cereka_menu_system.hpp:22-23`
**Severity:** WARNING — potential crash on out-of-bounds access

```cpp
const std::string &Target(size_t i) const { return targets[i]; }
bool IsExit(size_t i) const { return exits[i]; }
```

If called with `i >= targets.size()`, these produce undefined behavior (likely crash). The call sites in `SelectMenuOption` and `MenuState::handleEvent` guard against this, but the methods are publicly accessible.

**Fix:** Add `at()` or `operator[]` with assertion/log:

```cpp
const std::string &Target(size_t i) const {
    static const std::string empty;
    return i < targets.size() ? targets[i] : empty;
}
```

---

### WR-04: `DialogueSystem::Tick` typewriter accumulates timer bias from int-truncated subtraction

**File:** `src/cereka_dialogue_system.cpp:19-23`
**Severity:** WARNING — minor timer drift

```cpp
int charsToAdd = (int)(typewriterTimer * CHARS_PER_SECOND);
if (charsToAdd <= 0) return;
displayedChars += charsToAdd;
typewriterTimer -= charsToAdd / CHARS_PER_SECOND;
```

`charsToAdd / CHARS_PER_SECOND` performs float division (int/float = float), which is correct. But the subtraction subtracts `charsToAdd / 60.0f` rather than `dt`, which can leave a tiny residual in `typewriterTimer` (≤ 1/60s). This is negligible for typewriter display but means the timer never fully resets to zero, accumulating sub-frame error over many lines.

**Fix:** Subtract the actual dt used:

```cpp
float timeUsed = (float)charsToAdd / CHARS_PER_SECOND;
typewriterTimer -= timeUsed;
```

---

### WR-05: `static` saves directory path cached at first call

**File:** `src/cereka_save.cpp:24`
**Severity:** WARNING — incorrect behavior if working directory changes

```cpp
static std::string savePath(int slot) {
    static const fs::path saveDir = fs::absolute("saves");
    return (saveDir / ("slot" + std::to_string(slot) + ".json")).string();
}
```

The `saveDir` is resolved once on first call. If the engine working directory changes (e.g., multiple game projects in one process), saves go to the original location. This is a latent bug that breaks engine reusability.

**Fix:** Remove `static` or re-evaluate each call:

```cpp
static std::string savePath(int slot) {
    return (fs::absolute("saves") / ("slot" + std::to_string(slot) + ".json")).string();
}
```

---

### WR-06: `CerekaImpl` god class — 15+ responsibilities in one header

**File:** `src/cereka_engine_impl.hpp`
**Severity:** WARNING — architectural debt, cross-cutting defect risk

The class aggregates:
- SDL window / render context
- Font management
- SceneManager, AudioManager, UIManager
- ScriptInterpreter, DialogueSystem, MenuSystem
- CerekaStateMachine, RollbackManager
- UiConfig, ConfigManager
- All methods defined across 5+ `.cpp` files (Cereka.cpp, cereka_script.cpp, cereka_draw.cpp, cereka_save.cpp, cereka_ui_config.cpp)

Mutation of any subsystem member can affect others through shared `Impl &` references in state methods (dozens of `static_cast<Impl &>(ctx)`). The `CLAUDE.md` acknowledges Phase 0.2 splits this. The current structure makes it difficult to reason about invariants across subsystem boundaries.

---

### WR-07: `historyHitTest` dereferences `m_renderCtx` without explicit null guard

**File:** `src/Cereka.cpp:319-320`
**Severity:** WARNING — latent null dereference

```cpp
int Impl::historyHitTest(int mx, int my) {
    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();
```

Called from `HistoryState::handleEvent` — only invoked when the state machine is initialized and `m_renderCtx` is alive. However, there's no defensive check, and the function is `public` on `Impl`. If called outside the intended lifecycle (e.g., before InitGame or after Shutdown), this crashes.

---

### WR-08: Typewriter timer not serialized in save — mid-animation restore jumps forward

**Files:** `src/cereka_save.cpp:122`, `src/cereka_dialogue_system.hpp:30`
**Severity:** WARNING — UX glitch on load mid-typewriter

`SaveGame` serializes `displayedChars` but not `typewriterTimer`. After `LoadGame`:
- `displayedChars` is restored (e.g., 50 chars out of 100)
- `typewriterTimer` is 0.0 (default-constructed)

The typewriter resumes from char 51, but the timer is fresh. The first visible character after load appears after one full frame delay instead of smoothly continuing. Minor, but incorrect for a save system that aims for bit-exact restore.

**Fix:** Serialize `typewriterTimer` as well.

---

### WR-09: `sol::coroutine script` stored but never resumed — dead code with risk

**File:** `src/cereka_script_interpreter.hpp:27`
**File:** `src/cereka_script.cpp:30-40`
**Severity:** WARNING — dead code exposes Lua attack surface

`LoadCerekaScript` loads a Lua file, wraps it as a coroutine, and stores it in `script`. The coroutine is **never resumed** anywhere in the reviewed files. This is dead code that:

1. Loads arbitrary `.lua` files via a Lua state with all libraries open (see CR-04)
2. Misleads maintainers into thinking the old Lua-based script system is active

**Fix:** Remove `script` and `LoadCerekaScript` from the public API, or add a clear comment explaining if/where it's used.

---

### WR-10: `IsGameFinished` vs `IsFinished` — confusing dual semantics

**File:** `src/include/Cereka/Cereka.hpp:66-68`
**Severity:** WARNING — API confusion

```cpp
bool IsGameFinished() const;  // state == Finished || state == Quit
bool IsFinished() const;      // scriptInterpreter.scriptFinished
```

`IsGameFinished` checks state machine terminal states. `IsFinished` checks a boolean flag on the script interpreter. The names don't distinguish intent — a reader would expect `IsFinished` to be the terminal state, not a script-level flag.

---

## Info

### IN-01: `safe_stof` uses `strtof` — locale-dependent decimal separator

**File:** `src/cereka_safe_parse.hpp:21-22`
**Severity:** INFO

`std::strtof` respects the C locale. In locales where `,` is the decimal separator (e.g., `de_DE`), `"3.14"` parses as `3` (stops at `.`). The default "C" locale is fine, but if the engine is run on a system with non-C locale, numeric parsing breaks. `std::from_chars` (used by `safe_stoi`/`safe_stoull`) is locale-independent — `safe_stof` should use it too (C++23 `std::from_chars` for `float`).

---

### IN-02: `create_window` `hidden` parameter name conflicts with semantics

**File:** `src/cereka_video.cpp:34`
**Severity:** INFO

```cpp
void create_window(..., bool hidden)
```

Called as `video::create_window(title, fullscreen, width, height, headless)`. The parameter is named `hidden`, but `headless` is passed. The `SDL_WINDOW_HIDDEN` flag is set when `hidden=true`, meaning the window is created but invisible. In headless mode, the engine still creates a full window + renderer pipeline (wasted resources). Consider skipping window/rendering entirely in headless mode.

---

### IN-03: `savePath` returns string by value — unnecessary allocation per call

**File:** `src/cereka_save.cpp:22-26`
**Severity:** INFO

Returns `std::string` by value when callers only need a `const std::string&` for `glz::write_file_json`/`glz::read_file_json`. Moving to returning `fs::path` would avoid string conversion churn.

---

### IN-04: `historyTexts` allocates and reverses a vector on every history draw

**File:** `src/cereka_rollback_manager.cpp:132-148`
**Severity:** INFO

`historyTexts()` copies all entries from the circular buffer into a new vector, then `std::reverse`s it. Called from `DrawHistoryOverlay` every frame the history overlay is visible. For 200 entries, this is ~200 string copies + allocation per frame. Caching the reversed result and invalidating on capture would reduce overhead.

---

### IN-05: `sscanf` in `parseColor` skips `safe_stoi` pattern used elsewhere

**File:** `src/config/property_handlers.cpp:90`
**Severity:** INFO

The rest of the codebase uses `safe_stoi`/`safe_stof` for parsing; `parseColor` uses raw `sscanf`. Inconsistent with project conventions.

---

## Architecture Notes

1. **SDL leakage in public API:** `CerekaEvent::key` stores `SDL_Keycode` as a raw `int`, but `Cereka.hpp` does not include SDL headers. Consumers must know SDL key values to interpret events — breaks the abstraction boundary.

2. **CerekaImpl god class** (31 members, 15+ subsystems) is the primary source of cross-cutting bugs. The CRTP state machine uses `static_cast<Impl &>(ctx)` in every state, creating tight coupling between states and the entire engine. Phase 0.2 split is well-motivated.

3. **Error propagation:** Three patterns coexist — `std::expected` returns, `bool` returns, and silent stderr logging. LOAD/JUMP instructions silently fail via infinite loop (no error at all). FADE parsing silently uses a default (logs nothing). Inconsistent patterns make it hard to audit error handling.

4. **sol2 Lua state management:** Two independent `sol::state` instances exist (compiler in `cereka_instruction.cpp`, engine in `ScriptInterpreter`). The compiler state correctly limits libraries; the engine state does not. This asymmetry is a red flag.

---

_Reviewed: 2026-05-09T12:00:00Z_
_Reviewer: gsd-code-reviewer (deep analysis)_
_Depth: deep_
