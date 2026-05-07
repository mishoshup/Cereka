// cereka_states.cpp — Concrete state implementations
//
// Handles state-specific logic for each game state.

#include "cereka_states.hpp"
#include "cereka_engine_impl.hpp"
#include "cereka_safe_parse.hpp"

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
                ctx.changeState(CerekaState::WaitingForInput);
                si.pc++;
                return;

            case compiler::Op::NARRATE:
                impl.Narrate(ins.b);
                ctx.changeState(CerekaState::WaitingForInput);
                si.pc++;
                return;

            case compiler::Op::MENU:
                impl.EnterMenu();
                ctx.changeState(CerekaState::InMenu);
                si.pc++;
                return;

            case compiler::Op::JUMP:
                si.pc = si.labelMap[ins.a];
                continue;

            case compiler::Op::CALL:
                if (si.callStack.size() >= 32) {
                    std::cerr << "[CEREKA] Call stack overflow (max 32)\n";
                    ctx.changeState(CerekaState::Finished);
                    return;
                }
                si.callStack.push_back(si.pc + 1);
                si.pc = si.labelMap[ins.a];
                continue;

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
                si.variables[ins.a] = std::to_string(result);
                si.pc++;
                continue;
            }

            case compiler::Op::IF_EQ: {
                auto it = si.variables.find(ins.a);
                std::string val = (it != si.variables.end()) ? it->second : "";
                if (val != ins.b) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::IF_NEQ: {
                auto it = si.variables.find(ins.a);
                std::string val = (it != si.variables.end()) ? it->second : "";
                if (val == ins.b) {
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

            case compiler::Op::STOP_BGM:
                impl.audio.StopBGM();
                si.pc++;
                continue;

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
                if (slot >= 1 && slot <= 10)
                    impl.LoadGame(slot);  // restores pc and state from file
                return;
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
    if (event.type != CerekaEvent::MouseDown)
        return;

    auto &impl = static_cast<Impl &>(ctx);
    int idx = impl.menu.HitTest(
        event.mouseX, event.mouseY, impl.screenWidth, impl.screenHeight,
        impl.uiCfg.button.w, impl.uiCfg.button.h);
    if (idx < 0)
        return;

    if (impl.menu.IsExit(idx)) {
        impl.ExitMenu();
        ctx.changeState(CerekaState::Finished);
        return;
    }

    const std::string &target = impl.menu.Target(idx);
    impl.scriptInterpreter.pc =
        target.empty() ? impl.menu.EndPC() : impl.scriptInterpreter.labelMap[target];
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
            impl.SaveGame(slot);
            ctx.popOverlay();
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
            impl.LoadGame(slot);  // restores pc, variables, state via m_stateMachine
            // No overlay cleanup needed — LoadGame already called
            // clearOverlays() + changeState() on m_stateMachine.
        }
    }
}

void LoadMenuState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.DrawSaveLoadOverlay(false);
}

}  // namespace cereka
