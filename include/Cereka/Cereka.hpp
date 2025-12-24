// VNEngine/VNEngine.hpp
#pragma once

#include "exceptions.hpp"
#include "vn_instruction.hpp"
#include <string>
namespace cereka {

struct CerekaEvent {
    enum Type { Quit, KeyDown, MouseDown, Unknown };
    Type type = Unknown;
    int key = 0;
    float mouseX = 0.f;
    float mouseY = 0.f;
};

enum class CerekaState { Running, WaitingForInput, InMenu, Finished };

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
    void AdvanceScriptOnce();
    void TickScript();

    void ExitMenu();
    void Reset();
    void HandleEvent(const CerekaEvent &e);
    void Update(float dt);
    void Draw();

    int HitTestButton(int mx,
                      int my);
    bool InMenu() const;                     // tambah
    const std::string &CurrentText() const;  // tambah

    size_t ButtonCount() const;
    size_t ProgramCounter() const;

    bool IsGameFinished() const;
    bool IsScriptFinished() const;
    bool IsFinished() const;

   private:
    class Implementation;
    Implementation *pImplementation;
};

}  // namespace cereka
