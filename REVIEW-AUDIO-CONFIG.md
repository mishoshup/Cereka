---
phase: code-review
reviewed: 2026-05-09T12:00:00Z
depth: deep
files_reviewed: 17
files_reviewed_list:
  - src/cereka_audio_manager.hpp
  - src/cereka_audio_manager.cpp
  - src/cereka_rollback_manager.hpp
  - src/cereka_rollback_manager.cpp
  - src/config/config_manager.hpp
  - src/config/config_manager.cpp
  - src/config/property_types.hpp
  - src/config/property_handlers.cpp
  - src/compiler/cereka_instruction.hpp
  - src/compiler/cereka_instruction.cpp
  - src/cereka_test.cpp
  - scripts/cereka_compiler.lua
  - tests/audio_manager_test.cpp
  - tests/rollback_manager_test.cpp
  - tests/config_test.cpp
  - tests/save_data_test.cpp
  - tests/cereka_script_test.cpp
findings:
  critical: 5
  warning: 12
  info: 6
  total: 23
status: issues_found
---

# Code Review Report: Audio, Rollback, Config, Compiler & Tests

**Reviewed:** 2026-05-09T12:00:00Z
**Depth:** deep
**Files Reviewed:** 17
**Status:** issues_found

## Summary

Reviewed 17 source files across five subsystems: AudioManager (SDL3_mixer integration), RollbackManager (narrative state snapshots), ConfigManager (property map pattern), Compiler (Op enum, Instruction struct, Lua bridge), and Tests. Found 5 critical defects, 12 warnings, and 6 info items.

**Most critical finding:** The C++ bridge in `cereka_instruction.cpp` silently drops 5 instruction types (`PLAY_BGM_FADE`, `STOP_BGM_FADE`, `BGM_CROSSFADE`, `SG_SET`, `SG_REMOVE`) that the Lua compiler emits and the VM expects — these features are compiled but never executed. The snapshot tests pass because they only test Lua compiler output, not the full pipeline.

Also found: RollbackManager's `goTo` has a wrong index formula for wrapped buffers; AudioManager destroys current BGM before new load succeeds; and test coverage is superficial across all subsystems.

---

## Critical Issues

### CR-01: C++ bridge silently drops PLAY_BGM_FADE, STOP_BGM_FADE, BGM_CROSSFADE, SG_SET, SG_REMOVE

**File:** `src/compiler/cereka_instruction.cpp:68-136`
**Issue:** The Lua compiler emits these op string names (confirmed by snapshot tests `tests/compile/expected/audio_fade.txt` and `tests/compile/expected/scene_graph.txt`), and the VM in `src/state/cereka_states.cpp` has full switch cases for the corresponding `Op` enum values. But the C++ string→enum mapping in `RunLuaCompiler` does NOT have `else if` branches for:
- `"PLAY_BGM_FADE"` → `Op::PLAY_BGM_FADE`
- `"STOP_BGM_FADE"` → `Op::STOP_BGM_FADE`
- `"BGM_CROSSFADE"` → `Op::BGM_CROSSFADE`
- `"SG_SET"` → `Op::SG_SET`
- `"SG_REMOVE"` → `Op::SG_REMOVE`

All five hit the fallback at line 135:
```cpp
std::cerr << "[CEREKA] Unknown op: " << op << "\n";
continue;  // ← instruction dropped from program
```
The instruction is discarded. The game silently fails to fade/crossfade BGM and fails to perform any scene graph operations. Snapshot tests never catch this because they test Lua output, not the C++ bridge.

**Fix:** Add the missing `else if` branches. For example, between the `CALL` and `UI_SET` blocks:
```cpp
else if (op == "PLAY_BGM_FADE")
    ins.op = Op::PLAY_BGM_FADE;
else if (op == "STOP_BGM_FADE")
    ins.op = Op::STOP_BGM_FADE;
else if (op == "BGM_CROSSFADE")
    ins.op = Op::BGM_CROSSFADE;
else if (op == "SG_SET")
    ins.op = Op::SG_SET;
else if (op == "SG_REMOVE")
    ins.op = Op::SG_REMOVE;
```

### CR-02: RollbackManager goTo index formula wrong for wrapped circular buffer

**File:** `src/cereka_rollback_manager.cpp:78-82`
**Issue:** When `count_ == capacity_` (buffer has wrapped), the `goTo` method computes the physical buffer index using:
```cpp
bufIdx = (head_ - 1 - index + capacity_) % capacity_;
```
This is wrong. The correct mapping from logical index (0 = oldest, count-1 = newest) to physical index in a wrapped ring buffer is:
```cpp
bufIdx = (head_ + index) % capacity_;
```

The current formula maps index=0 to the newest and index=capacity-1 to the oldest — the inverse of what is expected. Furthermore, `restore()` passes `prevIndex()` (a physical index) as `index`, but the wrapped-branch re-interprets it as a logical index, creating a type confusion.

This means `restore()` and `goTo()` load from incorrect slots when the buffer is full and has wrapped (i.e., after more than `capacity_` captures).

**Trace:**
- `capacity_ = 5`, `head_ = 2` (next write at index 2, 7 captures done, wrapped)
- `prevIndex()` = `(2 - 1 + 5) % 5 = 1` → physical index of newest snapshot ✓
- `restore()` calls `goTo(impl, 1)` where `index = 1`
- Wrapped branch computes: `(2 - 1 - 1 + 5) % 5 = 5 % 5 = 0` → loads buffer[0] instead of buffer[1]
- Wrong snapshot is loaded

**Fix:** Replace the wrapped-branch formula:
```cpp
if (count_ < capacity_) {
    bufIdx = index;
} else {
    bufIdx = (head_ + index) % capacity_;
}
```

### CR-03: AudioManager destroys current BGM before new file is loaded

**File:** `src/cereka_audio_manager.cpp:88-103`
**Issue:** `PlayBGM()` calls `destroyBgmHandles()` at line 88 *before* attempting to load the new file at line 92. If `MIX_LoadAudio()` fails, the old BGM handles are already destroyed and there's no way to recover. The currently playing music is silenced permanently.

```cpp
void AudioManager::PlayBGM(const std::string &filename, float fadeDuration)
{
    if (!initialized) return;
    destroyBgmHandles();         // ← old BGM destroyed HERE
    bgmPath = filename;
    std::string path = "assets/sounds/" + filename;
    bgmAudio = MIX_LoadAudio(mixer, path.c_str(), true);   // ← may fail
    if (!bgmAudio) {
        // bgmTrack and bgmAudio are both null now
        // The currently playing music is gone with no recovery
        return;
    }
    ...
}
```

If the file is missing, renamed, or a path error occurs, the user hears silence instead of the previous BGM continuing. The `CrossfadeBGM` method does this correctly (preserves old handles in `fadeState_` on failure), but `PlayBGM` does not.

**Fix:** Defer `destroyBgmHandles()` until after the new file loads successfully:
```cpp
void AudioManager::PlayBGM(const std::string &filename, float fadeDuration)
{
    if (!initialized) return;

    std::string path = "assets/sounds/" + filename;
    MIX_Audio *newAudio = MIX_LoadAudio(mixer, path.c_str(), true);
    if (!newAudio) {
        std::cerr << "[CEREKA] Failed to load BGM: " << path << " — " << SDL_GetError() << "\n";
        return;
    }

    MIX_Track *newTrack = MIX_CreateTrack(mixer);
    if (!newTrack) {
        std::cerr << "[CEREKA] Failed to create BGM track: " << SDL_GetError() << "\n";
        MIX_DestroyAudio(newAudio);
        return;
    }

    // All new resources ready — destroy old ones
    destroyBgmHandles();
    bgmPath = filename;
    bgmAudio = newAudio;
    bgmTrack = newTrack;

    MIX_SetTrackAudio(bgmTrack, bgmAudio);
    ...
}
```

### CR-04: sol2 `protected_function_result` cast without type check can crash

**File:** `src/compiler/cereka_instruction.cpp:46-49`
**Issue:** After a successful `compileFunc(scriptText)` call, the code casts the result to `sol::table` without checking if it IS a table:
```cpp
sol::table result = res;        // line 46: unchecked cast
if (!result.valid()) {          // line 47: only checks if cast gave a valid ref
    return std::unexpected(...);
}
```
If the Lua `compile()` function returns a non-table (e.g., a string, number, or nil due to a Lua runtime bug), this cast silently produces an invalid `sol::table` reference. The `valid()` check at line 47 catches some cases, but accessing `result["instructions"]` on a dangling reference is undefined behavior and can crash.

**Fix:** Use `res.is<sol::table>()` before casting, or use `res.get<sol::table>()` which returns `std::optional` / `sol::optional`:
```cpp
if (!res.is<sol::table>()) {
    return std::unexpected("[CEREKA] Compiler result is not a table");
}
sol::table result = res;
```

### CR-05: RollbackManager does not capture/restore Lua interpreter state

**File:** `src/cereka_rollback_manager.cpp:20-65`
**Issue:** `ScriptInterpreter` holds a `sol::state lua` and `sol::coroutine script` that contain runtime Lua state (evaluated expression caches, coroutine resume points). Neither is captured by `capture()` nor restored by `restore()`. The rollback-based restore resets `variables`, `numVariables`, `pc`, `callStack` etc., but the `sol::state` object is left dirty — containing stale Lua closures, evaluated expression results, and coroutine execution state that is inconsistent with the restored variables.

If the script interpreter uses the Lua coroutine for execution (as the field `sol::coroutine script` suggests), a rollback would result in a state mismatch between the restored C++ state and the Lua coroutine's internal instruction pointer, causing undefined behavior or crashes on the next `Update()` call.

**Fix:** Either (a) wrap `sol::state` state capture and restore in `capture()`/`restore()`, (b) don't use `sol::coroutine` for script execution and remove it to avoid confusion, or (c) document that the Lua state must be re-initialized from scratch after a rollback. Given the complexity of serializing a `sol::state`, option (b) or (c) is most practical.

---

## Warnings

### WR-01: AudioManager does not check MIX_PlayTrack / MIX_PlayAudio return values

**File:** `src/cereka_audio_manager.cpp:110,191,219`
**Issue:** The return values of `MIX_PlayTrack(bgmTrack, props)` (lines 110 and 191) and `MIX_PlayAudio(mixer, it->second)` (line 219) are never checked. These calls can fail (e.g., track/audio device disconnected, channel exhaustion for SFX), but the code treats them as infallible. The BGM appears to be playing but silently produces no output.

**Fix:** Check return values and log errors, e.g.:
```cpp
if (!MIX_PlayTrack(bgmTrack, props)) {
    std::cerr << "[CEREKA] MIX_PlayTrack failed: " << SDL_GetError() << "\n";
}
```

### WR-02: ConfigManager::getValue returns "" for unknown keys

**File:** `src/config/config_manager.cpp:165-236`
**Issue:** `getValue()` does an if-else chain but never validates the key via `getDef()` first. If called with an unknown key like `"textbox.nonexistent"`, it silently returns `""`. A caller cannot distinguish "property is set to empty string" from "property doesn't exist". This can hide configuration bugs.

**Fix:** Add a key validation at the top:
```cpp
std::string ConfigManager::getValue(const std::string &key) const {
    if (!getDef(key)) {
        std::cerr << "[CONFIG] Unknown property: " << key << "\n";
        return "";
    }
    if (!ctx_.uiCfg) return "";
    ...
```

### WR-03: PropertyValue::asColor uses raw sscanf with no bounds or error checking

**File:** `src/config/config_manager.cpp:128-132`
**Issue:** `std::sscanf` with `%d` reads into `int` variables, then truncates to `uint8_t`. If the value is outside 0-255 (e.g., "99999 0 0 0"), it silently truncates to `(uint8_t)99999 = 159` or worse. `sscanf` also doesn't validate that all 4 values were successfully parsed (it returns number of items matched, which is ignored).

**Fix:** Use safe parsing:
```cpp
Color PropertyValue::asColor() const {
    auto parts = safe_stoi(serialized);  // parse ints component by component
    auto parsed = serializers::parseColorStrict(serialized);
    return parsed.value_or(Color{255, 255, 255, 255});
}
```

### WR-04: Duplicate `serializeDim` implementations with different rounding logic

**File:** `src/config/config_manager.cpp:171-181` and `src/config/property_handlers.cpp:183-192`
**Issue:** Two separate implementations of `serializeDim`:
- config_manager.cpp uses tolerance-based rounding (`if (pct >= rounded - 0.001f && pct <= rounded + 0.001f)`)
- property_handlers.cpp uses exact float comparison (`if (pct == static_cast<int>(pct))`)

The exact comparison is unreliable for floats — values like `0.75 * 100.0` might not compare equal to `75` due to floating-point representation. This means the same `Dim` value can serialize differently depending on which code path is hit.

**Fix:** Use the tolerance-based implementation in both places, or better, create a shared helper function in property_types.hpp and use it from both files.

### WR-05: RollbackManager::capture resizes dialogueTexts_ to buffer size every call — O(capacity) per capture

**File:** `src/cereka_rollback_manager.cpp:60`
**Issue:** `dialogueTexts_.resize(buffer_.size())` is called on every `capture()`. If capacity is 200, this is 200 assignments every capture, even though the size only needs to change when capacity changes. `setCapacity()` already handles resizing the buffer, so `dialogueTexts_` should be resized there too.

**Fix:** Move the resize to `setCapacity()`:
```cpp
void RollbackManager::setCapacity(size_t cap) {
    capacity_ = cap;
    buffer_.resize(cap);
    dialogueTexts_.resize(cap);  // <-- add here
    ...
}
```

### WR-06: Sol coroutine state not captured — rollback can leave Lua state inconsistent

**File:** `src/cereka_rollback_manager.cpp:20-65`
**Issue:** (Continuation of CR-05 but at WARNING level since the coroutine might not be actively used.) `capture()` reads and stores `variables` and `numVariables` from `ScriptInterpreter`, but `sol::state lua` and `sol::coroutine script` are opaque C++ objects that can't be snapshotted. Any runtime state the Lua environment holds (evaluated closures, upvalues, coroutine frames) is lost on restore. This can cause `EvalExpr` or coroutine re-entry to operate on inconsistent data.

**Fix:** Audit whether `sol::coroutine script` is actually needed for execution. If not, remove the field to eliminate the risk. If yes, the entire Lua state must be reconstructed on rollback (create fresh `sol::state`, reload compiler, reinitialize).

### WR-07: LookupNumVar and EvalExpr use empty catch blocks that swallow all exceptions

**File:** `src/cereka_script_interpreter.cpp:21-25, 67-72`
**Issue:** Both `LookupNumVar` and `parseFactor` catch exceptions with `catch (...) { }` (empty body). This swallows `std::bad_alloc`, `std::out_of_range`, and any other unexpected exceptions without logging. A malformed variable value silently returns 0.0f, making numeric bugs extremely hard to diagnose.

```cpp
try { return std::stof(sit->second); }
catch (...) { }  // ← swallowed silently
```

**Fix:** At minimum log the error. For `LookupNumVar`, use `safe_stof` instead of `std::stof`:
```cpp
auto r = safe_stof(sit->second);
if (r) return *r;
// fallthrough to return 0.0f
```

### WR-08: No include cycle detection beyond depth limit

**File:** `src/compiler/cereka_instruction.cpp:171-175`
**Issue:** The recursive `CompileFile` enforces a MAX_DEPTH of 32 but does not detect cycles. If file A includes file B which includes file A, the recursion will hit the depth limit with a generic "depth limit reached" error that gives no indication it's a cycle. For user-authored content this is frustrating — the user could fix a cycle if told about it, but "depth limit reached" sounds like an internal limit.

**Fix:** Track visited paths in a set:
```cpp
static std::expected<...> CompileFile(const fs::path &path, int depth,
                                       std::unordered_set<fs::path> &visited) {
    if (!visited.insert(path.filename()).second) {
        return std::unexpected("[CEREKA] Circular include detected: " + path.string());
    }
    ...
}
```

### WR-09: parseKeyList only recognizes 9 key names — common keys silently ignored

**File:** `src/config/property_handlers.cpp:25-35,104-122`
**Issue:** `KEY_MAP` only maps 9 named keys: space, enter, return, escape, tab, up, down, left, right. Keys like `a`-`z`, `0`-`9`, `F1`-`F12`, backspace, delete, home, end, pageup, pagedown are not recognized. If a user writes `ui advance_keys a b c`, the unrecognized tokens are silently consumed by the loop at line 113-119 and ignored — no error, no warning. The property appears to have been set but common keys are missing.

**Fix:** Either (a) build a comprehensive key map from `SDL_Keycode` names, (b) fall back to `SDL_GetKeyName()` or `SDL_GetKeyFromName()` for unrecognized tokens, or (c) warn on unrecognized tokens.

### WR-10: AudioManager SFX cache never evicts — monotonically growing

**File:** `src/cereka_audio_manager.cpp:215`
**Issue:** The `sfxCache` unordered_map stores every loaded SFX audio forever. In a game with many sound effects or a long play session, this grows without bound. There is no LRU eviction, no max size, and no explicit `UnloadSFX` function.

**Fix:** Add a maximum cache size with LRU eviction, or add an `UnloadSFX(const std::string &filename)` method.

### WR-11: Tests don't exercise core rollback functionality

**File:** `tests/rollback_manager_test.cpp:1-53`
**Issue:** All 7 tests only check construction, capacity, and clearing. None call `capture()`, `restore()`, or `goTo()`. The most critical functionality (snapshot/restore roundtrip) is completely untested, including the index calculation bug (CR-02).

**Fix:** Add tests for: single capture + restore, multiple captures (both non-wrapped and wrapped), restore to specific index, restore into a modified engine state, and edge cases (zero capacity, single capture, overflow).

### WR-12: AudioManager tests only test a private helper function

**File:** `tests/audio_manager_test.cpp:1-65`
**Issue:** All audio tests only test the anonymous-namespace `applyCurve` function and the `BgmFade` struct defaults. They don't test any public API of `AudioManager` (Init, PlayBGM, StopBGM, CrossfadeBGM, PlaySFX, Shutdown). The `applyCurve` function is not even accessible outside the translation unit. These tests verify curve math but provide zero coverage of audio lifecycle management, error handling, or state transitions.

**Fix:** Either (a) make `applyCurve` a public/accessible function and test it, or (b) add integration tests that test AudioManager's public interface using mock SDL_mixer functions. The tests should at minimum cover: Init/Shutdown, PlayBGM when uninitialized (no-op), and the `destroyBgmHandles` cleanup path.

---

## Info

### IN-01: ConfigManager::initDefaults() is a no-op — misleading

**File:** `src/config/config_manager.cpp:153-157`
**Issue:** `initDefaults()` does nothing. The comment says "Property table is statically initialized — nothing to do here", but this is misleading because the property table and the UiConfig defaults are separate things. The function name implies it applies defaults to the UI config, but it doesn't. If defaults aren't already set in the UiConfig struct by the time `getValue()` is called, all values return `""`.

**Fix:** Either remove `initDefaults()` if it serves no purpose, or have it iterate the property table and apply UiConfig defaults.

### IN-02: ConfigManager::apply() writes raw parsed values without validation

**File:** `src/config/config_manager.cpp:238-341`
**Issue:** Several property assignments directly assign `parsed.floatVal` or `parsed.intVal` without any range validation. For example, `textbox.line_spacing` (line 277) accepts any float including negative or extremely large values. `font.size` (line 328) accepts 0 or negative values which could crash SDL_ttf. Consider adding validation bounds.

### IN-03: Inconsistent use of plural vs singular in CerekaImpl includes

**File:** `src/cereka_engine_impl.hpp:10`
**Issue:** The file includes `cereka_dialogue_system.hpp`, `cereka_menu_system.hpp`, and `cereka_scene_manager.hpp`. These are class names but the file naming convention mixes `_system` with `_manager`. For consistency with `AudioManager` and `RollbackManager`, the scene equivalent could be `cereka_scene_manager.hpp` (which it is), but dialogue and menu don't follow the `_manager` convention. Minor naming inconsistency.

### IN-04: Line splitter in Lua compiler doesn't handle \r\n mid-line

**File:** `scripts/cereka_compiler.lua:33-36`
**Issue:** The `\r\n` handling works correctly for Windows line endings, but the CRLF check at line 36 could be slightly wrong if a bare `\r` appears without `\n` — it would be left in the line text. This is unlikely in practice since most editors normalize line endings, but worth noting.

### IN-05: `safe_stof` uses `std::strtof` which is locale-dependent

**File:** `src/cereka_safe_parse.hpp:21-22`
**Issue:** `std::strtof` behavior depends on the C locale's `LC_NUMERIC`. In locales where `,` is the decimal separator, strings like "3.14" might parse incorrectly (stopping at the `.`). For cross-platform game engines that may run on user-configured systems, this is a latent bug. The `.crka` script language uses `.` as decimal separator, but if the game is running on a German-locale system, `strtof("3.14")` could return 3.0f.

**Fix:** Use `std::from_chars` for `safe_stof` too (consistent with `safe_stoi`), as `from_chars` is locale-independent. However, C++17 `from_chars` for `float` may not be available on all toolchains — use it if available, or set locale to "C" before calling `strtof`.

### IN-06: AudioManager BGM path string concatenation vulnerable to path traversal

**File:** `src/cereka_audio_manager.cpp:91,164,208`
**Issue:** The path `"assets/sounds/" + filename` is constructed via string concatenation with no sanitization. While the engine trusts game authors (not a security boundary), a malformed filename like `"../../etc/passwd"` would reference files outside the project directory. This is not a security vulnerability in the traditional sense for a game engine, but could cause confusing errors. A simple normalization check or `std::filesystem::weakly_canonical` validation would prevent this.

---

## Summary by Subsystem

| Subsystem | Critical | Warning | Info | Key Concern |
|-----------|----------|---------|------|-------------|
| AudioManager | 1 | 2 | 1 | Destroys before loading |
| RollbackManager | 2 | 2 | 0 | Index formula wrong; Lua state not captured |
| ConfigManager | 0 | 3 | 2 | sscanf unsafety; no key validation |
| Compiler (C++ bridge) | 2 | 1 | 0 | 5 ops silently dropped; cast without type check |
| Lua compiler | 0 | 1 | 1 | No cycle detection; \r edge case |
| Tests | 0 | 2 | 0 | Rollback + Audio tests don't test actual functionality |

---

_Reviewed: 2026-05-09T12:00:00Z_
_Reviewer: gsd-code-reviewer (deep mode)_
_Depth: deep_
