#pragma once

#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <unordered_map>

namespace cereka {

enum class FadeCurve { Linear, EaseIn, EaseOut, EaseInOut };

struct BgmFade {
    enum class State { None, FadingOut, FadingIn, CrossfadeOut, CrossfadeIn };
    State state = State::None;
    float timer = 0.0f;
    float duration = 0.0f;
    FadeCurve curve = FadeCurve::Linear;

    MIX_Track *oldTrack = nullptr;
    MIX_Audio *oldAudio = nullptr;
    std::string oldPath;
};

class AudioManager {
   public:
    bool Init();
    void Shutdown();

    void PlayBGM(const std::string &filename);
    void PlayBGM(const std::string &filename, float fadeDuration);
    void StopBGM();
    void StopBGM(float fadeDuration);
    void CrossfadeBGM(const std::string &filename, float duration);
    void PlaySFX(const std::string &filename);

    void Update(float dt);

    const std::string &BgmPath() const { return bgmPath; }
    bool IsInitialized() const { return initialized; }

   private:
    void destroyBgmHandles();

    bool initialized = false;
    MIX_Mixer *mixer = nullptr;
    MIX_Audio *bgmAudio = nullptr;
    MIX_Track *bgmTrack = nullptr;
    std::string bgmPath;
    std::unordered_map<std::string, MIX_Audio *> sfxCache;
    BgmFade fadeState_;
};

}  // namespace cereka
