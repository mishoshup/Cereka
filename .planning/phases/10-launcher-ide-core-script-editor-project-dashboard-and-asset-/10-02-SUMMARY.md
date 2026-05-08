---
phase: 10-launcher-ide-core
plan: 2
subsystem: ui
tags: [qt6, lsp, code-editor, syntax-highlighter, json-rpc]

# Dependency graph
requires:
  - phase: 10-01
    provides: Launcher IDE infrastructure (sidebar, project manager, theme)
provides:
  - CodeEditor widget (QPlainTextEdit extension)
  - LSP client (JSON-RPC over child process)
  - CrkaHighlighter (.crka syntax coloring)
  - EditorPage (file tree + tabbed editor + LSP integration)
affects: [10-03, 10-04]

# Tech tracking
tech-stack:
  added: [QPlainTextEdit, QSyntaxHighlighter, QProcess, QCompleter, QTabWidget, QSplitter, QFileSystemWatcher, JSON-RPC]
  patterns: [LSP client-server architecture, autosave-debounce, cross-file navigation with URI mapping]

key-files:
  created:
    - launcher/lsp_client.hpp/cpp
    - launcher/code_editor.hpp/cpp
    - launcher/syntax_highlighter.hpp/cpp
    - launcher/editor_page.hpp/cpp
  modified:
    - launcher/main.cpp
    - launcher/CMakeLists.txt

key-decisions:
  - "LspClient uses std::function callbacks for request/response pattern (not QPromise)"
  - "CodeEditor holds optional LspClient pointer for direct LSP interaction (gutter, Ctrl+click, hover, completions)"
  - "EditorPage manages tab lifecycle with separate CodeEditor+CrkaHighlighter per tab"
  - "Autosave uses 1.5s debounce timer + incremental document version for LSP didChange"
  - "QLabel status bar for LSP connection state (connected/unavailable/not found)"

patterns-established:
  - "File → Tab mapping: URI ↔ local path via QUrl conversion"
  - "Diagnostics from LSP → CodeEditor::setDiagnostics → extra selections with WaveUnderline"
  - "LSP request chaining: goToDefinition response opens cross-file tabs with cursor navigation"
  - "Graceful degradation: editor works without LSP (keyword-only completion, no squiggles)"

requirements-completed: []

# Metrics
duration: 35min
completed: 2026-05-09
---

# Phase 10 Plan 2: CodeEditor Widget + LSP Client

**Custom QPlainTextEdit code editor with line numbers, bracket matching, error squiggles, indentation guides, LSP client with JSON-RPC over child process, CrkaHighlighter for .crka syntax coloring, and EditorPage with file tree and tabbed editing**

## Performance

- **Duration:** 35 min
- **Started:** 2026-05-09T07:13:29+08:00
- **Completed:** 2026-05-09T07:22:59+08:00
- **Tasks:** 4
- **Files created:** 8
- **Files modified:** 2

## Accomplishments

- **LspClient**: Full LSP client managing a child QProcess with JSON-RPC frame parser (Content-Length header), request ID tracking with callback map, complete LSP lifecycle (initialize/initialized handshake, didOpen/didChange/didClose), request methods (completion, definition, hover, references, documentSymbol), notification handlers (publishDiagnostics, showMessage), and exponential backoff reconnection on crash (1s, 2s, 4s, max 30s)
- **CodeEditor**: QPlainTextEdit extension with line number gutter, fold markers, error squiggles via QTextCharFormat::WaveUnderline from LSP diagnostics, bracket matching with highlighted background, indentation guides at each indent level, Ctrl+click go-to-definition via LSP, QCompleter with .crka keyword fallback model + Ctrl+Space trigger, hover debounce timer for LSP hover requests, dark theme matching launcher Theme::* constants
- **CrkaHighlighter**: QSyntaxHighlighter for .crka script language — keywords in gold bold, strings in ice-blue, comments (;) in italic dim gray, numbers in green, label names in gold bold, variables ({name}) in light purple (overrides inside strings), operators in gold. String region detection prevents keyword/operator rules matching inside quotes
- **EditorPage**: Replaces placeholder with full editor page. QSplitter layout with file tree (200px left, .crka files from project assets/scripts/) and tabbed editor (right, with closeable tabs). LSP lifecycle on project load/clear. Diagnostics → error squiggles. Cross-file navigation on go-to-definition (opens file, scrolls to line). Autosave with 1.5s debounce + LSP didChange. Ctrl+Space completion. Status bar for LSP state

## Task Commits

Each task was committed atomically:

1. **Task 1: LspClient** - `e3ff3b2` (feat)
2. **Task 2: CodeEditor widget** - `4779440` (feat)
3. **Task 3: CrkaHighlighter** - `84868a1` (feat)
4. **Task 4: EditorPage + wiring** - `0eb22cf` (feat)

**Plan metadata:** (committed in the final docs commit below)

## Files Created/Modified

- `launcher/lsp_client.hpp` - LspClient class declaration with JSON-RPC methods, signals, and callback types
- `launcher/lsp_client.cpp` - Full LSP client implementation: frame parser, request/response handling, binary resolution, reconnection
- `launcher/code_editor.hpp` - CodeEditor class declaration (QPlainTextEdit extension)
- `launcher/code_editor.cpp` - CodeEditor implementation: gutter painting, bracket matching, squiggles, indentation guides, completer, hover
- `launcher/syntax_highlighter.hpp` - CrkaHighlighter class declaration (QSyntaxHighlighter)
- `launcher/syntax_highlighter.cpp` - Syntax highlighting rules for .crka language (keywords, strings, comments, numbers, labels, variables, operators)
- `launcher/editor_page.hpp` - EditorPage class declaration with tab tracking and LSP integration
- `launcher/editor_page.cpp` - EditorPage implementation: file tree, tabs, autosave, LSP lifecycle, definition navigation
- `launcher/main.cpp` - Replaced editor placeholder with EditorPage, wired project selection to update editor
- `launcher/CMakeLists.txt` - Added 4 new source files to launcher target

## Decisions Made

- **Callback-based LSP requests**: Used `std::function<void(QJsonObject)>` callbacks with pending request map (keyed by monotonically incrementing ID) instead of QPromise. Simpler to implement and sufficient for the current use case. Can migrate to QFuture/QPromise in a future refactor
- **CodeEditor with optional LspClient pointer**: The editor holds a nullable `m_lspClient` pointer. When set, it enables LSP-dependent features (Ctrl+click definition, hover, completions). When null, the editor degrades gracefully with keyword-only completion and no squiggles — enables standalone testing
- **Per-tab CodeEditor+CrkaHighlighter**: Each editor tab creates its own CodeEditor and CrkaHighlighter pair. This avoids complexity of swapping documents and preserves per-file editor state (scroll position, selection)
- **Autosave with LSP sync**: 1.5s debounce timer triggers disk write + LSP didChange with incremental version number. Documents opened fresh send didOpen with full content
- **QFileSystemWatcher for file tree**: Watches .crka files for external changes. File tree repopulates when files are added/removed

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- **Qt 6.11 vs 6.8.3 QRegularExpression enum**: The development environment has Qt 6.11.0 installed (not the 6.8.3 in CMakeLists.txt). Used fully-qualified `QRegularExpression::PatternOption::CaseInsensitiveOption` instead of shorthand
- **QFileSystemWatcher API**: `addFile()` does not exist in Qt 6 (method is `addPath()`). Fixed during build verification

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- CodeEditor, LSP client, syntax highlighter, and EditorPage are complete and compiling
- LSP server binary (`cereka-lsp`) availability determines full-featured vs degraded mode — resolved at runtime via `LspClient::resolveBinary()`
- Ready for Plan 10-03 (project dashboard widgets, build status panel) or 10-04 (asset browser)
- File tree shows .crka files via recursive_directory_iterator — no filtering yet (all .crka files shown flat; subdirectory structure could be added)

---

*Phase: 10-launcher-ide-core*
*Completed: 2026-05-09*
