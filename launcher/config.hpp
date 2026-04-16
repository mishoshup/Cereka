#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class Config {
   public:
    static Config &instance();

    fs::path projectsDir() const
    {
        return m_projectsDir;
    }
    void setProjectsDir(const fs::path &dir);

    bool isFirstLaunch() const
    {
        return m_projectsDir.empty();
    }

   private:
    Config();
    ~Config() = default;
    Config(const Config &) = delete;
    Config &operator=(const Config &) = delete;

    void load();
    void save();

    fs::path m_configPath;
    fs::path m_projectsDir;
};
