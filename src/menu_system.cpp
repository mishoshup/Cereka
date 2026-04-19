#include "menu_system.hpp"

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
}

void MenuSystem::Close()
{
    open = false;
    texts.clear();
    targets.clear();
    exits.clear();
}

int MenuSystem::HitTest(int mx,
                        int my,
                        int screenW,
                        int screenH,
                        float bw,
                        float bh) const
{
    float y = screenH * 0.4f;
    float x = (float)screenW / 2.0f - bw / 2.0f;

    for (size_t i = 0; i < texts.size(); ++i) {
        if (mx >= x && mx <= x + bw && my >= y && my <= y + bh)
            return (int)i;
        y += bh + 20.0f;
    }
    return -1;
}

}  // namespace cereka
