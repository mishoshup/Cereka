# Menu System Research — Analysis & Design

## Current Architecture Summary

### Data Flow
```
.crka script → MENU op → Impl::EnterMenu() → MenuSystem::Open()
  MenuSystem holds: texts[], targets[], exits[], endPC

MenuState::update() — NO-OP (empty body)
MenuState::handleEvent(MouseDown) → menu.HitTest() → dispatch
MenuState::draw() → impl.ui.DrawMenuButtons(menu, uiCfg)

UIManager::DrawMenuButtons():
  - Hardcoded: y = screenH * 0.4f, spacing = 20px
  - Loops all buttons, draws rect + text
  - No hover check, no pagination
```

### What Already Exists (but is unused)
- `uiCfg.button.hoverImage` / `hoverImagePath` — loaded via ConfigManager, stored, destroyed on shutdown
- `button.hover_image` registered in PROPERTY_TABLE
- `hoverImage` is **never checked** in DrawMenuButtons

### What's Missing
| Feature | Status |
|---|---|
| Mouse motion event pipeline | ❌ No `MouseMove` type in `CerekaEvent` |
| Hover state tracking | ❌ No hoveredIndex anywhere |
| Hover rendering | ❌ hoverImage exists but unused; no fallback tint |
| Keyboard navigation | ❌ Only MouseDown handled |
| Pagination | ❌ Buttons overflow off-screen |
| Configurable layout | ❌ y and spacing hardcoded in 2 places |
| Visual polish | ❌ No selection indicator, no feedback |

---

## Question-by-Question Analysis

### 1. Is the current button rendering approach good enough, or does it need architectural changes?

**It needs changes, but NOT architectural ones.** The current approach (UIManager draws rects + text) is fine. The rendering pipeline is clean: `IRenderContext::FillRect` for solid color, `DrawTexture` for images, `CreateTextTexture` for labels. No need to change how rendering works.

What needs to change:
- `DrawMenuButtons` needs parameters for hovered index and page number
- `MenuState` needs to own interactive state (hoveredIndex, selectedIndex, currentPage)

### 2. For hover states: does SDL provide hit-testing? How should we track which button is hovered?

**SDL provides raw mouse position via `SDL_EVENT_MOUSE_MOTION`.** Hit-testing is our own math (rect containment), which we already have in `MenuSystem::HitTest()`.

Decision: Track hovered index in `MenuSystem` itself (as user instructed: "track mouse position in MenuSystem"). The `MenuState` updates it on mouse move events. `DrawMenuButtons` reads it.

Algorithm:
1. Add `MouseMove` to `CerekaEvent::Type`
2. Forward `SDL_EVENT_MOUSE_MOTION` → `CerekaEvent{type:MouseMove, mouseX, mouseY}`
3. `MenuState::handleEvent(MouseMove)` → `menu.HitTest(...)` → `menu.SetHoveredIndex(result)`
4. `DrawMenuButtons` checks hovered index per button → renders differently

### 3. For pagination: what happens in real VN gameplay with 10+ choices? Does Ren'Py paginate or scroll?

**Ren'Py paginates.** When there are too many choices, it splits them into pages with a visual "Next" indicator (typically "▼ Continue" at the bottom). Users click the indicator to advance the page. This is the standard VN pattern.

Scrolling would require scrollbar rendering, mouse wheel support, and hit-testing on scrollbar — more complexity and not what VN users expect. **Pagination is the correct approach.**

Algorithm:
- Compute `buttonsPerPage = floor((screenH - buttonY - bottomMargin) / (buttonH + spacing))`
- Clamp to `[1, totalButtons]`
- `totalPages = ceil(totalButtons / buttonsPerPage)`
- Current page tracked in `MenuSystem` as `currentPage`
- Render only buttons for current page range: `[page * buttonsPerPage, min((page+1) * buttonsPerPage, totalButtons))`
- If `page < totalPages - 1`, render "▼" indicator at last button position
- If `page > 0`, render "▲" indicator at first button position
- Clicking on indicator advances/retreats page
- HitTest adjusted for page offset

Note: pagination is extremely rare for typical VN menus (usually 2-4 choices). It's a safety net, not a frequent code path.

### 4. For configurable layout: what properties actually matter for game authors?

Game authors need to control:
1. **Starting Y position** — `button.y` (Dim, default "40%") — controls where the button column starts
2. **Button spacing** — `button.spacing` (Float, default 20px) — vertical gap between buttons

These are the ONLY two layout properties that matter. Button width and height are already configurable. X position is always centered — that's the VN convention.

### 5. What visual polish is missing that makes menus feel "not enterprise grade"?

Problems:
1. **No hover feedback** — users don't know which button they're about to click
2. **No selection highlight** — keyboard users get zero visual feedback
3. **Buttons that overflow the screen** — silently unreachable
4. **No keyboard support** — forces mouse use
5. **No page indicator** — when paginated, no visual clue that more choices exist

### 6. Should we add keyboard navigation (arrow keys to select, enter to confirm)?

**Yes, absolutely.** This is table-stakes for a VN engine:
- Up/Down arrows cycle between buttons
- Enter/Space confirm selection
- Selected index tracks which button is "focused"
- Mouse move overrides selection (hover = selection)
- Keyboard selection sets hovered index too

No need for Home/End or letter-key shortcuts for Phase 1 — those are polish for later.

### 7. Does the current draw order/z-ordering handle overlays correctly?

**Yes.** The rendering order in `Impl::Draw()` is:
```
Background → Characters → SceneGraph → state.draw()
```
When an overlay is active (`hasOverlays()` → true), it returns early and does NOT draw the dialogue box. The overlay state (SaveMenu, LoadMenu, History) draws its own content. During `InMenu`, MenuState draws buttons on top of the scene.

This is correct. Menu buttons appear over the background/characters but under overlays (which makes sense — overlays like Save/Load are modal).
