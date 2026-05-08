# Discussion Log — Phase 5

**Date:** 2026-05-08
**Participants:** User + the agent

## Areas Discussed

### Root Cause
- macOS HiDPI/Retina pixel density mismatch — window uses logical points, rendering mixes physical and logical coordinate spaces

### Fix Strategy
- **Chosen:** `SDL_SetRenderLogicalPresentation()` with `SDL_LOGICAL_PRESENTATION_LETTERBOX`
- Rationale: SDL3's built-in cross-platform scaling API, same approach as Unity/Unreal
- Rejected alternatives: manual scaling math, renderer backend switching

### Renderer Backend
- Metal is correct on macOS — OpenGL deprecated
- No backend switching needed

### Testing
- Headless unit test via `SDL_WINDOW_HIDDEN` verifying logical→physical output mapping
- Manual visual sign-off on macOS

## Decisions Summary
See `05-CONTEXT.md` for full decision record.
