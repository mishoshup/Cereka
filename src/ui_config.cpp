// ui_config.cpp — ApplyUiSet using ConfigManager + font loading

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
// InitConfigManager — setup ConfigManager with engine context
// ---------------------------------------------------------------------------

void Impl::InitConfigManager()
{
    config::ApplyContext ctx;
    ctx.renderer = renderer;
    ctx.fontPath = fontPath;
    ctx.uiCfg = &uiCfg;

    ctx.reloadFont = [this](int size) { LoadFont(size); };
    ctx.loadTexture = [this](SDL_Texture *&tex, const std::string &path) {
        if (tex) {
            SDL_DestroyTexture(tex);
            tex = nullptr;
        }
        if (!path.empty()) {
            tex = IMG_LoadTexture(renderer, path.c_str());
            if (!tex) {
                std::cerr << "[CONFIG] Failed to load texture '" << path << "': " << SDL_GetError()
                          << "\n";
            }
        }
    };

    configManager.setContext(ctx);
    configManager.initDefaults();
}

// ---------------------------------------------------------------------------
// ApplyUiSet — delegate to ConfigManager
// ---------------------------------------------------------------------------

void Impl::ApplyUiSet(const std::string &key,
                      const std::string &val)
{
    configManager.apply(key, val);
}
