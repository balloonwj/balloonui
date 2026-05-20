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
        MSG_WM_SETFOCUS(OnSetFocus)
        MSG_WM_KILLFOCUS(OnKillFocus)
        MSG_WM_COMMAND(OnCommand)
        MESSAGE_HANDLER_EX(WM_NOTIFY, OnNotifyMsg)
        MESSAGE_HANDLER(WM_DPICHANGED,    OnDpiChangedMsg)
        MESSAGE_HANDLER(WM_CTLCOLOREDIT,  OnCtlColorMsg)
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC,OnCtlColorMsg)
        MESSAGE_HANDLER(WM_CTLCOLORBTN,   OnCtlColorMsg)
        MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, OnCtlColorMsg)
    END_MSG_MAP()

    // Replace / get the root control. Host takes ownership.
    void        SetRoot(std::unique_ptr<DuiControl> root);
    DuiControl* GetRoot() const { return m_root.get(); }

    // Bubble a DuiNotify to the parent HWND (synchronous).
    // ctrl may be nullptr; in that case ctrlId comes from `notify.ctrlId`.
    LRESULT     SendNotify(DuiControl* ctrl, UINT code, LPARAM extra = 0);

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

    // Suppress OnCommand routing during tree mutations. SetContent /
    // SetRoot / any RemoveChild that may destroy HWND-hosted children
    // wraps the mutation with Begin/EndTreeChange so synchronous EN_*
    // notifications from siblings whose focus shifts are simply dropped
    // (DefWindowProc handles them) instead of running the m_root walker
    // over a tree that's being torn down by ~vector.
    void        BeginTreeChange() { ++m_suppressOnCommand; }
    void        EndTreeChange()
    {
        if (m_suppressOnCommand > 0)
        {
            --m_suppressOnCommand;
        }
    }

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
    void    OnCommand(UINT codeNotify, int nID, HWND hwndCtl);
    LRESULT OnNotifyMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnDpiChangedMsg(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnCtlColorMsg  (UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

private:
    void        EnsureBackBuffer(int cx, int cy);
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
    bool        m_bMouseTracking = false;
    int         m_suppressOnCommand = 0;

    // Back buffer
    HDC     m_hMemDC   = nullptr;
    HBITMAP m_hMemBmp  = nullptr;
    HBITMAP m_hOldBmp  = nullptr;
    int     m_cxBuf    = 0;
    int     m_cyBuf    = 0;

    // Cached per-monitor DPI; 96 until OnCreate / WM_DPICHANGED.
    int     m_dpi      = 96;

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
