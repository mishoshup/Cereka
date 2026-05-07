#pragma once
// cereka_states.hpp — Concrete state declarations
//
// Defines all game states used by Cereka engine.
//
// States:
// - DialogueState: Normal dialogue/tick execution
// - MenuState: In-game menu with buttons
// - FadeState: Background fade transitions
// - SaveMenuState: Save game overlay
// - LoadMenuState: Load game overlay
// - FinishedState: Script ended (terminal)
// - QuitState: Engine shutdown requested

#ifndef CEREKA_STATES_HPP
#    define CEREKA_STATES_HPP

#    include "cereka_state.hpp"

namespace cereka {

// ============================================================================
// DialogueState — Normal dialogue and tick execution
// ============================================================================

class DialogueState : public CerekaStateBase<CerekaState::Running> {
   public:
    void update(float dt,
                ICerekaStateContext &ctx) override;
};

// ============================================================================
// MenuState — In-game menu with buttons
// ============================================================================

class MenuState : public CerekaStateBase<CerekaState::InMenu> {
   public:
    void update(float dt,
                ICerekaStateContext &ctx) override;
    void handleEvent(const CerekaEvent &event,
                     ICerekaStateContext &ctx) override;
    void draw(ICerekaStateContext &ctx) const override;
};

// ============================================================================
// FadeState — Background fade transitions
// ============================================================================

class FadeState : public CerekaStateBase<CerekaState::Fading> {
   public:
    void onEnter(ICerekaStateContext &ctx) override;
    void update(float dt,
                ICerekaStateContext &ctx) override;
    void draw(ICerekaStateContext &ctx) const override;
};

// ============================================================================
// SaveMenuState — Save game overlay
// ============================================================================

class SaveMenuState : public CerekaStateBase<CerekaState::SaveMenuState> {
   public:
    void handleEvent(const CerekaEvent &event,
                     ICerekaStateContext &ctx) override;
    void draw(ICerekaStateContext &ctx) const override;
};

// ============================================================================
// LoadMenuState — Load game overlay
// ============================================================================

class LoadMenuState : public CerekaStateBase<CerekaState::LoadMenuState> {
   public:
    void handleEvent(const CerekaEvent &event,
                     ICerekaStateContext &ctx) override;
    void draw(ICerekaStateContext &ctx) const override;
};

// ============================================================================
// FinishedState — Script ended (terminal)
// ============================================================================

class FinishedState : public CerekaStateBase<CerekaState::Finished> {};

// ============================================================================
// QuitState — Engine shutdown requested
// ============================================================================

class QuitState : public CerekaStateBase<CerekaState::Quit> {};

}  // namespace cereka
#endif  // CEREKA_STATES_HPP
