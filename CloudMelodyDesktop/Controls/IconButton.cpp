#include "stdafx.h"
#include "IconButton.h"
#include "PaintHelpers.h"
#include "../App/CloudMelodyTheme.h"

#include "DuiResMgr.h"
#include "Controls/Feedback/DuiToolTip.h"

using namespace balloonwjui;

namespace cloudmelody {

// hover 圆角矩形的圆角半径 —— 与 SidebarNavItem 一致（kRadiusSm）。
static const int kIconBtnSquareRadius = 4;

IconButton::~IconButton()
{
    // 控件销毁前从 tooltip 管理器移除，避免 hover 时拿到野指针。
    // 未注册过的 ctrl 在 Unregister 里静默忽略，安全调用。
    DuiToolTipMgr::Inst().Unregister(this);
}

void IconButton::SetTooltip(LPCTSTR text)
{
    if (text && text[0])
    {
        DuiToolTipMgr::Inst().Register(this, text);
    }
    else
    {
        DuiToolTipMgr::Inst().Unregister(this);
    }
}

void IconButton::SetGlyph(LPCTSTR sz)
{
    m_glyph = sz ? sz : _T("");
    Invalidate();
}

void IconButton::SetGlyphColor(COLORREF c)
{
    m_glyphColor = c;
    Invalidate();
}

void IconButton::SetHoverShape(HoverShape s)
{
    m_shape = s;
    Invalidate();
}

bool IconButton::OnMouseEnter()
{
    m_hover = true;
    Invalidate();
    return true;
}

bool IconButton::OnMouseLeave()
{
    m_hover   = false;
    m_pressed = false;
    Invalidate();
    return true;
}

bool IconButton::OnLButtonDown(POINT, UINT)
{
    m_pressed = true;
    Invalidate();
    return true;
}

bool IconButton::OnLButtonUp(POINT pt, UINT)
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

bool IconButton::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    return true;
}

void IconButton::OnPaint(HDC hdc, const RECT&)
{
    if (!m_bVisible)
    {
        return;
    }

    // 1) hover / pressed 背景 —— idle 全透明（让父背景透出）
    if (m_hover || m_pressed)
    {
        COLORREF bg = m_pressed ? kColorSurfaceContainerHigh
                                : kColorSurfaceContainer;
        // cell 内缩 2 px，避免和邻居 hover 区贴边。
        RECT r = m_rcItem;
        ::InflateRect(&r, -2, -2);
        if (m_shape == ShapeCircle)
        {
            // 圆形：直径 = min(w, h)；居中
            int w = r.right - r.left;
            int h = r.bottom - r.top;
            int d = (w < h) ? w : h;
            int cx = (r.left + r.right) / 2;
            int cy = (r.top  + r.bottom) / 2;
            RECT rcCircle = { cx - d / 2, cy - d / 2,
                              cx + d / 2, cy + d / 2 };
            PaintHelpers::FillEllipse(hdc, rcCircle, bg);
        }
        else
        {
            PaintHelpers::FillRoundedRect(hdc, r, bg,
                                          kIconBtnSquareRadius);
        }
    }

    // 2) glyph 文字居中
    if (!m_glyph.IsEmpty())
    {
        COLORREF txt = (m_glyphColor != 0) ? m_glyphColor
                                           : kColorOnSurfaceVar;
        HFONT hOld    = (HFONT)::SelectObject(hdc,
                            DuiResMgr::Inst().GetDefaultFont());
        int   oldBk   = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldC = ::SetTextColor(hdc, txt);
        ::DrawText(hdc, (LPCTSTR)m_glyph, -1,
                   const_cast<RECT*>(&m_rcItem),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOld);
    }
}

} // namespace cloudmelody
