#pragma once
#include <SDL3/SDL.h>
#include <string>

// ---------------------------------------------------------------------------
// Dim — a measurement that can be expressed as absolute pixels or a % of the
//       screen dimension it is resolved against.
//
//   Dim::parse("75%") → {0.75, relative=true}
//   Dim::parse("540")  → {540,  relative=false}
// ---------------------------------------------------------------------------
struct Dim {
    float value    = 0.0f;
    bool  relative = false;

    float resolve(float screenDim) const
    {
        return relative ? value * screenDim : value;
    }

    static Dim parse(const std::string &s)
    {
        if (s.empty()) return {};
        Dim d;
        if (s.back() == '%') {
            d.value    = std::stof(s) / 100.0f;
            d.relative = true;
        } else {
            d.value = std::stof(s);
        }
        return d;
    }
};

// ---------------------------------------------------------------------------
// UiConfig — all theme-able properties with sensible defaults.
//
// Set from .crka scripts using:
//
//   ui textbox
//       color 0 0 0 160
//       y     75%
//       h     25%
//       image assets/ui/textbox.png   ; optional — overrides solid color
//
// ---------------------------------------------------------------------------
struct UiConfig {

    struct Textbox {
        std::string  imagePath;
        SDL_Texture *image       = nullptr;
        SDL_Color    color       = {0, 0, 0, 160};
        Dim          y           = {0.75f, true};
        Dim          h           = {0.25f, true};
        float        textMarginX = 70.0f;
        SDL_Color    textColor   = {255, 255, 255, 255};
    } textbox;

    struct Namebox {
        std::string  imagePath;
        SDL_Texture *image     = nullptr;
        SDL_Color    color     = {0, 255, 0, 255};
        float        x        = 50.0f;
        float        yOffset  = -70.0f; // pixels above the textbox top edge
        float        w        = 300.0f;
        float        h        = 60.0f;
        SDL_Color    textColor = {255, 255, 255, 255};
    } namebox;

    struct Button {
        std::string  imagePath;
        SDL_Texture *image          = nullptr;
        std::string  hoverImagePath;
        SDL_Texture *hoverImage     = nullptr;
        SDL_Color    color          = {0, 255, 255, 255};
        float        w              = 600.0f;
        float        h              = 80.0f;
        SDL_Color    textColor      = {255, 255, 255, 255};
    } button;

    int fontSize = 36;
};
