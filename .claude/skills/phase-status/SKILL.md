---
name: phase-status
description: Read PLAN.md and report which phase is in-progress, what's next, and what blockers remain. Use when the user asks "where are we", "what's next", "phase status", or at the start of a session.
---

# Phase status

Source of truth: `PLAN.md` at the repo root.

## Procedure
1. Read `PLAN.md`. Skim the "Current State" section and every `## Phase` header.
2. Look for these markers in headings/bodies:
   - `✅` / `done` / `complete` → **shipped**
   - `🟡` / `partial` / `in progress` → **in-progress**
   - `🔴` / `blocked` → **blocker**
   - No marker but listed under a future phase → **upcoming**
3. Cross-check with the actual repo state — PLAN.md can lag reality:
   - "compiler rewrite" claim → check `scripts/compiler.lua` is the tokenizer+AST version, not regex.
   - "state machine extracted" → confirm `src/state/cereka_states.cpp` has real bodies, not empty stubs.
   - "save format = glaze JSON" → confirm `src/save_data.hpp` exists and is used.
   - "split god class" → confirm `src/engine_impl.hpp` has *fewer* fields than before, not more.

## Report format
Three short sections, no preamble:

**Done** — bullet list of completed phases (one line each).
**In progress** — current phase + which file(s) are mid-change.
**Next** — the immediate next phase from PLAN.md, with the *first concrete file* to touch.

If reality and PLAN.md disagree, flag it: `⚠ PLAN.md says X but repo state shows Y`. Don't silently trust either source.

## What this skill is NOT
- Not a plan re-write — call out drift but don't edit PLAN.md unless the user asks.
- Not a code review — see `/review` for that.
