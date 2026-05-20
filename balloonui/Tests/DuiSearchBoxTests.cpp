#include "stdafx.h"
#include "DuiSearchBoxTests.h"

#if BUI_FEATURE_SEARCHBOX

namespace balloonwjui {

namespace DuiSearchBoxTests {

namespace {

struct Result
{
    CString name;
    bool    ok;
    CString detail;
};

static Result OK(const CString& n)   { Result r; r.name = n; r.ok = true; return r; }
static Result Fail(const CString& n, const CString& d) { Result r; r.name = n; r.ok = false; r.detail = d; return r; }

#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)

// ctor 装好左 magnifier 图标，右 × 默认不显示（文字空）
static Result Test_DefaultsLeftInstalledRightHidden()
{
    DuiSearchBox sb;
    // 左 gutter 宽 24（kDefaultGlyphW）
    EXPECT_INT(sb.GetGlyphStripWidth(), 24, _T("def/leftW"));
    // 左 icon 装饰 —— 不可点击
    EXPECT_BOOL(sb.IsIconClickable(DuiSearchBox::LeftIcon), false,
                _T("def/leftClickable"));
    // 右 × 默认不显示（文字空）
    EXPECT_BOOL(sb.IsClearShowing(), false, _T("def/rightHidden"));
    EXPECT_INT(sb.GetIconWidth(DuiSearchBox::RightIcon), 0,
               _T("def/rightW0"));
    return OK(_T("DefaultsLeftInstalledRightHidden"));
}

// SetGlyphStripWidth round-trip 通过 GetIconWidth(LeftIcon)
static Result Test_SetGlyphStripWidth()
{
    DuiSearchBox sb;
    sb.SetGlyphStripWidth(32);
    EXPECT_INT(sb.GetGlyphStripWidth(), 32, _T("glyph/32"));
    sb.SetGlyphStripWidth(0);
    EXPECT_INT(sb.GetGlyphStripWidth(), 0, _T("glyph/0"));
    return OK(_T("SetGlyphStripWidth"));
}

// SetClearStripWidth clamp >= 14
static Result Test_SetClearStripWidthClamp()
{
    DuiSearchBox sb;
    sb.SetClearStripWidth(8);
    EXPECT_INT(sb.GetClearStripWidth(), 14, _T("clear/clamp"));
    sb.SetClearStripWidth(30);
    EXPECT_INT(sb.GetClearStripWidth(), 30, _T("clear/30"));
    return OK(_T("SetClearStripWidthClamp"));
}

// 直接 Test_SetTextDirect 灌缓存模拟"文字非空" —— 此时 IsClearShowing
// 还没改，因为 SyncClear_ 是私有 + 通过 EN_CHANGE 触发的。本测试的
// 价值是确认 GetClearRect / IsClearShowing 在 RightIcon width=0 时确
// 实返回 empty/false（防御性）
static Result Test_ClearRectWhenHidden()
{
    DuiSearchBox sb;
    EXPECT_BOOL(sb.IsClearShowing(), false, _T("clear/hidden"));
    RECT r = sb.GetClearRect();
    EXPECT_INT(r.left,   0, _T("clearRect/L"));
    EXPECT_INT(r.right,  0, _T("clearRect/R"));
    EXPECT_INT(r.top,    0, _T("clearRect/T"));
    EXPECT_INT(r.bottom, 0, _T("clearRect/B"));
    return OK(_T("ClearRectWhenHidden"));
}

// GetEdit() 现在 = this（重构后本类即 EDIT，向后兼容）
static Result Test_GetEditReturnsSelf()
{
    DuiSearchBox sb;
    EXPECT_BOOL(sb.GetEdit() == &sb, true, _T("getEdit/self"));
    return OK(_T("GetEditReturnsSelf"));
}

#undef EXPECT_BOOL
#undef EXPECT_INT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("DefaultsLeftInstalledRightHidden"),
                                       &Test_DefaultsLeftInstalledRightHidden },
        { _T("SetGlyphStripWidth"),    &Test_SetGlyphStripWidth    },
        { _T("SetClearStripWidthClamp"), &Test_SetClearStripWidthClamp },
        { _T("ClearRectWhenHidden"),   &Test_ClearRectWhenHidden   },
        { _T("GetEditReturnsSelf"),    &Test_GetEditReturnsSelf    },
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
        if (!out.IsEmpty()) { out += _T("\r\n"); }
        out += line;
    }
    CString summary;
    summary.Format(_T("[summary] DuiSearchBoxTests passed=%d failed=%d"),
                   passed, failed);
    if (!out.IsEmpty()) { out += _T("\r\n"); }
    out += summary;
    return out;
}

} // namespace DuiSearchBoxTests

} // namespace balloonwjui

#endif // BUI_FEATURE_SEARCHBOX
