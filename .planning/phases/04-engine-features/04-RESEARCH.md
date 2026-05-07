# Phase 4: Engine Features — Research

**Researched:** 2026-05-07
**Domain:** Visual novel engine features (scene graph, text markup, audio fading, rollback)
**Confidence:** HIGH

## Summary

Phase 4 delivers four feature workstreams that bring Cereka to feature parity with Ren'Py's baseline: scene graph with transform tree, text markup rendering, audio fade/crossfade, and rollback with dialogue history. Each workstream builds directly on the architectural foundation laid in Phase 3 (UIManager, IRenderContext, CerekaStateMachine, ConfigManager) and extends the compiler, engine subsystems, and rendering pipeline.

**Primary recommendation:** Execute in the build order specified in CONTEXT.md — scene graph first (all subsequent visual features depend on it), text markup parallel, audio fade slotted anywhere, rollback last (depends on stable everything). Each workstream adds new `Op` enum entries, compiler lowerer handlers, and VM dispatch cases. Do not reuse existing ops — create distinct ops for fade-vs-instant variants.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Scene graph transform tree | UIManager | SceneManager | UIManager owns all rendering; scene graph nodes are visual elements drawn per-frame. SceneManager's existing texture management (bg/characters) can be a source for node textures. |
| Scene graph .crka API | Compiler | CerekaImpl | Compiler parses `scene_graph <id> <prop>(<val>)` lines. CerekaImpl/ScriptInterpreter dispatches the ops to UIManager's graph. |
| Text markup parsing | Compiler | (none) | Parser operates on .crka string literals during compilation, producing Instruction arrays with markup segments. |
| Text markup rendering | IRenderContext | UIManager | IRenderContext gets rich text drawing methods. UIManager calls them in DrawDialogueBox. |
| Audio fade ramping | AudioManager | CerekaImpl::Update | AudioManager holds fade state and timer; Impl::Update() calls AudioManager::Update(dt) each frame for volume ramping. |
| Audio fade .crka API | Compiler | AudioManager | New ops PLAY_BGM_FADE, STOP_BGM_FADE, BGM_CROSSFADE. AudioManager::PlayBGM/StopBGM get fade param variants. |
| Rollback snapshots | CerekaImpl | RollbackManager | RollbackManager owns the circular snapshot buffer. Snapshots serialize all engine state (same data as save). |
| Rollback restore | CerekaImpl | CerekaStateMachine | Restoring a snapshot is like LoadGame but from memory. State machine is reset to the snapshotted state. |
| Dialogue history UI | UIManager | CerekaStateMachine | New HistoryState overlay (pushOverlay). UIManager draws the scrollable history list. |

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### D-01: Scene Graph — Lightweight Transform Tree
- Lightweight tree of visual nodes with hierarchical transforms (parent/child for coordinated animations)
- Each node has position, opacity, scale, rotation at launch
- UIManager owns the scene graph — extends its visual tree
- Imperative node API from .crka: `scene_graph my_node set pos(50%,80%) scale(1.2) rotate(15)`
- Sensible defaults: center position, 100% scale, 0° rotate, full opacity
- Author only specifies what they want to change

#### D-02: Text Markup — Angle Brackets
- `<tag>` syntax (not `{tag}` which conflicts with `{var}` variable substitution)
- Full tag set at launch: bold, italic, underline, strikethrough, color, size, font, alpha, spacing
- Custom tag API: .crka devs can define `<mytag>...</mytag>` handlers via script
- Parser integrated into existing .crka compiler's string literal handling
- `<` and `>` in literal text need escaping (`<<` / `>>`)

#### D-03: Audio Fade — Timer-Based Volume Ramping
- Timer-based volume ramping in the existing Update() loop — increment/decrement per frame over a configured duration
- Full control over fade curve (linear, ease-in, ease-out, custom)
- Crossfade = simultaneous fade-out of current BGM + fade-in of new BGM
- No dependency on SDL3_mixer-specific fade features
- .crka API: `bgm "file.ogg" fade(2.0)`, `stop_bgm fade(1.5)`, `bgm "next.ogg" crossfade(1.0)`

#### D-04: Rollback — Configurable State Snapshots
- Atomic state snapshots of full CerekaStateMachine state + variables
- Default cap: 200 steps, overridable in game.cfg
- Configurable per-game: author sets snapshot frequency in game.cfg
- Player-adjustable when unlocked: modders/players can edit game.cfg
- Lockable for design: `rollback_lock = true` — launcher bakes value into binary at packaging time, cheat-proof
- Triggers: every dialogue advance + every menu choice (author can tune)
- Snapshot restored instantly — no replay/flicker, best UX

#### D-05: Moddability — Source-Accessible, Configurable at Packaging
- Scripts ship as .crka source, compiled at game startup — same model as Ren'Py/DDLC
- Launcher packaging step offers toggle: ship source, pre-compile, or both
- Game directory exposes: assets (.png, .ogg), game.cfg, scripts
- No dedicated mod loader at launch — deferred until community demands it

#### Build Order
1. Scene graph (foundation for all visual effects)
2. Text markup (parallel track)
3. Audio fade (small, can slot anywhere)
4. Rollback (most complex, depends on stable everything)

### the agent's Discretion
- Exact transform hierarchy design (tree depth, node types)
- Text markup parser implementation details
- Fade curve math (linear vs ease — decide based on visual quality)
- Rollback snapshot serialization format

### Deferred Ideas (OUT OF SCOPE)
- Dedicated mod loader system with `mods/` directory and override priority
- Particle effects for scene graph
- Movie/video playback
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SG-01 | Scene graph with hierarchical transforms (position, scale, rotation, opacity) | SG architecture: SceneNode tree owned by UIManager, drawn via depth-first traversal |
| SG-02 | Imperative .crka API: scene_graph node set pos(50%,80%) scale(1.2) rotate(15) | New compiler ops + lowerer entries + VM dispatch cases |
| TM-01 | `<tag>` text markup with bold, italic, underline, strikethrough, color, size, alpha, font, spacing | SDL3_ttf TTF_SetFontStyle for bold/italic/underline/strikethrough; TTF_RenderGlyph_Blended for per-segment rendering |
| TM-02 | Custom tag API for .crka devs | Register tag handlers in .crka script; runtime resolves by handler map |
| AF-01 | bgm "file.ogg" fade(2.0), stop_bgm fade(1.5), bgm "next.ogg" crossfade(1.0) | Compiler: parse fade/crossfade modifiers on bgm/stop_bgm. VM: dispatch to new ops. |
| AF-02 | Timer-based volume ramping in Update() with configurable fade curves | AudioManager::Update(dt) ramps MIX_SetTrackGain() over duration. Crossfade uses two simultaneous tracks. |
| RB-01 | Configurable rollback snapshots (200 default, per-game tuning, lockable) | RollbackManager with circular buffer of SerializableSaveData. Triggers at dialogue advance + menu choice. |
| RB-02 | Instant rollback restore (no replay) | Restore from in-memory snapshot — direct state copy, no re-execution. |
| RB-03 | Dialogue history UI as scrollable overlay | HistoryState (state machine overlay). UIManager draws scrollable text list. Same patterns as SaveMenuState/LoadMenuState. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++23 | — | Scene graph tree, markup parser, rollback ring buffer | Existing codebase standard. No external dep needed for tree data structures. |
| SDL3_ttf | 3.3.0 | Per-segment rich text rendering (TTF_SetFontStyle, TTF_RenderText_Blended, TTF_MeasureString) | Already vendored. Provides style flags (BOLD/ITALIC/UNDERLINE/STRIKETHROUGH), glyph metrics, measure, and per-glyph rendering for custom rich text layout. |
| SDL3_mixer | 3.1.0 | Track gain ramping (MIX_SetTrackGain) | Already vendored. MIX_SetTrackGain gives per-track volume control for fade ramping. Two simultaneous tracks for crossfade. |
| Glaze JSON | 7.3.3 | Rollback snapshot serialization format | Already vendored. SerializableSaveData already uses Glaze — reuse the same schema/pattern for rollback snapshots. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| sol2 | 4.0.0 | Custom tag handler resolution in Lua | If custom text markup tags call back into Lua runtime for handler dispatch |
| Glaze JSON | 7.3.3 | Rollback on-disk persistence (optional) | If rollback snapshots should persist across sessions (deferred — not in initial scope) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Manual transform tree | ECS (EnTT) | ECS adds complex dependency. Scene graph needs small, hierarchical, visual trees — a manual tree with struct nodes is simpler, more transparent, and sufficient for VN scene graphs (typically < 50 nodes). |
| SDL3_ttf style flags | Per-font-variant approach (load bold.ttf, italic.ttf) | Style flags work with any font (SDL3_ttf applies algorithmic bold/italic). Per-variant gives better typography but requires 4+ font files per typeface. Style flags are sufficient for initial launch. |
| Manual volume ramping | SDL3_mixer MIX_TrackFade | User decision explicitly says no SDL3_mixer fade features. MIX_SetTrackGain for raw volume control + our timer logic. |
| Memory-mapped rollback | File-based snapshots | Memory snapshots give instant restore (no disk I/O). File-based would dominate frame time. Circular buffer of 200 SerializableSaveData structs at ~1KB each = ~200KB, negligible. |

### Installation
No new dependencies — all libraries are already vendored.

### Version verification
[VERIFIED: vendor submodules] SDL3_ttf 3.3.0, SDL3_mixer 3.1.0, glaze 7.3.3, sol2 4.0.0 — all confirmed by vendored submodule versions. No npm registry applies to C++ vendored libraries.

## Architecture Patterns

### System Architecture Diagram

```
.crka Script (.crka source files)
    │
    ▼
┌─────────────────────────────────────────────────────────────┐
│                 cereka_compiler.lua (Lua)                    │
│  Tokenizer → Parser → AST → Lowerer → Instruction[]         │
│  NEW: scene_graph keyword tokens, markup in strings          │
│  NEW: fade/crossfade modifiers on bgm/stop_bgm               │
└─────────────────────┬─────────────────────────────────────────┘
                      │ Instruction[] {op, a, b, c}
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                 ScriptVM / CerekaImpl::Update(dt)            │
│  DialogueState::update() dispatches Instruction switch       │
│  NEW ops: SG_CREATE, SG_SET_*, PLAY_BGM_FADE,               │
│           STOP_BGM_FADE, BGM_CROSSFADE,                      │
│           ROLLBACK_SNAPSHOT, ROLLBACK_RESTORE                │
└──┬──────┬──────┬──────┬──────┬────────────────────────────────┘
   │      │      │      │      │
   ▼      ▼      ▼      ▼      ▼
┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────────┐
│ SG │ │ TM │ │ AF │ │ RB │ │ States │
│    │ │    │ │    │ │    │ │        │
│ UIManager:: │ AudioMgr │ Rollback│ Diaogue  │
│ DrawScene() │ ::Update │ Manager │ State→    │
│ traverses   │ ramps    │ ::snap- │ Waiting→  │
│ SceneGraph  │ MIX_Set  │ shot()  │ Menu→    │
│ tree after  │ TrackGain│ ::rest- │ Fade→     │
│ bg/char     │ per frame│ ore()   │ Finished  │
└────┴────┘ └────┴────┘ └────────┘ └─────────┘
   │                              │
   │                              │
   ▼                              ▼
┌──────────────────────────────────────────┐
│            IRenderContext                │
│  NEW: DrawRichText(Segment[], x, y, wrap)│
│  TTF_SetFontStyle + TTF_RenderText_...   │
│  for each segment, manual line layout    │
└──────────────┬─────────────────────────────┘
               │
               ▼
┌──────────────────────────────────────────┐
│         SdlRenderContext                 │
│  SDL_Texture* per segment, SDL_Render    │
└──────────────────────────────────────────┘
```

### Recommended Project Structure

New files added alongside existing ones (all under `src/`):

```
src/
├── scene_graph.hpp              # SceneNode struct, SceneGraph tree
├── scene_graph.cpp              # SceneGraph tree management (create, remove, set transform)
├── text/                        # New text markup directory
│   ├── markup_parser.hpp        # Parse <tag>text</tag> → vector<TextSegment>
│   ├── markup_parser.cpp        # Implementation
│   ├── rich_text_renderer.hpp   # Render TextSegment[] to IRenderContext
│   └── rich_text_renderer.cpp   # Per-segment SDL_ttf rendering + line layout
├── cereka_rollback_manager.hpp  # RollbackManager: ring buffer, snapshot/restore
├── cereka_rollback_manager.cpp  # Implementation
├── state/
│   ├── cereka_state.hpp         # UNCHANGED (CerekaStateMachine is stable)
│   ├── cereka_states.hpp        # ADD HistoryState declaration
│   └── cereka_states.cpp        # ADD HistoryState implementation (update/draw/handle)
├── renderer/
│   ├── irender_context.hpp      # ADD rich text drawing methods
│   ├── sdl_render_context.cpp   # IMPLEMENT rich text via SDL3_ttf
```

No changes to:
- `include/Cereka/Cereka.hpp` — public API unchanged (rollback is engine-internal)
- `src/renderer/irecture.hpp` — ITexture interface unchanged
- `src/cereka_engine_impl.hpp` — add new member fields only
- `src/cereka_save_data.hpp` — unchanged (reused for snapshots)

### Pattern 1: Scene Graph — Lightweight Transform Tree

**What:** A tree of `SceneNode` objects, each with a local transform (pos, scale, rotation, opacity), an optional texture, and parent/child relationships. UIManager owns the root node and traverses the tree during draw.

**When to use:** Any visual element that needs independent transform control — animated backgrounds, sprite layers, particle-like effects, character positioning.

**Design:**
```cpp
struct SceneNode {
    std::string id;
    struct Transform {
        float x = 0.5f, y = 0.5f;       // normalized 0-1, center by default
        float scaleX = 1.0f, scaleY = 1.0f;
        float rotation = 0.0f;           // degrees
        float opacity = 1.0f;            // 0.0-1.0
    } transform;
    std::shared_ptr<ITexture> texture;
    bool visible = true;
    SceneNode *parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;
};
```

**Traversal (depth-first, in UIManager::DrawSceneGraph):**
```cpp
void drawNode(const SceneNode &node, const Transform &parentAccum) {
    if (!node.visible) return;
    Transform world = accumulate(node.transform, parentAccum);
    if (node.texture) {
        // Draw at world position with world scale/rotation/opacity
        // Use IRenderContext::DrawTexture with computed dst rect
        // Apply alpha via SetBlendMode
    }
    for (auto &child : node.children)
        drawNode(*child, world);
}
```

[ASSUMED] Design is based on standard game engine scene graph patterns (Unity GameObject hierarchy, Ren'Py ATL transforms).

### Pattern 2: Text Markup — Rich Text Rendering Pipeline

**What:** A multi-stage pipeline: (1) Parse `<tag>` syntax into flat `TextSegment[]`, (2) Measure and layout segments into lines with word-wrap, (3) Render each segment with appropriate TTF font style and color.

**When to use:** All text that contains markup — dialogue text, narrator text. Applies after `SubstituteVariables()`.

**Integration point** in `UIManager::DrawDialogueBox`:
```
current: CreateTextTextureWrapped(font, visible, color, wrapWidth)
new:     auto segments = ParseMarkup(visible)
         DrawRichText(segments, x, y, wrapWidth)
```

**Segment structure:**
```cpp
struct TextSegment {
    std::string text;
    struct Style {
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strikethrough = false;
        Color color = {255, 255, 255, 255};
        float size = 0.0f;      // 0 = use current font size
        float alpha = 1.0f;
        // font, spacing — extensible
    } style;
};
```

**Tag parsing (how `<b>` becomes bold: true):**
```
Input:  "Hello <b>world</b>!"
Tokens: [Text("Hello "), OpenTag("b"), Text("world"), CloseTag("b"), Text("!")]
Output: [{text:"Hello ", style:{bold:false}}, {text:"world", style:{bold:true}}, {text:"!", style:{bold:false}}]
```

[ASSUMED] Rich text layout segments → then measure → then render per segment.

### Pattern 3: Audio Fade — Timer-Based Volume Ramping

**What:** Per-frame volume adjustment via `MIX_SetTrackGain()` driven by a timer in `AudioManager::Update(float dt)`.

**When to use:** Any audio transition — fade-in, fade-out, crossfade.

```cpp
// AudioManager additions
enum class FadeState { None, FadingOut, FadingIn, CrossfadeOut, CrossfadeIn };

// State per fade
struct Fade {
    FadeState state = FadeState::None;
    float timer = 0.0f;
    float duration = 0.0f;
    FadeCurve curve = FadeCurve::Linear;
    float startVolume = 1.0f;
    
    // For crossfade (new BGM plays alongside current)
    MIX_Audio *nextAudio = nullptr;
    MIX_Track *nextTrack = nullptr;
    float nextStartVolume = 0.0f;
};

void AudioManager::Update(float dt) {
    if (fade.state == FadeState::FadingOut) {
        fade.timer += dt;
        float t = std::min(fade.timer / fade.duration, 1.0f);
        float gain = applyCurve(1.0f - t, fade.curve);  // 1→0
        MIX_SetTrackGain(bgmTrack, gain);
        if (t >= 1.0f) { stopBGM(); fade = {}; }
    }
    // ... FadingIn, CrossfadeOut, CrossfadeIn
}
```

[VERIFIED: vendor/SDL_mixer/include/SDL3_mixer/SDL_mixer.h line 2159] `MIX_SetTrackGain(MIX_Track *track, float gain)` exists for per-track volume control.

### Pattern 4: Rollback — Ring Buffer of State Snapshots

**What:** A circular buffer (boost::circular_buffer or manual array) serializes the full engine state (same fields as `SerializableSaveData`) at each trigger point. Restore copies the snapshot back.

**When to use:** Every time the script advances (dialogue line, menu choice). Restore when player triggers rollback (scroll wheel, button).

```cpp
class RollbackManager {
    static constexpr size_t DEFAULT_CAPACITY = 200;
    size_t capacity_;
    size_t head_ = 0;     // write position
    size_t count_ = 0;    // how many snapshots stored
    std::vector<SerializableSaveData> buffer_;
    bool enabled_ = true;

public:
    void snapshot(const Impl &impl);   // captures current state
    void restore(Impl &impl);          // restores most recent snapshot
    bool canRollback() const;          // count_ > 0 && enabled_
    std::vector<std::string> historyTexts() const; // dialogue lines for history UI
};
```

**Snapshot trigger locations** (in `DialogueState::update`):
- Before `ctx.changeState(CerekaState::WaitingForInput)` — after SAY/NARRATE
- Before `ctx.changeState(CerekaState::InMenu)` — after MENU

**Restore trigger:** In `WaitingForInputState::handleEvent()`:
- Mouse wheel up or dedicated history button → pushOverlay(HistoryState)
- HistoryState shows history UI, on selection → rollbackManager.restore() + popOverlay()

[ASSUMED] SerializableSaveData has sufficient fields to capture full engine state for rollback. Based on existing save/load round-trip which restores all visible state.

### Anti-Patterns to Avoid
- **Single monolithic render-everything function:** Scene graph must be traversable independently. Do not add scene graph rendering inside existing DrawBackground/DrawCharacters — keep it as a separate pass.
- **Mutable shared font state for rich text:** `TTF_SetFontStyle` mutates the font object. For multi-segment rendering, render each segment to its own surface before advancing to the next, rather than rendering one by one with intermediate style changes to a single font (which would corrupt in-flight rendering).
- **Deep snapshot copies every frame:** Rollback snapshots are at dialogue advance, not per frame. 200 snapshots of ~1KB each = ~200KB total — acceptable memory overhead.
- **Overloading existing ops with fade variants:** Don't add a `b` field to `PLAY_BGM` to signal fade. Create distinct ops (`PLAY_BGM_FADE`, `STOP_BGM_FADE`, `BGM_CROSSFADE`) so the compiler and VM dispatch are explicit.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Text layout/word-wrap | Custom word-wrap algorithm | TTF_MeasureString + TTF_GetGlyphMetrics | SDL3_ttf already measures strings and individual glyphs. Use these to compute line breaks for mixed-format segments. |
| Font style simulation | Algorithmic bold from scratch | TTF_SetFontStyle | SDL3_ttf provides TTF_STYLE_BOLD/ITALIC/UNDERLINE/STRIKETHROUGH. Set these on the font before rendering each segment. |
| Volume control | Software mixing/amplification | MIX_SetTrackGain | SDL3_mixer already applies gain per track. Setting gain factor is one function call. |
| Circular buffer | Custom ring buffer | std::vector + manual head/count | Simple enough that standard library suffices. No need for external circular_buffer. 200-element vector at ~1KB per element = trivial. |
| JSON serialization | Custom serialization | Glaze (already vendored) | SerializableSaveData already has glaze meta. Rollback snapshots reuse the same struct and serialization code. |

**Key insight:** Every feature builds on existing vendored libraries. SDL3_ttf provides all the primitives for rich text (style flags, measurement, per-glyph rendering). SDL3_mixer provides track-level gain control. Glaze provides snapshot serialization. The only truly new code is the scene graph tree, markup parser, and the orchestration logic.

## Common Pitfalls

### Pitfall 1: Scene Graph World Transform Accumulation
**What goes wrong:** Each frame computes world transforms by walking the tree from root to leaf. If parent transform changes mid-frame (e.g., animation), children get stale transforms for that frame.
**Why it happens:** Tree traversal and transform accumulation happen in the same pass.
**How to avoid:** Compute world transforms in a separate update pass before draw. `SceneGraph::updateTransforms()` walks tree, accumulates, caches in each node. Then `draw()` reads cached world transforms.
**Warning signs:** Nodes jittering or lagging one frame behind parent animation.

### Pitfall 2: Rich Text Segment Layout Fails on Nested Tags
**What goes wrong:** `<b>bold <i>bold+italic</i> back to bold</b>` produces wrong segment boundaries or measure widths.
**Why it happens:** Markup parser must stack tags (open `<b>`, open `<i>`, close `</i>`, close `</b>`) — not track individual booleans.
**How to avoid:** Use a style stack during parsing. Each TextSegment stores the current cumulative style. When a tag opens, push style delta; when it closes, pop.
**Warning signs:** Nested tags produce mangled output; `TTF_MeasureString` returns widths that don't match rendered glyphs.

### Pitfall 3: Audio Crossfade Creates Two Simultaneous Tracks, Needs Track Management
**What goes wrong:** Crossfade creates a new track for the incoming BGM but fails to destroy the old track after the fade-out completes. Or old track audio continues playing silently but consuming resources.
**Why it happens:** The AudioManager currently assumes one bgmTrack at a time. Crossfade temporarily requires two.
**How to avoid:** Store `nextTrack` / `nextAudio` alongside current. When crossfade completes (fade-out done → fade-in done), destroy the old track/audio and swap pointers. Ensure `destroyBgmHandles()` handles the dual-track case.
**Warning signs:** After crossfade, memory usage grows; or calling `PlayBGM` during crossfade leaves dangling tracks.

### Pitfall 4: Rollback Snapshot = Deep Copy of Shared Textures
**What goes wrong:** SerializableSaveData stores filenames (not textures). If rollback snapshot copies texture pointers and the textures are later destroyed/reloaded, the snapshot has dangling pointers.
**Why it happens:** RollbackManager captures state at dialogue advance. If a scene transition happens (old bg textures destroyed, new ones loaded), the snapshot references stale textures.
**How to avoid:** SerializableSaveData already stores `std::string` paths for textures, not pointers. Rollback snapshots must use the same pattern — save the *paths*, not the pointers. On restore, reload textures from paths (SceneManager::ShowBackground will recreate them).
**Warning signs:** Rollback causes segfaults in DrawBackground/DrawCharacters after scene transition.

### Pitfall 5: Overlay Stack + Rollback State Machine Interaction
**What goes wrong:** Rollback happens while history overlay is active. Restoring state must properly pop all overlays and set the machine to the restored state.
**Why it happens:** HistoryState is an overlay (pushOverlay). When user selects a history entry to rollback to, the engine needs to popOverlay *and then* change state to the restored state.
**How to avoid:** Rollback restore should call `clearOverlays()` on the state machine first, then `changeState()` — same pattern as `LoadGame()` in `cereka_save.cpp`.
**Warning signs:** After history-rollback, save/load overlay remains on screen; or state machine has stale overlay stack.

## Code Examples

### Scene Graph Node Creation and Drawing

```cpp
// src/scene_graph.hpp (new file)
// [ASSUMED] Based on standard game engine scene graph patterns

struct SceneNode {
    std::string id;
    
    struct Transform {
        float x = 0.5f, y = 0.5f;      // normalized position (0-1)
        float scaleX = 1.0f, scaleY = 1.0f;
        float rotationDeg = 0.0f;       // degrees clockwise
        float opacity = 1.0f;           // 0.0-1.0
    };
    Transform local;
    Transform world;  // accumulated, cached each frame
    
    std::shared_ptr<ITexture> texture;
    bool visible = true;
    
    SceneNode *parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;
};

class SceneGraph {
public:
    SceneNode *createNode(const std::string &id);
    void removeNode(const std::string &id);
    SceneNode *findNode(const std::string &id);
    
    void updateTransforms();  // depth-first: accumulate world from local + parent world
    void visit(const std::function<void(const SceneNode&)>& visitor) const;  // for drawing
    
private:
    std::unique_ptr<SceneNode> root_;     // hidden root (all user nodes attach here)
    std::unordered_map<std::string, SceneNode*> nodeMap_;
};
```

### Rich Text Markup Parsing

```cpp
// src/text/markup_parser.hpp (new file)
// [ASSUMED] Based on standard XML-like tag parsing with style stack

struct TextStyle {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    Color color = {255, 255, 255, 255};
    float size = 0.0f;  // 0 = current font size
    float alpha = 1.0f;
};

struct TextSegment {
    std::string text;
    TextStyle style;
};

// Parse "<b>bold</b> normal <color=#ff0000>red</color>" into segments
// Returns empty vector on parse error
std::vector<TextSegment> ParseMarkup(const std::string &input);
```

### Rich Text Drawing via IRenderContext

```cpp
// ADD to irender_context.hpp:

// Render rich text (segments with per-segment style) at (x, y), wrapped to maxWidth.
// Returns the height of the rendered text block (for multi-line layout calculations).
virtual float DrawRichText(TTF_Font *baseFont,
                           const std::vector<TextSegment> &segments,
                           float x, float y, float maxWidth) = 0;
```

### Audio Manager Volume Ramping

```cpp
// ADD to cereka_audio_manager.hpp:

enum class FadeCurve { Linear, EaseIn, EaseOut, EaseInOut };

struct BgmFade {
    enum class State { None, FadingOut, FadingIn, CrossfadeOut, CrossfadeIn };
    State state = State::None;
    float timer = 0.0f;
    float duration = 0.0f;
    FadeCurve curve = FadeCurve::Linear;
    
    // Crossfade: outgoing track pointer
    MIX_Track *oldTrack = nullptr;
    MIX_Audio *oldAudio = nullptr;
    std::string oldPath;
};

// ADD to AudioManager:
void Update(float dt);
void PlayBGM(const std::string &filename, float fadeDuration = 0.0f);
void StopBGM(float fadeDuration = 0.0f);
void CrossfadeBGM(const std::string &filename, float duration);

// Implementation (added to cereka_audio_manager.cpp):
void AudioManager::Update(float dt) {
    if (fade_.state == BgmFade::State::FadingOut) {
        fade_.timer += dt;
        float t = std::min(fade_.timer / fade_.duration, 1.0f);
        float gain = 1.0f - t;  // linear: 1→0
        MIX_SetTrackGain(bgmTrack, gain);
        if (t >= 1.0f) {
            destroyBgmHandles();  // destroys bgmTrack and bgmAudio
            bgmPath.clear();
            fade_ = {};
        }
    }
    // ... FadingIn, CrossfadeOut, CrossfadeIn
}
```

[VERIFIED: vendor/SDL_mixer/include/SDL3_mixer/SDL_mixer.h line 2159] `MIX_SetTrackGain` exists.

### Rollback Snapshot + Restore

```cpp
// src/cereka_rollback_manager.hpp (new file)
// [ASSUMED] Based on SerializableSaveData pattern from existing save system

class RollbackManager {
public:
    explicit RollbackManager(size_t capacity = 200)
        : buffer_(capacity), capacity_(capacity) {}
    
    void capture(const ScriptInterpreter &si,
                 const SceneManager &scene,
                 const DialogueSystem &dialogue,
                 const AudioManager &audio,
                 CerekaState state,
                 bool skipMode, int skipDepth)
    {
        if (capacity_ == 0) return;
        SerializableSaveData &slot = buffer_[head_];
        slot = {};  // reset to defaults
        slot.programCounter = si.pc;
        slot.callStack = si.callStack;
        slot.variables = si.variables;
        slot.numVariables = si.numVariables;
        slot.background = scene.BgPath();
        // ... capture characters, bgm, state, dialogue ...
        slot.skipMode = skipMode;
        slot.skipDepth = skipDepth;
        head_ = (head_ + 1) % capacity_;
        if (count_ < capacity_) ++count_;
    }
    
    bool restore(Impl &impl) {
        if (count_ == 0) return false;
        size_t idx = (head_ - 1 + capacity_) % capacity_;
        const auto &data = buffer_[idx];
        // Apply snapshot to Impl (same logic as LoadGame but from memory)
        // ...
        return true;
    }
    
private:
    std::vector<SerializableSaveData> buffer_;
    size_t capacity_;
    size_t head_ = 0;
    size_t count_ = 0;
};
```

### Dialogue History State (State Machine Integration)

```cpp
// ADD to cereka_states.hpp:

class HistoryState : public CerekaStateBase<CerekaState::HistoryState> {
public:
    void onEnter(ICerekaStateContext &ctx) override;
    void handleEvent(const CerekaEvent &event,
                     ICerekaStateContext &ctx) override;
    void draw(ICerekaStateContext &ctx) const override;
};

// ADD to CerekaState enum in Cereka.hpp:
// HistoryState

// Implementation:
void HistoryState::handleEvent(const CerekaEvent &event,
                                ICerekaStateContext &ctx) {
    auto &impl = static_cast<Impl &>(ctx);
    if (event.type == CerekaEvent::KeyDown && event.key == SDLK_ESCAPE) {
        ctx.popOverlay();  // return to gameplay
        return;
    }
    if (event.type == CerekaEvent::MouseDown) {
        // Hit-test history entries — if one is selected, rollback to it
        int idx = impl.historyHitTest(event.mouseX, event.mouseY);
        if (idx >= 0 && impl.rollbackManager.goTo(impl, idx)) {
            ctx.popOverlay();  // history overlay gone
            // state machine is now in the restored state
        }
    }
}
```

[ASSUMED] HistoryState follows same pattern as SaveMenuState/LoadMenuState (pushOverlay, draw overlay, popOverlay on ESC/action).

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| bg/characters drawn directly in Draw() | UIManager orchestrates all drawing | Phase 3 | Scene graph extends UIManager's draw pipeline |
| Plain text rendering via CreateTextTexture | Rich text with style segments | Phase 4 | IRenderContext needs new DrawRichText method |
| Audio: instant start/stop | Audio: fade in/out/crossfade | Phase 4 | AudioManager gets Update() and fade state machine |
| Save: 10-slot file-based persistence | Rollback: 200-snapshot in-memory ring buffer | Phase 4 | New RollbackManager, reuses SerializableSaveData |

**Deprecated/outdated:**
- `CreateTextTexture()` and `CreateTextTextureWrapped()` become insufficient for markup text — replace calls with `DrawRichText()` for dialogue/narrator text
- Old `PLAY_BGM`/`STOP_BGM` ops remain for instant audio (no fade) — new fade variants coexist

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Scene graph tree with manual Traverse/UpdateTransforms is sufficient (no ECS needed) | Architecture Patterns | If VN scene graphs grow > 1000 nodes, ECS would be faster. For typical VN use (< 50 nodes), manual tree is fine. |
| A2 | SerializableSaveData fields are sufficient for rollback state capture | Rollback Pattern | Missing fields would cause incomplete state restoration. Can be fixed by adding fields to SerializableSaveData. |
| A3 | HistoryState follows same pushOverlay/popOverlay pattern as SaveMenuState | Code Examples | If HistoryState needs different lifecycle management, the pattern changes. Unlikely — overlays are designed for this. |
| A4 | TTF_SetFontStyle's algorithmic bold/italic produces acceptable visual quality | Rich Text | For fonts without bold/italic variants, algorithmic rendering may look poor. Authors can provide separate font files for best results — not blocked on this. |
| A5 | MIX_CreateTrack allows two simultaneous BGM tracks for crossfade | Audio Fade | If SDL3_mixer limits to one active track, crossfade design changes. Verification needed in implementation. |

## Open Questions

1. **SDL3_mixer: Does crossfade work with two simultaneous tracks on same mixer?**
   - What we know: MIX_CreateTrack allocates a track. The current implementation uses one track for BGM. Crossfade creates a second track while the first is still playing.
   - What's unclear: Will two tracks playing different audio simultaneously cause clipping, contention, or driver-level issues?
   - Recommendation: Prototype dual-track playback early in implementation. If problematic, crossfade can mix the two audios into a single track by pre-mixing at load time.

2. **Rollback + resource lifecycle: BGM path stored in snapshot, but SceneManager bg is also there. What about preloaded-but-not-displayed resources?**
   - What we know: SerializableSaveData captures loaded resources by path (bgm filename, bg path, character files). Textures are recreated on restore.
   - What's unclear: Are there engine-internal cached resources that snapshots miss?
   - Recommendation: Audit `Impl` for any non-serialized state that affects visible output. Add fields to SerializableSaveData as needed.

3. **Text markup custom tag API: How does a .crka author register a handler?**
   - What we know: User wants `define_tag <mytag> ... </mytag>` in .crka scripts.
   - What's unclear: Should custom handlers run Lua code or just map to existing formatting? If Lua, how does the handler get invoked during rendering (which happens in C++ via IRenderContext)?
   - Recommendation: Defer custom tags to a follow-up. Ship with built-in tags only in Phase 4. Custom tags go in Phase 5.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.14.0 (FetchContent) + Lua snapshot harness |
| Config file | CMakeLists.txt in tests/ |
| Quick run command | `ninja -C build cereka_test && ./build/tests/cereka_test` |
| Full suite command | Full: `ninja -C build cereka_test && ./build/tests/cereka_test && lua tests/compile/harness.lua` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SG-01 | SceneNode tree add/remove/find | unit | `cereka_test --gtest_filter=SceneGraphTest.*` | ❌ Wave 0 |
| SG-01 | Transform accumulation (parent+child world) | unit | `cereka_test --gtest_filter=SceneGraphTest.*` | ❌ Wave 0 |
| SG-02 | scene_graph compiler output matches expected ops | snapshot | `lua tests/compile/harness.lua` (add scene_graph.crka) | ❌ Wave 0 |
| TM-01 | Markup parser produces correct segments | unit | `cereka_test --gtest_filter=MarkupParserTest.*` | ❌ Wave 0 |
| TM-01 | Nested tags produce correct cumulative styles | unit | `cereka_test --gtest_filter=MarkupParserTest.*` | ❌ Wave 0 |
| TM-02 | Unclosed tags produce error gracefully | unit | `cereka_test --gtest_filter=MarkupParserTest.*` | ❌ Wave 0 |
| AF-01 | bgm fade / crossfade compiled ops | snapshot | `lua tests/compile/harness.lua` (add audio_fade.crka) | ❌ Wave 0 |
| AF-02 | FadeUpdate ramps volume correctly over duration | unit | `cereka_test --gtest_filter=AudioManagerTest.*` | ❌ Wave 0 |
| AF-02 | Crossfade: fade-out + fade-in complete in expected time | unit | `cereka_test --gtest_filter=AudioManagerTest.*` | ❌ Wave 0 |
| RB-01 | Rollback snapshot captures and restores state | unit | `cereka_test --gtest_filter=RollbackManagerTest.*` | ❌ Wave 0 |
| RB-02 | Snapshot ring buffer wraps at capacity | unit | `cereka_test --gtest_filter=RollbackManagerTest.*` | ❌ Wave 0 |
| RB-03 | History state push/pop from state machine | unit | `cereka_test --gtest_filter=StateMachineTest.*` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `ninja -C build cereka_test && ./build/tests/cereka_test --gtest_filter=<test>`
- **Per wave merge:** `ninja -C build cereka_test && ./build/tests/cereka_test && lua tests/compile/harness.lua`
- **Phase gate:** Full suite green before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] `tests/scene_graph_test.cpp` — covers SG-01 (SceneNode tree, transforms)
- [ ] `tests/markup_parser_test.cpp` — covers TM-01 (segment parsing, nesting, error handling)
- [ ] `tests/audio_manager_test.cpp` — covers AF-02 (fade timing curve)
- [ ] `tests/rollback_manager_test.cpp` — covers RB-01/RB-02 (snapshot, restore, capacity)
- [ ] `tests/compile/inputs/scene_graph.crka` + `expected/scene_graph.txt` — covers SG-02
- [ ] `tests/compile/inputs/audio_fade.crka` + `expected/audio_fade.txt` — covers AF-01
- [ ] `tests/compile/inputs/text_markup.crka` + `expected/text_markup.txt` — covers TM-01 compiler output

## Environment Availability

> Skip this section if the phase has no external dependencies — Phase 4 has no new external dependencies. All changes use existing vendored libraries (SDL3_ttf, SDL3_mixer, glaze) and C++23 standard library.

Step 2.6: SKIPPED (no external dependencies — all required libraries already vendored)

## Sources

### Primary (HIGH confidence)
- [VERIFIED: codebase] `src/cereka_audio_manager.hpp` lines 6-30 — AudioManager interface (PlayBGM, StopBGM, PlaySFX)
- [VERIFIED: codebase] `src/cereka_audio_manager.cpp` lines 52-110 — AudioManager implementation (MIX_CreateTrack, PlayTrack)
- [VERIFIED: codebase] `src/ui/ui_manager.hpp` — UIManager interface (scene graph attachment point)
- [VERIFIED: codebase] `src/state/cereka_state.hpp` — CerekaStateMachine overlay stack pattern
- [VERIFIED: codebase] `src/cereka_save_data.hpp` — SerializableSaveData (rollback snapshot schema)
- [VERIFIED: codebase] `src/cereka_save.cpp` — save/load patterns (restore logic)
- [VERIFIED: codebase] `src/state/cereka_states.cpp` — existing state implementations (overlay pattern reference)
- [VERIFIED: vendor] vendor/SDL_mixer/include/SDL3_mixer/SDL_mixer.h line 2159 — MIX_SetTrackGain API
- [VERIFIED: vendor] vendor/SDL_ttf/include/SDL3_ttf/SDL_ttf.h lines 456-459 — TTF_STYLE_BOLD/ITALIC/UNDERLINE/STRIKETHROUGH
- [VERIFIED: vendor] vendor/SDL_ttf/include/SDL3_ttf/SDL_ttf.h line 1555 — TTF_RenderText_Blended
- [VERIFIED: vendor] vendor/SDL_ttf/include/SDL3_ttf/SDL_ttf.h line 1591 — TTF_RenderText_Blended_Wrapped
- [VERIFIED: vendor] vendor/SDL_ttf/include/SDL3_ttf/SDL_ttf.h line 1254 — TTF_GetStringSize
- [VERIFIED: vendor] vendor/SDL_ttf/include/SDL3_ttf/SDL_ttf.h line 1310 — TTF_MeasureString
- [CITED: user decisions] CONTEXT.md D-01 through D-05 — locked design decisions

### Secondary (MEDIUM confidence)
- [CITED: standard game engine patterns] Scene node hierarchy with inherited world transforms is the approach used by Unity, Godot, Ren'Py ATL, and most game engines.
- [CITED: standard rich text patterns] Tag-stack-based markup parsing is the approach used by HTML, BBCode, Markdown, and Ren'Py text tags.

### Tertiary (LOW confidence)
- None — all architectural claims are either verified against the codebase or derived from standard patterns.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries already vendored and verified
- Architecture: HIGH — patterns directly extend existing codebase structures (UIManager, CerekaStateMachine, SerializableSaveData)
- Pitfalls: HIGH - derived from known issues in similar systems (transform accumulation in game engines, rich text layout in Ren'Py, crossfade resource management in audio systems)

**Research date:** 2026-05-07
**Valid until:** 2026-08-07 (stable — depends on SDL3_ttf/SDL3_mixer APIs which are stable)
