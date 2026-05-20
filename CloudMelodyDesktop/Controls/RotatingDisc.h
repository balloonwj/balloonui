#pragma once

// =============================================================================
// RotatingDisc —— NowPlayingPage 旋转黑胶
//
// 视觉：黑色圆盘（外圆环 + 浅灰内圆 + 中央实心黑圆点）+ 内嵌方形封面图。
//      封面图与圆盘<u>同步旋转</u>（实际唱机上印刷面跟着转，不是固定贴
//      纸效果）。
//
// 旋转：33⅓ RPM 真实唱机转速 ≈ 200°/s。host 通过 Tick(deltaMs) 推进
//      m_angle，OnPaint 用 GDI+ RotateTransform 应用到中心。
//
// 实现：纯自绘（DuiControl 派生）。圆形 / 圆环用 GDI+ FillEllipse + 抗锯
//      齿；封面占位用深灰圆角方块（M6 不接 PNG）。
// =============================================================================

#include "DuiControl.h"

namespace cloudmelody {

class RotatingDisc : public balloonwjui::DuiControl
{
public:
    RotatingDisc() = default;

    // host 周期调用（由 60Hz timer 驱动）。仅在 IsSpinning() 时累加角度。
    void Tick(int deltaMs);

    void SetSpinning(bool b) { m_spinning = b; }
    bool IsSpinning() const  { return m_spinning; }

    // 黑胶中央封面方块的 hue 种子（决定渐变色），曲目切换时调。
    void SetCoverHueSeed(int s) { m_coverSeed = s; Invalidate(); }

    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    float m_angle      = 0.0f;     // 度数，累加到 360 自动 wrap
    bool  m_spinning   = true;
    int   m_coverSeed  = 0;        // CoverArt 用
};

} // namespace cloudmelody
