# Phase 9: Headless Mode - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-08
**Phase:** 9-Headless mode
**Areas discussed:** CLI flag design, Output format, Auto-advance behavior, Exit conditions and error codes

---

## CLI Flag Design

| Option | Description | Selected |
|--------|-------------|----------|
| `--headless` with `--entry` override | Uses game.cfg, --entry overrides entry point | ✓ |
| `--headless` with positional script | No game.cfg needed, just a .crka file | |
| Separate `--script` mode | implies headless, auto-exits | |

**User's choice:** Chose `--headless` with optional `--entry` override as most enterprise-grade. Backward compatible, composable flags, no breaking changes.

## Output Format

| Option | Description | Selected |
|--------|-------------|----------|
| Plain text | Simple lines, grep-able | ✓ |
| JSON lines | Machine-parseable | ✓ |
| Both | Plain text default, --json flag | ✓ |

**User's choice:** Both. Plain text by default for human readability, `--json` flag for machine parsing.

## Auto-Advance Behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Instant execution | Max speed, no delay, no typewriter | ✓ |
| Timed auto-advance | Configurable delay per line | |
| Skip to end | No per-line output | |

**User's choice:** Instant execution — dispatch at max speed for CI speed.

## Exit Conditions and Error Codes

| Option | Description | Selected |
|--------|-------------|----------|
| Exit on end, code from FAIL grep | Simple pipe-friendly | |
| Built-in FAIL detection | Engine tracks FAIL, exits non-zero | ✓ |
| Both + --until label | Most flexible | |

**User's choice:** Rust-style error diagnostics with source locations, built-in FAIL detection, summary at end with PASS/FAIL count, exit code 0 on pass. Like Rust errors in motive (clear location, what went wrong, summary) but prettier format.

## the agent's Discretion

- Exact diagnostic output format
- `--json` schema
- Menu handling in headless mode
- Whether to suppress cereka_debug.txt

## Deferred Ideas

- None
