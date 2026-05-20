#include "stdafx.h"
#include "HoursBarChart.h"
#include "PaintHelpers.h"
#include "../App/CloudMelodyTheme.h"

using namespace balloonwjui;

namespace cloudmelody {

// 24 小时的归一化强度（0..1）。M4 第一版 mock —— 中午 / 晚上听得多，
// 凌晨听得少。曲线大致 sin-wave 偏 evening peak。
static const float kHourly[] = {
    0.05f, 0.03f, 0.02f, 0.02f, 0.03f, 0.05f,    // 0-5 凌晨
    0.10f, 0.20f, 0.35f, 0.30f, 0.25f, 0.30f,    // 6-11 上午
    0.45f, 0.40f, 0.35f, 0.40f, 0.50f, 0.65f,    // 12-17 下午
    0.85f, 1.00f, 0.95f, 0.80f, 0.40f, 0.20f     // 18-23 晚上
};
static const int kBarCount = sizeof(kHourly) / sizeof(kHourly[0]);

// 柱子之间的间距（px）—— 总宽 = (kBarCount * barW) + ((kBarCount-1) * gap)
static const int kBarGap = 4;
// 柱顶圆角半径（让柱子顶部柔和）
static const int kBarTopRadius = 3;

void HoursBarChart::OnPaint(HDC hdc, const RECT&)
{
    if (!m_bVisible)
    {
        return;
    }

    int w = m_rcItem.right - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // 平摊计算每柱宽度（向下取整保证不溢出）
    int totalGap = kBarGap * (kBarCount - 1);
    int barW = (w - totalGap) / kBarCount;
    if (barW < 1) { barW = 1; }

    int x = m_rcItem.left;
    for (int i = 0; i < kBarCount; ++i)
    {
        // 柱高映射到 cell 高
        int barH = (int)(h * kHourly[i]);
        if (barH < 2) { barH = 2; }    // 最低也给 2px，避免 0 高度看不见

        RECT r = {
            x,
            m_rcItem.bottom - barH,
            x + barW,
            m_rcItem.bottom
        };

        // 偶数柱用 primary-fixed-dim（浅红），奇数柱用 primary-accent
        // （鲜红）—— 节奏感
        COLORREF c = (i % 2 == 0)
            ? RGB(0xFF, 0xB3, 0xAD)        // primary-fixed-dim
            : kColorPrimaryAccent;

        PaintHelpers::FillRoundedRect(hdc, r, c, kBarTopRadius);

        x += barW + kBarGap;
    }
}

} // namespace cloudmelody
