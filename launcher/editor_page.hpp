#pragma once

#include "code_editor.hpp"
#include "lsp_client.hpp"
#include "syntax_highlighter.hpp"

#include <QFileSystemWatcher>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

#include <filesystem>

namespace fs = std::filesystem;

// ── EditorPage ────────────────────────────────────────────────────────────────

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

private:
    struct EditorTab {
        QString filePath;
        QString uri;
        CodeEditor *editor = nullptr;
        CrkaHighlighter *highlighter = nullptr;
        int version = 1;
        bool modified = false;
    };

    void buildUi();
    void populateFileTree();
    QStringList findCrkaFiles(const fs::path &dir) const;

    EditorTab *currentTab();
    int findTabByPath(const QString &filePath) const;
    int addTab(const QString &filePath);
    void closeTab(int index);

    QString localPathToUri(const QString &localPath) const;
    QString uriToLocalPath(const QString &uri) const;

    // LSP lifecycle
    void startLspClient();
    void stopLspClient();
    void notifyLspOpen(const EditorTab &tab);
    void notifyLspChange(EditorTab &tab);

    QSplitter *m_splitter = nullptr;
    QListWidget *m_fileTree = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    QLabel *m_statusBar = nullptr;

    LspClient *m_lspClient = nullptr;
    QVector<EditorTab> m_tabs;

    fs::path m_projectPath;
    QFileSystemWatcher *m_fsWatcher = nullptr;

    // Autosave
    QTimer *m_autosaveTimer = nullptr;
    static constexpr int AUTOSAVE_DELAY_MS = 1500;
};
