#include "cereka_video.hpp"
#include "Cereka/exceptions.hpp"
#include <SDL3/SDL.h>

namespace cereka::video {

SDL_Window *window = nullptr;
int width = 0;
int height = 0;

void init_video()
{
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        throw engine::error("SDL video subsystem already initialized");
    }

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        throw engine::error("Could not initialize SDL video: %s", SDL_GetError());
    }
}

void deinit_video()
{
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Log("quitting SDL video subsystem");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

void create_window(const char *title,
                   bool fullscreen,
                   int width,
                   int height)
{
    Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    video::width  = width;
    video::height = height;

    video::window = SDL_CreateWindow(title, width, height, flags);
    if (!video::window) {
        throw engine::error("Create window failed: %s", SDL_GetError());
    }
}

}  // namespace cereka::video
