#include "stdafx.h"
#include "DuiComboBoxTests.h"

#if BUI_FEATURE_COMBOBOX

#include "../Controls/List/DuiListBox.h"        // DUITN_ITEMDELETE（浮层通知分类用例）
#include "../Controls/Window/DuiScrollBar.h"    // 列表内部滚动条（浮层通知分类用例）
#include "../DuiNotify.h"


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

// ---- 边框颜色（可配；默认值与历史常量一致）-------------------------------

// 默认边框色：常态 RGB(150,150,150)，悬停 / 展开 RGB(80,130,200)。与引入这组接口
// 之前写死的常量相同，不调用 SetBorderColors 的地方外观不变。
static Result Test_BorderColorDefault()
{
    DuiComboBox c;
    EXPECT_INT((int)c.GetBorderNormalColor(), (int)RGB(150, 150, 150), _T("Brd/defNormal"));
    EXPECT_INT((int)c.GetBorderActiveColor(), (int)RGB( 80, 130, 200), _T("Brd/defActive"));
    return OK(_T("BorderColorDefault"));
}

// SetBorderColors / Get*BorderColor 往返。
static Result Test_BorderColorRoundTrip()
{
    DuiComboBox c;
    c.SetBorderColors(RGB(237, 237, 237), RGB(255, 122, 69));
    EXPECT_INT((int)c.GetBorderNormalColor(), (int)RGB(237, 237, 237), _T("Brd/normal"));
    EXPECT_INT((int)c.GetBorderActiveColor(), (int)RGB(255, 122, 69),  _T("Brd/active"));

    c.SetBorderColors(RGB(150, 150, 150), RGB(80, 130, 200));   // 回默认
    EXPECT_INT((int)c.GetBorderNormalColor(), (int)RGB(150, 150, 150), _T("Brd/backNormal"));
    EXPECT_INT((int)c.GetBorderActiveColor(), (int)RGB( 80, 130, 200), _T("Brd/backActive"));
    return OK(_T("BorderColorRoundTrip"));
}

// 与 ShowBorder / ArrowColor / BgColor 互不影响。
static Result Test_BorderColorOrthogonal()
{
    DuiComboBox c;
    c.SetArrowColor(RGB(102, 102, 102));
    c.SetBgColor(RGB(245, 246, 248));
    c.SetBorderColors(RGB(237, 237, 237), RGB(255, 122, 69));

    EXPECT_INT((int)c.GetArrowColor(), (int)RGB(102, 102, 102), _T("BrdOrth/arrow"));
    EXPECT_INT((int)c.GetBgColor(),    (int)RGB(245, 246, 248), _T("BrdOrth/bg"));

    // 隐藏边框不应清掉已设置的边框色。
    c.SetShowBorder(false);
    EXPECT_INT((int)c.GetBorderNormalColor(), (int)RGB(237, 237, 237), _T("BrdOrth/normalAfterHide"));
    EXPECT_INT((int)c.GetBorderActiveColor(), (int)RGB(255, 122, 69),  _T("BrdOrth/activeAfterHide"));
    return OK(_T("BorderColorOrthogonal"));
}

// 画出来的边框确实用了设置的常态色：边框用 GDI RoundRect + 1px 画笔绘制、不抗锯齿，
// 上边框正中那个像素就是画笔颜色本身。
static Result Test_BorderColorPainted()
{
    const int      kW      = 120;
    const int      kH      = 30;
    const COLORREF kNormal = RGB(10, 200, 30);   // 与底色、箭头色都不同的醒目色

    DuiComboBox c;
    c.SetBgColor(RGB(255, 255, 255));
    c.SetBorderColors(kNormal, RGB(255, 122, 69));
    RECT rc = { 0, 0, kW, kH };
    c.SetRect(rc);

    HDC screen = ::GetDC(NULL);
    HDC mem    = ::CreateCompatibleDC(screen);
    HBITMAP bmp = ::CreateCompatibleBitmap(screen, kW, kH);
    ::ReleaseDC(NULL, screen);
    HGDIOBJ old = ::SelectObject(mem, bmp);
    HBRUSH white = ::CreateSolidBrush(RGB(255, 255, 255));
    ::FillRect(mem, &rc, white);
    ::DeleteObject(white);

    c.OnPaint(mem, rc);
    const COLORREF top = ::GetPixel(mem, kW / 2, 0);

    ::SelectObject(mem, old);
    ::DeleteObject(bmp);
    ::DeleteDC(mem);

    EXPECT_INT((int)top, (int)kNormal, _T("BrdPaint/topEdge"));
    return OK(_T("BorderColorPainted"));
}

// =====================================================================
// 下拉浮层落点（combopopup::ClampPopupToWorkArea）
//
// 这些用例钉的是"浮层不许跑到桌面外"。真弹浮层要建顶层窗口、要问显示器，
// 测不了；而落点计算本身是纯算术，把工作区当参数传进来就能直接断言。
//
// 统一用一块 1920x1040 的工作区（1080 的屏幕减掉 40 高的任务栏），行高 22、
// 浮层宽 200 —— 与登录窗账号下拉的实际参数接近。
// =====================================================================

// 各用例共用的工作区：模拟 1920x1080 屏幕、底部 40 像素任务栏。
static RECT MakeWorkArea()
{
    RECT work;
    work.left   = 0;
    work.top    = 0;
    work.right  = 1920;
    work.bottom = 1040;
    return work;
}

// 造一个下拉框矩形：左上角 (x, y)，宽 200、高 40（与登录窗账号框同高）。
static RECT MakeCombo(int x, int y)
{
    RECT rc;
    rc.left   = x;
    rc.top    = y;
    rc.right  = x + 200;
    rc.bottom = y + 40;
    return rc;
}

// 按行数算浮层期望高度（与 OpenPopup 里的算法一致）。
static int PopupHeightForRows(int rows, int itemH)
{
    return rows * itemH + combopopup::kPopupBorderThickness;
}

// 下方空间充足 —— 浮层就贴在下拉框正下方、与其左对齐同宽，高度不打折。
static Result Test_PopupFitsBelow()
{
    const RECT work  = MakeWorkArea();
    const RECT combo = MakeCombo(300, 200);
    const int  h     = PopupHeightForRows(8, 22);

    RECT rc = combopopup::ClampPopupToWorkArea(combo, 200, h, 22, work);

    EXPECT_INT(rc.left,          300,             _T("Below/left"));
    EXPECT_INT(rc.top,           combo.bottom,    _T("Below/top"));
    EXPECT_INT(rc.right - rc.left, 200,           _T("Below/width"));
    EXPECT_INT(rc.bottom - rc.top, h,             _T("Below/height"));
    return OK(_T("PopupFitsBelow"));
}

// 下拉框贴近屏幕底部、下方装不下，但上方装得下 —— 翻到下拉框上方展开，
// 底边正好贴住下拉框顶边，高度不打折。
static Result Test_PopupFlipsAbove()
{
    const RECT work  = MakeWorkArea();
    const int  h     = PopupHeightForRows(15, 22);   // 332，下方只剩 60
    const RECT combo = MakeCombo(300, 940);          // bottom=980，work.bottom=1040

    RECT rc = combopopup::ClampPopupToWorkArea(combo, 200, h, 22, work);

    EXPECT_INT(rc.bottom,          combo.top,  _T("Above/bottom"));
    EXPECT_INT(rc.top,             combo.top - h, _T("Above/top"));
    EXPECT_INT(rc.bottom - rc.top, h,          _T("Above/height"));
    return OK(_T("PopupFlipsAbove"));
}

// 上下都装不下 —— 选空间大的那一侧，并把高度压到该侧能容纳的整行数。
// 这里下拉框放在偏上的位置，下方空间更大，故仍往下展开但压矮。
static Result Test_PopupShrinksToLargerSide()
{
    const RECT work  = MakeWorkArea();
    const int  h     = PopupHeightForRows(40, 22);   // 882，上下都装不下
    const RECT combo = MakeCombo(300, 300);          // 上方 300、下方 1040-340=700

    RECT rc = combopopup::ClampPopupToWorkArea(combo, 200, h, 22, work);

    // 下方空间 700，能容纳 (700-2)/22 = 31 整行。
    const int expectH = PopupHeightForRows(31, 22);
    EXPECT_INT(rc.top,             combo.bottom, _T("Shrink/top"));
    EXPECT_INT(rc.bottom - rc.top, expectH,      _T("Shrink/height"));
    EXPECT_BOOL(rc.bottom <= work.bottom, true,  _T("Shrink/inWork"));
    return OK(_T("PopupShrinksToLargerSide"));
}

// 下拉框贴着屏幕右缘 —— 浮层右边会越界，须整体往左挪到刚好贴住右边界，
// 宽度不变（不允许把浮层压窄）。
static Result Test_PopupClampsRightEdge()
{
    const RECT work  = MakeWorkArea();
    const RECT combo = MakeCombo(1850, 200);   // 右边到 2050，已超出 1920
    const int  h     = PopupHeightForRows(8, 22);

    RECT rc = combopopup::ClampPopupToWorkArea(combo, 200, h, 22, work);

    EXPECT_INT(rc.right,           work.right, _T("Right/right"));
    EXPECT_INT(rc.left,            1720,       _T("Right/left"));
    EXPECT_INT(rc.right - rc.left, 200,        _T("Right/width"));
    return OK(_T("PopupClampsRightEdge"));
}

// 浮层比整个工作区还宽 —— 往左挪会挪过头顶出左边界，此时以左边界为准贴住，
// 保证左上角始终落在桌面内（右侧溢出无法避免，宽度是调用方定的）。
static Result Test_PopupClampsLeftEdge()
{
    const RECT work  = MakeWorkArea();
    const RECT combo = MakeCombo(100, 200);
    const int  h     = PopupHeightForRows(8, 22);

    RECT rc = combopopup::ClampPopupToWorkArea(combo, 2400, h, 22, work);

    EXPECT_INT(rc.left, work.left, _T("Left/left"));
    return OK(_T("PopupClampsLeftEdge"));
}

// 工作区比一行还矮的极端情况 —— 高度按下限保底为一行，位置贴住工作区上沿，
// 至少保证左上角在桌面内，不能算出负坐标把浮层甩到屏幕外。
static Result Test_PopupTinyWorkArea()
{
    RECT work;
    work.left   = 0;
    work.top    = 0;
    work.right  = 1920;
    work.bottom = 10;                          // 比一行(22)还矮
    const RECT combo = MakeCombo(300, 2);
    const int  h     = PopupHeightForRows(8, 22);

    RECT rc = combopopup::ClampPopupToWorkArea(combo, 200, h, 22, work);

    EXPECT_INT(rc.bottom - rc.top, PopupHeightForRows(combopopup::kPopupMinRows, 22),
                                                _T("Tiny/height"));
    EXPECT_INT(rc.top,             work.top,    _T("Tiny/top"));
    EXPECT_BOOL(rc.left >= work.left, true,     _T("Tiny/left"));
    return OK(_T("PopupTinyWorkArea"));
}

// ---- 浮层项下标 -> m_items 下标的映射 ----
//
// 过滤激活时浮层只显示命中的几项，它报回来的是"第几个命中项"。不映射的话
// 选中会选错人、删除会删错人（账号下拉里点删除叉最容易撞上）。

static Result Test_MapPopupIndexNoFilter()
{
    // 映射表为空 = 没在过滤，浮层下标与 m_items 下标 1:1。
    std::vector<int> none;
    EXPECT_INT(DuiComboBox::MapPopupIndexWithFilter(0, none), 0, _T("Map/none0"));
    EXPECT_INT(DuiComboBox::MapPopupIndexWithFilter(3, none), 3, _T("Map/none3"));
    return OK(_T("MapPopupIndexNoFilter"));
}

static Result Test_MapPopupIndexWithFilter()
{
    // 过滤命中的是 m_items 的第 0、1、4 项（与 IncPrefixCaseInsensitive 同款）。
    std::vector<int> filtered;
    filtered.push_back(0);
    filtered.push_back(1);
    filtered.push_back(4);

    EXPECT_INT(DuiComboBox::MapPopupIndexWithFilter(0, filtered), 0, _T("Map/f0"));
    EXPECT_INT(DuiComboBox::MapPopupIndexWithFilter(1, filtered), 1, _T("Map/f1"));
    // 关键的一条：浮层第 3 行是 m_items 的第 5 项，不是第 3 项。
    EXPECT_INT(DuiComboBox::MapPopupIndexWithFilter(2, filtered), 4, _T("Map/f2"));
    return OK(_T("MapPopupIndexWithFilter"));
}

static Result Test_MapPopupIndexOutOfRange()
{
    // 越出映射表范围时原样返回，由调用方自己判越界（各调用点都判了）。
    std::vector<int> filtered;
    filtered.push_back(2);
    filtered.push_back(7);

    EXPECT_INT(DuiComboBox::MapPopupIndexWithFilter(-1, filtered), -1, _T("Map/neg"));
    EXPECT_INT(DuiComboBox::MapPopupIndexWithFilter(2,  filtered), 2,  _T("Map/over"));
    EXPECT_INT(DuiComboBox::MapPopupIndexWithFilter(99, filtered), 99, _T("Map/far"));
    return OK(_T("MapPopupIndexOutOfRange"));
}

// ---- 浮层对控件通知的分类 ----
//
// 浮层的宿主会把树里所有控件的通知都转给浮层窗口。列表内部的滚动条滚动时同样发
// DUIN_VALUECHANGED（extra 是滚动位置）；只比较通知码的话，滚一下滚轮就被当成选中，
// 浮层随即关闭（2026-10-01 在客户端「清理聊天记录」对话框的时 / 分下拉框上发现）。

// 列表本身发来的选中与删除叉通知照常处理。
static Result Test_PopupNotifyFromList()
{
    EXPECT_INT(combopopup::ClassifyPopupNotify(DUIN_VALUECHANGED, combopopup::kPopupListCtrlId),
               combopopup::kPopupNotifySelect, _T("Notify/listSelect"));
    EXPECT_INT(combopopup::ClassifyPopupNotify((UINT)DuiListBox::DUITN_ITEMDELETE,
                                               combopopup::kPopupListCtrlId),
               combopopup::kPopupNotifyItemDelete, _T("Notify/listDelete"));
    return OK(_T("PopupNotifyFromList"));
}

// 列表内部滚动条（未设 ctrlId，即 0）发来的值变化不算选中，浮层保持打开。
static Result Test_PopupNotifyFromScrollBarIgnored()
{
    DuiScrollBar sb;
    EXPECT_BOOL(sb.GetCtrlId() != combopopup::kPopupListCtrlId, true, _T("Notify/sbIdDiffers"));
    EXPECT_INT(combopopup::ClassifyPopupNotify(DUIN_VALUECHANGED, sb.GetCtrlId()),
               combopopup::kPopupNotifyIgnore, _T("Notify/sbValueChanged"));
    EXPECT_INT(combopopup::ClassifyPopupNotify((UINT)DuiListBox::DUITN_ITEMDELETE, sb.GetCtrlId()),
               combopopup::kPopupNotifyIgnore, _T("Notify/sbItemDelete"));
    return OK(_T("PopupNotifyFromScrollBarIgnored"));
}

// 与选择无关的通知（鼠标进出、点击）一律忽略，即使来自列表本身。
static Result Test_PopupNotifyOtherCodesIgnored()
{
    EXPECT_INT(combopopup::ClassifyPopupNotify(DUIN_MOUSEENTER, combopopup::kPopupListCtrlId),
               combopopup::kPopupNotifyIgnore, _T("Notify/enter"));
    EXPECT_INT(combopopup::ClassifyPopupNotify(DUIN_MOUSELEAVE, combopopup::kPopupListCtrlId),
               combopopup::kPopupNotifyIgnore, _T("Notify/leave"));
    EXPECT_INT(combopopup::ClassifyPopupNotify(DUIN_CLICK, combopopup::kPopupListCtrlId),
               combopopup::kPopupNotifyIgnore, _T("Notify/click"));
    return OK(_T("PopupNotifyOtherCodesIgnored"));
}


// ----- 每项图标与副文字（2026-10-04 起，登录窗账号下拉列表用）-----

// 下拉框每项的图标与副文字跟着条目走：删除、清空、追加都不会错位，越界读写安全
static Result Test_ItemIconSubText()
{
    const int kDefaultIconPx = 16;   // 图标边长的默认值
    const int kIconPx = 28;          // 改过的图标边长
    const int kFarIndex = 99;        // 越界的下标
    HBITMAP icon0 = ::CreateBitmap(1, 1, 1, 32, NULL);
    HBITMAP icon1 = ::CreateBitmap(1, 1, 1, 32, NULL);
    DuiComboBox c;
    c.AddString(_T("Alpha"));
    c.AddString(_T("alligator"));
    c.AddString(_T("Bravo"));
    EXPECT_INT(c.GetIconSize(), kDefaultIconPx, _T("CBIcon/defaultSize"));
    c.SetIconSize(kIconPx);
    EXPECT_INT(c.GetIconSize(), kIconPx, _T("CBIcon/size"));
    EXPECT_BOOL(c.GetItemIcon(0) == NULL, true, _T("CBIcon/defaultNone"));
    EXPECT_BOOL(c.GetItemSubText(0).IsEmpty(), true, _T("CBIcon/defaultSub"));

    c.SetItemIcon(0, icon0);
    c.SetItemSubText(0, _T("A0"));
    c.SetItemIcon(1, icon1);
    c.SetItemSubText(2, _T("B2"));
    // 改主文字不动图标与副文字
    c.SetItemText(0, _T("Alpha2"));
    EXPECT_BOOL(c.GetItemIcon(0) == icon0, true, _T("CBIcon/keepOnSetText"));

    // 删掉第一项：后面的项连同图标与副文字前移
    c.DeleteString(0);
    EXPECT_BOOL(c.GetItemIcon(0) == icon1, true, _T("CBIcon/deleteShift"));
    EXPECT_BOOL(c.GetItemSubText(1) == _T("B2"), true, _T("CBIcon/deleteShiftSub"));

    // 越界读写安全
    c.SetItemIcon(kFarIndex, icon0);
    c.SetItemSubText(-1, _T("x"));
    EXPECT_BOOL(c.GetItemIcon(kFarIndex) == NULL, true, _T("CBIcon/oobIcon"));
    EXPECT_BOOL(c.GetItemSubText(-1).IsEmpty(), true, _T("CBIcon/oobSub"));

    // 清空后再追加：新项没有图标与副文字
    c.ResetContent();
    c.AddString(_T("x"));
    EXPECT_BOOL(c.GetItemIcon(0) == NULL, true, _T("CBIcon/afterReset"));
    EXPECT_BOOL(c.GetItemSubText(0).IsEmpty(), true, _T("CBIcon/afterResetSub"));
    ::DeleteObject(icon0);
    ::DeleteObject(icon1);
    return OK(_T("ItemIconSubText"));
}

// 浮层各行带上对应项的文字、图标与副文字；增量搜索过滤时按过滤结果取，不错位
static Result Test_PopupItemsCarryIconSubText()
{
    HBITMAP icon0 = ::CreateBitmap(1, 1, 1, 32, NULL);
    HBITMAP icon1 = ::CreateBitmap(1, 1, 1, 32, NULL);
    DuiComboBox c;
    c.AddString(_T("Alpha"));
    c.AddString(_T("Bravo"));
    c.AddString(_T("alligator"));
    c.SetItemIcon(0, icon0);
    c.SetItemSubText(0, _T("A0"));
    c.SetItemIcon(2, icon1);
    c.SetItemSubText(2, _T("L2"));

    // 没有过滤：全部各项按原顺序
    std::vector<int> none;
    std::vector<DuiComboBox::PopupItem> all = c.BuildPopupItems(none);
    EXPECT_SIZE(all.size(), 3, _T("CBPopup/allSize"));
    EXPECT_BOOL(all[0].m_text == _T("Alpha"), true, _T("CBPopup/text0"));
    EXPECT_BOOL(all[0].m_subText == _T("A0"), true, _T("CBPopup/sub0"));
    EXPECT_BOOL(all[0].m_icon == icon0, true, _T("CBPopup/icon0"));
    EXPECT_BOOL(all[1].m_icon == NULL, true, _T("CBPopup/icon1None"));
    EXPECT_BOOL(all[1].m_subText.IsEmpty(), true, _T("CBPopup/sub1None"));

    // 过滤「al」命中第 0、2 项：浮层第 2 行是第 2 项，带它自己的图标与副文字
    std::vector<int> filtered = c.ComputeFilteredIndices(_T("al"));
    std::vector<DuiComboBox::PopupItem> part = c.BuildPopupItems(filtered);
    EXPECT_SIZE(part.size(), 2, _T("CBPopup/filteredSize"));
    EXPECT_BOOL(part[1].m_text == _T("alligator"), true, _T("CBPopup/filteredText"));
    EXPECT_BOOL(part[1].m_icon == icon1, true, _T("CBPopup/filteredIcon"));
    EXPECT_BOOL(part[1].m_subText == _T("L2"), true, _T("CBPopup/filteredSub"));
    ::DeleteObject(icon0);
    ::DeleteObject(icon1);
    return OK(_T("PopupItemsCarryIconSubText"));
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
        { _T("ArrowColorWithDataModel"),   &Test_ArrowColorWithDataModel   },
        // ---- 边框颜色 ----
        { _T("BorderColorDefault"),        &Test_BorderColorDefault        },
        { _T("BorderColorRoundTrip"),      &Test_BorderColorRoundTrip      },
        { _T("BorderColorOrthogonal"),     &Test_BorderColorOrthogonal     },
        { _T("BorderColorPainted"),        &Test_BorderColorPainted        },
        // ---- 下拉浮层落点夹取 ----
        { _T("PopupFitsBelow"),            &Test_PopupFitsBelow            },
        { _T("PopupFlipsAbove"),           &Test_PopupFlipsAbove           },
        { _T("PopupShrinksToLargerSide"),  &Test_PopupShrinksToLargerSide  },
        { _T("PopupClampsRightEdge"),      &Test_PopupClampsRightEdge      },
        { _T("PopupClampsLeftEdge"),       &Test_PopupClampsLeftEdge       },
        { _T("PopupTinyWorkArea"),         &Test_PopupTinyWorkArea         },
        // ---- 浮层项下标映射 ----
        { _T("MapPopupIndexNoFilter"),     &Test_MapPopupIndexNoFilter     },
        { _T("MapPopupIndexWithFilter"),   &Test_MapPopupIndexWithFilter   },
        { _T("MapPopupIndexOutOfRange"),   &Test_MapPopupIndexOutOfRange   },
        // ---- 浮层对控件通知的分类 ----
        { _T("PopupNotifyFromList"),       &Test_PopupNotifyFromList       },
        { _T("PopupNotifyFromScrollBarIgnored"), &Test_PopupNotifyFromScrollBarIgnored },
        { _T("PopupNotifyOtherCodesIgnored"),    &Test_PopupNotifyOtherCodesIgnored    },
        { _T("ItemIconSubText"),                 &Test_ItemIconSubText                 },
        { _T("PopupItemsCarryIconSubText"),      &Test_PopupItemsCarryIconSubText      }
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
