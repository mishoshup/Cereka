#include "config.hpp"

#ifdef _WIN32
#    include <windows.h>
#else
#    include <unistd.h>
#endif

#include <fstream>

namespace fs = std::filesystem;

static fs::path homeDir()
{
#ifdef _WIN32
    char path[MAX_PATH] = {};
    GetEnvironmentVariableA("APPDATA", path, MAX_PATH);
    return fs::path(path);
#else
    const char *home = getenv("HOME");
    return fs::path(home ? home : ".");
#endif
}

Config::Config() : m_configPath(homeDir() / ".cereka" / "cereka.cfg")
{
    load();
}

Config &Config::instance()
{
    static Config instance;
    return instance;
}

void Config::load()
{
    if (!fs::exists(m_configPath.parent_path()))
        fs::create_directories(m_configPath.parent_path());

    if (!fs::exists(m_configPath))
        return;

    std::ifstream f(m_configPath);
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("projects_dir=") == 0 && line.size() > 13) {
            m_projectsDir = fs::path(line.substr(13));
        }
    }
}

void Config::save()
{
    if (!fs::exists(m_configPath.parent_path()))
        fs::create_directories(m_configPath.parent_path());

    std::ofstream f(m_configPath);
    if (!m_projectsDir.empty())
        f << "projects_dir=" << m_projectsDir.string() << "\n";
}

void Config::setProjectsDir(const fs::path &dir)
{
    m_projectsDir = dir;
    save();
}
