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

/**
 * Render text as a blended texture (no wrapping).
 * Returns nullptr on failure or empty input.
 */
SDL_Texture *RenderText(TTF_Font *font,
                        SDL_Renderer *renderer,
                        const std::string &text,
                        SDL_Color color);

/**
 * Render text as a blended texture with word wrapping.
 * wrapWidth == 0 means no wrap constraint.
 * Returns nullptr on failure or empty input.
 */
SDL_Texture *RenderTextWrapped(TTF_Font *font,
                               SDL_Renderer *renderer,
                               const std::string &text,
                               SDL_Color color,
                               int wrapWidth);

}  // namespace cereka::text_renderer
