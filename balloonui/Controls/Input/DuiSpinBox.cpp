#include "stdafx.h"
#include "DuiSpinBox.h"

#if BUI_FEATURE_SPINBOX

#include "../../DuiPaintAA.h"

namespace balloonwjui {

DuiSpinBox::DuiSpinBox()
{
    SetTabStop(false);

    auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    m_edit = e.get();
    AddChild(std::move(e));
    PushValueToEdit();
}

bool DuiSpinBox::EnsureCreated(HWND hwndParent)
{
    if (!m_edit)
    {
        return false;
    }
    bool ok = m_edit->EnsureCreated(hwndParent);
    if (ok)
    {
        PushValueToEdit();
    }
    return ok;
}

void DuiSpinBox::SetRange(int minV, int maxV)
{
    if (minV > maxV)
    {
        int t = minV;
        minV = maxV;
        maxV = t;
    }
    m_min = minV;
    m_max = maxV;
    int newV = ClampOrWrap(m_value, m_min, m_max, false);
    if (newV != m_value)
    {
        m_value = newV;
        PushValueToEdit();
    }
}

void DuiSpinBox::SetStep(int s)
{
    if (s < 1)
    {
        s = 1;
    }
    m_step = s;
}

void DuiSpinBox::SetValue(int v, bool notify)
{
    int nv = ClampOrWrap(v, m_min, m_max, m_wrap);
    if (nv == m_value)
    {
        return;
    }
    m_value = nv;
    PushValueToEdit();
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_value);
    }
}

void DuiSpinBox::SetSpinStripWidth(int px)
{
    if (px < 12)
    {
        px = 12;
    }
    if (m_spinW == px)
    {
        return;
    }
    m_spinW = px;
    Layout(m_rcItem);
    Invalidate();
}

int DuiSpinBox::ClampOrWrap(int v, int minV, int maxV, bool wrap)
{
    if (minV > maxV)
    {
        int t = minV;
        minV = maxV;
        maxV = t;
    }
    int range = maxV - minV + 1;
    if (range <= 0)
    {
        return minV;
    }
    if (!wrap)
    {
        if (v < minV)
        {
            return minV;
        }
        if (v > maxV)
        {
            return maxV;
        }
        return v;
    }
    // Wrap: bring into [min, max] modulo range, handling negative offset.
    int off = (v - minV) % range;
    if (off < 0)
    {
        off += range;
    }
    return minV + off;
}

void DuiSpinBox::StepBy(int delta)
{
    SetValue(m_value + delta, /*notify=*/true);
}

void DuiSpinBox::PushValueToEdit()
{
    if (!m_edit)
    {
        return;
    }
    CString s;
    s.Format(_T("%d"), m_value);
    m_edit->SetText(s);
}

RECT DuiSpinBox::GetUpRect() const
{
    RECT r = { 0, 0, 0, 0 };
    if (m_spinW <= 0)
    {
        return r;
    }
    r.right  = m_rcItem.right;
    r.left   = r.right - m_spinW;
    r.top    = m_rcItem.top;
    r.bottom = (m_rcItem.top + m_rcItem.bottom) / 2;
    return r;
}

RECT DuiSpinBox::GetDownRect() const
{
    RECT r = { 0, 0, 0, 0 };
    if (m_spinW <= 0)
    {
        return r;
    }
    r.right  = m_rcItem.right;
    r.left   = r.right - m_spinW;
    r.top    = (m_rcItem.top + m_rcItem.bottom) / 2;
    r.bottom = m_rcItem.bottom;
    return r;
}

void DuiSpinBox::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    if (!m_edit)
    {
        return;
    }
    RECT er = m_rcItem;
    er.right -= m_spinW;
    if (er.right < er.left)
    {
        er.right = er.left;
    }
    m_edit->SetRect(er);
}

void DuiSpinBox::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }

    if (m_edit && m_edit->IsVisible())
    {
        RECT inter;
        if (::IntersectRect(&inter, &m_edit->GetRect(), &rcDirty))
        {
            m_edit->OnPaint(hdc, inter);
        }
    }

    // Background fill for the spin strip.
    RECT strip;
    strip.right  = m_rcItem.right;
    strip.left   = m_rcItem.right - m_spinW;
    strip.top    = m_rcItem.top;
    strip.bottom = m_rcItem.bottom;

    HBRUSH bg = ::CreateSolidBrush(RGB(245, 246, 250));
    ::FillRect(hdc, &strip, bg);
    ::DeleteObject(bg);

    // 1px separator on the left edge of the strip.
    HPEN pen = ::CreatePen(PS_SOLID, 1, RGB(220, 220, 224));
    HPEN op  = (HPEN)::SelectObject(hdc, pen);
    ::MoveToEx(hdc, strip.left, strip.top, nullptr);
    ::LineTo  (hdc, strip.left, strip.bottom);
    ::SelectObject(hdc, op);
    ::DeleteObject(pen);

    // Up arrow ▲ in the top half, Down arrow ▼ in the bottom half.
    auto drawArrow = [&](const RECT& r, bool up, bool pressed)
    {
        int cx = (r.left + r.right) / 2;
        int cy = (r.top  + r.bottom) / 2;
        int s = 4;
        POINT pts[3];
        if (up)
        {
            pts[0] = { cx - s, cy + s/2 };
            pts[1] = { cx + s, cy + s/2 };
            pts[2] = { cx,     cy - s/2 };
        }
        else
        {
            pts[0] = { cx - s, cy - s/2 };
            pts[1] = { cx + s, cy - s/2 };
            pts[2] = { cx,     cy + s/2 };
        }
        DuiAA::FillPolygon(hdc, pts, 3,
                           pressed ? RGB(45, 108, 223) : RGB(80, 80, 100),
                           CLR_INVALID);
    };

    drawArrow(GetUpRect(),   true,  m_pressZone == 1);
    drawArrow(GetDownRect(), false, m_pressZone == 2);
}

bool DuiSpinBox::OnLButtonDown(POINT pt, UINT)
{
    if (!m_bEnabled)
    {
        return false;
    }
    if (::PtInRect(&GetUpRect(), pt))
    {
        m_pressZone = 1;
        Capture();
        Invalidate();
        return true;
    }
    if (::PtInRect(&GetDownRect(), pt))
    {
        m_pressZone = 2;
        Capture();
        Invalidate();
        return true;
    }
    return false;
}

bool DuiSpinBox::OnLButtonUp(POINT pt, UINT)
{
    int z = m_pressZone;
    m_pressZone = 0;
    if (m_bCapture)
    {
        ReleaseCapture();
    }
    Invalidate();
    if (z == 1 && ::PtInRect(&GetUpRect(), pt))
    {
        StepBy(+m_step);
        return true;
    }
    if (z == 2 && ::PtInRect(&GetDownRect(), pt))
    {
        StepBy(-m_step);
        return true;
    }
    return false;
}

DuiControl* DuiSpinBox::HitTest(POINT pt)
{
    if (!m_bVisible || !m_bEnabled)
    {
        return nullptr;
    }
    if (!::PtInRect(&m_rcItem, pt))
    {
        return nullptr;
    }

    // Spin strip clicks consumed by us; otherwise edit handles the
    // click for IME / caret.
    if (::PtInRect(&GetUpRect(), pt))
    {
        return this;
    }
    if (::PtInRect(&GetDownRect(), pt))
    {
        return this;
    }
    if (m_edit)
    {
        if (DuiControl* hit = m_edit->HitTest(pt))
        {
            return hit;
        }
    }
    return this;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SPINBOX
