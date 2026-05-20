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
