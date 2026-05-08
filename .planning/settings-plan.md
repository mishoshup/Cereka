# Implementation Plan: Settings + Pause Menu + Save/Load UX

## Overview

Add three features that share UI infrastructure:
- **A.** Persistent settings (`settings.json`) via SettingsManager
- **B.** Pause menu overlay (PauseMenuState)
- **C.** Save/load UX improvements (scene metadata, confirm overwrite)

All changes are in `src/`. No CMakeLists.txt changes needed (file(GLOB_RECURSE)).

## Order of Implementation

### 1. Add PauseMenuState + ConfirmOverwriteState to CerekaState enum (blocker for everything else)
**Files:** `include/Cereka/Cereka.hpp`
- Add `PauseMenuState`, `ConfirmOverwriteState` to `CerekaState` enum
**Files:** `src/state/cereka_state.hpp`
- Add `PauseMenuState`, `ConfirmOverwriteState` to `stateLabel()` switch

### 2. Make DialogueSystem text speed configurable
**Files:** `src/cereka_dialogue_system.hpp`, `src/cereka_dialogue_system.cpp`
- Change `static constexpr float CHARS_PER_SECOND = 60.0f` → `float charsPerSecond = 60.0f;`
- Add `SetTextSpeed(float cps)`, `GetTextSpeed()`
- Use `charsPerSecond` in `Tick()` instead of constant

### 3. Add volume control to AudioManager
**Files:** `src/cereka_audio_manager.hpp`, `src/cereka_audio_manager.cpp`
- Add `float bgmVolume_ = 1.0f` and `float sfxVolume_ = 1.0f`
- Add `SetBgmVolume(float)`, `GetBgmVolume()`, `SetSfxVolume(float)`, `GetSfxVolume()`
- In `PlayBGM()`: apply `bgmVolume_` to initial gain
- In `Update()`: scale target gain by `bgmVolume_`
- Track un-scaled BGM gain for correct volume scaling

### 4. Create SettingsManager
**Files:** `src/cereka_settings_manager.hpp`, `src/cereka_settings_manager.cpp`
- `SettingsManager` struct with glaze-serialized fields:
  - `float textSpeed = 60.0f`
  - `float bgmVolume = 1.0f`
  - `float sfxVolume = 1.0f`
  - `bool autoAdvance = false`
  - `bool fullscreen = false`
  - `bool skipUnseen = false`
- `Load()` from `settings.json`, `Save()` to `settings.json`
- `Apply()` method to wire values to subsystems
- Include in `cereka_engine_impl.hpp`

### 5. Add scene description to save slots
**Files:** `src/cereka_save_data.hpp`, `src/cereka_save.cpp`
- Add `std::string sceneDescription` to `SerializableSaveData`
- In `SaveGame()`: compose scene description from bg path + visible chars
- In `GetSlotTimestamp()`: return it alongside timestamp (need new method)

### 6. Add pause menu drawing to UIManager
**Files:** `src/ui/ui_manager.hpp`, `src/ui/ui_manager.cpp`
- Add `DrawPauseOverlay()` — semi-transparent dim + panel + buttons
- Add hit test method for pause menu buttons

### 7. Add confirm overwrite drawing to UIManager
**Files:** `src/ui/ui_manager.hpp`, `src/ui/ui_manager.cpp`
- Add `DrawConfirmOverwriteDialog()` — "Overwrite Slot N?" with Yes/No

### 8. Implement PauseMenuState
**Files:** `src/state/cereka_states.hpp`, `src/state/cereka_states.cpp`
- New state class with `handleEvent` and `draw`
- Buttons: Continue, Save, Load, Settings, Quit
- Save → push SaveMenuState overlay on top
- Load → push LoadMenuState overlay on top
- Settings → push SettingsMenuState (or just placeholder for now)
- Quit → changeState(Quit)
- Continue/ESC → popOverlay

### 9. Implement ConfirmOverwriteState
**Files:** `src/state/cereka_states.hpp`, `src/state/cereka_states.cpp`
- Follows same pattern as other overlay states
- Tracks `pendingSlot` and `isSaving` flag
- Yes → execute save, pop both confirm and save overlays
- No → pop just confirm overlay

### 10. Wire everything in Cereka.cpp and engine_impl.hpp
**Files:** `src/Cereka.cpp`, `src/cereka_engine_impl.hpp`
- Register PauseMenuState, ConfirmOverwriteState in InitGame
- Init SettingsManager in InitGame
- Change ESC handler: push PauseMenuState instead of SaveMenuState
- Wire SettingsManager.Apply() connection

### 11. Update save/load overlay to show scene metadata
**Files:** `src/ui/ui_manager.cpp`
- Modify `DrawSaveLoadOverlay` to show scene description and timestamp layout

## Verification
1. `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build -j12`
2. `ninja -C build cereka_test && ./build/tests/cereka_test`
3. `lua tests/compile/harness.lua`
