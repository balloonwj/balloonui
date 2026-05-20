#include "stdafx.h"
#include "DuiSeparator.h"

#if BUI_FEATURE_SEPARATOR


namespace balloonwjui {

DuiSeparator::DuiSeparator()
{
    SetTabStop(false);
}

void DuiSeparator::SetOrientation(Orientation o)
{
    if (m_orient == o)
    {
        return;
    }
    m_orient = o;
    Invalidate();
}

void DuiSeparator::SetColor(COLORREF c)
{
    if (m_color == c)
    {
        return;
    }
    m_color = c;
    Invalidate();
}

void DuiSeparator::SetThickness(int px)
{
    if (px < 1)
    {
        px = 1;
    }
    if (m_thick == px)
    {
        return;
    }
    m_thick = px;
    Invalidate();
}

void DuiSeparator::SetInset(int px)
{
    if (px < 0)
    {
        px = 0;
    }
    if (m_inset == px)
    {
        return;
    }
    m_inset = px;
    Invalidate();
}

void DuiSeparator::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    int w = m_rcItem.right  - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // Compute the line rect, centered within m_rcItem along the
    // perpendicular axis and inset along the parallel axis.
    RECT line;
    if (m_orient == Horizontal)
    {
        int cy = (m_rcItem.top + m_rcItem.bottom) / 2;
        line.left   = m_rcItem.left  + m_inset;
        line.right  = m_rcItem.right - m_inset;
        line.top    = cy - m_thick / 2;
        line.bottom = line.top + m_thick;
    }
    else
    {
        int cx = (m_rcItem.left + m_rcItem.right) / 2;
        line.top    = m_rcItem.top    + m_inset;
        line.bottom = m_rcItem.bottom - m_inset;
        line.left   = cx - m_thick / 2;
        line.right  = line.left + m_thick;
    }
    if (line.right <= line.left || line.bottom <= line.top)
    {
        return;
    }

    RECT inter;
    if (!::IntersectRect(&inter, &line, &rcDirty))
    {
        return;
    }

    HBRUSH br = ::CreateSolidBrush(m_color);
    ::FillRect(hdc, &inter, br);
    ::DeleteObject(br);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SEPARATOR
