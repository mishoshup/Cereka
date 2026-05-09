#pragma once

#include "project_metadata.hpp"

#include <chrono>
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
    bool createProject(const std::string &name,
                       const std::string &templateName = "Default");
    bool initProject(const fs::path &path);
    bool renameProject(const fs::path &oldPath,
                       const std::string &newName);
    bool loadProject(const fs::path &projectPath);
    bool currentHasGameCfg() const;

    std::string currentTitle() const
    {
        return m_currentTitle;
    }
    fs::path currentPath() const
    {
        return m_currentPath;
    }
    const ProjectMetadata &currentMetadata() const
    {
        return m_metadata;
    }

    /// Read the `entry` field from game.cfg.
    std::string currentEntry() const;

    // ── Play-time tracking ──────────────────────────────────────────────────

    /// Start a play session clock (called when game launches).
    void startPlaySession();

    /// Add elapsed play time to metadata and persist to disk.
    void endPlaySession();

    /// Save current metadata (.cereka/project.json) to disk.
    void saveMetadata();

   private:
    ProjectManager() = default;
    ~ProjectManager() = default;
    ProjectManager(const ProjectManager &) = delete;
    ProjectManager &operator=(const ProjectManager &) = delete;

    void loadGameCfg(const fs::path &projectPath);

    fs::path m_currentPath;
    std::string m_currentTitle;
    ProjectMetadata m_metadata;

    // Play session state
    std::chrono::steady_clock::time_point m_playSessionStart;
    bool m_sessionActive = false;
};
