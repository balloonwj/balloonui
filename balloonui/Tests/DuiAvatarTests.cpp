#include "stdafx.h"
#include "DuiAvatarTests.h"

#if BUI_FEATURE_AVATAR


namespace balloonwjui {

namespace DuiAvatarTests {

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
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { return Fail(name, _T("string mismatch: \"") + _a + _T("\" vs \"") + _e + _T("\"")); } \
    } while (0)
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)

// ----- Defaults --------------------------------------------------------

static Result Test_Defaults()
{
    DuiAvatar a;
    EXPECT_INT(a.GetShape(),  DuiAvatar::ShapeCircle, _T("Def/shape"));
    EXPECT_INT(a.GetStatus(), DuiAvatar::StatusNone,  _T("Def/status"));
    EXPECT_INT(a.GetCornerRadius(), 8, _T("Def/corner"));
    EXPECT_TRUE(a.GetBitmap() == nullptr, _T("Def/bitmap"));
    EXPECT_TRUE(a.GetName().IsEmpty(),    _T("Def/name"));
    return OK(_T("Defaults"));
}

// ----- Setter round-trips ---------------------------------------------

static Result Test_SetterRoundTrip()
{
    DuiAvatar a;
    a.SetShape(DuiAvatar::ShapeRoundRect);
    EXPECT_INT(a.GetShape(), DuiAvatar::ShapeRoundRect, _T("RT/shape"));
    a.SetCornerRadius(12);
    EXPECT_INT(a.GetCornerRadius(), 12, _T("RT/corner"));
    a.SetCornerRadius(-5);
    EXPECT_INT(a.GetCornerRadius(), 0, _T("RT/cornerClamp"));
    a.SetStatus(DuiAvatar::StatusBusy);
    EXPECT_INT(a.GetStatus(), DuiAvatar::StatusBusy, _T("RT/status"));
    a.SetName(_T("Alice"));
    EXPECT_STR(a.GetName(), _T("Alice"), _T("RT/name"));
    a.SetName(nullptr);
    EXPECT_STR(a.GetName(), _T(""), _T("RT/nameNull"));
    a.SetFallbackBgColor(RGB(11, 22, 33));
    EXPECT_INT((int)a.GetFallbackBgColor(), (int)RGB(11, 22, 33), _T("RT/bg"));
    a.SetInitialsColor(RGB(44, 55, 66));
    EXPECT_INT((int)a.GetInitialsColor(), (int)RGB(44, 55, 66), _T("RT/initClr"));
    return OK(_T("SetterRoundTrip"));
}

// ----- ComputeInitials -------------------------------------------------

static Result Test_ComputeInitials()
{
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("Alice")),       _T("A"),  _T("Init/oneWord"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("Alice Smith")), _T("AS"), _T("Init/twoWords"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("alice b cox")), _T("AC"), _T("Init/firstAndLast"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("  spaced  ")),  _T("S"),  _T("Init/onlyTrim"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("X")),           _T("X"),  _T("Init/single"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("")),            _T(""),   _T("Init/empty"));
    EXPECT_STR(DuiAvatar::ComputeInitials(nullptr),           _T(""),   _T("Init/null"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("\t\n  ")),      _T(""),   _T("Init/onlyWS"));
    return OK(_T("ComputeInitials"));
}

// ----- Status dot color ------------------------------------------------

static Result Test_StatusDotColor()
{
    using A = DuiAvatar;
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusOnline),  (int)RGB( 60,175, 80), _T("Dot/online"));
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusAway),    (int)RGB(240,175, 40), _T("Dot/away"));
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusBusy),    (int)RGB(220, 60, 60), _T("Dot/busy"));
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusOffline), (int)RGB(150,150,150), _T("Dot/offline"));
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusNone),    (int)CLR_INVALID,      _T("Dot/none"));
    return OK(_T("StatusDotColor"));
}

// ----- SetBitmap idempotence ------------------------------------------

static Result Test_SetBitmapStores()
{
    DuiAvatar a;
    HBITMAP fakeOpaque = (HBITMAP)0x1234;   // never dereferenced
    a.SetBitmap(fakeOpaque);
    EXPECT_TRUE(a.GetBitmap() == fakeOpaque, _T("Bm/store"));
    a.SetBitmap(nullptr);
    EXPECT_TRUE(a.GetBitmap() == nullptr, _T("Bm/clear"));
    return OK(_T("SetBitmapStores"));
}

// ----- Paint smoke (no bitmap, with initials) -------------------------

static HBITMAP MakeAvatarSrc()
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 32;
    bi.bmiHeader.biHeight   = -32;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!h)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < 32 * 32; ++i)
    {
        p[i*4 + 0] = 200;     // B
        p[i*4 + 1] = 100;     // G
        p[i*4 + 2] =  50;     // R
        p[i*4 + 3] = 255;     // A
    }
    return h;
}

static HBITMAP MakeDestBitmap(int sz)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = sz;
    bi.bmiHeader.biHeight   = -sz;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!h)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < sz * sz; ++i)
    {
        p[i*4]=255;
        p[i*4+1]=255;
        p[i*4+2]=255;
        p[i*4+3]=255;
    }
    return h;
}

static Result Test_PaintSmoke_Initials()
{
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, 64, 64 });
    a.SetName(_T("Alice Smith"));
    a.SetStatus(DuiAvatar::StatusOnline);

    HBITMAP hbmDst = MakeDestBitmap(64);
    if (!hbmDst)
    {
        return Fail(_T("Smoke/initials"), _T("dest dib failed"));
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hbmDst);

    a.OnPaint(hdc, RECT{ 0, 0, 64, 64 });    // must not crash

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(hbmDst);
    return OK(_T("PaintSmoke_Initials"));
}

static Result Test_PaintSmoke_Bitmap()
{
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, 64, 64 });
    HBITMAP src = MakeAvatarSrc();
    if (!src)
    {
        return Fail(_T("Smoke/bitmapSrc"), _T("src dib failed"));
    }
    a.SetBitmap(src);
    a.SetStatus(DuiAvatar::StatusBusy);
    a.SetShape(DuiAvatar::ShapeRoundRect);
    a.SetCornerRadius(12);

    HBITMAP hbmDst = MakeDestBitmap(64);
    if (!hbmDst)
    {
        ::DeleteObject(src);
        return Fail(_T("Smoke/bitmapDst"), _T("dest dib failed"));
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hbmDst);

    a.OnPaint(hdc, RECT{ 0, 0, 64, 64 });    // must not crash

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(hbmDst);
    ::DeleteObject(src);
    return OK(_T("PaintSmoke_Bitmap"));
}

// Painting into a tiny rect is not expected to crash; corner-radius
// auto-clamps to half the shorter side.
static Result Test_PaintSmoke_TinyRect()
{
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, 6, 6 });
    a.SetShape(DuiAvatar::ShapeRoundRect);
    a.SetCornerRadius(99);   // larger than the rect; expected to clamp
    a.SetStatus(DuiAvatar::StatusOnline);

    HBITMAP hbmDst = MakeDestBitmap(8);
    if (!hbmDst)
    {
        return Fail(_T("Smoke/tinyDst"), _T("dest dib failed"));
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hbmDst);

    a.OnPaint(hdc, RECT{ 0, 0, 6, 6 });

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(hbmDst);
    return OK(_T("PaintSmoke_TinyRect"));
}

#undef EXPECT_INT
#undef EXPECT_STR
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
        { _T("Defaults"),             &Test_Defaults             },
        { _T("SetterRoundTrip"),      &Test_SetterRoundTrip      },
        { _T("ComputeInitials"),      &Test_ComputeInitials      },
        { _T("StatusDotColor"),       &Test_StatusDotColor       },
        { _T("SetBitmapStores"),      &Test_SetBitmapStores      },
        { _T("PaintSmoke_Initials"),  &Test_PaintSmoke_Initials  },
        { _T("PaintSmoke_Bitmap"),    &Test_PaintSmoke_Bitmap    },
        { _T("PaintSmoke_TinyRect"),  &Test_PaintSmoke_TinyRect  },
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
    summary.Format(_T("[summary] DuiAvatarTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiAvatarTests

} // namespace balloonwjui

#endif // BUI_FEATURE_AVATAR
