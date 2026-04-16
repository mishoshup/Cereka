#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class ProjectManager {
   public:
    static ProjectManager &instance();

    struct ProjectInfo {
        std::string name;
        fs::path path;
        std::string title;
    };

    std::vector<ProjectInfo> listProjects() const;
    bool createProject(const std::string &name);
    bool renameProject(const fs::path &oldPath,
                       const std::string &newName);
    bool loadProject(const fs::path &projectPath);

    std::string currentTitle() const
    {
        return m_currentTitle;
    }
    fs::path currentPath() const
    {
        return m_currentPath;
    }

   private:
    ProjectManager() = default;
    ~ProjectManager() = default;
    ProjectManager(const ProjectManager &) = delete;
    ProjectManager &operator=(const ProjectManager &) = delete;

    void loadGameCfg(const fs::path &projectPath);

    fs::path m_currentPath;
    std::string m_currentTitle;
};
