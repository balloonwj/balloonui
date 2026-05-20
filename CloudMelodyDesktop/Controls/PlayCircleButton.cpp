#include "stdafx.h"
#include "PlayCircleButton.h"
#include "../App/CloudMelodyTheme.h"

#include "DuiPaintAA.h"

using namespace balloonwjui;

namespace cloudmelody {

// 播放三角的"内缩"比例 —— 三角占圆直径的 0.36 ~ 0.48 范围最像实际播放
// 按钮（太大顶到圆边、太小看不清）。0.40 在 40px 圆上看起来恰好。
static const float kTriangleSizeRatio = 0.40f;
// 三角向右偏移量（视觉上把三角形心置于圆几何中心后看起来略偏左，所以
// 主动右偏 0.06 直径让光学重心居中）。
static const float kTriangleRightShift = 0.06f;
// 暂停态两根矩形的尺寸 / 间距比例。
static const float kPauseBarWidth = 0.16f;   // 每根矩形宽 = 直径 * 0.16
static const float kPauseBarHeight = 0.40f;  // 高 = 直径 * 0.40
static const float kPauseBarGap   = 0.10f;   // 两根之间间距 = 直径 * 0.10

void PlayCircleButton::SetPlaying(bool b)
{
    if (m_playing != b)
    {
        m_playing = b;
        Invalidate();
    }
}

bool PlayCircleButton::OnMouseEnter()
{
    m_hover = true;
    Invalidate();
    return true;
}

bool PlayCircleButton::OnMouseLeave()
{
    m_hover   = false;
    m_pressed = false;
    Invalidate();
    return true;
}

bool PlayCircleButton::OnLButtonDown(POINT, UINT)
{
    m_pressed = true;
    Invalidate();
    return true;
}

bool PlayCircleButton::OnLButtonUp(POINT pt, UINT)
{
    bool wasPressed = m_pressed;
    m_pressed = false;
    Invalidate();
    if (wasPressed && ::PtInRect(&m_rcItem, pt))
    {
        m_playing = !m_playing;       // 立即视觉切换
        Invalidate();
        NotifyParent(DUIN_CLICK, 0);
    }
    return true;
}

bool PlayCircleButton::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    return true;
}

void PlayCircleButton::OnPaint(HDC hdc, const RECT&)
{
    if (!m_bVisible)
    {
        return;
    }

    // 计算居中圆的几何 —— 取 cell 较小边为直径，圆心 = cell 几何中心。
    int w = m_rcItem.right - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    int d = (w < h) ? w : h;
    int cx = (m_rcItem.left + m_rcItem.right) / 2;
    int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;

    // 1) 红色圆（hover 切到 primary-container）—— 用 GDI+ 抗锯齿画
    COLORREF circleColor = m_hover ? kColorPrimaryHover : kColorPrimaryAccent;
    RECT rcCircle = { cx - d / 2, cy - d / 2, cx + d / 2, cy + d / 2 };
    DuiAA::FillEllipse(hdc, rcCircle, circleColor);

    // 2) 中央 glyph
    if (!m_playing)
    {
        // ▶ 三角：3 个顶点。GDI+ 抗锯齿避免斜边锯齿。
        float radius = d * 0.5f;
        float triH = d * kTriangleSizeRatio;
        float triW = triH * 0.866f;            // 等边三角宽 = 高 * cos30°
        float ox = (float)cx + d * kTriangleRightShift;
        float oy = (float)cy;
        POINT tri[3] = {
            { (LONG)(ox - triW * 0.5f), (LONG)(oy - triH * 0.5f) },
            { (LONG)(ox - triW * 0.5f), (LONG)(oy + triH * 0.5f) },
            { (LONG)(ox + triW * 0.5f), (LONG)(oy)               }
        };
        (void)radius;
        DuiAA::FillPolygon(hdc, tri, 3, kColorOnPrimary);
    }
    else
    {
        // ⏸ 两根白色圆角矩形 —— 轴对齐，用普通 RoundRect 即可。
        int barW = (int)(d * kPauseBarWidth);
        int barH = (int)(d * kPauseBarHeight);
        int gap  = (int)(d * kPauseBarGap);
        int leftX  = cx - gap / 2 - barW;
        int rightX = cx + gap / 2;
        int top    = cy - barH / 2;
        int bot    = cy + barH / 2;

        HBRUSH hbrW = ::CreateSolidBrush(kColorOnPrimary);
        HPEN   hpW  = ::CreatePen(PS_NULL, 0, 0);
        HBRUSH oldB = (HBRUSH)::SelectObject(hdc, hbrW);
        HPEN   oldP = (HPEN)  ::SelectObject(hdc, hpW);
        ::RoundRect(hdc, leftX,  top, leftX + barW,  bot, 3, 3);
        ::RoundRect(hdc, rightX, top, rightX + barW, bot, 3, 3);
        ::SelectObject(hdc, oldB);
        ::SelectObject(hdc, oldP);
        ::DeleteObject(hbrW);
        ::DeleteObject(hpW);
    }
}

} // namespace cloudmelody
