#pragma once

#include <windows.h>
#include <vector>
#include <memory>
#include "DuiNotify.h"
#include "BalloonUiApi.h"

namespace balloonwjui {

class DuiHost;

// =================================================================
// DuiControl —— DUI 控件基类（无 HWND）
// =================================================================
//
// 用途：所有"逻辑"UI 元素的共同基类。它<u>没有自己的 HWND</u>，纯粹
// 是个逻辑节点，由 host 在 host HWND 的客户区 DC 上画。所有自绘 DUI
// 控件（DuiButton / DuiLabel / DuiSlider / DuiTreeView / 业务自家控件）
// 都直接或间接继承自它。
//
// 工作机制：
//   · 持有矩形、子控件、绘制状态、基础事件 handler。
//   · 子控件用 std::unique_ptr 树形持有；销毁父或 RemoveChild 时一并
//     销毁子。
//   · 默认 mouse / key / focus 事件返 false（不消费）→ host 把对应
//     DuiNotify 沿 WM_DUI_NOTIFY 路径上冒到 host 的 HWND 父。
//   · 线程归属：所有 DuiControl 方法<u>必须</u>在 UI 线程（host HWND
//     线程）调；后台 worker 想回调控件得 PostMessage 回 UI 线程。
//
// 代码用法（自定义子类）：
//
//     class MyControl : public balloonwjui::DuiControl
//     {
//     public:
//         void OnPaint(HDC hdc, const RECT&) override
//         {
//             ::FillRect(hdc, &m_rcItem, (HBRUSH)(COLOR_WINDOW + 1));
//         }
//         bool OnLButtonUp(POINT, UINT) override
//         {
//             NotifyParent(DUIN_CLICK);
//             return true;
//         }
//     };
//
//     auto host = std::make_unique<balloonwjui::DuiHost>();
//     host->Create(hParent);
//     auto child = std::make_unique<MyControl>();
//     child->SetCtrlId(1001);
//     host->GetRoot()->AddChild(std::move(child));
//
// XML 用法：基类自身不是标签，但所有 builder 内置标签的<u>通用属性</u>
// （id / fixedWidth / fixedHeight / weight / margin）都映射到本基类的
// 字段（详见 guides.html §3.2）。业务自定义控件通过 CustomFactory
// 接入 XML（详见 §3.6）。
class BUI_API DuiControl
{
public:
    DuiControl();
    virtual ~DuiControl();

    DuiControl(const DuiControl&) = delete;
    DuiControl& operator=(const DuiControl&) = delete;

    // Tree
    void        AddChild(std::unique_ptr<DuiControl> child);
    void        RemoveChild(DuiControl* child);

protected:
    // Hook called by RemoveChild RIGHT BEFORE the child is destroyed
    // (raw pointer still valid). Subclasses that maintain side tables
    // keyed by child pointer (e.g. DuiLayout's m_hints) override this
    // to scrub their entry. Default is no-op.
    virtual void OnChildRemoved_(DuiControl* /*child*/) {}

public:
    DuiControl* GetParent() const { return m_pParent; }
    DuiHost*    GetHost()   const { return m_pHost; }
    void        AttachToHost(DuiHost* host);   // sets m_pHost recursively

    // Identity
    void        SetCtrlId(UINT id) { m_uCtrlId = id; }
    UINT        GetCtrlId() const  { return m_uCtrlId; }

    // 在以本控件为根的子树里按 ctrlId 深度优先搜（含本控件）。命中第一
    // 个返指针；找不到返 nullptr；多个同 id 不警告（业务侧不应该这么
    // 用）。典型用法：FromFrameXml 拿到 root 后，按 id 拿引用配置控件
    // （tooltip / dropdown / 监听等）。
    DuiControl* FindCtrlById(UINT id);

    // Geometry
    void        SetRect(const RECT& rc);
    const RECT& GetRect() const { return m_rcItem; }

    // 强制以 rc 重新布局（写入 m_rcItem + 调 Layout()），即使 rc 与当前相
    // 同也不短路。SetRect 在 EqualRect 时早 return —— 这对纯 size 改变是
    // 优化，但当 caller 是因为<u>子树结构改变</u>（DuiFrameWindow::
    // SetClientContent 换了客户区控件）需要重排时，那个早 return 会让新
    // 子树永远不被定位。这种场景须显式调本函数。
    void        ForceLayout(const RECT& rc);

    // Visibility / enabled / focusable
    //
    // SetVisible 设置自身 m_bVisible 状态。除了触发 Invalidate，<u>还会
    // 递归同步所有 HwndHostControl 后代</u>的真 HWND 子窗显隐：
    // ShowWindow(SW_HIDE) 当后代 IsEffectivelyVisible == false（即自身或
    // 任意祖先是 invisible），SW_SHOWNOACTIVATE 当 effective 可见。这样
    // 业务隐藏一个父容器时，里面真 EDIT / RICHEDIT 子窗也跟着隐藏，
    // 不会"DUI 不画但 HWND 还透出来"。
    void        SetVisible(bool b);
    bool        IsVisible() const { return m_bVisible; }

    // 走父链算"有效可见性"：自己 + 所有祖先都 m_bVisible == true 才返
    // true。HwndHostControl 用本接口判断真 HWND 子窗该不该显示。
    bool        IsEffectivelyVisible() const;
    void        SetEnabled(bool b);
    bool        IsEnabled() const { return m_bEnabled; }
    void        SetTabStop(bool b) { m_bTabStop = b; }
    bool        IsTabStop() const  { return m_bTabStop && m_bVisible && m_bEnabled; }

    // Per-control state (read-only externally)
    bool        IsFocused() const  { return m_bFocused; }
    bool        IsHover() const    { return m_bHover; }
    bool        IsCaptured() const { return m_bCapture; }

    // Debug / documentation helper: force the visual hover / focus flag so
    // the control paints in that state without any real mouse / focus
    // input. Used by DuiGallery to lay rows of "normal / hover / focused"
    // instances side-by-side for screenshot capture (see guides.html).
    // Calling these on controls in production code is a misuse — the
    // host's mouse / focus tracking will overwrite the override on the
    // next mouse move or focus change. Triggers Invalidate().
    //
    // For the per-control "pressed" state (DuiButton::m_pressed and
    // siblings), each subclass exposes its own DebugSetPressed() because
    // the pressed flag does not live on the base.
    void        DebugSetHover(bool b);
    void        DebugSetFocused(bool b);

    // Hit test: does (pt, in host client coords) fall inside this subtree?
    // Returns the deepest visible+enabled child (or nullptr).
    virtual DuiControl* HitTest(POINT ptHostClient);

    // Paint (clipped to m_rcItem by the host); default paints children.
    virtual void OnPaint(HDC hdc, const RECT& rcDirty);

    // Layout: laid out by parent into rcAvail; default sets m_rcItem = rcAvail.
    virtual void Layout(const RECT& rcAvail);
    virtual SIZE GetDesiredSize() const { return SIZE{0, 0}; }

    // Event handlers (host coords). Return true if consumed; otherwise the
    // host bubbles a DuiNotify with the corresponding code to its parent HWND.
    virtual bool OnMouseMove   (POINT pt, UINT mkFlags);
    virtual bool OnMouseEnter  ();
    virtual bool OnMouseLeave  ();
    virtual bool OnLButtonDown (POINT pt, UINT mkFlags);
    virtual bool OnLButtonUp   (POINT pt, UINT mkFlags);
    virtual bool OnLButtonDblClk(POINT pt, UINT mkFlags);
    virtual bool OnRButtonDown (POINT pt, UINT mkFlags);
    virtual bool OnMouseWheel  (POINT pt, short zDelta, UINT mkFlags);
    virtual bool OnChar        (TCHAR ch);
    virtual bool OnKeyDown     (UINT vk, UINT flags);
    virtual bool OnSetFocus    ();
    virtual bool OnKillFocus   ();
    virtual bool OnSetCursor   (POINT pt);   // return true if SetCursor() was called

    // Convenience: invalidate own rect through the host.
    void Invalidate();

    // Convenience: ask host to capture / release / set focus / set timer to this control.
    void Capture();
    void ReleaseCapture();
    void SetFocus();

    // Bubble a DuiNotify (code [, extra]) to the host's parent HWND.
    // Public so composite controls (which are themselves DuiControls) can fire.
    LRESULT NotifyParent(UINT code, LPARAM extra = 0);

    // Read-only access to the immediate children. Framework code (e.g.
    // DuiScrollView clipping its hosted HWNDs to the viewport) walks the
    // subtree without needing to subclass DuiControl.
    const std::vector<std::unique_ptr<DuiControl>>& Children() const { return m_children; }

protected:
    // Internal: host calls this when adding control to the tree.
    void SetParent_(DuiControl* parent) { m_pParent = parent; }

    // 递归遍历 node 子树（含自身），对每个 HwndHostControl 后代按其
    // IsEffectivelyVisible 同步真 HWND 显隐。SetVisible 状态变化后调一次。
    static void SyncHwndVisibilitySubtree_(DuiControl* node);
    void SetHost_(DuiHost* host)        { m_pHost = host; }

protected:
    RECT          m_rcItem{};       // host-client coords
    DuiControl*   m_pParent = nullptr;
    DuiHost*      m_pHost   = nullptr;
    UINT          m_uCtrlId = 0;
    bool          m_bVisible = true;
    bool          m_bEnabled = true;
    bool          m_bTabStop = false;
    bool          m_bFocused = false;
    bool          m_bHover   = false;
    bool          m_bCapture = false;
    std::vector<std::unique_ptr<DuiControl>> m_children;

    friend class DuiHost;
};

} // namespace balloonwjui
