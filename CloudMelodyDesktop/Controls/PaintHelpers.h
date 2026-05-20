#pragma once

// =============================================================================
// PaintHelpers —— GDI+ 抗锯齿圆角矩形 / 描边帮助函数
//
// 目的：替换业务控件里散落的 `CreateRoundRectRgn + FillRgn`（不抗锯齿，
//      圆角处可见楼梯锯齿）。统一走 GDI+ Graphics + GraphicsPath +
//      AntiAliased SmoothingMode。
//
// 用法：
//     PaintHelpers::FillRoundedRect(hdc, m_rcItem, RGB(180,40,40), 8);
//     PaintHelpers::DrawRoundedRectBorder(hdc, m_rcItem,
//                                          RGB(220,220,220), 18, 1.0f);
// =============================================================================

#include <windows.h>

namespace cloudmelody {

namespace PaintHelpers {

// 抗锯齿填充圆角矩形。radius<=0 退化为普通 FillRect（轴对齐，不需 AA）。
//   hdc：目标 DC
//   rc：矩形区域（左上 inclusive、右下 exclusive，与 GDI 习惯一致）
//   color：填充色（实色，alpha=255）
//   radius：圆角半径（px）
void FillRoundedRect(HDC hdc, const RECT& rc, COLORREF color, int radius);

// 抗锯齿圆角矩形描边。stroke 居中于路径上，所以会向内/外各占 width/2。
//   width：1.0f / 2.0f 等线宽（GDI+ REAL，可小数）
void DrawRoundedRectBorder(HDC hdc, const RECT& rc, COLORREF color,
                            int radius, float width = 1.0f);

// 抗锯齿圆形 region 的 fill 替代品（替代 CreateEllipticRgn）。
void FillEllipse(HDC hdc, const RECT& rc, COLORREF color);

} // namespace PaintHelpers

} // namespace cloudmelody
