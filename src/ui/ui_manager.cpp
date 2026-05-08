// ui_manager.cpp — UIManager rendering implementation
//
// All per-frame drawing logic extracted from cereka_draw.cpp,
// cereka_save.cpp, and state draw methods. All rendering goes
// through IRenderContext — no SDL calls directly.

#include "ui_manager.hpp"
#include "text/markup_parser.hpp"
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

    // --- Dialogue text with markup ---
    std::string visible = dialogue.Text().substr(0, dialogue.DisplayedChars());

    float effectiveWrapW = uiCfg.textbox.wrapWidth.resolve((float)screenW);
    if (effectiveWrapW <= 0.0f)
        effectiveWrapW = (float)screenW * 0.9f;

    float textAreaW = effectiveWrapW - 2.0f * uiCfg.textbox.textMarginX;
    float margin = uiCfg.textbox.textMarginX;

    auto segments = cereka::text::ParseMarkup(visible);
    if (!segments.empty()) {
        m_renderCtx->DrawRichText(m_font, segments, margin, tbY + 40.0f, textAreaW);
    }
}

// ============================================================================
// UIManager::DrawMenuButtons
// ============================================================================

static Color brightenColor(Color c, int amount)
{
    Color result;
    result.r = (uint8_t)std::min(255, (int)c.r + amount);
    result.g = (uint8_t)std::min(255, (int)c.g + amount);
    result.b = (uint8_t)std::min(255, (int)c.b + amount);
    result.a = c.a;
    return result;
}

void UIManager::DrawMenuButtons(const MenuSystem &menu,
                                const UiConfig &uiCfg)
{
    if (!menu.IsOpen())
        return;

    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();

    const float bw = uiCfg.button.w;
    const float bh = uiCfg.button.h;
    const float spacing = uiCfg.button.spacing;
    const Dim &buttonY = uiCfg.button.y;
    int hovered = menu.HoveredIndex();

    // Pagination: compute visible range
    int totalButtons = (int)menu.ButtonCount();
    int bpp = MenuSystem::ButtonsPerPage((float)screenH, buttonY, bh, spacing);
    int page = menu.CurrentPage();
    int totalPages = (totalButtons + bpp - 1) / bpp;
    // Sync total pages back to menu (needed by MenuState for navigation bounds)
    // We don't have a non-const ref, so handle at state level

    int startIdx = page * bpp;
    int endIdx = std::min(startIdx + bpp, totalButtons);

    const auto &buttonTexts = menu.Texts();
    float baseX = (float)screenW / 2.0f - bw / 2.0f;
    float y0 = buttonY.resolve((float)screenH);

    // Draw page-up indicator if not on first page
    if (page > 0) {
        float indY = y0 - bh * 0.5f;
        Rect ind{(float)screenW / 2.0f - 40.0f, indY, 80.0f, bh * 0.5f};
        m_renderCtx->SetBlendMode(true);
        m_renderCtx->FillRect(ind, Color{80, 80, 120, 200});
        auto upTex = m_renderCtx->CreateTextTexture(
            m_font, "\xe2\x96\xb2 Back", Color{180, 200, 255, 255});
        if (upTex) {
            float tw = upTex->Width();
            float th = upTex->Height();
            Rect tr{baseX, indY + (bh * 0.5f - th) / 2.0f, tw, th};
            m_renderCtx->DrawTexture(*upTex, nullptr, &tr);
        }
    }

    // Draw visible buttons
    for (int i = startIdx; i < endIdx; ++i) {
        int localIdx = i - startIdx;
        float y = y0 + (float)localIdx * (bh + spacing);
        Rect btn{baseX, y, bw, bh};

        bool isHovered = (i == hovered);
        bool isSelected = (i == menu.SelectedIndex());

        if (isHovered && uiCfg.button.hoverImage) {
            // Hover image takes priority
            m_renderCtx->DrawTexture(*uiCfg.button.hoverImage, nullptr, &btn);
        } else if (uiCfg.button.image) {
            // Normal image
            m_renderCtx->DrawTexture(*uiCfg.button.image, nullptr, &btn);
        } else {
            m_renderCtx->SetBlendMode(true);
            Color fillColor = isHovered ? brightenColor(uiCfg.button.color, 50)
                                        : uiCfg.button.color;
            m_renderCtx->FillRect(btn, fillColor);

            // Selected but not hovered: draw a subtle border
            if (isSelected && !isHovered) {
                // Top edge
                m_renderCtx->FillRect(Rect{btn.x, btn.y, btn.w, 2.0f},
                                       Color{255, 255, 255, 120});
                // Bottom edge
                m_renderCtx->FillRect(Rect{btn.x, btn.y + btn.h - 2.0f, btn.w, 2.0f},
                                       Color{255, 255, 255, 120});
            }
        }

        Color textCol = isHovered ? brightenColor(uiCfg.button.textColor, 60)
                                  : uiCfg.button.textColor;
        auto textTex = m_renderCtx->CreateTextTexture(m_font, buttonTexts[i], textCol);
        if (textTex) {
            float tw2 = textTex->Width();
            float th2 = textTex->Height();
            Rect tr{baseX + (bw - tw2) / 2.0f,
                    y + bh / 2.0f - th2 / 2.0f,
                    tw2, th2};
            m_renderCtx->DrawTexture(*textTex, nullptr, &tr);
        }

        // Hovered button: draw border indicator
        if (isHovered) {
            Color borderCol = brightenColor(uiCfg.button.color, 100);
            m_renderCtx->FillRect(Rect{btn.x, btn.y, btn.w, 3.0f}, borderCol);
            m_renderCtx->FillRect(Rect{btn.x, btn.y + btn.h - 3.0f, btn.w, 3.0f}, borderCol);
        }
    }

    // Draw page-down indicator if more pages exist
    if (page < totalPages - 1) {
        int lastLocal = std::min(bpp, endIdx - startIdx) - 1;
        float indY = y0 + (float)lastLocal * (bh + spacing) + bh;
        Rect ind{(float)screenW / 2.0f - 40.0f, indY, 80.0f, bh * 0.5f};
        m_renderCtx->SetBlendMode(true);
        m_renderCtx->FillRect(ind, Color{80, 80, 120, 200});
        auto downTex = m_renderCtx->CreateTextTexture(
            m_font, "\xe2\x96\xbc More", Color{180, 200, 255, 255});
        if (downTex) {
            float tw = downTex->Width();
            float th = downTex->Height();
            Rect tr{baseX, indY + (bh * 0.5f - th) / 2.0f, tw, th};
            m_renderCtx->DrawTexture(*downTex, nullptr, &tr);
        }
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

void UIManager::DrawHistoryOverlay(const std::vector<std::string> &historyTexts)
{
    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();

    m_renderCtx->SetBlendMode(true);
    m_renderCtx->FillScreen(Color{0, 0, 0, 160});

    float panelX = screenW * 0.1f;
    float panelY = screenH * 0.05f;
    float panelW = screenW * 0.8f;
    float panelH = screenH * 0.9f;

    drawRect(panelX, panelY, panelW, panelH, Color{20, 22, 38, 230});

    auto titleTex = m_renderCtx->CreateTextTexture(
        m_font, "DIALOGUE HISTORY", Color{180, 200, 255, 255});
    if (titleTex) {
        float tw = titleTex->Width();
        float th = titleTex->Height();
        Rect dst{panelX + (panelW - tw) * 0.5f, panelY + 8.0f, tw, th};
        m_renderCtx->DrawTexture(*titleTex, nullptr, &dst);
    }

    float entryY = panelY + 50.0f;
    float lineH = 40.0f;
    float margin = 10.0f;

    for (size_t i = 0; i < historyTexts.size() && entryY + lineH < panelY + panelH - 10.0f; ++i) {
        drawRect(panelX + margin, entryY, panelW - 2.0f * margin, lineH - 2.0f,
                 Color{40, 44, 66, 210});

        std::string display = historyTexts[i];
        if (display.length() > 80)
            display = display.substr(0, 77) + "...";
        if (display.empty())
            display = "(continue)";

        auto entryTex = m_renderCtx->CreateTextTexture(
            m_font, display, Color{200, 200, 200, 255});
        if (entryTex) {
            float tw = entryTex->Width();
            float th = entryTex->Height();
            Rect dst{panelX + margin + 8.0f, entryY + (lineH - th) * 0.5f, tw, th};
            m_renderCtx->DrawTexture(*entryTex, nullptr, &dst);
        }
        entryY += lineH;
    }

    auto hintTex = m_renderCtx->CreateTextTexture(
        m_font, "ESC to return | Click entry to rollback", Color{120, 120, 120, 255});
    if (hintTex) {
        float tw = hintTex->Width();
        float th = hintTex->Height();
        Rect dst{panelX + (panelW - tw) * 0.5f, panelY + panelH - th - 8.0f, tw, th};
        m_renderCtx->DrawTexture(*hintTex, nullptr, &dst);
    }
}

void UIManager::DrawSaveLoadOverlay(bool isSaving,
                                    const SlotMetadata (&slots)[10],
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

        const SlotMetadata &meta = slots[i];
        bool hasData = !meta.timestamp.empty();

        // Slot number + timestamp (first line)
        std::string line1 = "Slot " + std::to_string(i + 1)
                          + (hasData ? "   " + meta.timestamp : "   Empty");
        auto slotTex = m_renderCtx->CreateTextTexture(
            m_font, line1,
            hasData ? Color{220, 220, 220, 255} : Color{100, 100, 100, 255});
        if (slotTex) {
            float tw = slotTex->Width();
            float th = slotTex->Height();
            Rect dst{slotRect.x + 10.0f, slotY + 4.0f, tw, th};
            m_renderCtx->DrawTexture(*slotTex, nullptr, &dst);
        }

        // Scene description (second line, smaller visual weight)
        if (hasData && !meta.sceneDescription.empty()) {
            auto sceneTex = m_renderCtx->CreateTextTexture(
                m_font, meta.sceneDescription, Color{160, 160, 180, 255});
            if (sceneTex) {
                float tw = sceneTex->Width();
                float th = sceneTex->Height();
                Rect dst{slotRect.x + 10.0f, slotY + slotH - th - 4.0f, tw, th};
                m_renderCtx->DrawTexture(*sceneTex, nullptr, &dst);
            }
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

// ============================================================================
// UIManager::DrawPauseOverlay
// ============================================================================

static const char *PAUSE_BUTTON_LABELS[] = {
    "Continue", "Save", "Load", "Settings", "Quit to Menu"
};

UIManager::PauseButtonLayout UIManager::calcPauseLayout(int screenW, int screenH) const
{
    PauseButtonLayout l{};
    l.buttonCount = 5;
    l.panelW = std::min(500.0f, screenW * 0.6f);
    l.btnH = 60.0f;
    l.btnSpacing = 12.0f;
    float contentH = l.buttonCount * l.btnH + (l.buttonCount - 1) * l.btnSpacing + 80.0f;
    l.panelH = contentH;
    l.panelX = (screenW - l.panelW) * 0.5f;
    l.panelY = (screenH - l.panelH) * 0.5f;
    l.btnY0 = l.panelY + 45.0f;
    return l;
}

void UIManager::DrawPauseOverlay(const UiConfig &uiCfg)
{
    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();

    auto layout = calcPauseLayout(screenW, screenH);

    // Dim background
    m_renderCtx->SetBlendMode(true);
    m_renderCtx->FillScreen(Color{0, 0, 0, 160});

    // Panel background
    drawRect(layout.panelX, layout.panelY, layout.panelW, layout.panelH,
             Color{20, 22, 38, 230});

    // Title
    auto titleTex = m_renderCtx->CreateTextTexture(
        m_font, "PAUSED", Color{180, 200, 255, 255});
    if (titleTex) {
        float tw = titleTex->Width();
        float th = titleTex->Height();
        Rect dst{layout.panelX + (layout.panelW - tw) * 0.5f,
                 layout.panelY + 8.0f, tw, th};
        m_renderCtx->DrawTexture(*titleTex, nullptr, &dst);
    }

    // Buttons
    float btnW = layout.panelW * 0.75f;
    for (int i = 0; i < layout.buttonCount; ++i) {
        float by = layout.btnY0 + i * (layout.btnH + layout.btnSpacing);
        float bx = layout.panelX + (layout.panelW - btnW) * 0.5f;

        drawRect(bx, by, btnW, layout.btnH, Color{40, 44, 66, 210});

        auto btnTex = m_renderCtx->CreateTextTexture(
            m_font, PAUSE_BUTTON_LABELS[i], Color{220, 220, 220, 255});
        if (btnTex) {
            float tw = btnTex->Width();
            float th = btnTex->Height();
            Rect dst{bx + (btnW - tw) * 0.5f, by + (layout.btnH - th) * 0.5f, tw, th};
            m_renderCtx->DrawTexture(*btnTex, nullptr, &dst);
        }
    }

    // ESC hint
    auto hintTex = m_renderCtx->CreateTextTexture(
        m_font, "ESC to resume", Color{120, 120, 120, 255});
    if (hintTex) {
        float tw = hintTex->Width();
        float th = hintTex->Height();
        Rect dst{layout.panelX + (layout.panelW - tw) * 0.5f,
                 layout.panelY + layout.panelH - th - 8.0f, tw, th};
        m_renderCtx->DrawTexture(*hintTex, nullptr, &dst);
    }
}

int UIManager::HitTestPauseButton(int mx, int my, int screenW, int screenH) const
{
    auto layout = calcPauseLayout(screenW, screenH);
    float btnW = layout.panelW * 0.75f;

    for (int i = 0; i < layout.buttonCount; ++i) {
        float by = layout.btnY0 + i * (layout.btnH + layout.btnSpacing);
        float bx = layout.panelX + (layout.panelW - btnW) * 0.5f;
        if ((float)mx >= bx && (float)mx <= bx + btnW &&
            (float)my >= by && (float)my <= by + layout.btnH)
            return i;
    }
    return -1;
}

// ============================================================================
// UIManager::DrawConfirmOverwriteDialog
// ============================================================================

UIManager::ConfirmLayout UIManager::calcConfirmLayout(int screenW, int screenH) const
{
    ConfirmLayout l{};
    l.panelW = 420.0f;
    l.panelH = 200.0f;
    l.panelX = (screenW - l.panelW) * 0.5f;
    l.panelY = (screenH - l.panelH) * 0.5f;
    l.yesW = 140.0f;
    l.yesH = 50.0f;
    l.noW = 140.0f;
    l.noH = 50.0f;
    float gap = 30.0f;
    float totalBtnW = l.yesW + gap + l.noW;
    float baseX = l.panelX + (l.panelW - totalBtnW) * 0.5f;
    l.yesX = baseX;
    l.yesY = l.panelY + 110.0f;
    l.noX = baseX + l.yesW + gap;
    l.noY = l.panelY + 110.0f;
    return l;
}

void UIManager::DrawConfirmOverwriteDialog(int slot, const UiConfig &uiCfg)
{
    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();

    m_renderCtx->SetBlendMode(true);
    m_renderCtx->FillScreen(Color{0, 0, 0, 180});

    auto l = calcConfirmLayout(screenW, screenH);
    drawRect(l.panelX, l.panelY, l.panelW, l.panelH, Color{30, 32, 48, 240});

    // Prompt text
    std::string prompt = "Overwrite Slot " + std::to_string(slot) + "?";
    auto promptTex = m_renderCtx->CreateTextTexture(
        m_font, prompt, Color{255, 255, 255, 255});
    if (promptTex) {
        float tw = promptTex->Width();
        float th = promptTex->Height();
        Rect dst{l.panelX + (l.panelW - tw) * 0.5f, l.panelY + 30.0f, tw, th};
        m_renderCtx->DrawTexture(*promptTex, nullptr, &dst);
    }

    std::string sub = "This save has existing data.";
    auto subTex = m_renderCtx->CreateTextTexture(
        m_font, sub, Color{200, 200, 200, 255});
    if (subTex) {
        float tw = subTex->Width();
        float th = subTex->Height();
        Rect dst{l.panelX + (l.panelW - tw) * 0.5f, l.panelY + 65.0f, tw, th};
        m_renderCtx->DrawTexture(*subTex, nullptr, &dst);
    }

    // Yes button
    drawRect(l.yesX, l.yesY, l.yesW, l.yesH, Color{50, 120, 50, 230});
    auto yesTex = m_renderCtx->CreateTextTexture(
        m_font, "Yes, Overwrite", Color{255, 255, 255, 255});
    if (yesTex) {
        float tw = yesTex->Width();
        float th = yesTex->Height();
        Rect dst{l.yesX + (l.yesW - tw) * 0.5f, l.yesY + (l.yesH - th) * 0.5f, tw, th};
        m_renderCtx->DrawTexture(*yesTex, nullptr, &dst);
    }

    // No button
    drawRect(l.noX, l.noY, l.noW, l.noH, Color{100, 50, 50, 230});
    auto noTex = m_renderCtx->CreateTextTexture(
        m_font, "Cancel", Color{255, 255, 255, 255});
    if (noTex) {
        float tw = noTex->Width();
        float th = noTex->Height();
        Rect dst{l.noX + (l.noW - tw) * 0.5f, l.noY + (l.noH - th) * 0.5f, tw, th};
        m_renderCtx->DrawTexture(*noTex, nullptr, &dst);
    }
}

int UIManager::HitTestConfirmButton(int mx, int my, int screenW, int screenH) const
{
    auto l = calcConfirmLayout(screenW, screenH);
    // Yes
    if ((float)mx >= l.yesX && (float)mx <= l.yesX + l.yesW &&
        (float)my >= l.yesY && (float)my <= l.yesY + l.yesH)
        return 1;
    // No
    if ((float)mx >= l.noX && (float)mx <= l.noX + l.noW &&
        (float)my >= l.noY && (float)my <= l.noY + l.noH)
        return 2;
    return 0;
}

}  // namespace cereka
