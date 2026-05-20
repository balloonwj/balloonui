#pragma once

#include "DuiControl.h"
#include "DuiResMgr.h"

// =============================================================================
// DemoChatBubble — guides.html 自绘控件章节示例 #3
//
// 一句话：聊天气泡，左/右对齐 + 圆角矩形 + 小尾巴 + 自动换行 + 可选时间戳。
//
// 学习点（相对于示例 #1 / #2 增加的）：
//   1) "异形"自绘 — 用 GDI Polygon 画三角尾巴 + RoundRect 画气泡身。
//   2) 文字测高（MeasureHeight）— 给定宽度，算出气泡需要多高才能放下
//      WORDBREAK 后的所有文字。是聊天列表"按需排版"的关键 API。
//   3) 多种属性的相互依赖（side 切换会反转尾巴方向 + 色板）。
//   4) 仍然 NotifyParent(DUIN_CLICK) — 用户点气泡可触发"复制 / 引用"等
//      上下文菜单。
//
// 注意：本示例只画文字内容，不做内嵌图片 / 表情 / 引用块 — 那些是产线
// DuiChatBubble 应做的，超出"自绘 demo"范围。
// =============================================================================
class DemoChatBubble : public balloonwjui::DuiControl
{
public:
    enum Side
    {
        SideIn  = 0,   // 对方消息：左对齐，灰白色气泡，尾巴指左
        SideOut = 1    // 我方消息：右对齐，品牌绿气泡，尾巴指右
    };

    DemoChatBubble() = default;

    void SetSide(Side s);
    void SetText(LPCTSTR text);
    void SetTimestamp(LPCTSTR ts);   // "" 隐藏

    // 给定可用宽度，算出本气泡需要的总高度（含 padding + 尾巴 + 时间戳）。
    // 调用方在排聊天列表时用此函数算每条消息的高度。
    int  MeasureHeight(int availWidth) const;

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnLButtonUp(POINT pt, UINT mkFlags) override;

private:
    // 画气泡身 + 尾巴。返回气泡内容矩形（已扣 padding）。
    RECT PaintBubble(HDC hdc) const;

    // 内边距 / 尾巴几何 — 集中常量便于调整。
    static const int kPadX        = 12;   // 气泡内文字左右 padding
    static const int kPadY        = 8;    // 气泡内文字上下 padding
    static const int kRadius      = 8;    // 气泡圆角
    static const int kTailWidth   = 8;    // 尾巴宽（从气泡侧边伸出的距离）
    static const int kTailHeight  = 10;   // 尾巴高
    static const int kTailOffsetY = 14;   // 尾巴顶角与气泡顶部的距离
    static const int kMetaGap     = 2;    // 文字与时间戳之间的 gap
    static const int kMetaH       = 14;   // 时间戳行高

    Side    m_side  = SideIn;
    CString m_text;
    CString m_ts;
};
