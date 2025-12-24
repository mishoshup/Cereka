
#include "Cereka/Cereka.hpp"
#include "text_renderer.hpp"
#include "video.hpp"
#include "vn_instruction.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <sol/sol.hpp>
#include <unordered_map>

using namespace cereka;

class CerekaEngine::Implementation {

   public:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    int screenWidth = 0;
    int screenHeight = 0;

    TTF_Font *font = nullptr;
    SDL_Texture *background = nullptr;
    SDL_Texture *textBox = nullptr;
    SDL_Texture *nameBox = nullptr;
    SDL_Texture *buttonTexture = nullptr;
    std::unordered_map<std::string, SDL_Texture *> characters;

    sol::state lua;
    sol::coroutine script;
    std::vector<cereka::scenario::Instruction> program;
    std::unordered_map<std::string, size_t> labelMap;
    size_t pc = 0;
    size_t menuEndPC = 0;
    bool scriptFinished = false;

    std::string currentSpeaker;
    std::string currentName;
    std::string currentText;
    float typewriterTimer = 0.0f;
    int displayedChars = 0;
    static constexpr float CHARS_PER_SECOND = 60.0f;

    // Menu state
    bool inMenu = false;
    std::vector<std::string> buttonTexts;
    std::vector<std::string> buttonTargets;
    std::vector<bool> buttonExits;

    CerekaState state = CerekaState::Running;

    bool InitGame(const char *title,
                  int width,
                  int height,
                  bool fullscreen)
    {
        // Video Related Functions
        video::init_video();
        video::create_window(title, fullscreen, width, height);
        this->window = video::window;
        this->screenWidth = video::width;
        this->screenHeight = video::height;

        text_renderer::init_ttf();
        this->font = text_renderer::OpenFont("assets/fonts/Montserrat-Medium.ttf", 36);

        this->renderer = CreateBestRenderer(this->window);
        if (!this->renderer) {
            throw engine::error("All renderer attempts failed\n");
        }

        this->textBox = CreateSolidTexture(
            screenWidth, static_cast<int>(screenHeight * 0.25f), 0, 0, 0, 130);

        this->nameBox = CreateSolidTexture(300, 60, 0, 255, 0, 255);

        this->buttonTexture = CreateSolidTexture(600, 80, 0, 255, 255, 255);
        if (!this->buttonTexture) {
            throw engine::error("buttonTexture NULL – CreateSolidTexture failed: %s",
                                SDL_GetError());
        }

        return true;
    }

    void ShutDown()
    {
        if (this->background) {
            SDL_DestroyTexture(this->background);
            this->background = nullptr;
        }
        if (this->textBox) {
            SDL_DestroyTexture(this->textBox);
            this->textBox = nullptr;
        }
        if (this->nameBox) {
            SDL_DestroyTexture(this->nameBox);
            this->nameBox = nullptr;
        }

        for (auto &[id, tex] : this->characters) {
            SDL_DestroyTexture(tex);
        }
        this->characters.clear();

        if (this->font) {
            TTF_CloseFont(this->font);
            this->font = nullptr;
        }
        if (this->renderer) {
            SDL_DestroyRenderer(this->renderer);
            this->renderer = nullptr;
        }
        if (this->window) {
            SDL_DestroyWindow(this->window);
            this->window = nullptr;
        }

        TTF_Quit();
        SDL_Quit();
    }

    bool PollEvent(CerekaEvent &e)
    {
        SDL_Event sdl;
        if (!SDL_PollEvent(&sdl))
            return false;

        switch (sdl.type) {
            case SDL_EVENT_QUIT:
                e = {CerekaEvent::Quit, 0};
                return true;
            case SDL_EVENT_KEY_DOWN:
                e = {CerekaEvent::KeyDown, int(sdl.key.key)};
                return true;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                e.type = CerekaEvent::MouseDown;
                e.key = 0;
                e.mouseX = sdl.button.x;
                e.mouseY = sdl.button.y;
                return true;
                return true;
            default:
                e = {CerekaEvent::Unknown, 0};
                return true;
        }
    }

    void Present()
    {
        SDL_RenderPresent(this->renderer);
    }

    void HandleEvent(const CerekaEvent &e)
    {
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

            if (!buttonTargets[idx].empty())
                pc = labelMap[buttonTargets[idx]];
            else
                pc = menuEndPC;  //

            ExitMenu();
            state = CerekaState::Running;
        }
    }
    void TickScript()
    {
        if (state != CerekaState::Running)
            return;

        while (pc < program.size()) {
            const auto &ins = program[pc];

            switch (ins.op) {

                case scenario::Op::BG:
                    ShowBackground(ins.a);
                    pc++;
                    continue;

                case scenario::Op::CHAR:
                    ShowCharacter(ins.a, ins.b);
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

    void EnterMenu()
    {
        buttonTexts.clear();
        buttonTargets.clear();
        buttonExits.clear();

        // Kita execute semua benda dalam menu block serta-merta
        size_t scan = pc + 1;  // mula selepas MENU

        while (scan < program.size()) {
            const auto &ins = program[scan];

            if (ins.op == scenario::Op::BG) {
                ShowBackground(ins.a);  // ← LOAD BACKGROUND DALAM MENU!
                scan++;
            }
            else if (ins.op == scenario::Op::BUTTON) {
                buttonTexts.push_back(ins.a);
                buttonTargets.push_back(ins.b);
                buttonExits.push_back(ins.exit_button);
                scan++;
            }
            else {
                // Bukan BG atau BUTTON → tamat menu block
                break;
            }
        }

        inMenu = true;
        this->menuEndPC = scan;

        std::cout << "[MENU] Loaded background and " << buttonTexts.size() << " buttons!"
                  << std::endl;
    }

    void Update(float dt)
    {
        if (currentText.empty())
            return;

        if (displayedChars < (int)currentText.length()) {
            typewriterTimer += dt;
            int charsToAdd = (int)(typewriterTimer * CHARS_PER_SECOND);
            if (charsToAdd > 0) {
                displayedChars += charsToAdd;
                typewriterTimer -= charsToAdd / CHARS_PER_SECOND;
                if (displayedChars > (int)currentText.length())
                    displayedChars = currentText.length();
            }
        }
    }

    void Draw()
    {
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_RenderClear(renderer);

        printf("Draw: inMenu=%d  buttons=%zu\n", inMenu, buttonTexts.size());
        if (background) {
            SDL_RenderTexture(renderer, background, nullptr, nullptr);
        }

        // Draw characters
        float xPos = screenWidth * 0.1f;
        const float spacing = screenWidth * 0.3f;
        for (const auto &[id, tex] : characters) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(tex, &tw, &th);
            float scale = (screenHeight * 0.8f) / th;
            SDL_FRect dst{
                xPos, screenHeight - th * scale - screenHeight * 0.1f, tw * scale, th * scale};
            SDL_RenderTexture(renderer, tex, nullptr, &dst);
            xPos += spacing;
        }
        printf("Draw: inMenu=%d  buttons=%zu\n", inMenu, buttonTexts.size());
        // Menu buttons
        if (inMenu) {
            float y = screenHeight * 0.4f;
            const float h = 80, spacing = 20;
            for (size_t i = 0; i < buttonTexts.size(); ++i) {
                SDL_FRect btn{float(screenWidth) / 2 - 300, y, 600, h};
                SDL_RenderTexture(renderer, buttonTexture, nullptr, &btn);

                auto textTex = RenderText(buttonTexts[i], {255, 255, 255, 255});
                if (textTex) {
                    float tw, th;
                    SDL_GetTextureSize(textTex, &tw, &th);
                    SDL_FRect tr{float(screenWidth) / 2 - tw / 2, y + 40 - th / 2, tw, th};
                    SDL_RenderTexture(renderer, textTex, nullptr, &tr);
                    SDL_DestroyTexture(textTex);
                }
                y += h + spacing;
            }
        }
        printf("Draw: inMenu=%d  buttons=%zu\n", inMenu, buttonTexts.size());
        // Dialogue box
        if (!currentText.empty()) {
            if (textBox) {
                SDL_FRect tb{0, screenHeight * 0.75f, (float)screenWidth, screenHeight * 0.25f};
                SDL_RenderTexture(renderer, textBox, nullptr, &tb);
            }
            if (!currentSpeaker.empty() && nameBox) {
                SDL_FRect nb{50, screenHeight * 0.75f - 70, 300, 60};
                SDL_RenderTexture(renderer, nameBox, nullptr, &nb);
                auto nameTex = RenderText(currentName, {255, 255, 255, 255});
                if (nameTex) {
                    float w, h;
                    SDL_GetTextureSize(nameTex, &w, &h);
                    SDL_FRect dst{70, screenHeight * 0.751f - 60, w, h};
                    SDL_RenderTexture(renderer, nameTex, nullptr, &dst);
                    SDL_DestroyTexture(nameTex);
                }
            }

            std::string visible = currentText.substr(0, displayedChars);
            auto textTex = RenderText(visible, {255, 255, 255, 255});
            if (textTex) {
                float w, h;
                SDL_GetTextureSize(textTex, &w, &h);
                float margin = 70;
                float maxW = screenWidth - 2 * margin;
                float scale = w > maxW ? maxW / w : 1.0f;
                SDL_FRect dst{margin, screenHeight * 0.80f, w * scale, h * scale};
                SDL_RenderTexture(renderer, textTex, nullptr, &dst);
                SDL_DestroyTexture(textTex);
            }
        }
    }

    // Private helpers
    SDL_Renderer *CreateBestRenderer(SDL_Window *window)
    {
        const char *preferred_drivers[] = {"gpu", "vulkan", "opengl", "opengles2"};
        const int num_preferred = sizeof(preferred_drivers) / sizeof(preferred_drivers[0]);

        for (int i = 0; i < num_preferred; ++i) {
            const char *name = preferred_drivers[i];
            SDL_Renderer *renderer = SDL_CreateRenderer(window, name);
            if (renderer) {
                SDL_Log("Successfully created renderer: %s", name);

                if (SDL_SetRenderVSync(renderer, 1)) {
                    SDL_Log("VSync enabled successfully.");
                }
                else {
                    std::cerr << "Warning: VSync failed (" << SDL_GetError()
                              << "), continuing without it.\n";
                }

                return renderer;
            }
            else {
                std::cout << "Failed to create renderer '" << name << "': " << SDL_GetError()
                          << "\n";
            }
        }

        SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer) {
            std::cout << "Fallback renderer created.\n";
        }
        return renderer;
    }

    SDL_Texture *LoadTexture(const std::string &path)
    {
        SDL_Texture *tex = IMG_LoadTexture(this->renderer, ("assets/bg/" + path).c_str());
        if (!tex)
            std::cerr << "Failed to load bg: " << path << " - " << SDL_GetError() << '\n';
        return tex;
    }

    void LoadCompiledScript(const std::vector<scenario::Instruction> &compiled)
    {
        program = compiled;
        pc = 0;
        scriptFinished = false;

        // build label map
        labelMap.clear();
        for (size_t i = 0; i < program.size(); ++i) {
            if (program[i].op == scenario::Op::LABEL)
                labelMap[program[i].a] = i;
        }

        // auto-start at first menu
        for (size_t i = 0; i < program.size(); ++i) {
            if (program[i].op == scenario::Op::MENU) {
                pc = i;
                break;
            }
        }
    }

    void AdvanceScriptOnce()
    {
        if (scriptFinished || pc >= program.size())
            return;

        const scenario::Instruction &ins = program[pc];
        printf("Advance pc=%zu op=%d  a='%s'\n", pc, static_cast<int>(ins.op), ins.a.c_str());

        switch (ins.op) {
            case scenario::Op::BG:
                ShowBackground(ins.a);
                pc++;
                break;
            case scenario::Op::CHAR:
                ShowCharacter(ins.a, ins.b);
                pc++;
                break;
            case scenario::Op::SAY:
                Say(ins.a, ins.a, ins.b);
                pc++;
                break;
            case scenario::Op::NARRATE:
                Narrate(ins.b);
                pc++;
                break;
            case scenario::Op::BUTTON:
                buttonTexts.push_back(ins.a);
                buttonTargets.push_back(ins.b);
                buttonExits.push_back(ins.exit_button);
                printf("BUTTON pushed  txt=%s  btn.size=%zu\n", ins.a.c_str(), buttonTexts.size());
                pc++;
                break;
                break;
            case scenario::Op::JUMP:
                if (labelMap.count(ins.a)) {
                    pc = labelMap[ins.a];
                }
                else {
                    std::cerr << "[ERROR] Unknown label: " << ins.a << "\n";
                    pc++;
                }
                break;
            case scenario::Op::END:
                scriptFinished = true;
                break;
            default:
                pc++;
                break;
        }
    }

    void ShowBackground(const std::string &f)
    {
        if (this->background) {
            SDL_DestroyTexture(this->background);
        }
        this->background = LoadTexture(f);
    }

    void ShowCharacter(const std::string &id,
                       const std::string &)
    {
        HideCharacter(id);
        std::string path = "assets/characters/" + id + "_normal.jpg";
        SDL_Texture *tex = IMG_LoadTexture(this->renderer, path.c_str());
        if (tex)
            this->characters[id] = tex;
    }

    void HideCharacter(const std::string &id)
    {
        auto it = this->characters.find(id);
        if (it != this->characters.end()) {
            SDL_DestroyTexture(it->second);
            this->characters.erase(it);
        }
    }

    void Say(const std::string &speaker,
             const std::string &name,
             const std::string &text)
    {
        this->currentSpeaker = speaker;
        this->currentName = name;
        this->currentText = text;
        this->displayedChars = 0;
        this->typewriterTimer = 0.0f;
    }

    void Narrate(const std::string &text)
    {
        Say("", "Narrator", text);
    }

    SDL_Texture *CreateSolidTexture(int w,
                                    int h,
                                    Uint8 r,
                                    Uint8 g,
                                    Uint8 b,
                                    Uint8 a)
    {
        SDL_Texture *texture = SDL_CreateTexture(
            this->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
        if (!texture) {
            return nullptr;
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(this->renderer, texture);
        SDL_SetRenderDrawColor(this->renderer, r, g, b, a);
        SDL_RenderClear(this->renderer);
        SDL_SetRenderTarget(this->renderer, nullptr);
        return texture;
    }

    SDL_Texture *RenderText(const std::string &text,
                            SDL_Color color)
    {
        if (text.empty() || !this->font)
            return nullptr;
        SDL_Surface *surf = TTF_RenderText_Blended(this->font, text.c_str(), text.size(), color);
        if (!surf)
            return nullptr;
        SDL_Texture *tex = SDL_CreateTextureFromSurface(this->renderer, surf);
        SDL_DestroySurface(surf);
        return tex;
    }
    void ExitMenu()
    {
        inMenu = false;
        buttonTexts.clear();
        buttonTargets.clear();
        buttonExits.clear();
        std::cout << "[MENU] Exited menu, buttons cleared." << std::endl;
    }
    void LoadScript(const std::string &filename)
    {
        std::cout << "[DEBUG] Loading script: " << filename << std::endl;
        sol::load_result chunk = lua.load_file(filename);
        if (!chunk.valid()) {
            sol::error err = chunk;
            std::cerr << "[ERROR] Lua load error in " << filename << ": " << err.what() << "\n";
            return;
        }
        script = sol::coroutine(chunk);
        scriptFinished = false;
        std::cout << "[DEBUG] Script loaded successfully, starting execution..." << std::endl;
        std::cout << "[DEBUG] Initial run complete. Status: " << (int)script.status();
    }

    void Reset()
    {
        this->currentText.clear();
        this->displayedChars = 0;
        this->typewriterTimer = 0.0f;
        this->currentSpeaker.clear();
        this->currentName.clear();
        if (this->background) {
            SDL_DestroyTexture(this->background);
            this->background = nullptr;
        }
        this->characters.clear();
    }

    int HitTestButton(int mx,
                      int my)
    {
        float y = screenHeight * 0.4f;
        const float h = 80;
        const float spacing = 20;
        float x = screenWidth / 2.0f - 300;

        for (size_t i = 0; i < buttonTexts.size(); ++i) {
            SDL_FRect r{x, y, 600, h};
            if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                return (int)i;
            }
            y += h + spacing;
        }
        return -1;
    }
};

CerekaEngine::CerekaEngine() : pImplementation(new Implementation()) {}

CerekaEngine::~CerekaEngine()
{
    delete pImplementation;
}

bool CerekaEngine::InitGame(const char *title,
                            int w,
                            int h,
                            bool fullscreen)
{
    return pImplementation->InitGame(title, w, h, fullscreen);
}

void CerekaEngine::ShutDown()
{
    pImplementation->ShutDown();
}

bool CerekaEngine::PollEvent(CerekaEvent &e)
{
    return pImplementation->PollEvent(e);
}

void CerekaEngine::Present()
{
    pImplementation->Present();
}

int CerekaEngine::Width() const
{
    return pImplementation->screenWidth;
}

int CerekaEngine::Height() const
{
    return pImplementation->screenHeight;
}

void CerekaEngine::LoadScript(const std::string &filename)
{
    pImplementation->LoadScript(filename);
}

void CerekaEngine::Reset()
{
    pImplementation->Reset();
}

void CerekaEngine::HandleEvent(const CerekaEvent &e)
{
    pImplementation->HandleEvent(e);
}

void CerekaEngine::Update(float dt)
{
    pImplementation->Update(dt);
}

void CerekaEngine::Draw()
{
    pImplementation->Draw();
}

bool CerekaEngine::IsFinished() const
{
    return pImplementation->scriptFinished;
}

bool CerekaEngine::IsGameFinished() const
{
    return pImplementation->state == CerekaState::Finished;
}

void CerekaEngine::LoadCompiledScript(const std::vector<scenario::Instruction> &compiled)
{
    pImplementation->LoadCompiledScript(compiled);
}

void CerekaEngine::AdvanceScriptOnce()
{
    pImplementation->AdvanceScriptOnce();
}
bool CerekaEngine::InMenu() const
{
    return pImplementation->inMenu;
}

const std::string &CerekaEngine::CurrentText() const
{
    return pImplementation->currentText;
}

size_t CerekaEngine::ButtonCount() const
{
    return pImplementation->buttonTexts.size();
}

size_t CerekaEngine::ProgramCounter() const
{
    return pImplementation->pc;
}

void CerekaEngine::TickScript()
{
    return pImplementation->TickScript();
}
