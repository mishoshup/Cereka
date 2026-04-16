# Cereka - Visual Novel Engine Development Plan

## Philosophy

- **Custom script only** (`.crka`) — no external languages
- **Sensible defaults** — works great out of the box
- **Highly customizable** — power users can override/tweak everything
- Goal: As powerful as Ren'Py but custom script only

## Core Architecture Decisions

### Script Language

- Indent-based blocks (Ren'Py-style)
- `.crka` files with compile-time include system
- Hyprland-like import system for screens/widgets

### Screen System (First-Class Citizen)

- Screens are hierarchical layout trees
- Built-in screens auto-available
- User override allowed (like Ren'Py)
- ATL transforms work on screens

### Widget System

- Widgets defined with indent-based DSL
- Sensible defaults out of the box
- Override allowed with full customization
- Screen-like definition syntax

### ATL (Animation & Transformation Language)

- Full ATL support (Ren'Py-equivalent)
- Works on: characters, backgrounds, screens
- Ease functions: linear, ease-in, ease-out, ease-in-out

### Audio Architecture

- 4-channel mixing: BGM, Voice, SFX, Master
- Per-channel volume control
- Fade in/out support

---

## Sprint 1: Foundation

### ~~1.1 Variables: Numeric + Arithmetic~~

- ~~Operators: `+`, `-`, `*`, `/`, `%`~~
- ~~Comparisons: `==`, `!=`, `>`, `<`, `>=`, `<=`~~
- ~~Bonus: `{var}` substitution in text~~

### 1.2 Audio: 4-Channel Mixer

- Channels: `bgm`, `voice`, `sfx`, `master`
- Volume: 0.0 to 1.0
- Preferences persist across sessions
- `SetVolume` action for screens

### 1.3 Audio: Fade In/Out

```
bgm music.ogg fade 2.0
stop_bgm fade 1.0
SetVolume("bgm", 0.5, fade=1.0)
```

### 1.4 Voice Lines

```
voice "alice_01.ogg"
say alice "Hello!"
; Voice auto-plays with dialogue
; Uses voice volume channel
```

---

## Sprint 2: ATL Core

### 2.1 ATL Transforms (Characters)

```
char eileen happy center with dissolve
char alice surprised left with fade
char bob thinking right with ease-in move
char cathy happy center with zoom 1.5
char dave neutral left with rotate 15
```

- Transform types: `dissolve`, `fade`, `move`, `zoom`, `rotate`
- Duration: default 0.5s or explicit
- Ease: linear, ease-in, ease-out, ease-in-out

### 2.2 ATL Transforms (Backgrounds)

```
bg park with dissolve
bg beach with flash
bg city with fade
```

- Also supports: `vpunch`, `hpunch`, `shake`

### 2.3 ATL Transforms (Screens)

```
show screen quick_menu with slideawayleft
show screen inventory with fade
hide screen settings with slideawayright
```

---

## Sprint 3: Text & UI

### 3.1 Text Markup

```
say alice "{b}Bold{/b}"
say bob "{i}Italic{/i}"
say cathy "{color=ff0000}Red{/color}"
say dave "Click to continue.{nw}"
say eve "{cps=30}Slow{/cps}"
```

- Bonus: `{var}` already implemented ✅

### 3.2 Screen System Core

```
screen settings:
    frame:
        align (0.5, 0.5)
        padding 20

        vbox:
            label "Settings"

            hbox:
                text "BGM"
                slider value Preference("bgm_volume")

            hbox:
                text "Voice"
                slider value Preference("voice_volume")

screen confirm(message):
    frame:
        text message
        textbutton "Yes" action Return(True)
        textbutton "No" action Return(False)
```

### 3.3 Screen Widgets

- `frame` — bordered container
- `vbox` — vertical layout
- `hbox` — horizontal layout
- `label` — styled text
- `textbutton` — clickable text
- `slider` — volume/value slider
- `toggle` — on/off checkbox
- `text` — plain text

### 3.4 Screen Layout

```
align (0.5, 0.5)     ; center
xpos 100              ; pixels
ypos 0.3              ; or 30%
padding 20
spacing 10
```

### 3.5 Screen Actions

```crka
; Navigation
ShowScreen("settings")
HideScreen("quick_menu")
Jump("scene_b")
Call("scene_b")
Return()

; Settings
SetVolume("bgm", 0.5)
ToggleSetting("skip_unseen")
SetPreference("text_speed", 60)

; Save/Load
SaveGame(1)
LoadGame(1)
QuickSave()
QuickLoad()

; Flow
Pause()
Quit()
Skip()
Rollback()

; Custom
action UnlockGallery("beach_scene")
```

### 3.6 Screen State Binding

```
slider value Preference("bgm_volume")
toggle value Preference("auto_forward")
```

- Auto-syncs with engine preferences
- React-like state binding

### 3.7 Screen Events

```
textbutton "Save":
    action ShowScreen("save")
    hovered PlaySound("hover.wav")
    clicked PlaySound("click.wav")
```

---

## Sprint 4: UI Components

### 4.1 textbutton Customization

```
; Default works out of box
textbutton "Save" action ShowScreen("save")

; Override allowed
textbutton PlayButton:
    y_pos 0.9
    text_size 24
    idle_image "button_idle.png"
    hover_image "button_hover.png"
    hover_sound "hover.wav"
    click_sound "click.wav"
```

### 4.2 Built-in: quick_menu

```
; Default auto-available
show screen quick_menu

; Auto-shows during dialogue
; Default buttons: Save, Load, Settings, History
; User can override screen definition
```

### 4.3 Built-in: settings

```
; Default auto-available
show screen settings

; Default controls:
; - BGM volume slider
; - Voice volume slider
; - SFX volume slider
; - Text speed slider
; - Auto-forward toggle
; - Skip unseen toggle
```

### 4.4 Built-in: history

```
; Default auto-available
show screen history

; Scrollable dialogue log
; Shows past dialogue with character names
```

### 4.5 Built-in: save/load

```
; Default auto-available
show screen save
show screen load

; 10 slots
; Thumbnails
; Timestamp display
```

---

## Sprint 5: Polish

### 5.1 Rollback

- Click dialogue history to rewind state
- Restores: pc, variables, bg, characters, text position
- Essential player expectation

### 5.2 wait/pause

```
wait 2.0              ; wait N seconds
wait for voice        ; wait for voice line to finish
```

### 5.3 Subpositions

```
char eileen center at 0.3
char alice left at 0.1
char bob right at 0.9
```

- Arbitrary x position (0.0 to 1.0)
- Not just left/center/right

---

## Ticket Index

### High Priority

| #   | Ticket                          | Status  |
| --- | ------------------------------- | ------- |
| 1   | Variables: Numeric + arithmetic | ✅ Done |
| 2   | Audio: 4-channel mixer          | 🔜 Next |
| 3   | Audio: Fade in/out              | Pending |
| 4   | Voice lines                     | Pending |
| 5   | ATL transforms (chars)          | Pending |
| 6   | ATL transforms (bg)             | Pending |
| 7   | ATL transforms (screens)        | Pending |
| 8   | Text markup                     | Pending |
| 9   | Screen system core              | Pending |
| 10  | Screen widgets                  | Pending |
| 11  | Screen layout                   | Pending |
| 12  | Screen actions                  | Pending |
| 13  | Screen state binding            | Pending |
| 14  | Screen events                   | Pending |
| 15  | textbutton customization        | Pending |
| 16  | Built-in quick_menu             | Pending |
| 17  | Built-in settings               | Pending |
| 18  | Built-in history                | Pending |
| 19  | Built-in save/load              | Pending |

### Medium Priority

| #   | Ticket       | Status  |
| --- | ------------ | ------- |
| 20  | Rollback     | Pending |
| 21  | wait/pause   | Pending |
| 22  | Subpositions | Pending |

---

## Technical Notes

### Implementation Order

~~Variables~~ → Audio → Voice → ATL core → Text markup → Screens → Built-ins → Rollback

### Screen Rendering

- Screens are renderable nodes (same base as sprite/background/character)
- ATL transforms apply universally
- Z-order: backgrounds < characters < screens < overlays

### File Structure

```
game/
├── game.cfg
├── ui.crka              ; UI definitions (include in main)
├── main.crka
├── screens/
│   ├── settings.crka
│   ├── confirm.crka
│   └── ...
└── assets/...
```

### Include System

```
include screens/settings.crka
include screens/confirm.crka
```
