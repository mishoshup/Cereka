# Phase 8: Fix inconsistent type coercion - Context

**Gathered:** 2026-05-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix bug: comparing a string variable to an integer variable fails, but comparing a string variable to an integer literal succeeds. Both should coerce consistently.

</domain>

<decisions>
### D-01: Fix Location — IF_EQ/IF_NEQ dispatch in cereka_states.cpp
- Root cause: `IF_EQ` and `IF_NEQ` compare `val` (resolved from `ins.a`) against `ins.b` as a raw string
- When RHS is a variable name (e.g., `spike_int_five`), `ins.b` contains the name, not the value
- Fix: resolve `ins.b` from `variables` map before comparing

### D-02: Test — compile snapshot
- Add a .crka exercising all comparison patterns:
  - string var == int var (should coerce)
  - string var == literal int (already works)
  - int var == int var

</decisions>

<canonical_refs>
- `src/state/cereka_states.cpp` §IF_EQ/IF_NEQ dispatch — fix location
- `scripts/cereka_compiler.lua` §parse_if — how RHS is parsed (raw text, no variable resolution)
- `tests/compile/inputs/` — snapshot test directory
</canonical_refs>
