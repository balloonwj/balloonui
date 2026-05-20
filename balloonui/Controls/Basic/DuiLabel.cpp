#include "stdafx.h"
#include "DuiLabel.h"

#if BUI_FEATURE_LABEL

#include "../../DuiResMgr.h"
#include "../../DuiMnemonic.h"
#include <shellapi.h>

namespace balloonwjui {

DuiLabel::DuiLabel()
{
    // Text labels are not focusable; links are (so keyboard users can Tab to them).
    SetTabStop(false);
}

DuiLabel::~DuiLabel()
{
    if (m_hFontUnderline)
    {
        ::DeleteObject(m_hFontUnderline);
        m_hFontUnderline = nullptr;
    }
}

void DuiLabel::SetText(LPCTSTR sz)
{
    m_text = sz ? sz : _T("");
    Invalidate();
}

TCHAR DuiLabel::GetMnemonicChar() const
{
    return DuiMnemonic::FindChar(m_text);
}

void DuiLabel::SetMode(Mode m)
{
    if (m_mode == m)
    {
        return;
    }
    m_mode = m;
    SetTabStop(m == ModeLink);
    Invalidate();
}

COLORREF DuiLabel::EffectiveColor() const
{
    if (!m_bEnabled)
    {
        return RGB(140, 140, 140);
    }
    if (m_mode == ModeLink)
    {
        if (m_bHover)
        {
            return m_clrHover;
        }
        if (m_visited)
        {
            return m_clrVisited;
        }
        return m_clrLink;
    }
    return m_clrText;
}

HFONT DuiLabel::EffectiveFont(HDC hdc) const
{
    HFONT base = m_hFont
                 ? m_hFont
                 : DuiResMgr::Inst().GetDefaultFont();
    if (!base)
    {
        base = (HFONT)::GetCurrentObject(hdc, OBJ_FONT);
    }
    if (m_mode != ModeLink)
    {
        return base;
    }

    // Build / refresh an underlined sibling of the base font on demand.
    if (m_hFontUnderline && m_lastBaseFont == base)
    {
        return m_hFontUnderline;
    }

    if (m_hFontUnderline)
    {
        ::DeleteObject(m_hFontUnderline);
        m_hFontUnderline = nullptr;
    }
    LOGFONT lf;
    ::GetObject(base, sizeof(lf), &lf);
    lf.lfUnderline = TRUE;
    m_hFontUnderline = ::CreateFontIndirect(&lf);
    m_lastBaseFont = base;
    return m_hFontUnderline ? m_hFontUnderline : base;
}

void DuiLabel::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }
    if (m_text.IsEmpty())
    {
        return;
    }

    HFONT useFont = EffectiveFont(hdc);
    HFONT oldFont = (HFONT)::SelectObject(hdc, useFont);
    int      oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, EffectiveColor());

    DWORD dtFlags = m_dtFlags;
    if (m_wordWrap)
    {
        // DT_WORDBREAK + DT_SINGLELINE is undefined behavior; drop the
        // single-line bit and add wordbreak. DT_TOP is enforced because
        // DT_VCENTER is meaningless with multi-line text.
        dtFlags &= ~(DT_SINGLELINE | DT_VCENTER | DT_BOTTOM);
        dtFlags |= DT_WORDBREAK | DT_TOP;
    }

    RECT rc = m_rcItem;
    ::DrawText(hdc, m_text, -1, &rc, dtFlags);

    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, oldFont);
}

void DuiLabel::SetWordWrap(bool b)
{
    if (m_wordWrap == b)
    {
        return;
    }
    m_wordWrap = b;
    Invalidate();
}

int DuiLabel::MeasureHeight(int width) const
{
    if (m_text.IsEmpty())
    {
        return 0;
    }
    HDC hdc = ::GetDC(nullptr);
    if (!hdc)
    {
        return 0;
    }
    HFONT use = m_hFont ? m_hFont : DuiResMgr::Inst().GetDefaultFont();
    HFONT old = use ? (HFONT)::SelectObject(hdc, use) : nullptr;

    DWORD flags = DT_CALCRECT | DT_LEFT | DT_TOP | DT_NOPREFIX;
    if (m_wordWrap)
    {
        flags |= DT_WORDBREAK;
    }
    else
    {
        flags |= DT_SINGLELINE;
    }
    // DT_CALCRECT requires a non-zero `right` to constrain wrapping.
    // width <= 0 = "natural width" => omit DT_WORDBREAK constraint by
    // measuring single-line; DrawText then ignores `right`.
    int w = (width <= 0) ? 0x7FFF : width;
    RECT rc = { 0, 0, w, 0 };
    ::DrawText(hdc, m_text, -1, &rc, flags);
    int h = rc.bottom - rc.top;

    if (old)
    {
        ::SelectObject(hdc, old);
    }
    ::ReleaseDC(nullptr, hdc);
    return h;
}

bool DuiLabel::OnLButtonUp(POINT pt, UINT /*mkFlags*/)
{
    if (m_mode != ModeLink)
    {
        return false;
    }
    if (!m_bEnabled)
    {
        return false;
    }
    if (!::PtInRect(&m_rcItem, pt))
    {
        return false;
    }

    m_visited = true;
    Invalidate();
    NotifyParent(DUIN_CLICK);
    if (m_autoNav && !m_url.IsEmpty())
    {
        ::ShellExecute(nullptr, _T("open"), m_url, nullptr, nullptr, SW_SHOWNORMAL);
    }
    return true;
}

bool DuiLabel::OnSetCursor(POINT)
{
    if (m_mode == ModeLink && m_bEnabled)
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
        return true;
    }
    return false;   // let host fall back to arrow
}

} // namespace balloonwjui

#endif // BUI_FEATURE_LABEL
