#pragma once

// =================================================================
// DUI 通知机制 —— WM_DUI_NOTIFY + struct DuiNotify
// =================================================================
//
// 用途：DuiControl 没有 HWND，业务想监听 click / value-changed / focus
// 等事件，必须有一个统一的"子控件 → host → host-的-HWND-父" 的转发约定。
// 这是这个约定的载体。
//
// 流程：控件触发交互（如 LButton-up 在按钮内）→ 调
// DuiControl::NotifyParent(code, extra) → 经 host 同步 SendMessage(
// WM_DUI_NOTIFY, ctrlId, &DuiNotify) 到 host 父 HWND。父按 ctrlId +
// code 派发到具体处理函数。详见 guides.html §6 事件处理与路由。

#include <windows.h>

// WM_DUI_NOTIFY 是预处理器宏，放在文件域（宏不能放 namespace）。
// 它携带的 C++ 类型（DuiNotifyCode、DuiNotify struct）放在下面的
// balloonwjui 命名空间里。
#define WM_DUI_NOTIFY  (WM_USER + 0x500)

namespace balloonwjui {

// 通用通知码。每个控件可以从 DUIN_CUSTOM 起扩展自家的 NotifyCode。
enum DuiNotifyCode
{
    DUIN_FIRST              = 0,
    DUIN_CLICK,             // 左键点击
    DUIN_DBLCLK,            // 左键双击
    DUIN_RCLICK,            // 右键点击
    DUIN_MOUSEENTER,
    DUIN_MOUSELEAVE,
    DUIN_SETFOCUS,
    DUIN_KILLFOCUS,
    DUIN_VALUECHANGED,      // 通用状态变化（选中 / 勾选 / 文字 等）
    DUIN_CUSTOM             = 0x1000   // 控件特有 NotifyCode 从这往后扩展
};

#pragma pack(push, 1)
struct DuiNotify
{
    UINT    code;       // DuiNotifyCode 或控件特有码（>= DUIN_CUSTOM）
    UINT    ctrlId;     // 触发控件的 m_uCtrlId（0 = 匿名）
    void*   pCtrl;      // 触发控件的裸指针（<u>不要</u> delete；仅在
                        //  SendMessage 调用栈期间有效）
    LPARAM  extra;      // 控件特有载荷（例如 DUIN_VALUECHANGED 时是新值）
};
#pragma pack(pop)

// 父 HWND 收到的消息布局：WPARAM = (UINT)ctrlId，LPARAM = (DuiNotify*)。
// SendMessage 是同步的，所以 DuiNotify 在发送方的栈上即可。

} // namespace balloonwjui
