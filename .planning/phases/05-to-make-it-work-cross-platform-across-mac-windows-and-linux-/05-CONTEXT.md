# Phase 5: macOS Display Fix - Context

**Gathered:** 2026-05-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix macOS display issues. Windows and Linux work correctly — macOS has:
1. Inconsistent window sizing — game renders distorted inside the window even when the window frame appears correct
2. Text not displayed in dialogue text box on macOS

Root cause (distortion): HiDPI/Retina pixel density mismatch. macOS uses physical pixels (@2x typically) while SDL3 rendering operations use logical points. The rendering pipeline mixes coordinate spaces across operations.

Root cause (font): Unknown — likely one of:
- SDL3_ttf + Metal renderer texture format incompatibility on macOS
- Font file path resolution failure on macOS bundle layout
- TTF_RenderText_Blended surface-to-texture conversion produces wrong format under Metal

</domain>

<decisions>
## Implementation Decisions

### D-01: Fix Strategy — SDL3 Logical Presentation
- Use `SDL_SetRenderLogicalPresentation()` with `SDL_LOGICAL_PRESENTATION_LETTERBOX` — SDL3's built-in cross-platform scaling API
- Engine sets a fixed virtual resolution from `game.cfg` (width/height)
- SDL3 handles the physical → logical mapping automatically, including HiDPI/Retina
- All .crka scripts and engine logic consistently use logical coordinates
- Letterbox fills remaining space with black bars (standard VN behavior)
- Same approach Unity/Unreal use for cross-platform resolution handling

### D-02: Fix Location — `video.cpp` renderer initialization
- `video::create_window()` currently ignores `game.cfg` width/height — fix to read and apply them
- After `SDL_CreateRenderer()`, immediately call:
  ```
  SDL_SetRenderLogicalPresentation(renderer, gameWidth, gameHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX)
  ```
- Estimated scope: ~20 lines of C++ including test

### D-03: Renderer Backend — Keep Metal
- SDL3 on macOS uses Metal by default — OpenGL is deprecated on macOS
- The issue is not the backend choice, it's that logical presentation isn't configured
- No backend switching needed

### D-04: Font Investigation — macOS Text Rendering
- Font not displayed in dialogue text box on macOS but works on Linux/Windows
- Suspect 1: `TTF_RenderText_Blended` → `SDL_CreateTextureFromSurface` — the surface pixel format may not be compatible with Metal renderer (needs SDL_PIXELFORMAT_ARGB8888 with premultiplied alpha)
- Suspect 2: Font file not found — directory_iterator on "assets/fonts" may resolve differently on macOS bundle
- Suspect 3: `TTF_OpenFont` returns null silently and the empty-font guard in CreateTextTexture returns nullptr
- Fix: Add macOS-specific format conversion after surface creation, or use `TTF_RenderText_Blended` with explicit format

### D-05: Testing — Headless renderer unit test
- SDL3 supports `SDL_WINDOW_HIDDEN` — create offscreen window + renderer
- Set logical presentation at various sizes
- Verify `SDL_GetRenderLogicalPresentationRect()` returns expected rect with correct aspect ratio
- Verify coordinate conversion via `SDL_RenderCoordinatesToWindow/FromWindow`
- Catch sizing regressions without a display
- Manual visual verification on macOS for final sign-off

### the agent's Discretion
- Exact letterbox color (black is standard for VNs)
- Font fix approach if multiple options exist

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Scope
- `.planning/ROADMAP.md` §Phase 5 — scope anchor and goals

### Engine Display
- `src/video.hpp` — window/renderer creation, width/height globals
- `src/video.cpp` — `create_window()` implementation (fix target)
- `src/cereka_engine_impl.hpp` — game.cfg reading for width/height
- `src/cereka_config.cpp` — ConfigManager setup

### SDL3 Documentation
- SDL3 docs: `SDL_SetRenderLogicalPresentation`, `SDL_LOGICAL_PRESENTATION_LETTERBOX`
- SDL3 docs: `SDL_GetWindowSizeInPixels`, `SDL_GetRenderLogicalPresentationOutputSize`

### Codebase Context
- `.planning/codebase/CONCERNS.md` — "Window resolution ignored" (MED severity)
- `.planning/codebase/ARCHITECTURE.md` §Video — architecture of display system

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **video.cpp** — `create_window()` already creates SDL3 window + renderer; extend with logical presentation
- **cereka_config.cpp** — reads `width`/`height` from `game.cfg` into ConfigManager

### Established Patterns
- **ConfigManager** — Property Map pattern: game.cfg values accessed via typed getters
- **`video::width` / `video::height`** — existing global ints for current resolution (currently unused/wrong)

### Integration Points
- `video::create_window()` → reads game.cfg width/height → `SDL_SetRenderLogicalPresentation()`
- All rendering code that uses `video::width`/`video::height` must use logical coordinates
- Draw code in `cereka_draw.cpp` already uses `video::width`/`video::height` — if on logical coords, no further changes needed

</code_context>

<specifics>
## Specific Ideas

- Enterprise-grade: use SDL3's native logical presentation API instead of manual scaling math
- Same cross-platform approach used by Unity/Unreal for resolution handling
- 3-line API call in the right place produces the fix

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 5-macOS Display Fix*
*Context gathered: 2026-05-08*
