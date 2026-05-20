#include "stdafx.h"
#include "DuiEmojiPanelTests.h"

#if BUI_FEATURE_EMOJIPANEL


namespace balloonwjui {

namespace DuiEmojiPanelTests {

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
         if (_a != _e) return Fail(name, _T("string mismatch: \"") + _a + _T("\" vs \"") + _e + _T("\"")); \
    } while (0)

// ----- Defaults --------------------------------------------------------

static Result Test_Defaults()
{
    DuiEmojiPanel p;
    EXPECT_INT(p.GetCount(),    0,  _T("Def/count"));
    EXPECT_INT(p.GetCellSize(), 32, _T("Def/cell"));
    EXPECT_INT(p.GetColumns(),  8,  _T("Def/cols"));
    return OK(_T("Defaults"));
}

// ----- AddEmoji + GetEmojiAt round-trip -------------------------------

static Result Test_AddAndGet()
{
    DuiEmojiPanel p;
    p.AddEmoji(_T("\xD83D\xDE00"), _T("grinning"));   // 😀
    p.AddEmoji(_T("\xD83D\xDE02"));                   // 😂
    p.AddEmoji(_T(""));                               // empty rejected
    p.AddEmoji(nullptr);                              // null rejected
    EXPECT_INT(p.GetCount(), 2, _T("AG/count"));
    EXPECT_STR(p.GetEmojiAt(0),    _T("\xD83D\xDE00"), _T("AG/seq0"));
    EXPECT_STR(p.GetTooltipAt(0),  _T("grinning"),     _T("AG/tip0"));
    EXPECT_STR(p.GetEmojiAt(1),    _T("\xD83D\xDE02"), _T("AG/seq1"));
    EXPECT_TRUE(p.GetEmojiAt(99).IsEmpty(),  _T("AG/oobEmpty"));
    EXPECT_TRUE(p.GetEmojiAt(-1).IsEmpty(),  _T("AG/negEmpty"));
    return OK(_T("AddAndGet"));
}

static Result Test_AddSet()
{
    DuiEmojiPanel p;
    LPCTSTR seqs[] = { _T("A"), _T("B"), _T("C") };
    p.AddEmojiSet(seqs, 3);
    EXPECT_INT(p.GetCount(), 3, _T("Set/count"));
    EXPECT_STR(p.GetEmojiAt(2), _T("C"), _T("Set/last"));
    p.AddEmojiSet(nullptr, 5);   // null array is a safe no-op
    EXPECT_INT(p.GetCount(), 3, _T("Set/nullSafe"));
    return OK(_T("AddSet"));
}

static Result Test_Clear()
{
    DuiEmojiPanel p;
    p.AddEmoji(_T("X"));
    p.AddEmoji(_T("Y"));
    EXPECT_INT(p.GetCount(), 2, _T("Clr/before"));
    p.Clear();
    EXPECT_INT(p.GetCount(), 0, _T("Clr/after"));
    EXPECT_INT(p.Test_HoverIdx(), -1, _T("Clr/hover"));
    return OK(_T("Clear"));
}

// ----- size / column clamps -------------------------------------------

static Result Test_ClampMetrics()
{
    DuiEmojiPanel p;
    p.SetCellSize(8);                    // clamps to 16
    EXPECT_INT(p.GetCellSize(), 16, _T("Clamp/cell"));
    p.SetCellSize(40);
    EXPECT_INT(p.GetCellSize(), 40, _T("Clamp/cellOK"));
    p.SetColumns(0);                     // clamps to 1
    EXPECT_INT(p.GetColumns(), 1, _T("Clamp/cols"));
    p.SetColumns(12);
    EXPECT_INT(p.GetColumns(), 12, _T("Clamp/colsOK"));
    return OK(_T("ClampMetrics"));
}

// ----- GetRowCount + GetDesiredSize -----------------------------------

static Result Test_DesiredSize()
{
    DuiEmojiPanel p;
    p.SetColumns(4);
    p.SetCellSize(20);
    LPCTSTR seqs[10] = {
        _T("a"),_T("b"),_T("c"),_T("d"),_T("e"),
        _T("f"),_T("g"),_T("h"),_T("i"),_T("j")
    };
    p.AddEmojiSet(seqs, 10);
    // 10 / 4 = 3 rows (round up).
    EXPECT_INT(p.GetRowCount(), 3, _T("DS/rows"));
    SIZE s = p.GetDesiredSize();
    EXPECT_INT(s.cx, 80, _T("DS/cx"));    // 4 * 20
    EXPECT_INT(s.cy, 60, _T("DS/cy"));    // 3 * 20
    return OK(_T("DesiredSize"));
}

// ----- HitTestIndex grid math -----------------------------------------

static Result Test_HitTestGrid()
{
    DuiEmojiPanel p;
    p.SetColumns(4);
    p.SetCellSize(20);
    p.SetRect(RECT{ 100, 50, 100 + 80, 50 + 60 });
    LPCTSTR seqs[10] = { _T("0"),_T("1"),_T("2"),_T("3"),_T("4"),_T("5"),_T("6"),_T("7"),_T("8"),_T("9") };
    p.AddEmojiSet(seqs, 10);

    // Row 0 col 0 -> idx 0
    EXPECT_INT(p.HitTestIndex(POINT{ 105, 55 }), 0, _T("HT/r0c0"));
    // Row 0 col 3 -> idx 3
    EXPECT_INT(p.HitTestIndex(POINT{ 175, 55 }), 3, _T("HT/r0c3"));
    // Row 1 col 1 -> idx 5
    EXPECT_INT(p.HitTestIndex(POINT{ 125, 75 }), 5, _T("HT/r1c1"));
    // Row 2 col 1 -> idx 9 (still in count)
    EXPECT_INT(p.HitTestIndex(POINT{ 125, 95 }), 9, _T("HT/r2c1"));
    // Row 2 col 3 -> idx 11 -> out of range -> -1
    EXPECT_INT(p.HitTestIndex(POINT{ 175, 95 }), -1, _T("HT/oobLastRow"));
    // Outside rect entirely
    EXPECT_INT(p.HitTestIndex(POINT{ 50, 50 }),  -1, _T("HT/leftOutside"));
    EXPECT_INT(p.HitTestIndex(POINT{ 200, 50 }), -1, _T("HT/rightOutside"));
    return OK(_T("HitTestGrid"));
}

// ----- Click flow + pick callback -------------------------------------

static Result Test_ClickFiresCallback()
{
    DuiEmojiPanel p;
    p.SetColumns(2);
    p.SetCellSize(20);
    p.SetRect(RECT{ 0, 0, 40, 40 });
    LPCTSTR seqs[3] = { _T("X"), _T("Y"), _T("Z") };
    p.AddEmojiSet(seqs, 3);

    struct Hit { CString seq; int idx; int n; };
    Hit hit = { CString(), -1, 0 };
    p.SetPickCallback(
        [](void* ud, LPCTSTR seq, int idx)
        {
            Hit* h = (Hit*)ud;
            h->seq = seq;
            h->idx = idx;
            ++h->n;
        }, &hit);

    // Click cell (1,0) = idx 1 = "Y".
    p.OnLButtonDown(POINT{ 25, 5 }, 0);
    p.OnLButtonUp  (POINT{ 25, 5 }, 0);
    EXPECT_INT(hit.n, 1, _T("Click/fires"));
    EXPECT_INT(hit.idx, 1, _T("Click/idx"));
    EXPECT_STR(hit.seq, _T("Y"), _T("Click/seq"));

    // Drag-out cancels click — no callback fired.
    p.OnLButtonDown(POINT{ 25, 5 }, 0);
    p.OnLButtonUp  (POINT{ 999, 999 }, 0);
    EXPECT_INT(hit.n, 1, _T("Click/dragOut"));
    return OK(_T("ClickFiresCallback"));
}

// ----- bitmap-bearing entries -----------------------------------------

static Result Test_BitmapEntries()
{
    DuiEmojiPanel p;
    HBITMAP fake = (HBITMAP)0xDEADu;
    p.AddEmojiBitmap(_T("[smile]"), fake, _T("smile"));
    EXPECT_INT(p.GetCount(), 1, _T("Bm/count"));
    EXPECT_TRUE(p.GetIconAt(0) == fake, _T("Bm/icon"));
    EXPECT_STR(p.GetEmojiAt(0), _T("[smile]"), _T("Bm/seq"));
    EXPECT_STR(p.GetTooltipAt(0), _T("smile"), _T("Bm/tip"));

    // Bitmap-only entry (empty seq + valid icon) is still accepted.
    p.AddEmojiBitmap(nullptr, fake, nullptr);
    EXPECT_INT(p.GetCount(), 2, _T("Bm/iconOnly"));
    EXPECT_TRUE(p.GetEmojiAt(1).IsEmpty(), _T("Bm/iconOnlyEmptySeq"));

    // No seq + no icon -> rejected.
    p.AddEmojiBitmap(nullptr, nullptr, nullptr);
    EXPECT_INT(p.GetCount(), 2, _T("Bm/bothNullRejected"));

    // Bogus index queries are safe.
    EXPECT_TRUE(p.GetIconAt(99)  == nullptr, _T("Bm/oobIcon"));
    EXPECT_TRUE(p.GetIconAt(-1)  == nullptr, _T("Bm/negIcon"));
    return OK(_T("BitmapEntries"));
}

// ----- hover index updates --------------------------------------------

static Result Test_HoverTracking()
{
    DuiEmojiPanel p;
    p.SetColumns(2);
    p.SetCellSize(20);
    p.SetRect(RECT{ 0, 0, 40, 40 });
    LPCTSTR seqs[3] = { _T("A"), _T("B"), _T("C") };
    p.AddEmojiSet(seqs, 3);
    EXPECT_INT(p.Test_HoverIdx(), -1, _T("Hov/init"));
    p.OnMouseMove(POINT{ 5, 5 }, 0);
    EXPECT_INT(p.Test_HoverIdx(), 0, _T("Hov/r0c0"));
    p.OnMouseMove(POINT{ 5, 25 }, 0);          // row 1 col 0 = idx 2
    EXPECT_INT(p.Test_HoverIdx(), 2, _T("Hov/r1c0"));
    p.OnMouseLeave();
    EXPECT_INT(p.Test_HoverIdx(), -1, _T("Hov/leave"));
    return OK(_T("HoverTracking"));
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
        { _T("Defaults"),            &Test_Defaults            },
        { _T("AddAndGet"),           &Test_AddAndGet           },
        { _T("AddSet"),              &Test_AddSet              },
        { _T("Clear"),               &Test_Clear               },
        { _T("ClampMetrics"),        &Test_ClampMetrics        },
        { _T("DesiredSize"),         &Test_DesiredSize         },
        { _T("HitTestGrid"),         &Test_HitTestGrid         },
        { _T("ClickFiresCallback"),  &Test_ClickFiresCallback  },
        { _T("BitmapEntries"),       &Test_BitmapEntries       },
        { _T("HoverTracking"),       &Test_HoverTracking       },
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
    summary.Format(_T("[summary] DuiEmojiPanelTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiEmojiPanelTests

} // namespace balloonwjui

#endif // BUI_FEATURE_EMOJIPANEL
