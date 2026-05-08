# Save & Load

Save and load commands let players persist their progress.

## Save Menu Overlay

### `save_menu`

Open the save menu overlay, showing 10 save slots. The player selects a slot to save their progress. Press <kbd>ESC</kbd> to cancel and return to the game.

```crka
save_menu                    ; show save slot picker
```

Each slot displays:
- Slot number (1–10)
- Timestamp of the saved game
- Whether the slot is empty or occupied

---

## Load Menu Overlay

### `load_menu`

Open the load menu overlay, showing 10 save slots. The player selects a slot to load a previously saved game. Press <kbd>ESC</kbd> to cancel and return to the game.

```crka
load_menu                    ; show load slot picker
```

Empty slots are shown but cannot be selected.

---

## Silent Save & Load

### `save <slot>`

Save the game to a specific slot (1–10) without showing the save menu overlay.

```crka
save 1                       ; save to slot 1
save 5                       ; save to slot 5
```

### `load <slot>`

Load a game from a specific slot (1–10) without showing the load menu overlay.

```crka
load 3                       ; load from slot 3
```

---

## Save Format

Save files are stored as JSON in the `saves/` directory within the game's project root:

```
saves/
    slot1.json
    slot2.json
    ...
    slot10.json
```

Each save file uses Glaze JSON serialization and contains the complete game state:

```json
{
    "version": 1,
    "timestamp": "2025-12-01T14:30:00",
    "programCounter": 142,
    "callStack": [],
    "variables": {
        "player_name": "Saki"
    },
    "numVariables": {
        "hp": 75.0,
        "gold": 150.0
    },
    "background": "castle.jpg",
    "characters": [
        {"id": "saki", "file": "saki_happy.png", "position": "left"}
    ],
    "bgm": "theme.ogg",
    "state": "DialogueState",
    "speaker": "saki",
    "name": "",
    "text": "What a beautiful day!",
    "displayedChars": 22,
    "skipMode": false,
    "skipDepth": 0
}
```

The `version` field enables future schema migration. All game state is captured, including the current script position (`programCounter`), call stack, variables, background, characters, BGM state, and dialogue state.

---

## Save Locations

| Method        | Player Choice | Visual Overlay | ESC to Cancel |
|---------------|:-------------:|:--------------:|:-------------:|
| `save_menu`   | Yes           | Yes            | Yes           |
| `load_menu`   | Yes           | Yes            | Yes           |
| `save <N>`    | No            | No             | No            |
| `load <N>`    | No            | No             | No            |

---

## Examples

```crka
; Autosave
save 1

; Let player choose where to save
narrate "You find a save point."
save_menu

; Auto-load
label continue_game
load_menu

; Quick save / quick load
$ quicksave_pressed = 1
if quicksave_pressed == 1
    save 10
endif
```
