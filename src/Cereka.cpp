// Cereka.cpp — engine init/shutdown, event handling, SDL helpers, public API wrapper

#include "cereka_engine_impl.hpp"
#include "renderer/sdl_render_context.hpp"
#include "state/cereka_states.hpp"

using namespace cereka::compiler;
using namespace cereka::video;
using namespace cereka::text_renderer;
using namespace cereka::engine;

// Forward declaration for static helper used in InitGame
static SDL_Renderer *CreateBestRenderer(SDL_Window *win);

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

bool Impl::InitGame(const char *title,
                    int width,
                    int height,
                    bool fullscreen)
{
    video::init_video();
    video::create_window(title, fullscreen, width, height);
    window = video::window;
    screenWidth = video::width;
    screenHeight = video::height;

    text_renderer::init_ttf();

    auto *sdlRenderer = CreateBestRenderer(window);
    if (!sdlRenderer)
        throw engine::error("All renderer attempts failed");

    SDL_SetRenderLogicalPresentation(sdlRenderer, screenWidth, screenHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    m_renderCtx = std::make_unique<SdlRenderContext>(sdlRenderer, screenWidth, screenHeight);

    LoadFont(uiCfg.fontSize);
    ui.SetFont(font);
    InitConfigManager();

    ui.Init(*m_renderCtx);
    scene.Init(*m_renderCtx);
    audio.Init();

    // --- State machine ---
    m_stateMachine.setContext(*this);
    m_stateMachine.registerState<DialogueState>();
    m_stateMachine.registerState<WaitingForInputState>();
    m_stateMachine.registerState<MenuState>();
    m_stateMachine.registerState<FadeState>();
    m_stateMachine.registerState<SaveMenuState>();
    m_stateMachine.registerState<LoadMenuState>();
    m_stateMachine.registerState<HistoryState>();
    m_stateMachine.registerState<FinishedState>();
    m_stateMachine.registerState<QuitState>();
    m_stateMachine.setInitialState(CerekaState::Running);
    return true;
}

void Impl::ShutDown()
{
    auto destroyTex = [](ITexture *&t) {
        delete t;
        t = nullptr;
    };

    destroyTex(uiCfg.textbox.image);
    destroyTex(uiCfg.namebox.image);
    destroyTex(uiCfg.button.image);
    destroyTex(uiCfg.button.hoverImage);

    scene.Shutdown();

    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
    if (m_renderCtx) {
        m_renderCtx.reset();
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    audio.Shutdown();

    TTF_Quit();
    SDL_Quit();
}

// ---------------------------------------------------------------------------
// ICerekaStateContext — state machine interface
// ---------------------------------------------------------------------------

void Impl::changeState(CerekaState newState)
{
    if (m_stateMachine.isInitialized())
        m_stateMachine.changeState(newState);
}

void Impl::pushOverlay(CerekaState overlayState)
{
    if (m_stateMachine.isInitialized())
        m_stateMachine.pushOverlay(overlayState);
}

void Impl::popOverlay()
{
    if (m_stateMachine.isInitialized())
        m_stateMachine.popOverlay();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

bool Impl::PollEvent(cereka::CerekaEvent &e)
{
    SDL_Event sdl;
    if (!SDL_PollEvent(&sdl))
        return false;

    switch (sdl.type) {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        case SDL_EVENT_QUIT:
            e = {cereka::CerekaEvent::Quit, 0};
            return true;
        case SDL_EVENT_KEY_DOWN:
            e = {cereka::CerekaEvent::KeyDown, int(sdl.key.key)};
            return true;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            e.type = cereka::CerekaEvent::MouseDown;
            e.key = 0;
            e.mouseX = sdl.button.x;
            e.mouseY = sdl.button.y;
            return true;
        default:
            e = {cereka::CerekaEvent::Unknown, 0};
            return true;
    }
}

void Impl::Present()
{
    m_renderCtx->Present();
}

// ---------------------------------------------------------------------------
// SDL helpers
// ---------------------------------------------------------------------------

static SDL_Renderer *CreateBestRenderer(SDL_Window *win)
{
    const char *preferred[] = {"gpu", "vulkan", "opengl", "opengles2"};
    for (const char *name : preferred) {
        SDL_Renderer *r = SDL_CreateRenderer(win, name);
        if (r) {
            SDL_Log("Renderer: %s", name);
            if (!SDL_SetRenderVSync(r, 1))
                std::cerr << "[CEREKA] VSync unavailable: " << SDL_GetError() << "\n";
            return r;
        }
    }
    return SDL_CreateRenderer(win, nullptr);
}

void Impl::Say(const std::string &speaker,
               const std::string &name,
               const std::string &text)
{
    dialogue.Show(speaker, name, SubstituteVariables(text));
}

void Impl::Narrate(const std::string &text)
{
    Say("", "", text);
}

std::string Impl::SubstituteVariables(const std::string &text)
{
    std::string result = text;
    size_t pos = 0;
    while ((pos = result.find('{', pos)) != std::string::npos) {
        size_t end = result.find('}', pos);
        if (end == std::string::npos)
            break;
        std::string varName = result.substr(pos + 1, end - pos - 1);
        std::string replacement;
        auto it = scriptInterpreter.numVariables.find(varName);
        if (it != scriptInterpreter.numVariables.end()) {
            int intVal = (int)it->second;
            if (it->second == intVal)
                replacement = std::to_string(intVal);
            else
                replacement = std::to_string(it->second);
        }
        else {
            auto sit = scriptInterpreter.variables.find(varName);
            if (sit != scriptInterpreter.variables.end())
                replacement = sit->second;
        }
        result.replace(pos, end - pos + 1, replacement);
        pos += replacement.length();
    }
    return result;
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------

void Impl::EnterMenu()
{
    std::vector<std::string> texts, targets;
    std::vector<bool> exits;

    size_t scan = scriptInterpreter.pc + 1;
    while (scan < scriptInterpreter.program.size()) {
        const auto &ins = scriptInterpreter.program[scan];

        if (ins.op == compiler::Op::BG || ins.op == compiler::Op::FADE) {
            // Instant swap inside menu — no game loop available to animate
            scene.ShowBackground(ins.a);
            scan++;
        }
        else if (ins.op == compiler::Op::BUTTON) {
            texts.push_back(SubstituteVariables(ins.a));
            targets.push_back(ins.b);
            exits.push_back(ins.exit_button);
            scan++;
        }
        else {
            break;
        }
    }

    menu.Open(std::move(texts), std::move(targets), std::move(exits), scan);
}

void Impl::ExitMenu()
{
    menu.Close();
}

// ---------------------------------------------------------------------------
// Event handling — delegates to state machine
// ---------------------------------------------------------------------------

void Impl::HandleEvent(const CerekaEvent &e)
{
    // Always delegate to the state machine for states to handle
    m_stateMachine.handleEvent(e);

    // Global events handled at engine level regardless of state
    if (e.type == CerekaEvent::Quit) {
        changeState(CerekaState::Quit);
        return;
    }

    // Escape during normal play opens save menu
    if (e.type == CerekaEvent::KeyDown && e.key == SDLK_ESCAPE) {
        auto cur = m_stateMachine.currentType();
        if (cur == CerekaState::WaitingForInput || cur == CerekaState::Running) {
            pushOverlay(CerekaState::SaveMenuState);
            return;
        }
    }

    // H key opens dialogue history
    if (e.type == CerekaEvent::KeyDown && e.key == SDLK_H && rollbackManager.canRollback()) {
        auto cur = m_stateMachine.currentType();
        if (cur == CerekaState::WaitingForInput || cur == CerekaState::Running) {
            pushOverlay(CerekaState::HistoryState);
            return;
        }
    }

    // Advance key: WaitingForInput → Running
    if (m_stateMachine.currentType() == CerekaState::WaitingForInput &&
        (e.type == CerekaEvent::MouseDown ||
         (e.type == CerekaEvent::KeyDown &&
          std::find(uiCfg.advanceKeys.begin(), uiCfg.advanceKeys.end(), (SDL_Keycode)e.key) !=
              uiCfg.advanceKeys.end())))
    {
        changeState(CerekaState::Running);
        return;
    }
}

int Impl::historyHitTest(int mx, int my)
{
    int screenW = m_renderCtx->Width();
    int screenH = m_renderCtx->Height();

    float panelX = screenW * 0.1f;
    float panelY = screenH * 0.05f;
    float panelW = screenW * 0.8f;
    float panelH = screenH * 0.9f;
    float margin = 10.0f;
    float lineH = 40.0f;

    auto texts = rollbackManager.historyTexts();
    float entryY = panelY + 50.0f;

    for (size_t i = 0; i < texts.size() && entryY + lineH < panelY + panelH - 10.0f; ++i) {
        float ex = panelX + margin;
        float ey = entryY;
        float ew = panelW - 2.0f * margin;
        float eh = lineH - 2.0f;

        if ((float)mx >= ex && (float)mx <= ex + ew &&
            (float)my >= ey && (float)my <= ey + eh)
            return (int)i;

        entryY += lineH;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Public CerekaEngine wrapper — thin delegates to Impl
// ---------------------------------------------------------------------------

cereka::CerekaEngine::CerekaEngine() : pImplementation(new Impl()) {}
cereka::CerekaEngine::~CerekaEngine()
{
    delete pImplementation;
}

bool cereka::CerekaEngine::InitGame(const char *title,
                                    int w,
                                    int h,
                                    bool fullscreen)
{
    return pImplementation->InitGame(title, w, h, fullscreen);
}

void cereka::CerekaEngine::ShutDown()
{
    pImplementation->ShutDown();
}

bool cereka::CerekaEngine::PollEvent(CerekaEvent &e)
{
    return pImplementation->PollEvent(e);
}

void cereka::CerekaEngine::Present()
{
    pImplementation->Present();
}
int cereka::CerekaEngine::Width() const
{
    return pImplementation->screenWidth;
}
int cereka::CerekaEngine::Height() const
{
    return pImplementation->screenHeight;
}

void cereka::CerekaEngine::LoadCompiledCerekaScript(const std::vector<compiler::Instruction> &compiled)
{
    pImplementation->LoadCompiledCerekaScript(compiled);
}

void cereka::CerekaEngine::LoadCerekaScript(const std::string &filename)
{
    pImplementation->LoadCerekaScript(filename);
}

void cereka::CerekaEngine::Reset()
{
    pImplementation->Reset();
}
void cereka::CerekaEngine::HandleEvent(const CerekaEvent &e)
{
    pImplementation->HandleEvent(e);
}
void cereka::CerekaEngine::Update(float dt)
{
    pImplementation->Update(dt);
}
void cereka::CerekaEngine::Draw()
{
    pImplementation->Draw();
}

bool cereka::CerekaEngine::InMenu() const
{
    return pImplementation->menu.IsOpen();
}
const std::string &cereka::CerekaEngine::CurrentText() const
{
    return pImplementation->dialogue.Text();
}
size_t cereka::CerekaEngine::ButtonCount() const
{
    return pImplementation->menu.ButtonCount();
}
size_t cereka::CerekaEngine::ProgramCounter() const
{
    return pImplementation->scriptInterpreter.pc;
}

bool cereka::CerekaEngine::IsGameFinished() const
{
    auto s = pImplementation->m_stateMachine.currentType();
    return s == CerekaState::Finished || s == CerekaState::Quit;
}

bool cereka::CerekaEngine::IsGameQuit() const
{
    return pImplementation->m_stateMachine.currentType() == CerekaState::Quit;
}

bool cereka::CerekaEngine::IsFinished() const
{
    return pImplementation->scriptInterpreter.scriptFinished;
}

bool cereka::CerekaEngine::SaveGame(int slot)
{
    return pImplementation->SaveGame(slot);
}

bool cereka::CerekaEngine::LoadGame(int slot)
{
    return pImplementation->LoadGame(slot);
}
