# Dialogue

Dialogue commands display character speech and narration text.

## `say <id> "text"`

Display a character's spoken dialogue. The `<id>` identifies the speaking character's name in the name box, and the text appears in the dialogue box.

```crka
say saki "What a beautiful day!"
say ken "Indeed. The flowers are blooming."
```

The character name is drawn from the character file's id — it is not a separate display name. To differentiate display name from file id, set string variables:

```crka
set display_name "Saki"
say saki "Hello!"    ; name box shows "saki" (the character id)
```

### Variable Substitution

Dialogue text can include `{variable}` syntax to substitute the current value of a string or numeric variable inline.

```crka
set name "Saki"
say saki "My name is {name}."    ; displays: My name is Saki.

$ score = 100
say saki "I scored {score} points!"  ; displays: I scored 100 points.
```

## `narrate "text"`

Display narration text without a character name. The text appears in the dialogue box without a name box.

```crka
narrate "It was a quiet morning in the village."
narrate "The sun cast long shadows across the cobblestone streets."
```

Narration supports text markup and variable substitution, just like `say`.

```crka
set town "Elmswood"
narrate "Welcome to {town}."
```

---

## Text Markup

Dialogue text supports inline markup for styling. See [Text Markup](text-markup.md) for a full reference.

```crka
say saki "That was <b>really</b> exciting!"
say saki "I <color=#ff0000>love</color> this part."
```

---

## Advance Keys

By default, dialogue advances on <kbd>Space</kbd> or <kbd>Enter</kbd>. This can be configured via `ui advance_keys` in your UI configuration file. See [UI Theming](ui-theming.md).

## Examples

```crka
; Simple narration
narrate "Prologue: The journey begins."

; Character dialogue
say saki "Are you ready?"
say ken "I was born ready."

; Variable substitution
set place "the dungeon"
say saki "We're heading to {place}."

; Narration between dialogue
say ken "What's that? Behind the door..."
narrate "A faint glow emanates from the crack under the door."
```
