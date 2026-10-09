#include "stdafx.h"
#include "DuiHost.h"
#include "BalloonUiFeatures.h"
#if BUI_FEATURE_TOOLTIP
#  include "Controls/Feedback/DuiToolTip.h"
#endif
#include "DuiDpi.h"
#include "DuiResMgr.h"
#include "DuiNinePatch.h"
#include "DuiTrace.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// 拖放分发器要用的注册接口。
#include <ole2.h>
#pragma comment(lib, "ole32.lib")

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
    // 整个更换根控件的过程都标记为「控件树变更中」：旧的整棵树在这里就地析构，
    // 期间宿主不得对 m_root 做任何遍历。
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

        // 纯 DUI 控件想收键盘消息，必须由宿主窗口先把 Win32 焦点拿到手 ——
        // 否则字符与按键消息会一路投递给宿主的父窗口，控件一个也收不到。
        // 只有主动声明需要的控件才走这一步，现有控件行为不变。
        if (m_pFocus->NeedsWin32Focus())
        {
            EnsureWin32Focus();
        }
    }
}

void DuiHost::EnsureWin32Focus()
{
    if (!IsWindow())
    {
        return;
    }

    HWND hwndFocus = ::GetFocus();
    if (hwndFocus == m_hWnd)
    {
        // 已经在手上，不必再要。
        return;
    }

    // ::SetFocus 会同步发出本窗口的 WM_SETFOCUS，而那里又会给当前 DUI 焦点
    // 控件发一次获焦通知。刚才 SetDuiFocus 已经发过了，这里用标志压住，
    // 免得同一次获焦发出两条通知（业务侧常据此做「开始编辑」之类的动作，
    // 发两次会出问题）。
    m_bSyncingWin32Focus = true;
    ::SetFocus(m_hWnd);
    m_bSyncingWin32Focus = false;
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

// =====================================================================
// 拖放分发器
// =====================================================================
//
// 收到系统的拖放事件后按光标位置找到命中的控件，把调用原样转发给它的
// 拖放目标。控件之间切换时负责补发离开与进入，让每个控件看到的调用序列
// 都是完整的「进入 → 若干次经过 → 离开或放下」。
class DuiHost::DropDispatch : public IDropTarget
{
public:
    explicit DropDispatch(DuiHost* pHost)
        : m_cRef(1)
        , m_pHost(pHost)
        , m_pCurrent(nullptr)
        , m_pData(nullptr)
    {
    }

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (ppv == nullptr)
        {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (::IsEqualIID(riid, IID_IUnknown) || ::IsEqualIID(riid, IID_IDropTarget))
        {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++m_cRef;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG n = --m_cRef;
        if (n == 0)
        {
            delete this;
        }
        return n;
    }

    // ---- IDropTarget ----

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
                                        POINTL pt, DWORD* pdwEffect) override
    {
        // 记下数据对象：控件之间切换时要拿它去给新控件补一次「进入」。
        ReleaseData();
        m_pData = pDataObj;
        if (m_pData != nullptr)
        {
            m_pData->AddRef();
        }
        return RouteTo(FindTargetAt(pt), pDataObj, grfKeyState, pt, pdwEffect,
                       /*bEnter=*/true);
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt,
                                       DWORD* pdwEffect) override
    {
        IDropTarget* pNew = FindTargetAt(pt);
        if (pNew != m_pCurrent)
        {
            // 换控件了：给旧的补一次「离开」，给新的补一次「进入」，
            // 这样每个控件看到的调用序列都是完整的。少了这一步，控件会
            // 收到没有前置「进入」的「经过」，多数实现会直接拒收。
            if (m_pCurrent != nullptr)
            {
                m_pCurrent->DragLeave();
            }
            m_pCurrent = nullptr;
            return RouteTo(pNew, m_pData, grfKeyState, pt, pdwEffect,
                           /*bEnter=*/true);
        }

        if (m_pCurrent == nullptr)
        {
            if (pdwEffect != nullptr)
            {
                *pdwEffect = DROPEFFECT_NONE;
            }
            return S_OK;
        }
        return m_pCurrent->DragOver(grfKeyState, pt, pdwEffect);
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        HRESULT hr = S_OK;
        if (m_pCurrent != nullptr)
        {
            hr = m_pCurrent->DragLeave();
            m_pCurrent = nullptr;
        }
        ReleaseData();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState,
                                   POINTL pt, DWORD* pdwEffect) override
    {
        HRESULT hr = S_OK;
        if (m_pCurrent != nullptr)
        {
            hr = m_pCurrent->Drop(pDataObj, grfKeyState, pt, pdwEffect);
            m_pCurrent = nullptr;
        }
        else if (pdwEffect != nullptr)
        {
            *pdwEffect = DROPEFFECT_NONE;
        }
        ReleaseData();
        return hr;
    }

private:
    ~DropDispatch()
    {
        ReleaseData();
    }

    void ReleaseData()
    {
        if (m_pData != nullptr)
        {
            m_pData->Release();
            m_pData = nullptr;
        }
    }

    // 按屏幕坐标找出该由谁接收。
    IDropTarget* FindTargetAt(POINTL pt)
    {
        if (m_pHost == nullptr || !m_pHost->IsWindow())
        {
            return nullptr;
        }
        POINT ptClient;
        ptClient.x = pt.x;
        ptClient.y = pt.y;
        // 系统给的是屏幕坐标，控件用的是宿主客户区坐标，必须换算。
        ::ScreenToClient(m_pHost->m_hWnd, &ptClient);

        DuiControl* pCtrl = m_pHost->FindDropTargetControl(ptClient);
        return (pCtrl != nullptr) ? pCtrl->GetDropTarget() : nullptr;
    }

    // 把「进入」转发给指定目标并记为当前目标。目标为空时报告「不能放」。
    HRESULT RouteTo(IDropTarget* pTarget, IDataObject* pDataObj, DWORD grfKeyState,
                    POINTL pt, DWORD* pdwEffect, bool bEnter)
    {
        if (pTarget == nullptr || pDataObj == nullptr)
        {
            if (pdwEffect != nullptr)
            {
                *pdwEffect = DROPEFFECT_NONE;
            }
            return S_OK;
        }
        m_pCurrent = pTarget;
        if (bEnter)
        {
            return m_pCurrent->DragEnter(pDataObj, grfKeyState, pt, pdwEffect);
        }
        return m_pCurrent->DragOver(grfKeyState, pt, pdwEffect);
    }

private:
    ULONG        m_cRef;       // COM 引用计数；只在 UI 线程访问，不做原子操作
    DuiHost*     m_pHost;      // 所属宿主；不持有所有权
    IDropTarget* m_pCurrent;   // 当前正在接收的控件目标；所有权归控件，不释放
    IDataObject* m_pData;      // 本次拖动的数据对象；持有一份引用
};

bool DuiHost::EnableDropDispatch(bool b)
{
    if (!b)
    {
        if (m_pDropDispatch != nullptr)
        {
            if (IsWindow())
            {
                ::RevokeDragDrop(m_hWnd);
            }
            m_pDropDispatch->Release();
            m_pDropDispatch = nullptr;
        }
        return true;
    }

    if (m_pDropDispatch != nullptr)
    {
        return true;        // 已经开着，重复调用无害
    }
    if (!IsWindow())
    {
        return false;
    }

    DropDispatch* pDispatch = new DropDispatch(this);
    HRESULT hr = ::RegisterDragDrop(m_hWnd, pDispatch);
    if (FAILED(hr))
    {
        // 最常见的失败是这个窗口已经被别的代码注册过拖放目标 —— 一个窗口
        // 只能有一个。此时不报错、不崩溃，只是控件的「拖入」用不了；
        // 谁先注册谁生效，后来者安静退让。
        pDispatch->Release();
        return false;
    }
    m_pDropDispatch = pDispatch;
    return true;
}

DuiControl* DuiHost::FindDropTargetControl(POINT ptClient)
{
    DuiControl* pCtrl = HitTopMost(ptClient);
    // 从命中的最深控件起沿父链往上找 —— 命中的可能是个不接收拖放的子控件
    // （比如富文本控件内部的滚动条），真正想接收的是它的某一层祖先。
    while (pCtrl != nullptr)
    {
        if (pCtrl->GetDropTarget() != nullptr)
        {
            return pCtrl;
        }
        pCtrl = pCtrl->GetParent();
    }
    return nullptr;
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
    m_hasWindowDpi = true;
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
    m_hasWindowDpi = true;
    DuiResMgr::Inst().SetDpi(dpi);

    // 先把新 DPI 通知到每个控件，让按 DPI 缓存了字体或尺寸的控件刷新缓存，
    // 下面移动窗口触发的布局用的才是新参数。
    if (m_root)
    {
        m_root->DispatchDpiChanged_(dpi);
    }

    // Apply the suggested rect (system-computed for the new monitor).
    if (lParam)
    {
        const RECT* sug = (const RECT*)lParam;
        SetWindowPos(nullptr, sug->left, sug->top,
                     sug->right - sug->left, sug->bottom - sug->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // 整棵树重新布局。用 ForceLayout 而不是 SetRect：客户区矩形可能没变（lParam
    // 为空，或建议矩形与原尺寸相同），SetRect 会因此直接返回；矩形没变的子控件
    // 也要逐层重排，因为它们的内部排版依赖字号。
    if (m_root)
    {
        CRect rc;
        GetClientRect(&rc);
        m_root->ForceLayout(rc);
        m_root->RelayoutDescendants_();
    }
    DestroyBackBuffer();        // size of buffer changes with new client
    Invalidate(FALSE);
    bHandled = TRUE;
    return 0;
}

void DuiHost::OnDestroy()
{
    // 先注销拖放，再拆控件树 —— 反过来的话，注销过程中若还有拖放事件
    // 进来，会转发到正在销毁的控件上。
    EnableDropDispatch(false);

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

void DuiHost::RequestRelayout()
{
    // 窗口还没建出来（或已经销毁）时无处投递，直接返回。等窗口建好后
    // 会走一次 WM_SIZE，那次排版自然会用上最新的期望尺寸。
    if (m_hWnd == nullptr || !::IsWindow(m_hWnd))
    {
        return;
    }
    // 已经有一条排队中的请求了，本次直接合并进去。
    if (m_bRelayoutPending)
    {
        return;
    }
    m_bRelayoutPending = true;
    ::PostMessage(m_hWnd, kMsgRelayout, 0, 0);
}

LRESULT DuiHost::OnRelayoutMsg(UINT, WPARAM, LPARAM)
{
    // 先清标志再排版：排版过程中控件如果又发现自己的期望尺寸变了，
    // 应当能再投递一次请求，而不是被这一次的「排队中」状态挡掉。
    m_bRelayoutPending = false;

    if (m_root)
    {
        CRect rcClient;
        GetClientRect(&rcClient);
        // 这里必须用 ForceLayout 而不是 SetRect。SetRect 在矩形没变时会直接
        // 返回，而重排请求的场景恰恰是「窗口尺寸没变，变的是某个子控件想要
        // 多大」—— 用 SetRect 的话这条路径永远不会真的重排。
        m_root->ForceLayout(rcClient);
    }
    Invalidate(FALSE);
    return 0;
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

    // 把系统认定的待重画区域一并记下 —— 排查「界面更新不及时」时，
    // 需要区分「系统认为要重画的区域太大」和「重画本身太慢」。
    BUI_TRACE("HOST-PAINT-BEGIN update=(%d,%d,%d,%d) client=(%d,%d)",
              (int)dc.m_ps.rcPaint.left, (int)dc.m_ps.rcPaint.top,
              (int)dc.m_ps.rcPaint.right, (int)dc.m_ps.rcPaint.bottom,
              (int)rcClient.Width(), (int)rcClient.Height());

    // 缓冲容量不足时会在这里重建。重建出来的位图内容是未初始化的随机像素，
    // 那种情况必须整块重画。
    const bool bBufferRecreated = EnsureBackBuffer(rcClient.Width(), rcClient.Height());

    // 本次真正要重画的区域。
    //
    // 系统在 BeginPaint 时已经把合并好的脏区算得很准（打一个字通常只有一行
    // 文字那么高），照它办就能跳过绝大部分控件。**以前这里取的是整个客户区，
    // 等于把系统算好的信息丢掉，每次重画都要把整棵控件树画一遍** —— 静态
    // 界面偶尔全量重画一次察觉不到，但文本输入每敲一个键就要求重画一次，
    // 累积起来就是肉眼可见的迟钝。
    //
    // 两种情况退回整块：缓冲刚重建（内容是随机像素），以及脏区为空（不该
    // 发生，保险起见）。
    CRect rcPaint = dc.m_ps.rcPaint;
    if (bBufferRecreated || rcPaint.IsRectEmpty())
    {
        rcPaint = rcClient;
    }

    // 把内存设备上下文裁剪到脏区。
    //
    // **这一步是正确性保障，不是性能优化。** 很多控件的绘制函数并不看传进来
    // 的脏区，上来就把自己的整个矩形铺满（铺背景色、画卡片底）—— 在原来
    // 「每次都整块重画」的模型下这么写完全合理。若只把脏区传下去而不加裁剪，
    // 一个大容器会把整片区域刷成背景色，而它那些不与脏区相交的子控件又不会
    // 重画，结果就是脏区之外的内容被抹掉。加了裁剪，这类越界绘制被硬性挡在
    // 脏区之内，缓冲里其余部分原样保留。
    //
    // 注意裁剪只限制「画到哪儿」，不改变「画成什么样」：下面九宫格背景与
    // 客户区边框的几何仍按完整客户区计算，否则图案会跟着脏区跑。
    const int nSavedDC = (m_hMemDC != nullptr) ? ::SaveDC(m_hMemDC) : 0;
    if (nSavedDC != 0)
    {
        ::IntersectClipRect(m_hMemDC, rcPaint.left, rcPaint.top,
                            rcPaint.right, rcPaint.bottom);
    }

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
        // 纯色背景直接只铺脏区 —— 裁剪已经保证不会越界，这里显式给小矩形
        // 是为了连遍历都省掉。
        HBRUSH hbr = ::GetSysColorBrush(COLOR_BTNFACE);
        ::FillRect(m_hMemDC, &rcPaint, hbr);
    }

    BUI_TRACE("HOST-PAINT-BG-DONE paint=(%d,%d,%d,%d) full=%d",
              (int)rcPaint.left, (int)rcPaint.top,
              (int)rcPaint.right, (int)rcPaint.bottom,
              bBufferRecreated ? 1 : 0);

    if (m_root)
    {
        // 传脏区而不是整个客户区：控件树的默认实现按「子控件矩形与脏区是否
        // 相交」决定要不要往下递归，传得准，无关的整条分支根本不会进去。
        m_root->OnPaint(m_hMemDC, rcPaint);
    }

    BUI_TRACE("HOST-PAINT-TREE-DONE");

    // 客户区 1px 边框 —— 仅当 caller 显式设了颜色 + 没有 bg 图时画。
    // 这条线压在所有内容最上面，所以即使 root 控件填到边缘，也能看见
    // 窗口外轮廓。bg 图存在时跳过，因为图本身（圆角 + 阴影 + 装饰）
    // 已经表达了边界，再叠 1px 直线会把圆角切方。
    if (m_clientBorderColor != CLR_INVALID && !m_hBgImage)
    {
        // 几何按完整客户区算，超出脏区的部分由上面那道裁剪挡掉。
        HBRUSH hbrBorder = ::CreateSolidBrush(m_clientBorderColor);
        ::FrameRect(m_hMemDC, &rcClient, hbrBorder);
        ::DeleteObject(hbrBorder);
    }

    if (nSavedDC != 0)
    {
        ::RestoreDC(m_hMemDC, nSavedDC);
    }

    // 只把脏区那一块拷上屏。缓冲里其余部分仍是上一帧的正确画面，屏幕上
    // 对应位置也没有变化，不需要搬运。
    ::BitBlt(dc, rcPaint.left, rcPaint.top, rcPaint.Width(), rcPaint.Height(),
             m_hMemDC, rcPaint.left, rcPaint.top, SRCCOPY);
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
    //
    // 命中控件不处理时<u>继续沿父链上冒</u>（见 DispatchMouseWheel）。原先只问
    // 最深那一个控件、不处理就作罢，于是往 DuiScrollView 里放任何可命中的控件
    // 都会让滚轮在它身上失灵 —— 而"滚动容器里装可点击的东西"恰恰是滚动容器最
    // 常见的用法。
    DuiControl* tgt = HitTopMost(ptClient);
    if (DispatchMouseWheel(tgt, ptClient, zDelta, mkFlags))
    {
        return TRUE;
    }
    SetMsgHandled(FALSE);
    return FALSE;
}

bool DuiHost::DispatchMouseWheel(DuiControl* hit, POINT ptClient,
                                 short zDelta, UINT mkFlags)
{
    // 只向上走、不回头，父链本身是一棵树，因此不存在绕成环的可能。
    //
    // 先取好 parent 再调 OnMouseWheel：个别控件的滚轮处理会引发重新布局乃至
    // 内容替换（DuiScrollView::SetContent 会销毁旧内容子树），调用返回后再读
    // node->GetParent() 有读到已释放对象的风险。
    //
    // ---- 关于"自己会滚的控件滚到边界"的约定 ----
    //
    // 库内所有自滚控件在<u>已经滚到顶 / 滚到底</u>时仍然返回 true（已处理），
    // 因此滚轮不会在到达边界后继续上传、把外层容器一并带着滚：
    //   · DuiScrollBar::OnMouseWheel   —— 只要存在可滚范围就 return true（位置
    //     由 SetPos 内部夹取，滚到边界也照样消费）；DuiScrollView / DuiListBox /
    //     DuiVirtualList 都把滚轮转给它，故一致。
    //   · DuiTreeView::OnMouseWheel    —— 多列模式无条件 return true。
    //   · DuiSlider::OnMouseWheel      —— 无条件 return true。
    //   · DuiRichEdit::OnMouseWheel    —— 只要内容溢出（存在可滚范围）就
    //     return true，滚到顶 / 到底也照样消费。
    // 采用这一约定而非"到边界就交给外层"，是因为后者在实际使用中容易误滚：
    // 用户在内层列表里滚到底后往往会再多滚一两下，此时整页突然跟着动，视觉参照
    // 一下子全变，比"滚不动"更让人困惑。Win32 原生控件也是这个行为。
    //
    // 反过来，确实<u>没有</u>滚动能力的控件必须如实返回 false，事件才能上冒：
    //   · 单列模式的 DuiTreeView（它不自己滚，靠外层 DuiScrollView）；
    //   · 压根没有可滚范围（max == min）的 DuiScrollBar，以及由此转发的
    //     DuiScrollView / DuiListBox / DuiVirtualList —— 内容放得下时滚动条
    //     本就隐藏，此时把滚轮拦下会让外层滚动容器失灵；
    //   · 一切没有覆写 OnMouseWheel 的控件（DuiControl::OnMouseWheel 返回 false）。
    DuiControl* node = hit;
    while (node != nullptr)
    {
        DuiControl* parent = node->GetParent();
        if (node->OnMouseWheel(ptClient, zDelta, mkFlags))
        {
            return true;
        }
        node = parent;
    }
    return false;
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

void DuiHost::OnSetFocus(CWindow /*wndOldFocus*/)
{
    // 这次获焦是 EnsureWin32Focus 主动索取的结果，对应的 DUI 焦点控件刚刚
    // 已经收到过获焦通知，这里不能再发一次。
    if (m_bSyncingWin32Focus)
    {
        return;
    }

    if (m_pFocus)
    {
        m_pFocus->OnSetFocus();
    }
    else if (m_root)
    {
        FocusNext(false);
    }
}

void DuiHost::OnKillFocus(CWindow /*wndNewFocus*/)
{
    if (m_pFocus)
    {
        m_pFocus->OnKillFocus();
    }
}

LRESULT DuiHost::OnForwardToFocus(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // 白名单内的原始消息（按键抬起、系统键、输入法那几条、输入语言切换）
    // 一律转交当前 DUI 焦点控件，由它自行解释。白名单的内容与理由见
    // DuiHost.h 消息映射表里的注释。
    //
    // 这里不需要判断「控件树正在变更」（见 IsTreeChanging）—— 本函数不遍历
    // 控件树，只用 m_pFocus 这一个指针，而 DuiControl 析构时会主动向宿主注销
    // 自己，该指针不会变成悬空指针。
    LRESULT lResult = 0;
    if (m_pFocus != nullptr && m_pFocus->OnRawMessage(uMsg, wParam, lParam, lResult))
    {
        return lResult;
    }

    // 没有焦点控件，或焦点控件不处理这条消息：交回系统默认处理。
    // 这一步不能省 —— 输入法相关消息若被无声吞掉，输入法会工作不正常。
    SetMsgHandled(FALSE);
    return 0;
}

// --- Back buffer ---

// 缓冲容量的取整粒度（像素）。重建缓冲时把需要的尺寸向上取整到该值的整数倍，
// 留出余量，使连续放大窗口时不必每几个像素就重新分配一次位图。256 是经验值：
// 1920 宽的窗口最多浪费 255 列（约 1MB），换来的是拖动放大过程中通常只重建
// 一两次而不是每帧都重建。
static const int kBackBufferGrowStep = 256;
// 缓冲容量上限（像素）。防御性上限，避免异常尺寸请求分配出天文数字的位图。
// 16384 远大于任何现实显示器分辨率。
static const int kBackBufferMaxDim = 16384;

// 把 v 向上取整到 step 的整数倍。
static int RoundUpTo(int v, int step)
{
    if (step <= 1)
    {
        return v;
    }
    return ((v + step - 1) / step) * step;
}

bool DuiHost::EnsureBackBuffer(int cx, int cy)
{
    if (cx <= 0 || cy <= 0)
    {
        return false;
    }

    // 只增不减：现有缓冲装得下就直接复用，客户区变小不重建。
    //
    // 返回 false 表示「缓冲被复用了，里面还是上一帧的正确画面」——
    // 绘制方据此可以只重画脏区，其余部分留用旧内容。
    if (m_hMemDC != nullptr && cx <= m_cxBuf && cy <= m_cyBuf)
    {
        return false;
    }

    // 需要重建。新容量取「现有容量」与「本次所需（向上取整）」的较大者，
    // 避免宽度增长时把已经够用的高度容量缩回去、来回抖动反复重建。
    int newCx = RoundUpTo(cx, kBackBufferGrowStep);
    int newCy = RoundUpTo(cy, kBackBufferGrowStep);
    if (newCx < m_cxBuf)
    {
        newCx = m_cxBuf;
    }
    if (newCy < m_cyBuf)
    {
        newCy = m_cyBuf;
    }
    if (newCx > kBackBufferMaxDim)
    {
        newCx = kBackBufferMaxDim;
    }
    if (newCy > kBackBufferMaxDim)
    {
        newCy = kBackBufferMaxDim;
    }

    DestroyBackBuffer();

    HDC hScreen = ::GetDC(m_hWnd);
    m_hMemDC = ::CreateCompatibleDC(hScreen);
    ::ReleaseDC(m_hWnd, hScreen);
    if (m_hMemDC == nullptr)
    {
        // 建不出来。返回 true 让绘制方走全量路径 —— 反正没有缓冲可用，
        // 保守一点不会画坏。
        return true;
    }

    // 32bpp top-down DIB section。负高度表示自上而下排列，与控件绘制使用的
    // 客户区坐标系一致。GDI+ 能直接读写 DIB 的像素，抗锯齿绘制远快于 DDB。
    BITMAPINFO bmi;
    ::ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = newCx;
    bmi.bmiHeader.biHeight      = -newCy;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_pBits   = nullptr;
    m_hMemBmp = ::CreateDIBSection(m_hMemDC, &bmi, DIB_RGB_COLORS,
                                   &m_pBits, nullptr, 0);
    if (m_hMemBmp == nullptr)
    {
        // 分配失败：退回无缓冲状态，OnPaint 会在下一帧重试。
        ::DeleteDC(m_hMemDC);
        m_hMemDC = nullptr;
        m_pBits  = nullptr;
        return true;
    }

    m_hOldBmp = (HBITMAP)::SelectObject(m_hMemDC, m_hMemBmp);
    m_cxBuf = newCx;
    m_cyBuf = newCy;

    // 新建出来的位图内容是**未初始化的内存**，里面是随机像素。绘制方必须
    // 据此走一次全量绘制把整块填满，否则脏区之外会露出彩色噪点。
    return true;
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
    // 像素内存归 DIB section 所有，随位图一起释放，这里只清指针。
    m_pBits = nullptr;
    m_cxBuf = m_cyBuf = 0;
}

} // namespace balloonwjui
