#pragma once
#include <SDL3_ttf/SDL_ttf.h>
#include <string>

namespace cereka::text_renderer {

/**
 * Initialize the TTF text renderer. Throws cereka::engine::Error on failure.
 */
void init_ttf();

/**
 * Open the font for the application
 *
 * This must be called before attempting to use any TTF functions.
 */
TTF_Font *OpenFont(const std::string &fontPath,
                   int fontSize);
}  // namespace cereka::text_renderer
