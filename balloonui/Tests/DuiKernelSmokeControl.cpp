#include "stdafx.h"
#include "DuiKernelSmokeControl.h"

namespace balloonwjui {

DuiKernelSmokeControl::DuiKernelSmokeControl()
{
    SetTabStop(true);
}

void DuiKernelSmokeControl::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    COLORREF clrFill = RGB(220, 220, 220);
    if (!m_bEnabled)
    {
        clrFill = RGB(180, 180, 180);
    }
    else if (m_bCapture)
    {
        clrFill = RGB(120, 170, 230);
    }
    else if (m_bHover)
    {
        clrFill = RGB(180, 210, 240);
    }
    if (m_bFocused)
    {
        clrFill = RGB(255, 220, 130);
    }

    HBRUSH hbr = ::CreateSolidBrush(clrFill);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    HPEN hpen = ::CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    HPEN hOldPen = (HPEN)::SelectObject(hdc, hpen);
    HBRUSH hOldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    ::Rectangle(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom);
    ::SelectObject(hdc, hOldBr);
    ::SelectObject(hdc, hOldPen);
    ::DeleteObject(hpen);

    if (!m_label.IsEmpty())
    {
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        ::DrawText(hdc, m_label, -1, const_cast<RECT*>(&m_rcItem),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ::SetBkMode(hdc, oldBk);
    }
}

bool DuiKernelSmokeControl::OnLButtonDown(POINT pt, UINT mkFlags)
{
    Capture();
    Invalidate();
    DuiControl::OnLButtonDown(pt, mkFlags);
    return true;
}

bool DuiKernelSmokeControl::OnLButtonUp(POINT pt, UINT mkFlags)
{
    bool wasInside = ::PtInRect(&m_rcItem, pt) != FALSE;
    ReleaseCapture();
    Invalidate();
    if (wasInside)
    {
        ++m_clickCount;
    }
    return DuiControl::OnLButtonUp(pt, mkFlags);
}

bool DuiKernelSmokeControl::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    return true;
}

} // namespace balloonwjui
