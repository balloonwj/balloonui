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
//     按裸指针匹配；Unregister 应在控件 dtor 里调，否则 hover 时可能拿到
//     已销毁指针 → 走 std::vector 比较时崩。
//   · DuiHost 在 hover 控件变化时调 OnHover / OnLeave —— 不调也不会崩，
//     只是没 tooltip 出来。
//   · 显示走 SetTimer + 内部 DuiToolTipWnd 顶层 WS_POPUP 浮窗（同
//     DuiComboBoxPopup 的 OnFinalMessage 自销毁模式）。
//   · 默认延时 500ms（SetDelay 可调）。HideNow() 让 host 在 resize /
//     capture 切换等场景立刻关掉。
//
// 代码用法：
//
//     // 控件注册：
//     DuiToolTipMgr::Inst().Register(myCtrl, _T("发送一条消息"));
//
//     // 控件 dtor 里取消注册：
//     ~MyCtrl() { DuiToolTipMgr::Inst().Unregister(this); }
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
    // 进程级单例访问入口。线程不安全，仅 host 线程调。
    static DuiToolTipMgr& Inst();

    // 给 ctrl 注册一段 tooltip 文字。重复 Register 同 ctrl 会覆盖文字。
    //   ctrl：受 hover 跟踪的 DuiControl 指针。
    //   text：tooltip 文字（任意字符串）。
    void    Register  (DuiControl* ctrl, LPCTSTR text);

    // 取消注册（控件 dtor 里调）。未注册过的 ctrl 静默忽略。
    void    Unregister(DuiControl* ctrl);

    // 读取已注册的文字。未注册返回空。
    CString GetText   (DuiControl* ctrl) const;

    // hover / leave 钩子 —— DuiHost 在 hover 控件变化时主动调。
    void    OnHover(DuiControl* ctrl, POINT screenPt);
    void    OnLeave(DuiControl* ctrl);

    // 设置 / 读取 tooltip 弹出延时（ms）。默认 500。
    void    SetDelay(UINT ms) { m_delayMs = ms; }
    UINT    GetDelay() const  { return m_delayMs; }

    // 程序触发关闭（host 在 size / capture 切换时调，避免 tooltip 跟丢）。
    void    HideNow();

    // ---- 测试用 ----
    bool    IsShowing() const  { return m_window != nullptr; }
    CString GetShowingText() const;

private:
    DuiToolTipMgr() = default;
    ~DuiToolTipMgr() = default;
    DuiToolTipMgr(const DuiToolTipMgr&) = delete;
    DuiToolTipMgr& operator=(const DuiToolTipMgr&) = delete;

    static VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

    void    StartTimer();
    void    StopTimer();
    void    PopShow();         // 构造 + 显示浮窗
    void    DetachWindow();    // 浮窗自销毁时清空指针

    friend class DuiToolTipWnd;

private:
    std::vector<std::pair<DuiControl*, CString>>  m_entries;
    DuiControl*    m_pending      = nullptr;
    POINT          m_pendingPt    = {};
    UINT           m_delayMs      = 500;
    UINT_PTR       m_timerId      = 0;
    DuiToolTipWnd* m_window       = nullptr;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_TOOLTIP
