// cereka_states.cpp — Concrete state implementations
//
// Handles state-specific logic for each game state.

#include "cereka_states.hpp"
#include "cereka_engine_impl.hpp"
#include "cereka_safe_parse.hpp"
#include <algorithm>

namespace cereka {

// ============================================================================
// DialogueState — Normal dialogue and tick execution
// ============================================================================

void DialogueState::update(float dt,
                           ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    auto &si = impl.scriptInterpreter;

    // No guard on state — machine only calls update() when in Running state
    while (si.pc < si.program.size()) {
        const auto &ins = si.program[si.pc];

        // Skip mode: inside a false if-block
        if (si.skipMode) {
            if (ins.op == compiler::Op::IF_EQ || ins.op == compiler::Op::IF_NEQ ||
                ins.op == compiler::Op::IF_GT || ins.op == compiler::Op::IF_LT ||
                ins.op == compiler::Op::IF_GE || ins.op == compiler::Op::IF_LE) {
                si.skipDepth++;
            } else if (ins.op == compiler::Op::ELSE) {
                // ELSE at the outermost skip depth means the IF body is done
                // and the ELSE body should execute (exit skip mode).
                if (si.skipDepth == 1) {
                    si.skipMode = false;
                    si.skipDepth = 0;
                    si.pc++;
                    continue;
                }
                // Nested ELSE: still inside a skipped outer block, keep skipping
            } else if (ins.op == compiler::Op::ENDIF) {
                si.skipDepth--;
                if (si.skipDepth == 0)
                    si.skipMode = false;
            }
            si.pc++;
            continue;
        }

        switch (ins.op) {

            case compiler::Op::BG:
                impl.scene.ShowBackground(ins.a);
                si.pc++;
                continue;

            case compiler::Op::FADE: {
                float totalDur = 0.5f;
                if (!ins.b.empty()) {
                    auto parsed = safe_stof(ins.b);
                    if (parsed) totalDur = *parsed;
                }
                impl.scene.StartFade(ins.a, totalDur);
                ctx.changeState(CerekaState::Fading);
                si.pc++;
                return;
            }

            case compiler::Op::CHAR:
                impl.scene.ShowCharacter(ins.a, ins.b, ins.c);
                si.pc++;
                continue;

            case compiler::Op::HIDE_CHAR:
                impl.scene.HideCharacter(ins.a);
                si.pc++;
                continue;

            case compiler::Op::SAY:
                impl.Say(ins.a, ins.a, ins.b);
                impl.rollbackManager.capture(impl);
                ctx.changeState(CerekaState::WaitingForInput);
                si.pc++;
                return;

            case compiler::Op::NARRATE:
                impl.Narrate(ins.b);
                impl.rollbackManager.capture(impl);
                ctx.changeState(CerekaState::WaitingForInput);
                si.pc++;
                return;

            case compiler::Op::MENU:
                impl.EnterMenu();
                impl.rollbackManager.capture(impl);
                ctx.changeState(CerekaState::InMenu);
                si.pc++;
                return;

            case compiler::Op::JUMP: {
                auto it = si.labelMap.find(ins.a);
                if (it != si.labelMap.end()) {
                    si.pc = it->second;
                } else {
                    std::cerr << "[CEREKA] JUMP to unknown label: " << ins.a << "\n";
                    si.pc++;
                }
                continue;
            }

            case compiler::Op::CALL: {
                if (si.callStack.size() >= 32) {
                    std::cerr << "[CEREKA] Call stack overflow (max 32)\n";
                    ctx.changeState(CerekaState::Finished);
                    return;
                }
                auto it = si.labelMap.find(ins.a);
                if (it != si.labelMap.end()) {
                    si.callStack.push_back(si.pc + 1);
                    si.pc = it->second;
                } else {
                    std::cerr << "[CEREKA] CALL to unknown label: " << ins.a << "\n";
                    si.pc++;
                }
                continue;
            }

            case compiler::Op::RETURN:
                if (!si.callStack.empty()) {
                    si.pc = si.callStack.back();
                    si.callStack.pop_back();
                } else {
                    ctx.changeState(CerekaState::Finished);
                }
                continue;

            case compiler::Op::SET_VAR:
                si.variables[ins.a] = ins.b;
                si.pc++;
                continue;

            case compiler::Op::SET_VAR_NUM: {
                float lhs = si.LookupNumVar(ins.a);
                float rhs = si.EvalExpr(ins.c);
                float result = 0.0f;
                if (ins.b == "+")
                    result = lhs + rhs;
                else if (ins.b == "-")
                    result = lhs - rhs;
                else if (ins.b == "*")
                    result = lhs * rhs;
                else if (ins.b == "/")
                    result = (rhs != 0.0f) ? (lhs / rhs) : 0.0f;
                else
                    result = rhs;  // "=" plain assignment
                si.numVariables[ins.a] = result;
                {
                    std::string s = std::to_string(result);
                    auto dot = s.find('.');
                    if (dot != std::string::npos) {
                        s.erase(s.find_last_not_of('0') + 1);
                        if (s.back() == '.') s.pop_back();
                    }
                    si.variables[ins.a] = s;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::IF_EQ: {
                auto it = si.variables.find(ins.a);
                std::string val = (it != si.variables.end()) ? it->second : "";
                std::string rhs = ins.b;
                auto rit = si.variables.find(ins.b);
                if (rit != si.variables.end())
                    rhs = rit->second;
                bool eq = (val == rhs);
                if (!eq) {
                    auto lf = safe_stof(val);
                    auto rf = safe_stof(rhs);
                    if (lf && rf && *lf == *rf)
                        eq = true;
                }
                if (!eq) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::IF_NEQ: {
                auto it = si.variables.find(ins.a);
                std::string val = (it != si.variables.end()) ? it->second : "";
                std::string rhs = ins.b;
                auto rit = si.variables.find(ins.b);
                if (rit != si.variables.end())
                    rhs = rit->second;
                bool eq = (val == rhs);
                if (!eq) {
                    auto lf = safe_stof(val);
                    auto rf = safe_stof(rhs);
                    if (lf && rf && *lf == *rf)
                        eq = true;
                }
                if (eq) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::IF_GT:
            case compiler::Op::IF_LT:
            case compiler::Op::IF_GE:
            case compiler::Op::IF_LE: {
                float lhs = si.LookupNumVar(ins.a);
                float rhs = si.EvalExpr(ins.b);
                bool cond = false;
                switch (ins.op) {
                    case compiler::Op::IF_GT: cond = lhs > rhs; break;
                    case compiler::Op::IF_LT: cond = lhs < rhs; break;
                    case compiler::Op::IF_GE: cond = lhs >= rhs; break;
                    case compiler::Op::IF_LE: cond = lhs <= rhs; break;
                    default: break;
                }
                if (!cond) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::ENDIF:
                si.pc++;
                continue;

            case compiler::Op::ELSE:
                // If we reach here, skipMode was false, meaning the IF branch was executed.
                // So we must skip the ELSE branch.
                si.skipMode = true;
                si.skipDepth = 1;
                si.pc++;
                continue;

            case compiler::Op::PLAY_BGM:
                impl.audio.PlayBGM(ins.a);
                si.pc++;
                continue;

            case compiler::Op::PLAY_BGM_FADE: {
                float dur = 0.0f;
                if (!ins.b.empty()) {
                    auto parsed = safe_stof(ins.b);
                    if (parsed) dur = *parsed;
                }
                impl.audio.PlayBGM(ins.a, dur);
                si.pc++;
                continue;
            }

            case compiler::Op::STOP_BGM:
                impl.audio.StopBGM();
                si.pc++;
                continue;

            case compiler::Op::STOP_BGM_FADE: {
                float dur = 0.0f;
                if (!ins.b.empty()) {
                    auto parsed = safe_stof(ins.b);
                    if (parsed) dur = *parsed;
                }
                impl.audio.StopBGM(dur);
                si.pc++;
                continue;
            }

            case compiler::Op::BGM_CROSSFADE: {
                float dur = 1.0f;
                if (!ins.b.empty()) {
                    auto parsed = safe_stof(ins.b);
                    if (parsed) dur = *parsed;
                }
                impl.audio.CrossfadeBGM(ins.a, dur);
                si.pc++;
                continue;
            }

            case compiler::Op::PLAY_SFX:
                impl.audio.PlaySFX(ins.a);
                si.pc++;
                continue;

            case compiler::Op::SG_CREATE:
                impl.ui.GetSceneGraph().createNode(ins.a);
                si.pc++;
                continue;

            case compiler::Op::SG_SET:
                impl.ui.GetSceneGraph().setTransform(ins.a, ins.b);
                si.pc++;
                continue;

            case compiler::Op::SG_REMOVE:
                impl.ui.GetSceneGraph().removeNode(ins.a);
                si.pc++;
                continue;

            case compiler::Op::UI_SET:
                impl.ApplyUiSet(ins.a, ins.b);
                si.pc++;
                continue;

            case compiler::Op::SAVE_MENU:
                ctx.pushOverlay(CerekaState::SaveMenuState);
                si.pc++;
                return;

            case compiler::Op::LOAD_MENU:
                ctx.pushOverlay(CerekaState::LoadMenuState);
                si.pc++;
                return;

            case compiler::Op::SAVE: {
                int slot = 0;
                if (!ins.a.empty()) {
                    auto r = safe_stoi(ins.a);
                    if (r) slot = *r;
                }
                if (slot >= 1 && slot <= 10)
                    impl.SaveGame(slot);
                si.pc++;
                continue;
            }

            case compiler::Op::LOAD: {
                int slot = 0;
                if (!ins.a.empty()) {
                    auto r = safe_stoi(ins.a);
                    if (r) slot = *r;
                }
                if (slot >= 1 && slot <= 10) {
                    if (impl.LoadGame(slot))
                        return;  // LoadGame restored PC from save data
                }
                si.pc++;
                continue;
            }

            case compiler::Op::CHECKPOINT_STORE: {
                CheckpointData cp;
                cp.variables = si.variables;
                cp.numVariables = si.numVariables;
                si.checkpoints[ins.a] = std::move(cp);
                si.pc++;
                continue;
            }

            case compiler::Op::CHECKPOINT_LOAD: {
                auto it = si.checkpoints.find(ins.a);
                if (it != si.checkpoints.end()) {
                    si.variables = it->second.variables;
                    si.numVariables = it->second.numVariables;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::END:
                ctx.changeState(CerekaState::Finished);
                return;

            case compiler::Op::LABEL:
                si.pc++;
                continue;

            default:
                si.pc++;
                continue;
        }
    }
}

// ============================================================================
// MenuState — In-game menu with buttons
// ============================================================================

void MenuState::update(float dt,
                       ICerekaStateContext & /*ctx*/)
{
}

void MenuState::handleEvent(const CerekaEvent &event,
                            ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    auto &menu = impl.menu;

    // ---- Mouse motion: update hovered index ----
    if (event.type == CerekaEvent::MouseMove) {
        int idx = menu.HitTest(
            (int)event.mouseX, (int)event.mouseY,
            impl.screenWidth, impl.screenHeight,
            impl.uiCfg.button.w, impl.uiCfg.button.h,
            impl.uiCfg.button.y, impl.uiCfg.button.spacing);
        menu.SetHoveredIndex(idx);
        // Mouse movement transfers selection to hover position
        if (idx >= 0)
            menu.SetSelectedIndex(idx);
        return;
    }

    // ---- Keyboard navigation ----
    if (event.type == CerekaEvent::KeyDown) {
        if (event.key == SDLK_UP || event.key == SDLK_DOWN) {
            int count = (int)menu.ButtonCount();
            if (count == 0) return;

            int bpp = MenuSystem::ButtonsPerPage(
                (float)impl.screenHeight, impl.uiCfg.button.y,
                impl.uiCfg.button.h, impl.uiCfg.button.spacing);

            int sel = menu.SelectedIndex();
            int newSel = sel;

            if (event.key == SDLK_DOWN) {
                // Next button or next page
                if (sel + 1 < count) {
                    newSel = sel + 1;
                } else if (menu.CurrentPage() < menu.TotalPages() - 1) {
                    // Wrap to next page
                    menu.SetCurrentPage(menu.CurrentPage() + 1);
                    newSel = menu.CurrentPage() * bpp;
                }
            } else {
                // SDLK_UP: prev button or prev page
                if (sel > 0) {
                    newSel = sel - 1;
                } else if (menu.CurrentPage() > 0) {
                    menu.SetCurrentPage(menu.CurrentPage() - 1);
                    int pageStart = menu.CurrentPage() * bpp;
                    int pageEnd = std::min(pageStart + bpp, count);
                    newSel = pageEnd - 1;
                }
            }

            if (newSel != sel) {
                menu.SetSelectedIndex(newSel);
                menu.SetHoveredIndex(newSel);

                // Clamp page if selection moved across page boundary
                int newPage = newSel / bpp;
                if (newPage != menu.CurrentPage())
                    menu.SetCurrentPage(newPage);
            }
            return;
        }

        if (event.key == SDLK_RETURN || event.key == SDLK_SPACE) {
            int sel = menu.SelectedIndex();
            if (sel < 0 || sel >= (int)menu.ButtonCount())
                return;

            activateButton(impl, menu, sel, ctx);
            return;
        }

        // Left/Right for page navigation
        if (event.key == SDLK_LEFT || event.key == SDLK_RIGHT) {
            int dir = (event.key == SDLK_RIGHT) ? 1 : -1;
            int newPage = menu.CurrentPage() + dir;
            if (newPage >= 0 && newPage < menu.TotalPages()) {
                menu.SetCurrentPage(newPage);
                int bpp = MenuSystem::ButtonsPerPage(
                    (float)impl.screenHeight, impl.uiCfg.button.y,
                    impl.uiCfg.button.h, impl.uiCfg.button.spacing);
                int newSel = std::min(newPage * bpp, (int)menu.ButtonCount() - 1);
                menu.SetSelectedIndex(newSel);
                menu.SetHoveredIndex(newSel);
            }
            return;
        }
    }

    // ---- Mouse click: select button ----
    if (event.type == CerekaEvent::MouseDown) {
        int idx = menu.HitTest(
            (int)event.mouseX, (int)event.mouseY,
            impl.screenWidth, impl.screenHeight,
            impl.uiCfg.button.w, impl.uiCfg.button.h,
            impl.uiCfg.button.y, impl.uiCfg.button.spacing);

        if (idx < 0)
            return;

        // Check page navigation sentinels
        int btnCount = (int)menu.ButtonCount();
        if (idx == btnCount) {
            // "next page" sentinel
            if (menu.CurrentPage() < menu.TotalPages() - 1) {
                menu.SetCurrentPage(menu.CurrentPage() + 1);
                int bpp = MenuSystem::ButtonsPerPage(
                    (float)impl.screenHeight, impl.uiCfg.button.y,
                    impl.uiCfg.button.h, impl.uiCfg.button.spacing);
                int newSel = menu.CurrentPage() * bpp;
                menu.SetSelectedIndex(newSel);
                menu.SetHoveredIndex(newSel);
            }
            return;
        }
        if (idx == btnCount + 1) {
            // "prev page" sentinel
            if (menu.CurrentPage() > 0) {
                menu.SetCurrentPage(menu.CurrentPage() - 1);
                int bpp = MenuSystem::ButtonsPerPage(
                    (float)impl.screenHeight, impl.uiCfg.button.y,
                    impl.uiCfg.button.h, impl.uiCfg.button.spacing);
                int pageStart = menu.CurrentPage() * bpp;
                int pageEnd = std::min(pageStart + bpp, btnCount);
                menu.SetSelectedIndex(pageEnd - 1);
                menu.SetHoveredIndex(pageEnd - 1);
            }
            return;
        }

        activateButton(impl, menu, idx, ctx);
    }
}

void MenuState::activateButton(CerekaImpl &impl,
                                const MenuSystem &menu,
                                int idx,
                                ICerekaStateContext &ctx)
{
    if (menu.IsExit(idx)) {
        impl.ExitMenu();
        ctx.changeState(CerekaState::Finished);
        return;
    }

    const std::string &target = menu.Target(idx);
    impl.scriptInterpreter.pc =
        target.empty() ? menu.EndPC() : impl.scriptInterpreter.labelMap[target];
    impl.ExitMenu();
    ctx.changeState(CerekaState::Running);
}

void MenuState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.ui.DrawMenuButtons(impl.menu, impl.uiCfg);
}

// ============================================================================
// FadeState — Background fade transitions
// ============================================================================

void FadeState::onEnter(ICerekaStateContext & /*ctx*/) {}

void FadeState::update(float dt,
                       ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    if (impl.scene.TickFade(dt))
        ctx.changeState(CerekaState::Running);
}

void FadeState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.ui.DrawFadeOverlay(impl.scene);
}

// ============================================================================
// SaveMenuState — Save game overlay
// ============================================================================

void SaveMenuState::handleEvent(const CerekaEvent &event,
                                ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    if (event.type == CerekaEvent::KeyDown && event.key == SDLK_ESCAPE) {
        ctx.popOverlay();
        return;
    }
    if (event.type == CerekaEvent::MouseDown) {
        int slot = impl.HitTestSaveSlot((int)event.mouseX, (int)event.mouseY);
        if (slot >= 1 && slot <= 10) {
            // Check if slot has existing data — ask for confirmation
            auto meta = impl.GetSlotMetadata(slot);
            if (!meta.timestamp.empty()) {
                impl.pendingConfirmSlot_ = slot;
                ctx.pushOverlay(CerekaState::ConfirmOverwriteState);
            } else {
                impl.SaveGame(slot);
                ctx.popOverlay();
            }
        }
    }
}

void SaveMenuState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.DrawSaveLoadOverlay(true);
}

// ============================================================================
// LoadMenuState — Load game overlay
// ============================================================================

void LoadMenuState::handleEvent(const CerekaEvent &event,
                                ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    if (event.type == CerekaEvent::KeyDown && event.key == SDLK_ESCAPE) {
        ctx.popOverlay();
        return;
    }
    if (event.type == CerekaEvent::MouseDown) {
        int slot = impl.HitTestSaveSlot((int)event.mouseX, (int)event.mouseY);
        if (slot >= 1 && slot <= 10) {
            auto meta = impl.GetSlotMetadata(slot);
            if (!meta.timestamp.empty()) {
                impl.LoadGame(slot);  // restores pc, variables, state via m_stateMachine
                // No overlay cleanup needed — LoadGame already called
                // clearOverlays() + changeState() on m_stateMachine.
            }
        }
    }
}

void LoadMenuState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.DrawSaveLoadOverlay(false);
}

// ============================================================================
// PauseMenuState — Pause menu overlay
// ============================================================================

void PauseMenuState::handleEvent(const CerekaEvent &event,
                                  ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);

    if (event.type == CerekaEvent::KeyDown && event.key == SDLK_ESCAPE) {
        ctx.popOverlay();
        return;
    }

    if (event.type == CerekaEvent::MouseDown) {
        int btn = impl.ui.HitTestPauseButton(
            (int)event.mouseX, (int)event.mouseY,
            impl.screenWidth, impl.screenHeight);
        if (btn < 0)
            return;

        switch (btn) {
            case 0: // Continue
                ctx.popOverlay();
                return;
            case 1: // Save
                ctx.pushOverlay(CerekaState::SaveMenuState);
                return;
            case 2: // Load
                ctx.pushOverlay(CerekaState::LoadMenuState);
                return;
            case 3: // Settings
                ctx.pushOverlay(CerekaState::SettingsMenuState);
                return;
            case 4: // Quit to Menu
                ctx.changeState(CerekaState::Finished);
                return;
        }
    }
}

void PauseMenuState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.ui.DrawPauseOverlay(impl.uiCfg);
}

// ============================================================================
// ConfirmOverwriteState — "Overwrite Slot N?" dialog
// ============================================================================

void ConfirmOverwriteState::handleEvent(const CerekaEvent &event,
                                         ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);

    if (event.type == CerekaEvent::KeyDown && event.key == SDLK_ESCAPE) {
        ctx.popOverlay(); // cancel confirm, back to save menu
        return;
    }

    if (event.type == CerekaEvent::MouseDown) {
        int result = impl.ui.HitTestConfirmButton(
            (int)event.mouseX, (int)event.mouseY,
            impl.screenWidth, impl.screenHeight);
        if (result == 0)
            return;

        if (result == 1) {
            // Yes — save and pop both confirm + save overlays
            impl.SaveGame(impl.pendingConfirmSlot_);
            impl.pendingConfirmSlot_ = -1;
            ctx.popOverlay(); // pop confirm
            ctx.popOverlay(); // pop save menu
        } else {
            // No — just pop confirm, back to save menu
            ctx.popOverlay();
        }
    }
}

void ConfirmOverwriteState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.ui.DrawConfirmOverwriteDialog(impl.pendingConfirmSlot_, impl.uiCfg);
}

// ============================================================================
// SettingsMenuState — Settings display overlay
// ============================================================================

void SettingsMenuState::cycleSetting(int row, CerekaImpl &impl)
{
    auto &s = impl.settingsManager.Get();
    switch (row) {
        case 0: { // Text Speed
            static const float speeds[] = {30.0f, 60.0f, 90.0f, 120.0f, 200.0f};
            int idx = 0;
            for (int i = 0; i < 5; ++i) {
                if (s.textSpeed <= speeds[i] + 1.0f) { idx = i; break; }
                idx = i;
            }
            idx = (idx + 1) % 5;
            s.textSpeed = speeds[idx];
            break;
        }
        case 1: { // BGM Volume
            s.bgmVolume += 0.25f;
            if (s.bgmVolume > 1.0f + 0.01f) s.bgmVolume = 0.0f;
            break;
        }
        case 2: { // SFX Volume
            s.sfxVolume += 0.25f;
            if (s.sfxVolume > 1.0f + 0.01f) s.sfxVolume = 0.0f;
            break;
        }
        case 3: // Auto Advance
            s.autoAdvance = !s.autoAdvance;
            break;
        case 4: // Skip Unseen
            s.skipUnseen = !s.skipUnseen;
            break;
    }
    impl.settingsManager.Apply(impl.dialogue, impl.audio);
    impl.settingsManager.Save();
}

void SettingsMenuState::handleEvent(const CerekaEvent &event,
                                     ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);

    if (event.type == CerekaEvent::KeyDown && event.key == SDLK_ESCAPE) {
        ctx.popOverlay(); // back to pause menu
        return;
    }

    if (event.type == CerekaEvent::MouseDown) {
        // Hit-test the 5 setting rows
        int screenW = impl.screenWidth;
        int screenH = impl.screenHeight;
        float panelW = std::min(550.0f, screenW * 0.65f);
        float rowH = 50.0f;
        float contentH = 5.0f * rowH + 4.0f * 6.0f + 80.0f;
        float panelH = contentH;
        float panelX = (screenW - panelW) * 0.5f;
        float panelY = (screenH - panelH) * 0.5f;
        float y0 = panelY + 45.0f;
        float mx = (float)event.mouseX;
        float my = (float)event.mouseY;

        if (mx >= panelX + 10.0f && mx <= panelX + panelW - 10.0f) {
            for (int row = 0; row < 5; ++row) {
                float ry = y0 + row * (rowH + 6.0f);
                if (my >= ry && my <= ry + rowH) {
                    cycleSetting(row, impl);
                    return;
                }
            }
        }
    }
}

void SettingsMenuState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    int screenW = impl.screenWidth;
    int screenH = impl.screenHeight;

    // Dim background
    impl.m_renderCtx->SetBlendMode(true);
    impl.m_renderCtx->FillScreen(Color{0, 0, 0, 160});

    float panelW = std::min(550.0f, screenW * 0.65f);
    float rowH = 50.0f;
    float rowGap = 6.0f;
    float contentH = 5.0f * rowH + 4.0f * rowGap + 80.0f;
    float panelH = contentH;
    float panelX = (screenW - panelW) * 0.5f;
    float panelY = (screenH - panelH) * 0.5f;

    // Panel background
    impl.m_renderCtx->SetBlendMode(true);
    impl.m_renderCtx->FillRect(
        Rect{panelX, panelY, panelW, panelH},
        Color{20, 22, 38, 230});

    // Title
    auto titleTex = impl.m_renderCtx->CreateTextTexture(
        impl.font, "SETTINGS", Color{180, 200, 255, 255});
    if (titleTex) {
        float tw = titleTex->Width();
        float th = titleTex->Height();
        Rect dst{panelX + (panelW - tw) * 0.5f, panelY + 8.0f, tw, th};
        impl.m_renderCtx->DrawTexture(*titleTex, nullptr, &dst);
    }

    // Read settings
    const auto &s = impl.settingsManager.Get();

    // Setting row labels
    static const char *SETTING_LABELS[] = {
        "Text Speed", "BGM Volume", "SFX Volume", "Auto Advance", "Skip Unseen"
    };
    float y0 = panelY + 45.0f;

    for (int i = 0; i < 5; ++i) {
        float ry = y0 + i * (rowH + rowGap);
        impl.m_renderCtx->SetBlendMode(true);
        impl.m_renderCtx->FillRect(
            Rect{panelX + 10.0f, ry, panelW - 20.0f, rowH},
            Color{40, 44, 66, 210});

        // Build value string
        std::string valStr;
        switch (i) {
            case 0: valStr = std::to_string((int)s.textSpeed) + " cps"; break;
            case 1: valStr = std::to_string((int)(s.bgmVolume * 100.0f)) + "%"; break;
            case 2: valStr = std::to_string((int)(s.sfxVolume * 100.0f)) + "%"; break;
            case 3: valStr = s.autoAdvance ? "ON" : "OFF"; break;
            case 4: valStr = s.skipUnseen ? "ON" : "OFF"; break;
        }

        // Row label
        auto labelTex = impl.m_renderCtx->CreateTextTexture(
            impl.font, SETTING_LABELS[i], Color{220, 220, 220, 255});
        if (labelTex) {
            float tw = labelTex->Width();
            float th = labelTex->Height();
            Rect dst{panelX + 20.0f, ry + (rowH - th) * 0.5f, tw, th};
            impl.m_renderCtx->DrawTexture(*labelTex, nullptr, &dst);
        }

        // Value text (right-aligned)
        auto valTex = impl.m_renderCtx->CreateTextTexture(
            impl.font, valStr, Color{160, 200, 255, 255});
        if (valTex) {
            float tw = valTex->Width();
            float th = valTex->Height();
            Rect dst{panelX + panelW - 20.0f - tw, ry + (rowH - th) * 0.5f, tw, th};
            impl.m_renderCtx->DrawTexture(*valTex, nullptr, &dst);
        }
    }

    // ESC hint
    auto hintTex = impl.m_renderCtx->CreateTextTexture(
        impl.font, "ESC to return | Click a row to change", Color{120, 120, 120, 255});
    if (hintTex) {
        float tw = hintTex->Width();
        float th = hintTex->Height();
        Rect dst{panelX + (panelW - tw) * 0.5f, panelY + panelH - th - 8.0f, tw, th};
        impl.m_renderCtx->DrawTexture(*hintTex, nullptr, &dst);
    }
}

// ============================================================================
// HistoryState — Dialogue history overlay
// ============================================================================

void HistoryState::handleEvent(const CerekaEvent &event,
                                ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    if (event.type == CerekaEvent::KeyDown && event.key == SDLK_ESCAPE) {
        ctx.popOverlay();
        return;
    }
    if (event.type == CerekaEvent::MouseDown) {
        int idx = impl.historyHitTest((int)event.mouseX, (int)event.mouseY);
        if (idx >= 0) {
            ctx.popOverlay();
            impl.rollbackManager.goTo(impl, (size_t)idx);
        }
    }
}

void HistoryState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.ui.DrawHistoryOverlay(impl.rollbackManager.historyTexts());
}

}  // namespace cereka
