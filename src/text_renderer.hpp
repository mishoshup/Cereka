#pragma once
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

namespace cereka::text_renderer {

/**
 * Initialize the TTF text renderer.
 *
 * This must be called before attempting to use any TTF functions.
 */
bool init_ttf();

/**
 * Open the font for the application
 *
 * This must be called before attempting to use any TTF functions.
 */
TTF_Font *OpenFont(const std::string &fontPath, int fontSize);
} // namespace cereka::text_renderer
