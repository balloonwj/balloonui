#pragma once

#include "DuiControl.h"
#include "DuiResMgr.h"

// =============================================================================
// DemoFileTypeIcon — guides.html 自绘控件章节示例 #4
//
// 一句话：文件后缀彩色徽章 — PDF / DOC / MP4 / ZIP 等不同后缀映射不同色板。
//
// 学习点（相对前 3 个示例增加的）：
//   1) 数据驱动配色 — extension 字符串 → COLORREF 查表，集中"色板/规则"。
//   2) 经典 fold-corner（折角）效果 — 用 GDI Polygon 在右上画三角折痕。
//   3) HitTest + 鼠标 hover 视觉反馈 — 控件自身可以察觉 hover 状态并
//      重绘（DuiControl 基类的 m_bHover 由 host 在 mouse-enter/leave 时
//      置位，OnPaint 直接读 IsHover() 即可）。
//   4) NotifyParent(DUIN_CLICK) 触发"打开文件"业务行为。
// =============================================================================
class DemoFileTypeIcon : public balloonwjui::DuiControl
{
public:
    DemoFileTypeIcon() = default;

    // 文件扩展名（不含点）。大小写不敏感；未知后缀回退默认色板。
    void SetExt(LPCTSTR ext);
    CString GetExt() const { return m_ext; }

    // 显示在徽章中央的文字。SetExt 时会自动同步为 ext.upper()，调用
    // SetLabel 可覆盖（如 "MP3" / "FIG" 等设计稿口径）。
    void SetLabel(LPCTSTR text);

    // 圆角半径（默认 6）。
    void SetCornerRadius(int r);

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnLButtonUp(POINT pt, UINT mkFlags) override;

    // 静态助手：暴露给 tests + 文档用。
    // ext 大小写不敏感；nullptr / "" 都返回 default。
    struct Palette { COLORREF bg; COLORREF fg; COLORREF fold; };
    static Palette LookupPalette(LPCTSTR ext);

private:
    CString m_ext;       // 原始 ext，小写
    CString m_label;     // 显示文字
    int     m_radius = 6;
};
