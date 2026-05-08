#pragma once

#include "code_editor.hpp"
#include "editor_tab_bar.hpp"
#include "lsp_client.hpp"
#include "syntax_highlighter.hpp"

#include <QFileSystemWatcher>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QWidget>

#include <filesystem>

namespace fs = std::filesystem;

// ── EditorTab ─────────────────────────────────────────────────────────────────

/// One open document in an editor panel.
struct EditorTab {
    QString filePath;
    QString uri;
    CodeEditor *editor = nullptr;
    CrkaHighlighter *highlighter = nullptr;
    int version = 1;
    bool modified = false;
};

// ── EditorPage ────────────────────────────────────────────────────────────────

/// Tab-bar + stacked-editor page for the launcher IDE.
/// Uses a QTabBar + QStackedWidget (not QTabWidget) so we can install a
/// custom EditorTabBar with right-click context menu and drag-to-split.
class EditorPage : public QWidget {
    Q_OBJECT
public:
    explicit EditorPage(QWidget *parent = nullptr);
    ~EditorPage() override;

    /// Set the current project path. Starts LSP, populates file tree.
    void setProjectPath(const fs::path &path);
    void clearProject();

    /// Open a specific file by path (used for cross-file navigation).
    void openFile(const QString &filePath);

private slots:
    void onFileTreeClicked(QListWidgetItem *item);
    void onTabCloseRequested(int index);
    void onTabChanged(int index);
    void onEditorTextChanged();
    void onAutosaveTimeout();
    void onDiagnosticsReceived(const QString &uri,
                               const QList<LspDiagnostic> &diagnostics);
    void onGoToDefinition(const QString &uri, int line, int col);
    void onCompletionTriggered(const QString &uri, int line, int col);

    // Split-pane slots
    void onSplitRight();
    void onMergeSplit();
    void onTabBarSplitRequested();

private:
    void buildUi();
    void populateFileTree();
    QStringList findCrkaFiles(const fs::path &dir) const;

    // Tab management
    EditorTab *currentTab();
    int findTabByPath(const QString &filePath) const;
    int addTab(const QString &filePath);
    void closeTab(int index);
    void updateTabLabel(int index);

    // URI helpers
    QString localPathToUri(const QString &localPath) const;
    QString uriToLocalPath(const QString &uri) const;

    // LSP lifecycle
    void startLspClient();
    void stopLspClient();
    void notifyLspOpen(const EditorTab &tab);
    void notifyLspChange(EditorTab &tab);

    // ── Widgets ────────────────────────────────────────────────────────────────
    QSplitter *m_mainSplitter = nullptr;
    QListWidget *m_fileTree = nullptr;

    // Editor area: custom tab bar + stacked widget (replaces QTabWidget)
    QWidget *m_editorArea = nullptr;
    EditorTabBar *m_tabBar = nullptr;
    QStackedWidget *m_editorStack = nullptr;
    QLabel *m_statusBar = nullptr;

    // ── Tab data ───────────────────────────────────────────────────────────────
    QVector<EditorTab> m_tabs;

    // ── LSP ────────────────────────────────────────────────────────────────────
    LspClient *m_lspClient = nullptr;

    // ── Project ────────────────────────────────────────────────────────────────
    fs::path m_projectPath;
    QFileSystemWatcher *m_fsWatcher = nullptr;

    // ── Autosave ───────────────────────────────────────────────────────────────
    QTimer *m_autosaveTimer = nullptr;
    static constexpr int AUTOSAVE_DELAY_MS = 1500;
};
