# Scripting Enhancement Review: else-if, &&, ||

## Summary of Changes

Added three language features to the .crka compiler:

1. **`elseif` / `else if`** — Chained conditional blocks
2. **`&&`** — Logical AND in conditions
3. **`||`** — Logical OR in conditions

## Key Design Decisions

### No new VM instructions
`&&` and `||` are lowered entirely using existing instructions (IF_EQ, SET_VAR_NUM, ELSE, ENDIF). No changes to the Op enum, C++ bridge, or VM dispatcher were needed.

### Temp-variable approach for compound conditions
Rather than duplicating body instructions (which would be needed for `if a || b { body }` lowering to nested if-blocks), compound conditions use a temporary numeric variable:

```
if a && b || c { body }

→

$ __cnd_N = 0
if a
    if b
        $ __cnd_N = 1
    endif
endif
if c
    $ __cnd_N = 1
endif
if __cnd_N == 1
    body
endif
```

Benefits:
- No body duplication in the instruction stream
- Works uniformly for `&&`, `||`, `else if`, and `else` clauses
- Short-circuit within AND groups (nested IFs)
- Loses short-circuit across OR groups (acceptable for VN since conditions have no side effects)

### Tree-structured If AST
If/elseif/else/endif blocks were refactored from flat AST nodes (where `If`, body statements, `Else`, body statements, `Endif` were all separate AST nodes) to a tree structure. The `If` AST node now owns its children, elseIf list, and elseChildren. This was necessary to support compound condition lowering, which needs to emit multiple IF/ENDIF pairs around the body.

### Precedence
`&&` binds tighter than `||`, matching C/C++ convention. Parsed via precedence climbing: `or > and > primary`.

### Both `elseif` and `else if` supported
- `elseif` as a single keyword
- `else if` as two keywords (desugars to same AST)

## Backward Compatibility

All existing .crka scripts continue to work unchanged — all 14 existing compile-snapshot tests pass and all 67 C++ unit tests pass. The only behavioral change is a better error message: bare `endif` or `else` outside an if-block now produces a compile-time error ("unknown statement") instead of silently emitting instructions.

## Grammar Sync

`scripts/ops.json` updated with `logical_ops: ["&&", "||"]` and updated `flow_keywords` / `two_char_ops`. The tree-sitter grammar sync script (`scripts/gen_tree_sitter_grammar.js`) ran successfully; `grammar.js` is up to date.

## Concerns

1. **Temp variable uniqueness:** The temp variable counter (`__cnd_N`) is global across all compilations. In the current design, the Lua compiler is loaded fresh for each compile via `RunLuaCompiler`, so the counter resets. This is safe. If the Lua state is reused, the counter would increment without reset — but the current architecture never reuses state.

2. **Complex nested compound conditions:** `if (a && b) || (c && d)` produces correct code but generates a relatively large number of instructions for a simple logical expression. For a VN scripting language this is acceptable.

3. **Line number accuracy:** Synthetic instructions (temp var SET_VAR, nested ENDIFs) use the nearest keyword's line number. This is adequate for error reporting.
