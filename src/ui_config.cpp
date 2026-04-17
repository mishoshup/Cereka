// ui_config.cpp — ApplyUiSet (processes `ui` blocks from .crka scripts) + font loading

#include "engine_impl.hpp"
#include <cstdio>

// ---------------------------------------------------------------------------
// Font loading (also called from InitGame and when font.size changes)
// ---------------------------------------------------------------------------

void Impl::LoadFont(int size)
{
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }

    if (!fontPath.empty()) {
        font = text_renderer::OpenFont(fontPath, size);
        return;
    }

    // Discover the first .ttf/.otf in assets/fonts/
    std::error_code ec;
    for (auto &entry : fs::directory_iterator("assets/fonts", ec)) {
        auto ext = entry.path().extension().string();
        if (ext == ".ttf" || ext == ".otf") {
            fontPath = entry.path().string();
            font = text_renderer::OpenFont(fontPath, size);
            if (font)
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// ApplyUiSet — called by TickScript when it hits a UI_SET instruction
// ---------------------------------------------------------------------------

void Impl::ApplyUiSet(const std::string &key,
                      const std::string &val)
{
    // Parse "r g b a" → SDL_Color
    auto color4 = [](const std::string &s) -> SDL_Color {
        int r = 255, g = 255, b = 255, a = 255;
        std::sscanf(s.c_str(), "%d %d %d %d", &r, &g, &b, &a);
        return {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
    };

    // Load a texture, destroying the previous one if present
    auto reloadTex = [this](SDL_Texture *&tex, const std::string &path) {
        if (tex) {
            SDL_DestroyTexture(tex);
            tex = nullptr;
        }
        tex = IMG_LoadTexture(renderer, path.c_str());
        if (!tex)
            std::cerr << "[CEREKA] ui: failed to load '" << path << "': " << SDL_GetError()
                      << "\n";
    };

    // --- textbox ---
    if (key == "textbox.image") {
        reloadTex(uiCfg.textbox.image, val);
    }
    else if (key == "textbox.color") {
        uiCfg.textbox.color = color4(val);
    }
    else if (key == "textbox.y") {
        uiCfg.textbox.y = Dim::parse(val);
    }
    else if (key == "textbox.h") {
        uiCfg.textbox.h = Dim::parse(val);
    }
    else if (key == "textbox.text_margin_x") {
        uiCfg.textbox.textMarginX = std::stof(val);
    }
    else if (key == "textbox.text_color") {
        uiCfg.textbox.textColor = color4(val);
    }

    // --- namebox ---
    else if (key == "namebox.image") {
        reloadTex(uiCfg.namebox.image, val);
    }
    else if (key == "namebox.color") {
        uiCfg.namebox.color = color4(val);
    }
    else if (key == "namebox.x") {
        uiCfg.namebox.x = std::stof(val);
    }
    else if (key == "namebox.y_offset") {
        uiCfg.namebox.yOffset = std::stof(val);
    }
    else if (key == "namebox.w") {
        uiCfg.namebox.w = std::stof(val);
    }
    else if (key == "namebox.h") {
        uiCfg.namebox.h = std::stof(val);
    }
    else if (key == "namebox.text_color") {
        uiCfg.namebox.textColor = color4(val);
    }

    // --- button ---
    else if (key == "button.image") {
        reloadTex(uiCfg.button.image, val);
    }
    else if (key == "button.hover_image") {
        reloadTex(uiCfg.button.hoverImage, val);
    }
    else if (key == "button.color") {
        uiCfg.button.color = color4(val);
    }
    else if (key == "button.w") {
        uiCfg.button.w = std::stof(val);
    }
    else if (key == "button.h") {
        uiCfg.button.h = std::stof(val);
    }
    else if (key == "button.text_color") {
        uiCfg.button.textColor = color4(val);
    }

    // --- font ---
    else if (key == "font.size") {
        int newSize = std::stoi(val);
        if (newSize != uiCfg.fontSize) {
            uiCfg.fontSize = newSize;
            LoadFont(newSize);  // re-open font at new size
        }
    }

    // --- advance_keys ---
    else if (key == "advance_keys") {
        uiCfg.advanceKeys.clear();
        std::string keyStr = val;
        size_t pos = 0;
        while (pos < keyStr.size()) {
            size_t space = keyStr.find(' ', pos);
            std::string keyName = keyStr.substr(
                pos, space == std::string::npos ? std::string::npos : space - pos);
            if (!keyName.empty()) {
                if (keyName == "space")
                    uiCfg.advanceKeys.push_back(SDLK_SPACE);
                else if (keyName == "enter")
                    uiCfg.advanceKeys.push_back(SDLK_RETURN);
                else if (keyName == "escape")
                    uiCfg.advanceKeys.push_back(SDLK_ESCAPE);
                else if (keyName == "return")
                    uiCfg.advanceKeys.push_back(SDLK_RETURN);
                else if (keyName == "click") {
                    // special: mouse click is handled separately
                }
                else
                    std::cerr << "[CEREKA] Unknown advance key: " << keyName << "\n";
            }
            if (space == std::string::npos)
                break;
            pos = space + 1;
        }
    }

    else {
        std::cerr << "[CEREKA] Unknown ui property: " << key << "\n";
    }
}
