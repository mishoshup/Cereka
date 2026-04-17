// audio.cpp — BGM and SFX playback

#include "engine_impl.hpp"

void Impl::PlayBGM(const std::string &filename)
{
    if (!audioInitialized)
        return;

    bgmPath = filename;
    if (bgmTrack) {
        MIX_StopTrack(bgmTrack, 0);
        MIX_DestroyTrack(bgmTrack);
        bgmTrack = nullptr;
    }
    if (bgmAudio) {
        MIX_DestroyAudio(bgmAudio);
        bgmAudio = nullptr;
    }

    std::string path = "assets/sounds/" + filename;
    bgmAudio = MIX_LoadAudio(mixer, path.c_str(), true);  // pre-decode for looping support
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

void Impl::StopBGM()
{
    if (!audioInitialized)
        return;
    bgmPath.clear();
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

void Impl::PlaySFX(const std::string &filename)
{
    if (!audioInitialized)
        return;

    auto it = sfxCache.find(filename);
    if (it == sfxCache.end()) {
        std::string path = "assets/sounds/" + filename;
        MIX_Audio *audio = MIX_LoadAudio(mixer, path.c_str(), true);  // pre-decode SFX
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
