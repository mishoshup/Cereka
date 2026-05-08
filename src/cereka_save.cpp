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
        case CerekaState::HistoryState:    return "HistoryState";
        case CerekaState::PauseMenuState:        return "PauseMenu";
        case CerekaState::ConfirmOverwriteState: return "ConfirmOverwrite";
        case CerekaState::SettingsMenuState:     return "SettingsMenu";
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
    if (label == "PauseMenu") return CerekaState::PauseMenuState;
    if (label == "ConfirmOverwrite") return CerekaState::ConfirmOverwriteState;
    if (label == "SettingsMenu") return CerekaState::SettingsMenuState;
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

    // State machine — save the effective gameplay state (under any overlays)
    data.state = stateToString(m_stateMachine.effectiveState());

    // Dialogue state
    data.speaker = dialogue.Speaker();
    data.name = dialogue.Name();
    data.text = dialogue.Text();
    data.displayedChars = dialogue.DisplayedChars();

    // Scene description for save slot UI
    {
        std::string desc;
        if (!data.background.empty()) {
            desc = data.background;
            // Strip directory prefix
            auto slash = desc.find('/');
            if (slash != std::string::npos) desc = desc.substr(slash + 1);
        }
        if (!data.characters.empty()) {
            if (!desc.empty()) desc += " + ";
            desc += std::to_string(data.characters.size()) + " char(s)";
            // Append first character ID
            desc += " (" + data.characters[0].id + ")";
        }
        if (desc.empty()) desc = "(no scene)";
        data.sceneDescription = desc;
    }

    // Skip mode
    data.skipMode = scriptInterpreter.skipMode;
    data.skipDepth = scriptInterpreter.skipDepth;

    // Write JSON to file
    std::string buffer;
    auto result = glz::write_file_json(data, savePath(slot), buffer);
    return !result;
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
        return false;
    }

    if (data.version < 1 || data.version > 1)
        {}

    // Tear down current state
    scene.Clear();
    audio.StopBGM();
    scriptInterpreter.variables.clear();
    scriptInterpreter.numVariables.clear();
    scriptInterpreter.callStack.clear();
    dialogue.Clear();
    scriptInterpreter.skipMode = false;
    scriptInterpreter.skipDepth = 0;

    // Restore script state
    scriptInterpreter.pc = std::min(data.programCounter,
                                     scriptInterpreter.program.empty()
                                         ? 0
                                         : scriptInterpreter.program.size() - 1);
    scriptInterpreter.callStack = data.callStack;
    scriptInterpreter.variables = data.variables;
    scriptInterpreter.numVariables = data.numVariables;

    // Restore scene
    if (!data.background.empty())
        scene.ShowBackground(data.background);
    for (auto &ch : data.characters)
        scene.ShowCharacter(ch.id, ch.file, ch.position);

    // Restore audio
    if (!data.bgm.empty())
        audio.PlayBGM(data.bgm);

    // Restore state machine
    m_stateMachine.clearOverlays();
    m_stateMachine.changeState(parseState(data.state));

    // Rollback buffer invalid after load
    rollbackManager.clear();

    // Restore dialogue
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
// GetSlotMetadata — reads summary metadata from a save slot
// ---------------------------------------------------------------------------

cereka::SlotMetadata Impl::GetSlotMetadata(int slot)
{
    SlotMetadata meta;
    SerializableSaveData data;
    std::string buffer;
    auto result = glz::read_file_json(data, savePath(slot), buffer);
    if (result)
        return meta;
    meta.timestamp = data.timestamp;
    meta.sceneDescription = data.sceneDescription;
    return meta;
}

// ---------------------------------------------------------------------------
// DrawSaveLoadOverlay — delegates to UIManager
// ---------------------------------------------------------------------------

void Impl::DrawSaveLoadOverlay(bool isSaving)
{
    SlotMetadata slots[10];
    for (int i = 1; i <= 10; i++)
        slots[i - 1] = GetSlotMetadata(i);
    ui.DrawSaveLoadOverlay(isSaving, slots, uiCfg);
}

// ---------------------------------------------------------------------------
// HitTestSaveSlot — returns slot 1-10 or -1
// ---------------------------------------------------------------------------

int Impl::HitTestSaveSlot(int mx, int my)
{
    return ui.HitTestSaveSlot(mx, my, screenWidth, screenHeight);
}
