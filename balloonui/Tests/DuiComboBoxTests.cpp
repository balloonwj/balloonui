#include "stdafx.h"
#include "DuiComboBoxTests.h"

#if BUI_FEATURE_COMBOBOX


namespace balloonwjui {

namespace DuiComboBoxTests {

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
#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_SIZE(actual, expected, name) \
    do { size_t _a = (actual); size_t _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%Iu got=%Iu"), _e, _a); return Fail(name, _d); } \
    } while (0)

// Defaults: incremental search is OFF; both modifiers OFF.
static Result Test_IncDefaults()
{
    DuiComboBox c;
    EXPECT_BOOL(c.GetIncrementalSearch(),                false, _T("Inc/off"));
    EXPECT_BOOL(c.GetIncrementalSearchSubstring(),       false, _T("Inc/prefix"));
    EXPECT_BOOL(c.GetIncrementalSearchCaseSensitive(),   false, _T("Inc/caseIns"));
    return OK(_T("IncDefaults"));
}

// Round-trip the three boolean toggles.
static Result Test_IncToggleRoundTrip()
{
    DuiComboBox c;
    c.SetIncrementalSearch(true);
    EXPECT_BOOL(c.GetIncrementalSearch(), true, _T("RT/on"));
    c.SetIncrementalSearch(false);
    EXPECT_BOOL(c.GetIncrementalSearch(), false, _T("RT/off"));

    c.SetIncrementalSearchSubstring(true);
    EXPECT_BOOL(c.GetIncrementalSearchSubstring(), true, _T("RT/sub"));
    c.SetIncrementalSearchSubstring(false);
    EXPECT_BOOL(c.GetIncrementalSearchSubstring(), false, _T("RT/prefix"));

    c.SetIncrementalSearchCaseSensitive(true);
    EXPECT_BOOL(c.GetIncrementalSearchCaseSensitive(), true, _T("RT/caseSens"));
    c.SetIncrementalSearchCaseSensitive(false);
    EXPECT_BOOL(c.GetIncrementalSearchCaseSensitive(), false, _T("RT/caseIns"));
    return OK(_T("IncToggleRoundTrip"));
}

// Empty (or null) query => all items pass through, in order.
static Result Test_IncEmptyQueryAll()
{
    DuiComboBox c;
    c.AddString(_T("apple"));
    c.AddString(_T("banana"));
    c.AddString(_T("cherry"));

    auto v0 = c.ComputeFilteredIndices(nullptr);
    EXPECT_SIZE(v0.size(), 3, _T("Inc/null/size"));
    EXPECT_INT (v0[0], 0, _T("Inc/null/0"));
    EXPECT_INT (v0[2], 2, _T("Inc/null/2"));

    auto v1 = c.ComputeFilteredIndices(_T(""));
    EXPECT_SIZE(v1.size(), 3, _T("Inc/empty/size"));
    return OK(_T("IncEmptyQueryAll"));
}

// Default: prefix match, case-insensitive.
static Result Test_IncPrefixCaseInsensitive()
{
    DuiComboBox c;
    c.AddString(_T("Alpha"));
    c.AddString(_T("alligator"));
    c.AddString(_T("Bravo"));
    c.AddString(_T("Banana"));
    c.AddString(_T("ALPS"));

    auto v = c.ComputeFilteredIndices(_T("al"));
    // "Alpha", "alligator", "ALPS" -> indices 0, 1, 4.
    EXPECT_SIZE(v.size(), 3,        _T("Pre/size"));
    EXPECT_INT (v[0], 0,            _T("Pre/0"));
    EXPECT_INT (v[1], 1,            _T("Pre/1"));
    EXPECT_INT (v[2], 4,            _T("Pre/2"));

    // No prefix hit.
    auto v2 = c.ComputeFilteredIndices(_T("z"));
    EXPECT_SIZE(v2.size(), 0,       _T("Pre/miss"));
    return OK(_T("IncPrefixCaseInsensitive"));
}

// Case-sensitive prefix: "Al" matches "Alpha" + "ALPS"? No: "ALPS" begins
// with "AL" not "Al". So only "Alpha".
static Result Test_IncPrefixCaseSensitive()
{
    DuiComboBox c;
    c.AddString(_T("Alpha"));
    c.AddString(_T("alligator"));
    c.AddString(_T("ALPS"));
    c.SetIncrementalSearchCaseSensitive(true);

    auto v = c.ComputeFilteredIndices(_T("Al"));
    EXPECT_SIZE(v.size(), 1, _T("PreCS/size"));
    EXPECT_INT (v[0], 0,     _T("PreCS/Alpha"));

    auto v2 = c.ComputeFilteredIndices(_T("al"));
    EXPECT_SIZE(v2.size(), 1, _T("PreCS/lower/size"));
    EXPECT_INT (v2[0], 1,     _T("PreCS/lower/alligator"));
    return OK(_T("IncPrefixCaseSensitive"));
}

// Substring matching (case-insensitive default): "an" hits "banana" and
// "candy" but not "cherry".
static Result Test_IncSubstringCaseInsensitive()
{
    DuiComboBox c;
    c.AddString(_T("apple"));
    c.AddString(_T("Banana"));
    c.AddString(_T("Cherry"));
    c.AddString(_T("candy"));
    c.SetIncrementalSearchSubstring(true);

    auto v = c.ComputeFilteredIndices(_T("an"));
    EXPECT_SIZE(v.size(), 2, _T("Sub/size"));
    EXPECT_INT (v[0], 1,     _T("Sub/banana"));
    EXPECT_INT (v[1], 3,     _T("Sub/candy"));
    return OK(_T("IncSubstringCaseInsensitive"));
}

// Substring + case-sensitive: "An" appears in nothing; "an" in "banana"
// and "candy".
static Result Test_IncSubstringCaseSensitive()
{
    DuiComboBox c;
    c.AddString(_T("Banana"));
    c.AddString(_T("candy"));
    c.SetIncrementalSearchSubstring(true);
    c.SetIncrementalSearchCaseSensitive(true);

    auto v1 = c.ComputeFilteredIndices(_T("An"));
    EXPECT_SIZE(v1.size(), 0, _T("SubCS/An/miss"));
    auto v2 = c.ComputeFilteredIndices(_T("an"));
    EXPECT_SIZE(v2.size(), 2, _T("SubCS/an/hit"));
    return OK(_T("IncSubstringCaseSensitive"));
}

// Filter against an empty item list: empty result, no crash.
static Result Test_IncEmptyItemList()
{
    DuiComboBox c;
    auto v = c.ComputeFilteredIndices(_T("anything"));
    EXPECT_SIZE(v.size(), 0, _T("Inc/empty"));
    return OK(_T("IncEmptyItemList"));
}

// SetIncrementalSearch(false) clears any cached filter so subsequent
// OpenPopup paths don't accidentally see stale state. We can't see the
// internal vector directly, but we can confirm the toggle round-trip
// stays clean across many flips.
static Result Test_IncToggleResets()
{
    DuiComboBox c;
    c.AddString(_T("alpha"));
    c.AddString(_T("beta"));
    c.SetIncrementalSearch(true);
    EXPECT_BOOL(c.GetIncrementalSearch(), true, _T("Reset/on1"));
    c.SetIncrementalSearch(false);
    EXPECT_BOOL(c.GetIncrementalSearch(), false, _T("Reset/off1"));
    c.SetIncrementalSearch(true);
    EXPECT_BOOL(c.GetIncrementalSearch(), true, _T("Reset/on2"));
    c.SetIncrementalSearch(true);   // idempotent
    EXPECT_BOOL(c.GetIncrementalSearch(), true, _T("Reset/idem"));
    return OK(_T("IncToggleResets"));
}

#undef EXPECT_INT
#undef EXPECT_BOOL
#undef EXPECT_SIZE

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("IncDefaults"),               &Test_IncDefaults               },
        { _T("IncToggleRoundTrip"),        &Test_IncToggleRoundTrip        },
        { _T("IncEmptyQueryAll"),          &Test_IncEmptyQueryAll          },
        { _T("IncPrefixCaseInsensitive"),  &Test_IncPrefixCaseInsensitive  },
        { _T("IncPrefixCaseSensitive"),    &Test_IncPrefixCaseSensitive    },
        { _T("IncSubstringCaseInsensitive"),&Test_IncSubstringCaseInsensitive },
        { _T("IncSubstringCaseSensitive"), &Test_IncSubstringCaseSensitive },
        { _T("IncEmptyItemList"),          &Test_IncEmptyItemList          },
        { _T("IncToggleResets"),           &Test_IncToggleResets           }
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
    summary.Format(_T("[summary] DuiComboBoxTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiComboBoxTests

} // namespace balloonwjui

#endif // BUI_FEATURE_COMBOBOX
