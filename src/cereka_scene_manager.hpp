#pragma once

#include "renderer/irender_context.hpp"
#include "renderer/irecture.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace cereka {

class SceneManager {
   public:
    enum class FadePhase { None, Out, In };

    struct CharacterEntry {
        std::shared_ptr<ITexture> tex;
        float xNorm;  // 0.0–1.0 horizontal centre
    };

    void Init(IRenderContext &renderCtx);
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

    ITexture *Background() const { return background.get(); }
    const std::string &BgPath() const { return bgPath; }
    const std::unordered_map<std::string, CharacterEntry> &Characters() const { return characters; }
    const std::unordered_map<std::string, std::string> &CharPaths() const { return charPaths; }

    FadePhase Phase() const { return fadePhase; }
    float FadeTimer() const { return fadeTimer; }
    float FadePhaseDuration() const { return fadePhaseDuration; }

    static float posToXNorm(const std::string &pos);

   private:
    std::shared_ptr<ITexture> loadBg(const std::string &filename);

    IRenderContext *m_renderCtx = nullptr;
    std::shared_ptr<ITexture> background;
    std::string bgPath;
    std::unordered_map<std::string, CharacterEntry> characters;
    std::unordered_map<std::string, std::string> charPaths;
    std::shared_ptr<ITexture> pendingBg;
    FadePhase fadePhase = FadePhase::None;
    float fadePhaseDuration = 0.25f;
    float fadeTimer = 0.0f;
};

}  // namespace cereka
