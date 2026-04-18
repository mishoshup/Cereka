#include "audio_manager.hpp"

#include <SDL3/SDL.h>
#include <iostream>

namespace cereka {

bool AudioManager::Init()
{
    if (!MIX_Init()) {
        std::cerr << "[CEREKA] MIX_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer) {
        std::cerr << "[CEREKA] MIX_CreateMixerDevice failed: " << SDL_GetError() << "\n";
        MIX_Quit();
        return false;
    }
    initialized = true;
    return true;
}

void AudioManager::Shutdown()
{
    if (!initialized)
        return;
    destroyBgmHandles();
    bgmPath.clear();
    for (auto &[name, audio] : sfxCache)
        MIX_DestroyAudio(audio);
    sfxCache.clear();
    MIX_DestroyMixer(mixer);
    mixer = nullptr;
    MIX_Quit();
    initialized = false;
}

void AudioManager::destroyBgmHandles()
{
    if (bgmTrack) {
        MIX_StopTrack(bgmTrack, 0);
        MIX_DestroyTrack(bgmTrack);
        bgmTrack = nullptr;
    }
    if (bgmAudio) {
        MIX_DestroyAudio(bgmAudio);
        bgmAudio = nullptr;
    }
}

void AudioManager::PlayBGM(const std::string &filename)
{
    if (!initialized)
        return;

    destroyBgmHandles();
    bgmPath = filename;

    std::string path = "assets/sounds/" + filename;
    bgmAudio = MIX_LoadAudio(mixer, path.c_str(), true);  // pre-decode for looping
    if (!bgmAudio) {
        std::cerr << "[CEREKA] Failed to load BGM: " << path << " — " << SDL_GetError() << "\n";
        return;
    }

    bgmTrack = MIX_CreateTrack(mixer);
    if (!bgmTrack) {
        std::cerr << "[CEREKA] Failed to create BGM track: " << SDL_GetError() << "\n";
        MIX_DestroyAudio(bgmAudio);
        bgmAudio = nullptr;
        return;
    }

    MIX_SetTrackAudio(bgmTrack, bgmAudio);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    MIX_PlayTrack(bgmTrack, props);
    SDL_DestroyProperties(props);
}

void AudioManager::StopBGM()
{
    if (!initialized)
        return;
    destroyBgmHandles();
    bgmPath.clear();
}

void AudioManager::PlaySFX(const std::string &filename)
{
    if (!initialized)
        return;

    auto it = sfxCache.find(filename);
    if (it == sfxCache.end()) {
        std::string path = "assets/sounds/" + filename;
        MIX_Audio *audio = MIX_LoadAudio(mixer, path.c_str(), true);
        if (!audio) {
            std::cerr << "[CEREKA] Failed to load SFX: " << path << " — " << SDL_GetError()
                      << "\n";
            return;
        }
        sfxCache[filename] = audio;
        it = sfxCache.find(filename);
    }

    MIX_PlayAudio(mixer, it->second);  // fire and forget
}

}  // namespace cereka
