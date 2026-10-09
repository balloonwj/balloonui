#pragma once

// NOTE: this header relies on ATL/WTL types (CWindow, CWindowImpl, CDCHandle,
// CSize, CRect, etc.) that come from stdafx.h. Any .cpp including this file
// MUST include stdafx.h first (matches the project-wide PCH convention).
#include "DuiControl.h"
#include "BalloonUiApi.h"
#include <memory>

namespace balloonwjui {

// =================================================================
// DuiHost —— DUI 控件树的真窗口宿主
// =================================================================
//
// 用途：整棵 DUI 控件树的<u>唯一</u> HWND。所有 OS 消息进来到这里，
// 转发给具体 DuiControl。整个 balloonui 的"无 HWND DUI"架构就是建立
// 在它之上的：只有它一个真 HWND，所有子控件都是逻辑节点。
//
// 工作机制：
//   · 所有真 Windows 消息（WM_PAINT、WM_*MOUSE*、WM_KEY*、
//     WM_SETCURSOR、WM_SETFOCUS / WM_KILLFOCUS、WM_TIMER）都派发到这里，
//     host 通过 hit-test / capture / focus 状态路由给具体 DuiControl。
//   · 绘制双缓冲（离屏 DC）—— 控件画在普通 HDC 上不用考虑闪烁。
//   · 两种使用模式：
//     1) 从零创建：Create(...) 像普通 CWindowImpl 那样建顶层 HWND。
//     2) Subclass：SubclassWindow(hwnd) 把已存在 HWND（典型是
//        CDialogImpl 创建的对话框）变成 DUI 容器（兼容 CSkinDialog 模式）。
//   · 跟踪 per-monitor DPI；控件 paint / layout 时调 GetDpi() 拿当前
//     DPI 做 DPI-aware 几何。
//   · 支持 9-grid bg image（SetBgImage）和客户区 1px 边框
//     （SetClientBorderColor），详见对应 setter 注释。
//
// 代码用法（subclass 已存在的对话框）：
//
//     class MyDlg : public CDialogImpl<MyDlg> {
//         balloonwjui::DuiHost m_host;
//         LRESULT OnInitDialog(...) {
//             m_host.SubclassWindow(m_hWnd);
//             m_host.SetRoot(BuildUi());
//             return 0;
//         }
//     };
//
// XML 用法：客户区控件树用 DuiXmlBuilder::FromString 构造、
// SetRoot 装入；DuiHost 自身的属性（bg image / 客户区边框）目前是
// C++ setter 调。完整 frame XML（顶层 + 客户区）走 DuiFrameWindow
// 的 FromFrameXml + ApplyConfig（详见 guides.html §11）。
class BUI_API DuiHost : public CWindowImpl<DuiHost, CWindow>
{
public:
    DECLARE_WND_CLASS(_T("__DuiHost__"))

    DuiHost();
    virtual ~DuiHost();

    DuiHost(const DuiHost&) = delete;
    DuiHost& operator=(const DuiHost&) = delete;

    // 宿主自用的私有消息：请求重新排版整棵 DUI 树，见 RequestRelayout()。
    //
    // 取值落在 WM_APP 段（0x8000-0xBFFF），该段按约定归应用程序自己支配，
    // 系统与通用控件都不会往这里发消息，所以不必用 RegisterWindowMessage
    // 去换一个全局唯一编号。加的偏移只是为了跟宿主将来可能新增的私有消息
    // 错开，数值本身没有特别含义。
    enum { kMsgRelayout = WM_APP + 0x120 };

    BEGIN_MSG_MAP(DuiHost)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_SETCURSOR(OnSetCursor)
        MSG_WM_MOUSEMOVE(OnMouseMove)
        MSG_WM_MOUSELEAVE(OnMouseLeave)
        MSG_WM_LBUTTONDOWN(OnLButtonDown)
        MSG_WM_LBUTTONUP(OnLButtonUp)
        MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
        MSG_WM_RBUTTONDOWN(OnRButtonDown)
        MSG_WM_MOUSEWHEEL(OnMouseWheel)
        MSG_WM_CHAR(OnChar)
        MSG_WM_KEYDOWN(OnKeyDown)
        // ---- 转发给焦点控件的原始消息白名单 ----
        //
        // 下面这批消息宿主自己不解释，原样交给当前 DUI 焦点控件的
        // DuiControl::OnRawMessage；控件不处理时交回系统默认处理。
        //
        // 它们服务的是「自己没有 HWND、却需要完整窗口消息序列」的控件，
        // 目前是无窗口富文本控件 DuiRichEdit。为什么必须补这些：
        //   · 按键抬起 / 系统键：上面只登记了按键按下，引擎拿不到成对的
        //     抬起消息就无法维护 Shift、Ctrl 这类修饰键的状态机，按住
        //     Shift 配合方向键选文字会失灵；
        //   · 输入法那几条：中文输入的组字过程全靠它们传递，缺了就打不了中文；
        //   · 输入语言切换：用户切换中英文输入法时引擎需要知道。
        //
        // 采用白名单而非「所有未处理消息一律下发」，是为了把截获面限制在
        // 明确列出的这几条上，不影响现有控件的行为。新增消息时在此加一行即可。
        //
        // 注意这里<u>没有</u> WM_TIMER：排版引擎要的定时器由控件自己用线程
        // 定时器实现（与 DuiToolTipMgr、DuiAnimMgr 同一手法），不经过宿主。
        // 那样做既避开了「引擎自选的定时器编号在多个控件间撞号」的问题，
        // 也省掉了宿主这一层转发。
        MESSAGE_HANDLER_EX(WM_KEYUP,                 OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_SYSKEYDOWN,            OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_SYSKEYUP,              OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_SYSCHAR,               OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_INPUTLANGCHANGE,       OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_IME_SETCONTEXT,        OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_IME_STARTCOMPOSITION,  OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_IME_COMPOSITION,       OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_IME_ENDCOMPOSITION,    OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_IME_NOTIFY,            OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_IME_CHAR,              OnForwardToFocus)
        MESSAGE_HANDLER_EX(WM_IME_REQUEST,           OnForwardToFocus)
        MSG_WM_SETFOCUS(OnSetFocus)
        MSG_WM_KILLFOCUS(OnKillFocus)
        MESSAGE_HANDLER(WM_DPICHANGED,    OnDpiChangedMsg)
        MESSAGE_HANDLER_EX(kMsgRelayout, OnRelayoutMsg)
    END_MSG_MAP()

    // Replace / get the root control. Host takes ownership.
    void        SetRoot(std::unique_ptr<DuiControl> root);
    DuiControl* GetRoot() const { return m_root.get(); }

    // Bubble a DuiNotify to the parent HWND (synchronous).
    // ctrl may be nullptr; in that case ctrlId comes from `notify.ctrlId`.
    LRESULT     SendNotify(DuiControl* ctrl, UINT code, LPARAM extra = 0);

    // 请求重新排版整棵 DUI 树 —— 异步执行，本函数立即返回。
    //
    // 给「期望尺寸会在运行期变化」的控件用。典型场景是无窗口富文本控件
    // DuiRichEdit 打开自动增高之后：用户每敲一个字，控件报告的期望高度就可能
    // 变大一点，需要重新走一遍父容器的排版才能真的长高。
    //
    // **为什么必须异步（投递消息而不是当场排版）**：调用方通常是在某个控件的
    // 回调里发现自己变高了，而那个回调往往又是在更底层的处理过程中被调用的。
    // 富文本控件的 EN_CHANGE 就是这样 —— 它由排版引擎在处理按键消息的中途回调
    // 上来。此时如果当场重排，就会在引擎还没处理完这次按键时，反过来调它的
    // 「客户区矩形变了」接口让它重新排版文本，属于重入调用，引擎对此没有保证。
    // 投递一条消息把重排推迟到本次消息处理结束之后，就不存在这个问题。
    //
    // **重复调用会自动合并**：连续快速输入时每个字都会调一次，但只要上一次
    // 请求还没被处理，后续请求就直接丢弃，不会积压出一堆重排。
    //
    // 窗口尚未创建时直接返回，调用方不必判断。
    void        RequestRelayout();

    // Capture / focus management (DUI-internal, NOT Win32 SetCapture).
    void        SetDuiCapture(DuiControl* ctrl);
    DuiControl* GetDuiCapture() const { return m_pCapture; }
    void        ReleaseDuiCapture(DuiControl* ctrl);

    void        SetDuiFocus(DuiControl* ctrl);
    DuiControl* GetDuiFocus() const { return m_pFocus; }
    DuiControl* GetDuiHover() const { return m_pHover; }

    // Clear m_pHover if it points to ctrl (called from ~DuiControl so the
    // host doesn't keep a dangling pointer after a tree-mutation tear-down
    // like DuiScrollView::SetContent).
    void        ClearHoverIfMatches(DuiControl* ctrl);

    // 标记「控件树正在被整体替换或拆除」的区间。SetRoot、
    // DuiScrollView::SetContent、DuiFrameWindow::SetClientContent 这类会就地
    // 析构整棵子树的操作，用 Begin / End 成对把整个过程包起来。宿主在该区间内
    // 不执行任何需要遍历控件树的消息路由 —— 子控件正在逐个析构，父容器的
    // m_children 里可能仍留着已经释放的指针，遍历到它就会读到无效内存。
    //
    // 计数器允许嵌套：外层容器换内容的过程中，内层容器可能也在换自己的内容。
    void        BeginTreeChange() { ++m_treeChangeDepth; }
    void        EndTreeChange()
    {
        if (m_treeChangeDepth > 0)
        {
            --m_treeChangeDepth;
        }
    }

    // 查询当前是否处于控件树变更区间内。返回 true 时，任何需要遍历控件树的
    // 处理都必须直接返回，不得走到 m_root 上去。
    bool        IsTreeChanging() const { return m_treeChangeDepth > 0; }

    // Tab navigation: walk the tree in declaration order honoring IsTabStop().
    void        FocusNext(bool backward);

    // Invalidate a sub-rect (host-client coords). Use through DuiControl::Invalidate.
    void        InvalidateDuiRect(const RECT& rc);

    // Get the offscreen back-buffer DC if a control needs to compose against
    // the host background (rarely needed). Valid only inside OnPaint chain.
    HDC         GetBackBufferDC() const { return m_hMemDC; }

    // Per-monitor DPI of this host's HWND. 96 means "no scaling" (or no
    // HWND yet). Refreshed lazily on creation and whenever WM_DPICHANGED
    // fires. Controls that need DPI-aware geometry can read this in
    // their paint / layout paths.
    int         GetDpi() const { return m_dpi; }

    // GetDpi 的值是否已经取自本窗口（窗口创建或收到 WM_DPICHANGED 之后为 true）。
    // 为 false 时 GetDpi 返回的只是构造时的默认值 96，DuiControl::GetDpi 据此改用
    // DuiResMgr 的全局 DPI。
    bool        HasWindowDpi() const { return m_hasWindowDpi; }

    // 9-grid background image. When set, OnPaint replaces the default
    // COLOR_BTNFACE clear with a DuiNinePatch::Draw of `hbm` filling the
    // entire client area. The 4 inset values define the corners that
    // stay 1:1 (no stretch) — caller picks them based on the source
    // image's decorations (rounded corners, gradient header, drop
    // shadow margin, etc.). See guides.html "9-grid 背景图" for a full
    // walkthrough of the math.
    //
    // hbm:
    //   - Caller-owned. Host does NOT copy or DeleteObject. The bitmap
    //     must outlive the host (or callers can SetBgImage(nullptr, ...)
    //     before destroying the bitmap).
    //   - 32bpp DIB with premultiplied alpha is honored (DuiNinePatch
    //     handles the alpha-blend path internally). 24bpp also works
    //     (no transparency).
    //   - Pass nullptr to clear and revert to the COLOR_BTNFACE clear.
    //
    // insets:
    //   - Source-pixel offsets. left/top/right/bottom = thickness of
    //     the four "non-stretch" border bands.
    //   - Clamped to [0, srcDim] so left+right ≤ srcW and top+bottom ≤ srcH;
    //     overflow shrinks proportionally (no overlap).
    //
    // Triggers Invalidate() on the host so the new bg is visible
    // immediately.
    void        SetBgImage(HBITMAP hbm, const RECT& insets);

    // 双 inset 重载：源内距和目标内距独立指定。
    //
    // 当源图的"装饰带"（顶部渐变 / 阴影 / 边框纹理）在源里高 ST 像素，但你
    // 希望渲染到目标里高 DT 像素时（典型是 ST > DT，把装饰条压扁成更紧凑的
    // 标题栏），用这个重载：
    //   srcInsets.top = ST  (源图里装饰带的真实高度)
    //   dstInsets.top = DT  (目标里你希望它呈现成多高)
    //
    // 单 inset 版本等价于 src == dst，4 角 1:1 复制。
    // 双 inset 版本会让 4 角也参与缩放（src.thickness → dst.thickness）—
    // 对纯色 / 渐变色的角无视觉损失；若源图四角带硬边装饰则会被等比缩放。
    void        SetBgImage(HBITMAP hbm,
                           const RECT& srcInsets,
                           const RECT& dstInsets);

    // 文件路径加载便捷重载 —— 内部用 GDI+ 加载 PNG / BMP / JPG（白底
    // 预合成，规避 PNG alpha=0 的 BitBlt 黑屏坑），加载出的 HBITMAP 由
    // host 持有，析构时自动 DeleteObject。返回 false 表示加载失败
    // （文件不存在 / 解码失败），此时不动当前 bg。
    //
    // path 可以是绝对路径，也可以是相对路径（caller 自己决定 base —
    // 库不做隐式解析，防止 cwd 漂移）。XML 走 ResolveAssetPath 后再传进
    // 来。GDI+ 会按需 startup（首次调用时初始化，进程结束由 OS 收）。
    bool        LoadBgImageFromFile(LPCTSTR path, const RECT& insets);
    bool        LoadBgImageFromFile(LPCTSTR path,
                                    const RECT& srcInsets,
                                    const RECT& dstInsets);

    HBITMAP     GetBgImage() const { return m_hBgImage; }

    // 客户区四周 1px 边框 —— 给"无 bg 图、无系统 chrome"的 frame 一个
    // 视觉的窗口范围标记（DuiFrameWindow 抹掉了系统非客户区，没有任何
    // 边线，否则窗口边在浅色客户区里完全看不见）。
    //
    // 规则：
    //   · color == CLR_INVALID（默认）→ 不画边框
    //   · m_hBgImage != nullptr      → 不画边框（图本身已经有装饰边）
    //   · 否则在 OnPaint 末尾画一圈 1px FrameRect
    //
    // 颜色一般用浅灰（如 RGB(200,200,200)），可与 client area 浅色背景
    // 形成 ~10% 对比但不抢戏。
    void        SetClientBorderColor(COLORREF c);
    COLORREF    GetClientBorderColor() const { return m_clientBorderColor; }

    // ---- 双击合成（默认关闭）----
    //
    // 何时需要：窗口类不带 CS_DBLCLKS 时，系统不会投递 WM_LBUTTONDBLCLK，
    // DuiControl 的 OnLButtonDblClk 永不触发，此时可在<u>运行期、按需</u>
    // 打开"双击合成"。
    //
    // 注意（2026-10-06 更正）：此前这里写的是"balloonui 的
    // 窗口类（__DuiHost__ / __DuiFrameWindow__）未注册 CS_DBLCLKS"，与实际
    // 不符 —— __DuiHost__ 由 DECLARE_WND_CLASS 声明，默认样式含 CS_DBLCLKS；
    // __DuiFrameWindow__ 现在也明确带 CS_DBLCLKS。这两类窗口里系统本就会投递
    // WM_LBUTTONDBLCLK（由 OnLButtonDblClk 转给控件），第二次按下不会经过
    // OnLButtonDown，所以对它们开启本接口实际不起作用。本接口只对窗口类不带
    // CS_DBLCLKS 的宿主（如经 SubclassWindow 挂到别的窗口类上）才有意义。
    //
    // 开启后，DuiHost 在 OnLButtonDown 里按系统双击时限（GetDoubleClick
    // Time）+ 位移容差（SM_CXDOUBLECLK / SM_CYDOUBLECLK）自行判定双击；
    // 命中时改派一次 OnLButtonDblClk，语义与系统 CS_DBLCLKS 一致 ——
    // 第二次按下以双击形式派发，<u>不再</u>额外派发一次 OnLButtonDown。
    //
    // 本方案不修改窗口类样式，故只影响调用本方法的<u>这一个</u> host
    // 实例，不波及同窗口类的其他窗口。
    //
    //   enable：true 开启双击合成；false 关闭。默认关闭。
    void        EnableDoubleClick(bool enable) { m_dblClkEnabled = enable; }

    // 查询双击合成当前是否开启。
    //   返回：已开启返回 true；否则 false。
    bool        IsDoubleClickEnabled() const { return m_dblClkEnabled; }

    // ---- 滚轮派发（冒泡）----
    //
    // ---- 拖放分发 ----
    //
    // 操作系统的拖放目标是**按窗口注册**的，而且一个窗口只能注册一个。
    // 纯 DUI 控件没有自己的窗口、共用宿主窗口，因此不能各注册各的。
    // 解法是宿主注册唯一一个分发器：收到拖放事件后按光标位置找到命中的
    // 控件，再转发给该控件的 DuiControl::GetDropTarget()。
    //
    // 由需要接收拖放的控件自行请求打开，重复调用无害。
    //   b：true 打开、false 关闭并注销。
    //   返回：true 表示分发器已就绪；false 表示注册失败。最常见的失败原因是
    //         **这个窗口已经被别的代码注册过拖放目标**（例如业务自己挂了一个
    //         接收拖入文件的），一个窗口只能有一个。此时控件的「拖入」不可用，
    //         但不影响拖出、也不影响其它任何行为。
    bool        EnableDropDispatch(bool b);
    bool        IsDropDispatchEnabled() const { return m_pDropDispatch != nullptr; }

    // 按客户区坐标找出能接收拖放的控件：从命中的最深控件起沿父链往上找，
    // 返回第一个 GetDropTarget() 非空的。供拖放分发器使用。
    //   ptClient：宿主客户区坐标。
    //   返回：找到的控件；没有则返回 nullptr。
    DuiControl* FindDropTargetControl(POINT ptClient);

    // 把一次滚轮事件从命中控件开始、沿 GetParent() 父链依次投递，直到某一层
    // 的 OnMouseWheel 返回 true 为止。滚轮在 Win32 与绝大多数 UI 框架里都是
    // 冒泡的：鼠标底下那个控件不管这件事，就轮到它的容器管。
    //
    // 之所以做成 public static：本函数是纯粹的树遍历逻辑，不依赖 HWND，
    // 单元测试可以直接搭一棵控件树调它，不必真的建窗口（见 DuiHostTests）。
    //
    //   hit：命中的最深控件，可为 nullptr（鼠标不在任何控件上），此时直接
    //        返回 false，不做任何事。所有权不转移。
    //   ptClient：鼠标位置，host 客户区坐标，原样传给每一层。
    //   zDelta：滚轮增量（WHEEL_DELTA 的整数倍，正数为向上滚），原样传递。
    //   mkFlags：键鼠修饰位（MK_SHIFT / MK_CONTROL 等），原样传递。
    //   返回：true 表示链上某一层已处理；false 表示一路到根都没人处理，
    //         调用方应把消息交回 DefWindowProc。
    static bool DispatchMouseWheel(DuiControl* hit, POINT ptClient,
                                   short zDelta, UINT mkFlags);

protected:
    int     OnCreate(LPCREATESTRUCT lpcs);
    void    OnDestroy();
    void    OnSize(UINT nType, CSize size);
    BOOL    OnEraseBkgnd(CDCHandle dc);
    void    OnPaint(CDCHandle dc);
    BOOL    OnSetCursor(CWindow wnd, UINT nHitTest, UINT message);
    void    OnMouseMove(UINT mkFlags, CPoint pt);
    void    OnMouseLeave();
    void    OnLButtonDown(UINT mkFlags, CPoint pt);
    void    OnLButtonUp(UINT mkFlags, CPoint pt);
    void    OnLButtonDblClk(UINT mkFlags, CPoint pt);
    void    OnRButtonDown(UINT mkFlags, CPoint pt);
    BOOL    OnMouseWheel(UINT mkFlags, short zDelta, CPoint pt);
    void    OnChar(TCHAR ch, UINT nRepCnt, UINT nFlags);
    void    OnKeyDown(TCHAR vk, UINT nRepCnt, UINT nFlags);
    void    OnSetFocus(CWindow wndOld);
    void    OnKillFocus(CWindow wndNew);

    // 白名单内的原始消息统一走这里：转交当前焦点控件的 OnRawMessage，
    // 控件不处理时 SetMsgHandled(FALSE) 交回系统默认处理。
    // 白名单的内容与理由见上方消息映射表里的注释。
    LRESULT OnForwardToFocus(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnDpiChangedMsg(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

    // kMsgRelayout 的处理：真正执行一次全树重排。见 RequestRelayout()。
    LRESULT OnRelayoutMsg  (UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    // 让宿主窗口本身持有 Win32 键盘焦点。
    //
    // 仅在新获得 DUI 焦点的控件声明了 NeedsWin32Focus() 时才调用，因此对
    // 现有控件没有任何影响。详见 DuiControl::NeedsWin32Focus 的注释。
    //
    // 内部会置起 m_bSyncingWin32Focus，因为 ::SetFocus 会同步触发本窗口的
    // WM_SETFOCUS，若不加标志会导致刚获得焦点的控件被通知两次。
    void        EnsureWin32Focus();

    // 准备后台缓冲，容量不足时重建（只增不减）。
    //   cx / cy：本次需要的最小尺寸（像素）。
    //   返回：true 表示**缓冲是新建的、内容未初始化**，调用方必须整块重画；
    //         false 表示复用了原有缓冲，里面仍是上一帧的正确画面，
    //         调用方可以只重画脏区。
    bool        EnsureBackBuffer(int cx, int cy);
    void        DestroyBackBuffer();
    DuiControl* HitTopMost(POINT pt);
    void        TrackHover(POINT pt);
    void        StartMouseTracking();

    // Tab-walk helper
    void        CollectTabStops(DuiControl* node, std::vector<DuiControl*>& out) const;

private:
    std::unique_ptr<DuiControl> m_root;

    // Hover / capture / focus state
    DuiControl* m_pHover   = nullptr;
    DuiControl* m_pCapture = nullptr;
    DuiControl* m_pFocus   = nullptr;

    // 正在由 EnsureWin32Focus 主动索取 Win32 焦点。用于压住由此触发的
    // WM_SETFOCUS，避免把焦点通知重复发给同一个控件。
    bool        m_bSyncingWin32Focus = false;

    // 拖放分发器。非空表示已注册到本窗口上。类型是不透明指针，实现细节
    // 藏在 .cpp 里，免得把 OLE 的头文件拖进本头文件。
    class       DropDispatch;
    DropDispatch* m_pDropDispatch = nullptr;
    bool        m_bMouseTracking = false;
    // 控件树变更区间的嵌套层数，0 表示当前不在变更过程中。由
    // BeginTreeChange / EndTreeChange 成对增减，生命周期与宿主相同。
    int         m_treeChangeDepth = 0;
    // 已投递但尚未处理的重排请求标志，用于把连续多次 RequestRelayout()
    // 合并成一次。在 OnRelayoutMsg 真正开始排版时清掉。
    bool        m_bRelayoutPending = false;

    // 双击合成状态（见 EnableDoubleClick）。默认关闭。
    bool        m_dblClkEnabled = false;     // 是否开启双击合成
    DWORD       m_lastLDownTick = 0;         // 上一次左键按下的消息时刻（GetMessageTime）
    POINT       m_lastLDownPt   = { 0, 0 };  // 上一次左键按下的客户区坐标

    // ---- 后台缓冲（双缓冲绘制的离屏画布）----
    //
    // 位图用 32bpp top-down 的 DIB section，<u>不用</u> CreateCompatibleBitmap
    // 的屏幕兼容位图（DDB）—— GDI+ 往 DDB 上做抗锯齿绘制要慢三到四倍，而
    // balloonui 的控件几乎全是 GDI+ 抗锯齿绘制。本机 1458x947 实测：整屏
    // 线性渐变 DDB 13.1ms / DIB 3.6ms，200 个抗锯齿圆角矩形 DDB 30.2ms /
    // DIB 10.1ms，缓冲创建+销毁 DDB 3.32ms / DIB 0.06ms。
    //
    // m_cxBuf / m_cyBuf 是缓冲的<u>容量</u>而非当前客户区尺寸：缓冲只增不减，
    // 客户区变小时继续沿用大缓冲（只用其左上角一块），仅当客户区超出容量时
    // 才重建。这样拖动窗口改变尺寸时绝大多数帧不需要重新分配位图。
    HDC     m_hMemDC   = nullptr;  // 离屏 DC，选入 m_hMemBmp
    HBITMAP m_hMemBmp  = nullptr;  // 离屏位图（DIB section，host 持有并释放）
    HBITMAP m_hOldBmp  = nullptr;  // m_hMemDC 原有位图，销毁前需选回
    void*   m_pBits    = nullptr;  // DIB 像素首地址（由 GDI 拥有，随位图释放）
    int     m_cxBuf    = 0;        // 缓冲容量宽度（像素），0 = 尚未分配
    int     m_cyBuf    = 0;        // 缓冲容量高度（像素），0 = 尚未分配

    // Cached per-monitor DPI; 96 until OnCreate / WM_DPICHANGED.
    int     m_dpi      = 96;
    bool    m_hasWindowDpi = false;   // m_dpi 是否已取自本窗口（OnCreate / WM_DPICHANGED 后置 true）

    // 9-grid background image (caller-owned). nullptr → no bg image,
    // OnPaint clears with COLOR_BTNFACE.
    //
    // 单 inset 模式：m_bgSrcInset == m_bgDstInset。
    // 双 inset 模式：两者独立，OnPaint 走双 inset 版本的 DuiNinePatch::Draw。
    HBITMAP m_hBgImage              = nullptr;
    // 通过 LoadBgImageFromFile 加载的 bitmap 由 host 持有，析构时 DeleteObject。
    // 通过 SetBgImage(hbm, ...) 直接传入的 caller-owned 句柄不写到这里。
    HBITMAP m_hOwnedBgImage         = nullptr;
    int     m_bgSrcInsetLeft        = 0;
    int     m_bgSrcInsetTop         = 0;
    int     m_bgSrcInsetRight       = 0;
    int     m_bgSrcInsetBottom      = 0;
    int     m_bgDstInsetLeft        = 0;
    int     m_bgDstInsetTop         = 0;
    int     m_bgDstInsetRight       = 0;
    int     m_bgDstInsetBottom      = 0;

    // 客户区 1px 边框颜色（CLR_INVALID = 不画）。详见 SetClientBorderColor。
    COLORREF m_clientBorderColor    = CLR_INVALID;
};

} // namespace balloonwjui
