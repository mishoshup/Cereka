#include "rich_text_renderer.hpp"

namespace cereka::text {

float DrawRichText(IRenderContext &ctx,
                   TTF_Font *font,
                   const std::vector<TextSegment> &segments,
                   float x, float y, float maxWidth)
{
    (void)ctx;
    (void)font;
    (void)segments;
    (void)x;
    (void)y;
    (void)maxWidth;
    return 0.0f;
}

}  // namespace cereka::text
