// script_vm.cpp — script VM: TickScript, Update (typewriter + fade), Load, Reset

#include "engine_impl.hpp"
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// Expression evaluation for $ arithmetic and if-comparisons.
// Grammar: term (('+'|'-') term)*
//          term    = factor (('*'|'/') factor)*
//          factor  = NUMBER | IDENT
// IDENT is resolved via numVariables (fallback to variables parsed as float).
// ---------------------------------------------------------------------------

float Impl::LookupNumVar(const std::string &name) const
{
    auto it = numVariables.find(name);
    if (it != numVariables.end())
        return it->second;
    auto sit = variables.find(name);
    if (sit != variables.end()) {
        try {
            return std::stof(sit->second);
        }
        catch (...) {
        }
    }
    return 0.0f;
}

namespace {

struct ExprParser {
    const std::string &src;
    size_t i = 0;
    const cereka::CerekaImpl &vm;

    ExprParser(const std::string &s, const cereka::CerekaImpl &v) : src(s), vm(v) {}

    void skipWs()
    {
        while (i < src.size() && std::isspace((unsigned char)src[i]))
            ++i;
    }

    float parseFactor()
    {
        skipWs();
        if (i >= src.size())
            return 0.0f;
        char c = src[i];
        if (c == '(') {
            ++i;
            float v = parseExpr();
            skipWs();
            if (i < src.size() && src[i] == ')')
                ++i;
            return v;
        }
        if (c == '-') {
            ++i;
            return -parseFactor();
        }
        if (std::isdigit((unsigned char)c) || c == '.') {
            size_t start = i;
            while (i < src.size() && (std::isdigit((unsigned char)src[i]) || src[i] == '.'))
                ++i;
            try {
                return std::stof(src.substr(start, i - start));
            }
            catch (...) {
                return 0.0f;
            }
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < src.size() &&
                   (std::isalnum((unsigned char)src[i]) || src[i] == '_'))
                ++i;
            return vm.LookupNumVar(src.substr(start, i - start));
        }
        ++i;  // skip unknown
        return 0.0f;
    }

    float parseTerm()
    {
        float lhs = parseFactor();
        while (true) {
            skipWs();
            if (i >= src.size())
                break;
            char c = src[i];
            if (c != '*' && c != '/')
                break;
            ++i;
            float rhs = parseFactor();
            if (c == '*')
                lhs = lhs * rhs;
            else
                lhs = (rhs != 0.0f) ? (lhs / rhs) : 0.0f;
        }
        return lhs;
    }

    float parseExpr()
    {
        float lhs = parseTerm();
        while (true) {
            skipWs();
            if (i >= src.size())
                break;
            char c = src[i];
            if (c != '+' && c != '-')
                break;
            ++i;
            float rhs = parseTerm();
            lhs = (c == '+') ? (lhs + rhs) : (lhs - rhs);
        }
        return lhs;
    }
};

}  // namespace

float Impl::EvalExpr(const std::string &expr) const
{
    ExprParser p(expr, *this);
    return p.parseExpr();
}

// ---------------------------------------------------------------------------
// Script loading
// ---------------------------------------------------------------------------

void Impl::LoadCompiledScript(const std::vector<scenario::Instruction> &compiled)
{
    program = compiled;
    pc = 0;
    scriptFinished = false;
    variables.clear();
    numVariables.clear();
    callStack.clear();
    skipMode = false;
    skipDepth = 0;

    labelMap.clear();
    for (size_t i = 0; i < program.size(); ++i)
        if (program[i].op == scenario::Op::LABEL)
            labelMap[program[i].a] = i;
}

void Impl::LoadScript(const std::string &filename)
{
    sol::load_result chunk = lua.load_file(filename);
    if (!chunk.valid()) {
        sol::error err = chunk;
        std::cerr << "[CEREKA] Lua load error in " << filename << ": " << err.what() << "\n";
        return;
    }
    script = sol::coroutine(chunk);
    scriptFinished = false;
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
        pc = target.empty() ? menu.EndPC() : labelMap[target];
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

    while (pc < program.size()) {
        const auto &ins = program[pc];

        // Skip mode: inside a false if-block
        if (skipMode) {
            if (ins.op == scenario::Op::IF_EQ || ins.op == scenario::Op::IF_NEQ ||
                ins.op == scenario::Op::IF_GT || ins.op == scenario::Op::IF_LT ||
                ins.op == scenario::Op::IF_GE || ins.op == scenario::Op::IF_LE)
                skipDepth++;
            else if (ins.op == scenario::Op::ELSE) {
                // Already skipping due to false IF - entering else block, restore execution
                skipMode = false;
                skipDepth = 0;
            }
            else if (ins.op == scenario::Op::ENDIF) {
                skipDepth--;
                if (skipDepth == 0)
                    skipMode = false;
            }
            pc++;
            continue;
        }

        switch (ins.op) {

            case scenario::Op::BG:
                scene.ShowBackground(ins.a);
                pc++;
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
                pc++;
                return;
            }

            case scenario::Op::CHAR:
                scene.ShowCharacter(ins.a, ins.b, ins.c);
                pc++;
                continue;

            case scenario::Op::HIDE_CHAR:
                scene.HideCharacter(ins.a);
                pc++;
                continue;

            case scenario::Op::SAY:
                Say(ins.a, ins.a, ins.b);
                state = CerekaState::WaitingForInput;
                pc++;
                return;

            case scenario::Op::NARRATE:
                Narrate(ins.b);
                state = CerekaState::WaitingForInput;
                pc++;
                return;

            case scenario::Op::MENU:
                EnterMenu();
                state = CerekaState::InMenu;
                pc++;
                return;

            case scenario::Op::JUMP:
                pc = labelMap[ins.a];
                continue;

            case scenario::Op::CALL:
                callStack.push_back(pc + 1);
                pc = labelMap[ins.a];
                continue;

            case scenario::Op::RETURN:
                if (!callStack.empty()) {
                    pc = callStack.back();
                    callStack.pop_back();
                }
                else {
                    state = CerekaState::Finished;
                }
                continue;

            case scenario::Op::SET_VAR:
                variables[ins.a] = ins.b;
                pc++;
                continue;

            case scenario::Op::SET_VAR_NUM: {
                float lhs = LookupNumVar(ins.a);
                float rhs = EvalExpr(ins.c);
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
                numVariables[ins.a] = result;
                variables[ins.a] = std::to_string(result);
                pc++;
                continue;
            }

            case scenario::Op::IF_EQ: {
                auto it = variables.find(ins.a);
                std::string val = (it != variables.end()) ? it->second : "";
                if (val != ins.b) {
                    skipMode = true;
                    skipDepth = 1;
                }
                pc++;
                continue;
            }

            case scenario::Op::IF_NEQ: {
                auto it = variables.find(ins.a);
                std::string val = (it != variables.end()) ? it->second : "";
                if (val == ins.b) {
                    skipMode = true;
                    skipDepth = 1;
                }
                pc++;
                continue;
            }

            case scenario::Op::IF_GT:
            case scenario::Op::IF_LT:
            case scenario::Op::IF_GE:
            case scenario::Op::IF_LE: {
                float lhs = LookupNumVar(ins.a);
                float rhs = EvalExpr(ins.b);
                bool cond = false;
                switch (ins.op) {
                    case scenario::Op::IF_GT: cond = lhs > rhs; break;
                    case scenario::Op::IF_LT: cond = lhs < rhs; break;
                    case scenario::Op::IF_GE: cond = lhs >= rhs; break;
                    case scenario::Op::IF_LE: cond = lhs <= rhs; break;
                    default: break;
                }
                if (!cond) {
                    skipMode = true;
                    skipDepth = 1;
                }
                pc++;
                continue;
            }

            case scenario::Op::ENDIF:
                pc++;
                continue;

            case scenario::Op::ELSE:
                if (skipMode) {
                    // Already skipping from false if-condition: enter else block, stop skipping
                    // skipDepth already = 1 (from the IF that was false)
                    skipMode = false;
                    skipDepth = 0;
                }
                else {
                    // If condition was true - skip the else block
                    skipMode = true;
                    skipDepth = 1;
                }
                pc++;
                continue;

            case scenario::Op::PLAY_BGM:
                audio.PlayBGM(ins.a);
                pc++;
                continue;

            case scenario::Op::STOP_BGM:
                audio.StopBGM();
                pc++;
                continue;

            case scenario::Op::PLAY_SFX:
                audio.PlaySFX(ins.a);
                pc++;
                continue;

            case scenario::Op::UI_SET:
                ApplyUiSet(ins.a, ins.b);
                pc++;
                continue;

            case scenario::Op::SAVE_MENU:
                stateBeforeSaveMenu = state;
                state = CerekaState::SaveMenuState;
                pc++;
                return;

            case scenario::Op::LOAD_MENU:
                stateBeforeSaveMenu = state;
                state = CerekaState::LoadMenuState;
                pc++;
                return;

            case scenario::Op::SAVE: {
                int slot = ins.a.empty() ? 0 : std::stoi(ins.a);
                if (slot >= 1 && slot <= 10) {
                    stateBeforeSaveMenu = state;
                    SaveGame(slot);
                }
                pc++;
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
                pc++;
                continue;

            default:
                pc++;
                continue;
        }
    }
}
