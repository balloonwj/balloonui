#include "stdafx.h"
#include "PaintHelpers.h"

#include <gdiplus.h>

namespace cloudmelody {

namespace PaintHelpers {

namespace {

// 把 COLORREF (BGR + 1 byte) 转 GDI+ Color (ARGB)。
Gdiplus::Color ToGdiColor(COLORREF c)
{
    return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

// 构造一个圆角矩形路径。caller 已 inset，本函数直接用传入的 L/T/R/B。
void AddRoundedRectPath(Gdiplus::GraphicsPath& path,
                        Gdiplus::REAL L, Gdiplus::REAL T,
                        Gdiplus::REAL R, Gdiplus::REAL B,
                        int radius)
{
    Gdiplus::REAL d = (Gdiplus::REAL)(radius * 2);
    // clamp 圆角防止半径 > 半边长（小卡片 + 大圆角时崩）
    Gdiplus::REAL maxD = (R - L < B - T) ? (R - L) : (B - T);
    if (d > maxD) { d = maxD; }
    if (d <= 0)
    {
        path.AddRectangle(Gdiplus::RectF(L, T, R - L, B - T));
        return;
    }
    path.AddArc(L,     T,     d, d, 180, 90);
    path.AddArc(R - d, T,     d, d, 270, 90);
    path.AddArc(R - d, B - d, d, d, 0,   90);
    path.AddArc(L,     B - d, d, d, 90,  90);
    path.CloseFigure();
}

} // anonymous

void FillRoundedRect(HDC hdc, const RECT& rc, COLORREF color, int radius)
{
    if (radius <= 0)
    {
        // 轴对齐矩形 —— 普通 FillRect 即可，无锯齿
        HBRUSH hbr = ::CreateSolidBrush(color);
        ::FillRect(hdc, &rc, hbr);
        ::DeleteObject(hbr);
        return;
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::SolidBrush brush(ToGdiColor(color));
    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(path,
        (Gdiplus::REAL)rc.left,  (Gdiplus::REAL)rc.top,
        (Gdiplus::REAL)rc.right, (Gdiplus::REAL)rc.bottom,
        radius);
    g.FillPath(&brush, &path);
}

void DrawRoundedRectBorder(HDC hdc, const RECT& rc, COLORREF color,
                            int radius, float width)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::Pen pen(ToGdiColor(color), width);
    // 描边居中在 path 上 → 整体内缩 width/2，避免最外圈像素被裁
    Gdiplus::REAL inset = width * 0.5f;
    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(path,
        (Gdiplus::REAL)rc.left  + inset,
        (Gdiplus::REAL)rc.top   + inset,
        (Gdiplus::REAL)rc.right - inset,
        (Gdiplus::REAL)rc.bottom- inset,
        radius);
    g.DrawPath(&pen, &path);
}

void FillEllipse(HDC hdc, const RECT& rc, COLORREF color)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::SolidBrush brush(ToGdiColor(color));
    g.FillEllipse(&brush,
                  (Gdiplus::REAL)rc.left,
                  (Gdiplus::REAL)rc.top,
                  (Gdiplus::REAL)(rc.right - rc.left),
                  (Gdiplus::REAL)(rc.bottom - rc.top));
}

} // namespace PaintHelpers

} // namespace cloudmelody
