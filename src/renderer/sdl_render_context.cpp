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
#include <string_view>

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
            size_t extentSz = 0;
            const char *textPtr = seg.text.c_str() + offset;
            size_t remaining = textLen - offset;
            float lineRemaining = maxWidth - (currentX - x);

            if (!TTF_MeasureString(font, textPtr, remaining,
                                   (int)lineRemaining,
                                   &measuredWidth, &extentSz))
                break;

            if (extentSz == 0) {
                // No glyph fits in the remaining width.
                if (currentX == x) {
                    // Even a single glyph doesn't fit at the start of a line.
                    // Force-advance at least one byte to prevent infinite loop
                    // (REVIEW-RENDERER.md CR-03). Handle UTF-8: skip past any
                    // continuation bytes to land on the next codepoint start.
                    size_t toSkip = 1;
                    while (offset + toSkip < textLen &&
                           (seg.text[offset + toSkip] & 0xC0) == 0x80)
                        toSkip++;
                    offset += toSkip;
                    currentX = x;
                    currentY += lineHeight;
                    continue;
                }
                // Wrap to next line and retry the same text position.
                currentX = x;
                currentY += lineHeight;
                continue;
            }

            // --- Word-wrap: scan back for word boundary ---
            // TTF_MeasureString breaks at any glyph boundary, which can split a
            // word across two lines.  If we didn't fit everything, look for the
            // last space character in the fitted range and break there instead.
            // For CJK text (which doesn't use spaces between words) and for
            // single long words wider than the line, this falls through to the
            // glyph-level break from TTF_MeasureString.
            if (extentSz < remaining) {
                std::string_view fitted(textPtr, extentSz);
                auto lastSpace = fitted.rfind(' ');

                if (lastSpace != std::string_view::npos) {
                    if (lastSpace == 0) {
                        // First character is a space at the start of the line
                        // (happens after a previous word-wrap). Skip it.
                        offset += 1;
                        continue;
                    }

                    // Break at the last word boundary.  Place text up to the
                    // space (exclusive), then advance past the space so the
                    // next word starts cleanly on the following line.
                    size_t wordEnd = lastSpace;  // bytes to place
                    size_t newExtentSz = 0;
                    int newMeasuredWidth = 0;
                    TTF_MeasureString(font, textPtr, wordEnd,
                                      (int)lineRemaining,
                                      &newMeasuredWidth, &newExtentSz);
                    if (newExtentSz > 0) {
                        placed.push_back({&seg, offset, offset + newExtentSz,
                                          currentX, currentY});
                        currentX += (float)newMeasuredWidth;
                        offset += newExtentSz + 1; // +1 skips the space
                        continue;
                    }
                }
                // No space found (long word) or re-measure failed.
                // Fall through to place the glyph-level break.
            }
            // --- End word-wrap adjustment ---

            placed.push_back({&seg, offset, offset + extentSz, currentX, currentY});
            currentX += (float)measuredWidth;
            offset += extentSz;
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
