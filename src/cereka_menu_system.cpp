#include "cereka_menu_system.hpp"
#include <cmath>
#include <algorithm>

namespace cereka {

void MenuSystem::Open(std::vector<std::string> t,
                      std::vector<std::string> tg,
                      std::vector<bool> ex,
                      size_t end)
{
    texts = std::move(t);
    targets = std::move(tg);
    exits = std::move(ex);
    endPC = end;
    open = true;

    // Reset interaction state
    hoveredIndex_ = -1;
    selectedIndex_ = 0;
    currentPage_ = 0;
    totalPages_ = 1;
}

void MenuSystem::Close()
{
    open = false;
    texts.clear();
    targets.clear();
    exits.clear();
    hoveredIndex_ = -1;
    selectedIndex_ = 0;
    currentPage_ = 0;
    totalPages_ = 1;
}

float MenuSystem::buttonYPos(int idx,
                              int screenH,
                              const Dim &buttonY,
                              float buttonH,
                              float spacing) const
{
    float y0 = buttonY.resolve((float)screenH);
    return y0 + (float)idx * (buttonH + spacing);
}

int MenuSystem::ButtonsPerPage(float screenH,
                                const Dim &buttonY,
                                float buttonH,
                                float spacing)
{
    float y0 = buttonY.resolve(screenH);
    float available = screenH - y0 - 60.0f;  // 60px bottom margin
    if (available <= 0.0f)
        return 1;
    int count = (int)(available / (buttonH + spacing));
    return std::max(1, count);
}

int MenuSystem::HitTest(int mx,
                        int my,
                        int screenW,
                        int screenH,
                        float bw,
                        float bh,
                        const Dim &buttonY,
                        float spacing) const
{
    if (!open || texts.empty())
        return -1;

    int bpp = ButtonsPerPage((float)screenH, buttonY, bh, spacing);
    int startIdx = currentPage_ * bpp;
    int endIdx = std::min(startIdx + bpp, (int)texts.size());

    float x = (float)screenW / 2.0f - bw / 2.0f;

    for (int i = startIdx; i < endIdx; ++i) {
        float y = buttonYPos(i - startIdx, screenH, buttonY, bh, spacing);
        if (mx >= x && mx <= x + bw && my >= y && my <= y + bh)
            return i;
    }

    // Check page navigation indicators
    // "▼ More" at bottom of current page
    if (currentPage_ < totalPages_ - 1) {
        int lastLocal = bpp - 1;
        if (lastLocal >= endIdx - startIdx)
            lastLocal = endIdx - startIdx - 1;
        float indY = buttonYPos(lastLocal, screenH, buttonY, bh, spacing) + bh;
        if (mx >= x && mx <= x + bw && my >= indY && my <= indY + bh * 0.5f)
            return (int)texts.size();  // special sentinel: "next page"
    }

    // "▲ Back" at top of current page (not first page)
    if (currentPage_ > 0) {
        float indY = buttonYPos(0, screenH, buttonY, bh, spacing) - bh * 0.5f;
        if (mx >= x && mx <= x + bw && my >= indY && my <= indY + bh * 0.5f)
            return (int)texts.size() + 1;  // special sentinel: "prev page"
    }

    return -1;
}

}  // namespace cereka
