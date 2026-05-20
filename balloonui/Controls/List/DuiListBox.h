#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_LISTBOX

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"
#include "../Window/DuiScrollBar.h"

namespace balloonwjui {

// =================================================================
// DuiListBox —— 单列字符串列表（无 HWND，含滚动条）
// =================================================================
//
// 用途：联系人列表、会话列表、emoji 选择面板的"列"轨等所有"一行一项、
// 用户在里面选一项 / 多项"的场景。内置一个 DuiScrollBar 处理溢出。
//
// 选择模型：
//   · 默认单选：m_curSel = -1 表示未选。
//   · SetMultiSelect(true)：每项独立 selected 标志。
//     - 普通点击 → 单选当前行（清掉其它）；
//     - Ctrl-点击 → 切换该行；
//     - Shift-点击 → 从 m_curSel（"锚"）扩展到点击行；
//     - SetItemSelected(idx, bool) 程序设置时跟锚无关。
//
// 可选特性：
//   · SetShowCheckboxes(true)：每行左侧留 22px checkbox 列。点击勾框
//     toggle，独立于 selection。
//   · SetDragReorderEnabled(true)：拖一行上下移动重排底层 vector，
//     落点处显示横线提示。
//
// 工作机制：
//   · 持有自己的 m_items vector（CString text + LPARAM + 选中 / 勾选
//     标志）。
//   · 嵌入 DuiScrollBar 处理纵向溢出；滚轮按行滚；键盘 Up/Down/PgUp/
//     PgDn 移动 m_curSel 并自动滚到可见。
//   · 多选 / checkbox / 拖拽重排都是 opt-in；可以同时开启。
//   · 所有通知靠 WM_DUI_NOTIFY 同步上冒，extra 是受影响的行索引。
//
// 代码用法（单选好友列表）：
//
//     auto lb = std::unique_ptr<DuiListBox>(new DuiListBox());
//     lb->SetCtrlId(IDC_FRIENDS);
//     lb->SetItemHeight(28);
//     for (auto& f : friends)
//     {
//         lb->AddItem(f.nickname, (LPARAM)f.userid);
//     }
//     parent->AddChild(std::move(lb));
//     // 父 OnDuiNotify:
//     //   if (code == DUIN_VALUECHANGED && id == IDC_FRIENDS)
//     //       OnFriendSelected((int)extra);
//
// XML 用法：<u>暂未原生支持</u>。原因：列表是有 model 的控件（item
// 列表 + 每项 text + LPARAM），实际场景多半运行时填，写死 XML 价值有限。
// 业务需要 XML 化的话写 CustomFactory 注册 <listbox> + 嵌套 <item>
// 子节点（详见 guides.html §3.6）。
//
// 事件（ctrlId = listbox id）：
//   · DUIN_VALUECHANGED  — 选中行变化；extra = 新 index 或 -1。
//   · DUIN_DBLCLK        — 双击某行；extra = 行 index。
//   · DUITN_CHECKED      — checkbox 切换；extra = 行 index。
//   · DUITN_REORDERED    — 拖拽重排完成；extra = 新位置。
//
// 替代关系：CSkinListBox（老 UI 里的好友 / emoji / 最近会话面板都用过）。
class BUI_API DuiListBox : public DuiControl
{
public:
    enum NotifyCode
    {
        DUITN_CHECKED   = DUIN_CUSTOM + 1,
        DUITN_REORDERED = DUIN_CUSTOM + 2,
    };

    DuiListBox();

    // ---- 数据 model ----

    // 末尾追加一项；返回新项 index。
    //   text：显示文本。
    //   lParam：附带任意 LPARAM 数据（业务自定义，如 userid 指针）。
    int     AddItem(LPCTSTR text, LPARAM lParam = 0);

    // 在 index 处插入一项（>= count 时等价 AddItem）。
    void    InsertItem(int index, LPCTSTR text, LPARAM lParam = 0);

    // 删除指定项；m_curSel / hover / drag-source 按需调整。
    void    DeleteItem(int index);

    // 清空全部项。
    void    DeleteAllItems();

    int     GetItemCount() const { return (int)m_items.size(); }

    CString GetItemText (int index) const;
    LPARAM  GetItemParam(int index) const;
    void    SetItemText (int index, LPCTSTR text);
    void    SetItemParam(int index, LPARAM lParam);

    // ---- 选中状态（锚 + flag）----

    // 当前"主"行 —— 锚（shift-extend 起点 + Up/Down 跟踪行）。
    // 单选模式下也是唯一选中行。
    int     GetCurSel() const { return m_curSel; }

    // 设置 m_curSel（锚）。单选模式下也清掉其它项的 selected。
    //   index：[0, count) 或 -1 取消。
    //   notify：true 时触发 DUIN_VALUECHANGED。
    void    SetCurSel(int index, bool notify = true);

    // 切换多选模式。注意：从多选切回单选时，仅保留 m_curSel 那一行选中。
    void    SetMultiSelect(bool b);
    bool    IsMultiSelect() const { return m_multiSelect; }

    // 第 idx 项是否选中（多选模式有意义；单选模式 = (idx == m_curSel)）。
    bool    IsItemSelected(int idx) const;

    // 程序切换某项的选中状态（独立于锚）。仅多选模式调有意义。
    //   idx：行索引。
    //   sel：true 选中 / false 取消。
    //   notify：true 时发 DUIN_VALUECHANGED（extra = idx）。
    void    SetItemSelected(int idx, bool sel, bool notify = true);

    int     GetSelectionCount() const;
    void    GetSelectedIndices(std::vector<int>& out) const;
    void    ClearSelection();

    // ---- 每行 checkbox 列 ----

    // 启用 / 关闭每行左侧 22px 勾选框列。
    void    SetShowCheckboxes(bool b);
    bool    GetShowCheckboxes() const { return m_showCheckboxes; }

    // 第 idx 项的勾选状态。
    bool    IsItemChecked(int idx) const;

    // 程序切换勾选；notify=true 时发 DUITN_CHECKED（extra = idx）。
    void    SetItemChecked(int idx, bool checked, bool notify = true);

    // ---- 拖拽重排 ----

    // 启用 / 关闭拖拽重排。开启后用户可拖一行上下移动。
    void    SetDragReorderEnabled(bool b) { m_dragReorder = b; }
    bool    GetDragReorderEnabled() const { return m_dragReorder; }

    // 程序移动一行。成功时发 DUITN_REORDERED（extra = 新位置）。
    void    MoveItem(int from, int to);

    // ---- 外观 ----

    // 单行高度（px）。
    void    SetItemHeight(int h);
    int     GetItemHeight() const { return m_itemH; }

    void    SetTextColor    (COLORREF c) { m_clrText      = c; Invalidate(); }
    void    SetBgColor      (COLORREF c) { m_clrBg        = c; Invalidate(); }
    void    SetSelTextColor (COLORREF c) { m_clrSelText   = c; Invalidate(); }
    void    SetSelBgColor   (COLORREF c) { m_clrSelBg     = c; Invalidate(); }
    void    SetHoverBgColor (COLORREF c) { m_clrHoverBg   = c; Invalidate(); }

    // ---- 滚动 ----

    int     GetScrollPos() const;
    void    SetScrollPos(int p);

    // 滚动让 index 行可见（已可见时不动）。
    void    EnsureVisible(int index);

    // 拿内嵌滚动条指针（用于程序化操作 / 测试）。
    DuiScrollBar* GetScrollBar() { return m_sb; }

    // ---- DuiControl 覆写 ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    DuiControl* HitTest(POINT ptHostClient) override;
    bool    OnLButtonDown (POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp   (POINT pt, UINT mkFlags) override;
    bool    OnLButtonDblClk(POINT pt, UINT mkFlags) override;
    bool    OnMouseMove   (POINT pt, UINT mkFlags) override;
    bool    OnMouseLeave  () override;
    bool    OnMouseWheel  (POINT pt, short zDelta, UINT mkFlags) override;
    bool    OnKeyDown     (UINT vk, UINT flags) override;

private:
    static void OnSbScrolledStub(void* user, int newPos);
    void        UpdateScrollRange();
    int         RowsVisible() const;
    int         IndexFromPoint(POINT pt) const;
    RECT        ItemRect(int index) const;          // host-客户区坐标（已应用 scroll）
    int         ContentHeight() const               { return (int)m_items.size() * m_itemH; }
    int         BodyWidth() const;                  // m_rcItem 宽 - 滚动条宽

private:
    struct Item { CString text; LPARAM lParam; bool selected; bool checked; };
    std::vector<Item>   m_items;
    int                 m_curSel    = -1;
    int                 m_hoverIdx  = -1;
    int                 m_itemH     = 22;
    int                 m_sbWidth   = 12;
    DuiScrollBar*       m_sb        = nullptr;       // 所有权在 m_children

    bool                m_multiSelect    = false;
    bool                m_showCheckboxes = false;
    bool                m_dragReorder    = false;
    int                 m_dragSrcIdx     = -1;       // 当前拖动源行
    int                 m_dragDropIdx    = -1;       // 拖动期间计算出的落点行

    static const int kCheckColW = 22;

    COLORREF            m_clrBg       = RGB(255, 255, 255);
    COLORREF            m_clrText     = RGB( 30,  30,  30);
    COLORREF            m_clrHoverBg  = RGB(232, 240, 252);
    COLORREF            m_clrSelBg    = RGB(180, 210, 245);
    COLORREF            m_clrSelText  = RGB( 10,  30,  90);
};

// =================================================================
// DuiVirtualList —— 虚拟列表（不持有数据，按需画行）
// =================================================================
//
// 用途：超大数据集（最近会话几万条、聊天历史几十万条）的列表。跟
// DuiListBox 一样的滚动 + 选择 + 鼠标 / 键盘行为，但<u>不持有任何行
// 数据</u> —— caller 通过 SetItemCount(n) 声明行数 + 注册 PaintRowFn
// 回调画每行；选中状态、滚动位置、hover 状态由控件维护。
//
// 工作机制：
//   · 数据源型列表：caller 声明行数 + 给一个 paint 回调；控件永远不
//     缓存行内容。
//   · 滚动条 / 滚轮 / Up-Down 键 / DUIN_VALUECHANGED 事件模型同
//     DuiListBox。每次 paint 重新算可见行窗口（start = scrollPos /
//     itemH，count = visibleRows + 1）。
//   · PaintRowFn 每行调一次：(hdc, rowIndex, rowRect, selected, hover)。
//     selected / hover 由控件管，caller 只画行<u>内容</u>。
//   · RowClickFn（可选）单击 + 双击都 fire；选中事件外另开一路通知。
//
// 代码用法（基于 std::deque 的聊天历史）：
//
//     DuiVirtualList vlist;
//     vlist.SetRowHeight(40);
//     vlist.SetRowCount((int)history.size());
//     vlist.SetPaintRowCallback([](void* u, HDC hdc, int row, const RECT& rc,
//                                  bool sel, bool hover)
//     {
//         auto* h = static_cast<History*>(u);
//         PaintMessage(hdc, rc, h->at(row), sel, hover);
//     }, &history);
//     parent->AddChild(std::unique_ptr<DuiVirtualList>(&vlist));
//
// XML 用法：暂未原生支持（同 DuiListBox 的原因）。
//
// 事件：DUIN_VALUECHANGED（extra = 选中行 index）。可选 RowClickFn 回调。
//
// 替代关系：CRecentListCtrl（老 MainDlg 里巨大的最近会话缓存列表）。
class BUI_API DuiVirtualList : public DuiControl
{
public:
    // Paint 回调签名：在自己的位置画 rowIndex 行内容到 hdc 上。
    //   user：SetPaintRowCallback 时传入的指针。
    //   hdc：host 背缓冲 DC。
    //   rowIndex：行索引（[0, GetRowCount())）。
    //   rowRect：行矩形（host-客户区坐标，已应用 scroll）。
    //   selected：本行是否处于选中态。
    //   hover：本行是否处于 hover 态。
    typedef void (*PaintRowFn)(void* user, HDC hdc, int rowIndex,
                               const RECT& rowRect, bool selected, bool hover);

    // 单击 / 双击回调。isDoubleClick=true 表示双击。
    typedef void (*RowClickFn)(void* user, int rowIndex, bool isDoubleClick);

    DuiVirtualList();

    // 设置行数（变化时刷新滚动范围）。
    void    SetRowCount(int n);
    int     GetRowCount() const { return m_rowCount; }

    // 单行高度（px）。
    void    SetRowHeight(int h);
    int     GetRowHeight() const { return m_rowH; }

    // 注册 paint 回调（必填）。
    void    SetPaintRowCallback(PaintRowFn fn, void* user) { m_paint = fn; m_paintUser = user; }

    // 注册可选的 click 回调。
    void    SetRowClickCallback(RowClickFn fn, void* user) { m_click = fn; m_clickUser = user; }

    int     GetCurSel() const { return m_curSel; }
    void    SetCurSel(int idx, bool notify = true);

    void    EnsureVisible(int idx);
    int     GetScrollPos() const;
    void    SetScrollPos(int p);

    DuiScrollBar* GetScrollBar() { return m_sb; }

    // ---- DuiControl 覆写 ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    DuiControl* HitTest(POINT ptHostClient) override;
    bool    OnLButtonDown (POINT pt, UINT mkFlags) override;
    bool    OnLButtonDblClk(POINT pt, UINT mkFlags) override;
    bool    OnMouseMove   (POINT pt, UINT mkFlags) override;
    bool    OnMouseLeave  () override;
    bool    OnMouseWheel  (POINT pt, short zDelta, UINT mkFlags) override;
    bool    OnKeyDown     (UINT vk, UINT flags) override;

private:
    static void OnSbScrolledStub(void* user, int newPos);
    void        UpdateScrollRange();
    int         RowsVisible() const;
    int         IndexFromPoint(POINT pt) const;
    int         BodyWidth() const;

private:
    int             m_rowCount   = 0;
    int             m_rowH       = 28;
    int             m_curSel     = -1;
    int             m_hoverIdx   = -1;
    int             m_sbWidth    = 12;
    DuiScrollBar*   m_sb         = nullptr;
    PaintRowFn      m_paint      = nullptr;
    void*           m_paintUser  = nullptr;
    RowClickFn      m_click      = nullptr;
    void*           m_clickUser  = nullptr;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_LISTBOX
