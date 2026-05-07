#pragma once
#include <SDL3/SDL.h>

namespace cereka::video {

extern SDL_Window *window;
extern int width;
extern int height;

/******************/
/* Initialization */
/******************/

/**
 * Initialize the video subsystem.
 *
 * This must be called before attempting to use any video functions.
 */
void init_video();

/**
 * Deinitialize the video subsystem.
 *
 * This flushes all texture caches and disconnects the SDL video subsystem.
 */
void deinit_video();

void create_window(const char *title,
                   bool fullscreen,
                   int width,
                   int height);

}  // namespace cereka::video
