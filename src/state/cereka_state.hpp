#pragma once
// cereka_state.hpp — Cereka VN Engine State System
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
//   IVNState (interface)
//     └── VNState<T> (CRTP base)
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
#    include <memory>
#    include <string>
#    include <unordered_map>
#    include <utility>
#    include <vector>

namespace cereka {

// ============================================================================
// IVNStateContext — Interface for state → engine communication
// ============================================================================

/**
 * @brief Interface for VN states to request actions from the engine.
 *
 * States should never directly manipulate engine state. They request
 * changes through this interface, enabling loose coupling and testability.
 */
class IVNStateContext {
   public:
    virtual ~IVNStateContext() = default;

    virtual void changeState(CerekaState newState) = 0;
    virtual void pushOverlay(CerekaState overlayState) = 0;
    virtual void popOverlay() = 0;
    virtual CerekaState getSavedState() const = 0;
    virtual void setSavedState(CerekaState state) = 0;
};

// ============================================================================
// IVNState — Abstract base for all VN game states
// ============================================================================

/**
 * @brief Abstract base for all visual novel game states.
 *
 * Each state encapsulates:
 * - onEnter: Called when state becomes active
 * - onExit: Called when state becomes inactive
 * - update: Per-frame logic (dt = delta time in seconds)
 * - handleEvent: Input event processing
 * - draw: Per-frame rendering
 */
class IVNState {
   public:
    virtual ~IVNState() = default;

    [[nodiscard]] virtual CerekaState type() const = 0;

    virtual void onEnter(IVNStateContext &) {}
    virtual void onExit(IVNStateContext &) {}
    virtual void update(float dt,
                        IVNStateContext &)
    {
    }
    virtual void draw(IVNStateContext &) const {}
    virtual void handleEvent(const CerekaEvent &event,
                             IVNStateContext &)
    {
    }
};

// ============================================================================
// VNState — CRTP template for simple states
// ============================================================================

/**
 * @brief CRTP base class for states with compile-time type dispatch.
 *
 * Usage:
 *   class DialogueState : public VNState<CerekaState::Dialogue> {
 *       void onEnter(IVNStateContext& ctx) override { ... }
 *   };
 */
template<CerekaState T> class VNState : public IVNState {
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

    void setContext(IVNStateContext &ctx)
    {
        ctx_ = &ctx;
    }

    template<typename StateT> void registerState()
    {
        static_assert(std::is_base_of_v<IVNState, StateT>, "StateT must derive from IVNState");
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

        currentState_->onExit(*ctx_);
        currentState_ = nullptr;

        auto it = states_.find(newType);
        if (it != states_.end()) {
            currentType_ = newType;
            currentState_ = it->second.get();
            currentState_->onEnter(*ctx_);
        }
    }

    void pushOverlay(CerekaState overlayType)
    {
        if (!ctx_)
            return;

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

    [[nodiscard]] bool hasOverlays() const
    {
        return !overlayStack_.empty();
    }

   private:
    IVNStateContext *ctx_ = nullptr;
    CerekaState currentType_ = CerekaState::Running;
    IVNState *currentState_ = nullptr;
    std::unordered_map<CerekaState, std::unique_ptr<IVNState>> states_;
    std::vector<std::pair<CerekaState, IVNState *>> overlayStack_;
};

}  // namespace cereka
#endif  // CEREKA_STATE_HPP
