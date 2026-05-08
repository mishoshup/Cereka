#include <gtest/gtest.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <filesystem>
#include <cstdio>
#include <string>
#include <vector>

struct DisplayTest : ::testing::Test
{
    void SetUp() override
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
            ASSERT_TRUE(SDL_InitSubSystem(SDL_INIT_VIDEO))
                << "SDL_InitSubSystem(SDL_INIT_VIDEO) failed with dummy driver: "
                << SDL_GetError();
        }
    }

    void TearDown() override
    {
        if (SDL_WasInit(SDL_INIT_VIDEO))
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
};

// Helper: create hidden window + renderer pair for headless testing.
// Caller must DestroyRenderer + DestroyWindow.
struct TestRenderer
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
};

static TestRenderer CreateTestRenderer(int w, int h)
{
    TestRenderer tr;
    tr.window = SDL_CreateWindow("test", w, h, SDL_WINDOW_HIDDEN);
    EXPECT_NE(tr.window, nullptr) << SDL_GetError();
    if (tr.window)
        tr.renderer = SDL_CreateRenderer(tr.window, nullptr);
    EXPECT_NE(tr.renderer, nullptr) << SDL_GetError();
    return tr;
}

// Helper: find the first TTF file under assets/fonts/ or tests/
static std::string FindFont()
{
    std::vector<std::string> candidates;
    auto tryDir = [&](const char *dir)
    {
        std::error_code ec;
        for (auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            auto ext = entry.path().extension().string();
            if (ext == ".ttf" || ext == ".otf")
                candidates.push_back(entry.path().string());
        }
    };
    tryDir("assets/fonts");
    tryDir("tests");
    return candidates.empty() ? "" : candidates[0];
}

TEST_F(DisplayTest, LogicalPresentationSetAndGet)
{
    SDL_Window *win = SDL_CreateWindow("test", 1280, 720, SDL_WINDOW_HIDDEN);
    ASSERT_NE(win, nullptr) << SDL_GetError();

    SDL_Renderer *ren = SDL_CreateRenderer(win, nullptr);
    ASSERT_NE(ren, nullptr) << SDL_GetError();

    int logicalW = 1280, logicalH = 720;
    ASSERT_TRUE(SDL_SetRenderLogicalPresentation(ren, logicalW, logicalH,
                                                 SDL_LOGICAL_PRESENTATION_LETTERBOX));

    int gotW = 0, gotH = 0;
    SDL_RendererLogicalPresentation gotMode;
    ASSERT_TRUE(SDL_GetRenderLogicalPresentation(ren, &gotW, &gotH, &gotMode));

    EXPECT_EQ(gotW, logicalW);
    EXPECT_EQ(gotH, logicalH);
    EXPECT_EQ(gotMode, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_FRect rect;
    ASSERT_TRUE(SDL_GetRenderLogicalPresentationRect(ren, &rect));
    EXPECT_GT(rect.w, 0);
    EXPECT_GT(rect.h, 0);

    float aspect = rect.w / rect.h;
    float expected = static_cast<float>(logicalW) / static_cast<float>(logicalH);
    EXPECT_NEAR(aspect, expected, 0.01f);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
}

TEST_F(DisplayTest, LogicalPresentationScalesMultipleResolutions)
{
    SDL_Window *win = SDL_CreateWindow("test", 1920, 1080, SDL_WINDOW_HIDDEN);
    ASSERT_NE(win, nullptr) << SDL_GetError();

    SDL_Renderer *ren = SDL_CreateRenderer(win, nullptr);
    ASSERT_NE(ren, nullptr) << SDL_GetError();

    auto check = [ren](int logW, int logH)
    {
        ASSERT_TRUE(SDL_SetRenderLogicalPresentation(ren, logW, logH,
                                                     SDL_LOGICAL_PRESENTATION_LETTERBOX));

        int gotW = 0, gotH = 0;
        SDL_RendererLogicalPresentation gotMode;
        ASSERT_TRUE(SDL_GetRenderLogicalPresentation(ren, &gotW, &gotH, &gotMode));
        EXPECT_EQ(gotW, logW);
        EXPECT_EQ(gotH, logH);

        SDL_FRect rect;
        ASSERT_TRUE(SDL_GetRenderLogicalPresentationRect(ren, &rect));
        EXPECT_GT(rect.w, 0);
        EXPECT_GT(rect.h, 0);

        float aspect = rect.w / rect.h;
        float expected = static_cast<float>(logW) / static_cast<float>(logH);
        EXPECT_NEAR(aspect, expected, 0.01f);
    };

    check(1280, 720);
    check(1920, 1080);
    check(800, 600);
    check(640, 480);
    check(3840, 2160);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
}

TEST_F(DisplayTest, RenderCoordinatesConversion)
{
    SDL_Window *win = SDL_CreateWindow("test", 1280, 720, SDL_WINDOW_HIDDEN);
    ASSERT_NE(win, nullptr) << SDL_GetError();

    SDL_Renderer *ren = SDL_CreateRenderer(win, nullptr);
    ASSERT_NE(ren, nullptr) << SDL_GetError();

    ASSERT_TRUE(SDL_SetRenderLogicalPresentation(ren, 1280, 720,
                                                 SDL_LOGICAL_PRESENTATION_LETTERBOX));

    float rx = 0, ry = 0;
    ASSERT_TRUE(SDL_RenderCoordinatesToWindow(ren, 640, 360, &rx, &ry));
    EXPECT_GT(rx, 0);
    EXPECT_GT(ry, 0);

    float wx = 0, wy = 0;
    ASSERT_TRUE(SDL_RenderCoordinatesFromWindow(ren, 640, 360, &wx, &wy));
    ASSERT_GT(wx, 0);
    ASSERT_GT(wy, 0);
    EXPECT_NEAR(wx, 640.0f, 1.0f);
    EXPECT_NEAR(wy, 360.0f, 1.0f);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
}

// ============================================================================
// Font rendering tests — isolate macOS font issue
// ============================================================================

TEST_F(DisplayTest, FontInitAndOpen)
{
    ASSERT_TRUE(TTF_Init()) << SDL_GetError();

    std::string fontPath = FindFont();
    ASSERT_FALSE(fontPath.empty()) << "No .ttf/.otf found in assets/fonts/ or tests/";

    TTF_Font *font = TTF_OpenFont(fontPath.c_str(), 24);
    ASSERT_NE(font, nullptr) << "TTF_OpenFont failed: " << SDL_GetError();

    int height = TTF_GetFontHeight(font);
    EXPECT_GT(height, 0);

    TTF_CloseFont(font);
    TTF_Quit();
}

TEST_F(DisplayTest, FontRenderCreatesTexture)
{
    ASSERT_TRUE(TTF_Init()) << SDL_GetError();

    std::string fontPath = FindFont();
    ASSERT_FALSE(fontPath.empty()) << "No font found";

    TTF_Font *font = TTF_OpenFont(fontPath.c_str(), 24);
    ASSERT_NE(font, nullptr);

    auto tr = CreateTestRenderer(1280, 720);
    ASSERT_NE(tr.renderer, nullptr);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderText_Blended(font, "Hello macOS", 11, white);
    ASSERT_NE(surf, nullptr) << "TTF_RenderText_Blended failed: " << SDL_GetError();
    EXPECT_GT(surf->w, 0);
    EXPECT_GT(surf->h, 0);

    SDL_Texture *tex = SDL_CreateTextureFromSurface(tr.renderer, surf);
    ASSERT_NE(tex, nullptr) << "SDL_CreateTextureFromSurface failed: " << SDL_GetError();

    float tw = 0, th = 0;
    ASSERT_TRUE(SDL_GetTextureSize(tex, &tw, &th));
    EXPECT_GT(tw, 0);
    EXPECT_GT(th, 0);

    SDL_DestroyTexture(tex);
    SDL_DestroySurface(surf);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(tr.renderer);
    SDL_DestroyWindow(tr.window);
    TTF_Quit();
}

TEST_F(DisplayTest, FontRenderWithLogicalPresentation)
{
    ASSERT_TRUE(TTF_Init()) << SDL_GetError();

    std::string fontPath = FindFont();
    ASSERT_FALSE(fontPath.empty()) << "No font found";

    TTF_Font *font = TTF_OpenFont(fontPath.c_str(), 24);
    ASSERT_NE(font, nullptr);

    auto tr = CreateTestRenderer(1280, 720);
    ASSERT_NE(tr.renderer, nullptr);

    ASSERT_TRUE(SDL_SetRenderLogicalPresentation(tr.renderer, 1280, 720,
                                                 SDL_LOGICAL_PRESENTATION_LETTERBOX));

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderText_Blended(font, "Hello macOS", 11, white);
    ASSERT_NE(surf, nullptr);

    SDL_Texture *tex = SDL_CreateTextureFromSurface(tr.renderer, surf);
    ASSERT_NE(tex, nullptr) << "SDL_CreateTextureFromSurface with logical presentation: "
                            << SDL_GetError();

    float tw = 0, th = 0;
    ASSERT_TRUE(SDL_GetTextureSize(tex, &tw, &th));
    EXPECT_GT(tw, 0);
    EXPECT_GT(th, 0);

    SDL_FRect dst{100, 100, tw, th};
    ASSERT_TRUE(SDL_RenderTexture(tr.renderer, tex, nullptr, &dst));

    SDL_DestroyTexture(tex);
    SDL_DestroySurface(surf);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(tr.renderer);
    SDL_DestroyWindow(tr.window);
    TTF_Quit();
}

TEST_F(DisplayTest, FontRenderRichTextEndToEnd)
{
    ASSERT_TRUE(TTF_Init()) << SDL_GetError();

    std::string fontPath = FindFont();
    ASSERT_FALSE(fontPath.empty()) << "No font found";

    TTF_Font *font = TTF_OpenFont(fontPath.c_str(), 24);
    ASSERT_NE(font, nullptr) << "TTF_OpenFont failed: " << SDL_GetError();

    auto tr = CreateTestRenderer(1280, 720);
    ASSERT_NE(tr.renderer, nullptr);

    ASSERT_TRUE(SDL_SetRenderLogicalPresentation(tr.renderer, 1280, 720,
                                                 SDL_LOGICAL_PRESENTATION_LETTERBOX));

    SDL_Color sdlColor{255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderText_Blended(font, "Dialogue text", 13, sdlColor);
    ASSERT_NE(surf, nullptr) << "TTF_RenderText_Blended failed: " << SDL_GetError();

    SDL_Texture *tex = SDL_CreateTextureFromSurface(tr.renderer, surf);
    ASSERT_NE(tex, nullptr) << "SDL_CreateTextureFromSurface failed: " << SDL_GetError();

    float tw = (float)surf->w;
    float th = (float)surf->h;
    EXPECT_GT(tw, 0);
    EXPECT_GT(th, 0);

    SDL_FRect dst{50, 200, tw, th};
    ASSERT_TRUE(SDL_RenderTexture(tr.renderer, tex, nullptr, &dst));

    SDL_DestroyTexture(tex);
    SDL_DestroySurface(surf);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(tr.renderer);
    SDL_DestroyWindow(tr.window);
    TTF_Quit();
}

// Simulates the DrawRichText word-wrapping loop to guard against regressions
TEST_F(DisplayTest, RichTextWordWrapLoopPlacesSegments)
{
    ASSERT_TRUE(TTF_Init()) << SDL_GetError();

    std::string fontPath = FindFont();
    ASSERT_FALSE(fontPath.empty());

    TTF_Font *font = TTF_OpenFont(fontPath.c_str(), 24);
    ASSERT_NE(font, nullptr);

    auto tr = CreateTestRenderer(1280, 720);
    ASSERT_NE(tr.renderer, nullptr);

    ASSERT_TRUE(SDL_SetRenderLogicalPresentation(tr.renderer, 1280, 720,
                                                 SDL_LOGICAL_PRESENTATION_LETTERBOX));

    float lineHeight = (float)TTF_GetFontHeight(font);
    EXPECT_GT(lineHeight, 0);

    // The old bug: an unconditional break after TTF_MeasureString prevented
    // the loop body from ever executing. Verify that measure + place works.
    // Use a narrow width to force wrapping into multiple segments.
    const char *text = "Hello world this wraps";
    size_t textLen = strlen(text);
    size_t offset = 0;
    float maxWidth = 150.0f;
    int segmentCount = 0;

    while (offset < textLen) {
        int measuredWidth = 0;
        size_t extentSz = 0;
        const char *textPtr = text + offset;
        size_t remaining = textLen - offset;

        ASSERT_TRUE(TTF_MeasureString(font, textPtr, remaining,
                                      (int)maxWidth,
                                      &measuredWidth, &extentSz));

        int extent = (int)extentSz;
        ASSERT_GT(extent, 0) << "extent should be > 0 at offset " << offset;
        ASSERT_GT(measuredWidth, 0) << "measuredWidth should be > 0 at offset " << offset;

        segmentCount++;
        offset += (size_t)extent;
    }

    EXPECT_GT(segmentCount, 1) << "Should have wrapped into multiple segments at 150px width";

    TTF_CloseFont(font);
    SDL_DestroyRenderer(tr.renderer);
    SDL_DestroyWindow(tr.window);
    TTF_Quit();
}
