#include <gtest/gtest.h>
#include "cereka_engine_impl.hpp"
#include "state/cereka_states.hpp"

using namespace cereka;
using namespace cereka::compiler;

class VMTest : public ::testing::Test {
protected:
    Impl engine;
    
    void SetUp() override {
        engine.m_stateMachine.setContext(engine);
        engine.m_stateMachine.registerState<DialogueState>();
        engine.m_stateMachine.registerState<WaitingForInputState>();
        engine.m_stateMachine.registerState<MenuState>();
        engine.m_stateMachine.registerState<FadeState>();
        engine.m_stateMachine.registerState<SaveMenuState>();
        engine.m_stateMachine.registerState<LoadMenuState>();
        engine.m_stateMachine.registerState<FinishedState>();
        engine.m_stateMachine.registerState<QuitState>();
        engine.m_stateMachine.setInitialState(CerekaState::Running);
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

    engine.LoadCompiledCerekaScript(program);

    // Set variables AFTER LoadCompiledCerekaScript (which clears them)
    engine.scriptInterpreter.variables["var"] = "0";
    engine.scriptInterpreter.variables["var2"] = "2";

    // State machine is already initialized to Running in SetUp

    // Expected: outer IF is false → skipMode=true, skipDepth=1
    //   inner IF (skipped) → skipDepth=2
    //   SAY "nested" (skipped)
    //   ELSE (skipped, skipDepth==2 so must NOT exit skipMode)
    //   ENDIF → skipDepth=1
    //   ENDIF → skipDepth=0, skipMode=false
    //   SAY "after" → executed

    engine.m_stateMachine.update(1.0f / 60.0f);

    EXPECT_EQ(engine.m_stateMachine.currentType(), CerekaState::WaitingForInput);
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

    engine.LoadCompiledCerekaScript(program);

    // Set variables AFTER LoadCompiledCerekaScript (which clears them)
    engine.scriptInterpreter.variables["var"] = "1";

    // State machine is already initialized to Running in SetUp

    // Tick 1: IF is true → execute SAY "inside if", return WaitingForInput
    engine.m_stateMachine.update(1.0f / 60.0f);
    EXPECT_EQ(engine.dialogue.Text(), "inside if");

    // Tick 2: ELSE → skipMode=true (IF branch was taken, skip ELSE block)
    //   SAY "inside else" (skipped)
    //   ENDIF → skipMode=false
    //   SAY "after" → executed
    engine.m_stateMachine.changeState(CerekaState::Running);
    engine.m_stateMachine.update(1.0f / 60.0f);
    EXPECT_EQ(engine.dialogue.Text(), "after");
}

TEST_F(VMTest, DeeplyNestedIfElse) {
    // Three levels of nesting, outer is false.
    // No instruction inside the skipped block should execute,
    // BUT the ELSE body for the outermost IF should execute.
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

    engine.LoadCompiledCerekaScript(program);
    engine.scriptInterpreter.variables["outer"] = "no";
    engine.scriptInterpreter.variables["mid"] = "yes";
    engine.scriptInterpreter.variables["inner"] = "yes";

    // Tick 1: ELSE at depth 1 exits skip mode → "outer else" executes
    engine.m_stateMachine.update(1.0f / 60.0f);
    EXPECT_EQ(engine.m_stateMachine.currentType(), CerekaState::WaitingForInput);
    EXPECT_EQ(engine.dialogue.Text(), "outer else");

    // Tick 2: continue past ENDIF → "after all" executes
    engine.m_stateMachine.changeState(CerekaState::Running);
    engine.m_stateMachine.update(1.0f / 60.0f);
    EXPECT_EQ(engine.m_stateMachine.currentType(), CerekaState::WaitingForInput);
    EXPECT_EQ(engine.dialogue.Text(), "after all");
}
