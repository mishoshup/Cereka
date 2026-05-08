# Phase 7: Fix critical bugs — var substitution in menus + save/load variable restoration - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-08
**Phase:** 7-Fix critical bugs
**Areas discussed:** Fix scope, Test strategy, Headless mode feasibility

---

## Fix Scope — One or Both Bugs?

| Option | Description | Selected |
|--------|-------------|----------|
| Both bugs | Fix both Bug #1 and Bug #2 | |
| Bug #1 only first | Fix var substitution, test it, then investigate Bug #2 | ✓ |

**User's choice:** Fix Bug #1 only first, then test it. Address Bug #2 later.

## Test Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Compile snapshot | Add .crka input + expected output to existing test suite | ✓ |
| C++ unit test | Write GoogleTest for save/load round-trip | |
| Both | Both test approaches | |

**User's choice:** Compile snapshot test + whatever is pragmatic and easy to ensure correctness.

## Headless Mode

| Option | Description | Selected |
|--------|-------------|----------|
| Build headless mode | Add --headless flag for automated testing | |
| Defer | Separate phase, not in scope here | ✓ |

**User's choice:** Deferred. Asked about difficulty — estimated ~100 lines, not trivial, out of scope for this phase.

## the agent's Discretion

- Exact compile snapshot test file naming and expected output format
- Whether to add inline C++ unit test alongside snapshot

## Deferred Ideas

- Bug #2 (save/load variable restoration) — address after Bug #1
- Issue #4 (headless mode) — separate phase
