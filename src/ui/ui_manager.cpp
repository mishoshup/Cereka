// ui_manager.cpp — UIManager rendering implementation
//
// All per-frame drawing logic extracted from cereka_draw.cpp,
// cereka_save.cpp, and state draw methods. All rendering goes
// through IRenderContext — no SDL calls directly.

#include "ui_manager.hpp"
#include "cereka_scene_manager.hpp"
#include "cereka_dialogue_system.hpp"
#include "cereka_menu_system.hpp"
#include "cereka_ui_config.hpp"

#include <algorithm>

namespace cereka {

// ============================================================================
// Lifecycle
// ============================================================================

void UIManager::Init(IRenderContext &renderCtx)
{
    m_renderCtx = &renderCtx;
}

// ============================================================================
// Private helpers
// ============================================================================

void UIManager::drawRect(float x, float y, float w, float h, Color color, bool blend)
{
    if (blend)
        m_renderCtx->SetBlendMode(true);
    m_renderCtx->FillRect(Rect{x, y, w, h}, color);
}

void UIManager::drawTexturedRect(ITexture *tex, float x, float y, float w, float h)
{
    if (!tex)
        return;
    Rect dst{x, y, w, h};
    m_renderCtx->DrawTexture(*tex, nullptr, &dst);
}

// ============================================================================
// UIManager::DrawSceneGraph
// ============================================================================

void UIManager::DrawSceneGraph()
{
    m_sceneGraph.updateTransforms();
    m_renderCtx->SetBlendMode(true);
    m_sceneGraph.visit([this](const SceneNode &node) {
        if (!node.texture)
            return;
        int sw = m_renderCtx->Width();
        int sh = m_renderCtx->Height();
        float sx = node.world.x * sw;
        float sy = node.world.y * sh;
        float w = node.texture->Width() * node.world.scaleX;
        float h = node.texture->Height() * node.world.scaleY;
        Rect dst{sx - w / 2.0f, sy - h / 2.0f, w, h};
        m_renderCtx->DrawTexture(*node.texture, nullptr, &dst);
    });
}

// ============================================================================
// UIManager::DrawBackground
// ============================================================================

void UIManager::DrawBackground(const SceneManager &scene)
{
    if (auto *bg = scene.Background())
        m_renderCtx->DrawTexture(*bg, nullptr, nullptr);
}

// ============================================================================
// UIManager::DrawCharacters
// ============================================================================

void UIManager::DrawCharacters(const SceneManager &scene)
{
    int screenH = m_renderCtx->Height();
    int screenW = m_renderCtx->Width();

    for (const auto &[id, entry] : scene.Characters()) {
        if (!entry.tex)
            continue;
        float scale = (screenH * 0.8f) / entry.tex->Height();
        float centreX = screenW * entry.xNorm;
        Rect dst{centreX - entry.tex->Width() * scale * 0.5f,
                 screenH - entry.tex->Height() * scale - screenH * 0.1f,
                 entry.tex->Width() * scale,
                 entry.tex->Height() * scale};
        m_renderCtx->DrawTexture(*entry.tex, nullptr, &dst);
    }
}

// ============================================================================
// UIManager::DrawDialogueBox
// ============================================================================

void UIManager::DrawDialogueBox(const DialogueSystem &dialogue,
                                const UiConfig &uiCfg)
{
    if (dialogue.Text().empty())
        return;

    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();
    float tbY = uiCfg.textbox.y.resolve((float)screenH);
    float tbH = uiCfg.textbox.h.resolve((float)screenH);

    // --- Textbox background ---
    if (uiCfg.textbox.image) {
        drawTexturedRect(uiCfg.textbox.image, 0, tbY, (float)screenW, tbH);
    } else {
        drawRect(0, tbY, (float)screenW, tbH, uiCfg.textbox.color, true);
    }

    // --- Name box ---
    if (!dialogue.Speaker().empty()) {
        float nbY = tbY + uiCfg.namebox.yOffset;

        if (uiCfg.namebox.image) {
            drawTexturedRect(uiCfg.namebox.image,
                             uiCfg.namebox.x, nbY,
                             uiCfg.namebox.w, uiCfg.namebox.h);
        } else {
            drawRect(uiCfg.namebox.x, nbY,
                     uiCfg.namebox.w, uiCfg.namebox.h,
                     uiCfg.namebox.color, true);
        }

        auto nameTex = m_renderCtx->CreateTextTexture(
            m_font, dialogue.Name(), uiCfg.namebox.textColor);
        if (nameTex) {
            float nw = nameTex->Width();
            float nh = nameTex->Height();
            Rect dst{uiCfg.namebox.x + 15.0f,
                     nbY + (uiCfg.namebox.h - nh) / 2.0f,
                     nw, nh};
            m_renderCtx->DrawTexture(*nameTex, nullptr, &dst);
        }
    }

    // --- Dialogue text with word wrap ---
    std::string visible = dialogue.Text().substr(0, dialogue.DisplayedChars());

    float effectiveWrapW = uiCfg.textbox.wrapWidth.resolve((float)screenW);
    if (effectiveWrapW <= 0.0f)
        effectiveWrapW = (float)screenW * 0.9f;

    float textAreaW = effectiveWrapW - 2.0f * uiCfg.textbox.textMarginX;
    int wrapPx = static_cast<int>(textAreaW);

    auto textTex = m_renderCtx->CreateTextTextureWrapped(
        m_font, visible, uiCfg.textbox.textColor, wrapPx);
    if (textTex) {
        float tw = textTex->Width();
        float th = textTex->Height();
        float margin = uiCfg.textbox.textMarginX;
        float lineH = th + uiCfg.textbox.lineSpacing;
        Rect dst{margin, tbY + 40.0f, tw, lineH};
        m_renderCtx->DrawTexture(*textTex, nullptr, &dst);
    }
}

// ============================================================================
// UIManager::DrawMenuButtons
// ============================================================================

void UIManager::DrawMenuButtons(const MenuSystem &menu,
                                const UiConfig &uiCfg)
{
    if (!menu.IsOpen())
        return;

    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();

    const float bw = uiCfg.button.w;
    const float bh = uiCfg.button.h;
    const float spacing = 20.0f;
    float y = screenH * 0.4f;
    const auto &buttonTexts = menu.Texts();

    for (size_t i = 0; i < buttonTexts.size(); ++i) {
        Rect btn{(float)screenW / 2.0f - bw / 2.0f, y, bw, bh};

        if (uiCfg.button.image) {
            m_renderCtx->DrawTexture(*uiCfg.button.image, nullptr, &btn);
        } else {
            m_renderCtx->SetBlendMode(true);
            m_renderCtx->FillRect(btn, uiCfg.button.color);
        }

        auto textTex = m_renderCtx->CreateTextTexture(
            m_font, buttonTexts[i], uiCfg.button.textColor);
        if (textTex) {
            float tw2 = textTex->Width();
            float th2 = textTex->Height();
            Rect tr{(float)screenW / 2.0f - tw2 / 2.0f,
                    y + bh / 2.0f - th2 / 2.0f,
                    tw2, th2};
            m_renderCtx->DrawTexture(*textTex, nullptr, &tr);
        }
        y += bh + spacing;
    }
}

// ============================================================================
// UIManager::DrawFadeOverlay
// ============================================================================

void UIManager::DrawFadeOverlay(const SceneManager &scene)
{
    if (scene.Phase() == SceneManager::FadePhase::None)
        return;

    float t = std::min(scene.FadeTimer() / scene.FadePhaseDuration(), 1.0f);
    float alpha = (scene.Phase() == SceneManager::FadePhase::Out)
                      ? t
                      : (1.0f - t);
    m_renderCtx->SetBlendMode(true);
    m_renderCtx->FillScreen(Color{0, 0, 0, (uint8_t)(alpha * 255.0f)});
}

// ============================================================================
// UIManager::DrawSaveLoadOverlay
// ============================================================================

void UIManager::DrawSaveLoadOverlay(bool isSaving,
                                    const std::string (&timestamps)[10],
                                    const UiConfig &uiCfg)
{
    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();

    auto [panelX, panelY, panelW, panelH, slotH] = calcSaveLayout(screenW, screenH);

    // Dim background
    m_renderCtx->SetBlendMode(true);
    m_renderCtx->FillScreen(Color{0, 0, 0, 180});

    // Panel background
    drawRect(panelX, panelY, panelW, panelH, Color{20, 22, 38, 230});

    // Title
    auto titleTex = m_renderCtx->CreateTextTexture(
        m_font, isSaving ? "SAVE GAME" : "LOAD GAME", Color{180, 200, 255, 255});
    if (titleTex) {
        float tw = titleTex->Width();
        float th = titleTex->Height();
        Rect dst{panelX + (panelW - tw) * 0.5f, panelY + 8.0f, tw, th};
        m_renderCtx->DrawTexture(*titleTex, nullptr, &dst);
    }

    // Slot rows
    for (int i = 0; i < 10; ++i) {
        float slotY = panelY + 50.0f + i * slotH;
        Rect slotRect{panelX + 10.0f, slotY + 2.0f, panelW - 20.0f, slotH - 4.0f};

        drawRect(slotRect.x, slotRect.y, slotRect.w, slotRect.h, Color{40, 44, 66, 210});

        const std::string &ts = timestamps[i];
        std::string label = "Slot " + std::to_string(i + 1) + "   "
                          + (ts.empty() ? "Empty" : ts);

        auto slotTex = m_renderCtx->CreateTextTexture(
            m_font, label,
            ts.empty() ? Color{100, 100, 100, 255} : Color{220, 220, 220, 255});
        if (slotTex) {
            float tw = slotTex->Width();
            float th = slotTex->Height();
            Rect dst{slotRect.x + 10.0f, slotY + (slotH - th) * 0.5f, tw, th};
            m_renderCtx->DrawTexture(*slotTex, nullptr, &dst);
        }
    }

    // ESC hint
    auto hintTex = m_renderCtx->CreateTextTexture(
        m_font, "ESC to cancel", Color{120, 120, 120, 255});
    if (hintTex) {
        float tw = hintTex->Width();
        float th = hintTex->Height();
        Rect dst{panelX + (panelW - tw) * 0.5f, panelY + panelH - th - 8.0f, tw, th};
        m_renderCtx->DrawTexture(*hintTex, nullptr, &dst);
    }
}

// ============================================================================
// UIManager::HitTestSaveSlot
// ============================================================================

int UIManager::HitTestSaveSlot(int mx, int my, int screenW, int screenH)
{
    auto [panelX, panelY, panelW, panelH, slotH] = calcSaveLayout(screenW, screenH);

    for (int i = 1; i <= 10; ++i) {
        float slotY = panelY + 50.0f + (i - 1) * slotH;
        if ((float)mx >= panelX + 10.0f && (float)mx <= panelX + panelW - 10.0f &&
            (float)my >= slotY + 2.0f && (float)my <= slotY + slotH - 2.0f)
            return i;
    }
    return -1;
}

// ============================================================================
// UIManager::calcSaveLayout
// ============================================================================

UIManager::SaveOverlayLayout UIManager::calcSaveLayout(int screenW, int screenH) const
{
    SaveOverlayLayout l{};
    l.panelW = screenW * 0.5f;
    l.panelH = screenH * 0.8f;
    l.panelX = (screenW - l.panelW) * 0.5f;
    l.panelY = (screenH - l.panelH) * 0.5f;
    l.slotH = (l.panelH - 60.0f) / 10.0f;
    return l;
}

}  // namespace cereka
