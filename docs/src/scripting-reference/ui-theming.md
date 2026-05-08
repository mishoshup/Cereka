# UI Theming

UI theming controls the visual appearance of the game's interface elements: textbox, name box, buttons, font, and advance keys.

Theming is typically done in a `ui.crka` file included at the top of your main script.

```crka
; main.crka
include "ui.crka"
```

---

## Textbox

### `ui textbox`

Configure the dialogue textbox appearance.

```
ui textbox
    color 20 20 40 230          ; r g b a background color
    y 80%                       ; vertical position (percent or pixels)
    h 20%                       ; height (percent or pixels)
    text_margin_x 20            ; horizontal text padding in pixels
    text_color 220 220 255 255  ; text color r g b a
    image "assets/ui/textbox.png" ; background image (optional)
```

**Properties:**

| Property        | Type    | Description                                |
|-----------------|---------|--------------------------------------------|
| `color`         | RGBA    | Background color (r g b a, 0–255)          |
| `y`             | Dim     | Vertical position (percent or pixels)      |
| `h`             | Dim     | Height (percent or pixels)                 |
| `text_margin_x` | Int     | Horizontal text padding in pixels          |
| `text_color`    | RGBA    | Text color (r g b a, 0–255)               |
| `image`         | Texture | Background image (optional, overrides color) |

---

## Name Box

### `ui namebox`

Configure the character name box that appears above the textbox.

```
ui namebox
    color 40 40 80 255          ; background color
    x 10%                       ; horizontal position
    y_offset -10                ; offset from textbox top
    w 200                       ; width in pixels
    h 40                        ; height in pixels
    text_color 255 255 200 255  ; name text color
    image "assets/ui/namebox.png" ; background image (optional)
```

**Properties:**

| Property     | Type    | Description                               |
|--------------|---------|-------------------------------------------|
| `color`      | RGBA    | Background color                          |
| `x`          | Dim     | Horizontal position                       |
| `y_offset`   | Int     | Offset from the textbox top (can be negative to sit above) |
| `w`          | Dim     | Width                                     |
| `h`          | Dim     | Height                                    |
| `text_color` | RGBA    | Name text color                           |
| `image`      | Texture | Background image (optional)               |

---

## Button

### `ui button`

Configure menu button appearance.

```
ui button
    color 60 60 100 255         ; background color
    w 300                       ; width in pixels
    h 50                        ; height in pixels
    text_color 255 255 255 255  ; text color
    image "assets/ui/button.png"       ; normal image
    hover_image "assets/ui/button_hover.png" ; hover image (optional)
```

**Properties:**

| Property      | Type    | Description                         |
|---------------|---------|-------------------------------------|
| `color`       | RGBA    | Background color                    |
| `w`           | Dim     | Width                               |
| `h`           | Dim     | Height                              |
| `text_color`  | RGBA    | Text color                          |
| `image`       | Texture | Normal button image                 |
| `hover_image` | Texture | Hover/selected button image (optional) |

---

## Font

### `ui font`

Configure the font size used throughout the UI.

```
ui font
    size 28                        ; font size in pixels
```

**Properties:**

| Property | Type | Description              |
|----------|------|--------------------------|
| `size`   | Int  | Font size in pixels      |

Font files should be placed in `assets/fonts/`. The engine loads the first available font file found in that directory.

---

## Advance Keys

### `ui advance_keys <key1> <key2> ...`

Configure which keyboard keys advance dialogue. Default: `space enter`.

```
ui advance_keys space enter      ; space or enter advances dialogue (default)
ui advance_keys space            ; only space advances dialogue
ui advance_keys return           ; only return/enter
ui advance_keys space enter z    ; space, enter, or Z
```

**Available key names:**

| Key        | SDL Constant  |
|------------|---------------|
| `space`    | SDLK_SPACE    |
| `enter`    | SDLK_RETURN   |
| `return`   | SDLK_RETURN   |
| `escape`   | SDLK_ESCAPE   |
| `tab`      | SDLK_TAB      |
| `up`       | SDLK_UP       |
| `down`     | SDLK_DOWN     |
| `left`     | SDLK_LEFT     |
| `right`    | SDLK_RIGHT    |

---

## Complete Example

```crka
; ui.crka — complete UI theme
ui font
    size 28

ui textbox
    color 20 20 40 230
    y 80%
    h 20%
    text_margin_x 20
    text_color 220 220 255 255
    image "assets/ui/textbox.png"

ui namebox
    color 40 40 80 255
    x 10%
    y_offset -10
    w 200
    h 40
    text_color 255 255 200 255

ui button
    color 60 60 100 255
    w 300
    h 50
    text_color 255 255 255 255
    image "assets/ui/button.png"
    hover_image "assets/ui/button_hover.png"

ui advance_keys space enter
```
