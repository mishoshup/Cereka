#include "Cereka/Cereka.hpp"
#include "Cereka/exceptions.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Minimal key = value config parser
// Lines starting with # are comments. Inline comments not supported.
// ---------------------------------------------------------------------------
static std::unordered_map<std::string, std::string> parseConfig(const std::string &path)
{
    std::unordered_map<std::string, std::string> cfg;
    std::ifstream f(path);
    if (!f) return cfg;

    std::string line;
    while (std::getline(f, line)) {
        // strip comments and whitespace
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
        };
        line = trim(line);
        if (line.empty()) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        cfg[key] = val;
    }
    return cfg;
}

// ---------------------------------------------------------------------------
// Entry point
//
// Usage:
//   CerekaGame                      — uses current working directory
//   CerekaGame /path/to/game        — explicit project directory
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    // Determine project root
    std::string projectRoot;
    if (argc >= 2) {
        projectRoot = fs::absolute(argv[1]).lexically_normal().string();
    } else {
        projectRoot = fs::current_path().lexically_normal().string();
    }

    // Change working directory so all asset paths resolve correctly
    std::error_code ec;
    fs::current_path(projectRoot, ec);
    if (ec) {
        std::cerr << "[CerekaGame] Cannot set working dir to: " << projectRoot << "\n";
        return 1;
    }

    // Load game.cfg
    auto cfg = parseConfig("game.cfg");
    if (cfg.empty()) {
        std::cerr << "[CerekaGame] No game.cfg found in: " << projectRoot << "\n";
        return 1;
    }

    std::string title      = cfg.count("title")      ? cfg["title"]      : "Cereka Game";
    int         width      = cfg.count("width")       ? std::stoi(cfg["width"])  : 1280;
    int         height     = cfg.count("height")      ? std::stoi(cfg["height"]) : 720;
    bool        fullscreen = cfg.count("fullscreen")  ? (cfg["fullscreen"] == "true") : false;
    std::string entry      = cfg.count("entry")       ? cfg["entry"]      : "assets/scripts/main.crka";

    // Run
    cereka::CerekaEngine engine;

    if (!engine.InitGame(title.c_str(), width, height, fullscreen)) {
        std::cerr << "[CerekaGame] Failed to init engine\n";
        return 1;
    }

    auto script = cereka::scenario::CompileVNScript(entry);
    if (script.empty()) {
        std::cerr << "[CerekaGame] Failed to compile script: " << entry << "\n";
        engine.ShutDown();
        return 1;
    }

    engine.LoadCompiledScript(script);

    while (!engine.IsGameFinished()) {
        cereka::CerekaEvent e;
        while (engine.PollEvent(e))
            engine.HandleEvent(e);

        engine.Update(1.0f / 60.0f);
        engine.TickScript();
        engine.Draw();
        engine.Present();
    }

    engine.ShutDown();
    return 0;
}
