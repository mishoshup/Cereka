# Code Conventions

**Analysis Date:** 2026-05-07

## Naming

**Files:**
- `snake_case` for all source files: `scene_manager.cpp`, `config_manager.hpp`, `script_vm.cpp`
- Paired `.hpp`/`.cpp` for every class except header-only utilities
- Public API headers under `include/Cereka/` (namespace-matched directory)
- Private internal headers live in `src/` — never pulled into public headers

**Classes and Structs:**
- `PascalCase` throughout: `CerekaEngine`, `SceneManager`, `AudioManager`, `DialogueSystem`, `SerializableSaveData`
- Interface classes prefixed with `I`: `IVNState`, `IVNStateContext`
- Nested enums use `PascalCase` for both type and values: `FadePhase::None`, `FadePhase::Out`

**Free Functions:**
- Engine impl methods: `PascalCase` — `InitGame`, `ShutDown`, `TickFade`, `DrawSaveLoadOverlay`
- Module-level helpers: `snake_case` — `video::init_video()`, `text_renderer::init_ttf()`, `savePath()`
- Private static file-scope helpers: `snake_case`

**Member Variables:**
- Private members use trailing underscore: `ctx_`, `states_`, `currentType_`
- Public struct fields use `camelCase` with no underscore: `programCounter`, `displayedChars`, `bgmPath`
- SDL raw pointer members initialize to `nullptr` inline: `SDL_Window *window = nullptr;`

**Enums:**
- `enum class` always; values in `SCREAMING_SNAKE_CASE` for VM opcodes (`Op::BG`, `Op::SET_VAR_NUM`), `PascalCase` for engine states (`CerekaState::WaitingForInput`, `FadePhase::Out`)

**Namespaces:**
- Engine public types: `cereka`
- VM/compiler types: `cereka::scenario`
- Config subsystem: `config`
- Module-level groupings: `video`, `text_renderer` (module alias rather than class)
- Nested namespace shorthand: `namespace fs = std::filesystem;` repeated per translation unit

**Template Parameters:**
- CRTP pattern: `template<CerekaState T> class VNState` — value template parameter, not type

## Style Patterns

**Header Guards:**
Mixed usage — most headers use `#pragma once`; some also add `#ifndef` / `#define` / `#endif` guards (e.g., `save_data.hpp`, `cereka_state.hpp`). `#pragma once` is the canonical form; the double-guard pattern appears on older headers.

**Include Order (within a .cpp):**
1. Paired private header (`"engine_impl.hpp"`, `"config_manager.hpp"`)
2. STL headers (`<string>`, `<vector>`, `<filesystem>`, `<algorithm>`)
3. Vendor headers (`<SDL3/SDL.h>`, `<glaze/glaze.hpp>`)
- No strict blank-line separation enforced; grouping is informal

**Section Delimiters:**
Horizontal rule comments mark major logical sections inside long files:
```cpp
// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------
```
Section banners with `===` dividers appear inside headers to separate class declarations:
```cpp
// ============================================================================
// CerekaStateMachine — Manages state transitions and overlays
// ============================================================================
```

**Brace Style:**
Allman-style (opening brace on its own line) for function and method bodies. One-liner getters may stay inline:
```cpp
SDL_Texture *Background() const { return background; }
```

**Parameter Alignment:**
Multi-parameter function signatures align each parameter on its own line when the signature is long:
```cpp
bool InitGame(const char *title,
              int width,
              int height,
              bool fullscreen);
```

**Type Aliases:**
`using Impl = cereka::CerekaImpl;` declared at the bottom of `engine_impl.hpp` so all engine `.cpp` files share a short alias without a `using namespace`.

**`[[nodiscard]]`:**
Applied consistently to pure query functions that return booleans or optional results — `saveDataToJson`, `jsonToSaveData`, `type()`, `hasOverlays()`, `currentType()`.

## Comments

**Philosophy:**
Comments explain *why* — a non-obvious constraint, a workaround, a design decision. A comment that restates what the function name already says is not written. See `/no-buzzwords` skill: marketing phrases like "Enterprise-grade", "production-ready", "battle-tested" are forbidden.

**File-level:**
One-liner at top of `.cpp` describing what the file owns:
```cpp
// save.cpp — save/load game state and save/load UI overlay
```

**Class/header doc blocks:**
Short description of the abstraction, usage example, and key concepts. Written in plain prose — no `@author`, no date stamps:
```cpp
// config_manager.hpp — Data-driven Configuration Manager
//
// Key concepts:
//   PropertyDef   - Static definition of a property (name, type, description)
//   PropertyValue - Runtime value of a property
```

**Inline field comments:**
Single-line `//` after the field, on the same line for short explanations:
```cpp
std::string bgmPath;  // last filename passed to PlayBGM (for save/load)
int srcLine = 0;      // 0 means "unknown" (synthesized instructions)
```

**Section banners in implementations:**
Used to separate logical blocks inside long `.cpp` files, as horizontal rules.

**No per-instruction debug logging:**
Production code does not log on every VM dispatch (review rule). `std::cerr` is acceptable for startup-time errors (e.g., Lua load errors in `script_vm.cpp`).

## Error Handling

**Exceptions:**
`engine::error` (from `Cereka/exceptions.hpp`) thrown on fatal init failures — e.g., `throw engine::error("All renderer attempts failed")`. Not used inside the VM tick loop.

**Boolean returns:**
Fallible operations that are not fatal return `bool` — `SaveGame`, `LoadGame`, `saveDataToJson`, `jsonToSaveData`, `AudioManager::Init`.

**`sol::protected_function_result`:**
Lua compilation errors from sol2 surface as `protected_function_result`; the C++ bridge checks `.valid()` and prints to `std::cerr` with a file/error description.

**`std::error_code`:**
Used with filesystem operations to avoid exceptions in save paths:
```cpp
fs::create_directories("saves", ec);
```

**`[[nodiscard]]` enforcement:**
Callers are expected to check return values of `saveDataToJson` and `jsonToSaveData`; `[[nodiscard]]` makes ignoring them a compiler warning.

**No speculative guards:**
`nullptr` checks are only written where the pointer is genuinely optional (e.g., `if (!currentState_ || !ctx_) return;` in the state machine where state may not be set yet). Guards around things constructed in the same scope are forbidden (review rule).

## Memory Management

**SDL resources:**
All `SDL_Texture*`, `SDL_Window*`, `SDL_Renderer*` are raw pointers initialized to `nullptr` and destroyed explicitly in `ShutDown()` / `Shutdown()` / `Clear()`. Pattern:
```cpp
auto destroyTex = [](SDL_Texture *&t) {
    if (t) { SDL_DestroyTexture(t); t = nullptr; }
};
```

**Owned heap objects:**
`std::unique_ptr<IVNState>` used in `CerekaStateMachine::states_` map — clear RAII ownership.

**Non-owning observers:**
Raw pointers used for non-owning references — `IVNStateContext *ctx_`, `IVNState *currentState_`, `UiConfig *uiCfg`. These are set once at construction/init and never outlive their referent within normal engine lifetime.

**Containers:**
`std::unordered_map`, `std::vector`, `std::string` throughout — no manual heap allocation for engine data.

**Move semantics:**
`std::move` used in `DialogueSystem::Show` and setters to transfer string ownership:
```cpp
void SetSpeaker(std::string s) { speaker = std::move(s); }
```

**No `new`/`delete`:**
Not observed anywhere in the engine source. SDL creates its own objects; engine objects either live on the stack, in smart pointers, or in STL containers.

## C++ Features

**Standard:** C++23 (`set(CMAKE_CXX_STANDARD 23)` in root `CMakeLists.txt`)

**Used features:**
- `std::expected` — imported in `cereka_state.hpp` (available for state error returns)
- CRTP — `template<CerekaState T> class VNState : public IVNState`
- Structured bindings — `auto [prevType, prevState] = overlayStack_.back();`
- `std::filesystem` (`fs::create_directories`, `fs::path`)
- `std::function` for callbacks in `ApplyContext`
- `std::is_base_of_v<>` in `static_assert` inside `registerState<>()`
- `constexpr` for compile-time constants — `static constexpr float CHARS_PER_SECOND = 60.0f;`
- `[[nodiscard]]` attribute on query functions
- Range-based `for` universally; index-based `for` only when index is needed
- Lambdas for local inline helpers (resource cleanup, etc.)

**Avoided:**
- Raw `new`/`delete` — use smart pointers or STL containers
- `using namespace std` — not observed; `using namespace config;` appears only in test files with limited scope

## Cross-Platform

**Platform guards:**
`#ifdef _WIN32` used for:
- Timestamp formatting: `localtime_s` (Windows) vs `localtime_r` (POSIX) in `save.cpp`
- `wmain` entry point with `wchar_t *argv[]` on Windows vs `main` with `char *argv[]` on Linux in `tests/main.cpp` and `runner/main.cpp`

**Windows linker flags:**
Test and runner executables add `-Wl,--subsystem,console -municode` on `WIN32` targets so the console subsystem and Unicode entry point are correct.

**Filesystem paths:**
`std::filesystem` used for portable path manipulation. Save paths constructed as plain strings with `/` separator — SDL and glaze handle the rest.

**Toolchain:**
Linux native (system compiler) and Linux→Windows cross-compile via `cmake/toolchains/ucrt64.cmake` (llvm-mingw). All SDL vendor targets use namespaced CMake form: `SDL3::SDL3`, `SDL3_ttf::SDL3_ttf`, `SDL3_image::SDL3_image`, `SDL3_mixer::SDL3_mixer`.

---

*Convention analysis: 2026-05-07*
