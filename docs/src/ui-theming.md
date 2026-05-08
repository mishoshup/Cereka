# UI Theming Guide

This guide explains how to customize the visual appearance of Cereka's user interface.

## Overview

UI theming is done through `ui` blocks in `.crka` script files. Theme definitions are typically placed in a `ui.crka` file and included at the top of the main script:

```crka
include "ui.crka"
```

## How Theming Works

Each UI element (textbox, name box, button, font) has a set of configurable properties. Properties are set using `ui <element>` blocks:

```
ui textbox
    color 20 20 40 230
    y 80%
    h 20%
```

### Dimension Values

Some properties accept either:
- **Pixels:** plain number (e.g., `y 50`, `w 300`, `h 40`)
- **Percentage:** number followed by `%` (e.g., `y 80%`, `w 50%`, `h 20%`)

Percentages are relative to the game window size.

### Color Values

Colors are specified as four integers (0–255): `r g b a`

### Image Values

Image paths are relative to the game project root (e.g., `assets/ui/textbox.png`).

---

## UI Elements

### Textbox

The textbox is the main dialogue area at the bottom of the screen.

```
ui textbox
    color 20 20 40 230
    y 80%
    h 20%
    text_margin_x 20
    text_color 220 220 255 255
    image "assets/ui/textbox.png"
```

### Name Box

The name box sits above the textbox and displays the speaking character's name.

```
ui namebox
    color 40 40 80 255
    x 10%
    y_offset -10
    w 200
    h 40
    text_color 255 255 200 255
    image "assets/ui/namebox.png"
```

### Buttons

Buttons appear in menu choices.

```
ui button
    color 60 60 100 255
    w 300
    h 50
    text_color 255 255 255 255
    image "assets/ui/button.png"
    hover_image "assets/ui/button_hover.png"
```

### Font

The font size for all UI text.

```
ui font
    size 28
```

### Advance Keys

Configure which keyboard keys advance dialogue text.

```
ui advance_keys space enter
```

See [UI Theming Reference](scripting-reference/ui-theming.md) for all available key names.

---

## Theming Strategies

### Minimal Theme

Override only the colors you need and leave defaults for everything else:

```
ui textbox
    color 0 0 0 200
    text_color 255 255 255 255
```

### Full Custom Theme

Replace all visual elements with custom images:

```
ui textbox
    y 75%
    h 25%
    image "assets/ui/my_textbox.png"

ui namebox
    image "assets/ui/my_namebox.png"

ui button
    image "assets/ui/my_button.png"
    hover_image "assets/ui/my_button_hover.png"
```

### Multiple Themes Per Game

Change themes mid-game by re-including a different theme file:

```crka
; Day theme
include "theme_day.crka"

; ... gameplay ...

; Night theme
include "theme_night.crka"
```

The second `include` overrides the properties set by the first.
