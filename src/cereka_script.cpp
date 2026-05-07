// script_vm.cpp — CerekaImpl script VM methods: Update, script loading.
// Expression evaluation and the variable state container live in
// script_interpreter.{hpp,cpp}.

#include "cereka_engine_impl.hpp"

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


