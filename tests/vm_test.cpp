#include <gtest/gtest.h>
#include "engine_impl.hpp"

using namespace cereka;
using namespace cereka::scenario;

class VMTest : public ::testing::Test {
protected:
    Impl engine;
    
    void SetUp() override {
        // Mock minimal state if needed
    }
};

TEST_F(VMTest, NestedIfElseBug) {
    // if(false) {
    //   if(true) { say "nested"; }
    //   else { say "nested else"; }
    // }
    // say "after";

    std::vector<Instruction> program = {
        {Op::IF_EQ, "var", "1"}, // if (var == "1") -- initially false
            {Op::IF_EQ, "var2", "2"}, // if (var2 == "2") -- true
                {Op::SAY, "speaker", "nested"},
            {Op::ELSE},
                {Op::SAY, "speaker", "nested else"},
            {Op::ENDIF},
        {Op::ENDIF},
        {Op::SAY, "speaker", "after"},
        {Op::END}
    };

    engine.LoadCompiledScript(program);

    // Set variables AFTER LoadCompiledScript (which clears them)
    engine.scriptInterpreter.variables["var"] = "0";
    engine.scriptInterpreter.variables["var2"] = "2";

    engine.state = CerekaState::Running;

    // Expected: outer IF is false → skipMode=true, skipDepth=1
    //   inner IF (skipped) → skipDepth=2
    //   SAY "nested" (skipped)
    //   ELSE (skipped, skipDepth==2 so must NOT exit skipMode)
    //   ENDIF → skipDepth=1
    //   ENDIF → skipDepth=0, skipMode=false
    //   SAY "after" → executed

    engine.TickScript();

    EXPECT_EQ(engine.state, CerekaState::WaitingForInput);
    EXPECT_EQ(engine.dialogue.Text(), "after");
}

TEST_F(VMTest, IfTrueElseSkipped) {
    std::vector<Instruction> program = {
        {Op::IF_EQ, "var", "1"}, // if (var == "1") -- true
            {Op::SAY, "speaker", "inside if"},
        {Op::ELSE},
            {Op::SAY, "speaker", "inside else"},
        {Op::ENDIF},
        {Op::SAY, "speaker", "after"},
        {Op::END}
    };

    engine.LoadCompiledScript(program);

    // Set variables AFTER LoadCompiledScript (which clears them)
    engine.scriptInterpreter.variables["var"] = "1";

    engine.state = CerekaState::Running;

    // Tick 1: IF is true → execute SAY "inside if", return WaitingForInput
    engine.TickScript();
    EXPECT_EQ(engine.dialogue.Text(), "inside if");

    // Tick 2: ELSE → skipMode=true (IF branch was taken, skip ELSE block)
    //   SAY "inside else" (skipped)
    //   ENDIF → skipMode=false
    //   SAY "after" → executed
    engine.state = CerekaState::Running;
    engine.TickScript();
    EXPECT_EQ(engine.dialogue.Text(), "after");
}

TEST_F(VMTest, DeeplyNestedIfElse) {
    // Three levels of nesting, outer is false.
    // No instruction inside the skipped block should execute.
    std::vector<Instruction> program = {
        {Op::IF_EQ, "outer", "yes"},      // false
            {Op::IF_EQ, "mid", "yes"},     // skipped
                {Op::IF_EQ, "inner", "yes"}, // skipped
                    {Op::SAY, "s", "deep"},
                {Op::ELSE},
                    {Op::SAY, "s", "deep else"},
                {Op::ENDIF},
            {Op::ELSE},
                {Op::SAY, "s", "mid else"},
            {Op::ENDIF},
        {Op::ELSE},
            {Op::SAY, "s", "outer else"},
        {Op::ENDIF},
        {Op::SAY, "s", "after all"},
        {Op::END}
    };

    engine.LoadCompiledScript(program);
    engine.scriptInterpreter.variables["outer"] = "no";
    engine.scriptInterpreter.variables["mid"] = "yes";
    engine.scriptInterpreter.variables["inner"] = "yes";
    engine.state = CerekaState::Running;

    engine.TickScript();

    EXPECT_EQ(engine.state, CerekaState::WaitingForInput);
    EXPECT_EQ(engine.dialogue.Text(), "after all");
}
