# Phase 9: Headless Mode - Context (updated)

**Gathered:** 2026-05-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Add declarative test runner for CerekaGame using `.spec.crka` files. Game authors write tests that wait for dialogue, click buttons by label, and assert results — all without coordinates. Runs in headless mode for CI.

</domain>

<decisions>
## Implementation Decisions

### D-01: CLI — `--script <file.spec.crka>`
- Runs the spec file in headless mode (implies `--headless`)
- `CerekaGame --script spike.spec.crka`
- Spec file is a separate concept from the game script (entry in game.cfg)

### D-02: Test Controller — `CerekaTest`
- Separate class from `CerekaEngine` (Interface Segregation Principle)
- Wraps `CerekaImpl` directly — shares the same engine instance
- Methods:
  - `Run(specFile)` → int exit code (0 pass, 1 fail)
  - `ButtonLabels()` → vector of current button display texts
  - `SelectMenuOption(int idx)` → triggers button by index, no coordinates
- Lives in `src/` or `include/Cereka/` alongside the public API
- Named `CerekaTest` — short, noun, like `cargo test`

### D-03: `.spec.crka` format — Line-oriented commands
```
; comment
wait "text"                ; wait until dialogue contains text (default 5s timeout)
wait 30 "text"             ; wait with custom timeout (seconds)
click button:"label"       ; click button by exact case-sensitive label
click button:2             ; click button by 1-based index
assert "text"              ; fail (exit 1) if text contains this string
```
- File extension `.spec.crka` keeps it in the .crka family
- Semi-colon comments like .crka

### D-04: Selectors — Both label and index
- `button:"exact label"` — case-sensitive exact match against button display text
- `button:1` — 1-based index (first button is 1)
- Clear error if no match found

### D-05: Error handling — Rust-style diagnostics
- Default 5s timeout per `wait`, override with `wait N "text"`
- On timeout: print `error: timeout waiting for "text" (5s)` → exit 1
- On failed assert: print `error: assert failed: found "FAIL" at line 3` → exit 1
- On no matching button: print available buttons → exit 1
- Summary at end: `Finished: X commands, Y failures`
- Exit code 0 = all pass, non-zero = failure

### D-06: Engine API additions
- `CerekaEngine::SelectMenuOption(int)` — delegate to Impl, finds button target by index, sets PC, closes menu, returns to Running
- `CerekaEngine::CurrentState()` — expose current CerekaState as string (for test controller to detect InMenu/WaitingForInput)

### D-07: Headless flag refines
- `--headless` still exists for simple no-window runs
- `--script` implies headless with spec-file execution
- `--headless --entry test.crka` runs a single .crka to completion (current behavior)
- `--script spike.spec.crka` runs spec-driven test

### the agent's Discretion
- Exact parsing of `.spec.crka` commands
- CerekaTest header location (`include/Cereka/` vs `src/`)
- `SelectMenuOption` implementation details
- In-menu timeout handling

</decisions>

<canonical_refs>
### Previous Phase Decisions
- `.planning/phases/09-headless-mode/09-CONTEXT.md` — original headless decisions (--headless flag, instant execution, output format, FAIL detection)

### Runner
- `runner/main.cpp` — game loop, flag parsing
- `include/Cereka/Cereka.hpp` — CerekaEngine public API

### State Machine
- `src/state/cereka_state.hpp` — CerekaStateMachine, state transitions
- `src/state/cereka_states.cpp` — MenuState::handleEvent (button dispatch logic to replicate in SelectMenuOption)

### Menu System
- `src/cereka_menu_system.hpp` — MenuSystem (texts, targets, exits, EndPC)

### Codebase Context
- `.planning/codebase/ARCHITECTURE.md`
</canonical_refs>

<code_context>
### Reusable Assets
- `MenuSystem::Texts()` — returns vector of button text labels (for ButtonLabels)
- `MenuSystem::Target(idx)` — returns label target (for SelectMenuOption)
- `MenuSystem::IsExit(idx)` — checks if button exits menu
- `MenuSystem::EndPC()` — menu end position

### Established Patterns
- **pImpl** — CerekaEngine → CerekaImpl. CerekaTest wraps the same Impl.
- **Line-oriented parsing** — game.cfg parser in main.cpp is minimal, spec parser follows same pattern

### Integration Points
- `runner/main.cpp` — add `--script` flag parsing
- `include/Cereka/Cereka.hpp` — add `SelectMenuOption(int)`, `CurrentState()`, `ButtonLabels()`
- New file: `include/Cereka/CerekaTest.hpp` or just add to Cereka.hpp
</code_context>

<specifics>
- Rust philosophy: explicit over implicit, clear error messages, no magic
- "Like Rust errors but prettier" — format is unique, not a copy
- `CerekaTest` like `cargo test` — short, obvious
</specifics>

<deferred>
None.
</deferred>

---

*Phase: 9-Headless mode (updated)*
*Context gathered: 2026-05-08*
