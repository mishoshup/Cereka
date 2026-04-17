#pragma once
// save_data.hpp — Glaze-based game save serialization
//
// Uses Glaze library for JSON serialization/deserialization of game saves.

#ifndef CEREKA_SAVE_DATA_HPP
#    define CEREKA_SAVE_DATA_HPP

#    include <glaze/glaze.hpp>
#    include <string>
#    include <unordered_map>
#    include <vector>

namespace cereka {

// ============================================================================
// SerializableCharacter — Save data for visible character
// ============================================================================

struct SerializableCharacter {
    std::string id;
    std::string file;
    std::string position;
};

// ============================================================================
// SerializableSaveData — Complete game state for serialization
// ============================================================================

struct SerializableSaveData {
    std::string timestamp;
    size_t programCounter = 0;
    std::vector<size_t> callStack;
    std::unordered_map<std::string, std::string> variables;
    std::string background;
    std::vector<SerializableCharacter> characters;
    std::string bgm;
    std::string state;
    std::string speaker;
    std::string name;
    std::string text;
    int displayedChars = 0;
    bool skipMode = false;
    int skipDepth = 0;
};

// ============================================================================
// Serialization Functions
// ============================================================================

[[nodiscard]] inline bool saveDataToJson(const SerializableSaveData &data,
                                         std::string &jsonOut)
{
    auto result = glz::write_json(data);
    if (result) {
        jsonOut = *result;
        return true;
    }
    return false;
}

[[nodiscard]] inline bool jsonToSaveData(SerializableSaveData &data,
                                         const std::string &json)
{
    if (json.empty()) {
        data = SerializableSaveData{};
        return true;
    }
    auto result = glz::read_json<SerializableSaveData>(json);
    if (result) {
        data = *result;
        return true;
    }
    return false;
}

}  // namespace cereka

// ============================================================================
// Glaze meta specializations (must be in global or glz namespace)
// ============================================================================

template<> struct glz::meta<cereka::SerializableCharacter> {
    using T = cereka::SerializableCharacter;
    static constexpr auto value = object(&T::id, &T::file, &T::position);
};

template<> struct glz::meta<cereka::SerializableSaveData> {
    using T = cereka::SerializableSaveData;
    static constexpr auto value = object("timestamp",
                                         "programCounter",
                                         "callStack",
                                         "variables",
                                         "background",
                                         "characters",
                                         "bgm",
                                         "state",
                                         "speaker",
                                         "name",
                                         "text",
                                         "displayedChars",
                                         "skipMode",
                                         "skipDepth");
};

#endif  // CEREKA_SAVE_DATA_HPP
