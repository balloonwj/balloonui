#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiMnemonic —— 助记符前缀解析 helper
// =================================================================
//
// 用途：DUI 控件的 label 里嵌 "&X" 让用户按 Alt+X 激活控件，与 Win32
// 标准行为一致。共享给 DuiButton、DuiLabel、DuiMenu、未来 DuiTabPage /
// DuiGroupBox 标题等所有需要这个能力的控件。
//
// 约定：
//   · 单个 '&' 标记下一个字符为助记符。Windows GDI 的 DrawText（不带
//     DT_NOPREFIX）渲染时给该字符画下划线（默认隐藏，按 Alt 时显示）。
//   · "&&" 是字面 '&' 转义 —— 两个字符都不被当助记符。
//
// 本 header 的函数全是<u>纯函数</u>：无副作用 / 无 side table，可在
// 不依赖 DC / HWND 的单测里直接调。
//
// 代码用法：
//
//     TCHAR ch = balloonwjui::DuiMnemonic::FindChar(_T("&Save"));    // 's'
//     CString visible = balloonwjui::DuiMnemonic::StripPrefix(_T("Save &As"));
//     // visible == _T("Save As")
//
//     // 父对话框接 Alt 键派发的典型用法：
//     if (uMsg == WM_SYSCHAR)
//     {
//         TCHAR pressed = (TCHAR)_totlower((TCHAR)wParam);
//         if (pressed == balloonwjui::DuiMnemonic::FindChar(button.GetText()))
//         {
//             button.PerformClick();
//         }
//     }
//
// XML 用法：N/A（pure helper，不是控件）。

#include <windows.h>

namespace balloonwjui {

namespace DuiMnemonic
{
    // 返回 text 里第一个单个 '&' 后的字符（小写形式）。无则 0。
    // "&&" 被视为转义跳过；末尾孤零零的 '&' 返 0。
    //
    //   "&Save"        -> 's'
    //   "Save &As"     -> 'a'
    //   "Save && Quit" -> 0     （只有转义、没有真前缀）
    //   "&&&Foo"       -> 'f'   （前两个 && 转义后剩 &Foo）
    //   "trailing&"    -> 0
    //   ""             -> 0
    //   nullptr        -> 0
    TCHAR FindChar(LPCTSTR text);

    // 把 text 里所有单 '&' 标记去掉（"&&" 折成一个 '&'），返回可直接
    // 显示给用户的字符串。用于那些自绘 glyph 路径（不走 GDI DT_PREFIX）
    // 的控件。
    CString StripPrefix(LPCTSTR text);
}

} // namespace balloonwjui
