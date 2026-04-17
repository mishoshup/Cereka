// config_manager.cpp — Data-driven ConfigManager implementation
//
// Properties are defined as data in PROPERTY_TABLE.
// Each property entry contains: key, type, and description.
// Type handlers and serializers are shared across all properties.
//
// To add a new property:
//   1. Add entry to PROPERTY_TABLE
//   2. Add case in serialize() if needed
// ============================================================================

#include "config_manager.hpp"
#include "property_types.hpp"
#include <sstream>

namespace config {

// ============================================================================
// Property Table — all configurable properties defined as data
// ============================================================================
//
// Format: { key, type, description }
//
// Type determines:
//   - How the value is parsed from string
//   - How the value is serialized to string
//   - How the value is applied to UiConfig
//
// For simple types (Float, Int, String, Bool):
//   Handler is defined in the APPLY_TO_UI_CFG macro below.
//
// For complex types (Color, Dim, Texture):
//   Handler explicitly defined below.
// ============================================================================

static const PropertyDef PROPERTY_TABLE[] = {
    // ------------------------------------------------------------------------
    // Textbox properties
    // ------------------------------------------------------------------------
    {"textbox.image", PropType::Texture, "Textbox background image path"},
    {"textbox.color", PropType::Color, "Textbox background color (r g b a)"},
    {"textbox.y", PropType::Dim, "Textbox Y position (pixels or percentage%)"},
    {"textbox.h", PropType::Dim, "Textbox height (pixels or percentage%)"},
    {"textbox.text_margin_x", PropType::Float, "Text horizontal margin inside textbox"},
    {"textbox.text_color", PropType::Color, "Text color (r g b a)"},

    // ------------------------------------------------------------------------
    // Namebox properties
    // ------------------------------------------------------------------------
    {"namebox.image", PropType::Texture, "Namebox background image path"},
    {"namebox.color", PropType::Color, "Namebox background color (r g b a)"},
    {"namebox.x", PropType::Float, "Namebox X position (pixels from left)"},
    {"namebox.y_offset", PropType::Float, "Namebox Y offset from textbox top"},
    {"namebox.w", PropType::Float, "Namebox width"},
    {"namebox.h", PropType::Float, "Namebox height"},
    {"namebox.text_color", PropType::Color, "Name text color (r g b a)"},

    // ------------------------------------------------------------------------
    // Button properties
    // ------------------------------------------------------------------------
    {"button.image", PropType::Texture, "Button background image path"},
    {"button.hover_image", PropType::Texture, "Button hover image path"},
    {"button.color", PropType::Color, "Button background color (r g b a)"},
    {"button.w", PropType::Float, "Button width"},
    {"button.h", PropType::Float, "Button height"},
    {"button.text_color", PropType::Color, "Button text color (r g b a)"},

    // ------------------------------------------------------------------------
    // Font properties
    // ------------------------------------------------------------------------
    {"font.size", PropType::Int, "Font size in pixels"},

    // ------------------------------------------------------------------------
    // Interaction properties
    // ------------------------------------------------------------------------
    {"advance_keys", PropType::KeyList, "Keys that advance dialogue (space enter click)"},
};

static constexpr size_t PROPERTY_TABLE_SIZE = sizeof(PROPERTY_TABLE) / sizeof(PROPERTY_TABLE[0]);

// ============================================================================
// PropertyDef Lookup Map — for O(1) access by key
// ============================================================================

struct PropertyLookup {
    std::unordered_map<std::string, const PropertyDef *> byKey;

    PropertyLookup()
    {
        for (size_t i = 0; i < PROPERTY_TABLE_SIZE; ++i) {
            byKey[PROPERTY_TABLE[i].key] = &PROPERTY_TABLE[i];
        }
    }
};

static PropertyLookup LOOKUP;

// ============================================================================
// PropertyValue Implementation
// ============================================================================

float PropertyValue::asFloat() const
{
    return std::stof(serialized);
}

int PropertyValue::asInt() const
{
    return std::stoi(serialized);
}

std::string PropertyValue::asString() const
{
    return serialized;
}

bool PropertyValue::asBool() const
{
    return serialized == "true" || serialized == "1" || serialized == "yes";
}

SDL_Color PropertyValue::asColor() const
{
    int r = 255, g = 255, b = 255, a = 255;
    std::sscanf(serialized.c_str(), "%d %d %d %d", &r, &g, &b, &a);
    return {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
}

Dim PropertyValue::asDim() const
{
    return Dim::parse(serialized);
}

std::vector<SDL_Keycode> PropertyValue::asKeyList() const
{
    return parsers::parseKeyList(serialized).keyListVal;
}

// ============================================================================
// ConfigManager Implementation
// ============================================================================

void ConfigManager::setContext(const ApplyContext &ctx)
{
    ctx_ = ctx;
}

void ConfigManager::initDefaults()
{
    // Property table is statically initialized - nothing to do here
    // All properties are registered by default
}

const PropertyDef *ConfigManager::getDef(const std::string &key) const
{
    auto it = LOOKUP.byKey.find(key);
    return (it != LOOKUP.byKey.end()) ? it->second : nullptr;
}

std::string ConfigManager::getValue(const std::string &key) const
{
    if (!ctx_.uiCfg)
        return "";

    // Helper to serialize Dim
    auto serializeDim = [](const Dim &d) -> std::string {
        if (d.relative) {
            return std::to_string(d.value * 100) + "%";
        }
        return std::to_string(d.value);
    };

    // Branch only for serialization (reads current values)
    if (key == "textbox.image")
        return ctx_.uiCfg->textbox.imagePath;
    if (key == "textbox.color")
        return serializers::serializeColor(ctx_.uiCfg->textbox.color);
    if (key == "textbox.y")
        return serializeDim(ctx_.uiCfg->textbox.y);
    if (key == "textbox.h")
        return serializeDim(ctx_.uiCfg->textbox.h);
    if (key == "textbox.text_margin_x")
        return serializers::serializeFloat(ctx_.uiCfg->textbox.textMarginX);
    if (key == "textbox.text_color")
        return serializers::serializeColor(ctx_.uiCfg->textbox.textColor);

    if (key == "namebox.image")
        return ctx_.uiCfg->namebox.imagePath;
    if (key == "namebox.color")
        return serializers::serializeColor(ctx_.uiCfg->namebox.color);
    if (key == "namebox.x")
        return serializers::serializeFloat(ctx_.uiCfg->namebox.x);
    if (key == "namebox.y_offset")
        return serializers::serializeFloat(ctx_.uiCfg->namebox.yOffset);
    if (key == "namebox.w")
        return serializers::serializeFloat(ctx_.uiCfg->namebox.w);
    if (key == "namebox.h")
        return serializers::serializeFloat(ctx_.uiCfg->namebox.h);
    if (key == "namebox.text_color")
        return serializers::serializeColor(ctx_.uiCfg->namebox.textColor);

    if (key == "button.image")
        return ctx_.uiCfg->button.imagePath;
    if (key == "button.hover_image")
        return ctx_.uiCfg->button.hoverImagePath;
    if (key == "button.color")
        return serializers::serializeColor(ctx_.uiCfg->button.color);
    if (key == "button.w")
        return serializers::serializeFloat(ctx_.uiCfg->button.w);
    if (key == "button.h")
        return serializers::serializeFloat(ctx_.uiCfg->button.h);
    if (key == "button.text_color")
        return serializers::serializeColor(ctx_.uiCfg->button.textColor);

    if (key == "font.size")
        return serializers::serializeInt(ctx_.uiCfg->fontSize);

    if (key == "advance_keys")
        return serializers::serializeKeyList(ctx_.uiCfg->advanceKeys);

    return "";
}

void ConfigManager::apply(const std::string &key,
                          const std::string &value)
{
    const PropertyDef *def = getDef(key);
    if (!def) {
        std::cerr << "[CONFIG] Unknown property: " << key << "\n";
        return;
    }

    // Parse value based on type
    ApplyValue parsed = parseByType(def->type, value);

    // Apply based on specific property
    // Note: Each property has specific target in UiConfig

    // ---- Textbox ----
    if (key == "textbox.image") {
        handlers::applyTexture(
            ctx_, parsed, ctx_.uiCfg->textbox.image, ctx_.uiCfg->textbox.imagePath);
    }
    else if (key == "textbox.color") {
        handlers::applyColor(ctx_, parsed, &ctx_.uiCfg->textbox.color);
    }
    else if (key == "textbox.y") {
        handlers::applyDim(ctx_, parsed, &ctx_.uiCfg->textbox.y);
    }
    else if (key == "textbox.h") {
        handlers::applyDim(ctx_, parsed, &ctx_.uiCfg->textbox.h);
    }
    else if (key == "textbox.text_margin_x") {
        ctx_.uiCfg->textbox.textMarginX = parsed.floatVal;
    }
    else if (key == "textbox.text_color") {
        handlers::applyColor(ctx_, parsed, &ctx_.uiCfg->textbox.textColor);
    }

    // ---- Namebox ----
    else if (key == "namebox.image") {
        handlers::applyTexture(
            ctx_, parsed, ctx_.uiCfg->namebox.image, ctx_.uiCfg->namebox.imagePath);
    }
    else if (key == "namebox.color") {
        handlers::applyColor(ctx_, parsed, &ctx_.uiCfg->namebox.color);
    }
    else if (key == "namebox.x") {
        ctx_.uiCfg->namebox.x = parsed.floatVal;
    }
    else if (key == "namebox.y_offset") {
        ctx_.uiCfg->namebox.yOffset = parsed.floatVal;
    }
    else if (key == "namebox.w") {
        ctx_.uiCfg->namebox.w = parsed.floatVal;
    }
    else if (key == "namebox.h") {
        ctx_.uiCfg->namebox.h = parsed.floatVal;
    }
    else if (key == "namebox.text_color") {
        handlers::applyColor(ctx_, parsed, &ctx_.uiCfg->namebox.textColor);
    }

    // ---- Button ----
    else if (key == "button.image") {
        handlers::applyTexture(
            ctx_, parsed, ctx_.uiCfg->button.image, ctx_.uiCfg->button.imagePath);
    }
    else if (key == "button.hover_image") {
        handlers::applyTexture(
            ctx_, parsed, ctx_.uiCfg->button.hoverImage, ctx_.uiCfg->button.hoverImagePath);
    }
    else if (key == "button.color") {
        handlers::applyColor(ctx_, parsed, &ctx_.uiCfg->button.color);
    }
    else if (key == "button.w") {
        ctx_.uiCfg->button.w = parsed.floatVal;
    }
    else if (key == "button.h") {
        ctx_.uiCfg->button.h = parsed.floatVal;
    }
    else if (key == "button.text_color") {
        handlers::applyColor(ctx_, parsed, &ctx_.uiCfg->button.textColor);
    }

    // ---- Font ----
    else if (key == "font.size") {
        int newSize = parsed.intVal;
        if (newSize != ctx_.uiCfg->fontSize) {
            ctx_.uiCfg->fontSize = newSize;
            if (ctx_.reloadFont) {
                ctx_.reloadFont(newSize);
            }
        }
    }

    // ---- Interaction ----
    else if (key == "advance_keys") {
        handlers::applyKeyList(ctx_, parsed);
    }
}

std::vector<std::string> ConfigManager::listProperties() const
{
    std::vector<std::string> keys;
    for (size_t i = 0; i < PROPERTY_TABLE_SIZE; ++i) {
        keys.push_back(PROPERTY_TABLE[i].key);
    }
    return keys;
}

void ConfigManager::serialize(std::ostream &os) const
{
    for (const auto &key : listProperties()) {
        const PropertyDef *def = getDef(key);
        if (def) {
            os << "# " << def->description << "\n";
            os << key << "=" << getValue(key) << "\n";
        }
    }
}

const std::vector<PropertyDef> &ConfigManager::getPropertyTable()
{
    static std::vector<PropertyDef> table(PROPERTY_TABLE, PROPERTY_TABLE + PROPERTY_TABLE_SIZE);
    return table;
}

const std::vector<PropertyDef> &getPropertyTable()
{
    return ConfigManager::getPropertyTable();
}

}  // namespace config
