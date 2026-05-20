#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiFocus —— 品牌色键盘焦点框
// =================================================================
//
// 用途：替代 Win32 自带的 ::DrawFocusRect。原 API 用 XOR 画 1px 虚线，
// 在花色背景上对比度差（XOR 与邻像素混合，焦点框可能消失），且无法
// 指定颜色（高对比 / 主题感知 UI 想用品牌蓝就没法用）。
//
// DuiFocus::DrawRing 画一条干净的 2px 实线，按指定 color 上色；可选
// 1px 白色 inset 让深 / 浅背景下都看得见。DuiButton / DuiSlider /
// DuiListBox / DuiEditHost 等控件聚焦时都用它。
//
// 代码用法：
//
//     // 控件 OnPaint 里 IsFocused() 时：
//     balloonwjui::DuiFocus::DrawRingThemed(hdc, m_rcItem);
//
//     // 自定义色（少用；推荐 DrawRingThemed 走主题）：
//     balloonwjui::DuiFocus::DrawRing(hdc, rc, RGB(45, 108, 223));
//
// XML 用法：N/A（绘制 helper）。

#include <windows.h>

namespace balloonwjui {

// Usage:
//   // From a control's OnPaint when IsFocused():
//   balloonwjui::DuiFocus::DrawRingThemed(hdc, m_rcItem);
//
//   // Custom color (rarely needed; prefer DrawRingThemed):
//   balloonwjui::DuiFocus::DrawRing(hdc, rc, RGB(45, 108, 223));

namespace DuiFocus
{
    // Default thickness of the outer ring. 2px reads well at 100-150%
    // DPI; callers at higher DPI can pass a larger value.
    enum { kDefaultThickness = 2 };

    // Draw a focus ring just inside `rc`. Outer ring is `color` at
    // `thickness` px; if `withInset` is true, an additional 1px white
    // ring is drawn one pixel inside the outer ring for contrast on
    // dark surfaces. Cheap GDI; no GDI+ dependency.
    void DrawRing(HDC hdc, const RECT& rc,
                  COLORREF color,
                  int thickness = kDefaultThickness,
                  bool withInset = true);

    // Convenience: read the ring color from DuiTheme::BrandPrimary so
    // call sites don't repeat the lookup. (Defined in cpp to avoid a
    // public DuiTheme dependency in this header.)
    void DrawRingThemed(HDC hdc, const RECT& rc,
                        int thickness = kDefaultThickness,
                        bool withInset = true);
}

} // namespace balloonwjui
