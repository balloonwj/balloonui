#include "stdafx.h"
#include "DuiPaintAA.h"
#include <gdiplus.h>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

namespace {

ULONG_PTR& GdipToken()
{
    static ULONG_PTR s_token = 0;
    return s_token;
}

void EnsureGdiplus()
{
    if (GdipToken() != 0)
    {
        return;
    }
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&GdipToken(), &input, nullptr);
}

inline Gdiplus::Color ToGdiColor(COLORREF c)
{
    return Gdiplus::Color(255,
                          GetRValue(c),
                          GetGValue(c),
                          GetBValue(c));
}

// Apply the standard DUI smoothing settings to a Graphics. Pixel-offset
// HighQuality keeps integer-coordinate fills crisp while letting diagonal
// edges anti-alias.
void ConfigureSmoothing(Gdiplus::Graphics& g)
{
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
}

} // namespace

namespace DuiAA {

void FillPolygon(HDC hdc, const POINT* pts, int count,
                 COLORREF fillRgb, COLORREF outlineRgb, float outlineWidth)
{
    if (!pts || count < 2)
    {
        return;
    }
    EnsureGdiplus();

    Gdiplus::Graphics g(hdc);
    ConfigureSmoothing(g);

    // Convert POINT[] to Gdiplus::PointF[] with a small inset bias so the
    // anti-aliased rasterizer doesn't round outside our integer rect.
    std::vector<Gdiplus::PointF> gp;
    gp.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        gp.push_back(Gdiplus::PointF((Gdiplus::REAL)pts[i].x,
                                     (Gdiplus::REAL)pts[i].y));
    }

    if (fillRgb != CLR_INVALID)
    {
        Gdiplus::SolidBrush br(ToGdiColor(fillRgb));
        g.FillPolygon(&br, gp.data(), count);
    }
    if (outlineRgb != CLR_INVALID)
    {
        Gdiplus::Pen pen(ToGdiColor(outlineRgb), outlineWidth);
        g.DrawPolygon(&pen, gp.data(), count);
    }
}

void FillEllipse(HDC hdc, const RECT& rc,
                 COLORREF fillRgb, COLORREF outlineRgb, float outlineWidth)
{
    EnsureGdiplus();
    Gdiplus::Graphics g(hdc);
    ConfigureSmoothing(g);

    int w = rc.right  - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    if (fillRgb != CLR_INVALID)
    {
        Gdiplus::SolidBrush br(ToGdiColor(fillRgb));
        // Inset slightly to keep the outline inside the integer rect.
        g.FillEllipse(&br,
                      (Gdiplus::REAL)rc.left,
                      (Gdiplus::REAL)rc.top,
                      (Gdiplus::REAL)w - 1,
                      (Gdiplus::REAL)h - 1);
    }
    if (outlineRgb != CLR_INVALID)
    {
        Gdiplus::Pen pen(ToGdiColor(outlineRgb), outlineWidth);
        g.DrawEllipse(&pen,
                      (Gdiplus::REAL)rc.left,
                      (Gdiplus::REAL)rc.top,
                      (Gdiplus::REAL)w - 1,
                      (Gdiplus::REAL)h - 1);
    }
}

void FillRoundRect(HDC hdc, const RECT& rc,
                   COLORREF fillRgb, int radius,
                   COLORREF outlineRgb, float outlineWidth)
{
    EnsureGdiplus();

    int w = rc.right  - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    Gdiplus::Graphics g(hdc);
    ConfigureSmoothing(g);

    // 与 FillEllipse 一致：路径 / 描边沿用 [left, right-1] × [top, bottom-1]
    // 整数包围盒，避免抗锯齿外扩半个像素到右下相邻像素。
    const Gdiplus::REAL fx = (Gdiplus::REAL)rc.left;
    const Gdiplus::REAL fy = (Gdiplus::REAL)rc.top;
    const Gdiplus::REAL fw = (Gdiplus::REAL)(w - 1);
    const Gdiplus::REAL fh = (Gdiplus::REAL)(h - 1);

    // 半径夹紧：上限为 min(w,h)/2（再大就退化成胶囊形）。
    int r = radius;
    if (r < 0)
    {
        r = 0;
    }
    int halfMin = ((w < h) ? w : h) / 2;
    if (r > halfMin)
    {
        r = halfMin;
    }

    Gdiplus::GraphicsPath path;

    if (r <= 0)
    {
        // 退化为普通矩形 —— 直接用 FillRectangle / DrawRectangle
        // （AA 对轴对齐矩形无效果，但保持 API 通用性）。
        if (fillRgb != CLR_INVALID)
        {
            Gdiplus::SolidBrush br(ToGdiColor(fillRgb));
            g.FillRectangle(&br, fx, fy, fw, fh);
        }
        if (outlineRgb != CLR_INVALID)
        {
            Gdiplus::Pen pen(ToGdiColor(outlineRgb), outlineWidth);
            g.DrawRectangle(&pen, fx, fy, fw, fh);
        }
        return;
    }

    // 用四段 90°弧 + CloseFigure 构造圆角矩形闭合路径。
    // AddArc(x, y, width, height, startAngle, sweepAngle) —— 在以
    // (x,y,width,height) 为外接矩形的椭圆上取一段弧。
    const Gdiplus::REAL d = (Gdiplus::REAL)(r * 2);   // 弧的外接正方形边长
    // 左上角弧：起 180°，扫 90°
    path.AddArc(fx,                fy,                d, d, 180.0f, 90.0f);
    // 右上角弧：起 270°，扫 90°
    path.AddArc(fx + fw - d,       fy,                d, d, 270.0f, 90.0f);
    // 右下角弧：起 0°，扫 90°
    path.AddArc(fx + fw - d,       fy + fh - d,       d, d,   0.0f, 90.0f);
    // 左下角弧：起 90°，扫 90°
    path.AddArc(fx,                fy + fh - d,       d, d,  90.0f, 90.0f);
    path.CloseFigure();

    if (fillRgb != CLR_INVALID)
    {
        Gdiplus::SolidBrush br(ToGdiColor(fillRgb));
        g.FillPath(&br, &path);
    }
    if (outlineRgb != CLR_INVALID)
    {
        Gdiplus::Pen pen(ToGdiColor(outlineRgb), outlineWidth);
        g.DrawPath(&pen, &path);
    }
}

void DrawLine(HDC hdc, int x1, int y1, int x2, int y2,
              COLORREF rgb, float width)
{
    EnsureGdiplus();
    Gdiplus::Graphics g(hdc);
    ConfigureSmoothing(g);
    Gdiplus::Pen pen(ToGdiColor(rgb), width);
    g.DrawLine(&pen,
               (Gdiplus::REAL)x1, (Gdiplus::REAL)y1,
               (Gdiplus::REAL)x2, (Gdiplus::REAL)y2);
}

} // namespace DuiAA

} // namespace balloonwjui
