#include "stdafx.h"
#include "DuiEditHost.h"

#if BUI_FEATURE_EDIT

#include "../../DuiHost.h"
#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"

namespace balloonwjui {

// Eye toggle visual constants.
namespace {
    const int kEyeBoxW   = 22;          // reserved width on right side of EDIT
    const int kEyeIconW  = 18;          // visible eye icon width
    const int kEyeIconH  = 12;          // visible eye icon height

    // Border colors. The DUI host paints a 1px frame around the OS EDIT;
    // the color tracks input state so the user can see focus / hover /
    // disabled at a glance even though the EDIT itself doesn't recolor.
    const COLORREF kBorderDisabled = RGB(190, 190, 190);   // muted gray, low contrast
    const COLORREF kBorderFocused  = RGB( 80, 130, 200);   // saturated blue when typing
    const COLORREF kBorderHover    = RGB(100, 140, 200);   // slightly lighter than focused
    const COLORREF kBorderNormal   = RGB(150, 150, 150);   // resting medium gray

    // Background fill behind the host strip (1px outside the EDIT). Disabled
    // gets a flat light gray; enabled is system white. We don't extract pure
    // white here since it appears once and reads as "white" at the call site.
    const COLORREF kBgDisabled     = RGB(240, 240, 240);

    // Placeholder text color - matches Windows native placeholder gray so
    // the prompt is clearly distinguishable from real user-typed content.
    const COLORREF kPlaceholderText = RGB(160, 160, 160);

    // Eye toggle (password reveal) icon colors. Hover uses the brand blue
    // documented in CLAUDE.md PushButton style; resting state is medium
    // gray so it doesn't compete with the EDIT's own caret/text.
    const COLORREF kEyeIconHover   = RGB( 45, 108, 223);   // brand blue (#2D6CDF)
    const COLORREF kEyeIconNormal  = RGB(110, 110, 110);   // medium gray
}

namespace {
    // Use a high range for child control ids assigned to DuiEditHost EDITs
    // to avoid colliding with WTL/Resource-based control ids in the parent.
    UINT NextEditCtrlId()
    {
        static UINT s_next = 0xE000;
        return s_next++;
    }
}

DuiEditHost::DuiEditHost()
{
    SetTabStop(true);
}

DuiEditHost::~DuiEditHost() = default;

void DuiEditHost::SetMultiLine(bool b)
{
    if (m_multiLine == b)
    {
        return;
    }
    m_multiLine = b;
    if (HWND h = GetHostedHwnd())
    {
        // Recreate to honor the new style. Caller must re-Layout.
        HWND parent = ::GetParent(h);
        ::DestroyWindow(h);
        Detach();
        if (parent)
        {
            EnsureCreated(parent);
        }
    }
}

void DuiEditHost::SetPassword(bool b)
{
    if (m_password == b)
    {
        return;
    }
    m_password = b;
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

bool DuiEditHost::EnsureCreated(HWND hwndParent)
{
    if (GetHostedHwnd())
    {
        return true;
    }
    if (!hwndParent || !::IsWindow(hwndParent))
    {
        return false;
    }

    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
    if (m_multiLine)
    {
        style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
    }
    if (m_password)
    {
        style |= ES_PASSWORD;
    }
    if (m_readonly)
    {
        style |= ES_READONLY;
    }

    if (m_ctrlIdHwnd == 0)
    {
        m_ctrlIdHwnd = NextEditCtrlId();
    }

    RECT rc = m_rcItem;
    // Apply margins so the EDIT sits inside our 1px border + margins.
    ::InflateRect(&rc, -1, -1);
    rc.left   += m_marginL;
    rc.top    += m_marginT;
    rc.right  -= m_marginR;
    rc.bottom -= m_marginB;
    if (rc.right < rc.left)
    {
        rc.right = rc.left;
    }
    if (rc.bottom < rc.top)
    {
        rc.bottom = rc.top;
    }

    HWND h = ::CreateWindowEx(
        0,
        _T("EDIT"),
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

    // Use Microsoft YaHei (DUI default) so user-typed Chinese renders properly.
    HFONT hFont = DuiResMgr::Inst().GetDefaultFont();
    if (!hFont)
    {
        hFont = (HFONT)::SendMessage(hwndParent, WM_GETFONT, 0, 0);
    }
    if (hFont)
    {
        ::SendMessage(h, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(FALSE, 0));
    }

    if (m_maxLen > 0)
    {
        ::SendMessage(h, EM_SETLIMITTEXT, (WPARAM)m_maxLen, 0);
    }

    Attach(h);
    // Attach 之后 GetHostedHwnd() 才返回真 HWND；SyncPlaceholderToHwnd 才能把
    // EnsureCreated 之前调过的 SetPlaceholder 真正应用到新 EDIT。
    SyncPlaceholderToHwnd();
    return true;
}

void DuiEditHost::SetText(LPCTSTR sz)
{
    m_cachedText = sz ? sz : _T("");
    SyncTextToHwnd();
    Invalidate();
}

CString DuiEditHost::GetText() const
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

void DuiEditHost::SetReadOnly(bool b)
{
    m_readonly = b;
    SyncReadOnlyToHwnd();
}

void DuiEditHost::SetMaxLength(int n)
{
    m_maxLen = (n < 0) ? 0 : n;
    SyncMaxLengthToHwnd();
}

void DuiEditHost::SetMargins(int left, int top, int right, int bottom)
{
    m_marginL = left;
    m_marginT = top;
    m_marginR = right;
    m_marginB = bottom;
    Layout(m_rcItem);
    Invalidate();
}

void DuiEditHost::SetPlaceholder(LPCTSTR sz)
{
    m_placeholder = sz ? sz : _T("");
    SyncPlaceholderToHwnd();
    Invalidate();
}

void DuiEditHost::SetShowPlaceholder(bool b)
{
    m_showPlaceholder = b;
    SyncPlaceholderToHwnd();
    Invalidate();
}

bool DuiEditHost::IsShowingPlaceholder() const
{
    return m_showPlaceholder
        && !m_placeholder.IsEmpty()
        && m_cachedText.IsEmpty()
        && !m_bFocused;
}

void DuiEditHost::SyncReadOnlyToHwnd()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, EM_SETREADONLY, m_readonly ? TRUE : FALSE, 0);
    }
}

void DuiEditHost::SyncTextToHwnd()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SetWindowText(h, m_cachedText);
    }
}

void DuiEditHost::SyncMaxLengthToHwnd()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, EM_SETLIMITTEXT, (WPARAM)m_maxLen, 0);
    }
}

void DuiEditHost::SyncPlaceholderToHwnd()
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    // m_showPlaceholder=false 等价于"清空 cue"，cue 显示由 OS 在 EDIT empty
    // 时自动生效。第二参 FALSE = 获焦时消失（与 Win32 EDIT 默认 cue 行为一致）。
    // EM_SETCUEBANNER lParam 必须是 wchar_t* —— 项目是 _UNICODE 编译，
    // CString 是 wchar_t 串，可直接 cast。
    LPCWSTR cue = m_showPlaceholder ? (LPCWSTR)(LPCTSTR)m_placeholder : L"";
    ::SendMessageW(h, EM_SETCUEBANNER, FALSE, (LPARAM)cue);
}

void DuiEditHost::RefreshCacheFromHwnd()
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

void DuiEditHost::OnEditNotify(UINT enCode)
{
    switch (enCode)
    {
    case EN_CHANGE:
        // Refresh cached text from HWND so GetText() and IsShowingPlaceholder()
        // stay correct without needing another GetWindowText round-trip.
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

void DuiEditHost::Layout(const RECT& rcAvail)
{
    // Set our rect; HwndHostControl::Layout would resize the EDIT to fill
    // m_rcItem exactly, but we want margins + border, so override.
    m_rcItem = rcAvail;

    // Lazy-create the underlying EDIT HWND on first Layout once a host
    // is attached. This relieves callers (Pages.cpp / chat dialogs) from
    // having to track when SetRoot has run and call EnsureCreated by hand.
    if (!GetHostedHwnd() && m_pHost && m_pHost->m_hWnd && ::IsWindow(m_pHost->m_hWnd))
    {
        EnsureCreated(m_pHost->m_hWnd);
    }

    if (HWND h = GetHostedHwnd())
    {
        RECT inner = m_rcItem;
        ::InflateRect(&inner, -1, -1);     // skip 1px border
        inner.left   += m_marginL;
        inner.top    += m_marginT;
        inner.right  -= m_marginR;
        inner.bottom -= m_marginB;
        // If the eye toggle is visible, leave a kEyeBoxW strip on the
        // right so clicks there land on the DUI control (not the EDIT).
        if (EyeVisible())
        {
            inner.right -= kEyeBoxW;
        }
        // 内联图标 gutter —— EDIT 文字区收缩对应宽度，让 OnPaint 在外侧
        // 画图标。eye toggle 与 right icon 同时启用时，eye 优先（已经
        // 减过 kEyeBoxW；这里如再减一次会把 EDIT 挤到极小）—— 把右 icon
        // 在这种 corner case 下视为 0 宽（painter 不画）。
        inner.left  += m_iconL.width;
        if (!EyeVisible())
        {
            inner.right -= m_iconR.width;
        }
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

RECT DuiEditHost::EyeRect() const
{
    if (!EyeVisible())
    {
        return RECT{ 0, 0, 0, 0 };
    }
    int right = m_rcItem.right - 1 - m_marginR;     // skip border + right margin
    int left  = right - kEyeBoxW + (kEyeBoxW - kEyeIconW) / 2;
    int top   = (m_rcItem.top + m_rcItem.bottom - kEyeIconH) / 2;
    int rightIcon = left + kEyeIconW;
    return RECT{ left, top, rightIcon, top + kEyeIconH };
}

COLORREF DuiEditHost::BorderColor() const
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
    return kBorderNormal;
}

void DuiEditHost::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    // Background outside the EDIT (tiny strip given our margins) —— 用
    // SetBgColor 配置的色（默认白）。enabled=false 时仍走灰色 disabled
    // 底，独立于 m_bgColor。
    HBRUSH hbr = ::CreateSolidBrush(m_bEnabled ? m_bgColor : kBgDisabled);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    // Border —— 由 SetShowBorder 控制，默认 true 兼容旧行为
    if (m_showBorder)
    {
        HPEN hpen = ::CreatePen(PS_SOLID, 1, BorderColor());
        HPEN oldPen = (HPEN)::SelectObject(hdc, hpen);
        HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom);
        ::SelectObject(hdc, oldBr);
        ::SelectObject(hdc, oldPen);
        ::DeleteObject(hpen);
    }

    // Placeholder 由 EDIT 自己通过 EM_SETCUEBANNER 画 —— 见
    // SyncPlaceholderToHwnd。这里曾经在 host backbuffer 画 placeholder，
    // 但 EDIT 是 child HWND，OS 在 host paint 之后才让 EDIT 画自己（白
    // 底）→ EDIT 的白底必然覆盖 host 画的 placeholder，从来都看不见。
    // IsShowingPlaceholder() 仍然保留作为<u>状态查询</u> API（caller 可
    // 用来决定别的 UI 行为，例如"无字时弱化清除按钮"），但不再驱动绘制。

    // 内联图标 —— left icon 在 eye toggle 之前画；right icon 仅在
    // eye toggle 关闭时画（互斥，见 Layout 注释）。
    if (m_iconL.painter && m_iconL.width > 0)
    {
        RECT rcL = ComputeIconRect(m_rcItem, LeftIcon,
                                   m_iconL.width, 1, m_marginT);
        m_iconL.painter(hdc, rcL);
    }
    if (!EyeVisible() && m_iconR.painter && m_iconR.width > 0)
    {
        RECT rcR = ComputeIconRect(m_rcItem, RightIcon,
                                   m_iconR.width, 1, m_marginT);
        m_iconR.painter(hdc, rcR);
    }

    // Eye toggle button.
    if (EyeVisible())
    {
        RECT eye = EyeRect();
        COLORREF clr = m_eyeHover ? kEyeIconHover : kEyeIconNormal;
        // Almond shape: two arcs forming an open eye. We approximate with
        // a wide ellipse outline using DuiAA, plus a smaller filled pupil
        // when revealed (open). When masked (closed/private), draw a
        // diagonal slash through the eye to indicate "hidden".
        DuiAA::FillEllipse(hdc, eye, CLR_INVALID, clr, 1.5f);
        // Pupil (centered, smaller).
        int cx = (eye.left + eye.right) / 2;
        int cy = (eye.top + eye.bottom) / 2;
        RECT pupil = { cx - 3, cy - 3, cx + 3 + 1, cy + 3 + 1 };
        if (m_pwdRevealed)
        {
            DuiAA::FillEllipse(hdc, pupil, clr, CLR_INVALID);
        }
        else
        {
            // Slash: from upper-left to lower-right of the eye box.
            DuiAA::DrawLine(hdc, eye.left + 2, eye.top + 1,
                                 eye.right - 2, eye.bottom - 1, clr, 1.5f);
        }
    }
}

bool DuiEditHost::OnSetCursor(POINT pt)
{
    if (!m_bEnabled)
    {
        return false;
    }
    if (EyeVisible() && ::PtInRect(&EyeRect(), pt))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
        return true;
    }
    // 可点击图标 → 手形指针
    if (m_iconL.clickable && m_iconL.width > 0)
    {
        RECT r = ComputeIconRect(m_rcItem, LeftIcon,
                                 m_iconL.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
            return true;
        }
    }
    if (m_iconR.clickable && m_iconR.width > 0 && !EyeVisible())
    {
        RECT r = ComputeIconRect(m_rcItem, RightIcon,
                                 m_iconR.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
            return true;
        }
    }
    // The OS already shows IDC_IBEAM over the EDIT child window. The DUI
    // host only sees cursor queries for the strip outside the EDIT (1px
    // border + margins). Show I-beam there too so the cursor is consistent
    // across the whole text-input rect.
    ::SetCursor(::LoadCursor(nullptr, IDC_IBEAM));
    return true;
}

bool DuiEditHost::OnLButtonUp(POINT pt, UINT)
{
    if (EyeVisible() && ::PtInRect(&EyeRect(), pt))
    {
        SetPasswordRevealed(!m_pwdRevealed);
        return true;
    }
    if (m_iconL.clickable && m_iconL.width > 0)
    {
        RECT r = ComputeIconRect(m_rcItem, LeftIcon,
                                 m_iconL.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            // 子类可截：返 true 不发通知（自吞）
            if (!OnIconClicked_(LeftIcon))
            {
                NotifyParent(DUIEN_LEFT_ICON_CLICK, 0);
            }
            return true;
        }
    }
    if (m_iconR.clickable && m_iconR.width > 0 && !EyeVisible())
    {
        RECT r = ComputeIconRect(m_rcItem, RightIcon,
                                 m_iconR.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            if (!OnIconClicked_(RightIcon))
            {
                NotifyParent(DUIEN_RIGHT_ICON_CLICK, 0);
            }
            return true;
        }
    }
    return false;
}

bool DuiEditHost::OnMouseMove(POINT pt, UINT)
{
    bool nowOver = EyeVisible() && ::PtInRect(&EyeRect(), pt) ? true : false;
    if (nowOver != m_eyeHover)
    {
        m_eyeHover = nowOver;
        Invalidate();
    }
    return false;
}

bool DuiEditHost::OnMouseLeave()
{
    if (m_eyeHover)
    {
        m_eyeHover = false;
        Invalidate();
    }
    return DuiControl::OnMouseLeave();
}

void DuiEditHost::SetShowEyeToggle(bool b)
{
    if (m_showEye == b)
    {
        return;
    }
    m_showEye = b;
    // If turning off while plaintext is shown, revert mask.
    if (!m_showEye && m_pwdRevealed)
    {
        SetPasswordRevealed(false);
    }
    Layout(m_rcItem);   // re-position EDIT (its right edge changes)
    Invalidate();
}

void DuiEditHost::SetPasswordRevealed(bool b)
{
    if (!m_password)
    {
        return;
    }
    if (m_pwdRevealed == b)
    {
        return;
    }
    m_pwdRevealed = b;
    ApplyPasswordRevealToHwnd();
    Invalidate();
}

void DuiEditHost::ApplyPasswordRevealToHwnd()
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    if (m_savedMaskChar == 0)
    {
        // First call - capture the system default mask character so we
        // can restore it when re-hiding.
        TCHAR cur = (TCHAR)::SendMessage(h, EM_GETPASSWORDCHAR, 0, 0);
        if (cur != 0)
        {
            m_savedMaskChar = cur;
        }
        else
        {
            m_savedMaskChar = (TCHAR)0x25CF;   // BLACK CIRCLE fallback
        }
    }
    TCHAR newCh = m_pwdRevealed ? 0 : m_savedMaskChar;
    ::SendMessage(h, EM_SETPASSWORDCHAR, (WPARAM)newCh, 0);
    ::InvalidateRect(h, nullptr, TRUE);
}

// ─── 内联图标 + 底色 + 边框 API ───────────────────────────────────────

void DuiEditHost::SetShowBorder(bool b)
{
    if (m_showBorder == b) { return; }
    m_showBorder = b;
    Invalidate();
}

void DuiEditHost::SetBgColor(COLORREF c)
{
    if (m_bgColor == c)
    {
        return;
    }
    m_bgColor = c;
    // 同步给 Win32 EDIT 自身（OS 渲染时背景）：HwndHostControl 已实现
    // SetCtlBgColor + WM_CTLCOLOREDIT 路径，propagate 给它。
    SetCtlBgColor(c);
    Invalidate();
}

void DuiEditHost::SetIcon(IconSlot slot, int gutterWidth, IconPainter painter)
{
    if (gutterWidth < 0) { gutterWidth = 0; }
    IconState& st = (slot == LeftIcon) ? m_iconL : m_iconR;
    bool widthChanged = (st.width != gutterWidth);
    st.width   = painter ? gutterWidth : 0;
    st.painter = std::move(painter);
    if (widthChanged)
    {
        // EDIT 内缩量改变 —— 必须 re-layout 重排 HWND
        Layout(m_rcItem);
    }
    Invalidate();
}

void DuiEditHost::SetIconBitmap(IconSlot slot, int gutterWidth, HBITMAP hbm)
{
    if (!hbm)
    {
        ClearIcon(slot);
        return;
    }
    SetIcon(slot, gutterWidth,
            [hbm](HDC hdc, const RECT& rc)
    {
        BITMAP bi = {};
        if (::GetObject(hbm, sizeof(bi), &bi) == 0) { return; }
        HDC hdcMem = ::CreateCompatibleDC(hdc);
        HBITMAP old = (HBITMAP)::SelectObject(hdcMem, hbm);
        // 居中（不缩放）
        int x = (rc.left + rc.right - bi.bmWidth) / 2;
        int y = (rc.top  + rc.bottom - bi.bmHeight) / 2;
        ::BitBlt(hdc, x, y, bi.bmWidth, bi.bmHeight,
                 hdcMem, 0, 0, SRCCOPY);
        ::SelectObject(hdcMem, old);
        ::DeleteDC(hdcMem);
    });
}

void DuiEditHost::SetIconGlyph(IconSlot slot, int gutterWidth,
                                LPCTSTR glyph, COLORREF color)
{
    if (!glyph || glyph[0] == 0)
    {
        ClearIcon(slot);
        return;
    }
    CString text(glyph);
    SetIcon(slot, gutterWidth,
            [text, color](HDC hdc, const RECT& rc)
    {
        HFONT hOld = (HFONT)::SelectObject(hdc,
                          DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldC = ::SetTextColor(hdc, color);
        RECT r = rc;
        ::DrawText(hdc, (LPCTSTR)text, -1, &r,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOld);
    });
}

void DuiEditHost::ClearIcon(IconSlot slot)
{
    SetIcon(slot, 0, nullptr);
}

void DuiEditHost::SetIconClickable(IconSlot slot, bool clickable)
{
    IconState& st = (slot == LeftIcon) ? m_iconL : m_iconR;
    st.clickable = clickable;
}

bool DuiEditHost::IsIconClickable(IconSlot slot) const
{
    return (slot == LeftIcon) ? m_iconL.clickable : m_iconR.clickable;
}

int DuiEditHost::GetIconWidth(IconSlot slot) const
{
    return (slot == LeftIcon) ? m_iconL.width : m_iconR.width;
}

// 单测用纯函数 helper —— 给定 host rect + 边距 + slot + gutter 宽，
// 返回图标 RECT。无副作用，独立测试。
RECT DuiEditHost::ComputeIconRect(const RECT& rc, IconSlot slot,
                                   int gutterWidth, int borderPx,
                                   int marginVPx)
{
    if (gutterWidth <= 0)
    {
        RECT z = { 0, 0, 0, 0 };
        return z;
    }
    RECT r;
    r.top    = rc.top + borderPx + marginVPx;
    r.bottom = rc.bottom - borderPx - marginVPx;
    if (slot == LeftIcon)
    {
        r.left  = rc.left + borderPx;
        r.right = r.left + gutterWidth;
    }
    else
    {
        r.right = rc.right - borderPx;
        r.left  = r.right - gutterWidth;
    }
    return r;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT
