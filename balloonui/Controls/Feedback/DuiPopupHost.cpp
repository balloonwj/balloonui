#include "stdafx.h"
#include "DuiPopupHost.h"

#if BUI_FEATURE_POPUPHOST


namespace balloonwjui {

DuiPopupHost::DuiPopupHost() = default;

DuiPopupHost::~DuiPopupHost()
{
    if (m_hWnd)
    {
        DestroyWindow();
    }
}

void DuiPopupHost::SetContent(std::unique_ptr<DuiControl> root)
{
    SetRoot(std::move(root));
    if (IsShowing())
    {
        // Re-layout root to current client size now that there is content.
        RECT rc;
        GetClientRect(&rc);
        if (DuiControl* r = GetRoot())
        {
            r->SetRect(rc);
        }
        Invalidate(FALSE);
    }
}

void DuiPopupHost::SetSize(int w, int h)
{
    if (w < 1)
    {
        w = 1;
    }
    if (h < 1)
    {
        h = 1;
    }
    m_width  = w;
    m_height = h;
}

void DuiPopupHost::SetEdge(Edge e)
{
    m_edge = e;
}

void DuiPopupHost::SetDismissCallback(DismissCallback cb, void* userdata)
{
    m_dismissCb = cb;
    m_dismissUd = userdata;
}

bool DuiPopupHost::IsShowing() const
{
    return m_hWnd && ::IsWindowVisible(m_hWnd);
}

void DuiPopupHost::EnsureCreated(HWND hOwner)
{
    if (m_hWnd)
    {
        return;
    }
    // 不能直接调继承来的 Create：基类是 CWindowImpl<DuiHost>，它按模板参数 DuiHost 取窗口类信息，注册的是
    // __DuiHost__，本类 DECLARE_WND_CLASS_EX 声明的 __DuiPopupHost__（带阴影）永远用不上（bugs.md BUG-101）。
    // 这里按 ATL CWindowImpl::Create 的同样步骤，只把窗口类信息换成本类的：先注册本类的窗口类，再按窗口类
    // 编号创建窗口。窗口过程仍是 ATL 的 StartWindowProc，消息照常经 thunk 派发给本对象的消息映射。
    ATL::CWndClassInfo& info = DuiPopupHost::GetWndClassInfo();
    if (info.m_lpszOrigName == NULL)
    {
        info.m_lpszOrigName = GetWndClassName();
    }
    const ATOM atom = info.Register(&m_pfnSuperWindowProc);
    ATL::CWindowImplBaseT<ATL::CWindow, ATL::CControlWinTraits>::Create(
        hOwner, NULL, NULL,
        WS_POPUP,
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        0U, atom, NULL);
}

// Pure helper: pick a placement for the popup given an anchor rect,
// preferred edge, popup size, and a screen work area. Auto-flips when
// the preferred edge would push the popup off the work area.
RECT DuiPopupHost::ComputePositionScreen(const RECT& anchor,
                                         int popupW, int popupH,
                                         Edge prefer,
                                         const RECT& work)
{
    auto fits = [&](int x, int y) -> bool
    {
        return x >= work.left && y >= work.top
            && x + popupW <= work.right
            && y + popupH <= work.bottom;
    };

    int x = anchor.left, y = anchor.top;

    auto tryPlace = [&](Edge e) -> bool
    {
        switch (e)
        {
        case EdgeBelow:
            x = anchor.left;
            y = anchor.bottom;
            break;
        case EdgeAbove:
            x = anchor.left;
            y = anchor.top - popupH;
            break;
        case EdgeRight:
            x = anchor.right;
            y = anchor.top;
            break;
        case EdgeLeft:
            x = anchor.left - popupW;
            y = anchor.top;
            break;
        }
        return fits(x, y);
    };

    Edge flip = prefer;
    if (!tryPlace(flip))
    {
        switch (prefer)
        {
        case EdgeBelow:
            flip = EdgeAbove;
            break;
        case EdgeAbove:
            flip = EdgeBelow;
            break;
        case EdgeRight:
            flip = EdgeLeft;
            break;
        case EdgeLeft:
            flip = EdgeRight;
            break;
        }
        if (!tryPlace(flip))
        {
            // Neither side fits; restore prefer's anchor and clamp to work area.
            tryPlace(prefer);
        }
    }

    // Final clamp against the work area so we never end up partially
    // off-screen even when neither edge could fit.
    if (x + popupW > work.right)
    {
        x = work.right  - popupW;
    }
    if (y + popupH > work.bottom)
    {
        y = work.bottom - popupH;
    }
    if (x < work.left)
    {
        x = work.left;
    }
    if (y < work.top)
    {
        y = work.top;
    }

    RECT r;
    r.left   = x;
    r.top    = y;
    r.right  = x + popupW;
    r.bottom = y + popupH;
    return r;
}

void DuiPopupHost::Show(const RECT& anchorScreen, HWND hOwner)
{
    EnsureCreated(hOwner);
    if (!m_hWnd)
    {
        return;
    }

    // Determine target monitor's work area for the auto-flip math.
    HMONITOR mon = ::MonitorFromRect(&anchorScreen, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    RECT work = { 0, 0, 1920, 1080 };
    if (mon && ::GetMonitorInfo(mon, &mi))
    {
        work = mi.rcWork;
    }

    RECT pos = ComputePositionScreen(anchorScreen, m_width, m_height, m_edge, work);

    SetWindowPos(HWND_TOPMOST,
                 pos.left, pos.top,
                 pos.right - pos.left, pos.bottom - pos.top,
                 SWP_SHOWWINDOW);

    // Force a re-layout of root to match the new client size.
    RECT rc;
    GetClientRect(&rc);
    if (DuiControl* r = GetRoot())
    {
        r->SetRect(rc);
    }
    Invalidate(FALSE);
}

void DuiPopupHost::Hide(Reason r)
{
    if (!m_hWnd)
    {
        return;
    }
    if (!::IsWindowVisible(m_hWnd))
    {
        return;
    }
    ShowWindow(SW_HIDE);
    FireDismiss(r);
}

void DuiPopupHost::FireDismiss(Reason r)
{
    if (m_inDismiss)
    {
        return;
    }
    m_inDismiss = true;
    if (m_dismissCb)
    {
        m_dismissCb(m_dismissUd, r);
    }
    m_inDismiss = false;
}

void DuiPopupHost::OnPopupKeyDown(TCHAR vk, UINT, UINT)
{
    if (vk == VK_ESCAPE)
    {
        Hide(ReasonEscape);
        return;
    }
    // Forward other keys through DuiHost's key routing.
    SetMsgHandled(FALSE);
}

void DuiPopupHost::OnActivate(UINT nState, BOOL, CWindow)
{
    // Auto-dismiss when the popup loses activation. WA_INACTIVE fires
    // when the user clicks elsewhere, alt-tabs, etc. Win32 sends this
    // *before* the click is dispatched to the new target, so a single
    // click outside the popup dismisses + the click reaches its real
    // target on the same gesture (which is what users expect).
    if (nState == WA_INACTIVE)
    {
        Hide(ReasonLostFocus);
        return;
    }
    SetMsgHandled(FALSE);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_POPUPHOST
