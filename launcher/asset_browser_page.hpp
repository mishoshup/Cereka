#pragma once

#include <QAudioOutput>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMediaPlayer>
#include <QPushButton>
#include <QSplitter>
#include <QTreeView>
#include <QWidget>

#include <filesystem>

namespace fs = std::filesystem;

// ── AssetBrowserPage ─────────────────────────────────────────────────────────
//
// Tree view of the project's assets/ directory with a right-side preview
// panel.  Supports image (QPixmap), audio (QMediaPlayer), and font preview.
// Dragging from the tree produces MIME data with the file path (for editor
// drop support).
//
class AssetBrowserPage : public QWidget {
    Q_OBJECT

public:
    explicit AssetBrowserPage(QWidget *parent = nullptr);
    ~AssetBrowserPage() override;

    /// Point the browser at a project's assets/ directory.
    void setProjectPath(const fs::path &projectPath);
    void clearProject();

signals:
    /// Emitted when the user double-clicks a file or starts a drag.
    void fileActivated(const QString &filePath);

private slots:
    void onTreeClicked(const QModelIndex &index);
    void onTreeDoubleClicked(const QModelIndex &index);
    void onDirectoryChanged(const QString &path);
    void onPlayPauseAudio();
    void onMediaStateChanged(QMediaPlayer::PlaybackState state);

private:
    void buildUi();
    void showPreview(const QString &filePath);
    void showImagePreview(const QString &filePath);
    void showAudioPreview(const QString &filePath);
    void showFontPreview(const QString &filePath);
    void clearPreview();

    // ── Widgets ──────────────────────────────────────────────────────────────
    QSplitter *m_splitter = nullptr;

    // Tree
    QTreeView *m_treeView = nullptr;
    QFileSystemModel *m_fileModel = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;

    // Preview panel
    QWidget *m_previewPanel = nullptr;
    QLabel *m_previewLabel = nullptr;  ///< image / "no preview" label
    QLabel *m_fileInfoLabel = nullptr; ///< file name + size below preview

    // Audio controls
    QWidget *m_audioControls = nullptr;
    QPushButton *m_playPauseBtn = nullptr;
    QLabel *m_audioStatusLabel = nullptr;
    QMediaPlayer *m_mediaPlayer = nullptr;
    QAudioOutput *m_audioOutput = nullptr;

    // State
    fs::path m_projectPath;
    QString m_currentPreviewPath;
};
