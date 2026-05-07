# Phase 3: Architectural Cleanup - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-07
**Phase:** 3-architectural-cleanup
**Areas discussed:** CI failures, State machine unification, UIManager boundary, Renderer abstraction, Crash safety

---

## CI Fix (Prerequisite — user-initiated)

| Option | Description | Selected |
|--------|-------------|----------|
| Fix CI as first Phase 3 task | Install Qt6 on macOS runners, add libxtst-dev to Linux deps, wrap launcher guard for cross-compile | ✓ |
| Defer — fix separately | Phase 3 focuses on architectural items, CI is separate concern | |

**User's choice:** Fix CI as first Phase 3 task
**Notes:** User reported "CI of previous phases failed after I pushed." Three failures confirmed: Linux (XTEST missing), macOS (Qt6 not installed), Windows cross-compile (Qt6 find_package).

---

## State Machine Unification

| Option | Description | Selected |
|--------|-------------|----------|
| Replace reads, keep field | Replace all state reads with m_stateMachine.currentType() but keep Impl::state for save serialization | |
| Full removal of Impl::state | Remove Impl::state entirely. Save/load serializes via machine. | |
| Move dispatch into DialogueState | Move CerekaScriptTick() into DialogueState::update(). Full state encapsulation — Unity/Unreal pattern. | ✓ |

**User's choice:** Full state encapsulation (enterprise-grade, cleanest, scalable, maintainable)
**Notes:** User explicitly wants rival-Unity/Unreal/Ren'Py quality. Selected option 3 as the enterprise-grade standard.

---

## UIManager Boundary

| Option | Description | Selected |
|--------|-------------|----------|
| All draw + layout + config | UIManager owns every draw call, layout calculation, and config/theme system | ✓ |
| Dialogue box + menus only | UIManager owns dialogue box rendering, menu buttons, and theme config only | |
| Layout + config, not raw draw | UIManager owns layout calculation and theme config, raw SDL stays elsewhere | |

**User's choice:** Full scope — UIManager owns all visuals
**Notes:** User wants "cleanest scalable maintainable enterprise grade foundation of the greatest VN engine in the world."

---

## Renderer Abstraction

| Option | Description | Selected |
|--------|-------------|----------|
| Draw operations only | IRenderer wraps SDL draw calls only. Window creation stays in video.cpp | |
| Draw + window context | IRenderContext extends IRenderer with dimensions, texture factory, render target management | ✓ |

**User's choice:** Full IRenderContext (enterprise-grade)
**Notes:** UIManager depends on IRenderContext, partners with it. SDL fully behind interface.

---

## Crash Safety

| Option | Description | Selected |
|--------|-------------|----------|
| Scope-listed only + try/catch | Fix the 3 ROADMAP-listed risks with local guards | |
| All 6 risks + proper error propagation | Fix all 6 with std::expected/bounds-check/uniform pattern | ✓ |

**User's choice:** All 6 risks, uniform error propagation (enterprise-grade)

---

## Agent's Discretion

None — all areas discussed with user and explicit decisions captured.

## Deferred Ideas

- Scene graph + transform tree — Phase 4
- Audio fade in/out — Phase 4
- Text markup — Phase 4
