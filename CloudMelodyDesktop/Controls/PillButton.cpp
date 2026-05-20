#include "stdafx.h"
#include "PillButton.h"
#include "PaintHelpers.h"
#include "../App/CloudMelodyTheme.h"

#include "DuiResMgr.h"

using namespace balloonwjui;

namespace cloudmelody {

// 圆角半径（px）—— pill 形状的视觉关键。设计稿里搜索框 / 主按钮都是
// "近完全胶囊"，按钮高 36 时 18 = 高度一半 → 完美胶囊；高度变了也保持
// kRadiusPill 看着是圆头矩形。
static const int kPillRadiusPx = 18;

void PillButton::SetText(LPCTSTR sz)
{
    m_text = sz ? sz : _T("");
    Invalidate();
}

void PillButton::SetStyle(Style s)
{
    m_style = s;
    Invalidate();
}

bool PillButton::OnMouseEnter()
{
    m_hover = true;
    Invalidate();
    return true;
}

bool PillButton::OnMouseLeave()
{
    m_hover   = false;
    m_pressed = false;
    Invalidate();
    return true;
}

bool PillButton::OnLButtonDown(POINT, UINT)
{
    m_pressed = true;
    Invalidate();
    return true;
}

bool PillButton::OnLButtonUp(POINT pt, UINT)
{
    bool wasPressed = m_pressed;
    m_pressed = false;
    Invalidate();
    if (wasPressed && ::PtInRect(&m_rcItem, pt))
    {
        NotifyParent(DUIN_CLICK, 0);
    }
    return true;
}

bool PillButton::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    return true;
}

void PillButton::OnPaint(HDC hdc, const RECT&)
{
    if (!m_bVisible)
    {
        return;
    }

    // 三种 style 各自的色组（fill / border / text 三个槽）
    COLORREF fill   = 0;
    COLORREF border = 0;       // 0 表示不画边
    COLORREF text   = 0;
    bool transparentFill = false;

    if (m_style == StylePrimary)
    {
        fill = m_pressed ? kColorPrimary
             : m_hover   ? kColorPrimaryHover
             :             kColorPrimary;
        text = kColorOnPrimary;
    }
    else if (m_style == StyleSecondary)
    {
        // 灰底 + 深灰字。hover 切到稍深。border 用 outline-variant（灰）。
        fill   = m_pressed ? kColorSurfaceContainerHigh
               : m_hover   ? kColorSurfaceContainer
               :             kColorSurfaceContainerLow;
        border = kColorSurfaceDim;
        text   = kColorOnSurface;
    }
    else // StyleOnDark
    {
        transparentFill = true;
        fill   = m_pressed ? RGB(0xFF, 0xFF, 0xFF)        // 反色高亮 pressed
               : m_hover   ? RGB(0xFF, 0xFF, 0xFF)
               :             RGB(0xFF, 0xFF, 0xFF);       // 实际无 fill
        // hover/pressed 时改 fill 半透白；M7 polish 暂不做 alpha
        // 这里简化：hover 时切到一个半浅色、pressed 全色
        text   = (m_pressed) ? kColorPrimary : kColorOnPrimary;
    }

    // 抗锯齿圆角 fill —— 全部走 PaintHelpers，避免 GDI region 路径上的
    // 楼梯锯齿。
    if (m_style == StyleOnDark && (m_hover || m_pressed))
    {
        PaintHelpers::FillRoundedRect(
            hdc, m_rcItem,
            m_pressed ? RGB(0xFF, 0xFF, 0xFF) : RGB(0xE8, 0xE8, 0xE8),
            kPillRadiusPx);
    }
    else if (!transparentFill)
    {
        PaintHelpers::FillRoundedRect(hdc, m_rcItem, fill, kPillRadiusPx);
    }

    // border —— Secondary / OnDark 都画 1px 抗锯齿边
    if (m_style == StyleSecondary)
    {
        PaintHelpers::DrawRoundedRectBorder(hdc, m_rcItem, border,
                                            kPillRadiusPx, 1.0f);
    }
    else if (m_style == StyleOnDark)
    {
        PaintHelpers::DrawRoundedRectBorder(hdc, m_rcItem,
                                            RGB(0xFF, 0xFF, 0xFF),
                                            kPillRadiusPx, 1.0f);
    }

    // 文字居中
    if (!m_text.IsEmpty())
    {
        HFONT hOld    = (HFONT)::SelectObject(hdc,
                            DuiResMgr::Inst().GetDefaultFont());
        int   oldBk   = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldC = ::SetTextColor(hdc, text);
        ::DrawText(hdc, (LPCTSTR)m_text, -1,
                   const_cast<RECT*>(&m_rcItem),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOld);
    }
}

} // namespace cloudmelody
