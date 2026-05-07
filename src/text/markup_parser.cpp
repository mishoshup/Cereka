#include "markup_parser.hpp"
#include <cstdlib>
#include <stack>

namespace cereka::text {

static Color parseHexColor(const std::string &hex)
{
    Color c{255, 255, 255, 255};
    if (hex.size() < 7 || hex[0] != '#')
        return c;
    auto parsePair = [&](size_t off) -> uint8_t {
        std::string pair = hex.substr(off, 2);
        char *end = nullptr;
        long v = std::strtol(pair.c_str(), &end, 16);
        if (end != pair.c_str() + 2)
            return 0;
        return static_cast<uint8_t>(v & 0xFF);
    };
    c.r = parsePair(1);
    c.g = parsePair(3);
    c.b = parsePair(5);
    return c;
}

std::vector<TextSegment> ParseMarkup(const std::string &input)
{
    std::vector<TextSegment> result;
    if (input.empty())
        return result;

    std::stack<TextStyle> styleStack;
    TextStyle current;
    styleStack.push(current);

    std::string buf;
    size_t i = 0;
    size_t n = input.size();

    auto flush = [&]() {
        if (buf.empty()) return;
        result.push_back({buf, current});
        buf.clear();
    };

    while (i < n) {
        // Escape sequences
        if (i + 1 < n && input[i] == '<' && input[i + 1] == '<') {
            buf += '<';
            i += 2;
            continue;
        }
        if (i + 1 < n && input[i] == '>' && input[i + 1] == '>') {
            buf += '>';
            i += 2;
            continue;
        }

        // Tag open
        if (input[i] == '<') {
            size_t close = input.find('>', i + 1);
            if (close == std::string::npos) {
                // No closing '>', treat as literal
                buf += '<';
                i++;
                continue;
            }

            std::string tagContent = input.substr(i + 1, close - i - 1);
            bool isClosing = (!tagContent.empty() && tagContent[0] == '/');
            std::string tagName = isClosing ? tagContent.substr(1) : tagContent;

            // Check for color tag with value
            std::string colorValue;
            if (tagName.rfind("color=", 0) == 0) {
                colorValue = tagName.substr(6);
                tagName = "color";
            }

            bool knownTag = (tagName == "b" || tagName == "i" ||
                             tagName == "u" || tagName == "s" ||
                             tagName == "color");

            if (knownTag) {
                flush();
                if (isClosing) {
                    // Close tag — pop the stack
                    if (styleStack.size() > 1) {
                        styleStack.pop();
                        current = styleStack.top();
                    }
                } else {
                    // Open tag — push new style
                    TextStyle next = current;
                    if (tagName == "b") next.bold = true;
                    else if (tagName == "i") next.italic = true;
                    else if (tagName == "u") next.underline = true;
                    else if (tagName == "s") next.strikethrough = true;
                    else if (tagName == "color" && !colorValue.empty())
                        next.color = parseHexColor(colorValue);
                    styleStack.push(next);
                    current = next;
                }
                i = close + 1;
                continue;
            }
        }

        buf += input[i];
        i++;
    }

    flush();
    return result;
}

}  // namespace cereka::text
