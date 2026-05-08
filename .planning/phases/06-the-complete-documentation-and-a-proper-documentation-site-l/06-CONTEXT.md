# Phase 6: Documentation - Context

**Gathered:** 2026-05-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Create complete game author documentation for Cereka v1.0. Game authors write only `.crka` scripts — the docs must cover the full scripting language, UI theming system, project structure, and build/packaging flow. Hosted as a static documentation site on Cloudflare Pages using a custom domain.

Target audience: game authors who know nothing about Cereka internals. Not engine contributors.

</domain>

<decisions>
## Implementation Decisions

### D-01: Static Site Generator — mdBook
- **mdBook** (Rust-based) — simplest deploy, native Cloudflare Pages support, markdown source
- Deploy via Cloudflare Pages connected to the repo (or a docs/ subdirectory)
- Custom domain: user's own domain

### D-02: Scope — Game Author Docs Only
- Everything a game author needs to write and ship a `.crka` game
- NOT engine contributor docs (state machine, renderer, compiler internals)
- NOT the CLAUDE.md dev docs (those stay internal)

### D-03: Docs Sections (minimum)
1. **Getting Started** — quickstart, project structure, first `.crka` script
2. **Scripting Reference** — every op: bg, char, hide char, narrate, say, label, jump, call, include, end, menu + button, set, `$` numeric vars, if/else/endif, save/load, save_menu/load_menu, ui (textbox, namebox, button, font, advance_keys), bgm, stop_bgm, sfx
3. **UI Theming** — property reference for textbox, namebox, button, font, advance_keys with all sub-properties (color, image, dimensions, text color, etc.)
4. **Variables & Expressions** — string vars (`set`), numeric vars (`$`), expression RHS, `{var}` substitution in dialogue
5. **Project Structure** — `game.cfg`, `assets/` directory layout, save files
6. **Build & Package** — how to build the engine, package a game via CerekaLauncher, platform targets
7. **Examples** — full working example game(s) with annotated `.crka` code

### D-04: Example Game
- Include a complete playable example game (e.g., a short visual novel scene) as part of the docs
- Annotated source showing every feature in action
- Separate page, not inline snippets

### the agent's Discretion
- Exact mdBook theme customization (colors, logo)
- Domain-specific URL structure
- Example game story content
- Whether to include a search feature (mdBook built-in)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Scope
- `.planning/ROADMAP.md` §Phase 6 — scope anchor and goals

### Engine to Document
- `scripts/cereka_compiler.lua` — full compiler source (every op documented here)
- `src/compiler/cereka_instruction.hpp` — Op enum (every instruction)
- `src/cereka_script.cpp` — CerekaScriptTick dispatch (how ops execute)
- `src/Cereka.cpp` — public API (InitGame, Say, Narrate, etc.)
- `src/cereka_save_data.hpp` — save format schema
- `src/cereka_ui_config.hpp` — UiConfig struct (all UI properties)
- `src/cereka_draw.cpp` — rendering pipeline
- `runner/main.cpp` — game.cfg parsing
- `launcher/templates.hpp` — template game.cfg and ui.crka

### Existing Documentation
- `CLAUDE.md` — internal dev docs (not for game authors, but reference for feature list)
- `.planning/codebase/ARCHITECTURE.md` — architecture reference
- `.planning/codebase/STACK.md` — technology stack

### mdBook
- mdBook documentation: https://rust-lang.github.io/mdBook/
- Cloudflare Pages: https://developers.cloudflare.com/pages/framework-guides/deploy-anything/

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **Launcher templates** (`launcher/templates.hpp`) — includes template `game.cfg` and `ui.crka` that can be reproduced in docs
- **CLAUDE.md** — has a `.crka Script Language` section with op summaries that can be expanded into full docs
- **Compile snapshots** (`tests/compile/inputs/*.crka`) — small example scripts exercising every op

### Established Patterns
- **Markdown** — CLAUDE.md and all planning docs use markdown, so mdBook's markdown source is natural
- **Code examples** — compile test inputs serve as validated example snippets

### Integration Points
- Documentation site deployed via Cloudflare Pages, triggered on push to `docs/` or root
- Custom domain DNS configured in Cloudflare dashboard
- mdBook `book.toml` config at repo root or `docs/` directory

</code_context>

<specifics>
## Specific Ideas

- "Like how all engine have documentation" — mdBook is the standard choice (used by Rust, Bevy, Godot Rust bindings, etc.)
- Cloudflare Pages + custom domain for clean URL
- Game author focus: teach the `.crka` language, not the C++ internals

</specifics>

<deferred>
## Deferred Ideas

- Engine contributor docs (state machine, renderer abstraction, compiler pipeline) — not for v1 docs
- Video tutorials — defer to community content
- API reference auto-generated from C++ headers — too complex for v1

</deferred>

---

*Phase: 6-Documentation*
*Context gathered: 2026-05-08*
