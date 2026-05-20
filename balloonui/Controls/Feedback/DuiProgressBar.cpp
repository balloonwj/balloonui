#include "stdafx.h"
#include "DuiProgressBar.h"

#if BUI_FEATURE_PROGRESSBAR

#include "../../DuiResMgr.h"

namespace balloonwjui {

DuiProgressBar::DuiProgressBar() = default;

void DuiProgressBar::SetRange(int nMin, int nMax)
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

void DuiProgressBar::SetPos(int pos, bool notify)
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

void DuiProgressBar::SetText(LPCTSTR sz)
{
    m_text = sz ? sz : _T("");
    Invalidate();
}

int DuiProgressBar::ClampPos(int p) const
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

int DuiProgressBar::ComputeFillWidth() const
{
    int totalW = m_rcItem.right - m_rcItem.left;
    int range  = m_max - m_min;
    if (range <= 0 || totalW <= 0)
    {
        return 0;
    }
    int fill = (int)(((LONGLONG)totalW * (m_pos - m_min)) / range);
    if (fill < 0)
    {
        fill = 0;
    }
    if (fill > totalW)
    {
        fill = totalW;
    }
    return fill;
}

RECT DuiProgressBar::ComputeFillRect() const
{
    RECT r = m_rcItem;
    int range = m_max - m_min;
    if (m_vertical)
    {
        int totalH = m_rcItem.bottom - m_rcItem.top;
        if (range <= 0 || totalH <= 0)
        {
            r.bottom = r.top;
            return r;
        }
        int fill = (int)(((LONGLONG)totalH * (m_pos - m_min)) / range);
        if (fill < 0)
        {
            fill = 0;
        }
        if (fill > totalH)
        {
            fill = totalH;
        }
        r.bottom = r.top + fill;
        return r;
    }
    r.right = r.left + ComputeFillWidth();
    return r;
}

void DuiProgressBar::SetMarqueePhase(int phase)
{
    int p = phase % MarqueePeriod;
    if (p < 0)
    {
        p += MarqueePeriod;
    }
    if (m_marqueePhase == p)
    {
        return;
    }
    m_marqueePhase = p;
    if (m_marquee)
    {
        Invalidate();
    }
}

RECT DuiProgressBar::ComputeMarqueeRect() const
{
    RECT r = m_rcItem;
    int phase = m_marqueePhase;
    if (m_vertical)
    {
        int totalH = m_rcItem.bottom - m_rcItem.top;
        if (totalH <= 0)
        {
            r.bottom = r.top;
            return r;
        }
        // Slide a block of MarqueeBlockPx height from top to bottom over
        // MarqueePeriod ticks; the block's top y in [-MarqueeBlockPx, totalH).
        int travel = totalH + MarqueeBlockPx;
        int y = (int)(((LONGLONG)phase * travel) / MarqueePeriod) - MarqueeBlockPx;
        r.top    = m_rcItem.top + (y < 0 ? 0 : y);
        int blockH = MarqueeBlockPx;
        if (y < 0)
        {
            blockH += y;        // first sliver clipped at top
        }
        if (y + MarqueeBlockPx > totalH)
        {
            blockH = totalH - y;              // last sliver clipped at bottom
        }
        if (blockH < 0)
        {
            blockH = 0;
        }
        r.bottom = r.top + blockH;
        return r;
    }
    int totalW = m_rcItem.right - m_rcItem.left;
    if (totalW <= 0)
    {
        r.right = r.left;
        return r;
    }
    int travel = totalW + MarqueeBlockPx;
    int x = (int)(((LONGLONG)phase * travel) / MarqueePeriod) - MarqueeBlockPx;
    r.left = m_rcItem.left + (x < 0 ? 0 : x);
    int blockW = MarqueeBlockPx;
    if (x < 0)
    {
        blockW += x;
    }
    if (x + MarqueeBlockPx > totalW)
    {
        blockW = totalW - x;
    }
    if (blockW < 0)
    {
        blockW = 0;
    }
    r.right = r.left + blockW;
    return r;
}

void DuiProgressBar::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    // Background
    HBRUSH bg = ::CreateSolidBrush(m_clrBg);
    ::FillRect(hdc, &m_rcItem, bg);
    ::DeleteObject(bg);

    // Fill
    if (m_marquee)
    {
        RECT mr = ComputeMarqueeRect();
        if (mr.right > mr.left && mr.bottom > mr.top)
        {
            HBRUSH fb = ::CreateSolidBrush(m_clrFill);
            ::FillRect(hdc, &mr, fb);
            ::DeleteObject(fb);
        }
    }
    else
    {
        RECT rFill = ComputeFillRect();
        if (rFill.right > rFill.left && rFill.bottom > rFill.top)
        {
            HBRUSH fb = ::CreateSolidBrush(m_clrFill);
            ::FillRect(hdc, &rFill, fb);
            ::DeleteObject(fb);
        }
    }

    // Border
    HPEN pen = ::CreatePen(PS_SOLID, 1, m_clrBorder);
    HPEN oldPen = (HPEN)::SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    ::Rectangle(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom);
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pen);

    // Label overlay
    CString label = m_text;
    if (label.IsEmpty() && m_showPercent)
    {
        int range = m_max - m_min;
        int pct   = (range > 0) ? (int)(((LONGLONG)(m_pos - m_min) * 100) / range) : 0;
        label.Format(_T("%d%%"), pct);
    }
    if (!label.IsEmpty())
    {
        HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
        HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, m_clrText);
        ::DrawText(hdc, label, -1, const_cast<RECT*>(&m_rcItem),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        if (oldFont)
        {
            ::SelectObject(hdc, oldFont);
        }
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_PROGRESSBAR
