// audio.cpp — BGM and SFX playback

#include "engine_impl.hpp"

void Impl::PlayBGM(const std::string &filename)
{
    if (!audioInitialized) return;

    if (bgmTrack) { MIX_StopTrack(bgmTrack, 0); MIX_DestroyTrack(bgmTrack); bgmTrack = nullptr; }
    if (bgmAudio) { MIX_DestroyAudio(bgmAudio); bgmAudio = nullptr; }

    std::string path = "assets/sounds/" + filename;
    bgmAudio = MIX_LoadAudio(mixer, path.c_str(), false); // stream, don't pre-decode
    if (!bgmAudio) {
        std::cerr << "[CEREKA] Failed to load BGM: " << path << " — " << SDL_GetError() << "\n";
        return;
    }

    bgmTrack = MIX_CreateTrack(mixer);
    if (!bgmTrack) {
        std::cerr << "[CEREKA] Failed to create BGM track: " << SDL_GetError() << "\n";
        MIX_DestroyAudio(bgmAudio); bgmAudio = nullptr;
        return;
    }

    MIX_SetTrackAudio(bgmTrack, bgmAudio);
    MIX_SetTrackLoops(bgmTrack, -1); // loop forever
    MIX_PlayTrack(bgmTrack, 0);
}

void Impl::StopBGM()
{
    if (!audioInitialized) return;
    if (bgmTrack) { MIX_StopTrack(bgmTrack, 0); MIX_DestroyTrack(bgmTrack); bgmTrack = nullptr; }
    if (bgmAudio) { MIX_DestroyAudio(bgmAudio); bgmAudio = nullptr; }
}

void Impl::PlaySFX(const std::string &filename)
{
    if (!audioInitialized) return;

    auto it = sfxCache.find(filename);
    if (it == sfxCache.end()) {
        std::string path  = "assets/sounds/" + filename;
        MIX_Audio  *audio = MIX_LoadAudio(mixer, path.c_str(), true); // pre-decode SFX
        if (!audio) {
            std::cerr << "[CEREKA] Failed to load SFX: " << path << " — " << SDL_GetError() << "\n";
            return;
        }
        sfxCache[filename] = audio;
        it = sfxCache.find(filename);
    }

    MIX_PlayAudio(mixer, it->second); // fire and forget
}
