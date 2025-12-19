#include "VNEngine.hpp"
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

VNEngine::VNEngine(SDL_Renderer *renderer, int screenWidth, int screenHeight)
    : m_renderer(renderer), m_screenWidth(screenWidth),
      m_screenHeight(screenHeight) {

  m_font = TTF_OpenFont("assets/fonts/Montserrat-Medium.ttf", 36);
  if (!m_font) {
    std::cerr << "VNEngine: Failed to load font\n";
  }

  m_textBox = CreateSolidTexture(
      screenWidth, static_cast<int>(screenHeight * 0.25f), 20, 20, 40, 220);
  m_nameBox = CreateSolidTexture(300, 60, 40, 40, 80, 240);

  m_lua.open_libraries(sol::lib::base, sol::lib::string);

  // Bind functions to Lua
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
}

VNEngine::~VNEngine() {
  if (m_background)
    SDL_DestroyTexture(m_background);
  if (m_textBox)
    SDL_DestroyTexture(m_textBox);
  if (m_nameBox)
    SDL_DestroyTexture(m_nameBox);
  if (m_font)
    TTF_CloseFont(m_font);
  for (auto &[id, tex] : m_characters) {
    SDL_DestroyTexture(tex);
  }
}

void VNEngine::LoadScript(const std::string &filename) {
  auto result = m_lua.safe_script_file(
      filename, [](lua_State *, sol::protected_function_result pfr) {
        sol::error err = pfr;
        std::cerr << "Lua Script Error: " << err.what() << "\n";
        return pfr;
      });
  if (!result.valid()) {
    std::cerr << "Failed to load script: " << filename << "\n";
  }
}

void VNEngine::Reset() {
  m_currentText.clear();
  m_displayedChars = 0;
  m_typewriterTimer = 0.0f;
}

void VNEngine::ShowBackground(const std::string &filename) {
  if (m_background)
    SDL_DestroyTexture(m_background);
  m_background = LoadTexture("assets/bg/" + filename);
}

void VNEngine::ShowCharacter(const std::string &characterId,
                             const std::string &position) {
  HideCharacter(characterId); // Replace if already shown
  std::string path = "assets/characters/" + characterId + "_normal.png";
  SDL_Texture *tex = LoadTexture(path);
  if (tex)
    m_characters[characterId] = tex;
}

void VNEngine::HideCharacter(const std::string &characterId) {
  auto it = m_characters.find(characterId);
  if (it != m_characters.end()) {
    SDL_DestroyTexture(it->second);
    m_characters.erase(it);
  }
}

void VNEngine::Say(const std::string &speaker, const std::string &name,
                   const std::string &text) {
  m_currentSpeaker = speaker;
  m_currentName = name;
  m_currentText = text;
  m_displayedChars = 0;
  m_typewriterTimer = 0.0f;
}

void VNEngine::Narrate(const std::string &text) { Say("", "Narrator", text); }

SDL_Texture *VNEngine::LoadTexture(const std::string &path) {
  SDL_Texture *tex = IMG_LoadTexture(m_renderer, path.c_str());
  if (!tex) {
    std::cerr << "VNEngine: Failed to load texture '" << path
              << "': " << SDL_GetError() << "\n";
  }
  return tex;
}

SDL_Texture *VNEngine::CreateSolidTexture(int w, int h, Uint8 r, Uint8 g,
                                          Uint8 b, Uint8 a) {
  SDL_Texture *tex = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, w, h);
  if (!tex)
    return nullptr;
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(m_renderer, tex);
  SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
  SDL_RenderClear(m_renderer);
  SDL_SetRenderTarget(m_renderer, nullptr);
  return tex;
}

SDL_Texture *VNEngine::RenderText(const std::string &text, SDL_Color color) {
  if (text.empty() || !m_font)
    return nullptr;
  SDL_Surface *surf = TTF_RenderText_Blended(m_font, text.c_str(), 0, color);
  if (!surf)
    return nullptr;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(m_renderer, surf);
  SDL_DestroySurface(surf);
  return tex;
}

void VNEngine::Update(float deltaTime) {
  if (m_currentText.empty())
    return;

  if (m_displayedChars < static_cast<int>(m_currentText.length())) {
    m_typewriterTimer += deltaTime;
    int charsToAdd = static_cast<int>(m_typewriterTimer * CHARS_PER_SECOND);
    if (charsToAdd > 0) {
      m_displayedChars += charsToAdd;
      m_typewriterTimer -= charsToAdd / CHARS_PER_SECOND;
      if (m_displayedChars > static_cast<int>(m_currentText.length()))
        m_displayedChars = m_currentText.length();
    }
  }
}

void VNEngine::HandleEvent(const SDL_Event *event) {
  if (m_currentText.empty())
    return;

  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
      event->type == SDL_EVENT_KEY_DOWN) {
    bool full = m_displayedChars >= static_cast<int>(m_currentText.length());
    if (full) {
      m_currentText.clear();
    } else {
      m_displayedChars = m_currentText.length();
    }
  }
}

void VNEngine::Draw() {
  if (m_background) {
    SDL_RenderTexture(m_renderer, m_background, nullptr, nullptr);
  }

  // Draw characters (simple horizontal layout)
  float xPos = m_screenWidth * 0.1f;
  const float spacing = m_screenWidth * 0.3f;
  for (const auto &[id, tex] : m_characters) {
    float tw, th;
    SDL_GetTextureSize(tex, &tw, &th);
    float scale = (m_screenHeight * 0.8f) / th;
    SDL_FRect dst = {xPos, m_screenHeight - th * scale - m_screenHeight * 0.1f,
                     tw * scale, th * scale};
    SDL_RenderTexture(m_renderer, tex, nullptr, &dst);
    xPos += spacing;
  }

  if (m_currentText.empty())
    return;

  // Textbox
  if (m_textBox) {
    SDL_FRect tb{0, m_screenHeight * 0.75f, static_cast<float>(m_screenWidth),
                 m_screenHeight * 0.25f};
    SDL_RenderTexture(m_renderer, m_textBox, nullptr, &tb);
  }

  // Namebox
  if (!m_currentSpeaker.empty() && m_nameBox) {
    SDL_FRect nb{50, m_screenHeight * 0.75f - 70, 300, 60};
    SDL_RenderTexture(m_renderer, m_nameBox, nullptr, &nb);

    SDL_Texture *nameTex = RenderText(m_currentName, {255, 255, 255, 255});
    if (nameTex) {
      float w, h;
      SDL_GetTextureSize(nameTex, &w, &h);
      SDL_FRect dst{70, m_screenHeight * 0.75f - 60, w, h};
      SDL_RenderTexture(m_renderer, nameTex, nullptr, &dst);
      SDL_DestroyTexture(nameTex);
    }
  }

  // Dialogue text
  std::string visible = m_currentText.substr(0, m_displayedChars);
  SDL_Texture *textTex = RenderText(visible, {255, 255, 255, 255});
  if (textTex) {
    float w, h;
    SDL_GetTextureSize(textTex, &w, &h);
    float margin = 70.0f;
    float maxW = m_screenWidth - 2 * margin;
    float scale = (w > maxW) ? maxW / w : 1.0f;
    SDL_FRect dst{margin, m_screenHeight * 0.80f, w * scale, h * scale};
    SDL_RenderTexture(m_renderer, textTex, nullptr, &dst);
    SDL_DestroyTexture(textTex);
  }
}
