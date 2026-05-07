#include "scene_manager.hpp"

#include <SDL3_image/SDL_image.h>
#include <iostream>

namespace cereka {

void SceneManager::Init(SDL_Renderer *r)
{
    renderer = r;
}

void SceneManager::Shutdown()
{
    Clear();
    renderer = nullptr;
}

float SceneManager::posToXNorm(const std::string &pos)
{
    if (pos == "left")
        return 0.2f;
    if (pos == "right")
        return 0.8f;
    return 0.5f;
}

SDL_Texture *SceneManager::loadBg(const std::string &filename)
{
    SDL_Texture *tex = IMG_LoadTexture(renderer, ("assets/bg/" + filename).c_str());
    if (!tex)
        std::cerr << "[CEREKA] Failed to load bg: " << filename << " — " << SDL_GetError() << '\n';
    return tex;
}

void SceneManager::ShowBackground(const std::string &filename)
{
    bgPath = filename;
    if (background)
        SDL_DestroyTexture(background);
    background = loadBg(filename);
}

void SceneManager::ShowCharacter(const std::string &id,
                                 const std::string &filename,
                                 const std::string &pos)
{
    HideCharacter(id);
    charPaths[id] = filename;
    std::string path = "assets/characters/" + filename;
    SDL_Texture *tex = IMG_LoadTexture(renderer, path.c_str());
    if (!tex) {
        std::cerr << "[CEREKA] Failed to load character: " << path << " — " << SDL_GetError()
                  << "\n";
        charPaths.erase(id);
        return;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    characters[id] = {tex, posToXNorm(pos)};
}

void SceneManager::HideCharacter(const std::string &id)
{
    charPaths.erase(id);
    auto it = characters.find(id);
    if (it != characters.end()) {
        SDL_DestroyTexture(it->second.tex);
        characters.erase(it);
    }
}

void SceneManager::StartFade(const std::string &filename,
                             float totalDuration)
{
    fadePhaseDuration = totalDuration * 0.5f;
    fadeTimer = 0.0f;
    fadePhase = FadePhase::Out;
    if (pendingBg)
        SDL_DestroyTexture(pendingBg);
    pendingBg = loadBg(filename);
}

bool SceneManager::TickFade(float dt)
{
    if (fadePhase == FadePhase::None)
        return false;

    fadeTimer += dt;
    if (fadePhase == FadePhase::Out && fadeTimer >= fadePhaseDuration) {
        if (background)
            SDL_DestroyTexture(background);
        background = pendingBg;
        pendingBg = nullptr;
        fadePhase = FadePhase::In;
        fadeTimer = 0.0f;
        return false;
    }
    if (fadePhase == FadePhase::In && fadeTimer >= fadePhaseDuration) {
        fadePhase = FadePhase::None;
        fadeTimer = 0.0f;
        return true;
    }
    return false;
}

void SceneManager::Clear()
{
    if (background) {
        SDL_DestroyTexture(background);
        background = nullptr;
    }
    if (pendingBg) {
        SDL_DestroyTexture(pendingBg);
        pendingBg = nullptr;
    }
    bgPath.clear();
    for (auto &[id, entry] : characters)
        SDL_DestroyTexture(entry.tex);
    characters.clear();
    charPaths.clear();
    fadePhase = FadePhase::None;
    fadeTimer = 0.0f;
}

}  // namespace cereka
