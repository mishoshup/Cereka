---
name: test
description: Run the full test suite — Lua compile-output snapshots and C++ GoogleTest unit tests. Use when the user says "test", "run tests", "check tests", or after a refactor.
allowed-tools: Bash
---

# Run Cereka tests

Two independent suites. Run both unless the user names one.

## 1. Compile-output snapshot tests (Lua, no build needed)
```bash
lua tests/compile/harness.lua
```
- Exit 0 = all snapshots match.
- To regenerate after intentional compiler changes: `lua tests/compile/harness.lua --update` then re-run without `--update` to confirm green.
- Inputs: `tests/compile/inputs/*.crka` — expected: `tests/compile/expected/*.txt`.
- **Naming**: do NOT call these "golden tests" — the user finds the term cringe. Say "snapshot" or "compile" tests.

## 2. C++ unit tests (GoogleTest, requires build)
```bash
ninja -C build -j12 cereka_test    # build only the test target if needed
./build/tests/cereka_test           # Linux
./build/tests/cereka_test.exe       # Windows
```
- Currently covers `tests/config_test.cpp` and `tests/save_data_test.cpp`.
- Entry point is `tests/main.cpp` — cross-platform (`wmain` on Windows, `main` elsewhere).

## When tests fail
- Snapshot diff → check whether the compiler change is intentional. If yes, `--update` and review the diff. If no, fix the compiler.
- Unit test failure → never mask with `EXPECT_NEAR` slop or by deleting the test. Fix the code.
- Don't add new tests just to make the suite "look bigger" — every new test should catch a real regression class.

Report only failed cases plus a one-line pass count summary.
