# Testing

## Frameworks

| Framework | Version | Purpose |
|-----------|---------|---------|
| GoogleTest | via FetchContent (1.14.0) | C++ unit tests |
| Lua 5.4 harness | custom (`tests/compile/harness.lua`) | Compiler snapshot tests |

## Test Suites

| Suite | Location | Type | Run Command |
|-------|----------|------|-------------|
| C++ unit tests | `tests/config_test.cpp`, `tests/save_data_test.cpp` | Unit (GoogleTest) | `ninja -C build cereka_test && ./build/tests/cereka_test` |
| Compile snapshots | `tests/compile/` | Snapshot/regression | `lua tests/compile/harness.lua` |

## Running Tests

**C++ unit tests:**
```bash
ninja -C build cereka_test
./build/tests/cereka_test
```

**Compile snapshot tests:**
```bash
lua tests/compile/harness.lua
# Regenerate expected after intentional compiler changes:
lua tests/compile/harness.lua --update
```

**Both suites (via project skill):**
```
/test
```

## Coverage Areas

### C++ Unit Tests
| Test File | What It Covers |
|-----------|---------------|
| `config_test.cpp` | `ConfigManager` — property registration, typed apply, parse cycle |
| `save_data_test.cpp` | `SerializableSaveData` — glaze JSON round-trip (all fields) |

### Compile Snapshot Tests
| Input Scenario | What It Covers |
|---------------|---------------|
| `audio.crka` | `bgm`, `stop_bgm`, `sfx` instructions |
| `basic.crka` | `narrate`, `say`, `bg`, `char`, `hide`, `end` |
| `comments.crka` | Comment stripping (`;` prefix) |
| `flow.crka` | `label`, `jump`, `call`, `include`, `if/else/endif` |
| `menu.crka` | `menu`, `button goto/exit`, menu bg swap |
| `save_load.crka` | `save_menu`, `load_menu`, `save N`, `load N` |
| `ui_theme.crka` | `ui textbox/namebox/button/font/advance_keys` |
| `variables.crka` | `set`, `$ arithmetic`, `{var}` substitution |

## Test Naming Conventions

**C++ (GoogleTest):**
```cpp
TEST_F(FixtureName, PascalCaseName) {
    ...
}
```
Two-line format: fixture on first line, test name on second.

**Snapshot inputs:** `<scenario>.crka` → `expected/<scenario>.txt`

## Gaps

Areas with **no test coverage**:
- `script_vm.cpp` — VM dispatch loop (`TickScript`), expression evaluator (`EvalExpr`)
- `SceneManager`, `AudioManager`, `DialogueSystem`, `MenuSystem` — all extracted subsystems
- `CerekaStateMachine` and all concrete states (`cereka_states.cpp`)
- `save.cpp` — file-level save/load I/O (only data struct is tested, not I/O)
- `draw.cpp` — rendering (inherently hard to unit test)
- `ui_config.cpp` — UI property application
- Compiler error paths — invalid `.crka` syntax is not snapshot-tested
- `launcher/` — Qt6 components have no automated tests

## CI/CD

No CI/CD pipeline detected. Tests are run manually. No `.github/workflows/`, no Makefile test targets beyond the build system.
