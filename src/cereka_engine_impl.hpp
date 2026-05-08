#pragma once
// engine_impl.hpp — private CerekaImpl class shared across all engine .cpp files.
// Do NOT include this from public headers.

#include "Cereka/Cereka.hpp"
#include "Cereka/exceptions.hpp"
#include "cereka_audio_manager.hpp"
#include "cereka_rollback_manager.hpp"
#include "cereka_save_data.hpp"
#include "cereka_settings_manager.hpp"
#include "config/config_manager.hpp"
#include "ui/ui_manager.hpp"
#include "cereka_dialogue_system.hpp"
#include "cereka_menu_system.hpp"
#include "cereka_scene_manager.hpp"
#include "cereka_script_interpreter.hpp"
#include "renderer/irender_context.hpp"
#include "state/cereka_state.hpp"
#include "cereka_text_renderer.hpp"
#include "cereka_ui_config.hpp"
#include "cereka_video.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace cereka {

class CerekaImpl : public ICerekaStateContext {
   public:
    // --- Window / renderer ---
    SDL_Window *window = nullptr;
    std::unique_ptr<IRenderContext> m_renderCtx;
    int screenWidth = 0;
    int screenHeight = 0;

    // --- Font ---
    TTF_Font *font = nullptr;
    std::string fontPath;  // path of the loaded font file (for reloading on size change)

    // --- Scene state ---
    SceneManager scene;

    // --- Audio ---
    AudioManager audio;

    // --- UI ---
    UIManager ui;

    // --- Script interpreter ---
    ScriptInterpreter scriptInterpreter;

    // --- Dialogue ---
    DialogueSystem dialogue;

    // --- Menu ---
    MenuSystem menu;

    // --- State machine ---
    CerekaStateMachine m_stateMachine;

    // --- Rollback ---
    RollbackManager rollbackManager;

    // --- UI theme ---
    UiConfig uiCfg;
    cereka::config::ConfigManager configManager;

    // -----------------------------------------------------------------------
    // ICerekaStateContext — state machine interface
    // -----------------------------------------------------------------------
    void changeState(CerekaState newState) override;
    void pushOverlay(CerekaState overlayState) override;
    void popOverlay() override;

    // -----------------------------------------------------------------------
    // Methods — defined across the engine .cpp files
    // -----------------------------------------------------------------------

    // Cereka.cpp
    bool InitGame(const char *title,
                  int width,
                  int height,
                  bool fullscreen,
                  bool headless = false);
    void ShutDown();
    bool PollEvent(CerekaEvent &e);
    void Present();
    void Say(const std::string &speaker,
             const std::string &name,
             const std::string &text);
    void Narrate(const std::string &text);
    std::string SubstituteVariables(const std::string &text);
    void EnterMenu();
    void ExitMenu();
    void HandleEvent(const CerekaEvent &e);

    // script_vm.cpp
    void Update(float dt);
    void LoadCompiledCerekaScript(const std::vector<compiler::Instruction> &compiled);
    void LoadCerekaScript(const std::string &filename);
    void Reset();

    // draw.cpp
    void Draw();

    // save.cpp
    bool SaveGame(int slot);
    bool LoadGame(int slot);
    SlotMetadata GetSlotMetadata(int slot);
    void DrawSaveLoadOverlay(bool isSaving);
    int HitTestSaveSlot(int mx,
                        int my);
    int historyHitTest(int mx,
                       int my);

    // settings_manager.cpp
    SettingsManager settingsManager;

    // Temporary state for confirm-overwrite dialog
    int pendingConfirmSlot_ = -1;

    // test.cpp
    CerekaState CurrentState() const { return m_stateMachine.currentType(); }
    bool SelectMenuOption(int idx);
    std::vector<std::string> ButtonLabels() const { return menu.Texts(); }

    // ui_config.cpp
    void ApplyUiSet(const std::string &key,
                    const std::string &val);
    void InitConfigManager();
    void LoadFont(int size);
};

}  // namespace cereka

// Shorthand for all engine implementation .cpp files
using Impl = cereka::CerekaImpl;
