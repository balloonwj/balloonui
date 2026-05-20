#pragma once

#include "DuiControl.h"

// =============================================================================
// BgPaintTile — DemoNinePatchBg 内部 helper（不进库）。
//
// 用途：在自身 m_rcItem 范围内画一张位图。两种模式：
//   * NinePatch ：用 DuiNinePatch::Draw 切 9 块（4 角不缩、4 边一轴拉伸、
//                 中央双轴拉伸） — 与 DuiHost::SetBgImage 内部相同
//   * NaiveStretch：直接 StretchBlt 整图到目标矩形（破坏角落装饰）—
//                   仅作"反例"用，给文档对比
//
// 为什么 demo 内部要用 tile 而不是直接调 DuiHost::SetBgImage？
//   单个 host 同一时刻只能有一个客户区大小，没法在一张截图里同时呈现
//   "小/中/大 三种尺寸"。所以在 capture 时把 4 个不同尺寸的 tile 放在
//   一个画面里。Tile 内部走与 DuiHost::SetBgImage 完全同一个 helper
//   (DuiNinePatch::Draw)，视觉等价。
//
// 交互模式（用户直接运行 demo.exe，没带 --capture-all）走的是
// DuiHost::SetBgImage —— 真正展示 API；capture 模式才走 tile。
// =============================================================================
class BgPaintTile : public balloonwjui::DuiControl
{
public:
    enum Mode
    {
        ModeNinePatch    = 0,
        ModeNaiveStretch = 1,
    };

    BgPaintTile() = default;

    void SetBitmap(HBITMAP hbm);
    void SetInsets(int left, int top, int right, int bottom);   // src == dst
    void SetInsets(int srcL, int srcT, int srcR, int srcB,
                   int dstL, int dstT, int dstR, int dstB);     // 双 inset
    void SetMode(Mode m);

    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    HBITMAP m_hbm     = nullptr;
    int     m_srcL    = 0;
    int     m_srcT    = 0;
    int     m_srcR    = 0;
    int     m_srcB    = 0;
    int     m_dstL    = 0;
    int     m_dstT    = 0;
    int     m_dstR    = 0;
    int     m_dstB    = 0;
    Mode    m_mode    = ModeNinePatch;
};
