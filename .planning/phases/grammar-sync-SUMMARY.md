# Tree-sitter Grammar Sync Mechanism — Summary

**Type:** Infrastructure / Tooling
**Duration:** 2026-05-09

## Objective

Create a mechanism to keep the tree-sitter-cereka grammar (`grammar.js`) automatically synchronized with the Cereka engine's .crka compiler keyword set (`scripts/cereka_compiler.lua`).

## What Was Built

### 1. `scripts/ops.json` — Keyword Manifest
Canonical JSON manifest of every .crka keyword, operator, and UI element name extracted from the Lua compiler. Contains:
- `statement_keywords` (19 ops: bg, char, say, narrate, etc.)
- `block_keywords` (menu, ui)
- `menu_keywords` (bg, button)
- `flow_keywords` (if, else, endif, button)
- `comparison_ops` (==, !=, >, <, >=, <=)
- `arithmetic_ops` (+, -, *, /)
- `assignment_ops` (=, +=, -=, *=, /=)
- `ui_elements` (textbox, namebox, button, font)
- `positions` (left, center, right)
- `spec_keywords` (wait, click, assert)

### 2. Grammar Markers (`grammar.js`)
Three `// AUTO-GENERATED` / `// END AUTO-GENERATED` marker pairs in the tree-sitter grammar, placed around:
- `comparison_op` — comparison operator choice list
- `arithmetic_op` — arithmetic operator choice list
- `ui_element` — UI element name choice list

### 3. `scripts/gen_tree_sitter_grammar.js` — Generator
Node.js script that:
- Reads `ops.json` and reads `grammar.js` from the tree-sitter-cereka sibling repo
- Replaces content inside AUTO-GENERATED markers with current values from ops.json
- Validates that all compiler statement keywords have matching grammar rules
- Reports warnings for missing grammar rules (e.g., `checkpoint`, `scene_graph`)
- Supports `GRAMMAR_PATH` environment variable override

### 4. `scripts/update_grammar.sh` — Convenience Wrapper
Shell script that runs the generator and optionally rebuilds the WASM binary (`--rebuild` flag).

### 5. Updated `CLAUDE.md`
Added a **Tree-sitter Grammar Sync** section documenting the sync workflow, the files involved, marked sections in grammar.js, and step-by-step instructions for syncing after adding new keywords.

## Commits

### Cereka Engine (`cereka/`)
| Hash | Message |
|------|---------|
| 40137d8 | chore: add ops.json keyword manifest extracted from cereka_compiler.lua |
| d8adb84 | feat: add gen_tree_sitter_grammar.js — auto-sync tree-sitter grammar with compiler |
| 3927765 | chore: add update_grammar.sh convenience wrapper for tree-sitter sync |
| a684983 | docs: add tree-sitter grammar sync documentation to CLAUDE.md |

### Tree-sitter Grammar (`tree-sitter-cereka/`)
| Hash | Message |
|------|---------|
| 2bb3538 | chore: add AUTO-GENERATED markers for keyword synchronization |

## Known Gaps (validated by generator)

The validator reports that the following compiler keywords lack grammar rules in grammar.js:
- `checkpoint` — no tree-sitter rule exists yet
- `scene_graph` — no tree-sitter rule exists yet

These are expected gaps — the compiler implemented these features before the grammar was created. They will need manual grammar rule additions when IDE support is desired for these features.
