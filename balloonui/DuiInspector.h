#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiInspector —— 调试覆盖层（控件 bounding box + 标签）
// =================================================================
//
// 用途：开发新控件时把整棵 DUI 树的每个 visible 控件用红框圈出来，
// 鼠标 hover 的控件用绿框 + 黑底标签（class 名 + ctrlId + 矩形）。
// <u>不</u>对终端用户开放 —— 只在 debug 构建 / DuiGallery 菜单里手动
// Enable(true) 启用。
//
// 工作机制：
//   · 单例 + 全局开关。开启后 DuiHost::OnPaint 在 root 画完之后、
//     BitBlt 到屏前调 PaintOverlay(host, hdc) 叠一层。控件代码<u>无</u>
//     需任何配合。Enable(true/false) 实时切换。
//   · 调试着色：每个 visible 控件 2px 红框；hover 控件 2px 绿框；
//     hover 控件左上角小黑标签条显示 class + id + rect。
//
// 代码用法：
//
//     // 调试菜单 / Ctrl+Shift+I 快捷键里：
//     balloonwjui::DuiInspector::Inst().Enable(true);
//
//     // DuiHost::OnPaint 里（root 已画进背缓冲后）：
//     balloonwjui::DuiInspector::Inst().PaintOverlay(this, hMemDC);
//
// XML 用法：N/A（调试 overlay）。

#include <windows.h>
#include "DuiControl.h"

namespace balloonwjui {

class DuiHost;

// Usage:
//   // From a debug menu / Ctrl+Shift+I handler:
//   balloonwjui::DuiInspector::Inst().Enable(true);
//
//   // From DuiHost::OnPaint, after root paints into the back buffer:
//   balloonwjui::DuiInspector::Inst().PaintOverlay(this, hMemDC);
class DuiInspector
{
public:
    static DuiInspector& Inst();

    void  Enable(bool b);
    bool  IsEnabled() const { return m_enabled; }

    // Pure paint pass. Walks the host's tree, outlines every control,
    // and overlays a label for the host's current hover (if any).
    // Cheap GDI; no allocations beyond a temp HFONT.
    void  PaintOverlay(DuiHost* host, HDC hdc) const;

    // Format a single line describing a control: "DuiButton id=42 rect=(0,0,80,24)".
    // Pure helper, side-effect-free, used by tests.
    static CString FormatControlInfo(const DuiControl* c);

    // Walk the tree rooted at `root` collecting every visible control's
    // rect. Pure / read-only / testable. Stops descending into invisible
    // subtrees so collapsed sections don't leak boxes.
    static void   CollectVisibleRects(const DuiControl* root,
                                      std::vector<RECT>& out);

private:
    DuiInspector() = default;
    ~DuiInspector() = default;
    DuiInspector(const DuiInspector&) = delete;
    DuiInspector& operator=(const DuiInspector&) = delete;

    bool m_enabled = false;
};

} // namespace balloonwjui
