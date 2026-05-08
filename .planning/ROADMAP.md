# Cereka — Project Roadmap

## Project Goal
A Ren'Py rival game engine where game authors write only `.crka` scripts — no C++ needed.
Game devs package their game via CerekaLauncher into a self-contained ZIP for Linux and Windows.
Players download the ZIP, unzip, run the binary. Zero external dependencies.

---

## Phase 1 — Distribution & Packaging
**Goal:** Ship a game built with Cereka that "just works" on a fresh Linux or Windows machine, and a CerekaLauncher that devs can run without installing anything.

**Scope:**
- Windows CerekaGame: static-link SDL3* → single self-contained `.exe`
- Linux CerekaGame: `$ORIGIN` RPATH → binary finds bundled `.so` files in same dir, no wrapper script
- Launcher packager (`project_manager.cpp`): produce a correct ZIP per platform (exe only on Windows, binary + SDL .so on Linux)
- Windows Launcher: ensure `windeployqt` captures UCRT/MinGW runtime DLLs in addition to Qt6 DLLs
- Linux Launcher: AppImage so devs can run CerekaLauncher on any distro without Qt6 installed

**Status:** completed

**Plans:** 4 plans

Plans:
- [x] 01-01-PLAN.md — SDL3 static gate (Windows) + $ORIGIN RPATH + SDL .so routing (Linux)
- [x] 01-02-PLAN.md — windeployqt --compiler-runtime flag + WIN32 guard for launcher
- [x] 01-03-PLAN.md — AppImage packaging script + .desktop file + icon
- [x] 01-04-PLAN.md — doPackage() Linux branch: copy SDL .so files into game archive

---

## Phase 2 — Engine Correctness (HIGH severity bugs)
**Goal:** Fix all HIGH severity correctness bugs before building more features.
- Save format unification (`.sav` vs `.json` split)
- Nested if/else VM bug (`skipDepth` reset at depth > 1)
- Text word-wrap (long lines compress instead of wrapping)
- Wire `CerekaStateMachine` into `CerekaImpl` (dead state machine)

**Status:** in-progress

**Plans:** 5 plans

Plans:
- [x] 02-01-PLAN.md — CI Infrastructure (Linux/macOS/Windows) + Local verification scripts
- [x] 02-02-PLAN.md — Fix nested if/else VM bug + Implement themeable word-wrap
- [x] 02-03-PLAN.md — State Machine "Big Bang" migration (CerekaImpl decoupling)
- [x] 02-04-PLAN.md — Modern Save System (Glaze JSON + versioning)
- [x] 02-05-PLAN.md — Branding & Naming Conventions (SDL-style cereka_ prefix, type/namespace renames, docs rebrand)

---

## Phase 3 — Architectural Cleanup
**Goal:** Complete the CerekaImpl god-object split and add renderer abstraction.
- Wire `CerekaStateMachine` overlay push/pop as the single source of truth
- Extract UIManager from CerekaImpl
- Renderer abstraction (stop leaking SDL types into engine logic)
- Fix crash/safety risks (unguarded stoi, unbounded CALL stack, and adjacent)

**Status:** completed

**Plans:** 4 plans

Plans:
- [x] 03-01-PLAN.md — CI Fix + Crash Safety (D-01, D-05)
- [x] 03-02-PLAN.md — IRenderContext Abstraction (D-04)
- [x] 03-03-PLAN.md — UIManager Extraction (D-03)
- [x] 03-04-PLAN.md — State Machine Unification (D-02)

### Phase 5: to make it work cross platform across mac windows and linux, bcs windows and linux is good now. just macos since the resolution is weird. and unconvencitonal. somehow even if window resolution ok, the game inside the window is distorted

**Goal:** [To be planned]
**Requirements**: TBD
**Depends on:** Phase 4
**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd-plan-phase 5 to break down)

### Phase 6: the complete documentation and a proper documentation site. like how all engine have documentation

**Goal:** Create a complete game-author documentation site for Cereka v1.0 using mdBook, deployed on Cloudflare Pages with a custom domain. Author docs cover the full .crka scripting language, UI theming, project structure, build/packaging, and an annotated example game.

**Requirements**: D-03 (7 sections), D-04 (annotated example game)
**Depends on:** Phase 5
**Plans:** 2 plans

Plans:
- [ ] 06-01-PLAN.md — mdBook scaffolding + complete Scripting Reference (38 ops across 10 category pages)
- [ ] 06-02-PLAN.md — User guide sections (Getting Started, Project Structure, Build & Package) + UI Theming + Variables & Expressions + Annotated Example Game + Deploy config

### Phase 7: Fix critical bugs: var substitution in menus + save/load variable restoration

**Goal:** [To be planned]
**Requirements**: TBD
**Depends on:** Phase 6
**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd-plan-phase 7 to break down)

### Phase 8: Fix inconsistent type coercion: string var vs literal int in comparison

**Goal:** [To be planned]
**Requirements**: TBD
**Depends on:** Phase 7
**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd-plan-phase 8 to break down)

### Phase 9: headless mode

**Goal:** [To be planned]
**Requirements**: TBD
**Depends on:** Phase 8
**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd-plan-phase 9 to break down)

---

## Phase 6 — Documentation Site

**Goal:** Create a comprehensive documentation site for the Cereka game engine.
**Depends on:** None
**Plans:** 2/1 plans complete

Plans:
- [x] 06-01-PLAN.md — mdBook scaffolding + all scripting reference pages + user guides + annotated example

## Phase 4 — Engine Features
**Goal:** Feature parity with Ren'Py baseline.
- Scene graph + transform tree (prerequisite for ATL: dissolve/zoom/rotate)
- Text markup (`<b>`, color spans)
- Audio fade in/out
- Rollback + dialogue history

**Status:** in-progress

**Plans:** 4 plans

Plans:
- [x] 04-01-PLAN.md — Scene Graph (SceneNode tree, compiler ops, UIManager draw)
- [x] 04-02-PLAN.md — Text Markup (tag parser, rich text renderer, IRenderContext integration)
- [x] 04-03-PLAN.md — Audio Fade (timer-based volume ramping, fade curve math, crossfade)
- [x] 04-04-PLAN.md — Rollback + Dialogue History (ring buffer, HistoryState overlay)
