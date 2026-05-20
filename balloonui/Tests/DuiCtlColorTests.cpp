#include "stdafx.h"
#include "DuiCtlColorTests.h"

namespace balloonwjui {

namespace DuiCtlColorTests {

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
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)

// ----- defaults --------------------------------------------------------

static Result Test_Defaults()
{
    HwndHostControl h;
    EXPECT_INT((int)h.GetCtlBgColor(),   (int)CLR_INVALID, _T("Def/bg"));
    EXPECT_INT((int)h.GetCtlTextColor(), (int)CLR_INVALID, _T("Def/text"));
    return OK(_T("Defaults"));
}

// SetCtlBgColor / SetCtlTextColor round-trip.
static Result Test_RoundTrip()
{
    HwndHostControl h;
    h.SetCtlBgColor(RGB(40, 60, 90));
    EXPECT_INT((int)h.GetCtlBgColor(), (int)RGB(40, 60, 90), _T("RT/bg"));
    h.SetCtlTextColor(RGB(220, 220, 220));
    EXPECT_INT((int)h.GetCtlTextColor(), (int)RGB(220, 220, 220), _T("RT/text"));
    return OK(_T("RoundTrip"));
}

// GetCtlColorBrush with no override returns null (caller falls through
// to DefWindowProc). hdc text color is also untouched in that path.
static Result Test_NoOverrideNullBrush()
{
    HwndHostControl h;
    HDC mem = ::CreateCompatibleDC(nullptr);
    COLORREF before = ::GetTextColor(mem);
    HBRUSH br = h.GetCtlColorBrush(WM_CTLCOLOREDIT, mem);
    COLORREF after = ::GetTextColor(mem);
    ::DeleteDC(mem);
    EXPECT_TRUE(br == nullptr, _T("NoOver/nullBrush"));
    EXPECT_INT((int)before, (int)after, _T("NoOver/textUnchanged"));
    return OK(_T("NoOverrideNullBrush"));
}

// With bg+text override, GetCtlColorBrush returns a brush + sets HDC
// colors. Calling twice returns the same brush handle (cached).
static Result Test_OverrideReturnsBrush()
{
    HwndHostControl h;
    h.SetCtlBgColor(RGB(20, 40, 80));
    h.SetCtlTextColor(RGB(255, 255, 255));

    HDC mem = ::CreateCompatibleDC(nullptr);
    HBRUSH a = h.GetCtlColorBrush(WM_CTLCOLOREDIT, mem);
    EXPECT_TRUE(a != nullptr, _T("Over/notNull"));
    EXPECT_INT((int)::GetTextColor(mem), (int)RGB(255, 255, 255), _T("Over/text"));
    EXPECT_INT((int)::GetBkColor(mem),   (int)RGB(20, 40, 80),    _T("Over/bg"));
    HBRUSH b = h.GetCtlColorBrush(WM_CTLCOLORSTATIC, mem);
    EXPECT_TRUE(a == b, _T("Over/cached"));
    ::DeleteDC(mem);
    return OK(_T("OverrideReturnsBrush"));
}

// Changing bg invalidates cached brush; next call gives a different
// HBRUSH for the new color.
static Result Test_BgChangeInvalidatesCache()
{
    HwndHostControl h;
    h.SetCtlBgColor(RGB(10, 20, 30));
    HDC mem = ::CreateCompatibleDC(nullptr);
    HBRUSH a = h.GetCtlColorBrush(WM_CTLCOLOREDIT, mem);
    h.SetCtlBgColor(RGB(200, 50, 50));
    HBRUSH b = h.GetCtlColorBrush(WM_CTLCOLOREDIT, mem);
    ::DeleteDC(mem);
    EXPECT_TRUE(a != nullptr && b != nullptr, _T("Inval/bothPresent"));
    EXPECT_TRUE(a != b, _T("Inval/different"));
    return OK(_T("BgChangeInvalidatesCache"));
}

// Setting bg back to CLR_INVALID drops back to "no override" mode.
static Result Test_BgClearedFallsThrough()
{
    HwndHostControl h;
    h.SetCtlBgColor(RGB(0, 0, 0));
    HDC mem = ::CreateCompatibleDC(nullptr);
    EXPECT_TRUE(h.GetCtlColorBrush(WM_CTLCOLOREDIT, mem) != nullptr, _T("Clr/onBefore"));
    h.SetCtlBgColor(CLR_INVALID);
    EXPECT_TRUE(h.GetCtlColorBrush(WM_CTLCOLOREDIT, mem) == nullptr, _T("Clr/offAfter"));
    ::DeleteDC(mem);
    return OK(_T("BgClearedFallsThrough"));
}

// Null hdc returns null without crashing.
static Result Test_NullHdcSafe()
{
    HwndHostControl h;
    h.SetCtlBgColor(RGB(0, 0, 0));
    EXPECT_TRUE(h.GetCtlColorBrush(WM_CTLCOLOREDIT, nullptr) == nullptr, _T("Null/safe"));
    return OK(_T("NullHdcSafe"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE

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
        { _T("Defaults"),                &Test_Defaults                },
        { _T("RoundTrip"),               &Test_RoundTrip               },
        { _T("NoOverrideNullBrush"),     &Test_NoOverrideNullBrush     },
        { _T("OverrideReturnsBrush"),    &Test_OverrideReturnsBrush    },
        { _T("BgChangeInvalidatesCache"),&Test_BgChangeInvalidatesCache},
        { _T("BgClearedFallsThrough"),   &Test_BgClearedFallsThrough   },
        { _T("NullHdcSafe"),             &Test_NullHdcSafe             },
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
    summary.Format(_T("[summary] DuiCtlColorTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiCtlColorTests

} // namespace balloonwjui
