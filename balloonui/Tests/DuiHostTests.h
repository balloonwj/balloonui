#pragma once

/**
 *  Unit tests for DuiHost's mouse-wheel dispatching (bubbling up the
 *  parent chain). balloonwj@qq.com   2026-08-14
 */

// BalloonUiFeatures.h must come first: the .cpp guards its DuiScrollView
// cases with BUI_FEATURE_SCROLLBAR, and an undefined macro would silently
// evaluate to 0 and compile those cases away.
#include "../BalloonUiFeatures.h"
#include "../DuiHost.h"

namespace balloonwjui {

// Self-contained unit tests for DuiHost::DispatchMouseWheel.
//
// Runs in-process with no HWND and no message loop: DispatchMouseWheel is a
// static, HWND-free tree walk, so the tests only need a small control tree
// built out of stub controls (plus one real DuiScrollView for the
// "self-scrolling control at its boundary" case).
//
// 2026-10-14 起另含 DuiHost::SendNotify 投递目标的用例（DUI-N1 有所有者的 WS_POPUP 宿主发回自身、DUI-N2 WS_CHILD
// 宿主发给父窗口）：这两条建不显示的真实窗口，不需要消息循环（SendMessage 发给本线程的窗口是直接调用）。
//
// Returns a multi-line CString report; the last line is the summary.
namespace DuiHostTests {
    CString RunAll();
}

} // namespace balloonwjui
