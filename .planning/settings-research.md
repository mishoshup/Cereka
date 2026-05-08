# Settings, Pause Menu, and Save/Load UX — Research

## Codebase Architecture Summary

### State Machine (`src/state/`)
- `ICerekaState` interface with `onEnter`/`onExit`/`update`/`handleEvent`/`draw`
- `CerekaStateBase<T>` — CRTP base
- `CerekaStateMachine` — owns state map + overlay stack (`std::vector<pair<CerekaState, ICerekaState*>>`)
- `pushOverlay()`: saves current state to stack, switches to overlay state
- `popOverlay()`: exits current, restores previous from stack
- `effectiveState()`: returns state under top overlay (for save serialization)
- Concrete states in `cereka_states.hpp/.cpp`: Dialogue, WaitingForInput, Menu, Fade, SaveMenu, LoadMenu, History, Finished, Quit
- ESC is handled in `Impl::HandleEvent()` — currently opens SaveMenu directly during WaitingForInput/Running

### Config System (`src/config/`)
- `ConfigManager` with `PropertyDef`/`PropertyValue` + `ApplyContext`
- Property Map pattern — properties registered once in a table, applied by key
- Used for UI theming (`ui textbox color`, `ui button image`, etc.) — set from .crka scripts
- NOT appropriate for persistent user settings (text speed, volume, etc.)

### Save System (`src/cereka_save*.hpp/.cpp`)
- Uses **glaze JSON** (`glz::write_file_json` / `glz::read_file_json`)
- `SerializableSaveData` struct with glaze meta — version, timestamp, PC, variables, scene, audio, dialogue state
- `SaveGame(slot)` / `LoadGame(slot)` on `Impl`
- `GetSlotTimestamp(slot)` — reads just the timestamp field
- `DrawSaveLoadOverlay()` / `HitTestSaveSlot()` — delegated to UIManager

### Audio (`src/cereka_audio_manager.hpp/.cpp`)
- **No volume control** — uses raw `MIX_SetTrackGain(bgmTrack, 1.0f)` with no abstraction
- SFX via `MIX_PlayAudio(mixer, audio)` — no gain control at all
- BGM fade state machine (FadingIn, FadingOut, Crossfade)

### Dialogue System (`src/cereka_dialogue_system.hpp/.cpp`)
- Fixed `CHARS_PER_SECOND = 60.0f` — hardcoded constant, not configurable
- `Tick(float dt)` adds chars based on `typewriterTimer * CHARS_PER_SECOND`

### Draw Pipeline (`src/cereka_draw.cpp`)
- `Impl::Draw()`: Clear → DrawBackground → DrawCharacters → DrawSceneGraph → `m_stateMachine.draw()` → [if no overlays] DrawDialogueBox
- States' `draw()` methods are called via state machine; they draw their own overlay content
- The dialogue box is skipped when overlays are active (the `if (hasOverlays()) return` guard)

### Key Observations

1. **`file(GLOB_RECURSE)` on src/*.cpp** in `src/CMakeLists.txt` — no need to register new `.cpp` files, they're auto-discovered.

2. **`CerekaEngine`** (public) wraps `CerekaImpl` via pImpl. New features need methods on both if publicly exposed.

3. **ESC handling** is in `Impl::HandleEvent()`. Currently opens SaveMenu directly. This will need to open the pause menu instead.

4. **Settings** don't exist yet — no `settings.json`, no config file for user preferences.

5. **Fullscreen toggle** is set at init time in `video::create_window()` — no runtime toggle.

## Design Decisions

### Q1: Should settings be a separate manager class or part of ConfigManager?

**Decision: Separate `SettingsManager` struct/class.**

Rationale:
- `ConfigManager` is for UI theme properties set from `.crka` scripts — different domain entirely
- Settings are user preferences (persistent, loaded at startup, changed at runtime via UI)
- ConfigManager is key-value with serialized strings; Settings need typed accessors
- Better to keep concerns separate and have a focused class with glaze JSON serialization

### Q2: What settings actually matter for VN players?

Minimum viable set:
| Setting | Type | Default | Wired to |
|---|---|---|---|
| `textSpeed` | float (chars/sec) | 60.0 | DialogueSystem::charsPerSecond |
| `bgmVolume` | float (0.0–1.0) | 1.0 | AudioManager (BGM gain) |
| `sfxVolume` | float (0.0–1.0) | 1.0 | AudioManager (SFX gain) |
| `autoAdvance` | bool | false | Script tick / WaitingForInput flow |
| `fullscreen` | bool | false | Video toggle (future) |
| `skipUnseen` | bool | false | Script tick skip mode (future) |

For now, implement textSpeed, bgmVolume, sfxVolume as wired. autoAdvance, fullscreen, skipUnseen as stored-but-not-yet-wired (the settings panel exists, wiring comes later).

### Q3: Should the pause menu be a standalone state or an overlay?

**Decision: Overlay state** (`PauseMenuState`).

Rationale:
- The overlay stack already handles this perfectly — `pushOverlay` saves current state, `popOverlay` restores it
- No need to save/restore state manually
- Consistent with existing SaveMenuState, LoadMenuState, HistoryState patterns
- ESC handler in `Impl::HandleEvent()` changes from `pushOverlay(SaveMenuState)` to `pushOverlay(PauseMenuState)`

### Q4: Save/load UX improvements — highest impact with minimal code?

1. **Scene metadata in save slots**: Store `sceneDescription` string in `SerializableSaveData` — set during save from the current bg filename + visible character IDs. Display in slot labels.

2. **Confirm overwrite**: New `ConfirmOverwriteState` overlay that pushes on top of SaveMenuState. Shows "Overwrite Slot N?" with Yes/No buttons. Uses existing menu infrastructure? No — simpler to add a confirm flag pattern: SaveMenuState tracks `pendingSlot`, draws a confirmation dialog, and only saves on confirm.

3. **Slot metadata display**: Already partially done (timestamps shown). Add scene description field below each timestamp.

## Files to Create/Modify

### New Files
- `src/cereka_settings_manager.hpp` — SettingsManager class + SerializableSettings struct
- `src/cereka_settings_manager.cpp` — SettingsManager implementation

### Modified Files
- `src/state/cereka_states.hpp` — Add PauseMenuState, ConfirmOverwriteState
- `src/state/cereka_states.cpp` — Implement both states
- `src/cereka_engine_impl.hpp` — Add SettingsManager member, new state methods
- `src/cereka_ui_config.hpp` — UiConfig might need pause menu colors
- `src/Cereka.cpp` — Register new states, init settings, wire ESC
- `src/cereka_draw.cpp` — Pause menu rendering in draw pipeline
- `src/cereka_dialogue_system.hpp/.cpp` — Make text speed configurable
- `src/cereka_audio_manager.hpp/.cpp` — Add volume control
- `src/cereka_save_data.hpp` — Add sceneDescription to SerializableSaveData
- `src/cereka_save.cpp` — Save/load scene description
- `src/ui/ui_manager.hpp/.cpp` — Add pause menu draw, confirm overwrite draw
- `src/cereka_script.cpp` — Auto-advance in Update loop
- `include/Cereka/Cereka.hpp` — Add PauseMenuState to enum

### Not Modified
- `src/config/config_manager.hpp/.cpp` — Settings are separate concern
- `CMakeLists.txt` — `file(GLOB_RECURSE)` auto-discovers new .cpp files
