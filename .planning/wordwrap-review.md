# Word Wrap Fix — Self-Review

## Summary

Modified `src/renderer/sdl_render_context.cpp` to add word-boundary-aware wrapping in `DrawRichText()`. Previously, `TTF_MeasureString` could split words at any glyph boundary ("magnificent" → "magnifice" / "nt"). Now the algorithm scans backward for the last space in the fitted range and breaks at the word boundary when possible.

Also fixed the pre-existing infinite loop edge case (REVIEW-RENDERER.md CR-03): when no glyph fits at the start of a line (e.g., very small `maxWidth` or a glyph wider than the available space), the code now force-advances by at least one byte instead of breaking out of the loop silently (`break`) or looping forever.

## Quality Concerns

| Concern | Severity | Explanation |
|---|---|---|
| **No hyphenation** | Low | Words like "magnificent" that overflow the line width will still break at glyph boundaries. Hyphenation would require a dictionary. Acceptable for VN stage. |
| **No CJK kinsoku rules** | Low | CJK text can break at any character boundary, which is standard for VN engines. Professional CJK typesetting uses kinsoku rules (prohibited line-start/line-end characters) but this requires locale-aware data. |
| **Multiple consecutive spaces** | Low | "hello   world" — after breaking at the first space and skipping it, remaining spaces at the start of the next line get collapsed one-at-a-time (each triggers `lastSpace==0` → skip). This works but takes N iterations for N leading spaces. Cosmetic only — multi-space text is rare in VN dialogue. |
| **Re-measure cost** | Low | Each word-wrap adjustment adds one extra `TTF_MeasureString` call per line break. For VN dialogue (<500 chars), this is well within real-time budgets (sub-millisecond). Could optimize by caching. |
| **Non-ASCII spaces** | Low | Non-breaking space (U+00A0, 2-byte UTF-8) is not matched by `rfind(' ')`. Glyph-level break. This is actually *correct* — NBSP should not be a word break point. |

## Edge Cases NOT Handled

1. **Hyphenation** — words wider than the line still break at glyph boundaries
2. **Unicode line-breaking rules** (UAX #14) — no break opportunities after hyphens, em-dashes, etc.
3. **Zero-width spaces** (U+200B) — invisible break opportunity, not recognized
4. **Soft hyphens** (U+00AD) — invisible hyphenation hint, not supported
5. **Tab characters** — not handled as break opportunities (acceptable for VN)
6. **Right-to-left text** — not tested (no RTL support in engine yet)

## Testing

- All 67 C++ unit tests pass
- All 14 Lua compile snapshot tests pass
- Tested with `DisplayTest.RichTextWordWrapLoopPlacesSegments` — validates TTF_MeasureString wrapping behavior

## Code Quality

- ~30 net lines added to wrapping logic
- Clean fall-through: CJK/long-word cases use original glyph-level break
- `string_view` used (zero copy) for the fitted range scan
- Comments explain algorithm and edge cases
- No new dependencies, no interface changes

## Suggestions for Future Improvement

1. **Hyphenation dictionary** — integrate libhyphen or a simple en-US hyphenation table
2. **CJK kinsoku** — add prohibited line-start/line-end character checking for CJK locales
3. **Word-wrap performance** — pre-split text into word tokens before layout (avoids re-measure)
4. **Configurable wrapping mode** — "char" (old behavior), "word" (current), "hyphenate" (future)
5. **Soft hyphen support** — `\u00ad` could trigger hyphenation at the break point
