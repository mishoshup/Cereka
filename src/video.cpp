#include "video.hpp"
#include "Cereka/exceptions.hpp"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_init.h"
#include <SDL3/SDL.h>
#include <cassert>
#include <iostream>

namespace cereka::video {

SDL_Window *window = nullptr;
int width = 0;
int height = 0;

void init_video() {
  if (SDL_WasInit(SDL_INIT_VIDEO)) {
    throw engine::error("Could not initialize SDL_video: %s", SDL_GetError());
  }

  if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false) {
    throw engine::error("Could not initialize SDL_video: %s", SDL_GetError());
  }
}

void deinit_video() {
  // SDL_INIT_TIMER is always initialized at program start.
  // If it is not initialized here, there is a problem.
  // assert(SDL_WasInit(SDL_INIT_VIDEO));

  // Clear any static texture caches,
  // lest they try to delete textures after SDL_Quit.
  // image::flush_cache();
  // render_texture_.reset();
  // current_render_target_.reset();

  // Destroy the window, and thus also the renderer.
  // window.reset();

  // Close the video subsystem.
  if (SDL_WasInit(SDL_INIT_VIDEO)) {
    SDL_Log("quitting SDL video subsystem");
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
  if (SDL_WasInit(SDL_INIT_VIDEO)) {
    // This should not have been initialized multiple times
    throw engine::error("video subsystem still initialized after deinit\n");
  }
}

void create_window(const char *title, bool fullscreen, int width, int height) {
  Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (fullscreen)
    flags |= SDL_WINDOW_FULLSCREEN;

  SDL_DisplayID id = SDL_GetPrimaryDisplay();
  std::cout << "display id: " << id << std::endl;

  const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(id);
  video::width = mode->w;
  video::height = mode->h;

  std::cout << "Width: " << video::width << std::endl;
  std::cout << "Height: " << video::height << std::endl;

  video::window = SDL_CreateWindow(title, video::width, video::height, flags);
  if (!video::window) {
    throw engine::error("Create window failed: %s", SDL_GetError());
  }
}
} // namespace cereka::video
