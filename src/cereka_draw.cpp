// draw.cpp — every frame rendering (delegates to UIManager)

#include "cereka_engine_impl.hpp"

void Impl::Draw()
{
    m_renderCtx->Clear(Color{0, 0, 0, 255});

    ui.DrawBackground(scene);
    ui.DrawCharacters(scene);
    ui.DrawSceneGraph();

    m_stateMachine.draw();

    if (m_stateMachine.hasOverlays())
        return;

    ui.DrawDialogueBox(dialogue, uiCfg);
}
