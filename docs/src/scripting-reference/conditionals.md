# Conditionals

Conditionals let your script make decisions based on variable values.

## If Statement

### `if <var> <operator> <value>`

Execute a block of statements only if the comparison is true. The condition body is indented.

**String comparisons** use `==` and `!=`:

```crka
set quest_active "yes"
if quest_active == "yes"
    say saki "I still have a quest to complete."
endif
```

**Numeric comparisons** support all operators:

```crka
$ hp = 50
if hp > 0
    narrate "You're still standing!"
endif
```

### Supported Operators

| Operator | Meaning            | Works With     |
|----------|--------------------|----------------|
| `==`     | Equal to           | String, Number |
| `!=`     | Not equal to       | String, Number |
| `>`      | Greater than       | Number         |
| `<`      | Less than          | Number         |
| `>=`     | Greater or equal   | Number         |
| `<=`     | Less or equal      | Number         |

### Expression RHS

Numeric comparisons accept expressions on the right-hand side:

```crka
$ hp = 30
$ max_hp = 100
if hp < max_hp / 2
    narrate "You are wounded!"
endif

if score >= base + bonus * 2
    narrate "High score achieved!"
endif
```

---

## Else

### `else`

Provide an alternative block when the `if` condition is false:

```crka
$ hp = 0
if hp > 0
    say saki "I'm okay!"
else
    say saki "I've fallen..."
endif
```

If the `if` condition is true, the `if` block runs and the `else` block is skipped. If false, the `else` block runs instead.

---

## Endif

### `endif`

Terminates the conditional block. Every `if` must have a matching `endif`.

```crka
if gold >= 100
    narrate "You're rich!"
endif
```

---

## Nested Conditionals

Conditionals can be nested inside each other:

```crka
$ hp = 75
$ has_potion = 1

if hp > 0
    if hp < 30
        say saki "I need healing!"
    else
        if has_potion == 1
            say saki "I have a potion if things get worse."
        else
            say saki "I'm fine for now."
        endif
    endif
else
    narrate "Game Over"
endif
```

---

## Examples

```crka
; Simple health check
$ hp = 75
if hp > 50
    say saki "Feeling great!"
endif
```

```crka
; Branching with else
set key_found "yes"
if key_found == "yes"
    narrate "You unlock the door."
else
    narrate "The door is locked. Find the key."
endif
```

```crka
; Numeric threshold with expression
$ level = 5
$ xp = 450
if xp >= level * 100
    narrate "Level up!"
    $ level += 1
else
    narrate "Keep fighting."
endif
```
