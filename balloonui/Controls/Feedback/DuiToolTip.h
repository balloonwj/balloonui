#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_TOOLTIP

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiToolTipMgr —— 进程级单例 tooltip 管理器
// =================================================================
//
// 用途：业务给某个 DuiControl 注册一段提示文字，鼠标悬停 N ms 后弹出
// 一个浮动小窗显示文字；移走 / 控件销毁自动消失。比老 CToolTipCtrl
// 散落在各 CSkin* 控件里干净得多 —— 全进程一个共享的 owner-drawn 浮
// 窗口。
//
// 工作机制：
//   · 单例 + 注册表（vector<DuiControl*, CString>）。Register / Unregister
//     按裸指针匹配；DuiControl 的析构函数会自动调 Unregister，子类不必再调。
//   · DuiHost 在 hover 控件变化时调 OnHover / OnLeave —— 不调也不会崩，
//     只是没 tooltip 出来。
//   · 显示走 SetTimer + 内部 DuiToolTipWnd 顶层 WS_POPUP 浮窗（同
//     DuiComboBoxPopup 的 OnFinalMessage 自销毁模式）。
//   · 默认延时 500ms（SetDelay 可调）。已有提示正在显示、或刚离开一个正在显示的提示
//     不超过一个延时，此时悬停到下一个控件只等五分之一的延时（2026-10-14 起，与 Windows
//     自带提示一致）。
//   · DuiHost 在按下鼠标左键或右键、按键、失去焦点、滚轮滚动了内容时调 HideNow 收起提示
//     （2026-10-14 起）。
//   · 浮窗按弹出位置所在显示器的 DPI 排版；整体限制在工作区内，下方放不下时移到光标
//     上方；文字超过最大宽度时自动换行（均 2026-10-14 起）。
//
// 代码用法：
//
//     // 控件注册：
//     DuiToolTipMgr::Inst().Register(myCtrl, _T("发送一条消息"));
//
//     // 多行提示（2026-10-06 起）：文字里用 '\n' 分行，浮窗按最长一行定宽
//     DuiToolTipMgr::Inst().Register(myCtrl, _T("第一行\n第二行"));
//
//     // 同一控件内光标换到另一块热区、提示文字随之变化（2026-10-14 起）：
//     DuiToolTipMgr::Inst().Retarget(myCtrl, _T("新的提示文字"), screenPt);
//
//     // 控件析构时由 DuiControl 基类自动取消注册；运行期撤掉某个控件的提示时手动调：
//     DuiToolTipMgr::Inst().Unregister(myCtrl);
//
// XML 用法：<u>暂未原生支持</u>。tooltip 是横切式提示（任意控件都可
// 以挂），不适合 per-控件 XML 属性。要 XML 化最自然的形式是给所有控件
// 加一个 tooltip="..." 通用属性 —— 这是一个跨所有 builder 的改造，未来
// 可能加。当前业务侧在 BuildOne 后用 FindControlById + Register 自己挂。
//
// 事件：无（管理器；不发 WM_DUI_NOTIFY）。

#include "../../DuiControl.h"

namespace balloonwjui {

class DuiToolTipWnd;

class BUI_API DuiToolTipMgr
{
public:
    // 提示浮窗的尺寸参数，单位像素，由 MetricsForDpi 按显示器 DPI 换算得到。
    struct Metrics
    {
        int m_padX;          // 文字与浮窗左右边框之间的距离
        int m_padY;          // 文字与浮窗上下边框之间的距离
        int m_offsetX;       // 浮窗放在光标右下方时，左边缘相对光标热点向右的偏移
        int m_offsetY;       // 浮窗放在光标右下方时，上边缘相对光标热点向下的偏移（让开光标箭头）
        int m_gapAbove;      // 下方放不下、浮窗移到光标上方时，浮窗下边缘与光标热点之间的间隙
        int m_maxTextWidth;  // 文字区域的最大宽度，超过时自动换行
    };

    // 提示文字的排版结果。浮窗的测量与绘制共用这一份，保证浮窗尺寸与绘制出的文字一致。
    struct TextLayout
    {
        SIZE m_size;         // 文字区域尺寸，单位像素；文字为空时为 {0, 0}
        UINT m_flags;        // 绘制时交给 DrawText 的格式标志
    };

    // 进程级单例访问入口。线程不安全，仅 host 线程调。
    static DuiToolTipMgr& Inst();

    // 给 ctrl 注册一段 tooltip 文字。重复 Register 同 ctrl 会覆盖文字；已经显示的浮窗不随之
    // 更新，要换掉正在显示的文字用 Retarget。
    //   ctrl：受 hover 跟踪的 DuiControl 指针；为空时忽略。
    //   text：tooltip 文字（任意字符串）。
    void    Register  (DuiControl* ctrl, LPCTSTR text);

    // 取消注册。DuiControl 析构时自动调用；运行期撤掉某个控件的提示时也可调用。未注册过的
    // ctrl 静默忽略。ctrl 的提示正在等待弹出或已经显示时一并取消 / 关闭（2026-10-13 起关闭
    // 已显示的提示，否则窗口在悬停期间被程序关闭时提示框会留在屏幕上）。
    void    Unregister(DuiControl* ctrl);

    // 读取已注册的文字。未注册返回空。
    CString GetText   (DuiControl* ctrl) const;

    // hover 钩子 —— DuiHost 在 hover 控件变化时主动调。ctrl 登记了提示文字时开始计时，到时在
    // screenPt（光标的屏幕坐标）附近弹出；ctrl 已经在计时或已经显示时不做任何事；ctrl 为空或
    // 没有登记提示文字时等同 OnLeave(nullptr)。
    void    OnHover(DuiControl* ctrl, POINT screenPt);

    // leave 钩子 —— DuiHost 在 hover 离开 ctrl 时主动调。ctrl 是当前跟踪的控件、或有提示正在
    // 显示时，停止计时、关闭提示并不再跟踪任何控件；ctrl 为 nullptr 时无条件这样做。离开一个
    // 正在显示的提示之后，一个延时之内悬停到别的控件只等五分之一的延时。
    void    OnLeave(DuiControl* ctrl);

    // 同一控件内光标换到另一块热区、提示文字随之变化时调用（2026-10-14 起）。等价于先
    // OnLeave(ctrl)，再按 text 重新 Register、OnHover(ctrl, screenPt)；text 为空时只取消注册。
    // 原来正在显示的提示先关闭，新文字按连续悬停的短延时弹出。调用方自行判断文字是否变化，
    // 文字没变时不必调用（调用了也只是重新计时）。
    //   ctrl：提示所属的控件；为空时忽略。
    //   text：新的提示文字；nullptr 或空串表示这块热区没有提示。
    //   screenPt：光标的屏幕坐标，浮窗弹在它附近。
    void    Retarget(DuiControl* ctrl, LPCTSTR text, POINT screenPt);

    // 设置 / 读取 tooltip 弹出延时（ms）。默认 500。
    void    SetDelay(UINT ms) { m_delayMs = ms; }
    UINT    GetDelay() const  { return m_delayMs; }

    // 立即收起提示：关闭已显示的浮窗，并停掉还在等延时的弹出计时（2026-10-14 起；此前只关
    // 浮窗、计时照走，提示随后仍会弹出）。仍记住当前跟踪的控件，所以光标停在同一控件上时不会
    // 再弹出，直到 hover 换到别的控件；之后的悬停按正常延时，不按连续悬停的短延时。
    // DuiHost 在按下鼠标左键或右键、按键、失去焦点、滚轮滚动了内容时调用；业务代码在点击之后要收起
    // 提示时也可调用。
    void    HideNow();

    // ---- 排版（2026-10-06 起支持多行；2026-10-14 起超宽自动换行、尺寸随 DPI 缩放）----

    // 按 dpi 换算浮窗的尺寸参数。逻辑值以 96 DPI 为准，dpi <= 0 时按 96 处理。
    //   dpi：浮窗所在显示器的 DPI。
    static Metrics MetricsForDpi(int dpi);

    // 用 hdc 当前选入的字体排版提示文字，测量与绘制共用其结果：
    //   · 不含换行符、且单行宽度不超过 maxWidth：按单行排版（DT_SINGLELINE），尺寸取
    //     GetTextExtentPoint32，与原先一致；
    //   · 否则按多行排版：'\n' 处分行，超过 maxWidth 的行在词间自动换行，比一行还宽的长词
    //     （如没有空格的文件名、路径）按字符断开；宽度取最长的一行，高度为行数 × 行高。
    //   两种排版的格式标志都带 DT_NOPREFIX：'&' 按普通字符显示，不当成助记符前缀（2026-10-14 起）。
    //   hdc：已选好字体的设备上下文（借用，不转移所有权）。
    //   text：提示文字；为空时尺寸为 {0, 0}。
    //   maxWidth：文字区域的最大宽度，单位像素，须大于 0。
    static TextLayout LayoutText(HDC hdc, const CString& text, int maxWidth);

    // ---- 测试用 ----
    bool    IsShowing() const  { return m_window != nullptr; }
    CString GetShowingText() const;
    // 最近一次开始弹出计时所用的延时（ms），供测试检查连续悬停的短延时；从未计时过时为 0。
    UINT    GetScheduledDelay() const { return m_scheduledDelayMs; }

private:
    DuiToolTipMgr() = default;
    ~DuiToolTipMgr() = default;
    DuiToolTipMgr(const DuiToolTipMgr&) = delete;
    DuiToolTipMgr& operator=(const DuiToolTipMgr&) = delete;

    static VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

    void    StartTimer(UINT delayMs);  // 按 delayMs 开始（或重新开始）弹出计时
    void    StopTimer();
    void    PopShow();             // 构造 + 显示浮窗
    void    DestroyTipWindow();    // 只销毁已显示的浮窗，不动计时与跟踪状态
    void    DetachWindow();        // 浮窗自销毁时清空指针
    bool    IsReshowWindowOpen() const;  // 此刻悬停到新控件时是否按连续悬停的短延时弹出

    friend class DuiToolTipWnd;

private:
    // 注册表：控件 → 提示文字，按裸指针匹配
    std::vector<std::pair<DuiControl*, CString>>  m_entries;
    // 当前跟踪的控件：它的提示正在等延时，或已经弹出；为空表示没有跟踪任何控件。提示弹出后
    // 它仍指向该控件。不变式：m_window 非空时，显示的就是 m_pending 的提示（PopShow 只为
    // m_pending 建浮窗，每处改写 m_pending 时都会同时关闭浮窗）。OnHover 的去重与 Unregister
    // 关闭已显示的提示都依赖这一点。
    DuiControl*    m_pending      = nullptr;
    // 开始计时时光标的屏幕坐标，浮窗弹在它附近
    POINT          m_pendingPt    = {};
    // 正常弹出延时，单位 ms
    UINT           m_delayMs      = 500;
    // 最近一次开始计时所用的延时，单位 ms，仅供测试读取
    UINT           m_scheduledDelayMs = 0;
    // 连续悬停短延时时段的截止时刻（GetTickCount64 的值）；0 表示不在该时段。离开一个正在
    // 显示的提示时设置，HideNow 清零
    ULONGLONG      m_reshowUntil  = 0;
    // SetTimer 返回的线程计时器编号；0 表示没有在计时
    UINT_PTR       m_timerId      = 0;
    // 正在显示的浮窗；浮窗自销毁时经 DetachWindow 置空
    DuiToolTipWnd* m_window       = nullptr;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_TOOLTIP
