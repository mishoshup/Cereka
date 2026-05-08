# Phase 7: Fix critical bugs — var substitution in menus + save/load variable restoration - Context

**Gathered:** 2026-05-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix two engine correctness bugs that break core gameplay mechanics:
1. `{var}` substitution doesn't work in menu `button` labels (Bug #1)
2. Save/load doesn't restore variables correctly (Bug #2)

Phase scope: fix Bug #1 first, test it, then address Bug #2.

</domain>

<decisions>
## Implementation Decisions

### D-01: Fix Order — Bug #1 First
- Fix `{var}` substitution in menu button labels first
- Bug #2 (save/load) may already be stale (filed against old `.sav` format before Glaze JSON migration)
- Fix Bug #1, test it, then investigate/address Bug #2 separately

### D-02: Test Strategy — Compile Snapshot + Manual
- Add compile snapshot test for menu with `{var}` in button labels (existing `tests/compile/` pattern)
- The snapshot locks in compiler output; the actual runtime bug is in C++ `EnterMenu()` not calling `SubstituteVariables()`
- Manual in-game verification via `./CerekaGame` with a test `.crka` script

### D-03: No Headless Mode
- Issue #4 (headless mode) is out of scope — would be its own phase
- ~100 lines, new execution path, not needed for verifying these fixes

### the agent's Discretion
- Exact compile snapshot test filename and expected output format
- Whether to add a C++ unit test alongside the snapshot test

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Bug #1 — Var substitution in button labels
- `src/Cereka.cpp` §`Impl::EnterMenu()` — fix location: `texts.push_back(ins.a)` needs variable substitution
- `src/Cereka.cpp` §`Impl::SubstituteVariables()` — existing substitution function, reuse
- `src/cereka_menu_system.hpp` — MenuSystem class, stores button texts
- `src/state/cereka_states.cpp` §`MenuState::draw()` — where button texts are rendered

### Bug #2 — Save/load variable restoration
- `src/cereka_save.cpp` §`Impl::SaveGame()` / `Impl::LoadGame()` — save/load variable round-trip
- `src/cereka_save_data.hpp` — `SerializableSaveData` with `variables`/`numVariables` fields
- `src/state/cereka_states.cpp` §`Op::LOAD` — dispatch code at line 284

### Testing
- `tests/compile/` — existing compile test infrastructure
- `tests/compile/inputs/` — input `.crka` files
- `tests/compile/expected/` — expected compiler output

### Phase Scope
- `.planning/ROADMAP.md` §Phase 7 — scope anchor and goals

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`SubstituteVariables()`** in `src/Cereka.cpp:183` — already handles `{var}` → value replacement for say/narrate. Reuse in `EnterMenu()`.
- **Compile test harness** — `tests/compile/harness.lua` with `--update` flag for regenerating expected output

### Established Patterns
- **Compile snapshot tests** — input `.crka` → expected `.txt` diff. Add input file, run `harness.lua --update` to generate expected.

### Integration Points
- `EnterMenu()` at `Cereka.cpp:216` reads `ins.a` as raw button text — call `SubstituteVariables(ins.a)` before `texts.push_back()`
- `tests/compile/` — add new `.crka` input exercising `menu button "Value: {var}" goto label`

</code_context>

<specifics>
## Specific Ideas

- Fix is a one-liner: wrap `ins.a` with `SubstituteVariables()` in `EnterMenu()`
- Compile snapshot test validates the compiler passes `{var}` through correctly in button instruction payloads

</specifics>

<deferred>
## Deferred Ideas

- Bug #2 (save/load variable restoration) — address after Bug #1 is fixed and tested
- Issue #4 (headless mode for automated testing) — separate phase, not in scope here

</deferred>

---

*Phase: 7-Fix critical bugs*
*Context gathered: 2026-05-08*
