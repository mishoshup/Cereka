# Word Wrapping Research — Cereka Text Engine

## Date: 2026-05-09

## 1. Root Cause Analysis

### The wrapping code

All text wrapping happens in `SdlRenderContext::DrawRichText()` at `src/renderer/sdl_render_context.cpp:159-248`.

The algorithm (simplified):

```
for each segment:
  offset = 0
  while offset < textLen:
    TTF_MeasureString(font, text+offset, remaining, maxWidth, &w, &extent)
    if extent == 0:
      if currentX == x: break           // can't fit a single glyph
      currentX = x; currentY += lineH   // wrap to next line
      continue
    place(text[offset..offset+extent])   // <-- THIS IS THE PROBLEM
    offset += extent
```

**Root cause:** `TTF_MeasureString` (SDL3_ttf) returns a glyph count — the number of **glyphs** that fit in the given pixel width. It does NOT consider word boundaries. It measures glyph-atomically and returns however many whole glyphs fit, even if that means splitting a word across two lines.

For example, with text "The magnificent castle" at a width that can fit ~20 chars:
- TTF_MeasureString might return extent=14 → "The magnificen"
- This splits "magnificent" into "magnificen" on line 1 and "t castle" on line 2
- **The code never scans backward to find a word boundary**

### Infinite loop edge case (already known)

Review file `REVIEW-RENDERER.md` CR-03 documents an infinite loop when `extent==0` and the loop keeps wrapping without advancing `offset`. This is a pre-existing issue that we should also fix.

## 2. Evaluation of "scan back for word boundary" (the backlog suggestion)

### What it does

After `TTF_MeasureString` returns how many chars fit, scan backward in the fitted substring to find the last space character. If found, wrap there instead of at the glyph boundary.

### Pros
- Simple, localized change (~15 lines)
- Fixes the most visible issue (mid-word breaks in Latin text)
- Zero-dependency — works with stock SDL_ttf
- Greedy algorithm matches what most text layout systems do

### Cons / Edge Cases

| Concern | Assessment |
|---|---|
| **CJK text** | CJK writing systems don't use spaces for word boundaries. `rfind(' ')` returns `npos`, so the algorithm falls through to glyph-level break. **This is correct behavior** — CJK allows character-level line breaks. Professional CJK typesetting uses kinsoku rules (certain chars can't start/end a line), but that's overkill for a VN engine. |
| **Hyphenation** | No hyphenation support. The word is just broken at the space before the line overflow. Hyphenation would require a dictionary or algorithm. Acceptable for a VN engine at this stage. |
| **Performance** | Each wrap point requires at most one extra `TTF_MeasureString` call for the reduced substring. Since `TTF_MeasureString` is O(n) in glyph count and VN dialogue is typically <500 chars, the cost is negligible. No measurable impact. |
| **Markup tags** | Markup is already parsed into `TextSegment` objects by `ParseMarkup` before `DrawRichText` is called. Each segment is plain text with a style. So tags don't affect measurement. ✓ |
| **Trailing spaces** | When wrapping at a space, we must not render the space on the current line AND we should skip it at the start of the next line. Easy to handle. |
| **Long words > line width** | If a single word is wider than maxWidth (e.g., "Pneumonoultramicroscopicsilicovolcanoconiosis"), no space is found and the algorithm falls through to glyph-level break. **Correct** — no better option without hyphenation. |

### Verdict: The backlog suggestion is the right approach.

The "scan back for word boundary" approach is appropriate for this codebase. It's simple, correct for the primary use case (Latin text in a VN engine), and degrades gracefully for CJK and edge cases.

## 3. Alternative Approaches Considered

### A. Pre-process text into word segments

Split text into words before measuring, then build lines word-by-word.

- **Pros:** Cleaner algorithm, avoids re-measuring
- **Cons:** Requires handling markup-style rebinding of segments; more code changes; CJK has no whitespace-based word boundaries
- **Verdict:** Over-engineered for the current need

### B. Use TTF_GetStringSizing / different SDL_ttf API

SDL3_ttf's API for measuring strings is `TTF_MeasureString`. `TTF_GetStringSizing` exists in some SDL_ttf versions but does the same glyph-level measurement. No SDL_ttf API does word-boundary-aware wrapping natively.

- **Verdict:** Not viable — no relevant API exists

### C. Delegate to TTF_RenderText_Blended_Wrapped

SDL3_ttf has `TTF_RenderText_Blended_Wrapped` which does built-in wrapping. However:
- It operates on a single style/text block — we have per-segment styles
- It renders to a single surface, losing per-segment granularity
- It also uses glyph-level wrapping (not word-boundary), based on the SDL_ttf source
- **Verdict:** Doesn't solve the problem and loses per-segment styling

### D. ICU / libunibreak for professional line breaking

Could use ICU's line-breaking rules or the libunibreak library. Handles CJK kinsoku rules, hyphenation, etc.

- **Pros:** Professional-level line breaking, handles all script families
- **Cons:** Adds a dependency; overkill for a VN engine at this stage
- **Verdict:** Future improvement, not needed now

## 4. What Ren'Py does

Ren'Py uses its own text layout engine built on top of FreeType directly (not SDL_ttf). Its wrapping:
1. Splits text into "tex" (text + tag) objects
2. Uses a word-based greedy layout algorithm 
3. Handles ruby text, font size changes within a line, and various text tags
4. Does NOT do hyphenation by default
5. For CJK text, it allows breaks at any character boundary (no kinsoku rules without plugins)

Ren'Py also supports `language` configuration that changes line-break rules per locale, using ICU's line-breaking properties.

**Observation:** Our "scan back for word boundary" approach mirrors Ren'Py's approach for Latin text. For a simpler engine, this is the correct level of sophistication.

## 5. What a professional VN engine does (Unity/Unreal VN frameworks)

- Unity's TextMeshPro: word wrapping, character wrapping, and hyphenation options. Uses a word-by-word layout with optional hyphenation via a dictionary.
- Unreal's RichTextBlock: word wrapping is the default, with an option for "overflow" (clip).

Both support CJK via character-boundary breaking. Neither does kinsoku rules without custom code.

## Conclusion

The "scan back for word boundary" approach is the correct fix for this codebase at this time. It fixes the visible bug with minimal code change, handles CJK gracefully (falls through to character-level break), and has no measurable performance impact for VN dialogue text.

The implementation should:
1. After `TTF_MeasureString` returns, check if `extent < remaining`
2. If so, scan the fitted substring with `rfind(' ')` for last space
3. If space found: adjust extent to break at the space, re-measure, and skip the space for next iteration
4. If no space found: keep glyph-level break (long word case)
5. If space is the very first character: skip it and continue (leading whitespace on wrapped line)

Additionally, fix the pre-existing infinite loop edge case (REVIEW-RENDERER.md CR-03) — when `extent==0` and we're at the start of a line, force-place at least one glyph to prevent infinite spin.
