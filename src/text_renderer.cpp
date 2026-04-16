#include "text_renderer.hpp"
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

}  // namespace cereka::text_renderer
