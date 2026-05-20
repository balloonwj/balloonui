#include "stdafx.h"
#include "HwndHostControl.h"
#include "DuiHost.h"

namespace balloonwjui {

HwndHostControl::HwndHostControl() = default;

void HwndHostControl::SetCtlBgColor(COLORREF c)
{
    if (m_ctlBg == c)
    {
        return;
    }
    m_ctlBg = c;
    if (m_ctlBgBrush)
    {
        ::DeleteObject(m_ctlBgBrush);
        m_ctlBgBrush = nullptr;
    }
    if (m_hwndChild && ::IsWindow(m_hwndChild))
    {
        ::InvalidateRect(m_hwndChild, nullptr, TRUE);
    }
}

void HwndHostControl::SetCtlTextColor(COLORREF c)
{
    if (m_ctlText == c)
    {
        return;
    }
    m_ctlText = c;
    if (m_hwndChild && ::IsWindow(m_hwndChild))
    {
        ::InvalidateRect(m_hwndChild, nullptr, TRUE);
    }
}

HBRUSH HwndHostControl::GetCtlColorBrush(UINT /*msgKind*/, HDC hdc)
{
    if (!hdc)
    {
        return nullptr;
    }
    if (m_ctlText != (COLORREF)CLR_INVALID)
    {
        ::SetTextColor(hdc, m_ctlText);
    }
    if (m_ctlBg == (COLORREF)CLR_INVALID)
    {
        return nullptr;     // fall through to DefWindowProc
    }

    ::SetBkColor(hdc, m_ctlBg);
    if (!m_ctlBgBrush)
    {
        m_ctlBgBrush = ::CreateSolidBrush(m_ctlBg);
    }
    return m_ctlBgBrush;
}

HwndHostControl::~HwndHostControl()
{
    if (m_hwndChild && m_bOwnsHwnd && ::IsWindow(m_hwndChild))
    {
        // Reparent the hosted HWND to the system message-only window
        // BEFORE DestroyWindow. Confirmed by instrumentation that this
        // succeeds for WS_CHILD EDITs (SetParent returns the old parent
        // and GetParent reports the message-only container afterwards),
        // and that no WM_COMMAND notifications reach DuiHost::OnCommand
        // during the subsequent DestroyWindow - which prevents the
        // walker from descending the parent's m_children mid-~vector
        // and dereferencing already-freed siblings.
        ::SetParent(m_hwndChild, HWND_MESSAGE);
        ::DestroyWindow(m_hwndChild);
    }
    m_hwndChild = nullptr;
    if (m_ctlBgBrush)
    {
        ::DeleteObject(m_ctlBgBrush);
        m_ctlBgBrush = nullptr;
    }
}

void HwndHostControl::Attach(HWND hwnd)
{
    m_hwndChild = hwnd;
    SyncWindowState();
}

HWND HwndHostControl::Detach()
{
    HWND h = m_hwndChild;
    m_hwndChild = nullptr;
    return h;
}

void HwndHostControl::Layout(const RECT& rcAvail)
{
    DuiControl::Layout(rcAvail);
    if (m_hwndChild && ::IsWindow(m_hwndChild))
    {
        ::SetWindowPos(m_hwndChild, nullptr,
                       rcAvail.left, rcAvail.top,
                       rcAvail.right - rcAvail.left,
                       rcAvail.bottom - rcAvail.top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void HwndHostControl::OnPaint(HDC, const RECT&)
{
    // The hosted HWND draws itself via its own WM_PAINT; nothing to do here.
}

DuiControl* HwndHostControl::HitTest(POINT pt)
{
    if (!m_bVisible || !::PtInRect(&m_rcItem, pt))
    {
        return nullptr;
    }
    // Hit ourselves so the host stops descending - Win32 routes mouse events
    // to the child HWND directly anyway. We don't expose children below an
    // HWND host through DUI hit testing.
    return m_bEnabled ? this : nullptr;
}

void HwndHostControl::SyncWindowState()
{
    if (!m_hwndChild || !::IsWindow(m_hwndChild))
    {
        return;
    }
    ::ShowWindow(m_hwndChild, m_bVisible ? SW_SHOW : SW_HIDE);
    ::EnableWindow(m_hwndChild, m_bEnabled ? TRUE : FALSE);
}

} // namespace balloonwjui
