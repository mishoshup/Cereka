#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static std::string        s_projectRoot;
static std::string        s_buildDir;

static std::string        s_log;
static std::mutex         s_logMutex;
static std::atomic<bool>  s_busy{false};
static bool               s_scrollToBottom = false;
static bool               s_buildOk        = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

static std::vector<std::string> getScripts()
{
    std::vector<std::string> out;
    std::string dir = s_projectRoot + "/assets/scripts";
    std::error_code ec;
    for (auto &e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".crka")
            out.push_back(e.path().filename().string());
    return out;
}

// Run a shell command, stream output into the log. Runs on a background thread.
static void runCmd(const std::string &cmd, bool setOkFlag = false)
{
    appendLog("> " + cmd + "\n");

    std::string fullCmd = "cd \"" + s_buildDir + "\" && " + cmd + " 2>&1";
    FILE *pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        appendLog("[ERROR] popen failed\n");
        s_busy = false;
        return;
    }
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe))
        appendLog(buf);

    int ret = pclose(pipe);
    if (setOkFlag)
        s_buildOk = (ret == 0);

    s_busy = false;
    appendLog(ret == 0 ? "\n[OK]\n" : "\n[FAILED]\n");
    s_scrollToBottom = true;
}

static void doBuild()
{
    if (s_busy.exchange(true)) return;
    clearLog();
    std::thread([]() {
        runCmd("cmake .. -DCMAKE_BUILD_TYPE=Debug > /dev/null && cmake --build . -- -j$(nproc)", true);
    }).detach();
}

static void doRun()
{
    if (s_busy.exchange(true)) return;
    clearLog();
    std::thread([]() {
        runCmd("./VisualNovel");
    }).detach();
}

static void doBuildAndRun()
{
    if (s_busy.exchange(true)) return;
    clearLog();
    std::thread([]() {
        // Build first
        appendLog("> cmake --build . -- -j$(nproc)\n");
        std::string buildCmd = "cd \"" + s_buildDir +
                               "\" && cmake .. -DCMAKE_BUILD_TYPE=Debug > /dev/null"
                               " && cmake --build . -- -j$(nproc) 2>&1";
        FILE *pipe = popen(buildCmd.c_str(), "r");
        bool ok = false;
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe))
                appendLog(buf);
            ok = (pclose(pipe) == 0);
        }
        if (!ok) {
            appendLog("\n[FAILED] Build errors — not launching.\n");
            s_busy = false;
            return;
        }
        appendLog("\n[OK] Build succeeded — launching...\n");

        // Then run
        runCmd("./VisualNovel");
    }).detach();
}

// ---------------------------------------------------------------------------
// Project root detection: executable lives in build/, project root is ../
// ---------------------------------------------------------------------------
static void detectPaths()
{
    char exePath[2048] = {};
#if defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) exePath[len] = '\0';
#endif
    fs::path exe = fs::path(exePath).parent_path();  // build dir
    s_buildDir   = exe.string();
    s_projectRoot = (exe / "..").lexically_normal().string();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int, char **)
{
    detectPaths();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Cereka Launcher", 900, 600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Dark theme with slight customisation
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding  = 6.f;
    style.FrameRounding   = 4.f;
    style.ItemSpacing     = ImVec2(8, 6);
    style.Colors[ImGuiCol_TitleBg]       = ImVec4(0.08f, 0.08f, 0.10f, 1.f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.18f, 1.f);
    style.Colors[ImGuiCol_Button]        = ImVec4(0.20f, 0.22f, 0.30f, 1.f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.33f, 0.45f, 1.f);
    style.Colors[ImGuiCol_Header]        = ImVec4(0.20f, 0.22f, 0.30f, 1.f);

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) running = false;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // --- Full-screen dockable window ---
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)winW, (float)winH));
        ImGui::Begin("##root", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove    | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        // --- Header ---
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.f), "CEREKA LAUNCHER");
        ImGui::SameLine(0, 20);

        fs::path projPath = s_projectRoot;
        ImGui::TextDisabled("%s", projPath.filename().string().c_str());
        ImGui::Separator();

        // --- Action buttons ---
        bool busy = s_busy.load();
        if (busy) ImGui::BeginDisabled();

        if (ImGui::Button("  Build  "))     doBuild();
        ImGui::SameLine();
        if (ImGui::Button("  Run  "))       doRun();
        ImGui::SameLine();
        if (ImGui::Button("  Build & Run  ")) doBuildAndRun();

        if (busy) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "  working...");
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::SmallButton("Clear log")) clearLog();

        ImGui::Separator();

        // --- Two-column layout ---
        float leftW = 200.f;
        float rightW = ImGui::GetContentRegionAvail().x - leftW - style.ItemSpacing.x;
        float panelH = ImGui::GetContentRegionAvail().y;

        // Left: script list
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
        ImGui::BeginChild("##log", ImVec2(rightW, panelH), true);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Output");
        ImGui::Separator();

        ImGui::BeginChild("##logtext", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar);
        {
            std::lock_guard<std::mutex> lock(s_logMutex);
            // Colour error lines red
            const char *p = s_log.c_str();
            while (*p) {
                const char *nl = strchr(p, '\n');
                size_t len = nl ? (size_t)(nl - p) : strlen(p);
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

        ImGui::End();

        // Render
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
