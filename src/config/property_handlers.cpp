// property_handlers.cpp — Type parsers, serializers, and handlers
//
// Each type has a parser (string → typed value),
// a serializer (typed value → string),
// and a handler (apply typed value to target).

#include "property_types.hpp"
#include <cstdio>
#include <sstream>

namespace config {

// ============================================================================
// Key Name Mapping — string ↔ SDL_Keycode
// ============================================================================

struct KeyMapping {
    const char *name;
    SDL_Keycode code;
};

static const KeyMapping KEY_MAP[] = {
    {"space", SDLK_SPACE},
    {"enter", SDLK_RETURN},
    {"return", SDLK_RETURN},
    {"escape", SDLK_ESCAPE},
    {"tab", SDLK_TAB},
    {"up", SDLK_UP},
    {"down", SDLK_DOWN},
    {"left", SDLK_LEFT},
    {"right", SDLK_RIGHT},
};

static constexpr size_t KEY_MAP_COUNT = sizeof(KEY_MAP) / sizeof(KEY_MAP[0]);

// ============================================================================
// Type Parsers
// ============================================================================

namespace parsers {

ApplyValue parseFloat(const std::string &str)
{
    ApplyValue v;
    v.type = PropType::Float;
    v.raw = str;
    v.floatVal = std::stof(str);
    return v;
}

ApplyValue parseInt(const std::string &str)
{
    ApplyValue v;
    v.type = PropType::Int;
    v.raw = str;
    v.intVal = std::stoi(str);
    return v;
}

ApplyValue parseString(const std::string &str)
{
    ApplyValue v;
    v.type = PropType::String;
    v.raw = str;
    v.stringVal = str;
    return v;
}

ApplyValue parseBool(const std::string &str)
{
    ApplyValue v;
    v.type = PropType::Bool;
    v.raw = str;
    v.boolVal = (str == "true" || str == "1" || str == "yes");
    return v;
}

ApplyValue parseColor(const std::string &str)
{
    ApplyValue v;
    v.type = PropType::Color;
    v.raw = str;

    int r = 255, g = 255, b = 255, a = 255;
    std::sscanf(str.c_str(), "%d %d %d %d", &r, &g, &b, &a);
    v.colorVal = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
    return v;
}

ApplyValue parseDim(const std::string &str)
{
    ApplyValue v;
    v.type = PropType::Dim;
    v.raw = str;
    v.dimVal = Dim::parse(str);
    return v;
}

ApplyValue parseKeyList(const std::string &str)
{
    ApplyValue v;
    v.type = PropType::KeyList;
    v.raw = str;

    std::istringstream iss(str);
    std::string token;

    while (iss >> token) {
        for (size_t i = 0; i < KEY_MAP_COUNT; ++i) {
            if (token == KEY_MAP[i].name) {
                v.keyListVal.push_back(KEY_MAP[i].code);
                break;
            }
        }
    }
    return v;
}

ApplyValue parseTexture(const std::string &str)
{
    ApplyValue v;
    v.type = PropType::Texture;
    v.raw = str;
    v.stringVal = str;
    return v;
}

}  // namespace parsers

// ============================================================================
// Keycode to String (for serialization)
// ============================================================================

static std::string keycodeToString(SDL_Keycode k)
{
    for (size_t i = 0; i < KEY_MAP_COUNT; ++i) {
        if (k == KEY_MAP[i].code) {
            return KEY_MAP[i].name;
        }
    }
    return "unknown";
}

// ============================================================================
// Type Serializers
// ============================================================================

namespace serializers {

std::string serializeFloat(float val)
{
    return std::to_string(val);
}

std::string serializeInt(int val)
{
    return std::to_string(val);
}

std::string serializeString(const std::string &val)
{
    return val;
}

std::string serializeBool(bool val)
{
    return val ? "true" : "false";
}

std::string serializeColor(const SDL_Color &val)
{
    std::ostringstream oss;
    oss << (int)val.r << " " << (int)val.g << " " << (int)val.b << " " << (int)val.a;
    return oss.str();
}

std::string serializeDim(const Dim &val)
{
    if (val.relative) {
        return std::to_string(val.value * 100) + "%";
    }
    return std::to_string(val.value);
}

std::string serializeKeyList(const std::vector<SDL_Keycode> &keys)
{
    std::ostringstream oss;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0)
            oss << " ";
        oss << keycodeToString(keys[i]);
    }
    return oss.str();
}

std::string serializeTexture(const std::string &path)
{
    return path;
}

}  // namespace serializers

// ============================================================================
// Type Handlers — apply typed value to target
// ============================================================================

namespace handlers {

void applyFloat(ApplyContext &ctx,
                ApplyValue &val)
{
    (void)ctx;
    (void)val;
    // Float properties need target pointer (handled at property level)
}

void applyInt(ApplyContext &ctx,
              ApplyValue &val)
{
    (void)ctx;
    (void)val;
    // Int properties need target pointer (handled at property level)
}

void applyString(ApplyContext &ctx,
                 ApplyValue &val)
{
    (void)ctx;
    (void)val;
    // String properties need target pointer (handled at property level)
}

void applyBool(ApplyContext &ctx,
               ApplyValue &val)
{
    (void)ctx;
    (void)val;
    // Bool properties need target pointer (handled at property level)
}

void applyColor(ApplyContext &ctx,
                ApplyValue &val,
                SDL_Color *target)
{
    if (target) {
        *target = val.colorVal;
    }
}

void applyDim(ApplyContext &ctx,
              ApplyValue &val,
              Dim *target)
{
    (void)ctx;
    if (target) {
        *target = val.dimVal;
    }
}

void applyKeyList(ApplyContext &ctx,
                  ApplyValue &val)
{
    if (ctx.uiCfg) {
        ctx.uiCfg->advanceKeys = val.keyListVal;
    }
}

void applyTexture(ApplyContext &ctx,
                  ApplyValue &val,
                  SDL_Texture *&targetTex,
                  std::string &targetPath)
{
    if (!ctx.loadTexture)
        return;

    // Update path
    targetPath = val.stringVal;

    // Load new texture
    ctx.loadTexture(targetTex, val.stringVal);
}

}  // namespace handlers

// ============================================================================
// Property Parse Dispatch — select parser based on type
// ============================================================================

ApplyValue parseByType(PropType type,
                       const std::string &str)
{
    switch (type) {
        case PropType::Float:
            return parsers::parseFloat(str);
        case PropType::Int:
            return parsers::parseInt(str);
        case PropType::String:
            return parsers::parseString(str);
        case PropType::Bool:
            return parsers::parseBool(str);
        case PropType::Color:
            return parsers::parseColor(str);
        case PropType::Dim:
            return parsers::parseDim(str);
        case PropType::KeyList:
            return parsers::parseKeyList(str);
        case PropType::Texture:
            return parsers::parseTexture(str);
    }
    return parsers::parseString(str);
}

}  // namespace config
