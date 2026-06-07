#include "stdafx.h"
#include "DuiHost.h"
#include "HwndHostControl.h"
#include "BalloonUiFeatures.h"
#if BUI_FEATURE_TOOLTIP
#  include "Controls/Feedback/DuiToolTip.h"
#endif
#include "DuiDpi.h"
#include "DuiResMgr.h"
#include "DuiNinePatch.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

DuiHost::DuiHost() = default;

DuiHost::~DuiHost()
{
    DestroyBackBuffer();
    // 释放通过 LoadBgImageFromFile 加载、由 host 持有的 bitmap。
    // 如果 caller 之后又调了 SetBgImage(其它 hbm)，m_hOwnedBgImage 与
    // m_hBgImage 可能不同 —— 都按各自的所有权处理：m_hOwnedBgImage 一定
    // 由 host 释放（因为是 host 加载的），m_hBgImage 如果不同就不动
    // （是 caller 传入的）。
    if (m_hOwnedBgImage)
    {
        ::DeleteObject(m_hOwnedBgImage);
        m_hOwnedBgImage = nullptr;
    }
}

void DuiHost::SetRoot(std::unique_ptr<DuiControl> root)
{
    // Suppress OnCommand for the entire SetRoot tear-down so any synchronous
    // EN_* notifications fired by sibling HWND-hosted children (whose focus
    // shifts as their peers get destroyed) don't run the m_root walker over
    // a tree being torn down.
    BeginTreeChange();
    m_pHover = m_pCapture = m_pFocus = nullptr;
    m_root = std::move(root);
    if (m_root)
    {
        m_root->AttachToHost(this);
        if (IsWindow())
        {
            CRect rc;
            GetClientRect(&rc);
            m_root->SetRect(rc);
        }
    }
    EndTreeChange();
    if (IsWindow())
    {
        Invalidate(FALSE);
    }
}

LRESULT DuiHost::SendNotify(DuiControl* ctrl, UINT code, LPARAM extra)
{
    DuiNotify n;
    n.code   = code;
    n.ctrlId = ctrl ? ctrl->GetCtrlId() : 0;
    n.pCtrl  = ctrl;
    n.extra  = extra;

    // Notification target. Normally the host's parent HWND (i.e. the
    // dialog or frame embedding the host as a child). When the host is
    // the top-level window itself (no parent — e.g. DuiFrameWindow),
    // self-route so a subclass's WM_DUI_NOTIFY handler can still react
    // to its own children's events. Without this fallback, caption
    // buttons inside DuiFrameWindow (min/max/close) fire DUIN_CLICK
    // into the void and the window stops responding to them.
    HWND hTarget = ::GetParent(m_hWnd);
    if (!hTarget)
    {
        hTarget = m_hWnd;
    }
    if (!hTarget)
    {
        return 0;
    }
    return ::SendMessage(hTarget, WM_DUI_NOTIFY,
                         (WPARAM)n.ctrlId, (LPARAM)&n);
}

void DuiHost::SetDuiCapture(DuiControl* ctrl)
{
    if (m_pCapture == ctrl)
    {
        return;
    }
    if (m_pCapture)
    {
        m_pCapture->m_bCapture = false;
    }
    m_pCapture = ctrl;
    if (m_pCapture)
    {
        m_pCapture->m_bCapture = true;
    }

    // Mirror to Win32 so we receive mouse events outside our client area.
    if (m_pCapture)
    {
        ::SetCapture(m_hWnd);
    }
    else if (::GetCapture() == m_hWnd)
    {
        ::ReleaseCapture();
    }
}

void DuiHost::ReleaseDuiCapture(DuiControl* ctrl)
{
    if (m_pCapture && (ctrl == nullptr || m_pCapture == ctrl))
    {
        SetDuiCapture(nullptr);
    }
}

void DuiHost::ClearHoverIfMatches(DuiControl* ctrl)
{
    if (m_pHover == ctrl)
    {
        m_pHover = nullptr;
    }
}

void DuiHost::SetDuiFocus(DuiControl* ctrl)
{
    if (m_pFocus == ctrl)
    {
        return;
    }
    DuiControl* old = m_pFocus;
    m_pFocus = ctrl;
    if (old)
    {
        old->OnKillFocus();
    }
    if (m_pFocus)
    {
        m_pFocus->OnSetFocus();
    }
}

void DuiHost::CollectTabStops(DuiControl* node, std::vector<DuiControl*>& out) const
{
    if (!node || !node->IsVisible())
    {
        return;
    }
    if (node->IsTabStop())
    {
        out.push_back(node);
    }
    for (auto& c : node->m_children)
    {
        CollectTabStops(c.get(), out);
    }
}

void DuiHost::FocusNext(bool backward)
{
    if (!m_root)
    {
        return;
    }
    std::vector<DuiControl*> stops;
    CollectTabStops(m_root.get(), stops);
    if (stops.empty())
    {
        return;
    }

    int idx = -1;
    for (size_t i = 0; i < stops.size(); ++i)
    {
        if (stops[i] == m_pFocus)
        {
            idx = (int)i;
            break;
        }
    }

    int next;
    if (idx < 0)
    {
        next = backward ? (int)stops.size() - 1 : 0;
    }
    else if (backward)
    {
        next = (idx - 1 + (int)stops.size()) % (int)stops.size();
    }
    else
    {
        next = (idx + 1) % (int)stops.size();
    }

    SetDuiFocus(stops[next]);
}

void DuiHost::InvalidateDuiRect(const RECT& rc)
{
    if (IsWindow())
    {
        InvalidateRect(&rc, FALSE);
    }
}

// --- Win32 message handlers ---

int DuiHost::OnCreate(LPCREATESTRUCT)
{
    // Cache per-monitor DPI so controls can read it without re-querying
    // every paint. Refreshed by WM_DPICHANGED later.
    m_dpi = DuiDpi::GetWindowDpi(m_hWnd);
    DuiResMgr::Inst().SetDpi(m_dpi);

    if (m_root)
    {
        m_root->AttachToHost(this);
        CRect rc;
        GetClientRect(&rc);
        m_root->SetRect(rc);
    }
    return 0;
}

LRESULT DuiHost::OnDpiChangedMsg(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    int dpi = LOWORD(wParam);
    if (dpi <= 0)
    {
        dpi = HIWORD(wParam);
    }
    if (dpi <= 0)
    {
        dpi = DuiDpi::kDefaultDpi;
    }
    m_dpi = dpi;
    DuiResMgr::Inst().SetDpi(dpi);

    // Apply the suggested rect (system-computed for the new monitor).
    if (lParam)
    {
        const RECT* sug = (const RECT*)lParam;
        SetWindowPos(nullptr, sug->left, sug->top,
                     sug->right - sug->left, sug->bottom - sug->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (m_root)
    {
        CRect rc;
        GetClientRect(&rc);
        m_root->SetRect(rc);
    }
    DestroyBackBuffer();        // size of buffer changes with new client
    Invalidate(FALSE);
    bHandled = TRUE;
    return 0;
}

void DuiHost::OnDestroy()
{
    m_pHover = m_pCapture = m_pFocus = nullptr;
    m_root.reset();
    DestroyBackBuffer();
}

void DuiHost::OnSize(UINT, CSize size)
{
    EnsureBackBuffer(size.cx, size.cy);
    if (m_root)
    {
        RECT rc{0, 0, size.cx, size.cy};
        m_root->SetRect(rc);
    }
    Invalidate(FALSE);
}

BOOL DuiHost::OnEraseBkgnd(CDCHandle)
{
    return TRUE;   // suppress; OnPaint paints the whole rect from back buffer
}

void DuiHost::OnPaint(CDCHandle)
{
    CPaintDC dc(m_hWnd);
    CRect rcClient;
    GetClientRect(&rcClient);
    EnsureBackBuffer(rcClient.Width(), rcClient.Height());

    if (m_hBgImage)
    {
        // 9-grid 背景。源 / 目标 inset 分别给 — 当两者相等时退化为
        // 经典 9-grid（角 1:1 不缩）；不同时角也会缩放 (src→dst)，
        // 让源图的装饰带可压缩 / 拉伸到目标的指定高度。
        DuiNinePatch::Insets srcIns;
        srcIns.left   = m_bgSrcInsetLeft;
        srcIns.top    = m_bgSrcInsetTop;
        srcIns.right  = m_bgSrcInsetRight;
        srcIns.bottom = m_bgSrcInsetBottom;
        DuiNinePatch::Insets dstIns;
        dstIns.left   = m_bgDstInsetLeft;
        dstIns.top    = m_bgDstInsetTop;
        dstIns.right  = m_bgDstInsetRight;
        dstIns.bottom = m_bgDstInsetBottom;
        DuiNinePatch::Draw(m_hMemDC, m_hBgImage, rcClient,
                           srcIns, dstIns);
    }
    else
    {
        // Clear back buffer with COLOR_BTNFACE (subclasses may paint over it).
        HBRUSH hbr = ::GetSysColorBrush(COLOR_BTNFACE);
        ::FillRect(m_hMemDC, &rcClient, hbr);
    }

    if (m_root)
    {
        m_root->OnPaint(m_hMemDC, rcClient);
    }

    // 客户区 1px 边框 —— 仅当 caller 显式设了颜色 + 没有 bg 图时画。
    // 这条线压在所有内容最上面，所以即使 root 控件填到边缘，也能看见
    // 窗口外轮廓。bg 图存在时跳过，因为图本身（圆角 + 阴影 + 装饰）
    // 已经表达了边界，再叠 1px 直线会把圆角切方。
    if (m_clientBorderColor != CLR_INVALID && !m_hBgImage)
    {
        HBRUSH hbrBorder = ::CreateSolidBrush(m_clientBorderColor);
        ::FrameRect(m_hMemDC, &rcClient, hbrBorder);
        ::DeleteObject(hbrBorder);
    }

    ::BitBlt(dc, 0, 0, rcClient.Width(), rcClient.Height(),
             m_hMemDC, 0, 0, SRCCOPY);
}

void DuiHost::SetClientBorderColor(COLORREF c)
{
    if (m_clientBorderColor == c)
    {
        return;
    }
    m_clientBorderColor = c;
    if (IsWindow())
    {
        Invalidate(FALSE);
    }
}

void DuiHost::SetBgImage(HBITMAP hbm, const RECT& insets)
{
    // 单 inset 便捷重载 — 源 == 目标，等价于经典 9-grid（角 1:1 不缩）
    SetBgImage(hbm, insets, insets);
}

void DuiHost::SetBgImage(HBITMAP hbm,
                         const RECT& srcInsets,
                         const RECT& dstInsets)
{
    // 切换 bg 图源时，如果之前 host 持有过 owned bitmap（来自
    // LoadBgImageFromFile），且新 hbm 与它不同，则释放旧的 —— 否则永久
    // 泄漏（caller 不知道 owned bitmap 存在）。新 hbm 默认按 caller-owned
    // 处理，host 不释放它。
    if (m_hOwnedBgImage && m_hOwnedBgImage != hbm)
    {
        ::DeleteObject(m_hOwnedBgImage);
        m_hOwnedBgImage = nullptr;
    }
    m_hBgImage         = hbm;
    m_bgSrcInsetLeft   = srcInsets.left;
    m_bgSrcInsetTop    = srcInsets.top;
    m_bgSrcInsetRight  = srcInsets.right;
    m_bgSrcInsetBottom = srcInsets.bottom;
    m_bgDstInsetLeft   = dstInsets.left;
    m_bgDstInsetTop    = dstInsets.top;
    m_bgDstInsetRight  = dstInsets.right;
    m_bgDstInsetBottom = dstInsets.bottom;
    if (IsWindow())
    {
        Invalidate(FALSE);
    }
}

bool DuiHost::LoadBgImageFromFile(LPCTSTR path, const RECT& insets)
{
    return LoadBgImageFromFile(path, insets, insets);
}

bool DuiHost::LoadBgImageFromFile(LPCTSTR path,
                                  const RECT& srcInsets,
                                  const RECT& dstInsets)
{
    if (!path || !*path)
    {
        return false;
    }
    if (::GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    // GDI+ 进程级 startup —— 库内首次调用时跑一次，进程退出由 OS 收。
    // 不主动 Shutdown，因为 caller 可能还有别的 GDI+ 用户（demo 就是）。
    static ULONG_PTR s_gdiplusToken = 0;
    if (!s_gdiplusToken)
    {
        Gdiplus::GdiplusStartupInput gsi;
        if (Gdiplus::GdiplusStartup(&s_gdiplusToken, &gsi, nullptr) != Gdiplus::Ok)
        {
            return false;
        }
    }

    Gdiplus::Bitmap src(path);
    if (src.GetLastStatus() != Gdiplus::Ok)
    {
        return false;
    }

    // 用白底预合成 alpha → 输出 24/32bpp DDB，BitBlt / StretchBlt /
    // DuiNinePatch::Draw 都视它为不透明。规避两个常见坑：
    //   · PNG alpha=0 直接 BitBlt 渲染成黑色（颜色 = RGB×alpha=0）
    //   · 32bpp DIB section 的 RGBA 字节序与 GDI 假设不一致
    HBITMAP hbm = nullptr;
    if (src.GetHBITMAP(Gdiplus::Color(255, 255, 255, 255), &hbm) != Gdiplus::Ok || !hbm)
    {
        return false;
    }

    // SetBgImage 会在 m_hOwnedBgImage != hbm 时释放旧 owned —— 我们要先
    // 把 owned 字段更新为新 hbm，再调 SetBgImage，否则 SetBgImage 会把
    // 我们刚加载的新 hbm 当成"前一份 owned"误删。顺序：先清空 owned，
    // SetBgImage 用新 hbm，最后把 owned 标记成新 hbm。
    if (m_hOwnedBgImage)
    {
        ::DeleteObject(m_hOwnedBgImage);
        m_hOwnedBgImage = nullptr;
    }
    SetBgImage(hbm, srcInsets, dstInsets);
    m_hOwnedBgImage = hbm;
    return true;
}

BOOL DuiHost::OnSetCursor(CWindow, UINT nHitTest, UINT)
{
    if (nHitTest != HTCLIENT)
    {
        // 非客户区（HTLEFT/HTRIGHT/HTTOP/HTBOTTOM/HTTOPLEFT/HTTOPRIGHT/
        // HTBOTTOMLEFT/HTBOTTOMRIGHT/HTCAPTION/...）交给 DefWindowProc，
        // 让 OS 按 hittest 设对应光标 — 4 边 resize 光标 IDC_SIZEWE /
        // IDC_SIZENS、4 角 IDC_SIZENWSE / IDC_SIZENESW、HTCAPTION 上的
        // 箭头 IDC_ARROW 等。
        //
        // 注意：MSG_WM_SETCURSOR 宏默认 SetMsgHandled(TRUE)，仅 return
        // FALSE 不够 — 必须显式 SetMsgHandled(FALSE) 才会沿消息链走到
        // DefWindowProc。否则 OS 拿到 lResult=0 + "已处理"信号，光标
        // 保持上一次的状态（通常是 IDC_ARROW），用户在边缘看不到 resize
        // 光标。
        SetMsgHandled(FALSE);
        return FALSE;
    }
    POINT pt;
    ::GetCursorPos(&pt);
    ScreenToClient(&pt);
    DuiControl* hit = m_pCapture ? m_pCapture : HitTopMost(pt);
    if (hit && hit->OnSetCursor(pt))
    {
        return TRUE;
    }
    ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
    return TRUE;
}

DuiControl* DuiHost::HitTopMost(POINT pt)
{
    return m_root ? m_root->HitTest(pt) : nullptr;
}

void DuiHost::StartMouseTracking()
{
    if (m_bMouseTracking)
    {
        return;
    }
    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
    if (::TrackMouseEvent(&tme))
    {
        m_bMouseTracking = true;
    }
}

void DuiHost::TrackHover(POINT pt)
{
    DuiControl* tgt = m_pCapture ? m_pCapture : HitTopMost(pt);
    if (tgt == m_pHover)
    {
        return;
    }
    DuiControl* old = m_pHover;
    m_pHover = tgt;
    if (old)
    {
        old->OnMouseLeave();
    }
    if (m_pHover)
    {
        m_pHover->OnMouseEnter();
    }

    // Tooltip dispatch: leave the old hover, then start the new one.
    // Skipped when BUI_FEATURE_TOOLTIP is off (no DuiToolTipMgr type).
#if BUI_FEATURE_TOOLTIP
    DuiToolTipMgr& tt = DuiToolTipMgr::Inst();
    if (old)
    {
        tt.OnLeave(old);
    }
    if (m_pHover)
    {
        POINT screen = pt;
        ::ClientToScreen(m_hWnd, &screen);
        tt.OnHover(m_pHover, screen);
    }
#endif
}

void DuiHost::OnMouseMove(UINT mkFlags, CPoint pt)
{
    StartMouseTracking();
    TrackHover(pt);
    DuiControl* tgt = m_pCapture ? m_pCapture : m_pHover;
    if (tgt)
    {
        tgt->OnMouseMove(pt, mkFlags);
    }
}

void DuiHost::OnMouseLeave()
{
    m_bMouseTracking = false;
    if (m_pHover && !m_pCapture)
    {
        DuiControl* old = m_pHover;
        m_pHover = nullptr;
        old->OnMouseLeave();
#if BUI_FEATURE_TOOLTIP
        DuiToolTipMgr::Inst().OnLeave(old);
#endif
    }
#if BUI_FEATURE_TOOLTIP
    else
    {
        // No hover or held by capture; still hide any visible tip.
        DuiToolTipMgr::Inst().HideNow();
    }
#endif
}

void DuiHost::OnLButtonDown(UINT mkFlags, CPoint pt)
{
#if BUI_FEATURE_TOOLTIP
    DuiToolTipMgr::Inst().HideNow();
#endif
    DuiControl* tgt = m_pCapture ? m_pCapture : HitTopMost(pt);
    if (!tgt)
    {
        return;
    }
    if (tgt->IsTabStop())
    {
        SetDuiFocus(tgt);
    }

    // 双击合成（见 EnableDoubleClick）：开启后按系统双击时限 + 位移容差
    // 判定本次按下是否构成双击。命中则改派 OnLButtonDblClk —— 与系统
    // CS_DBLCLKS 语义一致：第二次按下以双击形式派发，不再额外派发一次
    // OnLButtonDown。
    if (m_dblClkEnabled)
    {
        const DWORD now = static_cast<DWORD>(::GetMessageTime());
        const bool isDbl =
            (now - m_lastLDownTick) <= ::GetDoubleClickTime() &&
            abs(pt.x - m_lastLDownPt.x) <= ::GetSystemMetrics(SM_CXDOUBLECLK) &&
            abs(pt.y - m_lastLDownPt.y) <= ::GetSystemMetrics(SM_CYDOUBLECLK);
        if (isDbl)
        {
            m_lastLDownTick = 0;   // 复位，避免三击被当成又一次双击
            tgt->OnLButtonDblClk(pt, mkFlags);
            return;
        }
        m_lastLDownTick = now;
        m_lastLDownPt   = pt;
    }

    tgt->OnLButtonDown(pt, mkFlags);
}

void DuiHost::OnLButtonUp(UINT mkFlags, CPoint pt)
{
    DuiControl* tgt = m_pCapture ? m_pCapture : HitTopMost(pt);
    if (tgt)
    {
        tgt->OnLButtonUp(pt, mkFlags);
    }
}

void DuiHost::OnLButtonDblClk(UINT mkFlags, CPoint pt)
{
    DuiControl* tgt = m_pCapture ? m_pCapture : HitTopMost(pt);
    if (tgt)
    {
        tgt->OnLButtonDblClk(pt, mkFlags);
    }
}

void DuiHost::OnRButtonDown(UINT mkFlags, CPoint pt)
{
    DuiControl* tgt = m_pCapture ? m_pCapture : HitTopMost(pt);
    if (tgt)
    {
        tgt->OnRButtonDown(pt, mkFlags);
    }
}

BOOL DuiHost::OnMouseWheel(UINT mkFlags, short zDelta, CPoint pt)
{
    POINT ptClient = pt;
    ScreenToClient(&ptClient);
    // 滚轮按<u>鼠标位置</u>路由 —— 不走 focus。Web/macOS/Win 系统菜单 /
    // Explorer / 某信 PC 都是这条标准行为：鼠标 hover 在哪个滚动容器，
    // 滚轮就滚那个。原本曾用 `m_pFocus ? m_pFocus : HitTopMost(...)` 优
    // 先 focus，结果是某个 list 被点过获得 focus 后，鼠标移到别处滚轮
    // 仍滚原 list（典型 bug：右栏 chat 滚不动，session-list 在滚）。
    // 键盘事件（OnChar/OnKeyDown）继续走 focus，下方各自分支。
    DuiControl* tgt = HitTopMost(ptClient);
    if (tgt && tgt->OnMouseWheel(ptClient, zDelta, mkFlags))
    {
        return TRUE;
    }
    SetMsgHandled(FALSE);
    return FALSE;
}

void DuiHost::OnChar(TCHAR ch, UINT, UINT)
{
    if (m_pFocus)
    {
        m_pFocus->OnChar(ch);
    }
}

void DuiHost::OnKeyDown(TCHAR vk, UINT, UINT flags)
{
    if (vk == VK_TAB)
    {
        bool back = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        FocusNext(back);
        return;
    }
    if (m_pFocus)
    {
        m_pFocus->OnKeyDown(vk, flags);
    }
}

void DuiHost::OnSetFocus(CWindow)
{
    if (m_pFocus)
    {
        m_pFocus->OnSetFocus();
    }
    else if (m_root)
    {
        FocusNext(false);
    }
}

void DuiHost::OnKillFocus(CWindow)
{
    if (m_pFocus)
    {
        m_pFocus->OnKillFocus();
    }
}

// Recursive walker to access protected DuiControl::m_children.
class DuiHost_ChildWalker : public DuiControl
{
public:
    static HwndHostControl* FindByHwnd(DuiControl* node, HWND target)
    {
        if (!node)
        {
            return nullptr;
        }
        if (HwndHostControl* h = dynamic_cast<HwndHostControl*>(node))
        {
            if (h->GetHostedHwnd() == target)
            {
                return h;
            }
        }
        DuiHost_ChildWalker* w = static_cast<DuiHost_ChildWalker*>(node);
        for (auto& up : w->m_children)
        {
            if (HwndHostControl* hit = FindByHwnd(up.get(), target))
            {
                return hit;
            }
        }
        return nullptr;
    }
};

void DuiHost::OnCommand(UINT codeNotify, int /*nID*/, HWND hwndCtl)
{
    // Tree mutation in progress (SetContent, SetRoot, RemoveChild that
    // may unwind HWND-hosted children). Don't walk m_root - it's being
    // destructed in place and any sibling pointer in m_children may have
    // already been freed by ~vector before the WM_COMMAND fires from a
    // still-alive sibling whose focus shifted.
    if (m_suppressOnCommand > 0)
    {
        SetMsgHandled(FALSE);
        return;
    }
    if (!hwndCtl)
    {
        SetMsgHandled(FALSE);
        return;
    }
    HwndHostControl* h = DuiHost_ChildWalker::FindByHwnd(m_root.get(), hwndCtl);
    if (h)
    {
        h->OnHwndCommand(codeNotify);
        return;
    }
    SetMsgHandled(FALSE);
}

LRESULT DuiHost::OnNotifyMsg(UINT, WPARAM, LPARAM lParam)
{
    // Same suppression rule as OnCommand: don't walk a tree being torn down.
    if (m_suppressOnCommand > 0)
    {
        SetMsgHandled(FALSE);
        return 0;
    }
    NMHDR* pnmh = (NMHDR*)lParam;
    if (!pnmh || !pnmh->hwndFrom)
    {
        SetMsgHandled(FALSE);
        return 0;
    }
    HwndHostControl* h = DuiHost_ChildWalker::FindByHwnd(m_root.get(), pnmh->hwndFrom);
    if (h)
    {
        return h->OnHwndNotify(pnmh);
    }
    SetMsgHandled(FALSE);
    return 0;
}

LRESULT DuiHost::OnCtlColorMsg(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (m_suppressOnCommand > 0)
    {
        bHandled = FALSE;
        return 0;
    }
    HDC  hdc      = (HDC)wParam;
    HWND hwndCtl  = (HWND)lParam;
    if (!hwndCtl)
    {
        bHandled = FALSE;
        return 0;
    }
    HwndHostControl* h = DuiHost_ChildWalker::FindByHwnd(m_root.get(), hwndCtl);
    if (!h)
    {
        bHandled = FALSE;
        return 0;
    }
    HBRUSH brush = h->GetCtlColorBrush(msg, hdc);
    if (!brush)
    {
        // Adapter declined; let DefWindowProc supply the system brush.
        bHandled = FALSE;
        return 0;
    }
    bHandled = TRUE;
    return (LRESULT)brush;
}

// --- Back buffer ---

void DuiHost::EnsureBackBuffer(int cx, int cy)
{
    if (cx <= 0 || cy <= 0)
    {
        return;
    }
    if (m_hMemDC && cx == m_cxBuf && cy == m_cyBuf)
    {
        return;
    }

    DestroyBackBuffer();

    HDC hScreen = ::GetDC(m_hWnd);
    m_hMemDC  = ::CreateCompatibleDC(hScreen);
    m_hMemBmp = ::CreateCompatibleBitmap(hScreen, cx, cy);
    ::ReleaseDC(m_hWnd, hScreen);

    m_hOldBmp = (HBITMAP)::SelectObject(m_hMemDC, m_hMemBmp);
    m_cxBuf = cx;
    m_cyBuf = cy;
}

void DuiHost::DestroyBackBuffer()
{
    if (m_hMemDC)
    {
        if (m_hOldBmp)
        {
            ::SelectObject(m_hMemDC, m_hOldBmp);
        }
        ::DeleteDC(m_hMemDC);
        m_hMemDC = nullptr;
        m_hOldBmp = nullptr;
    }
    if (m_hMemBmp)
    {
        ::DeleteObject(m_hMemBmp);
        m_hMemBmp = nullptr;
    }
    m_cxBuf = m_cyBuf = 0;
}

} // namespace balloonwjui
