#pragma once

#include <windows.h>
#include <vector>
#include <memory>
#include "DuiNotify.h"
#include "BalloonUiApi.h"

struct IDropTarget;

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

    // ---- DPI 与字体 ----
    //
    // 不同缩放比例的显示器上，窗口各有各的 DPI。控件绘制、测量文字时应按自己所在
    // 窗口的 DPI 取字体，而不是按 DuiResMgr 的全局 DPI —— 后者只是最近一次建窗或
    // DPI 变化时记下的值，多显示器缩放比例不同时会与本窗口不符。

    // 本控件所用的 DPI：已挂到宿主且宿主窗口已创建时，取宿主窗口的 DPI（随窗口所在
    // 显示器变化）；否则取 DuiResMgr 的当前全局 DPI。
    //   返回：DPI，恒 > 0。
    int         GetDpi() const;

    // 按本控件的 DPI 从 DuiResMgr 取共享字体，参数含义同 DuiResMgr 的同名接口。
    // 所有权在 DuiResMgr，<u>不要</u> DeleteObject。DPI 变化后再取得到的是按新 DPI
    // 缩放的另一份，所以不要长期保存句柄，绘制与测量时现取即可（DuiResMgr 有缓存，
    // 现取只是一次查表）。
    HFONT       GetDefaultFont() const;
    HFONT       GetFontByPointSize(int pt, bool bold = false) const;
    HFONT       GetAntiAliasedFontByPointSize(int pt, bool bold = false) const;

    // 宿主窗口的 DPI 变化（收到 WM_DPICHANGED）时，宿主对整棵控件树逐个调用本函数，
    // 父控件先于子控件；随后整棵树重新布局并重画。按 DPI 预先算好并缓存了字体或
    // 尺寸参数的控件覆写它刷新缓存（例如 DuiRichEdit 按新 DPI 重设默认字体）。
    // 调用时 GetDpi() 已返回新值。默认实现什么也不做。
    //   dpi：新的 DPI。
    virtual void OnDpiChanged(int /*dpi*/) {}

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
    // SetVisible 设置自身 m_bVisible 状态并触发 Invalidate。
    //
    // 注意它<u>不</u>触发重新布局：容器排版时会跳过不可见的子控件，因此控件在
    // 隐藏期间若发生过一次重排，它停留的矩形已经过时，再 SetVisible(true) 时
    // 那份矩形不会被重新算过。显隐变化之后需要由调用方对相应的容器调一次
    // ForceLayout。
    void        SetVisible(bool b);
    bool        IsVisible() const { return m_bVisible; }

    // 走父链算"有效可见性"：自己 + 所有祖先都 m_bVisible == true 才返 true。
    // 与 IsVisible 的区别在于后者只看本控件自己的标志，父容器被隐藏时它仍返回
    // true。需要判断「这个控件此刻是否真的会被画出来」时用本接口。
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

    // 绘制自身；默认实现递归绘制可见的子控件。
    //
    // 注意：**宿主不会替你裁剪到 m_rcItem**。宿主与本基类的绘制路径里都没有
    // 设置任何裁剪区，rcDirty 只是一个「这块区域需要重画」的建议值，画笔完全
    // 可以越出 m_rcItem。需要裁剪的控件必须自己在绘制前后 SaveDC / RestoreDC
    // 并调用 IntersectClipRect（参考 DuiListBox 的做法）。**不能用
    // SelectClipRgn** —— 那是替换语义，会把外层容器已经设好的裁剪一起撤掉。
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

    // 本控件获得 DUI 焦点时，是否需要**宿主窗口本身**持有 Win32 键盘焦点。
    //
    // 背景：纯 DUI 控件没有自己的窗口，键盘消息只能先投递到宿主窗口、再由
    // 宿主分发下来。而 Windows 不会因为用户点击了某个子窗口就自动把键盘焦点
    // 交给它 —— 必须由程序自己调用系统接口去要。宿主窗口若一直没有键盘焦点，
    // 字符与按键消息就会全部投递给它的父窗口，纯 DUI 控件一个也收不到。
    //
    // 为什么不干脆让宿主无条件抢焦点：那会改变现有控件的行为。点击按钮之类
    // 不需要键盘的控件时抢焦点，会让正在编辑的输入框收到失焦通知，业务可能
    // 据此认为「编辑结束了」。所以改成由控件自己声明需不需要。
    //
    // 默认 false。目前只有无窗口富文本控件覆写为 true。
    virtual bool NeedsWin32Focus() const { return false; }

    // 本控件用来接收拖放的目标对象。
    //
    // 背景：操作系统的拖放目标是**按窗口注册**的，一个窗口只能注册一个。
    // 而纯 DUI 控件没有自己的窗口、共用宿主窗口，所以不能各注册各的。
    // 解法是由宿主注册唯一一个分发器，收到拖放事件后按光标位置找到命中的
    // 控件，再转发给这里返回的对象。
    //
    //   返回：本控件的拖放目标；不接收拖放的控件返回 nullptr（默认）。
    //         **所有权归控件**，调用方只借用、不负责释放。
    virtual ::IDropTarget* GetDropTarget() { return nullptr; }
    virtual bool OnSetCursor   (POINT pt);   // return true if SetCursor() was called

    // ---- 宿主转发的原始窗口消息 ----
    //
    // 宿主收到白名单内的窗口消息时，原样转交给当前的 DUI 焦点控件。这条通道
    // 是给「自己没有 HWND、却需要完整窗口消息序列」的控件准备的。目前的使用者
    // 是无窗口富文本控件 DuiRichEdit —— 它内部的排版引擎要求宿主把按键抬起、
    // 系统键、输入法组字等消息原封不动地喂给它，缺一条就会出问题（例如缺了
    // 按键抬起，引擎就不知道 Shift 键是否还按着，按住 Shift 用方向键选文字
    // 会失灵）。
    //
    // 为什么做成一个通用接口，而不是每类消息各开一个虚函数：这批消息（按键
    // 抬起、系统键、输入法五条、输入语言切换等十余条）几乎只服务于文本编辑类
    // 控件。若按库内既有风格逐个开虚函数，全库每个控件都要背上十几个用不到的
    // 空实现。反过来也不采用「所有没人处理的消息一律下发」的兜底做法 —— 那样
    // 截获面过大，容易影响现有控件的行为。折中办法是：宿主用白名单明确列出
    // 转发哪些消息（见 DuiHost 的消息映射表），控件这边只开这一个口子。
    //
    // 默认实现返回 false（不处理），因此现有控件不受任何影响。
    //
    //   uMsg / wParam / lParam：原始窗口消息及其两个参数，未作任何加工。
    //   lResult：出参。返回 true 时由本控件填写该消息的返回值；返回 false
    //            时本参数不会被读取。
    //   返回：true 表示本控件已处理，宿主把 lResult 作为消息返回值；
    //         false 表示未处理，宿主交回系统默认处理。
    virtual bool OnRawMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

    // Convenience: invalidate own rect through the host.
    //
    // 失效矩形会与祖先链上每一个「把子控件裁剪在自己矩形内绘制」的容器
    // （GetChildClipRect 返回 true 的祖先，目前是 DuiScrollView）依次求交，再交给
    // 宿主。这类容器矩形之外的失效画不出任何东西，却会让宿主把相邻控件一并重画：
    // 滚动视口里的内容控件高度往往远超视口，滚动后它的矩形伸到视口上下方，不求交
    // 的话每次滚动都会让整个窗口重画。交集为空（本控件此刻完全不可见）时不产生失效。
    void Invalidate();

    // 本控件绘制子控件时是否把它们裁剪在某个矩形之内；是则经 outClip 返回该矩形
    // （宿主客户区坐标）。Invalidate 据此把子孙控件的失效区域限制在可见范围内。
    //   outClip：出参，返回 true 时填写裁剪矩形；返回 false 时不会被读取。
    //   返回：默认 false —— 普通容器不裁剪子控件，子控件可能本来就画在父容器之外，
    //         其失效必须原样上报。覆写为 true 的控件必须保证 OnPaint 确实把子控件
    //         裁在返回的矩形之内，否则矩形外的那部分会得不到重画。
    virtual bool GetChildClipRect(RECT& /*outClip*/) const { return false; }

    // Convenience: ask host to capture / release / set focus / set timer to this control.
    void Capture();
    void ReleaseCapture();
    void SetFocus();

    // Bubble a DuiNotify (code [, extra]) to the host's parent HWND.
    // Public so composite controls (which are themselves DuiControls) can fire.
    //
    // 声明为虚函数，是为了让被其它控件嵌在内部的子控件有办法停止发送自己的
    // 通知。通知直接发送到宿主窗口、不沿控件树逐层传递（见实现），因此外层
    // 控件无法拦截内层控件发出的通知；复合控件（例如下拉框内部嵌的输入框）
    // 若要对外只发送自身的一份通知，只能由内层控件覆写本方法并丢弃通知。
    virtual LRESULT NotifyParent(UINT code, LPARAM extra = 0);

    // 只读访问直接子控件。库内需要遍历子树的代码用它即可，不必为此继承
    // DuiControl 去取受保护的 m_children。
    const std::vector<std::unique_ptr<DuiControl>>& Children() const { return m_children; }

protected:
    // Internal: host calls this when adding control to the tree.
    void SetParent_(DuiControl* parent) { m_pParent = parent; }
    void SetHost_(DuiHost* host)        { m_pHost = host; }

    // 宿主内部使用：对本控件及其全部子孙按先父后子的顺序调用 OnDpiChanged(dpi)。
    void DispatchDpiChanged_(int dpi);

    // 宿主内部使用：按各子孙控件当前的矩形，把整棵子树（不含本控件）逐层重新布局
    // 一遍。容器给子控件 SetRect 时，矩形没变的子控件不会重新布局，而 DPI 变化后
    // 它们的内部排版（依赖字号）可能已经过时，所以 DPI 变化时要显式走这一遍。
    void RelayoutDescendants_();

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
