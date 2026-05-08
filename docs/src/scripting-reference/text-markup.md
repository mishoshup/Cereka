# Text Markup

Text markup allows inline styling within dialogue and narration text. Tags use angle brackets and can be nested.

## Supported Tags

| Tag                        | Description        |
|----------------------------|--------------------|
| `<b>`...`</b>`             | Bold text          |
| `<i>`...`</i>`             | Italic text        |
| `<u>`...`</u>`             | Underlined text    |
| `<s>`...`</s>`             | Strikethrough text |
| `<color=#rrggbb>`...`</color>` | Colored text  |

---

## Bold

Wrap text in `<b>` and `</b>` to make it bold.

```crka
say saki "That was <b>really</b> important!"
say saki "I <b>must</b> find the treasure."
```

## Italic

Wrap text in `<i>` and `</i>` to italicize it.

```crka
narrate "The letter read: <i>Meet me at midnight.</i>"
say saki "<i>I wonder what that means...</i>"
```

## Underlined

Wrap text in `<u>` and `</u>` to underline it.

```crka
say saki "Please read the <u>fine print</u>."
```

## Strikethrough

Wrap text in `<s>` and `</s>` to strikethrough it.

```crka
say saki "I was going to say something <s>stupid</s> clever."
```

## Colored Text

Use `<color=#rrggbb>` to change text color, and `</color>` to revert. The color value is a hex RGB triplet.

```crka
say saki "This is <color=#ff0000>red</color> text."
say saki "The sky is <color=#4488ff>blue</color> today."
say saki "Mixed <b><color=#ff8800>bold and orange</color></b> text."
```

---

## Nesting

Tags can be nested, but must be closed in reverse order (LIFO):

```crka
say saki "<b><i>Bold and italic</i></b>"
say saki "<color=#ff0000><b>Red and bold</b></color>"
```

---

## Escape Sequences

To include a literal `<` or `>` character in your text, double it:

| Escape | Output |
|--------|--------|
| `<<`   | `<`    |
| `>>`   | `>`    |

```crka
say saki "The symbol << is less than, >> is greater than."
narrate "The result was 5 << 10 (true)."
```

---

## Examples

```crka
; Combined styling
say saki "I <b>love</b> <color=#ffcc00>gold</color>!"

; Narrative emphasis
narrate "The <i>ancient</i> tome glowed with <color=#ff00ff>purple</color> energy."

; Dialogue with emotion
say saki "I can't believe you said that! <s>I'm so embarrassed.</s>"
```
