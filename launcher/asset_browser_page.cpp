#include "asset_browser_page.hpp"
#include "theme.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPixmap>
#include <QVBoxLayout>

#include <fstream>

// ── File-type helpers ─────────────────────────────────────────────────────────

static bool isImageFile(const QString &path)
{
    static const QStringList exts = {".png", ".jpg", ".jpeg", ".gif", ".bmp"};
    for (const auto &e : exts)
        if (path.endsWith(e, Qt::CaseInsensitive))
            return true;
    return false;
}

static bool isAudioFile(const QString &path)
{
    static const QStringList exts = {".ogg", ".wav", ".mp3", ".flac"};
    for (const auto &e : exts)
        if (path.endsWith(e, Qt::CaseInsensitive))
            return true;
    return false;
}

static bool isFontFile(const QString &path)
{
    static const QStringList exts = {".ttf", ".otf"};
    for (const auto &e : exts)
        if (path.endsWith(e, Qt::CaseInsensitive))
            return true;
    return false;
}

// ── HRule helper (inline, mirrors dashboard_page pattern) ───────────────────

static QFrame *makeHRuleAsset(const char *color = Theme::BorderDivider)
{
    QFrame *f = new QFrame();
    f->setFixedHeight(1);
    f->setStyleSheet(QString("background-color: %1; border: none;").arg(color));
    return f;
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Constructor / Destructor ─────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

AssetBrowserPage::AssetBrowserPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

AssetBrowserPage::~AssetBrowserPage()
{
    m_mediaPlayer->stop();
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Ui construction ─────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void AssetBrowserPage::buildUi()
{
    setStyleSheet(QString("background-color: %1;").arg(Theme::BgBase));

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);

    // ── Left: file tree ─────────────────────────────────────────────────────
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_fileModel->setReadOnly(true);

    // Name column only — hide size/type/date
    m_treeView = new QTreeView();
    m_treeView->setModel(m_fileModel);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    m_treeView->setHeaderHidden(true);
    m_treeView->setDragEnabled(true);
    m_treeView->setDragDropMode(QAbstractItemView::DragOnly);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setStyleSheet(QString(R"(
        QTreeView {
            background-color: %1; border: none; padding: 0px; color: %2;
            font-size: 12px;
        }
        QTreeView::item {
            padding: 3px 4px; border-radius: 0px;
        }
        QTreeView::item:selected {
            background-color: %3; color: %4;
        }
        QTreeView::item:hover:!selected {
            background-color: %5;
        }
        QTreeView::branch:has-children:!has-siblings:closed,
        QTreeView::branch:closed:has-children:has-siblings {
            border-image: none;
        }
    )").arg(Theme::BgDeep)
       .arg(Theme::TextMuted)
       .arg(Theme::GoldSelectedBg)
       .arg(Theme::GoldSelected)
       .arg(Theme::BgItemHover));

    // Hide columns 1-3 (size, type, date)
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);

    connect(m_treeView, &QTreeView::clicked,
            this, &AssetBrowserPage::onTreeClicked);
    connect(m_treeView, &QTreeView::doubleClicked,
            this, &AssetBrowserPage::onTreeDoubleClicked);

    m_splitter->addWidget(m_treeView);

    // ── Right: preview panel ────────────────────────────────────────────────
    m_previewPanel = new QWidget();
    m_previewPanel->setStyleSheet(QString("background-color: %1;")
                                      .arg(Theme::BgSurface));
    QVBoxLayout *previewV = new QVBoxLayout(m_previewPanel);
    previewV->setContentsMargins(12, 12, 12, 12);
    previewV->setSpacing(8);

    // Preview image label
    m_previewLabel = new QLabel("Select an asset to preview");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(200, 200);
    m_previewLabel->setStyleSheet(QString(R"(
        QLabel {
            background-color: %1; border: 1px solid %2;
            border-radius: %3px; color: %4; font-size: 12px;
        }
    )").arg(Theme::BgDeep)
       .arg(Theme::BorderAccent)
       .arg(Theme::RadiusStd)
       .arg(Theme::TextFaint));
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    previewV->addWidget(m_previewLabel, 1);

    // File info
    m_fileInfoLabel = new QLabel();
    m_fileInfoLabel->setStyleSheet(
        QString("color: %1; font-size: 11px;").arg(Theme::TextMicro));
    m_fileInfoLabel->setAlignment(Qt::AlignCenter);
    m_fileInfoLabel->setWordWrap(true);
    previewV->addWidget(m_fileInfoLabel);

    previewV->addWidget(makeHRuleAsset(Theme::BorderAccent));

    // ── Audio controls (hidden by default) ──────────────────────────────────
    m_audioControls = new QWidget();
    m_audioControls->setStyleSheet("background: transparent;");
    QHBoxLayout *audioL = new QHBoxLayout(m_audioControls);
    audioL->setContentsMargins(0, 0, 0, 0);
    audioL->setSpacing(6);

    m_playPauseBtn = new QPushButton("▶ Play");
    m_playPauseBtn->setMinimumHeight(Theme::BtnHeightMicro);
    m_playPauseBtn->setStyleSheet(Theme::primaryBtn(Theme::RadiusSmall)
                                  + "QPushButton { padding: 0 16px; font-size: 11px; }");
    connect(m_playPauseBtn, &QPushButton::clicked,
            this, &AssetBrowserPage::onPlayPauseAudio);
    audioL->addWidget(m_playPauseBtn);

    m_audioStatusLabel = new QLabel();
    m_audioStatusLabel->setStyleSheet(
        QString("color: %1; font-size: 11px;").arg(Theme::TextMicro));
    audioL->addWidget(m_audioStatusLabel, 1);

    m_audioControls->setVisible(false);
    previewV->addWidget(m_audioControls);

    m_splitter->addWidget(m_previewPanel);

    // 60/40 split
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_splitter, 1);

    // ── Watcher ─────────────────────────────────────────────────────────────
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &AssetBrowserPage::onDirectoryChanged);

    // ── Media player ────────────────────────────────────────────────────────
    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, &AssetBrowserPage::onMediaStateChanged);
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Project lifecycle ───────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void AssetBrowserPage::setProjectPath(const fs::path &projectPath)
{
    m_projectPath = projectPath;

    fs::path assetsDir = projectPath / "assets";
    std::string assetsStr = assetsDir.string();
    QString qAssets = QString::fromStdString(assetsStr);

    // Root the model at the assets/ directory
    m_fileModel->setRootPath(qAssets);

    QModelIndex rootIndex = m_fileModel->index(qAssets);
    m_treeView->setRootIndex(rootIndex);

    // Expand top-level asset subdirectories: bg, characters, sounds, fonts, ui, scripts
    m_treeView->expandAll();

    // Clear any previous selection
    clearPreview();

    // Start watching the assets dir for changes
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    m_watcher->addPath(qAssets);

    // Also watch known subdirectories
    static const char *subdirs[] = {
        "bg", "characters", "sounds", "fonts", "ui", "scripts", nullptr
    };
    for (const char **sd = subdirs; *sd; ++sd) {
        fs::path sub = assetsDir / *sd;
        if (fs::exists(sub))
            m_watcher->addPath(QString::fromStdString(sub.string()));
    }
}

void AssetBrowserPage::clearProject()
{
    m_projectPath.clear();
    m_treeView->setRootIndex(QModelIndex());
    m_watcher->removePaths(m_watcher->directories());
    clearPreview();
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Tree slots ──────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void AssetBrowserPage::onTreeClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    QString path = m_fileModel->filePath(index);

    // Only preview files, not directories
    QFileInfo fi(path);
    if (fi.isDir())
        return;

    showPreview(path);
}

void AssetBrowserPage::onTreeDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    QString path = m_fileModel->filePath(index);

    QFileInfo fi(path);
    if (fi.isDir())
        return;

    // Stop any playing audio
    m_mediaPlayer->stop();

    emit fileActivated(path);
}

void AssetBrowserPage::onDirectoryChanged(const QString &/*path*/)
{
    // QFileSystemModel auto-refreshes in Qt6, but force a re-sort/refresh
    // to ensure the view picks up new files immediately.
    m_fileModel->setRootPath(m_fileModel->rootPath());
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Preview panel ───────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void AssetBrowserPage::showPreview(const QString &filePath)
{
    m_currentPreviewPath = filePath;
    QFileInfo fi(filePath);
    QString fileName = fi.fileName();

    // Stop any playing audio first
    m_mediaPlayer->stop();
    m_audioControls->setVisible(false);

    if (isImageFile(filePath)) {
        showImagePreview(filePath);
    } else if (isAudioFile(filePath)) {
        showAudioPreview(filePath);
    } else if (isFontFile(filePath)) {
        showFontPreview(filePath);
    } else {
        clearPreview();
        m_previewLabel->setText("No preview available\nfor this file type.");
    }

    // Show file info
    QString info = fileName;
    qint64 size = fi.size();
    if (size < 1024)
        info += QString("  ·  %1 B").arg(size);
    else if (size < 1024 * 1024)
        info += QString("  ·  %1 KB").arg(size / 1024);
    else
        info += QString("  ·  %1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    m_fileInfoLabel->setText(info);
}

void AssetBrowserPage::showImagePreview(const QString &filePath)
{
    QPixmap pix(filePath);
    if (pix.isNull()) {
        m_previewLabel->setText("Failed to load image.");
        return;
    }

    // Scale to fit the label while preserving aspect ratio
    QSize labelSize = m_previewLabel->size();
    if (labelSize.width() < 50 || labelSize.height() < 50)
        labelSize = QSize(300, 300);

    QPixmap scaled = pix.scaled(labelSize, Qt::KeepAspectRatio,
                                Qt::SmoothTransformation);
    m_previewLabel->setPixmap(scaled);
    m_previewLabel->setText(QString()); // clear text
}

void AssetBrowserPage::showAudioPreview(const QString &filePath)
{
    m_previewLabel->setText("Audio file");
    m_previewLabel->setPixmap(QPixmap()); // clear pixmap

    // Show audio controls
    m_audioControls->setVisible(true);
    m_playPauseBtn->setText("▶ Play");
    m_audioStatusLabel->setText("Ready");

    // Set up media player
    m_mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
    m_audioOutput->setVolume(0.5);
}

void AssetBrowserPage::showFontPreview(const QString &filePath)
{
    m_previewLabel->setPixmap(QPixmap());
    m_audioControls->setVisible(false);

    // Load font and show preview text
    QFont font;
    font.setPointSize(28);
    font.setStyleHint(QFont::SansSerif);

    int fontId = QFontDatabase::addApplicationFont(filePath);
    if (fontId >= 0) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            font = QFont(families.first(), 28);
        }
    }

    // Preview text with common characters
    QString previewText =
        QStringLiteral("The quick brown fox jumps over the lazy dog.\n"
                       "0123456789  !@#$%%^&*()  ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
                       "abcdefghijklmnopqrstuvwxyz\n\n"
                       "Aa Bb Cc Dd Ee Ff Gg Hh Ii Jj Kk Ll Mm Nn Oo Pp Qq Rr Ss Tt Uu Vv Ww Xx Yy Zz");

    m_previewLabel->setFont(font);
    m_previewLabel->setText(previewText);
    m_previewLabel->setWordWrap(true);
}

void AssetBrowserPage::clearPreview()
{
    m_currentPreviewPath.clear();
    m_previewLabel->setText("Select an asset to preview");
    m_previewLabel->setPixmap(QPixmap());
    m_previewLabel->setFont(QFont()); // reset font
    m_previewLabel->setWordWrap(false);
    m_fileInfoLabel->clear();
    m_audioControls->setVisible(false);
    m_mediaPlayer->stop();
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Audio playback ──────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void AssetBrowserPage::onPlayPauseAudio()
{
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        m_mediaPlayer->pause();
    } else {
        m_mediaPlayer->play();
    }
}

void AssetBrowserPage::onMediaStateChanged(QMediaPlayer::PlaybackState state)
{
    switch (state) {
    case QMediaPlayer::PlayingState:
        m_playPauseBtn->setText("⏸ Pause");
        m_audioStatusLabel->setText("Playing...");
        break;
    case QMediaPlayer::PausedState:
        m_playPauseBtn->setText("▶ Play");
        m_audioStatusLabel->setText("Paused");
        break;
    case QMediaPlayer::StoppedState:
        m_playPauseBtn->setText("▶ Play");
        m_audioStatusLabel->setText("Stopped");
        break;
    }
}
