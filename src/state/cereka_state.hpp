#pragma once
// cereka_state.hpp — Cereka Engine State System
//
// Implements a hierarchical state machine pattern for managing game flow.
// States encapsulate their own behavior (enter, exit, update, events).
//
// Enterprise patterns:
// - RAII for resource management
// - std::expected for error handling
// - Pure virtual interfaces
// - Self-documenting code
//
// State hierarchy:
//   ICerekaState (interface)
//     └── CerekaState<T> (CRTP base)
//           └── Concrete states (DialogueState, MenuState, etc.)
//
// Usage:
//   CerekaStateMachine sm;
//   sm.registerState<DialogueState>();
//   sm.registerState<MenuState>();
//   sm.setInitialState(CerekaState::Dialogue);

#ifndef CEREKA_STATE_HPP
#    define CEREKA_STATE_HPP

#    include "Cereka/Cereka.hpp"
#    include <expected>
#    include <functional>
#    include <iostream>
#    include <memory>
#    include <string>
#    include <unordered_map>
#    include <utility>
#    include <vector>

namespace cereka {

// ============================================================================
// ICerekaStateContext — Interface for state → engine communication
// ============================================================================

/**
 * @brief Interface for Cereka states to request actions from the engine.
 *
 * States should never directly manipulate engine state. They request
 * changes through this interface, enabling loose coupling and testability.
 */
class ICerekaStateContext {
   public:
    virtual ~ICerekaStateContext() = default;

    virtual void changeState(CerekaState newState) = 0;
    virtual void pushOverlay(CerekaState overlayState) = 0;
    virtual void popOverlay() = 0;
    // getSavedState/setSavedState removed — overlay stack is the source of truth
};

// ============================================================================
// ICerekaState — Abstract base for all Cereka game states
// ============================================================================

/**
 * @brief Abstract base for all Cereka game states.
 *
 * Each state encapsulates:
 * - onEnter: Called when state becomes active
 * - onExit: Called when state becomes inactive
 * - update: Per-frame logic (dt = delta time in seconds)
 * - handleEvent: Input event processing
 * - draw: Per-frame rendering
 */
class ICerekaState {
   public:
    virtual ~ICerekaState() = default;

    [[nodiscard]] virtual CerekaState type() const = 0;

    virtual void onEnter(ICerekaStateContext &) {}
    virtual void onExit(ICerekaStateContext &) {}
    virtual void update(float dt,
                        ICerekaStateContext &)
    {
    }
    virtual void draw(ICerekaStateContext &) const {}
    virtual void handleEvent(const CerekaEvent &event,
                             ICerekaStateContext &)
    {
    }
};

// ============================================================================
// CerekaState — CRTP template for simple states
// ============================================================================

/**
 * @brief CRTP base class for states with compile-time type dispatch.
 *
 * Usage:
 *   class DialogueState : public CerekaStateBase<CerekaState::Dialogue> {
 *       void onEnter(ICerekaStateContext& ctx) override { ... }
 *   };
 */
template<CerekaState T> class CerekaStateBase : public ICerekaState {
   public:
    [[nodiscard]] CerekaState type() const override
    {
        return T;
    }
};

// ============================================================================
// CerekaStateMachine — Manages state transitions and overlays
// ============================================================================

class CerekaStateMachine {
   public:
    CerekaStateMachine() = default;

    /// Human-readable label for a CerekaState value.
    static const char *stateLabel(CerekaState s)
    {
        switch (s) {
            case CerekaState::Running:         return "Running";
            case CerekaState::WaitingForInput: return "WaitingForInput";
            case CerekaState::InMenu:          return "InMenu";
            case CerekaState::Fading:          return "Fading";
            case CerekaState::Finished:        return "Finished";
            case CerekaState::Quit:            return "Quit";
            case CerekaState::SaveMenuState:   return "SaveMenu";
            case CerekaState::LoadMenuState:   return "LoadMenu";
            case CerekaState::HistoryState:          return "HistoryState";
            case CerekaState::PauseMenuState:        return "PauseMenu";
            case CerekaState::ConfirmOverwriteState: return "ConfirmOverwrite";
            case CerekaState::SettingsMenuState:     return "SettingsMenu";
        }
        return "?";
    }

    void setContext(ICerekaStateContext &ctx)
    {
        ctx_ = &ctx;
    }

    template<typename StateT> void registerState()
    {
        static_assert(std::is_base_of_v<ICerekaState, StateT>, "StateT must derive from ICerekaState");
        auto state = std::make_unique<StateT>();
        CerekaState type = state->type();
        states_[type] = std::move(state);
    }

    void setInitialState(CerekaState type)
    {
        auto it = states_.find(type);
        if (it != states_.end()) {
            currentType_ = type;
            currentState_ = it->second.get();
            currentState_->onEnter(*ctx_);
        }
    }

    void changeState(CerekaState newType)
    {
        if (!currentState_ || !ctx_)
            return;

        if (!headless_)
            std::cout << "[STATE] " << stateLabel(currentType_) << " -> " << stateLabel(newType)
                      << "\n";

        currentState_->onExit(*ctx_);
        currentState_ = nullptr;

        auto it = states_.find(newType);
        if (it != states_.end()) {
            currentType_ = newType;
            currentState_ = it->second.get();
            currentState_->onEnter(*ctx_);
        }
    }

    void setHeadless(bool v) { headless_ = v; }

    void pushOverlay(CerekaState overlayType)
    {
        if (!ctx_)
            return;

        if (!headless_)
            std::cout << "[STATE] " << stateLabel(currentType_) << " -> pushOverlay("
                      << stateLabel(overlayType) << ")\n";

        overlayStack_.push_back({currentType_, currentState_});

        auto it = states_.find(overlayType);
        if (it != states_.end()) {
            currentType_ = overlayType;
            currentState_ = it->second.get();
            currentState_->onEnter(*ctx_);
        }
    }

    void popOverlay()
    {
        if (!ctx_ || overlayStack_.empty())
            return;

        if (!headless_)
            std::cout << "[STATE] popOverlay(" << stateLabel(currentType_) << ") -> "
                      << stateLabel(overlayStack_.back().first) << "\n";

        if (currentState_) {
            currentState_->onExit(*ctx_);
        }

        auto [prevType, prevState] = overlayStack_.back();
        overlayStack_.pop_back();

        currentType_ = prevType;
        currentState_ = prevState;
    }

    void update(float dt)
    {
        if (currentState_ && ctx_) {
            currentState_->update(dt, *ctx_);
        }
    }

    void draw() const
    {
        if (currentState_ && ctx_) {
            currentState_->draw(*ctx_);
        }
    }

    void handleEvent(const CerekaEvent &event)
    {
        if (currentState_ && ctx_) {
            currentState_->handleEvent(event, *ctx_);
        }
    }

    [[nodiscard]] CerekaState currentType() const
    {
        return currentType_;
    }

    [[nodiscard]] bool isInitialized() const
    {
        return ctx_ != nullptr && currentState_ != nullptr;
    }

    [[nodiscard]] bool hasOverlays() const
    {
        return !overlayStack_.empty();
    }

    // Returns the underlying gameplay state, walking past any overlay-only
    // states (pause menu, save/load, settings, history, confirm dialog).
    // Used by save serialization to capture the true game state rather than
    // the overlay state.
    [[nodiscard]] CerekaState effectiveState() const
    {
        if (overlayStack_.empty())
            return currentType_;

        // Walk the stack from bottom to top to find the first state that
        // is a genuine gameplay state (Running, WaitingForInput, InMenu,
        // Fading, Finished, Quit).
        std::vector<CerekaState> skipStates = {
            CerekaState::PauseMenuState,
            CerekaState::SaveMenuState,
            CerekaState::LoadMenuState,
            CerekaState::HistoryState,
            CerekaState::ConfirmOverwriteState,
            CerekaState::SettingsMenuState
        };

        auto isSkip = [&](CerekaState s) {
            for (auto ss : skipStates)
                if (s == ss) return true;
            return false;
        };

        // Check the current (top) state first
        if (!isSkip(currentType_))
            return currentType_;

        // Walk stack from bottom (first pushed) to top (last pushed)
        for (const auto &pair : overlayStack_) {
            if (!isSkip(pair.first))
                return pair.first;
        }

        // Fallback: all states are overlays, return the bottom of the stack
        return overlayStack_.front().first;
    }

    void clearOverlays()
    {
        if (!headless_)
            std::cout << "[STATE] clearOverlays (was: " << stateLabel(currentType_) << ")\n";
        if (currentState_) {
            currentState_->onExit(*ctx_);
        }
        overlayStack_.clear();
    }

   private:
    ICerekaStateContext *ctx_ = nullptr;
    CerekaState currentType_ = CerekaState::Quit;
    ICerekaState *currentState_ = nullptr;
    std::vector<std::pair<CerekaState, ICerekaState *>> overlayStack_;
    std::unordered_map<CerekaState, std::unique_ptr<ICerekaState>> states_;
    bool headless_ = false;
};

}  // namespace cereka
#endif  // CEREKA_STATE_HPP
