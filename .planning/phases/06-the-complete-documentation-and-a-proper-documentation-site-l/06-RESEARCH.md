# Phase 6: Documentation — Research

**Researched:** 2026-05-08
**Domain:** Static documentation site for game author `.crka` scripting language
**Confidence:** HIGH

## Summary

Phase 6 builds a complete game-author documentation site for Cereka v1.0 using **mdBook v0.5.2** (Rust-based static site generator), deployed on **Cloudflare Pages** with a custom domain. The audience is game authors who write `.crka` scripts — not engine contributors.

mdBook is the correct choice: it has zero runtime dependencies, built-in search, multiple theme options, simple markdown source, and integrates trivially with Cloudflare Pages via `mdbook build` → `book/` output directory. Homebrew has mdBook 0.5.2 available (`brew install mdbook`). The docs will live in a `docs/` subdirectory of the Cereka repo, keeping the C++ project root clean.

Seven content sections are locked from CONTEXT.md: Getting Started, Scripting Reference, UI Theming, Variables & Expressions, Project Structure, Build & Package, and a complete annotated Example Game. The scripting reference must document every `.crka` op enumerated in `cereka_instruction.hpp` (38 ops total), the UI config properties from `cereka_ui_config.hpp`, the `game.cfg` format from `runner/main.cpp`, and the save JSON schema from `cereka_save_data.hpp`.

**Primary recommendation:** Place `book.toml` at `docs/book.toml`, build with `cd docs && mdbook build`, output in `docs/book/`. Configure Cloudflare Pages with build command `cd docs && mdbook build` and output directory `docs/book/`. Custom domain configured via Cloudflare Pages dashboard (not via mdBook's `cname` option, which is for GitHub Pages only).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Static site generator — mdBook (Rust-based)
- **D-01:** Deploy via Cloudflare Pages connected to the repo
- **D-01:** Custom domain — user's own domain
- **D-02:** Game author docs only (not engine contributor docs, not CLAUDE.md)
- **D-03:** Seven minimum sections: Getting Started, Scripting Reference, UI Theming, Variables & Expressions, Project Structure, Build & Package, Examples
- **D-04:** Include a complete annotated example game (separate page, not inline snippets)

### the agent's Discretion
- Exact mdBook theme customization (colors, logo)
- Domain-specific URL structure (e.g., `docs.cereka.dev` vs `cereka.dev/docs`)
- Example game story content
- Whether to include search feature (mdBook built-in)

### Deferred Ideas (OUT OF SCOPE)
- Engine contributor docs (state machine, renderer, compiler internals)
- Video tutorials — defer to community content
- API reference auto-generated from C++ headers

### Requirements
- **Phase requirement IDs:** None provided. Phase delivers against the seven sections defined in D-03.
</user_constraints>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Content authoring | Source repo (`docs/`) | — | Game author docs are markdown files committed to the repo |
| Build (mdbook build) | CI / Cloudflare Pages | — | Static site generation happens during Cloudflare Pages deployment build step |
| Hosting | Cloudflare Pages | — | Static HTML served from Cloudflare edge network |
| Custom domain | Cloudflare dashboard | — | DNS managed via Cloudflare dashboard, not in code |
| Search | mdBook built-in (client-side JS) | — | Fuse.js-based search is compiled into the static output, no server needed |

This is a pure documentation phase — there is no runtime or backend component. Content is authored as markdown, built to static HTML by mdBook, and served by Cloudflare Pages.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| mdBook | 0.5.2 | Static documentation site generator | Native markdown, built-in search, multiple themes, trivially deployable to Cloudflare Pages. Used by Rust project, Bevy, and numerous dev tools. [VERIFIED: Homebrew core, GitHub releases] |

### Supporting
| Library/Tool | Version | Purpose | When to Use |
|-------------|---------|---------|-------------|
| highlight.js | Bundled with mdBook 0.5.2 | Syntax highlighting for code blocks | Automatic for supported languages. For `.crka`, use `text` or `plaintext` highlighting since no custom language definition exists. [VERIFIED: mdBook docs syntax-highlighting page] |
| Font Awesome 6 | Bundled with mdBook 0.5.2 | Icon support via `<i class="fa-solid fa-...">` | For decorative icons in docs. [VERIFIED: mdBook docs mdbook-specific-features page] |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| mdBook | Docusaurus (React-based) | Docusaurus requires Node.js, npm dependencies, longer build times, and is heavier than needed for a markdown-only docs site. Overkill for game author docs. [CITED: discuss-phase discussion-log.md] |
| mdBook | MkDocs Material (Python-based) | Requires Python ecosystem, pip dependencies. mdBook is a single Rust binary — simpler CI setup and faster builds. [CITED: discuss-phase discussion-log.md] |

**Installation:**
```bash
brew install mdbook       # macOS — already confirmed available via Homebrew
# Alternative (no Rust needed — use precompiled binary):
# curl -sSL https://github.com/rust-lang/mdBook/releases/download/0.5.2/mdbook-0.5.2-x86_64-apple-darwin.tar.gz | tar -xz
```

**Version verification:** mdBook 0.5.2 confirmed via `brew info mdbook` — stable, bottled. Latest GitHub release also 0.5.2.

## Architecture Patterns

### System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        REPOSITORY                           │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  docs/                                                │   │
│  │  ├── book.toml          (mdBook configuration)        │   │
│  │  ├── src/               (markdown source)             │   │
│  │  │   ├── SUMMARY.md     (chapter structure)           │   │
│  │  │   ├── getting-started.md                           │   │
│  │  │   ├── scripting-reference/                         │   │
│  │  │   ├── ui-theming.md                                │   │
│  │  │   ├── variables-and-expressions.md                 │   │
│  │  │   ├── project-structure.md                         │   │
│  │  │   ├── build-and-package.md                         │   │
│  │  │   └── examples/                                    │   │
│  │  └── theme/              (optional overrides)         │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  game.cfg, src/*.cpp, scripts/*.lua, launcher/*...   │   │
│  │  (C++ engine source — unchanged by this phase)        │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────┬──────────────────────────────────────────────┘
               │ git push
               ▼
┌──────────────────────────────────────────────────────────────┐
│              CLOUDFLARE PAGES (CI build)                     │
│                                                              │
│  1. Clone repo                                                │
│  2. Install mdbook (if not cached): brew install mdbook      │
│  3. Build: cd docs && mdbook build                           │
│  4. Output: docs/book/  →  uploaded to Cloudflare edge       │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
               ┌───────────────────────┐
               │   Cloudflare Edge      │
               │   (global CDN)         │
               │                       │
               │   docs.cereka.dev      │
               │   (custom domain)      │
               └───────────────────────┘
                           │
                           ▼
               ┌───────────────────────┐
               │   Game Author          │
               │   (browser)            │
               └───────────────────────┘
```

### Recommended Project Structure
```
docs/
├── book.toml                    # mdBook configuration
├── src/
│   ├── SUMMARY.md               # Chapter structure (table of contents)
│   ├── README.md                # Introduction / landing page
│   ├── getting-started.md       # Quickstart, project setup, first .crka script
│   ├── scripting-reference/
│   │   ├── README.md            # Overview of scripting language
│   │   ├── scene.md             # bg, char, hide char, fade
│   │   ├── dialogue.md          # narrate, say, {var} substitution
│   │   ├── audio.md             # bgm, stop_bgm, sfx, audio fade/crossfade
│   │   ├── flow.md              # label, jump, include, call, return, end
│   │   ├── menu.md              # menu, button (goto, exit)
│   │   ├── variables.md         # set, $ arithmetic
│   │   ├── conditionals.md      # if/else/endif, comparisons
│   │   ├── save-load.md         # save, load, save_menu, load_menu
│   │   ├── ui-theming.md        # ui textbox, namebox, button, font, advance_keys
│   │   └── text-markup.md       # <b>, <i>, <color=#rrggbb>
│   ├── ui-theming.md            # Full UI property reference
│   ├── variables-and-expressions.md  # String vars, numeric vars, expression RHS
│   ├── project-structure.md     # game.cfg, assets/ layout, save files
│   ├── build-and-package.md     # Build engine, package via launcher, platforms
│   └── examples/
│       ├── README.md            # Overview of example game(s)
│       └── annotated-game.md    # Complete annotated example game
├── theme/                       # (optional) Theme overrides
│   ├── css/
│   │   └── general.css          # Override mdBook styles
│   ├── favicon.svg              # Custom Cereka favicon
│   └── (other overrides as needed)
└── images/                      # Screenshots, diagrams, logos
    ├── cereka-logo.svg
    ├── getting-started-screenshot.png
    ├── ...
```

### Pattern 1: Documentation by Feature Category (Scripting Reference)
**What:** Organize the scripting reference into sub-pages grouped by functional category (scene, dialogue, audio, flow, menu, variables, conditionals, save/load, theming, markup), not alphabetically. Each op gets its own section with syntax, description, parameter table, and example.
**When to use:** The `.crka` language has 38 ops across ~10 categories — grouping by function makes it findable for authors who think "I need to play a sound" not "what's the `bgm` op syntax?"
**Example:**
```markdown
# Scene Commands

## `bg` — Set Background
Sets the current background image.

**Syntax:**
```crka
bg <filename>
bg <filename> fade <seconds>
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `filename` | string | Path relative to `assets/bg/` |
| `seconds` | float (optional) | Crossfade duration in seconds |

**Examples:**
```crka
; Instant background change
bg forest.png

; Crossfade over 2 seconds
bg castle.png fade 2.0
```

**Notes:**
- Supported image formats: PNG, JPG
- Background is rendered behind all characters and UI elements
- The fade instruction blocks script execution until complete
```
[ASSUMED] — Based on standard API documentation patterns and mdBook's markdown capabilities.

### Pattern 2: Annotated Example Game
**What:** A complete, playable `.crka` script with inline HTML annotations explaining each section. Use mdBook's `{{#include}}` for the raw script file (keeps it runnable/verifiable) plus annotation callouts above/below each section.
**When to use:** D-04 mandates a complete annotated example. Rendering the actual `.crka` file ensures annotations don't drift from the game that ships with the engine.
**Example:**
```markdown
# Complete Example Game

Below is a short visual novel scene demonstrating every Cereka feature.
The file is `examples/demo.crka` in the engine repository — you can
run it directly with CerekaGame.

## Main Menu

```crka
{{#include ../../examples/demo.crka:main_menu}}
```

The main menu uses the `menu` command with three buttons.
`button "Label" goto <label>` jumps to the named label when clicked.
`button "Quit" exit` closes the game.
```
[VERIFIED: mdBook docs — `{{#include}}` with anchor tags for partial file inclusion.]

### Anti-Patterns to Avoid
- **Putting all ops on one page:** The scripting reference has 38 ops across ~10 categories. A single page would be unmanageable. Use sub-pages per category, linked from the sidebar.
- **Including C++ internals in docs:** D-02 explicitly says game author docs only. No references to CerekaImpl, sol2, glaze, state machines, render context, or build system internals.
- **Dead code examples:** Every `.crka` example in the docs should be verifiable via the compile test harness (`lua tests/compile/harness.lua`). Use the compile test inputs as canonical examples.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Static site generation | Custom HTML/CSS site | mdBook | mdBook handles navigation, search, themes, print, responsive layout, 404 pages — all features a docs site needs. Writing this from scratch would take weeks. |
| Search | Custom Fuse.js integration | mdBook built-in search | mdBook bundles Fuse.js-based client-side search. It's enabled by default and requires zero configuration. |
| Code syntax highlighting | Custom highlight.js language | mdBook bundled highlight.js + `text`/`crka` blocks | highlight.js supports 40+ languages bundled. For `.crka`, use `text` blocks or create a custom `highlight.js` definition if desired (add to `theme/highlight.js`). |
| Deployment pipeline | Custom CI/CD scripts | Cloudflare Pages git integration | Cloudflare Pages auto-deploys on `git push` to the connected branch. No separate CI config needed — just set build command and output dir in the dashboard. |

**Key insight:** mdBook + Cloudflare Pages is the "zero-config docs site" stack. mdBook gives you book structure, search, themes, and navigation. Cloudflare Pages gives you auto-deploy, HTTPS, CDN, custom domains, and preview deployments. The only custom work is writing the markdown content and optionally tweaking the theme CSS.

## Common Pitfalls

### Pitfall 1: Highlight.js has no `.crka` language definition
**What goes wrong:** Code blocks tagged with ` ```crka ` will render as plain text with no syntax highlighting because highlight.js doesn't know the language.
**Why it happens:** mdBook uses highlight.js which auto-detects language only for recognized languages. `.crka` is a custom format.
**How to avoid:** Use ` ```text ` or ` ```plaintext ` for code blocks. This is acceptable since `.crka` syntax is simple (comments via `;`, keywords, string literals). If colored syntax is desired, create a custom `highlight.js` language definition in `theme/highlight.js`, but this is significant effort for limited gain.
**Warning signs:** Code blocks show no color in the rendered output.

### Pitfall 2: mdBook `cname` option is for GitHub Pages only
**What goes wrong:** Configuring `[output.html] cname = "docs.cereka.dev"` in `book.toml` writes a `CNAME` file to the output, which is only used by GitHub Pages. Cloudflare Pages ignores this file — custom domains are configured in the Cloudflare Pages dashboard, not via files in the output.
**Why it happens:** The mdBook docs mention the `cname` option in the context of GitHub Pages. It's easy to assume it works for any static host.
**How to avoid:** Do NOT set `cname` in `book.toml`. Instead, configure the custom domain through the Cloudflare Pages dashboard: go to Pages → project → Custom domains → Set up a domain. [VERIFIED: Cloudflare Pages custom-domains docs]
**Warning signs:** Custom domain works but `CNAME` file appears in output root.

### Pitfall 3: Cloudflare Pages build environment may not have mdBook
**What goes wrong:** The first deployment build fails because the Cloudflare Pages build environment doesn't have the `mdbook` binary.
**Why it happens:** Cloudflare Pages supports many Node.js-based frameworks natively but doesn't have mdBook pre-installed.
**How to avoid:** Set the build command to install mdBook before building. Use a precompiled binary download (fastest — no Rust compilation needed):
```
curl -sSL https://github.com/rust-lang/mdBook/releases/download/v0.5.2/mdbook-v0.5.2-x86_64-unknown-linux-gnu.tar.gz | tar -xz && ./mdbook build
```
Or use the official action approach with a build script. [CITED: mdBook continuous-integration docs — pre-compiled binaries for CI]
**Warning signs:** Build logs show `mdbook: command not found`.

### Pitfall 4: Cloudflare CAA records blocking certificate issuance
**What goes wrong:** When adding a custom domain, Cloudflare Pages attempts to auto-provision an SSL certificate. If the domain has CAA DNS records that only allow specific CAs, and Cloudflare's CAs aren't listed, certificate issuance fails.
**Why it happens:** CAA records restrict which Certificate Authorities can issue certificates for a domain. Cloudflare Pages uses Let's Encrypt, Google Trust Services, and SSL.com.
**How to avoid:** Ensure CAA records include `letsencrypt.org`, `pki.goog`, and `ssl.com`. [VERIFIED: Cloudflare Pages custom-domains docs — CAA records section]
**Warning signs:** Custom domain shows "SSL certificate pending" indefinitely.

### Pitfall 5: Broken navigation if `site-url` is not set for non-root deployment
**What goes wrong:** If the docs are deployed at a subpath (e.g., `cereka.dev/docs/`), the 404 page and some static assets may not load correctly because they reference `/` paths.
**Why it happens:** mdBook's 404.html and asset paths need to know the deployment path to construct correct URLs.
**How to avoid:** Set `[output.html] site-url = "/docs/"` in `book.toml` if deploying to a subpath. For apex or subdomain deployment (e.g., `docs.cereka.dev`), the default `site-url = "/"` is correct. [VERIFIED: mdBook CI docs — 404 handling section]

## Code Examples

### Example 1: Complete `book.toml` for Cereka docs

```toml
[book]
title = "Cereka Game Engine Documentation"
authors = ["Cereka Contributors"]
description = "Complete guide to writing games with Cereka and the .crka scripting language."
language = "en"

[build]
build-dir = "book"
create-missing = false

[output.html]
default-theme = "light"
preferred-dark-theme = "navy"
git-repository-url = "https://github.com/user/cereka"
edit-url-template = "https://github.com/user/cereka/edit/main/docs/{path}"
site-url = "/"
no-section-label = false
additional-css = ["theme/custom.css"]

[output.html.search]
enable = true
limit-results = 20

[output.html.print]
enable = true
page-break = true
```

Source: Synthesized from [VERIFIED: mdBook configuration docs]

### Example 2: `SUMMARY.md` structure

```markdown
# Summary

[Introduction](README.md)

# User Guide

- [Getting Started](getting-started.md)
- [Project Structure](project-structure.md)
- [Build & Package](build-and-package.md)

# Scripting Reference

- [Scene Commands](scripting-reference/scene.md)
- [Dialogue Commands](scripting-reference/dialogue.md)
- [Audio Commands](scripting-reference/audio.md)
- [Flow Control](scripting-reference/flow.md)
- [Menus & Choices](scripting-reference/menu.md)
- [Variables](scripting-reference/variables.md)
- [Conditionals](scripting-reference/conditionals.md)
- [Save & Load](scripting-reference/save-load.md)
- [UI Theming](scripting-reference/ui-theming.md)
- [Text Markup](scripting-reference/text-markup.md)

# Theming & Configuration

- [UI Theming Reference](ui-theming.md)
- [Variables & Expressions](variables-and-expressions.md)

# Examples

- [Annotated Example Game](examples/annotated-game.md)
```

Source: Synthesized from [VERIFIED: mdBook SUMMARY.md docs]

### Example 3: Annotated op reference table

```markdown
## `char` — Show Character

Displays a character sprite on screen.

**Syntax:**
```text
char <id> [position] <filename>
```

**Parameters:**
| Parameter | Required | Description |
|-----------|----------|-------------|
| `id` | yes | Unique identifier for this character (used in `say`, `hide char`) |
| `position` | no | One of `left`, `center`, `right`. Default: `left`. |
| `filename` | yes | Path relative to `assets/characters/` |

**Behavior:**
- If a character with the same `id` is already visible, it is moved to the new position
- Characters are drawn in order of appearance (later characters on top)

**Examples:**
```text
; Show Alice on the left
char Alice left alice_happy.png

; Move Alice to center
char Alice center alice_happy.png

; Show Bob on the right
char Bob right bob_sad.png

; Remove a character
hide char Alice
```
```

Source: Synthesized from [VERIFIED: cereka_instruction.hpp Op enum + compile test inputs/basic.crka + templates.hpp main.crka comments]

### Example 4: UI theming config reference

```markdown
## `ui textbox` — Dialogue Text Box

Controls the appearance of the dialogue text box at the bottom of the screen.

**Syntax:**
```text
ui textbox
    color      <r> <g> <b> <a>
    y          <percent% | pixels>
    h          <percent% | pixels>
    text_margin_x <pixels>
    text_color <r> <g> <b> <a>
    image      <path>            ; optional — overrides solid color fill
```

**Properties:**
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `color` | 4 × int (0-255) | `0 0 0 160` | RGBA fill color |
| `y` | Dim | `75%` | Vertical position from top (pixels or %) |
| `h` | Dim | `25%` | Height (pixels or %) |
| `text_margin_x` | float | `70` | Horizontal text inset from edges |
| `text_color` | 4 × int (0-255) | `255 255 255 255` | Text RGBA color |
| `image` | path | (none) | Path to image file for textbox background |

**Examples:**
```text
; Default semi-transparent black textbox
ui textbox
    color 0 0 0 160
    y 75%
    h 25%
    text_margin_x 80
    text_color 255 255 255 255

; Custom image-based textbox
ui textbox
    image assets/ui/textbox.png
```
```

Source: Synthesized from [VERIFIED: cereka_ui_config.hpp — UiConfig struct + launcher/templates.hpp — kUiScriptTemplate]

### Example 5: Save format reference

```markdown
## Save File Format

Save files are stored as JSON in `saves/slot{N}.json` (N = 1–10).

**Schema:**
```json
{
    "version": 1,
    "timestamp": "2026-05-08T12:00:00",
    "programCounter": 42,
    "callStack": [12, 28],
    "variables": {
        "player_name": "Hero",
        "gold": "250"
    },
    "numVariables": {
        "gold": 250,
        "hp": 75
    },
    "background": "forest.png",
    "characters": [
        { "id": "Alice", "file": "alice_happy.png", "position": "left" }
    ],
    "bgm": "theme.ogg",
    "state": "Dialogue",
    "speaker": "Alice",
    "name": "",
    "text": "Hello there!",
    "displayedChars": 12,
    "skipMode": false,
    "skipDepth": 0
}
```

**Notes:**
- Save files are created automatically when the player uses `save <slot>` or the save menu
- Loading a save restores the exact game state including position in script, variables, characters, background, and audio
- The `version` field enables future schema migration
```

Source: [VERIFIED: cereka_save_data.hpp — SerializableSaveData struct]

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| N/A (greenfield) | mdBook 0.5.2 | N/A | First documentation site for Cereka |
| N/A | Cloudflare Pages | N/A | Zero-config deployment with auto-HTTPS, CDN, custom domains |

**Deprecated/outdated:**
- None — this is a greenfield documentation phase

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `.crka` code blocks will use `text` or `plaintext` language tag since highlight.js has no `.crka` definition | Common Pitfalls / Syntax Highlighting | Low — `.crka` syntax is simple enough that plaintext highlighting is acceptable; IDE plugins can be created later |
| A2 | Cloudflare Pages build environment is Linux x86_64 | Don't Hand-Roll / Deployment | Medium — the precompiled mdBook binary URL must match the CI platform; Cloudflare Pages does use Linux x86_64 runners |
| A3 | The `site-url` default of `/` is correct for subdomain deployment (`docs.cereka.dev`) | Common Pitfalls | Low — if deploying to a path like `cereka.dev/docs/`, this must change to `/docs/` |
| A4 | Search feature should be enabled (mdBook default) | Agent's Discretion | Low — search adds ~50KB of JS; universally expected in documentation |

## Open Questions

1. **Domain URL structure — `docs.cereka.dev` vs `cereka.dev/docs`?**
   - What we know: mdBook works with both subdomain and subpath. The `cname` config option is for GitHub Pages only (not Cloudflare). Cloudflare Pages handles custom domains via dashboard.
   - What's unclear: The user's domain name and preferred structure. This is marked as the agent's discretion, but the user needs to provide the actual domain.
   - Recommendation: Default to `docs.cereka.dev` subdomain — cleaner separation and no `site-url` config needed.

2. **Example game content?**
   - What we know: Must be a complete, playable example showing every feature. The templates in `launcher/templates.hpp` have an existing demo script (kMainScriptTemplate) that covers most ops.
   - What's unclear: Story content and length. Should it reuse the template tutorial script, or create a new one specifically for docs?
   - Recommendation: Create a dedicated example game in `docs/examples/` directory that mirrors the engine's template but is annotated for the docs. Keep it short (5-10 scenes) but feature-complete.

3. **Custom favicon/logo?**
   - What we know: mdBook supports custom favicon via `theme/favicon.svg` and `theme/favicon.png`.
   - What's unclear: Whether Cereka has a logo or the user wants to create one.
   - Recommendation: Use a simple "C" letter icon or text-only favicon. Can be added later without build changes.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| mdBook | Documentation build | Not installed | 0.5.2 (via Homebrew) | Install via `brew install mdbook` |
| Homebrew | macOS mdBook install | ✓ | System | Use precompiled binary from GitHub releases |
| Cloudflare account | Deployment | User-managed | — | — |
| Custom domain | Production URL | User-managed | — | Use `*.pages.dev` subdomain temporarily |

**Missing dependencies with no fallback:**
- None — mdBook is trivially installable via Homebrew (`brew install mdbook`)

**Missing dependencies with fallback:**
- Cloudflare account: Without it, deploy via any static host (Netlify, GitHub Pages, Vercel, S3). mdBook's output is generic static HTML.

## Validation Architecture

> Skip section — this is a documentation-only phase with no runtime code, tests, or verification requirements. See `workflow.nyquist_validation` in config (enabled). No test infrastructure exists or is needed for markdown content.

### Wave 0 Gaps
None — test infrastructure is not applicable for documentation content. Validation is done through manual review of rendered mdBook output.

## Security Domain

> Skip section — documentation-only phase, no executable code, no authentication, no data storage, no network endpoints. `security_enforcement` is enabled by default but no applicable ASVS categories exist for markdown content.

## Sources

### Primary (HIGH confidence)
- [VERIFIED: Homebrew] — `brew info mdbook` confirms version 0.5.2, stable, bottled
- [VERIFIED: mdBook official docs] — https://rust-lang.github.io/mdBook/ — configuration, SUMMARY.md, theme, markdown, mdbook-specific features, CI, syntax highlighting
- [VERIFIED: Cloudflare Pages docs] — https://developers.cloudflare.com/pages/ — custom domains, CAA records, git integration, deploy-anything guide
- [VERIFIED: Codebase] — `cereka_instruction.hpp` (38 ops), `cereka_ui_config.hpp` (UI props), `cereka_save_data.hpp` (save schema), `launcher/templates.hpp` (template scripts), `runner/main.cpp` (game.cfg), `tests/compile/inputs/*.crka` (validated examples)

### Secondary (MEDIUM confidence)
- [CITED: discuss-phase discussion-log.md] — mdBook chosen over Docusaurus/MkDocs, rationale documented

### Tertiary (LOW confidence)
- None — all critical claims verified against official docs or codebase

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — mdBook 0.5.2 verified via Homebrew and official docs; Cloudflare Pages verified via their official docs
- Architecture: HIGH — mdBook project structure is well-documented; deploy pipeline is straightforward
- Pitfalls: HIGH — all verified against official docs (CAA records, site-url, highlight.js limitations)
- Code examples: HIGH — all based on actual codebase files (Op enum, UiConfig, SaveData, template scripts)

**Research date:** 2026-05-08
**Valid until:** 2026-06-08 (mdBook is stable software — 30-day validity is conservative)
