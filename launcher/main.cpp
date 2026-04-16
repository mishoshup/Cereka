#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMetaObject>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include "config.hpp"
#include "project_manager.hpp"

#ifdef _WIN32
#    include <windows.h>
#else
#    include <unistd.h>
#endif

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

namespace fs = std::filesystem;

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

static fs::path selfExeDir()
{
#ifdef _WIN32
    char path[MAX_PATH] = {};
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return fs::path(path).parent_path();
#else
    char exePath[2048] = {};
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0)
        return fs::path(std::string(exePath, len)).parent_path();
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

class LauncherWindow : public QWidget {
    Q_OBJECT
   public:
    LauncherWindow()
    {
        setWindowTitle("Cereka Launcher");
        setMinimumSize(700, 550);
        resize(900, 650);

        applyDarkTheme();

        m_layout = new QStackedLayout(this);
        setLayout(m_layout);

        createWelcomeScreen();
        createHomeScreen();
        createProjectScreen();

        m_layout->setCurrentIndex(0);

        if (!Config::instance().isFirstLaunch()) {
            m_layout->setCurrentIndex(1);
            refreshProjectList();
        }
    }

    void applyDarkTheme()
    {
        qApp->setStyle("Fusion");
        QPalette dark;
        dark.setColor(QPalette::Window, QColor(24, 24, 32));
        dark.setColor(QPalette::WindowText, Qt::white);
        dark.setColor(QPalette::Base, QColor(36, 36, 48));
        dark.setColor(QPalette::Text, Qt::white);
        dark.setColor(QPalette::Button, QColor(50, 50, 70));
        dark.setColor(QPalette::ButtonText, Qt::white);
        dark.setColor(QPalette::Highlight, QColor(184, 148, 74));
        dark.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(dark);
    }

    void refreshProjectList()
    {
        m_projectsList->clear();
        for (auto &p : ProjectManager::instance().listProjects()) {
            auto item = new QListWidgetItem(QString::fromStdString(p.title));
            item->setData(Qt::UserRole, QString::fromStdString(p.path.string()));
            m_projectsList->addItem(item);
        }

        m_projectsDirLabel->setText(
            QString::fromStdString(Config::instance().projectsDir().string()));
    }

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
        QString path = QFileDialog::getExistingDirectory(this,
                                                         "Select Projects Directory",
                                                         defaultPath,
                                                         QFileDialog::ShowDirsOnly |
                                                             QFileDialog::DontUseNativeDialog);

        if (!path.isEmpty()) {
            fs::path dir(path.toStdString());
            if (!fs::exists(dir)) {
                fs::create_directories(dir);
            }
            Config::instance().setProjectsDir(dir);
            m_layout->setCurrentIndex(1);
            refreshProjectList();
        }
    }

    void doNewProject()
    {
        QString name = QInputDialog::getText(this, "New Project", "Enter project name:");
        if (name.isEmpty())
            return;

        if (ProjectManager::instance().createProject(name.toStdString())) {
            refreshProjectList();
            showProjectScreen();
        }
        else {
            appendLog("[ERROR] Project already exists or creation failed.");
            updateLog();
        }
    }

    void doOpenProject()
    {
        auto item = m_projectsList->currentItem();
        if (!item)
            return;

        fs::path path = item->data(Qt::UserRole).toString().toStdString();
        if (ProjectManager::instance().loadProject(path)) {
            showProjectScreen();
        }
    }

    void doRenameProject()
    {
        auto item = m_projectsList->currentItem();
        if (!item)
            return;

        QString currentName = item->text();
        QString newName = QInputDialog::getText(
            this, "Rename Project", "Enter new name:", QLineEdit::Normal, currentName);

        if (newName.isEmpty() || newName == currentName)
            return;

        fs::path oldPath = item->data(Qt::UserRole).toString().toStdString();
        if (ProjectManager::instance().renameProject(oldPath, newName.toStdString())) {
            refreshProjectList();
        }
        else {
            appendLog("[ERROR] Rename failed. Project may already exist.");
            updateLog();
        }
    }

    void doChangeProjectsDir()
    {
        QString path = QFileDialog::getExistingDirectory(
            this,
            "Select Projects Directory",
            QString::fromStdString(Config::instance().projectsDir().string()),
            QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);

        if (!path.isEmpty()) {
            fs::path dir(path.toStdString());
            if (!fs::exists(dir)) {
                fs::create_directories(dir);
            }
            Config::instance().setProjectsDir(dir);
            refreshProjectList();
        }
    }

    void doBack()
    {
        m_layout->setCurrentIndex(1);
        refreshProjectList();
    }

    void doLaunch()
    {
        if (s_busy.exchange(true))
            return;
        clearLog();
        updateLog();
        setProjectUiEnabled(false);
        m_statusLabel->setText("Launching...");

        std::thread([this]() {
            std::string runner = findGameRunner();
            fs::path path = ProjectManager::instance().currentPath();
            appendLog("$ " + runner + " " + path.string());
            QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

            pid_t pid = 0;
#ifdef _WIN32
            STARTUPINFOA si{};
            PROCESS_INFORMATION pi{};
            si.cb = sizeof(si);
            std::string cmd = "\"" + runner + "\" \"" + path.string() + "\"";
            CreateProcessA(NULL,
                           (char *)cmd.c_str(),
                           NULL,
                           NULL,
                           FALSE,
                           0,
                           NULL,
                           path.string().c_str(),
                           &si,
                           &pi);
            appendLog("[OK] Game running");
#else
            pid = fork();
            if (pid == 0) {
                chdir(path.string().c_str());
                execlp(runner.c_str(), runner.c_str(), path.string().c_str(), nullptr);
                _exit(127);
            }
            else if (pid > 0) {
                appendLog("[OK] Game running");
            }
            else {
                appendLog("[ERROR] fork() failed");
            }
#endif

            s_busy = false;
            QMetaObject::invokeMethod(
                this,
                [this]() {
                    m_statusLabel->setText("");
                    setProjectUiEnabled(true);
                    updateLog();
                },
                Qt::QueuedConnection);
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
            fs::path dirStr = ProjectManager::instance().currentPath();
            std::string title = ProjectManager::instance().currentTitle();
            std::string gameName = title;
            for (char &c : gameName)
                if (c == ' ')
                    c = '_';

            appendLog("Packaging: " + dirStr.string());
            QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

            struct PlatformSpec {
                std::string name;
                std::string exe;
                std::string suffix;
            };
            static const PlatformSpec platforms[] = {
                {"linux", "CerekaGame", "-linux.tar.gz"},
                {"windows", "CerekaGame.exe", "-windows.zip"},
            };

            bool anyPackaged = false;

            for (auto &plat : platforms) {
                if (!filter.empty() && plat.name != filter)
                    continue;

                fs::path runtimeBin = runtimeDir(plat.name) / plat.exe;
                appendLog("Runtime path: " + runtimeBin.string());
                if (!fs::exists(runtimeBin)) {
                    appendLog("[skip] Runtime not found");
                    QMetaObject::invokeMethod(
                        this, [this]() { updateLog(); }, Qt::QueuedConnection);
                    continue;
                }

                appendLog("--- " + plat.name + " ---");

                fs::path stagingDir = dirStr.parent_path() /
                                      (gameName + "-" + plat.name + "-dist");
                fs::path gameSubDir = stagingDir / gameName;

                std::error_code ec;
                fs::remove_all(stagingDir, ec);
                fs::create_directories(stagingDir, ec);
                if (ec) {
                    appendLog("[ERROR] Cannot create staging dir: " + ec.message());
                    QMetaObject::invokeMethod(
                        this, [this]() { updateLog(); }, Qt::QueuedConnection);
                    continue;
                }

                appendLog("Copying runtime...");
                QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

                fs::copy_file(
                    runtimeBin, stagingDir / plat.exe, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    appendLog("[ERROR] Cannot copy runtime: " + ec.message());
                    fs::remove_all(stagingDir, ec);
                    QMetaObject::invokeMethod(
                        this, [this]() { updateLog(); }, Qt::QueuedConnection);
                    continue;
                }
                appendLog("Copied " + plat.exe);

                fs::create_directories(gameSubDir, ec);

                if (plat.name == "windows") {
                    for (auto &e : fs::directory_iterator(runtimeDir("windows"), ec)) {
                        if (e.path().extension() == ".dll") {
                            fs::copy_file(e.path(),
                                          stagingDir / e.path().filename(),
                                          fs::copy_options::overwrite_existing,
                                          ec);
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
                        }
                        else {
                            fs::copy_file(entry.path(),
                                          dst / entry.path().filename(),
                                          fs::copy_options::overwrite_existing,
                                          ec);
                            if (!ec)
                                appendLog("  + " + fs::relative(entry.path(), dirStr).string());
                        }
                    }
                };
                copyTree(dirStr, gameSubDir);

                appendLog("Creating archive...");
                QMetaObject::invokeMethod(this, [this]() { updateLog(); }, Qt::QueuedConnection);

                fs::path archivePath = dirStr.parent_path() / (gameName + plat.suffix);
                std::string stagingName = stagingDir.filename().string();
                int ret = 0;

                if (plat.name == "linux") {
                    {
                        fs::path launchSh = stagingDir / "launch.sh";
                        std::ofstream f(launchSh);
                        f << "#!/bin/bash\n"
                          << "cd \"$(dirname \"$0\")\"\n"
                          << "./CerekaGame \"" << gameName << "\"\n";
                        fs::permissions(launchSh,
                                        fs::perms::owner_exec | fs::perms::group_exec |
                                            fs::perms::others_exec,
                                        fs::perm_options::add,
                                        ec);
                    }
                    appendLog("Created launch.sh");

                    std::string cmd = "tar czf \"" + archivePath.string() + "\" -C \"" +
                                      dirStr.parent_path().string() + "\" \"" + stagingName +
                                      "\" 2>&1";
                    ret = system(cmd.c_str());
                    if (ret != 0)
                        appendLog("[ERROR] tar failed");
                }
                else {
                    {
                        std::ofstream f(stagingDir / "launch.bat");
                        f << "@echo off\r\n"
                          << "cd /d \"%~dp0\"\r\n"
                          << "CerekaGame.exe \"" << gameName << "\"\r\n";
                    }
                    appendLog("Created launch.bat");

#ifdef _WIN32
                    std::string cmd =
                        "powershell -NoProfile -Command \"Compress-Archive -Force"
                        " -Path '" +
                        stagingDir.string() + "' -DestinationPath '" + archivePath.string() +
                        "\" 2>&1";
                    ret = system(cmd.c_str());
                    if (ret != 0)
                        appendLog("[ERROR] Compress failed");
#else
                    ret = system("which zip > /dev/null 2>&1");
                    if (ret != 0) {
                        appendLog("[ERROR] zip not found. Install: sudo pacman -S zip");
                        fs::remove_all(stagingDir, ec);
                        QMetaObject::invokeMethod(
                            this, [this]() { updateLog(); }, Qt::QueuedConnection);
                        continue;
                    }
                    std::string cmd = "cd \"" + dirStr.parent_path().string() + "\" && zip -r \"" +
                                      archivePath.string() + "\" \"" + stagingName + "\" 2>&1";
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
            QMetaObject::invokeMethod(
                this,
                [this]() {
                    m_statusLabel->setText("");
                    setProjectUiEnabled(true);
                    updateLog();
                },
                Qt::QueuedConnection);
        }).detach();
    }

    void updateLog()
    {
        QString text = QString::fromStdString(s_log);
        if (m_homeLog->toPlainText() != text)
            m_homeLog->setPlainText(text);
        if (m_projLog->toPlainText() != text)
            m_projLog->setPlainText(text);
    }

    void showProjectScreen()
    {
        m_layout->setCurrentIndex(2);
        m_projTitleLabel->setText(
            QString::fromStdString(ProjectManager::instance().currentTitle()));
        updateLog();
    }

    void setProjectUiEnabled(bool enabled)
    {
        m_launchBtn->setEnabled(enabled);
    }

   private:
    void createWelcomeScreen()
    {
        QWidget *welcome = new QWidget();
        welcome->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *v = new QVBoxLayout(welcome);
        v->setSpacing(16);
        v->setContentsMargins(40, 40, 40, 40);

        v->addStretch();

        QLabel *hero = new QLabel("CEREKA");
        QFont heroFont;
        heroFont.setPointSize(40);
        heroFont.setBold(true);
        hero->setFont(heroFont);
        hero->setStyleSheet("color: #b8944a; letter-spacing: 10px;");
        v->addWidget(hero, 0, Qt::AlignCenter);

        QLabel *tagline = new QLabel("Visual Novel Engine");
        tagline->setStyleSheet("color: #777; font-size: 14px; letter-spacing: 3px;");
        v->addWidget(tagline, 0, Qt::AlignCenter);

        v->addSpacing(40);

        QLabel *welcomeText = new QLabel("Welcome!");
        welcomeText->setStyleSheet("color: white; font-size: 20px; font-weight: 600;");
        v->addWidget(welcomeText, 0, Qt::AlignCenter);

        QLabel *descText = new QLabel("Select a folder where all your projects will be stored.");
        descText->setStyleSheet("color: #999; font-size: 14px;");
        descText->setAlignment(Qt::AlignCenter);
        v->addWidget(descText, 0, Qt::AlignCenter);

        v->addSpacing(24);

        QPushButton *selectBtn = new QPushButton("Select Projects Folder");
        selectBtn->setMinimumHeight(48);
        selectBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        selectBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #b8944a; color: white; border: none;
                border-radius: 8px; padding: 12px 32px; font-size: 15px; font-weight: 600;
            }
            QPushButton:hover { background-color: #c8a55c; }
        )");
        connect(selectBtn, &QPushButton::clicked, this, &LauncherWindow::doSelectProjectsDir);
        v->addWidget(selectBtn, 0, Qt::AlignCenter);

        v->addStretch();

        m_layout->addWidget(welcome);
    }

    void createHomeScreen()
    {
        QWidget *home = new QWidget();
        home->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *v = new QVBoxLayout(home);
        v->setSpacing(16);
        v->setContentsMargins(40, 32, 40, 32);

        QLabel *hero = new QLabel("CEREKA");
        QFont heroFont;
        heroFont.setPointSize(28);
        heroFont.setBold(true);
        hero->setFont(heroFont);
        hero->setStyleSheet("color: #b8944a; letter-spacing: 8px;");
        v->addWidget(hero, 0, Qt::AlignCenter);

        QLabel *tagline = new QLabel("Visual Novel Engine");
        tagline->setStyleSheet("color: #777; font-size: 13px; letter-spacing: 2px;");
        v->addWidget(tagline, 0, Qt::AlignCenter);
        v->addSpacing(20);

        QLabel *dirLabel = new QLabel("Projects Folder:");
        dirLabel->setStyleSheet(
            "color: #888; font-size: 11px; font-weight: 500; text-transform: uppercase; "
            "letter-spacing: 2px;");
        v->addWidget(dirLabel);

        QHBoxLayout *dirRow = new QHBoxLayout();
        dirRow->setSpacing(8);
        m_projectsDirLabel = new QLabel();
        m_projectsDirLabel->setStyleSheet("color: #aaa; font-size: 13px;");
        m_projectsDirLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        dirRow->addWidget(m_projectsDirLabel, 1);

        QPushButton *changeBtn = new QPushButton("Change");
        changeBtn->setFixedWidth(80);
        changeBtn->setMinimumHeight(32);
        changeBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        changeBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #3a3a4a; color: white; border: none;
                border-radius: 6px; padding: 0 12px; font-size: 12px;
            }
            QPushButton:hover { background-color: #4a4a5a; }
        )");
        connect(changeBtn, &QPushButton::clicked, this, &LauncherWindow::doChangeProjectsDir);
        dirRow->addWidget(changeBtn);
        v->addLayout(dirRow);

        QLabel *projectsLabel = new QLabel("Projects");
        projectsLabel->setStyleSheet(
            "color: #888; font-size: 11px; font-weight: 500; text-transform: uppercase; "
            "letter-spacing: 2px;");
        v->addWidget(projectsLabel);

        m_projectsList = new QListWidget();
        m_projectsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_projectsList->setStyleSheet(R"(
            QListWidget {
                background-color: #2a2a3a; border: 1px solid #3a3a4a;
                border-radius: 8px; padding: 6px; outline: none;
            }
            QListWidget::item {
                padding: 10px 14px; border-radius: 4px; margin: 2px 0;
            }
            QListWidget::item:selected {
                background-color: #b8944a; color: white;
            }
            QListWidget::item:hover:!selected { background-color: #333344; }
        )");
        connect(
            m_projectsList, &QListWidget::itemDoubleClicked, this, &LauncherWindow::doOpenProject);
        connect(
            m_projectsList, &QListWidget::itemClicked, this, &LauncherWindow::onProjectSelected);
        v->addWidget(m_projectsList, 1);

        QHBoxLayout *actions = new QHBoxLayout();
        actions->setSpacing(8);

        QPushButton *newBtn = new QPushButton("+ New Project");
        newBtn->setMinimumHeight(40);
        newBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        newBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #b8944a; color: white; border: none;
                border-radius: 6px; padding: 8px 16px; font-size: 13px; font-weight: 600;
            }
            QPushButton:hover { background-color: #c8a55c; }
        )");
        connect(newBtn, &QPushButton::clicked, this, &LauncherWindow::doNewProject);
        actions->addWidget(newBtn);

        QPushButton *renameBtn = new QPushButton("Rename Project");
        renameBtn->setMinimumHeight(40);
        renameBtn->setEnabled(false);
        renameBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        renameBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #3a3a4a; color: white; border: none;
                border-radius: 6px; padding: 8px 14px; font-size: 13px;
            }
            QPushButton:hover:enabled { background-color: #4a4a5a; }
            QPushButton:disabled { background-color: #2a2a3a; color: #555; }
        )");
        connect(renameBtn, &QPushButton::clicked, this, &LauncherWindow::doRenameProject);
        m_renameBtn = renameBtn;
        actions->addWidget(renameBtn);

        QPushButton *openBtn = new QPushButton("Open Project");
        openBtn->setMinimumHeight(40);
        openBtn->setEnabled(false);
        openBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        openBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #3a3a4a; color: white; border: none;
                border-radius: 6px; padding: 8px 14px; font-size: 13px;
            }
            QPushButton:hover:enabled { background-color: #4a4a5a; }
            QPushButton:disabled { background-color: #2a2a3a; color: #555; }
        )");
        connect(openBtn, &QPushButton::clicked, this, &LauncherWindow::doOpenProject);
        m_openBtn = openBtn;
        actions->addWidget(openBtn);

        actions->addStretch();
        v->addLayout(actions);

        m_homeLog = new QTextEdit();
        m_homeLog->setReadOnly(true);
        m_homeLog->setMaximumHeight(70);
        m_homeLog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_homeLog->setPlaceholderText("Activity log...");
        m_homeLog->setStyleSheet(R"(
            QTextEdit {
                background-color: #2a2a3a; border: 1px solid #3a3a4a;
                border-radius: 8px; padding: 10px; font-size: 12px; color: #999;
            }
        )");
        v->addWidget(m_homeLog);

        m_layout->addWidget(home);
    }

    void onProjectSelected()
    {
        bool hasSelection = m_projectsList->currentItem() != nullptr;
        m_renameBtn->setEnabled(hasSelection);
        m_openBtn->setEnabled(hasSelection);
    }

    void createProjectScreen()
    {
        QWidget *proj = new QWidget();
        proj->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout *v = new QVBoxLayout(proj);
        v->setContentsMargins(20, 20, 20, 20);
        v->setSpacing(12);

        QHBoxLayout *header = new QHBoxLayout();
        header->setSpacing(8);

        QPushButton *backBtn = new QPushButton("< Back");
        backBtn->setFixedWidth(70);
        backBtn->setMinimumHeight(30);
        backBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        backBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #3a3a4a; color: white; border: none;
                border-radius: 6px; padding: 0 12px; font-size: 12px;
            }
            QPushButton:hover { background-color: #4a4a5a; }
        )");
        connect(backBtn, &QPushButton::clicked, this, &LauncherWindow::doBack);
        header->addWidget(backBtn);

        QFont headerFont;
        headerFont.setBold(true);
        headerFont.setPointSize(13);
        QLabel *logo = new QLabel("CEREKA");
        logo->setFont(headerFont);
        logo->setStyleSheet("color: #b8944a; letter-spacing: 3px;");
        header->addWidget(logo);

        m_projTitleLabel = new QLabel();
        m_projTitleLabel->setFont(headerFont);
        m_projTitleLabel->setStyleSheet("color: white;");
        m_projTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        header->addWidget(m_projTitleLabel);
        header->addStretch();

        QPushButton *renameBtn = new QPushButton("Rename Project");
        renameBtn->setMinimumHeight(30);
        renameBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        renameBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #3a3a4a; color: white; border: none;
                border-radius: 6px; padding: 0 12px; font-size: 12px;
            }
            QPushButton:hover { background-color: #4a4a5a; }
        )");
        connect(renameBtn, &QPushButton::clicked, this, &LauncherWindow::doRenameProject);
        header->addWidget(renameBtn);
        v->addLayout(header);

        QFrame *sep = new QFrame();
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #3a3a4a;");
        v->addWidget(sep);

        QHBoxLayout *actions = new QHBoxLayout();
        actions->setSpacing(8);

        m_launchBtn = new QPushButton("Launch Game");
        m_launchBtn->setMinimumHeight(42);
        m_launchBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_launchBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #b8944a; color: white; border: none;
                border-radius: 6px; padding: 10px 20px; font-size: 14px; font-weight: 600;
            }
            QPushButton:hover { background-color: #c8a55c; }
        )");
        connect(m_launchBtn, &QPushButton::clicked, this, &LauncherWindow::doLaunch);
        actions->addWidget(m_launchBtn);

        QPushButton *packageBtn = new QPushButton("Package");
        packageBtn->setMinimumHeight(42);
        packageBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        QMenu *menu = new QMenu(packageBtn);
        QAction *linuxAct = menu->addAction("Package for Linux");
        QAction *winAct = menu->addAction("Package for Windows");
        menu->addSeparator();
        QAction *allAct = menu->addAction("Package All");
        connect(linuxAct, &QAction::triggered, [this]() { doPackage("linux"); });
        connect(winAct, &QAction::triggered, [this]() { doPackage("windows"); });
        connect(allAct, &QAction::triggered, [this]() { doPackage(); });
        packageBtn->setMenu(menu);
        packageBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #3a3a4a; color: white; border: none;
                border-radius: 6px; padding: 10px 16px; font-size: 13px;
            }
            QPushButton:hover { background-color: #4a4a5a; }
            QPushButton::menu-indicator { subcontrol-position: right center; subcontrol-origin: padding; right: 10px; }
        )");
        actions->addWidget(packageBtn);
        actions->addStretch();
        v->addLayout(actions);

        m_statusLabel = new QLabel();
        m_statusLabel->setStyleSheet("color: #888; font-size: 12px;");
        v->addWidget(m_statusLabel, 0, Qt::AlignCenter);

        m_projLog = new QTextEdit();
        m_projLog->setReadOnly(true);
        m_projLog->setPlaceholderText("Activity log...");
        m_projLog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_projLog->setStyleSheet(R"(
            QTextEdit {
                background-color: #2a2a3a; border: 1px solid #3a3a4a;
                border-radius: 8px; padding: 10px; font-size: 12px; color: #999;
            }
        )");
        v->addWidget(m_projLog, 1);

        m_layout->addWidget(proj);
    }

    QStackedLayout *m_layout;

    QLabel *m_projectsDirLabel;
    QListWidget *m_projectsList;
    QPushButton *m_renameBtn;
    QPushButton *m_openBtn;
    QTextEdit *m_homeLog;

    QLabel *m_projTitleLabel;
    QPushButton *m_launchBtn;
    QLabel *m_statusLabel;
    QTextEdit *m_projLog;
};

#include "main.moc"

int main(int argc,
         char **argv)
{
    QApplication app(argc, argv);
    LauncherWindow w;
    w.show();
    return app.exec();
}
