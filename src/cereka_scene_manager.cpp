#include "cereka_scene_manager.hpp"
#include <iostream>

namespace cereka {

void SceneManager::Init(IRenderContext &renderCtx)
{
    m_renderCtx = &renderCtx;
}

void SceneManager::Shutdown()
{
    Clear();
    m_renderCtx = nullptr;
}

float SceneManager::posToXNorm(const std::string &pos)
{
    if (pos == "left")
        return 0.2f;
    if (pos == "right")
        return 0.8f;
    return 0.5f;
}

std::shared_ptr<ITexture> SceneManager::loadBg(const std::string &filename)
{
    if (!m_renderCtx)
        return nullptr;
    auto tex = m_renderCtx->CreateTexture("assets/bg/" + filename);
    if (!tex)
        std::cerr << "[CEREKA] Failed to load bg: " << filename << '\n';
    return tex;
}

void SceneManager::ShowBackground(const std::string &filename)
{
    bgPath = filename;
    background = loadBg(filename);
}

void SceneManager::ShowCharacter(const std::string &id,
                                 const std::string &filename,
                                 const std::string &pos)
{
    HideCharacter(id);
    charPaths[id] = filename;
    if (!m_renderCtx)
        return;
    auto tex = m_renderCtx->CreateTexture("assets/characters/" + filename);
    if (!tex) {
        std::cerr << "[CEREKA] Failed to load character: " << filename << "\n";
        charPaths.erase(id);
        return;
    }
    characters[id] = {std::shared_ptr<ITexture>(std::move(tex)), posToXNorm(pos)};
}

void SceneManager::HideCharacter(const std::string &id)
{
    charPaths.erase(id);
    auto it = characters.find(id);
    if (it != characters.end()) {
        characters.erase(it);
    }
}

void SceneManager::StartFade(const std::string &filename,
                             float totalDuration)
{
    fadePhaseDuration = totalDuration * 0.5f;
    fadeTimer = 0.0f;
    fadePhase = FadePhase::Out;
    pendingBg = loadBg(filename);
}

bool SceneManager::TickFade(float dt)
{
    if (fadePhase == FadePhase::None)
        return false;

    fadeTimer += dt;
    if (fadePhase == FadePhase::Out && fadeTimer >= fadePhaseDuration) {
        background = std::move(pendingBg);
        pendingBg.reset();
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
    background.reset();
    pendingBg.reset();
    bgPath.clear();
    characters.clear();
    charPaths.clear();
    fadePhase = FadePhase::None;
    fadeTimer = 0.0f;
}

}  // namespace cereka
