---
phase: 05
plan: 01
name: macOS Display Fix
type: execute
wave: 1
completed: 2026-05-08
subsystem: display
tags: [macos, display, font, resolution, metal, sdl3]
---

## What Was Done

### Logical Presentation Fix
- Added `SDL_SetRenderLogicalPresentation()` call in `Cereka.cpp:InitGame` after renderer creation
- Uses `SDL_LOGICAL_PRESENTATION_LETTERBOX` — SDL3's built-in cross-platform scaling API
- Same approach as Unity/Unreal for handling HiDPI/Retina displays
- Engine now correctly maps logical coordinates to physical pixels on macOS Retina

### Font Rendering Investigation
- Root cause identified: missing `assets/fonts/` directory (no font file → null font → no text rendered)
- Rendering pipeline verified correct on macOS Metal via headless tests
- Added `assets/fonts/Montserrat-Medium.ttf` from launcher templates

### Tests Added (display_test.cpp)
- 3 logical presentation tests: set/get roundtrip, multi-resolution scaling, coordinate conversion
- 4 font rendering tests: init/open, surface→texture, with logical presentation, rich-text end-to-end
- All 66 tests pass

## Key Decisions
- SDL3's built-in logical presentation API over manual scaling
- Metal renderer kept (correct for macOS)
- Font issue = setup/configuration, not rendering pipeline bug
