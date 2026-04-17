#include <cstdio>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMetaObject>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include "config.hpp"
#include "embedded_assets.h"
#include "project_manager.hpp"
#include "theme.hpp"

#ifdef _WIN32
#    include <windows.h>
#else
#    include <sys/wait.h>
#    include <unistd.h>
#endif

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

namespace fs = std::filesystem;

// ── Global log state ──────────────────────────────────────────────────────────

static std::string s_log;
static std::mutex s_logMutex;
static std::atomic<bool> s_busy{false};

static void appendLog(const std::string &text)
{
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_log += text;
    s_log += "\n";
}

static void clearLog()
{
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_log.clear();
}

// ── Platform helpers ──────────────────────────────────────────────────────────

static fs::path selfExeDir()
{
#ifdef _WIN32
    char path[MAX_PATH] = {};
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return fs::path(path).parent_path();
#else
    char buf[2048] = {};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0)
        return fs::path(std::string(buf, len)).parent_path();
    return fs::current_path();
#endif
}

static fs::path runtimeDir(const std::string &platform)
{
    return selfExeDir() / "runtimes" / platform;
}

static std::string findGameRunner()
{
#ifdef _WIN32
    fs::path r = runtimeDir("windows") / "CerekaGame.exe";
    if (fs::exists(r))
        return r.string();
    r = selfExeDir() / "CerekaGame.exe";
    return fs::exists(r) ? r.string() : "CerekaGame.exe";
#else
    fs::path r = runtimeDir("linux") / "CerekaGame";
    if (fs::exists(r))
        return r.string();
    r = selfExeDir() / "CerekaGame";
    return fs::exists(r) ? r.string() : "CerekaGame";
#endif
}

// ── Widget helpers ────────────────────────────────────────────────────────────

static QFrame *makeHRule(const char *color = Theme::BorderDivider)
{
    QFrame *f = new QFrame();
    f->setFixedHeight(1);
    f->setStyleSheet(QString("background-color: %1; border: none;").arg(color));
    return f;
}

static QLabel *makeSectionLabel(const QString &text)
{
    QLabel *l = new QLabel(text);
    l->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 600; letter-spacing: 2px;")
                         .arg(Theme::TextDimmer));
    return l;
}

static QFont boldFont(int ptSize)
{
    QFont f = qApp->font();
    f.setPointSize(ptSize);
    f.setBold(true);
    return f;
}

// ── Content page indices ──────────────────────────────────────────────────────

enum Page { PageWelcome = 0, PageEmpty = 1, PageProject = 2 };

// ── LauncherWindow ────────────────────────────────────────────────────────────

class LauncherWindow : public QWidget {
    Q_OBJECT

public:
    LauncherWindow()
    {
        setWindowTitle("Cereka Launcher");
        setMinimumSize(Theme::WindowMinW, Theme::WindowMinH);
        resize(Theme::WindowDefaultW, Theme::WindowDefaultH);

        loadFont();
        applyDarkTheme();

        QHBoxLayout *root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        root->addWidget(buildSidebar());
        root->addWidget(makeHRule(Theme::BorderDivider));

        m_contentStack = new QStackedWidget();
        m_contentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_contentStack->addWidget(buildWelcomeScreen()); // PageWelcome
        m_contentStack->addWidget(buildEmptyState());    // PageEmpty
        m_contentStack->addWidget(buildProjectDetail()); // PageProject
        root->addWidget(m_contentStack, 1);

        m_contentOpacity = new QGraphicsOpacityEffect(m_contentStack);
        m_contentStack->setGraphicsEffect(m_contentOpacity);
        m_contentOpacity->setOpacity(1.0);

        m_fadeAnim = new QPropertyAnimation(m_contentOpacity, "opacity", this);
        m_fadeAnim->setDuration(Theme::FadeDurationMs);
        connect(m_fadeAnim, &QPropertyAnimation::finished, this, &LauncherWindow::onFadeFinished);

        if (!Config::instance().isFirstLaunch()) {
            refreshSidebar();
            m_contentStack->setCurrentIndex(PageEmpty);
        }
    }

private slots:
    void onFadeFinished()
    {
        if (m_pendingPage >= 0) {
            m_contentStack->setCurrentIndex(m_pendingPage);
            m_pendingPage = -1;
            m_fadeAnim->setStartValue(0.0);
            m_fadeAnim->setEndValue(1.0);
            m_fadeAnim->start();
        }
    }

private:
    // ── Setup ─────────────────────────────────────────────────────────────────

    void loadFont()
    {
        int id = QFontDatabase::addApplicationFontFromData(
            QByteArray(reinterpret_cast<const char *>(kMontserratTtf), kMontserratTtf_len));
        if (id >= 0) {
            QString family = QFontDatabase::applicationFontFamilies(id).at(0);
            qApp->setFont(QFont(family, Theme::FontBase));
        }
    }

    void applyDarkTheme()
    {
        qApp->setStyle("Fusion");
        QPalette dark;
        dark.setColor(QPalette::Window,          QColor(24, 24, 32));
        dark.setColor(QPalette::WindowText,      Qt::white);
        dark.setColor(QPalette::Base,            QColor(36, 36, 48));
        dark.setColor(QPalette::Text,            Qt::white);
        dark.setColor(QPalette::Button,          QColor(50, 50, 70));
        dark.setColor(QPalette::ButtonText,      Qt::white);
        dark.setColor(QPalette::Highlight,       QColor(184, 148, 74));
        dark.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(dark);
    }

    void fadeToPage(int page)
    {
        if (m_pendingPage < 0 && m_contentStack->currentIndex() == page)
            return;
        m_pendingPage = page;
        m_fadeAnim->stop();
        m_fadeAnim->setStartValue(m_contentOpacity->opacity());
        m_fadeAnim->setEndValue(0.0);
        m_fadeAnim->start();
    }

    // ── Sidebar ───────────────────────────────────────────────────────────────

    QWidget *buildSidebar()
    {
        QWidget *sidebar = new QWidget();
        sidebar->setFixedWidth(Theme::SidebarWidth);
        sidebar->setStyleSheet(QString("background-color: %1;").arg(Theme::BgDeep));

        QVBoxLayout *v = new QVBoxLayout(sidebar);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        // Logo
        QWidget *logoArea = new QWidget();
        logoArea->setStyleSheet("background: transparent;");
        QVBoxLayout *logoLayout = new QVBoxLayout(logoArea);
        logoLayout->setContentsMargins(18, 22, 18, 18);
        logoLayout->setSpacing(3);

        QLabel *logo = new QLabel("CEREKA");
        QFont logoFont = boldFont(Theme::FontLogo);
        logoFont.setLetterSpacing(QFont::AbsoluteSpacing, 5);
        logo->setFont(logoFont);
        logo->setStyleSheet(QString("color: %1;").arg(Theme::Gold));
        logoLayout->addWidget(logo);

        QLabel *tagline = new QLabel("Visual Novel Engine");
        tagline->setStyleSheet(QString("color: %1; font-size: 10px;").arg(Theme::TextDim));
        logoLayout->addWidget(tagline);
        v->addWidget(logoArea);
        v->addWidget(makeHRule());

        // Projects section
        QWidget *sec = new QWidget();
        sec->setStyleSheet("background: transparent;");
        QVBoxLayout *secLayout = new QVBoxLayout(sec);
        secLayout->setContentsMargins(18, 14, 18, 6);
        secLayout->addWidget(makeSectionLabel("PROJECTS"));
        v->addWidget(sec);

        // Project list
        m_sidebarList = new QListWidget();
        m_sidebarList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_sidebarList->setStyleSheet(QString(R"(
            QListWidget {
                background: transparent; border: none;
                outline: none; padding: 0 8px;
            }
            QListWidget::item {
                padding: 9px 10px; border-radius: %1px;
                margin: 1px 0; color: %2; font-size: 13px;
            }
            QListWidget::item:selected {
                background-color: %3; color: %4; font-weight: 600;
            }
            QListWidget::item:hover:!selected { background-color: %5; }
        )").arg(Theme::RadiusStd)
           .arg(Theme::TextMuted)
           .arg(Theme::GoldSelectedBg)
           .arg(Theme::GoldSelected)
           .arg(Theme::BgItemHover));
        connect(m_sidebarList, &QListWidget::itemClicked,
                this, &LauncherWindow::onSidebarProjectClicked);
        v->addWidget(m_sidebarList, 1);
        v->addWidget(makeHRule());

        // Bottom buttons
        QWidget *bottom = new QWidget();
        bottom->setStyleSheet("background: transparent;");
        QVBoxLayout *bottomLayout = new QVBoxLayout(bottom);
        bottomLayout->setContentsMargins(12, 12, 12, 16);
        bottomLayout->setSpacing(6);

        QPushButton *newBtn = new QPushButton("+ New Project");
        newBtn->setMinimumHeight(Theme::BtnHeightSidebar);
        newBtn->setStyleSheet(Theme::primaryBtn(Theme::RadiusStd));
        connect(newBtn, &QPushButton::clicked, this, &LauncherWindow::doNewProject);
        bottomLayout->addWidget(newBtn);

        m_changeFolderBtn = new QPushButton("Change Folder");
        m_changeFolderBtn->setMinimumHeight(Theme::BtnHeightGhost);
        m_changeFolderBtn->setStyleSheet(Theme::ghostBtn());
        connect(m_changeFolderBtn, &QPushButton::clicked,
                this, &LauncherWindow::doChangeProjectsDir);
        bottomLayout->addWidget(m_changeFolderBtn);
        v->addWidget(bottom);

        return sidebar;
    }

    // ── Welcome screen ────────────────────────────────────────────────────────

    QWidget *buildWelcomeScreen()
    {
        QWidget *w = new QWidget();
        w->setStyleSheet(QString("background-color: %1;").arg(Theme::BgBase));
        QVBoxLayout *v = new QVBoxLayout(w);
        v->setContentsMargins(40, 40, 40, 40);
        v->setSpacing(0);
        v->addStretch();

        QLabel *hero = new QLabel("CEREKA");
        QFont heroFont = boldFont(Theme::FontHero);
        heroFont.setLetterSpacing(QFont::AbsoluteSpacing, 10);
        hero->setFont(heroFont);
        hero->setStyleSheet(QString("color: %1;").arg(Theme::Gold));
        hero->setAlignment(Qt::AlignCenter);
        v->addWidget(hero);

        v->addSpacing(8);

        QLabel *tagline = new QLabel("Visual Novel Engine");
        tagline->setStyleSheet(
            QString("color: %1; font-size: 13px; letter-spacing: 3px;").arg(Theme::TextDim));
        tagline->setAlignment(Qt::AlignCenter);
        v->addWidget(tagline);

        v->addSpacing(48);

        QLabel *heading = new QLabel("Welcome!");
        heading->setFont(boldFont(Theme::FontHeading));
        heading->setStyleSheet(QString("color: %1;").arg(Theme::TextPrimary));
        heading->setAlignment(Qt::AlignCenter);
        v->addWidget(heading);

        v->addSpacing(10);

        QLabel *desc = new QLabel("Select a folder where all your projects will be stored.");
        desc->setStyleSheet(
            QString("color: %1; font-size: 14px;").arg(Theme::TextFaint));
        desc->setAlignment(Qt::AlignCenter);
        v->addWidget(desc);

        v->addSpacing(28);

        QPushButton *selectBtn = new QPushButton("Select Projects Folder");
        selectBtn->setMinimumHeight(Theme::BtnHeightLarge);
        selectBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        selectBtn->setStyleSheet(Theme::primaryBtn(Theme::RadiusLarge)
                                 + "QPushButton { padding: 12px 36px; font-size: 15px; }");
        connect(selectBtn, &QPushButton::clicked, this, &LauncherWindow::doSelectProjectsDir);
        v->addWidget(selectBtn, 0, Qt::AlignCenter);

        v->addStretch();
        return w;
    }

    // ── Empty state ───────────────────────────────────────────────────────────

    QWidget *buildEmptyState()
    {
        QWidget *w = new QWidget();
        w->setStyleSheet(QString("background-color: %1;").arg(Theme::BgBase));
        QVBoxLayout *v = new QVBoxLayout(w);
        v->setAlignment(Qt::AlignCenter);
        v->setSpacing(12);

        QLabel *glyph = new QLabel("◈");
        glyph->setStyleSheet(
            QString("color: %1; font-size: 56px;").arg(Theme::TextGlyph));
        glyph->setAlignment(Qt::AlignCenter);
        v->addWidget(glyph);

        m_emptyTitle = new QLabel("No project selected");
        m_emptyTitle->setFont(boldFont(Theme::FontTitle - 1));
        m_emptyTitle->setStyleSheet(QString("color: %1;").arg(Theme::TextDim));
        m_emptyTitle->setAlignment(Qt::AlignCenter);
        v->addWidget(m_emptyTitle);

        m_emptyDesc = new QLabel("Create a new project or select one from the sidebar.");
        m_emptyDesc->setStyleSheet(
            QString("color: %1; font-size: 13px;").arg(Theme::TextGlyph));
        m_emptyDesc->setAlignment(Qt::AlignCenter);
        v->addWidget(m_emptyDesc);

        return w;
    }

    void updateEmptyState()
    {
        m_emptyTitle->setText("No project selected");
        m_emptyDesc->setText("Create a new project or select one from the sidebar.");
        m_changeFolderBtn->setText("Change Folder");
    }

    // ── Project detail ────────────────────────────────────────────────────────

    QWidget *buildProjectDetail()
    {
        QWidget *proj = new QWidget();
        proj->setStyleSheet(QString("background-color: %1;").arg(Theme::BgBase));
        QVBoxLayout *v = new QVBoxLayout(proj);
        v->setContentsMargins(28, 24, 28, 24);
        v->setSpacing(0);

        // Header: title + rename
        QHBoxLayout *header = new QHBoxLayout();
        header->setSpacing(12);

        m_projTitleLabel = new QLabel();
        m_projTitleLabel->setFont(boldFont(Theme::FontTitle));
        m_projTitleLabel->setStyleSheet(QString("color: %1;").arg(Theme::TextPrimary));
        m_projTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        header->addWidget(m_projTitleLabel);

        QPushButton *renameBtn = new QPushButton("Rename");
        renameBtn->setMinimumHeight(Theme::BtnHeightMicro);
        renameBtn->setStyleSheet(Theme::ghostBtn(Theme::RadiusSmall)
                                 + "QPushButton { font-size: 11px; padding: 0 12px; }");
        connect(renameBtn, &QPushButton::clicked, this, &LauncherWindow::doRenameProject);
        header->addWidget(renameBtn);
        v->addLayout(header);

        v->addSpacing(16);
        v->addWidget(makeHRule(Theme::BgSurface));
        v->addSpacing(22);

        // Game actions (launch + package) — hidden when no game.cfg
        m_gameActionsWidget = new QWidget();
        m_gameActionsWidget->setStyleSheet("background: transparent;");
        QVBoxLayout *actionsV = new QVBoxLayout(m_gameActionsWidget);
        actionsV->setContentsMargins(0, 0, 0, 0);
        actionsV->setSpacing(10);

        QHBoxLayout *actions = new QHBoxLayout();
        actions->setSpacing(10);

        m_launchBtn = new QPushButton("▶  Launch Game");
        m_launchBtn->setMinimumHeight(Theme::BtnHeightAction);
        m_launchBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_launchBtn->setStyleSheet(Theme::primaryBtn()
                                   + "QPushButton { padding: 0 26px; font-size: 14px; }");
        connect(m_launchBtn, &QPushButton::clicked, this, &LauncherWindow::doLaunch);
        actions->addWidget(m_launchBtn);

        QPushButton *packageBtn = new QPushButton("Package  ▾");
        packageBtn->setMinimumHeight(Theme::BtnHeightAction);
        packageBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        packageBtn->setStyleSheet(Theme::secondaryBtn()
                                  + "QPushButton { padding: 0 20px; font-size: 13px; }");

        QMenu *pkgMenu = new QMenu(packageBtn);
        pkgMenu->setStyleSheet(Theme::menuStyle());
        pkgMenu->addAction("Package for Linux",   [this]() { doPackage("linux"); });
        pkgMenu->addAction("Package for Windows", [this]() { doPackage("windows"); });
        pkgMenu->addSeparator();
        pkgMenu->addAction("Package All", [this]() { doPackage(); });
        packageBtn->setMenu(pkgMenu);
        actions->addWidget(packageBtn);
        actions->addStretch();
        actionsV->addLayout(actions);

        m_statusLabel = new QLabel();
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(Theme::Gold));
        actionsV->addWidget(m_statusLabel);
        v->addWidget(m_gameActionsWidget);

        // Init prompt — shown when no game.cfg
        m_initWidget = new QWidget();
        m_initWidget->setStyleSheet("background: transparent;");
        QVBoxLayout *initV = new QVBoxLayout(m_initWidget);
        initV->setContentsMargins(0, 0, 0, 0);
        initV->setSpacing(10);

        QLabel *initDesc = new QLabel("This folder is not a Cereka project.");
        initDesc->setStyleSheet(
            QString("color: %1; font-size: 13px;").arg(Theme::TextFaint));
        initV->addWidget(initDesc);

        m_initBtn = new QPushButton("Initialize as Cereka Project");
        m_initBtn->setMinimumHeight(Theme::BtnHeightAction);
        m_initBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_initBtn->setStyleSheet(Theme::outlineBtn()
                                 + "QPushButton { padding: 0 24px; font-size: 13px; }");
        connect(m_initBtn, &QPushButton::clicked, this, &LauncherWindow::doInitProject);
        initV->addWidget(m_initBtn, 0, Qt::AlignLeft);
        v->addWidget(m_initWidget);

        v->addSpacing(20);
        v->addWidget(makeSectionLabel("OUTPUT"));
        v->addSpacing(8);

        m_log = new QTextEdit();
        m_log->setReadOnly(true);
        m_log->setPlaceholderText("Output will appear here...");
        m_log->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_log->setStyleSheet(QString(R"(
            QTextEdit {
                background-color: %1; border: 1px solid %2;
                border-radius: %3px; padding: 14px; font-size: 12px; color: %4;
            }
        )").arg(Theme::BgDeep).arg(Theme::BorderLog).arg(Theme::RadiusStd).arg(Theme::TextLog));
        v->addWidget(m_log, 1);

        return proj;
    }

    // ── Sidebar / project actions ─────────────────────────────────────────────

    void onSidebarProjectClicked(QListWidgetItem *item)
    {
        fs::path path = item->data(Qt::UserRole).toString().toStdString();
        if (!ProjectManager::instance().loadProject(path))
            return;

        m_projTitleLabel->setText(
            QString::fromStdString(ProjectManager::instance().currentTitle()));
        clearLog();
        updateLog();

        bool hasGame = ProjectManager::instance().currentHasGameCfg();
        m_gameActionsWidget->setVisible(hasGame);
        m_initWidget->setVisible(!hasGame);
        fadeToPage(PageProject);
    }

    void refreshSidebar()
    {
        m_sidebarList->clear();
        for (auto &p : ProjectManager::instance().listProjects()) {
            auto *item = new QListWidgetItem(QString::fromStdString(p.title));
            item->setData(Qt::UserRole, QString::fromStdString(p.path.string()));
            m_sidebarList->addItem(item);
        }
        updateEmptyState();
        m_changeFolderBtn->setToolTip(
            QString::fromStdString(Config::instance().projectsDir().string()));
    }

    void selectSidebarByPath(const fs::path &path)
    {
        for (int i = 0; i < m_sidebarList->count(); ++i) {
            fs::path p = m_sidebarList->item(i)->data(Qt::UserRole).toString().toStdString();
            if (p == path) {
                m_sidebarList->setCurrentRow(i);
                return;
            }
        }
    }

    void selectSidebarByName(const QString &name)
    {
        for (int i = 0; i < m_sidebarList->count(); ++i) {
            if (m_sidebarList->item(i)->text() == name) {
                m_sidebarList->setCurrentRow(i);
                return;
            }
        }
    }

    // ── Actions ───────────────────────────────────────────────────────────────

    void doSelectProjectsDir()
    {
        QString defaultPath;
#ifdef _WIN32
        defaultPath =
            QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first() +
            "\\Cereka";
#else
        defaultPath = QDir::homePath() + "/Games";
#endif
        QString path = QFileDialog::getExistingDirectory(
            this, "Select Projects Directory", defaultPath,
            QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);

        if (!path.isEmpty()) {
            fs::path dir(path.toStdString());
            if (!fs::exists(dir))
                fs::create_directories(dir);
            Config::instance().setProjectsDir(dir);
            refreshSidebar();
            fadeToPage(PageEmpty);
        }
    }

    void doNewProject()
    {
        QString name = QInputDialog::getText(this, "New Project", "Enter project name:");
        if (name.isEmpty())
            return;

        if (ProjectManager::instance().createProject(name.toStdString())) {
            refreshSidebar();
            selectSidebarByName(name);
            onSidebarProjectClicked(m_sidebarList->currentItem());
        } else {
            appendLog("[ERROR] Project already exists or creation failed.");
            updateLog();
        }
    }

    void doInitProject()
    {
        fs::path path = ProjectManager::instance().currentPath();
        if (ProjectManager::instance().initProject(path)) {
            m_projTitleLabel->setText(
                QString::fromStdString(ProjectManager::instance().currentTitle()));
            m_gameActionsWidget->setVisible(true);
            m_initWidget->setVisible(false);
            refreshSidebar();
            selectSidebarByPath(path);
        }
    }

    void doChangeProjectsDir()
    {
        QString path = QFileDialog::getExistingDirectory(
            this, "Select Projects Directory",
            QString::fromStdString(Config::instance().projectsDir().string()),
            QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);

        if (!path.isEmpty()) {
            fs::path dir(path.toStdString());
            if (!fs::exists(dir))
                fs::create_directories(dir);
            Config::instance().setProjectsDir(dir);
            refreshSidebar();
        }
    }

    void doRenameProject()
    {
        auto *item = m_sidebarList->currentItem();
        if (!item)
            return;

        QString currentName = item->text();
        QString newName = QInputDialog::getText(
            this, "Rename Project", "New name:", QLineEdit::Normal, currentName);

        if (newName.isEmpty() || newName == currentName)
            return;

        fs::path oldPath = item->data(Qt::UserRole).toString().toStdString();
        if (ProjectManager::instance().renameProject(oldPath, newName.toStdString())) {
            refreshSidebar();
            selectSidebarByName(newName);
            m_projTitleLabel->setText(newName);
        } else {
            appendLog("[ERROR] Rename failed. Project may already exist.");
            updateLog();
        }
    }

    void doLaunch()
    {
        if (s_busy.exchange(true))
            return;
        clearLog();
        updateLog();
        setProjectUiEnabled(false);
        m_statusLabel->setText("Running...");

        std::thread([this]() {
            std::string runner = findGameRunner();
            fs::path path = ProjectManager::instance().currentPath();
            appendLog("$ " + runner + " " + path.string());
            QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

            auto drainPipe = [this](auto readFn) {
                char buf[512];
                std::string partial;
                while (true) {
                    int n = readFn(buf, sizeof(buf) - 1);
                    if (n <= 0)
                        break;
                    buf[n] = '\0';
                    for (int i = 0; i < n; ++i) {
                        char c = buf[i];
                        if (c == '\r')
                            continue;
                        if (c == '\n') {
                            appendLog(partial);
                            partial.clear();
                            QMetaObject::invokeMethod(
                                this, [this]() { updateLog(); }, Qt::QueuedConnection);
                        } else {
                            partial += c;
                        }
                    }
                }
                if (!partial.empty()) {
                    appendLog(partial);
                    QMetaObject::invokeMethod(
                        this, [this]() { updateLog(); }, Qt::QueuedConnection);
                }
            };

#ifdef _WIN32
            SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
            HANDLE hRead, hWrite;
            CreatePipe(&hRead, &hWrite, &sa, 0);
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si{};
            PROCESS_INFORMATION pi{};
            si.cb        = sizeof(si);
            si.hStdOutput = hWrite;
            si.hStdError  = hWrite;
            si.dwFlags    = STARTF_USESTDHANDLES;

            std::string cmd = "\"" + runner + "\" \"" + path.string() + "\"";
            BOOL ok = CreateProcessA(NULL, (char *)cmd.c_str(), NULL, NULL, TRUE, 0,
                                     NULL, path.string().c_str(), &si, &pi);
            CloseHandle(hWrite);
            if (ok) {
                drainPipe([&](char *b, int sz) -> int {
                    DWORD n = 0;
                    return ReadFile(hRead, b, sz, &n, NULL) ? (int)n : -1;
                });
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            } else {
                appendLog("[ERROR] CreateProcess failed");
            }
            CloseHandle(hRead);
#else
            int pipefd[2];
            if (pipe(pipefd) != 0) {
                appendLog("[ERROR] pipe() failed");
            } else {
                pid_t pid = fork();
                if (pid == 0) {
                    ::close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    dup2(pipefd[1], STDERR_FILENO);
                    ::close(pipefd[1]);
                    chdir(path.string().c_str());
                    execlp(runner.c_str(), runner.c_str(), path.string().c_str(), nullptr);
                    _exit(127);
                } else if (pid > 0) {
                    ::close(pipefd[1]);
                    drainPipe([&](char *b, int sz) -> int {
                        return (int)read(pipefd[0], b, sz);
                    });
                    ::close(pipefd[0]);
                    waitpid(pid, nullptr, 0);
                } else {
                    appendLog("[ERROR] fork() failed");
                    ::close(pipefd[0]);
                    ::close(pipefd[1]);
                }
            }
#endif
            s_busy = false;
            QMetaObject::invokeMethod(this, [this]() {
                m_statusLabel->setText("");
                setProjectUiEnabled(true);
                updateLog();
            }, Qt::QueuedConnection);
        }).detach();
    }

    void doPackage(const std::string &filter = "")
    {
        if (s_busy.exchange(true))
            return;
        clearLog();
        updateLog();
        setProjectUiEnabled(false);
        m_statusLabel->setText("Packaging...");

        std::thread([this, filter]() {
            fs::path projectDir = ProjectManager::instance().currentPath();
            std::string gameName = ProjectManager::instance().currentTitle();
            for (char &c : gameName)
                if (c == ' ')
                    c = '_';

            appendLog("Packaging: " + projectDir.string());
            QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

            struct PlatformSpec {
                std::string name;
                std::string runtimeExe;
                std::string archiveSuffix;
            };
            static const PlatformSpec platforms[] = {
                {"linux",   "CerekaGame",     "-linux.tar.gz"},
                {"windows", "CerekaGame.exe", "-windows.zip"},
            };

            bool anyPackaged = false;

            for (auto &plat : platforms) {
                if (!filter.empty() && plat.name != filter)
                    continue;

                fs::path runtimeBin = runtimeDir(plat.name) / plat.runtimeExe;
                appendLog("Runtime: " + runtimeBin.string());
                if (!fs::exists(runtimeBin)) {
                    appendLog("[skip] Runtime not found");
                    QMetaObject::invokeMethod(
                        this, [this]() { updateLog(); }, Qt::QueuedConnection);
                    continue;
                }

                appendLog("--- " + plat.name + " ---");

                std::string stagingName = gameName + "-" + plat.name;
                fs::path stagingDir     = projectDir.parent_path() / stagingName;
                std::string gameExe     = (plat.name == "windows")
                                              ? gameName + ".exe"
                                              : gameName;

                std::error_code ec;
                fs::remove_all(stagingDir, ec);
                fs::create_directories(stagingDir, ec);
                if (ec) {
                    appendLog("[ERROR] Cannot create staging dir: " + ec.message());
                    QMetaObject::invokeMethod(
                        this, [this]() { updateLog(); }, Qt::QueuedConnection);
                    continue;
                }

                appendLog("Copying runtime as " + gameExe + "...");
                QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

                fs::copy_file(runtimeBin, stagingDir / gameExe,
                              fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    appendLog("[ERROR] Cannot copy runtime: " + ec.message());
                    fs::remove_all(stagingDir, ec);
                    QMetaObject::invokeMethod(
                        this, [this]() { updateLog(); }, Qt::QueuedConnection);
                    continue;
                }

                if (plat.name == "linux") {
                    fs::permissions(stagingDir / gameExe,
                                    fs::perms::owner_exec | fs::perms::group_exec |
                                        fs::perms::others_exec,
                                    fs::perm_options::add, ec);
                } else {
                    for (auto &e : fs::directory_iterator(runtimeDir("windows"), ec)) {
                        if (e.path().extension() == ".dll") {
                            fs::copy_file(e.path(), stagingDir / e.path().filename(),
                                          fs::copy_options::overwrite_existing, ec);
                            if (!ec)
                                appendLog("Copied " + e.path().filename().string());
                        }
                    }
                }

                appendLog("Copying project files...");
                QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

                std::function<void(const fs::path &, const fs::path &)> copyTree;
                copyTree = [&](const fs::path &src, const fs::path &dst) {
                    fs::create_directories(dst, ec);
                    for (auto &entry : fs::directory_iterator(src, ec)) {
                        if (entry.path().filename() == "saves")
                            continue;
                        if (entry.is_directory(ec)) {
                            copyTree(entry.path(), dst / entry.path().filename());
                        } else {
                            fs::copy_file(entry.path(), dst / entry.path().filename(),
                                          fs::copy_options::overwrite_existing, ec);
                            if (!ec)
                                appendLog("  + " +
                                          fs::relative(entry.path(), projectDir).string());
                        }
                    }
                };
                copyTree(projectDir, stagingDir);

                appendLog("Creating archive...");
                QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

                fs::path archivePath =
                    projectDir.parent_path() / (gameName + plat.archiveSuffix);
                int ret = 0;

                if (plat.name == "linux") {
                    std::string cmd = "tar czf \"" + archivePath.string() + "\" -C \"" +
                                      projectDir.parent_path().string() + "\" \"" +
                                      stagingName + "\" 2>&1";
                    ret = system(cmd.c_str());
                    if (ret != 0)
                        appendLog("[ERROR] tar failed");
                } else {
#ifdef _WIN32
                    std::string cmd =
                        "powershell -NoProfile -Command \"Compress-Archive -Force"
                        " -Path '" + stagingDir.string() +
                        "' -DestinationPath '" + archivePath.string() + "\" 2>&1";
                    ret = system(cmd.c_str());
                    if (ret != 0)
                        appendLog("[ERROR] Compress failed");
#else
                    if (system("which zip > /dev/null 2>&1") != 0) {
                        appendLog("[ERROR] zip not found. Install: sudo pacman -S zip");
                        fs::remove_all(stagingDir, ec);
                        QMetaObject::invokeMethod(
                            this, [this]() { updateLog(); }, Qt::QueuedConnection);
                        continue;
                    }
                    std::string cmd = "cd \"" + projectDir.parent_path().string() +
                                      "\" && zip -r \"" + archivePath.string() +
                                      "\" \"" + stagingName + "\" 2>&1";
                    ret = system(cmd.c_str());
                    if (ret != 0)
                        appendLog("[ERROR] zip failed");
#endif
                }

                appendLog("Cleaning up...");
                QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);
                fs::remove_all(stagingDir, ec);

                if (ret == 0) {
                    appendLog("[OK] " + archivePath.filename().string());
                    anyPackaged = true;
                }
                QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);
            }

            if (!anyPackaged)
                appendLog("[ERROR] No platforms packaged.");

            s_busy = false;
            QMetaObject::invokeMethod(this, [this]() {
                m_statusLabel->setText("");
                setProjectUiEnabled(true);
                updateLog();
            }, Qt::QueuedConnection);
        }).detach();
    }

    void updateLog()
    {
        QString text = QString::fromStdString(s_log);
        if (m_log->toPlainText() != text)
            m_log->setPlainText(text);
    }

    void setProjectUiEnabled(bool enabled)
    {
        m_launchBtn->setEnabled(enabled);
    }

    // ── Members ───────────────────────────────────────────────────────────────

    QListWidget   *m_sidebarList       = nullptr;
    QPushButton   *m_changeFolderBtn   = nullptr;

    QStackedWidget *m_contentStack     = nullptr;
    QLabel         *m_emptyTitle       = nullptr;
    QLabel         *m_emptyDesc        = nullptr;

    QLabel        *m_projTitleLabel    = nullptr;
    QWidget       *m_gameActionsWidget = nullptr;
    QPushButton   *m_launchBtn         = nullptr;
    QLabel        *m_statusLabel       = nullptr;
    QWidget       *m_initWidget        = nullptr;
    QPushButton   *m_initBtn           = nullptr;
    QTextEdit     *m_log               = nullptr;

    QGraphicsOpacityEffect *m_contentOpacity = nullptr;
    QPropertyAnimation     *m_fadeAnim       = nullptr;
    int m_pendingPage = -1;
};

#include "main.moc"

int main(int argc, char **argv)
{
    std::string logPath = (selfExeDir() / "cereka_launcher.log").string();
    if (FILE *lf = std::fopen(logPath.c_str(), "w")) {
        std::fclose(lf);
        std::freopen(logPath.c_str(), "w", stdout);
        std::freopen(logPath.c_str(), "a", stderr);
        std::setvbuf(stdout, nullptr, _IOLBF, 0);
        std::setvbuf(stderr, nullptr, _IOLBF, 0);
    }

    QApplication app(argc, argv);
    LauncherWindow w;
    w.show();
    return app.exec();
}
