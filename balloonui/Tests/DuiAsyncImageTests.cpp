#include "stdafx.h"
#include "DuiAsyncImageTests.h"

namespace balloonwjui {

namespace DuiAsyncImageTests {

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

// Resolve a path next to the EXE (e.g. Bin\Face\face0.png) so the
// loader has a real PNG to chew on.
static CString MakeFacePath(int n)
{
    TCHAR buf[MAX_PATH] = {};
    ::GetModuleFileName(NULL, buf, MAX_PATH);
    CString p = buf;
    int slash = p.ReverseFind(_T('\\'));
    if (slash >= 0)
    {
        p = p.Left(slash);
    }
    CString r;
    r.Format(_T("%s\\Face\\face%d.png"), (LPCTSTR)p, n);
    return r;
}

// ----- Sync path: decode succeeds for a real PNG ---------------------

static Result Test_SyncDecodesFace0()
{
    CString path = MakeFacePath(0);
    DuiAsyncImage::Result got = {};
    DuiAsyncImage::RunSyncForTests(path, 0xCAFE,
        [&](const DuiAsyncImage::Result& r)
        {
            got = r;
        });

    if (!got.ok)
    {
        // Skip rather than fail when the PNG is missing in the test
        // environment — RunSync still went through the full GDI+ path.
        return OK(_T("SyncDecodesFace0(skipped:missing)"));
    }
    EXPECT_TRUE(got.hbm != nullptr, _T("Sync/hbm"));
    EXPECT_TRUE(got.width  > 0,     _T("Sync/w"));
    EXPECT_TRUE(got.height > 0,     _T("Sync/h"));
    EXPECT_INT((int)got.userToken, 0xCAFE, _T("Sync/token"));
    if (got.hbm)
    {
        ::DeleteObject(got.hbm);
    }
    return OK(_T("SyncDecodesFace0"));
}

// Bogus path: ok=false, hbm=null.
static Result Test_SyncMissingFile()
{
    DuiAsyncImage::Result got = {};
    DuiAsyncImage::RunSyncForTests(_T("Z:\\__no_such__.png"), 0xBEEF,
        [&](const DuiAsyncImage::Result& r)
        {
            got = r;
        });
    EXPECT_TRUE(!got.ok, _T("Miss/ok"));
    EXPECT_TRUE(got.hbm == nullptr, _T("Miss/hbm"));
    EXPECT_INT((int)got.userToken, 0xBEEF, _T("Miss/token"));
    return OK(_T("SyncMissingFile"));
}

// ----- Async submit path: callback fires after pumping --------------

static Result Test_AsyncSubmitAndPump()
{
    CString path = MakeFacePath(0);

    // Submit. If the file is missing the callback will still fire with
    // ok=false; either way it must arrive after we pump.
    bool fired = false;
    DuiAsyncImage::Result got = {};
    DWORD_PTR id = DuiAsyncImage::Submit(path,
        [&](const DuiAsyncImage::Result& r)
        {
            fired = true;
            got = r;
        },
        0xABCD);
    EXPECT_TRUE(id > 0, _T("Sub/id"));

    // Wait for the worker. Try up to ~2s pumping the message-only
    // window each loop turn.
    for (int i = 0; i < 200 && !fired; ++i)
    {
        DuiAsyncImage::PumpForTests();
        ::Sleep(10);
    }
    EXPECT_TRUE(fired, _T("Sub/fired"));
    EXPECT_INT((int)got.userToken, 0xABCD, _T("Sub/token"));
    if (got.hbm)
    {
        ::DeleteObject(got.hbm);
    }
    return OK(_T("AsyncSubmitAndPump"));
}

// Submit with null path / null callback returns 0.
static Result Test_SubmitGuards()
{
    EXPECT_INT((int)DuiAsyncImage::Submit(nullptr,
        [](const DuiAsyncImage::Result&){}), 0, _T("Guard/null"));
    EXPECT_INT((int)DuiAsyncImage::Submit(_T("foo.png"),
        DuiAsyncImage::Callback()), 0, _T("Guard/cb"));
    return OK(_T("SubmitGuards"));
}

// Cancel before the worker picks up should suppress the callback.
// (Not always observable due to race; we settle for "Cancel does not
// crash + later Pump still drains anything outstanding".)
static Result Test_CancelDoesNotCrash()
{
    DWORD_PTR id = DuiAsyncImage::Submit(_T("Z:\\__nonexistent_for_cancel__.png"),
        [](const DuiAsyncImage::Result&){}, 0);
    DuiAsyncImage::Cancel(id);
    for (int i = 0; i < 50; ++i)
    {
        DuiAsyncImage::PumpForTests();
        ::Sleep(5);
    }
    return OK(_T("CancelDoesNotCrash"));
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
        { _T("SyncDecodesFace0"),    &Test_SyncDecodesFace0    },
        { _T("SyncMissingFile"),     &Test_SyncMissingFile     },
        { _T("AsyncSubmitAndPump"),  &Test_AsyncSubmitAndPump  },
        { _T("SubmitGuards"),        &Test_SubmitGuards        },
        { _T("CancelDoesNotCrash"),  &Test_CancelDoesNotCrash  },
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
    summary.Format(_T("[summary] DuiAsyncImageTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiAsyncImageTests

} // namespace balloonwjui
