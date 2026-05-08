#include "Cereka/CerekaTest.hpp"
#include "Cereka/Cereka.hpp"
#include <SDL3/SDL.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace cereka {

CerekaTest::CerekaTest(CerekaEngine &engine)
    : engine_(engine)
{}

// ---------------------------------------------------------------------------
// Parse a .spec.crka file into commands
// ---------------------------------------------------------------------------

std::vector<CerekaTest::Command>
CerekaTest::parseSpec(const std::string &path)
{
    std::vector<Command> cmds;
    std::ifstream f(path);
    if (!f) {
        std::cerr << "error: cannot open spec file: " << path << "\n";
        return cmds;
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(f, line)) {
        lineNum++;

        auto semi = line.find(';');
        if (semi != std::string::npos)
            line = line.substr(0, semi);

        size_t a = line.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            continue;
        line = line.substr(a);

        if (line.empty())
            continue;

        if (line.rfind("wait", 0) == 0) {
            Command cmd;
            cmd.type = Command::Wait;
            cmd.timeoutSec = 5;

            std::string rest = line.substr(4);
            a = rest.find_first_not_of(" \t");
            if (a != std::string::npos)
                rest = rest.substr(a);

            if (!rest.empty() && rest[0] != '"') {
                std::istringstream is(rest);
                int sec = 0;
                if (is >> sec)
                    cmd.timeoutSec = sec;
                auto qpos = rest.find('"');
                if (qpos != std::string::npos)
                    rest = rest.substr(qpos);
            }

            auto q1 = rest.find('"');
            auto q2 = rest.rfind('"');
            if (q1 != std::string::npos && q2 > q1)
                cmd.text = rest.substr(q1 + 1, q2 - q1 - 1);
            cmds.push_back(cmd);
        }
        else if (line.rfind("click", 0) == 0) {
            Command cmd;
            cmd.type = Command::ClickButton;
            cmd.index = 0;

            std::string rest = line.substr(5);
            a = rest.find_first_not_of(" \t");
            if (a != std::string::npos)
                rest = rest.substr(a);

            if (rest.rfind("button:", 0) == 0) {
                rest = rest.substr(7);
                if (!rest.empty() && rest[0] == '"') {
                    auto q2 = rest.rfind('"');
                    if (q2 > 0)
                        cmd.text = rest.substr(1, q2 - 1);
                } else {
                    cmd.index = std::stoi(rest);
                }
            }
            cmds.push_back(cmd);
        }
        else if (line.rfind("assert", 0) == 0) {
            Command cmd;
            cmd.type = Command::Assert;

            std::string rest = line.substr(6);
            a = rest.find_first_not_of(" \t");
            if (a != std::string::npos)
                rest = rest.substr(a);

            auto q1 = rest.find('"');
            auto q2 = rest.rfind('"');
            if (q1 != std::string::npos && q2 > q1)
                cmd.text = rest.substr(q1 + 1, q2 - q1 - 1);
            cmds.push_back(cmd);
        }
        else {
            std::cerr << "error:" << path << ":" << lineNum
                      << ": unknown command: " << line << "\n";
        }
    }

    return cmds;
}

// ---------------------------------------------------------------------------
// Execute commands
// ---------------------------------------------------------------------------

int CerekaTest::execute(const std::vector<Command> &commands)
{
    int failed = 0;
    size_t cmdIdx = 0;
    std::string lastText;
    auto startTime = std::chrono::steady_clock::now();

    while (!engine_.IsGameFinished() && cmdIdx < commands.size()) {
        engine_.Update(1.0f / 60.0f);

        if (engine_.CurrentState() == CerekaState::WaitingForInput) {
            CerekaEvent advance;
            advance.type = CerekaEvent::KeyDown;
            advance.key = SDLK_SPACE;
            engine_.HandleEvent(advance);
        }

        const auto &cmd = commands[cmdIdx];

        switch (cmd.type) {
            case Command::Wait: {
                std::string cur = engine_.CurrentText();
                if (!cur.empty() && cur != lastText
                    && cur.find(cmd.text) != std::string::npos) {
                    lastText = cur;
                    cmdIdx++;
                    startTime = std::chrono::steady_clock::now();
                    continue;
                }

                if (engine_.IsGameFinished()) {
                    std::cout << "error: script ended before text \""
                              << cmd.text << "\" appeared\n";
                    failed++;
                    cmdIdx++;
                    startTime = std::chrono::steady_clock::now();
                    break;
                }
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()
                    >= cmd.timeoutSec) {
                    std::cout << "error: timeout waiting for \""
                              << cmd.text << "\" (" << cmd.timeoutSec << "s)\n";
                    failed++;
                    cmdIdx++;
                    startTime = std::chrono::steady_clock::now();
                }
                break;
            }

            case Command::ClickButton: {
                if (!engine_.InMenu()) {
                    if (!engine_.IsGameFinished())
                        break;
                    std::cout << "error: script ended before clicking \""
                              << cmd.text << "\"\n";
                    failed++;
                    cmdIdx++;
                    break;
                }

                int idx = -1;
                if (!cmd.text.empty()) {
                    auto labels = engine_.ButtonLabels();
                    for (size_t i = 0; i < labels.size(); i++) {
                        if (labels[i] == cmd.text) {
                            idx = (int)i;
                            break;
                        }
                    }
                    if (idx < 0) {
                        std::cout << "error: no button with label \""
                                  << cmd.text << "\"\n";
                        std::cout << "  available buttons:\n";
                        for (auto &l : engine_.ButtonLabels())
                            std::cout << "    " << l << "\n";
                        failed++;
                        cmdIdx++;
                        break;
                    }
                } else {
                    idx = cmd.index - 1;
                    if (idx < 0 || idx >= (int)engine_.ButtonCount()) {
                        std::cout << "error: button index " << cmd.index
                                  << " out of range (1-"
                                  << engine_.ButtonCount() << ")\n";
                        failed++;
                        cmdIdx++;
                        break;
                    }
                }

                if (!engine_.SelectMenuOption(idx)) {
                    std::cout << "error: failed to select menu option "
                              << (idx + 1) << "\n";
                    failed++;
                }
                lastText.clear();
                cmdIdx++;
                startTime = std::chrono::steady_clock::now();
                break;
            }

            case Command::Assert: {
                std::string allText = engine_.CurrentText();
                if (!allText.empty()
                    && allText.find(cmd.text) != std::string::npos) {
                    std::cout << "\n";
                    std::cout << "error: assert failed: found \""
                              << cmd.text << "\"\n";
                    failed++;
                }
                cmdIdx++;
                break;
            }
        }
    }

    std::cout << "\n";
    if (failed == 0)
        std::cout << "ok -- all commands passed\n";
    else
        std::cout << "error: " << failed
                  << (failed == 1 ? " command failed" : " commands failed")
                  << "\n";

    return failed;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int CerekaTest::Run(const std::string &specFile)
{
    auto cmds = parseSpec(specFile);
    if (cmds.empty()) {
        std::cerr << "error: no commands in spec file\n";
        return 1;
    }
    return execute(cmds);
}

std::vector<std::string> CerekaTest::ButtonLabels() const
{
    return engine_.ButtonLabels();
}

bool CerekaTest::SelectMenuOption(int idx)
{
    return engine_.SelectMenuOption(idx);
}

}  // namespace cereka
