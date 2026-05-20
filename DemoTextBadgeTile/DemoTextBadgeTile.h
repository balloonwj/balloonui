#pragma once

// .cpp must include stdafx.h first.
#include "DuiControl.h"
#include "DuiResMgr.h"

// =============================================================================
// DemoTextBadgeTile — guides.html 自绘控件章节示例 #1
//
// 一句话：圆角彩色矩形 + 居中文字，纯绘制控件。最小自绘 DuiControl 子类示例。
//
// 学习点：
//   1) 继承 DuiControl，覆写 OnPaint(HDC, RECT) 完成自绘。
//   2) 用 GDI ::RoundRect 画背景，::DrawText 画居中文字 — 不需要 GDI+。
//   3) 用 DuiResMgr::Inst().GetDefaultFont() 取库默认 UI 字体（YaHei 9pt），
//      避免硬编码 LOGFONT，便于统一字号。
//   4) 任意属性 setter 内部调 Invalidate() 触发重绘 — DuiControl 帮你做。
//   5) 覆写 OnLButtonUp 通过 NotifyParent(DUIN_CLICK) 把 click 冒泡给父
//      HWND（详见 guides.html 第 6 章 "事件处理与路由"）。
//
// 故意不做的：动画、多状态视觉效果、HitTest 改写。等更复杂的示例再展示。
//
// 父对话框收事件：
//   if (n->ctrlId == kBadgeId && n->code == DUIN_CLICK) DoStuff();
// =============================================================================
class DemoTextBadgeTile : public balloonwjui::DuiControl
{
public:
    DemoTextBadgeTile() = default;

    // 文字内容（默认空串 → 仅画背景）
    void SetText(LPCTSTR sz);

    // 背景填充色（默认品牌蓝 #2D6CDF）
    void SetBgColor(COLORREF c);

    // 文字色（默认白色，搭配深色背景）
    void SetFgColor(COLORREF c);

    // 圆角半径，单位 px（默认 6）。0 = 直角矩形。
    void SetCornerRadius(int r);

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnLButtonUp(POINT pt, UINT mkFlags) override;

private:
    CString  m_text;
    COLORREF m_bgColor = RGB( 45, 108, 223);   // #2D6CDF — DuiTheme::brand
    COLORREF m_fgColor = RGB(255, 255, 255);
    int      m_radius  = 6;
};
