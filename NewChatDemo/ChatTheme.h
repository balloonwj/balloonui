// ChatTheme: design-token palette for the WeChat-style demo. Translated
// from docs/wechat-demo/styles.css `:root` block. oklch values are
// approximated to nearest sRGB; the design is OK with small color drift
// since these are reference mockups, not pixel-bound brand assets.
//
// Naming follows the CSS variables (e.g. --bg-app -> kBgApp) to make
// cross-checking against the design source trivial.
#pragma once

#include <windows.h>

namespace newchatdemo {

// ----- Surfaces ---------------------------------------------------------
static const COLORREF kBgApp        = RGB(248, 248, 246);   // --bg-app
static const COLORREF kBgSidebar    = RGB(240, 241, 239);   // --bg-sidebar
static const COLORREF kBgSidebar2   = RGB(231, 232, 230);   // --bg-sidebar-2
static const COLORREF kBgPanel      = RGB(251, 251, 250);   // --bg-panel
static const COLORREF kBgHover      = RGB(235, 236, 233);   // --bg-hover
static const COLORREF kBgActive     = RGB(223, 232, 227);   // --bg-active (brand-tinted)
static const COLORREF kBgBubbleIn   = RGB(255, 255, 255);
static const COLORREF kBgBubbleOut  = RGB(149, 225, 192);   // brand-bubble
static const COLORREF kBgTitleBar   = RGB(244, 245, 242);   // titlebar gradient mid

// ----- Ink --------------------------------------------------------------
static const COLORREF kInk1     = RGB( 43,  46,  48);       // primary text
static const COLORREF kInk2     = RGB( 90,  94,  98);
static const COLORREF kInk3     = RGB(135, 137, 141);
static const COLORREF kInk4     = RGB(170, 173, 176);
static const COLORREF kInkInv   = RGB(255, 255, 255);

// ----- Brand: Flamingo Green --------------------------------------------
static const COLORREF kBrand        = RGB( 76, 199, 161);
static const COLORREF kBrandDeep    = RGB( 47, 170, 134);
static const COLORREF kBrandSoft    = RGB(225, 240, 233);
static const COLORREF kBrandBubble  = RGB(149, 225, 192);

// ----- Status -----------------------------------------------------------
static const COLORREF kRed      = RGB(232,  90,  60);
static const COLORREF kAmber    = RGB(232, 169,  77);
static const COLORREF kOnline   = RGB( 93, 199, 117);

// ----- Lines ------------------------------------------------------------
static const COLORREF kLine1    = RGB(226, 227, 225);
static const COLORREF kLine2    = RGB(212, 213, 211);

// ----- macOS traffic light dots -----------------------------------------
static const COLORREF kTLRed    = RGB(255,  95,  87);
static const COLORREF kTLYellow = RGB(254, 188,  46);
static const COLORREF kTLGreen  = RGB( 40, 200,  64);

// ----- Pre-derived avatar gradient anchors (for sample contacts) --------
//
// In styles.css avatars use `linear-gradient(135deg, oklch(0.78 0.10 H), oklch(0.55 0.13 H))`
// where H is the conversation's color hue. Since DuiAvatar paints solid-fill
// + initials, we fold the gradient down to a single mid-tone color per hue.
// Hues from screen-chat.jsx SAMPLE_CONVS: 165, 200, 320, 240, 75, 145, 25, 300, 55.
static const COLORREF kAv165 = RGB( 76, 199, 161);   // brand green
static const COLORREF kAv200 = RGB( 88, 168, 199);   // teal blue
static const COLORREF kAv320 = RGB(199,  88, 161);   // magenta
static const COLORREF kAv240 = RGB( 96, 132, 207);   // indigo
static const COLORREF kAv075 = RGB(199, 168,  88);   // mustard
static const COLORREF kAv145 = RGB(105, 196, 124);   // grass green
static const COLORREF kAv025 = RGB(207, 121,  82);   // terracotta
static const COLORREF kAv300 = RGB(178,  98, 199);   // violet
static const COLORREF kAv055 = RGB(199, 178,  88);   // ochre

inline COLORREF AvatarColorByHue(int hue)
{
    switch (hue)
    {
        case 165: return kAv165;
        case 200: return kAv200;
        case 320: return kAv320;
        case 240: return kAv240;
        case  75: return kAv075;
        case 145: return kAv145;
        case  25: return kAv025;
        case 300: return kAv300;
        case  55: return kAv055;
        default:  return kAv165;
    }
}

} // namespace newchatdemo
