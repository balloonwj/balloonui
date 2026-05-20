#include "stdafx.h"
#include "DuiFocusVisual.h"
#include "DuiTheme.h"

namespace balloonwjui {

namespace DuiFocus {

void DrawRing(HDC hdc, const RECT& rc, COLORREF color, int thickness, bool withInset)
{
    if (!hdc)
    {
        return;
    }
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }
    if (thickness < 1)
    {
        thickness = 1;
    }

    HPEN pen   = ::CreatePen(PS_SOLID, thickness, color);
    HPEN oldP  = (HPEN)::SelectObject(hdc, pen);
    HBRUSH oldB = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    ::Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    ::SelectObject(hdc, oldP);
    ::DeleteObject(pen);

    if (withInset)
    {
        // 1px white inset, one px inside the outer ring. Skip when the
        // rect is too small for the inset to stay visible.
        RECT inner = rc;
        ::InflateRect(&inner, -thickness, -thickness);
        if (inner.right > inner.left && inner.bottom > inner.top)
        {
            HPEN ipen = ::CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
            HPEN op2  = (HPEN)::SelectObject(hdc, ipen);
            ::Rectangle(hdc, inner.left, inner.top, inner.right, inner.bottom);
            ::SelectObject(hdc, op2);
            ::DeleteObject(ipen);
        }
    }
    ::SelectObject(hdc, oldB);
}

void DrawRingThemed(HDC hdc, const RECT& rc, int thickness, bool withInset)
{
    DrawRing(hdc, rc, DuiTheme::Inst().Get(DuiTheme::BrandPrimary),
             thickness, withInset);
}

} // namespace DuiFocus

} // namespace balloonwjui
