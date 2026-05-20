#include "stdafx.h"
#include "DuiSlider.h"

#if BUI_FEATURE_SLIDER

#include "../../DuiPaintAA.h"

namespace balloonwjui {

DuiSlider::DuiSlider()
{
    SetTabStop(true);
}

void DuiSlider::SetRange(int nMin, int nMax)
{
    if (nMax < nMin)
    {
        nMax = nMin;
    }
    m_min = nMin;
    m_max = nMax;
    m_pos = ClampPos(m_pos);
    Invalidate();
}

void DuiSlider::SetPos(int pos, bool notify)
{
    int newPos = ClampPos(pos);
    if (newPos == m_pos)
    {
        return;
    }
    m_pos = newPos;
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_pos);
    }
}

int DuiSlider::ClampPos(int p) const
{
    if (p < m_min)
    {
        return m_min;
    }
    if (p > m_max)
    {
        return m_max;
    }
    return p;
}

RECT DuiSlider::TrackRect() const
{
    if (m_vertical)
    {
        int cx = (m_rcItem.left + m_rcItem.right) / 2;
        return RECT{ cx - m_trackH / 2,
                     m_rcItem.top + m_thumbR,
                     cx - m_trackH / 2 + m_trackH,
                     m_rcItem.bottom - m_thumbR };
    }
    int cy = (m_rcItem.top + m_rcItem.bottom) / 2;
    return RECT{ m_rcItem.left + m_thumbR, cy - m_trackH / 2,
                 m_rcItem.right - m_thumbR, cy - m_trackH / 2 + m_trackH };
}

POINT DuiSlider::ComputeThumbCenter() const
{
    RECT track = TrackRect();
    int range = m_max - m_min;
    if (m_vertical)
    {
        int spanH = track.bottom - track.top;
        int y = track.top;
        if (range > 0 && spanH > 0)
        {
            y += (int)(((LONGLONG)spanH * (m_pos - m_min)) / range);
        }
        int cx = (track.left + track.right) / 2;
        return POINT{ cx, y };
    }
    int spanW = track.right - track.left;
    int x = track.left;
    if (range > 0 && spanW > 0)
    {
        x += (int)(((LONGLONG)spanW * (m_pos - m_min)) / range);
    }
    int cy = (track.top + track.bottom) / 2;
    return POINT{ x, cy };
}

RECT DuiSlider::ThumbHitRect() const
{
    POINT c = ComputeThumbCenter();
    return RECT{ c.x - m_thumbR, c.y - m_thumbR, c.x + m_thumbR + 1, c.y + m_thumbR + 1 };
}

int DuiSlider::PosFromPixelX(int x) const
{
    return PosFromPoint(POINT{ x, 0 });
}

int DuiSlider::PosFromPoint(POINT pt) const
{
    RECT track = TrackRect();
    int range = m_max - m_min;
    if (m_vertical)
    {
        int spanH = track.bottom - track.top;
        if (spanH <= 0)
        {
            return m_min;
        }
        int dy = pt.y - track.top;
        if (dy < 0)
        {
            dy = 0;
        }
        if (dy > spanH)
        {
            dy = spanH;
        }
        return m_min + (int)(((LONGLONG)range * dy) / spanH);
    }
    int spanW = track.right - track.left;
    if (spanW <= 0)
    {
        return m_min;
    }
    int dx = pt.x - track.left;
    if (dx < 0)
    {
        dx = 0;
    }
    if (dx > spanW)
    {
        dx = spanW;
    }
    return m_min + (int)(((LONGLONG)range * dx) / spanW);
}

void DuiSlider::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    RECT track = TrackRect();

    // Empty track
    HBRUSH bg = ::CreateSolidBrush(m_clrTrack);
    ::FillRect(hdc, &track, bg);
    ::DeleteObject(bg);

    // Filled portion (track start --> thumb center).
    POINT thumb = ComputeThumbCenter();
    if (m_vertical)
    {
        if (thumb.y > track.top)
        {
            RECT rFill = track;
            rFill.bottom = thumb.y;
            HBRUSH fb = ::CreateSolidBrush(m_clrFill);
            ::FillRect(hdc, &rFill, fb);
            ::DeleteObject(fb);
        }
    }
    else
    {
        if (thumb.x > track.left)
        {
            RECT rFill = track;
            rFill.right = thumb.x;
            HBRUSH fb = ::CreateSolidBrush(m_clrFill);
            ::FillRect(hdc, &rFill, fb);
            ::DeleteObject(fb);
        }
    }

    // Tick marks every m_tickFreq value-units, drawn perpendicular to
    // the rail just outside the track.
    if (m_tickFreq > 0)
    {
        int range = m_max - m_min;
        if (range > 0)
        {
            HBRUSH tb = ::CreateSolidBrush(m_clrTick);
            for (int v = m_min; v <= m_max; v += m_tickFreq)
            {
                if (m_vertical)
                {
                    int spanH = track.bottom - track.top;
                    int y = track.top + (int)(((LONGLONG)spanH * (v - m_min)) / range);
                    RECT t = { track.right + 2, y, track.right + 6, y + 1 };
                    ::FillRect(hdc, &t, tb);
                }
                else
                {
                    int spanW = track.right - track.left;
                    int x = track.left + (int)(((LONGLONG)spanW * (v - m_min)) / range);
                    RECT t = { x, track.bottom + 2, x + 1, track.bottom + 6 };
                    ::FillRect(hdc, &t, tb);
                }
            }
            ::DeleteObject(tb);
        }
    }

    // Thumb circle (anti-aliased).
    RECT rcThumb = { thumb.x - m_thumbR, thumb.y - m_thumbR,
                     thumb.x + m_thumbR + 1, thumb.y + m_thumbR + 1 };
    DuiAA::FillEllipse(hdc, rcThumb, m_clrThumb, m_clrThumbBorder);

    if (m_bFocused)
    {
        RECT rf = m_rcItem;
        ::InflateRect(&rf, -2, -2);
        DrawFocusRect(hdc, &rf);
    }
}

bool DuiSlider::OnLButtonDown(POINT pt, UINT)
{
    RECT thumb = ThumbHitRect();
    if (::PtInRect(&thumb, pt))
    {
        m_dragging = true;
        Capture();
        return true;
    }
    // Click on track outside thumb -> step toward click
    int target = PosFromPoint(pt);
    if (target > m_pos)
    {
        SetPos(m_pos + m_lineSize);
    }
    else if (target < m_pos)
    {
        SetPos(m_pos - m_lineSize);
    }
    return true;
}

bool DuiSlider::OnLButtonUp(POINT, UINT)
{
    if (m_dragging)
    {
        m_dragging = false;
        ReleaseCapture();
    }
    return true;
}

bool DuiSlider::OnMouseMove(POINT pt, UINT)
{
    if (!m_dragging)
    {
        return false;
    }
    SetPos(PosFromPoint(pt));
    return true;
}

bool DuiSlider::OnMouseWheel(POINT, short zDelta, UINT)
{
    int delta = (zDelta > 0) ? m_lineSize : -m_lineSize;
    SetPos(m_pos + delta);
    return true;
}

bool DuiSlider::OnKeyDown(UINT vk, UINT)
{
    switch (vk)
    {
    case VK_LEFT:
    case VK_DOWN:
        SetPos(m_pos - m_lineSize);
        return true;
    case VK_RIGHT:
    case VK_UP:
        SetPos(m_pos + m_lineSize);
        return true;
    case VK_HOME:
        SetPos(m_min);
        return true;
    case VK_END:
        SetPos(m_max);
        return true;
    case VK_PRIOR:
        SetPos(m_pos - m_lineSize * 10);
        return true;
    case VK_NEXT:
        SetPos(m_pos + m_lineSize * 10);
        return true;
    default:
        return false;
    }
}

bool DuiSlider::OnSetCursor(POINT pt)
{
    if (m_bEnabled && ::PtInRect(&ThumbHitRect(), pt))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
        return true;
    }
    return false;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SLIDER
