# Getting Started

This guide walks you through creating your first Cereka visual novel project.

## Prerequisites

You need a compiled Cereka engine binary for your platform:
- **Linux:** `CerekaGame` binary
- **Windows:** `CerekaGame.exe`

## Project Structure

A Cereka game project has the following layout:

```
my-game/
├── game.cfg              # Game configuration
├── assets/
│   ├── scripts/
│   │   ├── main.crka     # Entry point script
│   │   └── ui.crka       # UI theme definitions
│   ├── bg/               # Background images
│   ├── characters/       # Character sprites
│   ├── sounds/           # BGM and SFX audio files
│   ├── fonts/            # TTF/OTF font files
│   └── ui/               # UI element images (optional)
└── saves/                # Auto-created save directory
```

## Creating Your First Game

### 1. Create the project folder

```
mkdir my-first-game
cd my-first-game
mkdir -p assets/scripts assets/bg assets/characters assets/sounds assets/fonts
```

### 2. Create game.cfg

```ini
title = "My First Game"
width = 1280
height = 720
fullscreen = false
entry = "main.crka"
```

### 3. Write a UI theme

Create `assets/scripts/ui.crka`:

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

ui advance_keys space enter
```

### 4. Write your first scene

Create `assets/scripts/main.crka`:

```
; Include UI theme
include "ui.crka"

; Start the game
label start

narrate "Welcome to my first visual novel!"

say saki "Hello! I'm Saki. Welcome to the world of Cereka."

say saki "Let's take a walk together."

bg park.png fade 1.0

char saki left saki_happy.png

say saki "The weather is beautiful today."

menu
    button "Talk about the weather" goto weather
    button "Change the subject" goto subject

label weather
say saki "Isn't the sky lovely?"
jump end

label subject
say saki "I have something important to tell you."

label end
narrate "Thanks for playing!"
```

### 5. Run the game

Place your `CerekaGame` binary next to your project folder and run:

```bash
./CerekaGame my-first-game/
```

Or use CerekaLauncher to open and run the project.

## Next Steps

- Learn the full scripting language in the [Scripting Reference](scripting-reference/scene.md)
- Customize your UI with [UI Theming](scripting-reference/ui-theming.md)
- Distribute your game with [Build & Package](build-and-package.md)
