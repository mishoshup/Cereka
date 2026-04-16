#include "project_manager.hpp"
#include "config.hpp"
#include "embedded_assets.h"
#include "templates.hpp"

#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;

static std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static bool writeAsset(const fs::path &dest,
                       const unsigned char *data,
                       unsigned int len)
{
    std::ofstream f(dest, std::ios::binary);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char *>(data), len);
    return f.good();
}

ProjectManager &ProjectManager::instance()
{
    static ProjectManager instance;
    return instance;
}

std::vector<ProjectManager::ProjectInfo> ProjectManager::listProjects() const
{
    std::vector<ProjectInfo> projects;
    const fs::path &projectsDir = Config::instance().projectsDir();

    if (!fs::exists(projectsDir) || !fs::is_directory(projectsDir))
        return projects;

    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(projectsDir, ec)) {
        if (!entry.is_directory(ec))
            continue;

        ProjectInfo info;
        info.path = entry.path();
        info.name = entry.path().filename().string();

        fs::path cfgPath = info.path / "game.cfg";
        if (fs::exists(cfgPath)) {
            std::ifstream f(cfgPath);
            std::string line;
            while (std::getline(f, line)) {
                if (line.find("title") == 0 && line.find('=') != std::string::npos) {
                    size_t eq = line.find('=');
                    info.title = trim(line.substr(eq + 1));
                    break;
                }
            }
        }

        if (info.title.empty())
            info.title = info.name;

        projects.push_back(std::move(info));
    }

    std::sort(projects.begin(), projects.end(), [](const ProjectInfo &a, const ProjectInfo &b) {
        return a.name < b.name;
    });

    return projects;
}

bool ProjectManager::createProject(const std::string &name)
{
    fs::path projectsDir = Config::instance().projectsDir();
    fs::path projectPath = projectsDir / name;

    if (fs::exists(projectPath))
        return false;

    std::error_code ec;

    fs::create_directories(projectPath / "assets/scripts", ec);
    fs::create_directories(projectPath / "assets/bg", ec);
    fs::create_directories(projectPath / "assets/characters", ec);
    fs::create_directories(projectPath / "assets/fonts", ec);
    fs::create_directories(projectPath / "assets/sounds", ec);
    fs::create_directories(projectPath / "assets/ui", ec);

    {
        std::string cfgContent = R"(# Cereka game configuration
# -----------------------------------------------
# title      : window title shown in the taskbar
# width/height: resolution in pixels
# fullscreen  : true or false
# entry       : path to the first script to run
# -----------------------------------------------
title      = )" + name + R"(
width      = 1280
height     = 720
fullscreen = false
entry      = assets/scripts/main.crka
)";

        std::ofstream cfgFile((projectPath / "game.cfg").string());
        cfgFile << cfgContent;
    }

    {
        std::ofstream f2((projectPath / "assets/scripts/ui.crka").string());
        f2 << kUiScriptTemplate;
    }

    {
        std::ofstream f3((projectPath / "assets/scripts/scene_two.crka").string());
        f3 << kSceneTwoTemplate;
    }

    {
        std::ofstream f4((projectPath / "assets/scripts/main.crka").string());
        f4 << kMainScriptTemplate;
    }

    if (!writeAsset(projectPath / "assets/bg/placeholder_bg.png", kBgPng, kBgPng_len))
        return false;
    if (!writeAsset(
            projectPath / "assets/characters/placeholder_char.png", kCharPng, kCharPng_len))
        return false;
    if (!writeAsset(projectPath / "assets/sounds/placeholder_sfx.wav", kSfxWav, kSfxWav_len))
        return false;
    if (!writeAsset(projectPath / "assets/sounds/placeholder_bgm.wav", kBgmWav, kBgmWav_len))
        return false;
    if (!writeAsset(projectPath / "assets/fonts/Montserrat-Medium.ttf",
                    kMontserratTtf,
                    kMontserratTtf_len))
        return false;

    return loadProject(projectPath);
}

bool ProjectManager::renameProject(const fs::path &oldPath,
                                   const std::string &newName)
{
    fs::path newPath = oldPath.parent_path() / newName;

    if (fs::exists(newPath) && newPath != oldPath)
        return false;

    std::error_code ec;
    fs::rename(oldPath, newPath, ec);
    if (ec)
        return false;

    fs::path cfgPath = newPath / "game.cfg";
    if (fs::exists(cfgPath)) {
        std::ifstream in(cfgPath);
        std::vector<std::string> lines;
        std::string line;
        bool found = false;

        while (std::getline(in, line)) {
            if (line.find("title") == 0 && line.find('=') != std::string::npos) {
                size_t eq = line.find('=');
                std::string before = line.substr(0, eq + 1);
                lines.push_back(before + " " + newName);
                found = true;
            }
            else {
                lines.push_back(line);
            }
        }
        in.close();

        if (!found) {
            lines.insert(lines.begin(), "title = " + newName);
        }

        std::ofstream out(cfgPath);
        for (auto &l : lines)
            out << l << "\n";
    }

    if (m_currentPath == oldPath) {
        m_currentPath = newPath;
        m_currentTitle = newName;
    }

    return true;
}

bool ProjectManager::loadProject(const fs::path &projectPath)
{
    m_currentPath = projectPath;
    m_currentTitle = projectPath.filename().string();

    fs::path cfgPath = projectPath / "game.cfg";
    if (fs::exists(cfgPath)) {
        loadGameCfg(cfgPath);
    }

    return true;
}

void ProjectManager::loadGameCfg(const fs::path &cfgPath)
{
    std::ifstream f(cfgPath);
    std::string line;

    while (std::getline(f, line)) {
        if (line.find("title") == 0 && line.find('=') != std::string::npos) {
            size_t eq = line.find('=');
            m_currentTitle = trim(line.substr(eq + 1));
            break;
        }
    }
}
