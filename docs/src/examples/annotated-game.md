# Annotated Example Game

This page walks through a complete Cereka game script with explanations for each section.

## Project Setup

**game.cfg:**

```ini
title = "The Lost Amulet"
width = 1280
height = 720
fullscreen = false
entry = "main.crka"
```

The entry point is `assets/scripts/main.crka`.

---

## UI Theme (ui.crka)

First, we define the visual theme:

```
ui font
    size 28

ui textbox
    color 20 20 40 230
    y 80%
    h 20%
    text_margin_x 20
    text_color 220 220 255 255

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

ui advance_keys space enter
```

This sets up a dark-themed UI with blue-tinted text, a semi-transparent textbox at the bottom 20% of the screen, and a compact name box above it.

---

## Main Script (main.crka)

```
; Include the UI theme
include "ui.crka"
```

**Line by line:** The `include` command pulls in `ui.crka` at compile time, making all UI settings available. This goes at the top so styling is active throughout.

```
; =============================================
; Scene 1: The Village
; =============================================

label start
```

**Line by line:** `label start` marks the script entry point. The engine begins execution at the top of the file, so `start` is at line 1.

```
bg village.png
narrate "A peaceful morning in the village of Elmswood."
```

**Line by line:** `bg village.png` loads the background. `narrate` displays text without a character name.

```
; Initialize game variables
set player_name "Saki"
$ hp = 100
$ max_hp = 100
$ gold = 10
```

**Line by line:** `set` creates string variables. `$` creates numeric variables. These track the game state throughout the story.

```
; Introduce the protagonist
say saki "Ah, what a lovely morning! The sun is shining, the birds are singing..."
char saki left saki_happy.png
say saki "I'm {player_name}, and I live in this quiet village."
```

**Line by line:** `say` shows dialogue with a name box. `char` displays the character sprite at the left position. Note `{player_name}` — the variable is substituted inline.

```
narrate "Suddenly, a messenger rushes into the square."
char messenger right messenger_urgent.png
say messenger "Villagers! The ancient amulet has been stolen from the temple!"
```

**Line by line:** A new character `messenger` appears on the right. The `char` command with a position moves them there. The urgent sprite conveys the mood.

```
say saki "The temple amulet? That's been there for centuries!"
say messenger "The elder requests your help, {player_name}. Will you go to the temple?"
```

**Line by line:** Dialogue continues. The story presents a call to adventure.

```
; Player choice
menu
    bg temple_exterior.png fade(1.0)
    button "I'll help!" goto accept_quest
    button "I'm busy." goto decline_quest
```

**Line by line:** `menu` opens a choice block. The optional `bg` command crossfades the background while the menu is open. Each `button` has display text and either a `goto` target or `exit`.

```
; =============================================
; Branch A: Accept the quest
; =============================================

label accept_quest
say saki "Of course I'll help! Show me the way."
char saki left saki_determined.png
hide char messenger
```

**Line by line:** After choosing, execution jumps to `accept_quest`. The sprite changes to `saki_determined.png`. `hide char messenger` removes the messenger from the screen.

```
bg forest_path.png fade(1.5)
say saki "<b>The forest seems darker than usual...</b>"
narrate "You make your way through the winding forest path."
```

**Line by line:** Background crossfade over 1.5 seconds. The `<b>` tag makes the dialogue bold — text markup works inside all dialogue strings.

```
; Random encounter — variable tracking
$ gold += 5
say saki "Oh! I found some gold coins on the ground. That's {gold} gold total!"
```

**Line by line:** `$ gold += 5` increments the variable. The new value is displayed in the dialogue via `{gold}`.

```
; Conditional encounter
$ hp -= 20
if hp > 0
    say saki "That trap caught me off guard, but I'm still standing."
else
    say saki "I can't go on..."
    jump game_over
endif
```

**Line by line:** A trap damages the player. The `if` block checks if HP is still positive. `endif` closes the conditional. `else` provides the alternative branch. If HP reaches 0, the story jumps to `game_over`.

```
; =============================================
; Branch B: Decline the quest
; =============================================

label decline_quest
say saki "Sorry, I have things to do today."
narrate "You walk away from the messenger."
narrate "The amulet remains lost, and the village never sees peace again."
narrate "<b>The End...?</b>"
end
```

**Line by line:** A shorter branch. `end` terminates the game when the player declines the quest.

```
; =============================================
; Temple Exterior
; =============================================

label temple
bg temple_exterior.png
char saki left saki_determined.png
```

**Line by line:** A new label for the temple scene. The background and character are set up.

```
say saki "The temple doors are massive. I'll need a key."
```

```
; Check if player has the key
set has_key "no"
; (in a full game, this would be set by a previous scene)

if has_key == "yes"
    jump temple_interior
else
    narrate "The key must be somewhere in the village."
endif
```

**Line by line:** A string variable check. If the player found the key in a previous scene, they proceed. Otherwise, they need to explore more.

```
; Save point
narrate "Let me save before going further."
save_menu
```

**Line by line:** `save_menu` opens the save overlay, letting the player pick a slot.

```
; =============================================
; Temple Interior
; =============================================

label temple_interior
bg temple_interior.png fade(1.0)
say saki "The amulet! There it is!"
```

```
; Final choice
menu
    button "Take the amulet" goto take_amulet
    button "Leave it alone" goto leave_amulet

label take_amulet
narrate "You grab the amulet. A warm energy flows through you."
say saki "I should return this to the temple."
$ gold += 100
narrate "You return the amulet and are rewarded <b>100 gold</b>!"
say saki "What an adventure! I have {gold} gold now!"
narrate "The village celebrates your bravery."
jump ending

label leave_amulet
narrate "You decide the amulet is best left untouched."
narrate "Some mysteries are better left unsolved."
```

**Line by line:** The final choice determines the ending. `$ gold += 100` rewards the player for completing the quest. Both branches converge at `ending`.

```
; =============================================
; Ending
; =============================================

label ending
bg celebration.png fade(2.0)
say saki "Thanks for joining me on this adventure!"
narrate "<b>The End</b>"
narrate "Thank you for playing!"
end
```

**Line by line:** The ending scene with a celebratory background. `end` terminates the game.

```
; =============================================
; Game Over
; =============================================

label game_over
bg game_over.png
narrate "Your journey has ended."
say saki "I should load a previous save..."
end
```

**Line by line:** The game over screen, reached when HP drops to 0. Players can load a save from the save menu.

---

## Summary

This annotated example demonstrates:

| Feature | Example |
|---------|---------|
| UI Theming | `ui.crka` with `ui textbox`, `ui namebox`, `ui button` |
| Background | `bg` with `fade` transitions |
| Characters | `char` with position, `hide char` |
| Dialogue | `say`, `narrate`, `{var}` substitution |
| Text Markup | `<b>bold</b>`, `<color=#rrggbb>color</color>` |
| Variables | `set` for strings, `$` for numbers |
| Arithmetic | `$ gold += 5`, `$ hp -= 20` |
| Conditionals | `if/else/endif` with `>`, `==` |
| Flow Control | `label`, `jump`, `include` |
| Menu | `menu` with `button goto` and `button exit` |
| Save | `save_menu` overlay |

This covers the core features of the Cereka scripting language. Each script command is documented in detail in the [Scripting Reference](../scripting-reference/scene.md).
