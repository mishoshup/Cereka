#pragma once
#include <QString>

namespace Theme {

// ── Colors ────────────────────────────────────────────────────────────────────

// Accent
constexpr const char *Gold            = "#B8944A";
constexpr const char *GoldHover       = "#C8A55C";
constexpr const char *GoldDimBg       = "#3A2E18";
constexpr const char *GoldDimText     = "#665533";
constexpr const char *GoldSelected    = "#D4A84E";
constexpr const char *GoldSelectedBg  = "#231C0E";

// Backgrounds
constexpr const char *BgDeep          = "#0E0E16";  // sidebar, log panel
constexpr const char *BgBase          = "#181820";  // content area
constexpr const char *BgSurface       = "#202030";  // secondary buttons, panels
constexpr const char *BgSurfaceHover  = "#2C2C42";
constexpr const char *BgItemHover     = "#14141E";  // sidebar list hover

// Borders / dividers
constexpr const char *BorderDivider   = "#1A1A26";  // hr lines
constexpr const char *BorderLog       = "#1E1E2C";  // log panel border
constexpr const char *BorderPanel     = "#2A2A3A";  // dropdown/panel borders
constexpr const char *BorderAccent    = "#3A3A52";  // outline button border

// Text
constexpr const char *TextPrimary     = "white";
constexpr const char *TextSecondary   = "#BBBBCC";
constexpr const char *TextMuted       = "#9999B0";
constexpr const char *TextHover       = "#AAAACC";
constexpr const char *TextFaint       = "#555566";
constexpr const char *TextDim         = "#44445A";
constexpr const char *TextDimmer      = "#33334A";  // section caps labels
constexpr const char *TextMicro       = "#666688";  // smallest muted text
constexpr const char *TextLog         = "#6666AA";  // output log text
constexpr const char *TextGlyph       = "#25252F";  // empty-state glyph

// ── Dimensions ────────────────────────────────────────────────────────────────

constexpr int SidebarWidth      = 210;

constexpr int WindowMinW        = 820;
constexpr int WindowMinH        = 560;
constexpr int WindowDefaultW    = 980;
constexpr int WindowDefaultH    = 660;

constexpr int FadeDurationMs    = 140;

// Font sizes (pt)
constexpr int FontBase          = 10;
constexpr int FontLogo          = 17;
constexpr int FontHero          = 40;
constexpr int FontHeading       = 16;
constexpr int FontTitle         = 15;

// Button heights
constexpr int BtnHeightLarge    = 48;  // welcome CTA
constexpr int BtnHeightAction   = 44;  // launch / package / init
constexpr int BtnHeightSidebar  = 36;  // new project
constexpr int BtnHeightGhost    = 30;  // change folder
constexpr int BtnHeightMicro    = 28;  // rename

// Border radii
constexpr int RadiusLarge       = 8;   // welcome CTA
constexpr int RadiusAction      = 7;   // launch / package / init
constexpr int RadiusStd         = 6;   // sidebar buttons
constexpr int RadiusSmall       = 5;   // rename / micro buttons

// ── Stylesheet helpers ────────────────────────────────────────────────────────

// Gold-filled primary button (Launch, New Project, Select Folder…)
inline QString primaryBtn(int radius = RadiusAction)
{
    return QString(R"(
        QPushButton {
            background-color: %1; color: #0A0A0A; border: none;
            border-radius: %2px; font-weight: 700;
        }
        QPushButton:hover    { background-color: %3; }
        QPushButton:disabled { background-color: %4; color: %5; }
    )").arg(Gold).arg(radius).arg(GoldHover).arg(GoldDimBg).arg(GoldDimText);
}

// Dark-filled secondary button (Package, secondary actions)
inline QString secondaryBtn(int radius = RadiusAction)
{
    return QString(R"(
        QPushButton {
            background-color: %1; color: %2; border: none;
            border-radius: %3px;
        }
        QPushButton:hover { background-color: %4; color: white; }
        QPushButton::menu-indicator { image: none; }
    )").arg(BgSurface).arg(TextSecondary).arg(radius).arg(BgSurfaceHover);
}

// Outlined secondary button (Initialize Project)
inline QString outlineBtn(int radius = RadiusAction)
{
    return QString(R"(
        QPushButton {
            background-color: %1; color: %2; border: 1px solid %3;
            border-radius: %4px; font-weight: 600;
        }
        QPushButton:hover { background-color: %5; color: white; }
    )").arg(BgSurface).arg(TextSecondary).arg(BorderAccent).arg(radius).arg(BgSurfaceHover);
}

// Transparent ghost button (Change Folder, Rename)
inline QString ghostBtn(int radius = RadiusStd)
{
    return QString(R"(
        QPushButton {
            background-color: transparent; color: %1; border: none;
            border-radius: %2px;
        }
        QPushButton:hover { color: %3; background-color: %4; }
    )").arg(TextDim).arg(radius).arg(TextHover).arg(BgItemHover);
}

// Dropdown menu (Package menu)
inline QString menuStyle()
{
    return QString(R"(
        QMenu {
            background-color: %1; border: 1px solid %2;
            border-radius: 8px; padding: 4px;
        }
        QMenu::item { padding: 8px 20px; color: %3; font-size: 13px; border-radius: 4px; }
        QMenu::item:selected { background-color: %4; color: #0A0A0A; }
        QMenu::separator { height: 1px; background: %2; margin: 4px 8px; }
    )").arg(BgDeep).arg(BorderPanel).arg(TextSecondary).arg(Gold);
}

} // namespace Theme
