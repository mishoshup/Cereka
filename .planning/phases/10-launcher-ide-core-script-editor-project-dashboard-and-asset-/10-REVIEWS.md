# Phase 10: Launcher IDE Core — Plan Review (Cycle 2)

**Reviewer:** gsd-code-reviewer
**Date:** 2026-05-09
**Cycle:** 2
**Previous Review:** Cycle 1 (10-REVIEWS.md)

## Previous HIGH Concerns

The user identified 4 HIGH concerns from a previous review that should now be addressed:

| # | Concern | Current Status | Verdict |
|---|---------|---------------|---------|
| H-01 | Compile-on-save linked against engine library | Uses `CerekaGame --compile-only` child process (Task 6) | ✅ RESOLVED |
| H-02 | Spec runner linked against engine library | Uses `CerekaGame --script` child process (Task 5) | ✅ RESOLVED |
| H-03 | QScintilla dependency risk | Conditional `find_package(QScintilla QUIET)` + `QPlainTextEdit`/`CrkaHighlighter` fallback (Task 1) | ✅ RESOLVED |
| H-04 | Qt6 Multimedia missing from CMake | `find_package(Qt6 COMPONENTS ... Multimedia REQUIRED)` + `target_link_libraries(... Qt6::Multimedia)` (Task 1) | ✅ RESOLVED |

All 4 previous HIGH concerns are properly resolved. Detailed analysis below.

---

## Assessment: Previous HIGH Concerns

### H-01: Compile-on-save → child process

**Status: RESOLVED ✅**

Task 6 clearly scopes the work:
- Adds `--compile-only` flag to `runner/main.cpp`
- Spawns `CerekaGame --compile-only <project-root>` as child process per-save
- Reuses shared `runChildProcess` helper (extracted from existing `doLaunch`)
- 500ms debounce to prevent process thrashing
- Parses error output for source-line markers; displays in gutter + Problems dock
- Explicit acceptance criterion: "The launcher does NOT link against Cereka engine library"

The plan correctly avoids engine library linking. The architecture constraint from CLAUDE.md ("Boundary rule: The launcher MUST NOT link against the Cereka engine static library") is respected.

**Minor observation (not a concern):** The plan describes the `--compile-only` implementation as "check after the existing compile step" (which runs unconditionally during init). This means SDL and engine init still happen before the early-exit. A more efficient approach would extract compilation before init, but the described approach is simpler, correct, and avoids coupling risks. The cost is a slower compile-only invocation (~hundreds of ms of unnecessary init) — acceptable for a save-triggered background operation.

### H-02: Spec runner → child process

**Status: RESOLVED ✅**

Task 5 explicitly calls out the architecture:
- "Spec runner uses `CerekaGame --script file.spec.crka` as a child process — NOT direct compilation"
- Reuses `doLaunch` pipe-capture pattern or extracted `runChildProcess` helper
- Shows pass/fail with exit code in log panel
- Spec file selector enumerates `.spec.crka` files

Clean, well-scoped. Uses the exact same pattern as the existing `doLaunch()` (fork/exec on Linux, CreateProcess on Windows).

### H-03: QScintilla → fallback path

**Status: RESOLVED ✅**

Task 1 covers the build-system changes:
- `find_package(QScintilla QUIET)` — never fails the build
- Conditional `target_compile_definitions(... HAS_QSCINTILLA=1)` when found
- Fallback: `QPlainTextEdit` + `CrkaHighlighter` (standalone `QSyntaxHighlighter` subclass in own `.hpp`/`.cpp`)
- Task 2 builds both paths with the same user-facing API

This fully addresses the Cycle 1 HIGH concern about QScintilla not being vendored. The fallback approach is superior to vendoring — it eliminates the dependency entirely where unavailable, while preserving the richer QScintilla experience where available. This mirrors the approach used by Godot.

### H-04: Qt6 Multimedia → in CMakeLists.txt

**Status: RESOLVED ✅**

Task 1 adds:
```cmake
find_package(Qt6 COMPONENTS Widgets Multimedia REQUIRED)
target_link_libraries(CerekaLauncher PRIVATE Qt6::Widgets Qt6::Multimedia)
```

Task 7 (asset browser) uses `QMediaPlayer` + `QAudioOutput` for audio preview. The dependency is justified by the feature.

---

## New Issues in the Updated Plan

### W-01 (Cycle 2): Missing file allocation for QsciLexerCrka

**File:** Plan lines 16-22, 69, 120
**Severity: WARNING**

The plan declares files `crka_highlighter.hpp` and `crka_highlighter.cpp` for the fallback `CrkaHighlighter` (a `QSyntaxHighlighter` subclass). However, the QScintilla path also requires a custom lexer class (`QsciLexerCrka` — mentioned in line 69 and 120). No files are allocated for this class.

**Current file allocation:**
```
launcher/crka_highlighter.hpp  → Fallback only (QSyntaxHighlighter)
launcher/crka_highlighter.cpp  → Fallback only
```

**Missing:**
- `launcher/crka_lexer.hpp` → QScintilla path (QsciLexerCrka, a QsciLexerCustom subclass)
- `launcher/crka_lexer.cpp` → QScintilla path

**Fix:** Either:
1. Add explicit `crka_lexer.hpp`/`crka_lexer.cpp` files to the plan's `files_created` list and the `add_executable` call, OR
2. Clarify that QsciLexerCrka lives inline in `editor_page.hpp`/`editor_page.cpp` (acceptable but less clean separation, and inconsistent with the plan's own pattern of "each major widget gets its own .hpp/.cpp pair")

### W-02 (Cycle 2): Label scanning sync/async mismatch

**File:** Plan Task 3 (line 168) vs Risk Register (line 580)
**Severity: WARNING**

Task 3 says: "On project load and file save, scan ALL `.crka` files for `label <name>` patterns and build a `std::unordered_map<std::string, SourceLocation>`."

The Risk Register entry says: "Large projects slow down label scanning" → "Scan async on project load, cache results in memory."

The task description does not specify async scanning. The implementation as written would block the UI thread on project load and on every file save. For large projects (hundreds of .crka files), this could cause noticeable freezes.

**Fix:** Update Task 3 to explicitly say "Scan asynchronously using `QtConcurrent::run` or a `QThread`" and note the cache-invalidation strategy (incremental update on save, full rescan on project load).

### W-03 (Cycle 2): Unsaved changes dialog on tab close

**File:** Plan Task 2 (lines 138-140)
**Severity: WARNING**

The plan mentions "File modified indicator (asterisk on tab title)" and "Close tab button" but does not mention what happens when a tab with unsaved changes is closed. Standard editor UX requires a "Save changes?" dialog (with Save/Discard/Cancel options).

Without this, users can accidentally lose edits by clicking the close button. The modified indicator is a prerequisite but is not sufficient — the confirm dialog is the safety net.

**Fix:** Add to Task 2: "On tab close with unsaved changes, show a QMessageBox confirmation dialog: 'Save changes to [filename]?' with Save/Discard/Cancel options."

### W-04 (Cycle 2): Asset browser no manual refresh

**File:** Plan Task 7 (lines 361-362)
**Severity: WARNING**

The plan says the directory tree should "Refresh on project load and when the page becomes visible." If the user adds or removes assets via file manager (or via the editor's file-tree "New Script" action), they won't see changes until navigating away and back.

This is a UX friction point. A manual refresh button (or a QFileSystemWatcher for automatic refresh) is standard in asset browsers.

**Fix:** Either:
1. Add a "Refresh ↻" button in the asset browser toolbar, OR
2. Use `QFileSystemWatcher` on the `assets/` directory to trigger automatic refresh on file system changes

### IN-01 (Cycle 2): Future-proof keywords in highlighter may mislead

**File:** Plan Task 2 (line 127)
**Severity: INFO**

The plan lists `scene_graph`, `transition`, `camera`, `layer` as keywords to highlight under "Future-proof." These tokens are not currently valid .crka keywords. Highlighting them as language keywords may mislead users into thinking they are supported features.

**Fix:** Either:
1. Remove future-proof keywords from the highlighter and add them when the corresponding language features land, OR
2. Add a comment in the code noting they are reserved placeholders and not functional

### IN-02 (Cycle 2): `--compile-only` init cost ambiguity

**File:** Plan Task 6 (line 312)
**Severity: INFO**

The plan states: "The `--compile-only` flag goes further by skipping all game state init beyond what's needed for compilation." However, the implementation approach described ("check after the existing compile step") implies engine + SDL init happens before the check. The plan doesn't clarify whether the `--compile-only` path will:
- (a) Init everything, compile, then early-exit (simple but wasteful), or
- (b) Skip SDL/renderer/audio init and only init Lua + compiler (efficient but requires refactoring)

Both approaches are valid, but the plan should be explicit about which is intended, as approach (b) requires nontrivial changes to `runner/main.cpp`'s initialization sequence.

**Fix:** Clarify which approach is intended. If (a), adjust the wording. If (b), add a note about the refactoring needed (e.g., extracting compile from InitGame).

---

## Summary Across All Dimensions

| Dimension | Verdict | Severity |
|-----------|---------|----------|
| H-01 resolved? | `--compile-only` child process | ✅ RESOLVED |
| H-02 resolved? | `--script` child process | ✅ RESOLVED |
| H-03 resolved? | QPlainTextEdit fallback | ✅ RESOLVED |
| H-04 resolved? | Qt6::Multimedia in CMake | ✅ RESOLVED |
| New HIGH concerns | None found | ✅ CLEAN |
| Plan completeness | Lacks QsciLexerCrka file allocation | ⚠️ W-01 |
| Async correctness | Label scanning sync/async ambiguity | ⚠️ W-02 |
| UX safety | No save-on-close dialog | ⚠️ W-03 |
| UX completeness | No asset browser refresh | ⚠️ W-04 |
| Code quality | Future-proof keyword misuse | ℹ️ IN-01 |
| Technical clarity | `--compile-only` init path unclear | ℹ️ IN-02 |

## Files Reviewed

The following plan document was reviewed for Cycle 2:

- `.planning/phases/10-launcher-ide-core-script-editor-project-dashboard-and-asset-/10-01-PLAN.md`
- `.planning/phases/10-launcher-ide-core-script-editor-project-dashboard-and-asset-/10-CONTEXT.md` (for decision traceability)
- `.planning/phases/10-launcher-ide-core-script-editor-project-dashboard-and-asset-/10-REVIEWS.md` (Cycle 1 findings)

Existing source code cross-referenced for architectural validation:
- `launcher/main.cpp` (doLaunch pattern, findGameRunner, sidebar, content stack)
- `launcher/CMakeLists.txt` (current build config)
- `launcher/project_manager.hpp` (current API surface)
- `launcher/templates.hpp` (current template structure)
- `runner/main.cpp` (current flag handling, init sequence, --script and --headless paths)

---

## Overall Assessment

**The updated plan resolves all 4 previous HIGH concerns cleanly.**

H-01 and H-02: Both now correctly use child-process spawning through `CerekaGame` rather than direct engine library linking. The reuse of the existing `doLaunch` infrastructure (fork/exec, pipe capture) is sound and avoids reinventing the wheel.

H-03: The QScintilla fallback approach is well-designed — conditional compilation with a full QPlainTextEdit/CrkaHighlighter fallback means zero build friction on platforms where QScintilla isn't available. The user-facing behavior is identical across both paths.

H-04: Qt6::Multimedia is properly declared as a REQUIRED CMake dependency and used by the asset browser's audio preview feature.

**No new HIGH concerns are introduced.** The 4 WARNING-level issues (missing QsciLexerCrka file, sync/async ambiguity, unsaved changes dialog, missing refresh button) are quality improvements that should be fixed before implementation, but none represent a correctness bug, security vulnerability, or architecture flaw.

---

## CYCLE_SUMMARY: current_high=0

## Current HIGH Concerns
None.
