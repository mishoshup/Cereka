#pragma once
#include <string>
#include <vector>

namespace cereka::scenario {

enum class Op {
    BG, CHAR, HIDE_CHAR, SAY, NARRATE, LABEL, JUMP, MENU, BUTTON, END,
    PLAY_BGM, STOP_BGM, PLAY_SFX,
    SET_VAR, IF_EQ, IF_NEQ, ENDIF,
    FADE,
    INCLUDE, CALL, RETURN,
    UI_SET,
    SAVE, LOAD, SAVE_MENU, LOAD_MENU,
};

struct ChoiceOption {
    std::string text;
    std::string targetLabel;
};

struct Instruction {
    Op op;
    std::string a;             // speaker / label / target / id
    std::string b;             // text / filename / value / duration
    std::string c;             // aux field (char position: left|center|right)
    bool exit_button = false;
    std::vector<ChoiceOption> choices;
};

std::vector<Instruction> CompileVNScript(const std::string &filename);

}  // namespace cereka::scenario
