#pragma once

#include "compiler/vn_instruction.hpp"

#include <cstddef>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace cereka {

// Holds the execution state of a running .crka script — the program,
// program counter, call stack, variables, labels, and skip-mode flags —
// and knows how to evaluate expressions against those variables.
// TickScript dispatch lives on CerekaImpl (see script_vm.cpp); this class
// is the bag of state that dispatch operates on, so rollback and save can
// snapshot it as a unit.
class ScriptInterpreter {
   public:
    sol::state lua;
    sol::coroutine script;

    std::vector<scenario::Instruction> program;
    std::unordered_map<std::string, size_t> labelMap;
    std::unordered_map<std::string, std::string> variables;
    std::unordered_map<std::string, float> numVariables;
    std::vector<size_t> callStack;

    size_t pc = 0;
    bool scriptFinished = false;
    bool skipMode = false;
    int skipDepth = 0;

    float LookupNumVar(const std::string &name) const;
    float EvalExpr(const std::string &expr) const;
};

}  // namespace cereka
