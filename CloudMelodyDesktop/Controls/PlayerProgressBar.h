#pragma once

// =============================================================================
// PlayerProgressBar —— 底部播放栏专用进度条
//
// 视觉：极细 track（idle 2px / hover 4px），左侧 elapsed 红色、右侧
//      remaining 浅灰；hover 时 track 加粗、显示拖拽指针。
//
// 与 balloonui DuiSlider 的关系：未继承 —— DuiSlider 默认有 thumb（圆
// 形滑块），关掉 thumb 后还要重写 track 渲染；不如直接派生 DuiControl。
//
// API：
//   SetRange(int min, int max)   — 默认 0..1000（千分进度）
//   SetValue(int v, bool notify) — 设当前值；超界自动 clamp
//   GetValue / GetMin / GetMax
//
// 通知：拖动 / 点击改值时 NotifyParent(DUIN_VALUECHANGED, value)。
// =============================================================================

#include "DuiControl.h"
#include "DuiNotify.h"

namespace cloudmelody {

class PlayerProgressBar : public balloonwjui::DuiControl
{
public:
    PlayerProgressBar() = default;

    void SetRange(int minV, int maxV);
    int  GetMin() const { return m_min; }
    int  GetMax() const { return m_max; }

    void SetValue(int v, bool notify = false);
    int  GetValue() const { return m_value; }

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnMouseEnter() override;
    bool OnMouseLeave() override;
    bool OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool OnMouseMove  (POINT pt, UINT mkFlags) override;
    bool OnSetCursor  (POINT pt) override;

private:
    int ValueFromX(int x) const;

    int  m_min     = 0;
    int  m_max     = 1000;
    int  m_value   = 0;
    bool m_hover   = false;
    bool m_dragging = false;
};

} // namespace cloudmelody
