# Phase 11: Launcher IDE Pro - Context

**Gathered:** 2026-05-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Build the advanced IDE tiers on top of Phase 10's foundation: a visual dialogue node editor, WYSIWYG UI theme designer, script debugger/profiler, in-editor game preview, plugin system, and git integration.

All panels plug into the same Qt6 LauncherWindow infrastructure from Phase 10.

</domain>

<decisions>
## Implementation Decisions

### D-01: Visual Dialogue Editor — QGraphicsView node graph
- Nodes represent `.crka` labels: label header + say/narrate text + menu buttons as output ports
- Drag to connect nodes: button target → target label node
- Under the hood: edits the actual .crka file, rearranges labels in text
- Live sync: edit source in text editor → node graph updates, and vice versa
- Zoom/pan/minimap via QGraphicsView built-in
- Auto-layout: arrange nodes in a top-to-bottom flow

### D-02: Timeline View — Linear script view
- All `narrate`/`say` lines in chronological order, like a film script
- Filter by character: show only Alice's lines
- Drag to reorder scenes (rearranges labels in .crka)
- Click line → highlights corresponding node in node graph

### D-03: Theme Designer — WYSIWYG split view
- Left: code panel showing `ui.crka` with live preview
- Right: game dialogue rendered using current theme
- Color pickers for each property (textbox bg, text color, button color, etc.)
- Font selector with live preview
- Export: save changes to `ui.crka`
- Preset browser: apply base themes (dark, light, cyberpunk, etc.)

### D-04: Debugger/Profiler — Engine IPC
- Launch CerekaGame in debug mode: `--debug --port 9876`
- IPC over local socket:
  - Set breakpoints on labels
  - Step over/step into instructions
  - Read variable state
  - Read call stack
- Profiler: per-instruction timing displayed as flame chart
- Save file viewer/inspector: decode and display save JSON

### D-05: In-Editor Preview — Embedded CerekaGame
- QWidget that embeds CerekaGame as a child process
- Render-to-texture approach or window embedding (platform-specific)
- Preview from current label: "Run from here" button
- See actual game rendering without launching a separate window

### D-06: Plugin System — Dynamic loading
- Plugins loaded from `~/.cereka/plugins/` directory
- Plugin API: register new panels, new importers, new export formats
- Example plugin: itch.io uploader
- Plugin manifest: `plugin.json` with name, version, entry point

### D-07: Git Integration — libgit2 or CLI wrapping
- Stage/commit .crka files from launcher
- Diff viewer for .crka files (line-level syntax highlighting diff)
- Project sharing: "Clone from URL" → git clone → open in launcher
- Branch visualization: simple branch graph

### D-08: Build Order
1. Visual dialogue editor (highest value, most distinctive)
2. Theme designer (unlocks custom UI without C++)
3. In-editor preview (tightens feedback loop)
4. Debugger/profiler (power users)
5. Plugin system (ecosystem)
6. Git integration (team collaboration)

### the agent's Discretion
- Exact node graph visual style
- IPC protocol design (JSON lines over TCP vs Unix socket)
- Plugin API design
- Theme preset designs
- Diff viewer implementation

</decisions>

<canonical_refs>
### Existing Launcher (Phase 10 foundation)
- `launcher/main.cpp` — LauncherWindow with content stack (after Phase 10 additions)
- `launcher/editor_page.hpp` — Script editor widget (Phase 10)
- `launcher/asset_browser_page.hpp` — Asset browser (Phase 10)

### Engine
- `src/compiler/cereka_instruction.hpp` — Op enum for debugger
- `src/state/cereka_states.hpp` — state machine for debugger IPC
- `src/cereka_script_interpreter.hpp` — variable state for debugger
- `include/Cereka/Cereka.hpp` — public API

### Node Graph
- Qt6 QGraphicsView framework — built-in, no external dependency
</canonical_refs>

<code_context>
### Reusable Assets
- **QGraphicsView** — Qt6's built-in scene graph framework for the node editor. No external dependency.
- **Phase 10 EditorPage** — script editor that syncs with node graph (bidirectional)
- **Phase 10 AssetBrowser** — provides drag-drop targets for theme designer icons

### Established Patterns
- **QStackedWidget pages** — new panels plug into existing infrastructure
- **Child process spawning** — ProjectManager::doLaunch pattern reused for debugger IPC
- **Dark Theme** — Theme::* color constants reused for all new panels

### Integration Points
- Visual editor modifies .crka files → editor picks up changes (file watcher)
- Theme designer outputs ui.crka → game picks up on next launch
- Debugger connects to CerekaGame process via socket → engine needs --debug flag
</code_context>

<specifics>
- Node graph is the flagship feature — make it polished
- Theme designer should feel like VS Code's color theme editor
- Debugger IPC should be JSON-based, not binary
</specifics>

<deferred>
- Steam integration → future milestone
- Web export → future milestone
- Mobile build pipeline → future milestone
</deferred>

---

*Phase: 11-Launcher IDE Pro*
*Context gathered: 2026-05-08*
