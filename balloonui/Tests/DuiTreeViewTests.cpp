#include "stdafx.h"
#include "DuiTreeViewTests.h"

#if BUI_FEATURE_TREEVIEW


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
