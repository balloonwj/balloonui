#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_TREEVIEW

// .cpp must include stdafx.h first.
#include "../../DuiControl.h"
#include <functional>
#include <map>
#include <memory>
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

        // ---- hover（鼠标 hover 节点变化；extra = itemId int）----
        // 鼠标进入新节点时 fire ENTER；离开之前节点时 fire LEAVE。
        // 库本身<u>无延时</u>：每次 m_hoverId 变化都立即 fire。业务侧若要
        // "悬停 ~500ms 才弹卡片"语义，自己在 ENTER 时启 SetTimer、LEAVE
        // 时 KillTimer 实现。两个 notify 都用普通 DuiNotify 载体，extra
        // 字段为节点 id；屏幕坐标业务侧自取 ::GetCursorPos()。
        DUITVN_HOVER_ENTER      = DUIN_CUSTOM + 10,   // 鼠标进入新节点
        DUITVN_HOVER_LEAVE      = DUIN_CUSTOM + 11,   // 鼠标离开之前节点
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

    // 收集当前所有处于展开态节点的 id 列表，便于"存 → 改数据 → 还"
    // 模式。返回顺序按 m_nodes 的插入序（与节点 id 单调递增同步），
    // 业务侧通常无需关心顺序。
    std::vector<int> GetExpandedSnapshot() const;
    // 把 snapshot 里包含的节点恢复成展开态。
    // <u>增量</u>语义：只对 snapshot 内 id 调 Expand；不在 snapshot 里
    // 的节点保持当前展开状态不变（不会被强制折叠）。snapshot 里如有
    // 已不存在的 id（节点期间被 Remove）静默跳过。一次刷新触发一次
    // RebuildVisible + Invalidate。
    void  RestoreExpanded(const std::vector<int>& ids);

    // ---- 节点可见性 / 过滤（不删数据，只改可见性）----
    // 单节点可见性开关。设为 false 时本节点 + 其后代（不管是否展开）
    // 不出现在可见行列表里，但仍在 m_nodes 数据里、不被销毁。
    // 默认 true。父被隐藏时子节点也跟着隐藏（按 depth 范围跳过）。
    void  SetItemVisible(int id, bool visible);
    bool  IsItemVisible (int id) const;

    // 节点是否可被选中。
    //   true (默认):点击该行,SetCurSel 设此节点为当前选中,PaintRow 画
    //               选中底色;
    //   false:      点击该行不变选中态(若有子,仍允许 toggle 展开),
    //               PaintRow 也不画选中底色。用于把 root 节点当成 grouping
    //               header 用 —— 业务侧把"我的好友"/"服务号"这类分组标题
    //               设为 selectable=false,避免点击 root 出现高亮底色。
    void  SetItemSelectable(int id, bool selectable);
    // 全局过滤器。Predicate 接收节点 id，返回 true 表示<u>保留可见</u>，
    // false 表示隐藏（连同其后代）。最终可见 = SetItemVisible 标志
    // 为 true 且（无 filter 或 filter 返回 true）。设为 nullptr =
    // 移除过滤器。每次 SetFilter 触发一次 RebuildVisible + Invalidate。
    void  SetFilter(std::function<bool(int /*nodeId*/)> filter);
    void  ClearFilter();

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
    // 节点副标签（仅<u>单列模式</u>有效；多列模式下忽略，多列请用 SetCellText）。
    // 非空时本节点行内分两行垂直绘制：主标签上方、副标签下方（字号
    // 较小、颜色更淡）；空字符串表示退回单行（主标签垂直居中）。
    // 行高沿用 SetRowHeight（默认 28），主+副紧凑放进同一行；要让两行
    // 视觉宽松，调用方应把行高加到 ~40。副标签字号 / 颜色由
    // SetSubLabelPointSize / SetSubLabelTextColor 全局控制。
    void     SetItemSubLabel   (int id, LPCTSTR sub);
    CString  GetItemSubLabel   (int id) const;
    void     SetItemIcon       (int id, HBITMAP icon);   // tree icon（col 0 文字左侧）
    HBITMAP  GetItemIcon       (int id) const;
    // 节点 icon 灰显标志（仅<u>单列模式</u>有效；多列模式忽略）。
    // 设为 true 时本节点的 SetItemIcon 位图在绘制阶段经 GdiPlus
    // ColorMatrix 转灰度（NTSC luma 权重）后输出，原 HBITMAP 不动；典型用
    // 例："离线好友 / 已禁用项" 这种"在但不可用"的视觉表达。
    void     SetItemIconGrayed (int id, bool grayed);
    bool     IsItemIconGrayed  (int id) const;
    void     SetItemParam      (int id, LPARAM param);
    LPARAM   GetItemParam      (int id) const;
    // 状态点（仅<u>单列模式</u>有效；多列模式下不画，因为右端可能在水平
    // 滚出去）。CLR_INVALID = 不画。
    void     SetItemStatusColor(int id, COLORREF color);
    COLORREF GetItemStatusColor(int id) const;
    // 节点右侧辅助文字（仅<u>单列模式</u>有效；多列模式忽略）。
    // 显示位置：行右端的 status dot 左侧（无 dot 时贴 kRightPad 右边距），
    // 与主 label 同行右对齐。空字符串 = 不绘。典型用例：分组节点的
    // "在线数 / 总数"、单元格的尺寸 / 时间 / 数量。绘出实际占用宽度后
    // 会向左收缩 textRight，主 label 自动 ellipsis。
    void     SetItemRightText  (int id, LPCTSTR rightText);
    CString  GetItemRightText  (int id) const;

    // ---- sub-label 全局视觉（单列模式两行节点的副文本属性）----
    // 副标签字号（pt），默认 8（比主标签的 9pt 略小）。调用 0 或负值取默认。
    void     SetSubLabelPointSize(int pt);
    int      GetSubLabelPointSize() const { return m_subLabelPt; }
    // 副标签文字颜色，默认 RGB(140,140,140) ── 浅灰，与主标签拉开层级。
    void     SetSubLabelTextColor(COLORREF c);
    COLORREF GetSubLabelTextColor() const { return m_clrSubLabelText; }

    // ---- right-text 全局视觉（单列模式行右端辅助文字属性）----
    // 右侧文字字号（pt），0 = 用默认字体（与主 label 同字号 9pt）。
    void     SetRightTextPointSize(int pt);
    int      GetRightTextPointSize() const { return m_rightTextPt; }
    // 右侧文字颜色，默认 RGB(140,140,140) ── 浅灰，与主 label 拉开层级。
    void     SetRightTextColor(COLORREF c);
    COLORREF GetRightTextColor() const { return m_clrRightText; }

    // ---- icon 视觉（仅<u>单列模式</u>及多列模式 col 0 的 tree icon 生效）----
    // 节点 icon 显示尺寸(逻辑像素,正方形)。默认 18(向后兼容老行为)。
    // 调用后影响节点 icon 的绘制大小 + 行内 cursor 偏移 + auto-fit 内容宽,
    // 不影响多列模式下 CELL_ICON / CELL_IMAGE 类 cell(它们有独立尺寸约束)。
    // 取值范围 clamp 到 [8, 64];小于 8 当 8,大于 64 当 64。
    void     SetIconSize(int px);
    int      GetIconSize() const { return m_iconSizePx; }
    // 节点 icon 圆角半径(逻辑像素)。默认 0 = 直角(向后兼容)。设为 > 0 时,
    // 绘 icon 前用 CreateRoundRectRgn 设剪裁区,使 StretchBlt 出的位图呈圆角
    // 外观;原 HBITMAP 内容不变。圆角半径会被 clamp 到 icon 一半(避免越界)。
    // 注意:HRGN 是 1-bit mask,圆形边缘没有抗锯齿(像素阶梯)。需要平滑圆形
    // 时改走 SetIconUsesAlpha(true) 路径——见下方说明。
    void     SetIconCornerRadius(int px);
    int      GetIconCornerRadius() const { return m_iconCornerRadiusPx; }
    // 节点 icon 是否走 alpha-aware 绘制路径。默认 false(向后兼容旧调用方)。
    //   false:绘制走 StretchBlt(SRCCOPY),icon 当不透明矩形,圆角靠
    //         ExtSelectClipRgn(round rect HRGN)做硬 mask 剪裁——HRGN 是
    //         1-bit mask,圆形边缘有锯齿。
    //   true: 绘制走 AlphaBlend(AC_SRC_OVER + AC_SRC_ALPHA),要求 icon 位图
    //         为 32bpp premultiplied alpha;圆角应由调用方在位图内自绘
    //         (推荐 GDI+ AA),DuiTreeView 不再做 HRGN clip(避免硬边)。
    //         本路径下 SetIconCornerRadius 的值被忽略。
    //         典型用例:GDI+ 画 AA 圆形头像 / 缩略图,无锯齿。
    // 全局开关,影响本 tree 所有节点 icon 渲染(单列 + 多列 col 0 的 tree
    // icon);不影响 CELL_ICON / CELL_IMAGE 类多列 cell。
    void     SetIconUsesAlpha(bool useAlpha);
    bool     GetIconUsesAlpha() const { return m_iconUsesAlpha; }

    // ---- 节点自绘控件(custom-node mode)----
    // 给节点挂自绘控件,接管该节点的内容区绘制和事件。
    //
    // 设置后,tree 对该节点行:
    //   - <u>不再</u>画内置内容(label / icon / sub-label / right-text / status dot)
    //   - <u>仍</u>画:行 bg(含选中/hover 高亮)、glyph 展开箭头、缩进 indent
    //   - 把内容区(glyph + indent 右侧的剩余宽度)交给 ctrl:
    //         ctrl->Layout(rcContent) + ctrl->OnPaint(hdc, rcDirty)
    //   - 内容区 mouse 事件 先转给 ctrl(ctrl 返 true = 消耗); ctrl 未消耗时
    //     tree 走默认行为(选中行 / hover 高亮等)。
    //
    // 所有权:tree 持有 ctrl;再次设置替换; nullptr 清除回退 legacy 模式。
    // 节点 Remove / Clear 时自动销毁 ctrl。
    //
    // 仅<u>单列模式</u>有效(多列模式已有 CELL_* 系列,本 API 忽略)。
    // 行高:v1 仍用全局 SetRowHeight;业务保证 ctrl 内容能装得下。按节点
    // 变高的 SetItemRowHeight(id, px) 留 v2 扩展。
    void         SetItemCustomControl(int id, std::unique_ptr<DuiControl> ctrl);
    DuiControl*  GetItemCustomControl(int id) const;

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
        int                   id           = 0;
        int                   parentIdx    = -1;
        int                   depth        = 0;
        bool                  expanded     = false;
        bool                  visible      = true;    // SetItemVisible 标志；false 时连同后代隐藏
        bool                  selectable   = true;    // false:点击此行不变选中态(仅切换展开);PaintRow 也不画选中底色。用于把 root 当成 grouping header。
        CString               label;                  // legacy single-col label（== cells[0].text 的镜像）
        CString               subLabel;               // 副标签（仅单列模式生效）；空字符串表示不显示，行内单行绘
        CString               rightText;              // 右侧辅助文字（仅单列模式生效）；空字符串表示不绘
        bool                  iconGrayed   = false;   // icon 是否灰显（仅单列模式生效）
        HBITMAP               icon         = nullptr; // legacy tree icon（col 0 文字左侧）
        LPARAM                param        = 0;
        COLORREF              statusColor  = CLR_INVALID;  // CLR_INVALID = no dot
        std::vector<Cell>     cells;                  // 多列模式 cells[col]；单列模式空
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

    // ---- custom-node mode 事件路由 helper ----
    // 给 h.visibleRow 处节点的 customControl 调 Layout(rcContent) 把 rcItem
    // 同步到当前几何,返回 ctrl 指针;未挂 custom ctrl 或行号越界返 nullptr。
    // OnXxx 事件 handler 据此把事件先转给 ctrl;ctrl 返 true 时认为消耗。
    DuiControl* LayoutCustomCtrlAtRow_(const HitInfo& h);

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
        kIconSizePx       = 18,    // 老 tree icon 默认尺寸(运行期可由 SetIconSize 覆盖,见 m_iconSizePx)
        kIconSizePxMin    = 8,     // SetIconSize 下限
        kIconSizePxMax    = 64,    // SetIconSize 上限
        kCellIconPx       = 18,    // CELL_ICON 尺寸(多列模式 CELL_ICON 类 cell,不受 SetIconSize 影响)
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

    // ---- sub-label 全局视觉（单列模式两行节点的副文本字号 / 颜色）----
    int       m_subLabelPt        = 8;                    // 副标签字号 (pt)
    COLORREF  m_clrSubLabelText   = RGB(140, 140, 140);   // 副标签文字颜色

    // ---- right-text 全局视觉（单列模式行右端辅助文字字号 / 颜色）----
    int       m_rightTextPt       = 0;                    // 右侧文字字号 (pt)，0 = 用默认字体
    COLORREF  m_clrRightText      = RGB(140, 140, 140);   // 右侧文字颜色

    // ---- 节点 icon 全局视觉(单列模式 / 多列模式 col0 的 tree icon)----
    int       m_iconSizePx          = kIconSizePx;        // icon 显示尺寸(逻辑像素,正方形)
    int       m_iconCornerRadiusPx  = 0;                  // icon 圆角半径(逻辑像素),0=直角
    bool      m_iconUsesAlpha       = false;              // icon 是否走 AlphaBlend 绘制路径(默认 false=旧 StretchBlt+HRGN clip 路径)

    // ---- 节点自绘控件(单列模式)----
    // 按节点 id 索引;value 为 tree 持有的 DuiControl。节点 Remove 时自动 erase;
    // SetItemCustomControl(id, nullptr) 也从这里清除。
    std::map<int, std::unique_ptr<DuiControl> >  m_customControls;

    // ---- 节点可见性过滤器 ----
    std::function<bool(int /*nodeId*/)>  m_filter;        // 空 = 无过滤器
    COLORREF  m_clrFocusBorder = RGB( 45, 108, 223);    // cell focus rect 主色
    COLORREF  m_clrLink        = RGB( 45, 108, 223);
};

} // namespace balloonwjui

#endif // BUI_FEATURE_TREEVIEW
