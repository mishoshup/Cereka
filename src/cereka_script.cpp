// script_vm.cpp — CerekaImpl script VM methods: CerekaScriptTick, Update,
// HandleEvent, script loading. Expression evaluation and the variable
// state container live in script_interpreter.{hpp,cpp}.

#include "cereka_engine_impl.hpp"
#include "cereka_safe_parse.hpp"
#include <algorithm>

using namespace cereka::compiler;

// ---------------------------------------------------------------------------
// Script loading
// ---------------------------------------------------------------------------

void Impl::LoadCompiledCerekaScript(const std::vector<compiler::Instruction> &compiled)
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
        if (scriptInterpreter.program[i].op == compiler::Op::LABEL)
            scriptInterpreter.labelMap[scriptInterpreter.program[i].a] = i;
}

void Impl::LoadCerekaScript(const std::string &filename)
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
// Update — delegates to state machine
// ---------------------------------------------------------------------------

void Impl::Update(float dt)
{
    m_stateMachine.update(dt);
}

// ---------------------------------------------------------------------------
// CerekaScriptTick — main VM dispatch loop
// ---------------------------------------------------------------------------

void Impl::CerekaScriptTick()
{
    if (state != CerekaState::Running)
        return;

    auto &si = scriptInterpreter;  // local alias keeps dispatch readable

    while (si.pc < si.program.size()) {
        const auto &ins = si.program[si.pc];

        // Skip mode: inside a false if-block
        if (si.skipMode) {
            if (ins.op == compiler::Op::IF_EQ || ins.op == compiler::Op::IF_NEQ ||
                ins.op == compiler::Op::IF_GT || ins.op == compiler::Op::IF_LT ||
                ins.op == compiler::Op::IF_GE || ins.op == compiler::Op::IF_LE) {
                si.skipDepth++;
            } else if (ins.op == compiler::Op::ENDIF) {
                si.skipDepth--;
                if (si.skipDepth == 0)
                    si.skipMode = false;
            }
            // ELSE is silently skipped while in skipMode — it belongs to a conditional
            // whose condition we never evaluated, so we must skip its block too.
            // Only ENDIF can exit skipMode (when skipDepth returns to 0).
            si.pc++;
            continue;
        }

        switch (ins.op) {

            case compiler::Op::BG:
                scene.ShowBackground(ins.a);
                si.pc++;
                continue;

            case compiler::Op::FADE: {
                float totalDur = 0.5f;
                if (!ins.b.empty()) {
                    try {
                        totalDur = std::stof(ins.b);
                    }
                    catch (...) {
                    }
                }
                scene.StartFade(ins.a, totalDur);
                changeState(CerekaState::Fading);
                si.pc++;
                return;
            }

            case compiler::Op::CHAR:
                scene.ShowCharacter(ins.a, ins.b, ins.c);
                si.pc++;
                continue;

            case compiler::Op::HIDE_CHAR:
                scene.HideCharacter(ins.a);
                si.pc++;
                continue;

            case compiler::Op::SAY:
                Say(ins.a, ins.a, ins.b);
                changeState(CerekaState::WaitingForInput);
                si.pc++;
                return;

            case compiler::Op::NARRATE:
                Narrate(ins.b);
                changeState(CerekaState::WaitingForInput);
                si.pc++;
                return;

            case compiler::Op::MENU:
                EnterMenu();
                changeState(CerekaState::InMenu);
                si.pc++;
                return;

            case compiler::Op::JUMP:
                si.pc = si.labelMap[ins.a];
                continue;

            case compiler::Op::CALL: {
                static constexpr size_t MAX_CALL_DEPTH = 32;
                if (si.callStack.size() >= MAX_CALL_DEPTH) {
                    std::cerr << "[CEREKA] Call stack overflow (max " << MAX_CALL_DEPTH << ")\n";
                    changeState(CerekaState::Finished);
                    return;
                }
                si.callStack.push_back(si.pc + 1);
                si.pc = si.labelMap[ins.a];
                continue;
            }

            case compiler::Op::RETURN:
                if (!si.callStack.empty()) {
                    si.pc = si.callStack.back();
                    si.callStack.pop_back();
                }
                else {
                    changeState(CerekaState::Finished);
                }
                continue;

            case compiler::Op::SET_VAR:
                si.variables[ins.a] = ins.b;
                si.pc++;
                continue;

            case compiler::Op::SET_VAR_NUM: {
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

            case compiler::Op::IF_EQ: {
                auto it = si.variables.find(ins.a);
                std::string val = (it != si.variables.end()) ? it->second : "";
                if (val != ins.b) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::IF_NEQ: {
                auto it = si.variables.find(ins.a);
                std::string val = (it != si.variables.end()) ? it->second : "";
                if (val == ins.b) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::IF_GT:
            case compiler::Op::IF_LT:
            case compiler::Op::IF_GE:
            case compiler::Op::IF_LE: {
                float lhs = si.LookupNumVar(ins.a);
                float rhs = si.EvalExpr(ins.b);
                bool cond = false;
                switch (ins.op) {
                    case compiler::Op::IF_GT: cond = lhs > rhs; break;
                    case compiler::Op::IF_LT: cond = lhs < rhs; break;
                    case compiler::Op::IF_GE: cond = lhs >= rhs; break;
                    case compiler::Op::IF_LE: cond = lhs <= rhs; break;
                    default: break;
                }
                if (!cond) {
                    si.skipMode = true;
                    si.skipDepth = 1;
                }
                si.pc++;
                continue;
            }

            case compiler::Op::ENDIF:
                si.pc++;
                continue;

            case compiler::Op::ELSE:
                // If we reach here, skipMode was false, meaning the IF branch was executed.
                // So we must skip the ELSE branch.
                si.skipMode = true;
                si.skipDepth = 1;
                si.pc++;
                continue;

            case compiler::Op::PLAY_BGM:
                audio.PlayBGM(ins.a);
                si.pc++;
                continue;

            case compiler::Op::STOP_BGM:
                audio.StopBGM();
                si.pc++;
                continue;

            case compiler::Op::PLAY_SFX:
                audio.PlaySFX(ins.a);
                si.pc++;
                continue;

            case compiler::Op::UI_SET:
                ApplyUiSet(ins.a, ins.b);
                si.pc++;
                continue;

            case compiler::Op::SAVE_MENU:
                pushOverlay(CerekaState::SaveMenuState);
                si.pc++;
                return;

            case compiler::Op::LOAD_MENU:
                pushOverlay(CerekaState::LoadMenuState);
                si.pc++;
                return;

            case compiler::Op::SAVE: {
                int slot = 0;
                if (!ins.a.empty()) {
                    auto r = safe_stoi(ins.a);
                    if (r) slot = *r;
                }
                if (slot >= 1 && slot <= 10) {
                    setSavedState(state);
                    SaveGame(slot);
                }
                si.pc++;
                continue;
            }

            case compiler::Op::LOAD: {
                int slot = 0;
                if (!ins.a.empty()) {
                    auto r = safe_stoi(ins.a);
                    if (r) slot = *r;
                }
                if (slot >= 1 && slot <= 10)
                    LoadGame(slot);  // restores pc and state from file
                return;
            }

            case compiler::Op::END:
                changeState(CerekaState::Finished);
                return;

            case compiler::Op::LABEL:
                si.pc++;
                continue;

            default:
                si.pc++;
                continue;
        }
    }
}
