#include "stdafx.h"
#include "CoverArt.h"
#include "../App/CloudMelodyFonts.h"

#include <gdiplus.h>

namespace cloudmelody {

namespace CoverArt {

namespace {

// 6 套预设色板（top → bottom 渐变）。每套两色，挑色思路：
//   · 主色饱和度足够（避免一片灰）
//   · 顶 / 底色亮度差 25-40%（视觉有层次但不刺眼）
//   · 6 套覆盖红 / 紫 / 橙 / 青 / 绿 / 粉，相邻 idx 不撞色
struct Palette
{
    Gdiplus::Color top;
    Gdiplus::Color bottom;
};

static const Palette kPalettes[] = {
    /* 0 红 */    { Gdiplus::Color(255, 0xC8, 0x2D, 0x3A),
                    Gdiplus::Color(255, 0x76, 0x12, 0x1E) },
    /* 1 紫 */    { Gdiplus::Color(255, 0x4F, 0x3D, 0x9C),
                    Gdiplus::Color(255, 0x1E, 0x14, 0x52) },
    /* 2 橙 */    { Gdiplus::Color(255, 0xE9, 0x88, 0x2D),
                    Gdiplus::Color(255, 0x8A, 0x44, 0x14) },
    /* 3 青 */    { Gdiplus::Color(255, 0x2D, 0x8A, 0x99),
                    Gdiplus::Color(255, 0x10, 0x44, 0x52) },
    /* 4 绿 */    { Gdiplus::Color(255, 0x4A, 0x9B, 0x4D),
                    Gdiplus::Color(255, 0x21, 0x55, 0x23) },
    /* 5 粉 */    { Gdiplus::Color(255, 0xE0, 0x6E, 0x9C),
                    Gdiplus::Color(255, 0x88, 0x2C, 0x55) },
};
static const int kPaletteCount = sizeof(kPalettes) / sizeof(kPalettes[0]);

} // anonymous

void Paint(HDC hdc, const RECT& rc, LPCTSTR title,
           int hueSeed, int cornerRadius)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) { return; }

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // 选色：hueSeed 模 6，负数也能 wrap 回来
    int idx = ((hueSeed % kPaletteCount) + kPaletteCount) % kPaletteCount;
    const Palette& p = kPalettes[idx];

    // 1) 渐变填充 —— 用圆角 region 当 clip
    Gdiplus::LinearGradientBrush brush(
        Gdiplus::PointF((Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.top),
        Gdiplus::PointF((Gdiplus::REAL)rc.right, (Gdiplus::REAL)rc.bottom),
        p.top, p.bottom);

    if (cornerRadius > 0)
    {
        // 圆角矩形路径（4 段 ArcTo）
        Gdiplus::GraphicsPath path;
        Gdiplus::REAL d = (Gdiplus::REAL)(cornerRadius * 2);
        Gdiplus::REAL L = (Gdiplus::REAL)rc.left;
        Gdiplus::REAL T = (Gdiplus::REAL)rc.top;
        Gdiplus::REAL R = (Gdiplus::REAL)rc.right;
        Gdiplus::REAL B = (Gdiplus::REAL)rc.bottom;
        path.AddArc(L, T, d, d, 180, 90);
        path.AddArc(R - d, T, d, d, 270, 90);
        path.AddArc(R - d, B - d, d, d, 0, 90);
        path.AddArc(L, B - d, d, d, 90, 90);
        path.CloseFigure();
        g.FillPath(&brush, &path);
    }
    else
    {
        g.FillRectangle(&brush, rc.left, rc.top, w, h);
    }

    // 2) 居中白色标题 —— 字号按矩形较小边自适应
    if (title && title[0])
    {
        // 选字号：cell <80 用 BodySm；80-160 用 TitleSm；>160 用 HeadlineMd
        int side = (w < h) ? w : h;
        Fonts::Kind k = (side < 80)  ? Fonts::BodySm
                      : (side < 160) ? Fonts::TitleSm
                      :                Fonts::HeadlineMd;
        HFONT hf = Fonts::Get(k);
        HFONT hOld = (HFONT)::SelectObject(hdc, hf);
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        // 阴影：先画黑色偏移 1px 再画白色 —— 在淡色渐变区上保留可读性
        RECT shadow = rc;
        ::OffsetRect(&shadow, 1, 1);
        COLORREF oldC = ::SetTextColor(hdc, RGB(0, 0, 0));
        ::DrawText(hdc, title, -1, &shadow,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE
                   | DT_END_ELLIPSIS | DT_NOPREFIX);
        ::SetTextColor(hdc, RGB(255, 255, 255));
        RECT body = rc;
        ::DrawText(hdc, title, -1, &body,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE
                   | DT_END_ELLIPSIS | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOld);
    }
}

} // namespace CoverArt

} // namespace cloudmelody
