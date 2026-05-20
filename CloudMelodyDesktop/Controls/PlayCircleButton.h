#pragma once

// =============================================================================
// PlayCircleButton —— PlayerBar 中央播放 / 暂停大圆按钮
//
// 视觉：直径 ~40px 红色圆 + 白色播放三角（▶）。播放态时切换成两根白色
//      圆角矩形（暂停 ⏸）。
//
// 实现：圆 / 三角属于"非轴对齐"形状，全部走 GDI+ 抗锯齿（balloonui
//      `DuiAA::FillEllipse` / `FillPolygon`）。两根圆角矩形是轴对齐的，
//      用普通 RoundRect 即可，不需要抗锯齿。
//
// 状态：
//   m_playing = false → 画 ▶
//   m_playing = true  → 画 ⏸
//   hover 时圆稍亮（hover 色 kColorPrimaryHover）
//
// 通知：DUIN_CLICK，点击时由父侧（PlayerBar / MainFrame）切换 m_playing
//      并 Invalidate。
// =============================================================================

#include "DuiControl.h"
#include "DuiNotify.h"

namespace cloudmelody {

class PlayCircleButton : public balloonwjui::DuiControl
{
public:
    PlayCircleButton() = default;

    void SetPlaying(bool b);
    bool IsPlaying() const { return m_playing; }

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnMouseEnter() override;
    bool OnMouseLeave() override;
    bool OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool OnSetCursor  (POINT pt) override;

private:
    bool m_playing = false;
    bool m_hover   = false;
    bool m_pressed = false;
};

} // namespace cloudmelody
