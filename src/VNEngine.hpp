#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

class VNEngine {
public:
  VNEngine(SDL_Renderer *renderer, int screenWidth, int screenHeight);
  ~VNEngine();

  void LoadScript(const std::string &filename);
  void Reset();

  void HandleEvent(const SDL_Event *event);
  void Update(float deltaTime);
  void Draw();

  bool IsFinished() const { return m_scriptFinished; }

private:
  SDL_Renderer *m_renderer;
  int m_screenWidth;
  int m_screenHeight;
  TTF_Font *m_font = nullptr;

  // Assets
  SDL_Texture *m_background = nullptr;
  SDL_Texture *m_textBox = nullptr;
  SDL_Texture *m_nameBox = nullptr;
  std::unordered_map<std::string, SDL_Texture *> m_characters;

  // Script state
  sol::state m_lua;
  bool m_scriptFinished = false;

  // Current dialogue
  std::string m_currentSpeaker;
  std::string m_currentName;
  std::string m_currentText;

  // Typewriter
  float m_typewriterTimer = 0.0f;
  int m_displayedChars = 0;
  static constexpr float CHARS_PER_SECOND = 60.0f;

  // Lua-exposed methods
  void ShowBackground(const std::string &filename);
  void ShowCharacter(const std::string &characterId,
                     const std::string &position = "center");
  void HideCharacter(const std::string &characterId);
  void Say(const std::string &speaker, const std::string &name,
           const std::string &text);
  void Narrate(const std::string &text);

  // Helpers
  SDL_Texture *LoadTexture(const std::string &path);
  SDL_Texture *CreateSolidTexture(int w, int h, Uint8 r, Uint8 g, Uint8 b,
                                  Uint8 a);
  SDL_Texture *RenderText(const std::string &text, SDL_Color color);
};
