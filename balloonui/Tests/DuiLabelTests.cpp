#include "stdafx.h"
#include "DuiLabelTests.h"

#if BUI_FEATURE_LABEL


namespace balloonwjui {

namespace DuiLabelTests {

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

#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)

// Default mode is text, not focusable, not visited.
static Result Test_Defaults()
{
    DuiLabel l;
    EXPECT_INT((int)l.GetMode(), (int)DuiLabel::ModeText, _T("Defaults/mode"));
    EXPECT_BOOL(l.IsTabStop(), false,                      _T("Defaults/tabStop"));
    EXPECT_BOOL(l.IsVisited(), false,                      _T("Defaults/visited"));
    return OK(_T("Defaults"));
}

// SetMode flips tab-stop only for link mode.
static Result Test_ModeAffectsTabStop()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    EXPECT_BOOL(l.IsTabStop(), true,  _T("Mode/linkTabStop"));
    l.SetMode(DuiLabel::ModeText);
    EXPECT_BOOL(l.IsTabStop(), false, _T("Mode/textTabStop"));
    return OK(_T("ModeAffectsTabStop"));
}

// In text mode, click is ignored (returns false, no visited flip, no notify).
static Result Test_TextClickIgnored()
{
    DuiLabel l;
    l.SetRect(RECT{ 0, 0, 100, 20 });
    bool consumed = l.OnLButtonUp(POINT{ 50, 10 }, 0);
    EXPECT_BOOL(consumed, false,        _T("TextClick/notConsumed"));
    EXPECT_BOOL(l.IsVisited(), false,   _T("TextClick/notVisited"));
    return OK(_T("TextClickIgnored"));
}

// In link mode, click inside flips visited and consumes the event.
static Result Test_LinkClickInside()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    l.SetRect(RECT{ 0, 0, 100, 20 });
    bool consumed = l.OnLButtonUp(POINT{ 50, 10 }, 0);
    EXPECT_BOOL(consumed, true,        _T("LinkClick/consumed"));
    EXPECT_BOOL(l.IsVisited(), true,   _T("LinkClick/visited"));
    return OK(_T("LinkClickInside"));
}

// Link click outside the rect doesn't flip visited or consume.
static Result Test_LinkClickOutside()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    l.SetRect(RECT{ 0, 0, 100, 20 });
    bool consumed = l.OnLButtonUp(POINT{ 999, 999 }, 0);
    EXPECT_BOOL(consumed, false,       _T("LinkClick/outsideNotConsumed"));
    EXPECT_BOOL(l.IsVisited(), false,  _T("LinkClick/outsideNotVisited"));
    return OK(_T("LinkClickOutside"));
}

// Disabled link doesn't react.
static Result Test_LinkDisabledIgnored()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    l.SetEnabled(false);
    l.SetRect(RECT{ 0, 0, 100, 20 });
    bool consumed = l.OnLButtonUp(POINT{ 50, 10 }, 0);
    EXPECT_BOOL(consumed, false,       _T("LinkDisabled/notConsumed"));
    EXPECT_BOOL(l.IsVisited(), false,  _T("LinkDisabled/notVisited"));
    return OK(_T("LinkDisabledIgnored"));
}

// SetVisited programmatic.
static Result Test_SetVisitedAPI()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    l.SetVisited(true);
    EXPECT_BOOL(l.IsVisited(), true,  _T("SetVisited/true"));
    l.SetVisited(false);
    EXPECT_BOOL(l.IsVisited(), false, _T("SetVisited/false"));
    return OK(_T("SetVisitedAPI"));
}

// Mode round-trip preserves text.
static Result Test_ModeSwitchRetainsText()
{
    DuiLabel l;
    l.SetText(_T("hello"));
    l.SetMode(DuiLabel::ModeLink);
    l.SetMode(DuiLabel::ModeText);
    if (l.GetText() != _T("hello"))
    {
        return Fail(_T("ModeSwitchRetainsText"), _T("text lost"));
    }
    return OK(_T("ModeSwitchRetainsText"));
}

// AutoNavigate flag round-trip and URL storage.
static Result Test_UrlAndAutoNav()
{
    DuiLabel l;
    l.SetUrl(_T("http://example.com"));
    l.SetAutoNavigate(true);
    if (l.GetUrl() != _T("http://example.com"))
    {
        return Fail(_T("UrlAndAutoNav/url"), _T("url roundtrip"));
    }
    EXPECT_BOOL(l.GetAutoNavigate(), true, _T("UrlAndAutoNav/flag"));
    return OK(_T("UrlAndAutoNav"));
}

// Word-wrap default OFF; toggle round-trips.
static Result Test_WrapDefaultOff()
{
    DuiLabel l;
    EXPECT_BOOL(l.IsWordWrap(), false, _T("Wrap/default"));
    l.SetWordWrap(true);
    EXPECT_BOOL(l.IsWordWrap(), true,  _T("Wrap/on"));
    l.SetWordWrap(false);
    EXPECT_BOOL(l.IsWordWrap(), false, _T("Wrap/off"));
    return OK(_T("WrapDefaultOff"));
}

// MeasureHeight on empty text is 0.
static Result Test_MeasureEmpty()
{
    DuiLabel l;
    EXPECT_INT(l.MeasureHeight(100), 0, _T("Measure/empty"));
    return OK(_T("MeasureEmpty"));
}

// MeasureHeight single-line: positive height; matches single-line height
// regardless of width because wrap is off.
static Result Test_MeasureSingleLine()
{
    DuiLabel l;
    l.SetText(_T("Hello world"));
    int h100 = l.MeasureHeight(100);
    int h500 = l.MeasureHeight(500);
    if (h100 <= 0)
    {
        return Fail(_T("MeasureSingleLine"), _T("h100<=0"));
    }
    if (h500 != h100)
    {
        CString d;
        d.Format(_T("h100=%d h500=%d (single-line should not change)"), h100, h500);
        return Fail(_T("MeasureSingleLine"), d);
    }
    return OK(_T("MeasureSingleLine"));
}

// MeasureHeight multi-line wrap: narrowing the width grows the height
// because the text wraps onto more lines.
static Result Test_MeasureWrapNarrows()
{
    DuiLabel l;
    l.SetText(_T("Hello world this is a long sentence that should wrap"
                 " across several lines when the width is narrow."));
    l.SetWordWrap(true);

    int hWide = l.MeasureHeight(800);
    int hNarrow = l.MeasureHeight(80);
    if (hWide <= 0 || hNarrow <= 0)
    {
        return Fail(_T("MeasureWrap"), _T("non-positive heights"));
    }
    if (!(hNarrow > hWide))
    {
        CString d;
        d.Format(_T("expected narrow > wide; wide=%d narrow=%d"),
                 hWide, hNarrow);
        return Fail(_T("MeasureWrap"), d);
    }
    return OK(_T("MeasureWrapNarrows"));
}

// MeasureHeight with embedded explicit \n line breaks: even in
// non-wrap mode, multi-line text via "\n" should produce a height
// proportional to line count when wrap is on.
static Result Test_MeasureExplicitNewlines()
{
    DuiLabel l;
    l.SetText(_T("Line 1\nLine 2\nLine 3"));
    l.SetWordWrap(true);

    int h = l.MeasureHeight(800);
    if (h <= 0)
    {
        return Fail(_T("MeasureNewlines"), _T("h<=0"));
    }

    DuiLabel one;
    one.SetText(_T("Line 1"));
    one.SetWordWrap(true);
    int h1 = one.MeasureHeight(800);

    if (h1 <= 0)
    {
        return Fail(_T("MeasureNewlines"), _T("h1<=0"));
    }
    // 3 lines should be ~3x the single-line height. Allow some slop.
    if (h < h1 * 2)
    {
        CString d;
        d.Format(_T("expected ~3x; h=%d h1=%d"), h, h1);
        return Fail(_T("MeasureNewlines"), d);
    }
    return OK(_T("MeasureExplicitNewlines"));
}

// MeasureHeight with width <= 0 falls back to "natural width" (no
// wrapping constraint). Returns the single-line height.
static Result Test_MeasureWidthZero()
{
    DuiLabel l;
    l.SetText(_T("a one-line string"));
    int h0 = l.MeasureHeight(0);
    int hN = l.MeasureHeight(-5);
    int hW = l.MeasureHeight(800);
    if (h0 <= 0)
    {
        return Fail(_T("MeasureW0"), _T("h0<=0"));
    }
    if (h0 != hN)
    {
        return Fail(_T("MeasureW0"), _T("h0 != hN(<0)"));
    }
    if (h0 != hW)
    {
        CString d;
        d.Format(_T("h0=%d hW=%d"), h0, hW);
        return Fail(_T("MeasureW0"), d);
    }
    return OK(_T("MeasureWidthZero"));
}

// Wrap mode preserves text content (sanity).
static Result Test_WrapPreservesText()
{
    DuiLabel l;
    l.SetText(_T("alpha beta gamma"));
    l.SetWordWrap(true);
    if (l.GetText() != _T("alpha beta gamma"))
    {
        return Fail(_T("WrapPreservesText"), _T("text lost"));
    }
    return OK(_T("WrapPreservesText"));
}

// SetTextColor / SetTextAlign round-trip.
static Result Test_StyleSetters()
{
    DuiLabel l;
    l.SetTextColor(RGB(1, 2, 3));
    if (l.GetTextColor() != RGB(1, 2, 3))
    {
        return Fail(_T("Style/color"), _T("color roundtrip"));
    }
    l.SetTextAlign(DT_RIGHT);
    if (l.GetTextAlign() != (DWORD)DT_RIGHT)
    {
        return Fail(_T("Style/align"), _T("align roundtrip"));
    }
    return OK(_T("StyleSetters"));
}

#undef EXPECT_BOOL
#undef EXPECT_INT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),               &Test_Defaults              },
        { _T("ModeAffectsTabStop"),     &Test_ModeAffectsTabStop    },
        { _T("TextClickIgnored"),       &Test_TextClickIgnored      },
        { _T("LinkClickInside"),        &Test_LinkClickInside       },
        { _T("LinkClickOutside"),       &Test_LinkClickOutside      },
        { _T("LinkDisabledIgnored"),    &Test_LinkDisabledIgnored   },
        { _T("SetVisitedAPI"),          &Test_SetVisitedAPI         },
        { _T("ModeSwitchRetainsText"),  &Test_ModeSwitchRetainsText },
        { _T("UrlAndAutoNav"),          &Test_UrlAndAutoNav         },
        { _T("StyleSetters"),           &Test_StyleSetters          },
        { _T("WrapDefaultOff"),         &Test_WrapDefaultOff        },
        { _T("MeasureEmpty"),           &Test_MeasureEmpty          },
        { _T("MeasureSingleLine"),      &Test_MeasureSingleLine     },
        { _T("MeasureWrapNarrows"),     &Test_MeasureWrapNarrows    },
        { _T("MeasureExplicitNewlines"),&Test_MeasureExplicitNewlines },
        { _T("MeasureWidthZero"),       &Test_MeasureWidthZero      },
        { _T("WrapPreservesText"),      &Test_WrapPreservesText     }
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
    summary.Format(_T("[summary] DuiLabelTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiLabelTests

} // namespace balloonwjui

#endif // BUI_FEATURE_LABEL
