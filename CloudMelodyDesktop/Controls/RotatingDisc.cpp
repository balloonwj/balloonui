#include "stdafx.h"
#include "RotatingDisc.h"
#include "../App/CloudMelodyTheme.h"

#include <gdiplus.h>

using namespace balloonwjui;

namespace cloudmelody {

// 33⅓ RPM 真实唱机转速 → 33.333 / 60 * 360 = 200°/s。
static const float kAnglePerSec = 200.0f;

// 圆盘三层尺寸（占 cell 较小边的比例）：
//   外圆 = 1.0（黑胶完整边）
//   内圆 = 0.32（中心标签区，实物上是纸贴）—— 这里也是封面方块的外接圆
//   中心点 = 0.04（主轴洞）
static const float kInnerRingRatio  = 0.32f;
static const float kCenterDotRatio  = 0.04f;
// 封面方块边长 = 内圆直径 / sqrt(2) * 0.95（让方块完全在内圆内、留 5% 余量）
static const float kCoverBoxRatio   = 0.32f * 0.7f;

void RotatingDisc::Tick(int deltaMs)
{
    if (!m_spinning) { return; }
    m_angle += kAnglePerSec * (deltaMs / 1000.0f);
    while (m_angle >= 360.0f) { m_angle -= 360.0f; }
    while (m_angle <  0.0f)   { m_angle += 360.0f; }
    Invalidate();
}

void RotatingDisc::OnPaint(HDC hdc, const RECT&)
{
    if (!m_bVisible) { return; }

    int w = m_rcItem.right - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    int side = (w < h) ? w : h;
    if (side <= 0) { return; }

    float cx = (m_rcItem.left + m_rcItem.right)  * 0.5f;
    float cy = (m_rcItem.top  + m_rcItem.bottom) * 0.5f;
    float outerR  = side * 0.5f;
    float innerR  = side * kInnerRingRatio * 0.5f;
    float dotR    = side * kCenterDotRatio * 0.5f;
    float coverHalf = side * kCoverBoxRatio * 0.5f;

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // 1) 外黑圆盘（黑胶面）
    {
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 28, 28, 28));
        g.FillEllipse(&brush, cx - outerR, cy - outerR,
                      outerR * 2, outerR * 2);
    }

    // 2) 装饰：几道浅色同心环（黑胶纹路）
    {
        Gdiplus::Pen pen(Gdiplus::Color(255, 50, 50, 50), 1.0f);
        for (int i = 1; i <= 5; ++i)
        {
            float r = outerR * (0.45f + 0.10f * i);
            g.DrawEllipse(&pen, cx - r, cy - r, r * 2, r * 2);
        }
    }

    // 3) 把"内圆 + 封面方块"作为旋转组：保存图形状态、平移到中心、
    //    rotate m_angle，画完恢复。
    g.TranslateTransform(cx, cy);
    g.RotateTransform(m_angle);

    // 3a) 内圆（label 圈）—— 浅灰
    {
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 230, 230, 230));
        g.FillEllipse(&brush, -innerR, -innerR, innerR * 2, innerR * 2);
    }

    // 3b) 封面方块 —— M7 polish 起：按 m_coverSeed 选 6 套渐变之一（与
    //     CoverArt 同套调色板，duplicate 6 行避免单独把 palette 抽公共
    //     module）。中央方块跟着 disc 一起旋转。
    {
        struct Pal { Gdiplus::Color t, b; };
        static const Pal palettes[] = {
            { Gdiplus::Color(255, 0xC8, 0x2D, 0x3A), Gdiplus::Color(255, 0x76, 0x12, 0x1E) },
            { Gdiplus::Color(255, 0x4F, 0x3D, 0x9C), Gdiplus::Color(255, 0x1E, 0x14, 0x52) },
            { Gdiplus::Color(255, 0xE9, 0x88, 0x2D), Gdiplus::Color(255, 0x8A, 0x44, 0x14) },
            { Gdiplus::Color(255, 0x2D, 0x8A, 0x99), Gdiplus::Color(255, 0x10, 0x44, 0x52) },
            { Gdiplus::Color(255, 0x4A, 0x9B, 0x4D), Gdiplus::Color(255, 0x21, 0x55, 0x23) },
            { Gdiplus::Color(255, 0xE0, 0x6E, 0x9C), Gdiplus::Color(255, 0x88, 0x2C, 0x55) },
        };
        int idx = ((m_coverSeed % 6) + 6) % 6;
        Gdiplus::LinearGradientBrush brush(
            Gdiplus::PointF(-coverHalf, -coverHalf),
            Gdiplus::PointF( coverHalf,  coverHalf),
            palettes[idx].t, palettes[idx].b);
        g.FillRectangle(&brush, -coverHalf, -coverHalf,
                        coverHalf * 2, coverHalf * 2);
    }

    // 4) 中心实心黑色主轴洞（不旋转 —— 保持视觉锚点稳定）
    g.ResetTransform();
    {
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 0, 0, 0));
        g.FillEllipse(&brush, cx - dotR, cy - dotR, dotR * 2, dotR * 2);
    }
}

} // namespace cloudmelody
