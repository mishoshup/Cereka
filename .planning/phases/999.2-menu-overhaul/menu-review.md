# Menu Overhaul — Quality Review

## Checklist

### Architecture
- [x] **God class growth** — `cereka_engine_impl.hpp` gained zero new fields. Interaction state lives in `MenuSystem`.
- [x] **Dead code** — All new code is wired end-to-end: event → MenuState → MenuSystem → DrawMenuButtons.
- [x] **Parallel state** — No duplicated state. `MenuSystem::hoveredIndex_` / `selectedIndex_` / `currentPage_` are single-source.
- [x] **Public surface leak** — `CerekaEvent::MouseMove` is the only public API change. No SDL types leaked.

### Code quality
- [x] **Buzzword headers** — None introduced. Existing "Enterprise patterns" comment in `state.hpp` is pre-existing.
- [x] **Obvious comments** — All comments explain *why* (e.g., "Mouse movement transfers selection to hover position"). No *what* comments.
- [x] **Premature abstraction** — No new abstractions. Direct implementation, no factories or CRTP abuse.
- [x] **Speculative error handling** — Minimal: bounds checks on selectedIndex, empty-button-list guards. All justified.
- [x] **`std::cerr` debug spam** — None added.

### Engine-specific
- [x] **No compiler changes** — No instructions or ops modified. No save format changes.
- [x] **No new SDL types in public surface** — All rendering stays behind `IRenderContext`.
- [x] **Existing patterns followed** — DrawMenuButtons texture-creation-per-frame matches existing codebase pattern.

### Tests
- [x] **67/67 C++ tests pass** — No regressions.
- [x] **14/14 snapshot tests pass** — No regressions.
- [ ] **New property coverage** — `button.y` and `button.spacing` are registered via PROPERTY_TABLE but not explicitly tested. Follows existing pattern (same pattern as all other button/namebox/textbox props). [config_test.cpp could be extended, but not a regression.]

### Process
- [x] **No `--no-verify`** — Commit hooks run normally.
- [x] **No mass `git add`** — Only changed files staged individually.

## Findings

### 1. Scrollback of ButtonCount (minor)
When the total number of buttons changes between menu invocations, `totalPages_` is recalculated on the next `Open()`. This is correct behavior — nothing leaked between menu instances.

### 2. Text texture churn (pre-existing)
`DrawMenuButtons` creates a new `ITexture` for every button label on every frame via `CreateTextTexture`. This is the same pattern used throughout the codebase (`DrawDialogueBox`, `DrawSaveLoadOverlay`, etc.). Not introduced by this change. A future optimization pass could cache text textures.

### 3. HoverImage loading (pre-existing)
`button.hover_image` was already a registered property and `hoverImage`/`hoverImagePath` were already allocated. This change simply **wires** them into rendering — no regression.

---

**Passes Cereka quality bar.** Changes are contained, follow existing patterns, introduce no architectural debt, and all tests pass.
