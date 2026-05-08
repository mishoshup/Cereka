#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// ── ProjectMetadata ──────────────────────────────────────────────────────────
//
// Persistent metadata stored in `.cereka/project.json` inside each project
// directory. Created with a UUID on initial project creation and updated on
// each load (lastOpened).
//
struct ProjectMetadata {
    std::string uuid;
    std::string title;
    std::string lastOpened;   // ISO 8601 UTC timestamp
    int playTimeSeconds = 0;
    std::string engineVersion;

    // Read `.cereka/project.json` from the given project root.
    // Returns false if the file does not exist or is unparseable.
    bool load(const fs::path &projectDir);

    // Write `.cereka/project.json` to the given project root.
    // Creates the `.cereka/` subdirectory if it does not exist.
    bool save(const fs::path &projectDir) const;

    // Factory: create a fresh metadata record with a new UUID and the
    // given title.  lastOpened is set to now; playTimeSeconds starts at 0.
    static ProjectMetadata create(const std::string &title);
};
