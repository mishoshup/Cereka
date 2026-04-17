// Cereka/Cereka.hpp — public engine API
#pragma once

#include "compiler/vn_instruction.hpp"
#include <string>

namespace cereka {

struct CerekaEvent {
    enum Type { Quit, KeyDown, MouseDown, Unknown };
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
    LoadMenuState
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
                  bool fullscreen = false);
    void ShutDown();

    bool PollEvent(CerekaEvent &e);
    void Present();

    int Width() const;
    int Height() const;

    void LoadCompiledScript(const std::vector<scenario::Instruction> &compiled);
    void LoadScript(const std::string &filename);
    void TickScript();

    void Reset();
    void HandleEvent(const CerekaEvent &e);
    void Update(float dt);
    void Draw();

    bool InMenu() const;
    const std::string &CurrentText() const;
    size_t ButtonCount() const;
    size_t ProgramCounter() const;

    bool IsGameFinished() const;
    bool IsGameQuit() const;
    bool IsFinished() const;

    bool SaveGame(int slot);
    bool LoadGame(int slot);

   private:
    CerekaImpl *pImplementation;
};

}  // namespace cereka
