#include "stdafx.h"
#include "DuiTreeView.h"

#if BUI_FEATURE_TREEVIEW

#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"
#include "../../DuiHost.h"
#include "../Window/DuiScrollBar.h"
#include "../Input/DuiEdit.h"
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
    ReleaseAllGrayIcons_();
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
    if (m_idIndexDirty)
    {
        RebuildIdIndex_();
    }

    //当前这批节点的 id 都落在 [m_idBase, m_nextId) 内，范围外的一定不存在
    if (id < m_idBase)
    {
        return -1;
    }
    const size_t slot = (size_t)(id - m_idBase);
    if (slot >= m_idToIndex.size())
    {
        return -1;
    }
    const int idx = m_idToIndex[slot];
    if (idx < 0)
    {
        return -1;   //该节点已删除
    }
    if (idx < (int)m_nodes.size() && m_nodes[(size_t)idx].id == id)
    {
        return idx;
    }

    //对照表与节点数组对不上：说明某处结构变化漏了标记。退回逐个比对保证结果正确，
    //并标记对照表过期，下次查找时整体重建
    m_idIndexDirty = true;
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].id == id)
        {
            return (int)i;
        }
    }
    return -1;
}

void DuiTreeView::RebuildIdIndex_() const
{
    m_idToIndex.assign((size_t)(m_nextId - m_idBase), -1);
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        const int slot = m_nodes[i].id - m_idBase;
        if (slot >= 0 && slot < (int)m_idToIndex.size())
        {
            m_idToIndex[(size_t)slot] = (int)i;
        }
    }
    m_idIndexDirty = false;
}

void DuiTreeView::AppendIdIndex_(int id, int idx)
{
    if (m_idIndexDirty)
    {
        return;   //对照表已过期，下次重建时会一并算上
    }
    //id 连续分配：新节点的槽位正好是表尾。对不上（不应发生）就标记过期，交给重建
    if (m_idToIndex.size() != (size_t)(id - m_idBase))
    {
        m_idIndexDirty = true;
        return;
    }
    m_idToIndex.push_back(idx);
}

bool DuiTreeView::SubtreeReachesEnd_(int parentIdx) const
{
    if (parentIdx < 0 || m_nodes.empty())
    {
        return false;
    }
    const int last = (int)m_nodes.size() - 1;
    if (last == parentIdx)
    {
        return true;
    }
    //子节点的下标一定大于父节点，沿父下标上溯到不大于 parentIdx 为止，看是否正好停在它上面
    int p = m_nodes[(size_t)last].parentIdx;
    while (p > parentIdx)
    {
        p = m_nodes[(size_t)p].parentIdx;
    }
    return p == parentIdx;
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
    AppendIdIndex_(n.id, (int)m_nodes.size() - 1);
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

    //新节点插在父节点子树的末尾。父节点的子树一直延伸到数组末尾时（深度优先建树基本都是
    //这种情况）直接追加，不必逐个扫描子树
    size_t insertAt = m_nodes.size();
    if (!SubtreeReachesEnd_(parentIdx))
    {
        insertAt = parentIdx + 1;
        while (insertAt < m_nodes.size() && m_nodes[insertAt].depth > parentDepth)
        {
            ++insertAt;
        }
    }
    const bool appended = (insertAt == m_nodes.size());

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

    if (appended)
    {
        //追加在末尾：其余节点的下标都没变，父下标不用修正，对照表只需补上表尾
        AppendIdIndex_(n.id, (int)insertAt);
    }
    else
    {
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
        //插入点之后的节点下标整体后移一位，对照表过期，下次查找时整体重建
        m_idIndexDirty = true;
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

    // Drop custom-control entries for removed nodes (auto-destroys via unique_ptr)。
    for (size_t k = idx; k < end; ++k)
    {
        m_customControls.erase(m_nodes[k].id);
        ReleaseGrayIcon_(m_nodes[k].id);
    }

    m_nodes.erase(m_nodes.begin() + idx, m_nodes.begin() + end);
    //删除区间之后的节点下标整体前移，对照表过期，下次查找时整体重建
    m_idIndexDirty = true;

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
    m_visibleDirty = false;   //空表就是正确的可见行表
    //对照表从当前的 m_nextId 重新起算：旧 id 都小于新的起点，查找时直接判为不存在
    m_idToIndex.clear();
    m_idBase       = m_nextId;
    m_idIndexDirty = false;
    m_selCells.clear();
    m_customControls.clear();   // 销毁所有节点自绘控件
    ReleaseAllGrayIcons_();     // 释放所有灰显节点的灰度图标
    m_curSelId   = -1;
    m_hoverId    = -1;
    m_focusCell  = CellRef{ -1, -1 };
    Invalidate();
}

bool DuiTreeView::MoveRoot(int id, int beforeId)
{
    const int idx = IndexOf(id);
    if (idx < 0 || m_nodes[(size_t)idx].depth != 0)
    {
        return false;   //不存在或不是根节点
    }
    int beforeIdx = -1;
    if (beforeId != -1)
    {
        beforeIdx = IndexOf(beforeId);
        if (beforeIdx < 0 || m_nodes[(size_t)beforeIdx].depth != 0)
        {
            return false;   //目标位置的根节点不存在或不是根节点
        }
    }
    if (id == beforeId)
    {
        return true;   //移到自己前面：位置不变
    }

    //要移动的是 [idx, end) 这一段：根节点加上它的整棵子树
    size_t end = (size_t)idx + 1;
    while (end < m_nodes.size() && m_nodes[end].depth > 0)
    {
        ++end;
    }

    //目标位置（移动前的数组下标）：beforeId 所在处，或数组末尾。beforeId 是另一个根节点，
    //一定不落在 [idx, end) 之内
    const size_t target = (beforeId == -1) ? m_nodes.size() : (size_t)beforeIdx;
    if (target == end)
    {
        return true;   //这段本来就紧挨在目标位置之前
    }

    //整段轮转到目标位置：目标在前面时把 [target, idx) 与这段对调，在后面时把这段与 [end, target) 对调。
    //下标发生变化的只有 [lo, hi) 这一段，其余节点原地不动
    size_t lo = 0;
    size_t hi = 0;
    if (target < (size_t)idx)
    {
        std::rotate(m_nodes.begin() + target, m_nodes.begin() + idx, m_nodes.begin() + end);
        lo = target;
        hi = end;
    }
    else
    {
        std::rotate(m_nodes.begin() + idx, m_nodes.begin() + end, m_nodes.begin() + target);
        lo = (size_t)idx;
        hi = target;
    }

    //只修正变动那一段的父下标与 id 对照表：把会话挪到最前面时，这一段的长度就是它原来的
    //排名，常聊的会话很短。段外节点的父节点也都在段外，不受影响
    RecomputeParentIdx_(lo, hi);
    UpdateIdIndexRange_(lo, hi);
    RebuildVisible();
    Invalidate();
    return true;
}

void DuiTreeView::RecomputeParentIdx_(size_t lo, size_t hi)
{
    //先序数组里，深度为 d 的节点的父节点，就是它之前最近的那个深度为 d-1 的节点。
    //lo 必须是某个根节点的起点，段内的父子关系才不会指到段外
    std::vector<int> lastAtDepth;
    for (size_t i = lo; i < hi && i < m_nodes.size(); ++i)
    {
        const int depth = m_nodes[i].depth;
        if ((int)lastAtDepth.size() <= depth)
        {
            lastAtDepth.resize((size_t)depth + 1, -1);
        }
        m_nodes[i].parentIdx = (depth > 0) ? lastAtDepth[(size_t)depth - 1] : -1;
        lastAtDepth[(size_t)depth] = (int)i;
    }
}

void DuiTreeView::UpdateIdIndexRange_(size_t lo, size_t hi)
{
    if (m_idIndexDirty)
    {
        return;   //对照表已过期，下次查找时整体重建即可
    }
    for (size_t i = lo; i < hi && i < m_nodes.size(); ++i)
    {
        const int slot = m_nodes[i].id - m_idBase;
        if (slot < 0 || slot >= (int)m_idToIndex.size())
        {
            m_idIndexDirty = true;   //不应发生：交给整体重建
            return;
        }
        m_idToIndex[(size_t)slot] = (int)i;
    }
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

// 收集当前所有处于展开态的节点 id。仅遍历 m_nodes（不看 m_visible），
// 因为"折叠的祖先底下被折叠的子节点也算 expanded=true"（结构语义）。
std::vector<int> DuiTreeView::GetExpandedSnapshot() const
{
    std::vector<int> ids;
    ids.reserve(m_nodes.size());
    for (const auto& n : m_nodes)
    {
        if (n.expanded)
        {
            ids.push_back(n.id);
        }
    }
    return ids;
}

// 增量恢复：对 snapshot 中存在的节点调 Expand；不在 snapshot 里的节点
// 保持当前展开状态不变。snapshot 里的过期 id 静默跳过。
// 一次性 RebuildVisible，避免 N 次 SetExpanded 触发 N 次 Invalidate。
void DuiTreeView::RestoreExpanded(const std::vector<int>& ids)
{
    if (ids.empty())
    {
        return;
    }
    bool anyChanged = false;
    for (int id : ids)
    {
        int idx = IndexOf(id);
        if (idx < 0)
        {
            continue;
        }
        if (!m_nodes[idx].expanded)
        {
            m_nodes[idx].expanded = true;
            anyChanged = true;
        }
    }
    if (anyChanged)
    {
        RebuildVisible();
        EnsureScrollRanges();
        Invalidate();
    }
}

// 单节点可见性标志。隐藏 = 连同后代不出现在 m_visible 列表；数据保留。
void DuiTreeView::SetItemVisible(int id, bool visible)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    if (m_nodes[idx].visible == visible)
    {
        return;
    }
    m_nodes[idx].visible = visible;
    RebuildVisible();
    EnsureScrollRanges();
    Invalidate();
}

void DuiTreeView::SetItemSelectable(int id, bool selectable)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    if (m_nodes[idx].selectable == selectable)
    {
        return;
    }
    m_nodes[idx].selectable = selectable;
    // 把变成不可选的节点从当前选中状态里摘掉,避免残留高亮。
    if (!selectable && m_curSelId == id)
    {
        m_curSelId = -1;
    }
    Invalidate();
}

bool DuiTreeView::IsItemVisible(int id) const
{
    int idx = IndexOf(id);
    return (idx >= 0) ? m_nodes[idx].visible : false;
}

// 设全局过滤器；nullptr / 空 lambda 等价于"无过滤器"。
void DuiTreeView::SetFilter(std::function<bool(int)> filter)
{
    m_filter = std::move(filter);
    RebuildVisible();
    EnsureScrollRanges();
    Invalidate();
}

void DuiTreeView::ClearFilter()
{
    if (!m_filter)
    {
        return;
    }
    m_filter = nullptr;
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

// 设置节点副标签（单列模式两行节点）。仅单列模式生效；多列模式调用本接口
// 会更新 Node.subLabel 字段但不会被 PaintRow 读取（多列模式 PaintBody
// 完全不看 subLabel）。
void DuiTreeView::SetItemSubLabel(int id, LPCTSTR sub)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    m_nodes[idx].subLabel = sub ? sub : _T("");
    Invalidate();
}

CString DuiTreeView::GetItemSubLabel(int id) const
{
    int idx = IndexOf(id);
    return idx < 0 ? CString() : m_nodes[idx].subLabel;
}

// 设置副标签字号；pt <= 0 取默认 8pt。
void DuiTreeView::SetSubLabelPointSize(int pt)
{
    int v = (pt > 0) ? pt : 8;
    if (m_subLabelPt == v)
    {
        return;
    }
    m_subLabelPt = v;
    Invalidate();
}

// 设置副标签文字颜色。
void DuiTreeView::SetSubLabelTextColor(COLORREF c)
{
    if (m_clrSubLabelText == c)
    {
        return;
    }
    m_clrSubLabelText = c;
    Invalidate();
}

void DuiTreeView::SetItemIcon(int id, HBITMAP icon)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    if (m_nodes[idx].icon != icon)
    {
        // 图标换了，按旧图标转出的灰度图作废，下次绘制按新图标重转
        ReleaseGrayIcon_(id);
    }
    m_nodes[idx].icon = icon;
    Invalidate();
}

HBITMAP DuiTreeView::GetItemIcon(int id) const
{
    int idx = IndexOf(id);
    return idx < 0 ? nullptr : m_nodes[idx].icon;
}

// 设置节点 icon 灰显标志（仅单列模式生效；多列模式 PaintBody 不读本字段）。
void DuiTreeView::SetItemIconGrayed(int id, bool grayed)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    if (m_nodes[idx].iconGrayed == grayed)
    {
        return;
    }
    m_nodes[idx].iconGrayed = grayed;
    if (!grayed)
    {
        // 恢复彩色后不再需要灰度图，及早释放；再次灰显时重转一次
        ReleaseGrayIcon_(id);
    }
    Invalidate();
}

HBITMAP DuiTreeView::GrayIconFor_(const Node& n, HDC hdc, int w, int h)
{
    std::map<int, GrayIconCache>::iterator it = m_grayIcons.find(n.id);
    if (it != m_grayIcons.end())
    {
        if (it->second.gray != nullptr && it->second.source == n.icon
            && it->second.premultiplied == m_iconUsesAlpha)
        {
            return it->second.gray;
        }
        ReleaseGrayIcon_(n.id);
    }
    if (n.icon == nullptr || w <= 0 || h <= 0)
    {
        return nullptr;
    }

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;   // 自上而下
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = ::CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib == nullptr || bits == nullptr)
    {
        if (dib != nullptr)
        {
            ::DeleteObject(dib);
        }
        return nullptr;
    }

    // 把原图标复制进 DIB，再逐像素按 NTSC 亮度系数 (R*299+G*587+B*114)/1000 转成灰度，alpha 字节保留。
    // 透明通道模式下图标是预乘值：亮度是线性组合，按预乘值算出的就是预乘后的灰度，不能再乘一次 alpha
    // （2026-10-04 之前多乘了一次，半透明的边缘像素偏暗）。
    HDC dstDC = ::CreateCompatibleDC(hdc);
    HDC srcDC = ::CreateCompatibleDC(hdc);
    HGDIOBJ oldDst = ::SelectObject(dstDC, dib);
    HGDIOBJ oldSrc = ::SelectObject(srcDC, n.icon);
    ::BitBlt(dstDC, 0, 0, w, h, srcDC, 0, 0, SRCCOPY);
    ::SelectObject(srcDC, oldSrc);
    ::SelectObject(dstDC, oldDst);
    ::DeleteDC(srcDC);
    ::DeleteDC(dstDC);
    ::GdiFlush();

    DWORD* pixels = static_cast<DWORD*>(bits);
    const int total = w * h;
    for (int i = 0; i < total; ++i)
    {
        const DWORD p = pixels[i];
        const int b = static_cast<int>(p & 0xFF);
        const int g = static_cast<int>((p >> 8) & 0xFF);
        const int r = static_cast<int>((p >> 16) & 0xFF);
        const DWORD gray = static_cast<DWORD>((r * 299 + g * 587 + b * 114) / 1000);
        pixels[i] = (p & 0xFF000000) | (gray << 16) | (gray << 8) | gray;
    }

    GrayIconCache entry;
    entry.source        = n.icon;
    entry.gray          = dib;
    entry.premultiplied = m_iconUsesAlpha;
    m_grayIcons[n.id] = entry;
    ++m_grayIconConversions;
    return dib;
}

void DuiTreeView::ReleaseGrayIcon_(int id)
{
    std::map<int, GrayIconCache>::iterator it = m_grayIcons.find(id);
    if (it == m_grayIcons.end())
    {
        return;
    }
    if (it->second.gray != nullptr)
    {
        ::DeleteObject(it->second.gray);
    }
    m_grayIcons.erase(it);
}

void DuiTreeView::ReleaseAllGrayIcons_()
{
    for (std::map<int, GrayIconCache>::iterator it = m_grayIcons.begin(); it != m_grayIcons.end(); ++it)
    {
        if (it->second.gray != nullptr)
        {
            ::DeleteObject(it->second.gray);
        }
    }
    m_grayIcons.clear();
}

bool DuiTreeView::IsItemIconGrayed(int id) const
{
    int idx = IndexOf(id);
    return idx >= 0 ? m_nodes[idx].iconGrayed : false;
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

// 设置节点状态点位图（32bpp premultiplied alpha；所有权归调用方，本控件只借用）。
// 非空时取代 statusColor 的纯色圆点；nullptr 撤销、退回纯色圆点。
void DuiTreeView::SetItemStatusIcon(int id, HBITMAP icon)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    m_nodes[idx].statusIcon = icon;
    Invalidate();
}

HBITMAP DuiTreeView::GetItemStatusIcon(int id) const
{
    int idx = IndexOf(id);
    return idx < 0 ? nullptr : m_nodes[idx].statusIcon;
}

// 取某节点主标签文字实际占用的矩形。约定见头文件。
//
// 本函数的几何必须与 OnPaint 单列分支里画标签那一段<u>逐行同源</u>：xCursor 的推进
// （缩进 → glyph 区 → icon）、textRight 的收缩（status dot / 右侧辅助文字）、以及
// 主标签的字体与 DT 标志，任何一处对不上，调用方叠的标记就会错位。改动那一段时
// 请同步本函数。
bool DuiTreeView::GetItemLabelTextRect(int id, RECT& out) const
{
    // 多列模式下不画主标签（走 cell 那套），无从给出。
    if (IsMultiCol())
    {
        return false;
    }

    int idx = IndexOf(id);
    if (idx < 0)
    {
        return false;
    }
    const Node& n = m_nodes[idx];
    if (n.label.IsEmpty())
    {
        return false;   // 没有文字，也就没有"文字之后"
    }

    // 该节点当前在第几可见行。被折叠 / 被隐藏时不在 m_visible 里。
    int visRow = -1;
    for (size_t i = 0; i < VisibleRows_().size(); ++i)
    {
        if (VisibleRows_()[i] == idx)
        {
            visRow = (int)i;
            break;
        }
    }
    if (visRow < 0)
    {
        return false;
    }

    // 行矩形 —— 与 OnPaint 单列分支同口径。
    RECT row;
    row.left   = m_rcItem.left;
    row.right  = m_rcItem.right;
    row.top    = m_rcItem.top + visRow * m_rowH;
    row.bottom = row.top + m_rowH;

    // ---- 左边界：缩进 → 展开箭头区 → icon ----
    int xCursor = row.left + n.depth * m_indent;
    xCursor += kGlyphStripPx;
    if (n.icon)
    {
        xCursor += m_iconSizePx + kCellPad;
    }

    // ---- 右边界：先贴右边距，再依次让开 status dot 与右侧辅助文字 ----
    int textRight = row.right - kRightPad;
    if (n.statusIcon != nullptr)
    {
        BITMAP bm = {};
        if (::GetObject(n.statusIcon, sizeof(bm), &bm) != 0
            && bm.bmWidth > 0 && bm.bmHeight > 0)
        {
            textRight = row.right - kRightPad - bm.bmWidth - kCellPad;
        }
    }
    else if (n.statusColor != (COLORREF)CLR_INVALID)
    {
        textRight = row.right - kRightPad - kStatusDotR * 2 - kCellPad;
    }

    if (textRight <= xCursor)
    {
        return false;   // 可用宽度已经被挤没了，谈不上文字矩形
    }

    // 量文字要一个 HDC。本控件是自绘控件、没有自己的 DC，借桌面 DC 量即可 ——
    // 只用来选字体做 GetTextExtentPoint32，不往上面画任何东西。
    HDC hdc = ::GetDC(NULL);
    if (hdc == NULL)
    {
        return false;
    }

    // 右侧辅助文字按自身宽度再收一次 textRight（与绘制路径同序）。
    if (!n.rightText.IsEmpty())
    {
        HFONT rtFont = (m_rightTextPt > 0)
                       ? GetFontByPointSize(m_rightTextPt, false)
                       : GetDefaultFont();
        HFONT oldRtFont = rtFont ? (HFONT)::SelectObject(hdc, rtFont) : nullptr;
        SIZE rtSz = {};
        ::GetTextExtentPoint32(hdc, n.rightText, n.rightText.GetLength(), &rtSz);
        if (oldRtFont)
        {
            ::SelectObject(hdc, oldRtFont);
        }
        textRight = textRight - rtSz.cx - kCellPad;
    }

    if (textRight <= xCursor)
    {
        ::ReleaseDC(NULL, hdc);
        return false;
    }

    // ---- 量主标签 ----
    HFONT useFont = GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    SIZE sz = {};
    ::GetTextExtentPoint32(hdc, n.label, n.label.GetLength(), &sz);
    TEXTMETRIC tm = {};
    ::GetTextMetrics(hdc, &tm);
    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
    ::ReleaseDC(NULL, hdc);

    // 放不下时绘制路径会加省略号截断，实际占满可用宽度到 textRight 为止。
    int textW = sz.cx;
    if (xCursor + textW > textRight)
    {
        textW = textRight - xCursor;
    }

    // ---- 垂直位置：单行居中；两行时主标签贴上半区下沿（DT_BOTTOM）----
    const int lineH = tm.tmHeight;
    int textTop = 0;
    if (n.subLabel.IsEmpty())
    {
        textTop = (row.top + row.bottom - lineH) / 2;
    }
    else
    {
        const int mid = (row.top + row.bottom) / 2;
        textTop = mid - lineH;
    }

    out.left   = xCursor;
    out.right  = xCursor + textW;
    out.top    = textTop;
    out.bottom = textTop + lineH;
    return true;
}

// 设置节点右侧辅助文字（仅单列模式生效）。空字符串 = 不绘。
void DuiTreeView::SetItemRightText(int id, LPCTSTR rightText)
{
    int idx = IndexOf(id);
    if (idx < 0)
    {
        return;
    }
    m_nodes[idx].rightText = rightText ? rightText : _T("");
    Invalidate();
}

CString DuiTreeView::GetItemRightText(int id) const
{
    int idx = IndexOf(id);
    return idx < 0 ? CString() : m_nodes[idx].rightText;
}

// 设置右侧文字字号；pt <= 0 表示用默认字体（与主 label 同字号 9pt）。
void DuiTreeView::SetRightTextPointSize(int pt)
{
    int v = (pt > 0) ? pt : 0;
    if (m_rightTextPt == v)
    {
        return;
    }
    m_rightTextPt = v;
    Invalidate();
}

void DuiTreeView::SetRightTextColor(COLORREF c)
{
    if (m_clrRightText == c)
    {
        return;
    }
    m_clrRightText = c;
    Invalidate();
}

// ================================================================
// 节点自绘控件(custom-node mode)
// ================================================================

// 设置节点自绘控件,接管该节点的内容区绘制和事件。详细语义见 .h 文档。
// 参数 ctrl 为 nullptr 时清除 custom 模式,节点回到 legacy(tree 内置画)。
// 重复设置会自动替换,旧 ctrl 随 unique_ptr 析构。
void DuiTreeView::SetItemCustomControl(int id, std::unique_ptr<DuiControl> ctrl)
{
    if (IndexOf(id) < 0)
    {
        return;
    }
    if (ctrl)
    {
        // 不把 ctrl 加进 m_children(那是 layout 子控件);它存在 m_customControls
        // map 里,由本控件独立 layout / paint。ctrl 不被加进父链,因此 ctrl
        // 内部如需向上 NotifyParent 需自行调用 host 接口;v1 IM 场景下 tree
        // 在点击时已 fire 行级 DUIN_CLICK,ctrl 不需自己发通知。
        m_customControls[id] = std::move(ctrl);
    }
    else
    {
        m_customControls.erase(id);
    }
    Invalidate();
}

DuiControl* DuiTreeView::GetItemCustomControl(int id) const
{
    auto it = m_customControls.find(id);
    return (it != m_customControls.end()) ? it->second.get() : nullptr;
}

// 据 HitInfo 找到该可见行对应的 customControl,Layout 到当前内容矩形再返回。
// 用于 OnLButtonDown / OnMouseMove 等事件 handler 在路由 mouse 事件给 ctrl
// 前确保 ctrl 的 m_rcItem 是最新的。返回 nullptr 表示该行未挂 custom ctrl
// 或行号越界,事件 handler 应回退到 tree 默认逻辑。
DuiControl* DuiTreeView::LayoutCustomCtrlAtRow_(const HitInfo& h)
{
    if (h.visibleRow < 0 || h.visibleRow >= (int)VisibleRows_().size())
    {
        return nullptr;
    }
    int idx = VisibleRows_()[h.visibleRow];
    auto it = m_customControls.find(m_nodes[idx].id);
    if (it == m_customControls.end() || !it->second)
    {
        return nullptr;
    }

    // 计算行内容区(与 PaintRow 中 custom-node 分支一致)。
    RECT row;
    row.left   = m_rcItem.left;
    row.right  = m_rcItem.right;
    row.top    = m_rcItem.top + h.visibleRow * m_rowH;
    row.bottom = row.top + m_rowH;
    int xCursor = row.left + m_nodes[idx].depth * m_indent + kGlyphStripPx;
    RECT rcContent;
    rcContent.left   = xCursor;
    rcContent.right  = row.right - kRightPad;
    rcContent.top    = row.top;
    rcContent.bottom = row.bottom;
    // 用 SetRect 而不是 Layout —— 见 PaintRow 中 custom-mode 分支同样的说明。
    it->second->SetRect(rcContent);
    return it->second.get();
}

// ================================================================
// 节点 icon 全局视觉(尺寸 / 圆角)
// ================================================================

// 设置节点 icon 显示尺寸(逻辑像素,正方形)。clamp 到 [kIconSizePxMin,
// kIconSizePxMax]。影响:单列模式 PaintRow 的 tree icon 绘制 + 多列
// 模式 col 0 tree icon 绘制 + 单列模式 auto-fit 内容宽度计算。
void DuiTreeView::SetIconSize(int px)
{
    if (px < kIconSizePxMin) { px = kIconSizePxMin; }
    if (px > kIconSizePxMax) { px = kIconSizePxMax; }
    if (m_iconSizePx == px)
    {
        return;
    }
    m_iconSizePx = px;
    Invalidate();
}

// 设置节点 icon 圆角半径(逻辑像素)。0 = 直角(原行为);> 0 时绘 icon
// 前用 CreateRoundRectRgn 设剪裁区,使输出位图呈圆角外观。原 HBITMAP
// 不动。负值视作 0,过大值在 PaintRow 内被 clamp 到 icon 一半。
void DuiTreeView::SetIconCornerRadius(int px)
{
    int v = (px < 0) ? 0 : px;
    if (m_iconCornerRadiusPx == v)
    {
        return;
    }
    m_iconCornerRadiusPx = v;
    Invalidate();
}

// 设置 icon 绘制路径。false=旧 StretchBlt+HRGN clip 路径(默认);
// true=新 AlphaBlend 路径,要求 icon 位图为 32bpp premultiplied alpha,
// 圆角由调用方在位图内自绘。详见 SetIconUsesAlpha 声明处注释。
void DuiTreeView::SetIconUsesAlpha(bool useAlpha)
{
    if (m_iconUsesAlpha == useAlpha)
    {
        return;
    }
    m_iconUsesAlpha = useAlpha;
    Invalidate();
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

namespace {

// 单元格内联编辑用的输入框。
//
// 与普通输入框的唯一差别是接管回车与 Esc。在单元格里这两个键的含义是"提交
// 本次编辑"与"放弃本次编辑"，不能按普通输入框的规则转成通知发给宿主窗口 ——
// 那样一来，用户在单元格里按 Esc 会顺带触发整个对话框的取消动作。
//
// 拦截必须在调用基类之前完成：基类一旦发出通知，就无法再撤回。
//
// 早先这两个键是由树自己的 OnKeyDown 处理的，但那段代码从来没有执行过 ——
// 编辑期间键盘焦点在输入框上，按键根本传不到树。改由输入框自己处理之后这条
// 路才真正通了。
class TreeCellEditor : public DuiEdit
{
public:
    // owner：所属的树。本控件不持有它的所有权，其生存期由树保证 ——
    //        本控件是树的子控件，一定先于树析构。
    explicit TreeCellEditor(DuiTreeView* owner)
        : m_pOwner(owner)
    {
    }

    // 按键处理。回车提交、Esc 取消，其余交给基类。
    //   vk：虚拟键码。
    //   flags：按键消息的附加标志位，原样转交基类。
    //   返回：true 表示本控件已消费该按键。
    bool OnKeyDown(UINT vk, UINT flags) override
    {
        if (m_pOwner != NULL && (vk == VK_RETURN || vk == VK_ESCAPE))
        {
            // 注意：下面这两个调用都会把本控件从控件树中移除并析构，也就是说
            // 本对象在调用返回之后已经不存在。因此调用之后<u>不得再访问任何
            // 成员</u>，必须直接返回。宿主一侧是安全的 —— DuiControl 的析构
            // 函数会清除宿主持有的焦点、鼠标捕获、悬停三个指针。
            if (vk == VK_RETURN)
            {
                m_pOwner->CommitEdit();
            }
            else
            {
                m_pOwner->CancelEdit();
            }
            return true;
        }
        return DuiEdit::OnKeyDown(vk, flags);
    }

private:
    DuiTreeView* m_pOwner;   // 所属的树；本控件由树持有，此处不负责它的生存期
};

}   // namespace

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
    std::unique_ptr<DuiEdit> editor(new TreeCellEditor(this));
    editor->SetText(GetCellText(id, col));
    DuiEdit* raw = editor.get();
    DuiControl::AddChild(std::move(editor));
    m_editor  = raw;
    m_editId  = id;
    m_editCol = col;
    PlaceEditor();
    //—— 输入框是无窗口控件，构造完成即可使用，不存在创建子窗口这一步。
    //   此处只需把键盘焦点交给它，使用户可以直接输入；焦点由宿主统一管理，
    //   因此没有宿主时跳过。
    if (m_pHost)
    {
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
    //只做标记，真正的重算推迟到下一次读可见行时（VisibleRows_）。连续添加几千个节点时，
    //原先每加一个就整表重算一遍，是建树耗时随节点数平方增长的主因
    m_visibleDirty = true;
}

void DuiTreeView::RebuildVisibleNow_() const
{
    m_visibleDirty = false;
    m_visible.clear();
    //hideUntilDepth 同时承担两种"跳过子树"语义：
    //  1) 父节点折叠（节点本身仍可见，仅后代跳过）
    //  2) 父节点被 SetItemVisible(false) 或 filter 拒绝（节点本身也跳过）
    //语义合并到同一 depth 阈值：本节点 push 前先判隐藏；push 后再判折叠。
    int hideUntilDepth = -1;
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        const Node& n = m_nodes[i];
        //—— 当前在被折叠/被隐藏的子树范围内：本节点直接跳过
        if (hideUntilDepth >= 0 && n.depth > hideUntilDepth)
        {
            continue;
        }
        hideUntilDepth = -1;

        //—— 节点本身被隐藏（visible flag 或 filter 拒绝）：连同其后代跳过
        bool hiddenByFlag   = !n.visible;
        bool hiddenByFilter = m_filter && !m_filter(n.id);
        if (hiddenByFlag || hiddenByFilter)
        {
            hideUntilDepth = n.depth;
            continue;
        }

        //—— 节点可见：进 m_visible；若处于折叠态，后续后代跳过
        m_visible.push_back((int)i);
        if (!n.expanded)
        {
            hideUntilDepth = n.depth;
        }
    }
}

int DuiTreeView::GetVisibleCount() const
{
    return (int)VisibleRows_().size();
}

int DuiTreeView::GetContentHeight() const
{
    return (int)VisibleRows_().size() * m_rowH;
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
    for (size_t r = 0; r < VisibleRows_().size(); ++r)
    {
        if (VisibleRows_()[r] == idx)
        {
            return (int)r;
        }
    }
    return -1;
}

int DuiTreeView::GetIdAtVisibleRow(int n) const
{
    if (n < 0 || n >= (int)VisibleRows_().size())
    {
        return -1;
    }
    return m_nodes[VisibleRows_()[n]].id;
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
        // 2026-10-04 起滚动条为悬浮式：浮在表体之上、不预留 gutter，表体就是表头以下的整块。
        r.top    += m_headerH;
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
    if (n > (int)VisibleRows_().size()) { n = (int)VisibleRows_().size(); }
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
    // 悬浮式滚动条（2026-10-04 起）不占表体宽高，两个方向是否需要滚动各自只看内容是否
    // 超出表体，彼此不再互相影响（此前内嵌式需要两阶段迭代）。
    int contentW = ComputeContentWidth();
    int contentH = (int)VisibleRows_().size() * m_rowH;
    int fullW = m_rcItem.right - m_rcItem.left;
    int fullH = m_rcItem.bottom - m_rcItem.top - m_headerH;
    m_needHScroll = (contentW > (fullW > 0 ? fullW : 1));
    m_needVScroll = (contentH > (fullH > 0 ? fullH : 1));

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
            if (row < 0 || row >= (int)VisibleRows_().size())
            {
                h.zone = HZ_BODY_BLANK;
                return h;
            }
            h.visibleRow = row;
            h.itemId     = m_nodes[VisibleRows_()[row]].id;

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
            if (h.col == 0 && HasChildrenIdx(VisibleRows_()[row]))
            {
                int depth = m_nodes[VisibleRows_()[row]].depth;
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
    if (VisibleRows_().empty())
    {
        return h;
    }
    int row = (pt.y - m_rcItem.top) / m_rowH;
    if (row < 0 || row >= (int)VisibleRows_().size())
    {
        return h;
    }
    h.visibleRow = row;
    h.itemId     = m_nodes[VisibleRows_()[row]].id;
    int idx = VisibleRows_()[row];
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
        // 悬浮式滚动条（2026-10-04 起）：命中带浮在表体右缘 / 下缘之上。两条都出现时，
        // 水平的一条在右端让出竖直那条的宽度，两条命中带不重叠。
        RECT rh;
        rh.left   = m_rcItem.left;
        rh.right  = m_rcItem.right - (m_needVScroll ? kScrollBarPx : 0);
        rh.bottom = m_rcItem.bottom;
        rh.top    = rh.bottom - kScrollBarPx;
        RECT rv;
        rv.top    = m_rcItem.top + m_headerH;
        rv.bottom = m_rcItem.bottom - (m_needHScroll ? kScrollBarPx : 0);
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
    if (!m_bVisible || (VisibleRows_().empty() && m_columns.empty()))
    {
        // Background only.
        FillSolidRect_(hdc, m_rcItem, m_clrRowBg);
        return;
    }

    if (!IsMultiCol())
    {
        // Single-col path: same behavior as legacy.
        if (VisibleRows_().empty())
        {
            return;
        }
        int firstRow = (rcDirty.top - m_rcItem.top) / m_rowH;
        if (firstRow < 0) { firstRow = 0; }
        int lastRow  = (rcDirty.bottom - m_rcItem.top + m_rowH - 1) / m_rowH;
        if (lastRow > (int)VisibleRows_().size()) { lastRow = (int)VisibleRows_().size(); }

        for (int r = firstRow; r < lastRow; ++r)
        {
            // Draw row background + content (legacy).
            int idx = VisibleRows_()[r];
            const Node& n = m_nodes[idx];
            RECT row;
            row.left   = m_rcItem.left;
            row.right  = m_rcItem.right;
            row.top    = m_rcItem.top + r * m_rowH;
            row.bottom = row.top + m_rowH;

            // selectable=false 的节点不显示选中底色(即便 m_curSelId
            // 残留指向它);hover 行为也一并跳过,grouping header 不需要 hover 高亮。
            bool selected = (n.id == m_curSelId) && n.selectable;
            bool hovered  = (n.id == m_hoverId) && !selected && n.selectable;
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

            //—— Custom-node mode:节点设了 customControl 时 tree 不再画
            //   icon / label / sub-label / right-text / status dot,把内容区
            //   (glyph + indent 右侧)交给 ctrl 自画。tree 已画 row bg + glyph
            //   + indent,因此 ctrl 看到的就是它要负责的内容矩形。
            //   ctrl->Layout(rcContent) 让控件按矩形排自身子树;OnPaint 直接
            //   叠加到 hdc。Mouse 事件路由由 OnXxx 处理器另行 forward。
            {
                std::map<int, std::unique_ptr<DuiControl> >::const_iterator
                    customIt = m_customControls.find(n.id);
                if (customIt != m_customControls.end() &&
                    customIt->second != nullptr)
                {
                    DuiControl* ctrl = customIt->second.get();
                    RECT rcContent;
                    rcContent.left   = xCursor;
                    rcContent.right  = row.right - kRightPad;
                    rcContent.top    = row.top;
                    rcContent.bottom = row.bottom;
                    // 必须用 SetRect 而不是 Layout —— DuiControl::Layout 基类
                    // 默认只把 rcAvail 转给 children, 不会把 m_rcItem 自身
                    // 更新; 用 SetRect 才会写 m_rcItem 并顺便调用 Layout。
                    // 否则 ctrl->OnPaint 时 m_rcItem 是初始 0 矩形, 画不出东西。
                    ctrl->SetRect(rcContent);
                    ctrl->OnPaint(hdc, rcDirty);
                    continue;   // 跳过本行内置绘制
                }
            }

            if (n.icon)
            {
                int iconY = (row.top + row.bottom - m_iconSizePx) / 2;
                BITMAP bm = {};
                ::GetObject(n.icon, sizeof(bm), &bm);
                if (bm.bmWidth > 0 && bm.bmHeight > 0)
                {
                    if (!m_iconUsesAlpha)
                    {
                        //==== 旧路径(向后兼容,m_iconUsesAlpha=false 默认走这里)====
                        //—— 圆角剪裁:m_iconCornerRadiusPx > 0 时,绘 icon 前把
                        //   剪裁区设为 icon 矩形的圆角区域;绘完恢复原剪裁。
                        //   纯 GDI 实现,与下方两条 StretchBlt 路径(灰显 / 原色)
                        //   共用同一剪裁,逻辑一次写完。clipR 被 clamp 到 icon
                        //   的一半,避免半径超出 icon 自身导致圆角不可见。
                        //   用 SaveDC + ExtSelectClipRgn(RGN_AND) + RestoreDC ——
                        //   RGN_AND 是相交语义(SelectClipRgn 是替换语义,会撤销
                        //   外层 ScrollView 等设的 clip);圆角是 round rect,所以
                        //   用 ExtSelectClipRgn 而不是矩形版 IntersectClipRect。
                        int iconClipSaved = -1;
                        if (m_iconCornerRadiusPx > 0)
                        {
                            int clipR = m_iconCornerRadiusPx;
                            if (clipR > m_iconSizePx / 2)
                            {
                                clipR = m_iconSizePx / 2;
                            }
                            iconClipSaved = ::SaveDC(hdc);
                            HRGN clipRgn = ::CreateRoundRectRgn(
                                xCursor, iconY,
                                xCursor + m_iconSizePx + 1,    // CreateRoundRectRgn 右下排他
                                iconY   + m_iconSizePx + 1,
                                clipR * 2, clipR * 2);
                            ::ExtSelectClipRgn(hdc, clipRgn, RGN_AND);
                            ::DeleteObject(clipRgn);
                        }

                        if (n.iconGrayed)
                        {
                            //—— 灰显路径：取缓存的灰度图（首次绘制时由 GrayIconFor_
                            //   转换一次），再 StretchBlt 到目标。原 HBITMAP 不动。
                            HBITMAP grayBmp = GrayIconFor_(n, hdc, bm.bmWidth, bm.bmHeight);
                            if (grayBmp != nullptr)
                            {
                                HDC memDC = ::CreateCompatibleDC(hdc);
                                HGDIOBJ oldDib = ::SelectObject(memDC, grayBmp);
                                ::SetStretchBltMode(hdc, HALFTONE);
                                ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                                ::StretchBlt(hdc, xCursor, iconY,
                                             m_iconSizePx, m_iconSizePx,
                                             memDC, 0, 0, bm.bmWidth, bm.bmHeight,
                                             SRCCOPY);
                                ::SelectObject(memDC, oldDib);
                                ::DeleteDC(memDC);
                            }
                        }
                        else
                        {
                            //—— 原路径：直接 StretchBlt，与改造前完全一致。
                            HDC memDC = ::CreateCompatibleDC(hdc);
                            HGDIOBJ old = ::SelectObject(memDC, n.icon);
                            ::SetStretchBltMode(hdc, HALFTONE);
                            ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                            ::StretchBlt(hdc, xCursor, iconY,
                                         m_iconSizePx, m_iconSizePx,
                                         memDC, 0, 0, bm.bmWidth, bm.bmHeight,
                                         SRCCOPY);
                            ::SelectObject(memDC, old);
                            ::DeleteDC(memDC);
                        }

                        //—— 恢复剪裁
                        if (iconClipSaved >= 0)
                        {
                            ::RestoreDC(hdc, iconClipSaved);
                        }
                    }
                    else
                    {
                        //==== 新 alpha 路径(m_iconUsesAlpha=true)====
                        // 要求 n.icon 是 32bpp premultiplied alpha 位图;圆角已
                        // 由调用方在位图内自绘(GDI+ AA),DuiTreeView 不再做
                        // HRGN clip——避免硬边锯齿。SetIconCornerRadius 在本
                        // 路径下被忽略。
                        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                        HDC memDC = ::CreateCompatibleDC(hdc);

                        if (n.iconGrayed)
                        {
                            //—— 灰显 + alpha 路径：取缓存的灰度图（预乘值，首次绘制时
                            //   由 GrayIconFor_ 转换一次），输出用 AlphaBlend 而非
                            //   StretchBlt，让边缘 alpha 渐变正确合成。
                            HBITMAP grayBmp = GrayIconFor_(n, hdc, bm.bmWidth, bm.bmHeight);
                            if (grayBmp != nullptr)
                            {
                                HGDIOBJ oldDib = ::SelectObject(memDC, grayBmp);
                                ::AlphaBlend(hdc, xCursor, iconY,
                                             m_iconSizePx, m_iconSizePx,
                                             memDC, 0, 0, bm.bmWidth, bm.bmHeight,
                                             bf);
                                ::SelectObject(memDC, oldDib);
                            }
                        }
                        else
                        {
                            HGDIOBJ old = ::SelectObject(memDC, n.icon);
                            ::AlphaBlend(hdc, xCursor, iconY,
                                         m_iconSizePx, m_iconSizePx,
                                         memDC, 0, 0, bm.bmWidth, bm.bmHeight,
                                         bf);
                            ::SelectObject(memDC, old);
                        }
                        ::DeleteDC(memDC);
                    }
                }
                xCursor += m_iconSizePx + kCellPad;
            }

            int textRight = row.right - kRightPad;
            if (n.statusIcon != nullptr)
            {
                //—— 状态点位图路径：调用方已经把形状画好（如 IM 的在线 / 离开 /
                //   忙碌 / 离线徽标），这里只摆位与合成。按位图自身尺寸绘制、不缩放，
                //   贴行右端并垂直居中；位图须是 32bpp premultiplied alpha，故走
                //   AlphaBlend(AC_SRC_OVER + AC_SRC_ALPHA)，与 alpha 路径的 tree
                //   icon 同款。位图非法（GetObject 失败 / 尺寸为 0）时什么都不画，
                //   也不回退到纯色圆点——调用方明确指定了位图，静默换一种画法反而
                //   更难排查。
                BITMAP bm = {};
                if (::GetObject(n.statusIcon, sizeof(bm), &bm) != 0
                    && bm.bmWidth > 0 && bm.bmHeight > 0)
                {
                    int cy   = (row.top + row.bottom) / 2;
                    int left = row.right - kRightPad - bm.bmWidth;
                    int top  = cy - bm.bmHeight / 2;

                    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                    HDC     memDC = ::CreateCompatibleDC(hdc);
                    HGDIOBJ old   = ::SelectObject(memDC, n.statusIcon);
                    ::AlphaBlend(hdc, left, top, bm.bmWidth, bm.bmHeight,
                                 memDC, 0, 0, bm.bmWidth, bm.bmHeight, bf);
                    ::SelectObject(memDC, old);
                    ::DeleteDC(memDC);

                    textRight = left - kCellPad;
                }
            }
            else if (n.statusColor != (COLORREF)CLR_INVALID)
            {
                int cy = (row.top + row.bottom) / 2;
                int cx = row.right - kRightPad - kStatusDotR;
                RECT dot = { cx - kStatusDotR, cy - kStatusDotR,
                             cx + kStatusDotR, cy + kStatusDotR };
                DuiAA::FillEllipse(hdc, dot, n.statusColor, CLR_INVALID);
                textRight = dot.left - kCellPad;
            }

            //—— 右侧辅助文字：放在 status dot 内侧（或贴右边距），按
            //   实际宽度收缩 textRight，让主 label 自动 ellipsis。
            if (!n.rightText.IsEmpty())
            {
                HFONT useFont = (m_rightTextPt > 0)
                                ? GetFontByPointSize(
                                      m_rightTextPt, false)
                                : GetDefaultFont();
                HFONT oldFont = useFont
                                ? (HFONT)::SelectObject(hdc, useFont)
                                : nullptr;
                int oldBk = ::SetBkMode(hdc, TRANSPARENT);
                COLORREF rtClr = selected ? m_clrTextSel : m_clrRightText;
                COLORREF oldClr = ::SetTextColor(hdc, rtClr);
                //—— 量字符串实际宽度，避免画太宽的辅助文字
                SIZE sz = {};
                ::GetTextExtentPoint32(hdc, n.rightText,
                                       n.rightText.GetLength(), &sz);
                RECT rcRt = { textRight - sz.cx, row.top,
                              textRight, row.bottom };
                ::DrawText(hdc, n.rightText, -1, &rcRt,
                           DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                ::SetTextColor(hdc, oldClr);
                ::SetBkMode(hdc, oldBk);
                if (oldFont) { ::SelectObject(hdc, oldFont); }
                textRight = rcRt.left - kCellPad;
            }

            if (!n.label.IsEmpty())
            {
                HFONT useFont = GetDefaultFont();
                HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
                int oldBk = ::SetBkMode(hdc, TRANSPARENT);
                COLORREF txt = selected ? m_clrTextSel : m_clrText;
                COLORREF oldClr = ::SetTextColor(hdc, txt);
                RECT rt = row;
                rt.left  = xCursor;
                rt.right = textRight;

                if (n.subLabel.IsEmpty())
                {
                    // 单行：主标签垂直居中（保持向后兼容）。
                    ::DrawText(hdc, n.label, -1, &rt,
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE
                               | DT_END_ELLIPSIS | DT_NOPREFIX);
                }
                else
                {
                    // 两行：主标签上半（贴 mid 线）+ 副标签下半（贴 mid 线）。
                    // 上下两半各自单行省略，共享 textRight 右边距。
                    int mid = (rt.top + rt.bottom) / 2;
                    RECT rtMain = { rt.left, rt.top, rt.right, mid };
                    RECT rtSub  = { rt.left, mid,   rt.right, rt.bottom };

                    //—— 主标签：贴下沿（DT_BOTTOM）让它靠近 mid 线
                    ::DrawText(hdc, n.label, -1, &rtMain,
                               DT_LEFT | DT_BOTTOM | DT_SINGLELINE
                               | DT_END_ELLIPSIS | DT_NOPREFIX);

                    //—— 副标签：换字体、换颜色（选中态用 m_clrTextSel
                    //   保持白色统一，否则用 m_clrSubLabelText 浅灰拉层级），
                    //   贴上沿（DT_TOP）紧挨主标签。
                    HFONT subFont = GetFontByPointSize(m_subLabelPt, false);
                    HFONT oldSubFont = subFont
                                       ? (HFONT)::SelectObject(hdc, subFont)
                                       : nullptr;
                    COLORREF subClr = selected ? m_clrTextSel
                                               : m_clrSubLabelText;
                    COLORREF oldSubClr = ::SetTextColor(hdc, subClr);
                    ::DrawText(hdc, n.subLabel, -1, &rtSub,
                               DT_LEFT | DT_TOP | DT_SINGLELINE
                               | DT_END_ELLIPSIS | DT_NOPREFIX);
                    ::SetTextColor(hdc, oldSubClr);
                    if (oldSubFont)
                    {
                        ::SelectObject(hdc, oldSubFont);
                    }
                }

                ::SetTextColor(hdc, oldClr);
                ::SetBkMode(hdc, oldBk);
                if (oldFont) { ::SelectObject(hdc, oldFont); }
            }
        }

        //—— 绘制子控件（单列模式下只可能是内联编辑用的输入框）。
        //   这一步不能省略：输入框已改为无窗口实现，它的像素完全由本控件的
        //   绘制流程画出，父控件不绘制它就不会出现在界面上。旧实现在控件内部
        //   嵌一个 Win32 输入框子窗口，由操作系统绘制在整个界面之上，因此
        //   当年此处不绘制也没有影响。多列路径末尾的同名调用承担同一职责。
        //
        //   与滚动条不存在重复绘制：单列模式下本控件不带自己的滚动条 ——
        //   EnsureScrollRanges 在单列模式开头即返回，不会创建滚动条；
        //   ClearColumns 从多列切回单列时也会移除已建出的两条滚动条。因此
        //   单列模式下 m_children 中能够存在的只有内联编辑的输入框一个。
        DuiControl::OnPaint(hdc, rcDirty);
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

    HFONT useFont = GetDefaultFont();
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
    // 用 SaveDC + IntersectClipRect + RestoreDC 而不是 SelectClipRgn ——
    // 后者是替换语义,把外层(如 ScrollView)设的 clip 一起撤销;尤其原本
    // 还原成 SelectClipRgn(hdc, nullptr) 会完全清空 clip,外层任何 clip
    // 都被丢掉,内容能画到 m_rcItem 之外。
    {
        int dcSaved = ::SaveDC(hdc);
        ::IntersectClipRect(hdc, rcHeader.left, rcHeader.top, frozenSplit, rcHeader.bottom);
        int x = rcHeader.left;
        for (int i = 0; i < m_frozenCols && i < (int)m_columns.size(); ++i)
        {
            paintColHeader(i, x);
            x += m_columns[i].width;
        }
        ::RestoreDC(hdc, dcSaved);
    }
    // Non-frozen header area: scrolls horizontally. 同上理由。
    {
        int dcSaved = ::SaveDC(hdc);
        ::IntersectClipRect(hdc, frozenSplit, rcHeader.top, rcHeader.right, rcHeader.bottom);
        int x = frozenSplit - m_scrollX;
        for (int i = m_frozenCols; i < (int)m_columns.size(); ++i)
        {
            paintColHeader(i, x);
            x += m_columns[i].width;
        }
        ::RestoreDC(hdc, dcSaved);
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

        // 同 Header 的说明:SaveDC + IntersectClipRect + RestoreDC,避免
        // 替换语义撤销外层(ScrollView)的 clip。
        int quadSaved = ::SaveDC(hdc);
        ::IntersectClipRect(hdc, rcQuad.left, rcQuad.top, rcQuad.right, rcQuad.bottom);

        for (int r = rowFrom; r < rowTo && r < (int)VisibleRows_().size(); ++r)
        {
            int idx = VisibleRows_()[r];
            const Node& n = m_nodes[idx];
            RECT row;
            row.left   = xOriginAbs + ColumnLeftAbs(colFrom);
            row.right  = xOriginAbs + ColumnLeftAbs(colTo);
            row.top    = yOriginAbs + r * m_rowH;
            row.bottom = row.top + m_rowH;

            // selectable=false 的节点不显示选中底色(即便 m_curSelId
            // 残留指向它);hover 行为也一并跳过,grouping header 不需要 hover 高亮。
            bool selected = (n.id == m_curSelId) && n.selectable;
            bool hovered  = (n.id == m_hoverId) && !selected && n.selectable;
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

        ::RestoreDC(hdc, quadSaved);
    };

    int frozenRows = m_frozenRows;
    if (frozenRows > (int)VisibleRows_().size()) { frozenRows = (int)VisibleRows_().size(); }
    int totalRows  = (int)VisibleRows_().size();
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
            // 多列模式 col 0 的 tree icon —— 同样响应 SetIconSize / SetIconCornerRadius
            // /SetIconUsesAlpha 全局视觉(与单列模式一致)。
            int iconY = (rcCell.top + rcCell.bottom - m_iconSizePx) / 2;

            BITMAP bm = {};
            ::GetObject(n.icon, sizeof(bm), &bm);

            if (!m_iconUsesAlpha)
            {
                //==== 旧路径(向后兼容,m_iconUsesAlpha=false 默认走这里)====
                // 同单列模式的说明:SaveDC + ExtSelectClipRgn(RGN_AND) + RestoreDC,
                // 圆角 round rect 相交语义,不撤销外层 clip。
                int iconClipSaved = -1;
                if (m_iconCornerRadiusPx > 0)
                {
                    int clipR = m_iconCornerRadiusPx;
                    if (clipR > m_iconSizePx / 2)
                    {
                        clipR = m_iconSizePx / 2;
                    }
                    iconClipSaved = ::SaveDC(hdc);
                    HRGN clipRgn = ::CreateRoundRectRgn(
                        xCursor, iconY,
                        xCursor + m_iconSizePx + 1,
                        iconY   + m_iconSizePx + 1,
                        clipR * 2, clipR * 2);
                    ::ExtSelectClipRgn(hdc, clipRgn, RGN_AND);
                    ::DeleteObject(clipRgn);
                }

                HDC memDC = ::CreateCompatibleDC(hdc);
                HGDIOBJ old = ::SelectObject(memDC, n.icon);
                if (bm.bmWidth > 0 && bm.bmHeight > 0)
                {
                    ::SetStretchBltMode(hdc, HALFTONE);
                    ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                    ::StretchBlt(hdc, xCursor, iconY, m_iconSizePx, m_iconSizePx,
                                 memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                }
                ::SelectObject(memDC, old);
                ::DeleteDC(memDC);

                if (iconClipSaved >= 0)
                {
                    ::RestoreDC(hdc, iconClipSaved);
                }
            }
            else
            {
                //==== 新 alpha 路径(m_iconUsesAlpha=true)====
                // n.icon 应为 32bpp premultiplied alpha;圆角在位图里已自绘,
                // DuiTreeView 不做 HRGN clip。SetIconCornerRadius 在本路径
                // 下被忽略。多列分支无灰显概念,直接 AlphaBlend。
                if (bm.bmWidth > 0 && bm.bmHeight > 0)
                {
                    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                    HDC memDC = ::CreateCompatibleDC(hdc);
                    HGDIOBJ old = ::SelectObject(memDC, n.icon);
                    ::AlphaBlend(hdc, xCursor, iconY,
                                 m_iconSizePx, m_iconSizePx,
                                 memDC, 0, 0, bm.bmWidth, bm.bmHeight, bf);
                    ::SelectObject(memDC, old);
                    ::DeleteDC(memDC);
                }
            }

            xCursor += m_iconSizePx + kCellPad;
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
    HFONT useFont = GetDefaultFont();
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
    HFONT hf = GetDefaultFont();
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

    // Custom-node mode: 落在 custom ctrl 内容区,先把事件转给 ctrl;
    // ctrl 消耗(返 true)则不再走 tree 默认逻辑(包括选中行)。
    if (h.zone == HZ_BODY_CELL)
    {
        DuiControl* customCtrl = LayoutCustomCtrlAtRow_(h);
        if (customCtrl && customCtrl->OnLButtonDown(pt, mkFlags))
        {
            return true;
        }
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
        SetExpanded(h.itemId, !m_nodes[VisibleRows_()[h.visibleRow]].expanded);
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
                for (int r = rA; r <= rB && r < (int)VisibleRows_().size(); ++r)
                {
                    int id = m_nodes[VisibleRows_()[r]].id;
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
            // selectable=false 的节点(grouping header):点击只 toggle 展开,
            // 不变选中态。这样 "我的好友" / "服务号" 这类分组标题点击不会
            // 出现选中底色,与 design 一致。
            int nodeIdx = IndexOf(h.itemId);
            if (nodeIdx >= 0 && !m_nodes[nodeIdx].selectable)
            {
                if (HasChildrenIdx(nodeIdx))
                {
                    SetExpanded(h.itemId, !m_nodes[nodeIdx].expanded);
                }
            }
            else
            {
                SetCurSel(h.itemId, /*notify=*/true);
            }
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

bool DuiTreeView::OnLButtonUp(POINT pt, UINT mkFlags)
{
    if (m_resizing)
    {
        m_resizing = false;
        ReleaseCapture();
        return true;
    }
    HitInfo h = HitTest_(pt);

    // Custom-node mode: 转发给 ctrl;ctrl 消耗则跳过 DUIN_CLICK fire。
    if (h.zone == HZ_BODY_CELL)
    {
        DuiControl* customCtrl = LayoutCustomCtrlAtRow_(h);
        if (customCtrl && customCtrl->OnLButtonUp(pt, mkFlags))
        {
            return true;
        }
    }

    if (h.zone == HZ_BODY_CELL && !m_editor)
    {
        NotifyParent(DUIN_CLICK, (LPARAM)h.itemId);
    }
    return true;
}

bool DuiTreeView::OnLButtonDblClk(POINT pt, UINT mkFlags)
{
    HitInfo h = HitTest_(pt);

    // Custom-node mode: 转发给 ctrl;ctrl 消耗则跳过 DUIN_DBLCLK fire / 编辑触发。
    if (h.zone == HZ_BODY_CELL)
    {
        DuiControl* customCtrl = LayoutCustomCtrlAtRow_(h);
        if (customCtrl && customCtrl->OnLButtonDblClk(pt, mkFlags))
        {
            return true;
        }
    }

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
            HFONT useFont = GetDefaultFont();
            HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
            SIZE sz;
            if (::GetTextExtentPoint32(hdc, m_columns[h.col].title,
                                       m_columns[h.col].title.GetLength(), &sz))
            {
                maxW = sz.cx;
            }
            for (int r = 0; r < (int)VisibleRows_().size(); ++r)
            {
                CString text = GetCellText(m_nodes[VisibleRows_()[r]].id, h.col);
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
            extra += maxDepth * m_indent + kGlyphStripPx + m_iconSizePx + kCellPad;
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

bool DuiTreeView::OnRButtonDown(POINT pt, UINT mkFlags)
{
    HitInfo h = HitTest_(pt);

    // Custom-node mode: 转发给 ctrl;ctrl 消耗则跳过 DUITVN_RCLICK fire。
    if (h.zone == HZ_BODY_CELL)
    {
        DuiControl* customCtrl = LayoutCustomCtrlAtRow_(h);
        if (customCtrl && customCtrl->OnRButtonDown(pt, mkFlags))
        {
            return true;
        }
    }

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

bool DuiTreeView::OnMouseMove(POINT pt, UINT mkFlags)
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

    // Custom-node mode: 转发 mouse move 给 ctrl(让 ctrl 维护自己的 hover 态)。
    // 不论 ctrl 是否消耗,tree 都继续更新行级 m_hoverId 让行高亮正常工作。
    if (h.zone == HZ_BODY_CELL)
    {
        DuiControl* customCtrl = LayoutCustomCtrlAtRow_(h);
        if (customCtrl)
        {
            customCtrl->OnMouseMove(pt, mkFlags);
        }
    }

    int newHover = (h.zone == HZ_BODY_CELL || h.zone == HZ_BODY_GLYPH) ? h.itemId : -1;
    if (newHover != m_hoverId)
    {
        //—— 先记下旧值再更新 m_hoverId，避免 NotifyParent 期间业务 handler
        //   读 GetHoverId（如果将来加 API）拿到错位值。
        int oldHover = m_hoverId;
        m_hoverId = newHover;
        Invalidate();
        //—— 同步 fire hover notify：离开旧节点 + 进入新节点。无延时，
        //   业务侧若要悬停弹卡片自己用 SetTimer 加延时。
        if (oldHover != -1)
        {
            NotifyParent((UINT)DUITVN_HOVER_LEAVE, (LPARAM)oldHover);
        }
        if (newHover != -1)
        {
            NotifyParent((UINT)DUITVN_HOVER_ENTER, (LPARAM)newHover);
        }
    }
    return false;
}

bool DuiTreeView::OnMouseLeave()
{
    if (m_hoverId != -1)
    {
        int oldHover = m_hoverId;
        m_hoverId = -1;
        Invalidate();
        //—— 鼠标离开控件：fire LEAVE，让业务侧关掉悬浮卡片
        NotifyParent((UINT)DUITVN_HOVER_LEAVE, (LPARAM)oldHover);
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
    // 滚动时让对应方向的滚动条淡入（悬浮式滚动条默认自动隐藏）
    if (mkFlags & MK_SHIFT)
    {
        ScrollX(lines * m_rowH);
        if (m_hScroll && m_hScroll->IsVisible())
        {
            static_cast<DuiScrollBar*>(m_hScroll)->TriggerShow();
        }
    }
    else
    {
        ScrollY(lines * m_rowH);
        if (m_vScroll && m_vScroll->IsVisible())
        {
            static_cast<DuiScrollBar*>(m_vScroll)->TriggerShow();
        }
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

    // Custom-node mode: 转发 cursor 设置给 ctrl(让 ctrl 控制自己的鼠标光标,
    // 如内含按钮 / 链接区时显示手型)。ctrl 返 true 表示已设置,tree 不再
    // 尝试默认光标。
    if (h.zone == HZ_BODY_CELL)
    {
        DuiControl* customCtrl = LayoutCustomCtrlAtRow_(h);
        if (customCtrl && customCtrl->OnSetCursor(pt))
        {
            return true;
        }
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
    // 正在内联编辑时，本函数收不到任何按键 —— 键盘焦点在编辑用的输入框上，
    // 而宿主只把按键投给当前焦点控件、不沿控件树上冒。编辑期间的回车与 Esc
    // 由输入框自己处理（见本文件里的 TreeCellEditor）。
    //
    // 这里原先有一段判 m_editor 非空、处理回车与 Esc 的代码，从来没有执行过，
    // 2026-08-17 随输入框无窗口化一并删除。
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
