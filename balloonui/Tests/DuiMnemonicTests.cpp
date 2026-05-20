#include "stdafx.h"
#include "DuiMnemonicTests.h"

#if BUI_FEATURE_BUTTON && BUI_FEATURE_LABEL

#include "../Controls/Basic/DuiButton.h"
#include "../Controls/Basic/DuiLabel.h"

namespace balloonwjui {

namespace DuiMnemonicTests {

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
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) return Fail(name, _T("string mismatch")); \
    } while (0)

// ----- FindChar ------------------------------------------------------

static Result Test_FindCharBasic()
{
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("&Save")),       (int)_T('s'), _T("Fc/Save"));
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("Save &As")),    (int)_T('a'), _T("Fc/As"));
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("plain")),       (int)0,        _T("Fc/none"));
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("")),            (int)0,        _T("Fc/empty"));
    EXPECT_INT((int)DuiMnemonic::FindChar(nullptr),           (int)0,        _T("Fc/null"));
    return OK(_T("FindCharBasic"));
}

static Result Test_FindCharEscapes()
{
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("Save && Quit")),(int)0,        _T("Fc/escapeOnly"));
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("&&&Foo")),      (int)_T('f'), _T("Fc/doubleThenReal"));
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("trailing&")),   (int)0,        _T("Fc/trailing"));
    return OK(_T("FindCharEscapes"));
}

static Result Test_FindCharCaseInsensitive()
{
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("&X")), (int)_T('x'), _T("Fc/upper"));
    EXPECT_INT((int)DuiMnemonic::FindChar(_T("&z")), (int)_T('z'), _T("Fc/lower"));
    return OK(_T("FindCharCaseInsensitive"));
}

// ----- StripPrefix ---------------------------------------------------

static Result Test_StripPrefixBasic()
{
    EXPECT_STR(DuiMnemonic::StripPrefix(_T("&Save")),       _T("Save"),     _T("Sp/Save"));
    EXPECT_STR(DuiMnemonic::StripPrefix(_T("Save &As")),    _T("Save As"),  _T("Sp/As"));
    EXPECT_STR(DuiMnemonic::StripPrefix(_T("plain")),       _T("plain"),    _T("Sp/plain"));
    EXPECT_STR(DuiMnemonic::StripPrefix(_T("")),            _T(""),         _T("Sp/empty"));
    EXPECT_STR(DuiMnemonic::StripPrefix(nullptr),           _T(""),         _T("Sp/null"));
    return OK(_T("StripPrefixBasic"));
}

// "&&" collapses to a single literal "&" in the output.
static Result Test_StripPrefixEscapes()
{
    EXPECT_STR(DuiMnemonic::StripPrefix(_T("A && B")),      _T("A & B"),    _T("Sp/escAmp"));
    EXPECT_STR(DuiMnemonic::StripPrefix(_T("&&&Foo")),      _T("&Foo"),     _T("Sp/doubleThenReal"));
    EXPECT_STR(DuiMnemonic::StripPrefix(_T("trailing&")),   _T("trailing"), _T("Sp/trailing"));
    return OK(_T("StripPrefixEscapes"));
}

// ----- Wired into DuiButton / DuiLabel -------------------------------

static Result Test_ButtonExposesMnemonic()
{
    DuiButton b;
    b.SetText(_T("&OK"));
    EXPECT_INT((int)b.GetMnemonicChar(), (int)_T('o'), _T("Btn/OK"));
    b.SetText(_T("Cancel"));
    EXPECT_INT((int)b.GetMnemonicChar(), (int)0,        _T("Btn/none"));
    return OK(_T("ButtonExposesMnemonic"));
}

static Result Test_LabelExposesMnemonic()
{
    DuiLabel l;
    l.SetText(_T("&Filename:"));
    EXPECT_INT((int)l.GetMnemonicChar(), (int)_T('f'), _T("Lbl/F"));
    l.SetText(_T("plain"));
    EXPECT_INT((int)l.GetMnemonicChar(), (int)0,        _T("Lbl/none"));
    return OK(_T("LabelExposesMnemonic"));
}

#undef EXPECT_INT
#undef EXPECT_STR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("FindCharBasic"),           &Test_FindCharBasic           },
        { _T("FindCharEscapes"),         &Test_FindCharEscapes         },
        { _T("FindCharCaseInsensitive"), &Test_FindCharCaseInsensitive },
        { _T("StripPrefixBasic"),        &Test_StripPrefixBasic        },
        { _T("StripPrefixEscapes"),      &Test_StripPrefixEscapes      },
        { _T("ButtonExposesMnemonic"),   &Test_ButtonExposesMnemonic   },
        { _T("LabelExposesMnemonic"),    &Test_LabelExposesMnemonic    },
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
    summary.Format(_T("[summary] DuiMnemonicTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiMnemonicTests

} // namespace balloonwjui

#endif // BUI_FEATURE_BUTTON && BUI_FEATURE_LABEL
