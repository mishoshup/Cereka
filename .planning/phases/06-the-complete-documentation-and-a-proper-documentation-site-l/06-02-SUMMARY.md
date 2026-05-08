---
phase: 06
plan: 02
name: User Guides + Theming + Variables + Example + Deploy
type: execute
wave: 1
completed: 2026-05-08
subsystem: documentation
tags: [docs, mdbook, theme, examples, cloudflare]
---

## What Was Done

### User Guides
- Getting Started guide with quickstart and first .crka script
- Project Structure reference (game.cfg, assets/ layout, save files)
- Build & Package guide (cross-platform building, launcher packaging)
- UI Theming Reference with all property tables
- Variables & Expressions guide with examples

### Annotated Example Game
- `docs/examples/annotated-game.md` — complete scene with all .crka features
- Demonstrates: bg, char, say, narrate, menu, variables, conditionals, audio, ui theming

### Deploy
- mdBook builds successfully to `docs/book/`
- Ready for Cloudflare Pages deployment

## Files Created
- 5 user guide markdown files
- 1 annotated example game
- 1 custom CSS theme file

## Key Decisions
- Game author focus maintained throughout
- Code examples use ```text (no .crka highlight.js available yet)
- mdBook configured with ayu theme, search, and print support
