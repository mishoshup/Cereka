# Variables

Cereka supports both string and numeric variables for tracking game state.

## String Variables

### `set <name> <value>`

Set a string variable to a value. Value can be quoted (with double quotes) or bare (a single word).

```crka
set player_name "Saki"
set town "Elmswood"
set weapon sword
```

String variables can be used in dialogue via `{variable}` substitution:

```crka
set name "Saki"
say saki "My name is {name}."        ; displays: My name is Saki.
```

---

## Numeric Variables

### `$ <name> = <expression>`

Assign the result of an expression to a numeric variable. Creates the variable if it doesn't exist.

```crka
$ score = 0
$ health = 100
$ gold = 50
```

Expressions support standard arithmetic with operator precedence (`*` and `/` evaluated before `+` and `-`).

| Operator | Example               |
|----------|-----------------------|
| `=`      | `$ score = 10`        |
| `+=`     | `$ score += 5`        |
| `-=`     | `$ score -= 3`        |
| `*=`     | `$ gold *= 2`         |
| `/=`     | `$ gold /= 4`         |

### Compound Assignment Operators

Numeric variables support compound assignment for common modifications:

```crka
$ health -= 10            ; subtract 10 from health
$ score += 1              ; increment score by 1
$ gold *= 2               ; double gold
$ mana /= 3               ; divide mana by 3
```

### Expression Syntax

The right-hand side of a numeric assignment (`=`, `+=`, `-=`, `*=`, `/=`) is an arithmetic expression with:

- Numbers: `10`, `3.5`
- Variable references (bare names): `score`, `health`, `base_damage`
- Operators: `+`, `-`, `*`, `/`
- Parentheses for grouping: `(a + b) * c`
- Unary minus: `-value`

```crka
$ total = base + bonus * 2          ; bonus * 2 is evaluated first
$ damage = (strength + weapon) * 1.5
$ health = max_hp - damage_taken
$ counter = -counter                ; negation
```

**Operator precedence** (highest to lowest):

1. Parentheses `( )`
2. Unary minus `-`
3. Multiplication `*`, Division `/`  (left-to-right)
4. Addition `+`, Subtraction `-`   (left-to-right)

### Variable Lookup

When evaluating expressions, the engine first checks numeric variables. If not found, it tries to parse the string variable as a number (using `std::stof`). If that fails, the value is treated as `0`.

```crka
set lives "3"
$ total = lives + 1                 ; total = 4 (lives parsed as number)

$ count = 10
set label "hello"
$ result = count + 5                ; result = 15 (uses numeric variable)
```

---

## Variable Types Compared

| Type    | Command       | Example                     | Use Case         |
|---------|---------------|-----------------------------|-------------------|
| String  | `set`         | `set name "Saki"`           | Character names, labels, flags |
| Numeric | `$`           | `$ score = 42`              | HP, gold, counters |
| Numeric | `$ ... =`     | `$ hp -= damage`            | Math operations   |

String and numeric variables share a common namespace. Using the same name for both a string and numeric variable will cause unexpected behavior — choose one type per variable name.

---

## Examples

```crka
; String variables
set player_name "Saki"
set current_location "forest"
say saki "I'm at the {current_location}."

; Numeric variables
$ hp = 100
$ max_hp = 100
$ gold = 0

; Taking damage
$ hp -= 25
if hp <= 0
    narrate "You have fallen!"
endif

; Reward
$ gold += 50
say saki "Found {gold} gold pieces!"

; Expression with grouping
$ total_score = (enemies * 100) + (treasure * 50)
```
