#include "editor_page.hpp"
#include "theme.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

// ── Constructor / destructor ──────────────────────────────────────────────────

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

// ── UI construction ───────────────────────────────────────────────────────────

void EditorPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);

    // ── File tree ─────────────────────────────────────────────────────────────
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
    m_splitter->addWidget(m_fileTree);

    // ── Tab widget ────────────────────────────────────────────────────────────
    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setStyleSheet(QString(R"(
        QTabWidget::pane {
            border: none; background-color: %1;
        }
        QTabBar::tab {
            background-color: %2; color: %3; padding: 6px 14px;
            border: none; border-right: 1px solid %4;
            font-size: 12px; min-width: 80px;
        }
        QTabBar::tab:selected {
            background-color: %1; color: %5;
            border-bottom: 2px solid %6;
        }
        QTabBar::tab:hover:!selected {
            background-color: %7;
        }
    )").arg(Theme::BgBase)
       .arg(Theme::BgSurface)
       .arg(Theme::TextMuted)
       .arg(Theme::BorderDivider)
       .arg(Theme::TextPrimary)
       .arg(Theme::Gold)
       .arg(Theme::BgSurfaceHover));
    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &EditorPage::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &EditorPage::onTabChanged);
    m_splitter->addWidget(m_tabWidget);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    layout->addWidget(m_splitter, 1);

    // ── Status bar ────────────────────────────────────────────────────────────
    m_statusBar = new QLabel();
    m_statusBar->setFixedHeight(24);
    m_statusBar->setStyleSheet(QString(
        "background-color: %1; color: %2; font-size: 11px; padding: 0 10px;"
    ).arg(Theme::BgDeep).arg(Theme::TextMicro));
    layout->addWidget(m_statusBar);
}

// ── Project management ────────────────────────────────────────────────────────

void EditorPage::setProjectPath(const fs::path &path)
{
    m_projectPath = path;

    // Clear existing state
    m_tabWidget->clear();
    m_tabs.clear();
    m_fileTree->clear();
    m_statusBar->clear();

    // Stop old LSP
    stopLspClient();

    // Populate file tree
    populateFileTree();

    // Start LSP
    startLspClient();
}

void EditorPage::clearProject()
{
    m_projectPath.clear();
    m_tabWidget->clear();
    m_tabs.clear();
    m_fileTree->clear();
    m_statusBar->setText("No project loaded");
    stopLspClient();
}

// ── File tree ─────────────────────────────────────────────────────────────────

void EditorPage::populateFileTree()
{
    if (m_projectPath.empty())
        return;

    fs::path scriptsDir = m_projectPath / "assets" / "scripts";
    if (!fs::exists(scriptsDir))
        return;

    QStringList files = findCrkaFiles(scriptsDir);
    std::sort(files.begin(), files.end());

    // Remove existing watcher paths
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

// ── Tab management ────────────────────────────────────────────────────────────

EditorPage::EditorTab *EditorPage::currentTab()
{
    int idx = m_tabWidget->currentIndex();
    if (idx < 0 || idx >= m_tabs.size())
        return nullptr;
    return &m_tabs[idx];
}

int EditorPage::findTabByPath(const QString &filePath) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath == filePath)
            return i;
    }
    return -1;
}

int EditorPage::addTab(const QString &filePath)
{
    // Check if already open
    int existing = findTabByPath(filePath);
    if (existing >= 0) {
        m_tabWidget->setCurrentIndex(existing);
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
    connect(editor, &CodeEditor::goToDefinitionRequested,
            this, &EditorPage::onGoToDefinition);
    connect(editor, &CodeEditor::completionTriggered,
            this, &EditorPage::onCompletionTriggered);
    connect(editor, &QPlainTextEdit::textChanged,
            this, &EditorPage::onEditorTextChanged);

    // Build tab entry
    EditorTab tab;
    tab.filePath = filePath;
    tab.uri = localPathToUri(filePath);
    tab.editor = editor;
    tab.highlighter = highlighter;
    tab.version = 1;
    tab.modified = false;

    m_tabs.append(tab);
    int idx = m_tabs.size() - 1;

    QString tabLabel = QFileInfo(filePath).fileName();
    m_tabWidget->addTab(editor, tabLabel);
    m_tabWidget->setCurrentIndex(idx);

    // Notify LSP
    if (m_lspClient && m_lspClient->isRunning())
        notifyLspOpen(m_tabs[idx]);

    return idx;
}

void EditorPage::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    EditorTab &tab = m_tabs[index];

    // Notify LSP
    if (m_lspClient && m_lspClient->isRunning())
        m_lspClient->didClose(tab.uri);

    m_tabWidget->removeTab(index);
    m_tabs.removeAt(index);
}

// ── Tab slots ─────────────────────────────────────────────────────────────────

void EditorPage::onFileTreeClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    QString filePath = item->data(Qt::UserRole).toString();
    addTab(filePath);
}

void EditorPage::onTabCloseRequested(int index)
{
    closeTab(index);
}

void EditorPage::onTabChanged(int index)
{
    Q_UNUSED(index);
    // Could update status bar with current file info
    auto *tab = currentTab();
    if (tab) {
        m_statusBar->setText(QFileInfo(tab->filePath).fileName()
                             + (tab->modified ? " ●" : ""));
    }
}

void EditorPage::onEditorTextChanged()
{
    auto *tab = currentTab();
    if (!tab)
        return;

    tab->modified = true;
    m_autosaveTimer->start();
}

void EditorPage::onAutosaveTimeout()
{
    auto *tab = currentTab();
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
        notifyLspChange(*tab);
    }

    tab->version++;
    tab->modified = false;

    // Update tab label
    int idx = m_tabWidget->currentIndex();
    if (idx >= 0) {
        QString label = QFileInfo(tab->filePath).fileName();
        m_tabWidget->setTabText(idx, label);
    }
    m_statusBar->setText(QFileInfo(tab->filePath).fileName());
}

// ── LSP notifications ─────────────────────────────────────────────────────────

void EditorPage::notifyLspOpen(const EditorTab &tab)
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

void EditorPage::notifyLspChange(EditorTab &tab)
{
    if (!m_lspClient || !m_lspClient->isRunning())
        return;

    QString content = tab.editor->toPlainText();
    m_lspClient->didChange(tab.uri, content, tab.version);
}

// ── LSP diagnostics ───────────────────────────────────────────────────────────

void EditorPage::onDiagnosticsReceived(const QString &uri,
                                       const QList<LspDiagnostic> &diagnostics)
{
    QString localPath = uriToLocalPath(uri);

    // Find which tab this URI belongs to
    int tabIdx = findTabByPath(localPath);
    if (tabIdx < 0)
        return;

    m_tabs[tabIdx].editor->setDiagnostics(diagnostics);
}

// ── LSP definition / completion / hover ───────────────────────────────────────

void EditorPage::onGoToDefinition(const QString &uri, int line, int col)
{
    Q_UNUSED(uri);
    auto *tab = currentTab();
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

            // Open file (or focus existing tab)
            int tabIdx = findTabByPath(localPath);
            if (tabIdx < 0) {
                tabIdx = addTab(localPath);
            }

            if (tabIdx >= 0) {
                m_tabWidget->setCurrentIndex(tabIdx);

                // Scroll to line
                auto *editor = m_tabs[tabIdx].editor;
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
    auto *tab = currentTab();
    if (!tab || !m_lspClient || !m_lspClient->isRunning())
        return;

    m_lspClient->completion(tab->uri, line, col,
        [this, tab](QJsonObject resp) {
            QStringList items;
            QJsonObject result = resp["result"].toObject();
            QJsonArray completions = result["items"].toArray();

            if (completions.isEmpty()) {
                // Check if result is directly an array
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

// ── Open file (for cross-file navigation) ────────────────────────────────────

void EditorPage::openFile(const QString &filePath)
{
    addTab(filePath);
}

// ── URI conversion ────────────────────────────────────────────────────────────

QString EditorPage::localPathToUri(const QString &localPath) const
{
    return QUrl::fromLocalFile(localPath).toString();
}

QString EditorPage::uriToLocalPath(const QString &uri) const
{
    return QUrl(uri).toLocalFile();
}

// ── LSP lifecycle ─────────────────────────────────────────────────────────────

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

    // Close all documents
    for (auto &tab : m_tabs) {
        m_lspClient->didClose(tab.uri);
    }

    m_lspClient->stop();
    m_lspClient->deleteLater();
    m_lspClient = nullptr;
}
