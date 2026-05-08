# Settings, Pause Menu, Save/Load UX — Implementation Review

## What was built

### A. Persistent Settings (`settings.json`)
- **New file:** `src/cereka_settings_manager.hpp/.cpp`
- `SerializableSettings` struct with glaze JSON meta (textSpeed, bgmVolume, sfxVolume, autoAdvance, fullscreen, skipUnseen)
- `SettingsManager` class with `Load()`/`Save()`/`Apply()` methods
- Settings file lives at `{cwd}/settings.json`
- `Apply()` wires textSpeed → DialogueSystem::SetTextSpeed(), volume → AudioManager
- Loaded and applied during `Impl::InitGame()`

### B. Pause Menu Overlay
- **New state:** `PauseMenuState` (pushed as overlay on ESC)
- Buttons: Continue, Save, Load, Settings, Quit to Menu
- ESC to resume (pops overlay, restores gameplay state via overlay stack)
- ESC handler changed from `pushOverlay(SaveMenuState)` → `pushOverlay(PauseMenuState)`
- Save/Load/Settings from pause menu push their respective overlays on top

### C. Settings Menu Overlay
- **New state:** `SettingsMenuState` (pushed from pause menu)
- Shows 5 rows: Text Speed, BGM Volume, SFX Volume, Auto Advance, Skip Unseen
- Click a row to cycle settings values
- Settings are persisted to `settings.json` on change
- Settings are applied to engine subsystems immediately

### D. Save/Load UX Improvements
- **Scene metadata:** `SerializableSaveData::sceneDescription` — composed from bg filename + visible character count/IDs
- **Slot metadata display:** Shows timestamp AND scene description in save/load grid
- **Confirm overwrite dialog:** `ConfirmOverwriteState` — "Overwrite Slot N?" with Yes/Cancel
- **Empty slot guard:** `LoadMenuState` skips loading from empty slots

### E. Enabling Infrastructure Changes
- **DialogueSystem text speed:** Made `charsPerSecond` a settable field (was `static constexpr`)
- **AudioManager volume control:** Added `SetBgmVolume()`/`SetSfxVolume()` with gain scaling
- **effectiveState() fix:** Now walks overlay stack to find the actual gameplay state (skipping overlay states like pause, settings, confirm)
- **SlotMetadata struct:** Clean data transfer object for save slot summary info

## Files Changed (18 total)

### New Files:
- `src/cereka_settings_manager.hpp` — SettingsManager + SerializableSettings
- `src/cereka_settings_manager.cpp` — Load/Save/Apply implementations

### Modified Files:
- `include/Cereka/Cereka.hpp` — PauseMenuState, ConfirmOverwriteState, SettingsMenuState enum values
- `src/state/cereka_state.hpp` — stateLabel entries + effectiveState() fix
- `src/state/cereka_states.hpp` — 3 new state class declarations
- `src/state/cereka_states.cpp` — 3 new state implementations (extensive)
- `src/cereka_engine_impl.hpp` — SettingsManager member, pendingConfirmSlot_, updated method sigs
- `src/Cereka.cpp` — Init: register states + load/apply settings; ESC → PauseMenuState
- `src/cereka_dialogue_system.hpp` — Configurable text speed
- `src/cereka_dialogue_system.cpp` — Use configurable speed
- `src/cereka_audio_manager.hpp` — Volume control fields/methods
- `src/cereka_audio_manager.cpp` — Volume gain scaling in fades
- `src/cereka_save_data.hpp` — sceneDescription field, SlotMetadata struct
- `src/cereka_save.cpp` — Scene description composition, GetSlotMetadata, updated overlay
- `src/ui/ui_manager.hpp` — Pause/confirm overlay draw + hit-test declarations
- `src/ui/ui_manager.cpp` — Pause/confirm overlay implementations, updated save/load overlay

## Test Results
- 67/67 C++ unit tests passed
- 14/14 Lua compile snapshot tests passed
- Build: 0 errors, 0 warnings

## Design Decisions

1. **SettingsManager vs ConfigManager:** Settings are separate because they're user preferences (persistent, per-machine), while ConfigManager handles UI theme from .crka scripts (per-game, set by author).

2. **Overlay pattern:** All new UI (pause, confirm, settings) use the existing overlay stack. This is consistent with SaveMenuState/LoadMenuState/HistoryState and means state restoration is automatic.

3. **Text speed via DialogueSystem::SetTextSpeed():** The dialogue system already had Tick() with a fixed rate. Making it configurable was a 3-line change and keeps text rendering logic in one place.

4. **Volume via gain scaling:** AudioManager multiplies all BGM gain values by bgmVolume_. This works with the existing fade system (fades transition to the scaled target naturally).

5. **Non-persistent overlay detection in effectiveState():** The save system now walks the overlay stack to find the base gameplay state (Running/WaitingForInput etc.), skipping UI-only overlays. This fixes the edge case where saving from a pause→save sub-menu would record the wrong state.

## Known Limitations / Deferred
- **autoAdvance/skipUnseen** are stored in settings.json but not wired to engine behavior yet (requires update loop changes)
- **fullscreen** setting is stored but not applied (requires SDL window mode toggle)
- **Quit to Menu** in pause menu currently transitions to Finished state (ends game) rather than returning to a main menu
- SettingsManager currently no unit test coverage (manual verification only)
