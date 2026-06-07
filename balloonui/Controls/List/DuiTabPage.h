#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_TABPAGE

// .cpp must include stdafx.h first.
#include "../../DuiControl.h"
#include "DuiTab.h"

namespace balloonwjui {

// =================================================================
// DuiTabPage —— Tab 头条 + 自动切页内容区（无 HWND）
// =================================================================
//
// 用途：DuiTab 只画 tab 头条不切页；DuiTabPage 把 tab 头条 + 多个内容
// page 整合到一个容器里，切 tab 自动 show/hide 对应 page。设置对话框、
// 个人资料卡的多分类视图都是典型场景。
//
// 工作机制：
//   · 内部持有一个私有 DuiTab 子类，其 selection-changed 事件被重
//     定向回本控件的 SetCurSel(idx, true)，再以本控件的 ctrlId 冒一次
//     DUIN_VALUECHANGED —— 外部业务看到的是一个"DuiTabPage 切页"事件，
//     而不是嵌套的"内部 DuiTab 切了"。
//   · pages 全是 m_children 的成员；每次切页给所有 page 重设 Visible
//     标志（仅当前 page 可见 + 接 hit-test）。
//   · 想直接调头条的样式 / 加 close / dropdown 标志通过 GetHeader() 拿
//     到内部 DuiTab*；<u>但 AddTab / RemoveTab 不要直接调它</u>，必须走
//     DuiTabPage 的同名方法，保证 page 列表同步。
//   · 整体不是 tab stop；头条内部的 DuiTab 是。
//
// 代码用法：
//
//     auto pages = std::make_unique<DuiTabPage>();
//     pages->AddPage(_T("通用"),   std::make_unique<MyGeneralPanel>());
//     pages->AddPage(_T("网络"),   std::make_unique<MyNetworkPanel>());
//     pages->AddPage(_T("关于"),   std::make_unique<DuiLabel>());
//     pages->SetCurSel(0, false);
//     pages->SetCtrlId(IDC_PREFS);
//     parent->AddChild(std::move(pages));
//     // 父 OnDuiNotify:
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_PREFS)
//     //       OnPrefsTabSwitched((int)n.extra);
//
// XML 用法：<u>暂未原生支持</u>。带 model 的复合控件 + 嵌套子树 page，
// XML 落地复杂度高（每 page 是任意子树）。业务侧 CustomFactory 注册
// <tab-page> + 多个 <tab-page-item title="..."> 子元素更合适（详见
// §3.6）。
//
// 事件：
//   · DUIN_VALUECHANGED — 切页；extra = 新 page index；ctrlId = DuiTabPage
//     的 id（HeaderTab 把内部 DuiTab 的切换事件重定向回 DuiTabPage，再由
//     DuiTabPage 用自己的 ctrlId 发出 DUIN_VALUECHANGED，业务收到的就是
//     "DuiTabPage 切页"事件，而不是"嵌套的内部 DuiTab 切了"。
//
// 关于"关闭 / 下拉" tab 头按钮：
//   DuiTabPage 当前的公开 API <u>不</u>支持声明 closeable / dropdown 标志的 page；
//   AddPage 没有 closeable / dropdown 参数，也<u>不</u>提供 SetPageCloseable /
//   SetPageDropdown。如果通过 GetHeader()->SetTabCloseable(idx, true) 绕路
//   打开 × 按钮，点击后内部 DuiTab 会以<u>内部 HeaderTab 的 ctrlId</u>
//   发出 DUITN_CLOSE（不是 DuiTabPage 的 ctrlId），且 DuiTabPage <u>不会</u>自动
//   RemovePage —— 业务自己负责接事件、调 RemovePage。要把这条用法变成一等公民，
//   需补 AddPage 的 closeable 参数、HeaderTab 拦截 DUITN_CLOSE 并以 DuiTabPage
//   的 ctrlId 重发，以及决定是否自动 RemovePage 的行为开关；当前未做。
class BUI_API DuiTabPage : public DuiControl
{
public:
    DuiTabPage();

    // Header strip height in px. Default 32. Will be clamped to >= 12.
    void    SetHeaderHeight(int px);
    int     GetHeaderHeight() const { return m_headerH; }

    // Add / remove pages. AddPage takes ownership of `page` (may be
    // nullptr — the tab still appears, with an empty content area, until
    // a SetPage replaces it). Returns the new page's index.
    //   icon: 可选 tab 头图标 (HBITMAP, caller-owned, 不 copy 不 DeleteObject);
    //         nullptr = 无图标。与 DuiTab::AddTab 的 icon 参数语义一致。
    int     AddPage(LPCTSTR title, std::unique_ptr<DuiControl> page, HBITMAP icon = nullptr);
    void    RemovePage(int index);
    int     GetPageCount() const { return (int)m_pages.size(); }

    // Replace an existing page's content (by index). Old page (if any)
    // is destroyed. Pass nullptr to clear and leave the tab empty.
    void    SetPage(int index, std::unique_ptr<DuiControl> page);
    DuiControl* GetPage(int index) const;

    // Tab title round-trip. Forwards to the inner DuiTab.
    void    SetPageTitle(int index, LPCTSTR title);
    CString GetPageTitle(int index) const;

    // Tab 头图标 round-trip。转给内部 DuiTab; 不影响 page slot 同步。
    //   index: page 序号 (即对应 tab 序号); 越界静默返回。
    //   hBmp:  HBITMAP, caller-owned, 不 copy 不 DeleteObject; nullptr 清除。
    void    SetPageIcon(int index, HBITMAP hBmp);
    HBITMAP GetPageIcon(int index) const;

    // 图标尺寸 / 间距 / 自适应宽度 —— 转给内部 DuiTab, 影响所有 tab。
    // 默认值与 DuiTab 一致 (iconSize=16 / iconGap=6 / autoFit=false)。
    void    SetIconSize(int px);
    int     GetIconSize() const;
    void    SetIconGap(int px);
    int     GetIconGap() const;
    void    SetAutoFitTabWidth(bool b);
    bool    GetAutoFitTabWidth() const;

    // Selection.
    int     GetCurSel() const { return m_curSel; }
    void    SetCurSel(int index, bool notify = true);

    // Direct access to the underlying DuiTab if callers need to set
    // colors / closeable / dropdown / register tooltips on individual
    // tabs. Don't AddTab/RemoveTab on it directly — go through DuiTabPage
    // so the page list stays in sync.
    DuiTab* GetHeader() { return m_header; }

    // ---- DuiControl overrides ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    DuiControl* HitTest(POINT ptHostClient) override;

private:
    // Set every page's visibility based on m_curSel.
    void    ApplyVisibility();

    // Layout the visible page into the content rect.
    void    LayoutContent();

private:
    DuiTab*  m_header  = nullptr;        // raw; ownership in m_children
    int      m_curSel  = -1;
    int      m_headerH = 32;

    // Page slots. Each is owned by m_children too (via AddChild) so that
    // host-attach + parent-link work; raw ptrs let us track which child
    // is which page slot independent of vector order.
    std::vector<DuiControl*> m_pages;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_TABPAGE
