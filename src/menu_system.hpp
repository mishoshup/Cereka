#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cereka {

class MenuSystem {
   public:
    void Open(std::vector<std::string> texts,
              std::vector<std::string> targets,
              std::vector<bool> exits,
              size_t endPC);
    void Close();

    bool IsOpen() const { return open; }
    size_t ButtonCount() const { return texts.size(); }
    size_t EndPC() const { return endPC; }

    const std::vector<std::string> &Texts() const { return texts; }
    const std::string &Target(size_t i) const { return targets[i]; }
    bool IsExit(size_t i) const { return exits[i]; }

    int HitTest(int mx,
                int my,
                int screenW,
                int screenH,
                float buttonW,
                float buttonH) const;

   private:
    bool open = false;
    std::vector<std::string> texts;
    std::vector<std::string> targets;
    std::vector<bool> exits;
    size_t endPC = 0;
};

}  // namespace cereka
