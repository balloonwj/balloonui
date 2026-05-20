#include "stdafx.h"
#include "DuiDock.h"

#if BUI_FEATURE_DOCK


namespace balloonwjui {

void DuiDock::SetPadding(int l, int t, int r, int b)
{
    m_padL = l;
    m_padT = t;
    m_padR = r;
    m_padB = b;
    Invalidate();
}

void DuiDock::SetGap(int gap)
{
    m_gap = gap < 0 ? 0 : gap;
    Invalidate();
}

DuiControl* DuiDock::AddDocked(std::unique_ptr<DuiControl> child, Side side, int sizePx)
{
    if (!child)
    {
        return nullptr;
    }
    if (sizePx < 0)
    {
        sizePx = 0;
    }
    DuiControl* raw = child.get();
    AddChild(std::move(child));        // sets parent + host

    Slot s;
    s.ctrl   = raw;
    s.side   = side;
    s.sizePx = sizePx;
    m_slots.push_back(s);
    return raw;
}

int DuiDock::FindSlot(DuiControl* c) const
{
    for (size_t i = 0; i < m_slots.size(); ++i)
    {
        if (m_slots[i].ctrl == c)
        {
            return (int)i;
        }
    }
    return -1;
}

DuiDock::Side DuiDock::GetChildSide(int i) const
{
    if (i < 0 || i >= (int)m_slots.size())
    {
        return DockFill;
    }
    return m_slots[i].side;
}

int DuiDock::GetChildSize(int i) const
{
    if (i < 0 || i >= (int)m_slots.size())
    {
        return 0;
    }
    return m_slots[i].sizePx;
}

void DuiDock::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;

    RECT inner = {
        rcAvail.left   + m_padL,
        rcAvail.top    + m_padT,
        rcAvail.right  - m_padR,
        rcAvail.bottom - m_padB
    };
    if (inner.right <= inner.left || inner.bottom <= inner.top || m_slots.empty())
    {
        return;
    }

    // Walk children in declaration order, each "shrinking" the inner
    // rect by its dock strip + the configured gap. Fill children land
    // at whatever inner rect remains.
    for (size_t i = 0; i < m_slots.size(); ++i)
    {
        const Slot& s = m_slots[i];
        if (inner.right <= inner.left || inner.bottom <= inner.top)
        {
            break;
        }

        RECT r;
        switch (s.side)
        {
        case DockTop:
        {
            int h = s.sizePx;
            int avail = inner.bottom - inner.top;
            if (h > avail)
            {
                h = avail;
            }
            r.left   = inner.left;
            r.top    = inner.top;
            r.right  = inner.right;
            r.bottom = inner.top + h;
            inner.top = r.bottom + m_gap;
            break;
        }
        case DockBottom:
        {
            int h = s.sizePx;
            int avail = inner.bottom - inner.top;
            if (h > avail)
            {
                h = avail;
            }
            r.left   = inner.left;
            r.top    = inner.bottom - h;
            r.right  = inner.right;
            r.bottom = inner.bottom;
            inner.bottom = r.top - m_gap;
            break;
        }
        case DockLeft:
        {
            int w = s.sizePx;
            int avail = inner.right - inner.left;
            if (w > avail)
            {
                w = avail;
            }
            r.left   = inner.left;
            r.top    = inner.top;
            r.right  = inner.left + w;
            r.bottom = inner.bottom;
            inner.left = r.right + m_gap;
            break;
        }
        case DockRight:
        {
            int w = s.sizePx;
            int avail = inner.right - inner.left;
            if (w > avail)
            {
                w = avail;
            }
            r.left   = inner.right - w;
            r.top    = inner.top;
            r.right  = inner.right;
            r.bottom = inner.bottom;
            inner.right = r.left - m_gap;
            break;
        }
        case DockFill:
        default:
            r = inner;
            // After Fill claims the rest, the inner shrinks to empty
            // so any later docked children get an empty strip — same
            // semantics as Win32's DockStyle.Fill.
            inner.left = inner.right;
            inner.top  = inner.bottom;
            break;
        }
        s.ctrl->SetRect(r);
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_DOCK
