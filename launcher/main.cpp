#include "embedded_assets.h"

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#ifdef _WIN32
#    include <windows.h>
#else
#    include <limits.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// Embedded project templates (see launcher/templates.hpp)
// ============================================================================

#include "templates.hpp"

// ============================================================================
// State
// ============================================================================

enum class Screen { Home, Project };

static Screen s_screen = Screen::Home;
static fs::path s_selectedDir;
static bool s_hasGameCfg = false;
static std::string s_gameTitle;
static char s_titleEdit[256]   = "";
static char s_folderRename[256] = "";

// Directory text input
static char s_dirInput[2048] = "";

// Directory browser popup
static bool s_openBrowser = false;
static fs::path s_browserPath;
static std::vector<std::string> s_browserDirs;

// Log / busy
static std::string s_log;
static std::mutex s_logMutex;
static std::atomic<bool> s_busy{false};
static bool s_scrollToBottom = false;

// Signal from background thread to reload project on main thread
static std::atomic<bool> s_reloadPending{false};

// Recent projects
static std::vector<std::string> s_recents;
static fs::path s_recentsFile;

// ============================================================================
// Log helpers
// ============================================================================

static void appendLog(const std::string &text)
{
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_log += text;
    s_scrollToBottom = true;
}

static void clearLog()
{
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_log.clear();
}

// ============================================================================
// Recents
// ============================================================================

static void saveRecents()
{
    std::ofstream f(s_recentsFile);
    for (auto &p : s_recents)
        f << p << "\n";
}

static void loadRecents()
{
    const char *home = getenv("HOME");
    s_recentsFile = fs::path(home ? home : ".") / ".cereka_recent";
    std::ifstream f(s_recentsFile);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && fs::is_directory(line))
            s_recents.push_back(line);
    }
}

static void addRecent(const std::string &path)
{
    auto it = std::find(s_recents.begin(), s_recents.end(), path);
    if (it != s_recents.end())
        s_recents.erase(it);
    s_recents.insert(s_recents.begin(), path);
    if (s_recents.size() > 8)
        s_recents.resize(8);
    saveRecents();
}

// ============================================================================
// Directory browser
// ============================================================================

static void refreshBrowser(const fs::path &p)
{
    s_browserPath = p;
    s_browserDirs.clear();
    std::error_code ec;
    for (auto &e : fs::directory_iterator(p, ec)) {
        if (!e.is_directory(ec))
            continue;
        std::string name = e.path().filename().string();
        if (!name.empty() && name[0] != '.')
            s_browserDirs.push_back(name);
    }
    std::sort(s_browserDirs.begin(), s_browserDirs.end());
}

// ============================================================================
// Project loading (main-thread only)
// ============================================================================

static void saveTitleToGameCfg(const std::string &newTitle)
{
    fs::path cfgPath = s_selectedDir / "game.cfg";
    std::vector<std::string> lines;
    {
        std::ifstream f(cfgPath);
        std::string line;
        while (std::getline(f, line))
            lines.push_back(line);
    }
    bool replaced = false;
    for (auto &l : lines) {
        if (!replaced && l.find("title") == 0 && l.find('=') != std::string::npos) {
            l = "title      = " + newTitle;
            replaced = true;
        }
    }
    if (!replaced)
        lines.push_back("title      = " + newTitle);
    {
        std::ofstream f(cfgPath);
        for (auto &l : lines)
            f << l << "\n";
    }
    s_gameTitle = newTitle;
}

static void loadProject(const fs::path &dir)
{
    s_selectedDir = dir;
    strncpy(s_dirInput, dir.string().c_str(), sizeof(s_dirInput) - 1);

    fs::path cfgPath = dir / "game.cfg";
    s_hasGameCfg = fs::exists(cfgPath);
    s_gameTitle.clear();

    if (s_hasGameCfg) {
        std::ifstream f(cfgPath);
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("title") == 0) {
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    auto trim = [](std::string s) -> std::string {
                        size_t a = s.find_first_not_of(" \t\r\n");
                        size_t b = s.find_last_not_of(" \t\r\n");
                        return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
                    };
                    s_gameTitle = trim(line.substr(eq + 1));
                }
                break;
            }
        }
        strncpy(s_titleEdit, s_gameTitle.c_str(), sizeof(s_titleEdit) - 1);
        addRecent(dir.string());
    }

    clearLog();
    s_screen = Screen::Project;
}

// ============================================================================
// Script list
// ============================================================================

static std::vector<std::string> getScripts()
{
    std::vector<std::string> out;
    fs::path scriptsDir = s_selectedDir / "assets" / "scripts";
    std::error_code ec;
    for (auto &e : fs::directory_iterator(scriptsDir, ec))
        if (e.path().extension() == ".crka")
            out.push_back(e.path().filename().string());
    return out;
}

// ============================================================================
// Find CerekaGame binary (sibling of this executable)
// ============================================================================

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
    if (fs::exists(r)) return r.string();
    r = selfExeDir() / "CerekaGame.exe";
    return fs::exists(r) ? r.string() : "CerekaGame.exe";
#else
    fs::path r = runtimeDir("linux") / "CerekaGame";
    if (fs::exists(r)) return r.string();
    r = selfExeDir() / "CerekaGame";
    return fs::exists(r) ? r.string() : "CerekaGame";
#endif
}

// ============================================================================
// Background actions
// ============================================================================

static bool writeAsset(const fs::path &path,
                       const unsigned char *data,
                       unsigned int len,
                       const std::string &label)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        appendLog("[ERROR] Cannot write " + label + "\n");
        return false;
    }
    f.write(reinterpret_cast<const char *>(data), len);
    appendLog("  + " + label + "\n");
    return true;
}

static void doCreateProject()
{
    if (s_busy.exchange(true))
        return;
    clearLog();

    std::thread([dir = s_selectedDir]() {
        appendLog("Scaffolding project in: " + dir.string() + "\n\n");

        std::error_code ec;
        for (const char *sub : {"assets/scripts",
                                "assets/bg",
                                "assets/characters",
                                "assets/fonts",
                                "assets/sounds",
                                "assets/ui"})
        {
            fs::path p = dir / sub;
            fs::create_directories(p, ec);
            if (ec) {
                appendLog("[ERROR] Cannot create " + p.string() + ": " + ec.message() + "\n");
                s_busy = false;
                return;
            }
            appendLog("  + " + std::string(sub) + "/\n");
        }

        // Text files
        {
            std::ofstream f(dir / "game.cfg");
            if (!f) {
                appendLog("[ERROR] Cannot write game.cfg\n");
                s_busy = false;
                return;
            }
            f << kGameCfgTemplate;
            appendLog("  + game.cfg\n");
        }
        {
            std::ofstream f(dir / "assets" / "scripts" / "ui.crka");
            if (!f) {
                appendLog("[ERROR] Cannot write ui.crka\n");
                s_busy = false;
                return;
            }
            f << kUiScriptTemplate;
            appendLog("  + assets/scripts/ui.crka\n");
        }
        {
            std::ofstream f(dir / "assets" / "scripts" / "scene_two.crka");
            if (!f) {
                appendLog("[ERROR] Cannot write scene_two.crka\n");
                s_busy = false;
                return;
            }
            f << kSceneTwoTemplate;
            appendLog("  + assets/scripts/scene_two.crka\n");
        }
        {
            std::ofstream f(dir / "assets" / "scripts" / "main.crka");
            if (!f) {
                appendLog("[ERROR] Cannot write main.crka\n");
                s_busy = false;
                return;
            }
            f << kMainScriptTemplate;
            appendLog("  + assets/scripts/main.crka\n");
        }

        // Placeholder binary assets
        if (!writeAsset(dir / "assets" / "bg" / "placeholder_bg.png",
                        kBgPng,
                        kBgPng_len,
                        "assets/bg/placeholder_bg.png"))
        {
            s_busy = false;
            return;
        }
        if (!writeAsset(dir / "assets" / "characters" / "placeholder_char.png",
                        kCharPng,
                        kCharPng_len,
                        "assets/characters/placeholder_char.png"))
        {
            s_busy = false;
            return;
        }
        if (!writeAsset(dir / "assets" / "sounds" / "placeholder_sfx.wav",
                        kSfxWav,
                        kSfxWav_len,
                        "assets/sounds/placeholder_sfx.wav"))
        {
            s_busy = false;
            return;
        }
        if (!writeAsset(dir / "assets" / "sounds" / "placeholder_bgm.wav",
                        kBgmWav,
                        kBgmWav_len,
                        "assets/sounds/placeholder_bgm.wav"))
        {
            s_busy = false;
            return;
        }
        if (!writeAsset(dir / "assets" / "fonts" / "Montserrat-Medium.ttf",
                        kMontserratTtf,
                        kMontserratTtf_len,
                        "assets/fonts/Montserrat-Medium.ttf"))
        {
            s_busy = false;
            return;
        }

        appendLog("\n[OK] Project created. Click Launch to run.\n");
        s_reloadPending = true;
        s_busy = false;
    }).detach();
}

// filter: "" = all platforms, "linux" or "windows" = single platform
static void doPackage(const std::string &filter = "")
{
    if (s_busy.exchange(true))
        return;
    clearLog();

    std::thread([dir = s_selectedDir, title = s_gameTitle, filter]() {
        // Derive a filesystem-safe name from the game title
        std::string gameName = title.empty() ? dir.filename().string() : title;
        for (char &c : gameName)
            if (c == ' ')
                c = '_';

        appendLog("Packaging: " + dir.string() + "\n\n");

        struct PlatformSpec {
            std::string name;
            std::string gameExe;
            std::string archiveSuffix;
        };
        static const PlatformSpec platforms[] = {
            { "linux",   "CerekaGame",     "-linux.tar.gz" },
            { "windows", "CerekaGame.exe", "-windows.zip"  },
        };

        bool anyPackaged = false;

        for (auto &plat : platforms) {
            if (!filter.empty() && plat.name != filter)
                continue;

            fs::path runtimeBin = runtimeDir(plat.name) / plat.gameExe;
            if (!fs::exists(runtimeBin)) {
                appendLog("  [skip] runtimes/" + plat.name + "/ not found\n");
                continue;
            }

            appendLog("--- " + plat.name + " ---\n");

            fs::path stagingDir = dir.parent_path() / (gameName + "-" + plat.name + "-dist");
            fs::path gameSubDir = stagingDir / gameName;

            std::error_code ec;
            fs::remove_all(stagingDir, ec);
            fs::create_directories(gameSubDir, ec);
            if (ec) {
                appendLog("[ERROR] Cannot create staging dir: " + ec.message() + "\n");
                continue;
            }

            // Copy game binary
            fs::copy_file(runtimeBin, stagingDir / plat.gameExe,
                          fs::copy_options::overwrite_existing, ec);
            if (ec) {
                appendLog("[ERROR] Cannot copy " + plat.gameExe + ": " + ec.message() + "\n");
                fs::remove_all(stagingDir, ec);
                continue;
            }
            appendLog("  + " + plat.gameExe + "\n");

            // Copy all DLLs from the runtime dir (Windows only)
            if (plat.name == "windows") {
                for (auto &e : fs::directory_iterator(runtimeDir("windows"), ec)) {
                    if (e.path().extension() == ".dll") {
                        fs::copy_file(e.path(), stagingDir / e.path().filename(),
                                      fs::copy_options::overwrite_existing, ec);
                        if (!ec)
                            appendLog("  + " + e.path().filename().string() + "\n");
                    }
                }
            }

            // Recursively copy project files (skip saves/)
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
                                      fs::relative(entry.path(), dir).string() + "\n");
                    }
                }
            };
            copyTree(dir, gameSubDir);

            fs::path archivePath = dir.parent_path() / (gameName + plat.archiveSuffix);
            std::string stagingName = stagingDir.filename().string();
            int ret = 0;

            if (plat.name == "linux") {
                // Write launch.sh
                {
                    fs::path launchSh = stagingDir / "launch.sh";
                    std::ofstream f(launchSh);
                    f << "#!/bin/bash\n"
                      << "cd \"$(dirname \"$0\")\"\n"
                      << "./CerekaGame \"" << gameName << "\"\n";
                    fs::permissions(launchSh,
                                    fs::perms::owner_exec | fs::perms::group_exec |
                                        fs::perms::others_exec,
                                    fs::perm_options::add, ec);
                }
                appendLog("  + launch.sh\n");

                std::string cmd = "tar czf \"" + archivePath.string() +
                                  "\" -C \"" + dir.parent_path().string() +
                                  "\" \"" + stagingName + "\"";
                appendLog("\n$ " + cmd + "\n");
                ret = system(cmd.c_str());
                if (ret != 0)
                    appendLog("[ERROR] tar failed (exit " + std::to_string(ret) + ")\n");

            } else {
                // Write launch.bat
                {
                    std::ofstream f(stagingDir / "launch.bat");
                    f << "@echo off\r\n"
                      << "cd /d \"%~dp0\"\r\n"
                      << "CerekaGame.exe \"" << gameName << "\"\r\n";
                }
                appendLog("  + launch.bat\n");

#ifdef _WIN32
                std::string cmd =
                    "powershell -NoProfile -Command \"Compress-Archive -Force"
                    " -Path '" + stagingDir.string() +
                    "' -DestinationPath '" + archivePath.string() + "'\"";
                appendLog("\n$ " + cmd + "\n");
                ret = system(cmd.c_str());
                if (ret != 0)
                    appendLog("[ERROR] Compress-Archive failed (exit " +
                              std::to_string(ret) + ")\n");
#else
                std::string cmd = "zip -r \"" + archivePath.string() +
                                  "\" \"" + stagingName + "\"" +
                                  " -C \"" + dir.parent_path().string() + "\"";
                // zip doesn't support -C; change dir first
                cmd = "cd \"" + dir.parent_path().string() +
                      "\" && zip -r \"" + archivePath.string() +
                      "\" \"" + stagingName + "\"";
                appendLog("\n$ " + cmd + "\n");
                ret = system(cmd.c_str());
                if (ret != 0)
                    appendLog("[ERROR] zip failed (exit " + std::to_string(ret) +
                              "). Install zip: sudo pacman -S zip  or  sudo apt install zip\n");
#endif
            }

            fs::remove_all(stagingDir, ec);

            if (ret == 0) {
                appendLog("[OK] " + archivePath.string() + "\n\n");
                anyPackaged = true;
            }
        }

        if (!anyPackaged)
            appendLog("\n[ERROR] No runtimes found. Build CerekaGame for at least one platform first.\n");

        s_busy = false;
    }).detach();
}

static void doLaunch()
{
    if (s_busy.exchange(true))
        return;

    clearLog();

    std::thread([dir = s_selectedDir]() {
        std::string runner = findGameRunner();
        std::string dirStr = dir.string();

#ifdef _WIN32
        std::string cmdLine = "\"" + runner + "\" \"" + dirStr + "\"";
        appendLog("$ " + cmdLine + "\n\n");

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);

        if (!CreateProcessA(
                NULL,
                cmdLine.data(),
                NULL, NULL,
                FALSE,
                0,
                NULL,
                dirStr.c_str(),     // working directory = project dir
                &si, &pi))
        {
            DWORD err = GetLastError();
            char msg[256] = {};
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, err, 0, msg, sizeof(msg), NULL);
            appendLog("[ERROR] CreateProcess failed (" + std::to_string(err) + "): " +
                      std::string(msg));
            s_busy = false;
            return;
        }

        CloseHandle(pi.hThread);

        // Wait up to 1 s — if the process dies that fast it crashed on startup
        if (WaitForSingleObject(pi.hProcess, 1000) == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            appendLog("[ERROR] Game exited immediately (exit code " +
                      std::to_string(exitCode) +
                      "). Check for missing DLLs next to the executable.\n");
        } else {
            appendLog("[OK] Game running (PID " + std::to_string(pi.dwProcessId) + ")\n");
        }

        CloseHandle(pi.hProcess);

#else
        // Linux / macOS: fork + exec so the launcher isn't blocked
        appendLog("$ \"" + runner + "\" \"" + dirStr + "\"\n\n");

        pid_t pid = fork();
        if (pid < 0) {
            appendLog("[ERROR] fork() failed\n");
            s_busy = false;
            return;
        }

        if (pid == 0) {
            // child
            if (chdir(dirStr.c_str()) != 0) { /* best-effort */ }
            execlp(runner.c_str(), runner.c_str(), dirStr.c_str(), (char *)nullptr);
            _exit(127);     // exec failed
        }

        // parent — wait up to 1 s to detect an immediate crash
        std::this_thread::sleep_for(std::chrono::seconds(1));
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            appendLog("[ERROR] Game exited immediately (exit code " +
                      std::to_string(code) + ").\n");
        } else {
            appendLog("[OK] Game running (PID " + std::to_string(pid) + ")\n");
        }
#endif

        s_busy = false;
    }).detach();
}

// ============================================================================
// Draw log panel (shared between screens)
// ============================================================================

static void drawLogPanel(float w,
                         float h)
{
    ImGui::BeginChild("##log", ImVec2(w, h), true);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Output");
    ImGui::Separator();

    ImGui::BeginChild("##logtext", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lock(s_logMutex);
        const char *p = s_log.c_str();
        while (*p) {
            const char *nl = strchr(p, '\n');
            size_t len = nl ? (size_t)(nl - p) : strlen(p);
            std::string line(p, len);
            bool isErr = line.find("error:") != std::string::npos ||
                         line.find("[ERROR]") != std::string::npos ||
                         line.find("[FAILED]") != std::string::npos;
            bool isOk = line.find("[OK]") != std::string::npos;
            if (isErr)
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "%s", line.c_str());
            else if (isOk)
                ImGui::TextColored(ImVec4(0.4f, 1.f, 0.5f, 1.f), "%s", line.c_str());
            else
                ImGui::TextUnformatted(line.c_str());
            p += len + (nl ? 1 : 0);
            if (!nl)
                break;
        }
    }
    if (s_scrollToBottom) {
        ImGui::SetScrollHereY(1.f);
        s_scrollToBottom = false;
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

// ============================================================================
// Home screen
// ============================================================================

static void drawHome(ImGuiStyle &style)
{
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.f), "CEREKA LAUNCHER");
    ImGui::SameLine(0, 12);
    ImGui::TextDisabled("Visual Novel Engine");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Game Directory:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.f);
    ImGui::InputText("##dirInput", s_dirInput, sizeof(s_dirInput));
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        const char *home = getenv("HOME");
        refreshBrowser(fs::path(home ? home : "/"));
        s_openBrowser = true;
        ImGui::OpenPopup("##dirbrowser");
    }
    ImGui::Spacing();

    if (ImGui::Button("  Open  ", ImVec2(90, 0))) {
        fs::path p = fs::path(s_dirInput).lexically_normal();
        if (!p.empty() && fs::is_directory(p))
            loadProject(p);
        else
            appendLog("[ERROR] Not a valid directory: " + p.string() + "\n");
    }

    // Recent projects
    if (!s_recents.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Recent Projects");
        ImGui::Spacing();
        for (auto &r : s_recents) {
            std::string label = fs::path(r).filename().string() + "##" + r;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.f));
            if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(200, 0))) {
                loadProject(fs::path(r));
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("%s", r.c_str());
        }
    }

    // Error log area
    float avail = ImGui::GetContentRegionAvail().y;
    if (!s_log.empty() && avail > 50) {
        ImGui::Spacing();
        ImGui::Separator();
        drawLogPanel(0, avail);
    }

    // Directory browser popup
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Always);
    if (ImGui::BeginPopupModal(
            "##dirbrowser", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
    {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.f, 1.f), "Select Directory");
        ImGui::Separator();

        // Current path + Up button
        ImGui::TextDisabled("%s", s_browserPath.string().c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
        if (ImGui::SmallButton("^ Up") && s_browserPath.has_parent_path())
            refreshBrowser(s_browserPath.parent_path());

        ImGui::Spacing();
        ImGui::BeginChild("##dirs", ImVec2(0, 310), true);
        if (s_browserDirs.empty()) {
            ImGui::TextDisabled("(no subdirectories)");
        }
        else {
            for (auto &d : s_browserDirs) {
                if (ImGui::Selectable(("> " + d).c_str()))
                    refreshBrowser(s_browserPath / d);
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();

        // New folder row
        static char s_newFolderName[256] = "";
        static std::string s_newFolderError;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110);
        ImGui::InputTextWithHint("##newfolder", "New folder name...", s_newFolderName, sizeof(s_newFolderName));
        ImGui::SameLine();
        if (ImGui::Button("Create", ImVec2(100, 0)) && s_newFolderName[0] != '\0') {
            fs::path newDir = s_browserPath / s_newFolderName;
            std::error_code ec;
            fs::create_directories(newDir, ec);
            if (ec) {
                s_newFolderError = "Failed: " + ec.message();
            } else {
                s_newFolderError.clear();
                s_newFolderName[0] = '\0';
                refreshBrowser(s_browserPath);
            }
        }
        if (!s_newFolderError.empty())
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "%s", s_newFolderError.c_str());

        ImGui::Spacing();
        if (ImGui::Button("Select This Folder", ImVec2(160, 0))) {
            strncpy(s_dirInput, s_browserPath.string().c_str(), sizeof(s_dirInput) - 1);
            s_openBrowser = false;
            s_newFolderName[0] = '\0';
            s_newFolderError.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            s_openBrowser = false;
            s_newFolderName[0] = '\0';
            s_newFolderError.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================================
// Project screen
// ============================================================================

static void drawProject(ImGuiStyle &style)
{
    bool busy = s_busy.load();

    // Header
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.f), "CEREKA LAUNCHER");
    ImGui::SameLine(0, 12);
    if (s_hasGameCfg) {
        // Editable game title — saves to game.cfg on Enter or unfocus
        ImGui::SetNextItemWidth(200.f);
        bool commit = ImGui::InputText("##title_edit", s_titleEdit, sizeof(s_titleEdit),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
        bool lostFocus = ImGui::IsItemDeactivatedAfterEdit();
        if ((commit || lostFocus) && s_titleEdit[0] != '\0') {
            std::string newTitle(s_titleEdit);
            if (newTitle != s_gameTitle)
                saveTitleToGameCfg(newTitle);
        }
    } else {
        ImGui::TextUnformatted(s_selectedDir.filename().string().c_str());
    }
    ImGui::SameLine(0, 8);
    ImGui::TextDisabled("(%s)", s_selectedDir.string().c_str());
    ImGui::SameLine(0, 8);
    if (ImGui::SmallButton("rename folder")) {
        strncpy(s_folderRename, s_selectedDir.filename().string().c_str(),
                sizeof(s_folderRename) - 1);
        ImGui::OpenPopup("##rename_folder");
    }
    if (ImGui::BeginPopup("##rename_folder")) {
        ImGui::TextUnformatted("New folder name:");
        ImGui::SetNextItemWidth(240.f);
        bool ok = ImGui::InputText("##folder_new_name", s_folderRename,
                                   sizeof(s_folderRename),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        ok = ok || ImGui::Button("OK");
        if (ok && s_folderRename[0] != '\0') {
            fs::path newDir = s_selectedDir.parent_path() / s_folderRename;
            std::error_code ec;
            fs::rename(s_selectedDir, newDir, ec);
            if (!ec) {
                std::string oldPath = s_selectedDir.string();
                auto it = std::find(s_recents.begin(), s_recents.end(), oldPath);
                if (it != s_recents.end()) *it = newDir.string();
                saveRecents();
                loadProject(newDir);
            } else {
                appendLog("[ERROR] Rename failed: " + ec.message() + "\n");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (busy)
        ImGui::BeginDisabled();

    if (!s_hasGameCfg) {
        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.3f, 1.f),
                           "No game.cfg found. Create a new project here?");
        ImGui::Spacing();
        if (ImGui::Button("  Create Game  ", ImVec2(130, 0)))
            doCreateProject();
    }
    else {
        if (ImGui::Button("  Launch Game  ", ImVec2(130, 0)))
            doLaunch();
        ImGui::SameLine(0, 8);
        // Package split-button: left half runs all platforms, arrow opens menu
        if (ImGui::Button("  Package  ", ImVec2(88, 0)))
            doPackage();
        ImGui::SameLine(0, 1);
        if (ImGui::ArrowButton("##pkg_arrow", ImGuiDir_Down))
            ImGui::OpenPopup("pkg_menu");
        if (ImGui::BeginPopup("pkg_menu")) {
            if (ImGui::MenuItem("All platforms"))  doPackage();
            if (ImGui::MenuItem("Linux only"))     doPackage("linux");
            if (ImGui::MenuItem("Windows only"))   doPackage("windows");
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine(0, 12);
    if (busy) {
        ImGui::EndDisabled();
        ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "working...");
        ImGui::SameLine();
    }

    if (!busy && ImGui::SmallButton("Clear log"))
        clearLog();

    ImGui::SameLine();
    if (ImGui::SmallButton("< Back")) {
        if (!busy) {
            s_screen = Screen::Home;
            clearLog();
        }
    }

    ImGui::Separator();

    // Two-column layout
    float leftW = 200.f;
    float rightW = ImGui::GetContentRegionAvail().x - leftW - style.ItemSpacing.x;
    float panelH = ImGui::GetContentRegionAvail().y;

    // Left: scripts
    ImGui::BeginChild("##scripts", ImVec2(leftW, panelH), true);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Scripts");
    ImGui::Separator();
    auto scripts = getScripts();
    if (scripts.empty()) {
        ImGui::TextDisabled("(none found)");
    }
    else {
        for (auto &s : scripts) {
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextUnformatted(s.c_str());
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right: output log
    drawLogPanel(rightW, panelH);
}

// ============================================================================
// Main
// ============================================================================

int main(int,
         char **)
{
    loadRecents();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Cereka Launcher", 900, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load Montserrat from embedded data — no file on disk needed
    io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char *>(kMontserratTtf), (int)kMontserratTtf_len, 16.f);

    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 6.f;
    style.FrameRounding = 4.f;
    style.ItemSpacing = ImVec2(8, 6);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.18f, 1.f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.30f, 1.f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.33f, 0.45f, 1.f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.22f, 0.30f, 1.f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.33f, 0.45f, 1.f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.14f, 1.f);

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT)
                running = false;
        }

        // Pick up reload signal from background create thread
        if (s_reloadPending.exchange(false))
            loadProject(s_selectedDir);

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)winW, (float)winH));
        ImGui::Begin("##root",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);

        switch (s_screen) {
            case Screen::Home:
                drawHome(style);
                break;
            case Screen::Project:
                drawProject(style);
                break;
        }

        ImGui::End();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 18, 18, 24, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
