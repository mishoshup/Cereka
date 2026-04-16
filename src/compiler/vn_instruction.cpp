#include "vn_instruction.hpp"
#include "compiler_lua_embed.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sol/sol.hpp>
#include <sstream>

namespace fs = std::filesystem;

namespace cereka::scenario {

// ---------------------------------------------------------------------------
// Forward declaration for recursive include/call resolution
// ---------------------------------------------------------------------------
static std::vector<Instruction> CompileFile(const fs::path &path, int depth);

// ---------------------------------------------------------------------------
// Run compiler.lua on script_text, return raw instruction list
// ---------------------------------------------------------------------------
static std::vector<Instruction> RunLuaCompiler(const std::string &scriptText)
{
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);

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

    sol::protected_function_result res = compileFunc(scriptText);
    if (!res.valid()) {
        sol::error err = res;
        std::cerr << "[CEREKA] Script compile error: " << err.what() << "\n";
        return {};
    }

    sol::table result = res;
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

        sol::table t  = p.second.as<sol::table>();
        std::string op = t["op"].get_or<std::string>("");
        if (op.empty()) continue;

        Instruction ins;
        if      (op == "SAY")       ins.op = Op::SAY;
        else if (op == "NARRATE")   ins.op = Op::NARRATE;
        else if (op == "JUMP")      ins.op = Op::JUMP;
        else if (op == "LABEL")     ins.op = Op::LABEL;
        else if (op == "END")       ins.op = Op::END;
        else if (op == "BG")        ins.op = Op::BG;
        else if (op == "CHAR")      ins.op = Op::CHAR;
        else if (op == "BUTTON")    ins.op = Op::BUTTON;
        else if (op == "MENU")      ins.op = Op::MENU;
        else if (op == "PLAY_BGM")  ins.op = Op::PLAY_BGM;
        else if (op == "STOP_BGM")  ins.op = Op::STOP_BGM;
        else if (op == "PLAY_SFX")  ins.op = Op::PLAY_SFX;
        else if (op == "HIDE_CHAR") ins.op = Op::HIDE_CHAR;
        else if (op == "SET_VAR")   ins.op = Op::SET_VAR;
        else if (op == "IF_EQ")     ins.op = Op::IF_EQ;
        else if (op == "IF_NEQ")    ins.op = Op::IF_NEQ;
        else if (op == "ENDIF")     ins.op = Op::ENDIF;
        else if (op == "FADE")      ins.op = Op::FADE;
        else if (op == "INCLUDE")   ins.op = Op::INCLUDE;
        else if (op == "CALL")      ins.op = Op::CALL;
        else if (op == "UI_SET")    ins.op = Op::UI_SET;
        else {
            std::cerr << "[CEREKA] Unknown op: " << op << "\n";
            continue;
        }

        ins.a           = t["a"].get_or<std::string>("");
        ins.b           = t["b"].get_or<std::string>("");
        ins.c           = t["c"].get_or<std::string>("");
        ins.exit_button = t["exit_button"].get_or(false);

        if (t["choices"].valid()) {
            sol::table choices = t["choices"];
            for (auto &c : choices.pairs()) {
                if (!c.second.is<sol::table>()) continue;
                sol::table ctbl = c.second.as<sol::table>();
                ChoiceOption opt;
                opt.text        = ctbl["text"].get_or<std::string>("");
                opt.targetLabel = ctbl["target"].get_or<std::string>("");
                ins.choices.push_back(opt);
            }
        }

        program.push_back(ins);
    }

    return program;
}

// ---------------------------------------------------------------------------
// Resolve INCLUDEs and CALLs recursively, then return a flat instruction list
// ---------------------------------------------------------------------------
static std::vector<Instruction> CompileFile(const fs::path &path, int depth)
{
    static constexpr int MAX_DEPTH = 32;
    if (depth > MAX_DEPTH) {
        std::cerr << "[CEREKA] Include/call depth limit reached at: " << path << "\n";
        return {};
    }

    std::ifstream f(path);
    if (!f) {
        std::cerr << "[CEREKA] Could not open script: " << path << "\n";
        return {};
    }
    std::stringstream buf;
    buf << f.rdbuf();

    std::vector<Instruction> raw = RunLuaCompiler(buf.str());

    fs::path dir = path.parent_path();
    std::vector<Instruction> resolved;
    std::vector<Instruction> subroutines; // appended after END
    int callId = 0;

    for (auto &ins : raw) {
        if (ins.op == Op::INCLUDE) {
            // Inline the included file — strip its trailing END
            auto sub = CompileFile(dir / ins.a, depth + 1);
            if (!sub.empty() && sub.back().op == Op::END)
                sub.pop_back();
            resolved.insert(resolved.end(), sub.begin(), sub.end());

        } else if (ins.op == Op::CALL) {
            // Replace CALL with a JUMP-to-subroutine + auto-generated label
            std::string label = "__call_" + path.stem().string()
                                + "_" + std::to_string(callId++) + "__";

            Instruction callIns;
            callIns.op = Op::CALL;
            callIns.a  = label; // runtime: push pc+1, jump to this label
            resolved.push_back(callIns);

            // Compile the subroutine; replace its END with RETURN
            auto sub = CompileFile(dir / ins.a, depth + 1);
            if (!sub.empty() && sub.back().op == Op::END)
                sub.back().op = Op::RETURN;
            else {
                Instruction ret; ret.op = Op::RETURN;
                sub.push_back(ret);
            }

            Instruction lbl; lbl.op = Op::LABEL; lbl.a = label;
            subroutines.push_back(lbl);
            subroutines.insert(subroutines.end(), sub.begin(), sub.end());

        } else {
            resolved.push_back(ins);
        }
    }

    // Subroutine blocks sit after the main END so normal execution never
    // falls into them; they are only reachable via CALL.
    resolved.insert(resolved.end(), subroutines.begin(), subroutines.end());
    return resolved;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
std::vector<Instruction> CompileVNScript(const std::string &filename)
{
    return CompileFile(fs::absolute(filename), 0);
}

}  // namespace cereka::scenario
