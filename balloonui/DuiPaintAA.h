#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiAA —— 抗锯齿 GDI+ 绘制 helper
// =================================================================
//
// 用途：所有 DUI 控件画"非轴对齐"形状（三角形、菱形、圆、对角线、
// RoundRect 之外的圆角）都必须走这里。原因：原始 GDI 的 Polygon /
// Ellipse / LineTo <u>不带</u>抗锯齿，对角边在屏上看到明显的锯齿
// （阶梯像素）；GDI+ 自带 SmoothingMode 解决这个问题。
//
// COLORREF 进出：标准 0x00BBGGRR 布局。Alpha 固定 255（不透明）；
// 真要带透明的话单独加重载（目前所有 DUI 控件都不需要）。
//
// GDI+ 在第一次调用时<u>懒</u>初始化（进程级 token），无需 caller
// 显式 startup / shutdown。
//
// 代码用法：
//
//     POINT tri[3] = { {10, 2}, {18, 14}, {2, 14} };
//     balloonwjui::DuiAA::FillPolygon(hdc, tri, 3, RGB(45,108,223));
//     balloonwjui::DuiAA::FillEllipse(hdc, RECT{0,0,16,16}, RGB(220,40,40));
//     balloonwjui::DuiAA::DrawLine(hdc, 0, 0, 100, 50, RGB(0,0,0), 1.5f);
//
// XML 用法：N/A（绘制 helper，不是控件）。

#include <windows.h>
#include "BalloonUiApi.h"

namespace balloonwjui {

namespace DuiAA
{
    // 抗锯齿填充多边形，可选 1px 抗锯齿描边。
    //   hdc：目标 DC（一般 host 背缓冲）。
    //   pts / count：多边形顶点数组。
    //   fillRgb：填充色。
    //   outlineRgb：CLR_INVALID 时不画描边。
    //   outlineWidth：描边宽度（pt 像素）。
    BUI_API void FillPolygon(HDC hdc,
                             const POINT* pts, int count,
                             COLORREF fillRgb,
                             COLORREF outlineRgb = CLR_INVALID,
                             float outlineWidth = 1.0f);

    // 抗锯齿填充椭圆，可选描边。参数同 FillPolygon。
    BUI_API void FillEllipse(HDC hdc, const RECT& rc,
                             COLORREF fillRgb,
                             COLORREF outlineRgb = CLR_INVALID,
                             float outlineWidth = 1.0f);

    // 抗锯齿填充圆角矩形，可选描边。
    //   hdc：目标 DC。
    //   rc：外接矩形（含右下边界，与 GDI ::RoundRect 语义一致）。
    //   fillRgb：填充色；CLR_INVALID 时不填充。
    //   radius：四角圆角半径（像素）；<=0 退化为普通矩形；
    //           >min(w,h)/2 时自动夹到 min(w,h)/2。
    //   outlineRgb：描边色；CLR_INVALID 时不描边。
    //   outlineWidth：描边宽度（pt 像素，可小数）。
    // 用途：替代 ::RoundRect 的锯齿圆角；DuiButton 外框走这里以消除
    // 8px 圆角在屏上的阶梯像素。
    BUI_API void FillRoundRect(HDC hdc, const RECT& rc,
                               COLORREF fillRgb,
                               int radius,
                               COLORREF outlineRgb = CLR_INVALID,
                               float outlineWidth = 1.0f);

    // 抗锯齿线段。
    //   x1/y1/x2/y2：两端点像素坐标。
    //   rgb：线色。
    //   width：宽度（pt 像素，可小数）。
    BUI_API void DrawLine(HDC hdc, int x1, int y1, int x2, int y2,
                          COLORREF rgb, float width = 1.0f);
}

} // namespace balloonwjui
