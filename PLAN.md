# Cereka - Visual Novel Engine Development Plan

## Philosophy

- **Custom script only** (`.crka`) — no external languages
- **Sensible defaults** — works great out of the box
- **Highly customizable** — power users can override/tweak everything
- **Enterprise maintainability** — scalable, testable, extensible for mobile (Android/iOS)
- Goal: As powerful as Ren'Py but custom script only

---

## Architecture Overview

### Design Principles

1. **Modularity**: Each system (state, config, input) is isolated and testable
2. **Abstraction**: Platform-specific code behind interfaces
3. **Configuration-driven**: Sensible defaults, everything overridable via `ui.crka`
4. **Testable**: Unit tests for core logic

### Directory Structure (Target)

```
src/
├── Cereka/                  # Public API headers
├── engine/                  # Core engine (platform-agnostic)
│   ├── engine.hpp          # Main engine class
│   ├── engine.cpp
│   └── impl/               # Implementation details
│       ├── scene_state.hpp    # Scene (bg, characters)
│       ├── dialog_state.hpp    # Dialogue/text state
│       ├── menu_state.hpp      # Menu/choices
│       ├── audio_state.hpp     # Audio manager
│       └── vm_state.hpp        # Script VM state
├── state/                  # State machine system
│   ├── state_machine.hpp
│   ├── i_state.hpp         # State interface
│   ├── running_state.cpp
│   ├── waiting_state.cpp
│   ├── menu_state.cpp
│   ├── fade_state.cpp
│   └── overlay_state.cpp   # Save/load menus
├── config/                 # Configuration system
│   ├── config_manager.hpp
│   └── property_parsers.hpp
├── input/                  # Input abstraction
│   ├── input_manager.hpp   # Unified input interface
│   └── input_desktop.cpp   # Keyboard/mouse
├── events/                 # Event system
│   └── event_bus.hpp
├── platform/               # Platform abstraction (placeholder)
│   └── platform.hpp
├── renderer/               # Rendering system
│   └── draw_manager.hpp
├── compiler/              # Script compiler
│   ├── vn_instruction.hpp
│   └── compiler.lua        # Embedded Lua compiler
└── tests/                  # Unit tests
    ├── config_test.cpp
    ├── state_machine_test.cpp
    └── vm_test.cpp
```

---

## Phase 0: Refactoring (Foundation)

> **Critical**: Complete before adding features to prevent technical debt.

### 0.1 Config System Refactor

**Problem**: `ApplyUiSet()` if-else chain doesn't scale with new properties.

**Solution**: Property Map Pattern with validation.

```cpp
namespace config {

struct PropertyDescriptor {
    std::string key;
    std::string description;
    std::function<void(const std::string&)> apply;
};

class ConfigManager {
    std::vector<PropertyDescriptor> properties;
public:
    void registerProperty(PropertyDescriptor prop);
    void apply(const std::string& key, const std::string& val);
    void serialize(std::ostream& os);
    void deserialize(std::istream& is);
};

} // namespace config
```

**Benefits**:
- Self-documenting (description field)
- Compile-time validation
- Easy to add new properties
- Serializable for save files

**Implementation**: Replace `ApplyUiSet()` with `ConfigManager::registerProperty()` calls.

---

### 0.2 State Machine Refactor

**Problem**: `stateBeforeSaveMenu` antipattern, monolithic `HandleEvent()`.

**Solution**: Full Hierarchical State Machine (HSM) pattern.

```cpp
namespace state {

class IState {
public:
    virtual ~IState() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void update(float dt) = 0;
    virtual void handleEvent(const CerekaEvent& e) = 0;
    virtual void draw() = 0;
    virtual CerekaState getType() const = 0;
};

class StateMachine {
    std::vector<std::unique_ptr<IState>> stateStack; // For overlays

public:
    void push(std::unique_ptr<IState> state);   // Overlay states
    void replace(std::unique_ptr<IState> state); // Transitions
    void pop();                                  // Return from overlay
    void update(float dt);
    void handleEvent(const CerekaEvent& e);
    void draw();
    IState* current() const;
};

} // namespace state
```

**State implementations**:
| State | Description |
|-------|-------------|
| `RunningState` | Normal dialogue execution |
| `WaitingState` | Waiting for user to advance |
| `MenuState` | Choice menu displayed |
| `FadeState` | Background/character transitions |
| `SaveMenuState` | Save overlay (stackable) |
| `LoadMenuState` | Load overlay (stackable) |
| `FinishedState` | Script ended naturally |
| `QuitState` | User closed window |

**Benefits**:
- Each state isolated with enter/exit hooks
- Overlay states naturally stack
- No more `stateBeforeSaveMenu` hack
- Easy to add new states

---

### 0.3 Testing Framework

**Setup**: GoogleTest (gtest)

```cmake
# tests/CMakeLists.txt
add_executable(cereka_test
    config_test.cpp
    state_machine_test.cpp
    vm_test.cpp
)
target_link_libraries(cereka_test PRIVATE Cereka gtest)
include(GoogleTest)
gtest_discover_tests(cereka_test)
```

**Test coverage**:
| Component | Tests |
|-----------|-------|
| ConfigManager | Property registration, parsing, serialization |
| StateMachine | State transitions, push/pop, event handling |
| ScriptVM | Instruction execution, variables, labels |
| Compiler | Lua parsing, instruction generation |

---

### 0.4 Component Split

**Problem**: `CerekaImpl` is a god class with 40+ members.

**Solution**: Split into focused components.

```
engine/impl/
├── scene_state.hpp      # background, characters
├── dialog_state.hpp     # currentText, typewriter, speaker
├── menu_state.hpp       # buttons, targets
├── audio_state.hpp      # BGM, SFX tracks
└── vm_state.hpp         # program, pc, variables, labels
```

**Benefits**:
- Clear ownership of data
- Easier to test individual components
- Parallel development

---

### 0.5 Event Bus

**Problem**: Hard-coded event flow between systems.

**Solution**: Decoupled event system.

```cpp
class EventBus {
    std::vector<std::function<void(const CerekaEvent&)>> listeners;
public:
    void subscribe(std::function<void(const CerekaEvent&)> handler);
    void emit(const CerekaEvent& e);
    void unsubscribeAll();
};
```

**Usage**:
```cpp
eventBus.subscribe([](const CerekaEvent& e) {
    if (e.type == CerekaEvent::KeyDown && e.key == config.advanceKeys[0])
        stateMachine.current()->onAdvance();
});
```

**Benefits**:
- Decouples input from logic
- Mobile touch events can emit same bus
- Easy to add keybindings

---

### 0.6 Platform Abstraction (Placeholder)

**Purpose**: Prepare for Android/iOS.

```cpp
namespace platform {

class IPlatform {
public:
    virtual ~IPlatform() = default;
    virtual std::string getStoragePath() = 0;
    virtual std::string getAssetPath() = 0;
    virtual void vibrate(int ms) = 0;
    virtual void showKeyboard() = 0;
    // Future: getScreenDPI(), getDeviceId(), etc.
};

class PlatformDesktop : public IPlatform { /* ... */ };
class PlatformAndroid : public IPlatform { /* TODO */ };
class PlatformIOS : public IPlatform { /* TODO */ };

} // namespace platform
```

**Note**: Desktop implementation only for now. Mobile is future.

---

## Phase 1: Foundation Features

> **Note**: Phase 0 must be complete before these.

### 1.1 Audio: 4-Channel Mixer

- Channels: `bgm`, `voice`, `sfx`, `master`
- Volume: 0.0 to 1.0
- Preferences persist across sessions

### 1.2 Audio: Fade In/Out

```
bgm music.ogg fade 2.0
stop_bgm fade 1.0
```

### 1.3 Voice Lines

```
voice "alice_01.ogg"
say alice "Hello!"
```

### 1.4 wait/pause

```
wait 2.0              ; wait N seconds
wait for voice        ; wait for voice line to finish
```

---

## Phase 2: ATL Core

### 2.1 ATL Transforms (Characters)

```
char eileen happy center with dissolve
char alice surprised left with fade
char bob thinking right with ease-in move
```

### 2.2 ATL Transforms (Backgrounds)

```
bg park with dissolve
bg beach with flash
```

### 2.3 ATL Transforms (Screens)

```
show screen quick_menu with slideawayleft
hide screen settings with slideawayright
```

---

## Phase 3: Text & UI

### 3.1 Text Markup

```
say alice "{b}Bold{/b}"
say bob "{i}Italic{/i}"
say cathy "{color=ff0000}Red{/color}"
say dave "{cps=30}Slow{/cps}"
```

### 3.2 Screen System Core

```
screen settings:
    frame:
        align (0.5, 0.5)
        vbox:
            label "Settings"
            slider value Preference("bgm_volume")
```

### 3.3 Screen Widgets

- `frame`, `vbox`, `hbox`, `label`, `textbutton`, `slider`, `toggle`, `text`

### 3.4 Built-in Screens

- `quick_menu`, `settings`, `history`, `save`, `load`

---

## Phase 4: Polish

### 4.1 Rollback

- Click dialogue history to rewind state
- Restores: pc, variables, bg, characters

### 4.2 Skip Mode

```
skip_mode                 ; skip seen dialogue
skip_mode all            ; skip all (including unseen)
```

### 4.3 Auto-Forward

```
ui auto_delay 3.0         ; seconds between advancing
```

### 4.4 Subpositions

```
char eileen center at 0.3
char alice left at 0.1
```

---

## Phase 5: Mini-Games

> **Vision**: Complex rendering under the hood, simple `.crka` scripting on top.
> Game authors write scripts, engine handles the complexity.

### 5.1 Philosophy

- **Author-facing**: Pure `.crka` scripting, no C++ needed
- **Engine-facing**: Sophisticated rendering/game systems
- **Progressive complexity**: Start simple, add capabilities

### 5.2 Mini-Game Types

#### Simple (Stat-Based) - Already possible
```
; Stat management, branching narratives
$health = $health - 10
if $health <= 0 goto game_over
```

#### Turn-Based - Variables + Choices
```
; RPG combat
menu
    button "Attack" goto attack
    button "Defend" goto defend
    button "Use Item" goto use_item
```

#### Timing-Based - Requires timing system
```
; Rhythm/timing mini-game
minigame timing
    beats "beat1.ogg", "beat2.ogg", "beat3.ogg"
    window 0.5
    on_hit goto hit_success
    on_miss goto hit_fail
```

#### Spatial - Requires canvas/position system
```
; Click/drag mini-game
minigame spatial
    targets item1, item2, item3
    on_click item1 goto item1_clicked
    on_drag item2 target_zone goto item2_success
```

#### Tile-Based - Requires tile system
```
; Puzzle matching
minigame tiles
    grid 4x4
    tiles "red.png", "blue.png", "green.png"
    match 3
    on_match goto match_success
```

### 5.3 Script API (Proposed)

```crka
; Define a mini-game
minigame my_game:
    type timing           ; timing | spatial | tiles | turnbased
    music "bgm.ogg"
    difficulty 3          ; 1-5

    ; Type-specific config
    beats "beat1.ogg", "beat2.ogg"
    window 0.5            ; timing window in seconds

; Start mini-game
start minigame my_game

; Mini-game callbacks
label hit_success
narrate "Perfect!"
jump continue_story

label hit_fail
narrate "Miss!"
jump continue_story

; Conditional mini-game (optional)
minigame if $has_timing_game
    type timing
    beats "beat.ogg"
end
```

### 5.4 Rendering Pipeline

```
MiniGameEngine
├── Canvas (2D drawing API)
│   ├── shapes (rect, circle, line)
│   ├── sprites (animated)
│   └── particles (effects)
├── TileMap (grid-based)
│   ├── tile layers
│   ├── collision detection
│   └── camera (pan/zoom)
├── SpriteAnimator
│   ├── sprite sheets
│   ├── keyframe animations
│   └── skeletal (future)
└── Physics (lightweight)
    ├── AABB collision
    ├── velocity/acceleration
    └── gravity (optional)
```

### 5.5 Implementation Order

| # | Component | Description |
|---|-----------|-------------|
| 5.1 | Canvas API | Basic 2D drawing (shapes, sprites) |
| 5.2 | MiniGame State | Integrate mini-game into state machine |
| 5.3 | Timing System | Beat/timing mini-game support |
| 5.4 | Spatial System | Click/drag mini-game support |
| 5.5 | Tile Map | Grid-based puzzle support |
| 5.6 | Physics | Basic physics (collision, velocity) |

### 5.6 Example: Timing Mini-Game

```crka
; Simple rhythm game
minigame rhythm_test:
    type timing
    music "rhythm.ogg"
    difficulty 2

    beats "beat1.wav", "beat2.wav", "beat3.wav", "beat4.wav"
    window 0.3

label start_rhythm
narrate "Press when the beats align!"
start minigame rhythm_test

label perfect
$score = $score + 100
narrate "Perfect! Score: {$score}"
jump start_rhythm

label good
$score = $score + 50
narrate "Good! Score: {$score}"
jump start_rhythm

label miss
$lives = $lives - 1
if $lives <= 0 goto game_over
narrate "Miss! Lives: {$lives}"
jump start_rhythm

label game_over
narrate "Game Over! Final Score: {$score}"
end
```

---

## Current Status

### Completed

| # | Feature |
|---|---------|
| ✅ | Variables: Numeric + arithmetic |
| ✅ | advance_keys configuration |
| ✅ | Window close handling (Quit state) |
| ✅ | BGM looping |

### In Progress

| # | Feature |
|---|---------|
| 🔄 | Phase 0: Config System Refactor |

### Pending

| # | Feature |
|---|---------|
| ⏳ | Audio: 4-channel mixer |
| ⏳ | Audio: Fade in/out |
| ⏳ | Voice lines |
| ⏳ | wait/pause |
| ⏳ | State Machine Refactor |
| ⏳ | Testing Framework |
| ⏳ | Component Split |
| ⏳ | Event Bus |
| ⏳ | Platform Abstraction |
| ⏳ | ATL transforms |
| ⏳ | Text markup |
| ⏳ | Screen system |
| ⏳ | Built-in screens |
| ⏳ | Rollback |
| ⏳ | Skip mode |
| ⏳ | Auto-forward |
| ⏳ | Phase 5: Mini-Games (Canvas API, Timing, Spatial, Tiles) |

---

## Technical Notes

### Implementation Order

```
Phase 0 (Foundation) → Phase 1 (Audio) → Phase 2 (ATL) → Phase 3 (UI) → Phase 4 (Polish)
```

### Lua Compiler Strategy

- **Current**: Embedded in binary at build time
- **Rationale**: Simple, single binary, protected scripts
- **Future**: Optional pre-compilation to bytecode for performance (when needed)

### Screen Rendering

- Screens are renderable nodes (same base as sprite/background/character)
- ATL transforms apply universally
- Z-order: backgrounds < characters < screens < overlays

### File Structure

```
game/
├── game.cfg
├── ui.crka              ; UI definitions (include in main)
├── main.crka
├── screens/
│   ├── settings.crka
│   ├── confirm.crka
│   └── ...
└── assets/...
```

### Include System

```
include screens/settings.crka
include screens/confirm.crka
```
