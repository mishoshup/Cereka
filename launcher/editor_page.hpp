#pragma once

#include "code_editor.hpp"
#include "editor_tab_bar.hpp"
#include "find_panel.hpp"
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

// ── EditorPanel ───────────────────────────────────────────────────────────────

/// A single editor panel within the split view.
struct EditorPanel {
    QWidget *container = nullptr;  ///< The widget container for this panel
    EditorTabBar *tabBar = nullptr;
    QStackedWidget *editorStack = nullptr;
    QVector<EditorTab> tabs;

    void clear() {
        tabs.clear();
        tabBar = nullptr;
        editorStack = nullptr;
        container = nullptr;
    }
};

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

    // Split-pane slots
    void onSplitRight();
    void onMergeSplit();
    void onTabBarSplitRequested();

private:
    void buildUi();

    // File tree
    void populateFileTree();
    QStringList findCrkaFiles(const fs::path &dir) const;

    // Panel management
    int activePanelIndex() const { return m_activePanel; }
    EditorPanel &activePanel();
    EditorPanel &panel(int idx);
    int panelCount() const { return m_panelCount; }

    // Create or destroy a panel
    EditorPanel &createPanel(int index);
    void removePanel(int index);
    void installPanelWidgets(EditorPanel &panel);
    void connectPanelSignals(EditorPanel &panel);

    // Tab management (per-panel)
    EditorTab *currentTab(EditorPanel &panel);
    int findTabByPath(EditorPanel &panel, const QString &filePath) const;
    int addTab(EditorPanel &panel, const QString &filePath);
    void closeTab(EditorPanel &panel, int index);
    void updateTabLabel(EditorPanel &panel, int index);

    // URI helpers with pane suffix for LSP isolation in split mode
    QString localPathToUri(const QString &localPath, int pane = 0) const;
    QString uriToLocalPath(const QString &uri) const;
    int paneFromUri(const QString &uri) const;

    // LSP lifecycle — shared across all panes
    void startLspClient();
    void stopLspClient();
    void notifyLspOpen(EditorPanel &panel, const EditorTab &tab);
    void notifyLspChange(EditorPanel &panel, EditorTab &tab);

    // Focus tracking for split-pane
    void updatePaneReadOnlyState();
    bool eventFilter(QObject *watched, QEvent *event) override;

    // ── Widgets ────────────────────────────────────────────────────────────────
    QSplitter *m_mainSplitter = nullptr;
    QListWidget *m_fileTree = nullptr;
    QSplitter *m_editorSplitter = nullptr;
    QLabel *m_statusBar = nullptr;

    // ── Panels (fixed size array, m_panelCount tracks active) ──────────────────
    EditorPanel m_panels[2];
    int m_activePanel = 0;
    int m_panelCount = 1;

    // ── Find/Replace panel ─────────────────────────────────────────────────────
    FindPanel *m_findPanel = nullptr;

    // ── LSP ────────────────────────────────────────────────────────────────────
    LspClient *m_lspClient = nullptr;

    // ── Project ────────────────────────────────────────────────────────────────
    fs::path m_projectPath;
    QFileSystemWatcher *m_fsWatcher = nullptr;

    // ── Autosave ───────────────────────────────────────────────────────────────
    QTimer *m_autosaveTimer = nullptr;
    static constexpr int AUTOSAVE_DELAY_MS = 1500;
};
