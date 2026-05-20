#include "stdafx.h"
#include "DuiSplitter.h"

#if BUI_FEATURE_SPLITTER


namespace balloonwjui {

DuiSplitter::DuiSplitter()
{
    SetTabStop(false);
}

void DuiSplitter::SetOrientation(Orientation o)
{
    if (m_orient == o)
    {
        return;
    }
    m_orient = o;
    Layout(m_rcItem);
    Invalidate();
}

void DuiSplitter::SetBarThickness(int px)
{
    if (px < 1)
    {
        px = 1;
    }
    if (m_barThick == px)
    {
        return;
    }
    m_barThick = px;
    Layout(m_rcItem);
    Invalidate();
}

void DuiSplitter::SetMinSizes(int min0, int min1)
{
    m_min0 = min0 < 0 ? 0 : min0;
    m_min1 = min1 < 0 ? 0 : min1;
    Layout(m_rcItem);
    Invalidate();
}

void DuiSplitter::SetSplitPx(int px0)
{
    m_splitFrac = -1.0;            // explicit px wins over fraction
    m_targetPx  = px0;             // remember intent; Layout will clamp
    Layout(m_rcItem);
    Invalidate();
}

void DuiSplitter::SetSplitFraction(double f)
{
    if (f < 0.0)
    {
        f = 0.0;
    }
    if (f > 1.0)
    {
        f = 1.0;
    }
    m_splitFrac = f;
    Layout(m_rcItem);
    Invalidate();
}

void DuiSplitter::SetPane(int idx, std::unique_ptr<DuiControl> pane)
{
    if (idx < 0 || idx > 1)
    {
        return;
    }
    if (m_pane[idx])
    {
        RemoveChild(m_pane[idx]);
        m_pane[idx] = nullptr;
    }
    if (pane)
    {
        DuiControl* raw = pane.get();
        AddChild(std::move(pane));   // sets parent + host, owns
        m_pane[idx] = raw;
    }
    Layout(m_rcItem);
    Invalidate();
}

DuiControl* DuiSplitter::GetPane(int idx) const
{
    if (idx < 0 || idx > 1)
    {
        return nullptr;
    }
    return m_pane[idx];
}

int DuiSplitter::AvailAlong() const
{
    return (m_orient == Vertical)
        ? (m_rcItem.right  - m_rcItem.left)
        : (m_rcItem.bottom - m_rcItem.top);
}

int DuiSplitter::ClampSplit(int v) const
{
    int avail = AvailAlong();
    int maxFor0 = avail - m_barThick - m_min1;
    if (maxFor0 < m_min0)
    {
        maxFor0 = m_min0;   // degenerate: rect smaller than minimums
    }
    if (v < m_min0)
    {
        v = m_min0;
    }
    if (v > maxFor0)
    {
        v = maxFor0;
    }
    return v;
}

void DuiSplitter::ApplyPendingFraction()
{
    if (m_splitFrac < 0.0)
    {
        return;
    }
    int avail = AvailAlong();
    int track = avail - m_barThick;
    // Defer fraction resolution until we have a real rect: applying it
    // against a zero-size rect would consume the user's intent without
    // ever honoring it.
    if (track <= 0)
    {
        return;
    }
    m_targetPx = (int)(track * m_splitFrac);
    m_splitFrac = -1.0;            // consumed
}

RECT DuiSplitter::GetBarRect() const
{
    RECT r = { 0, 0, 0, 0 };
    if (m_orient == Vertical)
    {
        r.left   = m_rcItem.left + m_splitPx;
        r.right  = r.left + m_barThick;
        r.top    = m_rcItem.top;
        r.bottom = m_rcItem.bottom;
    }
    else
    {
        r.left   = m_rcItem.left;
        r.right  = m_rcItem.right;
        r.top    = m_rcItem.top + m_splitPx;
        r.bottom = r.top + m_barThick;
    }
    return r;
}

bool DuiSplitter::IsPointInBar(POINT pt) const
{
    RECT bar = GetBarRect();
    return ::PtInRect(&bar, pt) != FALSE;
}

void DuiSplitter::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    ApplyPendingFraction();
    m_splitPx = ClampSplit(m_targetPx);

    // Use SetRect (not Layout) on panes so their m_rcItem actually
    // gets stored — the convention everywhere else in the DUI tree.
    if (m_orient == Vertical)
    {
        if (m_pane[0])
        {
            RECT r = m_rcItem;
            r.right = r.left + m_splitPx;
            m_pane[0]->SetRect(r);
        }
        if (m_pane[1])
        {
            RECT r = m_rcItem;
            r.left = m_rcItem.left + m_splitPx + m_barThick;
            m_pane[1]->SetRect(r);
        }
    }
    else
    {
        if (m_pane[0])
        {
            RECT r = m_rcItem;
            r.bottom = r.top + m_splitPx;
            m_pane[0]->SetRect(r);
        }
        if (m_pane[1])
        {
            RECT r = m_rcItem;
            r.top = m_rcItem.top + m_splitPx + m_barThick;
            m_pane[1]->SetRect(r);
        }
    }
}

void DuiSplitter::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }

    // Pane painters first.
    for (int i = 0; i < 2; ++i)
    {
        DuiControl* p = m_pane[i];
        if (!p || !p->IsVisible())
        {
            continue;
        }
        RECT inter;
        if (!::IntersectRect(&inter, &p->GetRect(), &rcDirty))
        {
            continue;
        }
        p->OnPaint(hdc, inter);
    }

    // Splitter bar: light fill + dotted center for the grab affordance.
    RECT bar = GetBarRect();
    RECT inter;
    if (!::IntersectRect(&inter, &bar, &rcDirty))
    {
        return;
    }

    HBRUSH hbrFill = ::CreateSolidBrush(RGB(238, 238, 240));
    ::FillRect(hdc, &inter, hbrFill);
    ::DeleteObject(hbrFill);

    HBRUSH hbrDot = ::CreateSolidBrush(RGB(170, 170, 175));
    POINT mid = {
        (bar.left + bar.right ) / 2,
        (bar.top  + bar.bottom) / 2
    };
    if (m_orient == Vertical)
    {
        for (int dy : { -10, 0, 10 })
        {
            RECT d = { mid.x - 1, mid.y + dy - 1, mid.x + 1, mid.y + dy + 1 };
            RECT clipped;
            if (::IntersectRect(&clipped, &d, &inter))
            {
                ::FillRect(hdc, &clipped, hbrDot);
            }
        }
    }
    else
    {
        for (int dx : { -10, 0, 10 })
        {
            RECT d = { mid.x + dx - 1, mid.y - 1, mid.x + dx + 1, mid.y + 1 };
            RECT clipped;
            if (::IntersectRect(&clipped, &d, &inter))
            {
                ::FillRect(hdc, &clipped, hbrDot);
            }
        }
    }
    ::DeleteObject(hbrDot);
}

bool DuiSplitter::OnLButtonDown(POINT pt, UINT /*mkFlags*/)
{
    if (!m_bEnabled)
    {
        return false;
    }
    if (!IsPointInBar(pt))
    {
        return false;
    }

    m_dragging = true;
    m_dragStartCursor = (m_orient == Vertical) ? pt.x : pt.y;
    m_dragStartSplit  = m_splitPx;
    if (!m_bCapture)
    {
        Capture();
    }
    return true;
}

bool DuiSplitter::OnMouseMove(POINT pt, UINT /*mkFlags*/)
{
    if (!m_dragging)
    {
        return false;
    }

    int cur = (m_orient == Vertical) ? pt.x : pt.y;
    int delta = cur - m_dragStartCursor;
    int newSplit = ClampSplit(m_dragStartSplit + delta);
    if (newSplit != m_splitPx)
    {
        m_targetPx = newSplit;       // remember user intent across relayouts
        m_splitFrac = -1.0;          // user dragged: ditch any pending fraction
        Layout(m_rcItem);
        Invalidate();
    }
    return true;
}

bool DuiSplitter::OnLButtonUp(POINT /*pt*/, UINT /*mkFlags*/)
{
    if (!m_dragging)
    {
        return false;
    }
    m_dragging = false;
    if (m_bCapture)
    {
        ReleaseCapture();
    }
    NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_splitPx);
    return true;
}

bool DuiSplitter::OnSetCursor(POINT pt)
{
    if (m_bEnabled && (m_dragging || IsPointInBar(pt)))
    {
        ::SetCursor(::LoadCursor(nullptr,
                                 m_orient == Vertical ? IDC_SIZEWE : IDC_SIZENS));
        return true;
    }
    return false;
}

// HitTest: while dragging, claim the point so the host keeps routing
// MouseMove/LButtonUp here. When over the bar, return self too. Otherwise
// dispatch to the appropriate pane.
DuiControl* DuiSplitter::HitTest(POINT ptHostClient)
{
    if (!m_bVisible || !m_bEnabled)
    {
        return nullptr;
    }
    if (!::PtInRect(&m_rcItem, ptHostClient))
    {
        return nullptr;
    }

    if (m_dragging || IsPointInBar(ptHostClient))
    {
        return this;
    }

    for (int i = 0; i < 2; ++i)
    {
        DuiControl* p = m_pane[i];
        if (!p)
        {
            continue;
        }
        if (DuiControl* hit = p->HitTest(ptHostClient))
        {
            return hit;
        }
    }
    return this;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SPLITTER
