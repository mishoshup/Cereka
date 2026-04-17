#pragma once
// cereka_states.hpp — Concrete state declarations
//
// Defines all game states used by Cereka VN engine.
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

class DialogueState : public VNState<CerekaState::Running> {
   public:
    void update(float dt,
                IVNStateContext &ctx) override;
};

// ============================================================================
// MenuState — In-game menu with buttons
// ============================================================================

class MenuState : public VNState<CerekaState::InMenu> {
   public:
    void update(float dt,
                IVNStateContext &ctx) override;
    void handleEvent(const CerekaEvent &event,
                     IVNStateContext &ctx) override;
};

// ============================================================================
// FadeState — Background fade transitions
// ============================================================================

class FadeState : public VNState<CerekaState::Fading> {
   public:
    void onEnter(IVNStateContext &ctx) override;
    void update(float dt,
                IVNStateContext &ctx) override;
};

// ============================================================================
// SaveMenuState — Save game overlay
// ============================================================================

class SaveMenuState : public VNState<CerekaState::SaveMenuState> {
   public:
    void handleEvent(const CerekaEvent &event,
                     IVNStateContext &ctx) override;
};

// ============================================================================
// LoadMenuState — Load game overlay
// ============================================================================

class LoadMenuState : public VNState<CerekaState::LoadMenuState> {
   public:
    void handleEvent(const CerekaEvent &event,
                     IVNStateContext &ctx) override;
};

// ============================================================================
// FinishedState — Script ended (terminal)
// ============================================================================

class FinishedState : public VNState<CerekaState::Finished> {};

// ============================================================================
// QuitState — Engine shutdown requested
// ============================================================================

class QuitState : public VNState<CerekaState::Quit> {};

}  // namespace cereka
#endif  // CEREKA_STATES_HPP
