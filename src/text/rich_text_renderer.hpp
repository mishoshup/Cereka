#pragma once
#include "markup_parser.hpp"
#include "renderer/irender_context.hpp"
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>

namespace cereka::text {

float DrawRichText(IRenderContext &ctx,
                   TTF_Font *font,
                   const std::vector<TextSegment> &segments,
                   float x, float y, float maxWidth);

}  // namespace cereka::text
