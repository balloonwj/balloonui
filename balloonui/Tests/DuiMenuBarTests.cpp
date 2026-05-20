#include "stdafx.h"
#include "DuiMenuBarTests.h"

#if BUI_FEATURE_MENUBAR

namespace balloonwjui {

namespace DuiMenuBarTests {

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
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("got '%s'"), (LPCTSTR)_a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_PTR(actual, expected, name) \
    do { const void* _a = (const void*)(actual); const void* _e = (const void*)(expected); \
         if (_a != _e) { return Fail(name, _T("ptr mismatch")); } \
    } while (0)

// =============================================================================
// 纯数据 / builder 路径单测。OnPaint / OpenAndDriveDropdown / OnLButtonDown
// 等依赖 host 的路径不在这里测 —— 那些靠 DuiGallery 手动验证清单覆盖。
// =============================================================================

static Result Test_Defaults()
{
    DuiMenuBar bar;
    EXPECT_INT(bar.GetItemCount(),   0,  _T("Defaults/empty"));
    EXPECT_INT(bar.GetActiveIndex(), -1, _T("Defaults/active"));
    EXPECT_INT(bar.GetItemHeight(),  24, _T("Defaults/itemH"));
    return OK(_T("Defaults"));
}

static Result Test_AppendReturnsIndex()
{
    DuiMenuBar bar;
    EXPECT_INT(bar.AppendItem(101, _T("&File"),    nullptr), 0, _T("Idx/0"));
    EXPECT_INT(bar.AppendItem(102, _T("&Options"), nullptr), 1, _T("Idx/1"));
    EXPECT_INT(bar.AppendItem(103, _T("&View"),    nullptr), 2, _T("Idx/2"));
    EXPECT_INT(bar.GetItemCount(), 3, _T("Idx/total"));
    return OK(_T("AppendReturnsIndex"));
}

static Result Test_AppendStores()
{
    // Use opaque pointers — DuiMenuBar only stores them, never dereferences
    // in builder code. Same trick DuiMenuTests::Test_IconRoundTrip uses.
    DuiMenu* fakeA = reinterpret_cast<DuiMenu*>(0x11110000);
    DuiMenu* fakeB = reinterpret_cast<DuiMenu*>(0x22220000);

    DuiMenuBar bar;
    bar.AppendItem(101, _T("&File"),    fakeA);
    bar.AppendItem(102, _T("&Options"), fakeB);

    auto i0 = bar.GetItem(0);
    EXPECT_INT ((int)i0.nID,     101,           _T("Stores/0/id"));
    EXPECT_STR (i0.text,         _T("&File"),   _T("Stores/0/text"));
    EXPECT_PTR (i0.dropdown,     fakeA,         _T("Stores/0/dropdown"));
    EXPECT_BOOL(i0.enabled,      true,          _T("Stores/0/enabled"));

    auto i1 = bar.GetItem(1);
    EXPECT_INT ((int)i1.nID,     102,           _T("Stores/1/id"));
    EXPECT_STR (i1.text,         _T("&Options"),_T("Stores/1/text"));
    EXPECT_PTR (i1.dropdown,     fakeB,         _T("Stores/1/dropdown"));
    return OK(_T("AppendStores"));
}

static Result Test_OOBSafe()
{
    DuiMenuBar bar;
    auto empty = bar.GetItem(0);                  // 越界
    EXPECT_INT((int)empty.nID,     0,    _T("OOB/empty/id"));
    EXPECT_PTR(empty.dropdown,     nullptr, _T("OOB/empty/dropdown"));

    bar.AppendItem(101, _T("&File"), nullptr);
    bar.RemoveItem(99);                            // 越界 no-op
    EXPECT_INT(bar.GetItemCount(), 1, _T("OOB/remove"));

    bar.SetDropdown(99, reinterpret_cast<DuiMenu*>(0x33330000));   // 越界 no-op
    EXPECT_PTR(bar.GetDropdown(0),  nullptr,  _T("OOB/setDropdown"));
    EXPECT_PTR(bar.GetDropdown(99), nullptr,  _T("OOB/getDropdown"));

    EXPECT_BOOL(bar.IsItemEnabled(999), false, _T("OOB/enabled-id"));
    bar.SetItemEnabled(999, false);                // 不存在 id no-op
    return OK(_T("OOBSafe"));
}

static Result Test_RemoveItem()
{
    DuiMenuBar bar;
    bar.AppendItem(101, _T("&File"),    nullptr);
    bar.AppendItem(102, _T("&Options"), nullptr);
    bar.AppendItem(103, _T("&View"),    nullptr);
    bar.RemoveItem(1);
    EXPECT_INT(bar.GetItemCount(), 2, _T("Remove/count"));
    EXPECT_INT((int)bar.GetItem(0).nID, 101, _T("Remove/0"));
    EXPECT_INT((int)bar.GetItem(1).nID, 103, _T("Remove/1"));
    return OK(_T("RemoveItem"));
}

static Result Test_Clear()
{
    DuiMenuBar bar;
    bar.AppendItem(101, _T("&File"), nullptr);
    bar.AppendItem(102, _T("&Edit"), nullptr);
    EXPECT_INT(bar.GetItemCount(), 2, _T("Clear/before"));
    bar.Clear();
    EXPECT_INT(bar.GetItemCount(),   0,  _T("Clear/count"));
    EXPECT_INT(bar.GetActiveIndex(), -1, _T("Clear/active"));
    return OK(_T("Clear"));
}

static Result Test_EnabledRoundTrip()
{
    DuiMenuBar bar;
    bar.AppendItem(101, _T("&File"),    nullptr);
    bar.AppendItem(102, _T("&Options"), nullptr);
    EXPECT_BOOL(bar.IsItemEnabled(101), true, _T("Enabled/initialFile"));
    EXPECT_BOOL(bar.IsItemEnabled(102), true, _T("Enabled/initialOpt"));

    bar.SetItemEnabled(101, false);
    EXPECT_BOOL(bar.IsItemEnabled(101), false, _T("Enabled/disabled"));
    EXPECT_BOOL(bar.GetItem(0).enabled, false, _T("Enabled/itemSync"));
    EXPECT_BOOL(bar.IsItemEnabled(102), true,  _T("Enabled/otherUntouched"));

    bar.SetItemEnabled(101, true);
    EXPECT_BOOL(bar.IsItemEnabled(101), true,  _T("Enabled/reEnabled"));
    return OK(_T("EnabledRoundTrip"));
}

static Result Test_SetDropdownRoundTrip()
{
    DuiMenu* fakeA = reinterpret_cast<DuiMenu*>(0x11110000);
    DuiMenu* fakeB = reinterpret_cast<DuiMenu*>(0x22220000);

    DuiMenuBar bar;
    bar.AppendItem(101, _T("&File"), fakeA);
    EXPECT_PTR(bar.GetDropdown(0), fakeA, _T("SetDropdown/initial"));

    bar.SetDropdown(0, fakeB);
    EXPECT_PTR(bar.GetDropdown(0), fakeB, _T("SetDropdown/replaced"));
    EXPECT_PTR(bar.GetItem(0).dropdown, fakeB, _T("SetDropdown/itemSync"));

    bar.SetDropdown(0, nullptr);
    EXPECT_PTR(bar.GetDropdown(0), nullptr, _T("SetDropdown/cleared"));
    return OK(_T("SetDropdownRoundTrip"));
}

static Result Test_ItemHeight()
{
    DuiMenuBar bar;
    EXPECT_INT(bar.GetItemHeight(), 24, _T("ItemH/default"));

    bar.SetItemHeight(36);
    EXPECT_INT(bar.GetItemHeight(), 36, _T("ItemH/set36"));

    bar.SetItemHeight(5);                  // 应被 clamp 到 12
    EXPECT_INT(bar.GetItemHeight(), 12, _T("ItemH/clampMin"));
    return OK(_T("ItemHeightClamp"));
}

// ---- ProcessAltKey: 非 letter / 非 digit 路径 ----
//
// "命中匹配栏目"路径会跳进 OpenAndDriveDropdown 的消息循环（需要 host
// HWND），这里只测"非字母 / 非数字"和"找不到匹配"两条 false 路径。
// 命中行为靠 DuiGallery 手动验证清单覆盖。
static Result Test_ProcessAltKey_NonAlpha()
{
    DuiMenuBar bar;
    bar.AppendItem(101, _T("&File"),    nullptr);
    bar.AppendItem(102, _T("&Options"), nullptr);
    EXPECT_BOOL(bar.ProcessAltKey(VK_RETURN), false, _T("Alt/VKReturn"));
    EXPECT_BOOL(bar.ProcessAltKey(VK_ESCAPE), false, _T("Alt/VKEscape"));
    EXPECT_BOOL(bar.ProcessAltKey(VK_F1),     false, _T("Alt/VKF1"));
    return OK(_T("ProcessAltKey_NonAlpha"));
}

static Result Test_ProcessAltKey_NoMatch()
{
    DuiMenuBar bar;
    bar.AppendItem(101, _T("&File"),    nullptr);
    bar.AppendItem(102, _T("&Options"), nullptr);
    // 没有助记符是 'X' 的栏目 → 应返 false（且不进消息循环）
    EXPECT_BOOL(bar.ProcessAltKey('X'), false, _T("Alt/noMatch"));
    EXPECT_BOOL(bar.ProcessAltKey('Z'), false, _T("Alt/noMatchZ"));
    return OK(_T("ProcessAltKey_NoMatch"));
}

#undef EXPECT_INT
#undef EXPECT_BOOL
#undef EXPECT_STR
#undef EXPECT_PTR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),               &Test_Defaults               },
        { _T("AppendReturnsIndex"),     &Test_AppendReturnsIndex     },
        { _T("AppendStores"),           &Test_AppendStores           },
        { _T("OOBSafe"),                &Test_OOBSafe                },
        { _T("RemoveItem"),             &Test_RemoveItem             },
        { _T("Clear"),                  &Test_Clear                  },
        { _T("EnabledRoundTrip"),       &Test_EnabledRoundTrip       },
        { _T("SetDropdownRoundTrip"),   &Test_SetDropdownRoundTrip   },
        { _T("ItemHeightClamp"),        &Test_ItemHeight             },
        { _T("ProcessAltKey_NonAlpha"), &Test_ProcessAltKey_NonAlpha },
        { _T("ProcessAltKey_NoMatch"),  &Test_ProcessAltKey_NoMatch  },
    };

    CString out;
    int passed = 0, failed = 0;
    for (auto& e : tests)
    {
        Result r = e.fn();
        if (r.ok)
        {
            ++passed;
            CString line; line.Format(_T("[ok]   %s"), e.name);
            if (!out.IsEmpty()) { out += _T("\r\n"); }
            out += line;
        }
        else
        {
            ++failed;
            CString line; line.Format(_T("[FAIL] %s : %s"), e.name, (LPCTSTR)r.detail);
            if (!out.IsEmpty()) { out += _T("\r\n"); }
            out += line;
        }
    }
    CString summary;
    summary.Format(_T("[summary] DuiMenuBarTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiMenuBarTests

} // namespace balloonwjui

#endif // BUI_FEATURE_MENUBAR
