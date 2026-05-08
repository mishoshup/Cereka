// settings_manager.cpp — Persistent user settings via glaze JSON
//
// Settings file lives at {cwd}/settings.json alongside saves/.

#include "cereka_settings_manager.hpp"
#include "cereka_audio_manager.hpp"
#include "cereka_dialogue_system.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace cereka {

// ============================================================================
// Path helper
// ============================================================================

std::string SettingsManager::settingsPath()
{
    return (fs::absolute("settings.json")).string();
}

// ============================================================================
// Load
// ============================================================================

bool SettingsManager::Load()
{
    std::string buffer;
    auto result = glz::read_file_json(settings_, settingsPath(), buffer);
    if (result) {
        // File missing or corrupt — use defaults
        settings_ = SerializableSettings{};
        return false;
    }
    return true;
}

// ============================================================================
// Save
// ============================================================================

bool SettingsManager::Save() const
{
    std::string buffer;
    auto result = glz::write_file_json(settings_, settingsPath(), buffer);
    if (result) {
        std::cerr << "[CEREKA] Failed to write settings.json\n";
        return false;
    }
    return true;
}

// ============================================================================
// Apply — wire settings to engine subsystems
// ============================================================================

void SettingsManager::Apply(DialogueSystem &dialogue, AudioManager &audio) const
{
    dialogue.SetTextSpeed(settings_.textSpeed);
    audio.SetBgmVolume(settings_.bgmVolume);
    audio.SetSfxVolume(settings_.sfxVolume);
    // autoAdvance, fullscreen, skipUnseen are stored but wiring is deferred
    // (autoAdvance needs a timer in the update loop; fullscreen needs an
    // SDL window mode toggle; skipUnseen needs an already-seen tracker)
}

}  // namespace cereka
