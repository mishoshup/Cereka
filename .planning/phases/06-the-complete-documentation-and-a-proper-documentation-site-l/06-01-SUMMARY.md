---
phase: 06-documentation-site
plan: 01
subsystem: documentation
tags: mdbook, scripting-reference, docs, .crka, user-guide

requires: []
provides:
  - mdBook documentation site scaffolding
  - 10 scripting reference pages covering all .crka language features
  - User guides (getting started, project structure, ui theming, variables, build/package)
  - Annotated example game

affects: []

tech-stack:
  added: mdBook
  patterns: reference documentation, user guide structure

key-files:
  created:
    - docs/book.toml
    - docs/src/SUMMARY.md
    - docs/src/README.md
    - docs/src/getting-started.md
    - docs/src/project-structure.md
    - docs/src/ui-theming.md
    - docs/src/variables-and-expressions.md
    - docs/src/build-and-package.md
    - docs/src/scripting-reference/scene.md
    - docs/src/scripting-reference/dialogue.md
    - docs/src/scripting-reference/audio.md
    - docs/src/scripting-reference/flow.md
    - docs/src/scripting-reference/menu.md
    - docs/src/scripting-reference/variables.md
    - docs/src/scripting-reference/conditionals.md
    - docs/src/scripting-reference/save-load.md
    - docs/src/scripting-reference/ui-theming.md
    - docs/src/scripting-reference/text-markup.md
    - docs/src/examples/annotated-game.md
  modified: []

key-decisions:
  - "10 scripting reference pages organized under docs/src/scripting-reference/"
  - "User guides at top level (docs/src/*.md) separate from scripting reference"
  - "Annotated example game covers all core scripting features in one walkthrough"
  - "Content accuracy verified against engine source code (Op enum, compiler, markup parser, save schema)"

patterns-established:
  - "Reference pages: syntax table → detailed sections → complete examples at end"
  - "User guides: tutorial-first with progressive disclosure"
  - "Cross-linking: reference pages link to each other where features intersect"

requirements-completed: []

duration: 31 min
completed: 2026-05-08
---

# Phase 6 Plan 1: mdBook Scaffolding & Scripting Reference Pages

**mdBook documentation site with 10 scripting reference pages covering all Cereka .crka language features, 5 user guides, and an annotated example game**

## Performance

- **Duration:** 31 min
- **Started:** 2026-05-08T08:44:00Z
- **Completed:** 2026-05-08T09:15:00Z
- **Tasks:** 11 (10 scripting reference + 1 supporting pages)
- **Files modified:** 19

## Accomplishments

- Created mdBook configuration (`book.toml`) with Ayu theme, search, and print support
- Created `SUMMARY.md` with full table of contents (17 linked pages)
- All 10 scripting reference pages written with accurate, code-verified content:
  - Scene (bg, char, hide char, scene_graph)
  - Dialogue (say, narrate, {var} substitution)
  - Audio (bgm, sfx, stop_bgm, fade/crossfade variants)
  - Flow Control (label, jump, include, call with 32-depth stack)
  - Menu & Choices (menu blocks, button goto/exit)
  - Variables (set strings, $ numeric, all arithmetic operators, precedence)
  - Conditionals (if/else/endif, all 6 comparison operators, expression RHS)
  - Save & Load (10-slot JSON, save_menu/load_menu overlays)
  - UI Theming (textbox, namebox, button, font, advance_keys with all properties)
  - Text Markup (b, i, u, s, color=#rrggbb, nesting, escape sequences)
- 5 user guides created: Getting Started, Project Structure, UI Theming Guide, Variables & Expressions, Build & Package
- Annotated example game demonstrating all core features in context
- Every reference page verified against actual engine source code (Op enum, compiler Lua, markup parser, save schema, property handlers)

## Task Commits

Each task was committed atomically:

1. **Task 1: Scene reference page** - `3bf77e7` (docs)
2. **Task 2: Dialogue reference page** - `364d9d3` (docs)
3. **Task 3: Audio reference page** - `7824692` (docs)
4. **Task 4: Flow Control reference page** - `afbf6c5` (docs)
5. **Task 5: Menu & Choices reference page** - `b563d5a` (docs)
6. **Task 6: Variables reference page** - `37f753e` (docs)
7. **Task 7: Conditionals reference page** - `60f1cd9` (docs)
8. **Task 8: Save & Load reference page** - `5824b1e` (docs)
9. **Task 9: UI Theming reference page** - `ab2dbbc` (docs)
10. **Task 10: Text Markup reference page** - `eecad44` (docs)
11. **Task 11: README + User Guides + Example** - `cf84a93` (docs)

**Pre-existing scaffolding (book.toml, SUMMARY.md):** `9c96743` (docs)

## Files Created/Modified

### Scripting Reference (10 pages)
- `docs/src/scripting-reference/scene.md` - Background, character, scene graph commands
- `docs/src/scripting-reference/dialogue.md` - Say, narrate, variable substitution
- `docs/src/scripting-reference/audio.md` - BGM, SFX, fade/crossfade transitions
- `docs/src/scripting-reference/flow.md` - Labels, jumps, includes, call subroutines
- `docs/src/scripting-reference/menu.md` - Choice menus with buttons
- `docs/src/scripting-reference/variables.md` - String and numeric variables, arithmetic
- `docs/src/scripting-reference/conditionals.md` - If/else with comparisons
- `docs/src/scripting-reference/save-load.md` - Save/load system, JSON format
- `docs/src/scripting-reference/ui-theming.md` - Textbox, name box, button, font config
- `docs/src/scripting-reference/text-markup.md` - Bold, italic, color tags

### User Guides (5 pages)
- `docs/src/getting-started.md` - First project walkthrough
- `docs/src/project-structure.md` - Game project directory layout
- `docs/src/ui-theming.md` - UI customization guide
- `docs/src/variables-and-expressions.md` - Variables deep dive
- `docs/src/build-and-package.md` - Distribution instructions

### Overview & Examples (3 files)
- `docs/src/README.md` - Welcome page with quick links
- `docs/src/examples/annotated-game.md` - Complete annotated game script
- `docs/book.toml` - mdBook configuration (Ayu theme, search, print)
- `docs/src/SUMMARY.md` - Table of contents

## Decisions Made

- **Reference vs. Guide separation:** Scripting reference pages are strict API documentation (syntax, parameters, examples). User guides are tutorial-style with progressive learning. This separation matches mdBook's SUMMARY.md structure.
- **Page structure pattern:** Each scripting reference page follows: overview/syntax table → detailed per-command documentation with parameters → complete code examples.
- **Cross-linking:** Text markup page, dialogue page, and UI theming guide all cross-reference each other where features overlap (e.g., markup in dialogue, theming in menu reference).
- **Annotated example as integration test:** The example game script exercises all core features in one narrative, serving as both documentation and a functional test case for new users.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - all tasks completed without issues.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Documentation site scaffolding complete and ready for mdBook rendering
- All scripting reference pages are written — content can be reviewed and refined
- Future documentation phases could add: tutorial series, architecture docs, contribution guide, API reference for C++ extension
- An mdBook build command or CI step should be added to render the site

## Self-Check: PASSED

All 19 files verified on disk. All 12 commits verified in git log. No accidental deletions. No untracked files aside from plan metadata.

---

*Phase: 06-documentation-site*
*Completed: 2026-05-08*
