// script_vm.cpp — script VM: TickScript, Update (typewriter + fade), Load, Reset

#include "engine_impl.hpp"

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
    currentText.clear();
    displayedChars = 0;
    typewriterTimer = 0.0f;
    currentSpeaker.clear();
    currentName.clear();
    if (background) {
        SDL_DestroyTexture(background);
        background = nullptr;
    }
    characters.clear();
}

// ---------------------------------------------------------------------------
// Update — typewriter + fade transition
// ---------------------------------------------------------------------------

void Impl::Update(float dt)
{
    // Typewriter
    if (!currentText.empty() && displayedChars < (int)currentText.length()) {
        typewriterTimer += dt;
        int charsToAdd = (int)(typewriterTimer * CHARS_PER_SECOND);
        if (charsToAdd > 0) {
            displayedChars += charsToAdd;
            typewriterTimer -= charsToAdd / CHARS_PER_SECOND;
            if (displayedChars > (int)currentText.length())
                displayedChars = (int)currentText.length();
        }
    }

    // Fade transition
    if (state == CerekaState::Fading) {
        fadeTimer += dt;
        if (fadePhase == FadePhase::Out && fadeTimer >= fadePhaseDuration) {
            if (background) {
                SDL_DestroyTexture(background);
                background = nullptr;
            }
            background = pendingBg;
            pendingBg = nullptr;
            fadePhase = FadePhase::In;
            fadeTimer = 0.0f;
        }
        else if (fadePhase == FadePhase::In && fadeTimer >= fadePhaseDuration) {
            fadePhase = FadePhase::None;
            fadeTimer = 0.0f;
            state = CerekaState::Running;
        }
    }
}

// ---------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------

void Impl::HandleEvent(const CerekaEvent &e)
{
    // Save/Load overlay — intercept all input while overlay is open
    if (state == CerekaState::SaveMenu || state == CerekaState::LoadMenu) {
        bool isSaving = (state == CerekaState::SaveMenu);
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
            state = CerekaState::SaveMenu;
            return;
        }
    }

    if (state == CerekaState::WaitingForInput &&
        (e.type == CerekaEvent::MouseDown || e.type == CerekaEvent::KeyDown))
    {
        state = CerekaState::Running;
        return;
    }

    if (state == CerekaState::InMenu && e.type == CerekaEvent::MouseDown) {
        int idx = HitTestButton(e.mouseX, e.mouseY);
        if (idx < 0)
            return;

        if (buttonExits[idx]) {
            ExitMenu();
            state = CerekaState::Finished;
            return;
        }

        pc = buttonTargets[idx].empty() ? menuEndPC : labelMap[buttonTargets[idx]];
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
                ShowBackground(ins.a);
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
                fadePhaseDuration = totalDur * 0.5f;
                fadeTimer = 0.0f;
                fadePhase = FadePhase::Out;
                if (pendingBg)
                    SDL_DestroyTexture(pendingBg);
                pendingBg = LoadTexture(ins.a);
                state = CerekaState::Fading;
                pc++;
                return;
            }

            case scenario::Op::CHAR:
                ShowCharacter(ins.a, ins.b, ins.c);
                pc++;
                continue;

            case scenario::Op::HIDE_CHAR:
                HideCharacter(ins.a);
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
                float lhs = 0.0f;
                auto it = numVariables.find(ins.a);
                if (it != numVariables.end())
                    lhs = it->second;
                else {
                    auto sit = variables.find(ins.a);
                    if (sit != variables.end()) {
                        try {
                            lhs = std::stof(sit->second);
                        }
                        catch (...) {
                        }
                    }
                }

                float rhs = 0.0f;
                try {
                    rhs = std::stof(ins.c);
                }
                catch (...) {
                }

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
                    result = rhs;

                numVariables[ins.a] = result;
                variables[ins.a] = std::to_string(result);
                std::cerr << "[CEREKA] NUM: $" << ins.a << " (" << lhs << " " << ins.b << " "
                          << rhs << ") = " << result << "\n";
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

            case scenario::Op::IF_GT: {
                float lhs = 0.0f;
                auto it = numVariables.find(ins.a);
                if (it != numVariables.end())
                    lhs = it->second;
                else {
                    auto sit = variables.find(ins.a);
                    if (sit != variables.end()) {
                        try {
                            lhs = std::stof(sit->second);
                        }
                        catch (...) {
                        }
                    }
                }
                float rhs = 0.0f;
                try {
                    rhs = std::stof(ins.b);
                }
                catch (...) {
                }
                bool cond = lhs > rhs;
                std::cerr << "[CEREKA] IF: $" << ins.a << " (" << lhs << ") > " << ins.b << " ("
                          << rhs << ") = " << (cond ? "true" : "false") << "\n";
                if (!cond) {
                    skipMode = true;
                    skipDepth = 1;
                }
                pc++;
                continue;
            }

            case scenario::Op::IF_LT: {
                float lhs = 0.0f;
                auto it = numVariables.find(ins.a);
                if (it != numVariables.end())
                    lhs = it->second;
                else {
                    auto sit = variables.find(ins.a);
                    if (sit != variables.end()) {
                        try {
                            lhs = std::stof(sit->second);
                        }
                        catch (...) {
                        }
                    }
                }
                float rhs = 0.0f;
                try {
                    rhs = std::stof(ins.b);
                }
                catch (...) {
                }
                bool cond = lhs < rhs;
                std::cerr << "[CEREKA] IF: $" << ins.a << " (" << lhs << ") < " << ins.b << " ("
                          << rhs << ") = " << (cond ? "true" : "false") << "\n";
                if (!cond) {
                    skipMode = true;
                    skipDepth = 1;
                }
                pc++;
                continue;
            }

            case scenario::Op::IF_GE: {
                float lhs = 0.0f;
                auto it = numVariables.find(ins.a);
                if (it != numVariables.end())
                    lhs = it->second;
                else {
                    auto sit = variables.find(ins.a);
                    if (sit != variables.end()) {
                        try {
                            lhs = std::stof(sit->second);
                        }
                        catch (...) {
                        }
                    }
                }
                float rhs = 0.0f;
                try {
                    rhs = std::stof(ins.b);
                }
                catch (...) {
                }
                bool cond = lhs >= rhs;
                std::cerr << "[CEREKA] IF: $" << ins.a << " (" << lhs << ") >= " << ins.b << " ("
                          << rhs << ") = " << (cond ? "true" : "false") << "\n";
                if (!cond) {
                    skipMode = true;
                    skipDepth = 1;
                }
                pc++;
                continue;
            }

            case scenario::Op::IF_LE: {
                float lhs = 0.0f;
                auto it = numVariables.find(ins.a);
                if (it != numVariables.end())
                    lhs = it->second;
                else {
                    auto sit = variables.find(ins.a);
                    if (sit != variables.end()) {
                        try {
                            lhs = std::stof(sit->second);
                        }
                        catch (...) {
                        }
                    }
                }
                float rhs = 0.0f;
                try {
                    rhs = std::stof(ins.b);
                }
                catch (...) {
                }
                bool cond = lhs <= rhs;
                std::cerr << "[CEREKA] IF: $" << ins.a << " (" << lhs << ") <= " << ins.b << " ("
                          << rhs << ") = " << (cond ? "true" : "false") << "\n";
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

            case scenario::Op::PLAY_BGM:
                PlayBGM(ins.a);
                pc++;
                continue;

            case scenario::Op::STOP_BGM:
                StopBGM();
                pc++;
                continue;

            case scenario::Op::PLAY_SFX:
                PlaySFX(ins.a);
                pc++;
                continue;

            case scenario::Op::UI_SET:
                ApplyUiSet(ins.a, ins.b);
                pc++;
                continue;

            case scenario::Op::SAVE_MENU:
                stateBeforeSaveMenu = state;
                state = CerekaState::SaveMenu;
                pc++;
                return;

            case scenario::Op::LOAD_MENU:
                stateBeforeSaveMenu = state;
                state = CerekaState::LoadMenu;
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
