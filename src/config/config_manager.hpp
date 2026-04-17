#pragma once
// config_manager.hpp — Enterprise config system with Property Map Pattern
//
// Provides a scalable, self-documenting configuration system.
// Properties are registered once with descriptions and apply functions.

#include "ui_config.hpp"
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace config {

// ---------------------------------------------------------------------------
// PropertyDescriptor — describes a single configurable property
// ---------------------------------------------------------------------------

struct PropertyDescriptor {
    std::string key;
    std::string description;
    std::function<void(const std::string &value)> apply;
};

// ---------------------------------------------------------------------------
// Context — engine context for side effects (texture loading, font reloading)
// ---------------------------------------------------------------------------

struct ApplyContext {
    SDL_Renderer *renderer = nullptr;
    std::string fontPath;
    UiConfig *uiCfg = nullptr;

    std::function<void(int size)> reloadFont;
    std::function<void(SDL_Texture *&tex, const std::string &path)> loadTexture;
};

// ---------------------------------------------------------------------------
// ConfigManager — central configuration registry
// ---------------------------------------------------------------------------

class ConfigManager {
    std::unordered_map<std::string, PropertyDescriptor> properties;
    ApplyContext ctx;

   public:
    ConfigManager() = default;

    void setContext(const ApplyContext &context)
    {
        ctx = context;
    }

    void registerProperty(const PropertyDescriptor &prop)
    {
        properties[prop.key] = prop;
    }

    void apply(const std::string &key,
               const std::string &value)
    {
        auto it = properties.find(key);
        if (it != properties.end()) {
            it->second.apply(value);
        }
        else {
            std::cerr << "[CONFIG] Unknown property: " << key << "\n";
        }
    }

    bool hasProperty(const std::string &key) const
    {
        return properties.find(key) != properties.end();
    }

    const PropertyDescriptor *getProperty(const std::string &key) const
    {
        auto it = properties.find(key);
        return it != properties.end() ? &it->second : nullptr;
    }

    std::vector<std::string> listProperties() const
    {
        std::vector<std::string> keys;
        for (const auto &p : properties) {
            keys.push_back(p.first);
        }
        return keys;
    }

    // Serialization for preferences
    void serialize(std::ostream &os) const
    {
        for (const auto &p : properties) {
            os << "# " << p.second.description << "\n";
            os << p.first << "=" << getPropertyValue(p.first) << "\n";
        }
    }

    std::string getPropertyValue(const std::string &key) const;

    // Initialize all known properties
    void initDefaults();
};

// ---------------------------------------------------------------------------
// Helper: Parse color string "r g b a" → SDL_Color
// ---------------------------------------------------------------------------

inline SDL_Color parseColor(const std::string &s)
{
    int r = 255, g = 255, b = 255, a = 255;
    std::sscanf(s.c_str(), "%d %d %d %d", &r, &g, &b, &a);
    return {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
}

// ---------------------------------------------------------------------------
// Helper: Parse key name → SDL_Keycode
// ---------------------------------------------------------------------------

inline SDL_Keycode parseKeyName(const std::string &name)
{
    if (name == "space")
        return SDLK_SPACE;
    if (name == "enter" || name == "return")
        return SDLK_RETURN;
    if (name == "escape")
        return SDLK_ESCAPE;
    if (name == "tab")
        return SDLK_TAB;
    if (name == "up")
        return SDLK_UP;
    if (name == "down")
        return SDLK_DOWN;
    if (name == "left")
        return SDLK_LEFT;
    if (name == "right")
        return SDLK_RIGHT;
    return SDLK_UNKNOWN;
}

// ---------------------------------------------------------------------------
// Helper: Serialize SDL_Color → "r g b a"
// ---------------------------------------------------------------------------

inline std::string serializeColor(const SDL_Color &c)
{
    return std::to_string((int)c.r) + " " + std::to_string((int)c.g) + " " +
           std::to_string((int)c.b) + " " + std::to_string((int)c.a);
}

// ---------------------------------------------------------------------------
// Helper: Serialize key list → "key1 key2 key3"
// ---------------------------------------------------------------------------

inline std::string serializeKeyList(const std::vector<SDL_Keycode> &keys)
{
    std::ostringstream oss;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0)
            oss << " ";
        switch (keys[i]) {
            case SDLK_SPACE:
                oss << "space";
                break;
            case SDLK_RETURN:
                oss << "enter";
                break;
            case SDLK_ESCAPE:
                oss << "escape";
                break;
            default:
                oss << "unknown";
                break;
        }
    }
    return oss.str();
}

// ---------------------------------------------------------------------------
// KeyCodeMap — for getPropertyValue serialization
// ---------------------------------------------------------------------------

inline std::string keycodeToString(SDL_Keycode k)
{
    switch (k) {
        case SDLK_SPACE:
            return "space";
        case SDLK_RETURN:
            return "enter";
        case SDLK_ESCAPE:
            return "escape";
        default:
            return "unknown";
    }
}

}  // namespace config
