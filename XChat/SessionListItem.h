#pragma once

// =============================================================================
// SessionListItem —— 会话行的纯绘制函数（Phase 3）。
//
// 没有控件类 —— 用 `DuiVirtualList` + paint 回调比"为每行做一个 DuiControl"
// 简单得多（数据驱动、无 child 树管理）。本文件只暴露一个绘制函数：
// `PaintSessionRow(hdc, rc, session, selected, hover)`。所有视觉规则
// （行高 / 头像位置 / 字号 / 颜色 / badge 形状）都在 .cpp 里 named const，
// 改尺寸 / 改色直接到那里。
//
// 标准行布局（68px 高）：
//
//   ┌──────────────────────────────────────────────────────────────────┐
//   │  [Av]  Name                                          time         │
//   │        preview text (single-line, ellipsis)         [bell|badge]  │
//   └──────────────────────────────────────────────────────────────────┘
//      ↑     ↑                                            ↑
//      14    14 + avatar(40) + 12 = 66                    rc.right - 14
//
// 头像：圆角 8px 矩形，背景色取 Session.avatarBg，白字打 Session.avatarLetter。
// 选中态：整行底色换 #EDEDED；hover：#F2F2F2；未选 & 未 hover：透明（露出
// 中栏 245,245,245 底色）。
// =============================================================================

#include <windows.h>

namespace xchat {

struct Session;     // forward — full def in MockData.h

// 把一行画到 hdc 上。rc 是该行在 host 客户区坐标系下的目标矩形。
//   selected：DuiVirtualList 报告的选中态。
//   hover   ：同上，鼠标悬停态。
void PaintSessionRow(HDC hdc, const RECT& rc, const Session& s,
                     bool selected, bool hover);

// 行高（px）—— DuiVirtualList::SetRowHeight 用得上。改这个值需要回头调
// .cpp 里的 vertical-center 算法。
int  GetSessionRowHeight();

} // namespace xchat
