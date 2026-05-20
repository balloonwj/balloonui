#include "stdafx.h"
#include "DuiTreeView.h"

#if BUI_FEATURE_TREEVIEW

#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"
#include "../../DuiHost.h"
#include "../Window/DuiScrollBar.h"
#include "../Input/DuiEditHost.h"
#include <algorithm>
#include <gdiplus.h>

namespace balloonwjui {

#pragma comment(lib, "gdiplus.lib")

// ================================================================
// internal painted constants（语义化常量；修改要谨慎）
// ================================================================

// 默认行高最小值；防止 caller 给 0 或负数把行画没。
static const int kRowHeightFloor   = 8;
// 缩进最小值。
static const int kIndentFloor      = 1;
// header 高度最小值；低于此值字根本画不出来。
static const int kHeaderHeightFloor = 12;
// progress bar 文字两侧 padding（避免百分比文字贴边）。
static const int kProgressTextPad  = 4;
// hyperlink 单元格 hit-test 不要求精确到字符，只在 cell 矩形内任意点都算。
// CELL_ICON / CELL_IMAGE 行内顶下两端 padding（让图标不贴边）。
static const int kIconCellPadY     = 2;

// ================================================================
// ctor / dtor
// ================================================================

DuiTreeView::DuiTreeView()
{
    SetTabStop(true);
}

DuiTreeView::~DuiTreeView()
{
    // editor 是 m_children 的一员，析构链会清掉；这里只清 raw 指针。
    m_editor   = nullptr;
    m_hScroll  = nullptr;
    m_vScroll  = nullptr;
}

// ================================================================
// node helpers
// ================================================================

int DuiTreeView::IndexOf(int id) const
{
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].id == id)
        {
            return (int)i;
        }
    }
    return -1;
}

bool DuiTreeView::HasChildrenIdx(int idx) const
{
    if (idx < 0)
    {
        return false;
    }
    for (size_t i = idx + 1; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].depth <= m_nodes[idx].depth)
        {
            return false;
        }
        if (m_nodes[i].depth == m_nodes[idx].depth + 1)
        {
            return true;
        }
    }
    return false;
}

bool DuiTreeView::HasChildren(int id) const
{
    return HasChildrenIdx(IndexOf(id));
}

// ================================================================
// structure ops
// ================================================================

int DuiTreeView::AddRoot(LPCTSTR label, HBITMAP icon, LPARAM param)
{
    Node n;
    n.id          = m_nextId++;
    n.parentIdx   = -1;
    n.depth       = 0;
    n.expanded    = true;
    n.label       = label ? label : _T("");
    n.icon        = icon;
    n.param       = param;
    n.statusColor = CLR_INVALID;
    m_nodes.push_back(n);
    SyncLegacyToCell0((int)m_nodes.size() - 1);
    RebuildVisible();
    Invalidate();
    return n.id;
}

int DuiTreeView::AddChild(int parentId, LPCTSTR label, HBITMAP icon, LPARAM param)
{
    int parentIdx = IndexOf(parentId);
    if (parentIdx < 0)
    {
        return -1;
    }

    int parentDepth = m_nodes[parentIdx].depth;
    size_t insertAt = parentIdx + 1;
    while (insertAt < m_nodes.size() && m_nodes[insertAt].depth > parentDepth)
    {
        ++insertAt;
    }

    Node n;
    n.id          = m_nextId++;
    n.parentIdx   = parentIdx;
    n.depth       = parentDepth + 1;
    n.expanded    = true;
    n.label       = label ? label : _T("");
    n.icon        = icon;
    n.param       = param;
    n.statusColor = CLR_INVALID;
    m_nodes.insert(m_nodes.begin() + insertAt, n);

    // Insert shifts indices >= insertAt; fix up parentIdx values.
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if ((int)i == (int)insertAt)
        {
            continue;
        }
        if (m_nodes[i].parentIdx >= (int)insertAt)
        {
            m_nodes[i].parentIdx += 1;
        }
    }
    SyncLegacyToCell0((int)insertAt);
    RebuildVisible();
    Invalidate();
    return n.id;
}

void DuiTreeView::RemoveSubtree(int idx)
{
    if (idx < 0 || idx >= (int)m_nodes.size())
    {
        return;
    }
    int rootDepth = m_nodes[idx].depth;
    size_t end = idx + 1;
    while (end < m_nodes.size() && m_nodes[end].depth > rootDepth)
    {
        ++end;
    }
    int removed = (int)(end - idx);
    bool removedSelOnce = false;

    for (size_t k = idx; k < end; ++k)
    {
        if (m_nodes[k].id == m_curSelId)
        {
            removedSelOnce = true;
            break;
        }
    }

    // Drop selected cells whose itemId is inside removal range.
    for (auto it = m_selCells.begin(); it != m_selCells.end(); )
    {
        bool drop = false;
        for (size_t k = idx; k < end; ++k)
        {
            if (m_nodes[k].id == it->itemId)
            {
                drop = true;
                break;
            }
        }
        if (drop)
        {
            it = m_selCells.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_nodes.erase(m_nodes.begin() + idx, m_nodes.begin() + end);

    for (auto& n : m_nodes)
    {
        if (n.parentIdx >= (int)end)
        {
            n.parentIdx -= removed;
        }
        else if (n.parentIdx >= idx)
        {
            n.parentIdx = -1;
        }
    }

    if (removedSelOnce)
    {
        m_curSelId = -1;
    }
}

void DuiTreeView::Remove(int id)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    if (m_editId == id)
    {
        CancelEdit();
    }
    RemoveSubtree(idx);
    RebuildVisible();
    Invalidate();
}

void DuiTreeView::Clear()
{
    CancelEdit();
    m_nodes.clear();
    m_visible.clear();
    m_selCells.clear();
    m_curSelId   = -1;
    m_hoverId    = -1;
    m_focusCell  = CellRef{ -1, -1 };
    Invalidate();
}

int DuiTreeView::GetRootCount() const
{
    int n = 0;
    for (auto& nd : m_nodes)
    {
        if (nd.depth == 0)
        {
            ++n;
        }
    }
    return n;
}

int DuiTreeView::GetChildCount(int parentId) const
{
    int parentIdx = IndexOf(parentId);
    if (parentIdx < 0)
    {
        return 0;
    }
    int parentDepth = m_nodes[parentIdx].depth;
    int n = 0;
    for (size_t i = parentIdx + 1; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].depth <= parentDepth)
        {
            break;
        }
        if (m_nodes[i].depth == parentDepth + 1)
        {
            ++n;
        }
    }
    return n;
}

// ================================================================
// expand / collapse
// ================================================================

void DuiTreeView::SetExpanded(int id, bool exp)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    if (m_nodes[idx].expanded == exp)
    {
        return;
    }
    m_nodes[idx].expanded = exp;
    RebuildVisible();
    EnsureScrollRanges();
    Invalidate();
    NotifyParent((UINT)DUITVN_EXPAND_TOGGLED, (LPARAM)id);
}

bool DuiTreeView::IsExpanded(int id) const
{
    int idx = IndexOf(id);
    return idx >= 0 && m_nodes[idx].expanded;
}

void DuiTreeView::ExpandAll()
{
    for (auto& n : m_nodes)
    {
        n.expanded = true;
    }
    RebuildVisible();
    EnsureScrollRanges();
    Invalidate();
}

void DuiTreeView::CollapseAll()
{
    for (auto& n : m_nodes)
    {
        n.expanded = false;
    }
    RebuildVisible();
    EnsureScrollRanges();
    Invalidate();
}

// ================================================================
// selection
// ================================================================

void DuiTreeView::SetCurSel(int id, bool notify)
{
    if (id != -1 && IndexOf(id) < 0)
    {
        return;
    }
    if (m_curSelId == id)
    {
        return;
    }
    m_curSelId = id;
    if (!IsMultiCol())
    {
        m_selCells.clear();
    }
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)id);
    }
}

DuiTreeView::CellRef DuiTreeView::GetCurSelCell() const
{
    if (!IsMultiCol())
    {
        return CellRef{ m_curSelId, m_curSelId == -1 ? -1 : 0 };
    }
    return m_focusCell;
}

void DuiTreeView::SetCurSelCell(int itemId, int col, bool notify)
{
    if (!IsMultiCol())
    {
        SetCurSel(itemId, notify);
        return;
    }
    if (itemId == -1 || IndexOf(itemId) < 0)
    {
        m_focusCell = CellRef{ -1, -1 };
        m_selCells.clear();
        m_curSelId  = -1;
        Invalidate();
        if (notify)
        {
            NotifyParent(DUIN_VALUECHANGED, (LPARAM)-1);
        }
        return;
    }
    if (col < 0 || col >= (int)m_columns.size())
    {
        col = 0;
    }
    m_focusCell = CellRef{ itemId, col };
    m_selCells.clear();
    m_selCells.push_back(m_focusCell);
    m_curSelId  = itemId;
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)itemId);
    }
}

int DuiTreeView::GetSelectedCellCount() const
{
    if (!IsMultiCol())
    {
        return m_curSelId == -1 ? 0 : 1;
    }
    return (int)m_selCells.size();
}

DuiTreeView::CellRef DuiTreeView::GetSelectedCell(int n) const
{
    if (!IsMultiCol())
    {
        if (n != 0 || m_curSelId == -1)
        {
            return CellRef{ -1, -1 };
        }
        return CellRef{ m_curSelId, 0 };
    }
    if (n < 0 || n >= (int)m_selCells.size())
    {
        return CellRef{ -1, -1 };
    }
    return m_selCells[n];
}

void DuiTreeView::ClearSelection()
{
    m_curSelId   = -1;
    m_focusCell  = CellRef{ -1, -1 };
    m_selCells.clear();
    Invalidate();
}

// ================================================================
// per-item state (legacy)
// ================================================================

void DuiTreeView::SetItemLabel(int id, LPCTSTR label)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    m_nodes[idx].label = label ? label : _T("");
    SyncLegacyToCell0(idx);
    Invalidate();
}

CString DuiTreeView::GetItemLabel(int id) const
{
    int idx = IndexOf(id);
    return idx < 0 ? CString() : m_nodes[idx].label;
}

void DuiTreeView::SetItemIcon(int id, HBITMAP icon)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    m_nodes[idx].icon = icon;
    Invalidate();
}

HBITMAP DuiTreeView::GetItemIcon(int id) const
{
    int idx = IndexOf(id);
    return idx < 0 ? nullptr : m_nodes[idx].icon;
}

void DuiTreeView::SetItemParam(int id, LPARAM param)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    m_nodes[idx].param = param;
}

LPARAM DuiTreeView::GetItemParam(int id) const
{
    int idx = IndexOf(id);
    return idx < 0 ? 0 : m_nodes[idx].param;
}

void DuiTreeView::SetItemStatusColor(int id, COLORREF color)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    m_nodes[idx].statusColor = color;
    Invalidate();
}

COLORREF DuiTreeView::GetItemStatusColor(int id) const
{
    int idx = IndexOf(id);
    return idx < 0 ? (COLORREF)CLR_INVALID : m_nodes[idx].statusColor;
}

// ================================================================
// columns
// ================================================================

int DuiTreeView::GetColumnCount() const
{
    return (int)m_columns.size();
}

int DuiTreeView::AddColumn(LPCTSTR title, int width, int minWidth, UINT textAlign, bool sortable)
{
    if (width  < kColMinWidthDefault) { width = kColMinWidthDefault; }
    if (minWidth < kColMinWidthDefault) { minWidth = kColMinWidthDefault; }
    if (width < minWidth) { width = minWidth; }

    ColumnDef c;
    c.title     = title ? title : _T("");
    c.width     = width;
    c.minWidth  = minWidth;
    c.textAlign = textAlign;
    c.sortable  = sortable;
    c.editable  = true;
    m_columns.push_back(c);
    EnsureScrollRanges();
    Invalidate();
    return (int)m_columns.size() - 1;
}

void DuiTreeView::ClearColumns()
{
    CancelEdit();
    m_columns.clear();
    m_frozenCols = 0;
    m_frozenRows = 0;
    m_sortCol    = -1;
    m_sortDir    =  0;
    m_selCells.clear();
    m_focusCell  = CellRef{ -1, -1 };
    m_scrollX = m_scrollY = 0;
    if (m_hScroll)
    {
        DuiControl::RemoveChild(m_hScroll);
        m_hScroll = nullptr;
    }
    if (m_vScroll)
    {
        DuiControl::RemoveChild(m_vScroll);
        m_vScroll = nullptr;
    }
    Invalidate();
}

void DuiTreeView::SetColumnTitle(int col, LPCTSTR title)
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return;
    }
    m_columns[col].title = title ? title : _T("");
    Invalidate();
}

CString DuiTreeView::GetColumnTitle(int col) const
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return CString();
    }
    return m_columns[col].title;
}

void DuiTreeView::SetColumnWidth(int col, int px)
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return;
    }
    if (px < m_columns[col].minWidth)
    {
        px = m_columns[col].minWidth;
    }
    if (m_columns[col].width == px)
    {
        return;
    }
    m_columns[col].width = px;
    EnsureScrollRanges();
    Invalidate();
}

int DuiTreeView::GetColumnWidth(int col) const
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return 0;
    }
    return m_columns[col].width;
}

void DuiTreeView::SetColumnMinWidth(int col, int px)
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return;
    }
    if (px < kColMinWidthDefault) { px = kColMinWidthDefault; }
    m_columns[col].minWidth = px;
    if (m_columns[col].width < px)
    {
        m_columns[col].width = px;
        EnsureScrollRanges();
    }
    Invalidate();
}

int DuiTreeView::GetColumnMinWidth(int col) const
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return 0;
    }
    return m_columns[col].minWidth;
}

void DuiTreeView::SetColumnTextAlign(int col, UINT dt)
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return;
    }
    m_columns[col].textAlign = dt;
    Invalidate();
}

UINT DuiTreeView::GetColumnTextAlign(int col) const
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return DT_LEFT;
    }
    return m_columns[col].textAlign;
}

void DuiTreeView::SetColumnSortable(int col, bool b)
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return;
    }
    m_columns[col].sortable = b;
}

bool DuiTreeView::IsColumnSortable(int col) const
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return false;
    }
    return m_columns[col].sortable;
}

void DuiTreeView::SetSortIndicator(int col, int dir)
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        m_sortCol = -1;
        m_sortDir = 0;
    }
    else
    {
        m_sortCol = col;
        m_sortDir = (dir > 0) ? 1 : (dir < 0 ? -1 : 0);
    }
    Invalidate();
}

// ================================================================
// frozen panes
// ================================================================

void DuiTreeView::SetFrozenColumns(int n)
{
    if (n < 0) { n = 0; }
    if (n > (int)m_columns.size()) { n = (int)m_columns.size(); }
    m_frozenCols = n;
    Invalidate();
}

void DuiTreeView::SetFrozenRows(int n)
{
    if (n < 0) { n = 0; }
    m_frozenRows = n;
    Invalidate();
}

// ================================================================
// cell content
// ================================================================

DuiTreeView::Cell& DuiTreeView::EnsureCell(int idx, int col)
{
    Node& n = m_nodes[idx];
    while ((int)n.cells.size() <= col)
    {
        n.cells.push_back(Cell{});
    }
    return n.cells[col];
}

const DuiTreeView::Cell* DuiTreeView::TryGetCell(int idx, int col) const
{
    if (idx < 0 || idx >= (int)m_nodes.size())
    {
        return nullptr;
    }
    if (col < 0 || col >= (int)m_nodes[idx].cells.size())
    {
        return nullptr;
    }
    return &m_nodes[idx].cells[col];
}

void DuiTreeView::SyncLegacyToCell0(int idx)
{
    if (idx < 0 || idx >= (int)m_nodes.size())
    {
        return;
    }
    if (m_nodes[idx].cells.empty())
    {
        m_nodes[idx].cells.push_back(Cell{});
    }
    Cell& c0 = m_nodes[idx].cells[0];
    if (c0.type == CELL_TEXT)
    {
        c0.text = m_nodes[idx].label;
    }
}

void DuiTreeView::SyncCell0ToLegacy(int idx)
{
    if (idx < 0 || idx >= (int)m_nodes.size())
    {
        return;
    }
    if (m_nodes[idx].cells.empty())
    {
        return;
    }
    const Cell& c0 = m_nodes[idx].cells[0];
    if (c0.type == CELL_TEXT)
    {
        m_nodes[idx].label = c0.text;
    }
}

void DuiTreeView::SetCellType(int id, int col, CellType t)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    Cell& c = EnsureCell(idx, col);
    c.type = t;
    if (col == 0)
    {
        SyncCell0ToLegacy(idx);
    }
    Invalidate();
}

DuiTreeView::CellType DuiTreeView::GetCellType(int id, int col) const
{
    const Cell* c = TryGetCell(IndexOf(id), col);
    return c ? c->type : CELL_TEXT;
}

void DuiTreeView::SetCellText(int id, int col, LPCTSTR text)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    Cell& c = EnsureCell(idx, col);
    c.text = text ? text : _T("");
    if (col == 0)
    {
        m_nodes[idx].label = c.text;
    }
    Invalidate();
}

CString DuiTreeView::GetCellText(int id, int col) const
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return CString();
    }
    if (col == 0)
    {
        return m_nodes[idx].label;
    }
    const Cell* c = TryGetCell(idx, col);
    return c ? c->text : CString();
}

void DuiTreeView::SetCellIcon(int id, int col, HBITMAP icon)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    Cell& c = EnsureCell(idx, col);
    c.type   = CELL_ICON;
    c.bitmap = icon;
    Invalidate();
}

void DuiTreeView::SetCellImage(int id, int col, HBITMAP image)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    Cell& c = EnsureCell(idx, col);
    c.type   = CELL_IMAGE;
    c.bitmap = image;
    Invalidate();
}

HBITMAP DuiTreeView::GetCellBitmap(int id, int col) const
{
    const Cell* c = TryGetCell(IndexOf(id), col);
    return c ? c->bitmap : nullptr;
}

void DuiTreeView::SetCellChecked(int id, int col, bool checked)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    Cell& c = EnsureCell(idx, col);
    c.type    = CELL_CHECKBOX;
    c.checked = checked;
    Invalidate();
}

bool DuiTreeView::IsCellChecked(int id, int col) const
{
    const Cell* c = TryGetCell(IndexOf(id), col);
    return c ? c->checked : false;
}

void DuiTreeView::SetCellValue(int id, int col, int value)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    if (value < 0)   { value = 0;   }
    if (value > 100) { value = 100; }
    Cell& c = EnsureCell(idx, col);
    c.type  = CELL_PROGRESS;
    c.value = value;
    Invalidate();
}

int DuiTreeView::GetCellValue(int id, int col) const
{
    const Cell* c = TryGetCell(IndexOf(id), col);
    return c ? c->value : 0;
}

void DuiTreeView::SetCellLink(int id, int col, LPCTSTR display, LPCTSTR url)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    Cell& c = EnsureCell(idx, col);
    c.type = CELL_HYPERLINK;
    c.text = display ? display : _T("");
    c.url  = url     ? url     : _T("");
    if (col == 0)
    {
        m_nodes[idx].label = c.text;
    }
    Invalidate();
}

CString DuiTreeView::GetCellLinkUrl(int id, int col) const
{
    const Cell* c = TryGetCell(IndexOf(id), col);
    return c ? c->url : CString();
}

// ================================================================
// editing
// ================================================================

void DuiTreeView::SetEditable(bool b)
{
    if (m_editable == b)
    {
        return;
    }
    m_editable = b;
    if (!b)
    {
        CancelEdit();
    }
}

void DuiTreeView::SetColumnEditable(int col, bool b)
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return;
    }
    m_columns[col].editable = b;
    if (!b && m_editCol == col)
    {
        CancelEdit();
    }
}

bool DuiTreeView::IsColumnEditable(int col) const
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return false;
    }
    return m_columns[col].editable;
}

bool DuiTreeView::BeginEdit(int id, int col)
{
    if (!m_editable)
    {
        return false;
    }
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return false;
    }
    if (IsMultiCol())
    {
        if (col < 0 || col >= (int)m_columns.size())
        {
            return false;
        }
        if (!m_columns[col].editable)
        {
            return false;
        }
    }
    else
    {
        col = 0;
    }
    CellType t = GetCellType(id, col);
    if (t != CELL_TEXT)
    {
        return false;
    }
    if (m_editor)
    {
        CommitEdit();
    }
    auto editor = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    editor->SetText(GetCellText(id, col));
    DuiEditHost* raw = editor.get();
    DuiControl::AddChild(std::move(editor));
    m_editor  = raw;
    m_editId  = id;
    m_editCol = col;
    PlaceEditor();
    if (m_pHost)
    {
        raw->EnsureCreated(m_pHost->m_hWnd);
        raw->SetFocus();
    }
    return true;
}

void DuiTreeView::CancelEdit()
{
    if (!m_editor)
    {
        return;
    }
    DuiControl::RemoveChild(m_editor);
    m_editor  = nullptr;
    m_editId  = -1;
    m_editCol = -1;
    Invalidate();
}

void DuiTreeView::CommitEdit()
{
    if (!m_editor)
    {
        return;
    }
    CString text = m_editor->GetText();
    int id  = m_editId;
    int col = m_editCol;
    DuiControl::RemoveChild(m_editor);
    m_editor  = nullptr;
    m_editId  = -1;
    m_editCol = -1;
    SetCellText(id, col, text);
    DuiTreeCellNotify n{};
    n.itemId = id;
    n.col    = col;
    n.text   = text;
    NotifyParent((UINT)DUITVN_CELLEDITED, (LPARAM)&n);
    Invalidate();
}

void DuiTreeView::PlaceEditor()
{
    if (!m_editor || m_editId < 0)
    {
        return;
    }
    int row = GetVisibleRow(m_editId);
    if (row < 0)
    {
        CancelEdit();
        return;
    }

    int yTop;
    if (row < m_frozenRows || !IsMultiCol())
    {
        yTop = m_rcItem.top + (IsMultiCol() ? m_headerH : 0) + row * m_rowH;
    }
    else
    {
        yTop = m_rcItem.top + m_headerH + row * m_rowH - m_scrollY;
    }

    int xLeft;
    int xRight;
    if (!IsMultiCol())
    {
        xLeft  = m_rcItem.left;
        xRight = m_rcItem.right;
    }
    else
    {
        int colLeft  = ColumnLeftAbs(m_editCol);
        int colRight = ColumnRightAbs(m_editCol);
        if (m_editCol < m_frozenCols)
        {
            xLeft  = m_rcItem.left + colLeft;
            xRight = m_rcItem.left + colRight;
        }
        else
        {
            xLeft  = m_rcItem.left + colLeft  - m_scrollX;
            xRight = m_rcItem.left + colRight - m_scrollX;
        }
    }

    RECT body = BodyRect();
    RECT rc;
    rc.left   = xLeft  + 1;
    rc.top    = yTop   + 1;
    rc.right  = xRight - 1;
    rc.bottom = yTop + m_rowH - 1;
    // If editor is fully outside the body view (scrolled away), cancel.
    if (rc.right <= body.left || rc.left >= body.right
        || rc.bottom <= body.top || rc.top >= body.bottom)
    {
        CancelEdit();
        return;
    }
    if (rc.left   < body.left)   { rc.left   = body.left;   }
    if (rc.top    < body.top)    { rc.top    = body.top;    }
    if (rc.right  > body.right)  { rc.right  = body.right;  }
    if (rc.bottom > body.bottom) { rc.bottom = body.bottom; }
    m_editor->SetRect(rc);
}

// ================================================================
// visible queries
// ================================================================

void DuiTreeView::RebuildVisible()
{
    m_visible.clear();
    int hideUntilDepth = -1;
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        const Node& n = m_nodes[i];
        if (hideUntilDepth >= 0 && n.depth > hideUntilDepth)
        {
            continue;
        }
        hideUntilDepth = -1;
        m_visible.push_back((int)i);
        if (!n.expanded)
        {
            hideUntilDepth = n.depth;
        }
    }
}

int DuiTreeView::GetVisibleCount() const
{
    return (int)m_visible.size();
}

int DuiTreeView::GetContentHeight() const
{
    return (int)m_visible.size() * m_rowH;
}

int DuiTreeView::GetContentWidth() const
{
    return ComputeContentWidth();
}

int DuiTreeView::GetVisibleRow(int id) const
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return -1;
    }
    for (size_t r = 0; r < m_visible.size(); ++r)
    {
        if (m_visible[r] == idx)
        {
            return (int)r;
        }
    }
    return -1;
}

int DuiTreeView::GetIdAtVisibleRow(int n) const
{
    if (n < 0 || n >= (int)m_visible.size())
    {
        return -1;
    }
    return m_nodes[m_visible[n]].id;
}

// ================================================================
// geometry
// ================================================================

int DuiTreeView::ComputeContentWidth() const
{
    int w = 0;
    for (auto& c : m_columns)
    {
        w += c.width;
    }
    return w;
}

int DuiTreeView::ColumnLeftAbs(int col) const
{
    int x = 0;
    int n = (int)m_columns.size();
    if (col > n) { col = n; }
    for (int i = 0; i < col; ++i)
    {
        x += m_columns[i].width;
    }
    return x;
}

int DuiTreeView::ColumnRightAbs(int col) const
{
    if (col < 0 || col >= (int)m_columns.size())
    {
        return ColumnLeftAbs(col);
    }
    return ColumnLeftAbs(col) + m_columns[col].width;
}

RECT DuiTreeView::HeaderRect() const
{
    RECT r{};
    if (!IsMultiCol())
    {
        return r;
    }
    r.left   = m_rcItem.left;
    r.right  = m_rcItem.right;
    r.top    = m_rcItem.top;
    r.bottom = m_rcItem.top + m_headerH;
    return r;
}

RECT DuiTreeView::BodyRect() const
{
    RECT r = m_rcItem;
    if (IsMultiCol())
    {
        // 两个轴都按需预留 gutter。EnsureScrollRanges 每次 layout 跑 2
        // 阶段算法刷新 m_needHScroll / m_needVScroll；BodyRect 这里只读
        // 它们决定 right / bottom 是否收。
        r.top    += m_headerH;
        if (m_needVScroll)
        {
            r.right -= kScrollBarPx;
        }
        if (m_needHScroll)
        {
            r.bottom -= kScrollBarPx;
        }
    }
    return r;
}

RECT DuiTreeView::RowRect(int visibleRow) const
{
    RECT r;
    r.left   = 0;
    r.right  = IsMultiCol() ? ComputeContentWidth() : (m_rcItem.right - m_rcItem.left);
    r.top    = visibleRow * m_rowH;
    r.bottom = r.top + m_rowH;
    return r;
}

RECT DuiTreeView::GlyphRectOf(int visibleRow, int depth) const
{
    RECT row = RowRect(visibleRow);
    int xStart = row.left + depth * m_indent;
    RECT g;
    g.left   = xStart;
    g.right  = xStart + kGlyphStripPx;
    int cy   = (row.top + row.bottom) / 2;
    g.top    = cy - kGlyphStripPx / 2;
    g.bottom = cy + kGlyphStripPx / 2;
    return g;
}

RECT DuiTreeView::CellRectInRow(const RECT& rowRc, int col) const
{
    RECT r = rowRc;
    if (!IsMultiCol() || col < 0 || col >= (int)m_columns.size())
    {
        return r;
    }
    int x = 0;
    for (int i = 0; i < col; ++i)
    {
        x += m_columns[i].width;
    }
    r.left  = rowRc.left + x;
    r.right = r.left + m_columns[col].width;
    return r;
}

int DuiTreeView::FrozenColsRightX() const
{
    int x = 0;
    int n = (m_frozenCols < (int)m_columns.size()) ? m_frozenCols : (int)m_columns.size();
    for (int i = 0; i < n; ++i)
    {
        x += m_columns[i].width;
    }
    return x;
}

int DuiTreeView::FrozenRowsBottomY() const
{
    int n = m_frozenRows;
    if (n > (int)m_visible.size()) { n = (int)m_visible.size(); }
    return n * m_rowH;
}

void DuiTreeView::ComputeQuadrants(RECT& rcTL, RECT& rcTR, RECT& rcBL, RECT& rcBR) const
{
    RECT body = BodyRect();
    int splitX = body.left + FrozenColsRightX();
    int splitY = body.top  + FrozenRowsBottomY();
    if (splitX > body.right)  { splitX = body.right; }
    if (splitY > body.bottom) { splitY = body.bottom; }
    rcTL = RECT{ body.left, body.top, splitX, splitY };
    rcTR = RECT{ splitX,    body.top, body.right, splitY };
    rcBL = RECT{ body.left, splitY, splitX, body.bottom };
    rcBR = RECT{ splitX,    splitY, body.right, body.bottom };
}

// ================================================================
// scrollbar setup
// ================================================================

// C-style trampolines from DuiScrollBar OnScrollFn back into the tree.
static void DuiTreeView_OnHScroll(void* user, int newPos)
{
    static_cast<DuiTreeView*>(user)->ScrollPosX(newPos);
}
static void DuiTreeView_OnVScroll(void* user, int newPos)
{
    static_cast<DuiTreeView*>(user)->ScrollPosY(newPos);
}

void DuiTreeView::EnsureScrollRanges()
{
    if (!IsMultiCol())
    {
        return;
    }
    if (!m_hScroll)
    {
        auto h = std::unique_ptr<DuiScrollBar>(new DuiScrollBar(/*horizontal=*/true));
        h->SetLineSize(m_rowH);
        h->SetOnScroll(&DuiTreeView_OnHScroll, this);
        m_hScroll = h.get();
        DuiControl::AddChild(std::move(h));
    }
    if (!m_vScroll)
    {
        auto v = std::unique_ptr<DuiScrollBar>(new DuiScrollBar(/*horizontal=*/false));
        v->SetLineSize(m_rowH);
        v->SetOnScroll(&DuiTreeView_OnVScroll, this);
        m_vScroll = v.get();
        DuiControl::AddChild(std::move(v));
    }
    // 2 阶段 layout：H 和 V 互相依赖（needV 受 needH 收的 viewH 影响、反
    // 之亦然）。从"都不需要"假设迭代两轮就稳定（最坏从 false/false 升到
    // true/true，第二轮用最终 needH/V 重算 view 即收敛）。
    int contentW = ComputeContentWidth();
    int contentH = (int)m_visible.size() * m_rowH;
    int fullW = m_rcItem.right - m_rcItem.left;
    int fullH = m_rcItem.bottom - m_rcItem.top - m_headerH;

    bool needH = false;
    bool needV = false;
    for (int pass = 0; pass < 2; ++pass)
    {
        int viewW = fullW - (needV ? kScrollBarPx : 0);
        int viewH = fullH - (needH ? kScrollBarPx : 0);
        if (viewW < 1) { viewW = 1; }
        if (viewH < 1) { viewH = 1; }
        needH = (contentW > viewW);
        needV = (contentH > viewH);
    }
    m_needHScroll = needH;
    m_needVScroll = needV;

    // 用最终 BodyRect 算 scroll 上下限。
    RECT body = BodyRect();
    int viewW = body.right - body.left;
    int viewH = body.bottom - body.top;

    int hMax = contentW > viewW ? contentW - viewW : 0;
    int vMax = contentH > viewH ? contentH - viewH : 0;
    DuiScrollBar* hb = static_cast<DuiScrollBar*>(m_hScroll);
    DuiScrollBar* vb = static_cast<DuiScrollBar*>(m_vScroll);
    hb->SetRange(0, hMax);
    hb->SetPage(viewW > 0 ? viewW : 1);
    vb->SetRange(0, vMax);
    vb->SetPage(viewH > 0 ? viewH : 1);

    if (m_scrollX > hMax) { m_scrollX = hMax; }
    if (m_scrollY > vMax) { m_scrollY = vMax; }
    hb->SetPos(m_scrollX, /*notify=*/false);
    vb->SetPos(m_scrollY, /*notify=*/false);

    m_hScroll->SetVisible(m_needHScroll);
    m_vScroll->SetVisible(m_needVScroll);
}

void DuiTreeView::ScrollX(int dx)
{
    ScrollPosX(m_scrollX + dx);
}

void DuiTreeView::ScrollY(int dy)
{
    ScrollPosY(m_scrollY + dy);
}

void DuiTreeView::ScrollPosX(int x)
{
    int hMax = 0;
    if (m_hScroll) { hMax = static_cast<DuiScrollBar*>(m_hScroll)->GetMax(); }
    if (x < 0) { x = 0; }
    if (x > hMax) { x = hMax; }
    if (m_scrollX == x)
    {
        return;
    }
    m_scrollX = x;
    if (m_hScroll)
    {
        static_cast<DuiScrollBar*>(m_hScroll)->SetPos(x, /*notify=*/false);
    }
    PlaceEditor();
    Invalidate();
}

void DuiTreeView::ScrollPosY(int y)
{
    int vMax = 0;
    if (m_vScroll) { vMax = static_cast<DuiScrollBar*>(m_vScroll)->GetMax(); }
    if (y < 0) { y = 0; }
    if (y > vMax) { y = vMax; }
    if (m_scrollY == y)
    {
        return;
    }
    m_scrollY = y;
    if (m_vScroll)
    {
        static_cast<DuiScrollBar*>(m_vScroll)->SetPos(y, /*notify=*/false);
    }
    PlaceEditor();
    Invalidate();
}

// ================================================================
// hit test
// ================================================================

DuiTreeView::HitInfo DuiTreeView::HitTest_(POINT pt) const
{
    HitInfo h;
    if (!::PtInRect(&m_rcItem, pt))
    {
        return h;
    }

    if (IsMultiCol())
    {
        // Header
        RECT rcHeader = HeaderRect();
        if (::PtInRect(&rcHeader, pt))
        {
            // Find which column.
            int x = pt.x - rcHeader.left;
            // Frozen cols are at fixed left; non-frozen cols offset by -m_scrollX.
            int frozenRightX = FrozenColsRightX();
            int absX;
            if (x < frozenRightX)
            {
                absX = x;
            }
            else
            {
                absX = x + m_scrollX;
            }
            int cur = 0;
            for (int i = 0; i < (int)m_columns.size(); ++i)
            {
                int colLeft  = cur;
                int colRight = cur + m_columns[i].width;
                cur = colRight;
                if (absX >= colLeft && absX < colRight)
                {
                    h.col = i;
                    int distRight = colRight - absX;
                    if (distRight <= kHeaderSepHotPx && i < (int)m_columns.size() - 1)
                    {
                        h.zone = HZ_HEADER_SEP;
                    }
                    else
                    {
                        h.zone = HZ_HEADER_TEXT;
                    }
                    return h;
                }
            }
            // Past last column → blank header area.
            h.zone = HZ_NONE;
            return h;
        }

        // Body
        RECT rcBody = BodyRect();
        if (::PtInRect(&rcBody, pt))
        {
            int yInBody = pt.y - rcBody.top;
            int xInBody = pt.x - rcBody.left;

            // Determine row (frozen rows: no v-scroll; others: + m_scrollY).
            int frozenBottomY = FrozenRowsBottomY();
            int absRowY;
            if (yInBody < frozenBottomY)
            {
                absRowY = yInBody;
            }
            else
            {
                absRowY = yInBody + m_scrollY;
            }
            int row = absRowY / m_rowH;
            if (row < 0 || row >= (int)m_visible.size())
            {
                h.zone = HZ_BODY_BLANK;
                return h;
            }
            h.visibleRow = row;
            h.itemId     = m_nodes[m_visible[row]].id;

            // Determine column.
            int frozenRightX = FrozenColsRightX();
            int absColX;
            if (xInBody < frozenRightX)
            {
                absColX = xInBody;
            }
            else
            {
                absColX = xInBody + m_scrollX;
            }
            int cur = 0;
            for (int i = 0; i < (int)m_columns.size(); ++i)
            {
                int colLeft  = cur;
                int colRight = cur + m_columns[i].width;
                cur = colRight;
                if (absColX >= colLeft && absColX < colRight)
                {
                    h.col = i;
                    break;
                }
            }
            if (h.col < 0)
            {
                h.zone = HZ_BODY_BLANK;
                return h;
            }
            // col 0: glyph hit?
            if (h.col == 0 && HasChildrenIdx(m_visible[row]))
            {
                int depth = m_nodes[m_visible[row]].depth;
                int gxStart = depth * m_indent;
                int gxEnd   = gxStart + kGlyphStripPx;
                if (absColX >= gxStart && absColX < gxEnd)
                {
                    h.zone = HZ_BODY_GLYPH;
                    return h;
                }
            }
            h.zone = HZ_BODY_CELL;
            return h;
        }
        return h;
    }

    // Single-col mode (legacy)
    if (m_visible.empty())
    {
        return h;
    }
    int row = (pt.y - m_rcItem.top) / m_rowH;
    if (row < 0 || row >= (int)m_visible.size())
    {
        return h;
    }
    h.visibleRow = row;
    h.itemId     = m_nodes[m_visible[row]].id;
    int idx = m_visible[row];
    if (HasChildrenIdx(idx))
    {
        int depth = m_nodes[idx].depth;
        int gxStart = m_rcItem.left + depth * m_indent;
        int gxEnd   = gxStart + kGlyphStripPx;
        if (pt.x >= gxStart && pt.x < gxEnd)
        {
            h.zone = HZ_BODY_GLYPH;
            return h;
        }
    }
    h.zone = HZ_BODY_CELL;
    h.col  = 0;
    return h;
}

int DuiTreeView::HitTestId(POINT pt) const
{
    HitInfo h = HitTest_(pt);
    return h.itemId;
}

bool DuiTreeView::HitTestCell(POINT pt, int& outId, int& outCol) const
{
    HitInfo h = HitTest_(pt);
    if (h.zone == HZ_BODY_CELL || h.zone == HZ_BODY_GLYPH)
    {
        outId  = h.itemId;
        outCol = h.col;
        return true;
    }
    outId  = -1;
    outCol = -1;
    return false;
}

// ================================================================
// layout
// ================================================================

void DuiTreeView::Layout(const RECT& rcAvail)
{
    SetRect(rcAvail);
    EnsureScrollRanges();
    if (IsMultiCol() && m_hScroll && m_vScroll)
    {
        // Always reserve scrollbar gutters; bottom-right corner is the
        // standard "dead" square between the two bars.
        RECT rh;
        rh.left   = m_rcItem.left;
        rh.right  = m_rcItem.right - kScrollBarPx;
        rh.bottom = m_rcItem.bottom;
        rh.top    = rh.bottom - kScrollBarPx;
        RECT rv;
        rv.top    = m_rcItem.top + m_headerH;
        rv.bottom = m_rcItem.bottom - kScrollBarPx;
        rv.right  = m_rcItem.right;
        rv.left   = rv.right - kScrollBarPx;
        m_hScroll->SetRect(rh);
        m_vScroll->SetRect(rv);
    }
    PlaceEditor();
}

// ================================================================
// paint
// ================================================================

static inline void FillSolidRect_(HDC hdc, const RECT& rc, COLORREF c)
{
    HBRUSH hbr = ::CreateSolidBrush(c);
    ::FillRect(hdc, &rc, hbr);
    ::DeleteObject(hbr);
}

static inline void FrameSolidRect_(HDC hdc, const RECT& rc, COLORREF c)
{
    HBRUSH hbr = ::CreateSolidBrush(c);
    ::FrameRect(hdc, &rc, hbr);
    ::DeleteObject(hbr);
}

void DuiTreeView::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible || (m_visible.empty() && m_columns.empty()))
    {
        // Background only.
        FillSolidRect_(hdc, m_rcItem, m_clrRowBg);
        return;
    }

    if (!IsMultiCol())
    {
        // Single-col path: same behavior as legacy.
        if (m_visible.empty())
        {
            return;
        }
        int firstRow = (rcDirty.top - m_rcItem.top) / m_rowH;
        if (firstRow < 0) { firstRow = 0; }
        int lastRow  = (rcDirty.bottom - m_rcItem.top + m_rowH - 1) / m_rowH;
        if (lastRow > (int)m_visible.size()) { lastRow = (int)m_visible.size(); }

        for (int r = firstRow; r < lastRow; ++r)
        {
            // Draw row background + content (legacy).
            int idx = m_visible[r];
            const Node& n = m_nodes[idx];
            RECT row;
            row.left   = m_rcItem.left;
            row.right  = m_rcItem.right;
            row.top    = m_rcItem.top + r * m_rowH;
            row.bottom = row.top + m_rowH;

            bool selected = (n.id == m_curSelId);
            bool hovered  = (n.id == m_hoverId) && !selected;
            COLORREF bg = m_clrRowBg;
            if (m_zebra && (r & 1) == 1 && !selected) { bg = m_clrZebra; }
            if (selected) { bg = m_clrRowSel; }
            else if (hovered) { bg = m_clrRowHover; }
            FillSolidRect_(hdc, row, bg);

            int xCursor = row.left + n.depth * m_indent;
            if (HasChildrenIdx(idx))
            {
                RECT g;
                g.left   = xCursor;
                g.right  = xCursor + kGlyphStripPx;
                int cy   = (row.top + row.bottom) / 2;
                g.top    = cy - kGlyphStripPx / 2;
                g.bottom = cy + kGlyphStripPx / 2;
                PaintGlyph(hdc, g, n.expanded);
            }
            xCursor += kGlyphStripPx;

            if (n.icon)
            {
                int iconY = (row.top + row.bottom - kIconSizePx) / 2;
                HDC memDC = ::CreateCompatibleDC(hdc);
                HGDIOBJ old = ::SelectObject(memDC, n.icon);
                BITMAP bm = {};
                ::GetObject(n.icon, sizeof(bm), &bm);
                if (bm.bmWidth > 0 && bm.bmHeight > 0)
                {
                    ::SetStretchBltMode(hdc, HALFTONE);
                    ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                    ::StretchBlt(hdc, xCursor, iconY, kIconSizePx, kIconSizePx,
                                 memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                }
                ::SelectObject(memDC, old);
                ::DeleteDC(memDC);
                xCursor += kIconSizePx + kCellPad;
            }

            int textRight = row.right - kRightPad;
            if (n.statusColor != (COLORREF)CLR_INVALID)
            {
                int cy = (row.top + row.bottom) / 2;
                int cx = row.right - kRightPad - kStatusDotR;
                RECT dot = { cx - kStatusDotR, cy - kStatusDotR,
                             cx + kStatusDotR, cy + kStatusDotR };
                DuiAA::FillEllipse(hdc, dot, n.statusColor, CLR_INVALID);
                textRight = dot.left - kCellPad;
            }

            if (!n.label.IsEmpty())
            {
                HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
                HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
                int oldBk = ::SetBkMode(hdc, TRANSPARENT);
                COLORREF txt = selected ? m_clrTextSel : m_clrText;
                COLORREF oldClr = ::SetTextColor(hdc, txt);
                RECT rt = row;
                rt.left  = xCursor;
                rt.right = textRight;
                ::DrawText(hdc, n.label, -1, &rt,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                ::SetTextColor(hdc, oldClr);
                ::SetBkMode(hdc, oldBk);
                if (oldFont) { ::SelectObject(hdc, oldFont); }
            }
        }
        return;
    }

    // Multi-col path.
    FillSolidRect_(hdc, m_rcItem, m_clrRowBg);
    PaintHeader(hdc, rcDirty);
    PaintBody  (hdc, rcDirty);

    // Children (scrollbars, editor).
    DuiControl::OnPaint(hdc, rcDirty);
}

void DuiTreeView::PaintHeader(HDC hdc, const RECT& /*rcDirty*/) const
{
    RECT rcHeader = HeaderRect();
    FillSolidRect_(hdc, rcHeader, m_clrHeaderBg);

    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, m_clrHeaderText);

    int frozenRightX = FrozenColsRightX();
    int frozenSplit  = rcHeader.left + frozenRightX;

    auto paintColHeader = [&](int colIdx, int xLeft) {
        const ColumnDef& col = m_columns[colIdx];
        RECT rc;
        rc.left   = xLeft;
        rc.right  = xLeft + col.width;
        rc.top    = rcHeader.top;
        rc.bottom = rcHeader.bottom;
        if (rc.right < rcHeader.left || rc.left > rcHeader.right)
        {
            return;
        }
        // Right separator line.
        RECT sep;
        sep.left   = rc.right - 1;
        sep.right  = rc.right;
        sep.top    = rc.top + 4;
        sep.bottom = rc.bottom - 4;
        FillSolidRect_(hdc, sep, m_clrGrid);

        // Title text.
        RECT rt = rc;
        rt.left  += kCellPad;
        rt.right -= kCellPad;
        UINT dt = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
        dt |= (col.textAlign & (DT_LEFT | DT_CENTER | DT_RIGHT));
        if ((dt & (DT_LEFT | DT_CENTER | DT_RIGHT)) == 0)
        {
            dt |= DT_LEFT;
        }
        ::DrawText(hdc, col.title, -1, &rt, dt);

        // Sort arrow on this column?
        if (m_sortCol == colIdx && m_sortDir != 0)
        {
            RECT ra;
            ra.right  = rc.right - kCellPad;
            ra.left   = ra.right - kSortArrowSizePx;
            int cy = (rc.top + rc.bottom) / 2;
            ra.top    = cy - kSortArrowSizePx / 2;
            ra.bottom = cy + kSortArrowSizePx / 2;
            PaintSortArrow(hdc, ra, m_sortDir);
        }
    };

    // Frozen header area: left-aligned, no scroll.
    {
        HRGN clip = ::CreateRectRgn(rcHeader.left, rcHeader.top, frozenSplit, rcHeader.bottom);
        ::SelectClipRgn(hdc, clip);
        int x = rcHeader.left;
        for (int i = 0; i < m_frozenCols && i < (int)m_columns.size(); ++i)
        {
            paintColHeader(i, x);
            x += m_columns[i].width;
        }
        ::SelectClipRgn(hdc, nullptr);
        ::DeleteObject(clip);
    }
    // Non-frozen header area: scrolls horizontally.
    {
        HRGN clip = ::CreateRectRgn(frozenSplit, rcHeader.top, rcHeader.right, rcHeader.bottom);
        ::SelectClipRgn(hdc, clip);
        int x = frozenSplit - m_scrollX;
        for (int i = m_frozenCols; i < (int)m_columns.size(); ++i)
        {
            paintColHeader(i, x);
            x += m_columns[i].width;
        }
        ::SelectClipRgn(hdc, nullptr);
        ::DeleteObject(clip);
    }

    // Bottom border under header.
    RECT under;
    under.left = rcHeader.left;
    under.right = rcHeader.right;
    under.bottom = rcHeader.bottom;
    under.top    = rcHeader.bottom - 1;
    FillSolidRect_(hdc, under, m_clrGrid);

    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    if (oldFont) { ::SelectObject(hdc, oldFont); }
}

void DuiTreeView::PaintSortArrow(HDC hdc, const RECT& rc, int dir) const
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top  + rc.bottom) / 2;
    int s = kSortArrowSizePx / 2;
    POINT pts[3];
    if (dir > 0)
    {
        pts[0] = { cx,     cy - s };
        pts[1] = { cx + s, cy + s };
        pts[2] = { cx - s, cy + s };
    }
    else
    {
        pts[0] = { cx - s, cy - s };
        pts[1] = { cx + s, cy - s };
        pts[2] = { cx,     cy + s };
    }
    DuiAA::FillPolygon(hdc, pts, 3, m_clrHeaderText, CLR_INVALID);
}

void DuiTreeView::PaintBody(HDC hdc, const RECT& rcDirty) const
{
    RECT rcTL, rcTR, rcBL, rcBR;
    ComputeQuadrants(rcTL, rcTR, rcBL, rcBR);

    // 每象限按 dirty-rect 收窄行号：只画行 y 区间与 (rcDirty ∩ rcQuad) 相交
    // 的那几行。不做这步的话，BR 象限每帧都会完整循环 m_visible × m_columns，
    // 上万行时这是主要瓶颈。
    //
    // yOriginAbs = 该象限坐标系下"第 0 行"的绝对 y 值
    //   · 冻结行象限 TL / TR：= body.top；
    //   · 滚动行象限 BL / BR：= body.top - m_scrollY。
    // 行 r 占据 y ∈ [yOriginAbs + r*m_rowH, yOriginAbs + (r+1)*m_rowH)。
    auto paintQuadrant = [&](const RECT& rcQuad, int rowFrom, int rowTo,
                             int colFrom, int colTo,
                             int xOriginAbs, int yOriginAbs) {
        if (rcQuad.right <= rcQuad.left || rcQuad.bottom <= rcQuad.top)
        {
            return;
        }

        // 在 y 轴上把 rcDirty 和 rcQuad 求交，得到本帧真正需要遍历的行段；
        // 再转成行号，并钳到调用方传入的 [rowFrom, rowTo) 窗口里。
        int yLo = (rcDirty.top    > rcQuad.top)    ? rcDirty.top    : rcQuad.top;
        int yHi = (rcDirty.bottom < rcQuad.bottom) ? rcDirty.bottom : rcQuad.bottom;
        if (yHi <= yLo)
        {
            return;
        }
        // 行号取 floor / ceil。正常布局下 yLo - yOriginAbs >= 0
        // （yOriginAbs 总在象限顶部之上或齐平）；万一未来几何调整出现负值，
        // 走 visLo = 0 兜底，再与 rowFrom 取 max 也不会越界少画。
        int relLo = yLo - yOriginAbs;
        int relHi = yHi - yOriginAbs;
        int visLo = (relLo >= 0) ? (relLo / m_rowH) : 0;
        int visHi = (relHi >= 0) ? ((relHi + m_rowH - 1) / m_rowH) : 0;
        if (visLo > rowFrom) { rowFrom = visLo; }
        if (visHi < rowTo)   { rowTo   = visHi; }
        if (rowFrom >= rowTo)
        {
            return;
        }

        HRGN clip = ::CreateRectRgn(rcQuad.left, rcQuad.top, rcQuad.right, rcQuad.bottom);
        ::SelectClipRgn(hdc, clip);

        for (int r = rowFrom; r < rowTo && r < (int)m_visible.size(); ++r)
        {
            int idx = m_visible[r];
            const Node& n = m_nodes[idx];
            RECT row;
            row.left   = xOriginAbs + ColumnLeftAbs(colFrom);
            row.right  = xOriginAbs + ColumnLeftAbs(colTo);
            row.top    = yOriginAbs + r * m_rowH;
            row.bottom = row.top + m_rowH;

            bool selected = (n.id == m_curSelId);
            bool hovered  = (n.id == m_hoverId) && !selected;
            COLORREF bg = m_clrRowBg;
            if (m_zebra && (r & 1) == 1 && !selected) { bg = m_clrZebra; }
            if (selected) { bg = m_clrRowSel; }
            else if (hovered) { bg = m_clrRowHover; }
            FillSolidRect_(hdc, row, bg);

            for (int col = colFrom; col < colTo && col < (int)m_columns.size(); ++col)
            {
                RECT rcCell;
                rcCell.left   = xOriginAbs + ColumnLeftAbs(col);
                rcCell.right  = rcCell.left + m_columns[col].width;
                rcCell.top    = row.top;
                rcCell.bottom = row.bottom;

                bool focusCell = IsMultiCol()
                                 && m_focusCell.itemId == n.id
                                 && m_focusCell.col    == col;
                // cell-level selection paints a brand-blue overlay when not whole-row selected.
                bool cellSel = false;
                for (auto& cs : m_selCells)
                {
                    if (cs.itemId == n.id && cs.col == col)
                    {
                        cellSel = true;
                        break;
                    }
                }
                if (cellSel && !selected)
                {
                    FillSolidRect_(hdc, rcCell, m_clrRowSel);
                }

                PaintCell(hdc, rcCell, n, col, selected || cellSel, focusCell);

                // Right grid line.
                RECT gv;
                gv.right  = rcCell.right;
                gv.left   = rcCell.right - 1;
                gv.top    = rcCell.top;
                gv.bottom = rcCell.bottom;
                FillSolidRect_(hdc, gv, m_clrGrid);
            }

            // Bottom grid line.
            RECT gh;
            gh.left   = row.left;
            gh.right  = row.right;
            gh.bottom = row.bottom;
            gh.top    = row.bottom - 1;
            FillSolidRect_(hdc, gh, m_clrGrid);
        }

        ::SelectClipRgn(hdc, nullptr);
        ::DeleteObject(clip);
    };

    int frozenRows = m_frozenRows;
    if (frozenRows > (int)m_visible.size()) { frozenRows = (int)m_visible.size(); }
    int totalRows  = (int)m_visible.size();
    int frozenCols = m_frozenCols;
    if (frozenCols > (int)m_columns.size()) { frozenCols = (int)m_columns.size(); }
    int totalCols  = (int)m_columns.size();

    RECT body = BodyRect();

    // TL: frozen rows × frozen cols (no scroll).
    paintQuadrant(rcTL, 0, frozenRows, 0, frozenCols,
                  body.left, body.top);
    // TR: frozen rows × non-frozen cols (h-scroll only).
    paintQuadrant(rcTR, 0, frozenRows, frozenCols, totalCols,
                  body.left - m_scrollX, body.top);
    // BL: non-frozen rows × frozen cols (v-scroll only).
    paintQuadrant(rcBL, frozenRows, totalRows, 0, frozenCols,
                  body.left, body.top - m_scrollY);
    // BR: non-frozen rows × non-frozen cols (both scroll).
    paintQuadrant(rcBR, frozenRows, totalRows, frozenCols, totalCols,
                  body.left - m_scrollX, body.top - m_scrollY);

    // Focus cell rect overlay (drawn last, on top of everything).
    if (m_focusCell.itemId != -1)
    {
        int row = GetVisibleRow(m_focusCell.itemId);
        int col = m_focusCell.col;
        if (row >= 0 && col >= 0 && col < (int)m_columns.size())
        {
            RECT rcCell;
            // Determine quadrant origin.
            int xOff;
            int yOff;
            if (col < frozenCols)  { xOff = body.left; }
            else                   { xOff = body.left - m_scrollX; }
            if (row < frozenRows)  { yOff = body.top; }
            else                   { yOff = body.top - m_scrollY; }
            rcCell.left   = xOff + ColumnLeftAbs(col);
            rcCell.right  = rcCell.left + m_columns[col].width;
            rcCell.top    = yOff + row * m_rowH;
            rcCell.bottom = rcCell.top + m_rowH;
            // Clip to body.
            RECT inter;
            ::IntersectRect(&inter, &rcCell, &body);
            if (inter.right > inter.left && inter.bottom > inter.top)
            {
                FrameSolidRect_(hdc, inter, m_clrFocusBorder);
                RECT inner = inter;
                ::InflateRect(&inner, -1, -1);
                if (inner.right > inner.left && inner.bottom > inner.top)
                {
                    // Inner 1px white dashed-ish: just draw a slightly lighter solid for now.
                    FrameSolidRect_(hdc, inner, RGB(255, 255, 255));
                }
            }
        }
    }
}

void DuiTreeView::PaintCell(HDC hdc, const RECT& rcCell, const Node& n,
                            int col, bool selected, bool /*focusCell*/) const
{
    const Cell* c = TryGetCell(IndexOf(n.id), col);
    int xCursor = rcCell.left;
    int xRight  = rcCell.right - kCellPad;

    // col 0 always gets glyph + indent + tree icon (legacy semantics).
    if (col == 0)
    {
        int idx = IndexOf(n.id);
        xCursor = rcCell.left + n.depth * m_indent;
        if (HasChildrenIdx(idx))
        {
            RECT g;
            g.left   = xCursor;
            g.right  = xCursor + kGlyphStripPx;
            int cy   = (rcCell.top + rcCell.bottom) / 2;
            g.top    = cy - kGlyphStripPx / 2;
            g.bottom = cy + kGlyphStripPx / 2;
            PaintGlyph(hdc, g, n.expanded);
        }
        xCursor += kGlyphStripPx;
        if (n.icon)
        {
            int iconY = (rcCell.top + rcCell.bottom - kIconSizePx) / 2;
            HDC memDC = ::CreateCompatibleDC(hdc);
            HGDIOBJ old = ::SelectObject(memDC, n.icon);
            BITMAP bm = {};
            ::GetObject(n.icon, sizeof(bm), &bm);
            if (bm.bmWidth > 0 && bm.bmHeight > 0)
            {
                ::SetStretchBltMode(hdc, HALFTONE);
                ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                ::StretchBlt(hdc, xCursor, iconY, kIconSizePx, kIconSizePx,
                             memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
            }
            ::SelectObject(memDC, old);
            ::DeleteDC(memDC);
            xCursor += kIconSizePx + kCellPad;
        }
        else
        {
            xCursor += kCellPad;
        }
    }
    else
    {
        xCursor += kCellPad;
    }

    if (xCursor >= xRight)
    {
        return;
    }

    RECT rcContent;
    rcContent.left   = xCursor;
    rcContent.right  = xRight;
    rcContent.top    = rcCell.top;
    rcContent.bottom = rcCell.bottom;

    CellType type = c ? c->type : CELL_TEXT;
    UINT dt = (col < (int)m_columns.size()) ? m_columns[col].textAlign : (UINT)DT_LEFT;
    dt |= DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);

    switch (type)
    {
        case CELL_TEXT:
        {
            CString text = (col == 0) ? n.label : (c ? c->text : CString());
            COLORREF txt = selected ? m_clrTextSel : m_clrText;
            COLORREF oldClr = ::SetTextColor(hdc, txt);
            ::DrawText(hdc, text, -1, &rcContent, dt);
            ::SetTextColor(hdc, oldClr);
            break;
        }
        case CELL_ICON:
        {
            HBITMAP bm = c ? c->bitmap : nullptr;
            if (bm)
            {
                int iconCx = (rcContent.left + rcContent.right) / 2;
                int iconY  = (rcContent.top  + rcContent.bottom - kCellIconPx) / 2;
                int iconX  = iconCx - kCellIconPx / 2;
                BITMAP info = {};
                ::GetObject(bm, sizeof(info), &info);
                HDC memDC = ::CreateCompatibleDC(hdc);
                HGDIOBJ old = ::SelectObject(memDC, bm);
                if (info.bmWidth > 0 && info.bmHeight > 0)
                {
                    ::SetStretchBltMode(hdc, HALFTONE);
                    ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                    ::StretchBlt(hdc, iconX, iconY, kCellIconPx, kCellIconPx,
                                 memDC, 0, 0, info.bmWidth, info.bmHeight, SRCCOPY);
                }
                ::SelectObject(memDC, old);
                ::DeleteDC(memDC);
            }
            break;
        }
        case CELL_IMAGE:
        {
            HBITMAP bm = c ? c->bitmap : nullptr;
            if (bm)
            {
                BITMAP info = {};
                ::GetObject(bm, sizeof(info), &info);
                if (info.bmWidth > 0 && info.bmHeight > 0)
                {
                    int dstW = rcContent.right - rcContent.left;
                    int dstH = rcContent.bottom - rcContent.top - 2 * kIconCellPadY;
                    if (dstH <= 0) { dstH = rcContent.bottom - rcContent.top; }
                    HDC memDC = ::CreateCompatibleDC(hdc);
                    HGDIOBJ old = ::SelectObject(memDC, bm);
                    ::SetStretchBltMode(hdc, HALFTONE);
                    ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                    ::StretchBlt(hdc, rcContent.left, rcContent.top + kIconCellPadY,
                                 dstW, dstH,
                                 memDC, 0, 0, info.bmWidth, info.bmHeight, SRCCOPY);
                    ::SelectObject(memDC, old);
                    ::DeleteDC(memDC);
                }
            }
            break;
        }
        case CELL_CHECKBOX:
        {
            int boxCx = rcContent.left + kCheckBoxSizePx / 2 + kCellPad / 2;
            int boxCy = (rcContent.top + rcContent.bottom) / 2;
            RECT rcBox;
            rcBox.left   = boxCx - kCheckBoxSizePx / 2;
            rcBox.right  = boxCx + kCheckBoxSizePx / 2;
            rcBox.top    = boxCy - kCheckBoxSizePx / 2;
            rcBox.bottom = boxCy + kCheckBoxSizePx / 2;
            PaintCheckBox(hdc, rcBox, c ? c->checked : false);
            // Optional caption to the right of the box.
            if (c && !c->text.IsEmpty())
            {
                RECT rcCaption = rcContent;
                rcCaption.left = rcBox.right + kCellPad;
                COLORREF txt = selected ? m_clrTextSel : m_clrText;
                COLORREF oldClr = ::SetTextColor(hdc, txt);
                ::DrawText(hdc, c->text, -1, &rcCaption, dt);
                ::SetTextColor(hdc, oldClr);
            }
            break;
        }
        case CELL_PROGRESS:
        {
            int v = c ? c->value : 0;
            CString text = (c && !c->text.IsEmpty()) ? c->text : CString();
            if (text.IsEmpty())
            {
                text.Format(_T("%d%%"), v);
            }
            PaintProgress(hdc, rcContent, v, text);
            break;
        }
        case CELL_HYPERLINK:
        {
            CString text = c ? c->text : CString();
            COLORREF txt = selected ? m_clrTextSel : m_clrLink;
            PaintHyperlink(hdc, rcContent, text, txt);
            break;
        }
    }

    ::SetBkMode(hdc, oldBk);
    if (oldFont) { ::SelectObject(hdc, oldFont); }
}

void DuiTreeView::PaintCheckBox(HDC hdc, const RECT& rcBox, bool checked) const
{
    FrameSolidRect_(hdc, rcBox, RGB(120, 120, 120));
    RECT inner = rcBox;
    ::InflateRect(&inner, -1, -1);
    FillSolidRect_(hdc, inner, RGB(255, 255, 255));
    if (checked)
    {
        // Draw a brand-blue check tick using AA polyline.
        int cx0 = rcBox.left + 3;
        int cy0 = (rcBox.top + rcBox.bottom) / 2;
        int cx1 = (rcBox.left + rcBox.right) / 2 - 1;
        int cy1 = rcBox.bottom - 4;
        int cx2 = rcBox.right - 3;
        int cy2 = rcBox.top + 3;
        DuiAA::DrawLine(hdc, cx0, cy0, cx1, cy1, m_clrFocusBorder, 2.0f);
        DuiAA::DrawLine(hdc, cx1, cy1, cx2, cy2, m_clrFocusBorder, 2.0f);
    }
}

void DuiTreeView::PaintProgress(HDC hdc, const RECT& rcCell, int value, LPCTSTR text) const
{
    RECT rcBar = rcCell;
    int cy = (rcCell.top + rcCell.bottom) / 2;
    rcBar.top    = cy - kProgressBarH / 2;
    rcBar.bottom = cy + kProgressBarH / 2;
    rcBar.left  += kCellPad;
    rcBar.right -= kCellPad;
    if (rcBar.right <= rcBar.left)
    {
        return;
    }
    FillSolidRect_(hdc, rcBar, RGB(232, 234, 240));
    int barW = rcBar.right - rcBar.left;
    int fillW = barW * value / 100;
    if (fillW > 0)
    {
        RECT rcFill = rcBar;
        rcFill.right = rcFill.left + fillW;
        FillSolidRect_(hdc, rcFill, m_clrFocusBorder);
    }
    FrameSolidRect_(hdc, rcBar, RGB(180, 184, 192));

    if (text && text[0])
    {
        RECT rt = rcCell;
        rt.left  += kProgressTextPad;
        rt.right -= kProgressTextPad;
        COLORREF oldClr = ::SetTextColor(hdc, RGB(60, 60, 60));
        UINT dt = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
        ::DrawText(hdc, text, -1, &rt, dt);
        ::SetTextColor(hdc, oldClr);
    }
}

void DuiTreeView::PaintHyperlink(HDC hdc, const RECT& rcCell, LPCTSTR text, COLORREF color) const
{
    if (!text || !text[0])
    {
        return;
    }
    HFONT hf = DuiResMgr::Inst().GetDefaultFont();
    LOGFONT lf = {};
    if (hf)
    {
        ::GetObject(hf, sizeof(lf), &lf);
    }
    lf.lfUnderline = TRUE;
    HFONT hfu = ::CreateFontIndirect(&lf);
    HFONT old = (HFONT)::SelectObject(hdc, hfu);
    COLORREF oldClr = ::SetTextColor(hdc, color);
    UINT dt = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
    ::DrawText(hdc, text, -1, const_cast<LPRECT>(&rcCell), dt);
    ::SetTextColor(hdc, oldClr);
    ::SelectObject(hdc, old);
    ::DeleteObject(hfu);
}

void DuiTreeView::PaintGlyph(HDC hdc, const RECT& rc, bool expanded) const
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int s = 4;       // glyph half-size: tuned to 8x8 visual at 1.0 DPI
    POINT pts[3];
    if (expanded)
    {
        pts[0] = { cx - s, cy - s/2 };
        pts[1] = { cx + s, cy - s/2 };
        pts[2] = { cx,     cy + s   };
    }
    else
    {
        pts[0] = { cx - s/2, cy - s };
        pts[1] = { cx + s,   cy     };
        pts[2] = { cx - s/2, cy + s };
    }
    DuiAA::FillPolygon(hdc, pts, 3, m_clrGlyph, CLR_INVALID);
}

// ================================================================
// mouse / keyboard
// ================================================================

bool DuiTreeView::OnLButtonDown(POINT pt, UINT mkFlags)
{
    if (!m_bEnabled)
    {
        return false;
    }
    HitInfo h = HitTest_(pt);

    // Editor active outside hit area → commit it.
    if (m_editor && (h.zone == HZ_NONE || h.itemId != m_editId || h.col != m_editCol))
    {
        CommitEdit();
    }

    if (h.zone == HZ_HEADER_SEP)
    {
        // Begin column resize drag.
        m_resizing     = true;
        m_resizeCol    = h.col;
        m_resizeStartX = pt.x;
        m_resizeStartW = m_columns[h.col].width;
        Capture();
        return true;
    }
    if (h.zone == HZ_HEADER_TEXT)
    {
        // Sort intent.
        if (m_columns[h.col].sortable)
        {
            bool ascending = !(m_sortCol == h.col && m_sortDir > 0);
            DuiTreeCellNotify n{};
            n.itemId    = -1;
            n.col       = h.col;
            n.ascending = ascending;
            NotifyParent((UINT)DUITVN_COLUMNCLICK, (LPARAM)&n);
        }
        return true;
    }
    if (h.zone == HZ_BODY_GLYPH)
    {
        SetExpanded(h.itemId, !m_nodes[m_visible[h.visibleRow]].expanded);
        return true;
    }
    if (h.zone == HZ_BODY_CELL)
    {
        // Cell-type interactions (subject to editable gates).
        bool colEdit = !IsMultiCol() ? true
                       : (h.col >= 0 && h.col < (int)m_columns.size() && m_columns[h.col].editable);
        bool gate    = m_editable && colEdit;

        const Cell* c = TryGetCell(IndexOf(h.itemId), h.col);
        CellType type = c ? c->type : CELL_TEXT;

        if (type == CELL_HYPERLINK)
        {
            // Always clickable.
            DuiTreeCellNotify nn{};
            nn.itemId = h.itemId;
            nn.col    = h.col;
            nn.text   = c->url;
            NotifyParent((UINT)DUITVN_LINKCLICK, (LPARAM)&nn);
            return true;
        }
        if (type == CELL_CHECKBOX && gate)
        {
            int idx = IndexOf(h.itemId);
            bool newVal = !(c ? c->checked : false);
            EnsureCell(idx, h.col).checked = newVal;
            Invalidate();
            DuiTreeCellNotify nn{};
            nn.itemId  = h.itemId;
            nn.col     = h.col;
            nn.checked = newVal;
            NotifyParent((UINT)DUITVN_CHECKED, (LPARAM)&nn);
            return true;
        }
        if (type == CELL_PROGRESS && gate)
        {
            // Map x within content area to 0..100.
            RECT rowRc = RowRect(h.visibleRow);
            RECT rcCell = CellRectInRow(rowRc, h.col);
            int barW = (rcCell.right - rcCell.left) - 2 * kCellPad;
            if (barW > 0)
            {
                // pt.x is host coord; convert to in-cell content x.
                RECT body = BodyRect();
                int xOff = body.left;
                if (h.col >= m_frozenCols) { xOff -= m_scrollX; }
                int cellLeftAbs = xOff + ColumnLeftAbs(h.col) + kCellPad;
                int xx = pt.x - cellLeftAbs;
                if (xx < 0) { xx = 0; }
                if (xx > barW) { xx = barW; }
                int newVal = xx * 100 / barW;
                int idx = IndexOf(h.itemId);
                EnsureCell(idx, h.col).value = newVal;
                Invalidate();
                DuiTreeCellNotify nn{};
                nn.itemId = h.itemId;
                nn.col    = h.col;
                nn.value  = newVal;
                NotifyParent((UINT)DUITVN_VALUECHANGED_CELL, (LPARAM)&nn);
                return true;
            }
        }

        // Default: select.
        if (IsMultiCol())
        {
            bool ctrl  = (mkFlags & MK_CONTROL) != 0;
            bool shift = (mkFlags & MK_SHIFT)   != 0;
            CellRef target{ h.itemId, h.col };
            if (!ctrl && !shift)
            {
                m_selCells.clear();
                m_selCells.push_back(target);
                m_focusCell = target;
            }
            else if (ctrl)
            {
                // Toggle.
                bool found = false;
                for (auto it = m_selCells.begin(); it != m_selCells.end(); ++it)
                {
                    if (it->itemId == target.itemId && it->col == target.col)
                    {
                        m_selCells.erase(it);
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    m_selCells.push_back(target);
                }
                m_focusCell = target;
            }
            else if (shift && m_focusCell.itemId != -1)
            {
                int rA = GetVisibleRow(m_focusCell.itemId);
                int rB = h.visibleRow;
                int cA = m_focusCell.col;
                int cB = h.col;
                if (rA > rB) { std::swap(rA, rB); }
                if (cA > cB) { std::swap(cA, cB); }
                m_selCells.clear();
                for (int r = rA; r <= rB && r < (int)m_visible.size(); ++r)
                {
                    int id = m_nodes[m_visible[r]].id;
                    for (int cc = cA; cc <= cB; ++cc)
                    {
                        m_selCells.push_back(CellRef{ id, cc });
                    }
                }
            }
            m_curSelId = h.itemId;
            Invalidate();
            NotifyParent(DUIN_VALUECHANGED, (LPARAM)h.itemId);
        }
        else
        {
            SetCurSel(h.itemId, /*notify=*/true);
        }
        return true;
    }
    if (h.zone == HZ_BODY_BLANK)
    {
        if (IsMultiCol())
        {
            ClearSelection();
        }
        else
        {
            SetCurSel(-1, /*notify=*/true);
        }
        return true;
    }
    return false;
}

bool DuiTreeView::OnLButtonUp(POINT pt, UINT /*mkFlags*/)
{
    if (m_resizing)
    {
        m_resizing = false;
        ReleaseCapture();
        return true;
    }
    HitInfo h = HitTest_(pt);
    if (h.zone == HZ_BODY_CELL && !m_editor)
    {
        NotifyParent(DUIN_CLICK, (LPARAM)h.itemId);
    }
    return true;
}

bool DuiTreeView::OnLButtonDblClk(POINT pt, UINT /*mkFlags*/)
{
    HitInfo h = HitTest_(pt);
    if (h.zone == HZ_HEADER_SEP)
    {
        // Auto-fit: scan visible rows and take widest text in this column.
        // Fallback: use header title width.
        HDC hdc = nullptr;
        if (m_pHost)
        {
            hdc = ::GetDC(m_pHost->m_hWnd);
        }
        int maxW = 0;
        if (hdc)
        {
            HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
            HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
            SIZE sz;
            if (::GetTextExtentPoint32(hdc, m_columns[h.col].title,
                                       m_columns[h.col].title.GetLength(), &sz))
            {
                maxW = sz.cx;
            }
            for (int r = 0; r < (int)m_visible.size(); ++r)
            {
                CString text = GetCellText(m_nodes[m_visible[r]].id, h.col);
                if (!text.IsEmpty())
                {
                    ::GetTextExtentPoint32(hdc, text, text.GetLength(), &sz);
                    if (sz.cx > maxW) { maxW = sz.cx; }
                }
            }
            if (oldFont) { ::SelectObject(hdc, oldFont); }
            ::ReleaseDC(m_pHost->m_hWnd, hdc);
        }
        // Add padding (icon strip if col 0 + tree indent).
        int extra = kCellPad * 2 + kSortArrowSizePx + kCellPad;
        if (h.col == 0)
        {
            int maxDepth = 0;
            for (auto& n : m_nodes)
            {
                if (n.depth > maxDepth) { maxDepth = n.depth; }
            }
            extra += maxDepth * m_indent + kGlyphStripPx + kIconSizePx + kCellPad;
        }
        int newW = maxW + extra;
        if (newW < m_columns[h.col].minWidth) { newW = m_columns[h.col].minWidth; }
        SetColumnWidth(h.col, newW);
        return true;
    }
    if (h.zone == HZ_BODY_CELL)
    {
        // Try in-place edit for CELL_TEXT.
        const Cell* c = TryGetCell(IndexOf(h.itemId), h.col);
        CellType type = c ? c->type : CELL_TEXT;
        if (type == CELL_TEXT && m_editable
            && (!IsMultiCol() || m_columns[h.col].editable))
        {
            if (BeginEdit(h.itemId, h.col))
            {
                return true;
            }
        }
        NotifyParent(DUIN_DBLCLK, (LPARAM)h.itemId);
        return true;
    }
    return false;
}

bool DuiTreeView::OnRButtonDown(POINT pt, UINT /*mkFlags*/)
{
    HitInfo h = HitTest_(pt);
    if (h.zone == HZ_HEADER_TEXT || h.zone == HZ_HEADER_SEP)
    {
        DuiTreeCellNotify n{};
        n.itemId = -1;
        n.col    = h.col;
        NotifyParent((UINT)DUITVN_HEADER_RCLICK, (LPARAM)&n);
        return true;
    }
    if (h.zone == HZ_BODY_BLANK)
    {
        DuiTreeCellNotify n{};
        n.itemId = -1;
        n.col    = -1;
        NotifyParent((UINT)DUITVN_BLANK_RCLICK, (LPARAM)&n);
        return true;
    }
    if (h.zone == HZ_BODY_CELL || h.zone == HZ_BODY_GLYPH)
    {
        SetCurSel(h.itemId, /*notify=*/true);
        NotifyParent((UINT)DUITVN_RCLICK, (LPARAM)h.itemId);
        return true;
    }
    return false;
}

bool DuiTreeView::OnMouseMove(POINT pt, UINT /*mkFlags*/)
{
    if (m_resizing && m_resizeCol >= 0 && m_resizeCol < (int)m_columns.size())
    {
        int dx = pt.x - m_resizeStartX;
        int newW = m_resizeStartW + dx;
        if (newW < m_columns[m_resizeCol].minWidth)
        {
            newW = m_columns[m_resizeCol].minWidth;
        }
        m_columns[m_resizeCol].width = newW;
        EnsureScrollRanges();
        Invalidate();
        return true;
    }
    HitInfo h = HitTest_(pt);
    int newHover = (h.zone == HZ_BODY_CELL || h.zone == HZ_BODY_GLYPH) ? h.itemId : -1;
    if (newHover != m_hoverId)
    {
        m_hoverId = newHover;
        Invalidate();
    }
    return false;
}

bool DuiTreeView::OnMouseLeave()
{
    if (m_hoverId != -1)
    {
        m_hoverId = -1;
        Invalidate();
    }
    return false;
}

bool DuiTreeView::OnMouseWheel(POINT /*pt*/, short zDelta, UINT mkFlags)
{
    if (!IsMultiCol())
    {
        return false;
    }
    int lines = -zDelta / WHEEL_DELTA;
    if (lines == 0) { lines = (zDelta > 0 ? -1 : 1); }
    if (mkFlags & MK_SHIFT)
    {
        ScrollX(lines * m_rowH);
    }
    else
    {
        ScrollY(lines * m_rowH);
    }
    return true;
}

bool DuiTreeView::OnSetCursor(POINT pt)
{
    HitInfo h = HitTest_(pt);
    if (h.zone == HZ_HEADER_SEP || m_resizing)
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_SIZEWE));
        return true;
    }
    if (h.zone == HZ_BODY_CELL)
    {
        const Cell* c = TryGetCell(IndexOf(h.itemId), h.col);
        if (c && c->type == CELL_HYPERLINK)
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
            return true;
        }
    }
    return false;
}

bool DuiTreeView::OnKeyDown(UINT vk, UINT /*flags*/)
{
    if (m_editor)
    {
        if (vk == VK_RETURN)
        {
            CommitEdit();
            return true;
        }
        if (vk == VK_ESCAPE)
        {
            CancelEdit();
            return true;
        }
        return false;
    }
    if (vk == VK_F2 && m_editable && m_curSelId != -1)
    {
        int col = IsMultiCol() ? m_focusCell.col : 0;
        if (col < 0) { col = 0; }
        BeginEdit(m_curSelId, col);
        return true;
    }
    return false;
}

// ================================================================
// appearance setters
// ================================================================

void DuiTreeView::SetRowHeight(int px)
{
    if (px < kRowHeightFloor) { px = kRowHeightFloor; }
    if (m_rowH == px) { return; }
    m_rowH = px;
    EnsureScrollRanges();
    Invalidate();
}

void DuiTreeView::SetIndentPx(int px)
{
    if (px < kIndentFloor) { px = kIndentFloor; }
    if (m_indent == px) { return; }
    m_indent = px;
    Invalidate();
}

void DuiTreeView::SetHeaderHeight(int px)
{
    if (px < kHeaderHeightFloor) { px = kHeaderHeightFloor; }
    if (m_headerH == px) { return; }
    m_headerH = px;
    EnsureScrollRanges();
    Invalidate();
}

void DuiTreeView::SetZebra(bool b)
{
    if (m_zebra == b) { return; }
    m_zebra = b;
    Invalidate();
}

// ================================================================
// legacy
// ================================================================

int DuiTreeView::FirstAncestorIdx(int idx, int rootIdx) const
{
    while (idx >= 0 && m_nodes[idx].parentIdx != rootIdx && m_nodes[idx].parentIdx >= 0)
    {
        idx = m_nodes[idx].parentIdx;
    }
    return idx;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_TREEVIEW
