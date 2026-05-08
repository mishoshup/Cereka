# Audio

Audio commands control background music and sound effects.

## Background Music

### `bgm <file>`

Play a looping background music track. The file is loaded from `assets/sounds/`.

```crka
bgm theme.ogg              ; play theme music on loop
bgm forest_ambient.wav     ; play forest ambient on loop
```

Switching BGM replaces the current track immediately. Only one BGM plays at a time.

### `bgm <file> fade(<seconds>)`

Fade in a new BGM track over a duration while the current track fades out simultaneously. This creates a smooth transition between songs.

```crka
bgm battle.ogg fade(2.0)       ; crossfade from current BGM to battle theme over 2 seconds
bgm victory.wav fade(1.5)      ; crossfade to victory fanfare over 1.5 seconds
```

### `bgm <file> crossfade(<seconds>)`

Alias for `bgm ... fade(N)`. Performs a simultaneous fade-out of the current track and fade-in of the new track.

```crka
bgm sad_theme.ogg crossfade(3.0)
```

### `stop_bgm`

Stop the currently playing background music immediately.

```crka
stop_bgm                       ; silence
```

### `stop_bgm fade(<seconds>)`

Fade out the current BGM over a duration before stopping.

```crka
stop_bgm fade(2.0)            ; fade out over 2 seconds
```

---

## Sound Effects

### `sfx <file>`

Play a one-shot sound effect. The file is loaded from `assets/sounds/`. Sound effects play on top of BGM and do not loop.

```crka
sfx door_creak.wav            ; play door creak
sfx sword_swing.wav           ; play sword swing
sfx coin_pickup.ogg           ; play coin pickup
```

Multiple SFX can overlap — the engine handles concurrent playback.

---

## Supported Formats

The engine supports common audio formats through SDL3_mixer:

| Format | Description       |
|--------|-------------------|
| OGG    | Vorbis compressed |
| WAV    | Uncompressed PCM  |
| FLAC   | Lossless          |
| MP3    | MPEG audio        |

---

## Examples

```crka
; Start background music
bgm peaceful_village.ogg

; Play a sound effect
sfx bird_chirp.wav

; Crossfade to a new track
bgm tense_moment.ogg fade(2.0)

; Stop music with fade
stop_bgm fade(1.0)
```
