#include "stdafx.h"
#include "DuiDropTargetTests.h"

namespace balloonwjui {

namespace DuiDropTargetTests {

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
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { return Fail(name, _T("string mismatch")); } \
    } while (0)

// ----- Default state --------------------------------------------------

static Result Test_DefaultState()
{
    DuiDropTargetHelper h;
    EXPECT_TRUE(!h.IsRegistered(), _T("Def/notReg"));
    return OK(_T("DefaultState"));
}

// ----- Register / Unregister against a real HWND ----------------------

static Result Test_RegisterUnregister()
{
    // Need a real HWND for RegisterDragDrop to bind to.
    HINSTANCE hInst = ::GetModuleHandle(nullptr);
    HWND h = ::CreateWindowEx(0, _T("STATIC"), _T(""),
                              WS_OVERLAPPED | WS_POPUP,
                              0, 0, 1, 1, nullptr, nullptr, hInst, nullptr);
    if (!h)
    {
        return Fail(_T("Reg/createHwnd"), _T("CreateWindowEx failed"));
    }

    DuiDropTargetHelper helper;
    bool ok = helper.Register(h);
    EXPECT_TRUE(ok, _T("Reg/ok"));
    EXPECT_TRUE(helper.IsRegistered(), _T("Reg/isReg"));
    helper.Unregister();
    EXPECT_TRUE(!helper.IsRegistered(), _T("Reg/unreg"));

    ::DestroyWindow(h);
    return OK(_T("RegisterUnregister"));
}

// ----- Null HWND register fails gracefully ----------------------------

static Result Test_RegisterNullFails()
{
    DuiDropTargetHelper h;
    EXPECT_TRUE(!h.Register(nullptr), _T("Null/fail"));
    EXPECT_TRUE(!h.IsRegistered(), _T("Null/notReg"));
    return OK(_T("RegisterNullFails"));
}

// ----- ExtractFilesFromHDrop synthesizes a CF_HDROP block -------------

// Build a synthetic HDROP HGLOBAL with two file paths, then verify
// ExtractFilesFromHDrop returns both paths in order.
static Result Test_ExtractFromHDrop()
{
    // CF_HDROP layout: DROPFILES header, then concatenated null-
    // terminated file paths, ending with a double null. Wide format.
    LPCTSTR p0 = _T("C:\\foo\\bar.png");
    LPCTSTR p1 = _T("D:\\baz quux.txt");
    SIZE_T len0 = _tcslen(p0);
    SIZE_T len1 = _tcslen(p1);

    SIZE_T payloadChars = len0 + 1 + len1 + 1 + 1;     // +1 sentinel
    SIZE_T payloadBytes = payloadChars * sizeof(TCHAR);
    SIZE_T total = sizeof(DROPFILES) + payloadBytes;

    HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total);
    if (!h)
    {
        return Fail(_T("Hdrop/alloc"), _T("GlobalAlloc failed"));
    }
    DROPFILES* df = (DROPFILES*)::GlobalLock(h);
    df->pFiles = sizeof(DROPFILES);
#ifdef _UNICODE
    df->fWide = TRUE;
#else
    df->fWide = FALSE;
#endif
    TCHAR* paths = (TCHAR*)((BYTE*)df + sizeof(DROPFILES));
    _tcscpy_s(paths, len0 + 1, p0);
    _tcscpy_s(paths + len0 + 1, len1 + 1, p1);
    paths[len0 + 1 + len1 + 1] = 0;     // double-null terminator
    ::GlobalUnlock(h);

    auto v = DuiDropTargetHelper::ExtractFilesFromHDrop((HDROP)h);
    bool wasTwo = (v.size() == 2);
    if (!wasTwo)
    {
        ::GlobalFree(h);
        CString d;
        d.Format(_T("expected 2 got %d"), (int)v.size());
        return Fail(_T("Hdrop/count"), d);
    }
    EXPECT_STR(v[0], CString(p0), _T("Hdrop/p0"));
    EXPECT_STR(v[1], CString(p1), _T("Hdrop/p1"));
    ::GlobalFree(h);
    return OK(_T("ExtractFromHDrop"));
}

// Null HDROP returns empty vec.
static Result Test_ExtractFromNullDropEmpty()
{
    auto v = DuiDropTargetHelper::ExtractFilesFromHDrop(nullptr);
    EXPECT_INT((int)v.size(), 0, _T("Null/empty"));
    return OK(_T("ExtractFromNullDropEmpty"));
}

// SetCallbacks before Register: storing must not crash.
static Result Test_SetCallbacksBeforeRegister()
{
    DuiDropTargetHelper h;
    h.SetCallbacks(
        [](const std::vector<CString>&) {},
        [](HBITMAP) {});
    EXPECT_TRUE(!h.IsRegistered(), _T("Cb/notReg"));
    return OK(_T("SetCallbacksBeforeRegister"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_STR

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
        { _T("DefaultState"),               &Test_DefaultState               },
        { _T("RegisterUnregister"),         &Test_RegisterUnregister         },
        { _T("RegisterNullFails"),          &Test_RegisterNullFails          },
        { _T("ExtractFromHDrop"),           &Test_ExtractFromHDrop           },
        { _T("ExtractFromNullDropEmpty"),   &Test_ExtractFromNullDropEmpty   },
        { _T("SetCallbacksBeforeRegister"), &Test_SetCallbacksBeforeRegister },
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
    summary.Format(_T("[summary] DuiDropTargetTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiDropTargetTests

} // namespace balloonwjui
