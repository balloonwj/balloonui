#include "stdafx.h"
#include "DuiButton.h"

#if BUI_FEATURE_BUTTON

#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"
#include "../../DuiNinePatch.h"
#include "../../DuiMnemonic.h"

namespace balloonwjui {

namespace {

// ---- Push-button brand palette (StylePushButton) ----------------------------
// Colors match the project-wide brand blue specified in CLAUDE.md.
// Hover is a deliberate "one step darker" of normal (#2D6CDF -> #2559B8) and
// pressed steps darker still (#1E4A99) so the user feels positive feedback
// on hold without the button blending into the dialog chrome.

// Normal fill - the brand blue users see most of the time. #2D6CDF.
const COLORREF kPushButtonFillNormal   = RGB( 45, 108, 223);
// Hover fill - one step darker than normal for clear hover affordance. #2559B8.
const COLORREF kPushButtonFillHover    = RGB( 37,  89, 184);
// Pressed fill (cursor still over button) - one step darker than hover. #1E4A99.
const COLORREF kPushButtonFillPressed  = RGB( 30,  74, 153);
// Disabled fill - neutral mid-gray; conveys "inert" without screaming.
const COLORREF kPushButtonFillDisabled = RGB(180, 180, 180);
// Border - matches the pressed fill so the button looks crisp at every state.
const COLORREF kPushButtonBorder         = RGB( 30,  74, 153);
// Border when disabled - lighter gray, parallel to disabled fill.
const COLORREF kPushButtonBorderDisabled = RGB(150, 150, 150);
// Label color when enabled - white for legibility on brand-blue fill.
const COLORREF kPushButtonLabelEnabled  = RGB(255, 255, 255);
// Label color when disabled - pale gray, still readable on disabled fill.
const COLORREF kPushButtonLabelDisabled = RGB(220, 220, 220);

// ---- Checkbox / Radio / Icon (light-fill family) ----------------------------
// These styles sit on a light background so their inner glyph (check tick,
// radio dot, icon) stays visible. Each of the five interaction states gets
// its own pale-blue ramp.

// Light styles: disabled fill - flat light gray, no chroma.
const COLORREF kLightFillDisabled = RGB(225, 225, 225);
// Light styles: pressed-and-hovered (active confirm) - mid blue.
const COLORREF kLightFillPressedHover = RGB(120, 170, 220);
// Light styles: pressed but cursor dragged off - in-between blue.
const COLORREF kLightFillPressedOnly  = RGB(160, 190, 225);
// Light styles: hover only - pale blue tint.
const COLORREF kLightFillHover        = RGB(200, 220, 245);
// Light styles: focused (no hover) - very pale blue, just a tint.
const COLORREF kLightFillFocused      = RGB(235, 245, 255);
// Light styles: idle - near-white neutral.
const COLORREF kLightFillIdle         = RGB(245, 245, 245);

// Light styles: border colors paralleling the fill ramp.
const COLORREF kLightBorderDisabled = RGB(180, 180, 180);
const COLORREF kLightBorderFocused  = RGB( 80, 130, 200);
const COLORREF kLightBorderHover    = RGB(100, 140, 200);
const COLORREF kLightBorderIdle     = RGB(150, 150, 150);

// Light-style label color when disabled.
const COLORREF kLightLabelDisabled = RGB(140, 140, 140);

// ---- Glyph colors for radio / checkbox / icon -------------------------------
// White interior of the radio/checkbox glyph rectangle.
const COLORREF kGlyphInteriorWhite = RGB(255, 255, 255);
// Solid blue dot inside a checked radio button.
const COLORREF kRadioDotBlue       = RGB( 40, 100, 200);
// Green tick stroke for checked checkbox - matches Windows convention.
const COLORREF kCheckTickGreen     = RGB( 40, 120,  40);
// Diamond glyph fill for the placeholder icon style.
const COLORREF kIconDiamondFill    = RGB( 80, 130, 220);
// Diamond glyph stroke - one step darker than the fill.
const COLORREF kIconDiamondStroke  = RGB( 40,  80, 160);

// ---- Layout constants -------------------------------------------------------
// Corner radius (px) for the rounded-rect background of buttons without a
// 9-grid bitmap. 8px matches the project-wide button radius from CLAUDE.md.
const int kCornerRadiusPx = 8;
// Inner glyph square edge (px) for radio / checkbox / icon - 16x16 reads
// well at the default 9pt font without dwarfing the label.
const int kGlyphSizePx    = 16;
// Padding (px) on each side of the glyph; also between glyph and label.
const int kGlyphPadPx     = 6;
// Stroke width (px) for the anti-aliased check tick - matches GDI+ pen widths.
const float kCheckTickStrokePx = 2.0f;
// Inset (px) used to inflate the focus rectangle inward from the button rect.
const int kFocusRectInsetPx    = 3;

} // anonymous namespace

// Forward decl at namespace scope so the call site below resolves the
// symbol unambiguously to balloonwjui::DuiButton_UncheckRadioPeers.
// (A block-scope `extern` inside the function body is mangled inconsistently
// across MSVC versions and was producing a global-namespace LNK2019.)
void DuiButton_UncheckRadioPeers(DuiControl* parent, DuiButton* self, int group);

DuiButton::DuiButton()
{
    SetTabStop(true);
}

void DuiButton::SetButtonType(Style s)
{
    if (m_style == s)
    {
        return;
    }
    m_style = s;
    Invalidate();
}

void DuiButton::SetBgBitmap(HBITMAP normal, HBITMAP hover,
                            HBITMAP pressed, HBITMAP disabled)
{
    m_hBgNormal   = normal;
    m_hBgHover    = hover;
    m_hBgPressed  = pressed;
    m_hBgDisabled = disabled;
    Invalidate();
}

void DuiButton::SetBgInsets(int left, int top, int right, int bottom)
{
    m_bgInsets.left   = left   < 0 ? 0 : left;
    m_bgInsets.top    = top    < 0 ? 0 : top;
    m_bgInsets.right  = right  < 0 ? 0 : right;
    m_bgInsets.bottom = bottom < 0 ? 0 : bottom;
    Invalidate();
}

HBITMAP DuiButton::GetBgBitmapForCurrentState() const
{
    if (!m_bEnabled)
    {
        return m_hBgDisabled ? m_hBgDisabled : m_hBgNormal;
    }
    if (m_pressed && m_bHover)
    {
        return m_hBgPressed ? m_hBgPressed
             : m_hBgHover   ? m_hBgHover
                            : m_hBgNormal;
    }
    if (m_pressed)
    {
        return m_hBgHover ? m_hBgHover : m_hBgNormal;
    }
    if (m_bHover)
    {
        return m_hBgHover ? m_hBgHover : m_hBgNormal;
    }
    return m_hBgNormal;
}

void DuiButton::SetText(LPCTSTR sz)
{
    m_text = sz ? sz : _T("");
    Invalidate();
}

TCHAR DuiButton::GetMnemonicChar() const
{
    return DuiMnemonic::FindChar(m_text);
}

void DuiButton::SetCheck(bool checked, bool notify)
{
    if (m_checked == checked)
    {
        return;
    }
    m_checked = checked;
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, m_checked ? 1 : 0);
    }
}

void DuiButton::DoToggle()
{
    if (m_style == StyleCheckbox)
    {
        SetCheck(!m_checked);
    }
    else if (m_style == StyleRadio)
    {
        if (m_checked)
        {
            return;     // clicking an already-checked radio is no-op
        }
        UncheckRadioPeers();
        SetCheck(true);
    }
}

void DuiButton::UncheckRadioPeers()
{
    DuiControl* parent = m_pParent;
    if (!parent || m_radioGroup == 0)
    {
        return;
    }

    // m_pParent->m_children is protected; we're not a friend. Walk indirectly:
    // we use HitTest of parent? No - we need to enumerate. Easiest path:
    // iterate via the parent's children indirectly by asking each sibling
    // through... Actually we don't have an enumerator. Add a tiny helper:
    // walk by issuing a downward HitTest sweep would be wrong.
    //
    // Pragmatic: since m_pParent is a DuiControl, and DuiControl's m_children
    // is `protected`, we can't reach it from a non-friend. Add a helper
    // on the parent? That changes DuiControl. Instead, mark DuiButton as
    // a friend of DuiControl... or expose a const enumerator.
    //
    // To keep DuiControl clean and avoid header churn, we walk via a
    // nested helper class that befriends DuiControl. See bottom of file.
    DuiButton_UncheckRadioPeers(parent, this, m_radioGroup);
}

// Brand palette for StylePushButton:
//   normal   #2D6CDF   (45, 108, 223)
//   hover    #2559B8   (37,  89, 184)   - darker per user spec
//   pressed  #1E4A99   (30,  74, 153)   - one step darker than hover
//   border   #1E4A99
// Other styles (Checkbox / Radio / Icon) keep a light background so
// their glyphs remain visible.
COLORREF DuiButton::FillColor() const
{
    if (m_style == StylePushButton)
    {
        if (!m_bEnabled)
        {
            return kPushButtonFillDisabled;
        }
        if (m_pressed && m_bHover)
        {
            return kPushButtonFillPressed;
        }
        if (m_pressed)
        {
            return kPushButtonFillHover;   // dragged-out, fall back to hover
        }
        if (m_bHover)
        {
            return kPushButtonFillHover;
        }
        return kPushButtonFillNormal;
    }

    if (!m_bEnabled)
    {
        return kLightFillDisabled;
    }
    if (m_pressed && m_bHover)
    {
        return kLightFillPressedHover;
    }
    if (m_pressed)
    {
        return kLightFillPressedOnly;
    }
    if (m_bHover)
    {
        return kLightFillHover;
    }
    if (m_bFocused)
    {
        return kLightFillFocused;
    }
    return kLightFillIdle;
}

COLORREF DuiButton::BorderColor() const
{
    if (m_style == StylePushButton)
    {
        if (!m_bEnabled)
        {
            return kPushButtonBorderDisabled;
        }
        return kPushButtonBorder;
    }

    if (!m_bEnabled)
    {
        return kLightBorderDisabled;
    }
    if (m_bFocused)
    {
        return kLightBorderFocused;
    }
    if (m_bHover)
    {
        return kLightBorderHover;
    }
    return kLightBorderIdle;
}

void DuiButton::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    // Background path: if a state bitmap is configured, paint via 9-grid
    // so corners stay crisp at any size; otherwise fall back to the
    // brand-color RoundRect path so plain (un-skinned) buttons still look
    // right.
    HBITMAP hbmBg = GetBgBitmapForCurrentState();
    if (hbmBg)
    {
        DuiNinePatch::Options opts;
        opts.hasAlpha = true;   // skin assets are typically PNG with alpha
        DuiNinePatch::Draw(hdc, hbmBg, m_rcItem, m_bgInsets, opts);
    }
    else
    {
        // Rounded background + border drawn in one RoundRect call so the
        // fill and stroke share the same rounded geometry (no square fill
        // peeking out of the rounded border at the corners).
        HBRUSH hbr  = ::CreateSolidBrush(FillColor());
        HPEN   hpen = ::CreatePen(PS_SOLID, 1, BorderColor());
        HBRUSH oldBr  = (HBRUSH)::SelectObject(hdc, hbr);
        HPEN   oldPen = (HPEN)  ::SelectObject(hdc, hpen);
        ::RoundRect(hdc, m_rcItem.left, m_rcItem.top,
                         m_rcItem.right, m_rcItem.bottom,
                         kCornerRadiusPx, kCornerRadiusPx);
        ::SelectObject(hdc, oldBr);
        ::SelectObject(hdc, oldPen);
        ::DeleteObject(hbr);
        ::DeleteObject(hpen);
    }

    // Compute text rect; reserve a left strip for checkbox / radio / icon glyph.
    RECT rcText = m_rcItem;
    if (m_style == StyleCheckbox || m_style == StyleRadio || m_style == StyleIcon)
    {
        RECT rcGlyph = { m_rcItem.left + kGlyphPadPx,
                         (m_rcItem.top + m_rcItem.bottom - kGlyphSizePx) / 2,
                         m_rcItem.left + kGlyphPadPx + kGlyphSizePx,
                         (m_rcItem.top + m_rcItem.bottom - kGlyphSizePx) / 2 + kGlyphSizePx };

        // Draw the glyph (anti-aliased: ellipses, polygons, diagonal lines).
        if (m_style == StyleRadio)
        {
            DuiAA::FillEllipse(hdc, rcGlyph, kGlyphInteriorWhite, BorderColor());
            if (m_checked)
            {
                RECT rcDot = { rcGlyph.left + 4, rcGlyph.top + 4,
                               rcGlyph.right - 4, rcGlyph.bottom - 4 };
                DuiAA::FillEllipse(hdc, rcDot, kRadioDotBlue, CLR_INVALID);
            }
        }
        else if (m_style == StyleCheckbox)
        {
            // Outer rect is axis-aligned -> plain GDI is fine.
            HBRUSH glyphBg = ::CreateSolidBrush(kGlyphInteriorWhite);
            HPEN pen2 = ::CreatePen(PS_SOLID, 1, BorderColor());
            HPEN op2  = (HPEN)::SelectObject(hdc, pen2);
            HBRUSH ob = (HBRUSH)::SelectObject(hdc, glyphBg);
            ::Rectangle(hdc, rcGlyph.left, rcGlyph.top, rcGlyph.right, rcGlyph.bottom);
            ::SelectObject(hdc, ob);
            ::SelectObject(hdc, op2);
            ::DeleteObject(pen2);
            ::DeleteObject(glyphBg);
            if (m_checked)
            {
                // Two anti-aliased diagonal segments form the tick.
                DuiAA::DrawLine(hdc, rcGlyph.left + 3,  rcGlyph.top    + 8,
                                     rcGlyph.left + 7,  rcGlyph.bottom - 3, kCheckTickGreen, kCheckTickStrokePx);
                DuiAA::DrawLine(hdc, rcGlyph.left + 7,  rcGlyph.bottom - 3,
                                     rcGlyph.right - 3, rcGlyph.top    + 4, kCheckTickGreen, kCheckTickStrokePx);
            }
        }
        else // StyleIcon
        {
            // Filled diamond as a placeholder icon.
            POINT pts[4] = {
                { (rcGlyph.left + rcGlyph.right) / 2, rcGlyph.top },
                { rcGlyph.right, (rcGlyph.top + rcGlyph.bottom) / 2 },
                { (rcGlyph.left + rcGlyph.right) / 2, rcGlyph.bottom },
                { rcGlyph.left,  (rcGlyph.top + rcGlyph.bottom) / 2 }
            };
            DuiAA::FillPolygon(hdc, pts, 4, kIconDiamondFill, kIconDiamondStroke);
        }

        rcText.left += kGlyphPadPx + kGlyphSizePx + kGlyphPadPx;   // shift text rect past glyph
    }

    // Focus dotted border (one-pixel inset)
    if (m_bFocused)
    {
        RECT rf = m_rcItem;
        ::InflateRect(&rf, -kFocusRectInsetPx, -kFocusRectInsetPx);
        DrawFocusRect(hdc, &rf);
    }

    // Label
    if (!m_text.IsEmpty())
    {
        HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
        HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        // PushButton uses the brand-blue background, so its label must be
        // white (or pale gray when disabled) for legibility. Other styles
        // sit on a light fill and use the caller-controlled m_clrText.
        COLORREF txt;
        if (m_style == StylePushButton)
        {
            txt = m_bEnabled ? kPushButtonLabelEnabled : kPushButtonLabelDisabled;
        }
        else
        {
            txt = m_bEnabled ? m_clrText : kLightLabelDisabled;
        }
        COLORREF oldClr = ::SetTextColor(hdc, txt);
        DWORD dt = m_dtFlags;
        // For Checkbox/Radio/Icon labels are easier read left-aligned.
        if (m_style != StylePushButton)
        {
            dt = (dt & ~(DT_CENTER | DT_RIGHT)) | DT_LEFT;
        }
        ::DrawText(hdc, m_text, -1, &rcText, dt);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        if (oldFont)
        {
            ::SelectObject(hdc, oldFont);
        }
    }
}

bool DuiButton::OnLButtonDown(POINT /*pt*/, UINT /*mkFlags*/)
{
    if (!m_bEnabled)
    {
        return true;
    }
    m_pressed = true;
    Capture();
    Invalidate();
    return true;
}

bool DuiButton::OnLButtonUp(POINT pt, UINT /*mkFlags*/)
{
    bool wasPressed = m_pressed;
    m_pressed = false;
    if (m_bCapture)
    {
        ReleaseCapture();
    }
    Invalidate();

    bool insideAtRelease = ::PtInRect(&m_rcItem, pt) != FALSE;
    if (m_bEnabled && wasPressed && insideAtRelease)
    {
        DoToggle();                          // no-op for PushButton/Icon
        NotifyParent(DUIN_CLICK);
    }
    return true;
}

bool DuiButton::OnSetCursor(POINT)
{
    if (m_bEnabled)
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    }
    return true;
}

void DuiButton::DebugSetPressed(bool b)
{
    if (m_pressed == b)
    {
        return;
    }
    m_pressed = b;
    Invalidate();
}

// ----- helper that needs DuiControl::m_children access ---------------------
// DuiControl makes m_children protected; befriending DuiButton in DuiControl
// would create a header dependency cycle. Instead, define a tiny subclass
// that grants access via inheritance.

class DuiControl_Walker : public DuiControl
{
public:
    template <typename Visitor>
    static void ForEachChild(DuiControl* parent, Visitor v)
    {
        if (!parent)
        {
            return;
        }
        // Cast to walker subclass to read protected m_children.
        DuiControl_Walker* w = static_cast<DuiControl_Walker*>(parent);
        for (auto& up : w->m_children)
        {
            v(up.get());
        }
    }
};

void DuiButton_UncheckRadioPeers(DuiControl* parent, DuiButton* self, int group)
{
    DuiControl_Walker::ForEachChild(parent, [&](DuiControl* sib) {
        if (sib == self)
        {
            return;
        }
        DuiButton* btn = dynamic_cast<DuiButton*>(sib);
        if (!btn)
        {
            return;
        }
        if (btn->GetButtonType() != DuiButton::StyleRadio)
        {
            return;
        }
        if (btn->GetRadioGroup() != group)
        {
            return;
        }
        btn->SetCheck(false, /*notify=*/true);
    });
}

} // namespace balloonwjui

#endif // BUI_FEATURE_BUTTON
