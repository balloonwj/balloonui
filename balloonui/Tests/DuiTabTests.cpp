#include "stdafx.h"
#include "DuiTabTests.h"

#if BUI_FEATURE_TAB


namespace balloonwjui {

namespace DuiTabTests {

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
#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("got '%s'"), (LPCTSTR)_a); return Fail(name, _d); } \
    } while (0)

static Result Test_Defaults()
{
    DuiTab t;
    EXPECT_INT(t.GetTabCount(), 0,  _T("Defaults/empty"));
    EXPECT_INT(t.GetCurSel(),  -1,  _T("Defaults/noSel"));
    return OK(_T("Defaults"));
}

// First AddTab auto-selects index 0.
static Result Test_FirstAddSelects()
{
    DuiTab t;
    EXPECT_INT(t.AddTab(_T("a")), 0,  _T("First/idx"));
    EXPECT_INT(t.GetCurSel(),     0,  _T("First/sel"));
    t.AddTab(_T("b"));
    EXPECT_INT(t.GetCurSel(), 0, _T("First/secondAddPreservesSel"));
    return OK(_T("FirstAddSelects"));
}

// Insert before selection shifts sel down by 1.
static Result Test_InsertShiftsSel()
{
    DuiTab t;
    t.AddTab(_T("a"));
    t.AddTab(_T("b"));
    t.AddTab(_T("c"));
    t.SetCurSel(2);
    t.InsertTab(0, _T("x"));
    EXPECT_INT(t.GetCurSel(), 3, _T("Insert/shift"));
    EXPECT_STR(t.GetTabText(0), _T("x"), _T("Insert/at0"));
    EXPECT_STR(t.GetTabText(3), _T("c"), _T("Insert/cMoved"));
    return OK(_T("InsertShiftsSel"));
}

// Remove selected: select neighbor.
static Result Test_RemoveSelected()
{
    DuiTab t;
    t.AddTab(_T("a"));
    t.AddTab(_T("b"));
    t.AddTab(_T("c"));
    t.SetCurSel(1);
    t.RemoveTab(1);   // remove "b" - sel was 1, should now be 1 ("c")
    EXPECT_INT(t.GetCurSel(), 1, _T("Remove/keepIdx"));
    EXPECT_STR(t.GetTabText(1), _T("c"), _T("Remove/cAt1"));
    t.RemoveTab(1);   // remove last; sel should fall back to 0
    EXPECT_INT(t.GetCurSel(), 0, _T("Remove/fallback"));
    t.RemoveTab(0);
    EXPECT_INT(t.GetCurSel(), -1, _T("Remove/empty"));
    return OK(_T("RemoveSelected"));
}

// FindByParam locates by user data.
static Result Test_FindByParam()
{
    DuiTab t;
    t.AddTab(_T("a"), false, false, 100);
    t.AddTab(_T("b"), false, false, 200);
    t.AddTab(_T("c"), false, false, 300);
    EXPECT_INT(t.FindByParam(200), 1,  _T("Find/200"));
    EXPECT_INT(t.FindByParam(999), -1, _T("Find/missing"));
    return OK(_T("FindByParam"));
}

// HitTest a coordinate inside one of the tabs.
static Result Test_HitTab()
{
    DuiTab t;
    t.AddTab(_T("alpha"));
    t.AddTab(_T("beta"));
    t.AddTab(_T("gamma"));
    t.SetRect(RECT{ 0, 0, 600, 50 });    // tab strip at bottom (h=28)
    // Click within tab #1's rect.
    RECT r1 = t.Test_TabRect(1);
    POINT p = { (r1.left + r1.right) / 2, (r1.top + r1.bottom) / 2 };
    EXPECT_INT(t.Test_HitTab(p), 1, _T("Hit/middle"));
    POINT p_outside = { 5, 5 };       // above tab strip
    EXPECT_INT(t.Test_HitTab(p_outside), -1, _T("Hit/aboveStrip"));
    return OK(_T("HitTab"));
}

// Closeable: close glyph rect inside the tab; non-closeable -> empty.
static Result Test_CloseRectGeometry()
{
    DuiTab t;
    t.AddTab(_T("plain"));
    t.AddTab(_T("closeable"), /*closeable=*/true);
    t.SetRect(RECT{ 0, 0, 600, 50 });

    RECT r0 = t.Test_CloseRect(0);
    if (r0.left != 0 || r0.right != 0 || r0.top != 0 || r0.bottom != 0)
    {
        return Fail(_T("Close/plainEmpty"), _T("non-empty rect"));
    }

    RECT r1 = t.Test_CloseRect(1);
    if (r1.right <= r1.left || r1.bottom <= r1.top)
    {
        return Fail(_T("Close/closeableSized"), _T("zero rect"));
    }

    // Close glyph should sit inside the tab.
    RECT t1 = t.Test_TabRect(1);
    if (r1.left < t1.left || r1.right > t1.right)
    {
        return Fail(_T("Close/insideTab"), _T("outside tab horizontally"));
    }
    return OK(_T("CloseRectGeometry"));
}

// Press close zone, release on close zone -> selection unchanged (parent
// decides). Press tab body, release on body -> selection moves.
static Result Test_ClickZones()
{
    DuiTab t;
    t.AddTab(_T("a"), /*closeable=*/true);
    t.AddTab(_T("b"), /*closeable=*/true);
    t.AddTab(_T("c"), /*closeable=*/true);
    t.SetRect(RECT{ 0, 0, 600, 60 });
    t.SetCurSel(0);

    // Click the body of tab 2.
    RECT t2 = t.Test_TabRect(2);
    POINT bodyPt = { t2.left + 8, (t2.top + t2.bottom) / 2 };
    t.OnLButtonDown(bodyPt, 0);
    t.OnLButtonUp  (bodyPt, 0);
    EXPECT_INT(t.GetCurSel(), 2, _T("Click/bodySelects"));

    // Click the close zone of tab 1; selection must NOT move to 1.
    RECT close1 = t.Test_CloseRect(1);
    POINT closePt = { (close1.left + close1.right) / 2, (close1.top + close1.bottom) / 2 };
    t.OnLButtonDown(closePt, 0);
    t.OnLButtonUp  (closePt, 0);
    EXPECT_INT(t.GetCurSel(), 2, _T("Click/closeNoSelect"));
    return OK(_T("ClickZones"));
}

// Press body of tab 2, release on tab 0 -> no selection change (drag-out cancels).
static Result Test_DragOutCancels()
{
    DuiTab t;
    t.AddTab(_T("a"));
    t.AddTab(_T("b"));
    t.AddTab(_T("c"));
    t.SetRect(RECT{ 0, 0, 600, 60 });
    t.SetCurSel(0);

    RECT t2 = t.Test_TabRect(2);
    RECT t0 = t.Test_TabRect(0);
    POINT downPt = { t2.left + 8, (t2.top + t2.bottom) / 2 };
    POINT upPt   = { t0.left + 8, (t0.top + t0.bottom) / 2 };
    t.OnLButtonDown(downPt, 0);
    t.OnLButtonUp  (upPt,   0);
    EXPECT_INT(t.GetCurSel(), 0, _T("DragOut/noSelectChange"));
    return OK(_T("DragOutCancels"));
}

// Tab text round-trip + setter.
static Result Test_TextRoundTrip()
{
    DuiTab t;
    t.AddTab(_T("hello"));
    EXPECT_STR(t.GetTabText(0), _T("hello"), _T("Text/init"));
    t.SetTabText(0, _T("world"));
    EXPECT_STR(t.GetTabText(0), _T("world"), _T("Text/set"));
    return OK(_T("TextRoundTrip"));
}

// SetCurSel idempotent: same index doesn't fire spurious work.
static Result Test_SetCurSelIdempotent()
{
    DuiTab t;
    t.AddTab(_T("a"));
    t.AddTab(_T("b"));
    t.SetCurSel(1, /*notify=*/false);
    EXPECT_INT(t.GetCurSel(), 1, _T("Idem/setOnce"));
    t.SetCurSel(1, /*notify=*/false);
    EXPECT_INT(t.GetCurSel(), 1, _T("Idem/setAgain"));
    return OK(_T("SetCurSelIdempotent"));
}

// ----- drag-reorder ---------------------------------------------------

static Result Test_ReorderDefaultOff()
{
    DuiTab t;
    EXPECT_BOOL(t.GetReorderEnabled(), false, _T("Reord/default"));
    t.SetReorderEnabled(true);
    EXPECT_BOOL(t.GetReorderEnabled(), true, _T("Reord/on"));
    return OK(_T("ReorderDefaultOff"));
}

// MoveTab moves "a" forward past b/c and "d" backward.
static Result Test_MoveTabBasic()
{
    DuiTab t;
    t.AddTab(_T("a"));
    t.AddTab(_T("b"));
    t.AddTab(_T("c"));
    t.AddTab(_T("d"));
    // Move "a" (idx 0) to slot 3 -> after-removal idx 2 -> order: b c a d
    t.MoveTab(0, 3);
    if (t.GetTabText(0) != _T("b"))
    {
        return Fail(_T("MvT/0"), _T("0 != b"));
    }
    if (t.GetTabText(1) != _T("c"))
    {
        return Fail(_T("MvT/1"), _T("1 != c"));
    }
    if (t.GetTabText(2) != _T("a"))
    {
        return Fail(_T("MvT/2"), _T("2 != a"));
    }
    if (t.GetTabText(3) != _T("d"))
    {
        return Fail(_T("MvT/3"), _T("3 != d"));
    }

    // Move "d" (idx 3) to slot 0 -> order: d b c a
    t.MoveTab(3, 0);
    if (t.GetTabText(0) != _T("d"))
    {
        return Fail(_T("MvT/0b"), _T("0 != d"));
    }
    if (t.GetTabText(1) != _T("b"))
    {
        return Fail(_T("MvT/1b"), _T("1 != b"));
    }
    if (t.GetTabText(2) != _T("c"))
    {
        return Fail(_T("MvT/2b"), _T("2 != c"));
    }
    if (t.GetTabText(3) != _T("a"))
    {
        return Fail(_T("MvT/3b"), _T("3 != a"));
    }
    return OK(_T("MoveTabBasic"));
}

// MoveTab(idx, idx) and MoveTab(idx, idx+1) are no-ops.
static Result Test_MoveTabIdempotent()
{
    DuiTab t;
    t.AddTab(_T("a"));
    t.AddTab(_T("b"));
    t.AddTab(_T("c"));
    t.MoveTab(1, 1);
    if (t.GetTabText(1) != _T("b"))
    {
        return Fail(_T("MvI/same"), _T("changed"));
    }
    t.MoveTab(1, 2);     // "to=idx+1 = original slot" -> no-op
    if (t.GetTabText(1) != _T("b"))
    {
        return Fail(_T("MvI/n+1"), _T("changed"));
    }
    return OK(_T("MoveTabIdempotent"));
}

// Selection follows the moved tab.
static Result Test_MoveTabTracksCurSel()
{
    DuiTab t;
    t.AddTab(_T("a"));
    t.AddTab(_T("b"));
    t.AddTab(_T("c"));
    t.SetCurSel(0, /*notify=*/false);
    t.MoveTab(0, 3);     // a -> end
    EXPECT_INT(t.GetCurSel(), 2, _T("MvCs/track"));
    return OK(_T("MoveTabTracksCurSel"));
}

#undef EXPECT_INT
#undef EXPECT_BOOL
#undef EXPECT_STR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),             &Test_Defaults             },
        { _T("FirstAddSelects"),      &Test_FirstAddSelects      },
        { _T("InsertShiftsSel"),      &Test_InsertShiftsSel      },
        { _T("RemoveSelected"),       &Test_RemoveSelected       },
        { _T("FindByParam"),          &Test_FindByParam          },
        { _T("HitTab"),               &Test_HitTab               },
        { _T("CloseRectGeometry"),    &Test_CloseRectGeometry    },
        { _T("ReorderDefaultOff"),    &Test_ReorderDefaultOff    },
        { _T("MoveTabBasic"),         &Test_MoveTabBasic         },
        { _T("MoveTabIdempotent"),    &Test_MoveTabIdempotent    },
        { _T("MoveTabTracksCurSel"),  &Test_MoveTabTracksCurSel  },
        { _T("ClickZones"),           &Test_ClickZones           },
        { _T("DragOutCancels"),       &Test_DragOutCancels       },
        { _T("TextRoundTrip"),        &Test_TextRoundTrip        },
        { _T("SetCurSelIdempotent"),  &Test_SetCurSelIdempotent  }
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
    summary.Format(_T("[summary] DuiTabTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiTabTests

} // namespace balloonwjui

#endif // BUI_FEATURE_TAB
