// cereka_states.cpp — Concrete state implementations
//
// Handles state-specific logic for each game state.

#include "cereka_states.hpp"
#include "engine_impl.hpp"

namespace cereka {

// ============================================================================
// DialogueState — Normal dialogue and tick execution
// ============================================================================

void DialogueState::update(float dt,
                           IVNStateContext & /*ctx*/)
{
}

// ============================================================================
// MenuState — In-game menu with buttons
// ============================================================================

void MenuState::update(float dt,
                       IVNStateContext & /*ctx*/)
{
}

void MenuState::handleEvent(const CerekaEvent &event,
                            IVNStateContext &ctx)
{
}

// ============================================================================
// FadeState — Background fade transitions
// ============================================================================

void FadeState::onEnter(IVNStateContext & /*ctx*/) {}

void FadeState::update(float dt,
                       IVNStateContext & /*ctx*/)
{
}

// ============================================================================
// SaveMenuState — Save game overlay
// ============================================================================

void SaveMenuState::handleEvent(const CerekaEvent &event,
                                IVNStateContext &ctx)
{
}

// ============================================================================
// LoadMenuState — Load game overlay
// ============================================================================

void LoadMenuState::handleEvent(const CerekaEvent &event,
                                IVNStateContext &ctx)
{
}

}  // namespace cereka
