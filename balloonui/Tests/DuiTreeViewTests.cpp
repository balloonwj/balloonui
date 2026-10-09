#include "stdafx.h"
#include "DuiTreeViewTests.h"

#if BUI_FEATURE_TREEVIEW

#if BUI_FEATURE_SCROLLBAR
#include "../Controls/Window/DuiScrollBar.h"   // TVO1：多列模式的悬浮式滚动条
#endif
#include <random>
#include <stdlib.h>   // abs：颜色分量比较
#include <vector>


namespace balloonwjui {

namespace DuiTreeViewTests {

namespace {

struct Result { CString name; bool ok; CString detail; };
static Result OK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok = true;
    return r;
}
static Result Fail(const CString& n, const CString& d)
{
    Result r;
    r.name = n;
    r.ok = false;
    r.detail = d;
    return r;
}

#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { return Fail(name, _T("string mismatch")); } \
    } while (0)

// ----- structure round-trips ------------------------------------------

static Result Test_AddRootGetCount()
{
    DuiTreeView t;
    int a = t.AddRoot(_T("Friends"));
    int b = t.AddRoot(_T("Groups"));
    EXPECT_TRUE(a > 0 && b > 0 && a != b, _T("Add/uniqueIds"));
    EXPECT_INT(t.GetRootCount(), 2, _T("Add/rootCount"));
    EXPECT_INT(t.GetVisibleCount(), 2, _T("Add/visible"));
    EXPECT_STR(t.GetItemLabel(a), _T("Friends"), _T("Add/labelA"));
    return OK(_T("AddRootGetCount"));
}

// AddChild lands in the right parent + tree-order is preserved
// (children come right after their parent's existing subtree).
static Result Test_AddChildOrder()
{
    DuiTreeView t;
    int root = t.AddRoot(_T("R"));
    int c1 = t.AddChild(root, _T("c1"));
    int c2 = t.AddChild(root, _T("c2"));
    int gc = t.AddChild(c1, _T("gc"));
    EXPECT_INT(t.GetChildCount(root), 2, _T("Order/rootChildren"));
    EXPECT_INT(t.GetChildCount(c1),   1, _T("Order/c1Children"));
    EXPECT_INT(t.GetVisibleCount(),   4, _T("Order/visAllExpanded"));

    // Visible row order: R, c1, gc, c2.
    EXPECT_INT(t.GetIdAtVisibleRow(0), root, _T("Order/r0"));
    EXPECT_INT(t.GetIdAtVisibleRow(1), c1,   _T("Order/r1"));
    EXPECT_INT(t.GetIdAtVisibleRow(2), gc,   _T("Order/r2"));
    EXPECT_INT(t.GetIdAtVisibleRow(3), c2,   _T("Order/r3"));
    return OK(_T("AddChildOrder"));
}

// AddChild with bad parent id returns -1.
static Result Test_AddChildBadParent()
{
    DuiTreeView t;
    int x = t.AddChild(99999, _T("orphan"));
    EXPECT_INT(x, -1, _T("BadParent/-1"));
    EXPECT_INT(t.GetVisibleCount(), 0, _T("BadParent/empty"));
    return OK(_T("AddChildBadParent"));
}

// ----- expand / collapse ----------------------------------------------

static Result Test_CollapseHidesDescendants()
{
    DuiTreeView t;
    int r = t.AddRoot(_T("R"));
    int c1 = t.AddChild(r, _T("c1"));
    int c2 = t.AddChild(r, _T("c2"));
    int gc = t.AddChild(c1, _T("gc"));
    (void)c2;
    (void)gc;
    EXPECT_INT(t.GetVisibleCount(), 4, _T("Coll/initAll"));
    t.Collapse(r);
    EXPECT_INT(t.GetVisibleCount(), 1, _T("Coll/onlyRoot"));
    EXPECT_TRUE(!t.IsExpanded(r), _T("Coll/state"));
    t.Expand(r);
    EXPECT_INT(t.GetVisibleCount(), 4, _T("Coll/reExpand"));

    // Inner collapse preserves outer expand state.
    t.Collapse(c1);
    EXPECT_INT(t.GetVisibleCount(), 3, _T("Coll/innerOnly"));
    EXPECT_INT(t.GetIdAtVisibleRow(2), c2, _T("Coll/innerOrder"));
    return OK(_T("CollapseHidesDescendants"));
}

// ExpandAll / CollapseAll.
static Result Test_ExpandCollapseAll()
{
    DuiTreeView t;
    int r1 = t.AddRoot(_T("R1"));
    int r2 = t.AddRoot(_T("R2"));
    t.AddChild(r1, _T("c1"));
    t.AddChild(r2, _T("c2"));
    t.CollapseAll();
    EXPECT_INT(t.GetVisibleCount(), 2, _T("All/collapsedRoots"));
    t.ExpandAll();
    EXPECT_INT(t.GetVisibleCount(), 4, _T("All/expanded"));
    return OK(_T("ExpandCollapseAll"));
}

// ----- selection ------------------------------------------------------

static Result Test_SelectionBasic()
{
    DuiTreeView t;
    int a = t.AddRoot(_T("A"));
    int b = t.AddRoot(_T("B"));
    EXPECT_INT(t.GetCurSel(), -1, _T("Sel/initNone"));
    t.SetCurSel(a, false);
    EXPECT_INT(t.GetCurSel(), a, _T("Sel/a"));
    t.SetCurSel(b, false);
    EXPECT_INT(t.GetCurSel(), b, _T("Sel/b"));
    t.SetCurSel(-1, false);
    EXPECT_INT(t.GetCurSel(), -1, _T("Sel/clear"));
    t.SetCurSel(99999, false);                // bogus id ignored
    EXPECT_INT(t.GetCurSel(), -1, _T("Sel/bogusIgnored"));
    return OK(_T("SelectionBasic"));
}

// Removing the selected node clears selection.
static Result Test_RemoveClearsSelection()
{
    DuiTreeView t;
    int r = t.AddRoot(_T("R"));
    int c = t.AddChild(r, _T("c"));
    t.SetCurSel(c, false);
    t.Remove(c);
    EXPECT_INT(t.GetCurSel(), -1, _T("RmSel/cleared"));
    EXPECT_INT(t.GetVisibleCount(), 1, _T("RmSel/onlyR"));
    return OK(_T("RemoveClearsSelection"));
}

// Removing a parent removes the entire subtree.
static Result Test_RemoveSubtree()
{
    DuiTreeView t;
    int r1 = t.AddRoot(_T("R1"));
    int r2 = t.AddRoot(_T("R2"));
    t.AddChild(r1, _T("c1"));
    t.AddChild(r1, _T("c2"));
    t.AddChild(r2, _T("c3"));
    t.Remove(r1);
    EXPECT_INT(t.GetRootCount(),    1, _T("RmST/rootsLeft"));
    EXPECT_INT(t.GetVisibleCount(), 2, _T("RmST/visLeft"));
    EXPECT_INT(t.GetIdAtVisibleRow(0), r2,  _T("RmST/r2"));
    return OK(_T("RemoveSubtree"));
}

// ----- per-item state -------------------------------------------------

static Result Test_PerItemStateRoundTrip()
{
    DuiTreeView t;
    int r = t.AddRoot(_T("R"), nullptr, 12345);
    EXPECT_INT((int)t.GetItemParam(r), 12345, _T("State/param"));

    t.SetItemLabel(r, _T("R2"));
    EXPECT_STR(t.GetItemLabel(r), _T("R2"), _T("State/label"));

    HBITMAP fakeIcon = (HBITMAP)0xCAFE;
    t.SetItemIcon(r, fakeIcon);
    EXPECT_TRUE(t.GetItemIcon(r) == fakeIcon, _T("State/icon"));

    t.SetItemStatusColor(r, RGB(60, 175, 80));
    EXPECT_INT((int)t.GetItemStatusColor(r), (int)RGB(60,175,80), _T("State/dotColor"));

    // 状态点位图与状态点颜色是两条独立的存储：设了位图不该抹掉颜色，撤销位图
    // (nullptr) 后应当退回纯色圆点那条老路径。
    HBITMAP fakeStatusIcon = (HBITMAP)0xBEEF;
    t.SetItemStatusIcon(r, fakeStatusIcon);
    EXPECT_TRUE(t.GetItemStatusIcon(r) == fakeStatusIcon, _T("State/statusIcon"));
    EXPECT_INT((int)t.GetItemStatusColor(r), (int)RGB(60,175,80), _T("State/dotColorKept"));

    t.SetItemStatusIcon(r, nullptr);
    EXPECT_TRUE(t.GetItemStatusIcon(r) == nullptr, _T("State/statusIconCleared"));
    return OK(_T("PerItemStateRoundTrip"));
}

// Bogus id queries are safe.
static Result Test_QueriesBogusIdSafe()
{
    DuiTreeView t;
    EXPECT_INT(t.GetVisibleRow(99), -1, _T("Bogus/visRow"));
    EXPECT_TRUE(t.GetItemLabel(99).IsEmpty(), _T("Bogus/label"));
    EXPECT_TRUE(t.GetItemIcon(99) == nullptr, _T("Bogus/icon"));
    EXPECT_INT((int)t.GetItemStatusColor(99), (int)CLR_INVALID, _T("Bogus/dot"));
    EXPECT_TRUE(t.GetItemStatusIcon(99) == nullptr, _T("Bogus/statusIcon"));
    return OK(_T("QueriesBogusIdSafe"));
}

// ----- geometry / hit-test --------------------------------------------

static Result Test_ContentHeight()
{
    DuiTreeView t;
    t.SetRowHeight(40);
    int r = t.AddRoot(_T("R"));
    t.AddChild(r, _T("c1"));
    t.AddChild(r, _T("c2"));
    EXPECT_INT(t.GetVisibleCount(),   3,  _T("CH/vis"));
    EXPECT_INT(t.GetContentHeight(), 120, _T("CH/h"));
    t.Collapse(r);
    EXPECT_INT(t.GetContentHeight(), 40,  _T("CH/collapsed"));
    return OK(_T("ContentHeight"));
}

static Result Test_HitTestRow()
{
    DuiTreeView t;
    t.SetRowHeight(20);
    int r = t.AddRoot(_T("R"));
    int c = t.AddChild(r, _T("c"));
    t.SetRect(RECT{ 0, 0, 200, 200 });

    // Row 0 spans y=0..19 (R), row 1 spans y=20..39 (c).
    EXPECT_INT(t.HitTestId(POINT{ 100,  5 }), r, _T("HT/row0"));
    EXPECT_INT(t.HitTestId(POINT{ 100, 30 }), c, _T("HT/row1"));
    EXPECT_INT(t.HitTestId(POINT{ 100, 99 }), -1,_T("HT/empty"));
    return OK(_T("HitTestRow"));
}

// SetRowHeight / SetIndentPx clamp lower bounds.
static Result Test_ClampMetrics()
{
    DuiTreeView t;
    t.SetRowHeight(2);
    EXPECT_INT(t.GetRowHeight(), 8, _T("Clamp/row"));
    t.SetIndentPx(0);
    EXPECT_INT(t.GetIndentPx(), 1, _T("Clamp/indent"));
    return OK(_T("ClampMetrics"));
}

// HasChildren + IsExpanded round-trip.
static Result Test_HasChildren()
{
    DuiTreeView t;
    int r = t.AddRoot(_T("R"));
    EXPECT_TRUE(!t.HasChildren(r), _T("HC/none"));
    int c = t.AddChild(r, _T("c"));
    (void)c;
    EXPECT_TRUE(t.HasChildren(r), _T("HC/yes"));
    EXPECT_TRUE(!t.HasChildren(c), _T("HC/leaf"));
    EXPECT_TRUE(t.IsExpanded(r), _T("HC/defaultExpanded"));
    return OK(_T("HasChildren"));
}

// Clear wipes everything.
static Result Test_Clear()
{
    DuiTreeView t;
    int r = t.AddRoot(_T("R"));
    t.AddChild(r, _T("c"));
    t.SetCurSel(r, false);
    t.Clear();
    EXPECT_INT(t.GetVisibleCount(), 0,  _T("Clr/empty"));
    EXPECT_INT(t.GetCurSel(),       -1, _T("Clr/sel"));
    return OK(_T("Clear"));
}

// ----- 大数据量：按 id 直接定位 / 可见行按需重算 / 末尾追加快速路径（2026-10-01） -----
//
// 这一组守护的是 DuiTreeView 在几千上万个节点下的正确性与速度。改动前按 id 找节点是逐个
// 比对、每加一个节点都整表重算可见行，客户端主面板 5000 人的通讯录建树要好几秒。
// TVB1~TVB6 验证行为（改动前后都必须通过），TVB7 验证速度（改动前超时）。

// TVB1：连续加 3000 个根节点，首、中、尾都能正确读写，越界 id 查不到。
static Result Test_IdLookupManyRoots()
{
    const int kCount = 3000;
    DuiTreeView t;
    std::vector<int> ids;
    for (int i = 0; i < kCount; ++i)
    {
        CString label;
        label.Format(_T("n%d"), i);
        ids.push_back(t.AddRoot(label));
    }
    EXPECT_STR(t.GetItemLabel(ids[0]), _T("n0"), _T("TVB1/first"));
    EXPECT_STR(t.GetItemLabel(ids[kCount / 2]), _T("n1500"), _T("TVB1/middle"));
    EXPECT_STR(t.GetItemLabel(ids[kCount - 1]), _T("n2999"), _T("TVB1/last"));

    t.SetItemLabel(ids[kCount / 2], _T("mid"));
    EXPECT_STR(t.GetItemLabel(ids[kCount / 2]), _T("mid"), _T("TVB1/setMiddle"));
    EXPECT_INT(t.GetVisibleRow(ids[kCount - 1]), kCount - 1, _T("TVB1/lastRow"));

    EXPECT_TRUE(t.GetItemLabel(0).IsEmpty(), _T("TVB1/idZero"));
    EXPECT_TRUE(t.GetItemLabel(ids[kCount - 1] + 1).IsEmpty(), _T("TVB1/idPastEnd"));
    EXPECT_INT(t.GetVisibleRow(ids[kCount - 1] + 1), -1, _T("TVB1/rowPastEnd"));
    return OK(_T("IdLookupManyRoots"));
}

// TVB2：在前面的节点下插入子节点（中间插入，后面的节点整体后移），前后节点都能正确查到。
static Result Test_MiddleInsertKeepsLookups()
{
    DuiTreeView t;
    int a  = t.AddRoot(_T("A"));
    int a1 = t.AddChild(a, _T("a1"));
    int b  = t.AddRoot(_T("B"));
    int b1 = t.AddChild(b, _T("b1"));
    int c  = t.AddRoot(_T("C"));
    int a2 = t.AddChild(a, _T("a2"));   // 插在 b 之前：b、b1、c 的下标后移
    int b2 = t.AddChild(b, _T("b2"));   // 插在 c 之前

    EXPECT_STR(t.GetItemLabel(a2), _T("a2"), _T("TVB2/a2"));
    EXPECT_STR(t.GetItemLabel(b),  _T("B"),  _T("TVB2/b"));
    EXPECT_STR(t.GetItemLabel(b1), _T("b1"), _T("TVB2/b1"));
    EXPECT_STR(t.GetItemLabel(b2), _T("b2"), _T("TVB2/b2"));
    EXPECT_STR(t.GetItemLabel(c),  _T("C"),  _T("TVB2/c"));
    EXPECT_INT(t.GetChildCount(a), 2, _T("TVB2/aChildren"));
    EXPECT_INT(t.GetChildCount(b), 2, _T("TVB2/bChildren"));

    const int expected[] = { a, a1, a2, b, b1, b2, c };
    EXPECT_INT(t.GetVisibleCount(), 7, _T("TVB2/visCount"));
    for (int i = 0; i < 7; ++i)
    {
        EXPECT_INT(t.GetIdAtVisibleRow(i), expected[i], _T("TVB2/order"));
    }

    t.Collapse(b);
    EXPECT_INT(t.GetVisibleCount(), 5, _T("TVB2/collapsedCount"));
    EXPECT_INT(t.GetIdAtVisibleRow(4), c, _T("TVB2/collapsedOrder"));
    return OK(_T("MiddleInsertKeepsLookups"));
}

// TVB3：删除中间的一棵子树后，被删的查不到，其余正确，之后还能正常添加。
static Result Test_RemoveMiddleSubtreeLookups()
{
    DuiTreeView t;
    int a   = t.AddRoot(_T("A"));
    int a1  = t.AddChild(a, _T("a1"));
    int b   = t.AddRoot(_T("B"));
    int b1  = t.AddChild(b, _T("b1"));
    int bb1 = t.AddChild(b1, _T("bb1"));
    int c   = t.AddRoot(_T("C"));
    int c1  = t.AddChild(c, _T("c1"));

    t.Remove(b);
    EXPECT_TRUE(t.GetItemLabel(b).IsEmpty(),   _T("TVB3/bGone"));
    EXPECT_TRUE(t.GetItemLabel(b1).IsEmpty(),  _T("TVB3/b1Gone"));
    EXPECT_TRUE(t.GetItemLabel(bb1).IsEmpty(), _T("TVB3/bb1Gone"));
    EXPECT_INT(t.GetVisibleRow(b1), -1, _T("TVB3/b1Row"));
    EXPECT_STR(t.GetItemLabel(c1), _T("c1"), _T("TVB3/c1"));

    int c2 = t.AddChild(c, _T("c2"));
    int d  = t.AddRoot(_T("D"));
    const int expected[] = { a, a1, c, c1, c2, d };
    EXPECT_INT(t.GetVisibleCount(), 6, _T("TVB3/visCount"));
    for (int i = 0; i < 6; ++i)
    {
        EXPECT_INT(t.GetIdAtVisibleRow(i), expected[i], _T("TVB3/order"));
    }
    EXPECT_STR(t.GetItemLabel(c2), _T("c2"), _T("TVB3/c2"));
    EXPECT_STR(t.GetItemLabel(d),  _T("D"),  _T("TVB3/d"));
    return OK(_T("RemoveMiddleSubtreeLookups"));
}

// TVB4：Clear 之后旧 id 查不到；新 id 比旧的大（不复用）且能正确查到。
static Result Test_ClearThenAddIds()
{
    DuiTreeView t;
    int r = t.AddRoot(_T("R"));
    int x = t.AddChild(r, _T("x"));
    t.Clear();
    EXPECT_TRUE(t.GetItemLabel(r).IsEmpty(), _T("TVB4/rGone"));
    EXPECT_TRUE(t.GetItemLabel(x).IsEmpty(), _T("TVB4/xGone"));

    int n = t.AddRoot(_T("N"));
    int m = t.AddChild(n, _T("m"));
    EXPECT_TRUE(n > x, _T("TVB4/noReuse"));
    EXPECT_STR(t.GetItemLabel(n), _T("N"), _T("TVB4/n"));
    EXPECT_STR(t.GetItemLabel(m), _T("m"), _T("TVB4/m"));
    EXPECT_INT(t.GetVisibleRow(m), 1, _T("TVB4/mRow"));
    EXPECT_INT(t.GetVisibleCount(), 2, _T("TVB4/count"));
    return OK(_T("ClearThenAddIds"));
}

// TVB5 用的过滤函数：只拒绝一个指定的节点（连同其后代），其余都放行。
struct RejectOneId
{
    int m_rejectedId;   // 被拒绝的节点 id

    explicit RejectOneId(int id)
        : m_rejectedId(id)
    {
    }

    bool operator()(int id) const
    {
        return id != m_rejectedId;
    }
};

// TVB5：大量添加之后不做任何额外调用，直接查询；折叠、隐藏、过滤照样生效。
static Result Test_VisibleQueriesFreshAfterBulkAdd()
{
    const int kRoots    = 50;
    const int kChildren = 3;
    const int kRowH     = 20;
    DuiTreeView t;
    t.SetRowHeight(kRowH);
    t.SetRect(RECT{ 0, 0, 200, 400 });

    std::vector<int> roots;
    std::vector<int> order;   // 全部展开时的可见行顺序
    for (int i = 0; i < kRoots; ++i)
    {
        int r = t.AddRoot(_T("R"));
        roots.push_back(r);
        order.push_back(r);
        for (int k = 0; k < kChildren; ++k)
        {
            order.push_back(t.AddChild(r, _T("c")));
        }
    }
    const int total = kRoots * (kChildren + 1);
    EXPECT_INT(t.GetVisibleCount(), total, _T("TVB5/count"));
    EXPECT_INT(t.GetContentHeight(), total * kRowH, _T("TVB5/height"));
    EXPECT_INT(t.GetIdAtVisibleRow(5), order[5], _T("TVB5/row5"));
    EXPECT_INT(t.HitTestId(POINT{ 100, 5 * kRowH + 5 }), order[5], _T("TVB5/hit"));

    t.Collapse(roots[0]);
    EXPECT_INT(t.GetVisibleCount(), total - kChildren, _T("TVB5/collapsed"));
    EXPECT_INT(t.GetIdAtVisibleRow(1), roots[1], _T("TVB5/collapsedRow1"));

    t.SetItemVisible(roots[1], false);
    EXPECT_INT(t.GetVisibleCount(), total - kChildren - (kChildren + 1), _T("TVB5/hidden"));

    t.SetFilter(RejectOneId(roots[2]));
    EXPECT_INT(t.GetVisibleCount(), total - kChildren - 2 * (kChildren + 1), _T("TVB5/filtered"));
    t.SetFilter(std::function<bool(int)>());
    EXPECT_INT(t.GetVisibleCount(), total - kChildren - (kChildren + 1), _T("TVB5/unfiltered"));

    t.Expand(roots[0]);
    EXPECT_INT(t.GetVisibleCount(), total - (kChildren + 1), _T("TVB5/reExpanded"));
    return OK(_T("VisibleQueriesFreshAfterBulkAdd"));
}

// TVB6 用的参照模型：一个节点在先序数组里的记录。
struct RefNode
{
    int     id;         // 树返回的节点 id
    int     depth;      // 深度，根为 0
    bool    expanded;   // 是否展开
    CString label;      // 文字
};

// 参照模型里某节点子树的结束位置（开区间）。
static size_t RefSubtreeEnd(const std::vector<RefNode>& ref, int idx)
{
    size_t end = (size_t)idx + 1;
    while (end < ref.size() && ref[end].depth > ref[(size_t)idx].depth)
    {
        ++end;
    }
    return end;
}

// 参照模型里某节点的直接子节点个数。
static int RefChildCount(const std::vector<RefNode>& ref, int idx)
{
    const size_t end = RefSubtreeEnd(ref, idx);
    int n = 0;
    for (size_t k = (size_t)idx + 1; k < end; ++k)
    {
        if (ref[k].depth == ref[(size_t)idx].depth + 1)
        {
            ++n;
        }
    }
    return n;
}

// 参照模型的可见行：先序遍历，折叠节点的后代跳过。
static std::vector<int> RefVisible(const std::vector<RefNode>& ref)
{
    std::vector<int> vis;
    int hideUntilDepth = -1;
    for (size_t i = 0; i < ref.size(); ++i)
    {
        if (hideUntilDepth >= 0 && ref[i].depth > hideUntilDepth)
        {
            continue;
        }
        hideUntilDepth = -1;
        vis.push_back(ref[i].id);
        if (!ref[i].expanded)
        {
            hideUntilDepth = ref[i].depth;
        }
    }
    return vis;
}

// TVB6：随机 2000 次操作，每 50 次与参照模型比对可见行顺序与全部节点文字。
static Result Test_RandomOpsMatchReference()
{
    const int kOps        = 2000;
    const int kCheckEvery = 50;
    std::mt19937 rng(20261001);
    DuiTreeView t;
    std::vector<RefNode> ref;
    std::vector<int> removedIds;

    for (int op = 1; op <= kOps; ++op)
    {
        const int roll = (int)(rng() % 100);
        CString label;
        label.Format(_T("n%d"), op);

        if (roll < 25 || ref.empty())
        {
            //加根节点：追加到末尾
            RefNode n;
            n.id       = t.AddRoot(label);
            n.depth    = 0;
            n.expanded = true;
            n.label    = label;
            ref.push_back(n);
        }
        else if (roll < 65)
        {
            //给随机一个现存节点加子节点：插到该节点子树的末尾
            const int pidx = (int)(rng() % ref.size());
            RefNode n;
            n.id       = t.AddChild(ref[(size_t)pidx].id, label);
            n.depth    = ref[(size_t)pidx].depth + 1;
            n.expanded = true;
            n.label    = label;
            ref.insert(ref.begin() + RefSubtreeEnd(ref, pidx), n);
        }
        else if (roll < 75)
        {
            //删除随机一个节点及其子树
            const int idx = (int)(rng() % ref.size());
            const size_t end = RefSubtreeEnd(ref, idx);
            for (size_t k = (size_t)idx; k < end; ++k)
            {
                removedIds.push_back(ref[k].id);
            }
            t.Remove(ref[(size_t)idx].id);
            ref.erase(ref.begin() + idx, ref.begin() + end);
        }
        else if (roll < 85)
        {
            //折叠 / 展开随机一个节点
            const int idx = (int)(rng() % ref.size());
            const bool exp = (roll < 80);
            ref[(size_t)idx].expanded = exp;
            if (exp)
            {
                t.Expand(ref[(size_t)idx].id);
            }
            else
            {
                t.Collapse(ref[(size_t)idx].id);
            }
        }
        else
        {
            //改随机一个节点的文字
            const int idx = (int)(rng() % ref.size());
            ref[(size_t)idx].label = label;
            t.SetItemLabel(ref[(size_t)idx].id, label);
        }

        if (op % kCheckEvery != 0)
        {
            continue;
        }
        const std::vector<int> vis = RefVisible(ref);
        EXPECT_INT(t.GetVisibleCount(), (int)vis.size(), _T("TVB6/visCount"));
        for (size_t i = 0; i < vis.size(); ++i)
        {
            EXPECT_INT(t.GetIdAtVisibleRow((int)i), vis[i], _T("TVB6/visOrder"));
        }
        for (size_t i = 0; i < ref.size(); ++i)
        {
            EXPECT_STR(t.GetItemLabel(ref[i].id), ref[i].label, _T("TVB6/label"));
            EXPECT_INT(t.GetChildCount(ref[i].id), RefChildCount(ref, (int)i), _T("TVB6/childCount"));
        }
        for (size_t i = 0; i < removedIds.size(); ++i)
        {
            EXPECT_TRUE(t.GetItemLabel(removedIds[i]).IsEmpty(), _T("TVB6/removedGone"));
        }
    }
    return OK(_T("RandomOpsMatchReference"));
}

// TVB7 的时间上限（毫秒）。按改动前实测的耗时定：改动前（逐个比对查找、每加一个节点就
// 整表重算可见行）2026-10-01 在 Debug|Win32 下实测 24690 毫秒，远超此值；机器负载的波动
// 不至于让改动后的实现跨过它。
static const int kBulkBuildLimitMs = 3000;

// TVB7：按通讯录的形状建 2 万个节点（1 个公司根、200 个部门、每部门 100 人，深度优先），
// 每加一个节点就设一次文字，最后查一次可见行数。
static Result Test_BulkBuildIsNotQuadratic()
{
    const int kDepts          = 200;
    const int kMembersPerDept = 100;
    LARGE_INTEGER freq;
    LARGE_INTEGER t0;
    LARGE_INTEGER t1;
    ::QueryPerformanceFrequency(&freq);
    ::QueryPerformanceCounter(&t0);

    DuiTreeView t;
    int company = t.AddRoot(_T("Company"));
    for (int d = 0; d < kDepts; ++d)
    {
        int dept = t.AddChild(company, _T("Dept"));
        t.SetItemLabel(dept, _T("Dept*"));
        for (int m = 0; m < kMembersPerDept; ++m)
        {
            int member = t.AddChild(dept, _T("Member"));
            t.SetItemLabel(member, _T("Member*"));
        }
    }
    const int visible = t.GetVisibleCount();

    ::QueryPerformanceCounter(&t1);
    const int elapsedMs = (int)((t1.QuadPart - t0.QuadPart) * 1000 / freq.QuadPart);

    EXPECT_INT(visible, 1 + kDepts * (1 + kMembersPerDept), _T("TVB7/count"));
    if (elapsedMs > kBulkBuildLimitMs)
    {
        CString d;
        d.Format(_T("elapsed=%d ms limit=%d ms"), elapsedMs, kBulkBuildLimitMs);
        return Fail(_T("TVB7/time"), d);
    }
    return OK(_T("BulkBuildIsNotQuadratic"));
}

// ----- MoveRoot：根节点连同子树整体移动（2026-10-01） -----
//
// 会话列表收到新消息时要把那一行挪到最前面。没有这个接口时只能整表重建，几千个会话每条
// 消息都要重建一遍。

// 把可见行顺序取成数组，便于整体比对。
static std::vector<int> VisibleIds(const DuiTreeView& t)
{
    std::vector<int> ids;
    for (int i = 0; i < t.GetVisibleCount(); ++i)
    {
        ids.push_back(t.GetIdAtVisibleRow(i));
    }
    return ids;
}

// 可见行顺序是否与期望一致。
static bool SameOrder(const DuiTreeView& t, const int* expected, int count)
{
    const std::vector<int> ids = VisibleIds(t);
    if ((int)ids.size() != count)
    {
        return false;
    }
    for (int i = 0; i < count; ++i)
    {
        if (ids[(size_t)i] != expected[i])
        {
            return false;
        }
    }
    return true;
}

// TVM1：把 C 移到 A 前面，子树跟着走，文字与子节点个数不变。
static Result Test_MoveRootToFront()
{
    DuiTreeView t;
    int a  = t.AddRoot(_T("A"));
    int a1 = t.AddChild(a, _T("a1"));
    int b  = t.AddRoot(_T("B"));
    int b1 = t.AddChild(b, _T("b1"));
    int b2 = t.AddChild(b, _T("b2"));
    int c  = t.AddRoot(_T("C"));

    EXPECT_TRUE(t.MoveRoot(c, a), _T("TVM1/ret"));
    const int expected[] = { c, a, a1, b, b1, b2 };
    EXPECT_TRUE(SameOrder(t, expected, 6), _T("TVM1/order"));
    EXPECT_STR(t.GetItemLabel(b2), _T("b2"), _T("TVM1/label"));
    EXPECT_INT(t.GetChildCount(b), 2, _T("TVM1/bChildren"));
    EXPECT_INT(t.GetChildCount(a), 1, _T("TVM1/aChildren"));
    EXPECT_INT(t.GetRootCount(), 3, _T("TVM1/roots"));

    // 把带子树的 B 移到最前。
    EXPECT_TRUE(t.MoveRoot(b, c), _T("TVM1/ret2"));
    const int expected2[] = { b, b1, b2, c, a, a1 };
    EXPECT_TRUE(SameOrder(t, expected2, 6), _T("TVM1/order2"));
    return OK(_T("MoveRootToFront"));
}

// TVM2：beforeId 传 -1 移到最后。
static Result Test_MoveRootToEnd()
{
    DuiTreeView t;
    int a  = t.AddRoot(_T("A"));
    int a1 = t.AddChild(a, _T("a1"));
    int b  = t.AddRoot(_T("B"));
    int b1 = t.AddChild(b, _T("b1"));
    int b2 = t.AddChild(b, _T("b2"));
    int c  = t.AddRoot(_T("C"));

    EXPECT_TRUE(t.MoveRoot(a, -1), _T("TVM2/ret"));
    const int expected[] = { b, b1, b2, c, a, a1 };
    EXPECT_TRUE(SameOrder(t, expected, 6), _T("TVM2/order"));

    // 已在最后：返回 true，顺序不变。
    EXPECT_TRUE(t.MoveRoot(a, -1), _T("TVM2/alreadyLast"));
    EXPECT_TRUE(SameOrder(t, expected, 6), _T("TVM2/orderSame"));
    return OK(_T("MoveRootToEnd"));
}

// TVM3：移动一棵折叠着、含选中节点的子树，折叠状态与选中项保持不变。
static Result Test_MoveRootKeepsState()
{
    DuiTreeView t;
    int a  = t.AddRoot(_T("A"));
    int b  = t.AddRoot(_T("B"));
    int b1 = t.AddChild(b, _T("b1"));
    int b2 = t.AddChild(b, _T("b2"));
    int c  = t.AddRoot(_T("C"));
    t.SetCurSel(b1, false);
    t.Collapse(b);

    EXPECT_TRUE(t.MoveRoot(b, a), _T("TVM3/ret"));
    EXPECT_INT(t.GetCurSel(), b1, _T("TVM3/selKept"));
    EXPECT_TRUE(!t.IsExpanded(b), _T("TVM3/collapsedKept"));
    const int expected[] = { b, a, c };
    EXPECT_TRUE(SameOrder(t, expected, 3), _T("TVM3/order"));

    t.Expand(b);
    const int expanded[] = { b, b1, b2, a, c };
    EXPECT_TRUE(SameOrder(t, expanded, 5), _T("TVM3/expandedOrder"));
    return OK(_T("MoveRootKeepsState"));
}

// TVM4：非法参数返回 false 且树不变；移到自己前面返回 true 且树不变。
static Result Test_MoveRootInvalidArgs()
{
    DuiTreeView t;
    int a  = t.AddRoot(_T("A"));
    int a1 = t.AddChild(a, _T("a1"));
    int b  = t.AddRoot(_T("B"));
    const int expected[] = { a, a1, b };

    EXPECT_TRUE(!t.MoveRoot(a1, b), _T("TVM4/childAsId"));
    EXPECT_TRUE(!t.MoveRoot(99999, a), _T("TVM4/unknownId"));
    EXPECT_TRUE(!t.MoveRoot(b, a1), _T("TVM4/childAsBefore"));
    EXPECT_TRUE(!t.MoveRoot(b, 99999), _T("TVM4/unknownBefore"));
    EXPECT_TRUE(SameOrder(t, expected, 3), _T("TVM4/unchanged"));

    EXPECT_TRUE(t.MoveRoot(b, b), _T("TVM4/self"));
    EXPECT_TRUE(t.MoveRoot(a, b), _T("TVM4/alreadyBefore"));
    EXPECT_TRUE(SameOrder(t, expected, 3), _T("TVM4/stillUnchanged"));
    return OK(_T("MoveRootInvalidArgs"));
}

// TVM5：移动之后给被移动的根节点加子节点，落在其新位置的子树末尾。
static Result Test_AddChildAfterMove()
{
    DuiTreeView t;
    int a  = t.AddRoot(_T("A"));
    int a1 = t.AddChild(a, _T("a1"));
    int b  = t.AddRoot(_T("B"));
    int c  = t.AddRoot(_T("C"));

    EXPECT_TRUE(t.MoveRoot(c, a), _T("TVM5/move"));
    int c1 = t.AddChild(c, _T("c1"));
    int a2 = t.AddChild(a, _T("a2"));
    const int expected[] = { c, c1, a, a1, a2, b };
    EXPECT_TRUE(SameOrder(t, expected, 6), _T("TVM5/order"));
    EXPECT_STR(t.GetItemLabel(c1), _T("c1"), _T("TVM5/c1"));
    EXPECT_STR(t.GetItemLabel(a2), _T("a2"), _T("TVM5/a2"));
    EXPECT_INT(t.GetChildCount(c), 1, _T("TVM5/cChildren"));
    EXPECT_INT(t.GetChildCount(a), 2, _T("TVM5/aChildren"));
    return OK(_T("AddChildAfterMove"));
}

// 参照模型里的 MoveRoot：把 ref 中根节点 id 的子树移到根节点 beforeId 之前（-1 = 末尾）。
// 调用方保证两者都是根节点且 id != beforeId。
static void RefMoveRoot(std::vector<RefNode>& ref, int id, int beforeId)
{
    int idx = -1;
    for (size_t i = 0; i < ref.size(); ++i)
    {
        if (ref[i].id == id)
        {
            idx = (int)i;
            break;
        }
    }
    const size_t end = RefSubtreeEnd(ref, idx);
    std::vector<RefNode> block(ref.begin() + idx, ref.begin() + end);
    ref.erase(ref.begin() + idx, ref.begin() + end);

    size_t insertAt = ref.size();
    if (beforeId != -1)
    {
        for (size_t i = 0; i < ref.size(); ++i)
        {
            if (ref[i].id == beforeId)
            {
                insertAt = i;
                break;
            }
        }
    }
    ref.insert(ref.begin() + insertAt, block.begin(), block.end());
}

// 参照模型里随机挑一个根节点的 id；没有根节点时返回 -1。
static int RefRandomRoot(const std::vector<RefNode>& ref, std::mt19937& rng)
{
    std::vector<int> roots;
    for (size_t i = 0; i < ref.size(); ++i)
    {
        if (ref[i].depth == 0)
        {
            roots.push_back(ref[i].id);
        }
    }
    if (roots.empty())
    {
        return -1;
    }
    return roots[rng() % roots.size()];
}

// TVM6：随机 2000 次操作（加、删、折叠、移动），每 50 次与参照模型比对。
static Result Test_RandomMovesMatchReference()
{
    const int kOps        = 2000;
    const int kCheckEvery = 50;
    std::mt19937 rng(20261002);
    DuiTreeView t;
    std::vector<RefNode> ref;

    for (int op = 1; op <= kOps; ++op)
    {
        const int roll = (int)(rng() % 100);
        CString label;
        label.Format(_T("n%d"), op);

        if (roll < 20 || ref.empty())
        {
            //加根节点
            RefNode n;
            n.id       = t.AddRoot(label);
            n.depth    = 0;
            n.expanded = true;
            n.label    = label;
            ref.push_back(n);
        }
        else if (roll < 50)
        {
            //给随机节点加子节点
            const int pidx = (int)(rng() % ref.size());
            RefNode n;
            n.id       = t.AddChild(ref[(size_t)pidx].id, label);
            n.depth    = ref[(size_t)pidx].depth + 1;
            n.expanded = true;
            n.label    = label;
            ref.insert(ref.begin() + RefSubtreeEnd(ref, pidx), n);
        }
        else if (roll < 58)
        {
            //删除随机节点及其子树
            const int idx = (int)(rng() % ref.size());
            const size_t end = RefSubtreeEnd(ref, idx);
            t.Remove(ref[(size_t)idx].id);
            ref.erase(ref.begin() + idx, ref.begin() + end);
        }
        else if (roll < 66)
        {
            //折叠 / 展开随机节点
            const int idx = (int)(rng() % ref.size());
            const bool exp = (roll < 62);
            ref[(size_t)idx].expanded = exp;
            if (exp)
            {
                t.Expand(ref[(size_t)idx].id);
            }
            else
            {
                t.Collapse(ref[(size_t)idx].id);
            }
        }
        else
        {
            //移动随机根节点到随机根节点之前，或移到末尾
            const int id = RefRandomRoot(ref, rng);
            const int before = (rng() % 5 == 0) ? -1 : RefRandomRoot(ref, rng);
            EXPECT_TRUE(t.MoveRoot(id, before), _T("TVM6/moveRet"));
            if (id != before)
            {
                RefMoveRoot(ref, id, before);
            }
        }

        if (op % kCheckEvery != 0)
        {
            continue;
        }
        const std::vector<int> vis = RefVisible(ref);
        EXPECT_INT(t.GetVisibleCount(), (int)vis.size(), _T("TVM6/visCount"));
        for (size_t i = 0; i < vis.size(); ++i)
        {
            EXPECT_INT(t.GetIdAtVisibleRow((int)i), vis[i], _T("TVM6/visOrder"));
        }
        for (size_t i = 0; i < ref.size(); ++i)
        {
            EXPECT_STR(t.GetItemLabel(ref[i].id), ref[i].label, _T("TVM6/label"));
            EXPECT_INT(t.GetChildCount(ref[i].id), RefChildCount(ref, (int)i), _T("TVM6/childCount"));
        }
    }
    return OK(_T("RandomMovesMatchReference"));
}

// TVM7 的时间上限（毫秒）。3000 个根节点里随机移动 1000 次，每次要搬动的节点数与移动
// 距离成正比（平均约 1500 个）。2026-10-01 在 Debug|Win32 下实测约 1660 毫秒，主要花在
// std::rotate 搬动节点上（Node 含 3 个 CString 与一个 vector，CString 没有移动构造）。
// 上限只防止以后退化成每次移动都要平方级开销 —— 那样 1000 次会远超此值。
static const int kMoveBulkLimitMs = 5000;

// TVM8 的平均耗时上限（毫秒）：3000 个会话里把最后一个挪到最前面（真实场景的最坏情况），
// 每次不超过这么多。收到新消息时就是做这件事，远低于一帧的预算即可。
static const int kMoveLastToFrontAvgLimitMs = 20;

// TVM7：3000 个根节点里连续移动 1000 次，在上限内完成。
static Result Test_MoveRootBulkSpeed()
{
    const int kRoots = 3000;
    const int kMoves = 1000;
    std::mt19937 rng(20261003);
    DuiTreeView t;
    std::vector<int> roots;
    for (int i = 0; i < kRoots; ++i)
    {
        roots.push_back(t.AddRoot(_T("R")));
    }

    LARGE_INTEGER freq;
    LARGE_INTEGER t0;
    LARGE_INTEGER t1;
    ::QueryPerformanceFrequency(&freq);
    ::QueryPerformanceCounter(&t0);
    for (int m = 0; m < kMoves; ++m)
    {
        const int id     = roots[rng() % roots.size()];
        const int before = roots[rng() % roots.size()];
        t.MoveRoot(id, before);
        t.SetItemLabel(id, _T("moved"));   // 移动后立即按 id 访问，覆盖对照表重建的开销
    }
    const int visible = t.GetVisibleCount();
    ::QueryPerformanceCounter(&t1);
    const int elapsedMs = (int)((t1.QuadPart - t0.QuadPart) * 1000 / freq.QuadPart);

    EXPECT_INT(visible, kRoots, _T("TVM7/count"));
    if (elapsedMs > kMoveBulkLimitMs)
    {
        CString d;
        d.Format(_T("elapsed=%d ms limit=%d ms"), elapsedMs, kMoveBulkLimitMs);
        return Fail(_T("TVM7/time"), d);
    }
    return OK(_T("MoveRootBulkSpeed"));
}

// TVM8：3000 个根节点里把最后一个挪到最前面，重复 100 次，平均每次在上限内，顺序正确。
static Result Test_MoveLastToFrontSpeed()
{
    const int kRoots = 3000;
    const int kMoves = 100;
    DuiTreeView t;
    std::vector<int> order;   // 当前的根节点顺序，与树同步维护
    for (int i = 0; i < kRoots; ++i)
    {
        order.push_back(t.AddRoot(_T("Conversation")));
    }

    LARGE_INTEGER freq;
    LARGE_INTEGER t0;
    LARGE_INTEGER t1;
    ::QueryPerformanceFrequency(&freq);
    ::QueryPerformanceCounter(&t0);
    for (int m = 0; m < kMoves; ++m)
    {
        const int last  = order.back();
        const int first = order.front();
        if (!t.MoveRoot(last, first))
        {
            return Fail(_T("TVM8/ret"), _T("MoveRoot returned false"));
        }
        order.pop_back();
        order.insert(order.begin(), last);
    }
    ::QueryPerformanceCounter(&t1);
    const int elapsedMs = (int)((t1.QuadPart - t0.QuadPart) * 1000 / freq.QuadPart);

    EXPECT_INT(t.GetIdAtVisibleRow(0), order[0], _T("TVM8/first"));
    EXPECT_INT(t.GetIdAtVisibleRow(kRoots - 1), order[(size_t)kRoots - 1], _T("TVM8/last"));
    EXPECT_INT(t.GetVisibleRow(order[(size_t)kMoves]), kMoves, _T("TVM8/middle"));
    if (elapsedMs > kMoveLastToFrontAvgLimitMs * kMoves)
    {
        CString d;
        d.Format(_T("elapsed=%d ms for %d moves, limit avg=%d ms"), elapsedMs, kMoves,
                 kMoveLastToFrontAvgLimitMs);
        return Fail(_T("TVM8/time"), d);
    }
    return OK(_T("MoveLastToFrontSpeed"));
}

// ---- 灰显图标的缓存（2026-10-04）----
// 灰显节点原先每帧都临时新建位图、逐像素转灰度，整屏灰显时每帧绘制多 20%~25%。改为每个节点首次绘制时
// 转换一次并缓存，换图标、取消灰显、删除节点、Clear 与析构时释放。

// 灰显用例的图标边长、行高与画布宽度（像素）。
static const int kGrayIconPx  = 32;
static const int kGrayRowH    = 40;
static const int kGrayCanvasW = 200;

// 造一张 kGrayIconPx 见方、每个像素都是同一 BGRA 值的 32 位位图（值按预乘透明通道给出）。
static HBITMAP MakeSolidIcon(BYTE b, BYTE g, BYTE r, BYTE a)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = kGrayIconPx;
    bi.bmiHeader.biHeight      = -kGrayIconPx;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!h)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < kGrayIconPx * kGrayIconPx; ++i)
    {
        p[i * 4 + 0] = b;
        p[i * 4 + 1] = g;
        p[i * 4 + 2] = r;
        p[i * 4 + 3] = a;
    }
    return h;
}

// 把树画到白底的 32 位位图上；out 非空时取出全部像素（按行存放）。
static bool PaintTreeOnWhite(DuiTreeView& t, int h, std::vector<COLORREF>* out)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = kGrayCanvasW;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dst = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dst)
    {
        return false;
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, dst);
    RECT all = { 0, 0, kGrayCanvasW, h };
    HBRUSH white = ::CreateSolidBrush(RGB(255, 255, 255));
    ::FillRect(hdc, &all, white);
    ::DeleteObject(white);
    t.SetRect(all);
    t.OnPaint(hdc, all);
    ::GdiFlush();
    if (out != nullptr)
    {
        out->resize((size_t)kGrayCanvasW * h);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < kGrayCanvasW; ++x)
            {
                (*out)[(size_t)y * kGrayCanvasW + x] = ::GetPixel(hdc, x, y);
            }
        }
    }
    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(dst);
    return true;
}

// 建一棵单列树：rows 个根节点，各带一张图标并设为灰显。icons 收到建出来的图标（调用方负责释放）。
static void BuildGrayTree(DuiTreeView& t, int rows, bool usesAlpha, std::vector<HBITMAP>& icons,
                          std::vector<int>& ids)
{
    t.SetRowHeight(kGrayRowH);
    t.SetIconSize(kGrayIconPx);
    t.SetIconUsesAlpha(usesAlpha);
    for (int i = 0; i < rows; ++i)
    {
        HBITMAP ic = MakeSolidIcon((BYTE)(40 + i * 30), 120, 200, 255);
        icons.push_back(ic);
        const int id = t.AddRoot(_T(""), ic);
        t.SetItemIconGrayed(id, true);
        ids.push_back(id);
    }
}

static void DeleteIcons(std::vector<HBITMAP>& icons)
{
    for (size_t i = 0; i < icons.size(); ++i)
    {
        ::DeleteObject(icons[i]);
    }
    icons.clear();
}

// 每个灰显节点只转换一次：连续绘制多次，转换次数等于灰显节点数。旧画法与透明通道两种模式都检查。
static Result Test_GrayIconConvertedOnce()
{
    const int kRows = 3;
    const int kFrames = 5;
    for (int mode = 0; mode < 2; ++mode)
    {
        const bool usesAlpha = (mode == 1);
        std::vector<HBITMAP> icons;
        std::vector<int> ids;
        {
            DuiTreeView t;
            BuildGrayTree(t, kRows, usesAlpha, icons, ids);
            for (int f = 0; f < kFrames; ++f)
            {
                EXPECT_TRUE(PaintTreeOnWhite(t, kRows * kGrayRowH, nullptr), _T("GrayOnce/paint"));
            }
            EXPECT_INT(t.GetGrayIconConversionCount(), kRows, usesAlpha ? _T("GrayOnce/alpha/conversions")
                                                                        : _T("GrayOnce/legacy/conversions"));
            EXPECT_INT((int)t.GetGrayIconCacheCount(), kRows, _T("GrayOnce/cacheCount"));
        }
        DeleteIcons(icons);
    }
    return OK(_T("GrayIconConvertedOnce"));
}

// 换图标后重新转换；取消灰显时释放缓存、之后不再转换。
static Result Test_GrayIconRebuiltOnIconChange()
{
    std::vector<HBITMAP> icons;
    std::vector<int> ids;
    {
        DuiTreeView t;
        BuildGrayTree(t, 1, true, icons, ids);
        PaintTreeOnWhite(t, kGrayRowH, nullptr);
        EXPECT_INT(t.GetGrayIconConversionCount(), 1, _T("GrayRebuild/first"));

        HBITMAP other = MakeSolidIcon(10, 200, 60, 255);
        icons.push_back(other);
        t.SetItemIcon(ids[0], other);
        EXPECT_INT((int)t.GetGrayIconCacheCount(), 0, _T("GrayRebuild/releasedOnIconChange"));
        PaintTreeOnWhite(t, kGrayRowH, nullptr);
        EXPECT_INT(t.GetGrayIconConversionCount(), 2, _T("GrayRebuild/reconverted"));
        EXPECT_INT((int)t.GetGrayIconCacheCount(), 1, _T("GrayRebuild/cached"));

        t.SetItemIconGrayed(ids[0], false);
        EXPECT_INT((int)t.GetGrayIconCacheCount(), 0, _T("GrayRebuild/releasedOnUngray"));
        PaintTreeOnWhite(t, kGrayRowH, nullptr);
        EXPECT_INT(t.GetGrayIconConversionCount(), 2, _T("GrayRebuild/noConversionWhenColored"));
    }
    DeleteIcons(icons);
    return OK(_T("GrayIconRebuiltOnIconChange"));
}

// 删除节点、Clear 时释放缓存；控件销毁后 GDI 对象数回到建树之前，没有泄漏。
static Result Test_GrayIconReleasedNoLeak()
{
    const int kRows = 3;
    const DWORD before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    std::vector<HBITMAP> icons;
    std::vector<int> ids;
    {
        DuiTreeView t;
        BuildGrayTree(t, kRows, true, icons, ids);
        PaintTreeOnWhite(t, kRows * kGrayRowH, nullptr);
        EXPECT_INT((int)t.GetGrayIconCacheCount(), kRows, _T("GrayRelease/cached"));
        t.Remove(ids[1]);
        EXPECT_INT((int)t.GetGrayIconCacheCount(), kRows - 1, _T("GrayRelease/afterRemove"));
        t.Clear();
        EXPECT_INT((int)t.GetGrayIconCacheCount(), 0, _T("GrayRelease/afterClear"));

        // 重新建一棵、画过后不清空，交给析构释放
        std::vector<int> ids2;
        BuildGrayTree(t, kRows, true, icons, ids2);
        PaintTreeOnWhite(t, kRows * kGrayRowH, nullptr);
        EXPECT_INT((int)t.GetGrayIconCacheCount(), kRows, _T("GrayRelease/cachedAgain"));
    }
    DeleteIcons(icons);
    const DWORD after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    EXPECT_INT((int)after, (int)before, _T("GrayRelease/gdiObjects"));
    return OK(_T("GrayIconReleasedNoLeak"));
}

// 透明通道模式下半透明像素灰显后不变暗：图标像素是预乘值，灰度直接按预乘值换算即可，不能再乘一次
// alpha。半透明度 128、预乘灰度 64 的像素叠在白底上应得 64 + 255 × (255 − 128) / 255 ≈ 191；
// 多乘一次 alpha 会得到约 159（2026-10-04 之前的写法）。
static Result Test_GrayIconPremultipliedNotDarkened()
{
    const BYTE kAlpha = 128;
    const BYTE kPremulGray = 64;
    const int kExpected = 191;
    const int kTolerance = 4;
    HBITMAP icon = MakeSolidIcon(kPremulGray, kPremulGray, kPremulGray, kAlpha);
    if (!icon)
    {
        return Fail(_T("GrayPremul"), _T("icon dib failed"));
    }
    std::vector<COLORREF> px;
    {
        DuiTreeView t;
        t.SetRowHeight(kGrayRowH);
        t.SetIconSize(kGrayIconPx);
        t.SetIconUsesAlpha(true);
        const int id = t.AddRoot(_T(""), icon);
        t.SetItemIconGrayed(id, true);
        PaintTreeOnWhite(t, kGrayRowH, &px);
    }
    ::DeleteObject(icon);

    // 画布上图标所在的那些像素（不是白色的）都应当在期望值附近。
    int iconPixels = 0;
    int nearExpected = 0;
    for (size_t i = 0; i < px.size(); ++i)
    {
        const COLORREF c = px[i];
        if (GetRValue(c) == 255 && GetGValue(c) == 255 && GetBValue(c) == 255)
        {
            continue;
        }
        ++iconPixels;
        if (abs((int)GetRValue(c) - kExpected) <= kTolerance && abs((int)GetGValue(c) - kExpected) <= kTolerance
            && abs((int)GetBValue(c) - kExpected) <= kTolerance)
        {
            ++nearExpected;
        }
    }
    EXPECT_TRUE(iconPixels >= kGrayIconPx * kGrayIconPx, _T("GrayPremul/iconDrawn"));
    EXPECT_INT(nearExpected, kGrayIconPx * kGrayIconPx, _T("GrayPremul/notDarkened"));
    return OK(_T("GrayIconPremultipliedNotDarkened"));
}

// TVO1（2026-10-04 起）：多列模式的两条滚动条为悬浮式，不占表体宽高：竖直滚动范围按
// 「表头以下整块高度」算，两条命中带分别贴右缘 / 下缘，且彼此不重叠。
static Result Test_TVO1_MultiColBarsOverlay()
{
#if BUI_FEATURE_SCROLLBAR
    const int kW = 200;          // 控件宽
    const int kH = 100;          // 控件高
    const int kHeaderH = 26;     // 表头高
    const int kRowH = 28;        // 行高
    const int kRows = 10;        // 行数：10 × 28 = 280，超出表体 74
    const int kColW = 150;       // 两列各 150，共 300，超出宽 200
    const int kBand = DuiScrollBar::kOverlayBandPx;
    DuiTreeView t;
    t.SetHeaderHeight(kHeaderH);
    t.SetRowHeight(kRowH);
    t.AddColumn(_T("A"), kColW);
    t.AddColumn(_T("B"), kColW);
    for (int i = 0; i < kRows; ++i)
    {
        t.AddRoot(_T("row"));
    }
    t.Layout(RECT{ 0, 0, kW, kH });

    DuiScrollBar* vb = nullptr;
    DuiScrollBar* hb = nullptr;
    for (size_t i = 0; i < t.Children().size(); ++i)
    {
        DuiScrollBar* sb = dynamic_cast<DuiScrollBar*>(t.Children()[i].get());
        if (sb != nullptr)
        {
            if (sb->IsHorizontal())
            {
                hb = sb;
            }
            else
            {
                vb = sb;
            }
        }
    }
    if (vb == nullptr || hb == nullptr || !vb->IsVisible() || !hb->IsVisible())
    {
        return Fail(_T("TVO1"), _T("precondition: both scroll bars should be visible"));
    }
    // 竖直范围 = 内容高 - 表头以下整块高（不再扣掉水平滚动条那一条）
    EXPECT_INT(vb->GetMax(), kRows * kRowH - (kH - kHeaderH), _T("TVO1/vMax"));
    const RECT rv = vb->GetRect();
    const RECT rh = hb->GetRect();
    EXPECT_INT(rv.left, kW - kBand, _T("TVO1/vLeft"));
    EXPECT_INT(rv.right, kW, _T("TVO1/vRight"));
    EXPECT_INT(rv.top, kHeaderH, _T("TVO1/vTop"));
    EXPECT_INT(rv.bottom, kH - kBand, _T("TVO1/vBottom"));
    EXPECT_INT(rh.top, kH - kBand, _T("TVO1/hTop"));
    EXPECT_INT(rh.bottom, kH, _T("TVO1/hBottom"));
    EXPECT_INT(rh.right, kW - kBand, _T("TVO1/hRight"));
#endif
    return OK(_T("TVO1_MultiColBarsOverlay"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_STR


} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("AddRootGetCount"),       &Test_AddRootGetCount       },
        { _T("TVO1_MultiColBarsOverlay"), &Test_TVO1_MultiColBarsOverlay },
        { _T("AddChildOrder"),         &Test_AddChildOrder         },
        { _T("AddChildBadParent"),     &Test_AddChildBadParent     },
        { _T("CollapseHidesDescendants"), &Test_CollapseHidesDescendants },
        { _T("ExpandCollapseAll"),     &Test_ExpandCollapseAll     },
        { _T("SelectionBasic"),        &Test_SelectionBasic        },
        { _T("RemoveClearsSelection"), &Test_RemoveClearsSelection },
        { _T("RemoveSubtree"),         &Test_RemoveSubtree         },
        { _T("PerItemStateRoundTrip"), &Test_PerItemStateRoundTrip },
        { _T("QueriesBogusIdSafe"),    &Test_QueriesBogusIdSafe    },
        { _T("ContentHeight"),         &Test_ContentHeight         },
        { _T("HitTestRow"),            &Test_HitTestRow            },
        { _T("ClampMetrics"),          &Test_ClampMetrics          },
        { _T("HasChildren"),           &Test_HasChildren           },
        { _T("Clear"),                 &Test_Clear                 },
        { _T("IdLookupManyRoots"),               &Test_IdLookupManyRoots              },
        { _T("MiddleInsertKeepsLookups"),        &Test_MiddleInsertKeepsLookups       },
        { _T("RemoveMiddleSubtreeLookups"),      &Test_RemoveMiddleSubtreeLookups     },
        { _T("ClearThenAddIds"),                 &Test_ClearThenAddIds                },
        { _T("VisibleQueriesFreshAfterBulkAdd"), &Test_VisibleQueriesFreshAfterBulkAdd },
        { _T("RandomOpsMatchReference"),         &Test_RandomOpsMatchReference        },
        { _T("BulkBuildIsNotQuadratic"),         &Test_BulkBuildIsNotQuadratic        },
        { _T("MoveRootToFront"),                 &Test_MoveRootToFront                },
        { _T("MoveRootToEnd"),                   &Test_MoveRootToEnd                  },
        { _T("MoveRootKeepsState"),              &Test_MoveRootKeepsState             },
        { _T("MoveRootInvalidArgs"),             &Test_MoveRootInvalidArgs            },
        { _T("AddChildAfterMove"),               &Test_AddChildAfterMove              },
        { _T("RandomMovesMatchReference"),       &Test_RandomMovesMatchReference      },
        { _T("MoveRootBulkSpeed"),               &Test_MoveRootBulkSpeed              },
        { _T("MoveLastToFrontSpeed"),            &Test_MoveLastToFrontSpeed           },
        { _T("GrayIconConvertedOnce"),           &Test_GrayIconConvertedOnce          },
        { _T("GrayIconRebuiltOnIconChange"),     &Test_GrayIconRebuiltOnIconChange    },
        { _T("GrayIconReleasedNoLeak"),          &Test_GrayIconReleasedNoLeak         },
        { _T("GrayIconPremultipliedNotDarkened"), &Test_GrayIconPremultipliedNotDarkened },
    };

    CString out;
    int passed = 0, failed = 0;
    for (auto& e : tests)
    {
        Result r = e.fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), e.name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), e.name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }
    CString summary;
    summary.Format(_T("[summary] DuiTreeViewTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiTreeViewTests

} // namespace balloonwjui

#endif // BUI_FEATURE_TREEVIEW
