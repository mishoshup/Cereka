#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

/// Enhanced project dashboard — replaces the old buildProjectDetail().
///
/// Shows project metadata, quick-run / spec-run controls, template gallery,
/// and an output log panel.  Emits signals for actions the LauncherWindow
/// must handle (process spawning, page navigation, etc.).
class DashboardPage : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget *parent = nullptr);
    ~DashboardPage() override;

    /// Populate the dashboard for the currently loaded project.
    void loadProject(const fs::path &projectPath);
    void clearProject();

    /// Access the log widget so LauncherWindow can route thread output.
    QTextEdit *logWidget() const { return m_log; }

    void setRunning(bool running);

signals:
    void quickRunRequested(const QString &projectPath, const QString &entry);
    void specRunRequested(const QString &projectPath, const QString &specFile);
    void openInEditorRequested(const QString &projectPath, const QString &filePath);
    void renameRequested();
    void initRequested();
    void createFromTemplate(const QString &templateName, const QString &entryScriptName);

private slots:
    void onRefreshSpecFiles();
    void onQuickRunClicked();
    void onSpecRunClicked();
    void onOpenEditorClicked();
    void onRenameClicked();
    void onInitClicked();
    void onAutoSaveTick();

private:
    void buildUi();
    void refreshMetadata();
    void setHasGameCfg(bool hasGame);
    void addTemplateGallerySection(QVBoxLayout *parent);

    // ── Widgets ────────────────────────────────────────────────────────────────

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_renameBtn = nullptr;
    QLabel *m_statusLabel = nullptr;

    // Metadata section
    QWidget *m_metaSection = nullptr;
    QLabel *m_uuidValue = nullptr;
    QLabel *m_lastOpenedValue = nullptr;
    QLabel *m_playTimeValue = nullptr;
    QLabel *m_engineVersionValue = nullptr;

    // Game actions (shown when game.cfg exists)
    QWidget *m_gameActions = nullptr;
    QPushButton *m_quickRunBtn = nullptr;
    QComboBox *m_specCombo = nullptr;
    QPushButton *m_specRunBtn = nullptr;
    QPushButton *m_openEditorBtn = nullptr;

    // Template gallery section
    QWidget *m_templateGallery = nullptr;

    // Init widget (shown when no game.cfg)
    QWidget *m_initWidget = nullptr;

    // Log
    QTextEdit *m_log = nullptr;

    // Auto-save timer (play time + metadata persist)
    QTimer *m_autoSaveTimer = nullptr;

    fs::path m_projectPath;
    bool m_hasGameCfg = false;
};
