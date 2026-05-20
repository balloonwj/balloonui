#pragma once

// =============================================================================
// HoursBarChart —— ProfilePage 的「听歌时段」柱图
//
// 视觉：N 列等宽柱状图，柱顶高度按归一化的"听歌强度"映射；颜色用主题
//      浅红 / 深红交替，让相邻柱有节奏。柱底基线固定在 cell 底部，柱
//      高 = cell 高 × normalizedValue。
//
// 数据：写死 24h 的"小时段强度"数组（M4 第一版 simulate；M7 polish 接
//      真历史）。柱列数也写死，避免 API 表面变得复杂。
// =============================================================================

#include "DuiControl.h"

namespace cloudmelody {

class HoursBarChart : public balloonwjui::DuiControl
{
public:
    HoursBarChart() = default;

    void OnPaint(HDC hdc, const RECT& rcDirty) override;
};

} // namespace cloudmelody
