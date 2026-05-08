#!/usr/bin/env bash
# ============================================================================
# scripts/update_grammar.sh
#
# Syncs the tree-sitter-cereka grammar (grammar.js) with the Cereka engine's
# canonical keyword manifest (scripts/ops.json).
#
# Usage:
#   ./scripts/update_grammar.sh                  # dry-run / update only
#   ./scripts/update_grammar.sh --rebuild        # update + rebuild WASM
#   GRAMMAR_PATH=/custom/path/grammar.js ./scripts/update_grammar.sh
#
# The --rebuild flag attempts to rebuild tree-sitter-cereka.wasm after the
# grammar is updated. This requires the tree-sitter CLI to be installed
# (npm install -g tree-sitter-cli or npx tree-sitter).
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CEREKA_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TREE_SITTER_REPO="$(cd "$CEREKA_ROOT/../tree-sitter-cereka" && pwd)"

# Allow override via env var or auto-detect sibling
if [ -n "${GRAMMAR_PATH:-}" ]; then
    GRAMMAR_DIR="$(dirname "$GRAMMAR_PATH")"
else
    GRAMMAR_DIR="$TREE_SITTER_REPO"
fi

GRAMMAR_JS="$GRAMMAR_DIR/grammar.js"
OPS_JSON="$CEREKA_ROOT/scripts/ops.json"

REBUILD="${1:-}"
RUN_TS=""

echo "━━━ Tree-sitter Grammar Sync ━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "  Engine repo:   $CEREKA_ROOT"
echo "  Grammar file:  $GRAMMAR_JS"
echo "  Ops manifest:  $OPS_JSON"
echo ""

# ── 1. Run the generator ──────────────────────────────────────────────────

if [ ! -f "$GRAMMAR_JS" ]; then
    echo "  ! Grammar not found at: $GRAMMAR_JS"
    echo "  ! Set GRAMMAR_PATH if your tree-sitter-cereka checkout is elsewhere."
    echo "  ! Skipping grammar update."
    exit 1
fi

node "$CEREKA_ROOT/scripts/gen_tree_sitter_grammar.js"

# ── 2. Optionally rebuild WASM ─────────────────────────────────────────────

if [ "$REBUILD" = "--rebuild" ]; then
    echo ""
    echo "  Rebuilding tree-sitter-cereka.wasm ..."

    # Check for tree-sitter CLI
    if command -v tree-sitter &> /dev/null; then
        RUN_TS="tree-sitter"
    elif npx --yes tree-sitter --version &> /dev/null 2>&1; then
        RUN_TS="npx tree-sitter"
    else
        echo "  ! tree-sitter CLI not found."
        echo "  ! Install with: npm install -g tree-sitter-cli"
        echo "  ! Skipping WASM rebuild."
        exit 1
    fi

    if (cd "$GRAMMAR_DIR" && $RUN_TS build --wasm 2>&1); then
        echo "  ✓ WASM rebuild complete: $GRAMMAR_DIR/tree-sitter-cereka.wasm"
    else
        echo "  ! WASM rebuild failed (this may be OK if emscripten is not set up)."
        echo "  ! The grammar.js was still updated."
        exit 1
    fi
fi

echo ""
echo "━━━ Done ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
