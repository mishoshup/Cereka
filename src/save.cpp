// save.cpp — save/load game state and save/load UI overlay

#include "engine_impl.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static std::string savePath(int slot)
{
    return "saves/slot" + std::to_string(slot) + ".sav";
}

// Reverse xNorm to position string for serialization
static std::string xNormToPos(float xNorm)
{
    if (xNorm < 0.35f)
        return "left";
    if (xNorm > 0.65f)
        return "right";
    return "center";
}

// ---------------------------------------------------------------------------
// SaveGame
// ---------------------------------------------------------------------------

bool Impl::SaveGame(int slot)
{
    std::error_code ec;
    fs::create_directories("saves", ec);

    std::ofstream f(savePath(slot));
    if (!f)
        return false;

    // Timestamp line (read back by GetSlotTimestamp for the UI)
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char tsBuf[32] = {};
    struct tm tmInfo = {};
#ifdef _WIN32
    localtime_s(&tmInfo, &t);
#else
    localtime_r(&t, &tmInfo);
#endif
    strftime(tsBuf, sizeof(tsBuf), "%Y-%m-%d %H:%M", &tmInfo);
    f << "timestamp=" << tsBuf << "\n";

    f << "pc=" << pc << "\n";

    f << "callstack=";
    for (size_t i = 0; i < callStack.size(); ++i) {
        if (i)
            f << ",";
        f << callStack[i];
    }
    f << "\n";

    for (auto &[k, v] : variables)
        f << "var." << k << "=" << v << "\n";

    f << "bg=" << bgPath << "\n";

    for (auto &[id, filename] : charPaths) {
        auto it = characters.find(id);
        float xn = (it != characters.end()) ? it->second.xNorm : 0.5f;
        f << "char." << id << "=" << filename << ":" << xNormToPos(xn) << "\n";
    }

    f << "bgm=" << audio.BgmPath() << "\n";
    f << "state=" << (int)stateBeforeSaveMenu << "\n";
    f << "speaker=" << dialogue.Speaker() << "\n";
    f << "name=" << dialogue.Name() << "\n";
    f << "text=" << dialogue.Text() << "\n";
    f << "displayedChars=" << dialogue.DisplayedChars() << "\n";
    f << "skipMode=" << (skipMode ? 1 : 0) << "\n";
    f << "skipDepth=" << skipDepth << "\n";

    return true;
}

// ---------------------------------------------------------------------------
// LoadGame
// ---------------------------------------------------------------------------

bool Impl::LoadGame(int slot)
{
    std::ifstream f(savePath(slot));
    if (!f)
        return false;

    // Tear down current visual/audio state
    if (background) {
        SDL_DestroyTexture(background);
        background = nullptr;
    }
    bgPath.clear();
    for (auto &[id, entry] : characters)
        SDL_DestroyTexture(entry.tex);
    characters.clear();
    charPaths.clear();
    audio.StopBGM();
    variables.clear();
    numVariables.clear();
    callStack.clear();
    dialogue.Clear();
    skipMode = false;
    skipDepth = 0;

    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "pc") {
            pc = (size_t)std::stoull(val);
        }
        else if (key == "callstack") {
            if (!val.empty()) {
                std::istringstream ss(val);
                std::string tok;
                while (std::getline(ss, tok, ','))
                    if (!tok.empty())
                        callStack.push_back((size_t)std::stoull(tok));
            }
        }
        else if (key.size() > 4 && key.substr(0, 4) == "var.") {
            std::string varkey = key.substr(4);
            variables[varkey] = val;
            try {
                numVariables[varkey] = std::stof(val);
            }
            catch (...) {
            }
        }
        else if (key == "bg") {
            if (!val.empty())
                ShowBackground(val);
        }
        else if (key.size() > 5 && key.substr(0, 5) == "char.") {
            std::string id = key.substr(5);
            // val = "filename.png:left|center|right"
            auto colon = val.rfind(':');
            if (colon != std::string::npos) {
                std::string fname = val.substr(0, colon);
                std::string pos = val.substr(colon + 1);
                ShowCharacter(id, fname, pos);
            }
        }
        else if (key == "bgm") {
            if (!val.empty())
                audio.PlayBGM(val);
        }
        else if (key == "state") {
            state = (CerekaState)std::stoi(val);
        }
        else if (key == "speaker") {
            dialogue.SetSpeaker(val);
        }
        else if (key == "name") {
            dialogue.SetName(val);
        }
        else if (key == "text") {
            dialogue.SetText(val);
        }
        else if (key == "displayedChars") {
            dialogue.SetDisplayedChars(std::stoi(val));
        }
        else if (key == "skipMode") {
            skipMode = (val == "1");
        }
        else if (key == "skipDepth") {
            skipDepth = std::stoi(val);
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// GetSlotTimestamp — reads just the first line of a save file
// ---------------------------------------------------------------------------

std::string Impl::GetSlotTimestamp(int slot)
{
    std::ifstream f(savePath(slot));
    if (!f)
        return "";
    std::string line;
    if (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq != std::string::npos && line.substr(0, eq) == "timestamp")
            return line.substr(eq + 1);
    }
    return "???";
}

// ---------------------------------------------------------------------------
// DrawSaveLoadOverlay — SDL3-rendered overlay (no ImGui)
// ---------------------------------------------------------------------------

void Impl::DrawSaveLoadOverlay(bool isSaving)
{
    const float panelW = screenWidth * 0.5f;
    const float panelH = screenHeight * 0.8f;
    const float panelX = (screenWidth - panelW) * 0.5f;
    const float panelY = (screenHeight - panelH) * 0.5f;
    const float slotH = (panelH - 60.0f) / 10.0f;

    // Dim background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, nullptr);

    // Panel background
    SDL_SetRenderDrawColor(renderer, 20, 22, 38, 230);
    SDL_FRect panel{panelX, panelY, panelW, panelH};
    SDL_RenderFillRect(renderer, &panel);

    // Title
    SDL_Texture *titleTex = RenderText(isSaving ? "SAVE GAME" : "LOAD GAME", {180, 200, 255, 255});
    if (titleTex) {
        float tw, th;
        SDL_GetTextureSize(titleTex, &tw, &th);
        SDL_FRect dst{panelX + (panelW - tw) * 0.5f, panelY + 8.0f, tw, th};
        SDL_RenderTexture(renderer, titleTex, nullptr, &dst);
        SDL_DestroyTexture(titleTex);
    }

    // Slot rows
    for (int i = 1; i <= 10; ++i) {
        float slotY = panelY + 50.0f + (i - 1) * slotH;
        SDL_FRect slotRect{panelX + 10.0f, slotY + 2.0f, panelW - 20.0f, slotH - 4.0f};

        SDL_SetRenderDrawColor(renderer, 40, 44, 66, 210);
        SDL_RenderFillRect(renderer, &slotRect);

        std::string ts = GetSlotTimestamp(i);
        std::string label = "Slot " + std::to_string(i) + "   " + (ts.empty() ? "Empty" : ts);

        SDL_Texture *slotTex = RenderText(
            label, ts.empty() ? SDL_Color{100, 100, 100, 255} : SDL_Color{220, 220, 220, 255});
        if (slotTex) {
            float tw, th;
            SDL_GetTextureSize(slotTex, &tw, &th);
            SDL_FRect dst{slotRect.x + 10.0f, slotY + (slotH - th) * 0.5f, tw, th};
            SDL_RenderTexture(renderer, slotTex, nullptr, &dst);
            SDL_DestroyTexture(slotTex);
        }
    }

    // ESC hint
    SDL_Texture *hintTex = RenderText("ESC to cancel", {120, 120, 120, 255});
    if (hintTex) {
        float tw, th;
        SDL_GetTextureSize(hintTex, &tw, &th);
        SDL_FRect dst{panelX + (panelW - tw) * 0.5f, panelY + panelH - th - 8.0f, tw, th};
        SDL_RenderTexture(renderer, hintTex, nullptr, &dst);
        SDL_DestroyTexture(hintTex);
    }
}

// ---------------------------------------------------------------------------
// HitTestSaveSlot — returns slot 1-10 or -1
// ---------------------------------------------------------------------------

int Impl::HitTestSaveSlot(int mx,
                          int my)
{
    const float panelW = screenWidth * 0.5f;
    const float panelH = screenHeight * 0.8f;
    const float panelX = (screenWidth - panelW) * 0.5f;
    const float panelY = (screenHeight - panelH) * 0.5f;
    const float slotH = (panelH - 60.0f) / 10.0f;

    for (int i = 1; i <= 10; ++i) {
        float slotY = panelY + 50.0f + (i - 1) * slotH;
        if ((float)mx >= panelX + 10.0f && (float)mx <= panelX + panelW - 10.0f &&
            (float)my >= slotY + 2.0f && (float)my <= slotY + slotH - 2.0f)
            return i;
    }
    return -1;
}
