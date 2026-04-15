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
static std::unordered_map<std::string,
                          std::string>
parseConfig(const std::string &path)
{
    std::unordered_map<std::string, std::string> cfg;
    std::ifstream f(path);
    if (!f)
        return cfg;

    std::string line;
    while (std::getline(f, line)) {
        // strip comments and whitespace
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);
        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
        };
        line = trim(line);
        if (line.empty())
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

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
int main(int argc,
         char **argv)
{
    std::ofstream log("cereka_debug.txt", std::ios::app);

    auto L = [&](const std::string &s) {
        std::cout << s << std::endl;
        log << s << std::endl;
    };

    L("=== GAME START ===");

    for (int i = 0; i < argc; i++)
        L(std::string("argv[") + std::to_string(i) + "] = " + argv[i]);

    // ----------------------------------------------------
    // project root
    // ----------------------------------------------------
    std::string projectRoot;

    if (argc >= 2)
        projectRoot = fs::absolute(argv[1]).lexically_normal().string();
    else
        projectRoot = fs::current_path().lexically_normal().string();

    L("projectRoot = " + projectRoot);

    std::error_code ec;
    fs::current_path(projectRoot, ec);

    if (ec) {
        L("[FATAL] Cannot set working directory");
        L(ec.message());
        return 1;
    }

    L("cwd = " + fs::current_path().string());

    // ----------------------------------------------------
    // config
    // ----------------------------------------------------
    L("STEP: loading config");

    auto cfg = parseConfig("game.cfg");

    L(std::string("game.cfg exists = ") + (fs::exists("game.cfg") ? "true" : "false"));
    L(std::string("cfg size = ") + std::to_string(cfg.size()));

    if (cfg.empty()) {
        L("[FATAL] game.cfg missing or empty");
        return 1;
    }

    std::string title = cfg.count("title") ? cfg["title"] : "Cereka Game";
    int width = cfg.count("width") ? std::stoi(cfg["width"]) : 1280;
    int height = cfg.count("height") ? std::stoi(cfg["height"]) : 720;
    bool fullscreen = cfg.count("fullscreen") ? (cfg["fullscreen"] == "true") : false;
    std::string entry = cfg.count("entry") ? cfg["entry"] : "assets/scripts/main.crka";

    L("title = " + title);
    L("entry = " + entry);

    // ----------------------------------------------------
    // engine init
    // ----------------------------------------------------
    L("STEP: InitGame");

    cereka::CerekaEngine engine;

    if (!engine.InitGame(title.c_str(), width, height, fullscreen)) {
        L("[FATAL] InitGame FAILED");
        return 1;
    }

    L("InitGame OK");

    // ----------------------------------------------------
    // compile script
    // ----------------------------------------------------
    L("STEP: compile script");

    L("checking entry exists = " + std::string(fs::exists(entry) ? "true" : "false"));

    auto script = cereka::scenario::CompileVNScript(entry);

    if (script.empty()) {
        L("[FATAL] CompileVNScript returned empty");
        return 1;
    }

    L("compile OK");

    // ----------------------------------------------------
    // run
    // ----------------------------------------------------
    L("STEP: running script");

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

    L("GAME FINISHED");

    engine.ShutDown();
    return 0;
}
