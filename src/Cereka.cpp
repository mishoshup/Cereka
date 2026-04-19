// Cereka.cpp — engine init/shutdown, event handling, SDL helpers, public API wrapper

#include "engine_impl.hpp"

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

    renderer = CreateBestRenderer(window);
    if (!renderer)
        throw engine::error("All renderer attempts failed");

    LoadFont(uiCfg.fontSize);
    InitConfigManager();

    scene.Init(renderer);
    audio.Init();
    return true;
}

void Impl::ShutDown()
{
    auto destroyTex = [](SDL_Texture *&t) {
        if (t) {
            SDL_DestroyTexture(t);
            t = nullptr;
        }
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
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
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
    SDL_RenderPresent(renderer);
}

// ---------------------------------------------------------------------------
// SDL helpers
// ---------------------------------------------------------------------------

SDL_Renderer *Impl::CreateBestRenderer(SDL_Window *win)
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

SDL_Texture *Impl::RenderText(const std::string &text,
                              SDL_Color color)
{
    if (text.empty() || !font)
        return nullptr;
    SDL_Surface *surf = TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
    if (!surf)
        return nullptr;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
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

        if (ins.op == scenario::Op::BG || ins.op == scenario::Op::FADE) {
            // Instant swap inside menu — no game loop available to animate
            scene.ShowBackground(ins.a);
            scan++;
        }
        else if (ins.op == scenario::Op::BUTTON) {
            texts.push_back(ins.a);
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

void cereka::CerekaEngine::LoadCompiledScript(const std::vector<scenario::Instruction> &compiled)
{
    pImplementation->LoadCompiledScript(compiled);
}

void cereka::CerekaEngine::LoadScript(const std::string &filename)
{
    pImplementation->LoadScript(filename);
}

void cereka::CerekaEngine::TickScript()
{
    pImplementation->TickScript();
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
    return pImplementation->state == CerekaState::Finished ||
           pImplementation->state == CerekaState::Quit;
}

bool cereka::CerekaEngine::IsGameQuit() const
{
    return pImplementation->state == CerekaState::Quit;
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
