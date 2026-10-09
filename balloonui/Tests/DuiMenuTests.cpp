#include "stdafx.h"
#include "DuiMenuTests.h"

#if BUI_FEATURE_MENU

#include "../ImageEx.h"

namespace balloonwjui {

namespace DuiMenuTests {

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

static Result Test_Defaults()
{
    DuiMenu m;
    EXPECT_INT(m.GetCount(), 0, _T("Defaults/empty"));
    return OK(_T("Defaults"));
}

static Result Test_AppendVariety()
{
    DuiMenu m;
    m.AppendItem    (100, _T("Open"));
    m.AppendChecked (101, _T("Stay on top"), true);
    m.AppendDisabled(102, _T("Cannot click"));
    m.AppendSeparator();
    m.AppendItem    (103, _T("Quit"));
    EXPECT_INT(m.GetCount(), 5, _T("Append/count"));
    const auto& items = m.GetItems();
    EXPECT_INT((int)items[0].kind, (int)DuiMenu::ItemText,      _T("Append/0kind"));
    EXPECT_INT((int)items[1].kind, (int)DuiMenu::ItemCheckable, _T("Append/1kind"));
    EXPECT_BOOL(items[1].checked, true,                          _T("Append/1checked"));
    EXPECT_INT((int)items[2].kind, (int)DuiMenu::ItemText,      _T("Append/2kind"));
    EXPECT_BOOL(items[2].enabled, false,                         _T("Append/2enabled"));
    EXPECT_INT((int)items[3].kind, (int)DuiMenu::ItemSeparator, _T("Append/3kind"));
    EXPECT_INT((int)items[4].id,   103,                          _T("Append/4id"));
    EXPECT_STR(items[4].text,      _T("Quit"),                   _T("Append/4text"));
    return OK(_T("AppendVariety"));
}

static Result Test_SubMenuLink()
{
    DuiMenu sub;
    sub.AppendItem(200, _T("English"));
    sub.AppendItem(201, _T("Chinese"));
    DuiMenu m;
    m.AppendSubMenu(150, _T("Language"), &sub);
    EXPECT_INT(m.GetCount(), 1, _T("Sub/count"));
    const auto& it = m.GetItems()[0];
    EXPECT_INT((int)it.kind, (int)DuiMenu::ItemSubMenu, _T("Sub/kind"));
    EXPECT_BOOL(it.subMenu == &sub, true,               _T("Sub/ptr"));
    EXPECT_BOOL(it.enabled, true,                       _T("Sub/enabled"));
    EXPECT_INT(it.subMenu->GetCount(), 2,               _T("Sub/childCount"));
    return OK(_T("SubMenuLink"));
}

static Result Test_SubMenuNullDisabled()
{
    DuiMenu m;
    m.AppendSubMenu(160, _T("Empty"), nullptr);
    EXPECT_BOOL(m.GetItems()[0].enabled, false, _T("Sub/nullDisabled"));
    return OK(_T("SubMenuNullDisabled"));
}

static Result Test_SetCheckEnabled()
{
    DuiMenu m;
    m.AppendChecked(10, _T("X"), false);
    m.AppendItem   (11, _T("Y"));
    EXPECT_BOOL(m.IsChecked(10), false, _T("Mut/initFalse"));
    m.SetCheck(10, true);
    EXPECT_BOOL(m.IsChecked(10), true,  _T("Mut/checkOn"));
    EXPECT_BOOL(m.IsEnabled(11), true,  _T("Mut/itemEnabled"));
    m.SetEnabled(11, false);
    EXPECT_BOOL(m.IsEnabled(11), false, _T("Mut/disable"));
    EXPECT_BOOL(m.IsEnabled(99), false, _T("Mut/oobNotEnabled"));
    EXPECT_BOOL(m.IsChecked(99), false, _T("Mut/oobNotChecked"));
    return OK(_T("SetCheckEnabled"));
}

static Result Test_SeparatorSkipped()
{
    // FindIndexById skips separators; SetCheck on a sep-id no-ops.
    DuiMenu m;
    m.AppendSeparator();
    m.AppendItem(7, _T("Ok"));
    m.SetCheck(0, true);   // sep id is 0 - should NOT match
    return OK(_T("SeparatorSkipped"));
}

static Result Test_Clear()
{
    DuiMenu m;
    m.AppendItem(1, _T("a"));
    m.AppendItem(2, _T("b"));
    EXPECT_INT(m.GetCount(), 2, _T("Clear/before"));
    m.Clear();
    EXPECT_INT(m.GetCount(), 0, _T("Clear/after"));
    return OK(_T("Clear"));
}

static Result Test_OOBSafe()
{
    DuiMenu m;
    m.SetCheck(99, true);          // safe no-op on empty
    m.SetEnabled(99, false);       // safe no-op
    m.AppendItem(1, _T("hi"));
    EXPECT_BOOL(m.IsChecked(99), false, _T("OOB/checkedFalse"));
    return OK(_T("OOBSafe"));
}

// Icon storage: pointer round-trip via the constructor and via SetItemIcon.
// We don't load any real CImageEx here - just store opaque pointers.
static Result Test_IconRoundTrip()
{
    // We can't legally instantiate a CImageEx here without GDI+ etc., but
    // the menu only stores the pointer; using a fake address is fine for
    // pure-data tests since nothing dereferences it.
    CImageEx* fakeA = reinterpret_cast<CImageEx*>(0x11110000);
    CImageEx* fakeB = reinterpret_cast<CImageEx*>(0x22220000);

    DuiMenu m;
    m.AppendItem(1, _T("with icon"), fakeA);
    m.AppendItem(2, _T("no icon"));
    if (m.GetItemIcon(1) != fakeA)
    {
        return Fail(_T("Icon/initial"), _T("ptr mismatch"));
    }
    if (m.GetItemIcon(2) != nullptr)
    {
        return Fail(_T("Icon/nullDefault"), _T("expected null"));
    }
    m.SetItemIcon(2, fakeB);
    if (m.GetItemIcon(2) != fakeB)
    {
        return Fail(_T("Icon/setLater"), _T("ptr mismatch"));
    }
    m.SetItemIcon(1, nullptr);     // clear
    if (m.GetItemIcon(1) != nullptr)
    {
        return Fail(_T("Icon/clear"), _T("expected null"));
    }
    return OK(_T("IconRoundTrip"));
}

// Append* now returns the index of the newly-added item.
static Result Test_AppendReturnsIndex()
{
    DuiMenu m;
    EXPECT_INT(m.AppendItem(1, _T("a")), 0, _T("Idx/0"));
    EXPECT_INT(m.AppendChecked(2, _T("b"), false), 1, _T("Idx/1"));
    EXPECT_INT(m.AppendSeparator(), 2, _T("Idx/sep"));
    EXPECT_INT(m.AppendDisabled(3, _T("c")), 3, _T("Idx/3"));
    EXPECT_INT(m.AppendSubMenu(4, _T("d"), nullptr), 4, _T("Idx/sub"));
    EXPECT_INT(m.GetCount(), 5, _T("Idx/total"));
    return OK(_T("AppendReturnsIndex"));
}

// Disabled with an icon stays disabled and keeps the icon pointer.
static Result Test_DisabledIconStored()
{
    CImageEx* fake = reinterpret_cast<CImageEx*>(0x33330000);
    DuiMenu m;
    m.AppendDisabled(7, _T("nope"), fake);
    EXPECT_BOOL(m.IsEnabled(7), false, _T("DisabledIcon/disabled"));
    if (m.GetItemIcon(7) != fake)
    {
        return Fail(_T("DisabledIcon/iconKept"), _T("ptr mismatch"));
    }
    return OK(_T("DisabledIconStored"));
}

// ----- mnemonic + keyboard nav helpers --------------------------------

static Result Test_FindAcceleratorChar()
{
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("&Save")),       (int)_T('s'), _T("Acc/Save"));
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("Save &As")),    (int)_T('a'), _T("Acc/As"));
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("Save && Quit")),(int)0,        _T("Acc/escapedAmp"));
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("&&&Foo")),      (int)_T('f'), _T("Acc/escapedThenReal"));
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("plain")),       (int)0,        _T("Acc/none"));
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("")),            (int)0,        _T("Acc/empty"));
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(nullptr),           (int)0,        _T("Acc/null"));
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("trailing&")),   (int)0,        _T("Acc/trailing"));
    return OK(_T("FindAcceleratorChar"));
}

static Result Test_FindAcceleratorMatch()
{
    DuiMenu m;
    m.AppendItem(101, _T("&Save"));
    m.AppendItem(102, _T("Save &As"));
    m.AppendSeparator();
    m.AppendDisabled(103, _T("&Disabled"));
    m.AppendItem(104, _T("&save again"));    // dup 's' - first wins
    const auto& items = m.GetItems();

    EXPECT_INT(DuiMenu::FindAcceleratorMatch(items, _T('s')), 0, _T("M/firstS"));
    EXPECT_INT(DuiMenu::FindAcceleratorMatch(items, _T('S')), 0, _T("M/case"));
    EXPECT_INT(DuiMenu::FindAcceleratorMatch(items, _T('a')), 1, _T("M/As"));
    EXPECT_INT(DuiMenu::FindAcceleratorMatch(items, _T('d')), -1, _T("M/disabledSkipped"));
    EXPECT_INT(DuiMenu::FindAcceleratorMatch(items, _T('z')), -1, _T("M/none"));
    EXPECT_INT(DuiMenu::FindAcceleratorMatch(items, 0),       -1, _T("M/zero"));
    return OK(_T("FindAcceleratorMatch"));
}

static Result Test_KeyboardNavNext()
{
    DuiMenu m;
    m.AppendItem(1, _T("a"));      // 0
    m.AppendSeparator();           // 1 (skipped)
    m.AppendItem(2, _T("b"));      // 2
    m.AppendDisabled(3, _T("c"));  // 3 (skipped)
    m.AppendItem(4, _T("d"));      // 4
    const auto& items = m.GetItems();

    // Forward.
    EXPECT_INT(DuiMenu::KeyboardNavNext(items, -1, +1), 0, _T("Nav/initFwd"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  0, +1), 2, _T("Nav/0to2"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  2, +1), 4, _T("Nav/2to4"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  4, +1), 0, _T("Nav/wrap"));
    // Backward.
    EXPECT_INT(DuiMenu::KeyboardNavNext(items, -1, -1), 4, _T("Nav/initBack"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  4, -1), 2, _T("Nav/4to2"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  0, -1), 4, _T("Nav/wrapBack"));
    return OK(_T("KeyboardNavNext"));
}

static Result Test_KeyboardNavAllSeparator()
{
    DuiMenu m;
    m.AppendSeparator();
    m.AppendSeparator();
    const auto& items = m.GetItems();
    EXPECT_INT(DuiMenu::KeyboardNavNext(items, -1, +1), -1, _T("NavSep/none"));
    return OK(_T("KeyboardNavAllSeparator"));
}

// 分隔条一行高 5px（1px 线上下各留 2px，2026-10-04 由 12 收窄）：同样两项文字，
// 中间多一条分隔条的菜单正好高出这么多。用 MeasureSize 的高度差来量，不依赖文字行高。
static Result Test_SeparatorRowHeight()
{
    // 分隔条一行的期望高度（像素），与 DuiMenu.cpp 的 kSepRowH 相同
    const int kExpectedSepRowH = 5;

    DuiMenu plain;
    plain.AppendItem(1, _T("a"));
    plain.AppendItem(2, _T("b"));

    DuiMenu withSep;
    withSep.AppendItem(1, _T("a"));
    withSep.AppendSeparator();
    withSep.AppendItem(2, _T("b"));

    EXPECT_INT((int)(withSep.MeasureSize().cy - plain.MeasureSize().cy), kExpectedSepRowH,
               _T("SepRowH/delta"));
    return OK(_T("SeparatorRowHeight"));
}

// ---- 分组标题行（2026-10-05 起，主面板系统菜单按组加标题）----

// AppendHeader 返回新项的 index；这一项类型为 ItemHeader、id 为 0、不可用、文字原样保存，
// 文字传空指针时存为空串
static Result Test_HeaderAppend()
{
    DuiMenu m;
    m.AppendItem(1, _T("a"));
    EXPECT_INT(m.AppendHeader(_T("Account")), 1, _T("Header/index"));
    const std::vector<DuiMenu::Item>& items = m.GetItems();
    EXPECT_INT((int)items[1].kind, (int)DuiMenu::ItemHeader, _T("Header/kind"));
    EXPECT_INT((int)items[1].id, 0, _T("Header/id"));
    EXPECT_BOOL(items[1].enabled, false, _T("Header/disabled"));
    EXPECT_STR(items[1].text, _T("Account"), _T("Header/text"));
    const int nullIdx = m.AppendHeader(nullptr);
    EXPECT_STR(m.GetItems()[nullIdx].text, _T(""), _T("Header/nullText"));
    return OK(_T("HeaderAppend"));
}

// 键盘上下键跳过标题行：两个方向都跳，到头后环回时同样跳过；只有标题行与分隔条时没有可选项
static Result Test_HeaderSkippedByKeyboard()
{
    DuiMenu m;
    m.AppendHeader(_T("Account"));    // 0
    m.AppendItem(1, _T("Profile"));   // 1
    m.AppendItem(2, _T("Password"));  // 2
    m.AppendSeparator();              // 3
    m.AppendHeader(_T("Settings"));   // 4
    m.AppendItem(3, _T("Network"));   // 5
    const std::vector<DuiMenu::Item>& items = m.GetItems();
    EXPECT_INT(DuiMenu::KeyboardNavNext(items, -1, +1), 1, _T("NavHeader/firstDown"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  2, +1), 5, _T("NavHeader/overSepAndHeader"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  5, +1), 1, _T("NavHeader/wrapDown"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  5, -1), 2, _T("NavHeader/up"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items,  1, -1), 5, _T("NavHeader/wrapUp"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(items, -1, -1), 5, _T("NavHeader/firstUp"));

    DuiMenu onlyHeaders;
    onlyHeaders.AppendHeader(_T("A"));
    onlyHeaders.AppendSeparator();
    onlyHeaders.AppendHeader(_T("B"));
    EXPECT_INT(DuiMenu::KeyboardNavNext(onlyHeaders.GetItems(), -1, +1), -1, _T("NavHeader/none"));
    return OK(_T("HeaderSkippedByKeyboard"));
}

// 标题文字里即使带 '&'，按对应字母也不会选中标题行
static Result Test_HeaderNoAccelerator()
{
    DuiMenu m;
    m.AppendHeader(_T("&Account"));
    m.AppendItem(1, _T("&Profile"));
    const std::vector<DuiMenu::Item>& items = m.GetItems();
    EXPECT_INT(DuiMenu::FindAcceleratorMatch(items, _T('a')), -1, _T("AccelHeader/header"));
    EXPECT_INT(DuiMenu::FindAcceleratorMatch(items, _T('p')),  1, _T("AccelHeader/item"));
    return OK(_T("HeaderNoAccelerator"));
}

// 按 id 的修改不作用于标题行：标题行 id 为 0，SetEnabled(0, true) / SetCheck(0, true) 之后它仍不可用、未勾选
static Result Test_HeaderUnaffectedById()
{
    DuiMenu m;
    m.AppendHeader(_T("Account"));
    m.AppendItem(1, _T("Profile"));
    m.SetEnabled(0, true);
    m.SetCheck(0, true);
    const std::vector<DuiMenu::Item>& items = m.GetItems();
    EXPECT_BOOL(items[0].enabled, false, _T("IdHeader/stillDisabled"));
    EXPECT_BOOL(items[0].checked, false, _T("IdHeader/notChecked"));
    EXPECT_BOOL(m.IsEnabled(0), false, _T("IdHeader/isEnabled"));
    EXPECT_BOOL(m.IsEnabled(1), true, _T("IdHeader/itemEnabled"));
    return OK(_T("HeaderUnaffectedById"));
}

// 标题行一行高 24px（普通行 32px）：同样两项文字，中间多一个标题行的菜单正好高出这么多
static Result Test_HeaderRowHeight()
{
    // 标题行的期望高度（像素），与 DuiMenu.cpp 的 kHeaderRowH 相同
    const int kExpectedHeaderRowH = 24;

    DuiMenu plain;
    plain.AppendItem(1, _T("a"));
    plain.AppendItem(2, _T("b"));

    DuiMenu withHeader;
    withHeader.AppendItem(1, _T("a"));
    withHeader.AppendHeader(_T("g"));
    withHeader.AppendItem(2, _T("b"));

    EXPECT_INT((int)(withHeader.MeasureSize().cy - plain.MeasureSize().cy), kExpectedHeaderRowH,
               _T("HeaderRowH/delta"));
    return OK(_T("HeaderRowHeight"));
}

// 标题文字计入菜单宽度：标题比各项都长时菜单按标题加宽，标题不会被截断；
// 标题按小一号的字测量，同样的文字作为标题时比作为普通项窄
static Result Test_HeaderWidthCounted()
{
    // 足够长、但按两种字号测量都不超过 DuiMenu 文字宽度上限（320px）的标题文字
    const LPCTSTR kLongText = _T("Messages and tools group title");

    DuiMenu plain;
    plain.AppendItem(1, _T("a"));

    DuiMenu withHeader;
    withHeader.AppendHeader(kLongText);
    withHeader.AppendItem(1, _T("a"));

    DuiMenu asItem;
    asItem.AppendItem(2, kLongText);
    asItem.AppendItem(1, _T("a"));

    const LONG plainW = plain.MeasureSize().cx;
    const LONG headerW = withHeader.MeasureSize().cx;
    const LONG itemW = asItem.MeasureSize().cx;
    EXPECT_BOOL(headerW > plainW, true, _T("HeaderW/widens"));
    EXPECT_BOOL(headerW < itemW, true, _T("HeaderW/smallerFont"));
    return OK(_T("HeaderWidthCounted"));
}

// 菜单项文字在第一个 '\t' 处分成左段与快捷键（2026-10-07 起）；没有 '\t' 时整段是左段；NULL 时两段都空
static Result Test_SplitShortcut()
{
    CString label;
    CString shortcut;
    DuiMenu::SplitShortcut(_T("Heading 1\tCtrl+1"), label, shortcut);
    EXPECT_STR(label, _T("Heading 1"), _T("Split/label"));
    EXPECT_STR(shortcut, _T("Ctrl+1"), _T("Split/shortcut"));

    DuiMenu::SplitShortcut(_T("Plain"), label, shortcut);
    EXPECT_STR(label, _T("Plain"), _T("Split/noTab/label"));
    EXPECT_STR(shortcut, _T(""), _T("Split/noTab/shortcut"));

    //只在第一个 '\t' 处分开，其后的 '\t' 留在快捷键里
    DuiMenu::SplitShortcut(_T("a\tb\tc"), label, shortcut);
    EXPECT_STR(label, _T("a"), _T("Split/twoTabs/label"));
    EXPECT_STR(shortcut, _T("b\tc"), _T("Split/twoTabs/shortcut"));

    DuiMenu::SplitShortcut(nullptr, label, shortcut);
    EXPECT_STR(label, _T(""), _T("Split/null/label"));
    EXPECT_STR(shortcut, _T(""), _T("Split/null/shortcut"));
    return OK(_T("SplitShortcut"));
}

// 带快捷键的菜单比不带的宽，且比把两段直接连在一起写更宽（两段之间留有间距）；行高不变
static Result Test_ShortcutWidthCounted()
{
    DuiMenu plain;
    plain.AppendItem(1, _T("a"));

    DuiMenu withShortcut;
    withShortcut.AppendItem(1, _T("a\tCtrl+Shift+5"));

    DuiMenu joined;
    joined.AppendItem(1, _T("aCtrl+Shift+5"));

    const SIZE plainSize = plain.MeasureSize();
    const SIZE shortcutSize = withShortcut.MeasureSize();
    const SIZE joinedSize = joined.MeasureSize();
    EXPECT_BOOL(shortcutSize.cx > plainSize.cx, true, _T("ShortcutW/widens"));
    EXPECT_BOOL(shortcutSize.cx > joinedSize.cx, true, _T("ShortcutW/gap"));
    EXPECT_INT(shortcutSize.cy, plainSize.cy, _T("ShortcutW/rowHeight"));
    return OK(_T("ShortcutWidthCounted"));
}

// 助记符只认左段的 '&'：快捷键里的 '&' 不算
static Result Test_ShortcutNoAccelerator()
{
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("&Open\tCtrl+O")), (int)_T('o'), _T("ShortcutAcc/label"));
    EXPECT_INT((int)DuiMenu::FindAcceleratorChar(_T("Open\tCtrl+&X")), 0, _T("ShortcutAcc/inShortcut"));
    return OK(_T("ShortcutNoAccelerator"));
}

#undef EXPECT_INT
#undef EXPECT_BOOL
#undef EXPECT_STR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),            &Test_Defaults            },
        { _T("AppendVariety"),       &Test_AppendVariety       },
        { _T("SubMenuLink"),         &Test_SubMenuLink         },
        { _T("SubMenuNullDisabled"), &Test_SubMenuNullDisabled },
        { _T("SetCheckEnabled"),     &Test_SetCheckEnabled     },
        { _T("SeparatorSkipped"),    &Test_SeparatorSkipped    },
        { _T("Clear"),               &Test_Clear               },
        { _T("OOBSafe"),             &Test_OOBSafe             },
        { _T("IconRoundTrip"),       &Test_IconRoundTrip       },
        { _T("AppendReturnsIndex"),  &Test_AppendReturnsIndex  },
        { _T("DisabledIconStored"),  &Test_DisabledIconStored  },
        { _T("FindAcceleratorChar"), &Test_FindAcceleratorChar },
        { _T("FindAcceleratorMatch"),&Test_FindAcceleratorMatch},
        { _T("KeyboardNavNext"),     &Test_KeyboardNavNext     },
        { _T("KeyboardNavAllSeparator"), &Test_KeyboardNavAllSeparator },
        { _T("SeparatorRowHeight"),  &Test_SeparatorRowHeight  },
        { _T("HeaderAppend"),        &Test_HeaderAppend        },
        { _T("HeaderSkippedByKeyboard"), &Test_HeaderSkippedByKeyboard },
        { _T("HeaderNoAccelerator"), &Test_HeaderNoAccelerator },
        { _T("HeaderUnaffectedById"),&Test_HeaderUnaffectedById},
        { _T("HeaderRowHeight"),     &Test_HeaderRowHeight     },
        { _T("HeaderWidthCounted"),  &Test_HeaderWidthCounted  },
        { _T("SplitShortcut"),       &Test_SplitShortcut       },
        { _T("ShortcutWidthCounted"),&Test_ShortcutWidthCounted},
        { _T("ShortcutNoAccelerator"), &Test_ShortcutNoAccelerator }
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
    summary.Format(_T("[summary] DuiMenuTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiMenuTests

} // namespace balloonwjui

#endif // BUI_FEATURE_MENU
