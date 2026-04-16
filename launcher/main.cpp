#include "embedded_assets.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// Embedded project templates
// ============================================================================

static const char *kGameCfgTemplate = R"CRKA(# Cereka game configuration
# -----------------------------------------------
# title      : window title shown in the taskbar
# width/height: resolution in pixels
# fullscreen  : true or false
# entry       : path to the first script to run
# -----------------------------------------------
title      = My Visual Novel
width      = 1280
height     = 720
fullscreen = false
entry      = assets/scripts/main.crka
)CRKA";

// Starter script — uses the bundled placeholder assets so the game runs
// immediately out of the box. Every command is shown with comments.
// ---------------------------------------------------------------------------
// ui.crka — UI theme. Included by main.crka.
// ---------------------------------------------------------------------------
static const char *kUiScriptTemplate = R"CRKA(; ================================================================
; ui.crka — Cereka UI theme
;
; Included at the top of main.crka via:  include ui.crka
; Change any value here to restyle the whole game with no recompile.
;
; Positions:  use % for screen-relative (75%) or raw pixels (540)
; Colors:     r g b a  (0–255 each)
; Images:     path relative to project root — omit to use solid color
; ================================================================

; ---- Dialogue text box (bottom area) ----
ui textbox
    color          0 0 0 160
    y              75%
    h              25%
    text_margin_x  80
    text_color     255 255 255 255
;   image  assets/ui/textbox.png   ; uncomment to use a custom image

; ---- Speaker name box ----
ui namebox
    color       30 30 100 255
    x           50
    y_offset    -65
    w           260
    h           52
    text_color  255 220 120 255
;   image  assets/ui/namebox.png

; ---- Choice buttons ----
ui button
    color       20 80 120 255
    w           560
    h           72
    text_color  255 255 255 255
;   image        assets/ui/button.png
;   hover_image  assets/ui/button_hover.png

; ---- Font ----
ui font
    size  36
)CRKA";

// ---------------------------------------------------------------------------
// scene_two.crka — called from main.crka to demo multi-file scripts.
// ---------------------------------------------------------------------------
static const char *kSceneTwoTemplate = R"CRKA(; ================================================================
; scene_two.crka — a separate scene file
;
; Called from main.crka with:   call scene_two.crka
; The engine returns to main.crka automatically when this ends.
;
; Use 'call' for scenes that need to return (subroutines).
; Use 'include' to inline a file at compile time (no return).
; ================================================================

char Bob right placeholder_char.png
say Bob "I am Bob, loaded from scene_two.crka!"
say Bob "When this scene ends, the engine returns to main.crka."
hide char Bob
)CRKA";

// ---------------------------------------------------------------------------
// main.crka — entry point and full command tutorial.
// ---------------------------------------------------------------------------
static const char *kMainScriptTemplate = R"CRKA(; ================================================================
; main.crka — Cereka entry point
;
; This file is a full tutorial. Every command is shown with comments.
; Delete what you don't need and start writing your story.
; ================================================================

; INCLUDE — inline another script at compile time.
; Use it to load your UI theme, shared labels, etc.
; Syntax: include <filename>   (relative to this file's directory)
include ui.crka

label start

; BGM — background music, loops forever until stop_bgm.
; Syntax: bgm <filename>
bgm placeholder_bgm.wav

; BG — show a background image instantly.
; Syntax: bg <filename>
bg placeholder_bg.png

; NARRATE — narrator text, no speaker name shown.
; Click or press a key to advance.
narrate "Welcome to Cereka!"
narrate "Every command in the engine is shown in this script."

; BG FADE — crossfade to a new background over <seconds>.
; The script pauses until the transition completes.
; Syntax: bg <filename> fade <seconds>
bg placeholder_bg.png fade 1.0
narrate "That background faded in."

; CHAR — show a character sprite.
; Position: left, center (default), or right.
; Syntax: char <ID> [left|center|right] <filename>
char Alice center placeholder_char.png

; SAY — dialogue with a speaker name box.
; Syntax: say <ID> "text"
say Alice "Hi! I am loaded from assets/characters/"
say Alice "My position can be left, center, or right."

; SFX — one-shot sound effect. Does NOT pause the script.
; Syntax: sfx <filename>
sfx placeholder_sfx.wav

; CALL — run another script file as a subroutine and return here.
; Syntax: call <filename>
call scene_two.crka

narrate "Returned from scene_two.crka."

; SET — store a string variable.
; Syntax: set <variable> <value>
set choice none

; MENU — present choices. Indent bg/button lines inside.
;   button "Label" goto <label>  — jump to label on click
;   button "Label" exit          — quit the game
menu
    bg placeholder_bg.png fade 0.5
    button "Explore"   goto explore
    button "Skip"      goto the_end
    button "Quit"      exit

label explore

; HIDE CHAR — remove a character sprite.
; Syntax: hide char <ID>
hide char Alice

; IF / ENDIF — conditional block. Operators: == and !=
set choice explored

if choice == explored
    narrate "You chose to explore!"
endif

if choice != skipped
    narrate "This shows because choice != skipped."
endif

; JUMP — unconditional jump to a label.
; Syntax: jump <label>
jump the_end


label the_end

; STOP_BGM — stop background music.
stop_bgm

narrate "That covers every Cereka command."
narrate "Project layout:"
narrate "  assets/scripts/     .crka script files"
narrate "  assets/bg/          backgrounds"
narrate "  assets/characters/  sprites"
narrate "  assets/sounds/      music and sfx"
narrate "  assets/fonts/       .ttf / .otf fonts"
narrate "  game.cfg            title, resolution, entry point"
narrate "  ui.crka             UI theme (colors, sizes, images)"

; END — marks the end of the script.
end
)CRKA";

// ============================================================================
// State
// ============================================================================

enum class Screen { Home, Project };

static Screen       s_screen     = Screen::Home;
static fs::path     s_selectedDir;
static bool         s_hasGameCfg = false;
static std::string  s_gameTitle;

// Directory text input
static char         s_dirInput[2048] = "";

// Directory browser popup
static bool                     s_openBrowser = false;
static fs::path                 s_browserPath;
static std::vector<std::string> s_browserDirs;

// Log / busy
static std::string           s_log;
static std::mutex            s_logMutex;
static std::atomic<bool>     s_busy{false};
static bool                  s_scrollToBottom = false;

// Signal from background thread to reload project on main thread
static std::atomic<bool>     s_reloadPending{false};

// Recent projects
static std::vector<std::string> s_recents;
static fs::path                 s_recentsFile;

// ============================================================================
// Log helpers
// ============================================================================

static void appendLog(const std::string &text)
{
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_log += text;
    s_scrollToBottom = true;
}

static void clearLog()
{
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_log.clear();
}

// ============================================================================
// Recents
// ============================================================================

static void saveRecents()
{
    std::ofstream f(s_recentsFile);
    for (auto &p : s_recents) f << p << "\n";
}

static void loadRecents()
{
    const char *home = getenv("HOME");
    s_recentsFile = fs::path(home ? home : ".") / ".cereka_recent";
    std::ifstream f(s_recentsFile);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && fs::is_directory(line))
            s_recents.push_back(line);
    }
}

static void addRecent(const std::string &path)
{
    auto it = std::find(s_recents.begin(), s_recents.end(), path);
    if (it != s_recents.end()) s_recents.erase(it);
    s_recents.insert(s_recents.begin(), path);
    if (s_recents.size() > 8) s_recents.resize(8);
    saveRecents();
}

// ============================================================================
// Directory browser
// ============================================================================

static void refreshBrowser(const fs::path &p)
{
    s_browserPath = p;
    s_browserDirs.clear();
    std::error_code ec;
    for (auto &e : fs::directory_iterator(p, ec)) {
        if (!e.is_directory(ec)) continue;
        std::string name = e.path().filename().string();
        if (!name.empty() && name[0] != '.')
            s_browserDirs.push_back(name);
    }
    std::sort(s_browserDirs.begin(), s_browserDirs.end());
}

// ============================================================================
// Project loading (main-thread only)
// ============================================================================

static void loadProject(const fs::path &dir)
{
    s_selectedDir = dir;
    strncpy(s_dirInput, dir.string().c_str(), sizeof(s_dirInput) - 1);

    fs::path cfgPath = dir / "game.cfg";
    s_hasGameCfg = fs::exists(cfgPath);
    s_gameTitle.clear();

    if (s_hasGameCfg) {
        std::ifstream f(cfgPath);
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("title") == 0) {
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    auto trim = [](std::string s) -> std::string {
                        size_t a = s.find_first_not_of(" \t\r\n");
                        size_t b = s.find_last_not_of(" \t\r\n");
                        return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
                    };
                    s_gameTitle = trim(line.substr(eq + 1));
                }
                break;
            }
        }
        addRecent(dir.string());
    }

    clearLog();
    s_screen = Screen::Project;
}

// ============================================================================
// Script list
// ============================================================================

static std::vector<std::string> getScripts()
{
    std::vector<std::string> out;
    fs::path scriptsDir = s_selectedDir / "assets" / "scripts";
    std::error_code ec;
    for (auto &e : fs::directory_iterator(scriptsDir, ec))
        if (e.path().extension() == ".crka")
            out.push_back(e.path().filename().string());
    return out;
}

// ============================================================================
// Find CerekaGame binary (sibling of this executable)
// ============================================================================

static std::string findGameRunner()
{
    char exePath[2048] = {};
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        fs::path candidate = fs::path(exePath).parent_path() / "CerekaGame";
        if (fs::exists(candidate)) return candidate.string();
    }
    return "CerekaGame"; // fallback: search PATH
}

// ============================================================================
// Background actions
// ============================================================================

static bool writeAsset(const fs::path &path,
                       const unsigned char *data, unsigned int len,
                       const std::string &label)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        appendLog("[ERROR] Cannot write " + label + "\n");
        return false;
    }
    f.write(reinterpret_cast<const char *>(data), len);
    appendLog("  + " + label + "\n");
    return true;
}

static void doCreateProject()
{
    if (s_busy.exchange(true)) return;
    clearLog();

    std::thread([dir = s_selectedDir]() {
        appendLog("Scaffolding project in: " + dir.string() + "\n\n");

        std::error_code ec;
        for (const char *sub : {"assets/scripts", "assets/bg",
                                "assets/characters", "assets/fonts",
                                "assets/sounds", "assets/ui"}) {
            fs::path p = dir / sub;
            fs::create_directories(p, ec);
            if (ec) {
                appendLog("[ERROR] Cannot create " + p.string() + ": " + ec.message() + "\n");
                s_busy = false;
                return;
            }
            appendLog("  + " + std::string(sub) + "/\n");
        }

        // Text files
        {
            std::ofstream f(dir / "game.cfg");
            if (!f) { appendLog("[ERROR] Cannot write game.cfg\n"); s_busy = false; return; }
            f << kGameCfgTemplate;
            appendLog("  + game.cfg\n");
        }
        {
            std::ofstream f(dir / "assets" / "scripts" / "ui.crka");
            if (!f) { appendLog("[ERROR] Cannot write ui.crka\n"); s_busy = false; return; }
            f << kUiScriptTemplate;
            appendLog("  + assets/scripts/ui.crka\n");
        }
        {
            std::ofstream f(dir / "assets" / "scripts" / "scene_two.crka");
            if (!f) { appendLog("[ERROR] Cannot write scene_two.crka\n"); s_busy = false; return; }
            f << kSceneTwoTemplate;
            appendLog("  + assets/scripts/scene_two.crka\n");
        }
        {
            std::ofstream f(dir / "assets" / "scripts" / "main.crka");
            if (!f) { appendLog("[ERROR] Cannot write main.crka\n"); s_busy = false; return; }
            f << kMainScriptTemplate;
            appendLog("  + assets/scripts/main.crka\n");
        }

        // Placeholder binary assets
        if (!writeAsset(dir / "assets" / "bg"         / "placeholder_bg.png",
                        kBgPng,   kBgPng_len,   "assets/bg/placeholder_bg.png"))
            { s_busy = false; return; }
        if (!writeAsset(dir / "assets" / "characters" / "placeholder_char.png",
                        kCharPng, kCharPng_len, "assets/characters/placeholder_char.png"))
            { s_busy = false; return; }
        if (!writeAsset(dir / "assets" / "sounds"     / "placeholder_sfx.wav",
                        kSfxWav,  kSfxWav_len,  "assets/sounds/placeholder_sfx.wav"))
            { s_busy = false; return; }
        if (!writeAsset(dir / "assets" / "sounds"     / "placeholder_bgm.wav",
                        kBgmWav,  kBgmWav_len,  "assets/sounds/placeholder_bgm.wav"))
            { s_busy = false; return; }
        if (!writeAsset(dir / "assets" / "fonts"      / "Montserrat-Medium.ttf",
                        kMontserratTtf, kMontserratTtf_len, "assets/fonts/Montserrat-Medium.ttf"))
            { s_busy = false; return; }

        appendLog("\n[OK] Project created. Click Launch to run.\n");
        s_reloadPending = true;
        s_busy = false;
    }).detach();
}

static void doLaunch()
{
    if (s_busy.exchange(true)) return;
    clearLog();

    std::thread([dir = s_selectedDir]() {
        std::string runner = findGameRunner();
        std::string cmd = "\"" + runner + "\" \"" + dir.string() + "\" 2>&1";
        appendLog("$ " + cmd + "\n\n");

        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            appendLog("[ERROR] Failed to start CerekaGame. Is it in the same directory?\n");
            s_busy = false;
            return;
        }
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) appendLog(buf);
        int ret = pclose(pipe);
        appendLog(ret == 0 ? "\n[OK] Game exited cleanly.\n" : "\n[FAILED] Game exited with errors.\n");
        s_busy = false;
    }).detach();
}

// ============================================================================
// Draw log panel (shared between screens)
// ============================================================================

static void drawLogPanel(float w, float h)
{
    ImGui::BeginChild("##log", ImVec2(w, h), true);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Output");
    ImGui::Separator();

    ImGui::BeginChild("##logtext", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lock(s_logMutex);
        const char *p = s_log.c_str();
        while (*p) {
            const char *nl = strchr(p, '\n');
            size_t len     = nl ? (size_t)(nl - p) : strlen(p);
            std::string line(p, len);
            bool isErr = line.find("error:") != std::string::npos ||
                         line.find("[ERROR]") != std::string::npos ||
                         line.find("[FAILED]") != std::string::npos;
            bool isOk  = line.find("[OK]") != std::string::npos;
            if (isErr)
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "%s", line.c_str());
            else if (isOk)
                ImGui::TextColored(ImVec4(0.4f, 1.f, 0.5f, 1.f), "%s", line.c_str());
            else
                ImGui::TextUnformatted(line.c_str());
            p += len + (nl ? 1 : 0);
            if (!nl) break;
        }
    }
    if (s_scrollToBottom) {
        ImGui::SetScrollHereY(1.f);
        s_scrollToBottom = false;
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

// ============================================================================
// Home screen
// ============================================================================

static void drawHome(ImGuiStyle &style)
{
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.f), "CEREKA LAUNCHER");
    ImGui::SameLine(0, 12);
    ImGui::TextDisabled("Visual Novel Engine");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Game Directory:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.f);
    ImGui::InputText("##dirInput", s_dirInput, sizeof(s_dirInput));
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        const char *home = getenv("HOME");
        refreshBrowser(fs::path(home ? home : "/"));
        s_openBrowser = true;
        ImGui::OpenPopup("##dirbrowser");
    }
    ImGui::Spacing();

    if (ImGui::Button("  Open  ", ImVec2(90, 0))) {
        fs::path p = fs::path(s_dirInput).lexically_normal();
        if (!p.empty() && fs::is_directory(p))
            loadProject(p);
        else
            appendLog("[ERROR] Not a valid directory: " + p.string() + "\n");
    }

    // Recent projects
    if (!s_recents.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Recent Projects");
        ImGui::Spacing();
        for (auto &r : s_recents) {
            std::string label = fs::path(r).filename().string() + "##" + r;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.f));
            if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(200, 0))) {
                loadProject(fs::path(r));
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("%s", r.c_str());
        }
    }

    // Error log area
    float avail = ImGui::GetContentRegionAvail().y;
    if (!s_log.empty() && avail > 50) {
        ImGui::Spacing();
        ImGui::Separator();
        drawLogPanel(0, avail);
    }

    // Directory browser popup
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("##dirbrowser", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
    {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.f, 1.f), "Select Directory");
        ImGui::Separator();

        // Current path + Up button
        ImGui::TextDisabled("%s", s_browserPath.string().c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
        if (ImGui::SmallButton("^ Up") && s_browserPath.has_parent_path())
            refreshBrowser(s_browserPath.parent_path());

        ImGui::Spacing();
        ImGui::BeginChild("##dirs", ImVec2(0, 310), true);
        if (s_browserDirs.empty()) {
            ImGui::TextDisabled("(no subdirectories)");
        } else {
            for (auto &d : s_browserDirs) {
                if (ImGui::Selectable(("> " + d).c_str()))
                    refreshBrowser(s_browserPath / d);
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        if (ImGui::Button("Select This Folder", ImVec2(160, 0))) {
            strncpy(s_dirInput, s_browserPath.string().c_str(), sizeof(s_dirInput) - 1);
            s_openBrowser = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            s_openBrowser = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================================
// Project screen
// ============================================================================

static void drawProject(ImGuiStyle &style)
{
    bool busy = s_busy.load();

    // Header
    std::string title = (s_hasGameCfg && !s_gameTitle.empty())
                            ? s_gameTitle
                            : s_selectedDir.filename().string();
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.f), "CEREKA LAUNCHER");
    ImGui::SameLine(0, 12);
    ImGui::TextUnformatted(title.c_str());
    ImGui::SameLine(0, 12);
    ImGui::TextDisabled("(%s)", s_selectedDir.string().c_str());
    ImGui::Separator();
    ImGui::Spacing();

    if (busy) ImGui::BeginDisabled();

    if (!s_hasGameCfg) {
        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.3f, 1.f),
                           "No game.cfg found. Create a new project here?");
        ImGui::Spacing();
        if (ImGui::Button("  Create Game  ", ImVec2(130, 0)))
            doCreateProject();
    } else {
        if (ImGui::Button("  Launch Game  ", ImVec2(130, 0)))
            doLaunch();
    }

    ImGui::SameLine(0, 12);
    if (busy) {
        ImGui::EndDisabled();
        ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "working...");
        ImGui::SameLine();
    }

    if (!busy && ImGui::SmallButton("Clear log")) clearLog();

    ImGui::SameLine();
    if (ImGui::SmallButton("< Back")) {
        if (!busy) { s_screen = Screen::Home; clearLog(); }
    }

    ImGui::Separator();

    // Two-column layout
    float leftW  = 200.f;
    float rightW = ImGui::GetContentRegionAvail().x - leftW - style.ItemSpacing.x;
    float panelH = ImGui::GetContentRegionAvail().y;

    // Left: scripts
    ImGui::BeginChild("##scripts", ImVec2(leftW, panelH), true);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Scripts");
    ImGui::Separator();
    auto scripts = getScripts();
    if (scripts.empty()) {
        ImGui::TextDisabled("(none found)");
    } else {
        for (auto &s : scripts) {
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextUnformatted(s.c_str());
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right: output log
    drawLogPanel(rightW, panelH);
}

// ============================================================================
// Main
// ============================================================================

int main(int, char **)
{
    loadRecents();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Cereka Launcher", 900, 600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) { SDL_Log("SDL_CreateWindow: %s", SDL_GetError()); return 1; }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { SDL_Log("SDL_CreateRenderer: %s", SDL_GetError()); return 1; }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load Montserrat from embedded data — no file on disk needed
    io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char *>(kMontserratTtf), (int)kMontserratTtf_len, 16.f);

    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding  = 6.f;
    style.FrameRounding   = 4.f;
    style.ItemSpacing     = ImVec2(8, 6);
    style.Colors[ImGuiCol_TitleBg]        = ImVec4(0.08f, 0.08f, 0.10f, 1.f);
    style.Colors[ImGuiCol_TitleBgActive]  = ImVec4(0.12f, 0.12f, 0.18f, 1.f);
    style.Colors[ImGuiCol_Button]         = ImVec4(0.20f, 0.22f, 0.30f, 1.f);
    style.Colors[ImGuiCol_ButtonHovered]  = ImVec4(0.30f, 0.33f, 0.45f, 1.f);
    style.Colors[ImGuiCol_Header]         = ImVec4(0.20f, 0.22f, 0.30f, 1.f);
    style.Colors[ImGuiCol_HeaderHovered]  = ImVec4(0.30f, 0.33f, 0.45f, 1.f);
    style.Colors[ImGuiCol_PopupBg]        = ImVec4(0.10f, 0.10f, 0.14f, 1.f);

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) running = false;
        }

        // Pick up reload signal from background create thread
        if (s_reloadPending.exchange(false))
            loadProject(s_selectedDir);

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)winW, (float)winH));
        ImGui::Begin("##root", nullptr,
                     ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        switch (s_screen) {
            case Screen::Home:    drawHome(style);    break;
            case Screen::Project: drawProject(style); break;
        }

        ImGui::End();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 18, 18, 24, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
