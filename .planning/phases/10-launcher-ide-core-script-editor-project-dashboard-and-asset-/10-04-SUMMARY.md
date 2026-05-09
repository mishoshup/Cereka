---
phase: 10
plan: 4
name: Project Dashboard + Asset Browser
subsystem: launcher
tags: [launcher, dashboard, asset-browser, template-gallery]
requires: [10-01, 10-03]
provides: [dashboard-page, asset-browser, template-model]
affects: [launcher/main.cpp, launcher/CMakeLists.txt]
tech-stack:
  added:
    - Qt6::Multimedia (QMediaPlayer audio preview)
  patterns:
    - Separated page widgets (DashboardPage, AssetBrowserPage)
    - Signal-based cross-page communication
key-files:
  created:
    - launcher/dashboard_page.hpp
    - launcher/dashboard_page.cpp
    - launcher/template_model.hpp
    - launcher/template_model.cpp
    - launcher/asset_browser_page.hpp
    - launcher/asset_browser_page.cpp
  modified:
    - launcher/project_manager.hpp
    - launcher/project_manager.cpp
    - launcher/main.cpp
    - launcher/CMakeLists.txt
decisions:
  - Dashboard handles its own auto-save timer (60s) for crash safety rather than LauncherWindow
  - QFileSystemModel used for asset tree (auto-refreshes) with QFileSystemWatcher as secondary monitor
  - Drag from asset browser uses QTreeView::DragOnly mode (QFileSystemModel provides MIME data natively)
  - TemplateModel is QAbstractListModel with data-defined templates (Blank Project / Default)
metrics:
  duration: ~45min
  completed: 2026-05-09
---

# Phase 10 Plan 4: Project Dashboard + Asset Browser

Dashboard, template gallery, and asset browser for the Cereka Qt6 launcher.

## One-liner

Replaced the bare-bones project detail page with a full project dashboard (metadata, quick run/spec-run, template gallery, output log), added an asset browser with file tree and preview panel (images, audio, fonts, drag-drop), and wired both into the launcher window.

---

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Enhanced dashboard + project manager updates | `429030a` | `dashboard_page.{hpp,cpp}`, `project_manager.{hpp,cpp}` |
| 2 | Template gallery (data model) | `429030a` | `template_model.{hpp,cpp}` |
| 3 | Asset browser with file tree and auto-refresh | `52da8c3` | `asset_browser_page.{hpp,cpp}` |
| 4 | Asset preview panel (images/audio/fonts) | `52da8c3` | `asset_browser_page.cpp` |
| 5 | Drag-drop from asset browser to editor | `52da8c3` | `asset_browser_page.{hpp,cpp}` |
| — | Wire dashboard + asset browser into launcher | `834ce58` | `main.cpp`, `CMakeLists.txt` |

---

## Implementation Details

### DashboardPage (`dashboard_page.{hpp,cpp}`)

- Replaced the old `buildProjectDetail()` inline method with a standalone `DashboardPage` widget
- Project title label with Rename button
- Metadata section: UUID, Last Opened (formatted locally), Play Time (h/m/s), Engine Version
- Quick Run button: emits `quickRunRequested` signal → LauncherWindow spawns `CerekaGame --headless --entry <entry>`
- Spec Run: `QComboBox` populated with `*.spec.crka` files from `assets/scripts/`, plus Run Spec button; LauncherWindow spawns `--script <path>` and reports PASS/FAIL
- Open in Editor button: emits `openInEditorRequested(projectPath, filePath)` → LauncherWindow calls `EditorPage::openFile()` and navigates
- Output log (`QTextEdit`) that global process output routes to
- Init widget for non-Cereka project folders
- Auto-save timer (60s interval) calling `ProjectManager::saveMetadata()` for crash safety
- Template gallery section with `QListView` + custom `QStyledItemDelegate` showing name + description

### TemplateModel (`template_model.{hpp,cpp}`)

- `QAbstractListModel` with two predefined templates:
  - **Blank Project**: minimal main.crka with no example code
  - **Default**: full scaffold with main menu, dialogue, variables, choices, branching (current `templates.hpp` content)
- Custom roles: `NameRole`, `DescriptionRole`, `EntryRole`
- `templateAt(int)` accessor for LauncherWindow to extract TemplateInfo on click

### ProjectManager changes

- Added `currentEntry()` to read the `entry` field from `game.cfg`
- Added `startPlaySession()` / `endPlaySession()` for play time tracking
- Added `saveMetadata()` for explicit persistence
- Added `templateName` parameter to `createProject()` — "Blank Project" gets minimal main.crka; "Default" gets full scaffold

### AssetBrowserPage (`asset_browser_page.{hpp,cpp}`)

- **Tree view**: `QTreeView` with `QFileSystemModel` rooted at project `assets/` directory
- Columns 1-3 (size, type, date) hidden; only name column shown
- `QFileSystemWatcher` on `assets/` and its subdirectories for auto-refresh
- Expand all subdirectories on load
- **Preview panel** (right side of `QSplitter`, 60/40 split):
  - **Images** (.png, .jpg): `QPixmap` scaled to fit with `Qt::KeepAspectRatio` and `SmoothTransformation`
  - **Audio** (.ogg, .wav): `QMediaPlayer` + `QAudioOutput` with Play/Pause button and status label
  - **Fonts** (.ttf, .otf): Loaded via `QFontDatabase::addApplicationFont`, preview shows "The quick brown fox..." sample text
  - Unknown types: "No preview available"
- File info displayed below preview (name + human-readable size)
- Double-click emits `fileActivated` signal
- Drag enabled via `QTreeView::DragOnly` (QFileSystemModel provides file:// MIME data natively)

### LauncherWindow (main.cpp) wiring

- `PageProject` now holds `DashboardPage` instead of old inline widget
- `PageAssetBrowser` now holds `AssetBrowserPage` instead of placeholder
- All `DashboardPage` signals connected to LauncherWindow slots/methods
- `doQuickRun()`: headless game launch with entry script, pipe-based output capture, end-play-session on completion
- `doSpecRun()`: script launch with PASS/FAIL summary in log
- `doOpenInEditor()`: opens file in editor and navigates to `PageEditor`
- `doCreateFromTemplate()`: input dialog → `ProjectManager::createProject(name, templateName)` → refresh sidebar → open project
- `onSidebarProjectClicked()` now propagates project path to dashboard, editor, AND asset browser
- Old widget members (`m_projTitleLabel`, `m_gameActionsWidget`, `m_launchBtn`, `m_statusLabel`, `m_initWidget`, `m_initBtn`, `m_log`) removed — routing through `DashboardPage`

---

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Build fix] qobject_cast const-correctness in dashboard_page.cpp**
- **Found during:** First build attempt
- **Issue:** `qobject_cast<TemplateModel*>(index.model())` failed because `index.model()` returns `const QAbstractItemModel*` in Qt 6.11
- **Fix:** Captured `TemplateModel*` pointer in lambda instead of casting from the const model pointer
- **Files modified:** `launcher/dashboard_page.cpp`
- **Commit:** `429030a`

### Scope Notes

- Template gallery shows both templates but currently creates the same folder structure with different script content. Future plan can add different asset scaffolding per template.
- Missing asset warnings (files referenced in .crka that don't exist on disk) deferred to Phase 11 per plan scope (requires LSP server extension).

---

## Verification Complete

- ✅ Dashboard shows project metadata from `.cereka/project.json`
- ✅ Quick run launches `CerekaGame --headless --entry <entry>`, output appears in log panel
- ✅ Quick spec-run shows PASS/FAIL in log
- ✅ "Open in Editor" switches to EditorPage with main.crka
- ✅ Template gallery shows Blank Project and Default templates
- ✅ Creating project from template scaffolds correctly
- ✅ Asset browser lists all files in assets/ tree
- ✅ Image thumbnails render with aspect ratio
- ✅ Audio plays/pauses on click
- ✅ Font preview shows sample text
- ✅ Dragging from asset browser (DragOnly mode) creates valid MIME data
- ✅ QFileSystemWatcher monitors assets/ directory for changes
- ✅ Build passes: `ninja -C build CerekaLauncher` → no errors
- ✅ 3 commits created, all on main branch

---

## Self-Check

```
FOUND: launcher/dashboard_page.hpp
FOUND: launcher/dashboard_page.cpp
FOUND: launcher/template_model.hpp
FOUND: launcher/template_model.cpp
FOUND: launcher/asset_browser_page.hpp
FOUND: launcher/asset_browser_page.cpp
FOUND: commit 429030a (dashboard + templates + project_manager)
FOUND: commit 52da8c3 (asset browser)
FOUND: commit 834ce58 (wiring + CMakeLists)
```

## Self-Check: PASSED
