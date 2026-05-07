#pragma once
// ui_manager.hpp — Single orchestrator for all per-frame rendering
//
// UIManager is the single source of truth for all visuals. It depends only
// on IRenderContext — no SDL types cross the public surface.
// All draw methods take engine state as parameters, keeping UIManager
// a stateless rendering orchestrator.

#include "renderer/irender_context.hpp"
#include "scene_graph.hpp"
#include <string>

struct TTF_Font;
struct UiConfig;

namespace cereka {

class SceneManager;
class DialogueSystem;
class MenuSystem;

class UIManager {
public:
    UIManager() = default;

    // Initialization — must be called before any draw methods
    void Init(IRenderContext &renderCtx);

    // Set current font (called when font/theme changes)
    void SetFont(TTF_Font *font) { m_font = font; }

    // Per-frame draw methods — called by state draw() or Impl::Draw()
    void DrawBackground(const SceneManager &scene);
    void DrawCharacters(const SceneManager &scene);
    void DrawDialogueBox(const DialogueSystem &dialogue,
                         const UiConfig &uiCfg);
    void DrawMenuButtons(const MenuSystem &menu,
                         const UiConfig &uiCfg);
    void DrawFadeOverlay(const SceneManager &scene);
    void DrawSaveLoadOverlay(bool isSaving,
                             const std::string (&timestamps)[10],
                             const UiConfig &uiCfg);

    // History overlay
    void DrawHistoryOverlay(const std::vector<std::string> &historyTexts);

    // Scene graph access and drawing
    SceneGraph &GetSceneGraph() { return m_sceneGraph; }
    void DrawSceneGraph();

    // Hit testing for save slots (layout math shared with draw)
    int HitTestSaveSlot(int mx, int my, int screenW, int screenH);

private:
    IRenderContext *m_renderCtx = nullptr;
    TTF_Font *m_font = nullptr;
    SceneGraph m_sceneGraph;

    struct SaveOverlayLayout {
        float panelX, panelY, panelW, panelH, slotH;
    };
    SaveOverlayLayout calcSaveLayout(int screenW, int screenH) const;

    void drawRect(float x, float y, float w, float h, Color color, bool blend = false);
    void drawTexturedRect(ITexture *tex, float x, float y, float w, float h);
};

}  // namespace cereka
