---
phase: 10
plan: 3
subsystem: launcher
tags: [qt6, editor, tab-bar, split-pane, find-replace, lsp, outline]

requires:
  - phase: 9
    provides: launcher infrastructure
  - phase: 10-02
    provides: CodeEditor widget, LspClient, syntax highlighter

provides:
  - Tab bar with multi-file editing, context menu, and drag-to-split
  - Horizontal split-pane with same-file and two-file modes
  - Find/replace panel with match highlighting and regex support
  - Document outline panel listing label definitions via LSP

affects: [10-04]

tech-stack:
  added: []
  patterns:
    - Panel-based editor architecture (EditorPanel struct with per-panel tab bar + editor stack)
    - Find/Replace panel with QTextEdit::ExtraSelections for match highlighting
    - LSP documentSymbol integration for document outline

key-files:
  created:
    - launcher/editor_tab_bar.hpp — Custom QTabBar with context menu and drag-to-split
    - launcher/editor_tab_bar.cpp — Tab bar implementation
    - launcher/find_panel.hpp — Find/replace panel widget
    - launcher/find_panel.cpp — Find/replace with search-as-you-type and match highlighting
    - launcher/outline_panel.hpp — Document outline panel widget
    - launcher/outline_panel.cpp — Label symbol listing from LSP
  modified:
    - launcher/editor_page.hpp — Multi-panel architecture, find/outline integration
    - launcher/editor_page.cpp — Split-pane operations, focus tracking, shortcuts
    - launcher/code_editor.hpp — setSearchHighlights API
    - launcher/code_editor.cpp — Search highlight merging in updateExtraSelections
    - launcher/CMakeLists.txt — Added find_panel.cpp and outline_panel.cpp

key-decisions:
  - "Tab bar uses QTabBar directly (not QTabWidget) for custom context menu and drag handling"
  - "Split-pane LSP isolation uses pane suffix in URI fragment (pane=0, pane=1)"
  - "Only active pane sends didChange; inactive pane opens read-only to prevent duplicate notifications"
  - "Search highlights merged into CodeEditor's existing extra selections via setSearchHighlights API"
  - "Outline panel uses LSP documentSymbol with hierarchical tree display"

requirements-completed: []

# Metrics
duration: 45min
completed: 2026-05-09
---

# Phase 10 Plan 3: Editor UX Features — Tabs, Split, Find Panel, Outline

**Tab bar with multi-file editing, horizontal split-pane, full-featured find/replace panel with match highlighting, and document outline panel from LSP documentSymbol**

## Performance

- **Duration:** 45 min
- **Started:** 2026-05-09T10:00:00Z
- **Completed:** 2026-05-09T10:45:00Z
- **Tasks:** 4
- **Files modified:** 11

## Accomplishments

- **Tab bar** — Custom QTabBar subclass with right-click context menu (Close, Close Others, Close All, Copy Path, Split Right), middle-click close, drag-to-reorder, unsaved file indicator (●), and drag-to-split (VS Code style)
- **Split-pane** — QSplitter-based horizontal split supporting two different files or same file in both panes with independent scrolling. Focus tracking via eventFilter updates read-only state. LSP isolation uses pane suffix on URIs. Ctrl+\ toggles split/merge. Tab bar context menu shows Merge Split when in split mode.
- **Find/Replace panel** — Search-as-you-type with match highlighting via QTextEdit::ExtraSelections. Results list with line numbers and context, click to navigate. Case-sensitive, whole-word, and regex toggles. Replace single or replace all. Ctrl+F opens find, Ctrl+H opens find+replace, F3/Shift+F3 navigates matches, ESC closes.
- **Document outline panel** — Right-side panel listing all label definitions via LSP `textDocument/documentSymbol` response. Hierarchical tree display for nested labels. Click to navigate to label line in editor. Auto-refresh on tab change, autosave, and LSP diagnostics.

## Task Commits

Each task was committed atomically:

1. **Task 1: Tab bar** — `f860969` (feat: tab bar with multi-file editing)
2. **Task 2: Split-pane** — `aa21972` (feat: split-pane support)
3. **Task 3: Find/Replace panel** — `bdac84a` (feat: find/replace panel)
4. **Task 4: Document outline panel** — `ea599bc` (feat: document outline panel)
5. **Task 2 fix (deviation):** — `f0e8278` (fix: removeWidget Qt6 compat)

**Plan metadata:** `pending` (to be committed with this summary)

## Files Created/Modified

- `launcher/editor_tab_bar.hpp` — Custom tab bar with context menu, drag detection, merge support
- `launcher/editor_tab_bar.cpp` — Tab bar styling, context menu actions, drag-to-split mouse handling
- `launcher/editor_page.hpp` — Refactored to multi-panel architecture with EditorPanel struct
- `launcher/editor_page.cpp` — Split-pane operations, focus tracking, find/outline integration
- `launcher/code_editor.hpp` — Added `setSearchHighlights()` API
- `launcher/code_editor.cpp` — Search highlights merged into `updateExtraSelections()`
- `launcher/find_panel.hpp` — Find/replace panel widget declaration
- `launcher/find_panel.cpp` — Search logic, match highlighting, replace operations
- `launcher/outline_panel.hpp` — Document outline panel widget declaration
- `launcher/outline_panel.cpp` — LSP documentSymbol integration and navigation
- `launcher/CMakeLists.txt` — Added find_panel.cpp and outline_panel.cpp sources

## Decisions Made

- **Panel architecture:** EditorPage uses a fixed-size `EditorPanel m_panels[2]` array with `m_panelCount` tracking to avoid dynamic allocation overhead while supporting at most 2 panes
- **LSP isolation:** Same-file split uses pane-specific URIs (`file:///foo.crka#pane=0`, `file:///foo.crka#pane=1`) so the LSP server sees them as separate documents
- **Search highlights:** FindPanel calls `CodeEditor::setSearchHighlights()` which merges into the editor's existing `updateExtraSelections()` alongside diagnostic squiggles and bracket matching
- **Outline refresh:** Triggered on tab change, autosave, and LSP diagnostics to stay in sync with document content

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Replaced QSplitter::removeWidget with hide() for Qt6 compat**
- **Found during:** Task 2 (building split-pane changes)
- **Issue:** `QSplitter::removeWidget()` was removed in Qt 6.6+ — caused compile error on macOS with Qt 6.11.0
- **Fix:** Replaced with `p.container->hide()` followed by `p.container->deleteLater()`, which is the correct Qt6 pattern
- **Files modified:** `launcher/editor_page.cpp`
- **Verification:** Build succeeds
- **Committed in:** `f0e8278` (separate fix commit)

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Minor — single-line fix for Qt6 API change. No scope creep.

## Issues Encountered

- `QSplitter::removeWidget` removed in Qt 6.11.0 — replaced with `hide()` as documented in Qt6 migration guide. This was a pre-existing issue in the uncommitted split-pane implementation.

## User Setup Required

None — no external service configuration required.

## Self-Check: PASSED

All 6 created files exist on disk. All 5 plan commits found in git history.

## Next Phase Readiness

- All core editor UX features (tabs, split, find/replace, outline) are ready
- Ready for Plan 4: completion provider, hover tooltips, and editor preferences
