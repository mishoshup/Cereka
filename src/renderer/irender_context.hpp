#pragma once
// irender_context.hpp — IRenderContext abstraction boundary
//
// Pure virtual interface that decouples engine logic from SDL rendering.
// All SDL3 types stay behind SdlRenderContext; engine code sees only
// Color, Rect, ITexture, and IRenderContext.
//
// NativeRenderer() is a temporary escape hatch during the migration from
// raw SDL_Renderer* to IRenderContext. Plan 03-03 (UIManager extraction)
// removes all remaining direct SDL_Renderer* access. Do NOT add new uses.

#include "irecture.hpp"
#include <cstdint>
#include <memory>
#include <string>

struct SDL_Renderer;  // forward declare for NativeRenderer escape hatch
struct TTF_Font;

namespace cereka {

// ============================================================================
// Color — RGBA with uint8_t channels (avoids pulling SDL_Color across boundary)
// ============================================================================
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

// ============================================================================
// Rect — float-based rectangle (avoids pulling SDL_FRect across boundary)
// ============================================================================
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

// ============================================================================
// IRenderContext — pure virtual render interface
// ============================================================================
class IRenderContext {
public:
    virtual ~IRenderContext() = default;

    // --- Lifecycle ---
    virtual void Clear(Color c) = 0;
    virtual void Present() = 0;

    // --- Drawing ---
    virtual void FillRect(Rect rect, Color c) = 0;
    virtual void FillScreen(Color c) = 0;
    virtual void DrawTexture(ITexture &tex,
                             const Rect *srcRect,
                             const Rect *dstRect) = 0;

    // --- Texture management — returns nullptr on failure ---
    virtual std::unique_ptr<ITexture> CreateTexture(const std::string &filepath) = 0;

    // --- Text textures — font stays as raw TTF_Font* (not wrapped) ---
    virtual std::unique_ptr<ITexture> CreateTextTexture(
        TTF_Font *font, const std::string &text, Color color) = 0;
    virtual std::unique_ptr<ITexture> CreateTextTextureWrapped(
        TTF_Font *font, const std::string &text, Color color, int wrapWidth) = 0;

    // --- Blend mode ---
    virtual void SetBlendMode(bool enabled) = 0;

    // --- Dimensions ---
    virtual int Width() const = 0;
    virtual int Height() const = 0;

    // --- Escape hatch for existing draw code during migration (03-03 removes this) ---
    [[deprecated("Use IRenderContext methods instead of accessing SDL_Renderer* directly")]]
    virtual SDL_Renderer *NativeRenderer() = 0;
};

} // namespace cereka
