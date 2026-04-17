#pragma once
// config_manager.hpp — Data-driven Configuration Manager
//
// Enterprise-grade config system with Property Map Pattern.
//
// Key concepts:
//   PropertyDef   - Static definition of a property (name, type, description)
//   PropertyValue - Runtime value of a property
//   ApplyContext  - Engine state needed to apply properties
//
// Usage:
//   ConfigManager cfg;
//   cfg.setContext(ctx);
//   cfg.initDefaults();
//   cfg.apply("textbox.color", "0 0 0 160");
//
// Adding new properties:
//   1. Add PropertyDef entry to PROPERTY_TABLE in config_manager.cpp
//   2. That's it! No code changes needed.
// ============================================================================

#include "property_types.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace config {

// ============================================================================
// Property Value — runtime value with accessor
// ============================================================================

struct PropertyValue {
    PropType type;
    std::string serialized;

    // Getters for specific types
    float asFloat() const;
    int asInt() const;
    std::string asString() const;
    bool asBool() const;
    SDL_Color asColor() const;
    Dim asDim() const;
    std::vector<SDL_Keycode> asKeyList() const;
};

// ============================================================================
// Config Manager — central registry for all properties
// ============================================================================

class ConfigManager {
   public:
    ConfigManager() = default;

    // Set engine context for side effects
    void setContext(const ApplyContext &ctx);

    // Initialize all known properties from property table
    void initDefaults();

    // Apply a property value by key
    void apply(const std::string &key,
               const std::string &value);

    // Get property definition
    const PropertyDef *getDef(const std::string &key) const;

    // Get current property value (serialized string)
    std::string getValue(const std::string &key) const;

    // List all registered property keys
    std::vector<std::string> listProperties() const;

    // Serialize all properties to stream
    void serialize(std::ostream &os) const;

    // Get the property table (for iteration)
    static const std::vector<PropertyDef> &getPropertyTable();

   private:
    ApplyContext ctx_;
};

// ============================================================================
// Property Table Access — for iterating all properties
// ============================================================================

const std::vector<PropertyDef> &getPropertyTable();

}  // namespace config
