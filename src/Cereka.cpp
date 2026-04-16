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

    if (!MIX_Init()) {
        std::cerr << "[CEREKA] MIX_Init failed: " << SDL_GetError() << "\n";
    }
    else {
        mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!mixer) {
            std::cerr << "[CEREKA] MIX_CreateMixerDevice failed: " << SDL_GetError() << "\n";
            MIX_Quit();
        }
        else {
            audioInitialized = true;
        }
    }

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

    destroyTex(background);
    destroyTex(pendingBg);
    destroyTex(uiCfg.textbox.image);
    destroyTex(uiCfg.namebox.image);
    destroyTex(uiCfg.button.image);
    destroyTex(uiCfg.button.hoverImage);

    for (auto &[id, entry] : characters)
        SDL_DestroyTexture(entry.tex);
    characters.clear();

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

    if (audioInitialized) {
        if (bgmTrack) {
            MIX_StopTrack(bgmTrack, 0);
            MIX_DestroyTrack(bgmTrack);
            bgmTrack = nullptr;
        }
        if (bgmAudio) {
            MIX_DestroyAudio(bgmAudio);
            bgmAudio = nullptr;
        }
        for (auto &[name, audio] : sfxCache)
            MIX_DestroyAudio(audio);
        sfxCache.clear();
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
        MIX_Quit();
        audioInitialized = false;
    }

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

SDL_Texture *Impl::LoadTexture(const std::string &filename)
{
    SDL_Texture *tex = IMG_LoadTexture(renderer, ("assets/bg/" + filename).c_str());
    if (!tex)
        std::cerr << "[CEREKA] Failed to load bg: " << filename << " — " << SDL_GetError() << '\n';
    return tex;
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

// ---------------------------------------------------------------------------
// Scene helpers
// ---------------------------------------------------------------------------

void Impl::ShowBackground(const std::string &filename)
{
    bgPath = filename;
    if (background)
        SDL_DestroyTexture(background);
    background = LoadTexture(filename);
}

float Impl::posToXNorm(const std::string &pos)
{
    if (pos == "left")
        return 0.2f;
    if (pos == "right")
        return 0.8f;
    return 0.5f;
}

void Impl::ShowCharacter(const std::string &id,
                         const std::string &filename,
                         const std::string &pos)
{
    HideCharacter(id);
    charPaths[id] = filename;
    std::string path = "assets/characters/" + filename;
    SDL_Texture *tex = IMG_LoadTexture(renderer, path.c_str());
    if (!tex) {
        std::cerr << "[CEREKA] Failed to load character: " << path << " — " << SDL_GetError()
                  << "\n";
        charPaths.erase(id);
        return;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    characters[id] = {tex, posToXNorm(pos)};
}

void Impl::HideCharacter(const std::string &id)
{
    charPaths.erase(id);
    auto it = characters.find(id);
    if (it != characters.end()) {
        SDL_DestroyTexture(it->second.tex);
        characters.erase(it);
    }
}

void Impl::Say(const std::string &speaker,
               const std::string &name,
               const std::string &text)
{
    currentSpeaker = speaker;
    currentName = name;
    currentText = SubstituteVariables(text);
    displayedChars = 0;
    typewriterTimer = 0.0f;
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
        auto it = numVariables.find(varName);
        if (it != numVariables.end()) {
            int intVal = (int)it->second;
            if (it->second == intVal)
                replacement = std::to_string(intVal);
            else
                replacement = std::to_string(it->second);
        }
        else {
            auto sit = variables.find(varName);
            if (sit != variables.end())
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
    buttonTexts.clear();
    buttonTargets.clear();
    buttonExits.clear();

    size_t scan = pc + 1;
    while (scan < program.size()) {
        const auto &ins = program[scan];

        if (ins.op == scenario::Op::BG) {
            ShowBackground(ins.a);
            scan++;
        }
        else if (ins.op == scenario::Op::FADE) {
            // Instant swap inside menu — no game loop available to animate
            ShowBackground(ins.a);
            scan++;
        }
        else if (ins.op == scenario::Op::BUTTON) {
            buttonTexts.push_back(ins.a);
            buttonTargets.push_back(ins.b);
            buttonExits.push_back(ins.exit_button);
            scan++;
        }
        else {
            break;
        }
    }

    inMenu = true;
    menuEndPC = scan;
}

void Impl::ExitMenu()
{
    inMenu = false;
    buttonTexts.clear();
    buttonTargets.clear();
    buttonExits.clear();
}

int Impl::HitTestButton(int mx,
                        int my)
{
    const float bw = uiCfg.button.w;
    const float bh = uiCfg.button.h;
    float y = screenHeight * 0.4f;
    float x = (float)screenWidth / 2.0f - bw / 2.0f;

    for (size_t i = 0; i < buttonTexts.size(); ++i) {
        if (mx >= x && mx <= x + bw && my >= y && my <= y + bh)
            return (int)i;
        y += bh + 20.0f;
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
    return pImplementation->inMenu;
}
const std::string &cereka::CerekaEngine::CurrentText() const
{
    return pImplementation->currentText;
}
size_t cereka::CerekaEngine::ButtonCount() const
{
    return pImplementation->buttonTexts.size();
}
size_t cereka::CerekaEngine::ProgramCounter() const
{
    return pImplementation->pc;
}

bool cereka::CerekaEngine::IsGameFinished() const
{
    return pImplementation->state == CerekaState::Finished;
}

bool cereka::CerekaEngine::IsFinished() const
{
    return pImplementation->scriptFinished;
}

bool cereka::CerekaEngine::SaveGame(int slot)
{
    return pImplementation->SaveGame(slot);
}

bool cereka::CerekaEngine::LoadGame(int slot)
{
    return pImplementation->LoadGame(slot);
}
