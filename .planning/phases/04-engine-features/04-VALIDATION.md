---
phase: 4
slug: engine-features
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-07
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest v1.14.0 (FetchContent) + Lua snapshot harness |
| **Config file** | CMakeLists.txt in tests/ |
| **Quick run command** | `ninja -C build cereka_test && ./build/tests/cereka_test` |
| **Full suite command** | `ninja -C build cereka_test && ./build/tests/cereka_test && lua tests/compile/harness.lua` |
| **Estimated runtime** | ~2 minutes (build + unit tests) |

---

## Sampling Rate

- **After every task commit:** `ninja -C build cereka_test && ./build/tests/cereka_test --gtest_filter=<test>`
- **After every wave merge:** `ninja -C build cereka_test && ./build/tests/cereka_test && lua tests/compile/harness.lua`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** ~2 minutes

---

## Per-Task Verification Map

| Req ID | Behavior | Test Type | Automated Command | File Exists | Status |
|--------|----------|-----------|-------------------|-------------|--------|
| SG-01 | SceneNode tree add/remove/find | unit | `cereka_test --gtest_filter=SceneGraphTest.*` | ❌ Wave 0 | ⬜ pending |
| SG-01 | Transform accumulation (parent+child world) | unit | `cereka_test --gtest_filter=SceneGraphTest.*` | ❌ Wave 0 | ⬜ pending |
| SG-02 | scene_graph compiler output matches expected ops | snapshot | `lua tests/compile/harness.lua` (add scene_graph.crka) | ❌ Wave 0 | ⬜ pending |
| TM-01 | Markup parser produces correct segments | unit | `cereka_test --gtest_filter=MarkupParserTest.*` | ❌ Wave 0 | ⬜ pending |
| TM-01 | Nested tags produce correct cumulative styles | unit | `cereka_test --gtest_filter=MarkupParserTest.*` | ❌ Wave 0 | ⬜ pending |
| TM-01 | Unclosed tags produce error gracefully | unit | `cereka_test --gtest_filter=MarkupParserTest.*` | ❌ Wave 0 | ⬜ pending |
| AF-01 | bgm fade / crossfade compiled ops | snapshot | `lua tests/compile/harness.lua` (add audio_fade.crka) | ❌ Wave 0 | ⬜ pending |
| AF-02 | FadeUpdate ramps volume correctly over duration | unit | `cereka_test --gtest_filter=AudioManagerTest.*` | ❌ Wave 0 | ⬜ pending |
| AF-02 | Crossfade: fade-out + fade-in complete in expected time | unit | `cereka_test --gtest_filter=AudioManagerTest.*` | ❌ Wave 0 | ⬜ pending |
| RB-01 | Rollback snapshot captures and restores state | unit | `cereka_test --gtest_filter=RollbackManagerTest.*` | ❌ Wave 0 | ⬜ pending |
| RB-02 | Snapshot ring buffer wraps at capacity | unit | `cereka_test --gtest_filter=RollbackManagerTest.*` | ❌ Wave 0 | ⬜ pending |
| RB-03 | History state push/pop from state machine | unit | `cereka_test --gtest_filter=StateMachineTest.*` | ❌ Wave 0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

All test files are new — no existing test files cover Phase 4 features.

### Wave 0 Gaps

- [ ] `tests/scene_graph_test.cpp` — covers SG-01 (SceneNode tree, transforms)
- [ ] `tests/markup_parser_test.cpp` — covers TM-01 (segment parsing, nesting, error handling)
- [ ] `tests/audio_manager_test.cpp` — covers AF-02 (fade timing curve)
- [ ] `tests/rollback_manager_test.cpp` — covers RB-01/RB-02 (snapshot, restore, capacity)
- [ ] `tests/compile/inputs/scene_graph.crka` + `expected/scene_graph.txt` — covers SG-02
- [ ] `tests/compile/inputs/audio_fade.crka` + `expected/audio_fade.txt` — covers AF-01
- [ ] `tests/compile/inputs/text_markup.crka` + `expected/text_markup.txt` — covers TM-01 compiler output

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 2 minutes
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
