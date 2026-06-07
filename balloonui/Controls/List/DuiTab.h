#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_TAB

// .cpp must include stdafx.h first.
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiTab —— 自绘 tab 标签条（无 HWND）
// =================================================================
//
// 用途：聊天窗口顶部的多会话切换条、设置对话框的"通用 / 网络 / 隐私"
// 分页栏、消息历史的"全部 / 未读"过滤栏。<u>只画 tab 头条</u>，不管
// 内容区切换 —— 想要"tab + 自动切页"复合控件用 DuiTabPage。
//
// 工作机制：
//   · 每个 tab 数据：文字 + 可选 Closeable / Dropdown 标志 + 任意 LPARAM
//     payload（原 CChatTabMgr 里挂 HWND，业务可挂 userid 等）。
//   · 同一时刻只有一个"当前 tab"；SetCurSel 触发 DUIN_VALUECHANGED；
//     用户点 tab 自动选中。
//   · Closeable tab 上点 × 关闭按钮 → DUITN_CLOSE（extra = idx）；
//     <u>不</u>改 selection，由父决定要不要 RemoveTab。
//   · Dropdown tab 上点 ▾ → DUITN_DROPDOWN（extra = idx）；父典型行为
//     是在 tab 旁弹菜单。
//   · 可选拖拽重排（SetReorderEnabled 开）—— 按下 + 移动 4px 后转入
//     拖动，松键时 MoveTab 落地并发 DUITN_REORDERED。
//   · 水平溢出滚动 —— tab 总宽 > strip 时两端各出 18px 箭头按钮，
//     点击按 SetScrollStepPx 滚（默认 60px）。EnsureVisible 让指定
//     tab 滚到可见。
//   · 整条 tab strip 是<u>一个</u> tab stop（接键盘焦点）。
//
// 代码用法：
//
//     auto tabs = std::make_unique<DuiTab>();
//     int t0 = tabs->AddTab(_T("通用"));
//     int t1 = tabs->AddTab(_T("聊天"), /*closeable=*/false, /*dropdown=*/true);
//     int t2 = tabs->AddTab(_T("会话"), /*closeable=*/true);
//     tabs->SetCurSel(0, false);
//     tabs->SetCtrlId(IDC_PREF_TABS);
//     parent->AddChild(std::move(tabs));
//     // 父 OnDuiNotify:
//     //   if (n.ctrlId == IDC_PREF_TABS) {
//     //       if (n.code == DUIN_VALUECHANGED) ShowPage((int)n.extra);
//     //       else if (n.code == DuiTab::DUITN_CLOSE) RemoveSession((int)n.extra);
//     //   }
//
// XML 用法：<u>暂未原生支持</u>。tab 是有 model 的控件（tab 列表 + 每项
// 标志），且常在运行时随会话动态增删。要 XML 化业务自己写
// CustomFactory（详见 §3.6）。
//
// 事件（ctrlId = tab id）：
//   · DUIN_VALUECHANGED  — 切换 tab；extra = 新 tab index。
//   · DUITN_CLOSE        — × 按钮被点；extra = 该 tab index。<u>不</u>自动 RemoveTab。
//   · DUITN_DROPDOWN     — ▾ 按钮被点；extra = 该 tab index。
//   · DUITN_REORDERED    — 拖拽重排完成；extra = 新位置。
//
// 替代关系：CSkinTabCtrl（带可选 dropdown）+ CChatTabMgr（聊天窗 tab
// 含 close 按钮）。DuiTab 把这两个统一到一个控件里。
class BUI_API DuiTab : public DuiControl
{
public:
    enum NotifyCode
    {
        DUITN_CLOSE     = DUIN_CUSTOM + 1,
        DUITN_DROPDOWN  = DUIN_CUSTOM + 2,
        DUITN_REORDERED = DUIN_CUSTOM + 3,
    };

    DuiTab();

    // ---- tab data ----
    // icon 参数: HBITMAP, caller-owned, 控件不 copy 不 DeleteObject; 传
    // nullptr 表示无图标。与 DuiButton::SetLeadingIcon 一致。
    int     AddTab(LPCTSTR text, bool closeable = false, bool dropdown = false,
                   LPARAM lParam = 0, HBITMAP icon = nullptr);
    void    InsertTab(int index, LPCTSTR text, bool closeable = false, bool dropdown = false,
                      LPARAM lParam = 0, HBITMAP icon = nullptr);
    void    RemoveTab(int index);
    void    RemoveAllTabs();
    int     GetTabCount() const { return (int)m_tabs.size(); }

    CString GetTabText(int index) const;
    void    SetTabText(int index, LPCTSTR text);
    LPARAM  GetTabParam(int index) const;
    void    SetTabParam(int index, LPARAM lParam);
    bool    IsTabCloseable(int index) const;
    void    SetTabCloseable(int index, bool b);
    bool    IsTabDropdown(int index) const;
    void    SetTabDropdown(int index, bool b);

    // 单个 tab 的图标 (HBITMAP, caller-owned, 不 copy 不 DeleteObject)。
    //   index: tab 序号; 越界静默返回。
    //   hBmp:  32bpp 预乘 alpha 位图(同 DuiButton::SetLeadingIcon); nullptr 清除。
    void    SetTabIcon(int index, HBITMAP hBmp);
    // 取指定 tab 的图标 (caller-owned 借用)。越界返回 nullptr。
    HBITMAP GetTabIcon(int index) const;

    // ---- selection ----
    int     GetCurSel() const { return m_curSel; }
    void    SetCurSel(int index, bool notify = true);
    int     FindByParam(LPARAM lParam) const;

    // ---- appearance ----
    void    SetTabHeight(int h);
    int     GetTabHeight() const { return m_tabH; }
    void    SetMinTabWidth(int w);
    void    SetMaxTabWidth(int w);
    void    SetTabPadding(int leftRight) { m_tabPad = leftRight; Invalidate(); }
    void    SetGap(int g) { m_gap = g; Invalidate(); }

    // 图标尺寸 (像素, 默认 16); 所有 tab 共用一个尺寸, 渲染时按此尺寸
    // ::AlphaBlend 缩放。<1 自动钳到 1, 防御性。
    void    SetIconSize(int px);
    int     GetIconSize() const { return m_iconSize; }
    // 图标与文字之间的间距 (像素, 默认 6); <0 自动钳到 0。
    void    SetIconGap(int px);
    int     GetIconGap() const { return m_iconGap; }

    // 宽度自适应文字 (默认 false = 沿用 [m_minTabW, m_maxTabW] clamp)。
    // 打开后 MeasureTabs 仍计算 text + padding + icon + close + dropdown 的
    // 真实宽, 但<u>跳过</u> min / max clamp; 极短文字 tab 可窄于 minW,
    // 极长文字 tab 可宽于 maxW。
    void    SetAutoFitTabWidth(bool b);
    bool    GetAutoFitTabWidth() const { return m_autoFit; }

    void    SetBgColor      (COLORREF c) { m_clrBg       = c; Invalidate(); }
    void    SetTabColor     (COLORREF c) { m_clrTab      = c; Invalidate(); }
    void    SetTabHoverColor(COLORREF c) { m_clrTabHover = c; Invalidate(); }
    void    SetTabSelColor  (COLORREF c) { m_clrTabSel   = c; Invalidate(); }
    void    SetTextColor    (COLORREF c) { m_clrText     = c; Invalidate(); }
    void    SetSelTextColor (COLORREF c) { m_clrTextSel  = c; Invalidate(); }
    void    SetBorderColor  (COLORREF c) { m_clrBorder   = c; Invalidate(); }

    // ---- DuiControl overrides ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool    OnMouseMove  (POINT pt, UINT mkFlags) override;
    bool    OnMouseLeave () override;

    // ---- drag-to-reorder ----
    //
    // When enabled, a press-and-drag on a tab moves it left / right
    // along the strip; release commits the new order via MoveTab().
    // While dragging, the tab strip renders a thin brand-blue vertical
    // bar at the candidate insertion slot.
    void   SetReorderEnabled(bool b) { m_reorderEnabled = b; }
    bool   GetReorderEnabled() const { return m_reorderEnabled; }

    // Programmatic move + DUITN_REORDERED notify.
    // `to` is the *insertion slot* (0..GetTabCount()) — to == from or
    // to == from + 1 are no-ops. Selection follows the moved tab.
    void   MoveTab(int from, int to);

    // ---- horizontal scroll for overflow strips ----
    //
    // When the total tab content is wider than the strip's m_rcItem,
    // a 16px arrow is reserved at each end of the strip. The arrows
    // stand in for the off-screen tabs; clicking them scrolls the
    // strip by ScrollStepPx px (default 60). When all tabs fit, the
    // arrows are hidden + scroll resets to zero.
    void   SetScrollStepPx(int px);
    int    GetScrollStepPx() const { return m_scrollStepPx; }
    int    GetScrollOffset() const { return m_scrollOffset; }
    void   SetScrollOffset(int px);   // clamps to [0, max]

    // True when there's content beyond the visible band on the
    // corresponding side. Tests use these.
    bool   IsScrollableLeft () const;
    bool   IsScrollableRight() const;
    bool   NeedsScroll() const;

    // Maximum scroll offset (totalContent - visible). 0 when all tabs
    // fit. Pure helper for tests.
    int    GetMaxScrollOffset() const;

    // Scroll so the tab at idx is fully visible.
    void   EnsureVisible(int idx);

    // For tests.
    RECT    Test_TabRect(int index) const   { return TabRect(index); }
    RECT    Test_CloseRect(int index) const { return CloseRect(index); }
    RECT    Test_DropRect(int index) const  { return DropRect(index); }
    int     Test_HitTab(POINT pt) const     { return HitTabIndex(pt); }
    int     Test_TotalContentWidth() const  { return TotalContentWidth(); }
    RECT    Test_LeftArrowRect()  const     { return LeftArrowRect();  }
    RECT    Test_RightArrowRect() const     { return RightArrowRect(); }

protected:
    // Hook fired by SetCurSel (when notify=true). Default implementation
    // NotifyParents DUIN_VALUECHANGED, which is what most callers want.
    // Composite controls (DuiTabPage) override this to redirect the
    // notification through themselves instead of going straight to the
    // host's parent HWND.
    virtual void FireSelectionChanged(int newIdx);

private:
    struct Tab
    {
        CString text;
        bool    closeable;
        bool    dropdown;
        LPARAM  lParam;
        int     widthPx;       // computed in MeasureTabs(), incl. padding/glyphs
        HBITMAP icon;          // caller-owned 借用; nullptr = 无图标
    };
    enum HitZone { ZoneNone, ZoneTab, ZoneClose, ZoneDrop };

    void    MeasureTabs() const;        // recomputes widthPx for each tab
    RECT    TabRect(int index) const;
    RECT    CloseRect(int index) const; // empty if not closeable
    RECT    DropRect (int index) const; // empty if not dropdown
    int     HitTabIndex(POINT pt) const;
    int     TotalContentWidth() const;  // sum of widthPx + gaps
    RECT    LeftArrowRect () const;     // empty when no scroll needed
    RECT    RightArrowRect() const;
    int     ContentLeftEdge () const;   // left bound of tab area (after left arrow)
    int     ContentRightEdge() const;   // right bound (before right arrow)
    HitZone HitZoneAt(POINT pt, int& outIndex) const;
    int     TextWidthOf(LPCTSTR text) const;
    void    DrawTab(HDC hdc, int index) const;
    void    DrawCloseGlyph(HDC hdc, const RECT& rc, bool hover) const;
    void    DrawDropGlyph (HDC hdc, const RECT& rc, bool hover) const;

private:
    mutable std::vector<Tab>  m_tabs;
    int     m_curSel       = -1;
    int     m_hoverIdx     = -1;
    HitZone m_hoverZone    = ZoneNone;
    int     m_pressIdx     = -1;
    HitZone m_pressZone    = ZoneNone;

    int     m_tabH         = 28;
    int     m_tabPad       = 12;     // left/right padding inside tab
    int     m_gap          = 2;      // pixels between tabs
    int     m_minTabW      = 60;
    int     m_maxTabW      = 200;
    int     m_closeSize    = 14;     // close button glyph rect
    int     m_dropSize     = 12;     // dropdown arrow rect
    int     m_iconSize     = 16;     // tab 左侧图标尺寸(像素), 所有 tab 共用
    int     m_iconGap      = 6;      // 图标与文字之间的间距(像素)
    bool    m_autoFit      = false;  // true = MeasureTabs 跳过 [minTabW,maxTabW] clamp
    int     m_scrollOffset = 0;      // px shifted to the left
    int     m_scrollStepPx = 60;     // per-arrow-click step
    static const int kArrowW = 18;   // width of left / right arrow strip
    static const int kDragThresholdPx = 4;

    bool    m_reorderEnabled = false;
    int     m_dragSrcIdx     = -1;      // source tab while dragging
    int     m_dragDropSlot   = -1;      // candidate insertion slot during drag
    int     m_pressX         = 0;       // x at LButtonDown (for threshold)
    bool    m_dragging       = false;   // crossed threshold since press

    // 默认配色对齐 Win10 任务管理器 / Explorer 风：tab 头条底色与 host
    // 客户区接近，普通 tab 几乎透明（弱对比），hover 略深一格，选中
    // 白底显著突出。业务侧若需要其它主题（深色 / 品牌色）走 SetTabColor
    // 系列 setter 覆盖。
    COLORREF m_clrBg       = RGB(238, 240, 244);   // tab 头条整体底
    COLORREF m_clrTab      = RGB(238, 240, 244);   // 普通 tab：与底相同（透明感）
    COLORREF m_clrTabHover = RGB(228, 232, 240);   // hover：略深一格
    COLORREF m_clrTabSel   = RGB(255, 255, 255);   // 选中：白底突出
    COLORREF m_clrText     = RGB( 50,  50,  60);
    COLORREF m_clrTextSel  = RGB( 20,  30,  90);
    COLORREF m_clrBorder   = RGB(212, 215, 222);
};

} // namespace balloonwjui

#endif // BUI_FEATURE_TAB
