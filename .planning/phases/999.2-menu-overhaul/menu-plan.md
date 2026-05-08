# Menu Overhaul — Implementation Plan

## What Changes & Why

### Files Modified (8 files, ~150 lines total added)

| File | What Changes |
|---|---|
| `include/Cereka/Cereka.hpp` | Add `MouseMove` to `CerekaEvent::Type` |
| `src/Cereka.cpp` | Forward `SDL_EVENT_MOUSE_MOTION` in PollEvent |
| `src/cereka_menu_system.hpp` | Add `hoveredIndex`, `selectedIndex`, `currentPage` + getters/setters |
| `src/cereka_menu_system.cpp` | Implement state management; update `HitTest` to use cfg y/spacing/page; add `UpdateHitRect` |
| `src/cereka_ui_config.hpp` | Add `Button::y` (Dim), `Button::spacing` (float) |
| `src/config/config_manager.cpp` | Register `button.y`, `button.spacing` properties; getValue for new props |
| `src/state/cereka_states.cpp` | MenuState: handle MouseMove, key nav, pagination |
| `src/ui/ui_manager.cpp` | DrawMenuButtons: hover rendering, pagination, use cfg y/spacing |

### What does NOT change
- `Renderer` / `IRenderContext` — no new draw primitives needed
- `State machine` infrastructure — no new state types
- `ConfigManager` architecture — follows existing property pattern
- `MenuSystem::Open/Close` signatures — backward compatible
- Test API (`SelectMenuOption`, `ButtonLabels`) — unchanged

---

## Implementation Steps

### Step 1: CerekaEvent — Add MouseMove

**`include/Cereka/Cereka.hpp`:**
```cpp
enum Type { Quit, KeyDown, MouseDown, MouseMove, Unknown };
```

**`src/Cereka.cpp` — PollEvent:**
```cpp
case SDL_EVENT_MOUSE_MOTION:
    e.type = cereka::CerekaEvent::MouseMove;
    e.mouseX = sdl.motion.x;
    e.mouseY = sdl.motion.y;
    return true;
```

### Step 2: MenuSystem — Add Interaction State

**`cereka_menu_system.hpp` — Add to class:**
```cpp
public:
    int HoveredIndex() const { return hoveredIndex_; }
    void SetHoveredIndex(int idx) { hoveredIndex_ = idx; }
    int SelectedIndex() const { return selectedIndex_; }
    void SetSelectedIndex(int idx) { selectedIndex_ = idx; }
    int CurrentPage() const { return currentPage_; }
    void SetCurrentPage(int p) { currentPage_ = p; }
    // Returns how many buttons fit given screen dimensions and config
    static int ButtonsPerPage(float screenH, float buttonY, float buttonH, float spacing);

private:
    int hoveredIndex_ = -1;
    int selectedIndex_ = 0;
    int currentPage_ = 0;
```

**`cereka_menu_system.cpp`:**
- `Open()`: Reset hoveredIndex_ = -1, selectedIndex_ = 0, currentPage_ = 0
- `Close()`: same reset
- `ButtonsPerPage()`: static calculation
- `HitTest()`: Accept buttonY and spacing params, account for page offset

### Step 3: UiConfig — Add Configurable Layout

**`cereka_ui_config.hpp — Button struct:**
```cpp
Dim y = {0.4f, true};   // Y position of first button (default 40%)
float spacing = 20.0f;   // Gap between buttons
```

### Step 4: ConfigManager — Register Properties

**`config_manager.cpp` — PROPERTY_TABLE:**
```cpp
{"button.y", PropType::Dim, "Y position of first button (pixels or percentage%)"},
{"button.spacing", PropType::Float, "Vertical spacing between buttons (pixels)"},
```

**`config_manager.cpp` — apply():**
```cpp
else if (key == "button.y") {
    handlers::applyDim(ctx_, parsed, &ctx_.uiCfg->button.y);
}
else if (key == "button.spacing") {
    ctx_.uiCfg->button.spacing = parsed.floatVal;
}
```

**`config_manager.cpp` — getValue() add:**
```cpp
if (key == "button.y") return serializeDim(ctx_.uiCfg->button.y);
if (key == "button.spacing") return serializers::serializeFloat(ctx_.uiCfg->button.spacing);
```

### Step 5: MenuState — Event Handling

**`src/state/cereka_states.cpp — MenuState::handleEvent()`:**

```cpp
void MenuState::handleEvent(const CerekaEvent &event, ICerekaStateContext &ctx) {
    auto &impl = static_cast<Impl &>(ctx);
    const auto &menu = impl.menu;

    if (event.type == CerekaEvent::MouseMove) {
        int idx = impl.menu.HitTest(event.mouseX, event.mouseY,
            impl.screenWidth, impl.screenHeight,
            impl.uiCfg.button.w, impl.uiCfg.button.h,
            impl.uiCfg.button.y, impl.uiCfg.button.spacing);
        impl.menu.SetHoveredIndex(idx);
        if (idx >= 0) impl.menu.SetSelectedIndex(idx);
        return;
    }

    if (event.type == CerekaEvent::KeyDown) {
        if (event.key == SDLK_UP || event.key == SDLK_DOWN) {
            // ...
        }
        if (event.key == SDLK_RETURN || event.key == SDLK_SPACE) {
            // confirm selection
        }
    }

    if (event.type == CerekaEvent::MouseDown) {
        // existing hit test + dispatch
    }
}
```

### Step 6: DrawMenuButtons — Hover + Pagination

**`ui_manager.cpp — DrawMenuButtons()`:**
- Compute buttons per page from screen dimensions
- Use `uiCfg.button.y.resolve(screenH)` for first button position
- Use `uiCfg.button.spacing` for gap
- Loop only current page's buttons
- Check `menu.HoveredIndex()` against actual index (not page-relative)
- If hovered: use `uiCfg.button.hoverImage` if available, else tint color brighter
- If not hovered: normal rendering (existing code)
- Draw prev/next page indicators

### Hover Visual Design

When a button is hovered:
1. If `hoverImage` is loaded → draw it (full image swap)
2. Else → draw a brighter/bordered version of the normal color:
   - Lighten color: add 40 to each RGB channel, clamp to 255
   - Draw a 2px border in the original color
   - This provides clear visual feedback without requiring custom assets

This is better than just color tint because:
- Games with hoverImage assets get full art swap
- Games without get a visual highlight that's clearly distinguishable
- The 2px border makes it obvious which button is selected
```

**Hover Tint Formula:**
```cpp
Color hoverColor = btnColor;
hoverColor.r = std::min(255, (int)btnColor.r + 40);
hoverColor.g = std::min(255, (int)btnColor.g + 40);
hoverColor.b = std::min(255, (int)btnColor.b + 40);
```

### Pagination Visual

```
[Button 1]          ← current page buttons
[Button 2]
[Button 3]
  ▼ More...         ← only shown when more pages exist

--- or ---

  ▲ Back...         ← only shown when prev pages exist
[Button 4]
[Button 5]
  ▼ More...         ← next pages exist
```

Page indicators are simple text labels. Clicking them or pressing arrow keys advances the page.
