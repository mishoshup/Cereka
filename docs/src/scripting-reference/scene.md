# Scene

Scene commands manage backgrounds and characters on screen.

## Background

### `bg <file>`

Display a background image instantly. The file is loaded from `assets/bg/`.

```crka
bg castle.jpg           ; show castle background
bg forest_path.png      ; show forest path background
```

### `bg <file> fade <seconds>`

Crossfade between the current background and a new one over a duration.

```crka
bg castle.jpg fade 2.0      ; crossfade to castle over 2 seconds
bg night_sky.png fade 1.5   ; crossfade to night sky over 1.5 seconds
```

The fade creates a smooth transition by blending the old and new backgrounds.

---

## Characters

### `char <id> <file>`

Show a character sprite centered on screen. The file is loaded from `assets/characters/`.

```crka
char saki saki_happy.png           ; show Saki centered
char ken ken_neutral.png            ; show Ken centered
```

### `char <id> <position> <file>`

Show a character sprite at a specific position. Valid positions: `left`, `center`, `right`.

```crka
char saki left saki_happy.png      ; show Saki on the left
char ken right ken_neutral.png     ; show Ken on the right
```

Only one character can occupy each position. Placing a new character in an occupied position replaces the existing one.

### `hide char <id>`

Remove a character from the screen.

```crka
hide char saki                    ; remove Saki
hide char ken                     ; remove Ken
```

---

## Scene Graph

The scene graph provides direct control over on-screen elements through a node-based tree. This is an advanced feature used for complex scene manipulation.

### `scene_graph <id> set <properties>`

Set properties on a scene graph node identified by `<id>`.

```crka
scene_graph bg_node set opacity=0.5
scene_graph saki_node set scale=1.5 x=100
```

### `scene_graph <id> remove`

Remove a scene graph node.

```crka
scene_graph sparkle remove
```

---

## Examples

A simple scene transition:

```crka
; Start with a castle background
bg castle.jpg

; Fade to forest
bg forest_path.png fade 1.0

; Add characters
char saki left saki_happy.png
char ken right ken_neutral.png

; Remove a character
hide char ken
```
