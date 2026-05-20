#pragma once

#include "DuiControl.h"

namespace balloonwjui {

// =================================================================
// HwndHostControl —— 真 HWND 控件接入 DUI 树的适配器
// =================================================================
//
// 用途：一些 Win32 控件（EDIT、RICHEDIT、WebBrowser、老 CSkin* 控件）
// 必须有真 HWND 才能工作（IME、光标渲染、ActiveX 内嵌等）。本类是
// "HWND 逃生口"（CLAUDE.md 明确指出的例外路径）：让一个真 HWND 子控
// 件参与 DUI 树的 layout / 显示控制 / focus 协调，但保留它的 OS 原生
// 消息处理。
//
// 工作机制：
//   · 适配器自己<u>不</u>创建 HWND；caller 用 host HWND 作 parent 创建
//     好真子 HWND，然后 Attach(hwnd) 交给适配器。
//   · 适配器把 HWND 的 rect 跟 m_rcItem 同步、按 DUI 控件 visibility
//     显示 / 隐藏。
//   · HWND 继续接收自己的 Win32 消息；适配器<u>不</u>把 DUI 事件翻译
//     成 Win32 给它（鼠标 / 键盘事件由 OS 直送 HWND）。
//   · DuiHost 截获 host HWND 收到的 WM_COMMAND / WM_NOTIFY / WM_CTLCOLOR*
//     里 target 是我们子 HWND 的，转回 OnHwndCommand / OnHwndNotify /
//     GetCtlColorBrush 让子类响应（业务把 EN_CHANGE 转成
//     DUIN_VALUECHANGED 等）。
//
// 代码用法（在 DUI 树里寄宿一个 EDIT）：
//
//     class MyEditHost : public balloonwjui::HwndHostControl
//     {
//     public:
//         void OnHwndCommand(UINT code) override
//         {
//             if (code == EN_CHANGE) { NotifyParent(DUIN_VALUECHANGED); }
//         }
//     };
//     auto* host = new MyEditHost();
//     host->Attach(::CreateWindowEx(0, _T("EDIT"), _T(""),
//                                   WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
//                                   0, 0, 100, 24, hParent,
//                                   (HMENU)IDC_EDIT, GetModuleHandle(NULL), NULL));
//
// XML 用法：N/A（基类，不直接挂 XML 标签）。具体子类如 DuiEditHost
// / DuiRichEditHost / DuiSearchBox / DuiSpinBox 各自有 XML 支持。
class BUI_API HwndHostControl : public DuiControl
{
public:
    HwndHostControl();
    ~HwndHostControl() override;

    // Take ownership of an existing HWND (typically created with the host's
    // HWND as parent). On Detach() or destruction, the HWND is destroyed
    // unless DontDestroyOnDetach() was called.
    void    Attach(HWND hwnd);
    HWND    Detach();
    HWND    GetHostedHwnd() const { return m_hwndChild; }
    void    DontDestroyOnDetach() { m_bOwnsHwnd = false; }

    // DuiControl overrides - keep the child HWND aligned with our rect
    // and visibility.
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;

    // Hit test: HwndHostControl is a hit if the point is inside its rect,
    // but mouse events stop here - they're delivered by the OS to the
    // child HWND, not routed by the DUI host.
    DuiControl* HitTest(POINT ptHostClient) override;

protected:
    // visibility / enabled-state propagation to the HWND
    void    SyncWindowState();

public:
    // Called by DuiHost when it receives a WM_COMMAND whose (HWND)lParam
    // matches our hosted HWND. enCode is HIWORD(wParam). Default: ignore.
    virtual void OnHwndCommand(UINT /*enCode*/) {}

    // Called by DuiHost when it receives a WM_NOTIFY whose pnmh->hwndFrom
    // matches our hosted HWND. RichEdit (EN_LINK / EN_PROTECTED / EN_REQUESTRESIZE
    // / EN_MSGFILTER) and other common-controls children deliver via WM_NOTIFY.
    // Return value is forwarded as the WM_NOTIFY return; default: 0 (ignore).
    virtual LRESULT OnHwndNotify(NMHDR* /*pnmh*/) { return 0; }

    // Called by DuiHost when it receives one of WM_CTLCOLOREDIT,
    // WM_CTLCOLORSTATIC, WM_CTLCOLORBTN, WM_CTLCOLORLISTBOX whose
    // (HWND)lParam matches our hosted HWND. Return a non-null HBRUSH
    // (and configure hdc text/bg colors) to be used as the control's
    // background brush; return null to fall through to DefWindowProc
    // (which uses the system color matching the message). Default
    // implementation reads the stored bg color and sets up GetStockObject
    // accordingly.
    //
    // Use SetBgColor() to change what GetCtlColorBrush() returns; that
    // single setter covers the common case of "EDIT on a dark dialog
    // background should not have a white border-leakage strip".
    virtual HBRUSH GetCtlColorBrush(UINT msgKind, HDC hdc);

    // Per-host override for the background color used by the default
    // GetCtlColorBrush. CLR_INVALID means "no override; let the OS pick".
    void     SetCtlBgColor(COLORREF c);
    COLORREF GetCtlBgColor() const { return m_ctlBg; }
    void     SetCtlTextColor(COLORREF c);
    COLORREF GetCtlTextColor() const { return m_ctlText; }

private:
    HWND     m_hwndChild = nullptr;
    bool     m_bOwnsHwnd = true;
    COLORREF m_ctlBg     = CLR_INVALID;     // override for WM_CTLCOLOR* paint
    COLORREF m_ctlText   = CLR_INVALID;
    HBRUSH   m_ctlBgBrush = nullptr;        // cached for the supplied bg color
};

} // namespace balloonwjui
