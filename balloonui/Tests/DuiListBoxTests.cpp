#include "stdafx.h"
#include "DuiListBoxTests.h"
#include "../DuiAnimation.h"   // LBO3：用 DuiAnimMgr::TickAll 推进滚动条的淡入

#include <stdlib.h>   // abs：图标与副文字绘制用例比较像素分量

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

// Wheel consumption follows DuiScrollBar, which DuiListBox forwards to:
// a list whose items all fit has an empty scroll range and must let the
// wheel bubble to an outer scroll container; once the items overflow it
// consumes the wheel, edge included. See DuiHost::DispatchMouseWheel.
static Result Test_LB_WheelBubblesWhenItemsFit()
{
    DuiListBox lb;
    lb.SetItemHeight(22);
    lb.SetRect(RECT{ 0, 0, 300, 200 });
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));                 // 3 * 22 = 66 px, well under 200
    EXPECT_INT(lb.GetScrollBar()->GetMax(), 0, _T("LB_Wheel/emptyRange"));
    bool fits = lb.OnMouseWheel(POINT{ 10, 10 }, -WHEEL_DELTA, 0);
    EXPECT_INT((int)fits, 0, _T("LB_Wheel/fitsNotConsumed"));

    for (int i = 0; i < 50; ++i)         // 53 * 22 = 1166 px, overflows
    {
        lb.AddItem(_T("more"));
    }
    if (lb.GetScrollBar()->GetMax() <= 0)
    {
        return Fail(_T("LB_WheelBubblesWhenItemsFit"), _T("expected a scrollable range after filling"));
    }
    bool overflow = lb.OnMouseWheel(POINT{ 10, 10 }, -WHEEL_DELTA, 0);
    EXPECT_INT((int)overflow, 1, _T("LB_Wheel/overflowConsumed"));

    lb.GetScrollBar()->SetPos(lb.GetScrollBar()->GetMax());
    bool atBottom = lb.OnMouseWheel(POINT{ 10, 10 }, -WHEEL_DELTA, 0);
    EXPECT_INT((int)atBottom, 1, _T("LB_Wheel/atBottomStillConsumed"));
    return OK(_T("LB_WheelBubblesWhenItemsFit"));
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

// ----- 排版之后行数 / 行高变化时滚动条的位置（2026-10-03 修复） ----------
// 控件先按当前数据排好版，之后数据变多、内容超出一屏，滚动条从隐藏变为可见。
// 此时滚动条必须已经放在列表右侧：矩形为空的话既画不出来，也命中不到、无法拖动。

// 用例统一使用的列表尺寸，单位像素
static const int kSbListW = 200;
static const int kSbListH = 100;
// DuiListBox / DuiVirtualList 默认的滚动条命中带宽度（DuiListBox.h 的 m_sbWidth），单位像素；
// 2026-10-04 起为悬浮式滚动条的 DuiScrollBar::kOverlayBandPx，此前是 12
static const int kSbDefaultWidth = DuiScrollBar::kOverlayBandPx;
// 行高，单位像素：100 高的列表放得下 5 行
static const int kSbRowH = 20;
// 放得下（3 行共 60 像素）与放不下（30 行共 600 像素）的行数
static const int kSbRowsFit = 3;
static const int kSbRowsOverflow = 30;

// 列表右侧滚动条上的一点（滚动条宽度的正中、列表高度的正中）
static POINT SbProbePoint()
{
    POINT pt = { kSbListW - kSbDefaultWidth / 2, kSbListH / 2 };
    return pt;
}

// 检查滚动条可见、矩形贴在列表右侧，并且滚动条上的点命中滚动条本身
static Result CheckSbPlaced(DuiControl* list, DuiScrollBar* sb, LPCTSTR name)
{
    if (!sb->IsVisible())
    {
        return Fail(name, _T("scrollbar hidden although content overflows"));
    }
    const RECT& rc = sb->GetRect();
    RECT want = { kSbListW - kSbDefaultWidth, 0, kSbListW, kSbListH };
    if (!::EqualRect(&rc, &want))
    {
        CString d;
        d.Format(_T("scrollbar rect=(%d,%d,%d,%d) expected=(%d,%d,%d,%d)"),
                 rc.left, rc.top, rc.right, rc.bottom,
                 want.left, want.top, want.right, want.bottom);
        return Fail(name, d);
    }
    if (list->HitTest(SbProbePoint()) != sb)
    {
        return Fail(name, _T("HitTest on the scrollbar area did not return the scrollbar"));
    }
    return OK(name);
}

// 检查滚动条已隐藏，滚动条原来所在位置上的点交还给列表本身
static Result CheckSbHidden(DuiControl* list, DuiScrollBar* sb, LPCTSTR name)
{
    if (sb->IsVisible())
    {
        return Fail(name, _T("scrollbar visible although content fits"));
    }
    if (list->HitTest(SbProbePoint()) != list)
    {
        return Fail(name, _T("HitTest on the former scrollbar area did not return the list"));
    }
    return OK(name);
}

// 虚拟列表先按 0 行排版，数据到达后再设行数（flamingoAdmin 企业管理页即是这种用法）；
// 随后行数减少到放得下、再增多，滚动条应随之隐藏、再出现在原位置
static Result Test_VL_SbPlacedWhenRowsArriveAfterLayout()
{
    DuiVirtualList vl;
    vl.SetRowHeight(kSbRowH);
    vl.Layout(RECT{ 0, 0, kSbListW, kSbListH });
    Result r = CheckSbHidden(&vl, vl.GetScrollBar(), _T("VL_SbRows/empty"));
    if (!r.ok)
    {
        return r;
    }
    vl.SetRowCount(kSbRowsOverflow);
    r = CheckSbPlaced(&vl, vl.GetScrollBar(), _T("VL_SbRows/overflow"));
    if (!r.ok)
    {
        return r;
    }
    vl.SetRowCount(kSbRowsFit);
    r = CheckSbHidden(&vl, vl.GetScrollBar(), _T("VL_SbRows/shrink"));
    if (!r.ok)
    {
        return r;
    }
    vl.SetRowCount(kSbRowsOverflow);
    r = CheckSbPlaced(&vl, vl.GetScrollBar(), _T("VL_SbRows/regrow"));
    if (!r.ok)
    {
        return r;
    }
    return OK(_T("VL_SbPlacedWhenRowsArriveAfterLayout"));
}

// 虚拟列表排版之后加大行高，使内容超出一屏
static Result Test_VL_SbPlacedWhenRowHeightGrowsAfterLayout()
{
    DuiVirtualList vl;
    vl.SetRowHeight(kSbRowH);
    vl.SetRowCount(kSbRowsFit);
    vl.Layout(RECT{ 0, 0, kSbListW, kSbListH });
    Result r = CheckSbHidden(&vl, vl.GetScrollBar(), _T("VL_SbRowH/fit"));
    if (!r.ok)
    {
        return r;
    }
    // 3 行 × 60 像素 = 180 像素，超过 100
    vl.SetRowHeight(kSbRowH * kSbRowsFit);
    r = CheckSbPlaced(&vl, vl.GetScrollBar(), _T("VL_SbRowH/grow"));
    if (!r.ok)
    {
        return r;
    }
    return OK(_T("VL_SbPlacedWhenRowHeightGrowsAfterLayout"));
}

// 虚拟列表在排版之前就设好行数（客户端 SettingsNavList 的用法）：排版之后滚动条照常就位。
// 这条在修复前也能通过，用来确认修复没有改变这种用法的结果
static Result Test_VL_SbPlacedWhenRowsSetBeforeLayout()
{
    DuiVirtualList vl;
    vl.SetRowHeight(kSbRowH);
    vl.SetRowCount(kSbRowsOverflow);
    vl.Layout(RECT{ 0, 0, kSbListW, kSbListH });
    Result r = CheckSbPlaced(&vl, vl.GetScrollBar(), _T("VL_SbBefore/layout"));
    if (!r.ok)
    {
        return r;
    }
    return OK(_T("VL_SbPlacedWhenRowsSetBeforeLayout"));
}

// 普通列表排版之后逐条添加，直到超出一屏；清空后再用 InsertItem 插回去
static Result Test_LB_SbPlacedWhenItemsAddedAfterLayout()
{
    DuiListBox lb;
    lb.SetItemHeight(kSbRowH);
    lb.SetRect(RECT{ 0, 0, kSbListW, kSbListH });
    for (int i = 0; i < kSbRowsFit; ++i)
    {
        lb.AddItem(_T("fit"));
    }
    Result r = CheckSbHidden(&lb, lb.GetScrollBar(), _T("LB_SbAdd/fit"));
    if (!r.ok)
    {
        return r;
    }
    for (int i = kSbRowsFit; i < kSbRowsOverflow; ++i)
    {
        lb.AddItem(_T("more"));
    }
    r = CheckSbPlaced(&lb, lb.GetScrollBar(), _T("LB_SbAdd/overflow"));
    if (!r.ok)
    {
        return r;
    }
    lb.DeleteAllItems();
    r = CheckSbHidden(&lb, lb.GetScrollBar(), _T("LB_SbAdd/cleared"));
    if (!r.ok)
    {
        return r;
    }
    for (int i = 0; i < kSbRowsOverflow; ++i)
    {
        lb.InsertItem(0, _T("inserted"));
    }
    r = CheckSbPlaced(&lb, lb.GetScrollBar(), _T("LB_SbAdd/inserted"));
    if (!r.ok)
    {
        return r;
    }
    return OK(_T("LB_SbPlacedWhenItemsAddedAfterLayout"));
}

// 普通列表排版之后加大行高，使内容超出一屏
static Result Test_LB_SbPlacedWhenItemHeightGrowsAfterLayout()
{
    DuiListBox lb;
    lb.SetItemHeight(kSbRowH);
    for (int i = 0; i < kSbRowsFit; ++i)
    {
        lb.AddItem(_T("fit"));
    }
    lb.SetRect(RECT{ 0, 0, kSbListW, kSbListH });
    Result r = CheckSbHidden(&lb, lb.GetScrollBar(), _T("LB_SbItemH/fit"));
    if (!r.ok)
    {
        return r;
    }
    // 3 项 × 60 像素 = 180 像素，超过 100
    lb.SetItemHeight(kSbRowH * kSbRowsFit);
    r = CheckSbPlaced(&lb, lb.GetScrollBar(), _T("LB_SbItemH/grow"));
    if (!r.ok)
    {
        return r;
    }
    return OK(_T("LB_SbPlacedWhenItemHeightGrowsAfterLayout"));
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

// ----- DuiListBox：每行图标与副文字（2026-10-04 起，登录窗账号下拉列表用）-----

// 文件中段已取消了这两个宏的定义，本段重新定义（与文件开头相同），段末再取消
#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("got '%s'"), (LPCTSTR)_a); return Fail(name, _d); } \
    } while (0)

// 画图用例的内存位图：32 位、自上而下，像素为 0xAARRGGBB
struct TestDib
{
    HDC     m_dc;     // 选入了位图的内存设备上下文
    HBITMAP m_bmp;    // 位图
    HGDIOBJ m_old;    // 选入之前的位图，销毁时换回
    DWORD*  m_bits;   // 像素，按行从上到下排列；由位图持有
    int     m_w;      // 宽，单位：像素
    int     m_h;      // 高，单位：像素
};

// 建一张白底的内存位图。成功返回 true，out 由调用方之后交给 DestroyTestDib。
static bool CreateTestDib(int w, int h, TestDib& out)
{
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;   // 负值表示自上而下
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    out.m_dc = ::CreateCompatibleDC(NULL);
    out.m_bmp = ::CreateDIBSection(out.m_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (out.m_dc == NULL || out.m_bmp == NULL || bits == nullptr)
    {
        return false;
    }
    out.m_old = ::SelectObject(out.m_dc, out.m_bmp);
    out.m_bits = (DWORD*)bits;
    out.m_w = w;
    out.m_h = h;
    RECT rc = { 0, 0, w, h };
    ::FillRect(out.m_dc, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH));
    return true;
}

// 销毁 CreateTestDib 建的位图
static void DestroyTestDib(TestDib& d)
{
    ::SelectObject(d.m_dc, d.m_old);
    ::DeleteObject(d.m_bmp);
    ::DeleteDC(d.m_dc);
}

// 取一个像素的颜色
static COLORREF DibPixel(const TestDib& d, int x, int y)
{
    const DWORD v = d.m_bits[y * d.m_w + x];
    return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

// 建一个纯色、完全不透明的 32 位图标位图（预乘 alpha），调用方用完 DeleteObject
static HBITMAP MakeSolidIcon(int px, COLORREF color)
{
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = px;
    bi.bmiHeader.biHeight = -px;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bmp == NULL || bits == nullptr)
    {
        return NULL;
    }
    const DWORD v = 0xFF000000u | ((DWORD)GetRValue(color) << 16) | ((DWORD)GetGValue(color) << 8) | GetBValue(color);
    DWORD* p = (DWORD*)bits;
    for (int i = 0; i < px * px; ++i)
    {
        p[i] = v;
    }
    return bmp;
}

// 像素是否接近纯红（图标与红色副文字的颜色）
static bool IsReddish(COLORREF c)
{
    const int kStrong = 150;   // 红色分量至少这么大
    const int kWeak = 100;     // 绿、蓝分量至多这么大
    return GetRValue(c) > kStrong && GetGValue(c) < kWeak && GetBValue(c) < kWeak;
}

// 像素是否为深色的中性色（黑灰色的主文字笔画，不含 ClearType 的彩色边缘）
static bool IsDarkNeutral(COLORREF c)
{
    const int kDark = 128;      // 三个分量都低于这个值算深色
    const int kNeutral = 30;    // 三个分量两两相差不超过这个值算中性色
    const int r = GetRValue(c);
    const int g = GetGValue(c);
    const int b = GetBValue(c);
    return r < kDark && g < kDark && b < kDark && abs(r - g) <= kNeutral && abs(g - b) <= kNeutral;
}

// 像素是否明显偏蓝（纯蓝主文字的笔画）。白底上的纯蓝文字，不论是否经 ClearType 按子像素混合，
// 蓝通道在底色与文字里都是 255、混合后仍是 255，只有红、绿通道随笔画覆盖程度变小
static bool IsBluish(COLORREF c)
{
    const int kStrong = 150;   // 蓝色分量至少这么大
    const int kWeak = 100;     // 红、绿分量至多这么大
    return GetBValue(c) > kStrong && GetRValue(c) < kWeak && GetGValue(c) < kWeak;
}

// 图标与副文字跟着条目走：插入、删除、移动、改主文字都不会错位，越界读写安全
static Result Test_LB_IconSubTextFollowItems()
{
    const int kDefaultIconPx = 16;   // 图标边长的默认值
    const int kFarIndex = 99;        // 越界的下标
    HBITMAP iconA = MakeSolidIcon(kDefaultIconPx, RGB(255, 0, 0));
    HBITMAP iconC = MakeSolidIcon(kDefaultIconPx, RGB(0, 0, 255));
    DuiListBox lb;
    lb.AddItem(_T("a"));
    lb.AddItem(_T("b"));
    lb.AddItem(_T("c"));
    EXPECT_INT(lb.GetIconSize(), kDefaultIconPx, _T("LBIcon/defaultSize"));
    EXPECT_INT((int)(lb.GetItemIcon(0) == NULL), 1, _T("LBIcon/defaultNone"));
    EXPECT_STR(lb.GetItemSubText(0), _T(""), _T("LBIcon/defaultSub"));

    lb.SetItemIcon(0, iconA);
    lb.SetItemSubText(0, _T("sa"));
    lb.SetItemIcon(2, iconC);
    lb.SetItemSubText(2, _T("sc"));
    // 改主文字不动图标与副文字
    lb.SetItemText(0, _T("a2"));
    EXPECT_INT((int)(lb.GetItemIcon(0) == iconA), 1, _T("LBIcon/keepOnSetText"));
    EXPECT_STR(lb.GetItemSubText(0), _T("sa"), _T("LBIcon/subKeepOnSetText"));

    // 在最前面插一行：原来的各行连同图标与副文字后移一位，新行没有图标
    lb.InsertItem(0, _T("z"));
    EXPECT_INT((int)(lb.GetItemIcon(0) == NULL), 1, _T("LBIcon/insertedNone"));
    EXPECT_INT((int)(lb.GetItemIcon(1) == iconA), 1, _T("LBIcon/insertShift"));
    EXPECT_STR(lb.GetItemSubText(3), _T("sc"), _T("LBIcon/insertShiftSub"));

    // 删掉插入的那行：回到原样
    lb.DeleteItem(0);
    EXPECT_INT((int)(lb.GetItemIcon(0) == iconA), 1, _T("LBIcon/deleteShift"));
    EXPECT_STR(lb.GetItemSubText(2), _T("sc"), _T("LBIcon/deleteShiftSub"));

    // 把第一行移到最后：a2 b c -> b c a2
    lb.MoveItem(0, 3);
    EXPECT_STR(lb.GetItemText(2), _T("a2"), _T("LBIcon/moveText"));
    EXPECT_INT((int)(lb.GetItemIcon(2) == iconA), 1, _T("LBIcon/moveIcon"));
    EXPECT_INT((int)(lb.GetItemIcon(1) == iconC), 1, _T("LBIcon/moveOther"));
    EXPECT_STR(lb.GetItemSubText(2), _T("sa"), _T("LBIcon/moveSub"));

    // 越界读写安全
    lb.SetItemIcon(kFarIndex, iconA);
    lb.SetItemSubText(-1, _T("x"));
    EXPECT_INT((int)(lb.GetItemIcon(kFarIndex) == NULL), 1, _T("LBIcon/oobIcon"));
    EXPECT_STR(lb.GetItemSubText(-1), _T(""), _T("LBIcon/oobSub"));

    lb.DeleteAllItems();
    EXPECT_INT(lb.GetItemCount(), 0, _T("LBIcon/clear"));
    ::DeleteObject(iconA);
    ::DeleteObject(iconC);
    return OK(_T("LB_IconSubTextFollowItems"));
}

// 有图标的行在文字左侧画出图标，文字整体在图标右边；没有图标的行同一位置没有图标色
static Result Test_LB_IconPainted()
{
    const int kW = 200;       // 列表宽，单位：像素
    const int kRowH = 24;     // 行高
    const int kIconPx = 16;   // 图标边长
    const int kBorder = 2;    // 上下边框共占的像素
    const COLORREF kIconColor = RGB(255, 0, 0);
    HBITMAP icon = MakeSolidIcon(kIconPx, kIconColor);
    DuiListBox lb;
    lb.SetItemHeight(kRowH);
    lb.SetIconSize(kIconPx);
    lb.AddItem(_T("Wwww"));
    lb.AddItem(_T("Wwww"));
    lb.SetItemIcon(0, icon);
    const int h = kRowH * 2 + kBorder;
    const RECT rc = { 0, 0, kW, h };
    lb.SetRect(rc);
    TestDib dib;
    if (!CreateTestDib(kW, h, dib))
    {
        ::DeleteObject(icon);
        return Fail(_T("LB_IconPainted"), _T("create dib failed"));
    }
    lb.OnPaint(dib.m_dc, rc);

    // 第一行：数出图标色的像素与它的最右位置，以及主文字笔画的最左位置
    int redCount = 0;
    int maxRedX = -1;
    int minTextX = kW;
    for (int y = 1; y < 1 + kRowH; ++y)
    {
        for (int x = 0; x < kW; ++x)
        {
            const COLORREF c = DibPixel(dib, x, y);
            // 图标是不透明的纯色，画出来与图标颜色严格相等；文字的 ClearType 彩色边缘不会恰好是这个颜色
            if (c == kIconColor)
            {
                ++redCount;
                if (x > maxRedX)
                {
                    maxRedX = x;
                }
            }
            else if (IsDarkNeutral(c) && x > 1 && x < kW - 2 && x < minTextX)
            {
                minTextX = x;
            }
        }
    }
    // 第二行没有图标：不应有图标色
    int redInRow2 = 0;
    for (int y = 1 + kRowH; y < 1 + kRowH * 2; ++y)
    {
        for (int x = 0; x < kW; ++x)
        {
            if (DibPixel(dib, x, y) == kIconColor)
            {
                ++redInRow2;
            }
        }
    }
    DestroyTestDib(dib);
    ::DeleteObject(icon);
    EXPECT_INT((int)(redCount >= kIconPx * kIconPx), 1, _T("LBIcon/painted"));
    EXPECT_INT(redInRow2, 0, _T("LBIcon/noIconRow"));
    EXPECT_INT((int)(minTextX > maxRedX), 1, _T("LBIcon/textRightOfIcon"));
    return OK(_T("LB_IconPainted"));
}

// 副文字画在主文字右侧、用副文字颜色
//
// 主文字用纯蓝、副文字用纯红、底色固定为白色（2026-10-09 起）。原先主文字是黑色：系统开着
// ClearType 时，黑字笔画边缘的彩色镶边里有满足「偏红」条件的像素，被当成了副文字，用例结果
// 随系统的字体平滑设置变化（实测最左的「红色」像素落在主文字中间）。白底上的纯蓝文字蓝通道
// 恒为 255，不可能被判成偏红；纯红副文字红通道恒为 255，也不可能被判成偏蓝。
static Result Test_LB_SubTextPainted()
{
    const int kW = 300;       // 列表宽，单位：像素
    const int kRowH = 24;     // 行高
    const int kBorder = 2;    // 上下边框共占的像素
    DuiListBox lb;
    lb.SetItemHeight(kRowH);
    lb.SetBgColor(RGB(255, 255, 255));
    lb.SetTextColor(RGB(0, 0, 255));
    lb.SetSubTextColor(RGB(255, 0, 0));
    lb.AddItem(_T("MMMM"));
    lb.SetItemSubText(0, _T("NNNN"));
    const int h = kRowH + kBorder;
    const RECT rc = { 0, 0, kW, h };
    lb.SetRect(rc);
    TestDib dib;
    if (!CreateTestDib(kW, h, dib))
    {
        return Fail(_T("LB_SubTextPainted"), _T("create dib failed"));
    }
    lb.OnPaint(dib.m_dc, rc);
    int minRedX = kW;
    int maxBlueX = -1;
    for (int y = 1; y < 1 + kRowH; ++y)
    {
        for (int x = 2; x < kW - 2; ++x)
        {
            const COLORREF c = DibPixel(dib, x, y);
            if (IsReddish(c) && x < minRedX)
            {
                minRedX = x;
            }
            else if (IsBluish(c) && x > maxBlueX)
            {
                maxBlueX = x;
            }
        }
    }
    DestroyTestDib(dib);
    EXPECT_INT((int)(maxBlueX >= 0), 1, _T("LBSub/mainPainted"));
    EXPECT_INT((int)(minRedX < kW), 1, _T("LBSub/subPainted"));
    EXPECT_INT((int)(minRedX > maxBlueX), 1, _T("LBSub/subRightOfMain"));
    return OK(_T("LB_SubTextPainted"));
}

// 只改图标边长、没有任何行设图标或副文字时，绘制结果与不改完全相同
static Result Test_LB_IconSizeAloneNoChange()
{
    const int kW = 200;       // 列表宽，单位：像素
    const int kRowH = 24;     // 行高
    const int kIconPx = 28;   // 改过的图标边长
    const int kBorder = 2;    // 上下边框共占的像素
    const int h = kRowH * 2 + kBorder;
    const RECT rc = { 0, 0, kW, h };
    DuiListBox plain;
    DuiListBox sized;
    DuiListBox* lists[] = { &plain, &sized };
    for (int i = 0; i < 2; ++i)
    {
        lists[i]->SetItemHeight(kRowH);
        lists[i]->AddItem(_T("alpha"));
        lists[i]->AddItem(_T("beta"));
        lists[i]->SetRect(rc);
    }
    sized.SetIconSize(kIconPx);
    sized.SetItemSubText(1, _T(""));
    TestDib a;
    TestDib b;
    if (!CreateTestDib(kW, h, a) || !CreateTestDib(kW, h, b))
    {
        return Fail(_T("LB_IconSizeAloneNoChange"), _T("create dib failed"));
    }
    plain.OnPaint(a.m_dc, rc);
    sized.OnPaint(b.m_dc, rc);
    int diff = 0;
    for (int i = 0; i < kW * h; ++i)
    {
        if (a.m_bits[i] != b.m_bits[i])
        {
            ++diff;
        }
    }
    DestroyTestDib(a);
    DestroyTestDib(b);
    EXPECT_INT(diff, 0, _T("LBIcon/sameWithoutIcons"));
    return OK(_T("LB_IconSizeAloneNoChange"));
}


// ----- LBO 组（2026-10-04 起）：列表的悬浮式滚动条 -----

// 用例统一的列表尺寸与行高（像素）：100 高放得下 5 行，30 行必然溢出
const int kLboW = 200;
const int kLboH = 100;
const int kLboRowH = 20;
const int kLboRows = 30;
// 动画时间轴的起点（毫秒，任取）
const DWORD kLboT0 = 200000;

// 建一个溢出的列表：30 行、排在 (0,0)-(kLboW,kLboH)
static void LboFill(DuiListBox& lb)
{
    lb.SetItemHeight(kLboRowH);
    for (int i = 0; i < kLboRows; ++i)
    {
        CString t;
        t.Format(_T("row %d"), i);
        lb.AddItem(t);
    }
    lb.SetRect(RECT{ 0, 0, kLboW, kLboH });
}

// 像素是否为白色（列表底色）
static bool LboWhite(COLORREF c)
{
    return c == RGB(255, 255, 255);
}

// LBO1：出现滚动条时行仍按全宽排版：选中行的高亮一直铺到列表右缘（滚动条此刻透明）。
static Result Test_LBO1_RowsKeepFullWidth()
{
    DuiListBox lb;
    LboFill(lb);
    if (!lb.GetScrollBar()->IsVisible())
    {
        return Fail(_T("LBO1"), _T("precondition: scroll bar not visible"));
    }
    lb.SetCurSel(0);
    TestDib d;
    if (!CreateTestDib(kLboW, kLboH, d))
    {
        return Fail(_T("LBO1"), _T("cannot create DIB"));
    }
    lb.OnPaint(d.m_dc, RECT{ 0, 0, kLboW, kLboH });
    const COLORREF nearRight = DibPixel(d, kLboW - 3, kLboRowH / 2);
    DestroyTestDib(d);
    if (nearRight != RGB(180, 210, 245))   // DuiListBox 默认选中底色 m_clrSelBg
    {
        CString s;
        s.Format(_T("pixel at right edge = 0x%06X, expected the selection color"), nearRight);
        return Fail(_T("LBO1/selectionFullWidth"), s);
    }
    return OK(_T("LBO1_RowsKeepFullWidth"));
}

// LBO2：开了删除列的行，滚动条出现时删除叉移到命中带左边：命中带里不画叉、点到的是滚动条；
// 叉所在处点到的是列表本身。
static Result Test_LBO2_DeleteCrossLeavesBand()
{
    DuiListBox lb;
    lb.SetShowItemDelete(true);
    LboFill(lb);
    DuiScrollBar* sb = lb.GetScrollBar();
    if (!sb->IsVisible())
    {
        return Fail(_T("LBO2"), _T("precondition: scroll bar not visible"));
    }
    const int bandLeft = kLboW - DuiScrollBar::kOverlayBandPx;
    TestDib d;
    if (!CreateTestDib(kLboW, kLboH, d))
    {
        return Fail(_T("LBO2"), _T("cannot create DIB"));
    }
    lb.OnPaint(d.m_dc, RECT{ 0, 0, kLboW, kLboH });
    // 第 0 行（未选中、未悬停）里数非白像素：命中带里应为 0，命中带左边一段里应有叉的笔画。
    // 避开列表最外面一圈 1 像素的外框（DuiListBox::OnPaint 用 Rectangle 画的边）
    const int kScanW = 30;      // 命中带左边扫描的宽度，覆盖删除列
    const int kBorderPx = 1;    // 列表外框的宽度
    int inBand = 0;
    int leftOfBand = 0;
    for (int y = kBorderPx; y < kLboRowH; ++y)
    {
        for (int x = bandLeft; x < kLboW - kBorderPx; ++x)
        {
            if (!LboWhite(DibPixel(d, x, y)))
            {
                ++inBand;
            }
        }
        for (int x = bandLeft - kScanW; x < bandLeft; ++x)
        {
            if (!LboWhite(DibPixel(d, x, y)))
            {
                ++leftOfBand;
            }
        }
    }
    DestroyTestDib(d);
    CString s;
    s.Format(_T("non-white pixels: in band=%d, left of band=%d"), inBand, leftOfBand);
    if (inBand != 0 || leftOfBand == 0)
    {
        return Fail(_T("LBO2/crossMoved"), s);
    }
    POINT inBandPt = { kLboW - 3, kLboRowH / 2 };
    POINT crossPt = { bandLeft - 8, kLboRowH / 2 };
    if (lb.HitTest(inBandPt) != sb)
    {
        return Fail(_T("LBO2/bandHitsBar"), _T("band point did not hit the scroll bar"));
    }
    if (lb.HitTest(crossPt) != &lb)
    {
        return Fail(_T("LBO2/crossHitsList"), _T("cross point did not hit the list"));
    }
    return OK(_T("LBO2_DeleteCrossLeavesBand"));
}

// LBO3：鼠标在行上移动不唤出滚动条，滚轮会。
static Result Test_LBO3_HoverRowsDoesNotShowBar()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();
    DuiListBox lb;
    LboFill(lb);
    DuiScrollBar* sb = lb.GetScrollBar();
    lb.OnMouseMove(POINT{ kLboW / 2, kLboRowH / 2 }, 0);
    m.TickAll(kLboT0);
    m.TickAll(kLboT0 + DuiScrollBar::kFadeInMs);
    if (sb->GetAlpha() != 0.0f)
    {
        m.Clear();
        return Fail(_T("LBO3/hoverRows"), _T("scroll bar became visible on row hover"));
    }
    lb.OnMouseWheel(POINT{ kLboW / 2, kLboRowH / 2 }, -WHEEL_DELTA, 0);
    m.TickAll(kLboT0 + 1000);
    m.TickAll(kLboT0 + 1000 + DuiScrollBar::kFadeInMs);
    const float a = sb->GetAlpha();
    m.Clear();
    if (a < 0.99f)
    {
        return Fail(_T("LBO3/wheelShows"), _T("scroll bar not visible after wheel"));
    }
    return OK(_T("LBO3_HoverRowsDoesNotShowBar"));
}

#undef EXPECT_INT
#undef EXPECT_STR

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
        { _T("LB_WheelBubblesWhenItemsFit"), &Test_LB_WheelBubblesWhenItemsFit },
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
        { _T("LB_MoveItemTracksCurSel"),    &Test_LB_MoveItemTracksCurSel    },
        { _T("VL_SbPlacedWhenRowsArriveAfterLayout"),     &Test_VL_SbPlacedWhenRowsArriveAfterLayout     },
        { _T("VL_SbPlacedWhenRowHeightGrowsAfterLayout"), &Test_VL_SbPlacedWhenRowHeightGrowsAfterLayout },
        { _T("VL_SbPlacedWhenRowsSetBeforeLayout"),       &Test_VL_SbPlacedWhenRowsSetBeforeLayout       },
        { _T("LB_SbPlacedWhenItemsAddedAfterLayout"),     &Test_LB_SbPlacedWhenItemsAddedAfterLayout     },
        { _T("LB_SbPlacedWhenItemHeightGrowsAfterLayout"), &Test_LB_SbPlacedWhenItemHeightGrowsAfterLayout },
        { _T("LB_IconSubTextFollowItems"),  &Test_LB_IconSubTextFollowItems  },
        { _T("LB_IconPainted"),             &Test_LB_IconPainted             },
        { _T("LB_SubTextPainted"),          &Test_LB_SubTextPainted          },
        { _T("LB_IconSizeAloneNoChange"),   &Test_LB_IconSizeAloneNoChange   },
        { _T("LBO1_RowsKeepFullWidth"),     &Test_LBO1_RowsKeepFullWidth     },
        { _T("LBO2_DeleteCrossLeavesBand"), &Test_LBO2_DeleteCrossLeavesBand },
        { _T("LBO3_HoverRowsDoesNotShowBar"), &Test_LBO3_HoverRowsDoesNotShowBar }
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
