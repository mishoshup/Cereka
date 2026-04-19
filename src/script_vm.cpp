// script_vm.cpp — CerekaImpl script VM methods: TickScript, Update,
// HandleEvent, script loading. Expression evaluation and the variable
// state container live in script_interpreter.{hpp,cpp}.

#include "engine_impl.hpp"
#include <algorithm>

// ---------------------------------------------------------------------------
// Script loading
// ---------------------------------------------------------------------------

void Impl::LoadCompiledScript(const std::vector<scenario::Instruction> &compiled)
{
    scriptInterpreter.program = compiled;
    scriptInterpreter.pc = 0;
    scriptInterpreter.scriptFinished = false;
    scriptInterpreter.variables.clear();
    scriptInterpreter.numVariables.clear();
    scriptInterpreter.callStack.clear();
    scriptInterpreter.skipMode = false;
    scriptInterpreter.skipDepth = 0;

    scriptInterpreter.labelMap.clear();
    for (size_t i = 0; i < scriptInterpreter.program.size(); ++i)
        if (scriptInterpreter.program[i].op == scenario::Op::LABEL)
            scriptInterpreter.labelMap[scriptInterpreter.program[i].a] = i;
}

void Impl::LoadScript(const std::string &filename)
{
    sol::load_result chunk = scriptInterpreter.lua.load_file(filename);
    if (!chunk.valid()) {
        sol::error err = chunk;
        std::cerr << "[CEREKA] Lua load error in " << filename << ": " << err.what() << "\n";
        return;
    }
    scriptInterpreter.script = sol::coroutine(chunk);
    scriptInterpreter.scriptFinished = false;
}

void Impl::Reset()
{
    dialogue.Clear();
    scene.Clear();
}

// ---------------------------------------------------------------------------
// Update — typewriter + fade transition
// ---------------------------------------------------------------------------

void Impl::Update(float dt)
{
    dialogue.Tick(dt);

    if (state == CerekaState::Fading && scene.TickFade(dt))
        state = CerekaState::Running;
}

// ---------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------

void Impl::HandleEvent(const CerekaEvent &e)
{
    if (e.type == CerekaEvent::Quit) {
        state = CerekaState::Quit;
        return;
    }

    // Save/Load overlay — intercept all input while overlay is open
    if (state == CerekaState::SaveMenuState || state == CerekaState::LoadMenuState) {
        bool isSaving = (state == CerekaState::SaveMenuState);
        if (e.type == CerekaEvent::KeyDown && e.key == SDLK_ESCAPE) {
            state = stateBeforeSaveMenu;
            return;
        }
        if (e.type == CerekaEvent::MouseDown) {
            int slot = HitTestSaveSlot((int)e.mouseX, (int)e.mouseY);
            if (slot >= 1 && slot <= 10) {
                if (isSaving) {
                    SaveGame(slot);
                    state = stateBeforeSaveMenu;
                }
                else {
                    LoadGame(slot);  // restores state from file
                }
            }
        }
        return;
    }

    // Escape during normal play opens save menu
    if (e.type == CerekaEvent::KeyDown && e.key == SDLK_ESCAPE) {
        if (state == CerekaState::WaitingForInput || state == CerekaState::Running) {
            stateBeforeSaveMenu = state;
            state = CerekaState::SaveMenuState;
            return;
        }
    }

    if (state == CerekaState::WaitingForInput &&
        (e.type == CerekaEvent::MouseDown ||
         (e.type == CerekaEvent::KeyDown &&
          std::find(uiCfg.advanceKeys.begin(), uiCfg.advanceKeys.end(), (SDL_Keycode)e.key) !=
              uiCfg.advanceKeys.end())))
    {
        state = CerekaState::Running;
        return;
    }

    if (state == CerekaState::InMenu && e.type == CerekaEvent::MouseDown) {
        int idx = menu.HitTest(
            e.mouseX, e.mouseY, screenWidth, screenHeight, uiCfg.button.w, uiCfg.button.h);
        if (idx < 0)
            return;

        if (menu.IsExit(idx)) {
            ExitMenu();
            state = CerekaState::Finished;
            return;
        }

        const std::string &target = menu.Target(idx);
        scriptInterpreter.pc =
            target.empty() ? menu.EndPC() : scriptInterpreter.labelMap[target];
        ExitMenu();
        state = CerekaState::Running;
    }
}

// ---------------------------------------------------------------------------
// TickScript — main VM dispatch loop
// ---------------------------------------------------------------------------

void Impl::TickScript()
{
    if (state != CerekaState::Running)
        return;

    auto &si = scriptInterpreter;  // local alias keeps dispatch readable

    while (si.pc < si.program.size()) {
        const auto &ins = si.program[si.pc];

        // Skip mode: inside a false if-block
        if (si.skipMode) {
            if (ins.op == scenario::Op::IF_EQ || ins.op == scenario::Op::IF_NEQ ||
                ins.op == scenario::Op::IF_GT || ins.op == scenario::Op::IF_LT ||
                ins.op == scenario::Op::IF_GE || ins.op == scenario::Op::IF_LE)
                si.skipDepth++;
            else if (ins.op == scenario::Op::ELSE) {
                // Already skipping due to false IF - entering else block, restore execution
                si.skipMode = false;
                si.skipDepth = 0;
            }
            else if (ins.op == scenario::Op::ENDIF) {
                si.skipDepth--;
                if (si.skipDepth == 0)
                    si.skipMode = false;
            }
            si.pc++;
            continue;
        }

        switch (ins.op) {

            case scenario::Op::BG:
                scene.ShowBackground(ins.a);
                si.pc++;
                continue;

            case scenario::Op::FADE: {
                float totalDur = 0.5f;
                if (!ins.b.empty()) {
                    try {
                        totalDur = std::stof(ins.b);
                    }
                    catch (...) {
                    }
                }
                scene.StartFade(ins.a, totalDur);
                state = CerekaState::Fading;
                si.pc++;
                return;
            }

            case scenario::Op::CHAR:
                scene.ShowCharacter(ins.a, ins.b, ins.c);
                si.pc++;
                continue;

            case scenario::Op::HIDE_CHAR:
                scene.HideCharacter(ins.a);
                si.pc++;
                continue;

            case scenario::Op::SAY:
                Say(ins.a, ins.a, ins.b);
                state = CerekaState::WaitingForInput;
                si.pc++;
                return;

            case scenario::Op::NARRATE:
                Narrate(ins.b);
                state = CerekaState::WaitingForInput;
                si.pc++;
                return;

            case scenario::Op::MENU:
                EnterMenu();
                state = CerekaState::InMenu;
                si.pc++;
                return;

            case scenario::Op::JUMP:
                si.pc = si.labelMap[ins.a];
                continue;

            case scenario::Op::CALL:
                si.callStack.push_back(si.pc + 1);
                si.pc = si.labelMap[ins.a];
                continue;

            case scenario::Op::RETURN:
                if (!si.callStack.empty()) {
                    si.pc = si.callStack.back();
                    si.callStack.pop_back();
                }
                else {
                    state = CerekaState::Finished;
                }
                continue;

            case scenario::Op::SET_VAR:
                si.variables[ins.a] = ins.b;
                si.pc++;
                continue;

            case scenario::Op::SET_VAR_NUM: {
                float lhs = si.LookupNumVar(ins.a);
                float rhs = si.EvalExpr(ins.c);
                float result = 0.0f;
                if (ins.b == "+")
                    result = lhs + rhs;
                else if (ins.b == "-")
                    result = lhs - rhs;
                else if (ins.b == "*")
                    result = lhs * rhs;
                else if (ins.b == "/")
                    result = (rhs != 0.0f) ? (lhs / rhs) : 0.0f;
                else
                    result = rhs;  // "=" plain assignment
                si.numVariables[ins.a] = result;
                si.variables[ins.a] = std::to_string(result);
                si.pc++;
                continue;
            }

            case scenario::Op::IF_EQ: {
                auto it = si.variables.find(ins.a);
                std::string val = (it != si.variables.end()) ? it->second : "";
                if (val != ins.b) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case scenario::Op::IF_NEQ: {
                auto it = si.variables.find(ins.a);
                std::string val = (it != si.variables.end()) ? it->second : "";
                if (val == ins.b) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case scenario::Op::IF_GT:
            case scenario::Op::IF_LT:
            case scenario::Op::IF_GE:
            case scenario::Op::IF_LE: {
                float lhs = si.LookupNumVar(ins.a);
                float rhs = si.EvalExpr(ins.b);
                bool cond = false;
                switch (ins.op) {
                    case scenario::Op::IF_GT: cond = lhs > rhs; break;
                    case scenario::Op::IF_LT: cond = lhs < rhs; break;
                    case scenario::Op::IF_GE: cond = lhs >= rhs; break;
                    case scenario::Op::IF_LE: cond = lhs <= rhs; break;
                    default: break;
                }
                if (!cond) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case scenario::Op::ENDIF:
                si.pc++;
                continue;

            case scenario::Op::ELSE:
                if (si.skipMode) {
                    // Already skipping from false if-condition: enter else block, stop skipping
                    // skipDepth already = 1 (from the IF that was false)
                    si.skipMode = false;
                    si.skipDepth = 0;
                }
                else {
                    // If condition was true - skip the else block
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;

            case scenario::Op::PLAY_BGM:
                audio.PlayBGM(ins.a);
                si.pc++;
                continue;

            case scenario::Op::STOP_BGM:
                audio.StopBGM();
                si.pc++;
                continue;

            case scenario::Op::PLAY_SFX:
                audio.PlaySFX(ins.a);
                si.pc++;
                continue;

            case scenario::Op::UI_SET:
                ApplyUiSet(ins.a, ins.b);
                si.pc++;
                continue;

            case scenario::Op::SAVE_MENU:
                stateBeforeSaveMenu = state;
                state = CerekaState::SaveMenuState;
                si.pc++;
                return;

            case scenario::Op::LOAD_MENU:
                stateBeforeSaveMenu = state;
                state = CerekaState::LoadMenuState;
                si.pc++;
                return;

            case scenario::Op::SAVE: {
                int slot = ins.a.empty() ? 0 : std::stoi(ins.a);
                if (slot >= 1 && slot <= 10) {
                    stateBeforeSaveMenu = state;
                    SaveGame(slot);
                }
                si.pc++;
                continue;
            }

            case scenario::Op::LOAD: {
                int slot = ins.a.empty() ? 0 : std::stoi(ins.a);
                if (slot >= 1 && slot <= 10)
                    LoadGame(slot);  // restores pc and state from file
                return;
            }

            case scenario::Op::END:
                state = CerekaState::Finished;
                return;

            case scenario::Op::LABEL:
                si.pc++;
                continue;

            default:
                si.pc++;
                continue;
        }
    }
}
