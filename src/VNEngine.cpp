#include "VNEngine/VNEngine.hpp"
#include "SDL3/SDL_error.h"
#include <iostream>

bool VNEngine::Init(const char *title, int w, int h, bool fullscreen) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return false;
  }

  if (TTF_Init() < 0) {
    std::cerr << "TTF_Init failed: " << SDL_GetError() << '\n';
    return false;
  }

  Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (fullscreen)
    flags |= SDL_WINDOW_FULLSCREEN;

  m_window = SDL_CreateWindow(title, w, h, flags);
  if (!m_window) {
    std::cerr << "CreateWindow failed: " << SDL_GetError() << '\n';
    return false;
  }

  m_renderer = CreateBestRenderer(m_window);
  if (!m_renderer) {
    std::cerr << "All renderer attempts failed\n";
    return false;
  }

  m_screenWidth = w;
  m_screenHeight = h;

  m_font = TTF_OpenFont("assets/fonts/Montserrat-Medium.ttf", 36);
  if (!m_font)
    std::cerr << "VNEngine: Failed to load font\n";

  m_textBox = CreateSolidTexture(
      m_screenWidth, static_cast<int>(m_screenHeight * 0.25f), 20, 20, 40, 220);
  m_nameBox = CreateSolidTexture(300, 60, 40, 40, 80, 240);

  m_lua.open_libraries(sol::lib::base, sol::lib::string);
  m_lua.set_function("bg", [this](const std::string &f) { ShowBackground(f); });
  m_lua.set_function(
      "show", [this](const std::string &c, sol::optional<std::string> pos) {
        ShowCharacter(c, pos.value_or("center"));
      });
  m_lua.set_function("hide",
                     [this](const std::string &c) { HideCharacter(c); });
  m_lua.set_function("say", [this](sol::variadic_args va) {
    std::string speaker = va[0].as<std::string>();
    std::string name = va.size() >= 2 ? va[1].as<std::string>() : speaker;
    std::string text = va[2].as<std::string>();
    Say(speaker, name, text);
  });
  m_lua.set_function("narrate", [this](const std::string &t) { Narrate(t); });
  /* --------------------------------------------- */

  return true;
}

void VNEngine::ShutDown() {
  if (m_background) {
    SDL_DestroyTexture(m_background);
    m_background = nullptr;
  }
  if (m_textBox) {
    SDL_DestroyTexture(m_textBox);
    m_textBox = nullptr;
  }
  if (m_nameBox) {
    SDL_DestroyTexture(m_nameBox);
    m_nameBox = nullptr;
  }

  for (auto &[id, tex] : m_characters)
    SDL_DestroyTexture(tex);
  m_characters.clear();

  if (m_font) {
    TTF_CloseFont(m_font);
    m_font = nullptr;
  }
  if (m_renderer) {
    SDL_DestroyRenderer(m_renderer);
    m_renderer = nullptr;
  }
  if (m_window) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }

  SDL_Quit();
}

bool VNEngine::PollEvent(VNEvent &e) {
  SDL_Event sdl;
  if (!SDL_PollEvent(&sdl))
    return false;

  switch (sdl.type) {
  case SDL_EVENT_QUIT:
    e = {VNEvent::Quit, 0};
    return true;
  case SDL_EVENT_KEY_DOWN:
    e = {VNEvent::KeyDown, sdl.key.key};
    return true;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    e = {VNEvent::MouseDown, 0};
    return true;
  default:
    e = {VNEvent::Unknown, 0};
    return true;
  }
}

void VNEngine::Present() { SDL_RenderPresent(m_renderer); }

void VNEngine::HandleEvent(const VNEvent &e) {
  SDL_Event sdl{};
  switch (e.type) {
  case VNEvent::Quit:
    sdl.type = SDL_EVENT_QUIT;
    break;
  case VNEvent::KeyDown:
    sdl.type = SDL_EVENT_KEY_DOWN;
    sdl.key.key = e.key;
    break;
  case VNEvent::MouseDown:
    sdl.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    break;
  default:
    return;
  }
  HandleSDLEvent(sdl);
}

// ----------  stubs – fill later  ----------
void VNEngine::LoadScript(const std::string &) {}
void VNEngine::Reset() {}
void VNEngine::Update(float) {}
void VNEngine::Draw() {}
// bool VNEngine::IsFinished() const { return false; }

void VNEngine::ShowBackground(const std::string &) {}
void VNEngine::ShowCharacter(const std::string &, const std::string &) {}
void VNEngine::HideCharacter(const std::string &) {}
void VNEngine::Say(const std::string &, const std::string &,
                   const std::string &) {}
void VNEngine::Narrate(const std::string &) {}

SDL_Texture *VNEngine::LoadTexture(const std::string &) { return nullptr; }
SDL_Texture *VNEngine::CreateSolidTexture(int, int, Uint8, Uint8, Uint8,
                                          Uint8) {
  return nullptr;
}
SDL_Texture *VNEngine::RenderText(const std::string &, SDL_Color) {
  return nullptr;
}
SDL_Renderer *VNEngine::CreateBestRenderer(SDL_Window *) { return nullptr; }

void VNEngine::HandleSDLEvent(const SDL_Event &) {}
