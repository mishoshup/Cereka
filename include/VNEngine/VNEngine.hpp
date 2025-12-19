#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

struct SDL_Renderer;
using RendererHandle = SDL_Renderer *;

struct VNEvent {
  enum Type { Quit, KeyDown, MouseDown, Unknown };
  Type type = Unknown;
  unsigned int key = 0;
};

class VNEngine {
public:
  VNEngine() = default;
  ~VNEngine() { ShutDown(); }

  bool Init(const char *title, int w, int h, bool fullscreen);
  void ShutDown();

  bool PollEvent(VNEvent &e);
  void Present();

  SDL_Renderer *Renderer() const { return m_renderer; }
  int Width() const { return m_screenWidth; }
  int Height() const { return m_screenHeight; }

  void LoadScript(const std::string &filename);
  void Reset();
  void HandleEvent(const VNEvent &e);
  void Update(float dt);
  void Draw();
  bool IsFinished() const { return m_scriptFinished; }

private:
  SDL_Window *m_window = nullptr;
  SDL_Renderer *m_renderer = nullptr;
  int m_screenWidth = 0;
  int m_screenHeight = 0;
  TTF_Font *m_font = nullptr;

  SDL_Texture *m_background = nullptr;
  SDL_Texture *m_textBox = nullptr;
  SDL_Texture *m_nameBox = nullptr;
  std::unordered_map<std::string, SDL_Texture *> m_characters;

  sol::state m_lua;
  bool m_scriptFinished = false;

  std::string m_currentSpeaker;
  std::string m_currentName;
  std::string m_currentText;

  float m_typewriterTimer = 0.f;
  int m_displayedChars = 0;
  static constexpr float CHARS_PER_SECOND = 60.f;

  void ShowBackground(const std::string &filename);
  void ShowCharacter(const std::string &id, const std::string &pos = "center");
  void HideCharacter(const std::string &id);
  void Say(const std::string &speaker, const std::string &name,
           const std::string &text);
  void Narrate(const std::string &text);

  SDL_Texture *LoadTexture(const std::string &path);
  SDL_Texture *CreateSolidTexture(int w, int h, Uint8 r, Uint8 g, Uint8 b,
                                  Uint8 a);
  SDL_Texture *RenderText(const std::string &text, SDL_Color color);
  SDL_Renderer *CreateBestRenderer(SDL_Window *win);

  void HandleSDLEvent(const SDL_Event &sdl);
};
