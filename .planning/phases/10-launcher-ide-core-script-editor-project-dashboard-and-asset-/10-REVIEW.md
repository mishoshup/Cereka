---
phase: 10
name: Launcher IDE Core — Implementation Code Review
reviewed: 2026-05-09T13:00:00Z
depth: deep
files_reviewed: 22
files_reviewed_list:
  - launcher/code_editor.hpp
  - launcher/code_editor.cpp
  - launcher/lsp_client.hpp
  - launcher/lsp_client.cpp
  - launcher/syntax_highlighter.hpp
  - launcher/syntax_highlighter.cpp
  - launcher/editor_page.hpp
  - launcher/editor_page.cpp
  - launcher/editor_tab_bar.hpp
  - launcher/editor_tab_bar.cpp
  - launcher/find_panel.hpp
  - launcher/find_panel.cpp
  - launcher/outline_panel.hpp
  - launcher/outline_panel.cpp
  - launcher/dashboard_page.hpp
  - launcher/dashboard_page.cpp
  - launcher/asset_browser_page.hpp
  - launcher/asset_browser_page.cpp
  - launcher/template_model.hpp
  - launcher/template_model.cpp
  - launcher/project_metadata.hpp
  - launcher/project_metadata.cpp
  - launcher/main.cpp
findings:
  critical: 7
  warning: 6
  info: 8
  total: 21
status: issues_found
---

# Phase 10: Code Review — Launcher IDE Core

**Reviewed:** 2026-05-09T13:00:00Z
**Depth:** deep (cross-file analysis + call-chain tracing)
**Files Reviewed:** 22 launcher C++ source files
**Status:** issues_found

## Summary

This review audits the Phase 10 launcher implementation — 22 source files covering the CodeEditor widget, LSP client, syntax highlighter, editor page with split-pane support, find/replace panel, outline panel, dashboard page, asset browser, project metadata, and the main LauncherWindow. The code is functional and ambitious, but contains **7 critical defects** including memory leaks on tab close, orphaned floating windows, a null-pointer crash path on project creation, a broken LSP shutdown sequence, dangling pointers across project reloads, and UI-thread blocking during LSP startup. Several warnings around LSP spec compliance and code quality issues are also noted.

---

## Critical Issues

### CR-01: Memory leak — CodeEditor and CrkaHighlighter never deleted on tab close

**File:** `launcher/editor_page.cpp:588-594`
**Issue:** When a tab is closed, `closeTab()` calls `panel.editorStack->removeWidget(w)` which reparents the widget to `nullptr` but does **not** delete it. The `CodeEditor` and its attached `CrkaHighlighter` become orphaned objects that are never freed. Every tab close leaks these two heap objects indefinitely.

```cpp
// launcher/editor_page.cpp:588-594
if (panel.editorStack) {
    QWidget *w = panel.editorStack->widget(index);
    if (w)
        panel.editorStack->removeWidget(w);  // w NOT deleted — LEAK
}
panel.tabs.removeAt(index);
```

**Fix:** Call `w->deleteLater()` after `removeWidget`. The highlighter is parented to the editor's document and will be freed when the editor is destroyed.

```cpp
if (panel.editorStack) {
    QWidget *w = panel.editorStack->widget(index);
    if (w) {
        panel.editorStack->removeWidget(w);
        w->deleteLater();  // <-- ADD THIS
    }
}
```

---

### CR-02: Orphaned editor widgets become floating windows after tab close

**File:** `launcher/editor_page.cpp:590`
**Issue:** `QStackedWidget::removeWidget()` reparents the widget to `nullptr`. Since the widget was previously visible (as a child of a visible widget hierarchy), it **remains visible** as a top-level window when reparented. This means closing a tab leaves a frameless `QPlainTextEdit` window floating on the user's desktop.

The widget inherits visibility from its former visible parent chain. `QWidget::setVisible(true)` is transitive — when the parent is shown, children are shown. When reparented to `nullptr`, the visible flag persists. The result is an orphan window with no title bar, no close button, that can only be killed by terminating the process.

**Fix:** The same fix as CR-01 (`deleteLater`) resolves this — deleting the widget eliminates the floating window. Alternatively, explicitly hide before removal:

```cpp
w->hide();
panel.editorStack->removeWidget(w);
w->deleteLater();
```

---

### CR-03: Null pointer dereference in `onSidebarProjectClicked` via `doNewProject()`

**File:** `launcher/main.cpp:494-496, 575-588`
**Issue:** In `doNewProject()`, after `refreshSidebar()` and `selectSidebarByName(name)`, there is no guarantee that `selectSidebarByName` finds a match. If the project creation succeeded but the sidebar refresh didn't include the new item (e.g., filesystem race, or the project was created in a different directory), `selectSidebarByName` returns without setting a current item. Then `m_sidebarList->currentItem()` returns `nullptr`, and `onSidebarProjectClicked` dereferences it immediately:

```cpp
void LauncherWindow::onSidebarProjectClicked(QListWidgetItem *item)
{
    fs::path path = item->data(Qt::UserRole).toString().toStdString();  // CRASH if null
    ...
}
```

Called from:
```cpp
void LauncherWindow::doNewProject()
{
    ...
    if (ProjectManager::instance().createProject(name.toStdString())) {
        refreshSidebar();
        selectSidebarByName(name);         // may not find item
        onSidebarProjectClicked(m_sidebarList->currentItem());  // may be nullptr
    }
}
```

**Fix:** Add a null-check guard in `onSidebarProjectClicked`:

```cpp
void LauncherWindow::onSidebarProjectClicked(QListWidgetItem *item)
{
    if (!item) return;
    fs::path path = item->data(Qt::UserRole).toString().toStdString();
    ...
}
```

And ideally also add a null-check before calling it from `doNewProject()`:

```cpp
auto *item = m_sidebarList->currentItem();
if (item)
    onSidebarProjectClicked(item);
```

---

### CR-04: LSP `stop()` shutdown sequence is broken — exit notification never sent

**File:** `launcher/lsp_client.cpp:86-111`
**Issue:** The `stop()` method has a fundamental race condition between the shutdown request/response cycle and the blocking `waitForFinished`:

1. `sendRequest("shutdown", ..., callback)` queues the shutdown message to the process (line 89)
2. The callback captures `this` and, when fired, sends the `exit` notification and closes write channel (lines 90-97)
3. Immediately after, `waitForFinished(3000)` blocks the calling thread until the process exits OR 3 seconds elapse (line 101)
4. **The callback NEVER fires during `waitForFinished`** because `waitForFinished` is a synchronous blocking call that does NOT pump the Qt event loop. Since `onReadyRead` (which processes responses) is connected to `QProcess::readyReadStandardOutput`, and that signal only fires when the event loop runs, the shutdown response is never processed during the blocking wait.
5. After the timeout, `kill()` is called (line 102) — the server is killed without ever having received the `exit` notification.

This violates the LSP protocol, which requires shutdown → exit sequence. The server may leave behind temporary files, corrupt its state, or fail to save its workspace.

**Fix:** Restructure `stop()` to be asynchronous:

```cpp
void LspClient::stop()
{
    m_intentionalStop = true;
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        cleanup();
        return;
    }

    // Send shutdown — the response callback will send exit and schedule cleanup
    QJsonObject params;
    sendRequest("shutdown", params, [this](QJsonObject) {
        QJsonObject empty;
        sendNotification("exit", empty);
        // Schedule deferred cleanup to let the process handle exit gracefully
        QTimer::singleShot(500, this, [this]() {
            if (m_process) {
                m_process->closeWriteChannel();
                if (!m_process->waitForFinished(2000))
                    m_process->kill();
                cleanup();
            }
        });
    });

    // Fallback timer: if no response within 4s, force-kill
    QTimer::singleShot(4000, this, [this]() {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            cleanup();
        }
    });
}
```

Where `cleanup()` handles `m_pendingRequests.clear()`, `m_buffer.clear()`, and `delete m_process`.

---

### CR-05: Dangling LspClient pointer in orphaned editors after project clear+reload

**File:** `launcher/editor_page.cpp:505-506` (tab creation), `editor_page.cpp:907-919` (stopLspClient)
**Issue:** When `EditorPage::clearProject()` runs:
1. `removePanel()` calls `closeTab()` which removes editors from the stack but does NOT delete them (see CR-01)
2. `stopLspClient()` calls `m_lspClient->deleteLater()` and sets `m_lspClient = nullptr` (line 919)
3. The orphaned `CodeEditor` objects still hold the old `m_lspClient` pointer value via `setLspClient()`
4. After `deleteLater` takes effect (next event loop iteration), the LspClient object is destroyed
5. Any subsequent interaction with the orphaned editor (e.g., if it receives a mouse event or a timer fires) calling `m_lspClient->isRunning()` accesses freed memory

While orphaned editors are usually hidden, this is a latent use-after-free. If a `QToolTip` timer or a `QTimer` from the editor's internals fires on an orphan, it crashes.

**Fix:** Two things:
1. Fix the leak (CR-01) — deleting editors on tab close eliminates the dangling pointer
2. In `CodeEditor::setLspClient`, use `QPointer<LspClient>` instead of a raw pointer so the editor auto-detects when the client is destroyed:

In `code_editor.hpp`:
```cpp
QPointer<LspClient> m_lspClient;  // instead of LspClient*
```

And all accesses should check `if (m_lspClient)` (already done, but with `QPointer` the check remains valid after client destruction).

---

### CR-06: UI thread blocked for 5 seconds during LSP start

**File:** `launcher/lsp_client.cpp:69`
**Issue:** `LspClient::start()` calls `m_process->waitForStarted(5000)` which blocks the main (GUI) thread for up to 5 seconds. During this time:
- The launcher window is completely frozen (no paint events, no input)
- On macOS, the system may display a "spinning beachball" and flag the process as unresponsive
- If the LSP binary doesn't exist or fails to start, the UI hangs for the full timeout

```cpp
m_process->start(binaryPath, QStringList());

if (!m_process->waitForStarted(5000)) {  // BLOCKS UI THREAD
    delete m_process;
    m_process = nullptr;
    emit connectionFailed();
    return false;
}
```

**Fix:** Use an asynchronous approach. Connect to `QProcess::started()` and `QProcess::errorOccurred()` signals instead of blocking:

```cpp
bool LspClient::start(const QString &binaryPath)
{
    // ... validation ...
    m_process = new QProcess(this);
    // ... connect signals ...
    connect(m_process, &QProcess::started, this, [this]() {
        m_initialized = false;
        initialize();
        emit initializedOk();  // or similar
    });
    connect(m_process, &QProcess::errorOccurred, this,
            &LspClient::onProcessError);

    m_process->start(binaryPath, QStringList());
    return true;  // Don't wait — result comes via signal
}
```

---

### CR-07: `m_pendingPage` check on startup may navigate to stale page index

**File:** `launcher/main.cpp:210-216`
**Issue:** The `onFadeFinished` slot uses `m_pendingPage` as the target page index to navigate to. But `m_pendingPage` is initialized to `-1`, and the first call to `onFadeFinished` after the fade-out animation completes checks `if (m_pendingPage >= 0)`. If the user rapidly switches between pages via `fadeToPage` calls while a fade is in progress, `m_pendingPage` can be overwritten. Consider:

1. fadeToPage(PageProject) — sets m_pendingPage=2, starts fade-out
2. Before fade-out finishes, fadeToPage(PageEditor) — sets m_pendingPage=3
3. Fade-out completes, onFadeFinished fires, navigates to PageEditor correctly

This case actually works — the last value sticks. But a more problematic case:
1. fadeToPage(PageProject) — sets m_pendingPage=2, starts fade-out
2. fade-out animation completes, onFadeFinished fires, sets currentIndex=2, starts fade-in
3. During fade-in, fadeToPage(PageEditor) — sets m_pendingPage=3
4. fade-in completes, onFadeFinished fires (because the animation's finished signal fires again)
5. Now m_pendingPage=3, so navigates to PageEditor — correct

So this actually works in most cases. But there's no protection against a double-call to onFadeFinished if the fade animation is somehow triggered twice. Minor issue.

Actually, looking more carefully, the condition at line 249:
```cpp
if (m_pendingPage < 0 && m_contentStack->currentIndex() == page)
    return;
```
This means: if no pending page AND already on the target page, skip. But if `m_pendingPage >= 0` (a navigation is in flight) AND the current index happens to match the target, it still proceeds. This is fine.

**Severity downgraded to WARNING** — not actually exploitable in normal use. Move to warnings.

---

## Warnings

### WR-01: LSP Content-Length header parsing is case-sensitive

**File:** `launcher/lsp_client.cpp:442`
**Issue:** The LSP spec (JSON-RPC over stdin/stdout) specifies that HTTP-style headers are **case-insensitive**. The current code only looks for the exact string `"Content-Length: "`:

```cpp
static const char HEADER[] = "Content-Length: ";
int headerPos = m_buffer.indexOf(HEADER);
```

An LSP server that sends `"content-length: "` (lowercase) or `"CONTENT-LENGTH: "` will not be detected, causing the parser to reject all responses silently.

**Fix:** Use a case-insensitive search:

```cpp
int headerPos = m_buffer.indexOf("Content-Length: ", 0, Qt::CaseInsensitive);
```

---

### WR-02: `onGoToDefinition` doesn't handle `Location[]` result type per LSP spec

**File:** `launcher/editor_page.cpp:760`
**Issue:** The LSP `textDocument/definition` response can return either a `Location` (single object) **or** `Location[]` (array of objects). The code only handles the single-object case:

```cpp
QJsonObject result = resp["result"].toObject();
if (result.isEmpty()) return;

QString targetUri = result["uri"].toString();
```

If the LSP server returns an array (e.g., multiple definitions for overloaded labels), `toObject()` returns an empty `QJsonObject`, and the function silently does nothing.

**Fix:** Handle both cases:

```cpp
void EditorPage::onGoToDefinition(const QString &uri, int line, int col)
{
    // ...
    m_lspClient->definition(tab->uri, line, col,
        [this](QJsonObject resp) {
            QJsonValue resultVal = resp["result"];
            if (resultVal.isArray()) {
                // For now, navigate to the first definition
                auto arr = resultVal.toArray();
                if (arr.isEmpty()) return;
                navigateToLocation(arr[0].toObject());
            } else if (resultVal.isObject()) {
                navigateToLocation(resultVal.toObject());
            }
        });
}

void EditorPage::navigateToLocation(const QJsonObject &location)
{
    QString targetUri = location["uri"].toString();
    QJsonObject range = location["range"].toObject();
    QJsonObject start = range["start"].toObject();
    // ... rest of navigation logic ...
}
```

---

### WR-03: Empty placeholder URI used in CodeEditor signals — wrong document in split-pane mode

**File:** `launcher/code_editor.cpp:93, 430, 514, 562`
**Issue:** The `CodeEditor` emits go-to-definition, hover, and completion signals with a placeholder URI:

```cpp
QString uri = QUrl::fromLocalFile("").toString(); // caller fills real uri
emit goToDefinitionRequested(uri, line, col);
```

The `EditorPage` receivers ignore this URI (`Q_UNUSED(uri)`) and instead use the active tab's URI. In split-pane mode (where each pane has different documents open), if the **inactive** pane's editor triggers completion or hover, the signals are routed to the active pane's `EditorPage` handlers, which use the ACTIVE pane's tab URI — not the pane where the action originated. This means:

- Hover in inactive pane shows info for the active pane's document
- Completion in inactive pane returns completions for the wrong document
- Go-to-definition in inactive pane opens definitions in the active pane's context

**Fix:** Pass the file path through the capture chain. The simplest fix: store the file path in the `CodeEditor` and use it in the emitted signals:

In `code_editor.hpp`:
```cpp
void setFilePath(const QString &path) { m_filePath = path; }
QString filePath() const { return m_filePath; }
// ...
private:
    QString m_filePath;
```

In `editor_page.cpp` `addTab()`:
```cpp
editor->setFilePath(filePath);
```

In `code_editor.cpp` signal emissions:
```cpp
emit goToDefinitionRequested(QUrl::fromLocalFile(m_filePath).toString(),
                             line, col);
```

---

### WR-04: `LspClient::stop()` blocks with `waitForFinished` while event-driven callback expects to fire

**File:** `launcher/lsp_client.cpp:101`
**Issue:** (Related to CR-04 but milder case) Even in the non-shutdown case, `stop()` uses `waitForFinished(3000)` which blocks the calling thread. The `onReadyRead` slot that processes LSP responses is triggered by `QProcess::readyReadStandardOutput`, which only fires during Qt event loop processing. While `waitForFinished` is blocking, the event loop does not run, so any pending responses from the server are silently discarded.

This isn't a correctness bug per se (we're shutting down anyway), but it means the 3-second timeout is wasted — we could just kill immediately since we can't process responses during the wait.

**Fix:** Either use a signal-based approach (connect to `QProcess::finished`) or remove the `waitForFinished` and just kill immediately with a small grace period via `QTimer`.

---

### WR-05: Exit status not captured in `doLaunch()` on POSIX

**File:** `launcher/main.cpp:736`
**Issue:** In the `doLaunch()` method, the child process exit code is discarded:

```cpp
waitpid(pid, nullptr, 0);  // exit code ignored
```

This means if the game crashes or exits with an error, the launcher has no way to report it to the user. Contrast with `doSpecRun()` which correctly captures the exit code:

```cpp
waitpid(pid, &exitCode, 0);
exitCode = WEXITSTATUS(exitCode);
```

**Fix:** Capture and report the exit code:

```cpp
int childStatus = 0;
waitpid(pid, &childStatus, 0);
if (WIFEXITED(childStatus)) {
    int ec = WEXITSTATUS(childStatus);
    if (ec != 0) {
        appendLog("[WARN] Game exited with code " + std::to_string(ec));
    }
}
```

---

### WR-06: `QCompleter::activated` signal connection uses `QOverload` which is unnecessary in Qt6

**File:** `launcher/code_editor.cpp:78-79`
**Issue:** The connection uses `QOverload<const QString &>::of(&QCompleter::activated)` to disambiguate between overloaded signals. In Qt6, `QCompleter::activated(const QString &)` is the only overload (the `QModelIndex` overload was removed in Qt 5.15). This means `QOverload` will cause a **compile error** with Qt 6.8 (the target version per CMakeLists.txt).

```cpp
connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
        this, &CodeEditor::onCompleterActivated);
```

**Fix:** Remove the `QOverload` wrapper:

```cpp
connect(m_completer, &QCompleter::activated,
        this, &CodeEditor::onCompleterActivated);
```

---

## Info

### IN-01: Dead code — `myIndent = size() - size()` always zero

**File:** `launcher/code_editor.cpp:223`
**Issue:** `myIndent` is computed as `block.text().size() - block.text().size()` which is always 0. The variable is never used after assignment. This is either a placeholder for incomplete fold-marker logic or a copy-paste error.

```cpp
int myIndent = block.text().size() - block.text().size(); // Always 0
// Simplified: show marker on keyword blocks
```

**Fix:** Remove the unused variable or implement the actual indent comparison logic.

---

### IN-02: Dead `break;` after switch case block

**File:** `launcher/code_editor.cpp:497`
**Issue:** A `break;` statement appears outside the case block for `Qt::Key_Backtab`, after the closing `}`. This is unreachable dead code — the inner `break` (line 495) already exits the switch.

```cpp
case Qt::Key_Backtab: {
    // ...
    break;    // line 495 — exits switch
}
    break;    // line 497 — DEAD CODE, never reached
case Qt::Key_Escape:
```

**Fix:** Remove the extraneous `break;` on line 497.

---

### IN-03: `Q_UNUSED(rect)` but `rect` is used in `paintFoldMarkers`

**File:** `launcher/code_editor.cpp:202, 212`
**Issue:** The `rect` parameter is marked as `Q_UNUSED(rect)` on line 202, but it is actually used in the loop condition on line 212:

```cpp
while (block.isValid() && top <= rect.bottom()) {
```

**Fix:** Remove the `Q_UNUSED(rect)` macro.

---

### IN-04: Massively duplicated subprocess spawning code in `main.cpp`

**File:** `launcher/main.cpp`
- `doLaunch()` lines 652-749
- `doQuickRun()` lines 958-1086
- `doSpecRun()` lines 1098-1215

Each method contains ~100 lines of nearly identical subprocess code (pipe creation, fork/CreateProcess, stdout draining, exit code collection). The `drainPipe` lambda is copy-pasted in **four** places with slight variations. The only differences are:
- Command-line arguments passed to the runner
- Whether exit code is captured (spec run does, launch/quick run don't)
- The Windows pipe drain code variant uses a `readFn` template parameter while the POSIX variant embeds `read()` directly

**Impact:** ~300 lines of duplicated, hard-to-maintain code. A bug in pipe handling (e.g., the partial-line buffering at the end) would need to be fixed in 4 places.

**Fix:** Extract into a helper function or method:

```cpp
enum RunMode { RunGame, RunSpec, RunLaunch };
struct RunResult { int exitCode; };
RunResult LauncherWindow::runSubprocess(
    const std::string &runner,
    const std::string &cwd,
    const std::vector<std::string> &args,
    RunMode mode);
```

---

### IN-05: Log widget uses `QTextEdit` instead of `QPlainTextEdit`

**File:** `launcher/dashboard_page.cpp:198`
**Issue:** The output log uses `QTextEdit` which is a full rich-text editor with HTML parsing, OLE support, and heavy memory overhead. For a plain monospace log, `QPlainTextEdit` is significantly more efficient (less memory, faster text insertion, better scrolling performance for large documents).

The pattern `log->setPlainText(text)` works on both, but `QTextEdit` accumulates overhead per insertion.

**Fix:** Change `m_log` to `QPlainTextEdit` and update `logWidget()` accessor return type.

---

### IN-06: `onReplaceOne` resets navigation to first match after replace

**File:** `launcher/find_panel.cpp:463`
**Issue:** After performing a single replacement, `onReplaceOne()` calls `runSearch()` which resets the current match index to 0. The user is then navigated back to the first match rather than advancing to the next match. This means after each replacement, the user must manually navigate (F3) back to the position they were working on.

**Fix:** After `runSearch()`, compute the new index for the next match after the replaced position:

```cpp
void FindPanel::onReplaceOne()
{
    // ... existing replace logic ...
    int replaceLine = match.line;
    int replaceCol = match.col;
    runSearch();
    // Navigate to the first match after the replaced position
    for (int i = 0; i < m_matches.size(); ++i) {
        if (m_matches[i].line > replaceLine ||
            (m_matches[i].line == replaceLine && m_matches[i].col > replaceCol)) {
            navigateTo(i);
            return;
        }
    }
    // Fall back to first match if nothing after
    if (!m_matches.isEmpty()) navigateTo(0);
}
```

---

### IN-07: `paintIndentGuides` coordinate calculation is fragile

**File:** `launcher/code_editor.cpp:278-281`
**Issue:** The indent guide x-coordinates manually add `foldAreaWidth() + lineNumberAreaWidth()` to offset from the text area start. This duplicates the margin calculation done by `setViewportMargins` and `updateLineNumberAreaWidth()`. If the gutter width calculation changes in one place but not the other, the guides will be misaligned.

**Fix:** Store `lineNumberAreaWidth() + foldAreaWidth()` as a cached value and reuse it in both `setViewportMargins` and `paintIndentGuides`.

```cpp
// In updateLineNumberAreaWidth():
m_gutterWidth = lineNumberAreaWidth() + foldAreaWidth();
setViewportMargins(m_gutterWidth, 0, 0, 0);

// In paintIndentGuides():
int xOffset = m_gutterWidth;
```

---

### IN-08: `findCrkaFiles` uses `recursive_directory_iterator` but spec scanning uses non-recursive

**File:** `launcher/editor_page.cpp:450-460`, `launcher/dashboard_page.cpp:434`
**Issue:** `EditorPage::findCrkaFiles()` uses `fs::recursive_directory_iterator` to find `.crka` files (allowing nested script directories), but `DashboardPage::onRefreshSpecFiles()` uses `fs::directory_iterator` (non-recursive) to find `.spec.crka` files. If spec files are placed in subdirectories of `assets/scripts/`, they won't appear in the spec combo dropdown.

**Fix:** Make the spec scanning recursive:

```cpp
for (auto &entry : fs::recursive_directory_iterator(scriptsDir, ec)) {
    // ...
}
```

---

## Cross-File Issues Summary

### Call Chain: Tab Close → Orphan Widget

```
EditorTabBar::tabCloseRequested(int)
  → EditorPage::closeTab(panel, index)
    → panel.editorStack->removeWidget(w)    // w NOT deleted → CR-01
    → panel.tabs.removeAt(index)            // w becomes floating window → CR-02
```

### Call Chain: Project Clear → Dangling LspClient

```
EditorPage::clearProject()
  → stopLspClient()
    → m_lspClient->deleteLater()            // deferred deletion
    → m_lspClient = nullptr                 // EditorPage pointer cleared
  → removePanel(0)
    → closeTab() for each tab
      → editor NOT deleted (CR-01)
      → editor still has old m_lspClient pointer → CR-05
```

### Call Chain: LSP Shutdown → Blocking → Never Completes

```
LspClient::stop()
  → sendRequest("shutdown", ..., callback)   // callback sends exit + close
  → waitForFinished(3000)                    // blocks, event loop NOT running
  → callback NEVER fires (needs event loop for onReadyRead)
  → kill()                                   // process dies without exit → CR-04
```

---

## Recommendations

1. **Fix memory leaks immediately** (CR-01, CR-02) — these are the most impactful user-facing defects. Every tab close leaks memory and creates orphan windows.

2. **Fix LSP shutdown** (CR-04) — the current approach is fundamentally broken due to the blocking `waitForFinished` preventing the shutdown callback from ever firing.

3. **Add null guards** for sidebar item accesses (CR-03) — this is a 1-line fix that prevents a guaranteed crash path.

4. **Make LSP start async** (CR-06) — replace `waitForStarted(5000)` with signal-based connection to prevent UI freezes.

5. **Fix QOverload usage** for Qt6 compatibility (WR-06) — the current code won't compile against Qt 6.8.

6. **Remove QScintilla dead code** from CMakeLists.txt — per the pre-existing review finding CR-01.

7. **Consolidate subprocess spawning** (IN-04) — the 300-line duplication across `doLaunch`, `doQuickRun`, and `doSpecRun` is a maintenance liability.

---

**Reviewed:** 2026-05-09T13:00:00Z
**Reviewer:** gsd-code-reviewer (deep analysis)
**Depth:** deep
