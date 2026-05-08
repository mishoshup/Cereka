# Word Wrap Plan

## Files to modify

### Primary: `src/renderer/sdl_render_context.cpp`
- The `DrawRichText` method (lines 159-248)
- Insert word-wrap adjustment after `TTF_MeasureString` result
- Fix infinite loop edge case when `extent==0`

### No changes needed
- `src/renderer/sdl_render_context.hpp` — interface unchanged
- `src/renderer/irender_context.hpp` — interface unchanged
- `src/text/markup_parser.*` — markup parsing is separate
- `src/text/rich_text_renderer.*` — dead code, not involved in actual rendering
- `src/ui/ui_manager.cpp` — caller, no changes needed
- `tests/display_test.cpp` — no changes needed (existing test validates TTF_MeasureString behavior at a basic level)

## Algorithm (Detail)

```
while offset < textLen:
  TTF_MeasureString(font, textPtr, remaining, lineRemaining, &measuredWidth, &extent)

  if extent == 0:
    # INFINITE LOOP FIX: if we're at start of line and nothing fits,
    # force-place at least one glyph to prevent infinite spin
    if currentX == x:
      # Force a minimum of 1 byte (or 1 codepoint for UTF-8)
      extent = measure_one_glyph_or_minimum()
    else:
      wrap to next line
      continue

  # WORD WRAP ADJUSTMENT
  if extent < remaining:
    fitted = textPtr[0..extent)
    lastSpace = fitted.rfind(' ')

    if lastSpace != npos:
      if lastSpace == 0:
        # Leading space at start of line — skip it
        offset += 1
        continue

      # Break at word boundary: keep text up to (but not including) the space
      extent = lastSpace
      # Re-measure the shorter string for correct width
      TTF_MeasureString(font, textPtr, extent, lineRemaining, &measuredWidth, &extent)
      # extent might differ from lastSpace after re-measure (safety)
      # Place the text, then skip the space for next iteration
      place(textPtr[0..extent))
      offset += extent + 1   # +1 skips the space
      continue   # go back to check if more text fits on this line

  # Fall through: place at TTF_MeasureString result (no word-wrap adjustment needed)
  place(textPtr[0..extent))
  offset += extent
```

### Infinite loop fix detail

When `extent == 0` and `currentX == x`, the current code just `break`s. But this can cause issues if there's still text remaining. Instead, we should handle the case where a single glyph is wider than the maxWidth:

```cpp
if (extent == 0) {
    if (currentX == x) {
        // A single glyph is wider than the available width.
        // Advance offset by the minimum possible unit (1 byte for ASCII,
        // or skip to the next UTF-8 codepoint start byte).
        // This prevents the infinite loop documented in REVIEW-RENDERER.md CR-03.
        size_t charLen = 1;
        // For UTF-8: skip continuation bytes
        while (offset + charLen < textLen && 
               (seg.text[offset + charLen] & 0xC0) == 0x80) {
            charLen++;
        }
        offset += charLen;
        currentX = x;
        currentY += lineHeight;
        continue;
    }
    currentX = x;
    currentY += lineHeight;
    continue;
}
```

### UTF-8 safety

- `rfind(' ')` finds byte 0x20 (ASCII space). This is safe — spaces in UTF-8 are single-byte ASCII.
- CJK characters are multi-byte (3 bytes each in UTF-8). They won't match `rfind(' ')`, so the algorithm correctly falls through to glyph-level break for CJK.
- Multi-byte character boundaries: When skipping past a space (`offset += extent + 1`), we could land on a continuation byte if the space was preceded by... wait, spaces are single-byte, and the next byte after a space is always the start of a new codepoint. So this is safe.

## Edge Cases

| Edge Case | Behavior |
|---|---|
| **Empty string** | Handled by existing `if (text.empty()) return nullptr` |
| **Single character** | Fits or doesn't; infinite loop fix handles non-fitting case |
| **Long word > line width** | No space found, falls through to glyph-level break |
| **Multiple spaces** | "hello    world" — rfind finds last space, breaks there, next iteration skips that space. Remaining leading spaces will be in the "fitted" range of the next iteration. If the next line starts with spaces, `lastSpace == 0` triggers which skips one space. Multiple leading spaces would need multiple iterations to skip, which works but is slightly wasteful. Acceptable for a VN engine. |
| **Non-breaking space (U+00A0)** | Not matched by `rfind(' ')` (0x20). Glyph-level break. This is actually correct — NBSP should NOT be a word break point. |
| **Tabs** | Not handled specially. Tabs treated as regular chars. Acceptable for VN scripts. |
| **CJK text** | No spaces, no word-wrap adjustment. Glyph-level break. Correct for CJK. |
| **Mixed Latin/CJK** | Spaces in Latin text get word-wrap; CJK portions without spaces get glyph-level break. Mixed behavior is correct. |
| **Zero-width characters (joiners, marks)** | `TTF_MeasureString` handles these correctly (they have 0 width and don't count toward extent). Not affected by our changes. |
| **Newlines in text** | TTF_MeasureString handles newlines — they're zero-width "render to next line" characters. Our code is segment-based, and ParseMarkup doesn't produce segments with literal newlines in VN output. Not a concern. |

## Verification

1. Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -C build -j12`
2. Run tests: `ninja -C build cereka_test && ./build/tests/cereka_test`
3. Compile snapshot tests: `lua tests/compile/harness.lua`

## Future Improvements (not in scope)

- Hyphenation dictionary for professional line-breaking
- CJK kinsoku rules (line-start/line-end prohibited characters)
- Unicode line-breaking algorithm (UAX #14)
- Configurable wrapping mode (char wrap vs word wrap vs hyphenation)
