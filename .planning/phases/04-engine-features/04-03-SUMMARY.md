---
phase: 04-engine-features
plan: 03
type: execute
wave: 2
completed: 2026-05-07
subsystem: audio-fade
tags: [audio, fade, crossfade, compiler, vm-dispatch]
key-files:
  created:
    - tests/audio_manager_test.cpp
    - tests/compile/inputs/audio_fade.crka
  modified:
    - src/cereka_audio_manager.hpp
    - src/cereka_audio_manager.cpp
    - src/compiler/cereka_instruction.hpp
    - scripts/cereka_compiler.lua
    - src/state/cereka_states.cpp
    - src/cereka_script.cpp
    - tests/CMakeLists.txt
metrics:
  cpp_tests: 8 new (52 total pass)
  snapshot_tests: 1 new (11 total pass)
  test_duration_sec: 0
  build_status: pass
---

## Summary

Timer-based audio fade/crossfade system implemented: FadeCurve enum (Linear, EaseIn, EaseOut, EaseInOut), BgmFade state machine with Update() volume ramping, PlayBGM/StopBGM overloads with fadeDuration, CrossfadeBGM dual-track management, compiler `fade(2.0)`/`crossfade(1.0)` modifiers, VM dispatch for all three new ops, and per-frame AudioManager::Update call in Impl::Update().

### Tasks

| # | Commit | Description |
|---|--------|-------------|
| 1 | 0464937 | AudioManager fade fields, Update(), PlayBGM/StopBGM overloads, CrossfadeBGM |
| 2 | 7969832 | Compiler fade/crossfade ops, VM dispatch, engine Update hook |

### Test Results

```
52 tests from 6 test suites ran. (0 ms total)
[  PASSED  ] 52 tests.
11 passed, 0 failed (compile snapshots)
```

### Deviations from Plan

None — plan executed exactly as written.

## Self-Check: PASSED
