#pragma once

#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <unordered_map>

namespace cereka {

class AudioManager {
   public:
    bool Init();
    void Shutdown();

    void PlayBGM(const std::string &filename);
    void StopBGM();
    void PlaySFX(const std::string &filename);

    const std::string &BgmPath() const { return bgmPath; }
    bool IsInitialized() const { return initialized; }

   private:
    void destroyBgmHandles();

    bool initialized = false;
    MIX_Mixer *mixer = nullptr;
    MIX_Audio *bgmAudio = nullptr;
    MIX_Track *bgmTrack = nullptr;
    std::string bgmPath;  // last filename passed to PlayBGM (for save/load)
    std::unordered_map<std::string, MIX_Audio *> sfxCache;
};

}  // namespace cereka
