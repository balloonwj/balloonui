#include "stdafx.h"
#include "DuiTabPageTests.h"

#if BUI_FEATURE_TABPAGE


namespace balloonwjui {

namespace DuiTabPageTests {

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

class StubPage : public DuiControl
{
public:
    void OnPaint(HDC, const RECT&) override {}
};

// ----- Defaults --------------------------------------------------------

static Result Test_Defaults()
{
    DuiTabPage tp;
    EXPECT_INT(tp.GetPageCount(),    0,   _T("Def/count"));
    EXPECT_INT(tp.GetCurSel(),      -1,   _T("Def/sel"));
    EXPECT_INT(tp.GetHeaderHeight(), 32,  _T("Def/hdrH"));
    EXPECT_TRUE(tp.GetHeader() != nullptr, _T("Def/hdrPresent"));
    return OK(_T("Defaults"));
}

// AddPage returns sequential indices.
static Result Test_AddPageIndex()
{
    DuiTabPage tp;
    int i0 = tp.AddPage(_T("A"), std::unique_ptr<DuiControl>(new StubPage()));
    int i1 = tp.AddPage(_T("B"), std::unique_ptr<DuiControl>(new StubPage()));
    int i2 = tp.AddPage(_T("C"), std::unique_ptr<DuiControl>(new StubPage()));
    EXPECT_INT(i0, 0, _T("Add/i0"));
    EXPECT_INT(i1, 1, _T("Add/i1"));
    EXPECT_INT(i2, 2, _T("Add/i2"));
    EXPECT_INT(tp.GetPageCount(), 3, _T("Add/count"));
    EXPECT_INT(tp.GetCurSel(), 0,    _T("Add/firstSelected"));
    EXPECT_STR(tp.GetPageTitle(1), _T("B"), _T("Add/title"));
    return OK(_T("AddPageIndex"));
}

// SetCurSel toggles visibility: only selected page is visible.
static Result Test_SetCurSelVisibility()
{
    DuiTabPage tp;
    StubPage *a, *b, *c;
    tp.AddPage(_T("A"), std::unique_ptr<DuiControl>(a = new StubPage()));
    tp.AddPage(_T("B"), std::unique_ptr<DuiControl>(b = new StubPage()));
    tp.AddPage(_T("C"), std::unique_ptr<DuiControl>(c = new StubPage()));

    EXPECT_TRUE(a->IsVisible() && !b->IsVisible() && !c->IsVisible(), _T("Vis/init"));
    tp.SetCurSel(2, /*notify=*/false);
    EXPECT_INT(tp.GetCurSel(), 2, _T("Vis/sel2"));
    EXPECT_TRUE(!a->IsVisible() && !b->IsVisible() && c->IsVisible(), _T("Vis/onlyC"));
    tp.SetCurSel(1, /*notify=*/false);
    EXPECT_TRUE(!a->IsVisible() && b->IsVisible() && !c->IsVisible(), _T("Vis/onlyB"));
    return OK(_T("SetCurSelVisibility"));
}

// SetCurSel out of range: ignored, sel stays put.
static Result Test_SetCurSelOutOfRange()
{
    DuiTabPage tp;
    tp.AddPage(_T("A"), std::unique_ptr<DuiControl>(new StubPage()));
    tp.AddPage(_T("B"), std::unique_ptr<DuiControl>(new StubPage()));
    tp.SetCurSel(99, false);
    EXPECT_INT(tp.GetCurSel(), 0, _T("OOR/positive"));
    tp.SetCurSel(-5, false);
    EXPECT_INT(tp.GetCurSel(), 0, _T("OOR/negative"));
    return OK(_T("SetCurSelOutOfRange"));
}

// RemovePage shifts: removing index 0 leaves pages 1->0, 2->1; selection
// clamps if needed.
static Result Test_RemovePageShifts()
{
    DuiTabPage tp;
    StubPage *a, *b, *c;
    tp.AddPage(_T("A"), std::unique_ptr<DuiControl>(a = new StubPage()));
    tp.AddPage(_T("B"), std::unique_ptr<DuiControl>(b = new StubPage()));
    tp.AddPage(_T("C"), std::unique_ptr<DuiControl>(c = new StubPage()));
    tp.SetCurSel(2, false);   // select C

    tp.RemovePage(0);         // remove A; b,c shift down. sel was 2 -> clamp to 1.
    EXPECT_INT(tp.GetPageCount(), 2, _T("Rm/count"));
    EXPECT_INT(tp.GetCurSel(),    1, _T("Rm/selClamped"));
    EXPECT_TRUE(tp.GetPage(0) == b, _T("Rm/p0=B"));
    EXPECT_TRUE(tp.GetPage(1) == c, _T("Rm/p1=C"));

    tp.RemovePage(1);         // remove C (now at idx 1); sel clamps to 0.
    EXPECT_INT(tp.GetPageCount(), 1, _T("Rm/count2"));
    EXPECT_INT(tp.GetCurSel(),    0, _T("Rm/sel2"));

    tp.RemovePage(0);         // remove last
    EXPECT_INT(tp.GetPageCount(), 0, _T("Rm/empty"));
    EXPECT_INT(tp.GetCurSel(),   -1, _T("Rm/selEmpty"));
    return OK(_T("RemovePageShifts"));
}

// SetPage replaces the content, preserves the tab title.
static Result Test_SetPageReplaces()
{
    DuiTabPage tp;
    tp.AddPage(_T("First"), std::unique_ptr<DuiControl>(new StubPage()));
    tp.AddPage(_T("Second"), std::unique_ptr<DuiControl>(new StubPage()));

    StubPage* fresh;
    tp.SetPage(1, std::unique_ptr<DuiControl>(fresh = new StubPage()));
    EXPECT_TRUE(tp.GetPage(1) == fresh, _T("SP/replaced"));
    EXPECT_STR(tp.GetPageTitle(1), _T("Second"), _T("SP/titleKept"));

    // Replacing with nullptr clears the slot.
    tp.SetPage(1, nullptr);
    EXPECT_TRUE(tp.GetPage(1) == nullptr, _T("SP/cleared"));
    EXPECT_STR(tp.GetPageTitle(1), _T("Second"), _T("SP/titleStillKept"));
    return OK(_T("SetPageReplaces"));
}

// AddPage with nullptr accepted: tab appears, slot is empty.
static Result Test_AddPageNull()
{
    DuiTabPage tp;
    int i = tp.AddPage(_T("Empty"), nullptr);
    EXPECT_INT(i, 0, _T("AN/idx"));
    EXPECT_INT(tp.GetPageCount(), 1, _T("AN/count"));
    EXPECT_TRUE(tp.GetPage(0) == nullptr, _T("AN/slot"));
    EXPECT_STR(tp.GetPageTitle(0), _T("Empty"), _T("AN/title"));
    return OK(_T("AddPageNull"));
}

// SetHeaderHeight clamp.
static Result Test_SetHeaderHeightClamp()
{
    DuiTabPage tp;
    tp.SetHeaderHeight(40);
    EXPECT_INT(tp.GetHeaderHeight(), 40, _T("HH/set40"));
    tp.SetHeaderHeight(5);          // clamped to 12
    EXPECT_INT(tp.GetHeaderHeight(), 12, _T("HH/clamp"));
    return OK(_T("SetHeaderHeightClamp"));
}

// Layout splits area: header at top, content fills below.
static Result Test_LayoutSplitsArea()
{
    DuiTabPage tp;
    StubPage* p;
    tp.AddPage(_T("A"), std::unique_ptr<DuiControl>(p = new StubPage()));
    tp.SetHeaderHeight(30);
    tp.Layout(RECT{ 0, 0, 200, 150 });

    const RECT& hr = tp.GetHeader()->GetRect();
    EXPECT_INT(hr.top,    0,  _T("Lay/hdrTop"));
    EXPECT_INT(hr.bottom, 30, _T("Lay/hdrBot"));
    EXPECT_INT(hr.left,   0,  _T("Lay/hdrL"));
    EXPECT_INT(hr.right,  200,_T("Lay/hdrR"));

    const RECT& pr = p->GetRect();
    EXPECT_INT(pr.top,    30,  _T("Lay/pgTop"));
    EXPECT_INT(pr.bottom, 150, _T("Lay/pgBot"));
    EXPECT_INT(pr.left,   0,   _T("Lay/pgL"));
    EXPECT_INT(pr.right,  200, _T("Lay/pgR"));
    return OK(_T("LayoutSplitsArea"));
}

// ----- icon / auto-fit forwarding -----------------------------------------

// SetPageIcon 透传到内部 DuiTab; GetPageIcon 与 GetHeader()->GetTabIcon
// 一致(说明 forwarding 没绕路、改的就是同一份状态)。
static Result Test_Page_IconRoundTrip()
{
    DuiTabPage tp;
    tp.AddPage(_T("A"), std::unique_ptr<DuiControl>(new StubPage()));
    HBITMAP fake = (HBITMAP)(LONG_PTR)1;
    tp.SetPageIcon(0, fake);
    EXPECT_TRUE(tp.GetPageIcon(0) == fake, _T("PI/roundTrip"));
    EXPECT_TRUE(tp.GetHeader()->GetTabIcon(0) == fake, _T("PI/forwarded"));
    tp.SetPageIcon(0, nullptr);
    EXPECT_TRUE(tp.GetPageIcon(0) == nullptr, _T("PI/cleared"));
    return OK(_T("Page_IconRoundTrip"));
}

// SetAutoFitTabWidth / SetIconSize / SetIconGap 都透传到内部 DuiTab。
static Result Test_Page_AutoFitForward()
{
    DuiTabPage tp;
    EXPECT_TRUE(tp.GetAutoFitTabWidth() == false, _T("PAF/default"));
    tp.SetAutoFitTabWidth(true);
    EXPECT_TRUE(tp.GetAutoFitTabWidth() == true, _T("PAF/getOn"));
    EXPECT_TRUE(tp.GetHeader()->GetAutoFitTabWidth() == true, _T("PAF/forwardedOn"));

    tp.SetIconSize(20);
    EXPECT_INT(tp.GetIconSize(), 20, _T("PAF/iconSize"));
    EXPECT_INT(tp.GetHeader()->GetIconSize(), 20, _T("PAF/iconSizeFwd"));

    tp.SetIconGap(10);
    EXPECT_INT(tp.GetIconGap(), 10, _T("PAF/iconGap"));
    EXPECT_INT(tp.GetHeader()->GetIconGap(), 10, _T("PAF/iconGapFwd"));
    return OK(_T("Page_AutoFitForward"));
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
        { _T("Defaults"),               &Test_Defaults               },
        { _T("AddPageIndex"),           &Test_AddPageIndex           },
        { _T("SetCurSelVisibility"),    &Test_SetCurSelVisibility    },
        { _T("SetCurSelOutOfRange"),    &Test_SetCurSelOutOfRange    },
        { _T("RemovePageShifts"),       &Test_RemovePageShifts       },
        { _T("SetPageReplaces"),        &Test_SetPageReplaces        },
        { _T("AddPageNull"),            &Test_AddPageNull            },
        { _T("SetHeaderHeightClamp"),   &Test_SetHeaderHeightClamp   },
        { _T("LayoutSplitsArea"),       &Test_LayoutSplitsArea       },
        { _T("Page_IconRoundTrip"),     &Test_Page_IconRoundTrip     },
        { _T("Page_AutoFitForward"),    &Test_Page_AutoFitForward    },
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
    summary.Format(_T("[summary] DuiTabPageTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiTabPageTests

} // namespace balloonwjui

#endif // BUI_FEATURE_TABPAGE
