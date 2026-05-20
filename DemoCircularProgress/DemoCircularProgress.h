#pragma once

#include "DuiControl.h"
#include "DuiResMgr.h"

// =============================================================================
// DemoCircularProgress — guides.html 自绘控件章节示例 #2
//
// 一句话：圆形进度环，进度 0..100%。
//
// 学习点（相对于示例 #1 DemoTextBadgeTile 增加的）：
//   1) 用 GDI+（Gdiplus::Graphics + Pen + SmoothingMode::AntiAlias）画
//      抗锯齿圆环 / 弧 — 因为 DuiPaintAA 没有 Arc 帮助函数，自绘控件
//      可以直接用 GDI+。GDI+ 在第一次调用时已被 balloonui 内部初始化。
//   2) 演示"setter 限制 + clamp + 仅在值变化时 Invalidate"模式（避免无谓重绘）。
//   3) 仍然走 DuiResMgr 取默认字体画百分比文字。
//   4) 不带交互（无 hit-test 重写、无事件冒泡）— 纯展示控件，多 demo 才能
//      看清"该写哪里、不该写哪里"。
// =============================================================================
class DemoCircularProgress : public balloonwjui::DuiControl
{
public:
    DemoCircularProgress() = default;

    // 进度值。SetPercent 自动 clamp 到 [0, 100]，无变化时不重绘。
    void SetPercent(int p);
    int  GetPercent() const { return m_percent; }

    // 环厚度（像素），默认 6。0 = 退化为实心圆盘 — 不推荐。
    void SetThickness(int t);

    // 背景轨道色（环未填充部分），默认浅灰。
    void SetTrackColor(COLORREF c);

    // 前景填充色（已完成部分），默认品牌蓝。
    void SetFillColor(COLORREF c);

    // 中心文字色，默认深墨色。
    void SetTextColor(COLORREF c);

    // 是否在中心显示 "<n>%" 文字，默认 true。
    void SetShowText(bool b);

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    int      m_percent    = 0;
    int      m_thickness  = 6;
    COLORREF m_trackColor = RGB(230, 232, 236);   // 浅灰
    COLORREF m_fillColor  = RGB( 45, 108, 223);   // 品牌蓝
    COLORREF m_textColor  = RGB( 30,  30,  30);
    bool     m_showText   = true;
};
