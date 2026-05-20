#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_SEPARATOR

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =============================================================================
// DuiSeparator —— 单线分隔条（无 HWND）
// =============================================================================
//
// 用途：在一行 toolbar 里把相关按钮分组隔开，或在 VBox / HBox 里把上下两段
// 不同业务的内容用一根线视觉分割。比"画一个 1px 高的彩色面板"更便宜，因为
// Separator 自动按 m_rcItem 居中、还能两端 inset 出短一截的精致效果。
//
// 工作机制：
//   · 纯 GDI FillRect —— 1px（可配粗细）的轴对齐矩形，沿垂直于线方向的
//     轴居中、沿线方向两端按 SetInset 内缩。
//   · Horizontal（默认）= 横向线，左→右；Vertical = 竖向线，上→下。
//   · 不接收键盘焦点（tab stop = false），鼠标透过（无 hit-test 处理）。
//   · 跟所有 DuiControl 一样，必须在 host 线程上访问。
//
// 代码用法（HBox 里隔开两组 toolbar 按钮）：
//
//     auto sep = std::unique_ptr<DuiSeparator>(new DuiSeparator());
//     sep->SetOrientation(DuiSeparator::Vertical);
//     sep->SetColor(RGB(220, 220, 224));
//     sep->SetInset(4);                       // 上下各内缩 4px
//     sep->SetFixedSize(1, 0);                // 1px 宽，行高填满
//     hbox->AddChild(std::move(sep));
//
// XML 用法（详细属性见 guides.html §3.3.5）：
//
//     <hbox gap="6" fixedHeight="32">
//       <button text="复制"/>
//       <button text="粘贴"/>
//       <separator orientation="vertical" color="220,220,224"
//                  thickness="1" inset="4" fixedWidth="1"/>
//       <button text="删除"/>
//     </hbox>
//
// 事件：无（纯绘制控件，不发 WM_DUI_NOTIFY）。
class BUI_API DuiSeparator : public DuiControl
{
public:
    // 线方向。Horizontal = 横线（左右走），Vertical = 竖线（上下走）。
    enum Orientation
    {
        Horizontal = 0,    // 默认
        Vertical   = 1,
    };

    DuiSeparator();

    // 设置 / 读取线方向。
    //   o：Horizontal 或 Vertical。
    void  SetOrientation(Orientation o);
    Orientation GetOrientation() const { return m_orient; }

    // 设置 / 读取线颜色。
    //   c：COLORREF（用 RGB(r,g,b) 构造）。默认 RGB(220,220,224)。
    void  SetColor(COLORREF c);
    COLORREF GetColor() const { return m_color; }

    // 设置 / 读取线粗（垂直于线方向的厚度，像素）。
    //   px：>= 1 的整数；< 1 会被 clamp 到 1。默认 1。
    void  SetThickness(int px);
    int   GetThickness() const { return m_thick; }

    // 设置 / 读取两端 inset —— 沿线方向，从 m_rcItem 两端各内缩这么多 px。
    // 例：水平线 inset=4 → 实际线左右各离 m_rcItem 边 4px。用于做"短一截"
    // 的精致分隔效果。默认 0。
    //   px：>= 0 的整数。
    void  SetInset(int px);
    int   GetInset() const { return m_inset; }

    // ---- DuiControl 覆写 ----
    // 在自身 m_rcItem 内画线 —— 由 host 在 OnPaint pass 自动调。
    void  OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    Orientation m_orient = Horizontal;
    COLORREF    m_color  = RGB(220, 220, 224);
    int         m_thick  = 1;
    int         m_inset  = 0;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SEPARATOR
