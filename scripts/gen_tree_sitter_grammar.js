#!/usr/bin/env node

/**
 * scripts/gen_tree_sitter_grammar.js
 *
 * Reads ops.json (keyword manifest extracted from the Lua compiler) and
 * updates the tree-sitter grammar (grammar.js) to keep keyword and operator
 * lists in sync between the Cereka engine and the tree-sitter-cereka repo.
 *
 * Usage:
 *   node scripts/gen_tree_sitter_grammar.js
 *   GRAMMAR_PATH=/custom/path/grammar.js node scripts/gen_tree_sitter_grammar.js
 *
 * What it does:
 *   1. Replaces content inside AUTO-GENERATED markers in grammar.js with
 *      the corresponding lists from ops.json.
 *   2. Validates that every statement keyword in ops.json has a matching
 *      grammar rule in the statement choice list.
 *   3. Reports changes, warnings, and a sync summary.
 *
 * Marked sections (// AUTO-GENERATED: <name> … // END AUTO-GENERATED):
 *   - comparison_op: operator choice list for if-block comparisons
 *   - arithmetic_op: operator choice list for binary expressions
 *   - ui_element:    element name choice list for ui blocks
 */

const fs = require("fs");
const path = require("path");

// ── Configuration ────────────────────────────────────────────────────────────

/** Root of the cereka engine repo (where scripts/ lives). */
const ENGINE_ROOT = path.resolve(__dirname, "..");
const OPS_JSON = path.join(ENGINE_ROOT, "scripts", "ops.json");

/**
 * Default path to the tree-sitter grammar file.
 * Override with the GRAMMAR_PATH environment variable.
 */
const DEFAULT_GRAMMAR = path.resolve(
  ENGINE_ROOT,
  "..",
  "tree-sitter-cereka",
  "grammar.js",
);

const GRAMMAR_PATH = process.env.GRAMMAR_PATH || DEFAULT_GRAMMAR;

/** Sections that can be auto-replaced. Each maps an ops.json key to a
 *  formatter that produces the full grammar rule body. */
const AUTO_GENERATED = {
  comparison_op: {
    opsKey: "comparison_ops",
    format(items) {
      const list = items.map((o) => `"${o}"`).join(", ");
      return `    comparison_op: (_) => choice(${list}),`;
    },
  },

  arithmetic_op: {
    opsKey: "arithmetic_ops",
    format(items) {
      const list = items.map((o) => `"${o}"`).join(", ");
      return `    arithmetic_op: (_) => choice(${list}),`;
    },
  },

  ui_element: {
    opsKey: "ui_elements",
    format(items) {
      const list = items.map((e) => `"${e}"`).join(", ");
      return `    ui_element: (_) => choice(${list}),`;
    },
  },
};

// ── Helpers ───────────────────────────────────────────────────────────────────

function readJSON(p) {
  return JSON.parse(fs.readFileSync(p, "utf-8"));
}

function readText(p) {
  return fs.readFileSync(p, "utf-8");
}

function writeText(p, content) {
  fs.writeFileSync(p, content, "utf-8");
}

/**
 * Check whether the grammar file exists at the expected path and print a
 * sensible message if not (graceful degradation, not a hard error).
 */
function checkGrammarPath() {
  if (!fs.existsSync(GRAMMAR_PATH)) {
    console.log(
      "  Grammar not found at: " +
        GRAMMAR_PATH +
        "\n" +
        "  Set GRAMMAR_PATH to point at the tree-sitter grammar file.\n" +
        "  Skipping grammar update.",
    );
    return false;
  }
  return true;
}

// ── Section replacement ───────────────────────────────────────────────────────

/**
 * Replace the content between `// AUTO-GENERATED: <id>` and
 * `// END AUTO-GENERATED` with the freshly generated rule body.
 *
 * Returns `true` if a change was made, `false` if already up-to-date or
 * the marker was missing.
 */
function replaceSection(grammar, id, config, ops) {
  const startMarker = `// AUTO-GENERATED: ${id}`;
  const endMarker = "// END AUTO-GENERATED";

  const startIdx = grammar.indexOf(startMarker);
  if (startIdx === -1) {
    console.warn(`  ⚠ Marker not found in grammar.js: "// AUTO-GENERATED: ${id}"`);
    console.warn("    Add the marker manually after the corresponding rule definition.");
    return false;
  }

  // Match the end marker line with any leading whitespace
  const endRe = /\n([ \t]*)\/\/ END AUTO-GENERATED/;
  const tail = grammar.slice(startIdx);
  const endMatch = tail.match(endRe);
  if (!endMatch) {
    console.warn(`  ⚠ Closing marker "// END AUTO-GENERATED" not found after "${startMarker}"`);
    return false;
  }

  // endMatch.index is relative to `tail`; convert to absolute position
  const endIdx = startIdx + endMatch.index;
  // endMatch.index points to the newline *before* // END AUTO-GENERATED
  const contentStart = startIdx + startMarker.length;
  const endOfContent = endIdx; // position of the `\n` just before `// END AUTO-GENERATED` line

  const oldContent = grammar.slice(contentStart, endOfContent).trim();
  const newContent = config.format(ops[config.opsKey]);

  // Trim for comparison to ignore indentation differences between runs
  if (oldContent === newContent.trim()) {
    return false; // no change needed
  }

  // Rebuild: keep the start marker, replace the line between, keep the end marker.
  // grammar.slice(endOfContent) already includes the \n before // END AUTO-GENERATED,
  // so we just insert newContent + newline after the start marker and let the
  // trailing slice carry the END line.
  grammar =
    grammar.slice(0, contentStart) +
    "\n" +
    newContent +
    grammar.slice(endOfContent);

  console.log(`  ✓ Updated: ${id}`);
  return { grammar, changed: true };
}

// ── Validation ────────────────────────────────────────────────────────────────

/**
 * Extract all `$.ruleName` references from the `statement` choice block and
 * cross-reference them against the compiler's keyword list.
 *
 * This is a best-effort heuristic — it looks for the first `choice(` after
 * `statement:` and extracts `$.xxx` tokens. Reports mismatches as warnings.
 */
function validateStatements(grammar, ops) {
  const allKeywords = [
    ...ops.statement_keywords,
    ...ops.block_keywords,
    "$", // arithmetic_stmt
  ];

  // Map compiler keywords to their likely grammar rule names
  const keywordToRule = {
    bg: "bg_stmt",
    char: "char_stmt",
    hide: "hide_char",
    say: "say",
    narrate: "narrate",
    label: "label_stmt",
    jump: "jump",
    include: "include",
    call: "call",
    end: "end",
    menu: "menu",
    set: "set_stmt",
    $: "arithmetic_stmt",
    if: "if_block",
    bgm: "bgm",
    stop_bgm: "stop_bgm",
    sfx: "sfx",
    save_menu: "save_menu",
    load_menu: "load_menu",
    save: "save_stmt",
    load: "load_stmt",
    ui: "ui_block",
  };

  // Extract rule reference names from the statement choice block
  const stmtStart = grammar.search(/statement:\s*\(/);
  if (stmtStart === -1) {
    console.warn("  ⚠ Could not locate the statement rule in grammar.js");
    return;
  }

  // Find the matching closing paren of choice(
  const choiceStart = grammar.indexOf("choice(", stmtStart);
  if (choiceStart === -1) {
    console.warn("  ⚠ Could not locate choice() inside the statement rule");
    return;
  }

  const grammarRules = collectRuleRefs(grammar, choiceStart);

  // Check each keyword from the compiler
  const missing = [];
  for (const kw of allKeywords) {
    const ruleName = keywordToRule[kw] || `${kw}_stmt`;
    if (!grammarRules.has(ruleName)) {
      // Special cases: else/endif are inline in if_block, not separate rules
      if (kw === "else" || kw === "endif") continue;
      // scene_graph and checkpoint legitimately need grammar rules
      missing.push({ keyword: kw, expectedRule: ruleName });
    }
  }

  // Check for grammar rules that don't map to any compiler keyword
  const extraRules = findExtraRules(grammarRules, keywordToRule);

  if (missing.length > 0) {
    console.warn("\n  ⚠ STATEMENT VALIDATION — keywords missing grammar rules:");
    for (const m of missing) {
      console.warn(`     Missing rule: ${m.expectedRule} (for keyword "${m.keyword}")`);
    }
  }

  if (extraRules.length > 0) {
    console.warn("\n  ⚠ STATEMENT VALIDATION — grammar rules with no compiler keyword:");
    for (const r of extraRules) {
      console.warn(`     Orphaned rule: ${r}`);
    }
  }

  if (missing.length === 0 && extraRules.length === 0) {
    console.log("  ✓ Statement validation: all keywords have matching grammar rules");
  }
}

/**
 * Extract $.ruleName references from a `choice(...)` block starting at `start`.
 * Uses a simple paren-matching approach to handle nested parens.
 */
function collectRuleRefs(text, start) {
  const parenStart = text.indexOf("(", start);
  if (parenStart === -1) return new Set();

  // Match parens
  let depth = 1;
  let i = parenStart + 1;
  while (i < text.length && depth > 0) {
    const c = text[i];
    if (c === "(") depth++;
    else if (c === ")") depth--;
    i++;
  }

  const block = text.slice(parenStart, i);
  const refs = new Set();
  const refRe = /\$\.(\w+)/g;
  let m;
  while ((m = refRe.exec(block)) !== null) {
    refs.add(m[1]);
  }

  return refs;
}

/** Find rule references that don't correspond to any compiler keyword mapping. */
function findExtraRules(grammarRuleNames, keywordToRule) {
  const knownRules = new Set(Object.values(keywordToRule));
  // Also allow some internal helper rules
  knownRules.add("position");
  knownRules.add("menu_bg");
  knownRules.add("button");
  knownRules.add("value");
  knownRules.add("binary_expr");
  knownRules.add("expr");
  knownRules.add("ui_property");
  knownRules.add("ui_value");
  knownRules.add("identifier");
  knownRules.add("filename");
  knownRules.add("string");
  knownRules.add("number");
  knownRules.add("comment");
  knownRules.add("source_file");

  const extras = [];
  for (const rule of grammarRuleNames) {
    if (!knownRules.has(rule)) {
      extras.push(rule);
    }
  }
  return extras;
}

// ── Main ──────────────────────────────────────────────────────────────────────

function main() {
  console.log("── Tree-sitter Grammar Sync ──────────────────────────\n");

  // 1. Load ops.json
  const ops = readJSON(OPS_JSON);
  console.log(`  Source: ${OPS_JSON}  (ops v${ops.version}, ${ops.statement_keywords.length + ops.block_keywords.length + 1} keywords, ${ops.comparison_ops.length} comparison ops, ${ops.ui_elements.length} ui elements)`);

  // 2. Check grammar file exists
  if (!checkGrammarPath()) return;

  let grammar = readText(GRAMMAR_PATH);
  let changed = false;

  // 3. Replace auto-generated sections
  console.log("\n  Updating auto-generated sections:");
  for (const [id, config] of Object.entries(AUTO_GENERATED)) {
    const result = replaceSection(grammar, id, config, ops);
    if (result && result.changed) {
      grammar = result.grammar;
      changed = true;
    } else if (!result) {
      // marker missing — already warned inside replaceSection
    }
  }

  // 4. Validate statement rules
  console.log("");
  validateStatements(grammar, ops);

  // 5. Write back if changed
  if (changed) {
    writeText(GRAMMAR_PATH, grammar);
    console.log(`\n  ✎ Written: ${GRAMMAR_PATH}`);
  } else {
    console.log("\n  ✓ grammar.js is already up to date.");
  }

  // 6. Summary
  console.log(`\n──────────────────────────────────────────────────────`);
}

if (require.main === module) {
  main();
}

module.exports = { AUTO_GENERATED, validateStatements, collectRuleRefs };
