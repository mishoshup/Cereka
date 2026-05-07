---
phase: 03-architectural-cleanup
reviewed: 2026-05-07T12:00:00Z
depth: standard
files_reviewed: 35
files_reviewed_list:
  - .github/workflows/ci.yml
  - CMakeLists.txt
  - include/Cereka/Cereka.hpp
  - include/Cereka/exceptions.hpp
  - runner/main.cpp
  - src/Cereka.cpp
  - src/cereka_audio_manager.cpp
  - src/cereka_audio_manager.hpp
  - src/cereka_dialogue_system.cpp
  - src/cereka_dialogue_system.hpp
  - src/cereka_draw.cpp
  - src/cereka_engine_impl.hpp
  - src/cereka_menu_system.cpp
  - src/cereka_menu_system.hpp
  - src/cereka_safe_parse.hpp
  - src/cereka_save.cpp
  - src/cereka_save_data.hpp
  - src/cereka_scene_manager.cpp
  - src/cereka_scene_manager.hpp
  - src/cereka_script.cpp
  - src/cereka_script_interpreter.cpp
  - src/cereka_script_interpreter.hpp
  - src/cereka_text_renderer.cpp
  - src/cereka_text_renderer.hpp
  - src/cereka_ui_config.cpp
  - src/cereka_ui_config.hpp
  - src/cereka_video.cpp
  - src/cereka_video.hpp
  - src/compiler/cereka_instruction.cpp
  - src/compiler/cereka_instruction.hpp
  - src/config/config_manager.cpp
  - src/config/config_manager.hpp
  - src/config/property_handlers.cpp
  - src/config/property_types.hpp
  - src/renderer/irecture.hpp
  - src/renderer/irender_context.hpp
  - src/renderer/sdl_render_context.cpp
  - src/renderer/sdl_render_context.hpp
  - src/state/cereka_state.hpp
  - src/state/cereka_states.cpp
  - src/state/cereka_states.hpp
  - src/ui/ui_manager.cpp
  - src/ui/ui_manager.hpp
  - tests/cereka_script_test.cpp
  - tests/config_test.cpp
findings:
  critical: 5
  warning: 6
  info: 3
  total: 14
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-05-07T12:00:00Z
**Depth:** standard
**Files Reviewed:** 35
**Status:** issues_found

## Summary

Reviewed the entire Cereka engine source surface (35 files) at standard depth. Found **5 blocking issues** (incorrect behavior or data loss risk), **6 warnings** (quality/robustness deficits), and **3 info items**. The most serious issues involve:

1. **ESC key re-pushes save/load overlay immediately** — pressing ESC to dismiss SaveMenuState or LoadMenuState pops the overlay then immediately re-pushes it, trapping the user.
2. **`clearOverlays()` + `changeState()` double-calls `onExit`** — `LoadGame` calls `clearOverlays()` then `changeState()`, but `changeState` also calls `onExit` because `currentState_` is not nulled by `clearOverlays`.
3. **ELSE handler unconditionally resets `skipDepth=1`** — corrupts nesting depth tracking inside multi-level skipped if-blocks.
4. **`create_window` ignores width/height parameters** — windowed mode always uses full display dimensions.
5. **`safe_stof` accepts partial consumption** — `std::from_chars` success with trailing chars is not detected.

---

## Critical Issues

### CR-01: ESC in save/load overlay immediately re-pushes the overlay (user trapped)

**File:** `src/Cereka.cpp:253-268`
**File:** `src/state/cereka_states.cpp:328,355`

**Issue:** When ESC is pressed while SaveMenuState or LoadMenuState is active, `Impl::HandleEvent` processes the event in two stages:

1. `m_stateMachine.handleEvent(e)` — the overlay state's handler runs `popOverlay()`, restoring `currentType_` to the underlying state (e.g. `Running`)
2. The global handler then matches `e.type == KeyDown && e.key == SDLK_ESCAPE` and sees `currentType_ == Running`, so it immediately calls `pushOverlay(SaveMenuState)` again

Net result: pressing ESC in the save menu **pops the overlay and instantly re-pushes it**. The user is trapped — every ESC push restores the overlay rather than dismissing it. Each press also adds another entry to `overlayStack_`, so escaping requires N presses for N accidental opens.

**Fix:** Add a guard in the global ESC handler to check whether the state machine just had an overlay popped (i.e., the event was already consumed). The simplest fix is to move the global ESC handler *before* the state machine dispatch, or return early from the state handler if ESC was consumed:

Option A — In `Impl::HandleEvent`, check if an overlay was active before dispatching:
```cpp
void Impl::HandleEvent(const CerekaEvent &e)
{
    // Global events handled first
    if (e.type == CerekaEvent::Quit) {
        changeState(CerekaState::Quit);
        return;
    }
    if (e.type == CerekaEvent::KeyDown && e.key == SDLK_ESCAPE) {
        auto cur = m_stateMachine.currentType();
        if (cur == CerekaState::SaveMenuState || cur == CerekaState::LoadMenuState) {
            // Let the overlay state handle ESC via its own handler below
            m_stateMachine.handleEvent(e);
            return;
        }
        if (cur == CerekaState::WaitingForInput || cur == CerekaState::Running) {
            pushOverlay(CerekaState::SaveMenuState);
            return;
        }
    }
    m_stateMachine.handleEvent(e);
    // ... remaining advance-key check
}
```

### CR-02: `clearOverlays()` + `changeState()` in LoadGame double-calls `onExit`

**File:** `src/state/cereka_state.hpp:260-267`
**File:** `src/cereka_save.cpp:183-184`

**Issue:** `Impl::LoadGame()` calls:
```cpp
m_stateMachine.clearOverlays();  // calls onExit on current state, but does NOT null currentState_
m_stateMachine.changeState(parseState(data.state));  // calls onExit AGAIN on stale currentState_
```

`clearOverlays()` calls `currentState_->onExit(*ctx_)` but never sets `currentState_ = nullptr`. Then `changeState()` checks `if (!currentState_ || !ctx_)` — currentState_ is still non-null, so it calls `onExit(*ctx_)` a **second time** on the same (overlay) state before replacing it with the restored state.

Double `onExit` can corrupt internal state (e.g., double-free or unbalanced reference counts).

**Fix:** `clearOverlays()` must null `currentState_` before returning, or `changeState()` must tolerate an already-exited state. The cleanest fix:

```cpp
void clearOverlays()
{
    std::cout << "[STATE] clearOverlays (was: " << stateLabel(currentType_) << ")\n";
    if (currentState_) {
        currentState_->onExit(*ctx_);
        currentState_ = nullptr;  // <-- ADD THIS
    }
    overlayStack_.clear();
}
```

### CR-03: ELSE handler unconditionally resets `skipDepth=1`, corrupting nesting depth in skipped blocks

**File:** `src/state/cereka_states.cpp:183-189`

**Issue:** The `ELSE` case in `DialogueState::update` always sets `skipDepth = 1`:

```cpp
case compiler::Op::ELSE:
    si.skipMode = true;
    si.skipDepth = 1;  // <-- unconditional; corrupts depth when already in skip-mode
    si.pc++;
    continue;
```

When an outer `if` condition is false and an inner `if-else` block is encountered, the inner ELSE overwrites `skipDepth` from 2 to 1. This causes the inner `ENDIF` to decrement skipDepth to 0 and clear skipMode prematurely, potentially executing instructions that should be skipped.

**Example that would break:**
```
if(false)            → skipMode=true, skipDepth=1
  if(true)           → skipDepth=2
    say "inner"
  else               → skipDepth=1 (WRONG! was 2)
    say "inner else"
  endif              → skipDepth=0, skipMode=false (PREMATURE!)
  say "should be skipped"  ← INCORRECTLY EXECUTED
endif
say "done"
```

**Fix:** The ELSE handler must only set skipDepth when it is NOT already in skip mode. When already in skip mode, ELSE is inside a skipped block and should be ignored (the current skipDepth should be preserved):

```cpp
case compiler::Op::ELSE:
    if (!si.skipMode) {
        // We executed the IF branch; now skip the ELSE branch
        si.skipMode = true;
        si.skipDepth = 1;
    }
    // If already in skipMode, ELSE is inside a skipped outer block;
    // preserve the existing skipDepth.
    si.pc++;
    continue;
```

### CR-04: `create_window` ignores width and height parameters

**File:** `src/cereka_video.cpp:30-48`

**Issue:** The function accepts `width` and `height` parameters but completely ignores them, always using the display's native resolution:

```cpp
void create_window(const char *title, bool fullscreen, int width, int height)
{
    // ...
    SDL_DisplayID id = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(id);
    video::width  = mode->w;   // ignores passed width
    video::height = mode->h;   // ignores passed height
    // ...
}
```

A game configured at 1280×720 windowed will instead get a full-resolution window (1920×1080, 2560×1440, etc.), breaking the layout math everywhere (textbox positioning, menu centering, character placement all use `screenWidth`/`screenHeight`).

**Fix:** Use the passed dimensions when not in fullscreen mode:

```cpp
void create_window(const char *title, bool fullscreen, int width_, int height_)
{
    Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
        SDL_DisplayID id = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(id);
        video::width  = mode->w;
        video::height = mode->h;
    } else {
        video::width  = width_;
        video::height = height_;
    }
    video::window = SDL_CreateWindow(title, video::width, video::height, flags);
    // ...
}
```

### CR-05: `safe_stof` / `safe_stoi` do not validate full string consumption

**File:** `src/cereka_safe_parse.hpp:21-23,33-35`

**Issue:** `std::from_chars` returns a `ptr` pointing past the last consumed character, and `ec` indicating success/failure. The current code only checks `ec`, not `ptr`. This means strings like `"75%"` or `"123abc"` are accepted without error — `from_chars` parses the leading numeric portion and stops, reporting success.

This creates a latent bug surface for any caller that does not separately guard against trailing characters. Currently `Dim::parse` compensates by checking `s.back() == '%'`, but future callers using `safe_stof` on non-numeric-terminated strings would silently get wrong results.

**Fix:** Validate that the entire string was consumed:

```cpp
inline std::expected<float, std::string> safe_stof(const std::string &s) noexcept
{
    if (s.empty())
        return std::unexpected(std::string("empty string"));

    float val = 0.0f;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec != std::errc{} || ptr != s.data() + s.size())
        return std::unexpected(std::string("invalid float: '") + s + "'");
    return val;
}
```

(Same fix for `safe_stoi` and `safe_stoull`.)

---

## Warnings

### WR-01: `asColor` and `parseColor` use `sscanf` without return-value validation

**File:** `src/config/config_manager.cpp:130`
**File:** `src/config/property_handlers.cpp:89-91`

**Issue:** Both functions call `std::sscanf(str.c_str(), "%d %d %d %d", &r, &g, &b, &a)` and silently use initializer values (255 for all channels) if parsing fails or returns fewer than 4 matches. Passing `"abc"` would leave all channels at 255 with no error logged. Passing `"0 0"` sets r=0,g=0 but leaves b=255,a=255.

**Fix:** Check the return value and log a warning on malformed input:

```cpp
Color PropertyValue::asColor() const
{
    int r = 0, g = 0, b = 0, a = 255;
    int matched = std::sscanf(serialized.c_str(), "%d %d %d %d", &r, &g, &b, &a);
    if (matched < 3) {
        std::cerr << "[CONFIG] Malformed color value: '" << serialized << "'\n";
        return {255, 255, 255, 255};
    }
    if (matched < 4) a = 255;
    return {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
}
```

### WR-02: Dead code — old Lua coroutine path never called

**File:** `src/cereka_script.cpp:30-40`
**File:** `src/cereka_script_interpreter.hpp:21-22`

**Issue:** `Impl::LoadCerekaScript()`, the `sol::state lua` member, and the `sol::coroutine script` member are vestigial artifacts of the old Lua-based script interpretation. The runner (`runner/main.cpp`) and all game entry points use `CompileCerekaScript()` + `LoadCompiledCerekaScript()` instead. The old path is never called. It templates `sol::state lua` into every `ScriptInterpreter` instance, which is expensive (Lua state creation is not free).

**Fix:** Remove `sol::state lua` and `sol::coroutine script` from `ScriptInterpreter`, and remove `Impl::LoadCerekaScript()`.

### WR-03: Dead code — `saveDataToJson` and `jsonToSaveData` helper functions

**File:** `src/cereka_save_data.hpp:53-77`

**Issue:** These inline functions are defined but never called. `Impl::SaveGame` and `Impl::LoadGame` use `glz::write_file_json`/`glz::read_file_json` directly. The dead functions are unused and represent a maintenance burden if the save schema changes.

**Fix:** Remove these two functions. If they're intended as a public API for external tools, move them to a separate header with a clear consumer.

### WR-04: Runner uses `std::stoi` without error handling on config values

**File:** `runner/main.cpp:133-134`

**Issue:**
```cpp
int width = cfg.count("width") ? std::stoi(cfg["width"]) : 1280;
int height = cfg.count("height") ? std::stoi(cfg["height"]) : 720;
```
If `game.cfg` contains `width = abc`, `std::stoi` throws `std::invalid_argument`. This crash is unrecoverable and confusing to end users.

**Fix:** Use the `safe_stoi` parser from the engine, or wrap in a try-catch with a fallback to default and a log message.

### WR-05: `parseConfig` declares a `trim` lambda inside the read loop

**File:** `runner/main.cpp:52-56`

**Issue:** The `trim` lambda is defined on every iteration of the while loop. This is inefficient and suggests the lambda should be hoisted outside the loop.

**Fix:** Move the `trim` lambda outside the while loop.

```cpp
auto trim = [](std::string s) -> std::string {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
};
while (std::getline(f, line)) {
    // ...
    line = trim(line);
    // ...
}
```

### WR-06: `SubstituteVariables` compares float to int-cast for integer display heuristic

**File:** `src/Cereka.cpp:195-199`

**Issue:**
```cpp
int intVal = (int)it->second;
if (it->second == intVal)
    replacement = std::to_string(intVal);
else
    replacement = std::to_string(it->second);
```
The comparison `it->second == intVal` converts `intVal` to float. For values like `3.0000001f` resulting from imprecise float arithmetic, `(int)3.0000001f == 3` but `3.0000001f != 3.0f`, so the float path is taken, producing strings like `"3.000000"` instead of `"3"`. This is cosmetic but visible to the user.

**Fix:** Use a tolerance-based comparison or always format via a consistent method:

```cpp
if (std::fabs(it->second - std::round(it->second)) < 0.0001f)
    replacement = std::to_string(static_cast<int>(std::round(it->second)));
else
    replacement = std::to_string(it->second);
```

---

## Info

### IN-01: Buzzword comments in headers

**File:** `src/config/config_manager.hpp:5`

Line 5: `// Enterprise-grade config system with Property Map Pattern.`

**File:** `src/state/cereka_state.hpp:7-9`

Lines 7-9:
```
// Enterprise patterns:
// - RAII for resource management
// - std::expected for error handling
// - Pure virtual interfaces
// - Self-documenting code
```

The phrase "Enterprise-grade" and the "Enterprise patterns" list add no information; the code either is or isn't well-designed. Per the project's `/no-buzzwords` skill, these should be removed.

### IN-02: Redundant include guard + `#pragma once`

**File:** `src/state/cereka_state.hpp:1,24-26`
**File:** `src/state/cereka_states.hpp:1,15-17`
**File:** `src/cereka_save_data.hpp:2,6-8`

These headers use both `#pragma once` and `#ifndef`/`#define` guards. `#pragma once` is sufficient and universally supported by all toolchains targeting this project (MSVC, GCC, Clang). The `#ifndef` guards are redundant.

### IN-03: Global mutable state in `cereka_video.hpp`

**File:** `src/cereka_video.hpp:6-8`

```cpp
extern SDL_Window *window;
extern int width;
extern int height;
```

These mutable globals are set by `create_window` and read by `Impl::InitGame`. They create implicit coupling between translation units and prevent the video subsystem from being instantiated multiple times (e.g., for testing). This is a known pattern for SDL games but is worth flagging as the project matures. Consider wrapping in a `VideoContext` struct owned by `CerekaImpl`.

---

_Reviewed: 2026-05-07T12:00:00Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
