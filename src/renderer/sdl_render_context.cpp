// sdl_render_context.cpp — SDL3 implementation of IRenderContext
//
// Delegates all rendering to SDL3. The inner SdlTexture class wraps
// SDL_Texture* behind the ITexture interface. Font rendering (TTF) is
// handled here via SDL_Surface → SDL_Texture pipeline.

#include "sdl_render_context.hpp"
#include "text/markup_parser.hpp"
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

SdlRenderContext::~SdlRenderContext()
{
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
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

    SDL_RenderTexture(m_renderer, sdlTex.RawTexture(), srcPtr, dstPtr);
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

float SdlRenderContext::DrawRichText(
    TTF_Font *font,
    const std::vector<text::TextSegment> &segments,
    float x, float y, float maxWidth)
{
    if (segments.empty() || !font)
        return 0.0f;

    struct PlacedSegment {
        const text::TextSegment *seg;
        size_t startOffset;
        size_t endOffset;
        float lineX;
        float lineY;
    };
    std::vector<PlacedSegment> placed;
    float currentX = x;
    float currentY = y;
    float lineHeight = (float)TTF_GetFontHeight(font);

    for (const auto &seg : segments) {
        size_t offset = 0;
        size_t textLen = seg.text.size();

        while (offset < textLen) {
            int measuredWidth = 0;
            int extent = 0;
            const char *textPtr = seg.text.c_str() + offset;
            size_t remaining = textLen - offset;

            size_t extentSz = 0;
            if (!TTF_MeasureString(font, textPtr, remaining,
                                   (int)(maxWidth - (currentX - x)),
                                   &measuredWidth, &extentSz))
                break;
            extent = (int)extentSz;

            if (extent == 0) {
                currentX = x;
                currentY += lineHeight;
                continue;
            }

            placed.push_back({&seg, offset, offset + (size_t)extent, currentX, currentY});
            currentX += (float)measuredWidth;
            offset += (size_t)extent;
        }
    }

    if (placed.empty())
        return 0.0f;

    for (const auto &ps : placed) {
        int style = 0;
        if (ps.seg->style.bold)          style |= TTF_STYLE_BOLD;
        if (ps.seg->style.italic)        style |= TTF_STYLE_ITALIC;
        if (ps.seg->style.underline)     style |= TTF_STYLE_UNDERLINE;
        if (ps.seg->style.strikethrough) style |= TTF_STYLE_STRIKETHROUGH;
        TTF_SetFontStyle(font, style);

        SDL_Color sdlColor{
            ps.seg->style.color.r,
            ps.seg->style.color.g,
            ps.seg->style.color.b,
            ps.seg->style.color.a
        };

        std::string subText = ps.seg->text.substr(ps.startOffset, ps.endOffset - ps.startOffset);
        SDL_Surface *surf = TTF_RenderText_Blended(
            font, subText.c_str(), subText.size(), sdlColor);
        if (!surf) continue;

        SDL_Texture *tex = SDL_CreateTextureFromSurface(m_renderer, surf);
        float tw = (float)surf->w;
        float th = (float)surf->h;
        SDL_DestroySurface(surf);

        if (tex) {
            SDL_FRect dst{ps.lineX, ps.lineY, tw, th};
            SDL_RenderTexture(m_renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
    }

    return (currentY - y) + lineHeight;
}

} // namespace cereka
