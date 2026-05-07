// ui_config.cpp — ConfigManager integration + font loading
//
// Font loading is still here as it's engine-specific.
// ConfigManager handles all UI properties via the property system.

#include "cereka_engine_impl.hpp"

using namespace cereka::text_renderer;

// ============================================================================
// Font Loading
// ============================================================================

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

// ============================================================================
// ConfigManager Initialization
// ============================================================================

void Impl::InitConfigManager()
{
    cereka::config::ApplyContext ctx;
    ctx.renderCtx = m_renderCtx.get();
    ctx.fontPath = fontPath;
    ctx.uiCfg = &uiCfg;

    ctx.reloadFont = [this](int size) { LoadFont(size); };

    ctx.loadTexture = [this](ITexture *&tex, const std::string &path) {
        if (tex) {
            delete tex;
            tex = nullptr;
        }
        if (!path.empty()) {
            auto result = m_renderCtx->CreateTexture(path.c_str());
            tex = result.release();
            if (!tex) {
                std::cerr << "[CONFIG] Failed to load texture: " << path << "\n";
            }
        }
    };

    ctx.destroyTexture = [](ITexture *&tex) {
        delete tex;
        tex = nullptr;
    };

    configManager.setContext(ctx);
    configManager.initDefaults();
}

// ============================================================================
// ApplyUiSet — delegate to ConfigManager
// ============================================================================

void Impl::ApplyUiSet(const std::string &key,
                      const std::string &val)
{
    configManager.apply(key, val);
}
