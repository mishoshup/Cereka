# Flow Control

Flow control commands manage script execution order — branching, subroutines, file inclusion, and termination.

## Labels and Jumps

### `label <name>`

Define a named point in the script that can be jumped to. Labels mark positions for navigation.

```crka
label start
label battle_scene
label good_ending
```

### `jump <name>`

Unconditionally jump execution to a named label. The label must be defined within the same script file (or an included file — see `include`).

```crka
jump battle_scene        ; jump to battle_scene label
```

---

## Script Inclusion

### `include <file>`

Include another `.crka` file at compile time. The included file's content is inlined at the point of inclusion — it becomes part of the same compilation unit. Labels from included files are accessible to `jump`.

```crka
include "ui.crka"        ; include UI theme definitions
include "scenes.crka"    ; include scene definitions
```

Files are resolved relative to the project's `assets/scripts/` directory.

---

## Subroutines

### `call <file>`

Call another `.crka` file as a subroutine at runtime. Execution transfers to the called file; when the called file reaches `end`, execution returns to the instruction after the `call`.

```crka
; main.crka
narrate "Entering the forest..."
call "forest_scene.crka"
narrate "You have left the forest."
```

```crka
; forest_scene.crka
bg forest.png
say deer "Who goes there?"
narrate "A deer watches you from the shadows."
end       ; returns to main.crka
```

`call` supports nesting — a called file can itself call other files. The call stack has a maximum depth of **32 levels** to prevent runaway recursion.

```
main.crka → call "dungeon.crka" → call "boss.crka" → call "cutscene.crka"
```

---

## Termination

### `end`

End the game or terminate a subroutine called with `call`. In a main script file, `end` terminates the game. In a `call`-ed file, `end` returns to the caller.

```crka
; In main script
label game_over
narrate "Game Over"
end        ; game ends
```

```crka
; In called file
narrate "This is a subroutine"
end        ; returns to caller
```

---

## Execution Order

Scripts execute top-to-bottom by default. Control flow commands alter this order:

1. **Linear execution** — statements run in order
2. **Jump** — teleports to a label elsewhere
3. **Call** — enters a subroutine, returns on `end`
4. **Include** — compile-time inlining (not runtime)
5. **End** — terminates script or returns from call

---

## Examples

```crka
; Simple branching
label start
say saki "Where should we go?"
jump forest_path

label forest_path
bg forest.png
narrate "You follow the path into the woods."
```

```crka
; Calling a subroutine
label main
narrate "The adventure begins."
call "prologue.crka"
narrate "And so the story unfolds..."
```
