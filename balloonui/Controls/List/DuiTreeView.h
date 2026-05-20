#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_TREEVIEW

// .cpp must include stdafx.h first.
#include "../../DuiControl.h"
#include <vector>

namespace balloonwjui {

class DuiEditHost;

// =================================================================
// DuiTreeView —— 树形列表 + 多列表格（展开 / 折叠 + Sel-cell + 6 种 cell 类型 + 冻结面板）
// =================================================================
//
// 用途：好友列表（按分组）、文件浏览器、设置项分类树；以及作为多列表格
// 的"分组任务表"、"项目文件清单"、"测试用例矩阵"等场景。
//
// 工作机制（两种模式）：
//
//   · <u>单列模式</u>（默认；未调 AddColumn）—— 行为跟老 DuiTreeView 完全一致：
//     每行 [▶ glyph][indent][icon][label][...][status dot]，无 header，无水平
//     滚动，靠外层 DuiScrollView 滚。所有老 API（AddRoot / AddChild /
//     SetItemLabel / SetItemIcon / SetItemStatusColor）零修改可用。
//
//   · <u>多列模式</u>（调过 AddColumn 后）—— 顶部固定 header 条（H3：拖列
//     宽 + 单击排序 + 双击 auto-fit + 右键 DUITVN_HEADER_RCLICK），下方
//     表体含<u>4 象限</u>渲染：
//
//          +-----------+--------------------+
//          |  TL       |       TR (frozen-rows + scroll-cols)
//          |  freeze   +--------------------+
//          |   xy      |       BR (full scroll)
//          +-----------+--------------------+
//
//     SetFrozenColumns(N) 让前 N 列贴左不水平滚；SetFrozenRows(N) 让前 N
//     个可见行贴顶不垂直滚（Excel 风格）。控件自管<u>水平 + 垂直</u>两条
//     滚动条（HS-α）—— 即使外面没套 DuiScrollView 也能滚。
//
//     col 0 仍是 "tree column"：保留 ▶ glyph + 缩进 + 可选 icon；其他列各自
//     独立的 cell 类型（Text / Icon / Image / CheckBox / ProgressBar /
//     Hyperlink，CC-γ）。
//
// 节点 / 列 身份：
//   · <u>节点</u>：插入时分配自增 int id（从 1 起），所有 public 操作按 id
//     索引（父展开时行号会漂，所以<u>没有</u> "row index" 给 caller 用）。
//   · <u>列</u>：按 AddColumn 顺序的 0-based 索引；增删列后索引会重排，业
//     务侧应该缓存"列名→索引"的映射而不是硬编码索引。
//
// Cell 类型与编辑（CC-γ + E-γ + EG-β）：
//
//   类型              | payload                  | 显示              | 默认交互
//   ------------------|--------------------------|-------------------|----------------
//   CELL_TEXT         | text                     | 单行文字          | F2/双击 → in-place EDIT
//   CELL_ICON         | HBITMAP（18×18）         | 居中小图标        | 无
//   CELL_IMAGE        | HBITMAP（缩放到列宽）    | 自适应大图        | 无
//   CELL_CHECKBOX     | bool checked + 可选文字  | 方框 + ✓          | 单击切换
//   CELL_PROGRESS     | int value（0..100）+ 文字| 进度条 + 百分比   | 单击 / 拖动改值
//   CELL_HYPERLINK    | display text + url       | 蓝色下划线文字    | 单击发 LINKCLICK
//
//   编辑闸：SetEditable(false) <u>默认开</u>（库默认只读）—— Text 双击 / F2 不
//   进 EDIT；CheckBox 单击不切；ProgressBar 拖动不响应。Hyperlink<u>始终</u>
//   可点（导航语义跟编辑无关）；Icon / Image 始终纯显示。
//
//   per-列闸：SetColumnEditable(col, false) 关掉某列的编辑（即使全局开了）。
//
// 选中模型（Sel-cell）：
//   · <u>多列模式</u>下选中粒度 = 单元格（itemId, col）。Ctrl/Shift 多选。
//   · <u>单列模式</u>下选中粒度 = 行（兼容 GetCurSel 返回 itemId 的老语义）。
//   · 当前活动 cell 画 1px 品牌蓝边 + 1px 内白虚（cell focus rect）。
//
// 事件载体（EX-β）：
//   · <u>老事件</u>（DUIN_CLICK / DUIN_DBLCLK / DUITVN_RCLICK / DUITVN_EXPAND_TOGGLED
//     / DUIN_VALUECHANGED）—— extra = item id（int）。<u>不变</u>。
//   · <u>新事件</u>（DUITVN_HEADER_RCLICK / DUITVN_BLANK_RCLICK / DUITVN_COLUMNCLICK
//     / DUITVN_CHECKED / DUITVN_VALUECHANGED_CELL / DUITVN_LINKCLICK / DUITVN_CELLEDITED）
//     —— extra = `DuiTreeCellNotify*`（生命期仅在 SendMessage 调用栈期间有效）。
//
// 排序（S-α）：
//   库不排数据。点列名只发 DUITVN_COLUMNCLICK + 在该列画三角箭头（业务侧
//   排完数据后调 SetSortIndicator(col, +1/-1) 切换三角方向）。
//
// 代码用法（多列）：
//
//     auto tree = std::unique_ptr<DuiTreeView>(new DuiTreeView());
//     int colName = tree->AddColumn(_T("名称"), 200);
//     int colSize = tree->AddColumn(_T("大小"), 80, 40, DT_RIGHT);
//     int colDone = tree->AddColumn(_T("完成"), 60, 40, DT_CENTER);
//     int colURL  = tree->AddColumn(_T("链接"), 240);
//
//     int g = tree->AddRoot(_T("我的项目"));
//     int n = tree->AddChild(g, _T("README.md"));
//     tree->SetCellText (n, colSize, _T("4.2 KB"));
//     tree->SetCellChecked(n, colDone, true);
//     tree->SetCellLink (n, colURL, _T("打开"), _T("https://example.com"));
//
//     tree->SetFrozenColumns(1);              // 名称列贴左不水平滚
//     tree->SetEditable(true);                // 允许 Text 双击编辑
//     tree->SetCtrlId(IDC_PROJ_TREE);
//     parent->AddChild(std::move(tree));
//
// XML 用法（多列模式）：
//
//     <treeview id="100">
//         <column title="名称" width="200" min-width="60"/>
//         <column title="大小" width="80"  align="right" sortable="true"/>
//         <column title="完成" width="60"  align="center" editable="true"/>
//     </treeview>
//
//   注意：节点本身仍由 C++ AddRoot / AddChild 添加，XML <u>只配列</u>（XML 范围
//   B）—— 对运行时动态变化的数据用 XML 表达不划算。
//
// 事件汇总（ctrlId = treeview id）：
//
//   · DUIN_CLICK              — 选中（单击非 glyph 区）；extra = item id。
//   · DUIN_DBLCLK             — 双击；extra = item id。
//   · DUIN_VALUECHANGED       — 当前选中变化；extra = item id 或 -1。
//   · DUITVN_EXPAND_TOGGLED   — 展开 / 折叠完成；extra = item id。
//   · DUITVN_RCLICK           — 行右键；<u>不改</u>选中；extra = item id。
//   · DUITVN_HEADER_RCLICK    — header 列右键；extra = DuiTreeCellNotify*（col 字段有效）。
//   · DUITVN_BLANK_RCLICK     — 空白区右键；extra = DuiTreeCellNotify*（itemId=-1, col=-1）。
//   · DUITVN_COLUMNCLICK      — header 列单击（排序意图）；extra = DuiTreeCellNotify*（col + ascending）。
//   · DUITVN_CHECKED          — CheckBox cell 切换；extra = DuiTreeCellNotify*（itemId, col, checked）。
//   · DUITVN_VALUECHANGED_CELL — ProgressBar cell 拖动 / 单击改值；extra = DuiTreeCellNotify*（value）。
//   · DUITVN_LINKCLICK        — Hyperlink cell 单击；extra = DuiTreeCellNotify*（text=URL）。
//   · DUITVN_CELLEDITED       — Text cell in-place EDIT 提交；extra = DuiTreeCellNotify*（text=新文本）。
class BUI_API DuiTreeView : public DuiControl
{
public:
    // ---- cell type system ----

    enum CellType
    {
        CELL_TEXT      = 0,    // CString text（默认）
        CELL_ICON      = 1,    // HBITMAP，固定 18×18 居中
        CELL_IMAGE     = 2,    // HBITMAP，按列宽缩放
        CELL_CHECKBOX  = 3,    // bool checked + 可选文字
        CELL_PROGRESS  = 4,    // int value 0..100 + 可选文字
        CELL_HYPERLINK = 5,    // display text + URL
    };

    enum NotifyCode
    {
        // ---- legacy（向后兼容；extra = itemId int）----
        DUITVN_EXPAND_TOGGLED   = DUIN_CUSTOM + 1,
        DUITVN_RCLICK           = DUIN_CUSTOM + 2,

        // ---- new（多列模式；extra = DuiTreeCellNotify*）----
        DUITVN_HEADER_RCLICK    = DUIN_CUSTOM + 3,    // header 列名右键
        DUITVN_BLANK_RCLICK     = DUIN_CUSTOM + 4,    // 表体空白区右键
        DUITVN_COLUMNCLICK      = DUIN_CUSTOM + 5,    // header 列名单击（排序意图）
        DUITVN_CHECKED          = DUIN_CUSTOM + 6,    // CheckBox cell 切换
        DUITVN_VALUECHANGED_CELL = DUIN_CUSTOM + 7,   // ProgressBar cell 改值
        DUITVN_LINKCLICK        = DUIN_CUSTOM + 8,    // Hyperlink cell 单击
        DUITVN_CELLEDITED       = DUIN_CUSTOM + 9,    // Text cell EDIT 提交
    };

    // 多列事件载体。生命期：仅在父收到 WM_DUI_NOTIFY → SendMessage 同步
    // 调用栈期间有效（在控件栈上）。父若需要存值要 deep-copy text。
    struct DuiTreeCellNotify
    {
        int      itemId;       // -1 = 不关联具体节点（header / blank）
        int      col;          // 0-based 列索引；-1 = 不关联具体列
        bool     checked;      // 用于 DUITVN_CHECKED：新勾选状态
        int      value;        // 用于 DUITVN_VALUECHANGED_CELL：新值 0..100
        LPCTSTR  text;         // 用于 DUITVN_CELLEDITED：新文本；
                               // 用于 DUITVN_LINKCLICK：URL；
                               // 其他事件未定义。
        bool     ascending;    // 用于 DUITVN_COLUMNCLICK：业务"应该"以
                               // 升序还是降序排（库根据当前 sort 指示
                               // 自动翻转）。
    };

    // 单元格引用（用于 GetCurSelCell / GetSelectedCell 等）。
    struct CellRef
    {
        int  itemId;           // -1 = 无选中
        int  col;              // -1 = 无选中（或单列模式）
    };

    DuiTreeView();
    ~DuiTreeView() override;

    // ---- structure ----
    int   AddRoot (LPCTSTR label, HBITMAP icon = nullptr, LPARAM param = 0);
    int   AddChild(int parentId, LPCTSTR label, HBITMAP icon = nullptr, LPARAM param = 0);
    void  Remove(int id);              // 删除子树
    void  Clear();

    int   GetRootCount() const;
    int   GetChildCount(int parentId) const;
    bool  HasChildren(int id) const;

    // ---- expand / collapse ----
    void  Expand   (int id) { SetExpanded(id, true);  }
    void  Collapse (int id) { SetExpanded(id, false); }
    void  SetExpanded(int id, bool exp);
    bool  IsExpanded(int id) const;
    void  ExpandAll();
    void  CollapseAll();

    // ---- selection ----
    int     GetCurSel() const { return m_curSelId; }
    void    SetCurSel(int id, bool notify = true);     // -1 清空
    CellRef GetCurSelCell() const;                     // 多列模式返 (id, col)；单列返 (id, -1)
    void    SetCurSelCell(int itemId, int col, bool notify = true);

    // 多选：第 n 个选中 cell；n 越界返 {-1, -1}。
    int     GetSelectedCellCount() const;
    CellRef GetSelectedCell(int n) const;
    void    ClearSelection();

    // ---- per-item state（legacy；作用于 col 0）----
    void     SetItemLabel      (int id, LPCTSTR label);
    CString  GetItemLabel      (int id) const;
    void     SetItemIcon       (int id, HBITMAP icon);   // tree icon（col 0 文字左侧）
    HBITMAP  GetItemIcon       (int id) const;
    void     SetItemParam      (int id, LPARAM param);
    LPARAM   GetItemParam      (int id) const;
    // 状态点（仅<u>单列模式</u>有效；多列模式下不画，因为右端可能在水平
    // 滚出去）。CLR_INVALID = 不画。
    void     SetItemStatusColor(int id, COLORREF color);
    COLORREF GetItemStatusColor(int id) const;

    // ---- columns（调过 AddColumn 后切换到多列模式）----
    int     GetColumnCount() const;
    int     AddColumn(LPCTSTR title, int width,
                      int minWidth = kColMinWidthDefault,
                      UINT textAlign = DT_LEFT,
                      bool sortable = true);
    void    ClearColumns();                              // 切回单列模式
    void    SetColumnTitle    (int col, LPCTSTR title);
    CString GetColumnTitle    (int col) const;
    void    SetColumnWidth    (int col, int px);
    int     GetColumnWidth    (int col) const;
    void    SetColumnMinWidth (int col, int px);
    int     GetColumnMinWidth (int col) const;
    void    SetColumnTextAlign(int col, UINT dtFlags);
    UINT    GetColumnTextAlign(int col) const;
    void    SetColumnSortable (int col, bool b);
    bool    IsColumnSortable  (int col) const;

    // 排序三角指示器（S-α：业务排完调）。dir：0 = 不画，+1 = 升序（▲），
    // -1 = 降序（▼）。
    void    SetSortIndicator(int col, int dir);
    int     GetSortIndicatorCol() const { return m_sortCol; }
    int     GetSortIndicatorDir() const { return m_sortDir; }

    // ---- frozen panes（多列模式生效）----
    void    SetFrozenColumns(int n);          // 前 n 列不水平滚
    void    SetFrozenRows   (int n);          // 前 n 个可见行不垂直滚
    int     GetFrozenColumns() const { return m_frozenCols; }
    int     GetFrozenRows   () const { return m_frozenRows; }

    // ---- cell content（多列）----
    void     SetCellType   (int id, int col, CellType t);
    CellType GetCellType   (int id, int col) const;
    // SetCellText：等价于 SetItemLabel(id, ...) 当 col == 0。
    void     SetCellText   (int id, int col, LPCTSTR text);
    CString  GetCellText   (int id, int col) const;
    // 把单元格切到 ICON / IMAGE 类型并设位图。
    void     SetCellIcon   (int id, int col, HBITMAP icon);
    void     SetCellImage  (int id, int col, HBITMAP image);
    HBITMAP  GetCellBitmap (int id, int col) const;
    // 把单元格切到 CHECKBOX 类型并设勾选状态。
    void     SetCellChecked(int id, int col, bool checked);
    bool     IsCellChecked (int id, int col) const;
    // 把单元格切到 PROGRESS 类型并设值（自动 clamp 到 0..100）。
    void     SetCellValue  (int id, int col, int value);
    int      GetCellValue  (int id, int col) const;
    // 把单元格切到 HYPERLINK 类型，display 文字 + URL（HitTest 触发 LINKCLICK 时回带 URL）。
    void     SetCellLink   (int id, int col, LPCTSTR displayText, LPCTSTR url);
    CString  GetCellLinkUrl(int id, int col) const;

    // ---- editing（EG-β）----
    void    SetEditable      (bool b);                   // 全局开关；默认 false（只读）
    bool    IsEditable       () const { return m_editable; }
    void    SetColumnEditable(int col, bool b);
    bool    IsColumnEditable (int col) const;

    // 主动启动 / 取消 / 提交 in-place EDIT（仅 CELL_TEXT + 编辑闸均开启时
    // 起效）。BeginEdit 失败返 false。
    bool    BeginEdit (int id, int col);
    void    CancelEdit();
    void    CommitEdit();
    bool    IsEditing() const { return m_editor != nullptr; }

    // ---- visible-row queries ----
    int   GetVisibleCount () const;
    int   GetContentHeight() const;            // 表体内容总高（不含 header）
    int   GetContentWidth () const;            // 表体内容总宽（多列模式下 = 列宽和；单列 0）
    int   GetVisibleRow   (int id) const;      // 可见行号；隐藏 → -1
    int   GetIdAtVisibleRow(int n) const;

    // ---- hit test ----
    int   HitTestId  (POINT pt) const;
    bool  HitTestCell(POINT pt, int& outId, int& outCol) const;

    // ---- appearance ----
    void  SetRowHeight(int px);                // 默认 28px，clamp >=8
    int   GetRowHeight() const { return m_rowH; }
    void  SetIndentPx (int px);                // 默认 18px，clamp >=1
    int   GetIndentPx () const { return m_indent; }
    void  SetHeaderHeight(int px);             // 默认 26px；多列模式下生效
    int   GetHeaderHeight() const { return m_headerH; }

    void  SetRowBgColor    (COLORREF c) { m_clrRowBg    = c; Invalidate(); }
    void  SetRowSelColor   (COLORREF c) { m_clrRowSel   = c; Invalidate(); }
    void  SetRowHoverColor (COLORREF c) { m_clrRowHover = c; Invalidate(); }
    void  SetTextColor     (COLORREF c) { m_clrText     = c; Invalidate(); }
    void  SetSelTextColor  (COLORREF c) { m_clrTextSel  = c; Invalidate(); }
    void  SetGlyphColor    (COLORREF c) { m_clrGlyph    = c; Invalidate(); }
    void  SetHeaderBgColor (COLORREF c) { m_clrHeaderBg = c; Invalidate(); }
    void  SetHeaderTextColor(COLORREF c){ m_clrHeaderText = c; Invalidate(); }
    void  SetGridLineColor (COLORREF c) { m_clrGrid     = c; Invalidate(); }

    // 斑马纹（V-1：默认关）。
    void  SetZebra     (bool b);
    bool  IsZebra      () const         { return m_zebra; }
    void  SetZebraColor(COLORREF c)     { m_clrZebra   = c; Invalidate(); }

    // ---- DuiControl overrides ----
    void  Layout(const RECT& rcAvail) override;
    void  OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool  OnLButtonDown  (POINT pt, UINT mkFlags) override;
    bool  OnLButtonUp    (POINT pt, UINT mkFlags) override;
    bool  OnLButtonDblClk(POINT pt, UINT mkFlags) override;
    bool  OnRButtonDown  (POINT pt, UINT mkFlags) override;
    bool  OnMouseMove    (POINT pt, UINT mkFlags) override;
    bool  OnMouseLeave   () override;
    bool  OnMouseWheel   (POINT pt, short zDelta, UINT mkFlags) override;
    bool  OnSetCursor    (POINT pt) override;
    bool  OnKeyDown      (UINT vk, UINT flags) override;

    // ---- 默认常量 ----

    // 列默认最小像素宽（W-1）。低于此值即使 SetColumnWidth 也强制抬到这里。
    static const int kColMinWidthDefault = 40;
    // 行高默认（行内文字 + 上下 padding）。
    static const int kRowHeightDefault   = 28;
    // 多列 header 条默认高度。
    static const int kHeaderHeightDefault = 26;
    // 每深度缩进默认像素。
    static const int kIndentPxDefault    = 18;

private:
    // ---- internal data ----

    struct Cell
    {
        CellType  type        = CELL_TEXT;
        CString   text;
        CString   url;          // 仅 CELL_HYPERLINK
        HBITMAP   bitmap       = nullptr;     // CELL_ICON / CELL_IMAGE
        bool      checked      = false;
        int       value        = 0;           // CELL_PROGRESS：0..100
    };

    struct Node
    {
        int                   id;
        int                   parentIdx;       // -1 for root
        int                   depth;
        bool                  expanded;
        CString               label;           // legacy single-col label（== cells[0].text 的镜像）
        HBITMAP               icon;            // legacy tree icon（col 0 文字左侧）
        LPARAM                param;
        COLORREF              statusColor;     // CLR_INVALID = no dot
        std::vector<Cell>     cells;           // 多列模式 cells[col]；单列模式空
    };

    struct ColumnDef
    {
        CString  title;
        int      width;
        int      minWidth;
        UINT     textAlign;     // DT_LEFT / DT_CENTER / DT_RIGHT
        bool     sortable;
        bool     editable;
    };

    enum HitZone
    {
        HZ_NONE        = 0,
        HZ_HEADER_TEXT = 1,    // 在某列 header 文字区
        HZ_HEADER_SEP  = 2,    // 在某列 header 右边的分隔线（拖列宽 / 双击 auto-fit）
        HZ_BODY_GLYPH  = 3,    // 在 col 0 的展开 glyph 上
        HZ_BODY_CELL   = 4,    // 在表体某 cell 上
        HZ_BODY_BLANK  = 5,    // 在表体空白区（行外）
    };

    struct HitInfo
    {
        HitZone zone        = HZ_NONE;
        int     visibleRow  = -1;     // body 命中：可见行号
        int     col         = -1;     // 命中列；HZ_BODY_BLANK / HZ_NONE 时 -1
        int     itemId      = -1;     // 等价 m_visible[visibleRow]
    };

    // ---- node helpers ----
    int   IndexOf(int id) const;
    void  RemoveSubtree(int idx);
    void  RebuildVisible();
    bool  HasChildrenIdx(int idx) const;

    // ---- cell helpers ----
    Cell&       EnsureCell(int idx, int col);
    const Cell* TryGetCell(int idx, int col) const;
    void        SyncLegacyToCell0(int idx);           // cells[0] 与 node.label 同步
    void        SyncCell0ToLegacy(int idx);

    // ---- geometry ----
    bool  IsMultiCol() const { return !m_columns.empty(); }
    void  RecomputeLayout();                          // 算 header / body / scrollbars 矩形
    int   ComputeContentWidth() const;                // 多列模式下列宽和
    void  EnsureScrollRanges();                       // 同步滚动条 range / page
    int   ColumnLeftAbs(int col) const;               // 列左（content 坐标，0 = content 起点）
    int   ColumnRightAbs(int col) const;
    RECT  HeaderRect() const;                         // 多列 header 条；单列空 rect
    RECT  BodyRect() const;                           // header 下 + 滚动条 outside 的表体区
    RECT  RowRect(int visibleRow) const;              // 可见行行 rect（内容坐标 — 不含滚动）
    RECT  GlyphRectOf(int visibleRow, int depth) const;
    RECT  CellRectInRow(const RECT& rowRc, int col) const;  // 多列：列在行内的 cell rect
    void  ComputeQuadrants(RECT& rcTL, RECT& rcTR,
                           RECT& rcBL, RECT& rcBR) const;   // 4 象限（含冻结面板）
    int   FrozenColsRightX() const;                   // 冻结列右边 x（body 坐标）
    int   FrozenRowsBottomY() const;                  // 冻结行底部 y（body 坐标，相对 header 下方）

    // ---- hit test ----
    HitInfo HitTest_(POINT pt) const;

    // ---- paint ----
    void  PaintHeader  (HDC hdc, const RECT& rcDirty) const;
    void  PaintBody    (HDC hdc, const RECT& rcDirty) const;
    void  PaintRow     (HDC hdc, int visibleRow,
                        int xOffset, int yOffset,
                        bool clipToFrozenCols, bool drawOnlyFrozenCols) const;
    void  PaintGlyph   (HDC hdc, const RECT& rc, bool expanded) const;
    void  PaintCell    (HDC hdc, const RECT& rcCell, const Node& n,
                        int col, bool selected, bool focusCell) const;
    void  PaintSortArrow(HDC hdc, const RECT& rcArrow, int dir) const;
    void  PaintCheckBox(HDC hdc, const RECT& rcBox, bool checked) const;
    void  PaintProgress(HDC hdc, const RECT& rcCell, int value, LPCTSTR text) const;
    void  PaintHyperlink(HDC hdc, const RECT& rcCell, LPCTSTR text, COLORREF c) const;

    // ---- scroll ----
    // 注：ScrollPosX/Y 必须 public，因为它们由 DuiScrollBar 的 OnScrollFn
    // C-trampoline 调用（自由函数，没法做成 friend）。Caller 业务一般
    // 不直接调；用 SetCurSelCell + 让控件自己定位即可。
public:
    void  ScrollPosX(int x);
    void  ScrollPosY(int y);
private:
    void  ScrollX(int dx);
    void  ScrollY(int dy);
    int   ScrollPosX() const { return m_scrollX; }
    int   ScrollPosY() const { return m_scrollY; }

    // ---- editor (in-place EDIT for CELL_TEXT) ----
    void  PlaceEditor();                              // 把 m_editor 移到当前编辑 cell rect

    // ---- legacy compat ----
    int   FirstAncestorIdx(int idx, int rootIdx) const;

    // ---- internal painted constants ----
    enum : int
    {
        kGlyphStripPx     = 16,    // ▶ glyph 区宽
        kIconSizePx       = 18,    // 老 tree icon 尺寸
        kCellIconPx       = 18,    // CELL_ICON 尺寸
        kCellPad          = 6,     // cell 文字两侧 padding
        kStatusDotR       = 5,     // 老状态点半径
        kRightPad         = 8,     // 单列模式右端 padding
        kHeaderSepHotPx   = 4,     // 列分隔线鼠标命中宽（左右各 4px）
        kSortArrowSizePx  = 8,
        kCheckBoxSizePx   = 16,
        kProgressBarH     = 14,    // 进度条厚度
        kScrollBarPx      = 12,
        kZebraDefault     = 0,
    };

private:
    // ---- node data ----
    std::vector<Node>      m_nodes;
    std::vector<int>       m_visible;
    int                    m_nextId   = 1;
    int                    m_curSelId = -1;
    int                    m_hoverId  = -1;

    // ---- columns ----
    std::vector<ColumnDef> m_columns;
    int                    m_frozenCols = 0;
    int                    m_frozenRows = 0;
    int                    m_sortCol = -1;
    int                    m_sortDir =  0;     // 0=none, +1 asc, -1 desc

    // ---- cell-level selection (multi-col) ----
    std::vector<CellRef>   m_selCells;          // 多列模式；单列模式空（用 m_curSelId）
    CellRef                m_focusCell{ -1, -1 };  // 当前活动 cell（画 focus rect）

    // ---- editing state ----
    bool                   m_editable = false;
    DuiEditHost*           m_editor   = nullptr;   // 子控件；指针由 m_children 持有
    int                    m_editId   = -1;
    int                    m_editCol  = -1;

    // ---- scroll ----
    int                    m_scrollX  = 0;     // body 内容向左偏移的像素
    int                    m_scrollY  = 0;     // 同上向上
    DuiControl*            m_hScroll  = nullptr;     // DuiScrollBar*；m_children 持有
    DuiControl*            m_vScroll  = nullptr;
    // 滚动条按需显示。两个轴都跑 2 阶段算法：先按"都不需要"假设算
    // contentW/H 与 viewW/H，得 needH/V；再代入算稳定值。BodyRect 用
    // m_needHScroll / m_needVScroll 决定哪边预留 gutter。不需要时数据
    // 区直接占满到底/到右，不留空白条。
    bool                   m_needHScroll = false;
    bool                   m_needVScroll = false;

    // ---- header drag-resize state ----
    bool                   m_resizing      = false;
    int                    m_resizeCol     = -1;
    int                    m_resizeStartX  = 0;
    int                    m_resizeStartW  = 0;

    // ---- visual ----
    int       m_rowH    = kRowHeightDefault;
    int       m_indent  = kIndentPxDefault;
    int       m_headerH = kHeaderHeightDefault;
    bool      m_zebra   = false;

    COLORREF  m_clrRowBg       = RGB(255, 255, 255);
    COLORREF  m_clrRowSel      = RGB( 45, 108, 223);    // brand blue
    COLORREF  m_clrRowHover    = RGB(232, 240, 252);
    COLORREF  m_clrText        = RGB( 30,  30,  30);
    COLORREF  m_clrTextSel     = RGB(255, 255, 255);
    COLORREF  m_clrGlyph       = RGB(110, 110, 120);
    // header 背景默认几乎接近行底（仅靠底线 + 文字区分），与 Win10
    // 任务管理器 / Explorer 的"几乎无视感"表头一致。业务侧若需要深色
    // 表头可走 SetHeaderBgColor 覆盖。
    COLORREF  m_clrHeaderBg    = RGB(252, 252, 253);
    COLORREF  m_clrHeaderText  = RGB( 60,  60,  70);
    COLORREF  m_clrGrid        = RGB(225, 227, 232);    // 列分隔线 / 单元格分隔
    COLORREF  m_clrZebra       = RGB(248, 248, 250);
    COLORREF  m_clrFocusBorder = RGB( 45, 108, 223);    // cell focus rect 主色
    COLORREF  m_clrLink        = RGB( 45, 108, 223);
};

} // namespace balloonwjui

#endif // BUI_FEATURE_TREEVIEW
