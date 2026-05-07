// save.cpp — save/load game state and save/load UI overlay
//
// Uses Glaze JSON format for portable, versioned save files.
// Save schema defined in save_data.hpp — add fields there, not here.

#include "cereka_engine_impl.hpp"
#include "cereka_save_data.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

using cereka::CerekaState;

static std::string savePath(int slot)
{
    static const fs::path saveDir = fs::absolute("saves");
    return (saveDir / ("slot" + std::to_string(slot) + ".json")).string();
}

// Human-readable state labels mirroring CerekaStateMachine::stateLabel().
static std::string stateToString(CerekaState s)
{
    switch (s) {
        case CerekaState::Running:         return "Running";
        case CerekaState::WaitingForInput: return "WaitingForInput";
        case CerekaState::InMenu:          return "InMenu";
        case CerekaState::Fading:          return "Fading";
        case CerekaState::Finished:        return "Finished";
        case CerekaState::Quit:            return "Quit";
        case CerekaState::SaveMenuState:   return "SaveMenu";
        case CerekaState::LoadMenuState:   return "LoadMenu";
    }
    return "Running";
}

static CerekaState parseState(const std::string &label)
{
    if (label == "Running") return CerekaState::Running;
    if (label == "WaitingForInput") return CerekaState::WaitingForInput;
    if (label == "InMenu") return CerekaState::InMenu;
    if (label == "Fading") return CerekaState::Fading;
    if (label == "Finished") return CerekaState::Finished;
    if (label == "Quit") return CerekaState::Quit;
    if (label == "SaveMenu") return CerekaState::SaveMenuState;
    if (label == "LoadMenu") return CerekaState::LoadMenuState;
    return CerekaState::Running;
}

// Reverse xNorm to position string for serialization.
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
    fs::create_directories(fs::absolute("saves"), ec);

    SerializableSaveData data;
    data.version = 1;

    // Timestamp
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
    data.timestamp = tsBuf;

    // Script state
    data.programCounter = scriptInterpreter.pc;
    data.callStack = scriptInterpreter.callStack;
    data.variables = scriptInterpreter.variables;
    data.numVariables = scriptInterpreter.numVariables;

    // Scene state
    data.background = scene.BgPath();
    for (auto &[id, filename] : scene.CharPaths()) {
        SerializableCharacter ch;
        ch.id = id;
        ch.file = filename;
        const auto &chars = scene.Characters();
        auto it = chars.find(id);
        float xn = (it != chars.end()) ? it->second.xNorm : 0.5f;
        ch.position = xNormToPos(xn);
        data.characters.push_back(std::move(ch));
    }

    // Audio
    data.bgm = audio.BgmPath();

    // State machine — save the state before we entered the save menu overlay
    data.state = stateToString(stateBeforeSaveMenu);

    // Dialogue state
    data.speaker = dialogue.Speaker();
    data.name = dialogue.Name();
    data.text = dialogue.Text();
    data.displayedChars = dialogue.DisplayedChars();

    // Skip mode
    data.skipMode = scriptInterpreter.skipMode;
    data.skipDepth = scriptInterpreter.skipDepth;

    // Write JSON to file
    std::string buffer;
    auto result = glz::write_file_json(data, savePath(slot), buffer);
    return !result;  // error_ctx::operator bool returns true on error
}

// ---------------------------------------------------------------------------
// LoadGame
// ---------------------------------------------------------------------------

bool Impl::LoadGame(int slot)
{
    // Read JSON from file
    SerializableSaveData data;
    std::string buffer;
    auto result = glz::read_file_json(data, savePath(slot), buffer);
    if (result) {
        // Glaze error — malformed JSON, missing file, or schema mismatch
        return false;
    }

    // Validate version (graceful: v1 is the only version in alpha)
    if (data.version < 1 || data.version > 1) {
        // Unknown version — attempt best-effort load anyway
    }

    // Tear down current visual/audio state
    scene.Clear();
    audio.StopBGM();
    scriptInterpreter.variables.clear();
    scriptInterpreter.numVariables.clear();
    scriptInterpreter.callStack.clear();
    dialogue.Clear();
    scriptInterpreter.skipMode = false;
    scriptInterpreter.skipDepth = 0;

    // Restore script state — clamp PC to valid program range
    scriptInterpreter.pc = std::min(data.programCounter,
                                     scriptInterpreter.program.empty()
                                         ? 0
                                         : scriptInterpreter.program.size() - 1);
    scriptInterpreter.callStack = data.callStack;
    scriptInterpreter.variables = data.variables;
    scriptInterpreter.numVariables = data.numVariables;

    // Restore scene state
    if (!data.background.empty())
        scene.ShowBackground(data.background);
    for (auto &ch : data.characters)
        scene.ShowCharacter(ch.id, ch.file, ch.position);

    // Restore audio
    if (!data.bgm.empty())
        audio.PlayBGM(data.bgm);

    // Restore state machine
    state = parseState(data.state);

    // Restore dialogue state
    dialogue.SetSpeaker(data.speaker);
    dialogue.SetName(data.name);
    dialogue.SetText(data.text);
    dialogue.SetDisplayedChars(data.displayedChars);

    // Restore skip mode
    scriptInterpreter.skipMode = data.skipMode;
    scriptInterpreter.skipDepth = data.skipDepth;

    return true;
}

// ---------------------------------------------------------------------------
// GetSlotTimestamp — reads just the timestamp from a save file
// ---------------------------------------------------------------------------

std::string Impl::GetSlotTimestamp(int slot)
{
    SerializableSaveData data;
    std::string buffer;
    auto result = glz::read_file_json(data, savePath(slot), buffer);
    if (result)
        return "";
    return data.timestamp;
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
    SDL_Texture *titleTex = RenderText(isSaving ? "SAVE GAME" : "LOAD GAME", cereka::Color{180, 200, 255, 255});
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
            label, ts.empty() ? cereka::Color{100, 100, 100, 255} : cereka::Color{220, 220, 220, 255});
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
