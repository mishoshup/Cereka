---
name: review
description: Audit recent changes (or named files) against Cereka's quality bar — Ren'Py/Unity/Unreal class, no shortcuts. Use when the user asks "is this good?", "review this", "what's wrong with this code", or before merging meaningful work.
---

# Cereka quality review

The bar: code that genuinely belongs in an engine that *rivals* Ren'Py/Unity/Unreal. Not "works on my machine". Not "good enough for a hobby project". The user has explicitly said this is the standard.

## How to run a review
1. `git status` + `git diff` to see what changed since the last clean point. If the user named files, focus there.
2. Walk the checklist below in order. Stop at the first failed gate; do not produce a passing review when any item is red.
3. Output: a punch list — which items pass (one line each) and which fail (failing item + file:line + concrete fix). Lead with the failures.

## Checklist (red flags — every "yes" is a finding)

### Architecture
- **God class growth** — did `src/engine_impl.hpp` gain a field this change? Should it have lived in `SceneManager`/`DialogueSystem`/`AudioManager`/`ScriptVM`/`UIManager` instead? (Phase 0.2 in PLAN.md is about *removing* fields, not adding.)
- **Dead code** — new class with empty method bodies, abstract interface with one implementation that no one calls, "framework" header with no consumer. (Past offender: `src/state/cereka_states.cpp` shipped as empty stubs — don't repeat.)
- **Parallel state** — new `unordered_map<string, X>` that duplicates something already in `variables`/`numVariables`/scene state. Pick one source of truth.
- **Public surface leak** — `SDL_Texture*`, `MIX_Audio*`, or other vendor types in `include/Cereka/*.hpp`. Public API stays narrow.

### Code quality
- **Buzzword headers** — comments that say "Enterprise-grade", "production-ready", "battle-tested", "industrial-strength". Delete them. The code either is or isn't; the comment doesn't help.
- **Obvious comments** — comments explaining *what* a well-named function already says. Comments must explain *why* (a non-obvious constraint, a workaround for a specific bug). If removing it wouldn't confuse anyone, remove it.
- **Premature abstraction** — `IFooFactory` for a single concrete `Foo`. CRTP for one type. Three near-duplicate switch arms is fine; abstracting them prematurely is worse.
- **Speculative error handling** — `try/catch` around code that can't throw, `nullptr` checks for things constructed in the same scope, `std::expected` import that's never used.
- **`std::cerr` debug spam** — VM cases that print on every dispatch. Production code does not log per-instruction.

### Engine-specific gotchas
- **No `srcLine`/`srcCol`** — every `Instruction` emitted from the compiler must carry these. Lost source location = unusable error messages.
- **Save format change without version bump** — `src/save_data.hpp` has glaze JSON. Schema-breaking changes need a `version` field check + migration, not silent corruption of existing saves.
- **Float comparisons in tests** — `EXPECT_EQ` on floats is wrong; use `EXPECT_NEAR` *only* with a justified epsilon, otherwise restructure to avoid the comparison.
- **`std::stof` of a free-form string** — likely needs `EvalExpr` (the recursive-descent expr parser in `src/script_vm.cpp`) instead, so variable names and arithmetic in RHS work.
- **Adding a `case` to `TickScript()` without choosing pause behavior** — synchronous (`pc++; continue;`) vs. yield (`pc++; return;`). Both wrong choices break the tick loop.

### Tests
- **No coverage for new behavior** — compile-touching change with no `tests/compile/` snapshot, or VM-touching change with no `tests/cereka_test` case.
- **Tests adjusted to match buggy code** — snapshot regenerated *because* the code now does something wrong. Updating expected files is fine when the change is intentional; not fine to mask a regression.
- **Skipped test under `if (WIN32)`** — anything Linux-skipped or Windows-skipped without a written reason is a future bug.

### Process
- **`--no-verify` / `--no-gpg-sign`** — never. If a hook fails, fix the underlying issue.
- **`git add .` / `git add -A`** in suggested commands — prefer named files; mass-adds collect cruft.

## What "passes" means
A clean review is one line per item ("ok") followed by an explicit final line: **"passes Cereka quality bar"**. Anything less, list the failing items with file:line and the concrete fix — not vague "consider refactoring".

## What this skill is NOT
- Not a style linter — clang-tidy/clang-format handle that.
- Not a security review — use `/security-review` for that.
- Not a performance audit — call that out separately.
