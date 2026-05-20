#include "stdafx.h"
#include "DuiRichEditHost.h"

#if BUI_FEATURE_RICHEDIT

#include "../Media/DuiImageOle.h"
#include "../../DuiHost.h"
#include "../../DuiResMgr.h"
#include <richedit.h>
#include <gdiplus.h>
#include <algorithm>
#include <vector>

namespace balloonwjui {

namespace {
    // ---- Border colors ------------------------------------------------------
    // Border for a disabled RichEdit host - light gray, no chroma so the
    // control reads as inert.
    const COLORREF kBorderDisabled = RGB(190, 190, 190);
    // Border when keyboard focus is on the RichEdit - blue accent matches
    // the project's standard "active text input" affordance.
    const COLORREF kBorderFocused  = RGB( 80, 130, 200);
    // Border when the mouse is hovering but no focus - slightly lighter blue
    // than focused, signalling clickable-but-not-active.
    const COLORREF kBorderHover    = RGB(100, 140, 200);
    // Border in the idle (enabled, unfocused, no hover) state.
    const COLORREF kBorderIdle     = RGB(150, 150, 150);

    // Outer fill when the control is disabled - flat light gray, slightly
    // darker than the disabled border for visual hierarchy.
    const COLORREF kFillDisabled   = RGB(240, 240, 240);

    // Placeholder text color - mid-gray; clearly differentiated from real
    // user content (which uses m_textColor, typically near-black).
    const COLORREF kPlaceholderText = RGB(160, 160, 160);

    // ---- Quote / file-card colors -------------------------------------------
    // Quote block (sender + body lines) text color - mid-gray; renders the
    // quote visually de-emphasized vs. the user's own composition.
    const COLORREF kQuoteText      = RGB(120, 120, 120);
    // File-card text color - near-black with slight lightness; underlined
    // to read as "actionable" without using a hyperlink-blue that would
    // collide with auto-detected URLs.
    const COLORREF kFileCardText   = RGB( 50,  50,  50);

    // ---- Layout / sizing constants ------------------------------------------
    // Twips per point at 9pt; CHARFORMAT2::yHeight is in twips and we use
    // 9pt to match the project default UI font size from CLAUDE.md.
    // 9 points * 20 twips/pt = 180 twips.
    const int kQuoteFontHeightTwips = 9 * 20;

    // Process-wide lazy load of msftedit.dll (provides RICHEDIT50W).
    HMODULE EnsureRichEditLib()
    {
        static HMODULE s_h = nullptr;
        if (!s_h)
        {
            s_h = ::LoadLibrary(_T("Msftedit.dll"));
        }
        return s_h;
    }

    UINT NextRichCtrlId()
    {
        // Distinct range from DuiEditHost (0xE000) so trace logs disambiguate.
        static UINT s_next = 0xE800;
        return s_next++;
    }
}

DuiRichEditHost::DuiRichEditHost()
{
    SetTabStop(true);
}

DuiRichEditHost::~DuiRichEditHost() = default;

void DuiRichEditHost::SetMultiLine(bool b)
{
    if (m_multiLine == b)
    {
        return;
    }
    m_multiLine = b;
    if (HWND h = GetHostedHwnd())
    {
        HWND parent = ::GetParent(h);
        ::DestroyWindow(h);
        Detach();
        if (parent)
        {
            EnsureCreated(parent);
        }
    }
}

void DuiRichEditHost::SetWordWrap(bool b)
{
    if (m_wordWrap == b)
    {
        return;
    }
    m_wordWrap = b;
    if (HWND h = GetHostedHwnd())
    {
        // ES_AUTOHSCROLL is style-baked; recreate.
        HWND parent = ::GetParent(h);
        ::DestroyWindow(h);
        Detach();
        if (parent)
        {
            EnsureCreated(parent);
        }
    }
}

bool DuiRichEditHost::EnsureCreated(HWND hwndParent)
{
    if (GetHostedHwnd())
    {
        return true;
    }
    if (!hwndParent || !::IsWindow(hwndParent))
    {
        return false;
    }
    if (!EnsureRichEditLib())
    {
        return false;
    }

    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_NOHIDESEL;
    if (m_multiLine)
    {
        // WS_VSCROLL = scrollbar slot reserved; RichEdit auto-hides it
        // when the document fits in the visible area. WS_HSCROLL only when
        // word-wrap is off (lines can extend horizontally).
        style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
        if (!m_wordWrap)
        {
            style |= ES_AUTOHSCROLL | WS_HSCROLL;
        }
    }
    if (m_readonly)
    {
        style |= ES_READONLY;
    }

    if (m_ctrlIdHwnd == 0)
    {
        m_ctrlIdHwnd = NextRichCtrlId();
    }

    RECT rc = m_rcItem;
    ::InflateRect(&rc, -1, -1);
    rc.left   += m_marginL;
    rc.top    += m_marginT;
    rc.right  -= m_marginR;
    rc.bottom -= m_marginB;
    if (rc.right  < rc.left)
    {
        rc.right  = rc.left;
    }
    if (rc.bottom < rc.top)
    {
        rc.bottom = rc.top;
    }

    HWND h = ::CreateWindowEx(
        0,
        MSFTEDIT_CLASS,                     // L"RICHEDIT50W"
        m_cachedText,
        style,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        hwndParent,
        (HMENU)(UINT_PTR)m_ctrlIdHwnd,
        ::GetModuleHandle(nullptr),
        nullptr);
    if (!h)
    {
        return false;
    }

    // DUI default font; honors the project's 9pt Microsoft YaHei rule.
    HFONT hFont = DuiResMgr::Inst().GetDefaultFont();
    if (!hFont)
    {
        hFont = (HFONT)::SendMessage(hwndParent, WM_GETFONT, 0, 0);
    }
    if (hFont)
    {
        ::SendMessage(h, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(FALSE, 0));
    }

    // Subscribe to the events we care about. EN_LINK requires ENM_LINK.
    DWORD mask = ENM_CHANGE | ENM_SELCHANGE;
    if (m_autoUrl)
    {
        // Some older SDK headers omit AURL_ENABLEURL; non-zero wParam enables.
        ::SendMessage(h, EM_AUTOURLDETECT, (WPARAM)TRUE, 0);
        mask |= ENM_LINK;
    }
    ::SendMessage(h, EM_SETEVENTMASK, 0, (LPARAM)mask);

    // Background color.
    ::SendMessage(h, EM_SETBKGNDCOLOR, 0, (LPARAM)m_bgColor);

    // Make sure RichEdit auto-shows / hides the scrollbar based on content.
    // ECO_AUTOVSCROLL is normally implied by the ES_AUTOVSCROLL style bit
    // we set above, but RichEdit 5.0 sometimes needs an explicit OR-set
    // to start auto-managing the WS_VSCROLL slot reliably.
    if (m_multiLine)
    {
        ::SendMessage(h, EM_SETOPTIONS, ECOOP_OR,
                      (LPARAM)(ECO_AUTOVSCROLL | (m_wordWrap ? 0 : ECO_AUTOHSCROLL)));
    }

    if (m_maxLen > 0)
    {
        ::SendMessage(h, EM_EXLIMITTEXT, 0, (LPARAM)m_maxLen);
    }

    Attach(h);

    // Default char format (color + font name) - applied AFTER Attach so
    // ApplyDefaultCharFormat sees a valid GetHostedHwnd().
    ApplyDefaultCharFormat();
    return true;
}

void DuiRichEditHost::SetText(LPCTSTR sz)
{
    m_cachedText = sz ? sz : _T("");
    if (HWND h = GetHostedHwnd())
    {
        ::SetWindowText(h, m_cachedText);
    }
    Invalidate();
}

CString DuiRichEditHost::GetText() const
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return m_cachedText;
    }
    int n = ::GetWindowTextLength(h);
    if (n <= 0)
    {
        return CString();
    }
    CString s;
    LPTSTR buf = s.GetBufferSetLength(n);
    ::GetWindowText(h, buf, n + 1);
    s.ReleaseBuffer();
    return s;
}

int DuiRichEditHost::GetTextLength() const
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return m_cachedText.GetLength();
    }
    return ::GetWindowTextLength(h);
}

void DuiRichEditHost::SetReadOnly(bool b)
{
    m_readonly = b;
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, EM_SETREADONLY, b ? TRUE : FALSE, 0);
    }
}

void DuiRichEditHost::SetMaxLength(int n)
{
    m_maxLen = (n < 0) ? 0 : n;
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, EM_EXLIMITTEXT, 0, (LPARAM)(m_maxLen > 0 ? m_maxLen : 0x7FFFFFFE));
    }
}

void DuiRichEditHost::SetMargins(int l, int t, int r, int b)
{
    m_marginL = l;
    m_marginT = t;
    m_marginR = r;
    m_marginB = b;
    Layout(m_rcItem);
    Invalidate();
}

void DuiRichEditHost::SetPlaceholder(LPCTSTR sz)
{
    m_placeholder = sz ? sz : _T("");
    Invalidate();
}

void DuiRichEditHost::SetShowPlaceholder(bool b)
{
    m_showPlaceholder = b;
    Invalidate();
}

bool DuiRichEditHost::IsShowingPlaceholder() const
{
    return m_showPlaceholder
        && !m_placeholder.IsEmpty()
        && m_cachedText.IsEmpty()
        && !m_bFocused;
}

void DuiRichEditHost::SetBackgroundColor(COLORREF cr)
{
    m_bgColor = cr;
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, EM_SETBKGNDCOLOR, 0, (LPARAM)cr);
    }
    Invalidate();
}

void DuiRichEditHost::SetTextColor(COLORREF cr)
{
    m_textColor = cr;
    ApplyDefaultCharFormat();
}

void DuiRichEditHost::ApplyDefaultCharFormat()
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    CHARFORMAT2 cf{};
    cf.cbSize    = sizeof(cf);
    cf.dwMask    = CFM_COLOR;
    cf.crTextColor = m_textColor;
    ::SendMessage(h, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
}

void DuiRichEditHost::SetSel(long cpMin, long cpMax)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    CHARRANGE cr{ cpMin, cpMax };
    ::SendMessage(h, EM_EXSETSEL, 0, (LPARAM)&cr);
}

void DuiRichEditHost::GetSel(long& cpMin, long& cpMax) const
{
    cpMin = cpMax = 0;
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    CHARRANGE cr{};
    ::SendMessage(h, EM_EXGETSEL, 0, (LPARAM)&cr);
    cpMin = cr.cpMin;
    cpMax = cr.cpMax;
}

void DuiRichEditHost::SelectAll()
{
    SetSel(0, -1);
}

void DuiRichEditHost::ReplaceSel(LPCTSTR text, bool canUndo)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        m_cachedText = text ? text : _T("");
        return;
    }
    ::SendMessage(h, EM_REPLACESEL, canUndo ? TRUE : FALSE, (LPARAM)(text ? text : _T("")));
}

void DuiRichEditHost::AppendText(LPCTSTR text)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        m_cachedText += (text ? text : _T(""));
        return;
    }
    int len = ::GetWindowTextLength(h);
    SetSel(len, len);
    ReplaceSel(text, false);
}

int DuiRichEditHost::LineCount() const
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return 0;
    }
    return (int)::SendMessage(h, EM_GETLINECOUNT, 0, 0);
}

void DuiRichEditHost::Undo()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, EM_UNDO, 0, 0);
    }
}

void DuiRichEditHost::Cut()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, WM_CUT, 0, 0);
    }
}

void DuiRichEditHost::Copy()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, WM_COPY, 0, 0);
    }
}

void DuiRichEditHost::Paste()
{
    if (HWND h = GetHostedHwnd())
    {
        if (m_pastePlain)
        {
            PasteAsPlainText();
            return;
        }
        ::SendMessage(h, WM_PASTE, 0, 0);
    }
}

bool DuiRichEditHost::PasteAsPlainText()
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return false;
    }
    // EM_PASTESPECIAL takes a clipboard format ID in wParam. Prefer
    // CF_UNICODETEXT; fall back to CF_TEXT for ANSI-only clipboards.
    UINT cf = 0;
    if (::IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        cf = CF_UNICODETEXT;
    }
    else if (::IsClipboardFormatAvailable(CF_TEXT))
    {
        cf = CF_TEXT;
    }
    else
    {
        return false;
    }
    REPASTESPECIAL rps{};
    rps.dwAspect = DVASPECT_CONTENT;
    rps.dwParam  = 0;
    ::SendMessage(h, EM_PASTESPECIAL, (WPARAM)cf, (LPARAM)&rps);
    return true;
}

bool DuiRichEditHost::FindText(LPCTSTR needle,
                               long searchFrom,
                               bool forward,
                               bool matchCase,
                               bool matchWord,
                               CHARRANGE& outRange) const
{
    HWND h = GetHostedHwnd();
    if (!h || !needle || !*needle)
    {
        return false;
    }

    // Resolve the start position when caller passed -1: use the current
    // selection's end (forward) or start (backward) so successive
    // FindAndSelect calls advance through hits.
    long fromPos = searchFrom;
    if (fromPos < 0)
    {
        CHARRANGE selNow{};
        ::SendMessage(h, EM_EXGETSEL, 0, (LPARAM)&selNow);
        fromPos = forward ? selNow.cpMax : selNow.cpMin;
    }

    FINDTEXTEXW ftex{};
    ftex.chrg.cpMin = fromPos;
    // cpMax = -1 means "to end of document" (forward) or
    // "to start of document" (backward, with FR_DOWN cleared).
    ftex.chrg.cpMax = -1;
    ftex.lpstrText  = (LPWSTR)needle;
    ftex.chrgText.cpMin = -1;
    ftex.chrgText.cpMax = -1;

    DWORD flags = 0;
    if (forward)
    {
        flags |= FR_DOWN;
    }
    if (matchCase)
    {
        flags |= FR_MATCHCASE;
    }
    if (matchWord)
    {
        flags |= FR_WHOLEWORD;
    }

    LRESULT hit = ::SendMessage(h, EM_FINDTEXTEXW, (WPARAM)flags, (LPARAM)&ftex);
    if (hit < 0)
    {
        return false;
    }
    outRange = ftex.chrgText;
    return true;
}

bool DuiRichEditHost::FindAndSelect(LPCTSTR needle,
                                    long searchFrom,
                                    bool forward,
                                    bool matchCase,
                                    bool matchWord,
                                    bool wrap)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return false;
    }
    CHARRANGE r{};
    if (FindText(needle, searchFrom, forward, matchCase, matchWord, r))
    {
        SetSel(r.cpMin, r.cpMax);
        return true;
    }
    if (!wrap)
    {
        return false;
    }
    // Wrap: restart from the far end of the document.
    long restart = forward ? 0 : ::GetWindowTextLength(h);
    if (FindText(needle, restart, forward, matchCase, matchWord, r))
    {
        SetSel(r.cpMin, r.cpMax);
        return true;
    }
    return false;
}

void DuiRichEditHost::Clear()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, WM_CLEAR, 0, 0);
    }
}

void DuiRichEditHost::ApplySelectionCharFormatFlag(DWORD mask, DWORD effects, bool on)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    CHARFORMAT2 cf{};
    cf.cbSize  = sizeof(cf);
    cf.dwMask  = mask;
    cf.dwEffects = on ? effects : 0;
    ::SendMessage(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

void DuiRichEditHost::SetSelBold(bool b)
{
    ApplySelectionCharFormatFlag(CFM_BOLD, CFE_BOLD, b);
}

void DuiRichEditHost::SetSelItalic(bool b)
{
    ApplySelectionCharFormatFlag(CFM_ITALIC, CFE_ITALIC, b);
}

void DuiRichEditHost::SetSelUnderline(bool b)
{
    ApplySelectionCharFormatFlag(CFM_UNDERLINE, CFE_UNDERLINE, b);
}

void DuiRichEditHost::ApplySelectionTextColor(COLORREF cr)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    CHARFORMAT2 cf{};
    cf.cbSize      = sizeof(cf);
    cf.dwMask      = CFM_COLOR;
    cf.crTextColor = cr;
    ::SendMessage(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

void DuiRichEditHost::SetSelTextColor(COLORREF cr) { ApplySelectionTextColor(cr); }

void DuiRichEditHost::SetAutoUrlDetect(bool b)
{
    if (m_autoUrl == b)
    {
        return;
    }
    m_autoUrl = b;
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    ::SendMessage(h, EM_AUTOURLDETECT, b ? (WPARAM)TRUE : 0, 0);
    DWORD mask = (DWORD)::SendMessage(h, EM_GETEVENTMASK, 0, 0);
    if (b)
    {
        mask |=  ENM_LINK;
    }
    else
    {
        mask &= ~ENM_LINK;
    }
    ::SendMessage(h, EM_SETEVENTMASK, 0, (LPARAM)mask);
}

void DuiRichEditHost::RefreshCacheFromHwnd()
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    int n = ::GetWindowTextLength(h);
    if (n <= 0)
    {
        m_cachedText.Empty();
        return;
    }
    LPTSTR buf = m_cachedText.GetBufferSetLength(n);
    ::GetWindowText(h, buf, n + 1);
    m_cachedText.ReleaseBuffer();
}

void DuiRichEditHost::OnHwndCommand(UINT enCode)
{
    switch (enCode)
    {
    case EN_CHANGE:
        RefreshCacheFromHwnd();
        Invalidate();
        NotifyParent(DUIN_VALUECHANGED, 0);
        break;
    case EN_SETFOCUS:
        m_bFocused = true;
        Invalidate();
        NotifyParent(DUIN_SETFOCUS);
        break;
    case EN_KILLFOCUS:
        m_bFocused = false;
        Invalidate();
        NotifyParent(DUIN_KILLFOCUS);
        break;
    default:
        break;
    }
}

LRESULT DuiRichEditHost::OnHwndNotify(NMHDR* pnmh)
{
    if (!pnmh)
    {
        return 0;
    }
    switch (pnmh->code)
    {
    case EN_LINK:
        // Forward to parent dialog as DUIN_RE_LINKCLICK; the parent can
        // pull the ENLINK*->chrg out of `extra` and decide what to do.
        // Only fire on actual click - mouse-move events also fire EN_LINK
        // and would spam the parent.
        {
            ENLINK* el = (ENLINK*)pnmh;
            if (el->msg == WM_LBUTTONUP)
            {
                NotifyParent(DUIN_RE_LINKCLICK, (LPARAM)el);
            }
        }
        return 0;
    default:
        return 0;
    }
}

void DuiRichEditHost::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;

    if (!GetHostedHwnd() && m_pHost && m_pHost->m_hWnd && ::IsWindow(m_pHost->m_hWnd))
    {
        EnsureCreated(m_pHost->m_hWnd);
    }

    if (HWND h = GetHostedHwnd())
    {
        RECT inner = m_rcItem;
        ::InflateRect(&inner, -1, -1);
        inner.left   += m_marginL;
        inner.top    += m_marginT;
        inner.right  -= m_marginR;
        inner.bottom -= m_marginB;
        if (inner.right  < inner.left)
        {
            inner.right  = inner.left;
        }
        if (inner.bottom < inner.top)
        {
            inner.bottom = inner.top;
        }
        ::SetWindowPos(h, nullptr,
                       inner.left, inner.top,
                       inner.right - inner.left,
                       inner.bottom - inner.top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

COLORREF DuiRichEditHost::BorderColor() const
{
    if (!m_bEnabled)
    {
        return kBorderDisabled;
    }
    if (m_bFocused)
    {
        return kBorderFocused;
    }
    if (m_bHover)
    {
        return kBorderHover;
    }
    return kBorderIdle;
}

void DuiRichEditHost::OnPaint(HDC hdc, const RECT&)
{
    if (!m_bVisible)
    {
        return;
    }

    // Outer fill (paints the strip outside the RichEdit child rect).
    HBRUSH hbr = ::CreateSolidBrush(m_bEnabled ? m_bgColor : kFillDisabled);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    HPEN hpen = ::CreatePen(PS_SOLID, 1, BorderColor());
    HPEN oldPen = (HPEN)::SelectObject(hdc, hpen);
    HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    ::Rectangle(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom);
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(hpen);

    // Placeholder painted on top - only when EDIT is empty + unfocused.
    // Until the HWND is created, the strip itself is what the user sees.
    if (IsShowingPlaceholder())
    {
        RECT rcText = m_rcItem;
        ::InflateRect(&rcText, -1, -1);
        rcText.left += m_marginL;
        rcText.top  += m_marginT;
        HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
        HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, kPlaceholderText);
        ::DrawText(hdc, m_placeholder, -1, &rcText,
                   (m_multiLine ? DT_LEFT | DT_TOP | DT_WORDBREAK
                                : DT_LEFT | DT_VCENTER | DT_SINGLELINE));
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        if (oldFont)
        {
            ::SelectObject(hdc, oldFont);
        }
    }
}

// ============================================================
// Rich-content insertion: image / quote / file card
// ============================================================

// ============================================================
// RTF / plain-text streaming via EM_STREAMOUT / EM_STREAMIN
// ============================================================

namespace {

// Cookie passed through EDITSTREAM::dwCookie. One of {readPtr,readRem}
// vs writeBuf is used depending on direction. Kept as a single struct
// so both callbacks share the same cookie type and we don't have to
// templatize the static glue functions.
struct StreamCookie
{
    // Read path (used by EM_STREAMIN): bytes left to feed the parser.
    const BYTE* readPtr = nullptr;
    size_t      readRem = 0;
    // Write path (used by EM_STREAMOUT): destination append-only buffer.
    std::vector<BYTE>* writeBuf = nullptr;
};

// EM_STREAMIN callback: copy up to `cb` bytes from the cookie's read
// buffer into RichEdit's internal parser. Returning 0 + *pcb=0 signals
// EOF; non-zero return aborts the streamin operation.
DWORD CALLBACK StreamReadCb(DWORD_PTR cookie, LPBYTE pbBuf, LONG cb, LONG* pcb)
{
    StreamCookie* sc = reinterpret_cast<StreamCookie*>(cookie);
    if (!sc || !pbBuf || !pcb || cb <= 0)
    {
        if (pcb)
        {
            *pcb = 0;
        }
        return 0;
    }
    size_t want = static_cast<size_t>(cb);
    if (want > sc->readRem)
    {
        want = sc->readRem;
    }
    if (want > 0)
    {
        ::memcpy(pbBuf, sc->readPtr, want);
        sc->readPtr += want;
        sc->readRem -= want;
    }
    *pcb = static_cast<LONG>(want);
    return 0;
}

// EM_STREAMOUT callback: append `cb` bytes from RichEdit into the
// cookie's write buffer. RichEdit calls this repeatedly until the
// document is fully serialized; reporting *pcb=cb each call keeps the
// stream going.
DWORD CALLBACK StreamWriteCb(DWORD_PTR cookie, LPBYTE pbBuf, LONG cb, LONG* pcb)
{
    StreamCookie* sc = reinterpret_cast<StreamCookie*>(cookie);
    if (!sc || !sc->writeBuf || !pbBuf || !pcb || cb <= 0)
    {
        if (pcb)
        {
            *pcb = 0;
        }
        return 0;
    }
    sc->writeBuf->insert(sc->writeBuf->end(), pbBuf, pbBuf + cb);
    *pcb = cb;
    return 0;
}

} // anonymous

bool DuiRichEditHost::SaveRTF(CStringA& out) const
{
    out.Empty();
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return false;
    }
    std::vector<BYTE> buf;
    StreamCookie sc;
    sc.writeBuf = &buf;

    EDITSTREAM es{};
    es.dwCookie    = reinterpret_cast<DWORD_PTR>(&sc);
    es.dwError     = 0;
    es.pfnCallback = &StreamWriteCb;

    // SF_RTF = RTF format. SFF_PLAINRTF would strip language/font tables
    // for size; we want a fully self-contained document so omit it.
    ::SendMessage(h, EM_STREAMOUT, (WPARAM)SF_RTF, (LPARAM)&es);
    if (es.dwError != 0)
    {
        out.Empty();
        return false;
    }
    if (!buf.empty())
    {
        ::memcpy(out.GetBufferSetLength(static_cast<int>(buf.size())),
                 buf.data(), buf.size());
        out.ReleaseBuffer(static_cast<int>(buf.size()));
    }
    return true;
}

bool DuiRichEditHost::LoadRTF(const CStringA& in)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return false;
    }
    StreamCookie sc;
    sc.readPtr = reinterpret_cast<const BYTE*>((LPCSTR)in);
    sc.readRem = static_cast<size_t>(in.GetLength());

    EDITSTREAM es{};
    es.dwCookie    = reinterpret_cast<DWORD_PTR>(&sc);
    es.dwError     = 0;
    es.pfnCallback = &StreamReadCb;

    ::SendMessage(h, EM_STREAMIN, (WPARAM)SF_RTF, (LPARAM)&es);
    if (es.dwError != 0)
    {
        return false;
    }
    RefreshCacheFromHwnd();
    Invalidate();
    NotifyParent(DUIN_VALUECHANGED, 0);
    return true;
}

bool DuiRichEditHost::SaveText(CString& out) const
{
    out.Empty();
    HWND h = GetHostedHwnd();
    if (!h)
    {
        // Without an HWND, the cache *is* the document.
        out = m_cachedText;
        return true;
    }
    std::vector<BYTE> buf;
    StreamCookie sc;
    sc.writeBuf = &buf;

    EDITSTREAM es{};
    es.dwCookie    = reinterpret_cast<DWORD_PTR>(&sc);
    es.dwError     = 0;
    es.pfnCallback = &StreamWriteCb;

    // SF_TEXT | SF_UNICODE: write UTF-16 codeunits with no BOM.
    ::SendMessage(h, EM_STREAMOUT, (WPARAM)(SF_TEXT | SF_UNICODE), (LPARAM)&es);
    if (es.dwError != 0)
    {
        return false;
    }
    if (!buf.empty())
    {
        // buf is a byte stream of UTF-16 codeunits.
        size_t chars = buf.size() / sizeof(wchar_t);
        ::memcpy(out.GetBufferSetLength(static_cast<int>(chars)),
                 buf.data(), chars * sizeof(wchar_t));
        out.ReleaseBuffer(static_cast<int>(chars));
    }
    return true;
}

bool DuiRichEditHost::LoadText(const CString& in)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        m_cachedText = in;
        return true;
    }
    StreamCookie sc;
    sc.readPtr = reinterpret_cast<const BYTE*>((LPCWSTR)in);
    sc.readRem = static_cast<size_t>(in.GetLength()) * sizeof(wchar_t);

    EDITSTREAM es{};
    es.dwCookie    = reinterpret_cast<DWORD_PTR>(&sc);
    es.dwError     = 0;
    es.pfnCallback = &StreamReadCb;

    ::SendMessage(h, EM_STREAMIN, (WPARAM)(SF_TEXT | SF_UNICODE), (LPARAM)&es);
    if (es.dwError != 0)
    {
        return false;
    }
    RefreshCacheFromHwnd();
    Invalidate();
    NotifyParent(DUIN_VALUECHANGED, 0);
    return true;
}

CString DuiRichEditHost::FormatHumanSize(ULONGLONG bytes)
{
    static LPCTSTR units[] = { _T("B"), _T("KB"), _T("MB"), _T("GB"), _T("TB") };
    if (bytes < 1024)
    {
        CString s;
        s.Format(_T("%llu B"), bytes);
        return s;
    }
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024.0 && u < 4)
    {
        v /= 1024.0;
        ++u;
    }
    CString s;
    s.Format(_T("%.1f %s"), v, units[u]);
    return s;
}

namespace {

// Proportionally downscale an HBITMAP to fit (maxW, maxH). Either dimension
// can be 0 meaning "no constraint on this axis". Returns the (possibly new)
// HBITMAP and sets `outOwned = true` if the caller must DeleteObject it.
HBITMAP DownscaleHBITMAP(HBITMAP src, int maxW, int maxH, bool& outOwned)
{
    outOwned = false;
    if (!src)
    {
        return nullptr;
    }
    BITMAP bm{};
    if (!::GetObject(src, sizeof(bm), &bm))
    {
        return src;
    }
    int w = bm.bmWidth, h = bm.bmHeight;
    if ((maxW <= 0 || w <= maxW) && (maxH <= 0 || h <= maxH))
    {
        return src;
    }
    double sx = (maxW > 0) ? (double)maxW / w : 1.0;
    double sy = (maxH > 0) ? (double)maxH / h : 1.0;
    double s  = (std::min)(sx, sy);
    int nw = (std::max)(1, (int)(w * s));
    int nh = (std::max)(1, (int)(h * s));

    HDC hdcScreen = ::GetDC(nullptr);
    HDC hdcSrc = ::CreateCompatibleDC(hdcScreen);
    HDC hdcDst = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hbmDst = ::CreateCompatibleBitmap(hdcScreen, nw, nh);
    HBITMAP oldS = (HBITMAP)::SelectObject(hdcSrc, src);
    HBITMAP oldD = (HBITMAP)::SelectObject(hdcDst, hbmDst);
    ::SetStretchBltMode(hdcDst, HALFTONE);
    ::SetBrushOrgEx(hdcDst, 0, 0, nullptr);
    ::StretchBlt(hdcDst, 0, 0, nw, nh, hdcSrc, 0, 0, w, h, SRCCOPY);
    ::SelectObject(hdcSrc, oldS);
    ::SelectObject(hdcDst, oldD);
    ::DeleteDC(hdcSrc);
    ::DeleteDC(hdcDst);
    ::ReleaseDC(nullptr, hdcScreen);
    outOwned = true;
    return hbmDst;
}

// One-time GDI+ startup. We use it for image-file decoding (Gdiplus::Bitmap
// handles bmp/jpg/png/gif transparently). The token is leaked at process
// exit which matches the lifetime model of the rest of this codebase.
void EnsureGdiPlus()
{
    static ULONG_PTR s_token = 0;
    if (s_token)
    {
        return;
    }
    Gdiplus::GdiplusStartupInput in;
    Gdiplus::GdiplusStartup(&s_token, &in, nullptr);
}

} // anonymous

bool DuiRichEditHost::InsertImageFromBitmap(HBITMAP hbm, int maxW, int maxH)
{
    HWND h = GetHostedHwnd();
    if (!h || !hbm)
    {
        return false;
    }

    bool owned = false;
    HBITMAP hbmUse = DownscaleHBITMAP(hbm, maxW, maxH, owned);
    if (!hbmUse)
    {
        return false;
    }

    // Embed via a real OLE object instead of the historical CF_BITMAP /
    // EM_PASTESPECIAL clipboard hack. Side benefits: no clipboard
    // trampling, image survives selection without the user's clipboard
    // state leaking, and the foundation for RTF persistence is in place
    // (Save/Load currently no-op; can be filled in later).
    //
    // CDuiImageOle::InsertIntoRichEdit takes ownership of the bitmap
    // when ownsHbm=true, so we hand off `hbmUse` and clear the local
    // ownership flag.
    bool ok = CDuiImageOle::InsertIntoRichEdit(h, hbmUse, /*ownsHbm=*/owned);
    return ok;
}

bool DuiRichEditHost::InsertImageFromFile(LPCTSTR path, int maxW, int maxH)
{
    HWND h = GetHostedHwnd();
    if (!h || !path || !*path)
    {
        return false;
    }
    EnsureGdiPlus();

    Gdiplus::Bitmap bmp(path);
    if (bmp.GetLastStatus() != Gdiplus::Ok)
    {
        return false;
    }
    HBITMAP hbm = nullptr;
    if (bmp.GetHBITMAP(Gdiplus::Color(255, 255, 255), &hbm) != Gdiplus::Ok || !hbm)
    {
        return false;
    }

    bool ok = InsertImageFromBitmap(hbm, maxW, maxH);
    ::DeleteObject(hbm);
    return ok;
}

void DuiRichEditHost::InsertQuoteBlock(LPCTSTR senderName, LPCTSTR body)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }

    CString senderText(senderName ? senderName : _T(""));
    CString bodyText  (body ? body : _T(""));
    bodyText.Replace(_T("\r\n"), _T("\n"));
    bodyText.Replace(_T("\r"),   _T("\n"));

    CHARFORMAT2 cf{};
    cf.cbSize    = sizeof(cf);
    cf.dwMask    = CFM_COLOR | CFM_BOLD | CFM_SIZE | CFM_ITALIC;
    cf.crTextColor = kQuoteText;
    cf.yHeight   = kQuoteFontHeightTwips;       // 9pt in twips

    // Sender line, bold gray, prefixed with U+2503 HEAVY VERTICAL.
    cf.dwEffects = CFE_BOLD;
    ::SendMessage(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    CString line1;
    line1.Format(_T("┃  %s\r\n"), (LPCTSTR)senderText);
    ::SendMessage(h, EM_REPLACESEL, FALSE, (LPARAM)(LPCTSTR)line1);

    // Body lines, italic gray, same prefix.
    cf.dwEffects = CFE_ITALIC;
    ::SendMessage(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    int start = 0;
    while (true)
    {
        int nl = bodyText.Find(_T('\n'), start);
        CString seg = (nl < 0)
            ? bodyText.Mid(start)
            : bodyText.Mid(start, nl - start);
        CString lineOut;
        lineOut.Format(_T("┃  %s\r\n"), (LPCTSTR)seg);
        ::SendMessage(h, EM_REPLACESEL, FALSE, (LPARAM)(LPCTSTR)lineOut);
        if (nl < 0)
        {
            break;
        }
        start = nl + 1;
    }

    // Restore default char format for whatever the user types next.
    ApplyDefaultCharFormat();
}

void DuiRichEditHost::InsertFileCard(LPCTSTR fileName, ULONGLONG sizeBytes)
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }

    CHARFORMAT2 cf{};
    cf.cbSize      = sizeof(cf);
    cf.dwMask      = CFM_COLOR | CFM_UNDERLINE | CFM_SIZE | CFM_BOLD;
    cf.dwEffects   = CFE_UNDERLINE;
    cf.crTextColor = kFileCardText;
    cf.yHeight     = kQuoteFontHeightTwips;
    ::SendMessage(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // U+1F4CE PAPERCLIP (surrogate pair on UTF-16 builds: 0xD83D 0xDCCE)
    // U+00B7 MIDDLE DOT as separator
    CString line;
    line.Format(_T("\xD83D\xDCCE  %s   ·   %s\r\n"),
                fileName ? fileName : _T(""),
                (LPCTSTR)FormatHumanSize(sizeBytes));
    ::SendMessage(h, EM_REPLACESEL, FALSE, (LPARAM)(LPCTSTR)line);

    ApplyDefaultCharFormat();
}

bool DuiRichEditHost::OnSetCursor(POINT)
{
    if (!m_bEnabled)
    {
        return false;
    }
    // RichEdit handles its own cursor (I-beam over text, hand over auto-detected
    // links). DuiHost only sees cursor queries for the strip outside the RichEdit
    // child window - show I-beam there to keep behavior consistent.
    ::SetCursor(::LoadCursor(nullptr, IDC_IBEAM));
    return true;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHEDIT
