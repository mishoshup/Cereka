#pragma once
// sdl_render_context.hpp — SDL3 implementation of IRenderContext

#include "irender_context.hpp"
#include <SDL3/SDL.h>

namespace cereka {

class SdlRenderContext : public IRenderContext {
public:
    SdlRenderContext(SDL_Renderer *renderer, int width, int height);
    ~SdlRenderContext() override = default;

    // --- Lifecycle ---
    void Clear(Color c) override;
    void Present() override;

    // --- Drawing ---
    void FillRect(Rect rect, Color c) override;
    void FillScreen(Color c) override;
    void DrawTexture(ITexture &tex,
                     const Rect *srcRect,
                     const Rect *dstRect) override;

    // --- Texture factory ---
    std::unique_ptr<ITexture> CreateTexture(const std::string &filepath) override;
    std::unique_ptr<ITexture> CreateTextTexture(
        TTF_Font *font, const std::string &text, Color color) override;
    std::unique_ptr<ITexture> CreateTextTextureWrapped(
        TTF_Font *font, const std::string &text, Color color, int wrapWidth) override;

    // --- Rich text ---
    float DrawRichText(TTF_Font *baseFont,
                       const std::vector<text::TextSegment> &segments,
                       float x, float y, float maxWidth) override;

    // --- Blend mode ---
    void SetBlendMode(bool enabled) override;

    // --- Dimensions ---
    int Width() const override { return m_width; }
    int Height() const override { return m_height; }

    // --- Escape hatch ---
    SDL_Renderer *NativeRenderer() override { return m_renderer; }

private:
    // Inner class that wraps an SDL_Texture* behind ITexture.
    // Members are public since this is a private inner class (only
    // SdlRenderContext accesses it, and C++ has no special access
    // relationship between enclosing and nested classes).
    class SdlTexture : public ITexture {
    public:
        explicit SdlTexture(SDL_Texture *tex);
        ~SdlTexture() override;
        float Width() const override { return m_width; }
        float Height() const override { return m_height; }
        SDL_Texture *RawTexture() const override { return m_tex; }
    private:
        SDL_Texture *m_tex;
        float m_width = 0.0f;
        float m_height = 0.0f;
    };

    SDL_Renderer *m_renderer;
    int m_width;
    int m_height;
};

} // namespace cereka
