#pragma once
// settings_manager.hpp — Persistent user settings via glaze JSON
//
// Settings are separate from ConfigManager (which handles UI theme from .crka
// scripts). Settings are user preferences persisted to settings.json in the
// project root. Loaded at game init, saved on change.

#include <glaze/glaze.hpp>
#include <string>

namespace cereka {

// ============================================================================
// SerializableSettings — Struct that maps to settings.json
// ============================================================================

struct SerializableSettings {
    float textSpeed = 60.0f;    // characters per second
    float bgmVolume = 1.0f;     // 0.0–1.0
    float sfxVolume = 1.0f;     // 0.0–1.0
    bool autoAdvance = false;
    bool fullscreen = false;
    bool skipUnseen = false;

    struct glaze {
        using T = SerializableSettings;
        static constexpr auto value = glz::object(
            "textSpeed", &T::textSpeed,
            "bgmVolume", &T::bgmVolume,
            "sfxVolume", &T::sfxVolume,
            "autoAdvance", &T::autoAdvance,
            "fullscreen", &T::fullscreen,
            "skipUnseen", &T::skipUnseen);
    };
};

// ============================================================================
// SettingsManager — Loads, saves, and applies user settings
// ============================================================================

class DialogueSystem;
class AudioManager;

class SettingsManager {
   public:
    /// Load settings from settings.json. Returns false on failure (file
    /// missing, corrupt) — defaults are used in that case.
    bool Load();

    /// Save current settings to settings.json. Returns false on failure.
    bool Save() const;

    /// Apply settings to engine subsystems (text speed → DialogueSystem,
    /// volume → AudioManager). Called on init and after settings change.
    void Apply(DialogueSystem &dialogue, AudioManager &audio) const;

    /// Mutable access to the settings struct (call Save + Apply after
    /// modifying).
    SerializableSettings &Get() { return settings_; }
    const SerializableSettings &Get() const { return settings_; }

   private:
    SerializableSettings settings_;

    static std::string settingsPath();
};

}  // namespace cereka
