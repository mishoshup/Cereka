// draw.cpp — every frame rendering

#include "engine_impl.hpp"
#include <algorithm>

void Impl::Draw()
{
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderClear(renderer);

    // --- Background ---
    if (scene.Background())
        SDL_RenderTexture(renderer, scene.Background(), nullptr, nullptr);

    // --- Characters ---
    for (const auto &[id, entry] : scene.Characters()) {
        float tw = 0, th = 0;
        SDL_GetTextureSize(entry.tex, &tw, &th);
        float scale = (screenHeight * 0.8f) / th;
        float centreX = screenWidth * entry.xNorm;
        SDL_FRect dst{centreX - tw * scale * 0.5f,
                      screenHeight - th * scale - screenHeight * 0.1f,
                      tw * scale,
                      th * scale};
        SDL_RenderTexture(renderer, entry.tex, nullptr, &dst);
    }

    // --- State-specific drawing (menus, fades, overlays) ---
    m_stateMachine.draw();

    // Skip dialogue box when an overlay is active (save/load)
    if (m_stateMachine.hasOverlays())
        return;

    // --- Dialogue box (universal — drawn by engine, not by states) ---
    if (!dialogue.Text().empty()) {
        float tbY = uiCfg.textbox.y.resolve((float)screenHeight);
        float tbH = uiCfg.textbox.h.resolve((float)screenHeight);
        float tbW = (float)screenWidth;

        if (uiCfg.textbox.image) {
            SDL_FRect tb{0, tbY, tbW, tbH};
            SDL_RenderTexture(renderer, uiCfg.textbox.image, nullptr, &tb);
        }
        else {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer,
                                   uiCfg.textbox.color.r,
                                   uiCfg.textbox.color.g,
                                   uiCfg.textbox.color.b,
                                   uiCfg.textbox.color.a);
            SDL_FRect tb{0, tbY, tbW, tbH};
            SDL_RenderFillRect(renderer, &tb);
        }

        // Name box
        if (!dialogue.Speaker().empty()) {
            float nbY = tbY + uiCfg.namebox.yOffset;
            SDL_FRect nb{uiCfg.namebox.x, nbY, uiCfg.namebox.w, uiCfg.namebox.h};

            if (uiCfg.namebox.image) {
                SDL_RenderTexture(renderer, uiCfg.namebox.image, nullptr, &nb);
            }
            else {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,
                                       uiCfg.namebox.color.r,
                                       uiCfg.namebox.color.g,
                                       uiCfg.namebox.color.b,
                                       uiCfg.namebox.color.a);
                SDL_RenderFillRect(renderer, &nb);
            }

            auto nameTex = RenderText(dialogue.Name(), uiCfg.namebox.textColor);
            if (nameTex) {
                float nw, nh;
                SDL_GetTextureSize(nameTex, &nw, &nh);
                SDL_FRect dst{
                    uiCfg.namebox.x + 15.0f, nbY + (uiCfg.namebox.h - nh) / 2.0f, nw, nh};
                SDL_RenderTexture(renderer, nameTex, nullptr, &dst);
                SDL_DestroyTexture(nameTex);
            }
        }

        // Dialogue text with word wrap
        std::string visible = dialogue.Text().substr(0, dialogue.DisplayedChars());

        // Resolve wrap width: use configured value, or default to 90% of textbox width
        float effectiveWrapW = uiCfg.textbox.wrapWidth.resolve((float)screenWidth);
        if (effectiveWrapW <= 0.0f) {
            effectiveWrapW = (float)screenWidth * 0.9f;
        }
        // Subtract margins to get the actual text area width
        float textAreaW = effectiveWrapW - 2.0f * uiCfg.textbox.textMarginX;
        int wrapPx = static_cast<int>(textAreaW);

        auto textTex = RenderTextWrapped(visible, uiCfg.textbox.textColor, wrapPx);
        if (textTex) {
            float tw, th;
            SDL_GetTextureSize(textTex, &tw, &th);
            float margin = uiCfg.textbox.textMarginX;
            float lineHeight = th + uiCfg.textbox.lineSpacing;
            SDL_FRect dst{margin, tbY + 40.0f, tw, lineHeight};
            SDL_RenderTexture(renderer, textTex, nullptr, &dst);
            SDL_DestroyTexture(textTex);
        }
    }
}
