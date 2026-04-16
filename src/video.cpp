#include "video.hpp"
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

    SDL_DisplayID id = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(id);
    video::width  = mode->w;
    video::height = mode->h;

    video::window = SDL_CreateWindow(title, video::width, video::height, flags);
    if (!video::window) {
        throw engine::error("Create window failed: %s", SDL_GetError());
    }
}

}  // namespace cereka::video
