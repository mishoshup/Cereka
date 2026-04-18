---
name: no-buzzwords
description: Strip "Enterprise-grade" / "production-ready" / "industrial-strength" / "battle-tested" buzzword comments and headers from the codebase. Use when the user says "remove buzzwords", "clean up the marketing-speak", or after auditing pulled commits.
allowed-tools: Bash
---

# Remove buzzword cruft

The user's standard: "the code either is or isn't enterprise-grade — saying so in a comment doesn't help, it's just noise." Past offenders included `src/state/cereka_state.hpp` and `src/config/config_manager.hpp` headers that opened with marketing copy.

## Find offenders
```bash
rg -n -i 'enterprise|production-ready|industrial.strength|battle.tested|world.class|state.of.the.art|cutting.edge|robust enterprise|enterprise.grade|enterprise.level' \
   --glob '!vendor/**' --glob '!build/**' --glob '!build-*/**' --glob '!.git/**' \
   src/ include/ launcher/ runner/ tests/
```

## What to delete
- Header file banner comments asserting quality (`// Enterprise-grade state machine`).
- Class doc comments that describe the *aspiration* of the design rather than its *behavior*.
- Method doc comments that summarize the architecture pattern by name without saying what the method does.

## What to keep
- Comments explaining a non-obvious *why* (a constraint, a workaround for a specific bug, a subtle invariant).
- License headers.
- TODO/FIXME comments with concrete actions.

## How to fix
Don't replace buzzwords with milder buzzwords. Either:
1. Rewrite the comment to describe one concrete thing the code does that wouldn't be obvious from reading it, OR
2. Delete the comment entirely.

When unsure, delete. A removed comment never lies.

## After cleanup
- Re-run `rg` to confirm zero hits in non-vendor code.
- `ninja -C build -j12` — the changes should be comments-only, build is a sanity check.
