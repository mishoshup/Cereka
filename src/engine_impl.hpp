#pragma once
// engine_impl.hpp — private CerekaImpl class shared across all engine .cpp files.
// Do NOT include this from public headers.

#include "Cereka/Cereka.hpp"
#include "Cereka/exceptions.hpp"
#include "audio_manager.hpp"
#include "config/config_manager.hpp"
#include "dialogue_system.hpp"
#include "menu_system.hpp"
#include "scene_manager.hpp"
#include "script_interpreter.hpp"
#include "text_renderer.hpp"
#include "ui_config.hpp"
#include "video.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace cereka {

class CerekaImpl {
   public:
    // --- Window / renderer ---
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    int screenWidth = 0;
    int screenHeight = 0;

    // --- Font ---
    TTF_Font *font = nullptr;
    std::string fontPath;  // path of the loaded font file (for reloading on size change)

    // --- Scene state ---
    SceneManager scene;

    // --- Audio ---
    AudioManager audio;

    // --- Script interpreter ---
    ScriptInterpreter scriptInterpreter;

    // --- Dialogue ---
    DialogueSystem dialogue;

    // --- Menu ---
    MenuSystem menu;

    // --- State machine ---
    CerekaState state = CerekaState::Running;
    CerekaState stateBeforeSaveMenu = CerekaState::Running;  // restored when overlay closes

    // --- UI theme ---
    UiConfig uiCfg;
    config::ConfigManager configManager;

    // -----------------------------------------------------------------------
    // Methods — defined across the engine .cpp files
    // -----------------------------------------------------------------------

    // Cereka.cpp
    bool InitGame(const char *title,
                  int width,
                  int height,
                  bool fullscreen);
    void ShutDown();
    bool PollEvent(CerekaEvent &e);
    void Present();
    SDL_Renderer *CreateBestRenderer(SDL_Window *win);
    SDL_Texture *RenderText(const std::string &text,
                            SDL_Color color);
    void Say(const std::string &speaker,
             const std::string &name,
             const std::string &text);
    void Narrate(const std::string &text);
    std::string SubstituteVariables(const std::string &text);
    void EnterMenu();
    void ExitMenu();
    void HandleEvent(const CerekaEvent &e);

    // script_vm.cpp
    void TickScript();
    void Update(float dt);
    void LoadCompiledScript(const std::vector<scenario::Instruction> &compiled);
    void LoadScript(const std::string &filename);
    void Reset();

    // draw.cpp
    void Draw();

    // save.cpp
    bool SaveGame(int slot);
    bool LoadGame(int slot);
    std::string GetSlotTimestamp(int slot);
    void DrawSaveLoadOverlay(bool isSaving);
    int HitTestSaveSlot(int mx,
                        int my);

    // ui_config.cpp
    void ApplyUiSet(const std::string &key,
                    const std::string &val);
    void InitConfigManager();
    void LoadFont(int size);
};

}  // namespace cereka

// Shorthand for all engine implementation .cpp files
using Impl = cereka::CerekaImpl;
