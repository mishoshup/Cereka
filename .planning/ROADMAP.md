# Cereka — Project Roadmap

## Project Goal
A Ren'Py rival game engine where game authors write only `.crka` scripts — no C++ needed.

---

## Shipped Phases

### Phase 1 — Distribution & Packaging
**Goal:** Ship a game that "just works" on Linux/Windows, and a CerekaLauncher devs run without installing anything.
**Status:** completed

### Phase 2 — Engine Correctness
**Goal:** Fix HIGH severity bugs (save format, nested if/else, word-wrap, state machine wiring)
**Status:** completed

### Phase 3 — Architectural Cleanup
**Goal:** God-object split, renderer abstraction, safety fixes
**Status:** completed

### Phase 4 — Engine Features
**Goal:** Scene graph, text markup, audio fade, rollback + dialogue history
**Status:** completed

### Phase 5 — macOS Cross-Platform Fix
**Goal:** Fix resolution/game window distortion on macOS
**Status:** completed

### Phase 6 — Documentation Site
**Goal:** Complete mdBook documentation site for Cereka v1.0
**Status:** completed

### Phase 7 — Critical Bug Fixes
**Goal:** Var substitution in menus, checkpoint store/load, save/load variable restoration
**Status:** completed

### Phase 8 — Type Coercion
**Goal:** Fix inconsistent type coercion in if comparisons (string var vs literal int)
**Status:** completed

### Phase 9 — Headless Mode
**Goal:** Headless mode, CerekaTest, .spec.crka test runner
**Status:** completed

---

## In Progress

### Phase 10 — Launcher IDE Core
**Goal:** Script editor (syntax highlighting, tabs, split-pane, find/replace, outline, LSP), project dashboard, template gallery, asset browser
**Depends on:** Phase 9
**Plans:** 4 plans
- [x] 10-01 — Infrastructure + Project Metadata
- [x] 10-02 — CodeEditor Widget + LSP Client
- [x] 10-03 — Editor UX (tabs, split, find, outline)
- [x] 10-04 — Dashboard + Asset Browser
**Status:** executing (other terminal)

---

## Engine Bugfix Sprint (completed ad-hoc 2026-05-09)

The following were identified via deep code review and fixed in a parallel agent sprint:

- ELSE skip handler — `else` branches now execute correctly
- C++ bridge — 5 dropped ops (PLAY_BGM_FADE, STOP_BGM_FADE, BGM_CROSSFADE, SG_SET, SG_REMOVE) now map correctly
- LOAD/JUMP infinite loops — labelMap.find() instead of operator[], LoadGame return check
- Scene graph — children now accumulate parent position
- DrawRichText infinite loop — guard at line start with no fitting glyph
- CTest integration — gtest_discover_tests re-enabled
- Word-wrap — word-boundary aware wrapping (CJK/long-word fallback)
- Menu overhaul — hover states, keyboard nav, pagination, configurable spacing
- Scripting — else-if, &&, || operators with short-circuit evaluation
- Settings — persistent settings (text speed, volume, auto-advance, fullscreen)
- Pause menu — ESC overlay with Continue/Save/Load/Settings/Quit
- Save/load UX — scene metadata, confirm overwrite
- Volume wrap — fixed dead-code in settings menu cycling
- Tree-sitter sync — ops.json manifest + gen_tree_sitter_grammar.js

---

## Upcoming

### Phase 11 — Launcher IDE Pro
**Goal:** Visual editor, theme designer, debugger, plugins
**Depends on:** Phase 10
**Status:** discussed, ready to plan

---

## Backlog

### Engine polish (available for promotion)

| # | Item | Goal |
|---|------|------|
| 999.11 | ATL animation system | Dissolve, zoom, rotate, move with easing curves |
| 999.12 | Accessibility & input | Controller support, font scaling, key remapping |
| 999.13 | Dialogue UX | Configurable typewriter speed, auto-advance, skip-read |
| 999.14 | Whiteout game | Content pipeline, UI theme, packaging |

### Tech debt & infrastructure

| # | Item | Goal |
|---|------|------|
| — | Launcher refactor | Extract PackageManager, ProjectLifecycle from main.cpp |
| — | CMake hygiene | Replace GLOB_RECURSE with explicit source lists |
| — | CI/CD | GitHub Actions on every push |
| — | Versioned save format | Add version field + migration hook |
| — | Compiler error surfacing | Thread srcLine/srcCol through to runtime errors |
