---
name: snapshot
description: Add or update a `.crka` compile-output snapshot test. Use when the user wants to lock in compiler behavior, add coverage for a syntax case, or regenerate expected outputs after an intentional compiler change.
allowed-tools: Bash
---

# Manage compile snapshots

Snapshot suite lives in `tests/compile/`. **Never** call these "golden tests" — use "snapshot" or "compile" tests.

## Layout
```
tests/compile/
  harness.lua        — runner; supports --update
  inputs/<name>.crka — source under test
  expected/<name>.txt — recorded Instruction[] (one ins per line, fields alphabetized)
```

## Add a new case
1. `tests/compile/inputs/<name>.crka` — write a focused `.crka` snippet exercising the syntax you want covered. Keep it small — one feature per file.
2. `lua tests/compile/harness.lua --update` — regenerates `expected/<name>.txt`.
3. **Read the diff** before committing — verify each emitted instruction is what you intended (including `line=N col=N`).
4. `lua tests/compile/harness.lua` — must print `N passed, 0 failed`.

## Update after an intentional compiler change
1. `lua tests/compile/harness.lua` — see what changed.
2. If the diff matches the intent: `lua tests/compile/harness.lua --update`, review `git diff tests/compile/expected/`, commit.
3. If the diff includes things you did NOT intend: stop, the compiler change has unintended side effects.

## Output format
Each line is `key1=v1 key2=v2 ... op=NAME`, fields alphabetized. Empty fields are omitted. Use this for visual diffs — don't switch formats lightly, every prior snapshot would need regeneration.

## When NOT to use snapshots
- Runtime VM behavior — those need GoogleTest cases in `tests/`, not compile snapshots.
- Anything tied to filesystem / SDL — snapshots run in pure Lua.
