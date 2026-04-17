#pragma once
// engine_impl.hpp — private CerekaImpl class shared across all engine .cpp files.
// Do NOT include this from public headers.

#include "Cereka/Cereka.hpp"
#include "Cereka/exceptions.hpp"
#include "config/config_manager.hpp"
#include "text_renderer.hpp"
#include "ui_config.hpp"
#include "video.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <filesystem>
#include <iostream>
#include <sol/sol.hpp>
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
    SDL_Texture *background = nullptr;
    std::string bgPath;  // filename passed to ShowBackground (for save/load)

    struct CharacterEntry {
        SDL_Texture *tex;
        float xNorm;  // 0.0–1.0 horizontal centre
    };
    std::unordered_map<std::string, CharacterEntry> characters;
    std::unordered_map<std::string, std::string> charPaths;  // id → filename (for save/load)

    // --- Audio ---
    bool audioInitialized = false;
    MIX_Mixer *mixer = nullptr;
    MIX_Audio *bgmAudio = nullptr;
    MIX_Track *bgmTrack = nullptr;
    std::string bgmPath;  // filename passed to PlayBGM (for save/load)
    std::unordered_map<std::string, MIX_Audio *> sfxCache;

    // --- Script VM ---
    sol::state lua;
    sol::coroutine script;
    std::vector<scenario::Instruction> program;
    std::unordered_map<std::string, size_t> labelMap;
    std::unordered_map<std::string, std::string> variables;
    std::unordered_map<std::string, float> numVariables;
    std::vector<size_t> callStack;  // for CALL / RETURN subroutines
    size_t pc = 0;
    bool scriptFinished = false;

    // Skip-mode: inside a false if/endif block
    bool skipMode = false;
    int skipDepth = 0;

    // --- Dialogue ---
    std::string currentSpeaker;
    std::string currentName;
    std::string currentText;
    float typewriterTimer = 0.0f;
    int displayedChars = 0;
    static constexpr float CHARS_PER_SECOND = 60.0f;

    // --- Fade transition ---
    enum class FadePhase { None, Out, In };
    FadePhase fadePhase = FadePhase::None;
    float fadePhaseDuration = 0.25f;  // seconds per half-phase
    float fadeTimer = 0.0f;
    SDL_Texture *pendingBg = nullptr;

    // --- Menu ---
    bool inMenu = false;
    std::vector<std::string> buttonTexts;
    std::vector<std::string> buttonTargets;
    std::vector<bool> buttonExits;
    size_t menuEndPC = 0;

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
    SDL_Texture *LoadTexture(const std::string &filename);
    SDL_Texture *RenderText(const std::string &text,
                            SDL_Color color);
    void ShowBackground(const std::string &filename);
    void ShowCharacter(const std::string &id,
                       const std::string &filename,
                       const std::string &pos);
    void HideCharacter(const std::string &id);
    void Say(const std::string &speaker,
             const std::string &name,
             const std::string &text);
    void Narrate(const std::string &text);
    std::string SubstituteVariables(const std::string &text);
    void EnterMenu();
    void ExitMenu();
    int HitTestButton(int mx,
                      int my);
    static float posToXNorm(const std::string &pos);
    void HandleEvent(const CerekaEvent &e);

    // script_vm.cpp
    void TickScript();
    void Update(float dt);
    void LoadCompiledScript(const std::vector<scenario::Instruction> &compiled);
    void LoadScript(const std::string &filename);
    void Reset();

    // draw.cpp
    void Draw();

    // audio.cpp
    void PlayBGM(const std::string &filename);
    void StopBGM();
    void PlaySFX(const std::string &filename);

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
