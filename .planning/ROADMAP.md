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

**Status:** planned

---

## Phase 2 — Engine Correctness (HIGH severity bugs)
**Goal:** Fix all HIGH severity correctness bugs before building more features.
- Save format unification (`.sav` vs `.json` split)
- Nested if/else VM bug (`skipDepth` reset at depth > 1)
- Text word-wrap (long lines compress instead of wrapping)
- Wire `CerekaStateMachine` into `CerekaImpl` (dead state machine)

**Status:** backlog

---

## Phase 3 — Architectural Cleanup
**Goal:** Complete the CerekaImpl god-object split and add renderer abstraction.
- Wire `CerekaStateMachine` overlay push/pop as the single source of truth
- Extract UIManager from CerekaImpl
- Renderer abstraction (stop leaking SDL types into engine logic)
- Fix crash/safety risks (unguarded stoi, UB enum cast, unbounded CALL stack)

**Status:** backlog

---

## Phase 4 — Engine Features
**Goal:** Feature parity with Ren'Py baseline.
- Scene graph + transform tree (prerequisite for ATL: dissolve/zoom/rotate)
- Text markup (`{b}`, color spans)
- Audio fade in/out
- Rollback + dialogue history

**Status:** backlog
