#include "stdafx.h"
#include "DuiToolTipTests.h"

#if BUI_FEATURE_TOOLTIP

#include "../DuiControl.h"
#include "../DuiResMgr.h"

namespace balloonwjui {

namespace DuiToolTipTests {

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

#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("got '%s'"), (LPCTSTR)_a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)

class StubCtrl : public DuiControl
{
public:
    void OnPaint(HDC, const RECT&) override {}
};

// Default empty registry returns empty.
static Result Test_Defaults()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    EXPECT_STR(m.GetText(&c), _T(""), _T("Defaults/empty"));
    EXPECT_BOOL(m.IsShowing(), false, _T("Defaults/notShowing"));
    return OK(_T("Defaults"));
}

// Register / overwrite / Unregister round trip.
static Result Test_RegisterRoundTrip()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    m.Register(&c, _T("hello"));
    EXPECT_STR(m.GetText(&c), _T("hello"), _T("Reg/initial"));
    m.Register(&c, _T("world"));   // overwrite
    EXPECT_STR(m.GetText(&c), _T("world"), _T("Reg/overwrite"));
    m.Unregister(&c);
    EXPECT_STR(m.GetText(&c), _T(""), _T("Reg/afterUnreg"));
    return OK(_T("RegisterRoundTrip"));
}

// Independent entries don't collide.
static Result Test_MultipleControls()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl a, b, c;
    m.Register(&a, _T("A"));
    m.Register(&b, _T("B"));
    m.Register(&c, _T("C"));
    EXPECT_STR(m.GetText(&a), _T("A"), _T("Multi/a"));
    EXPECT_STR(m.GetText(&b), _T("B"), _T("Multi/b"));
    EXPECT_STR(m.GetText(&c), _T("C"), _T("Multi/c"));
    m.Unregister(&b);
    EXPECT_STR(m.GetText(&b), _T(""),  _T("Multi/bGone"));
    EXPECT_STR(m.GetText(&a), _T("A"), _T("Multi/aKeep"));
    EXPECT_STR(m.GetText(&c), _T("C"), _T("Multi/cKeep"));
    m.Unregister(&a);
    m.Unregister(&c);
    return OK(_T("MultipleControls"));
}

// Registering an unrelated text doesn't affect the showing-text query.
static Result Test_NotShowingByDefault()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    m.Register(&c, _T("zzz"));
    EXPECT_BOOL(m.IsShowing(), false, _T("NoShow/initial"));
    EXPECT_STR(m.GetShowingText(), _T(""), _T("NoShow/empty"));
    m.Unregister(&c);
    return OK(_T("NotShowingByDefault"));
}

// Unregister a control we never registered is safe.
static Result Test_UnregisterNoop()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    m.Unregister(&c);  // never registered - safe
    return OK(_T("UnregisterNoop"));
}

// Null arg is safe.
static Result Test_NullSafe()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    m.Register(nullptr, _T("ignored"));
    m.Unregister(nullptr);
    EXPECT_STR(m.GetText(nullptr), _T(""), _T("Null/getText"));
    return OK(_T("NullSafe"));
}

// SetDelay round-trip.
static Result Test_DelayRoundTrip()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    UINT old = m.GetDelay();
    m.SetDelay(123);
    if (m.GetDelay() != 123)
    {
        return Fail(_T("Delay/set123"), _T("not set"));
    }
    m.SetDelay(old);
    return OK(_T("DelayRoundTrip"));
}

// OnLeave for a control with no pending tooltip is safe.
static Result Test_LeaveBeforeHover()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    m.OnLeave(&c);
    EXPECT_BOOL(m.IsShowing(), false, _T("Leave/notShowing"));
    return OK(_T("LeaveBeforeHover"));
}

// Text with a '\n' is laid out as multiple lines; text without one keeps
// the original single-line flags.
static Result Test_DrawFlagsSingleVsMulti()
{
    const UINT single = DuiToolTipMgr::TextDrawFlags(_T("one line"));
    const UINT multi  = DuiToolTipMgr::TextDrawFlags(_T("line one\nline two"));
    EXPECT_BOOL((single & DT_SINGLELINE) != 0, true, _T("DrawFlags/single"));
    EXPECT_BOOL((multi & DT_SINGLELINE) != 0, false, _T("DrawFlags/multi"));
    EXPECT_BOOL((multi & DT_LEFT) == DT_LEFT && (multi & DT_TOP) == DT_TOP, true, _T("DrawFlags/align"));
    return OK(_T("DrawFlagsSingleVsMulti"));
}

// A two-line tip is as wide as its longest line and twice as tall as one
// line; a single-line tip measures the same as GetTextExtentPoint32.
static Result Test_MeasureMultiLine()
{
    HDC hdc = ::GetDC(nullptr);
    HFONT font = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = font ? (HFONT)::SelectObject(hdc, font) : nullptr;

    const CString shortLine = _T("ab");
    const CString longLine  = _T("a much longer second line");
    SIZE expectShort = { 0, 0 };
    SIZE expectLong  = { 0, 0 };
    ::GetTextExtentPoint32(hdc, shortLine, shortLine.GetLength(), &expectShort);
    ::GetTextExtentPoint32(hdc, longLine, longLine.GetLength(), &expectLong);
    const SIZE single = DuiToolTipMgr::MeasureTextExtent(hdc, shortLine);
    const SIZE multi  = DuiToolTipMgr::MeasureTextExtent(hdc, shortLine + _T("\n") + longLine);
    const SIZE empty  = DuiToolTipMgr::MeasureTextExtent(hdc, CString());

    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
    ::ReleaseDC(nullptr, hdc);

    CString d;
    if (single.cx != expectShort.cx || single.cy != expectShort.cy)
    {
        d.Format(_T("single got %dx%d expected %dx%d"), single.cx, single.cy, expectShort.cx, expectShort.cy);
        return Fail(_T("MeasureMultiLine"), d);
    }
    if (multi.cx != expectLong.cx || multi.cy != expectShort.cy * 2)
    {
        d.Format(_T("multi got %dx%d expected %dx%d"), multi.cx, multi.cy, expectLong.cx, expectShort.cy * 2);
        return Fail(_T("MeasureMultiLine"), d);
    }
    if (empty.cx != 0 || empty.cy != 0)
    {
        return Fail(_T("MeasureMultiLine"), _T("empty text should measure 0x0"));
    }
    return OK(_T("MeasureMultiLine"));
}

#undef EXPECT_STR
#undef EXPECT_BOOL

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),               &Test_Defaults              },
        { _T("RegisterRoundTrip"),      &Test_RegisterRoundTrip     },
        { _T("MultipleControls"),       &Test_MultipleControls      },
        { _T("NotShowingByDefault"),    &Test_NotShowingByDefault   },
        { _T("UnregisterNoop"),         &Test_UnregisterNoop        },
        { _T("NullSafe"),               &Test_NullSafe              },
        { _T("DelayRoundTrip"),         &Test_DelayRoundTrip        },
        { _T("LeaveBeforeHover"),       &Test_LeaveBeforeHover      },
        { _T("DrawFlagsSingleVsMulti"), &Test_DrawFlagsSingleVsMulti },
        { _T("MeasureMultiLine"),       &Test_MeasureMultiLine      }
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
    summary.Format(_T("[summary] DuiToolTipTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiToolTipTests

} // namespace balloonwjui

#endif // BUI_FEATURE_TOOLTIP
