#include "editor_page.hpp"
#include "theme.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

// ══════════════════════════════════════════════════════════════════════════════
// ── Constructor / destructor ─────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

EditorPage::EditorPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(AUTOSAVE_DELAY_MS);
    connect(m_autosaveTimer, &QTimer::timeout, this, &EditorPage::onAutosaveTimeout);

    m_fsWatcher = new QFileSystemWatcher(this);
}

EditorPage::~EditorPage()
{
    stopLspClient();
}

// ══════════════════════════════════════════════════════════════════════════════
// ── UI construction ──────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Outer splitter: file tree | editor area ─────────────────────────────
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(1);

    // ── File tree ───────────────────────────────────────────────────────────
    m_fileTree = new QListWidget();
    m_fileTree->setFixedWidth(200);
    m_fileTree->setStyleSheet(QString(R"(
        QListWidget {
            background-color: %1; border: none;
            border-right: 1px solid %2; padding: 4px;
            color: %3; font-size: 12px;
        }
        QListWidget::item {
            padding: 4px 8px; border-radius: 3px;
        }
        QListWidget::item:selected {
            background-color: %4; color: %5;
        }
        QListWidget::item:hover:!selected {
            background-color: %6;
        }
    )").arg(Theme::BgDeep)
       .arg(Theme::BorderDivider)
       .arg(Theme::TextMuted)
       .arg(Theme::GoldSelectedBg)
       .arg(Theme::GoldSelected)
       .arg(Theme::BgItemHover));
    connect(m_fileTree, &QListWidget::itemClicked,
            this, &EditorPage::onFileTreeClicked);
    m_mainSplitter->addWidget(m_fileTree);

    // ── Editor splitter (holds 1 or 2 panels) ───────────────────────────────
    m_editorSplitter = new QSplitter(Qt::Horizontal);
    m_editorSplitter->setHandleWidth(1);
    m_editorSplitter->setChildrenCollapsible(false);

    // Create the initial (only) panel
    createPanel(0);

    m_mainSplitter->addWidget(m_editorSplitter);

    // ── Outline panel (right side) ──────────────────────────────────────────
    m_outlinePanel = new OutlinePanel();
    m_mainSplitter->addWidget(m_outlinePanel);

    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    layout->addWidget(m_mainSplitter, 1);

    // ── Ctrl+\ to split/merge ───────────────────────────────────────────────
    auto *splitShortcut = new QShortcut(QKeySequence("Ctrl+\\"), this);
    connect(splitShortcut, &QShortcut::activated, this, [this]() {
        if (m_panelCount >= 2)
            onMergeSplit();
        else
            onSplitRight();
    });

    // ── Find/Replace panel ────────────────────────────────────────────────
    m_findPanel = new FindPanel();
    m_findPanel->setVisible(false);
    layout->addWidget(m_findPanel);

    connect(m_findPanel, &FindPanel::closed, this, [this]() {
        m_findPanel->setVisible(false);
        // Return focus to the active editor
        auto *tab = currentTab(m_panels[m_activePanel]);
        if (tab && tab->editor)
            tab->editor->setFocus();
    });

    // ── Ctrl+F to open find, Ctrl+H to open find+replace ──────────────────
    auto *findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        m_findPanel->setVisible(true);
        m_findPanel->showReplace(false);
        auto *tab = currentTab(m_panels[m_activePanel]);
        if (tab && tab->editor)
            m_findPanel->setEditor(tab->editor);
        m_findPanel->focusSearch();
    });

    auto *findReplaceShortcut = new QShortcut(QKeySequence("Ctrl+H"), this);
    connect(findReplaceShortcut, &QShortcut::activated, this, [this]() {
        m_findPanel->setVisible(true);
        m_findPanel->showReplace(true);
        auto *tab = currentTab(m_panels[m_activePanel]);
        if (tab && tab->editor)
            m_findPanel->setEditor(tab->editor);
        m_findPanel->focusSearch();
    });

    // ── F3/Shift+F3 to navigate matches ───────────────────────────────────
    auto *nextMatchShortcut = new QShortcut(QKeySequence("F3"), this);
    connect(nextMatchShortcut, &QShortcut::activated, this, [this]() {
        if (m_findPanel->isVisible())
            m_findPanel->onNextMatch();
    });

    auto *prevMatchShortcut = new QShortcut(QKeySequence("Shift+F3"), this);
    connect(prevMatchShortcut, &QShortcut::activated, this, [this]() {
        if (m_findPanel->isVisible())
            m_findPanel->onPreviousMatch();
    });

    // ── Status bar ──────────────────────────────────────────────────────────
    m_statusBar = new QLabel();
    m_statusBar->setFixedHeight(24);
    m_statusBar->setStyleSheet(QString(
        "background-color: %1; color: %2; font-size: 11px; padding: 0 10px;"
    ).arg(Theme::BgDeep).arg(Theme::TextMicro));
    layout->addWidget(m_statusBar);

    installEventFilter(this);
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Panel management ─────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

EditorPanel &EditorPage::activePanel()
{
    return m_panels[m_activePanel];
}

EditorPanel &EditorPage::panel(int idx)
{
    return m_panels[idx];
}

EditorPanel &EditorPage::createPanel(int index)
{
    EditorPanel &p = m_panels[index];
    p.container = new QWidget();
    auto *lay = new QVBoxLayout(p.container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Tab bar
    p.tabBar = new EditorTabBar();
    lay->addWidget(p.tabBar);

    // Editor stack
    p.editorStack = new QStackedWidget();
    p.editorStack->setStyleSheet(QString(
        "background-color: %1;"
    ).arg(Theme::BgBase));
    lay->addWidget(p.editorStack, 1);

    connectPanelSignals(p);

    // Insert into splitter at the given position
    if (index < m_editorSplitter->count())
        m_editorSplitter->insertWidget(index, p.container);
    else
        m_editorSplitter->addWidget(p.container);

    return p;
}

void EditorPage::removePanel(int index)
{
    EditorPanel &p = m_panels[index];
    if (!p.container)
        return;

    // Remove all tabs (notify LSP close)
    while (!p.tabs.isEmpty()) {
        closeTab(p, p.tabs.size() - 1);
    }

    // Remove from splitter (hide removes it from the splitter in Qt6)
    p.container->hide();
    p.container->deleteLater();
    p.clear();
}

void EditorPage::connectPanelSignals(EditorPanel &panel)
{
    if (!panel.tabBar || !panel.editorStack)
        return;

    // Use lambdas with the panel index captured to identify which panel
    int pIdx = (&panel - m_panels); // pointer arithmetic to get index

    connect(panel.tabBar, &QTabBar::currentChanged, this,
        [this, pIdx](int index) {
            if (index >= 0 && index < m_panels[pIdx].editorStack->count())
                m_panels[pIdx].editorStack->setCurrentIndex(index);
            onTabChanged(index);
        });

    connect(panel.tabBar, &QTabBar::tabCloseRequested, this,
        [this, pIdx](int index) {
            closeTab(m_panels[pIdx], index);
        });

    connect(panel.tabBar, &EditorTabBar::splitRightRequested, this,
        [this, pIdx]() {
            m_activePanel = pIdx;
            onSplitRight();
        });

    connect(panel.tabBar, &EditorTabBar::mergeRequested, this,
        [this]() {
            onMergeSplit();
        });
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Split-pane operations ────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::onSplitRight()
{
    if (m_panelCount >= 2)
        return; // already split

    EditorPanel &src = activePanel();

    // Create second panel
    m_panelCount = 2;
    createPanel(1);

    // Copy the active tab from the source panel to the new panel
    int srcIdx = src.tabBar->currentIndex();
    if (srcIdx >= 0 && srcIdx < src.tabs.size()) {
        const EditorTab &srcTab = src.tabs[srcIdx];
        addTab(m_panels[1], srcTab.filePath);
    }

    // Mark both tab bars as being in split mode
    m_panels[0].tabBar->setSplitActive(true);
    m_panels[1].tabBar->setSplitActive(true);

    // The new panel becomes active
    m_activePanel = 1;
    updatePaneReadOnlyState();
    m_statusBar->setText("Split view — click to switch focus");

    // Equal split
    QList<int> sizes = {m_editorSplitter->width() / 2,
                        m_editorSplitter->width() / 2};
    m_editorSplitter->setSizes(sizes);
}

void EditorPage::onMergeSplit()
{
    if (m_panelCount < 2)
        return;

    // Move all tabs from panel 1 to panel 0
    EditorPanel &src = m_panels[1];
    EditorPanel &dst = m_panels[0];

    for (const EditorTab &tab : src.tabs) {
        // Only add if not already in the destination
        if (findTabByPath(dst, tab.filePath) < 0) {
            addTab(dst, tab.filePath);
        }
    }

    // Mark tab bar as no longer in split mode
    m_panels[0].tabBar->setSplitActive(false);

    // Remove panel 1
    removePanel(1);
    m_panelCount = 1;
    m_activePanel = 0;
    updatePaneReadOnlyState();
    m_statusBar->setText("");
}

void EditorPage::onTabBarSplitRequested()
{
    onSplitRight();
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Focus / read-only tracking ───────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

bool EditorPage::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusIn && m_panelCount >= 2) {
        // Determine which panel the focused widget belongs to
        QWidget *focusWidget = qobject_cast<QWidget *>(watched);
        if (!focusWidget)
            focusWidget = qobject_cast<QWidget *>(watched->parent());

        // Walk parent chain to find which panel container contains this widget
        for (int i = 0; i < m_panelCount; ++i) {
            if (m_panels[i].container
                && (focusWidget == m_panels[i].container
                    || m_panels[i].container->isAncestorOf(focusWidget)))
            {
                if (m_activePanel != i) {
                    m_activePanel = i;
                    updatePaneReadOnlyState();
                    // Update status bar with active tab info
                    EditorTab *tab = currentTab(m_panels[i]);
                    if (tab) {
                        m_statusBar->setText(
                            QFileInfo(tab->filePath).fileName()
                            + (tab->modified ? " ●" : ""));
                    }
                }
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EditorPage::updatePaneReadOnlyState()
{
    // Only meaningful in split mode
    if (m_panelCount < 2)
        return;

    for (int i = 0; i < m_panelCount; ++i) {
        bool readonly = (i != m_activePanel);
        EditorPanel &p = m_panels[i];
        for (EditorTab &tab : p.tabs) {
            tab.editor->setReadOnly(readonly);
        }
        // Visual indicator on inactive tab bar
        p.tabBar->setEnabled(!readonly);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Project management ───────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::setProjectPath(const fs::path &path)
{
    m_projectPath = path;

    // Clear all panels
    for (int i = 0; i < m_panelCount; ++i) {
        removePanel(i);
    }
    m_panelCount = 1;
    m_activePanel = 0;
    createPanel(0);

    m_fileTree->clear();
    m_statusBar->clear();

    stopLspClient();
    populateFileTree();
    startLspClient();
}

void EditorPage::clearProject()
{
    m_projectPath.clear();
    for (int i = 0; i < m_panelCount; ++i) {
        removePanel(i);
    }
    m_panelCount = 1;
    m_activePanel = 0;
    createPanel(0);

    m_fileTree->clear();
    m_statusBar->setText("No project loaded");
    stopLspClient();
}

// ══════════════════════════════════════════════════════════════════════════════
// ── File tree ────────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::populateFileTree()
{
    if (m_projectPath.empty())
        return;

    fs::path scriptsDir = m_projectPath / "assets" / "scripts";
    if (!fs::exists(scriptsDir))
        return;

    QStringList files = findCrkaFiles(scriptsDir);
    std::sort(files.begin(), files.end());

    if (!m_fsWatcher->files().isEmpty())
        m_fsWatcher->removePaths(m_fsWatcher->files());

    for (const QString &file : files) {
        auto *item = new QListWidgetItem(QFileInfo(file).fileName());
        item->setData(Qt::UserRole, file);
        m_fileTree->addItem(item);
        m_fsWatcher->addPath(file);
    }
}

QStringList EditorPage::findCrkaFiles(const fs::path &dir) const
{
    QStringList result;
    std::error_code ec;
    for (const auto &entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.path().extension() == ".crka") {
            result.append(QString::fromStdString(entry.path().string()));
        }
    }
    return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Per-panel tab management ─────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

EditorTab *EditorPage::currentTab(EditorPanel &panel)
{
    int idx = panel.tabBar ? panel.tabBar->currentIndex() : -1;
    if (idx < 0 || idx >= panel.tabs.size())
        return nullptr;
    return &panel.tabs[idx];
}

int EditorPage::findTabByPath(EditorPanel &panel, const QString &filePath) const
{
    for (int i = 0; i < panel.tabs.size(); ++i) {
        if (panel.tabs[i].filePath == filePath)
            return i;
    }
    return -1;
}

int EditorPage::addTab(EditorPanel &panel, const QString &filePath)
{
    // Check if already open in this panel
    int existing = findTabByPath(panel, filePath);
    if (existing >= 0) {
        if (panel.tabBar)
            panel.tabBar->setCurrentIndex(existing);
        if (panel.editorStack)
            panel.editorStack->setCurrentIndex(existing);
        return existing;
    }

    // Read file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Create editor
    auto *editor = new CodeEditor();
    editor->setLspClient(m_lspClient);

    // Attach highlighter
    auto *highlighter = new CrkaHighlighter(editor->document());

    editor->setPlainText(content);

    // Connect signals
    int pIdx = (&panel - m_panels); // panel index via pointer arithmetic
    connect(editor, &CodeEditor::goToDefinitionRequested, this,
        [this](const QString &uri, int line, int col) {
            onGoToDefinition(uri, line, col);
        });
    connect(editor, &CodeEditor::completionTriggered, this,
        [this](const QString &uri, int line, int col) {
            onCompletionTriggered(uri, line, col);
        });
    connect(editor, &QPlainTextEdit::textChanged, this,
        [this, pIdx]() {
            // Only process text changes from the active panel
            if (pIdx == m_activePanel) {
                onEditorTextChanged();
            }
        });

    // Install event filter for focus tracking
    editor->installEventFilter(this);

    // Build tab entry with pane-specific URI
    EditorTab tab;
    tab.filePath = filePath;
    tab.uri = localPathToUri(filePath, pIdx);
    tab.editor = editor;
    tab.highlighter = highlighter;
    tab.version = 1;
    tab.modified = false;

    panel.tabs.append(tab);
    int idx = panel.tabs.size() - 1;

    // Add tab to this panel's tab bar
    if (panel.tabBar) {
        QString tabLabel = QFileInfo(filePath).fileName();
        panel.tabBar->addTab(tabLabel);
        panel.tabBar->setTabData(idx, filePath);
        panel.tabBar->setCurrentIndex(idx);
    }

    // Add editor to this panel's stack
    if (panel.editorStack) {
        panel.editorStack->addWidget(editor);
        panel.editorStack->setCurrentIndex(idx);
    }

    // Read-only state for inactive panels in split mode
    if (m_panelCount >= 2 && pIdx != m_activePanel) {
        editor->setReadOnly(true);
    }

    // Notify LSP
    if (m_lspClient && m_lspClient->isRunning())
        notifyLspOpen(panel, tab);

    return idx;
}

void EditorPage::closeTab(EditorPanel &panel, int index)
{
    if (index < 0 || index >= panel.tabs.size())
        return;

    EditorTab &tab = panel.tabs[index];

    // Notify LSP
    if (m_lspClient && m_lspClient->isRunning())
        m_lspClient->didClose(tab.uri);

    // Remove from tab bar
    if (panel.tabBar)
        panel.tabBar->removeTab(index);

    // Remove from stack
    if (panel.editorStack) {
        QWidget *w = panel.editorStack->widget(index);
        if (w) {
            panel.editorStack->removeWidget(w);
            w->deleteLater();
        }
    }

    panel.tabs.removeAt(index);
}

void EditorPage::updateTabLabel(EditorPanel &panel, int index)
{
    if (index < 0 || index >= panel.tabs.size())
        return;

    const EditorTab &tab = panel.tabs[index];
    QString label = QFileInfo(tab.filePath).fileName();
    if (tab.modified)
        label += " ●";

    if (panel.tabBar)
        panel.tabBar->setTabText(index, label);
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Tab slots ────────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::onFileTreeClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    QString filePath = item->data(Qt::UserRole).toString();
    addTab(m_panels[m_activePanel], filePath);
}

void EditorPage::onTabCloseRequested(int index)
{
    closeTab(m_panels[m_activePanel], index);
}

void EditorPage::onTabChanged(int /*index*/)
{
    EditorPanel &p = m_panels[m_activePanel];
    auto *tab = currentTab(p);
    if (tab) {
        m_statusBar->setText(QFileInfo(tab->filePath).fileName()
                             + (tab->modified ? " ●" : ""));
        // Refresh outline panel for the active document
        m_outlinePanel->setActiveDocument(tab->uri, tab->editor, m_lspClient);
    } else {
        m_outlinePanel->refresh();
    }
}

void EditorPage::onEditorTextChanged()
{
    EditorPanel &p = m_panels[m_activePanel];
    auto *tab = currentTab(p);
    if (!tab)
        return;

    tab->modified = true;
    m_autosaveTimer->start();

    int idx = p.tabBar ? p.tabBar->currentIndex() : -1;
    updateTabLabel(p, idx);
}

void EditorPage::onAutosaveTimeout()
{
    EditorPanel &p = m_panels[m_activePanel];
    auto *tab = currentTab(p);
    if (!tab || !tab->modified)
        return;

    // Save to disk
    QFile file(tab->filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << tab->editor->toPlainText();
        file.close();
    }

    // Notify LSP
    if (m_lspClient && m_lspClient->isRunning()) {
        notifyLspChange(p, *tab);
    }

    tab->version++;
    tab->modified = false;

    int idx = p.tabBar ? p.tabBar->currentIndex() : -1;
    updateTabLabel(p, idx);
    m_statusBar->setText(QFileInfo(tab->filePath).fileName());

    // Refresh outline panel after saving (document content changed)
    m_outlinePanel->refresh();
}

// ══════════════════════════════════════════════════════════════════════════════
// ── LSP notifications ────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::notifyLspOpen(EditorPanel &panel, const EditorTab &tab)
{
    if (!m_lspClient || !m_lspClient->isRunning())
        return;

    QFile file(tab.filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString content = in.readAll();
        m_lspClient->didOpen(tab.uri, content);
    }
}

void EditorPage::notifyLspChange(EditorPanel &panel, EditorTab &tab)
{
    if (!m_lspClient || !m_lspClient->isRunning())
        return;

    QString content = tab.editor->toPlainText();
    m_lspClient->didChange(tab.uri, content, tab.version);
}

// ══════════════════════════════════════════════════════════════════════════════
// ── LSP diagnostics ──────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::onDiagnosticsReceived(const QString &uri,
                                       const QList<LspDiagnostic> &diagnostics)
{
    QString localPath = uriToLocalPath(uri);
    int pIdx = paneFromUri(uri);

    // Find which tab in which panel this URI belongs to
    if (pIdx >= 0 && pIdx < m_panelCount) {
        int tabIdx = findTabByPath(m_panels[pIdx], localPath);
        if (tabIdx >= 0) {
            m_panels[pIdx].tabs[tabIdx].editor->setDiagnostics(diagnostics);
            return;
        }
    }

    // Fallback: search all panels
    for (int i = 0; i < m_panelCount; ++i) {
        int tabIdx = findTabByPath(m_panels[i], localPath);
        if (tabIdx >= 0) {
            m_panels[i].tabs[tabIdx].editor->setDiagnostics(diagnostics);
            return;
        }
    }

    // Refresh outline (LSP has processed the document)
    m_outlinePanel->refresh();
}

// ══════════════════════════════════════════════════════════════════════════════
// ── LSP definition / completion / hover ──────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::onGoToDefinition(const QString &uri, int line, int col)
{
    Q_UNUSED(uri);
    EditorPanel &p = m_panels[m_activePanel];
    auto *tab = currentTab(p);
    if (!tab || !m_lspClient || !m_lspClient->isRunning())
        return;

    m_lspClient->definition(tab->uri, line, col,
        [this](QJsonObject resp) {
            QJsonObject result = resp["result"].toObject();
            if (result.isEmpty())
                return;

            QString targetUri = result["uri"].toString();
            QJsonObject range = result["range"].toObject();
            QJsonObject start = range["start"].toObject();
            int targetLine = start["line"].toInt();
            int targetCol = start["character"].toInt();

            QString localPath = uriToLocalPath(targetUri);

            EditorPanel &p = m_panels[m_activePanel];
            int tabIdx = findTabByPath(p, localPath);
            if (tabIdx < 0) {
                tabIdx = addTab(p, localPath);
            }

            if (tabIdx >= 0) {
                if (p.tabBar)
                    p.tabBar->setCurrentIndex(tabIdx);
                if (p.editorStack)
                    p.editorStack->setCurrentIndex(tabIdx);

                auto *editor = p.tabs[tabIdx].editor;
                QTextBlock block = editor->document()->findBlockByNumber(targetLine);
                if (block.isValid()) {
                    QTextCursor cursor(block);
                    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                        targetCol);
                    editor->setTextCursor(cursor);
                    editor->ensureCursorVisible();
                    editor->centerCursor();
                }
            }
        });
}

void EditorPage::onCompletionTriggered(const QString &uri, int line, int col)
{
    Q_UNUSED(uri);
    EditorPanel &p = m_panels[m_activePanel];
    auto *tab = currentTab(p);
    if (!tab || !m_lspClient || !m_lspClient->isRunning())
        return;

    m_lspClient->completion(tab->uri, line, col,
        [this, tab](QJsonObject resp) {
            QStringList items;
            QJsonObject result = resp["result"].toObject();
            QJsonArray completions = result["items"].toArray();

            if (completions.isEmpty()) {
                QJsonArray arr = resp["result"].toArray();
                for (const QJsonValue &v : arr) {
                    items.append(v.toObject()["label"].toString());
                }
            } else {
                for (const QJsonValue &v : completions) {
                    items.append(v.toObject()["label"].toString());
                }
            }

            if (!items.isEmpty()) {
                tab->editor->showCompletions(items);
            }
        });
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Open file (for cross-file navigation) ────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::openFile(const QString &filePath)
{
    addTab(m_panels[m_activePanel], filePath);
}

// ══════════════════════════════════════════════════════════════════════════════
// ── URI conversion (with pane suffix) ────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

QString EditorPage::localPathToUri(const QString &localPath, int pane) const
{
    QUrl url = QUrl::fromLocalFile(localPath);
    // Add pane fragment to distinguish copies in split-pane mode
    if (m_panelCount >= 2) {
        url.setFragment(QString("pane=%1").arg(pane));
    }
    return url.toString();
}

QString EditorPage::uriToLocalPath(const QString &uri) const
{
    QUrl url(uri);
    url.setFragment(QString()); // strip the pane suffix
    return url.toLocalFile();
}

int EditorPage::paneFromUri(const QString &uri) const
{
    QUrl url(uri);
    QString fragment = url.fragment();
    if (fragment.startsWith("pane=")) {
        bool ok = false;
        int pane = fragment.mid(5).toInt(&ok);
        if (ok && pane >= 0 && pane < 2)
            return pane;
    }
    return 0; // default to pane 0
}

// ══════════════════════════════════════════════════════════════════════════════
// ── LSP lifecycle ────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void EditorPage::startLspClient()
{
    if (m_lspClient)
        return;

    m_lspClient = new LspClient(this);

    connect(m_lspClient, &LspClient::diagnosticsReceived,
            this, &EditorPage::onDiagnosticsReceived);
    connect(m_lspClient, &LspClient::initializedOk, this, [this]() {
        m_statusBar->setText("LSP connected");
    });
    connect(m_lspClient, &LspClient::connectionFailed, this, [this]() {
        m_statusBar->setText("LSP server unavailable");
    });

    QString binaryPath = LspClient::resolveBinary();
    if (binaryPath.isEmpty()) {
        m_statusBar->setText("LSP server not found — editor features limited");
        return;
    }

    m_statusBar->setText("Starting LSP: " + binaryPath);

    if (m_lspClient->start(binaryPath)) {
        m_lspClient->initialize();
    } else {
        m_statusBar->setText("LSP server unavailable");
    }
}

void EditorPage::stopLspClient()
{
    if (!m_lspClient)
        return;

    // Close all documents in all panels
    for (int i = 0; i < m_panelCount; ++i) {
        for (auto &tab : m_panels[i].tabs) {
            m_lspClient->didClose(tab.uri);
        }
    }

    m_lspClient->stop();
    m_lspClient->deleteLater();
    m_lspClient = nullptr;
}
