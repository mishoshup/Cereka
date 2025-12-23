#include "Cereka/exceptions.hpp"
#include "SDL3/SDL_error.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cassert>
#include <iostream>

namespace cereka::text_renderer {

void init_ttf() {
  if (TTF_Init() == false) {
    throw engine::error("TTF Init Failed: %s\n", SDL_GetError());
  }
}

TTF_Font *OpenFont(const std::string &fontPath, int fontSize) {
  TTF_Font *font = TTF_OpenFont(fontPath.c_str(), fontSize);
  if (!font) {
    std::cerr << "Failed to open font '" << fontPath << "': " << SDL_GetError()
              << std::endl;
  }
  return font;
}

} // namespace cereka::text_renderer
