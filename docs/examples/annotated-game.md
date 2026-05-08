# Annotated Example Game

This page walks through a complete `.crka` game script with annotations explaining each feature.

## Complete Script

```crka
; ============================================================
; A Day in Cereka — Annotated Example
; ============================================================

; --- UI Theme Setup ---
; Include your UI configuration (colors, fonts, layout)
ui font
    size 28

ui textbox
    color 0 0 0 200
    y 70%
    h 30%
    text_margin_x 40
    text_color 255 255 255 255

ui namebox
    color 40 40 40 255
    x 80
    y_offset -40
    w 300
    h 40
    text_color 200 200 255 255

ui button
    color 60 60 80 255
    w 400
    h 50
    text_color 255 255 255 255
    hover_color 80 80 120 255

; --- Game Content ---
bg park_scene fade 1.0

char alice left characters/alice_happy.png
char bob right characters/bob_neutral.png

narrate "A sunny afternoon in the park. Birds chirp in the distance."

say alice "Hey {player_name}! I'm so glad you could make it!"
say alice "The weather is perfect today, don't you think?"

$ player_mood = 1

menu
    button "Smile and agree" goto agree_path
    button "Mention the clouds" goto clouds_path

label agree_path
    say alice "Right? I knew you'd say that!"
    $ player_mood += 1
    jump continue_story

label clouds_path
    say bob "Well, there ARE some clouds on the horizon..."
    say alice "Oh don't be such a pessimist, Bob!"
    $ player_mood -= 1
    if player_mood <= 0
        say alice "Maybe we should just head home..."
        jump early_end
    endif
    jump continue_story

label continue_story
    if player_mood > 0
        say alice "Let's go get some ice cream!"
        sfx ice_cream_truck.wav
        bg ice_cream_shop fade 0.5
        narrate "The afternoon continues with laughter and sweet treats."
    else
        narrate "The mood has soured. Perhaps another day."
    endif

bgm ending_theme.ogg

narrate "And so, our story continues..."

label early_end
    narrate "Some stories end before they truly begin..."
    end

label final
    end
```

## Key Features Demonstrated

| Feature | Example |
|---------|---------|
| **Backgrounds** | `bg park_scene fade 1.0` — crossfade transition |
| **Characters** | `char alice left characters/alice_happy.png` — positioning and sprites |
| **Dialogue** | `say alice "Hello!"` — character-specific speech |
| **Narration** | `narrate "A sunny afternoon..."` — narrator voice |
| **Variables** | `set player_name "Alex"` — string variables |
| **Numeric Math** | `$ player_mood = 1` and `$ player_mood += 1` — numeric operations |
| **Menu Choices** | `menu` / `button "..." goto <label>` — branching narrative |
| **Conditionals** | `if` / `else` / `endif` — branching logic |
| **Audio** | `bgm`, `sfx`, `stop_bgm` — background music and sound effects |
| **Labels & Jumps** | `label <name>` / `jump <name>` — flow control |
| **UI Theming** | `ui textbox` / `ui button` — visual customization |
| **Fade Effects** | `fade 1.0` — smooth transitions between scenes |

## Running the Example

Save this as `assets/scripts/main.crka` in your game project and ensure the assets directory has the required images and audio files. Then run:

```bash
./CerekaGame
```
