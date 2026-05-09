# Phase 10: Launcher IDE Core - Context

**Gathered:** 2026-05-08 (updated)
**Status:** Ready for planning

<domain>
## Phase Boundary

Build an integrated development environment (IDE) into the existing Qt6 CerekaLauncher. Delivers the foundation tiers of a full game editor — script editing powered by tree-sitter + LSP, project management, and asset browsing. All built on top of the existing Qt6 codebase.

</domain>

<decisions>
## Implementation Decisions

### D-01: Text Editor — Custom QPlainTextEdit + LSP Client
- **Not QScintilla.** QScintilla is 25-year-old wrapping of Scintilla. Modern editors (VS Code, Neovim, Zed) use tree-sitter + LSP instead.
- **Custom CodeEditor widget** subclassing QPlainTextEdit with:
  - Line number gutter (custom paint)
  - Code folding markers in gutter (from tree-sitter fold queries)
  - Bracket matching highlight (via QSyntaxHighlighter)
  - Indentation guides (vertical lines at each indent level, custom paint)
  - Error squiggles under diagnostics (QSyntaxHighlighter to paint wavy underline)
- **No minimap** — deferred to future phase
- **Tab bar** at top for multiple open files. Click to switch, middle-click to close, drag to reorder.

### D-02: Editor Features — Full, like LazyVim/VS Code
- **Auto-complete**: LSP `textDocument/completion` → QCompleter popup with keyword/label/variable suggestions. Context-aware: `goto ` shows only labels.
- **Go-to-definition**: Ctrl+click on label reference → opens target file at label definition line
- **Find/Replace Panel**: Full-featured dockable panel, LazyVim-style:
  - Search as you type, highlight all matches in editor
  - Results list with context lines
  - Replace one / replace all
  - Regex, case-sensitive, whole-word options
  - F3 / Shift+F3 to navigate between results
- **Split-pane**: Both modes supported:
  - Two different files side-by-side
  - Same file in both panes (long file split view, like VS Code Ctrl+\)
- **Document outline**: Side panel listing all `label` definitions in current file. Click to jump.

### D-03: Syntax Engine — tree-sitter-cereka (separate repository)
- **tree-sitter-cereka** is a standalone repository at the same level as `cereka/` and `cereka-game/`:
  ```
  ~/personal/dev/
    cereka/               ← engine (this repo)
    cereka-game/          ← Whiteout
    tree-sitter-cereka/   ← grammar + LSP server
  ```
- Grammar defines the full `.crka` syntax as a tree-sitter grammar (grammar.js)
- Provides: syntax tree, code folding queries, syntax highlighting queries
- **Phase 10 includes updating the grammar** to support all current .crka ops (checkpoint, scene_graph, bg_fade, else-if from Phase 10 planning, etc.)
- Deployed as WASM for the LSP server, and as C source for potential native embedding

### D-04: LSP Server — Node.js, standalone via SEA
- **server.js** in `tree-sitter-cereka/lsp/` provides the Language Server Protocol:
  - `textDocument/diagnostic` — compile errors inline (red squiggles)
  - `textDocument/completion` — auto-complete
  - `textDocument/definition` — go-to-definition for labels
  - `textDocument/documentSymbol` — document outline (label list)
  - `textDocument/hover` — tooltip on hover
  - `textDocument/references` — find all references to a label
  - `workspace/symbol` — search labels across all project files
- **Standalone binary**: Compiled via Node SEA (Node.js Single Executable Application) — no Node.js runtime needed by end users
- **LSP lifecycle**: Launched when launcher starts, killed when launcher exits. Auto-restart on crash (LazyVim-style).
- **Release**: Separate release from engine. Published independently (like VS Code extensions), but bundled with complete launcher releases for convenience.

### D-05: Project Dashboard — Enhanced sidebar
- **Metadata per project**: `.cereka/project.json` inside each project directory with:
  - UUID (user-defined, persists across renames)
  - Title, last opened timestamp, total play time
  - Engine version used
- **Template gallery**: Scaffold new project from templates. Ships with: Blank Project, Default (current templates). More templates deferred to Phase 11.
- **Quick run**: `--headless` button, output in log panel
- **Quick spec-run**: Select `.spec.crka` from dropdown, run via `CerekaGame --script`, show pass/fail in log panel

### D-06: Asset Browser — Tree view with thumbnails
- **Tree view** of `assets/` subdirectories (bg, characters, sounds, fonts, ui)
- **Auto-refresh** via `QFileSystemWatcher` — tree updates when files change on disk
- **Image thumbnails**: QPixmap scaled to fit
- **Audio preview**: Play/stop via Qt6 Multimedia
- **Drag-to-insert**: Drag asset path from tree → drops into editor at cursor
- **Missing asset warnings**: Cross-reference all `bg`, `char`, `bgm`, `sfx`, `image` paths used in .crka files against actual files in assets/

### D-07: Architecture — New panels in existing LauncherWindow
- QStackedWidget adds new pages: EditorPage, AssetBrowserPage
- Each page is a standalone widget (separate .hpp/.cpp)
- EditorPage: vertical split (file tree | tabbed editor | outline panel)
- AssetBrowserPage: horizontal split (tree | preview panel)
- Find panel: dockable at bottom, shared across editor tabs
- Log output panel: shared across all pages (bottom dock)
- New panels never link the Cereka engine — communication via child process (LSP) or file system

### D-08: Technology Choices
- **CodeEditor widget**: QPlainTextEdit + custom paint for gutter, folding, indent guides
- **Syntax tree**: tree-sitter-cereka (separate repo, WASM for LSP)
- **LSP protocol**: Standard JSON-RPC over stdin/stdout
- **Metadata storage**: `.cereka/project.json` with UUID per project
- **File watching**: QFileSystemWatcher for asset browser auto-refresh
- **Audio**: Qt6::Multimedia (QMediaPlayer)

### D-09: LSP Client in Launcher
- QPlainTextEdit subclass that:
  - Sends `textDocument/didOpen` / `didChange` / `didClose` to LSP server
  - Receives `textDocument/publishDiagnostics` → paints error squiggles + gutter markers
  - Sends `textDocument/completion` → populates QCompleter
  - Sends `textDocument/definition` → navigates to target file/line
  - Sends `textDocument/hover` → shows tooltip (QToolTip)
  - Sends `textDocument/references` → shows results in find panel

### the agent's Discretion
- Exact dark theme colors for editor syntax highlighting
- Tab bar position (top vs bottom)
- Split-pane shortcut keys
- Find panel layout details
- QCompleter popup styling
- UUID generation strategy
- File watcher debounce interval

</decisions>

<canonical_refs>
### Existing Launcher
- `launcher/main.cpp` — LauncherWindow, sidebar, project detail, packaging
- `launcher/project_manager.hpp` — ProjectManager API
- `launcher/config.hpp` — Config persistence
- `launcher/CMakeLists.txt` — Build system

### Editor Technology
- `dev/tree-sitter-cereka/` (sibling repo) — grammar + LSP server (cloned at build time or CI)
- `dev/tree-sitter-cereka/grammar.js` — tree-sitter grammar source
- `dev/tree-sitter-cereka/lsp/server.js` — LSP server
- `dev/tree-sitter-cereka/queries/` — highlighting + fold queries

### Engine (for reference, NOT linked)
- `scripts/cereka_compiler.lua` — compiler source (grammar references op keywords)
- `src/compiler/cereka_instruction.hpp` — Op enum
- `CLAUDE.md` — .crka language reference (grammar should match this)

### Codebase Context
- `.planning/codebase/ARCHITECTURE.md` — launcher architecture
- `.planning/codebase/STACK.md` — Qt6 version, build system
</canonical_refs>

<code_context>
### Reusable Assets
- **LauncherWindow** — existing widget infrastructure (sidebar, stacked pages, log panel, dark theme)
- **ProjectManager** — existing lifecycle (list, create, rename, init, launch, package)
- **QPlainTextEdit** — Qt6 built-in text rendering widget (no external editor library needed)
- **QSyntaxHighlighter** — Qt6 built-in syntax highlighting base class
- **QFileSystemWatcher** — Qt6 built-in file watching for auto-refresh
- **QMediaPlayer** — Qt6 Multimedia for audio preview

### Established Patterns
- **QStackedWidget pages** — new panels plug into existing infrastructure
- **Child process spawning** — LSP server launched via same pattern as CerekaGame
- **Dark Theme** — existing Theme::* color constants reused

### Integration Points
- LauncherWindow::m_contentStack — add EditorPage + AssetBrowserPage
- Sidebar — add navigation items for Editor and Asset Browser
- Child process: LSP server (Node SEA binary) — stdin/stdout JSON-RPC
- File system: `.cereka/project.json` per project
</code_context>

<specifics>
- Godot/Unity/Unreal-level editor but starting with SLC (Simple Lovable Complete)
- Build incrementally on existing Qt6 launcher code — not a rewrite
- tree-sitter + LSP is the modern stack — VS Code, Neovim, Zed, Helix all use it
- Node SEA produces a standalone binary — no external Node.js dependency
- tree-sitter-cereka is a separate repo, independently versioned and released
- "Like LazyVim" for find/replace panel, "like VS Code" for tab/split behavior
</specifics>

<deferred>
- Minimap → future phase
- Node graph visual editor → Phase 11
- Theme designer (WYSIWYG) → Phase 11
- Debugger/profiler → Phase 11
- Plugin system → Phase 11
- Git integration → Phase 11
- Tree-sitter grammar maintained independently, but Phase 10 includes an audit + update pass
</deferred>

---

*Phase: 10-Launcher IDE Core*
*Context gathered: 2026-05-08*
