---
phase: renderer-scene-graph-review
reviewed: 2026-05-09T12:00:00Z
depth: deep
files_reviewed: 15
files_reviewed_list:
  - src/renderer/irender_context.hpp
  - src/renderer/sdl_render_context.hpp
  - src/renderer/sdl_render_context.cpp
  - src/renderer/irecture.hpp
  - src/scene_graph.hpp
  - src/scene_graph.cpp
  - src/text/rich_text_renderer.hpp
  - src/text/rich_text_renderer.cpp
  - src/text/markup_parser.hpp
  - src/text/markup_parser.cpp
  - src/ui/ui_manager.hpp
  - src/ui/ui_manager.cpp
  - tests/scene_graph_test.cpp
  - tests/markup_parser_test.cpp
  - tests/display_test.cpp
findings:
  critical: 6
  warning: 11
  info: 4
  total: 21
status: issues_found
---

# Renderer & Scene Graph: Code Review Report

**Reviewed:** 2026-05-09T12:00:00Z
**Depth:** deep (cross-file analysis + call chain tracing)
**Files Reviewed:** 15
**Status:** issues_found

## Summary

The renderer abstraction (`IRenderContext`) is well-designed conceptually — custom `Color` and `Rect` structs, pure virtual interface, no SDL types on most methods. However, the abstraction has two leaks (`TTF_Font*` and `SDL_Texture*` via `ITexture::RawTexture()`). The scene graph has **two critical correctness bugs**: (1) position does not accumulate from parent transforms, and (2) accumulated opacity is computed but never applied during rendering. The `DrawRichText` virtual method has an **infinite loop** edge case. There is **dead code** in `rich_text_renderer.hpp/.cpp` — an unimplemented stub that nothing includes. Raw texture pointers in `UiConfig` are a **use-after-free risk**. The markup parser is solid for basic cases but has gaps in color tag validation and whitespace handling.

---

## Critical Issues

### CR-01: Scene graph position does NOT accumulate from parent

**File:** `src/scene_graph.cpp:73-74`
**Issue:** `updateNode()` propagates `scaleX/scaleY`, `rotationDeg`, and `opacity` from parent to child, but **position `x`/`y` is NOT accumulated**. World `x`/`y` is set directly from `local.x`/`local.y`, ignoring the parent's world position. This means child nodes will not follow their parent when the parent moves — breaking a fundamental invariant of the scene graph pattern.

```cpp
// scene_graph.cpp:73-74 — BUG: no parent position accumulation
node.world.x = node.local.x;
node.world.y = node.local.y;
// scale/rotation/opacity DO accumulate correctly:
node.world.scaleX = parentAccum.scaleX * node.local.scaleX;
// ...
```

**Fix:** Accumulate position from parent. Since positions are normalized (0–1), the child's local position should be offset from the parent's world position:

```cpp
node.world.x = parentAccum.x + node.local.x * parentAccum.scaleX;
node.world.y = parentAccum.y + node.local.y * parentAccum.scaleY;
```

Or if positions are meant to be absolute, they must still account for parent transforms. Either way, the current code is wrong.

---

### CR-02: Scene graph opacity computed but never applied during rendering

**File:** `src/ui/ui_manager.cpp:50-66`
**Issue:** `updateTransforms()` accumulates `opacity` into `node.world.opacity`, but `DrawSceneGraph()` **never reads this field**. The rendering lambda only uses `world.x`, `world.y`, `world.scaleX`, `world.scaleY` — opacity is entirely ignored. Worse, `IRenderContext::DrawTexture` has **no alpha/opacity parameter**, so even if the scene graph wanted to apply opacity, it couldn't through the existing interface.

```cpp
void UIManager::DrawSceneGraph() {
    m_sceneGraph.updateTransforms();       // computes opacity...
    m_sceneGraph.visit([this](const SceneNode &node) {
        // ... but never uses node.world.opacity
        float sx = node.world.x * sw;
        float sy = node.world.y * sh;
        float w  = node.texture->Width() * node.world.scaleX;
        float h  = node.texture->Height() * node.world.scaleY;
        Rect dst{sx - w/2.0f, sy - h/2.0f, w, h};
        m_renderCtx->DrawTexture(*node.texture, nullptr, &dst);  // no alpha
    });
}
```

**Fix:** Add a `SetAlpha` or alpha parameter to `DrawTexture` (or add a separate alpha blend step). Then apply `node.world.opacity` when rendering each node.

---

### CR-03: Infinite loop in `DrawRichText` when no character fits `maxWidth`

**File:** `src/renderer/sdl_render_context.cpp:189-205`
**Issue:** When `TTF_MeasureString` returns `extentSz == 0` (e.g., a single glyph is wider than `maxWidth`, or `maxWidth` is very small), the code wraps to the next line but **does not advance `offset`**. On the next iteration, the same text is measured against the full `maxWidth`, `TTF_MeasureString` again returns extent=0, and the loop spins forever.

```cpp
while (offset < textLen) {
    // ...TTF_MeasureString returns extentSz=0...
    if (extent == 0) {
        currentX = x;          // "wrap" to next line
        currentY += lineHeight;
        continue;              // offset unchanged → infinite loop
    }
```

This triggers with a narrow text area and a font large enough that a single character exceeds `maxWidth`.

**Fix:** If `extent == 0` and `offset` hasn't advanced, force at least 1 character to prevent infinite looping:

```cpp
if (extent == 0) {
    if (offset == oldOffset) {  // or track "no progress this iteration"
        // Force-place at least one character to break deadlock
        extent = 1;
        measuredWidth = /* approximate glyph width */;
    } else {
        currentX = x;
        currentY += lineHeight;
        continue;
    }
}
```

---

### CR-04: `SdlRenderContext` destructor destroys renderer it doesn't own

**File:** `src/renderer/sdl_render_context.cpp:46-51`
**Issue:** The `SDL_Renderer*` is passed into the constructor from external code (e.g., `CreateTestRenderer` or the engine init), but `~SdlRenderContext()` calls `SDL_DestroyRenderer(m_renderer)`. The caller has no way to know the renderer's lifetime is now managed by `SdlRenderContext`. If the caller also destroys the renderer, this is a **double-free**. If the caller passes the same renderer to two contexts, the first destruction invalidates the second.

```cpp
SdlRenderContext::~SdlRenderContext() {
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);  // took ownership without documentation
        m_renderer = nullptr;
    }
}
```

**Fix:** Either:
1. Document that the constructor takes ownership, and remove the `SDL_Renderer*`-owning code from the caller, OR
2. Do NOT destroy the renderer in the destructor (the context is a non-owning observer), OR
3. Use `unique_ptr`/custom deleter to make ownership explicit.

---

### CR-05: Dead code — `rich_text_renderer.hpp/.cpp` is an unimplemented stub

**Files:** `src/text/rich_text_renderer.hpp`, `src/text/rich_text_renderer.cpp`
**Issue:** The free function `cereka::text::DrawRichText` in `rich_text_renderer.hpp` is **never included by any file** other than its own `.cpp`. It is dead code that returns `0.0f` and does nothing. The real implementation lives in `SdlRenderContext::DrawRichText` (`sdl_render_context.cpp:159`), which is a different function (virtual method vs free function). This is misleading — a developer seeing the header would expect a usable rich text function, but nothing uses it.

**Evidence:**
- `grep -r "rich_text_renderer.hpp"` → only matches `rich_text_renderer.cpp`
- `grep "DrawRichText"` → 6 matches, the only call site is `m_renderCtx->DrawRichText(...)` in `ui_manager.cpp:160`, which dispatches to the **virtual method** on `IRenderContext`, not the free function.

**Fix:** Remove `rich_text_renderer.hpp` and `rich_text_renderer.cpp` entirely, or implement the free function by delegating to the `IRenderContext` virtual method (consolidating rich text logic in one place).

---

### CR-06: `UiConfig` stores raw `ITexture*` — dangling pointer risk

**File:** `src/cereka_ui_config.hpp:58,73,83,85`
**Issue:** `UiConfig::Textbox::image`, `Namebox::image`, `Button::image`, and `Button::hoverImage` are raw `ITexture*` pointers with **no ownership semantics**. When `SceneManager::Clear()` is called (e.g., during game reset, load, or scene transition), the `std::shared_ptr<ITexture>` objects holding the actual textures may be destroyed. Any subsequent `UIManager` draw call that dereferences these raw pointers will access **freed memory**.

```cpp
struct Textbox {
    cereka::ITexture *image = nullptr;  // raw — who owns this?
    // ...
};
```

The `imagePath` string is also stored redundantly alongside the raw pointer, suggesting these should have been `shared_ptr<ITexture>` to share ownership with the texture cache.

**Fix:** Use `std::shared_ptr<ITexture>` for all UI image references, or ensure the texture cache outlives the `UiConfig`. At minimum, add clear lifetime documentation and null checks at every draw site.

---

## Warnings

### WR-01: `TTF_Font*` leaks through `IRenderContext` abstraction boundary

**File:** `src/renderer/irender_context.hpp:64-72`
**Issue:** Three methods on the pure virtual interface accept `TTF_Font*`, which is an SDL_ttf type:

```cpp
virtual std::unique_ptr<ITexture> CreateTextTexture(
    TTF_Font *font, const std::string &text, Color color) = 0;
virtual std::unique_ptr<ITexture> CreateTextTextureWrapped(
    TTF_Font *font, const std::string &text, Color color, int wrapWidth) = 0;
virtual float DrawRichText(TTF_Font *baseFont, ...) = 0;
```

This means any backend implementation (`IRenderContext` subclass) must link against SDL_ttf, defeating the abstraction. A forward declaration of `TTF_Font` is used, which mitigates the header dependency but doesn't eliminate the ABI coupling.

**Fix:** Introduce a `FontHandle` wrapper (opaque pointer or index into a font registry) that the render context implementation can map to its native font type.

---

### WR-02: `ITexture::RawTexture()` still leaks `SDL_Texture*` across the boundary

**File:** `src/renderer/irecture.hpp:17`
**Issue:** The deprecated `RawTexture()` method returns `SDL_Texture*`, which is an SDL3 type. Any consumer of `ITexture` can use this escape hatch, creating implicit SDL dependencies in engine code. Currently used in `sdl_render_context.cpp:100` within the backend itself (which is acceptable), but there's nothing preventing engine-layer code from calling it.

```cpp
[[deprecated("Use ITexture Width/Height + IRenderContext DrawTexture instead")]]
virtual SDL_Texture *RawTexture() const = 0;
```

The deprecation warning references "Plan 03-03" — a hardcoded plan reference that will be stale when the roadmap changes.

**Fix:** Remove `RawTexture()` from the interface. The `SdlRenderContext` backend can downcast internally (`static_cast<SdlTexture&>(tex)` already exists on line 84). No external code needs raw SDL texture access.

---

### WR-03: Typo in filename: `irecture.hpp` (missing 'x')

**File:** `src/renderer/irecture.hpp` (the filename itself)
**Issue:** File should be named `itexture.hpp`. The missing 'x' (`irecture` vs `itexture`) is a typo that causes confusion and reduces codebase professionalism. All includes reference this misspelled filename.

**Fix:** Rename file to `itexture.hpp` and update all `#include` references.

---

### WR-04: `parseFloatSafe` silently swallows parse errors

**File:** `src/scene_graph.cpp:102-108`
**Issue:** `parseFloatSafe` returns `0.5f` on ANY failure — empty string and invalid float alike. A malformed transform like `scale(abc)` would silently set the scale to `0.5` instead of `1.0` (the default). The developer gets no warning that input was invalid.

```cpp
static float parseFloatSafe(const std::string &s) {
    if (s.empty()) return 0.5f;
    auto r = safe_stof(s);
    return r.value_or(0.5f);  // parse failure → 0.5f, not an error
}
```

Furthermore, the fallback value of `0.5` is semantically meaningful (it's the default for `x`/`y`) but nonsensical for `scale`, `rotate`, and `opacity` — losing the distinction between "no value" and "parse error."

**Fix:** Use `std::optional<float>` and have the caller apply defaults. Or log parse errors to `std::cerr`. Do not silently substitute `0.5f`.

---

### WR-05: `SceneNode::world` is public — no encapsulation

**File:** `src/scene_graph.hpp:19-20`
**Issue:** `world` is a public member of `SceneNode`. External code can write to it, which will be **overwritten** on the next `updateTransforms()` call. Conversely, if someone reads `world` without calling `updateTransforms()` first, they get stale/identity values.

```cpp
struct SceneNode {
    // ...
    Transform world;  // public — writable, no synchronization with updateTransforms
    // ...
};
```

**Fix:** Make `world` private and provide `const` accessors. Force consumers to go through `updateTransforms()` + `visit()` to guarantee transform freshness.

---

### WR-06: Transform position semantics are non-standard for a scene graph

**File:** `src/scene_graph.cpp:73-74`
**Issue:** Positions (`x`, `y`) are expressed in normalized screen coordinates (0–1), and node positions are **absolute** — they do not accumulate from parent position (see CR-01). This means the scene graph is effectively flat: child positions do not compose with parent positions. This limits the scene graph to a "grouping + individual transform" model rather than a true hierarchical transform tree.

Additionally, the `scene_graph_test.cpp` has **no test** for parent-child position accumulation (see WR-10), so this non-standard behavior is undocumented and untested.

**Fix:** Either implement full position accumulation (CR-01) and document the coordinate system, or rename the data structure to reflect it's a flat node list with inherited scale/rotation/opacity.

---

### WR-07: `DrawRichText` creates + destroys SDL_Textures every frame

**File:** `src/renderer/sdl_render_context.cpp:227-240`
**Issue:** Each call to `DrawRichText` renders every text segment substring to an `SDL_Surface`, creates an `SDL_Texture` from it, draws, then destroys the texture. This means every frame with rich text triggers **multiple GPU texture uploads** — a surface → texture pipeline that involves pixel data transfer.

```cpp
for (const auto &ps : placed) {
    // ...
    SDL_Surface *surf = TTF_RenderText_Blended(font, subText.c_str(), ...);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    // draw...
    SDL_DestroyTexture(tex);  // fresh texture every frame
}
```

The same pattern repeats in `DrawMenuButtons`, `DrawSaveLoadOverlay`, and `DrawHistoryOverlay`.

**Fix:** Cache text textures by their content hash. Invalidate only when font or theme changes. Even a single-frame cache (render once, reuse during the same frame's draw sequence) would be a major improvement.

---

### WR-08: Dangling `SDL_Renderer*` context across state machine transitions

**File:** `src/ui/ui_manager.hpp:55`, `src/ui/ui_manager.cpp:24`
**Issue:** `UIManager::Init()` stores a raw pointer to `IRenderContext`:

```cpp
void UIManager::Init(IRenderContext &renderCtx) {
    m_renderCtx = &renderCtx;  // raw pointer
}
```

If the render context is destroyed (e.g., window recreation, resolution change) and a new one is created, `m_renderCtx` becomes a dangling pointer. There is no `Release()` or re-init mechanism.

**Fix:** Use `std::shared_ptr<IRenderContext>` or add a proper re-initialization/teardown protocol.

---

### WR-09: Markup parser doesn't handle whitespace around `=` in color tags

**File:** `src/text/markup_parser.cpp:75-78`
**Issue:** The color tag parser uses:

```cpp
if (tagName.rfind("color=", 0) == 0) {
    colorValue = tagName.substr(6);
    tagName = "color";
}
```

This requires the exact format `color=#rrggbb`. If a user writes `<color = #ff0000>` (with spaces around `=`), the `tagName` becomes `"color = #ff0000"`, which doesn't match `rfind("color=", 0)`. The tag is treated as unknown and rendered as literal text.

**Fix:** Allow optional whitespace: parse the tag content for `color\s*=\s*(#[0-9a-fA-F]+)`.

---

### WR-10: No test coverage for parent-child transform inheritance

**File:** `tests/scene_graph_test.cpp`
**Issue:** All transform-related tests check only root-level nodes (`WorldEqualsLocalForRootNode` tests a root child of the scene graph). There are **no tests** for:
- Child inheriting parent scale
- Child inheriting parent opacity
- Nested transforms at ≥2 levels depth
- Position relationship between parent and child
- Parent invisible → child subtree not visited
- Transform after `Clear()` + re-create

The `SetTransformParsesCorrectly` test only checks local values, not accumulated world values.

**Fix:** Add tests for multi-level transform accumulation, especially verifying the buggy position behavior documented in CR-01.

---

### WR-11: `DrawRichText` returning 0.0f on empty placed may suppress height reporting

**File:** `src/renderer/sdl_render_context.cpp:208-209`
**Issue:** If all segments produce no placed segments (e.g., only whitespace, or extent=0 infinite loop avoided), the function returns `0.0f`. Callers interpret this as "no height consumed," which may be incorrect — even empty text takes vertical space. The return value is meant to report the total rendered height, but `0.0f` is indistinguishable from "no text."

**Fix:** Check if the input had any segments but produced no placed text. In that case, return at least `lineHeight` to represent the occupied vertical space.

---

## Info

### IN-01: `CreateTestRenderer` uses `EXPECT_NE` instead of `ASSERT_NE`

**File:** `tests/display_test.cpp:40`
**Issue:** If `SDL_CreateWindow` fails, the test emits a non-fatal `EXPECT_NE` failure but continues to call `SDL_CreateRenderer(tr.window, ...)` with a null window. The `if (tr.window)` guard prevents a null dereference, but this is a fragile pattern — if someone removes the guard, the test will crash instead of failing gracefully.

### IN-02: Hardcoded string lengths in font rendering tests

**Files:** `tests/display_test.cpp:199,277,323`
**Issues:**
- Line 199: `TTF_RenderText_Blended(font, "Hello macOS", 11, white)`
- Line 277: `TTF_RenderText_Blended(font, "Dialogue text", 13, sdlColor)`

If the test string is modified without also updating the hardcoded length, the TTF function will use the wrong length. Prefer `strlen(text)` or `std::char_traits<char>::length(text)`.

### IN-03: `parseHexColor` silently accepts invalid hex values

**File:** `src/text/markup_parser.cpp:10-23`
**Issue:** The function checks `hex.size() >= 7` and `hex[0] == '#'`, but doesn't validate that characters at positions 1–6 are valid hex digits. `<color=#xyzxyz>` returns white (255, 255, 255) because `strtol` parsing fails at the first non-hex char and `end` comparison catches it... actually no — `strtol` processes as many valid hex digits as it can. For `#xyzxyz`, each pair would be: `"xy"` → `strtol("xy", &end, 16)` — `x` IS a valid hex digit (0–9, a–f), but `y` is not (it's after `x` in ASCII but not a-f). Actually `y` is not a hex digit. So `strtol("xy", &end, 16)` would return `0` and `end` would equal `"x"` (not `"xy"`), meaning `end == pair.c_str() + 2` is false, and we return 0. So it does fail for `#xyzxyz` but returns the channel value 0 instead of erroring. The result is black (r=0, g=0, b=0) which silently looks wrong.

For `<color=#ff00zz>`, `"ff"` and `"00"` parse fine, `"zz"` → `strtol("zz", ...)` → end equals start → return 0 → b=0. Result: yellow (ff, 00, 00) === red. Confusing.

### IN-04: `DrawHistoryOverlay` 80-char truncation can split UTF-8 sequences

**File:** `src/ui/ui_manager.cpp:261-262`
**Issue:**
```cpp
if (display.length() > 80)
    display = display.substr(0, 77) + "...";
```
`std::string::length()` counts bytes, not code points. If the 77th byte is in the middle of a multibyte UTF-8 sequence, the truncation produces an invalid UTF-8 string at the boundary. Use a code-point-aware truncation instead.

---

## Cross-File Summary

| Pattern | Files | Risk |
|---|---|---|
| Raw `TTF_Font*` in abstraction boundary | `irender_context.hpp`, `sdl_render_context.hpp/.cpp` | Architectural coupling |
| Raw `ITexture*` without ownership | `cereka_ui_config.hpp`, `ui_manager.cpp` | Use-after-free crash |
| Texture creation every frame (no cache) | `sdl_render_context.cpp`, `ui_manager.cpp` | Performance (v1 OOS) |
| Dead/incomplete rich text renderer | `text/rich_text_renderer.hpp/.cpp` | Developer confusion |
| Scene graph position non-accumulation | `scene_graph.hpp/.cpp`, `ui_manager.cpp` | Incorrect child positioning |
| Missing test coverage for transforms | `tests/scene_graph_test.cpp` | Undocumented behavior |

---

_Reviewed: 2026-05-09T12:00:00Z_
_Reviewer: gsd-code-reviewer (deep mode)_
_Depth: deep_
