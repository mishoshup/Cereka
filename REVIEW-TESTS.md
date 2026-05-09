---
phase: code-review-tests
reviewed: 2026-05-09T12:00:00Z
depth: deep
files_reviewed: 16
files_reviewed_list:
  - tests/main.cpp
  - tests/config_test.cpp
  - tests/save_data_test.cpp
  - tests/cereka_script_test.cpp
  - tests/scene_graph_test.cpp
  - tests/display_test.cpp
  - tests/audio_manager_test.cpp
  - tests/markup_parser_test.cpp
  - tests/rollback_manager_test.cpp
  - tests/compile/harness.lua
  - tests/compile/inputs/*.crka (14 files)
  - tests/compile/expected/*.txt (14 files)
  - scripts/cereka_compiler.lua
  - CMakeLists.txt
  - src/CMakeLists.txt
  - tests/CMakeLists.txt
findings:
  critical: 10
  warning: 14
  info: 6
  total: 30
status: issues_found
---

# Phase: Code Review Report — Test Suite & Build System

**Reviewed:** 2026-05-09T12:00:00Z
**Depth:** deep
**Files Reviewed:** 16 (9 C++ test files, 2 CMake files, Lua compiler, compile harness, 14 snapshot inputs/expected)
**Status:** issues_found

## Summary

The test suite has significant structural issues. Three test files (audio_manager_test.cpp, rollback_manager_test.cpp, display_test.cpp) are **actively misleading** — they either test duplicated logic instead of production code, test nothing at all despite claiming otherwise, or test only the SDL API surface without covering any Cereka engine code. The cereka_script_test.cpp only tests `if/else/endif` skip-mode logic and covers none of the other 30+ instruction types. CTest integration is disabled, so `ninja test` won't discover or run any Cereka tests. The snapshot tests are the most solid coverage but miss error paths entirely.

---

## Critical Issues

### CR-01: RollbackManagerTest tests DO NOT test the claimed behavior

**File:** `tests/rollback_manager_test.cpp:49-53`
**Issue:** Two test functions have names describing actions they never perform:

1. `CanRollbackAfterCapture` (line 49): Creates a `RollbackManager(10)`, calls `EXPECT_FALSE(rm.canRollback())`, but **never calls `rm.capture()`**. It confirms the initial state is non-rollbackable, but doesn't verify that capture enables rollback. The function name explicitly says "AfterCapture" — this is misleading.

2. `CountIncrementsOnCapture` (line 29): Creates a `RollbackManager rm(10)`, checks `EXPECT_EQ(rm.count(), 0)`, but **never calls `rm.capture()`**. The function name says "IncrementsOnCapture" — it doesn't test that at all. It only checks that the initial count is zero.

**Fix:** Remove the misleading test names or add the actual capture + verify assertions:
```cpp
TEST_F(RollbackManagerTest, CanRollbackAfterCapture) {
    RollbackManager rm(10);
    EXPECT_FALSE(rm.canRollback());
    CerekaImpl impl;
    rm.capture(impl);  // Actually capture
    EXPECT_TRUE(rm.canRollback());  // Now verify
}
```

### CR-02: Audio fade curve tests test duplicated logic, not production code

**File:** `tests/audio_manager_test.cpp:8-18`
**Issue:** The `applyCurve()` function is defined in an anonymous namespace inside the test file itself. It duplicates the fade curve math that should exist in `AudioManager::Update()`. If the production fade curve formulas are changed, these tests will **still pass** because they test a copy of the logic, not the real implementation. This is the classic "test double that never fails" anti-pattern.

The test file name is `audio_manager_test.cpp` but it never instantiates or tests the `AudioManager` class — not a single `PlayBGM()`, `StopBGM()`, `PlaySFX()`, `Init()`, `Shutdown()`, `CrossfadeBGM()`, or `Update()` call exists.

**Fix:** Either:
1. Move the curve calculation into a public/protected method on `AudioManager` and test that method, OR
2. Extract the curve math into a free function in a shared header and test that, OR
3. Remove the test-local duplicate and add integration tests that verify fade behavior through the `AudioManager` interface.

### CR-03: CTest integration disabled — `ninja test` won't run Cereka tests

**File:** `tests/CMakeLists.txt:52`
**Issue:** `gtest_discover_tests(cereka_test)` is permanently commented out with the note "# Disabled due to DLL path issues". This means:
- `ctest` and `ninja test` do NOT discover or run Cereka's unit tests
- Tests must be run manually via `./build/tests/cereka_test`
- CI pipelines using `ctest` will silently skip all Cereka tests
- No test count or pass/fail reporting through CTest

**Fix:** Either fix the DLL path issue (set `CMAKE_RUNTIME_OUTPUT_DIRECTORY` so DLLs are found) or add a custom target:
```cmake
add_custom_target(run_cereka_tests
    COMMAND cereka_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS cereka_test
)
```

### CR-04: GLOB_RECURSE in src/CMakeLists.txt requires re-running CMake for new files

**File:** `src/CMakeLists.txt:1-2`
**Issue:** Uses `file(GLOB_RECURSE SRC CONFIGURE_DEPENDS .../*.cpp)` to discover source files. The `CONFIGURE_DEPENDS` flag is not reliably supported by Ninja and older CMake generators. CLAUDE.md itself documents: "re-run cmake after adding/removing .cpp files". This is a known footgun that will silently produce stale builds. A developer adding a new `.cpp` file will see no compilation error but the file won't be linked.

**Fix:** Explicitly list source files instead of using GLOB_RECURSE:
```cmake
set(SRC
    Cereka.cpp
    cereka_script.cpp
    cereka_draw.cpp
    ...
)
```

### CR-05: save_data_test.cpp has zero coverage of `numVariables` (float variables)

**File:** `tests/save_data_test.cpp:26-66`
**Issue:** The `RoundtripSerializationPreservesData` test populates `original.variables` (string variables) but never populates or verifies `original.numVariables` (float/numeric variables per `SerializableSaveData` line 36). This means round-trip fidelity of numeric variables is completely untested despite being a critical feature (the `$ var += expr` system).

**Fix:** Add numeric variables to the round-trip test:
```cpp
original.numVariables["score"] = 100.5f;
original.numVariables["multiplier"] = 2.0f;
// ... verify after round-trip
```

### CR-06: CerekaScript VM tests only exercise if/else/endif — 0 of 30+ other ops tested

**File:** `tests/cereka_script_test.cpp`
**Issue:** All three test functions (`NestedIfElseBug`, `IfTrueElseSkipped`, `DeeplyNestedIfElse`) only test the skip-mode logic for if/else/endif. The following critical instruction types have **zero** VM test coverage:
- Arithmetic ops: `SET_VAR_NUM` (all operators: +=, -=, *=, /=)
- Flow: `JUMP`, `CALL`, `RETURN`, `LABEL`
- Menu: `MENU`, `BUTTON` with goto/exit
- Save/load: `SAVE`, `LOAD`, `SAVE_MENU`, `LOAD_MENU`
- Audio: `PLAY_BGM`, `STOP_BGM`, `PLAY_SFX`
- Checkpoint: `CHECKPOINT_STORE`, `CHECKPOINT_LOAD`
- Scene graph: `SG_SET`, `SG_REMOVE`
- UI: `UI_SET`
- Error handling: division by zero, undefined variable dereference, call-stack overflow (max 32)

**Fix:** Add VM-execution tests for at minimum: JUMP, CALL/RETURN, SET_VAR_NUM arithmetic, MENU/BUTTON, and error conditions.

### CR-07: Scene graph tests never create parent-child relationships

**File:** `tests/scene_graph_test.cpp`
**Issue:** The `SceneNode` struct (scene_graph.hpp:22-23) has `parent` pointer and `children` vector — this is the core feature of a scene graph. However, every test creates only root-level nodes. No test:
- Creates a child node with `scene_graph.hpp`'s `children.push_back()`
- Verifies `updateTransforms()` accumulates parent transforms into children
- Tests `world` transform computation through a hierarchy
- Tests removing a node that has children (orphan management)
- Tests `visit()` order with parent-child hierarchy

**Fix:** Add hierarchy tests:
```cpp
TEST(SceneGraphTest, ParentChildTransform) {
    SceneGraph sg;
    auto *parent = sg.createNode("parent");
    auto *child = sg.createNode("child");
    // Wire parent-child relationship directly:
    parent->children.push_back(std::make_unique<SceneNode>(...));
    parent->local.scaleX = 2.0f;
    sg.updateTransforms();
    EXPECT_FLOAT_EQ(child->world.scaleX, 2.0f);  // inherited
}
```

### CR-08: Invalid JSON handling never tested

**File:** `tests/save_data_test.cpp:118-123`
**Issue:** `EmptyJsonReturnsTrue` only tests empty string `""` and `"{}"`. It never tests:
- Malformed JSON (`"{broken"`, `"not json"`)
- Type-mismatched JSON (`{"programCounter": "not_a_number"}`)
- Partial JSON (`{"version": 42}` — missing fields)
- JSON null (`"null"`)

If `glz::read_json` throws or silently corrupts data on invalid input, no test catches it.

**Fix:** Add:
```cpp
EXPECT_FALSE(jsonToSaveData(data, "{broken"));
EXPECT_FALSE(jsonToSaveData(data, "\"not an object\""));
```

### CR-09: Compile snapshot harness has no error-case coverage

**File:** `tests/compile/inputs/*.crka` (all 14 input files)
**Issue:** All 14 snapshot inputs test only **valid, well-formed** `.crka` scripts. There are zero test inputs for:
- Syntax errors (unterminated strings, invalid characters)
- Semantic errors (undefined labels, recursive includes, circular calls)
- Malformed blocks (unindented menu children, missing button labels)
- Out-of-range values (save slot 0, slot 11)
- Empty scripts, scripts with only comments/whitespace
- 32+ deep call stack overflow
- Division by zero in arithmetic expressions

The compiler's error recovery and diagnostic messages are completely untested.

**Fix:** Add an `inputs/errors/` subdirectory with error-case inputs and expected error string patterns.

### CR-10: display_test.cpp tests SDL surface operations, not Cereka rendering

**File:** `tests/display_test.cpp`
**Issue:** Every test in this file tests SDL3 API surface or raw TTF operations. None of the tests call into any Cereka rendering code (`cereka_draw.cpp`, `cereka_text_renderer.hpp`, `rich_text_renderer.hpp`, `sdl_render_context.hpp`). The file is an SDL3 integration smoke test, not a Cereka engine test. It belongs in a separate standalone project or should be renamed to `sdl_integration_test.cpp`. More importantly, there are **zero** tests for:
- `DrawRichText()` word-wrap logic
- Background rendering
- Character sprite rendering
- Menu button rendering
- Save/load overlay rendering

**Fix:** Either (a) rename to `sdl_integration_test.cpp` to set correct expectations, or (b) add actual Cereka rendering function calls to test.

---

## Warnings

### WR-01: `numVariables` missing from compile snapshot coverage

**File:** `tests/compile/inputs/variables.crka`
**Issue:** The variables snapshot test covers `SET_VAR` (string) and `SET_VAR_NUM` with all operators (`=`, `+=`, `-=`, `*=` all shown), but has no coverage of `/=` (divide-equal). The compile snapshot lowerer handles this op but there's no input exercising it.

**Fix:** Add `$ gold /= 2` to `variables.crka`.

### WR-02: RollbackManager::restore(), goTo(), and circular buffer wrapping untested

**File:** `tests/rollback_manager_test.cpp`
**Issue:** The RollbackManager has `restore()`, `goTo()` methods and a circular buffer (`head_`, `count_`, `enabled_` fields from rollback_manager.hpp), but none are tested:
- No test fills the buffer past capacity to test wrapping
- No test calls `restore()` and verifies state restoration
- No test calls `goTo()` with valid/invalid indices
- No test for `prevIndex()` edge cases (empty buffer, single entry)

### WR-03: Markup parser tests don't test italic+underline+strikethrough combinations

**File:** `tests/markup_parser_test.cpp`
**Issue:** The `AllStyleTags` test verifies `<b>`, `<i>`, `<u>`, `<s>` individually but never tests combinations like `<b><i>both</i></b>`. The `MultipleTagsSameSegment` test only covers `<b><i>`. No test covers `<u><s>` or `<i><s>` or all four combined.

### WR-04: No test for `<<escaped` angle brackets mixed with real tags

**File:** `tests/markup_parser_test.cpp`
**Issue:** The `AngleEscape` test and `NoSpuriousStyleOnPlainText` test verify individual angle-bracket escaping cases, but no test mixes escaped brackets with real tags (e.g., `"<b>bold << real</b>"`).

### WR-05: `list_inputs()` in compile harness uses `io.popen("ls ...")`

**File:** `tests/compile/harness.lua:61`
**Issue:** The `list_inputs()` function shells out to `ls` to list directory contents:
```lua
local p = io.popen("ls '" .. INPUTS .. "' 2>/dev/null | sort")
```
This is fragile and non-portable (fails on Windows, fails if `ls` is not in PATH). It also triggers shell metacharacter processing on the INPUTS path (though INPUTS is derived from `arg[0]` which is not user-controlled in this context, the pattern itself is dangerous).

**Fix:** Use Lua's `io.popen` with `find` or, better, implement a pure-Lua directory listing:
```lua
local function list_inputs()
    local files = {}
    local f = io.popen("ls '" .. INPUTS .. "' 2>/dev/null | sort")
    ...
end
```
Since Lua 5.4 doesn't have `dir` built-in, use `io.popen("find ...")` or accept the shell dependency but at minimum sanitize the path.

### WR-06: No test for `RETURN` instruction generation from `call`

**File:** `tests/compile/inputs/flow.crka`
**Issue:** The `flow.crka` snapshot test covers `CALL` but there's no expected `RETURN` instruction in the output. The compiler should generate a `RETURN` after every `CALL`'s included content. Either `flow.crka` doesn't exercise this, or the expected output is missing `RETURN`.

### WR-07: `FadeStateDefaults` doesn't test that oldTrack/oldAudio are null

**File:** `tests/audio_manager_test.cpp:61-63`
**Issue:** The `BgmFade` struct has `MIX_Track *oldTrack = nullptr` and `MIX_Audio *oldAudio = nullptr` but the `FadeStateDefaults` test only checks `state`, `timer`, and `duration`. The pointer defaults are not verified.

### WR-08: VM tests leave engine subsystems uninitialized

**File:** `tests/cereka_script_test.cpp:10-23`
**Issue:** The `VMTest` fixture creates a full `CerekaImpl engine` but only sets up the state machine. The engine has `window = nullptr`, `m_renderCtx = nullptr`, `font = nullptr`, `audio` uninitialized, etc. If any instruction handler path attempts to use SDL/rendering resources (e.g., `BG`, `CHAR`, `PLAY_BGM`), it will cause undefined behavior or a crash. The test happens to work because only `SAY`/`NARRATE` (dialogue-only) ops are tested, but this is extremely fragile.

**Fix:** Either (a) mock the subsystems, or (b) add a safeguard in instruction handlers that checks for null window/renderer before proceeding, or (c) at minimum document this constraint clearly.

### WR-09: No test for `scene_graph create` (SG_CREATE) op

**File:** `tests/compile/inputs/scene_graph.crka`
**Issue:** The Op enum defines `SG_CREATE` but neither the compile snapshot tests nor the C++ VM tests exercise it. Only `SG_SET` and `SG_REMOVE` are tested.

### WR-10: `cm.serialize(oss)` test only checks substring presence

**File:** `tests/config_test.cpp:111-116`
**Issue:** The serialize test verifies `"textbox.color"` appears somewhere in the serialized output. It doesn't verify the serialization format: key-value structure, ordering, escaping, or multi-line format. A regression that produced `"textbox.color"` in a different format or alongside garbage would pass this test.

### WR-11: Markup parser `UnclosedTagGraceful` test doesn't verify opening tag resets after

**File:** `tests/markup_parser_test.cpp:53-58`
**Issue:** The unclosed tag `"<b>unclosed"` test verifies the segment is bold but doesn't verify what happens if text follows — does bold leak to the next segment? E.g., `ParseMarkup("<b>unclosed</b> trailing")` is tested, but `ParseMarkup("<b>unclosed trailing")` (no closing tag) isn't tested for style leakage.

### WR-12: No snapshot test for `menu` with `button` labels exceeding normal length

**File:** `tests/compile/inputs/menu.crka`
**Issue:** Button labels are short ("Go north", "Go south"). No snapshot tests cover long button text that might trigger width-wrapping or truncation in the compiler (though compile-time doesn't enforce width, this is about ensuring the compiler handles long strings correctly).

### WR-13: `SDL_RenderCoordinatesToWindow` test assumes positive coordinates

**File:** `tests/display_test.cpp:150-151`
**Issue:** The test asserts `EXPECT_GT(rx, 0)` and `EXPECT_GT(ry, 0)` after converting render coords (640, 360) to window coords. In a hidden window with the dummy driver, `SDL_RenderCoordinatesToWindow` might return different values depending on the platform. The test makes assumptions about SDL3's behavior with hidden windows that may not hold across all platforms.

### WR-14: No test for `load_menu` or `save_menu` interaction with game state

**File:** `tests/cereka_script_test.cpp` (all VM tests)
**Issue:** The `SAVE_MENU` and `LOAD_MENU` ops transition the state machine to overlay states, but no test verifies that saving/loading correctly serializes and restores the engine state through the VM.

---

## Info

### IN-01: `RichTextWordWrapLoop` test name is misleading — it doesn't test Cereka's DrawRichText

**File:** `tests/display_test.cpp:300`
**Issue:** The test comment says "Simulates the DrawRichText word-wrapping loop" and the test name claims `RichTextWordWrapLoop`. But it only tests `TTF_MeasureString` behavior, not the actual `DrawRichText()` function in `cereka_text_renderer.hpp`. If someone changes `DrawRichText()`, this test won't catch regressions. Rename to `TtfMeasureStringWrappingLoop`.

### IN-02: `NestedIfElseBug` test name references a bug that no longer exists

**File:** `tests/cereka_script_test.cpp:26`
**Issue:** The test is named `NestedIfElseBug` suggesting it exists to prevent regression of a specific bug. This is good practice, but the name is opaque — a future developer won't know what bug. Add a comment describing the bug scenario.

### IN-03: No `TTF_WasInit()` check before `TTF_Quit()` in display tests

**File:** `tests/display_test.cpp:182, 217, 257, 297, 351`
**Issue:** Each display test calls `TTF_Init()` at the start and `TTF_Quit()` at the end. SDL3_ttf uses a reference count, so calling `TTF_Quit()` more times than `TTF_Init()` would deinitialize the library for other tests. Tests should pair init/quit properly, but the current approach (init + quit inside each test) is fragile if test ordering changes or if exception-like early exits happen. Consider using `TTF_WasInit()` in a test fixture's `SetUp`/`TearDown`.

### IN-04: `FindFont()` returns first found font nondeterministically

**File:** `tests/display_test.cpp:48-63`
**Issue:** `FindFont()` iterates `assets/fonts/` then `tests/` and picks `candidates[0]`, which depends on filesystem iteration order. This is not deterministic across platforms. If multiple fonts are present, different platforms may pick different fonts, leading to subtle rendering differences in font-dependent tests.

### IN-05: `scene_graph set` test doesn't test `scaleY` independently from `scaleX`

**File:** `tests/scene_graph_test.cpp:66-77`
**Issue:** The `SetTransformParsesCorrectly` test sets `scale(1.5)` and only verifies `scaleX` (line 74). It should also verify `scaleY` is set to the same value (since `scale()` sets both).

### IN-06: `ROOT` path calculation in harness.lua may double-slash

**File:** `tests/compile/harness.lua:16`
**Issue:** `ROOT = HERE .. "../../"` where `HERE = p:match("(.*/)")`. If `HERE` ends with `/`, this produces paths like `./../.../../../`. While Lua path resolution handles this fine, it's fragile. Use `HERE = p:match("(.*[/\\])")` for Windows compatibility too.

---

## Summary of Coverage Gaps by Module

| Module | Test File | Coverage Assessment |
|---|---|---|
| ConfigManager | config_test.cpp | Covers happy path only. Error paths, type mismatches, unregistered keys: 0% |
| SerializableSaveData | save_data_test.cpp | Good round-trip for string vars, characters, callstack. **numVariables: 0%**, invalid JSON: 0% |
| Script VM | cereka_script_test.cpp | **Only if/else/endif skip logic tested.** All 30+ other ops: 0% |
| SceneGraph | scene_graph_test.cpp | Good node CRUD coverage. **Parent-child hierarchy: 0%**, transform inheritance: 0% |
| Display/Render | display_test.cpp | SDL3 surface ops only. **Actual Cereka rendering: 0%** |
| AudioManager | audio_manager_test.cpp | Tests duplicated curve math. **AudioManager class methods: 0%** |
| MarkupParser | markup_parser_test.cpp | **Best coverage in the suite.** Missing combo styles and mixed escape+tags |
| RollbackManager | rollback_manager_test.cpp | **Misleading tests that don't test what they claim.** restore/goTo/wrapping: 0% |
| Compile Snapshots | compile/ (14 inputs) | Good op coverage. **Error paths: 0%**, edge cases: 0% |
| Build System | CMakeLists.txt | CTest disabled, GLOB_RECURSE fragility |

---

_Reviewed: 2026-05-09T12:00:00Z_
_Reviewer: gsd-code-reviewer (deep)_
_Depth: deep_
