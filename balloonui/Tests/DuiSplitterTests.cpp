#include "stdafx.h"
#include "DuiSplitterTests.h"

#if BUI_FEATURE_SPLITTER


namespace balloonwjui {

namespace DuiSplitterTests {

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
#define EXPECT_RECT(rc, L, T, R, B, name) \
    do { const RECT& _r = (rc); \
         if (_r.left!=(L)||_r.top!=(T)||_r.right!=(R)||_r.bottom!=(B)) { \
             CString _d; _d.Format(_T("expected=(%d,%d,%d,%d) got=(%d,%d,%d,%d)"), \
                 (L),(T),(R),(B),_r.left,_r.top,_r.right,_r.bottom); \
             return Fail(name, _d); } \
    } while (0)

class StubChild : public DuiControl
{
public:
    void OnPaint(HDC, const RECT&) override {}
};

// ----- API round-trips ------------------------------------------------

static Result Test_Defaults()
{
    DuiSplitter s;
    EXPECT_INT(s.GetOrientation(), DuiSplitter::Vertical, _T("Def/orient"));
    EXPECT_INT(s.GetBarThickness(), 4,                    _T("Def/bar"));
    EXPECT_INT(s.GetMinSize0(), 40,                       _T("Def/min0"));
    EXPECT_INT(s.GetMinSize1(), 40,                       _T("Def/min1"));
    return OK(_T("Defaults"));
}

static Result Test_SetterRoundTrip()
{
    DuiSplitter s;
    s.SetOrientation(DuiSplitter::Horizontal);
    EXPECT_INT(s.GetOrientation(), DuiSplitter::Horizontal, _T("RT/orient"));
    s.SetBarThickness(8);
    EXPECT_INT(s.GetBarThickness(), 8, _T("RT/bar"));
    s.SetBarThickness(0);                       // clamp to >= 1
    EXPECT_INT(s.GetBarThickness(), 1, _T("RT/barClamp"));
    s.SetMinSizes(60, 90);
    EXPECT_INT(s.GetMinSize0(), 60, _T("RT/min0"));
    EXPECT_INT(s.GetMinSize1(), 90, _T("RT/min1"));
    s.SetMinSizes(-3, -5);                      // clamp to >= 0
    EXPECT_INT(s.GetMinSize0(), 0, _T("RT/min0Clamp"));
    EXPECT_INT(s.GetMinSize1(), 0, _T("RT/min1Clamp"));
    return OK(_T("SetterRoundTrip"));
}

// ----- Layout: pixel split --------------------------------------------

// Vertical split at 100px: pane0 = [0..100], pane1 = [104..300]
static Result Test_LayoutPixelSplit_Vertical()
{
    DuiSplitter s;
    StubChild *a, *b;
    s.SetPane(0, std::unique_ptr<DuiControl>(a = new StubChild()));
    s.SetPane(1, std::unique_ptr<DuiControl>(b = new StubChild()));
    s.SetBarThickness(4);
    s.SetMinSizes(20, 20);
    s.SetSplitPx(100);
    s.Layout(RECT{ 0, 0, 300, 200 });

    EXPECT_RECT(a->GetRect(),   0, 0, 100, 200, _T("VPx/p0"));
    EXPECT_RECT(b->GetRect(), 104, 0, 300, 200, _T("VPx/p1"));
    EXPECT_INT(s.GetSplitPx(), 100, _T("VPx/state"));
    return OK(_T("LayoutPixelSplit_Vertical"));
}

// Horizontal split at 80px (in a 300x200 rect): pane0 top half, pane1 bottom.
static Result Test_LayoutPixelSplit_Horizontal()
{
    DuiSplitter s;
    StubChild *a, *b;
    s.SetOrientation(DuiSplitter::Horizontal);
    s.SetPane(0, std::unique_ptr<DuiControl>(a = new StubChild()));
    s.SetPane(1, std::unique_ptr<DuiControl>(b = new StubChild()));
    s.SetBarThickness(4);
    s.SetSplitPx(80);
    s.Layout(RECT{ 0, 0, 300, 200 });

    EXPECT_RECT(a->GetRect(), 0,  0, 300,  80, _T("HPx/p0"));
    EXPECT_RECT(b->GetRect(), 0, 84, 300, 200, _T("HPx/p1"));
    return OK(_T("LayoutPixelSplit_Horizontal"));
}

// Layout: fraction is applied at next Layout. avail = 300, bar = 4,
// track = 296, fraction 0.5 -> splitPx = 148 (clamped to track).
static Result Test_LayoutFractionSplit()
{
    DuiSplitter s;
    StubChild *a, *b;
    s.SetPane(0, std::unique_ptr<DuiControl>(a = new StubChild()));
    s.SetPane(1, std::unique_ptr<DuiControl>(b = new StubChild()));
    s.SetBarThickness(4);
    s.SetSplitFraction(0.5);
    s.Layout(RECT{ 0, 0, 300, 200 });
    EXPECT_INT(s.GetSplitPx(), 148, _T("Frac/0.5"));
    s.SetSplitFraction(1.5);                    // clamped to 1.0 -> all to pane 0
    s.Layout(RECT{ 0, 0, 300, 200 });
    // splitPx clamped further by min1 (40): max possible is 300-4-40 = 256.
    EXPECT_INT(s.GetSplitPx(), 256, _T("Frac/over1"));
    return OK(_T("LayoutFractionSplit"));
}

// ----- Drag clamping --------------------------------------------------

// Drag bar far left: clamps to min0 = 50.
static Result Test_DragClampsToMin0()
{
    DuiSplitter s;
    StubChild *a, *b;
    s.SetPane(0, std::unique_ptr<DuiControl>(a = new StubChild()));
    s.SetPane(1, std::unique_ptr<DuiControl>(b = new StubChild()));
    s.SetMinSizes(50, 50);
    s.SetSplitPx(150);
    s.Layout(RECT{ 0, 0, 300, 200 });

    // Bar starts at x=150. Click within it.
    s.OnLButtonDown(POINT{ 152, 100 }, 0);
    s.OnMouseMove  (POINT{ -50, 100 }, 0);     // drag well past left edge
    s.OnLButtonUp  (POINT{ -50, 100 }, 0);
    EXPECT_INT(s.GetSplitPx(), 50, _T("DragMin0"));
    return OK(_T("DragClampsToMin0"));
}

// Drag bar far right: clamps such that pane1 has at least min1.
static Result Test_DragClampsToMin1()
{
    DuiSplitter s;
    StubChild *a, *b;
    s.SetPane(0, std::unique_ptr<DuiControl>(a = new StubChild()));
    s.SetPane(1, std::unique_ptr<DuiControl>(b = new StubChild()));
    s.SetMinSizes(20, 60);
    s.SetBarThickness(4);
    s.SetSplitPx(150);
    s.Layout(RECT{ 0, 0, 300, 200 });

    s.OnLButtonDown(POINT{ 152, 100 }, 0);
    s.OnMouseMove  (POINT{ 999, 100 }, 0);     // drag past right edge
    s.OnLButtonUp  (POINT{ 999, 100 }, 0);
    // Max splitPx = 300 - 4 - 60 = 236.
    EXPECT_INT(s.GetSplitPx(), 236, _T("DragMin1"));
    return OK(_T("DragClampsToMin1"));
}

// Click outside the bar does not start a drag.
static Result Test_ClickOutsideBarIgnored()
{
    DuiSplitter s;
    StubChild *a, *b;
    s.SetPane(0, std::unique_ptr<DuiControl>(a = new StubChild()));
    s.SetPane(1, std::unique_ptr<DuiControl>(b = new StubChild()));
    s.SetSplitPx(150);
    s.Layout(RECT{ 0, 0, 300, 200 });

    bool consumed = s.OnLButtonDown(POINT{ 50, 50 }, 0);   // inside pane 0
    EXPECT_TRUE(!consumed, _T("Click/notConsumed"));
    s.OnMouseMove(POINT{ 30, 50 }, 0);
    EXPECT_INT(s.GetSplitPx(), 150, _T("Click/splitUnchanged"));
    return OK(_T("ClickOutsideBarIgnored"));
}

// IsPointInBar gives the right answer for vertical / horizontal layouts.
static Result Test_HitTestBarRegion()
{
    DuiSplitter s;
    s.SetBarThickness(6);
    s.SetSplitPx(100);
    s.Layout(RECT{ 0, 0, 300, 200 });

    EXPECT_TRUE(s.IsPointInBar(POINT{ 100, 50 }), _T("HT/onBarLeftEdge"));
    EXPECT_TRUE(s.IsPointInBar(POINT{ 105, 50 }), _T("HT/onBarRightEdge"));
    EXPECT_TRUE(!s.IsPointInBar(POINT{ 99,  50 }), _T("HT/leftOfBar"));
    EXPECT_TRUE(!s.IsPointInBar(POINT{ 106, 50 }), _T("HT/rightOfBar"));

    s.SetOrientation(DuiSplitter::Horizontal);
    s.SetSplitPx(80);
    s.Layout(RECT{ 0, 0, 300, 200 });
    EXPECT_TRUE(s.IsPointInBar(POINT{ 50, 80 }), _T("HT/onHbar"));
    EXPECT_TRUE(s.IsPointInBar(POINT{ 50, 85 }), _T("HT/onHbarBottom"));
    EXPECT_TRUE(!s.IsPointInBar(POINT{ 50, 79 }), _T("HT/aboveHbar"));
    EXPECT_TRUE(!s.IsPointInBar(POINT{ 50, 86 }), _T("HT/belowHbar"));
    return OK(_T("HitTestBarRegion"));
}

// SetSplitPx clamps when given an out-of-range value.
static Result Test_SetSplitPxClamps()
{
    DuiSplitter s;
    StubChild *a, *b;
    s.SetPane(0, std::unique_ptr<DuiControl>(a = new StubChild()));
    s.SetPane(1, std::unique_ptr<DuiControl>(b = new StubChild()));
    s.SetMinSizes(30, 30);
    s.SetBarThickness(4);
    s.Layout(RECT{ 0, 0, 200, 200 });
    s.SetSplitPx(-50);
    EXPECT_INT(s.GetSplitPx(), 30, _T("Px/lower"));
    s.SetSplitPx(9999);
    // Max for pane0 = 200 - 4 - 30 = 166.
    EXPECT_INT(s.GetSplitPx(), 166, _T("Px/upper"));
    return OK(_T("SetSplitPxClamps"));
}

// Empty pane slots: layout still works on the bar position.
static Result Test_LayoutEmptyPanes()
{
    DuiSplitter s;
    s.SetSplitPx(80);
    s.Layout(RECT{ 0, 0, 200, 100 });
    RECT b = s.GetBarRect();
    EXPECT_RECT(b, 80, 0, 84, 100, _T("EP/bar"));
    return OK(_T("LayoutEmptyPanes"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_RECT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),                  &Test_Defaults                  },
        { _T("SetterRoundTrip"),           &Test_SetterRoundTrip           },
        { _T("LayoutPixelSplit_Vertical"), &Test_LayoutPixelSplit_Vertical },
        { _T("LayoutPixelSplit_Horizontal"), &Test_LayoutPixelSplit_Horizontal },
        { _T("LayoutFractionSplit"),       &Test_LayoutFractionSplit       },
        { _T("DragClampsToMin0"),          &Test_DragClampsToMin0          },
        { _T("DragClampsToMin1"),          &Test_DragClampsToMin1          },
        { _T("ClickOutsideBarIgnored"),    &Test_ClickOutsideBarIgnored    },
        { _T("HitTestBarRegion"),          &Test_HitTestBarRegion          },
        { _T("SetSplitPxClamps"),          &Test_SetSplitPxClamps          },
        { _T("LayoutEmptyPanes"),          &Test_LayoutEmptyPanes          },
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
    summary.Format(_T("[summary] DuiSplitterTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiSplitterTests

} // namespace balloonwjui

#endif // BUI_FEATURE_SPLITTER
