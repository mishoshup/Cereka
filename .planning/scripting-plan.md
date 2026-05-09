# Scripting Enhancement: else-if, &&, ||

## Design Decisions

### 1. Lowering, not new VM ops
`&&` and `||` are **lowered to nested if-blocks** using existing IF_EQ/IF_NEQ/ELSE/ENDIF instructions. No new Op enum entries, no C++ changes.

### 2. Temp-variable approach for all compound conditions
Rather than duplicating body instructions in the instruction stream (which would be needed for `if (a || b) { body }` lowering to `if a { body } else { if b { body } }`), we use a **temporary numeric variable** to capture the condition result. The pattern is:

```
if a && b || c { body }

→

$ __or_0 = 0
if a
    if b
        $ __or_0 = 1
    endif
endif
if c
    $ __or_0 = 1
endif
if __or_0 == 1
    body
endif
```

Benefits:
- No body duplication in instruction stream
- Works uniformly for `&&`, `||`, and mixed`else if` / `else` clauses
- Short-circuit within AND groups (nested IFs), no short-circuit across OR groups (acceptable for VN)
- Uses only IF_EQ, SET_VAR_NUM, and ENDIF — all existing instructions

### 3. Tree-structured If AST
If/elseif/else/endif blocks become tree-structured (like `menu` blocks), not flat AST nodes. The `if` keyword at the top level triggers block-aware parsing that collects the body, then handles `elseif`/`else`/`endif` continuations.

### 4. else-if chaining
Three forms supported:
- `elseif <cond>` (single keyword)
- `else if <cond>` (two keywords, syntactic sugar for `elseif`)
- `else` (bare else)

All desugared in the lowerer to ELSE + IF/condition chains.

### 5. Precedence
- `&&` binds tighter than `||` (same as C/C++)
- Evaluated via precedence climbing: `or > and > primary`

## Files to Modify

| File | Change |
|------|--------|
| `scripts/ops.json` | Add `&&`, `||`, `elseif` |
| `scripts/cereka_compiler.lua` | Parser: tree-structured if blocks; condition parsing with &&/\|\|; Lowerer: compound condition lowering |
| `tests/compile/inputs/logical_ops.crka` | New test input |
| `tests/compile/expected/logical_ops.txt` | Generated via `--update` |

No changes to C++ files (no new ops needed).

## Implementation Steps

1. Update tokenizer: add `&&`, `||` to TWO_CHAR_OPS and `&`, `|` to the operator char class
2. Update parser:
   - New condition parsing with `&&`/`||` precedence
   - New `parse_if_block` for tree-structured AST
   - Remove `else`/`endif` from STMT_HANDLERS
3. Update lowerer:
   - New `If` lowerer with compound condition support
   - Remove `Else`/`Endif` from LOWERERS
4. Add test case
5. Run `lua tests/compile/harness.lua --update`
6. Verify all existing tests pass
7. Verify build
