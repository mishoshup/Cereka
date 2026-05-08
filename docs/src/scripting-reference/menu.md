# Menu & Choices

Menu commands present the player with choices that branch the story.

## Menu Block

### `menu`

Open a menu block containing one or more `button` entries. Menu content is indented.

```
menu
    button "I'll investigate" goto investigate
    button "Let's go back" goto go_back
```

When the menu opens, the player sees the button text and must make a choice. The game pauses until a selection is made.

### Optional Background in Menu

You can swap the background within a menu:

```
menu
    bg dark_cave.png fade(1.0)
    button "Enter the cave" goto enter_cave
    button "Turn back" goto leave
```

---

## Buttons

### `button "text" goto <label>`

A choice that jumps to a label when selected.

```crka
menu
    button "Open the door" goto open_door
    button "Walk away" goto walk_away

label open_door
narrate "The door creaks open..."

label walk_away
narrate "You decide this isn't your business."
```

### `button "text" exit`

A choice that exits the menu and continues with the next line after the menu block.

```crka
menu
    button "Continue" exit
    button "Wait..." exit
```

Use `exit` when you don't need branching — the story continues linearly after the menu.

---

## Menu Styling

Menu buttons can be styled through UI theming. See [UI Theming](ui-theming.md) for customizing button colors, dimensions, text colors, and hover images.

```
; In ui.crka
ui button
    color 40 40 60 255
    text_color 255 255 255 255
    w 300
    h 50
    image "assets/ui/button.png"
    hover_image "assets/ui/button_hover.png"
```

---

## Examples

```crka
; Simple choice
label crossroads
bg crossroads.png
narrate "The path splits in two directions."
menu
    button "Take the left path" goto left_path
    button "Take the right path" goto right_path

label left_path
bg forest.png
say saki "This looks promising!"

label right_path
bg mountains.png
say saki "A steep climb ahead."
```

```crka
; Menu with background transition
label treasure_room
narrate "You enter a vast chamber."
menu
    bg treasure_room.png fade(1.0)
    button "Approach the pedestal" goto pedestal
    button "Examine the wall" goto wall
    button "Leave" exit

label pedestal
narrate "The pedestal hums with energy..."
```
