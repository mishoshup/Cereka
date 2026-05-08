# Variables & Expressions

Cereka provides two variable systems: string variables for text data and numeric variables for arithmetic.

## String Variables

### Setting String Variables

Use the `set` command to store text values:

```crka
set player_name "Saki"
set current_location "forest"
set quest_status "active"
```

Values can be quoted (with double quotes) or bare single words:

```crka
set weapon sword           ; single word, no quotes needed
set title "The Brave"      ; multi-word, quotes required
```

### Using String Variables

String variables are substituted into dialogue and narration text using `{name}` syntax:

```crka
set name "Saki"
say saki "Hello, my name is {name}!"

set location "the dark forest"
narrate "You arrive at {location}."
```

This makes string variables useful for:
- Player name tracking
- Dynamic location descriptions
- Quest state messages
- Character status updates

---

## Numeric Variables

### Setting Numeric Variables

Use the `$` prefix with assignment operators:

```crka
$ score = 0           ; assign
$ hp = 100
$ gold = 50
```

### Compound Assignment

Modify existing values with compound operators:

```crka
$ score += 10         ; add 10
$ hp -= 15            ; subtract 15
$ gold *= 2           ; multiply by 2
$ mana /= 3           ; divide by 3
```

### Expressions

The right-hand side of numeric assignments supports full arithmetic expressions:

| Example                    | Result    |
|----------------------------|-----------|
| `$ x = 5 + 3`             | 8         |
| `$ x = 10 - 4`            | 6         |
| `$ x = 3 * 4`             | 12        |
| `$ x = 20 / 5`            | 4         |
| `$ x = 5 + 3 * 2`         | 11        |
| `$ x = (5 + 3) * 2`       | 16        |
| `$ x = base + bonus * 2`  | Depends   |

### Variables in Expressions

Numeric variable names can appear on the right-hand side:

```crka
$ base_damage = 10
$ strength = 5
$ total_damage = base_damage + strength * 2     ; total_damage = 20
```

### Expression Precedence

| Precedence | Operators | Example                     |
|------------|-----------|-----------------------------|
| 1 (highest)| `( )`     | `(a + b) * c`               |
| 2          | unary `-` | `-value`                    |
| 3          | `* /`     | `a * b + c` (multiply first)|
| 4 (lowest) | `+ -`     | `a + b * c` (multiply first)|

---

## Using Variables in Conditionals

Variables are essential for branching logic:

```crka
$ hp = 75

if hp > 50
    say saki "I'm doing fine!"
else
    say saki "I need help..."
endif
```

String variables work in conditionals too:

```crka
set quest_complete "yes"

if quest_complete == "yes"
    narrate "Quest complete! You earned 100 gold."
    $ gold += 100
endif
```

For conditionals, `==` and `!=` work with both string and numeric variables. The numeric comparison operators (`>`, `<`, `>=`, `<=`) work with numeric variables and accept expression RHS:

```crka
$ level = 5
if xp >= level * 100
    narrate "Level up!"
endif
```

## Best Practices

1. **Keep variable names distinct:** Avoid using the same name for a string and numeric variable
2. **Use descriptive names:** `player_hp` is clearer than `hp`
3. **Initialize variables early:** Set defaults at the start of your script
4. **Name spacing:** Prefix related variables (e.g., `player_hp`, `player_gold`, `enemy_hp`)
5. **Check before using:** Use conditionals to verify variables have expected values

See the [Variables](scripting-reference/variables.md) and [Conditionals](scripting-reference/conditionals.md) reference pages for complete documentation.
