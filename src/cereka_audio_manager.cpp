#include "cereka_audio_manager.hpp"
#include "cereka_safe_parse.hpp"

#include <SDL3/SDL.h>
#include <cmath>
#include <iostream>

namespace cereka {

namespace {
float applyCurve(float t, FadeCurve curve)
{
    switch (curve) {
        case FadeCurve::Linear:    return t;
        case FadeCurve::EaseIn:    return t * t;
        case FadeCurve::EaseOut:   return 1.0f - (1.0f - t) * (1.0f - t);
        case FadeCurve::EaseInOut:
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }
    return t;
}
}

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
    if (fadeState_.oldTrack) {
        MIX_StopTrack(fadeState_.oldTrack, 0);
        MIX_DestroyTrack(fadeState_.oldTrack);
        fadeState_.oldTrack = nullptr;
    }
    if (fadeState_.oldAudio) {
        MIX_DestroyAudio(fadeState_.oldAudio);
        fadeState_.oldAudio = nullptr;
    }
    fadeState_ = {};
}

void AudioManager::PlayBGM(const std::string &filename)
{
    PlayBGM(filename, 0.0f);
}

void AudioManager::PlayBGM(const std::string &filename, float fadeDuration)
{
    if (!initialized)
        return;

    destroyBgmHandles();
    bgmPath = filename;

    std::string path = "assets/sounds/" + filename;
    bgmAudio = MIX_LoadAudio(mixer, path.c_str(), true);
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

    if (fadeDuration > 0.0f) {
        MIX_SetTrackGain(bgmTrack, 0.0f);
        fadeState_.state = BgmFade::State::FadingIn;
        fadeState_.timer = 0.0f;
        fadeState_.duration = fadeDuration;
        fadeState_.curve = FadeCurve::Linear;
    }
}

void AudioManager::StopBGM()
{
    StopBGM(0.0f);
}

void AudioManager::StopBGM(float fadeDuration)
{
    if (!initialized || !bgmTrack)
        return;

    if (fadeState_.state == BgmFade::State::CrossfadeOut && fadeState_.oldTrack) {
        MIX_StopTrack(fadeState_.oldTrack, 0);
        MIX_DestroyTrack(fadeState_.oldTrack);
        MIX_DestroyAudio(fadeState_.oldAudio);
        fadeState_.oldTrack = nullptr;
        fadeState_.oldAudio = nullptr;
    }

    if (fadeDuration > 0.0f) {
        fadeState_.state = BgmFade::State::FadingOut;
        fadeState_.timer = 0.0f;
        fadeState_.duration = fadeDuration;
        fadeState_.curve = FadeCurve::Linear;
    } else {
        destroyBgmHandles();
        bgmPath.clear();
        fadeState_ = {};
    }
}

void AudioManager::CrossfadeBGM(const std::string &filename, float duration)
{
    if (!initialized)
        return;

    bgmPath = filename;
    fadeState_.oldTrack = bgmTrack;
    fadeState_.oldAudio = bgmAudio;
    fadeState_.oldPath = bgmPath;
    bgmTrack = nullptr;
    bgmAudio = nullptr;

    std::string path = "assets/sounds/" + filename;
    bgmAudio = MIX_LoadAudio(mixer, path.c_str(), true);
    if (!bgmAudio) {
        std::cerr << "[CEREKA] Failed to load BGM: " << path << " — " << SDL_GetError() << "\n";
        bgmTrack = fadeState_.oldTrack;
        bgmAudio = fadeState_.oldAudio;
        bgmPath = fadeState_.oldPath;
        fadeState_ = {};
        return;
    }

    bgmTrack = MIX_CreateTrack(mixer);
    if (!bgmTrack) {
        std::cerr << "[CEREKA] Failed to create BGM track: " << SDL_GetError() << "\n";
        MIX_DestroyAudio(bgmAudio);
        bgmAudio = nullptr;
        bgmTrack = fadeState_.oldTrack;
        bgmAudio = fadeState_.oldAudio;
        bgmPath = fadeState_.oldPath;
        fadeState_ = {};
        return;
    }

    MIX_SetTrackAudio(bgmTrack, bgmAudio);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    MIX_PlayTrack(bgmTrack, props);
    SDL_DestroyProperties(props);

    MIX_SetTrackGain(bgmTrack, 0.0f);
    fadeState_.state = BgmFade::State::CrossfadeOut;
    fadeState_.timer = 0.0f;
    fadeState_.duration = duration;
    fadeState_.curve = FadeCurve::Linear;
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

    MIX_PlayAudio(mixer, it->second);
}

void AudioManager::Update(float dt)
{
    if (fadeState_.state == BgmFade::State::None)
        return;

    fadeState_.timer += dt;
    float t = std::min(fadeState_.timer / fadeState_.duration, 1.0f);

    switch (fadeState_.state) {
        case BgmFade::State::FadingOut: {
            float gain = applyCurve(1.0f - t, fadeState_.curve);
            MIX_SetTrackGain(bgmTrack, gain);
            if (t >= 1.0f) {
                destroyBgmHandles();
                bgmPath.clear();
                fadeState_ = {};
            }
            break;
        }
        case BgmFade::State::FadingIn: {
            float gain = applyCurve(t, fadeState_.curve);
            MIX_SetTrackGain(bgmTrack, gain);
            if (t >= 1.0f) {
                MIX_SetTrackGain(bgmTrack, 1.0f);
                fadeState_ = {};
            }
            break;
        }
        case BgmFade::State::CrossfadeOut: {
            float gain = applyCurve(1.0f - t, fadeState_.curve);
            MIX_SetTrackGain(fadeState_.oldTrack, gain);
            float newGain = applyCurve(t, fadeState_.curve);
            MIX_SetTrackGain(bgmTrack, newGain);
            if (t >= 1.0f) {
                MIX_StopTrack(fadeState_.oldTrack, 0);
                MIX_DestroyTrack(fadeState_.oldTrack);
                MIX_DestroyAudio(fadeState_.oldAudio);
                fadeState_.oldTrack = nullptr;
                fadeState_.oldAudio = nullptr;
                MIX_SetTrackGain(bgmTrack, 1.0f);
                fadeState_ = {};
            }
            break;
        }
        default:
            break;
    }
}

}  // namespace cereka
