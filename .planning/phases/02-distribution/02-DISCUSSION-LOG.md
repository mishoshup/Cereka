# Phase 02: Distribution - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-07
**Phase:** 02-distribution
**Areas discussed:** VN to Cereka rename, VM to Cereka rename, Namespace strategy, File naming patterns, Public API naming, Build scripts, Documentation branding

---

## VN to Cereka Rename

| Option | Description | Selected |
|--------|-------------|----------|
| ICerekaState / CerekaState | Full brand prefix — consistent with CerekaStateMachine | ✓ |
| IState / State | Minimal — drop VN without adding Cereka | |
| IStateNode / StateNode | Descriptive alternative — Node signals composability | |

**User's choice:** ICerekaState / CerekaState
**Notes:** Strong preference for full brand prefix.

| Option | Description | Selected |
|--------|-------------|----------|
| ICerekaStateContext | Consistent with ICerekaState — paired naming | ✓ |
| ICerekaEngineContext | More explicit — states communicate with engine | |
| IStateHost | Architecture-focused jargon | |

**User's choice:** ICerekaStateContext (agent recommended)
**Notes:** Agent recommended as strongest enterprise choice — pairs naturally with ICerekaState, GoF Context pattern.

| Option | Description | Selected |
|--------|-------------|----------|
| cereka_instruction + CompileScript | Clean brand prefix | ✓ |
| Keep vn_ but rename function | Less disruptive | |
| No prefixes | Simplest | |

**User's choice:** Full Cereka prefix like SDL naming convention
**Notes:** User explicitly referenced SDL naming (SDL_ttf, SDL_image) as the model. Files become cereka_instruction.hpp/cpp.

| Option | Description | Selected |
|--------|-------------|----------|
| Full path rename | All includes update immediately | ✓ |
| Forwarding shims | Keep old headers for compatibility | |
| Content-only rename | Leave file names as-is | |

**User's choice:** Full path rename

---

## VM to Cereka Rename

| Option | Description | Selected |
|--------|-------------|----------|
| CerekaScript | script_vm.cpp → cereka_script.cpp — describes instruction execution | ✓ |
| CerekaRuntime | More abstract — sounds like full execution environment | |
| CerekaVM | Keep VM but brand it | |

**User's choice:** CerekaScript (agent recommended)

| Option | Description | Selected |
|--------|-------------|----------|
| Keep Tick() | Short, clear, game engine convention | |
| CerekaScriptTick() | Explicit about what's being ticked | ✓ |
| CerekaTick() | Maximum brand signal | |

**User's choice:** CerekaScriptTick() (agent recommended)

| Option | Description | Selected |
|--------|-------------|----------|
| LoadCerekaScript / LoadCompiledCerekaScript | Consistent with CerekaScript naming | ✓ |
| LoadProgram / LoadCompiledProgram | Program as game dev term | |
| Keep as-is | Already clear enough | |

**User's choice:** LoadCerekaScript / LoadCompiledCerekaScript

| Option | Description | Selected |
|--------|-------------|----------|
| cereka_script_test.cpp | Consistent with source file rename | ✓ |
| cereka_runtime_test.cpp | Also good | |
| Keep vm_test.cpp | Low visibility | |

**User's choice:** cereka_script_test.cpp

---

## Namespace Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| cereka::config | All namespaces prefixed | ✓ |
| Keep as config | Only internal, not in public API | |

**User's choice:** cereka::config (agent recommended)

| Option | Description | Selected |
|--------|-------------|----------|
| cereka::compiler | Communicates exact purpose | ✓ |
| cereka::script | Alternative emphasis | |

**User's choice:** cereka::compiler (agent recommended)

---

## File Naming Convention

| Option | Description | Selected |
|--------|-------------|----------|
| Full cereka_ prefix | SDL-style — all files get cereka_ prefix | ✓ |
| Keep directory-based | Sub-namespace dirs as organizing principle | |
| Hybrid | Dirs + prefix inside | |

**User's choice:** Full cereka_ prefix (agent recommended)

---

## Build Scripts

| Option | Description | Selected |
|--------|-------------|----------|
| Full build script rename | compiler.lua, verify scripts, CI all get Cereka | |
| Only rename compiler.lua | Compiler is product; scripts are tooling | ✓ |
| Keep as-is | Build scripts are internal | |

**User's choice:** Only rename compiler.lua (agent recommended)
**Notes:** Agent argued SDL doesn't prefix build scripts — brand prefix is for product, not tooling.

---

## Documentation Branding

| Option | Description | Selected |
|--------|-------------|----------|
| Full docs rebrand | CLAUDE.md, README, code comments | ✓ |
| Public docs only | CLAUDE.md + README only | |
| Code only | No docs changes | |

**User's choice:** Full docs rebrand
**Notes:** User explicitly stated: no Ren'Py mentions. Cereka is a competitor, not an also-ran. Don't mention competitors — they mention us.

---

## Agent's Discretion

- **Build scripts (verify-linux.sh, verify-windows.sh, make-appimage.sh, CI):** User deferred to agent recommendation to keep names as-is. These are tooling, not product.
- **Public API (InMenu, CurrentText, ButtonCount, ProgramCounter):** User deferred to agent recommendation to keep as-is.
- **CerekaScriptTick() vs alternatives for TickScript():** User deferred to agent recommendation.

## Deferred Ideas

None — discussion stayed within branding scope.
