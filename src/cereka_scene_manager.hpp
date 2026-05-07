#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>

namespace cereka {

class SceneManager {
   public:
    enum class FadePhase { None, Out, In };

    struct CharacterEntry {
        SDL_Texture *tex;
        float xNorm;  // 0.0–1.0 horizontal centre
    };

    void Init(SDL_Renderer *r);
    void Shutdown();

    void ShowBackground(const std::string &filename);
    void ShowCharacter(const std::string &id,
                       const std::string &filename,
                       const std::string &pos);
    void HideCharacter(const std::string &id);

    // Begin a crossfade: fade current bg out then new bg in over totalDuration seconds.
    void StartFade(const std::string &filename,
                   float totalDuration);
    // Advance fade by dt. Returns true when the fade finishes on this tick.
    bool TickFade(float dt);

    // Tear down all textures (used by Reset and LoadGame).
    void Clear();

    SDL_Texture *Background() const { return background; }
    const std::string &BgPath() const { return bgPath; }
    const std::unordered_map<std::string, CharacterEntry> &Characters() const { return characters; }
    const std::unordered_map<std::string, std::string> &CharPaths() const { return charPaths; }

    FadePhase Phase() const { return fadePhase; }
    float FadeTimer() const { return fadeTimer; }
    float FadePhaseDuration() const { return fadePhaseDuration; }

    static float posToXNorm(const std::string &pos);

   private:
    SDL_Texture *loadBg(const std::string &filename);

    SDL_Renderer *renderer = nullptr;
    SDL_Texture *background = nullptr;
    std::string bgPath;
    std::unordered_map<std::string, CharacterEntry> characters;
    std::unordered_map<std::string, std::string> charPaths;
    SDL_Texture *pendingBg = nullptr;
    FadePhase fadePhase = FadePhase::None;
    float fadePhaseDuration = 0.25f;
    float fadeTimer = 0.0f;
};

}  // namespace cereka
