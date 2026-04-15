#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace cereka::scenario {

enum class Op { BG, CHAR, HIDE_CHAR, SAY, NARRATE, LABEL, JUMP, MENU, BUTTON, END, PLAY_BGM, STOP_BGM, PLAY_SFX, SET_VAR, IF_EQ, IF_NEQ, ENDIF };

struct ChoiceOption {
    std::string text;
    std::string targetLabel;
};

struct Instruction {
    Op op;
    std::string a;             // generic field (speaker / label / target)
    std::string b;             // generic field (text)
    bool exit_button = false;  // ← this name
    std::vector<ChoiceOption> choices;
};

std::vector<Instruction> CompileVNScript(const std::string &filename);

}  // namespace cereka::scenario
