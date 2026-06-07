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

// ---- 下拉箭头颜色(AA 抗锯齿 + 颜色可配)---------------------------------

// 默认箭头色 = kArrowEnabled = RGB(80, 100, 140)。
static Result Test_ArrowColorDefault()
{
    DuiComboBox c;
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(80, 100, 140), _T("Arr/def"));
    return OK(_T("ArrowColorDefault"));
}

// SetArrowColor / GetArrowColor 往返;切几个典型色后 getter 一致。
static Result Test_ArrowColorRoundTrip()
{
    DuiComboBox c;
    c.SetArrowColor(RGB(45, 108, 223));         // 品牌蓝
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(45, 108, 223),  _T("Arr/blue"));

    c.SetArrowColor(RGB(220, 60, 60));          // 红
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(220, 60, 60),   _T("Arr/red"));

    c.SetArrowColor(RGB(0, 0, 0));              // 纯黑
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(0, 0, 0),       _T("Arr/black"));

    c.SetArrowColor(RGB(80, 100, 140));         // 回默认
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(80, 100, 140),  _T("Arr/back_def"));
    return OK(_T("ArrowColorRoundTrip"));
}

// 与 ShowArrow / BgColor / ShowBorder 等其它属性正交,互不影响。
static Result Test_ArrowColorOrthogonal()
{
    DuiComboBox c;
    c.SetBgColor(RGB(245, 246, 248));
    c.SetShowBorder(false);
    c.SetShowArrow(true);
    c.SetArrowColor(RGB(45, 108, 223));

    EXPECT_INT((int)c.GetBgColor(),    (int)RGB(245, 246, 248), _T("Orth/bg"));
    EXPECT_BOOL(c.IsShowBorder(),       false,                   _T("Orth/border"));
    EXPECT_BOOL(c.IsShowArrow(),        true,                    _T("Orth/showArrow"));
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(45, 108, 223),  _T("Orth/arrow"));

    // 切 ShowArrow=false 不应清掉 ArrowColor。
    c.SetShowArrow(false);
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(45, 108, 223),  _T("Orth/arrow_after_hide"));
    return OK(_T("ArrowColorOrthogonal"));
}

// 与 SetEditable / Items / CurSel 等业务状态共存:setter 不应破坏数据 model。
static Result Test_ArrowColorWithDataModel()
{
    DuiComboBox c;
    c.AddString(_T("Foo"));
    c.AddString(_T("Bar"));
    c.SetCurSel(1, /*notify=*/false);
    c.SetArrowColor(RGB(220, 60, 60));

    EXPECT_INT(c.GetCount(),                            2,  _T("Data/count"));
    EXPECT_INT(c.GetCurSel(),                           1,  _T("Data/sel"));
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(220, 60, 60), _T("Data/arrow"));

    // 反向:换数据 / 改选不影响 arrow color。
    c.AddString(_T("Baz"));
    c.SetCurSel(2, /*notify=*/false);
    EXPECT_INT(c.GetCount(),                            3,  _T("Data/countAfter"));
    EXPECT_INT(c.GetCurSel(),                           2,  _T("Data/selAfter"));
    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(220, 60, 60), _T("Data/arrowAfter"));
    return OK(_T("ArrowColorWithDataModel"));
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
        { _T("IncToggleResets"),           &Test_IncToggleResets           },
        // ---- 下拉箭头颜色 ----
        { _T("ArrowColorDefault"),         &Test_ArrowColorDefault         },
        { _T("ArrowColorRoundTrip"),       &Test_ArrowColorRoundTrip       },
        { _T("ArrowColorOrthogonal"),      &Test_ArrowColorOrthogonal      },
        { _T("ArrowColorWithDataModel"),   &Test_ArrowColorWithDataModel   }
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
