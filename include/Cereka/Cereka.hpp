// Cereka/Cereka.hpp — public engine API
#pragma once

#include "compiler/cereka_instruction.hpp"
#include <string>

namespace cereka {

struct CerekaEvent {
    enum Type { Quit, KeyDown, MouseDown, MouseMove, Unknown };
    Type type = Unknown;
    int key = 0;
    float mouseX = 0.f;
    float mouseY = 0.f;
};

enum class CerekaState {
    Running,
    WaitingForInput,
    InMenu,
    Fading,
    Finished,
    Quit,
    SaveMenuState,
    LoadMenuState,
    HistoryState,
    PauseMenuState,
    ConfirmOverwriteState,
    SettingsMenuState
};

// Forward-declared here; fully defined in src/engine_impl.hpp (private)
class CerekaImpl;

class CerekaEngine {
   public:
    CerekaEngine();
    ~CerekaEngine();

    bool InitGame(const char *title,
                  int w,
                  int h,
                  bool fullscreen = false,
                  bool headless = false);
    void ShutDown();

    bool PollEvent(CerekaEvent &e);
    void Present();

    int Width() const;
    int Height() const;

    void LoadCompiledCerekaScript(const std::vector<compiler::Instruction> &compiled);
    void LoadCerekaScript(const std::string &filename);

    void Reset();
    void HandleEvent(const CerekaEvent &e);
    void Update(float dt);
    void Draw();

    bool InMenu() const;
    const std::string &CurrentText() const;
    size_t ButtonCount() const;
    size_t ProgramCounter() const;
    CerekaState CurrentState() const;
    bool SelectMenuOption(int idx);
    std::vector<std::string> ButtonLabels() const;

    bool IsGameFinished() const;
    bool IsGameQuit() const;
    bool IsFinished() const;

    bool SaveGame(int slot);
    bool LoadGame(int slot);

   private:
    CerekaImpl *pImplementation;
};

}  // namespace cereka
