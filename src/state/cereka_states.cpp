// cereka_states.cpp — Concrete state implementations
//
// Handles state-specific logic for each game state.

#include "cereka_states.hpp"
#include "cereka_engine_impl.hpp"

namespace cereka {

// ============================================================================
// DialogueState — Normal dialogue and tick execution
// ============================================================================

void DialogueState::update(float dt,
                           ICerekaStateContext &ctx)
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.CerekaScriptTick();
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
            impl.LoadGame(slot);  // restores pc, variables, state from file
            // After load the state machine overlay stack and internal state
            // must match what LoadGame restored (the `state` member).
            impl.m_stateMachine.clearOverlays();
            impl.m_stateMachine.changeState(impl.state);
        }
    }
}

void LoadMenuState::draw(ICerekaStateContext &ctx) const
{
    auto &impl = static_cast<Impl &>(ctx);
    impl.DrawSaveLoadOverlay(false);
}

}  // namespace cereka
