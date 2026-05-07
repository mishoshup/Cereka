#pragma once
#include "renderer/irender_context.hpp"
#include <string>
#include <vector>

namespace cereka::text {

struct TextStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    Color color = {255, 255, 255, 255};
};

struct TextSegment {
    std::string text;
    TextStyle style;
};

std::vector<TextSegment> ParseMarkup(const std::string &input);

}  // namespace cereka::text
