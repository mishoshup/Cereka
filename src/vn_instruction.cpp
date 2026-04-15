#include "vn_instruction.hpp"
#include "compiler_lua_embed.hpp"
#include <fstream>
#include <iostream>
#include <sol/sol.hpp>
#include <sstream>

namespace cereka::scenario {

std::vector<Instruction> CompileVNScript(const std::string &filename)
{
    std::ifstream f(filename);
    if (!f) {
        std::cerr << "[CEREKA] Could not open script: " << filename << "\n";
        return {};
    }

    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string scriptText = buffer.str();

    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);

    // Load the embedded compiler
    auto loadRes = lua.load(cereka::COMPILER_LUA_SOURCE);
    if (!loadRes.valid()) {
        sol::error err = loadRes;
        std::cerr << "[CEREKA] Compiler load error: " << err.what() << "\n";
        return {};
    }
    loadRes();

    sol::function compileFunc = lua["compile"];
    if (!compileFunc.valid()) {
        std::cerr << "[CEREKA] Lua function 'compile' not found\n";
        return {};
    }

    sol::protected_function_result resultRes = compileFunc(scriptText);
    if (!resultRes.valid()) {
        sol::error err = resultRes;
        std::cerr << "[CEREKA] Script compile error: " << err.what() << "\n";
        return {};
    }

    sol::table result = resultRes;
    if (!result.valid()) {
        std::cerr << "[CEREKA] Compiler returned invalid table\n";
        return {};
    }

    sol::table instructions = result["instructions"];
    if (!instructions.valid()) {
        std::cerr << "[CEREKA] Compiler result has no 'instructions' table\n";
        return {};
    }

    std::vector<Instruction> program;

    for (auto &p : instructions.pairs()) {
        if (!p.second.is<sol::table>()) continue;

        sol::table t = p.second.as<sol::table>();
        std::string op = t["op"].get_or<std::string>("");
        if (op.empty()) continue;

        Instruction ins;
        if      (op == "SAY")      ins.op = Op::SAY;
        else if (op == "NARRATE")  ins.op = Op::NARRATE;
        else if (op == "JUMP")     ins.op = Op::JUMP;
        else if (op == "LABEL")    ins.op = Op::LABEL;
        else if (op == "END")      ins.op = Op::END;
        else if (op == "BG")       ins.op = Op::BG;
        else if (op == "CHAR")     ins.op = Op::CHAR;
        else if (op == "BUTTON")   ins.op = Op::BUTTON;
        else if (op == "MENU")     ins.op = Op::MENU;
        else if (op == "PLAY_BGM")  ins.op = Op::PLAY_BGM;
        else if (op == "STOP_BGM")  ins.op = Op::STOP_BGM;
        else if (op == "PLAY_SFX")  ins.op = Op::PLAY_SFX;
        else if (op == "HIDE_CHAR") ins.op = Op::HIDE_CHAR;
        else if (op == "SET_VAR")   ins.op = Op::SET_VAR;
        else if (op == "IF_EQ")     ins.op = Op::IF_EQ;
        else if (op == "IF_NEQ")    ins.op = Op::IF_NEQ;
        else if (op == "ENDIF")     ins.op = Op::ENDIF;
        else {
            std::cerr << "[CEREKA] Unknown op: " << op << "\n";
            continue;
        }

        ins.a = t["a"].get_or<std::string>("");
        ins.b = t["b"].get_or<std::string>("");
        ins.exit_button = t["exit_button"].get_or(false);

        if (t["choices"].valid()) {
            sol::table choices = t["choices"];
            for (auto &c : choices.pairs()) {
                if (!c.second.is<sol::table>()) continue;
                sol::table ctbl = c.second.as<sol::table>();
                ChoiceOption opt;
                opt.text = ctbl["text"].get_or<std::string>("");
                opt.targetLabel = ctbl["target"].get_or<std::string>("");
                ins.choices.push_back(opt);
            }
        }

        program.push_back(ins);
    }

    return program;
}

}  // namespace cereka::scenario
