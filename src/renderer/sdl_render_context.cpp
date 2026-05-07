// sdl_render_context.cpp — SDL3 implementation of IRenderContext
//
// Delegates all rendering to SDL3. The inner SdlTexture class wraps
// SDL_Texture* behind the ITexture interface. Font rendering (TTF) is
// handled here via SDL_Surface → SDL_Texture pipeline.

#include "sdl_render_context.hpp"
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

namespace cereka {

// ============================================================================
// SdlTexture — ITexture wrapper around SDL_Texture*
// ============================================================================

SdlRenderContext::SdlTexture::SdlTexture(SDL_Texture *tex)
    : m_tex(tex)
{
    if (m_tex) {
        SDL_GetTextureSize(m_tex, &m_width, &m_height);
    }
}

SdlRenderContext::SdlTexture::~SdlTexture()
{
    if (m_tex) {
        SDL_DestroyTexture(m_tex);
        m_tex = nullptr;
    }
}

// ============================================================================
// SdlRenderContext
// ============================================================================

SdlRenderContext::SdlRenderContext(SDL_Renderer *renderer, int width, int height)
    : m_renderer(renderer)
    , m_width(width)
    , m_height(height)
{
}

void SdlRenderContext::Clear(Color c)
{
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(m_renderer);
}

void SdlRenderContext::Present()
{
    SDL_RenderPresent(m_renderer);
}

void SdlRenderContext::FillRect(Rect rect, Color c)
{
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    SDL_FRect fr{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderFillRect(m_renderer, &fr);
}

void SdlRenderContext::FillScreen(Color c)
{
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(m_renderer, nullptr);
}

void SdlRenderContext::DrawTexture(ITexture &tex,
                                   const Rect *srcRect,
                                   const Rect *dstRect)
{
    // SdlTexture is the only ITexture impl in this backend
    auto &sdlTex = static_cast<SdlTexture &>(tex);

    SDL_FRect src;
    SDL_FRect dst;
    SDL_FRect *srcPtr = nullptr;
    SDL_FRect *dstPtr = nullptr;

    if (srcRect) {
        src = {srcRect->x, srcRect->y, srcRect->w, srcRect->h};
        srcPtr = &src;
    }
    if (dstRect) {
        dst = {dstRect->x, dstRect->y, dstRect->w, dstRect->h};
        dstPtr = &dst;
    }

    SDL_RenderTexture(m_renderer, sdlTex.m_tex, srcPtr, dstPtr);
}

std::unique_ptr<ITexture> SdlRenderContext::CreateTexture(const std::string &filepath)
{
    SDL_Texture *tex = IMG_LoadTexture(m_renderer, filepath.c_str());
    if (!tex) {
        std::cerr << "[CEREKA] Failed to load texture: " << filepath
                  << " — " << SDL_GetError() << '\n';
        return nullptr;
    }
    return std::make_unique<SdlTexture>(tex);
}

std::unique_ptr<ITexture> SdlRenderContext::CreateTextTexture(
    TTF_Font *font, const std::string &text, Color color)
{
    if (text.empty() || !font)
        return nullptr;

    SDL_Color sdlColor{color.r, color.g, color.b, color.a};
    SDL_Surface *surf = TTF_RenderText_Blended(font, text.c_str(), text.size(), sdlColor);
    if (!surf)
        return nullptr;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    SDL_DestroySurface(surf);
    if (!tex)
        return nullptr;

    return std::make_unique<SdlTexture>(tex);
}

std::unique_ptr<ITexture> SdlRenderContext::CreateTextTextureWrapped(
    TTF_Font *font, const std::string &text, Color color, int wrapWidth)
{
    if (text.empty() || !font)
        return nullptr;

    SDL_Color sdlColor{color.r, color.g, color.b, color.a};
    SDL_Surface *surf = TTF_RenderText_Blended_Wrapped(
        font, text.c_str(), text.size(), sdlColor, wrapWidth > 0 ? wrapWidth : 0);
    if (!surf)
        return nullptr;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    SDL_DestroySurface(surf);
    if (!tex)
        return nullptr;

    return std::make_unique<SdlTexture>(tex);
}

void SdlRenderContext::SetBlendMode(bool enabled)
{
    SDL_SetRenderDrawBlendMode(m_renderer,
                               enabled ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
}

} // namespace cereka
