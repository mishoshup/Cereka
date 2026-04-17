// config_manager.cpp — ConfigManager implementation with property registrations

#include "config_manager.hpp"
#include "engine_impl.hpp"
#include <sstream>

namespace config {

// ---------------------------------------------------------------------------
// ConfigManager implementation
// ---------------------------------------------------------------------------

std::string ConfigManager::getPropertyValue(const std::string &key) const
{
    if (!ctx.uiCfg)
        return "";

    if (key == "textbox.image")
        return ctx.uiCfg->textbox.imagePath;
    if (key == "textbox.color")
        return serializeColor(ctx.uiCfg->textbox.color);
    if (key == "textbox.y") {
        return ctx.uiCfg->textbox.y.relative ?
                   std::to_string(ctx.uiCfg->textbox.y.value * 100) + "%" :
                   std::to_string(ctx.uiCfg->textbox.y.value);
    }
    if (key == "textbox.h") {
        return ctx.uiCfg->textbox.h.relative ?
                   std::to_string(ctx.uiCfg->textbox.h.value * 100) + "%" :
                   std::to_string(ctx.uiCfg->textbox.h.value);
    }
    if (key == "textbox.text_margin_x")
        return std::to_string(ctx.uiCfg->textbox.textMarginX);
    if (key == "textbox.text_color")
        return serializeColor(ctx.uiCfg->textbox.textColor);

    if (key == "namebox.image")
        return ctx.uiCfg->namebox.imagePath;
    if (key == "namebox.color")
        return serializeColor(ctx.uiCfg->namebox.color);
    if (key == "namebox.x")
        return std::to_string(ctx.uiCfg->namebox.x);
    if (key == "namebox.y_offset")
        return std::to_string(ctx.uiCfg->namebox.yOffset);
    if (key == "namebox.w")
        return std::to_string(ctx.uiCfg->namebox.w);
    if (key == "namebox.h")
        return std::to_string(ctx.uiCfg->namebox.h);
    if (key == "namebox.text_color")
        return serializeColor(ctx.uiCfg->namebox.textColor);

    if (key == "button.image")
        return ctx.uiCfg->button.imagePath;
    if (key == "button.hover_image")
        return ctx.uiCfg->button.hoverImagePath;
    if (key == "button.color")
        return serializeColor(ctx.uiCfg->button.color);
    if (key == "button.w")
        return std::to_string(ctx.uiCfg->button.w);
    if (key == "button.h")
        return std::to_string(ctx.uiCfg->button.h);
    if (key == "button.text_color")
        return serializeColor(ctx.uiCfg->button.textColor);

    if (key == "font.size")
        return std::to_string(ctx.uiCfg->fontSize);

    if (key == "advance_keys")
        return serializeKeyList(ctx.uiCfg->advanceKeys);

    return "";
}

// ---------------------------------------------------------------------------
// Helper: Load/reload texture with context
// ---------------------------------------------------------------------------

static void loadTextureWithContext(ApplyContext &c,
                                   SDL_Texture *&tex,
                                   const std::string &path)
{
    if (tex) {
        SDL_DestroyTexture(tex);
        tex = nullptr;
    }
    if (!path.empty() && c.renderer) {
        tex = IMG_LoadTexture(c.renderer, path.c_str());
        if (!tex) {
            std::cerr << "[CONFIG] Failed to load texture '" << path << "': " << SDL_GetError()
                      << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// initDefaults — register all known properties
// ---------------------------------------------------------------------------

void ConfigManager::initDefaults()
{
    // Load texture helper lambda
    auto loadTex = [&](SDL_Texture *&tex, const std::string &path) {
        loadTextureWithContext(ctx, tex, path);
    };

    // --- textbox ---
    registerProperty(
        {"textbox.image", "Textbox background image", [this, &loadTex](const std::string &v) {
             ctx.uiCfg->textbox.imagePath = v;
             loadTex(ctx.uiCfg->textbox.image, v);
         }});

    registerProperty({"textbox.color",
                      "Textbox background color (r g b a)",
                      [this](const std::string &v) { ctx.uiCfg->textbox.color = parseColor(v); }});

    registerProperty({"textbox.y",
                      "Textbox Y position (pixels or percentage%)",
                      [this](const std::string &v) { ctx.uiCfg->textbox.y = Dim::parse(v); }});

    registerProperty({"textbox.h",
                      "Textbox height (pixels or percentage%)",
                      [this](const std::string &v) { ctx.uiCfg->textbox.h = Dim::parse(v); }});

    registerProperty(
        {"textbox.text_margin_x",
         "Text horizontal margin inside textbox",
         [this](const std::string &v) { ctx.uiCfg->textbox.textMarginX = std::stof(v); }});

    registerProperty({"textbox.text_color", "Text color (r g b a)", [this](const std::string &v) {
                          ctx.uiCfg->textbox.textColor = parseColor(v);
                      }});

    // --- namebox ---
    registerProperty(
        {"namebox.image", "Namebox background image", [this, &loadTex](const std::string &v) {
             ctx.uiCfg->namebox.imagePath = v;
             loadTex(ctx.uiCfg->namebox.image, v);
         }});

    registerProperty({"namebox.color",
                      "Namebox background color (r g b a)",
                      [this](const std::string &v) { ctx.uiCfg->namebox.color = parseColor(v); }});

    registerProperty({"namebox.x",
                      "Namebox X position (pixels from left)",
                      [this](const std::string &v) { ctx.uiCfg->namebox.x = std::stof(v); }});

    registerProperty(
        {"namebox.y_offset", "Namebox Y offset from textbox top", [this](const std::string &v) {
             ctx.uiCfg->namebox.yOffset = std::stof(v);
         }});

    registerProperty({"namebox.w", "Namebox width", [this](const std::string &v) {
                          ctx.uiCfg->namebox.w = std::stof(v);
                      }});

    registerProperty({"namebox.h", "Namebox height", [this](const std::string &v) {
                          ctx.uiCfg->namebox.h = std::stof(v);
                      }});

    registerProperty(
        {"namebox.text_color", "Name text color (r g b a)", [this](const std::string &v) {
             ctx.uiCfg->namebox.textColor = parseColor(v);
         }});

    // --- button ---
    registerProperty(
        {"button.image", "Button background image", [this, &loadTex](const std::string &v) {
             ctx.uiCfg->button.imagePath = v;
             loadTex(ctx.uiCfg->button.image, v);
         }});

    registerProperty(
        {"button.hover_image", "Button hover image", [this, &loadTex](const std::string &v) {
             ctx.uiCfg->button.hoverImagePath = v;
             loadTex(ctx.uiCfg->button.hoverImage, v);
         }});

    registerProperty({"button.color",
                      "Button background color (r g b a)",
                      [this](const std::string &v) { ctx.uiCfg->button.color = parseColor(v); }});

    registerProperty({"button.w", "Button width", [this](const std::string &v) {
                          ctx.uiCfg->button.w = std::stof(v);
                      }});

    registerProperty({"button.h", "Button height", [this](const std::string &v) {
                          ctx.uiCfg->button.h = std::stof(v);
                      }});

    registerProperty(
        {"button.text_color", "Button text color (r g b a)", [this](const std::string &v) {
             ctx.uiCfg->button.textColor = parseColor(v);
         }});

    // --- font ---
    registerProperty({"font.size", "Font size in pixels", [this](const std::string &v) {
                          int newSize = std::stoi(v);
                          if (newSize != ctx.uiCfg->fontSize) {
                              ctx.uiCfg->fontSize = newSize;
                              if (ctx.reloadFont) {
                                  ctx.reloadFont(newSize);
                              }
                          }
                      }});

    // --- advance_keys ---
    registerProperty({"advance_keys",
                      "Keys that advance dialogue (space enter click)",
                      [this](const std::string &v) {
                          ctx.uiCfg->advanceKeys.clear();
                          std::string keyStr = v;
                          size_t pos = 0;
                          while (pos < keyStr.size()) {
                              size_t space = keyStr.find(' ', pos);
                              std::string keyName = keyStr.substr(
                                  pos,
                                  space == std::string::npos ? std::string::npos : space - pos);
                              if (!keyName.empty()) {
                                  if (keyName != "click") {
                                      SDL_Keycode k = parseKeyName(keyName);
                                      if (k != SDLK_UNKNOWN) {
                                          ctx.uiCfg->advanceKeys.push_back(k);
                                      }
                                      else {
                                          std::cerr << "[CONFIG] Unknown key: " << keyName << "\n";
                                      }
                                  }
                              }
                              if (space == std::string::npos)
                                  break;
                              pos = space + 1;
                          }
                      }});
}

}  // namespace config
