#include "stdafx.h"
#include "DuiButton.h"

#if BUI_FEATURE_BUTTON

#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"
#include "../../DuiNinePatch.h"
#include "../../DuiMnemonic.h"

// LeadingIcon 用 ::AlphaBlend 画 32bpp 预乘 alpha 位图(admin 的
// IconRenderer 生成的就是这种格式)。AlphaBlend 在 msimg32.lib。
#pragma comment(lib, "msimg32.lib")

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

namespace {

// 判断给定 Variant 是否属于"透明"族(Ghost / Outlined / Text)。
// 这三种 Variant 的 Normal 态 bg = CLR_INVALID, 在 Checkbox / Radio 上
// 用来达到"无边框无背景"样式(Q4=A2 决定:仅这三种 Variant 在
// Checkbox / Radio 上接管色板, Primary / Default / Danger 仍走 kLight*)。
bool IsTransparentVariant(DuiButton::Variant v)
{
    return v == DuiButton::Variant::Ghost
        || v == DuiButton::Variant::Outlined
        || v == DuiButton::Variant::Text;
}

} // anonymous

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

void DuiButton::SetVariant(Variant v)
{
    if (m_variant == v)
    {
        return;
    }
    m_variant = v;
    Invalidate();
}

void DuiButton::SetAntiAlias(bool on)
{
    if (m_antiAlias == on)
    {
        return;
    }
    m_antiAlias = on;
    Invalidate();
}

void DuiButton::SetFont(HFONT hFont)
{
    if (m_font == hFont)
    {
        return;
    }
    m_font = hFont;
    Invalidate();
}

void DuiButton::SetTextPointSize(int pt, bool bold)
{
    // pt <= 0 退化为默认字体(等同 SetFont(nullptr))。
    HFONT hf = (pt <= 0) ? nullptr
                         : DuiResMgr::Inst().GetFontByPointSize(pt, bold);
    SetFont(hf);
}

void DuiButton::SetLeadingIcon(HBITMAP hBmp)
{
    if (m_leadingIcon == hBmp)
    {
        return;
    }
    m_leadingIcon = hBmp;
    Invalidate();
}

void DuiButton::SetLeadingIconSize(int px)
{
    if (px < 1)
    {
        px = 1;     // 防御性钳:>=1 才有视觉
    }
    if (m_leadingIconSize == px)
    {
        return;
    }
    m_leadingIconSize = px;
    Invalidate();
}

void DuiButton::SetLeadingIconGap(int px)
{
    if (px < 0)
    {
        px = 0;     // gap 不能为负
    }
    if (m_leadingIconGap == px)
    {
        return;
    }
    m_leadingIconGap = px;
    Invalidate();
}

DuiButton::ButtonState DuiButton::ComputeState() const
{
    if (!m_bEnabled)
    {
        return ButtonState::Disabled;
    }
    // pressed && hover = 真正"按住"；pressed && !hover = 拖出按钮范围
    // 但还按着鼠标 —— 复用 Pressed 视觉给用户视觉确认（与历史 FillColor
    // 中 "m_pressed && !m_bHover -> hover" 路径行为一致；这里更明确地
    // 落到 Pressed 上以让 palette 看起来"按下"）。
    if (m_pressed)
    {
        return m_bHover ? ButtonState::Pressed : ButtonState::Hover;
    }
    if (m_bHover)
    {
        return ButtonState::Hover;
    }
    return ButtonState::Normal;
}

// 视觉变体色板。无 HDC / 无全局状态依赖，可直接单测。
DuiButton::ButtonPalette DuiButton::PaletteFor(Variant v, ButtonState s)
{
    // 简写宏，减少重复键入。
    #define BTN_P(_bg, _text, _border) \
        ButtonPalette{ (_bg), (_text), (_border) }

    switch (v)
    {
    case Variant::Primary:
        // 品牌主色实心 + 白字；hover/pressed 渐深；disabled 灰底浅字。
        switch (s)
        {
        case ButtonState::Normal:
            return BTN_P(RGB( 45,108,223), RGB(255,255,255), CLR_INVALID);
        case ButtonState::Hover:
            return BTN_P(RGB( 37, 89,184), RGB(255,255,255), CLR_INVALID);
        case ButtonState::Pressed:
            return BTN_P(RGB( 30, 74,153), RGB(255,255,255), CLR_INVALID);
        case ButtonState::Disabled:
            return BTN_P(RGB(180,180,180), RGB(220,220,220), CLR_INVALID);
        }
        break;
    case Variant::Default:
        // 白底 + 1px 灰边 + 深字；hover 微微变灰底。
        switch (s)
        {
        case ButtonState::Normal:
            return BTN_P(RGB(255,255,255), RGB( 80, 88,102), RGB(208,212,219));
        case ButtonState::Hover:
            return BTN_P(RGB(246,247,249), RGB( 80, 88,102), RGB(208,212,219));
        case ButtonState::Pressed:
            return BTN_P(RGB(235,238,242), RGB( 60, 68, 82), RGB(192,196,205));
        case ButtonState::Disabled:
            return BTN_P(RGB(248,248,248), RGB(160,160,160), RGB(224,224,224));
        }
        break;
    case Variant::Outlined:
        // 透明底 + 1px 品牌色边 + 品牌色字；hover 加浅品牌底。
        switch (s)
        {
        case ButtonState::Normal:
            return BTN_P(CLR_INVALID,      RGB( 45,108,223), RGB( 45,108,223));
        case ButtonState::Hover:
            return BTN_P(RGB(232,240,253), RGB( 45,108,223), RGB( 45,108,223));
        case ButtonState::Pressed:
            return BTN_P(RGB(214,228,250), RGB( 30, 74,153), RGB( 30, 74,153));
        case ButtonState::Disabled:
            return BTN_P(CLR_INVALID,      RGB(160,170,200), RGB(200,210,225));
        }
        break;
    case Variant::Ghost:
        // 透明 + 深字；hover 浅灰底（无边）。
        switch (s)
        {
        case ButtonState::Normal:
            return BTN_P(CLR_INVALID,      RGB(100,108,120), CLR_INVALID);
        case ButtonState::Hover:
            return BTN_P(RGB(240,242,246), RGB( 80, 88,102), CLR_INVALID);
        case ButtonState::Pressed:
            return BTN_P(RGB(225,228,234), RGB( 60, 68, 82), CLR_INVALID);
        case ButtonState::Disabled:
            return BTN_P(CLR_INVALID,      RGB(160,170,180), CLR_INVALID);
        }
        break;
    case Variant::Danger:
        // 红底 + 白字；hover/pressed 渐深；disabled 浅红底浅字。
        switch (s)
        {
        case ButtonState::Normal:
            return BTN_P(RGB(220, 60, 60), RGB(255,255,255), CLR_INVALID);
        case ButtonState::Hover:
            return BTN_P(RGB(184, 42, 34), RGB(255,255,255), CLR_INVALID);
        case ButtonState::Pressed:
            return BTN_P(RGB(153, 30, 22), RGB(255,255,255), CLR_INVALID);
        case ButtonState::Disabled:
            return BTN_P(RGB(232,180,180), RGB(245,245,245), CLR_INVALID);
        }
        break;
    case Variant::Text:
        // 无底无边 + 品牌字；hover 浅底（与 Outlined hover 同色）。
        switch (s)
        {
        case ButtonState::Normal:
            return BTN_P(CLR_INVALID,      RGB( 45,108,223), CLR_INVALID);
        case ButtonState::Hover:
            return BTN_P(RGB(232,240,253), RGB( 37, 89,184), CLR_INVALID);
        case ButtonState::Pressed:
            return BTN_P(RGB(214,228,250), RGB( 30, 74,153), CLR_INVALID);
        case ButtonState::Disabled:
            return BTN_P(CLR_INVALID,      RGB(160,170,200), CLR_INVALID);
        }
        break;
    }
    #undef BTN_P
    // 兜底（不可达，仅防 enum 漂移）：Primary Normal。
    return ButtonPalette{ RGB(45, 108, 223), RGB(255, 255, 255), CLR_INVALID };
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
        // PushButton 走 Variant palette；可能返回 CLR_INVALID（Ghost / Outlined
        // /Text 等透明变体）—— OnPaint 会跳过填充。
        return PaletteFor(m_variant, ComputeState()).bg;
    }

    // Checkbox / Radio 接受三个透明 Variant(Ghost / Outlined / Text);
    // 其余 Variant(Primary / Default / Danger) 兜底回 kLight* 家族。
    // Icon 仍只走 kLight*(用户当前只要求 Checkbox / Radio 支持无背景)。
    if ((m_style == StyleCheckbox || m_style == StyleRadio)
        && IsTransparentVariant(m_variant))
    {
        return PaletteFor(m_variant, ComputeState()).bg;
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
        // PushButton 走 Variant palette；CLR_INVALID 表示不画描边
        // （Primary / Danger / Ghost / Text 多数状态都没边框）。
        return PaletteFor(m_variant, ComputeState()).border;
    }

    // Checkbox / Radio 三个透明 Variant 接管 border 色:
    //   Ghost / Text → CLR_INVALID(无边)
    //   Outlined     → 品牌色边
    // 影响范围:外框 + glyph(方框 / 圆环)的描边一并跟随。
    if ((m_style == StyleCheckbox || m_style == StyleRadio)
        && IsTransparentVariant(m_variant))
    {
        return PaletteFor(m_variant, ComputeState()).border;
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

COLORREF DuiButton::GlyphBorderColor() const
{
    // 与 Variant 完全无关;给透明 Variant 下的 glyph 一个可见的边界。
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

COLORREF DuiButton::GlyphInteriorColor() const
{
    if ((m_style == StyleCheckbox || m_style == StyleRadio)
        && IsTransparentVariant(m_variant))
    {
        return CLR_INVALID;
    }
    return kGlyphInteriorWhite;
}

bool DuiButton::ShouldDrawFocusRect(Style style, bool focused)
{
    if (!focused)
    {
        return false;
    }
    // Checkbox / Radio 已用勾 / 圆点表态，点状焦点框冗余，一律不画。
    if (style == StyleCheckbox || style == StyleRadio)
    {
        return false;
    }
    return true;
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
        // Rounded background + border. 两条路径:
        //   AA 路径(m_antiAlias, 默认): 走 DuiAA::FillRoundRect, GDI+ 抗锯齿,
        //       8px 圆角丝滑无阶梯。
        //   GDI 路径(m_antiAlias=false): 走 ::RoundRect, 保留兜底以便对比 /
        //       极致性能场景。
        //
        // CLR_INVALID 语义(PushButton + Ghost / Outlined / Text 等透明变体):
        //   · fill   = CLR_INVALID → 不填充;
        //   · border = CLR_INVALID → 不描边;
        //   · 两者都 CLR_INVALID → 整段背景不画(如 Text 变体常态)。
        COLORREF fill   = FillColor();
        COLORREF border = BorderColor();
        if (fill != CLR_INVALID || border != CLR_INVALID)
        {
            if (m_antiAlias)
            {
                DuiAA::FillRoundRect(hdc, m_rcItem,
                                     fill, kCornerRadiusPx, border);
            }
            else
            {
                const bool createdBr  = (fill   != CLR_INVALID);
                const bool createdPen = (border != CLR_INVALID);
                HBRUSH hbr  = createdBr  ? ::CreateSolidBrush(fill)
                                         : (HBRUSH)::GetStockObject(HOLLOW_BRUSH);
                HPEN   hpen = createdPen ? ::CreatePen(PS_SOLID, 1, border)
                                         : (HPEN)  ::GetStockObject(NULL_PEN);
                HBRUSH oldBr  = (HBRUSH)::SelectObject(hdc, hbr);
                HPEN   oldPen = (HPEN)  ::SelectObject(hdc, hpen);
                ::RoundRect(hdc, m_rcItem.left, m_rcItem.top,
                                 m_rcItem.right, m_rcItem.bottom,
                                 kCornerRadiusPx, kCornerRadiusPx);
                ::SelectObject(hdc, oldBr);
                ::SelectObject(hdc, oldPen);
                // STOCK 对象不允许 DeleteObject —— 只删自己 create 的。
                if (createdBr)
                {
                    ::DeleteObject(hbr);
                }
                if (createdPen)
                {
                    ::DeleteObject(hpen);
                }
            }
        }
    }

    // Compute text rect; reserve a left strip for checkbox / radio / icon glyph.
    RECT rcText = m_rcItem;
    if (m_style == StyleCheckbox || m_style == StyleRadio || m_style == StyleIcon)
    {
        RECT rcGlyph = { m_rcItem.left + kGlyphPadPx,
                         (m_rcItem.top + m_rcItem.bottom - kGlyphSizePx) / 2,
                         m_rcItem.left + kGlyphPadPx + kGlyphSizePx,
                         (m_rcItem.top + m_rcItem.bottom - kGlyphSizePx) / 2 + kGlyphSizePx };

        // Glyph 公用的内 / 描边色:
        //   · 描边: 始终走 kLight* 家族(GlyphBorderColor), 透明 Variant 下
        //     仍要看得到 glyph 边框。
        //   · 内部: 默认白; 透明 Variant 下(Checkbox/Radio) 走 CLR_INVALID
        //     呈"空心"形态(Q5=B 决定)。
        COLORREF glyphFill   = GlyphInteriorColor();
        COLORREF glyphBorder = GlyphBorderColor();

        // Draw the glyph (anti-aliased: ellipses, polygons, diagonal lines).
        if (m_style == StyleRadio)
        {
            DuiAA::FillEllipse(hdc, rcGlyph, glyphFill, glyphBorder);
            if (m_checked)
            {
                RECT rcDot = { rcGlyph.left + 4, rcGlyph.top + 4,
                               rcGlyph.right - 4, rcGlyph.bottom - 4 };
                DuiAA::FillEllipse(hdc, rcDot, kRadioDotBlue, CLR_INVALID);
            }
        }
        else if (m_style == StyleCheckbox)
        {
            if (m_antiAlias)
            {
                // AA 路径:复用 FillRoundRect 但 radius=0 退化成普通矩形;
                // 在 GDI+ 中走 FillRectangle / DrawRectangle, 与 GDI
                // ::Rectangle 视觉一致但边线更干净。
                DuiAA::FillRoundRect(hdc, rcGlyph, glyphFill, /*radius=*/0,
                                     glyphBorder);
            }
            else
            {
                // GDI 兜底:与历史行为一致(轴对齐矩形, 锯齿不可见, 无 AA 也行)。
                const bool createdBr  = (glyphFill   != CLR_INVALID);
                const bool createdPen = (glyphBorder != CLR_INVALID);
                HBRUSH gb  = createdBr  ? ::CreateSolidBrush(glyphFill)
                                        : (HBRUSH)::GetStockObject(HOLLOW_BRUSH);
                HPEN   gp  = createdPen ? ::CreatePen(PS_SOLID, 1, glyphBorder)
                                        : (HPEN)  ::GetStockObject(NULL_PEN);
                HBRUSH ob  = (HBRUSH)::SelectObject(hdc, gb);
                HPEN   op2 = (HPEN)  ::SelectObject(hdc, gp);
                ::Rectangle(hdc, rcGlyph.left, rcGlyph.top,
                                 rcGlyph.right, rcGlyph.bottom);
                ::SelectObject(hdc, ob);
                ::SelectObject(hdc, op2);
                if (createdBr)
                {
                    ::DeleteObject(gb);
                }
                if (createdPen)
                {
                    ::DeleteObject(gp);
                }
            }
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

    // Focus dotted border (one-pixel inset)。Checkbox / Radio 不画 —— 它们已用
    // 勾 / 圆点表态，点状框是冗余视觉噪音（业务反馈：复选框点击后那圈虚线框碍眼）。
    if (ShouldDrawFocusRect(m_style, m_bFocused))
    {
        RECT rf = m_rcItem;
        ::InflateRect(&rf, -kFocusRectInsetPx, -kFocusRectInsetPx);
        DrawFocusRect(hdc, &rf);
    }

    // ---- Font scope + LeadingIcon + Label 三段共用 ----
    // 字体优先级:caller SetFont 设的 m_font > DuiResMgr 默认 9pt YaHei。
    // LeadingIcon 量文字宽和 Label 绘制必须共用同一字体,所以提到 if 外。
    HFONT useFont = m_font ? m_font : DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;

    // LeadingIcon —— 仅 StylePushButton 生效;设了 m_leadingIcon 时画。
    // 整组(图标 + gap + 文字)按 m_dtFlags 对齐;默认 DT_CENTER → 整组水
    // 平居中;DT_LEFT / DT_RIGHT 各从对应边算起。文字 rcText 会相应
    // 收窄到"图标右侧 + gap"开始,让后面 DrawText 接管文字渲染。
    if (m_style == StylePushButton && m_leadingIcon)
    {
        int iconSz = m_leadingIconSize;
        int gap    = m_leadingIconGap;
        // 量文字宽(共享 useFont)。空文本 → groupW 只含图标。
        int textW = 0;
        if (!m_text.IsEmpty())
        {
            SIZE sz = {};
            ::GetTextExtentPoint32(hdc, m_text, m_text.GetLength(), &sz);
            textW = sz.cx;
        }
        int groupW = iconSz + (m_text.IsEmpty() ? 0 : (gap + textW));
        int btnH   = m_rcItem.bottom - m_rcItem.top;

        // 整组按 m_dtFlags 对齐;DT_CENTER 是默认。
        // 极窄按钮场景下若 groupW 超宽则贴左边距。
        const int kEdgePadPx = 12;
        int groupLeft;
        if (m_dtFlags & DT_RIGHT)
        {
            groupLeft = m_rcItem.right - groupW - kEdgePadPx;
        }
        else if (m_dtFlags & DT_LEFT)
        {
            groupLeft = m_rcItem.left + kEdgePadPx;
        }
        else
        {
            groupLeft = (m_rcItem.left + m_rcItem.right - groupW) / 2;
        }
        if (groupLeft < m_rcItem.left + kEdgePadPx)
        {
            groupLeft = m_rcItem.left + kEdgePadPx;
        }
        int iconY = m_rcItem.top + (btnH - iconSz) / 2;

        // ::AlphaBlend 把图标位图缩放到 (iconSz, iconSz),保留预乘 alpha。
        BITMAP bi = {};
        ::GetObject(m_leadingIcon, sizeof(bi), &bi);
        int srcW = bi.bmWidth  > 0 ? bi.bmWidth  : iconSz;
        int srcH = bi.bmHeight > 0 ? bi.bmHeight : iconSz;
        HDC memDc = ::CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)::SelectObject(memDc, m_leadingIcon);
        BLENDFUNCTION bf;
        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat         = AC_SRC_ALPHA;
        ::AlphaBlend(hdc, groupLeft, iconY, iconSz, iconSz,
                     memDc, 0, 0, srcW, srcH, bf);
        ::SelectObject(memDc, oldBmp);
        ::DeleteDC(memDc);

        // 收窄 rcText 到图标右侧:文字 DT_LEFT 紧贴图标右边 + gap。
        rcText.left  = groupLeft + iconSz + gap;
        rcText.right = rcText.left + textW;   // 让 DrawText DT_CENTER/DT_LEFT 都不再二次偏移
        if (rcText.right > m_rcItem.right)
        {
            rcText.right = m_rcItem.right;
        }
    }

    // Label
    if (!m_text.IsEmpty())
    {
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        // PushButton 文字色走 Variant palette（与 fill / border 同源），
        // 确保 Primary 白字、Default 深字、Danger 白字、Ghost / Outlined /
        // Text 品牌色字 等视觉自洽。其余 Style（Checkbox / Radio / Icon）
        // 仍沿 caller 控制的 m_clrText（disabled 走 kLightLabelDisabled）。
        COLORREF txt;
        if (m_style == StylePushButton)
        {
            txt = PaletteFor(m_variant, ComputeState()).text;
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
        // PushButton + LeadingIcon:整组已居中, rcText 已收到文字精确位置,
        // 文字按 DT_LEFT 渲染避免二次偏移。
        else if (m_leadingIcon)
        {
            dt = (dt & ~(DT_CENTER | DT_RIGHT)) | DT_LEFT;
        }
        ::DrawText(hdc, m_text, -1, &rcText, dt);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
    }

    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
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
