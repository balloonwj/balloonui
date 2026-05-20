#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_EDIT

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../HwndHostControl.h"
#include "../../DuiNotify.h"
#include <functional>

namespace balloonwjui {

// =================================================================
// DuiEditHost —— 单 / 多行文本输入（HWND-hosted，包 Win32 EDIT）
// =================================================================
//
// 用途：所有需要"用户输入文字"的场景。balloonui 里两个明确的"HWND
// 逃生口"之一（另一个是 DuiRichEditHost）—— 因为 IME 候选条 + Win32
// 光标渲染只能在真 HWND 里跑，windowless 重写代价过大。对外它跟普通
// DuiControl 一样可以参与 layout、被 enable / disable / size / hide，
// caller 不需要意识到底下藏着 HWND。
//
// 在裸 EDIT 控件之上加的能力：
//   · 占位文字（"default text"）—— 空且未聚焦时显示灰字，对应
//     CSkinEdit::SetDefaultText 的语义。
//   · 多行模式开关 —— 必须在 EnsureCreated 之前调（多行风格是
//     baked-in 的窗口风格位）。
//   · 密码模式开关（同样 pre-Create only）。
//   · 只读 / 最大长度透传到 EDIT。
//   · 在 host OnPaint 里画 1px 边框（EDIT 自身关闭了 WS_BORDER /
//     WS_EX_STATICEDGE，让我们能跟 DUI focus / hover 样式联动着画）。
//
// 工作机制：
//   · HwndHostControl —— 自己持有一个 Win32 EDIT 子 HWND，
//     EnsureCreated(parentHwnd) 时真创建。multi-line / password 风格
//     是 CreateWindowEx 时 baked-in 的，flip 这俩需要销毁重建。
//   · DUI 侧只画 1px 边框（颜色随 enabled / focused / hover 变）；
//     EDIT 自己画文字。占位文字也是我们画的（空 + 未聚焦才画）。
//   · 文字变化通过 EN_CHANGE → host WM_COMMAND → OnHwndCommand 路径
//     转回业务 DUIN_VALUECHANGED。
//   · 密码字段可选"小眼睛"切换按钮 —— 在 EDIT <u>外</u>的 strip 里，
//     接收正常 DuiControl 鼠标事件（不冲 EDIT 的内部 hit-test）。
//   · 默认字体：微软雅黑 9pt（CreateWindowEx 后 WM_SETFONT 推过去）。
//
// 代码用法：
//
//     auto edit = std::unique_ptr<DuiEditHost>(new DuiEditHost());
//     edit->SetCtrlId(IDC_USERNAME);
//     edit->SetPlaceholder(_T("用户名"));
//     edit->SetMaxLength(32);
//     edit->SetRect(RECT{ 16, 16, 240, 40 });
//     auto* raw = edit.get();
//     parent->AddChild(std::move(edit));
//     raw->EnsureCreated(host.m_hWnd);     // 等 host HWND 就绪后调
//     // 父对话框 WM_DUI_NOTIFY:
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_USERNAME)
//     //       OnUserNameChanged(raw->GetText());
//
// XML 用法（详细属性见 guides.html §3.3.11）：
//
//     <edit id="100"
//           placeholder="请输入用户名"
//           password="false" multiline="false"
//           fixedHeight="32"/>
//
//   注意：跟所有 HWND-hosted 控件一样，builder 不调 EnsureCreated；
//   caller 在 host HWND 就绪后自己找到 edit 节点调一次。
//
// 事件：
//   · DUIN_VALUECHANGED — EN_CHANGE 文字变化（每键触发）；extra = 0。
//   · DUIN_SETFOCUS     — EDIT 获焦（同时 DuiHost 的 focus 跟踪也更新）。
//   · DUIN_KILLFOCUS    — EDIT 失焦。
//
// 替代关系：CSkinEdit。SetBgNormalPic / SetBgHotPic / SetIconPic 保留
// 为空 stub（迁移期占位）。
class BUI_API DuiEditHost : public HwndHostControl
{
public:
    // 控件特有通知码（在 DUIN_CUSTOM 之后扩展，详见 DuiNotify.h）
    enum
    {
        DUIEN_LEFT_ICON_CLICK  = DUIN_CUSTOM + 1,    // 点左侧可点击图标
        DUIEN_RIGHT_ICON_CLICK = DUIN_CUSTOM + 2,    // 点右侧可点击图标
    };

    DuiEditHost();
    ~DuiEditHost() override;

    // ---- Pre-Create 配置（必须在 EnsureCreated 之前调）----

    // 多行 / 单行。EnsureCreated 后再改会触发 HWND 销毁重建。
    //   b：true = 多行；false（默认）= 单行。
    void    SetMultiLine(bool b);

    // 密码模式（显示 ●●●●）。EnsureCreated 后再改也是销毁重建。
    //   b：true = 密码；false（默认）= 普通文本。
    void    SetPassword (bool b);
    bool    IsMultiLine() const { return m_multiLine; }
    bool    IsPassword () const { return m_password;  }

    // ---- 密码字段的小眼睛切换按钮 ----

    // 是否在 EDIT 右侧画一个小眼睛按钮（仅 password=true 时生效）。
    // 点击切换 EDIT 的 mask 字符（系统默认 ● 与 0 = 明文 之间切）。
    //   b：true = 显示按钮；false（默认）= 不显示。
    void    SetShowEyeToggle(bool b);
    bool    HasEyeToggle() const     { return m_showEye; }
    bool    IsPasswordRevealed() const { return m_pwdRevealed; }

    // 主动设置密码是否处于"明文"显示状态。
    void    SetPasswordRevealed(bool b);

    // ---- HWND 创建 ----

    // 创建 / 销毁 EDIT 子 HWND。caller 必须在 host HWND 就绪后调一次。
    //   hwndParent：host HWND（一般 = host.m_hWnd）。
    //   返回：true 创建成功；false 失败。
    bool    EnsureCreated(HWND hwndParent);

    // ---- 文本读写 ----

    // 设置 / 读取 EDIT 文本。HWND 未建时只更新缓存；建好后既更新缓存又
    // 同步到 EDIT。GetText 总是返回 m_cachedText（在 HWND 文字变化时
    // 自动 RefreshCacheFromHwnd 同步）。
    // virtual 让子类（如 DuiSearchBox）能在 SetText 完成后做"看到文字
    // 变化才需要做的事"（如刷新 × 清除按钮显隐）—— 没 HWND 时 EN_CHANGE
    // 不会发，要靠子类主动钩。
    virtual void SetText(LPCTSTR sz);
    CString GetText() const;

    // 设置 / 读取只读模式。
    void    SetReadOnly(bool b);
    bool    IsReadOnly() const { return m_readonly; }

    // 设置 / 读取最大字符数。0 = 无限制（EDIT 默认上限按 OS）。
    void    SetMaxLength(int n);
    int     GetMaxLength() const { return m_maxLen; }

    // 设置内边距（EDIT 文字到边框的间距）。透传到 EM_SETMARGINS。
    //   left/top/right/bottom：>= 0 的整数。默认 4 / 2 / 4 / 2。
    void    SetMargins(int left, int top, int right, int bottom);

    // ---- EDIT 整体底色 ----
    //
    // 同时控制 (1) DUI 侧 OnPaint 在 EDIT 周围 margin 区填的底色；
    //         (2) Win32 EDIT 控件本身的 bg（通过 WM_CTLCOLOREDIT 返新画刷）。
    // 默认 RGB(255,255,255) 白底，行为与本 API 加入前一致。改成 pill
    // 灰底等场景：SetBgColor(RGB(0xF3, 0xF3, 0xF4))。
    //
    // 内部还会把同一个色 propagate 给 HwndHostControl::SetCtlBgColor，让
    // OS 渲染 EDIT 文字时背景也用这个色（避免 DUI margin 是灰、EDIT 内
    // 部仍白的"边界感"）。
    void    SetBgColor(COLORREF c);
    COLORREF GetBgColor() const { return m_bgColor; }

    // 是否画 1px 边框（默认 true）。把 EDIT 嵌进自带圆角的容器（如
    // pill 形 SearchBox）时关掉，避免 EDIT 的方框边压在容器圆角上不好看。
    void    SetShowBorder(bool b);
    bool    IsShowBorder() const { return m_showBorder; }

    // ---- 内联图标（默认全部关闭，零回归）----
    //
    // 给 EDIT 框的左 / 右 gutter 内嵌图标。常见用途：搜索框左侧放大镜、
    // 邮箱框左侧 @、密码框右侧 👁（和 SetShowEyeToggle 二选一）等。
    // EDIT 文字区<u>自动避让 gutter</u>（Layout 把 HWND inner.left/right
    // 内缩对应宽度），不会盖到图标。
    //
    // 默认状态：左右 gutter 宽度都是 0、painter null、不可点击 —— 现有
    // 没调本 API 的 caller 行为不变。
    enum IconSlot
    {
        LeftIcon  = 0,
        RightIcon = 1
    };

    // Painter callback —— caller 在 callback 里用任何 GDI / GDI+ API 把
    // 图标画到指定 RECT 内。RECT 是 host 客户区坐标，宽度 = 调用 SetIcon
    // 时的 gutterWidth，高度 = EDIT 高度（含 border / margin）。
    using IconPainter = std::function<void(HDC hdc, const RECT& rcIcon)>;

    // 设图标核心 API。
    //   slot：LeftIcon / RightIcon。
    //   gutterWidth：图标占 EDIT 一侧多少 px。0 = 移除该侧图标。
    //   painter：画法 callback。nullptr 等价于 0 宽度（移除）。
    //
    // 注意：右侧图标若已通过 SetShowEyeToggle(true) 启用了密码小眼睛，
    // SetIcon(RightIcon, ...) 会被忽略 —— 小眼睛优先。
    void    SetIcon(IconSlot slot, int gutterWidth, IconPainter painter);

    // 简便：用 HBITMAP 当图标。caller 持有 HBITMAP 所有权（不 delete）。
    // 内部 painter 在 RECT 内居中 BitBlt 整张图，不缩放。
    void    SetIconBitmap(IconSlot slot, int gutterWidth, HBITMAP hbm);

    // 简便：用 Unicode 字符（如 _T("🔍") / _T("@")）当图标，
    // 用当前默认字体（DuiResMgr::GetDefaultFont）居中 DrawText。
    //   color：文字色。
    void    SetIconGlyph(IconSlot slot, int gutterWidth,
                         LPCTSTR glyph, COLORREF color);

    // 清除某侧图标 —— gutter 宽度归 0、文字区扩回。
    void    ClearIcon(IconSlot slot);

    // 图标可点击。默认 false（图标是装饰，鼠标穿过到 EDIT 设 caret）。
    // true 时鼠标 hit-test 落在 gutter 内会被本控件消费，点击时发
    // DUIN_EDIT_LEFT_ICON_CLICK / DUIN_EDIT_RIGHT_ICON_CLICK 通知给父
    // host；caller 在 OnDuiNotify 里按 ctrl id + 通知码识别处理。
    void    SetIconClickable(IconSlot slot, bool clickable);
    bool    IsIconClickable(IconSlot slot) const;

    int     GetIconWidth   (IconSlot slot) const;

    // ---- 占位文字（empty + 未聚焦时显示）----
    //
    // 实现：透传到 OS EDIT 原生的 EM_SETCUEBANNER。<u>不是</u>由 host
    // 自绘 —— DUI host 在 backbuffer 画的任何 placeholder 字一定会被
    // EDIT child HWND 在其后画的白底覆盖（child window 的 paint 顺序
    // 由 OS 决定，host 永远在下层）。所以 placeholder 必须由 EDIT 自
    // 己画。EM_SETCUEBANNER 是 Vista+ 原生支持的能力，OS 在 EDIT empty
    // 状态下用半透明灰画 cue 字；获焦后是否消失由第二参（FALSE = 获
    // 焦消失，TRUE = 获焦保持）控制 —— 我们用 FALSE，与 Win32 EDIT
    // 默认行为一致。
    //
    // SetPlaceholder 可在 EnsureCreated <u>前后</u>调；在前调缓存到
    // m_placeholder，EnsureCreated 时一并 sync 到 EDIT；在后调直接
    // 发 EM_SETCUEBANNER。

    void    SetPlaceholder(LPCTSTR sz);
    CString GetPlaceholder() const { return m_placeholder; }

    // 总开关。SetShowPlaceholder(false) 等价于 SetPlaceholder(_T(""))
    // 在 EDIT 上的可见效果（cue 清空），但不丢失 m_placeholder 缓存
    // 字符串 —— 重新 SetShowPlaceholder(true) 会把 cue 复原。
    void    SetShowPlaceholder(bool b);
    bool    IsShowingPlaceholder() const;

    // ---- DuiControl 覆写 ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnSetCursor(POINT pt) override;
    bool    OnLButtonUp (POINT pt, UINT mkFlags) override;
    bool    OnMouseMove (POINT pt, UINT mkFlags) override;
    bool    OnMouseLeave() override;

    // ---- HwndHostControl 覆写（接 EDIT 的 EN_* 通知）----

    // host 把 WM_COMMAND 里携带的 EN_CHANGE / EN_SETFOCUS / EN_KILLFOCUS
    // 转到这里。
    void    OnEditNotify(UINT enCode);
    void    OnHwndCommand(UINT enCode) override { OnEditNotify(enCode); }

    // 从真 EDIT 拉取最新文本到 m_cachedText，<u>不发通知</u>。public 是
    // 因为内嵌 EDIT 的复合控件（如 DuiComboBox）需要在用户输入后保持
    // 自家 cache 同步、又不想把 VALUECHANGED 沿内部 EDIT 的 ctrlId 冒上去。
    void    RefreshCacheFromHwnd();

    // ---- 测试用 ----
    void    Test_SetTextDirect(LPCTSTR sz)  { m_cachedText = sz ? sz : _T(""); }
    CString Test_GetCachedText() const      { return m_cachedText; }
    void    Test_SetFocused(bool b)         { m_bFocused = b; }

protected:
    // 内联图标点击 hook —— 子类可以拦截不让 DUIEN_*_ICON_CLICK 冒泡到
    // 父 host。返 true = 已在本类内处理，不发通知；返 false（默认）=
    // 发 DUIEN_LEFT_ICON_CLICK / DUIEN_RIGHT_ICON_CLICK 给父。
    // 典型用法：DuiSearchBox 拦右侧 × → SetText("") 自吞，不要让父业务
    // 关心"× 是搜索框内部行为"。
    virtual bool OnIconClicked_(IconSlot slot) { (void)slot; return false; }

public:

    // 单测用纯函数 helper：给定 host-客户区坐标的本控件 rect、border、
    // margin、gutter 宽，反推图标的画图 RECT。无副作用。
    //   m_rcItem 等价整个 EDIT host rect；borderPx 一般是 1（DUI 边框）；
    //   marginVPx = m_marginT；slot=Left → 紧贴左 border 内侧；
    //   slot=Right → 紧贴右 border 内侧。gutterWidth<=0 返回空 RECT。
    static RECT ComputeIconRect(const RECT& rc, IconSlot slot,
                                int gutterWidth, int borderPx,
                                int marginVPx);

    // 老 CSkinEdit 的 PNG 路径 setter，空 stub（迁移期占位）。
    void    SetBgNormalPic(LPCTSTR /*p*/, RECT* /*nine*/ = nullptr) {}
    void    SetBgHotPic   (LPCTSTR /*p*/, RECT* /*nine*/ = nullptr) {}
    void    SetIconPic    (LPCTSTR /*p*/) {}

private:
    // 边框颜色按 enabled / focused / hover 状态变化。
    COLORREF BorderColor() const;
    void     SyncReadOnlyToHwnd();
    void     SyncTextToHwnd();
    void     SyncMaxLengthToHwnd();
    // 把 m_placeholder（或 _T("") 当 m_showPlaceholder=false）同步到 EDIT
    // 的 cue banner。EDIT 未创建时是 no-op。
    void     SyncPlaceholderToHwnd();

    // 小眼睛按钮的 helper。
    bool     EyeVisible() const { return m_showEye && m_password; }
    RECT     EyeRect()    const;        // 不显示时返回空 rect
    void     ApplyPasswordRevealToHwnd();

private:
    bool        m_multiLine     = false;
    bool        m_password      = false;
    bool        m_readonly      = false;
    int         m_maxLen        = 0;        // 0 = 无限
    CString     m_cachedText;               // EDIT 内容的镜像（SetText / OnEditNotify 同步）
    CString     m_placeholder;
    bool        m_showPlaceholder = true;
    int         m_marginL = 4, m_marginT = 2, m_marginR = 4, m_marginB = 2;
    UINT        m_ctrlIdHwnd = 0;           // 子 HWND 的 control id（创建时分配）

    // 小眼睛按钮状态（仅 m_password && m_showEye 时有意义）。
    bool        m_showEye      = false;
    bool        m_pwdRevealed  = false;
    bool        m_eyeHover     = false;
    TCHAR       m_savedMaskChar = 0;        // 原始 EM_GETPASSWORDCHAR；首次 reveal 时缓存

    // 内联图标 + EDIT 底色（默认全 0 / 白，调本 API 前行为不变）
    struct IconState
    {
        int         width      = 0;
        IconPainter painter;
        bool        clickable  = false;
        bool        hover      = false;
    };
    IconState   m_iconL;
    IconState   m_iconR;
    COLORREF    m_bgColor      = RGB(255, 255, 255);
    bool        m_showBorder   = true;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT
