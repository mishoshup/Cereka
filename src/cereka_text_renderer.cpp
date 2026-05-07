#include "cereka_text_renderer.hpp"
#include "Cereka/exceptions.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

namespace cereka::text_renderer {

void init_ttf()
{
    if (!TTF_Init()) {
        throw engine::error("TTF_Init failed: %s", SDL_GetError());
    }
}

TTF_Font *OpenFont(const std::string &fontPath,
                   int fontSize)
{
    TTF_Font *font = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (!font) {
        std::cerr << "[CEREKA] Failed to open font '" << fontPath << "': "
                  << SDL_GetError() << '\n';
    }
    return font;
}

SDL_Texture *RenderText(TTF_Font *font,
                        SDL_Renderer *renderer,
                        const std::string &text,
                        SDL_Color color)
{
    if (text.empty() || !font)
        return nullptr;
    SDL_Surface *surf = TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
    if (!surf)
        return nullptr;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

SDL_Texture *RenderTextWrapped(TTF_Font *font,
                               SDL_Renderer *renderer,
                               const std::string &text,
                               SDL_Color color,
                               int wrapWidth)
{
    if (text.empty() || !font)
        return nullptr;
    SDL_Surface *surf = TTF_RenderText_Blended_Wrapped(
        font, text.c_str(), text.size(), color, wrapWidth > 0 ? wrapWidth : 0);
    if (!surf)
        return nullptr;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

}  // namespace cereka::text_renderer
