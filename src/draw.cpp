// draw.cpp — every frame rendering

#include "engine_impl.hpp"
#include <algorithm>

void Impl::Draw()
{
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderClear(renderer);

    // --- Background ---
    if (background)
        SDL_RenderTexture(renderer, background, nullptr, nullptr);

    // --- Fade overlay (black rect with animated alpha) ---
    if (fadePhase != FadePhase::None) {
        float t     = std::min(fadeTimer / fadePhaseDuration, 1.0f);
        float alpha = (fadePhase == FadePhase::Out) ? t : (1.0f - t);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(alpha * 255.0f));
        SDL_RenderFillRect(renderer, nullptr);
    }

    // --- Characters ---
    for (const auto &[id, entry] : characters) {
        float tw = 0, th = 0;
        SDL_GetTextureSize(entry.tex, &tw, &th);
        float scale   = (screenHeight * 0.8f) / th;
        float centreX = screenWidth * entry.xNorm;
        SDL_FRect dst{
            centreX - tw * scale * 0.5f,
            screenHeight - th * scale - screenHeight * 0.1f,
            tw * scale, th * scale};
        SDL_RenderTexture(renderer, entry.tex, nullptr, &dst);
    }

    // --- Menu buttons ---
    if (inMenu) {
        const float bw      = uiCfg.button.w;
        const float bh      = uiCfg.button.h;
        const float spacing = 20.0f;
        float y = screenHeight * 0.4f;

        for (size_t i = 0; i < buttonTexts.size(); ++i) {
            SDL_FRect btn{(float)screenWidth / 2.0f - bw / 2.0f, y, bw, bh};

            if (uiCfg.button.image) {
                SDL_RenderTexture(renderer, uiCfg.button.image, nullptr, &btn);
            } else {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,
                    uiCfg.button.color.r, uiCfg.button.color.g,
                    uiCfg.button.color.b, uiCfg.button.color.a);
                SDL_RenderFillRect(renderer, &btn);
            }

            auto textTex = RenderText(buttonTexts[i], uiCfg.button.textColor);
            if (textTex) {
                float tw2, th2;
                SDL_GetTextureSize(textTex, &tw2, &th2);
                SDL_FRect tr{
                    (float)screenWidth / 2.0f - tw2 / 2.0f,
                    y + bh / 2.0f - th2 / 2.0f,
                    tw2, th2};
                SDL_RenderTexture(renderer, textTex, nullptr, &tr);
                SDL_DestroyTexture(textTex);
            }
            y += bh + spacing;
        }
    }

    // --- Save/Load overlay (drawn on top of everything, before dialogue box) ---
    if (state == CerekaState::SaveMenu)
        DrawSaveLoadOverlay(true);
    else if (state == CerekaState::LoadMenu)
        DrawSaveLoadOverlay(false);

    if (state == CerekaState::SaveMenu || state == CerekaState::LoadMenu)
        return;

    // --- Dialogue box ---
    if (!currentText.empty()) {
        float tbY = uiCfg.textbox.y.resolve((float)screenHeight);
        float tbH = uiCfg.textbox.h.resolve((float)screenHeight);
        float tbW = (float)screenWidth;

        if (uiCfg.textbox.image) {
            SDL_FRect tb{0, tbY, tbW, tbH};
            SDL_RenderTexture(renderer, uiCfg.textbox.image, nullptr, &tb);
        } else {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer,
                uiCfg.textbox.color.r, uiCfg.textbox.color.g,
                uiCfg.textbox.color.b, uiCfg.textbox.color.a);
            SDL_FRect tb{0, tbY, tbW, tbH};
            SDL_RenderFillRect(renderer, &tb);
        }

        // Name box
        if (!currentSpeaker.empty()) {
            float nbY = tbY + uiCfg.namebox.yOffset;
            SDL_FRect nb{uiCfg.namebox.x, nbY, uiCfg.namebox.w, uiCfg.namebox.h};

            if (uiCfg.namebox.image) {
                SDL_RenderTexture(renderer, uiCfg.namebox.image, nullptr, &nb);
            } else {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,
                    uiCfg.namebox.color.r, uiCfg.namebox.color.g,
                    uiCfg.namebox.color.b, uiCfg.namebox.color.a);
                SDL_RenderFillRect(renderer, &nb);
            }

            auto nameTex = RenderText(currentName, uiCfg.namebox.textColor);
            if (nameTex) {
                float nw, nh;
                SDL_GetTextureSize(nameTex, &nw, &nh);
                SDL_FRect dst{
                    uiCfg.namebox.x + 15.0f,
                    nbY + (uiCfg.namebox.h - nh) / 2.0f,
                    nw, nh};
                SDL_RenderTexture(renderer, nameTex, nullptr, &dst);
                SDL_DestroyTexture(nameTex);
            }
        }

        // Dialogue text with word wrap
        std::string visible = currentText.substr(0, displayedChars);
        auto textTex = RenderText(visible, uiCfg.textbox.textColor);
        if (textTex) {
            float tw, th;
            SDL_GetTextureSize(textTex, &tw, &th);
            float margin = uiCfg.textbox.textMarginX;
            float maxW   = (float)screenWidth - 2.0f * margin;
            float scale  = tw > maxW ? maxW / tw : 1.0f;
            SDL_FRect dst{margin, tbY + 40.0f, tw * scale, th * scale};
            SDL_RenderTexture(renderer, textTex, nullptr, &dst);
            SDL_DestroyTexture(textTex);
        }
    }
}
