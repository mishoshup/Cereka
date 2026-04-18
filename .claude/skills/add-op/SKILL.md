---
name: add-op
description: Add a new VN instruction op end-to-end (parser → AST → lowerer → enum → bridge → VM dispatch). Use when the user wants to add a new `.crka` keyword or instruction.
---

# Add a new VN Op

Touch **exactly these files**, in order. Skipping any leaves the engine in a broken state.

## 1. `scripts/compiler.lua` — parser + lowerer
- Add a `parse_<keyword>(ctx)` function near the other `parse_*` functions. Use existing primitives: `take`, `expect`, `peek`, `rest_text`, `maybe_unquote`, `die(line, col, msg)`.
- Return an AST node `{ kind = "<Pascal>", ..., line = kw.lineno, col = kw.col }`.
- Register it in `STMT_HANDLERS` (or `MENU_HANDLERS` if it's a menu-block child).
- Add a `LOWERERS.<Pascal>` entry that emits `{ op = "<UPPER>", a = ..., b = ..., c = ..., line = n.line, col = n.col }`.

## 2. `src/compiler/vn_instruction.hpp` — Op enum
- Add `<UPPER>` to `enum class Op`. Pick a sensible group with similar ops.

## 3. `src/compiler/vn_instruction.cpp` — string→Op bridge
- Add `else if (op == "<UPPER>") ins.op = Op::<UPPER>;` in the dispatch chain inside `RunLuaCompiler`.

## 4. `src/script_vm.cpp` — VM dispatch
- Add a `case scenario::Op::<UPPER>:` in `TickScript()`'s switch.
- Decide pause behavior:
  - **Synchronous instruction** (BG, SET_VAR…): mutate state, `pc++`, `continue`.
  - **Yields to user / changes high-level state** (SAY, MENU, FADE, SAVE_MENU): set state, `pc++`, `return` to break the tick loop.

## 5. Snapshot test — capture the new op
- Add lines using the new keyword to an existing `tests/compile/inputs/*.crka` (or create a new one).
- `lua tests/compile/harness.lua --update` and inspect the diff.
- Re-run `lua tests/compile/harness.lua` to confirm green.

## 6. Skip-mode awareness (if relevant)
If the new op participates in if/else nesting (a new comparison, or a conditional flow op), update the skip-mode block at the top of `TickScript()` so it depth-tracks correctly.

## 7. Documentation
- Update the `.crka` syntax block in `CLAUDE.md` so the language reference stays in sync.
- Update `launcher/templates.hpp` only if the new op should appear in the scaffolded project (don't dump every op into the template).

## Don't
- Don't add a new field to `Instruction` for a single op — pack into existing `a`/`b`/`c` slots first; expand the struct only when ≥3 ops need it.
- Don't add `std::cerr` debug spam in the VM case — none of the production cases log.
- Don't introduce a parallel state map. Use `variables` / `numVariables` / scene state already present.
