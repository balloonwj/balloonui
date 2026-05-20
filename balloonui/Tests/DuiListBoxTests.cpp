#include "stdafx.h"
#include "DuiListBoxTests.h"

#if BUI_FEATURE_LISTBOX


namespace balloonwjui {

namespace DuiListBoxTests {

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
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("got '%s'"), (LPCTSTR)_a); return Fail(name, _d); } \
    } while (0)

// ----- DuiListBox: data --------------------------------------------------

static Result Test_LB_AddDeleteCount()
{
    DuiListBox lb;
    EXPECT_INT(lb.GetItemCount(), 0, _T("LB/empty"));
    EXPECT_INT(lb.AddItem(_T("a")), 0, _T("LB/add0idx"));
    EXPECT_INT(lb.AddItem(_T("b")), 1, _T("LB/add1idx"));
    EXPECT_INT(lb.AddItem(_T("c")), 2, _T("LB/add2idx"));
    EXPECT_INT(lb.GetItemCount(), 3, _T("LB/count3"));
    lb.DeleteItem(1);
    EXPECT_INT(lb.GetItemCount(), 2, _T("LB/count2"));
    EXPECT_STR(lb.GetItemText(0), _T("a"), _T("LB/firstAfterDel"));
    EXPECT_STR(lb.GetItemText(1), _T("c"), _T("LB/secondAfterDel"));
    lb.DeleteAllItems();
    EXPECT_INT(lb.GetItemCount(), 0, _T("LB/clearAll"));
    return OK(_T("LB_AddDeleteCount"));
}

// Insert in middle shifts selection.
static Result Test_LB_InsertShiftsSel()
{
    DuiListBox lb;
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    lb.SetCurSel(2);  // pointing at "c"
    EXPECT_INT(lb.GetCurSel(), 2, _T("LB/sel2"));
    lb.InsertItem(0, _T("x"));
    EXPECT_INT(lb.GetCurSel(), 3, _T("LB/selShifted"));
    EXPECT_STR(lb.GetItemText(0), _T("x"), _T("LB/inserted"));
    EXPECT_STR(lb.GetItemText(3), _T("c"), _T("LB/cMoved"));
    return OK(_T("LB_InsertShiftsSel"));
}

// Delete selected resets sel; delete before selected shifts down.
static Result Test_LB_DeleteAdjustsSel()
{
    DuiListBox lb;
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    lb.SetCurSel(2);
    lb.DeleteItem(1);   // remove "b" - sel was 2 ("c"), should now be 1
    EXPECT_INT(lb.GetCurSel(), 1, _T("LB/selShiftDownAfterDel"));
    EXPECT_STR(lb.GetItemText(1), _T("c"), _T("LB/cIsAt1"));
    lb.DeleteItem(1);   // remove "c" (the selection)
    EXPECT_INT(lb.GetCurSel(), -1, _T("LB/selResetAfterOwnDel"));
    return OK(_T("LB_DeleteAdjustsSel"));
}

static Result Test_LB_OOBSafe()
{
    DuiListBox lb;
    lb.AddItem(_T("a"));
    lb.SetCurSel(99);
    EXPECT_INT(lb.GetCurSel(), -1, _T("LB/oobSelIgnored"));
    EXPECT_STR(lb.GetItemText(-1), _T(""), _T("LB/oobNeg"));
    EXPECT_STR(lb.GetItemText(99), _T(""), _T("LB/oobPos"));
    lb.DeleteItem(99);   // safe no-op
    EXPECT_INT(lb.GetItemCount(), 1, _T("LB/oobDelNoop"));
    return OK(_T("LB_OOBSafe"));
}

// Keyboard navigation: VK_DOWN advances, VK_END jumps to last.
static Result Test_LB_KeyboardNav()
{
    DuiListBox lb;
    for (int i = 0; i < 5; ++i)
    {
        lb.AddItem(_T("x"));
    }
    lb.SetCurSel(0);
    lb.OnKeyDown(VK_DOWN, 0);
    EXPECT_INT(lb.GetCurSel(), 1, _T("LB/down"));
    lb.OnKeyDown(VK_END, 0);
    EXPECT_INT(lb.GetCurSel(), 4, _T("LB/end"));
    lb.OnKeyDown(VK_HOME, 0);
    EXPECT_INT(lb.GetCurSel(), 0, _T("LB/home"));
    lb.OnKeyDown(VK_DOWN, 0);
    lb.OnKeyDown(VK_DOWN, 0);   // sel=2
    lb.OnKeyDown(VK_UP, 0);
    EXPECT_INT(lb.GetCurSel(), 1, _T("LB/up"));
    return OK(_T("LB_KeyboardNav"));
}

// Item param round-trip.
static Result Test_LB_ItemParam()
{
    DuiListBox lb;
    lb.AddItem(_T("a"), 100);
    lb.AddItem(_T("b"), 200);
    EXPECT_INT((int)lb.GetItemParam(0), 100, _T("LB/p0"));
    EXPECT_INT((int)lb.GetItemParam(1), 200, _T("LB/p1"));
    lb.SetItemParam(1, 999);
    EXPECT_INT((int)lb.GetItemParam(1), 999, _T("LB/setParam"));
    return OK(_T("LB_ItemParam"));
}

// ----- DuiVirtualList ----------------------------------------------------

static Result Test_VL_Defaults()
{
    DuiVirtualList vl;
    EXPECT_INT(vl.GetRowCount(), 0,   _T("VL/empty"));
    EXPECT_INT(vl.GetCurSel(),  -1,   _T("VL/noSel"));
    return OK(_T("VL_Defaults"));
}

static Result Test_VL_RowCountClamps()
{
    DuiVirtualList vl;
    vl.SetRowCount(100);
    vl.SetCurSel(50);
    EXPECT_INT(vl.GetCurSel(), 50, _T("VL/sel50"));
    vl.SetRowCount(20);     // shrink below selection
    EXPECT_INT(vl.GetCurSel(), -1, _T("VL/selResetOnShrink"));
    vl.SetRowCount(-5);
    EXPECT_INT(vl.GetRowCount(), 0, _T("VL/negClamp"));
    return OK(_T("VL_RowCountClamps"));
}

static Result Test_VL_KeyboardPgUpDown()
{
    DuiVirtualList vl;
    vl.SetRowCount(1000);
    vl.SetRowHeight(20);
    vl.Layout(RECT{ 0, 0, 200, 200 });   // viewH=200 -> 10 rows visible
    vl.SetCurSel(0);
    vl.OnKeyDown(VK_NEXT, 0);    // PageDown 10
    EXPECT_INT(vl.GetCurSel(), 10, _T("VL/pgDown"));
    vl.OnKeyDown(VK_PRIOR, 0);
    EXPECT_INT(vl.GetCurSel(),  0, _T("VL/pgUp"));
    vl.OnKeyDown(VK_END, 0);
    EXPECT_INT(vl.GetCurSel(), 999, _T("VL/end"));
    vl.OnKeyDown(VK_HOME, 0);
    EXPECT_INT(vl.GetCurSel(),   0, _T("VL/home"));
    return OK(_T("VL_KeyboardPgUpDown"));
}

// Paint callback fires only for visible rows.
struct PaintCounter { int hits = 0; int firstSeen = -1; int lastSeen = -1; };
static void CountPaint(void* user, HDC, int idx, const RECT&, bool, bool)
{
    auto* c = static_cast<PaintCounter*>(user);
    c->hits++;
    if (c->firstSeen < 0)
    {
        c->firstSeen = idx;
    }
    c->lastSeen = idx;
}

static Result Test_VL_PaintOnlyVisible()
{
    DuiVirtualList vl;
    vl.SetRowCount(1000);
    vl.SetRowHeight(20);
    vl.Layout(RECT{ 0, 0, 200, 200 });   // 10 rows visible (+1 partial)
    PaintCounter pc;
    vl.SetPaintRowCallback(&CountPaint, &pc);
    HDC hdc = ::GetDC(nullptr);
    vl.OnPaint(hdc, RECT{ 0, 0, 200, 200 });
    ::ReleaseDC(nullptr, hdc);
    // Should paint 10 visible + 1 overlap = 11 rows starting at 0.
    if (pc.hits < 10 || pc.hits > 11)
    {
        CString d;
        d.Format(_T("hits=%d expected 10..11"), pc.hits);
        return Fail(_T("VL/paintHits"), d);
    }
    EXPECT_INT(pc.firstSeen, 0, _T("VL/paintFirst0"));
    return OK(_T("VL_PaintOnlyVisible"));
}

#undef EXPECT_INT
#undef EXPECT_STR

// ----- multi-select / checkbox / drag-reorder ------------------------

static Result Test_LB_DefaultsForNewState()
{
    DuiListBox lb;
    if (lb.IsMultiSelect())
    {
        return Fail(_T("Def/multi"), _T("expected single"));
    }
    if (lb.GetShowCheckboxes())
    {
        return Fail(_T("Def/cb"), _T("expected hidden"));
    }
    if (lb.GetDragReorderEnabled())
    {
        return Fail(_T("Def/drag"), _T("expected off"));
    }
    int a = lb.AddItem(_T("a"));
    if (lb.IsItemSelected(a))
    {
        return Fail(_T("Def/initSel"), _T("new item selected"));
    }
    if (lb.IsItemChecked(a))
    {
        return Fail(_T("Def/initChk"), _T("new item checked"));
    }
    return OK(_T("LB_DefaultsForNewState"));
}

static Result Test_LB_SingleSelectKeepsOneRow()
{
    DuiListBox lb;
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    lb.SetCurSel(1, false);
    if (!lb.IsItemSelected(1))
    {
        return Fail(_T("Sgl/1"), _T("idx1 not selected"));
    }
    if (lb.IsItemSelected(0) || lb.IsItemSelected(2))
    {
        return Fail(_T("Sgl/others"), _T("expected only one selected"));
    }
    if (lb.GetSelectionCount() != 1)
    {
        return Fail(_T("Sgl/count"), _T("count != 1"));
    }
    return OK(_T("LB_SingleSelectKeepsOneRow"));
}

static Result Test_LB_MultiSelectIndependent()
{
    DuiListBox lb;
    lb.SetMultiSelect(true);
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    lb.AddItem(_T("d"));
    lb.SetItemSelected(0, true, false);
    lb.SetItemSelected(2, true, false);
    if (lb.GetSelectionCount() != 2)
    {
        return Fail(_T("Mlt/count"), _T("count != 2"));
    }
    if (!lb.IsItemSelected(0))
    {
        return Fail(_T("Mlt/0"),     _T("0 not sel"));
    }
    if (lb.IsItemSelected(1))
    {
        return Fail(_T("Mlt/1"),     _T("1 unexpectedly sel"));
    }
    if (!lb.IsItemSelected(2))
    {
        return Fail(_T("Mlt/2"),     _T("2 not sel"));
    }

    std::vector<int> sel;
    lb.GetSelectedIndices(sel);
    if (sel.size() != 2 || sel[0] != 0 || sel[1] != 2)
    {
        return Fail(_T("Mlt/idx"), _T("indices wrong"));
    }

    lb.ClearSelection();
    if (lb.GetSelectionCount() != 0)
    {
        return Fail(_T("Mlt/clr"),   _T("not cleared"));
    }
    return OK(_T("LB_MultiSelectIndependent"));
}

static Result Test_LB_MultiToSingleCollapsesToCurSel()
{
    DuiListBox lb;
    lb.SetMultiSelect(true);
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    lb.SetItemSelected(0, true, false);
    lb.SetItemSelected(1, true, false);
    lb.SetItemSelected(2, true, false);
    lb.SetCurSel(1, false);
    lb.SetMultiSelect(false);
    if (lb.GetSelectionCount() != 1)
    {
        return Fail(_T("M2S/count"), _T("count != 1"));
    }
    if (!lb.IsItemSelected(1))
    {
        return Fail(_T("M2S/sel"),   _T("idx1 lost"));
    }
    return OK(_T("LB_MultiToSingleCollapsesToCurSel"));
}

static Result Test_LB_CheckedRoundTrip()
{
    DuiListBox lb;
    lb.SetShowCheckboxes(true);
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    if (lb.IsItemChecked(0))
    {
        return Fail(_T("Chk/init0"), _T("starts checked"));
    }
    lb.SetItemChecked(0, true);
    if (!lb.IsItemChecked(0))
    {
        return Fail(_T("Chk/set0"),  _T("set true failed"));
    }
    if (lb.IsItemChecked(1))
    {
        return Fail(_T("Chk/leak1"), _T("idx1 unexpectedly checked"));
    }
    lb.SetItemChecked(0, false);
    if (lb.IsItemChecked(0))
    {
        return Fail(_T("Chk/clr0"),  _T("clear failed"));
    }
    return OK(_T("LB_CheckedRoundTrip"));
}

static Result Test_LB_MoveItemReorders()
{
    DuiListBox lb;
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    lb.AddItem(_T("d"));
    // Move "a" (idx 0) to before "d" (idx 3): "to" = 3 means "land at
    // idx 3 after removing src=0", which is the "before-d" slot.
    lb.MoveItem(0, 3);
    if (lb.GetItemText(0) != _T("b"))
    {
        return Fail(_T("Mv/0"), _T("0 != b"));
    }
    if (lb.GetItemText(1) != _T("c"))
    {
        return Fail(_T("Mv/1"), _T("1 != c"));
    }
    if (lb.GetItemText(2) != _T("a"))
    {
        return Fail(_T("Mv/2"), _T("2 != a"));
    }
    if (lb.GetItemText(3) != _T("d"))
    {
        return Fail(_T("Mv/3"), _T("3 != d"));
    }

    // Move "d" (now idx 3) to slot 0 (the very front).
    lb.MoveItem(3, 0);
    if (lb.GetItemText(0) != _T("d"))
    {
        return Fail(_T("Mv2/0"), _T("0 != d"));
    }
    if (lb.GetItemText(1) != _T("b"))
    {
        return Fail(_T("Mv2/1"), _T("1 != b"));
    }

    return OK(_T("LB_MoveItemReorders"));
}

static Result Test_LB_MoveItemIdempotentSameSlot()
{
    DuiListBox lb;
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    lb.MoveItem(1, 1);     // same slot -> no change
    if (lb.GetItemText(1) != _T("b"))
    {
        return Fail(_T("MvId/1"), _T("rearranged"));
    }
    return OK(_T("LB_MoveItemIdempotentSameSlot"));
}

static Result Test_LB_MoveItemTracksCurSel()
{
    DuiListBox lb;
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    lb.SetCurSel(0, false);
    lb.MoveItem(0, 3);     // a -> end
    // m_curSel should have followed the moved item.
    if (lb.GetCurSel() != 2)
    {
        CString d;
        d.Format(_T("expected 2 got %d"), lb.GetCurSel());
        return Fail(_T("MvCs/track"), d);
    }
    return OK(_T("LB_MoveItemTracksCurSel"));
}

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("LB_AddDeleteCount"),    &Test_LB_AddDeleteCount    },
        { _T("LB_InsertShiftsSel"),   &Test_LB_InsertShiftsSel   },
        { _T("LB_DeleteAdjustsSel"),  &Test_LB_DeleteAdjustsSel  },
        { _T("LB_OOBSafe"),           &Test_LB_OOBSafe           },
        { _T("LB_KeyboardNav"),       &Test_LB_KeyboardNav       },
        { _T("LB_ItemParam"),         &Test_LB_ItemParam         },
        { _T("VL_Defaults"),          &Test_VL_Defaults          },
        { _T("VL_RowCountClamps"),    &Test_VL_RowCountClamps    },
        { _T("VL_KeyboardPgUpDown"),  &Test_VL_KeyboardPgUpDown  },
        { _T("VL_PaintOnlyVisible"),  &Test_VL_PaintOnlyVisible  },
        { _T("LB_DefaultsForNewState"),     &Test_LB_DefaultsForNewState     },
        { _T("LB_SingleSelectKeepsOneRow"), &Test_LB_SingleSelectKeepsOneRow },
        { _T("LB_MultiSelectIndependent"),  &Test_LB_MultiSelectIndependent  },
        { _T("LB_MultiToSingleCollapsesToCurSel"), &Test_LB_MultiToSingleCollapsesToCurSel },
        { _T("LB_CheckedRoundTrip"),        &Test_LB_CheckedRoundTrip        },
        { _T("LB_MoveItemReorders"),        &Test_LB_MoveItemReorders        },
        { _T("LB_MoveItemIdempotentSameSlot"), &Test_LB_MoveItemIdempotentSameSlot },
        { _T("LB_MoveItemTracksCurSel"),    &Test_LB_MoveItemTracksCurSel    }
    };

    CString out;
    int passed = 0, failed = 0;
    for (auto& e : tests)
    {
        Result r = e.fn();
        if (r.ok)
        {
            ++passed;
            CString line;
            line.Format(_T("[ok]   %s"), e.name);
            if (!out.IsEmpty())
            {
                out += _T("\r\n");
            }
            out += line;
        }
        else
        {
            ++failed;
            CString line;
            line.Format(_T("[FAIL] %s : %s"), e.name, (LPCTSTR)r.detail);
            if (!out.IsEmpty())
            {
                out += _T("\r\n");
            }
            out += line;
        }
    }
    CString summary;
    summary.Format(_T("[summary] DuiListBoxTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiListBoxTests

} // namespace balloonwjui

#endif // BUI_FEATURE_LISTBOX
