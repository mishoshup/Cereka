# Phase 02 Context — Engine Correctness (Enterprise-Grade)

## 1. Save System: Clean Break (v0.0.3 Alpha Status)
- **Decision:** Perform a "clean break." The existing manual `.sav` format is discarded.
- **Project Status:** As the project is currently v0.0.3 (pre-SLC), long-term migration hooks are deferred to avoid premature over-engineering. However, the new Glaze-based JSON schema MUST include a `version` field to ensure the engine is "migration-ready" for future phases.
- **Enterprise Rationale:** Prioritizes velocity while maintaining a data-integrity foundation.

## 2. Text Rendering: Themeable Wrapped Layout
- **Decision:** Implement `TTF_RenderText_Blended_Wrapped` with layout properties exposed to the script.
- **Implementation:**
    - Add `wrap_width` and `line_spacing` to the `ui textbox` config.
    - Default `wrap_width` to 90% of the textbox width.
    - The `TextRenderer` will handle layout calculation, removing rendering logic from `draw.cpp`.
- **Enterprise Rationale:** Decouples UI configuration from engine rendering code.

## 3. State Machine: "Big Bang" Decoupling
- **Decision:** Full migration to `CerekaStateMachine`.
- **Implementation:**
    - **Logic Delegation:** `CerekaImpl` delegates update/event loops to `m_stateMachine`.
    - **Data Ownership:** Transient state (timers, indices) moves into `VNState` classes.
    - **Observability:** State transitions must be traceable/loggable for debugging.
- **Enterprise Rationale:** Eliminates the "God Class" pattern to ensure long-term maintainability and support for advanced features like Rollback.

## 4. Automated Verification: No-Broken-Shit Infrastructure
- **Decision:** Establish a multi-tier verification pipeline to enforce distribution integrity.
- **Implementation:**
    - **GitHub Actions:** Linux/macOS/Windows builds + unit tests on every push.
    - **Windows Verification:** Headless `wine` used in CI to execute cross-compiled unit tests.
    - **Local Clean-Room Scripts:** 
        - `scripts/verify-linux.sh` (Docker-based build/test).
        - `scripts/verify-windows.sh` (llvm-mingw + wine test runner).
- **Enterprise Rationale:** Shifts the burden of correctness from developer memory to managed infrastructure.

---

## 5. Branding & Naming Conventions

**Decision:** Full codebase branding to Cereka — SDL-style naming convention (`cereka_` prefix on all source files, Cereka prefix on all types/functions). No competitor references in docs.

### VN → Cereka (State Machine & Compiler)

| Current | New | Rationale |
|---------|-----|-----------|
| `IVNState` | `ICerekaState` | Drop VN (Visual Novel) tag. Consistent with CerekaStateMachine |
| `VNState<T>` | `CerekaState<T>` | Same — brand prefix on CRTP base |
| `IVNStateContext` | `ICerekaStateContext` | Pairs with ICerekaState. GoF Context pattern, instantly recognizable |
| `vn_instruction.hpp` | `cereka_instruction.hpp` | SDL-style file naming |
| `vn_instruction.cpp` | `cereka_instruction.cpp` | SDL-style file naming |
| `CompileVNScript()` | `CompileCerekaScript()` | Brand prefix on entry point |
| `cereka::scenario` | `cereka::compiler` | "scenario" is vague; "compiler" is exact |

No forwarding shims — full path rename across the codebase.

### VM → Cereka Script (Runtime)

| Current | New | Rationale |
|---------|-----|-----------|
| `script_vm.cpp` | `cereka_script.cpp` | "Script" describes execution of compiled .crka instructions |
| `tests/vm_test.cpp` | `tests/cereka_script_test.cpp` | Matches source file rename |
| `TickScript()` | `CerekaScriptTick()` | Full brand prefix + descriptive verb |
| `LoadScript()` | `LoadCerekaScript()` | Brand prefix on public API |
| `LoadCompiledScript()` | `LoadCompiledCerekaScript()` | Brand prefix on public API |

### Namespaces

- bare `config` → `cereka::config` (includes `config::parsers`, `config::serializers`, `config::handlers`)
- All engine namespaces under `cereka::`

### File Naming (SDL-style)

All source and header files get `cereka_` prefix:

```
draw.cpp           → cereka_draw.cpp
save.cpp           → cereka_save.cpp
audio_manager.cpp  → cereka_audio_manager.cpp
scene_manager.cpp  → cereka_scene_manager.cpp
dialogue_system.*  → cereka_dialogue_system.*
menu_system.*      → cereka_menu_system.*
text_renderer.*    → cereka_text_renderer.*
video.*            → cereka_video.*
ui_config.*        → cereka_ui_config.*
engine_impl.hpp    → cereka_engine_impl.hpp
save_data.hpp      → cereka_save_data.hpp
```

Public API header stays `include/Cereka/Cereka.hpp`.

### Public API Surface

- `CerekaEngine` — keep (already branded)
- `CerekaEvent` — keep
- `CerekaState` — keep
- `InMenu()`, `CurrentText()`, `ButtonCount()`, `ProgramCounter()` — keep as-is

### Build Scripts

- `scripts/compiler.lua` → `scripts/cereka_compiler.lua` (the Lua compiler is part of the product)
- Verify scripts (`verify-linux.sh`, `verify-windows.sh`, `make-appimage.sh`) and CI files keep names — tooling, not product

### Documentation Branding

- No competitor mentions (Ren'Py, Unity, Unreal) in docs or code comments
- "Visual Novel" replaced with "Cereka game" or "Cereka" in all docs and comments
- `CLAUDE.md`, README, and all code comments rebranded
- Brand positioning: Cereka is in a category of its own — not "a Ren'Py rival" or "a visual novel engine"
