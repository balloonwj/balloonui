#include "stdafx.h"
#include "DuiGroupBox.h"

#if BUI_FEATURE_GROUPBOX

#include "../../DuiResMgr.h"

namespace balloonwjui {

DuiGroupBox::DuiGroupBox()
{
    SetTabStop(false);
}

void DuiGroupBox::SetTitle(LPCTSTR title)
{
    CString t = title ? title : _T("");
    if (m_title == t)
    {
        return;
    }
    m_title = t;
    Invalidate();
}

void DuiGroupBox::SetBorderColor(COLORREF c)
{
    if (m_clrBorder == c)
    {
        return;
    }
    m_clrBorder = c;
    Invalidate();
}

void DuiGroupBox::SetCornerRadius(int px)
{
    if (px < 0)
    {
        px = 0;
    }
    if (m_cornerR == px)
    {
        return;
    }
    m_cornerR = px;
    Invalidate();
}

void DuiGroupBox::SetTitleColor(COLORREF c)
{
    if (m_clrTitle == c)
    {
        return;
    }
    m_clrTitle = c;
    Invalidate();
}

void DuiGroupBox::SetPadding(int l, int t, int r, int b)
{
    m_padL = l < 0 ? 0 : l;
    m_padT = t < 0 ? 0 : t;
    m_padR = r < 0 ? 0 : r;
    m_padB = b < 0 ? 0 : b;
    Layout(m_rcItem);
    Invalidate();
}

void DuiGroupBox::SetTitleStripHeight(int px)
{
    if (px < 0)
    {
        px = 0;
    }
    if (m_titleH == px)
    {
        return;
    }
    m_titleH = px;
    Layout(m_rcItem);
    Invalidate();
}

void DuiGroupBox::SetContent(std::unique_ptr<DuiControl> content)
{
    if (m_content)
    {
        RemoveChild(m_content);
        m_content = nullptr;
    }
    if (content)
    {
        DuiControl* raw = content.get();
        AddChild(std::move(content));
        m_content = raw;
    }
    Layout(m_rcItem);
    Invalidate();
}

RECT DuiGroupBox::ComputeContentRect(const RECT& outer, int titleStripH,
                                     int padL, int padT, int padR, int padB)
{
    RECT inner = outer;
    inner.top    += titleStripH;
    inner.left   += padL;
    inner.top    += padT;
    inner.right  -= padR;
    inner.bottom -= padB;
    if (inner.right  < inner.left)
    {
        inner.right  = inner.left;
    }
    if (inner.bottom < inner.top)
    {
        inner.bottom = inner.top;
    }
    return inner;
}

void DuiGroupBox::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    if (!m_content)
    {
        return;
    }
    RECT r = ComputeContentRect(m_rcItem, m_titleH, m_padL, m_padT, m_padR, m_padB);
    m_content->SetRect(r);
}

void DuiGroupBox::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    int w = m_rcItem.right - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // Border rect inset by half the strip so the title appears to sit
    // on the top edge of the border.
    int titleY = m_rcItem.top + (m_titleH / 2);
    RECT border = { m_rcItem.left, titleY, m_rcItem.right, m_rcItem.bottom };

    HPEN pen = ::CreatePen(PS_SOLID, 1, m_clrBorder);
    HPEN op  = (HPEN)::SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    if (m_cornerR > 0)
    {
        ::RoundRect(hdc, border.left, border.top, border.right, border.bottom,
                    m_cornerR * 2, m_cornerR * 2);
    }
    else
    {
        ::Rectangle(hdc, border.left, border.top, border.right, border.bottom);
    }
    ::SelectObject(hdc, ob);
    ::SelectObject(hdc, op);
    ::DeleteObject(pen);

    // Title: paint on the top edge, with a small white-ish "cutout"
    // behind so the border line passes cleanly behind the text.
    if (!m_title.IsEmpty())
    {
        HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
        HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
        SIZE sz = {};
        ::GetTextExtentPoint32(hdc, m_title, m_title.GetLength(), &sz);

        const int kPad = 6;
        int textX = m_rcItem.left + 12;
        int textY = m_rcItem.top;
        RECT cutout = { textX - kPad, titleY - sz.cy / 2,
                        textX + sz.cx + kPad, titleY + sz.cy / 2 + 1 };
        // Background of host bleeds through if we use parent; default
        // GroupBox sits on a white-ish surface, so a white fill is the
        // most useful default.
        HBRUSH cb = ::CreateSolidBrush(RGB(255, 255, 255));
        ::FillRect(hdc, &cutout, cb);
        ::DeleteObject(cb);

        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, m_clrTitle);
        RECT rt = { textX, textY, textX + sz.cx + 1, textY + m_titleH };
        ::DrawText(hdc, m_title, -1, &rt,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        if (oldFont)
        {
            ::SelectObject(hdc, oldFont);
        }
    }

    // Content paints itself.
    if (m_content && m_content->IsVisible())
    {
        RECT inter;
        if (::IntersectRect(&inter, &m_content->GetRect(), &rcDirty))
        {
            m_content->OnPaint(hdc, inter);
        }
    }
}

DuiControl* DuiGroupBox::HitTest(POINT pt)
{
    if (!m_bVisible || !m_bEnabled)
    {
        return nullptr;
    }
    if (!::PtInRect(&m_rcItem, pt))
    {
        return nullptr;
    }
    if (m_content)
    {
        if (DuiControl* hit = m_content->HitTest(pt))
        {
            return hit;
        }
    }
    return this;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_GROUPBOX
