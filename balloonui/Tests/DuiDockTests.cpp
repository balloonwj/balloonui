#include "stdafx.h"
#include "DuiDockTests.h"

#if BUI_FEATURE_DOCK


namespace balloonwjui {

namespace DuiDockTests {

namespace {

struct Result
{
    CString name;
    bool ok;
    CString detail;
};

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

// Fill-only: child fills the whole rect.
static Result Test_FillOnly()
{
    DuiDock d;
    StubChild* c;
    d.AddDocked(std::unique_ptr<DuiControl>(c = new StubChild()), DuiDock::DockFill);
    d.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_RECT(c->GetRect(), 0, 0, 200, 100, _T("Fill/full"));
    return OK(_T("FillOnly"));
}

// Top + bottom + fill: top strip 30, bottom strip 20, fill in the middle.
static Result Test_TopBottomFill()
{
    DuiDock d;
    StubChild *t, *b, *f;
    d.AddDocked(std::unique_ptr<DuiControl>(t = new StubChild()), DuiDock::DockTop, 30);
    d.AddDocked(std::unique_ptr<DuiControl>(b = new StubChild()), DuiDock::DockBottom, 20);
    d.AddDocked(std::unique_ptr<DuiControl>(f = new StubChild()), DuiDock::DockFill);
    d.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_RECT(t->GetRect(),   0,   0, 200,  30, _T("TBF/top"));
    EXPECT_RECT(b->GetRect(),   0,  80, 200, 100, _T("TBF/bot"));
    EXPECT_RECT(f->GetRect(),   0,  30, 200,  80, _T("TBF/fill"));
    return OK(_T("TopBottomFill"));
}

// Left + right + fill: left strip 40, right strip 50, fill 110 wide.
static Result Test_LeftRightFill()
{
    DuiDock d;
    StubChild *l, *r, *f;
    d.AddDocked(std::unique_ptr<DuiControl>(l = new StubChild()), DuiDock::DockLeft,  40);
    d.AddDocked(std::unique_ptr<DuiControl>(r = new StubChild()), DuiDock::DockRight, 50);
    d.AddDocked(std::unique_ptr<DuiControl>(f = new StubChild()), DuiDock::DockFill);
    d.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_RECT(l->GetRect(),   0,   0,  40, 100, _T("LRF/left"));
    EXPECT_RECT(r->GetRect(), 150,   0, 200, 100, _T("LRF/right"));
    EXPECT_RECT(f->GetRect(),  40,   0, 150, 100, _T("LRF/fill"));
    return OK(_T("LeftRightFill"));
}

// Full chrome: top + bottom + left + right + fill.
static Result Test_AllSidesFill()
{
    DuiDock d;
    StubChild *t, *b, *l, *r, *f;
    d.AddDocked(std::unique_ptr<DuiControl>(t = new StubChild()), DuiDock::DockTop,    30);
    d.AddDocked(std::unique_ptr<DuiControl>(b = new StubChild()), DuiDock::DockBottom, 20);
    d.AddDocked(std::unique_ptr<DuiControl>(l = new StubChild()), DuiDock::DockLeft,   40);
    d.AddDocked(std::unique_ptr<DuiControl>(r = new StubChild()), DuiDock::DockRight,  50);
    d.AddDocked(std::unique_ptr<DuiControl>(f = new StubChild()), DuiDock::DockFill);
    d.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_RECT(t->GetRect(),   0,   0, 200,  30, _T("All/top"));
    EXPECT_RECT(b->GetRect(),   0,  80, 200, 100, _T("All/bot"));
    EXPECT_RECT(l->GetRect(),   0,  30,  40,  80, _T("All/left"));
    EXPECT_RECT(r->GetRect(), 150,  30, 200,  80, _T("All/right"));
    EXPECT_RECT(f->GetRect(),  40,  30, 150,  80, _T("All/fill"));
    return OK(_T("AllSidesFill"));
}

// Padding subtracts uniformly from inner rect.
static Result Test_Padding()
{
    DuiDock d;
    d.SetPadding(10, 8, 6, 4);
    StubChild* f;
    d.AddDocked(std::unique_ptr<DuiControl>(f = new StubChild()), DuiDock::DockFill);
    d.Layout(RECT{ 0, 0, 100, 100 });
    EXPECT_RECT(f->GetRect(), 10, 8, 94, 96, _T("Pad/fill"));
    return OK(_T("Padding"));
}

// Gap between dock strips.
static Result Test_Gap()
{
    DuiDock d;
    d.SetGap(5);
    StubChild *t, *b, *f;
    d.AddDocked(std::unique_ptr<DuiControl>(t = new StubChild()), DuiDock::DockTop, 20);
    d.AddDocked(std::unique_ptr<DuiControl>(b = new StubChild()), DuiDock::DockBottom, 20);
    d.AddDocked(std::unique_ptr<DuiControl>(f = new StubChild()), DuiDock::DockFill);
    d.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_RECT(t->GetRect(),   0,   0, 200,  20, _T("Gap/top"));
    EXPECT_RECT(b->GetRect(),   0,  80, 200, 100, _T("Gap/bot"));
    // After top: inner.top = 25 (20 + 5 gap). After bottom: inner.bottom = 75.
    EXPECT_RECT(f->GetRect(),   0,  25, 200,  75, _T("Gap/fill"));
    return OK(_T("Gap"));
}

// Strip larger than available area: clamps to available, child gets full
// rect, leftover shrinks to zero (later children get empty).
static Result Test_StripLargerThanAvail()
{
    DuiDock d;
    StubChild *t, *f;
    d.AddDocked(std::unique_ptr<DuiControl>(t = new StubChild()), DuiDock::DockTop, 999);
    d.AddDocked(std::unique_ptr<DuiControl>(f = new StubChild()), DuiDock::DockFill);
    d.Layout(RECT{ 0, 0, 100, 50 });
    EXPECT_RECT(t->GetRect(), 0, 0, 100, 50, _T("Lg/top"));
    // Fill drains to zero strip but layout still valid.
    EXPECT_INT(f->GetRect().right - f->GetRect().left, 0, _T("Lg/fillW"));
    return OK(_T("StripLargerThanAvail"));
}

// First Fill claims the rest; later siblings get empty rects.
static Result Test_SecondFillEmpty()
{
    DuiDock d;
    StubChild *f1, *f2;
    d.AddDocked(std::unique_ptr<DuiControl>(f1 = new StubChild()), DuiDock::DockFill);
    d.AddDocked(std::unique_ptr<DuiControl>(f2 = new StubChild()), DuiDock::DockFill);
    d.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_RECT(f1->GetRect(), 0, 0, 200, 100, _T("2F/first"));
    // f2 layout was skipped (inner drained); m_rcItem stays at default.
    return OK(_T("SecondFillEmpty"));
}

// Side / size accessors round-trip in declaration order.
static Result Test_AccessorsRoundTrip()
{
    DuiDock d;
    d.AddDocked(std::unique_ptr<DuiControl>(new StubChild()), DuiDock::DockTop,    30);
    d.AddDocked(std::unique_ptr<DuiControl>(new StubChild()), DuiDock::DockLeft,   40);
    d.AddDocked(std::unique_ptr<DuiControl>(new StubChild()), DuiDock::DockFill);
    EXPECT_INT(d.GetChildCount(), 3, _T("Acc/count"));
    EXPECT_INT(d.GetChildSide(0), DuiDock::DockTop,  _T("Acc/0side"));
    EXPECT_INT(d.GetChildSize(0), 30, _T("Acc/0size"));
    EXPECT_INT(d.GetChildSide(1), DuiDock::DockLeft, _T("Acc/1side"));
    EXPECT_INT(d.GetChildSize(1), 40, _T("Acc/1size"));
    EXPECT_INT(d.GetChildSide(2), DuiDock::DockFill, _T("Acc/2side"));
    return OK(_T("AccessorsRoundTrip"));
}

// Negative sizePx clamps to 0.
static Result Test_NegativeSizeClamps()
{
    DuiDock d;
    d.AddDocked(std::unique_ptr<DuiControl>(new StubChild()), DuiDock::DockTop, -5);
    EXPECT_INT(d.GetChildSize(0), 0, _T("Neg/clamp"));
    return OK(_T("NegativeSizeClamps"));
}

#undef EXPECT_INT
#undef EXPECT_RECT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn fn;
    };
    Entry tests[] = {
        { _T("FillOnly"),             &Test_FillOnly             },
        { _T("TopBottomFill"),        &Test_TopBottomFill        },
        { _T("LeftRightFill"),        &Test_LeftRightFill        },
        { _T("AllSidesFill"),         &Test_AllSidesFill         },
        { _T("Padding"),              &Test_Padding              },
        { _T("Gap"),                  &Test_Gap                  },
        { _T("StripLargerThanAvail"), &Test_StripLargerThanAvail },
        { _T("SecondFillEmpty"),      &Test_SecondFillEmpty      },
        { _T("AccessorsRoundTrip"),   &Test_AccessorsRoundTrip   },
        { _T("NegativeSizeClamps"),   &Test_NegativeSizeClamps   },
    };

    CString out;
    int passed = 0;
    int failed = 0;
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
    summary.Format(_T("[summary] DuiDockTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiDockTests

} // namespace balloonwjui

#endif // BUI_FEATURE_DOCK
