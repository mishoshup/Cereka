# Project Structure

A Cereka game project follows a standard directory layout.

## Directory Layout

```
my-game/
├── game.cfg              # Game configuration
├── assets/               # All game assets
│   ├── scripts/          # .crka script files
│   │   ├── main.crka     # Entry point (specified in game.cfg)
│   │   ├── ui.crka       # UI theme definitions
│   │   └── ...           # Additional script files
│   ├── bg/               # Background images
│   │   ├── castle.jpg
│   │   ├── forest.png
│   │   └── ...
│   ├── characters/       # Character sprites
│   │   ├── saki_happy.png
│   │   ├── ken_neutral.png
│   │   └── ...
│   ├── sounds/           # Audio files (OGG, WAV, FLAC, MP3)
│   │   ├── theme.ogg
│   │   ├── sfx_coin.wav
│   │   └── ...
│   ├── fonts/            # Font files (TTF, OTF)
│   │   ├── NotoSans-Regular.ttf
│   │   └── ...
│   └── ui/               # UI element images (optional)
│       ├── textbox.png
│       ├── button.png
│       ├── button_hover.png
│       └── ...
└── saves/                # Auto-created save directory
    ├── slot1.json
    ├── slot2.json
    └── ...
```

## Configuration File

### `game.cfg`

The `game.cfg` file defines project settings in a simple key=value format:

| Key          | Default   | Description                          |
|--------------|-----------|--------------------------------------|
| `title`      | "Game"    | Window title                         |
| `width`      | 1280      | Window width in pixels               |
| `height`     | 720       | Window height in pixels              |
| `fullscreen` | false     | Start in fullscreen mode             |
| `entry`      | -         | Main script file (relative to assets/scripts/) |

Example:

```ini
title = "My Visual Novel"
width = 1920
height = 1080
fullscreen = false
entry = "main.crka"
```

## Asset Directories

### `assets/scripts/`

Contains all `.crka` script files. The entry point defined in `game.cfg` is resolved relative to this directory. Scripts can include and call other scripts using `include` and `call` commands.

### `assets/bg/`

Background images displayed with the `bg` command. Supported formats depend on SDL3_image: PNG, JPEG, BMP, GIF, WebP, etc.

### `assets/characters/`

Character sprite images displayed with the `char` command. Same supported formats as backgrounds.

### `assets/sounds/`

Audio files for background music and sound effects. Supported formats: OGG Vorbis, WAV, FLAC, MP3 (via SDL3_mixer).

### `assets/fonts/`

Font files in TTF or OTF format. The engine loads the first available font file from this directory.

### `assets/ui/`

Optional UI element images for custom textbox, name box, and button appearances.

## Saves

The `saves/` directory is created automatically on first save. Save files are JSON format, named `slot{N}.json` where N is the slot number (1–10).

See [Save & Load](scripting-reference/save-load.md) for save format details.
