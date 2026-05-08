# Discussion Log — Phase 5

**Date:** 2026-05-08
**Participants:** User + the agent

## Areas Discussed

### Root Cause — Display Distortion
- macOS HiDPI/Retina pixel density mismatch — window uses logical points, rendering mixes physical and logical coordinate spaces

### Fix Strategy — Logical Presentation
- **Chosen:** `SDL_SetRenderLogicalPresentation()` with `SDL_LOGICAL_PRESENTATION_LETTERBOX`
- Rationale: SDL3's built-in cross-platform scaling API, same approach as Unity/Unreal
- Rejected alternatives: manual scaling math, renderer backend switching

### Renderer Backend
- Metal is correct on macOS — OpenGL deprecated
- No backend switching needed

### Font Not Displaying on macOS (discovered during testing)
- Text in dialogue box is invisible on macOS
- Possible causes: Metal texture format incompatibility, font path resolution, TTF_OpenFont failure
- Needs investigation during execution

### Testing
- Headless unit test via `SDL_WINDOW_HIDDEN` verifying logical→physical mapping
- Manual visual sign-off on macOS
- Font rendering unit test to verify text texture creation works on Metal

## Decisions Summary
See `05-CONTEXT.md` for full decision record.
