# Phase 4: Engine Features - Context

**Gathered:** 2026-05-07
**Status:** Ready for planning

<domain>
## Phase Boundary

Feature parity with Ren'Py baseline. Four workstreams in order:
1. Scene graph + transform tree (prerequisite for ATL: dissolve/zoom/rotate)
2. Text markup (`<b>`, color spans, custom tags)
3. Audio fade in/out + crossfade
4. Rollback + dialogue history

**Overarching motif:** highly customizable games via .crka, ease of use for game authors, enterprise-grade maintainability, and highly moddable games produced. Rivals Ren'Py in feature richness, Unity/Unreal in architecture quality.

</domain>

<decisions>
## Implementation Decisions

### D-01: Scene Graph — Lightweight Transform Tree
- **Lightweight tree of visual nodes** with hierarchical transforms (parent/child for coordinated animations)
- Each node has position, opacity, scale, rotation at launch
- UIManager owns the scene graph — extends its visual tree
- **Imperative node API** from .crka: `scene_graph my_node set pos(50%,80%) scale(1.2) rotate(15)`
- **Sensible defaults**: center position, 100% scale, 0° rotate, full opacity
- Author only specifies what they want to change
- Feature-rich and customizable, but works out of the box with defaults

### D-02: Text Markup — Angle Brackets
- **`<tag>` syntax** (not `{tag}` which conflicts with `{var}` variable substitution)
- Full tag set at launch: bold, italic, underline, strikethrough, color, size, font, alpha, spacing
- **Custom tag API**: .crka devs can define `<mytag>...</mytag>` handlers via script
- Parser integrated into existing .crka compiler's string literal handling
- `<` and `>` in literal text need escaping (`<<` / `>>`)

### D-03: Audio Fade — Timer-Based Volume Ramping
- **Timer-based volume ramping** in the existing Update() loop — increment/decrement per frame over a configured duration
- Full control over fade curve (linear, ease-in, ease-out, custom)
- Crossfade = simultaneous fade-out of current BGM + fade-in of new BGM
- No dependency on SDL3_mixer-specific fade features
- .crka API: `bgm "file.ogg" fade(2.0)`, `stop_bgm fade(1.5)`, `bgm "next.ogg" crossfade(1.0)`

### D-04: Rollback — Configurable State Snapshots
- **Atomic state snapshots** of full CerekaStateMachine state + variables
- Default cap: 200 steps, overridable in game.cfg:
  ```crka
  rollback_snapshots = 200
  rollback_lock = false
  ```
- **Configurable per-game**: author sets snapshot frequency in game.cfg
- **Player-adjustable when unlocked**: modders/players can edit game.cfg
- **Lockable for design**: `rollback_lock = true` — launcher bakes value into binary at packaging time, cheat-proof
- Triggers: every dialogue advance + every menu choice (author can tune)
- Snapshot restored instantly — no replay/flicker, best UX

### D-05: Moddability — Source-Accessible, Configurable at Packaging
- **Scripts ship as .crka source**, compiled at game startup — same model as Ren'Py/DDLC
- Launcher packaging step offers toggle:
  - Ship .crka source (compile at startup — modder-friendly)
  - Pre-compile scripts (faster startup, source protected)
  - Include .crka alongside compiled (best of both)
- Game directory exposes: assets (.png, .ogg), game.cfg, scripts
- No dedicated mod loader at launch — deferred until community demands it

### Build Order
1. Scene graph (foundation for all visual effects)
2. Text markup (parallel track)
3. Audio fade (small, can slot anywhere)
4. Rollback (most complex, depends on stable everything)

### the agent's Discretion
- Exact transform hierarchy design (tree depth, node types)
- Text markup parser implementation details
- Fade curve math (linear vs ease — decide based on visual quality)
- Rollback snapshot serialization format
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Scope
- `.planning/ROADMAP.md` §Phase 4 — scope anchor and goals
- `.planning/phases/03-architectural-cleanup/03-CONTEXT.md` §Deferred — Phase 4 items listed

### Engine Core
- `src/ui/ui_manager.hpp` — UIManager (scene graph extends this)
- `src/state/cereka_state.hpp` — CerekaStateMachine (rollback snapshots)
- `src/cereka_audio_manager.hpp` — AudioManager (add fade to)
- `src/cereka_audio_manager.cpp` — existing PlayBGM/StopBGM/PlaySFX
- `src/cereka_script.cpp` — dispatch loop (text markup integration in rendering)
- `src/cereka_save.cpp` — save/load patterns (rollback snapshot persistence)
- `src/cereka_save_data.hpp` — SerializableSaveData (extend for rollback)

### .crka Compiler
- `scripts/cereka_compiler.lua` — tokenizer (text markup parsing)
- `src/compiler/cereka_instruction.hpp` — Op enum, Instruction struct

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **UIManager** (`src/ui/ui_manager.hpp/cpp`) — owns all rendering via IRenderContext. Scene graph nodes will be drawn through UIManager's existing draw pipeline.
- **CerekaStateMachine** (`src/state/cereka_state.hpp`) — overlay stack, state transitions. Rollback snapshots capture machine state.
- **AudioManager** (`src/cereka_audio_manager.hpp/cpp`) — has PlayBGM/StopBGM/PlaySFX with Mix_Music/Mix_Chunk. Fade ramping in Update().

### Established Patterns
- **pImpl**: CerekaEngine → CerekaImpl. New subsystems follow this.
- **CRTP State Machine**: CerekaStateBase<T>. All states follow this.
- **Property Map**: ConfigManager maps string keys to typed properties.
- **Cereka prefix**: All source files use `cereka_` prefix.
- **UIManager pattern**: standalone Init/Shutdown, no SDL types in public surface.

### Integration Points
- **Scene graph + UIManager**: UIManager::Draw*() methods need to traverse scene graph nodes instead of drawing directly. Scene graph is owned by UIManager.
- **Text markup + render**: Text rendering pipeline (currently simple string→texture) needs markup parsing → segmented rendering.
- **Audio fade + Update()**: Fade timer ticks in per-frame Update, modifies volume each frame.
- **Rollback + state machine**: On rollback trigger, snapshot state. On rollback activate, restore full state machine + variables.
- **Rollback + save system**: Existing Glaze JSON save/load is the pattern for snapshot serialization.
</code_context>

<specifics>
## Specific Ideas

- Foundation of the greatest VN engine in the world — scene graph, markup, audio, and rollback must be clean enough to support future features without refactoring
- Games produced should be highly moddable á la Ren'Py/DDLC
- "Highly customizable game using .crka" is the north star
- "Enterprise-grade rivaling Unity, Unreal, Ren'Py" — every subsystem must match industry standard quality

</specifics>

<deferred>
## Deferred Ideas

- Dedicated mod loader system with `mods/` directory and override priority — defer until community demands it
- Particle effects for scene graph — defer to future phase
- Movie/video playback — not in scope for Phase 4

</deferred>

---

*Phase: 4-Engine Features*
*Context gathered: 2026-05-07*
