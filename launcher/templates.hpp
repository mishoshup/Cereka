#pragma once

// ============================================================================
// Embedded project template strings — included by launcher/main.cpp
// ============================================================================

static const char *kGameCfgTemplate = R"CRKA(# Cereka game configuration
# -----------------------------------------------
# title      : window title shown in the taskbar
# width/height: resolution in pixels
# fullscreen  : true or false
# entry       : path to the first script to run
# -----------------------------------------------
title      = My Visual Novel
width      = 1280
height     = 720
fullscreen = false
entry      = assets/scripts/main.crka
)CRKA";

// ---------------------------------------------------------------------------
// ui.crka — UI theme, included by main.crka
// ---------------------------------------------------------------------------
static const char *kUiScriptTemplate =
    R"CRKA(; ================================================================
; ui.crka — Cereka UI theme
;
; Included at the top of main.crka via:  include ui.crka
; Change any value here to restyle the whole game with no recompile.
;
; Positions:  use % for screen-relative (75%) or raw pixels (540)
; Colors:     r g b a  (0-255 each)
; Images:     path relative to project root - omit to use solid color
; ================================================================

; ---- Dialogue text box (bottom area) ----
ui textbox
    color          0 0 0 160
    y              75%
    h              25%
    text_margin_x  80
    text_color     255 255 255 255
;   image  assets/ui/textbox.png   ; uncomment to use a custom image

; ---- Speaker name box ----
ui namebox
    color       30 30 100 255
    x           50
    y_offset    -65
    w           260
    h           52
    text_color  255 220 120 255
;   image  assets/ui/namebox.png

; ---- Choice buttons ----
ui button
    color       20 80 120 255
    w           560
    h           72
    text_color  255 255 255 255
;   image        assets/ui/button.png
;   hover_image  assets/ui/button_hover.png

; ---- Font ----
ui font
    size  36
)CRKA";

// ---------------------------------------------------------------------------
// scene_two.crka — subroutine demo
// ---------------------------------------------------------------------------
static const char *kSceneTwoTemplate =
    R"CRKA(; ================================================================
; scene_two.crka — a separate scene file
;
; Called from main.crka with:   call scene_two.crka
; The engine returns to main.crka automatically when this ends.
;
; Use 'call' for scenes that need to return (subroutines).
; Use 'include' to inline a file at compile time (no return).
;
; Alice is already on the left when we arrive here.
; Bob enters on the right so they face each other.
; ================================================================

char Bob right placeholder_char.png
say Bob "I am Bob, loaded from scene_two.crka!"
say Alice "And I am still here — character state carries across calls."
say Bob "When this scene ends the engine returns to main.crka."
hide char Bob
)CRKA";

// ---------------------------------------------------------------------------
// main.crka — entry point: main menu + full command tutorial
// ---------------------------------------------------------------------------
static const char *kMainScriptTemplate =
    R"CRKA(; ================================================================
; main.crka — Cereka entry point
;
; Starts with a main menu (New Game / Load Game / Quit).
; The story begins at label new_game.
; Every engine command is shown with comments below.
; Delete what you don't need and write your story.
; ================================================================
;
; Project folder layout:
;
;   game.cfg                  — title, resolution, entry script
;   assets/
;     scripts/                — .crka script files (this folder)
;       main.crka             — entry point (set in game.cfg)
;       ui.crka               — UI theme (included by main.crka)
;       scene_two.crka        — example subroutine scene
;     bg/                     — background images (.png/.jpg)
;     characters/             — character sprites (.png)
;     sounds/                 — music and sound effects (.wav/.ogg)
;     fonts/                  — font files (.ttf/.otf)
;     ui/                     — optional UI images (textbox, buttons…)
;   saves/                    — save files, auto-created at runtime
;
; ================================================================

include ui.crka

; ================================================================
; MAIN MENU
; ================================================================

label main_menu

bg placeholder_bg.png

; MENU — present buttons. bg inside menu swaps background.
;   button "Label" goto <label>   — jump to label on click
;   button "Label" exit           — quit the game
menu
    button "New Game"  goto new_game
    button "Load Game" goto do_load
    button "Quit"      exit

; LOAD_MENU — show the 10-slot load overlay.
; ESC cancels and returns here (jumps back to main_menu).
label do_load
load_menu
jump main_menu

; ================================================================
; STORY BEGINS HERE
; ================================================================

label new_game

; BGM — background music, loops until stop_bgm.
; Syntax: bgm <filename>   (relative to assets/sounds/)
bgm placeholder_bgm.wav

; BG — instant background swap.
bg placeholder_bg.png

; NARRATE — narrator text (no name box). Click/key to advance.
narrate "Welcome to Cereka! This script is your tutorial."
narrate "Every command in the engine is shown here with comments."
narrate "When you are done reading, delete everything and write your story."

; ---- Folder layout tour ----
narrate "Your project is organized like this:"
narrate "  game.cfg             — title, resolution, entry script"
narrate "  assets/scripts/      — .crka script files (you are here)"
narrate "  assets/bg/           — background images (.png, .jpg)"
narrate "  assets/characters/   — character sprites (.png)"
narrate "  assets/sounds/       — music and sound effects (.wav, .ogg)"
narrate "  assets/fonts/        — font files (.ttf, .otf)"
narrate "  assets/ui/           — optional UI images (textbox, buttons...)"
narrate "  saves/               — save files, auto-created when you save"

; BG FADE — crossfade to a new background over <seconds>.
bg placeholder_bg.png fade 1.0
narrate "That background faded in over 1 second."

; CHAR — show a character sprite.
; Re-running char with the same ID moves it to the new position.
; Syntax: char <ID> [left|center|right] <filename>
char Alice left placeholder_char.png

; SAY — dialogue with a speaker name box.
; Syntax: say <ID> "text"
say Alice "Hi! I can stand on the left..."
char Alice center placeholder_char.png
say Alice "...the center..."
char Alice right placeholder_char.png
say Alice "...or the right!"
char Alice left placeholder_char.png
say Alice "Let me move aside. Someone is coming."

; SFX — one-shot sound effect (does not pause the script).
sfx placeholder_sfx.wav

; Tip: use  save_menu  before a big choice to let the player save.
; Players can also press ESC at any time to open the save menu.

; CALL — run another script as a subroutine and return here.
call scene_two.crka

narrate "Returned from scene_two.crka."

; SET + IF/ENDIF — string variables and conditional blocks.
set choice none

menu
    bg placeholder_bg.png fade 0.5
    button "Explore" goto explore
    button "Skip"    goto the_end
    button "Quit"    exit

label explore

; HIDE CHAR — remove a character sprite.
hide char Alice

set choice explored

if choice == explored
    narrate "You chose to explore!"
endif

if choice != skipped
    narrate "This shows because choice != skipped."
endif

; SAVE — silently save to slot 1 (no overlay).
save 1

jump the_end


label the_end

stop_bgm

narrate "That covers every Cereka command."
narrate "Save files: saves/slot1.sav to slot10.sav"
narrate "Press ESC during dialogue to open the save menu."

end
)CRKA";
