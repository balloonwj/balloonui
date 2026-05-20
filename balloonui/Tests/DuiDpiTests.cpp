#include "stdafx.h"
#include "DuiDpiTests.h"
#include "../DuiResMgr.h"

namespace balloonwjui {

namespace DuiDpiTests {

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

// ----- Scale / Unscale math -------------------------------------------

static Result Test_ScaleIdentity96()
{
    EXPECT_INT(DuiDpi::Scale(100, 96), 100, _T("Scl/96id"));
    EXPECT_INT(DuiDpi::Unscale(100, 96), 100, _T("Scl/96idU"));
    return OK(_T("ScaleIdentity96"));
}

static Result Test_Scale120pct()
{
    // 120dpi = 125%
    EXPECT_INT(DuiDpi::Scale(80, 120),  100, _T("Scl/125pct"));
    EXPECT_INT(DuiDpi::Unscale(100, 120), 80, _T("Scl/125pctU"));
    return OK(_T("Scale120pct"));
}

static Result Test_Scale144pct()
{
    // 144dpi = 150%
    EXPECT_INT(DuiDpi::Scale(40, 144),   60, _T("Scl/150pct"));
    EXPECT_INT(DuiDpi::Unscale(60, 144), 40, _T("Scl/150pctU"));
    return OK(_T("Scale144pct"));
}

static Result Test_ScaleZeroDpiFallsBack()
{
    EXPECT_INT(DuiDpi::Scale(123, 0),   123, _T("Scl/zeroFallback"));
    EXPECT_INT(DuiDpi::Scale(123, -10), 123, _T("Scl/negFallback"));
    return OK(_T("ScaleZeroDpiFallsBack"));
}

// ----- runtime queries ------------------------------------------------

static Result Test_GetSystemDpiPositive()
{
    int d = DuiDpi::GetSystemDpi();
    EXPECT_TRUE(d >= 72,  _T("Sys/lowerSane"));
    EXPECT_TRUE(d <= 600, _T("Sys/upperSane"));
    return OK(_T("GetSystemDpiPositive"));
}

static Result Test_GetWindowDpiNullFallback()
{
    int d = DuiDpi::GetWindowDpi(nullptr);
    EXPECT_TRUE(d > 0, _T("Win/nullFallback"));
    return OK(_T("GetWindowDpiNullFallback"));
}

// ----- DuiResMgr DPI plumbing -----------------------------------------

// Setting the DPI invalidates the cached HFONT so the next
// GetDefaultFont() re-creates one sized for the new DPI. We can't read
// the HFONT's pt size directly, but two calls at different DPIs should
// not return the *same* handle (the handle is recreated lazily).
static Result Test_DpiInvalidatesFont()
{
    DuiResMgr& rm = DuiResMgr::Inst();
    rm.SetDpi(96);
    HFONT a = rm.GetDefaultFont();
    EXPECT_TRUE(a != nullptr, _T("Font/atDpi96"));
    rm.SetDpi(192);
    HFONT b = rm.GetDefaultFont();
    EXPECT_TRUE(b != nullptr, _T("Font/atDpi192"));
    EXPECT_TRUE(a != b, _T("Font/handleChanged"));
    EXPECT_INT(rm.GetDpi(), 192, _T("Font/getDpi"));
    // Restore for downstream tests + ambient code.
    rm.SetDpi(DuiDpi::GetSystemDpi());
    return OK(_T("DpiInvalidatesFont"));
}

// SetDpi to the same value is a no-op (HFONT handle stays the same).
static Result Test_DpiIdempotent()
{
    DuiResMgr& rm = DuiResMgr::Inst();
    rm.SetDpi(96);
    HFONT a = rm.GetDefaultFont();
    rm.SetDpi(96);
    HFONT b = rm.GetDefaultFont();
    EXPECT_TRUE(a == b, _T("Idem/sameHandle"));
    rm.SetDpi(DuiDpi::GetSystemDpi());
    return OK(_T("DpiIdempotent"));
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
        { _T("ScaleIdentity96"),        &Test_ScaleIdentity96        },
        { _T("Scale120pct"),            &Test_Scale120pct            },
        { _T("Scale144pct"),            &Test_Scale144pct            },
        { _T("ScaleZeroDpiFallsBack"),  &Test_ScaleZeroDpiFallsBack  },
        { _T("GetSystemDpiPositive"),   &Test_GetSystemDpiPositive   },
        { _T("GetWindowDpiNullFallback"), &Test_GetWindowDpiNullFallback },
        { _T("DpiInvalidatesFont"),     &Test_DpiInvalidatesFont     },
        { _T("DpiIdempotent"),          &Test_DpiIdempotent          },
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
    summary.Format(_T("[summary] DuiDpiTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiDpiTests

} // namespace balloonwjui
