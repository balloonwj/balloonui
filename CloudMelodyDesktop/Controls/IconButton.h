#pragma once

// =============================================================================
// IconButton —— 32×32 透明底图标按钮
//
// 用途：TopBar 后退 / 前进 / 设置；PlayerBar 喜欢 / 队列 / 歌词 / 全屏；
//      MusicCard hover 浮 Play badge 的"非品牌色"伙伴等。所有"无背景、
//      只画 glyph"的小按钮都用这一个。
//
// 视觉 / 交互：
//   idle    ：透明背景 + glyph 灰 (kColorOnSurfaceVar)
//   hover   ：浅灰圆形 / 圆角底 (kColorSurfaceContainer) + glyph 略深
//   pressed ：稍深底 (kColorSurfaceContainerHigh)
//   click   ：NotifyParent(DUIN_CLICK, 0)
//   cursor  ：hand
//
// glyph 实现取巧：用 Unicode 字符（如 "‹" "›" "⚙" "♡" "▶"）走 DrawText。
// 不需要 PNG / GDI+ stroke 画线 —— 字体本身就能渲染这些符号。极简且
// 字号自适应，hover 状态切换成本几乎为 0。
//
// 缺点：细节 / 色调不能像 PNG 那么精致。M7 polish 阶段如需 fine-grained
// 控制再切到 GDI+ 自绘 + 抗锯齿。
//
// API：
//   SetGlyph(LPCTSTR) — 用作图标的 Unicode 字符（建议单字，2-3 字也行）
//   SetGlyphColor     — 默认 kColorOnSurfaceVar
//   SetHoverShape     — Square 圆角矩形（默认）/ Circle 圆形
// =============================================================================

#include "DuiControl.h"
#include "DuiNotify.h"

namespace cloudmelody {

class IconButton : public balloonwjui::DuiControl
{
public:
    enum HoverShape
    {
        ShapeSquare = 0,    // 圆角矩形（默认，4px 圆角）
        ShapeCircle = 1     // 完整圆形（PlayerBar 的喜欢 / 全屏等用）
    };

    IconButton() = default;
    ~IconButton() override;

    void SetGlyph(LPCTSTR sz);
    void SetGlyphColor(COLORREF c);
    void SetHoverShape(HoverShape s);

    // 注册 / 修改 tooltip 文字。空 / nullptr 不注册。控件 dtor 自动
    // Unregister，不必 caller 显式清。
    void SetTooltip(LPCTSTR text);

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnMouseEnter() override;
    bool OnMouseLeave() override;
    bool OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool OnSetCursor  (POINT pt) override;

private:
    CString    m_glyph;
    COLORREF   m_glyphColor = 0;          // 0 = 用 theme 默认色
    HoverShape m_shape      = ShapeSquare;
    bool       m_hover      = false;
    bool       m_pressed    = false;
};

} // namespace cloudmelody
