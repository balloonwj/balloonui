#pragma once

// =============================================================================
// CoverArt —— 程序化"封面图"绘制 helper
//
// 不接真 PNG，所有封面位都用 GDI+ 渐变 + 居中标题字渲染。优势：
//   · 没有素材依赖（demo 不带文件、不联网）
//   · 每张卡 / 每首曲都有可识别的色调（hueSeed 决定）
//   · 圆角 / 大小 / 旋转都跟着 caller 给的 RECT
//
// 调用：在 OnPaint 里
//   CoverArt::Paint(hdc, m_rcItem, _T("十年"), 1, kRadiusMd);
// 即可。hueSeed 一般用"曲目下标"或"卡片下标"，相邻 idx 颜色不同。
// =============================================================================

#include <windows.h>

namespace cloudmelody {

namespace CoverArt {

// 在 rc 内画一张程序化封面。
//   hdc：目标 DC（host 背缓冲）
//   rc：目标矩形（不必正方，文字会按矩形缩放居中）
//   title：标题字（白色覆盖在渐变上）。可 nullptr / 空 = 不画字
//   hueSeed：色调种子，任意 int；模 6 选 6 套预设色板之一
//   cornerRadius：圆角（px）。0 = 直角矩形
void Paint(HDC hdc, const RECT& rc, LPCTSTR title,
           int hueSeed, int cornerRadius = 0);

} // namespace CoverArt

} // namespace cloudmelody
