#include "stdafx.h"
#include "DuiBadge.h"

#if BUI_FEATURE_BADGE

#include "../../DuiPaintAA.h"
#include "../../DuiResMgr.h"

namespace balloonwjui {

DuiBadge::DuiBadge()
{
    SetTabStop(false);
}

void DuiBadge::SetText(LPCTSTR text)
{
    CString t = text ? text : _T("");
    if (t.GetLength() > 4)
    {
        t = t.Left(4);
    }
    if (m_text == t)
    {
        return;
    }
    m_text = t;
    Invalidate();
}

CString DuiBadge::FormatCount(int n)
{
    if (n <= 0)
    {
        return CString();
    }
    if (n > 99)
    {
        return CString(_T("99+"));
    }
    CString s;
    s.Format(_T("%d"), n);
    return s;
}

void DuiBadge::SetCount(int n)
{
    SetText(FormatCount(n));
}

void DuiBadge::SetBgColor(COLORREF c)
{
    if (m_bg == c)
    {
        return;
    }
    m_bg = c;
    Invalidate();
}

void DuiBadge::SetTextColor(COLORREF c)
{
    if (m_fg == c)
    {
        return;
    }
    m_fg = c;
    Invalidate();
}

void DuiBadge::SetHideWhenEmpty(bool b)
{
    if (m_hideEmpty == b)
    {
        return;
    }
    m_hideEmpty = b;
    Invalidate();
}

bool DuiBadge::IsShowing() const
{
    if (!m_bVisible)
    {
        return false;
    }
    if (m_text.IsEmpty() && m_hideEmpty)
    {
        return false;
    }
    return true;
}

void DuiBadge::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!IsShowing())
    {
        return;
    }
    int w = m_rcItem.right  - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // Pill rect: vertically centered, height = min(h, 20), width = max(height, text width + 12).
    int pillH = h < 20 ? h : 20;
    int textW = 0;

    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    if (!m_text.IsEmpty())
    {
        SIZE sz = {};
        ::GetTextExtentPoint32(hdc, m_text, m_text.GetLength(), &sz);
        textW = sz.cx;
    }
    int pillW = textW + 10;
    if (pillW < pillH)
    {
        pillW = pillH;          // minimum: a circle
    }
    if (pillW > w)
    {
        pillW = w;
    }
    if (pillH > h)
    {
        pillH = h;
    }

    int cx = (m_rcItem.left + m_rcItem.right) / 2;
    int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;
    RECT pill = { cx - pillW / 2, cy - pillH / 2,
                  cx + pillW / 2, cy + pillH / 2 };

    // Use AA ellipse for circle case + AA-rounded fill via center fill +
    // two end caps for pill case. Pill rendering (>square) is two caps
    // and a center rect; circle rendering is one ellipse.
    if (pillW <= pillH + 2)
    {
        // Effectively a circle.
        DuiAA::FillEllipse(hdc, pill, m_bg, CLR_INVALID);
    }
    else
    {
        // Pill = left semicircle + right semicircle + center rect.
        int r = pillH / 2;
        // Center rect (axis-aligned, plain GDI).
        RECT mid = { pill.left + r, pill.top, pill.right - r, pill.bottom };
        HBRUSH br = ::CreateSolidBrush(m_bg);
        ::FillRect(hdc, &mid, br);
        ::DeleteObject(br);
        // End caps.
        RECT capL = { pill.left, pill.top, pill.left + r * 2, pill.bottom };
        RECT capR = { pill.right - r * 2, pill.top, pill.right, pill.bottom };
        DuiAA::FillEllipse(hdc, capL, m_bg, CLR_INVALID);
        DuiAA::FillEllipse(hdc, capR, m_bg, CLR_INVALID);
    }

    // Text.
    if (!m_text.IsEmpty())
    {
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, m_fg);
        ::DrawText(hdc, m_text, -1, &pill,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
    }
    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_BADGE
