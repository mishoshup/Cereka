#pragma once

#include "cereka_ui_config.hpp"
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

    // --- Interaction state ---
    int HoveredIndex() const { return hoveredIndex_; }
    void SetHoveredIndex(int idx) { hoveredIndex_ = idx; }
    int SelectedIndex() const { return selectedIndex_; }
    void SetSelectedIndex(int idx) { selectedIndex_ = idx; }
    int CurrentPage() const { return currentPage_; }
    void SetCurrentPage(int p) { currentPage_ = p; }
    int TotalPages() const { return totalPages_; }
    void SetTotalPages(int p) { totalPages_ = p; }

    /// How many buttons fit per page given screen height and layout config.
    static int ButtonsPerPage(float screenH,
                              const Dim &buttonY,
                              float buttonH,
                              float spacing);

    // --- Hit testing (uses configurable layout) ---
    int HitTest(int mx,
                int my,
                int screenW,
                int screenH,
                float buttonW,
                float buttonH,
                const Dim &buttonY,
                float spacing) const;

   private:
    bool open = false;
    std::vector<std::string> texts;
    std::vector<std::string> targets;
    std::vector<bool> exits;
    size_t endPC = 0;

    // Interaction state (reset on Open/Close)
    int hoveredIndex_ = -1;
    int selectedIndex_ = 0;
    int currentPage_ = 0;
    int totalPages_ = 1;

    /// Compute the Y position of the button at the given index.
    float buttonYPos(int idx,
                     int screenH,
                     const Dim &buttonY,
                     float buttonH,
                     float spacing) const;
};

}  // namespace cereka
