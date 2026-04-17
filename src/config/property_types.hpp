#pragma once
// property_types.hpp — Type definitions for the property system
//
// Data-driven configuration: properties are defined as data, not code.
// Type handlers provide the logic for each property type.

#include "ui_config.hpp"
#include <functional>
#include <string>
#include <vector>

namespace config {

// ============================================================================
// Property Type Enum
// ============================================================================

enum class PropType {
    // Basic types
    Float,   // Single float value
    Int,     // Single integer value
    String,  // String value
    Bool,    // Boolean value

    // Complex types
    Color,    // SDL_Color (r g b a)
    Dim,      // Dim (pixels or percentage)
    KeyList,  // std::vector<SDL_Keycode>

    // Texture (special: requires renderer)
    Texture,  // SDL_Texture* (loaded from path)
};

// ============================================================================
// Property Definition — describes a single configurable property
// ============================================================================

struct PropertyDef {
    const char *key;          // Property name (e.g., "textbox.color")
    PropType type;            // Type of the property
    const char *description;  // Human-readable description
};

// ============================================================================
// Apply Context — provides access to engine state for property application
// ============================================================================

struct ApplyContext {
    SDL_Renderer *renderer = nullptr;

    // Font path for font reloading
    std::string fontPath;

    // UI Config (the data we're configuring)
    UiConfig *uiCfg = nullptr;

    // Callbacks for side effects
    std::function<void(int size)> reloadFont;
    std::function<void(SDL_Texture *&tex, const std::string &path)> loadTexture;
    std::function<void(SDL_Texture *&tex)> destroyTexture;
};

// ============================================================================
// Apply Value — holds a parsed value before applying to target
// ============================================================================

struct ApplyValue {
    PropType type;
    std::string raw;  // Raw string value

    // Parsed values (only one is valid based on type)
    float floatVal = 0.0f;
    int intVal = 0;
    std::string stringVal;
    bool boolVal = false;
    SDL_Color colorVal = {255, 255, 255, 255};
    Dim dimVal;
    std::vector<SDL_Keycode> keyListVal;
};

// ============================================================================
// Dispatcher — parse based on type enum
// ============================================================================

ApplyValue parseByType(PropType type,
                       const std::string &str);

// ============================================================================
// Type Parsers — convert string to typed value
// ============================================================================

namespace parsers {

ApplyValue parseFloat(const std::string &str);
ApplyValue parseInt(const std::string &str);
ApplyValue parseString(const std::string &str);
ApplyValue parseBool(const std::string &str);
ApplyValue parseColor(const std::string &str);
ApplyValue parseDim(const std::string &str);
ApplyValue parseKeyList(const std::string &str);
ApplyValue parseTexture(const std::string &str);

}  // namespace parsers

// ============================================================================
// Type Serializers — convert typed value to string
// ============================================================================

namespace serializers {

std::string serializeFloat(float val);
std::string serializeInt(int val);
std::string serializeString(const std::string &val);
std::string serializeBool(bool val);
std::string serializeColor(const SDL_Color &val);
std::string serializeDim(const Dim &val);
std::string serializeKeyList(const std::vector<SDL_Keycode> &keys);
std::string serializeTexture(const std::string &path);

}  // namespace serializers

// ============================================================================
// Type Handlers — apply typed value to UiConfig
// ============================================================================

namespace handlers {

void applyFloat(ApplyContext &ctx,
                ApplyValue &val);
void applyInt(ApplyContext &ctx,
              ApplyValue &val);
void applyString(ApplyContext &ctx,
                 ApplyValue &val);
void applyBool(ApplyContext &ctx,
               ApplyValue &val);
void applyColor(ApplyContext &ctx,
                ApplyValue &val,
                SDL_Color *target);
void applyDim(ApplyContext &ctx,
              ApplyValue &val,
              Dim *target);
void applyKeyList(ApplyContext &ctx,
                  ApplyValue &val);
void applyTexture(ApplyContext &ctx,
                  ApplyValue &val,
                  SDL_Texture *&targetTex,
                  std::string &targetPath);

}  // namespace handlers

}  // namespace config
