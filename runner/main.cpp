#include "Cereka/Cereka.hpp"
#include "Cereka/CerekaTest.hpp"
#include "Cereka/exceptions.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#    include <windows.h>
#else
#    include <unistd.h>
#endif

namespace fs = std::filesystem;

static fs::path exeDir()
{
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    return fs::path(buf).parent_path();
#else
    char buf[2048] = {};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0)
        return fs::path(std::string(buf, len)).parent_path();
    return fs::current_path();
#endif
}

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
// Headless output helpers
// ---------------------------------------------------------------------------

static int failedCount = 0;

static void headlessOutput(const std::string &text,
                            const std::string &file,
                            int line,
                            int col)
{
    if (text.find("FAIL") != std::string::npos) {
        failedCount++;
        std::cout << "  --> " << file << ":" << line << ":" << col << "\n"
                  << "   |\n"
                  << "   |     " << text << "\n"
                  << "   |     ";
        for (int i = 0; i < (int)text.find("FAIL"); i++)
            std::cout << " ";
        std::cout << "^^^^^^^ spike test failed\n"
                  << "   |\n"
                  << "   = note: narrate contained FAIL\n"
                  << "\n";
    } else {
        std::cout << file << ":" << line << ":" << col
                  << "  " << text << "\n";
    }
}

// ---------------------------------------------------------------------------
// Entry point
//
// Usage:
//   CerekaGame                      — uses current working directory
//   CerekaGame /path/to/game        — explicit project directory
//   CerekaGame --headless           — run without window
//   CerekaGame --headless --entry test.crka  — override entry point
//   CerekaGame --script test.spec.crka       — run spec test (implies headless)
// ---------------------------------------------------------------------------
int main(int argc,
         char **argv)
{
    // Parse flags before project root
    bool headless = false;
    std::string headlessEntry;
    std::string projectArg;
    std::string specFile;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--headless") {
            headless = true;
        } else if (a == "--entry") {
            if (i + 1 < argc)
                headlessEntry = argv[++i];
        } else if (a == "--script") {
            if (i + 1 < argc)
                specFile = argv[++i];
            headless = true;
        } else if (a[0] == '-') {
            // unknown flag, ignore
        } else {
            projectArg = a;
        }
    }

    std::ofstream log("cereka_debug.txt", std::ios::app);

    auto L = [&](const std::string &s) {
        std::cout << s << std::endl;
        log << s << std::endl;
    };

    if (!headless) {
        L("=== GAME START ===");
        for (int i = 0; i < argc; i++)
            L(std::string("argv[") + std::to_string(i) + "] = " + argv[i]);
    }

    // ----------------------------------------------------
    // project root
    // ----------------------------------------------------
    std::string projectRoot;

    if (!projectArg.empty())
        projectRoot = fs::absolute(projectArg).lexically_normal().string();
    else
        projectRoot = exeDir().lexically_normal().string();

    if (!headless)
        L("projectRoot = " + projectRoot);

    std::error_code ec;
    fs::current_path(projectRoot, ec);

    if (ec) {
        if (!headless)
            L("[FATAL] Cannot set working directory");
        std::cerr << "error: cannot set working directory: " << ec.message() << "\n";
        return 1;
    }

    if (!headless)
        L("cwd = " + fs::current_path().string());

    // ----------------------------------------------------
    // config
    // ----------------------------------------------------
    if (!headless)
        L("STEP: loading config");

    auto cfg = parseConfig("game.cfg");

    if (!headless) {
        L(std::string("game.cfg exists = ") + (fs::exists("game.cfg") ? "true" : "false"));
        L(std::string("cfg size = ") + std::to_string(cfg.size()));
    }

    if (cfg.empty()) {
        if (!headless)
            L("[FATAL] game.cfg missing or empty");
        std::cerr << "error: game.cfg missing or empty\n";
        return 1;
    }

    std::string title = cfg.count("title") ? cfg["title"] : "Cereka Game";
    int width = cfg.count("width") ? std::stoi(cfg["width"]) : 1280;
    int height = cfg.count("height") ? std::stoi(cfg["height"]) : 720;
    bool fullscreen = cfg.count("fullscreen") ? (cfg["fullscreen"] == "true") : false;
    std::string entry = cfg.count("entry") ? cfg["entry"] : "assets/scripts/main.crka";

    if (!headless) {
        L("title = " + title);
        L("entry = " + entry);
    }

    // ----------------------------------------------------
    // engine init
    // ----------------------------------------------------
    if (!headless)
        L("STEP: InitGame");

    cereka::CerekaEngine engine;

    if (!engine.InitGame(title.c_str(), width, height, fullscreen, headless)) {
        if (!headless)
            L("[FATAL] InitGame FAILED");
        std::cerr << "error: InitGame failed\n";
        return 1;
    }

    if (!headless)
        L("InitGame OK");

    // ----------------------------------------------------
    // compile script
    // ----------------------------------------------------
    if (!headless) {
        L("STEP: compile script");
        L("checking entry exists = " + std::string(fs::exists(entry) ? "true" : "false"));
    }

    if (!headlessEntry.empty())
        entry = headlessEntry;

    auto scriptResult = cereka::compiler::CompileCerekaScript(entry);
    if (!scriptResult) {
        if (!headless)
            L("[FATAL] " + scriptResult.error());
        std::cerr << "error: " << scriptResult.error() << "\n";
        return 1;
    }
    auto &script = *scriptResult;

    if (!headless)
        L("compile OK");

    // ----------------------------------------------------
    // run
    // ----------------------------------------------------
    if (!headless)
        L("STEP: running script");

    engine.LoadCompiledCerekaScript(script);

    // Spec test mode: run through CerekaTest
    if (!specFile.empty()) {
        cereka::CerekaTest test(engine);
        int code = test.Run(specFile);
        engine.ShutDown();
        return code;
    }

    if (headless) {
        // Headless mode: auto-advance, output to stdout, no rendering
        std::string lastText;
        while (!engine.IsGameFinished()) {
            engine.Update(1.0f / 60.0f);

            // After TickScript, capture new text (skip duplicates)
            if (!engine.CurrentText().empty()
                && engine.CurrentText() != lastText) {
                lastText = engine.CurrentText();
                size_t pc = engine.ProgramCounter();
                if (pc > 0 && pc <= script.size()) {
                    const auto &ins = script[pc - 1];
                    headlessOutput(lastText, entry,
                                   ins.srcLine, ins.srcCol);
                } else {
                    headlessOutput(lastText, entry, 0, 0);
                }
            }

            // Auto-advance: synthesize an advance event
            if (!engine.IsGameFinished()) {
                cereka::CerekaEvent advance;
                advance.type = cereka::CerekaEvent::KeyDown;
                advance.key = SDLK_SPACE;
                engine.HandleEvent(advance);
            }
        }
        if (failedCount > 0) {
            std::cout << "\n";
            std::cout << "error: " << failedCount
                      << (failedCount == 1 ? " spike test failed" : " spike tests failed")
                      << "\n";
        }
        engine.ShutDown();
        return failedCount > 0 ? 1 : 0;
    }

    // Normal mode: render loop
    while (!engine.IsGameFinished()) {
        cereka::CerekaEvent e;
        while (engine.PollEvent(e))
            engine.HandleEvent(e);

        engine.Update(1.0f / 60.0f);  // includes dispatch loop via DialogueState::update
        engine.Draw();
        engine.Present();
    }

    L(engine.IsGameQuit() ? "GAME QUIT" : "GAME FINISHED");

    engine.ShutDown();
    return 0;
}
