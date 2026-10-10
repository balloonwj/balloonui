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

// 链接模式下，在链接上按下并在链接上松开，构成一次点击：两个事件都被消费，visited 置位。
static Result Test_LinkClickInside()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    l.SetRect(RECT{ 0, 0, 100, 20 });
    bool downConsumed = l.OnLButtonDown(POINT{ 50, 10 }, 0);
    bool consumed = l.OnLButtonUp(POINT{ 50, 10 }, 0);
    EXPECT_BOOL(downConsumed, true,    _T("LinkClick/downConsumed"));
    EXPECT_BOOL(consumed, true,        _T("LinkClick/consumed"));
    EXPECT_BOOL(l.IsVisited(), true,   _T("LinkClick/visited"));
    return OK(_T("LinkClickInside"));
}

// 只收到松开、没有先在本控件上按下，不算点击。典型场景：下拉框的列表在鼠标按下时就完成选择
// 并关闭，随后松开的消息落到列表下方的链接上，不能因此打开链接。
static Result Test_LinkUpWithoutDownIgnored()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    l.SetRect(RECT{ 0, 0, 100, 20 });
    bool consumed = l.OnLButtonUp(POINT{ 50, 10 }, 0);
    EXPECT_BOOL(consumed, false,       _T("LinkUpOnly/notConsumed"));
    EXPECT_BOOL(l.IsVisited(), false,  _T("LinkUpOnly/notVisited"));

    // 一次未构成点击的松开不影响之后正常的点击
    l.OnLButtonDown(POINT{ 50, 10 }, 0);
    l.OnLButtonUp(POINT{ 50, 10 }, 0);
    EXPECT_BOOL(l.IsVisited(), true,   _T("LinkUpOnly/laterClickWorks"));
    return OK(_T("LinkUpWithoutDownIgnored"));
}

// 在链接上按下、移出链接后再松开，视为取消，不算点击；之后单独的一次松开也不算点击。
static Result Test_LinkDownUpOutsideIgnored()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    l.SetRect(RECT{ 0, 0, 100, 20 });
    l.OnLButtonDown(POINT{ 50, 10 }, 0);
    l.OnLButtonUp(POINT{ 999, 999 }, 0);
    EXPECT_BOOL(l.IsVisited(), false,  _T("LinkDownOut/notVisited"));

    // 按下状态已在上一次松开时清除，这里只收到松开，仍不算点击
    l.OnLButtonUp(POINT{ 50, 10 }, 0);
    EXPECT_BOOL(l.IsVisited(), false,  _T("LinkDownOut/staleUpNotVisited"));
    return OK(_T("LinkDownUpOutsideIgnored"));
}

// 禁用的链接，按下再松开也不算点击。
static Result Test_LinkDisabledDownUpIgnored()
{
    DuiLabel l;
    l.SetMode(DuiLabel::ModeLink);
    l.SetEnabled(false);
    l.SetRect(RECT{ 0, 0, 100, 20 });
    bool downConsumed = l.OnLButtonDown(POINT{ 50, 10 }, 0);
    bool consumed = l.OnLButtonUp(POINT{ 50, 10 }, 0);
    EXPECT_BOOL(downConsumed, false,   _T("LinkDisabledDownUp/downNotConsumed"));
    EXPECT_BOOL(consumed, false,       _T("LinkDisabledDownUp/notConsumed"));
    EXPECT_BOOL(l.IsVisited(), false,  _T("LinkDisabledDownUp/notVisited"));
    return OK(_T("LinkDisabledDownUpIgnored"));
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

// 折行文字里有比一行还宽的长词（典型是网址）时，MeasureHeight 量出的高度要容得下 OnPaint 实际画出的全部行
//
// 2026-10-10：DT_CALCRECT 遇到放不下的长词会把矩形加宽、整词放在一行里量，
// 而 OnPaint 在原宽度内画时把长词拆到几行，实测量出 5 行、画了 7 行，按量出的高度排版时最后两行被截掉。
// 这里把标签画到足够高的白底位图上，找出最下面一行有墨迹的位置，量出的高度不得小于它。
static Result Test_MeasureWrapLongWord()
{
    const int kW = 120;          // 标签宽，单位：像素；比文字里的网址窄
    const int kTall = 400;       // 位图高，足够画下全部行
    const int kInkLevel = 160;   // 三个分量都低于该值的像素算作墨迹（黑字白底，抗锯齿边缘较浅）
    DuiLabel l;
    l.SetText(_T("Web: https://test.mingyuan.example/portal\r\n")
              _T("API: https://test.mingyuan.example/api\r\n")
              _T("last line"));
    l.SetWordWrap(true);
    l.SetTextColor(RGB(0, 0, 0));
    l.SetTextAlign(DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    const RECT rc = { 0, 0, kW, kTall };
    l.SetRect(rc);

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kW;
    bi.bmiHeader.biHeight = -kTall;   // 负值表示自上而下
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = ::CreateCompatibleDC(NULL);
    HBITMAP bmp = ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (dc == NULL || bmp == NULL || bits == nullptr)
    {
        return Fail(_T("MeasureWrapLongWord"), _T("create dib failed"));
    }
    HGDIOBJ old = ::SelectObject(dc, bmp);
    ::FillRect(dc, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH));
    l.OnPaint(dc, rc);
    ::GdiFlush();

    int lastInkRow = -1;
    const DWORD* px = (const DWORD*)bits;
    for (int y = 0; y < kTall; ++y)
    {
        for (int x = 0; x < kW; ++x)
        {
            const DWORD v = px[y * kW + x];
            if (((v >> 16) & 0xFF) < kInkLevel && ((v >> 8) & 0xFF) < kInkLevel && (v & 0xFF) < kInkLevel)
            {
                lastInkRow = y;
                break;
            }
        }
    }
    ::SelectObject(dc, old);
    ::DeleteObject(bmp);
    ::DeleteDC(dc);

    const int measured = l.MeasureHeight(kW);
    if (lastInkRow < 0)
    {
        return Fail(_T("MeasureWrapLongWord"), _T("nothing painted"));
    }
    if (measured <= lastInkRow)
    {
        CString d;
        d.Format(_T("measured=%d but ink reaches row %d"), measured, lastInkRow);
        return Fail(_T("MeasureWrapLongWord"), d);
    }
    return OK(_T("MeasureWrapLongWord"));
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

// =================================================================
// 选中模式（SetSelectable）相关用例
// =================================================================

// CharIndexFromCumulativeWidths：边界 + 内部最近邻命中 + 平局规则。
// 平局（距左右边界等距）时返回 i+1（右偏），与实现内 `<` 严格比较一致。
static Result Test_CharIndexFromCumulativeWidths()
{
    // 构造 5 字符、每字符 10px 宽的累计宽度数组：dx[i] = 10*(i+1)。
    // 字符边界位置：0, 10, 20, 30, 40, 50。
    const int dx[] = { 10, 20, 30, 40, 50 };
    const int len = 5;

    // 左外 / 左端：均返回 0（早退）。
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len, -100), 0, _T("CIFCW/leftOut"));
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,   -1), 0, _T("CIFCW/leftMinus1"));
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,    0), 0, _T("CIFCW/leftZero"));

    // 右端 / 右外：均返回 len（早退）。
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,   50), 5, _T("CIFCW/rightEnd"));
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,  100), 5, _T("CIFCW/rightOut"));

    // 内部最近邻（非平局）：x=4 距 0 近(4) vs 10 远(6) → 0
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,    4), 0, _T("CIFCW/char0_nearLeft"));
    // x=6 距 0 远(6) vs 10 近(4) → 1
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,    6), 1, _T("CIFCW/char0_nearRight"));
    // x=14 距 10 近(4) vs 20 远(6) → 1
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,   14), 1, _T("CIFCW/char1_nearLeft"));
    // x=16 距 10 远(6) vs 20 近(4) → 2
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,   16), 2, _T("CIFCW/char1_nearRight"));

    // 平局：x=5（恰在 0/10 中点）→ 实现返回 i+1=1（右偏）
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,    5), 1, _T("CIFCW/char0_tie_rightBias"));
    // 平局：x=25（恰在 20/30 中点）→ 返回 3
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx, len,   25), 3, _T("CIFCW/char2_tie_rightBias"));

    // 防御性：空 / 非法
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(nullptr,   0,  0), 0, _T("CIFCW/nullEmpty"));
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(dx,        0,  5), 0, _T("CIFCW/zeroLen"));
    EXPECT_INT(DuiLabel::CharIndexFromCumulativeWidths(nullptr,   5,  5), 0, _T("CIFCW/nullPositiveLen"));

    return OK(_T("CharIndexFromCumulativeWidths"));
}

// NormalizeSelection：正向、反向、空选区。
static Result Test_NormalizeSelection()
{
    int lo = -1, hi = -1;

    DuiLabel::NormalizeSelection(2, 5, lo, hi);
    EXPECT_INT(lo, 2, _T("Norm/forward_lo"));
    EXPECT_INT(hi, 5, _T("Norm/forward_hi"));

    DuiLabel::NormalizeSelection(5, 2, lo, hi);
    EXPECT_INT(lo, 2, _T("Norm/reverse_lo"));
    EXPECT_INT(hi, 5, _T("Norm/reverse_hi"));

    DuiLabel::NormalizeSelection(3, 3, lo, hi);
    EXPECT_INT(lo, 3, _T("Norm/empty_lo"));
    EXPECT_INT(hi, 3, _T("Norm/empty_hi"));

    DuiLabel::NormalizeSelection(0, 0, lo, hi);
    EXPECT_INT(lo, 0, _T("Norm/zero_lo"));
    EXPECT_INT(hi, 0, _T("Norm/zero_hi"));

    return OK(_T("NormalizeSelection"));
}

// BuildCopyText：全选 / 子串 / 反向 / 空选区 / 空文本 / 越界容错。
static Result Test_BuildCopyText()
{
    // 子串
    if (DuiLabel::BuildCopyText(_T("hello"), 1, 4) != _T("ell"))
    {
        return Fail(_T("BuildCopy/sub"), _T("expected 'ell'"));
    }
    // 反向选区被规范化
    if (DuiLabel::BuildCopyText(_T("hello"), 4, 1) != _T("ell"))
    {
        return Fail(_T("BuildCopy/reverse"), _T("expected 'ell'"));
    }
    // 全选
    if (DuiLabel::BuildCopyText(_T("hello"), 0, 5) != _T("hello"))
    {
        return Fail(_T("BuildCopy/all"), _T("expected 'hello'"));
    }
    // 空选区 → 整段（常见 Ctrl+C 行为）
    if (DuiLabel::BuildCopyText(_T("hello"), 2, 2) != _T("hello"))
    {
        return Fail(_T("BuildCopy/empty_full"), _T("empty sel should copy all"));
    }
    // 空文本
    if (DuiLabel::BuildCopyText(_T(""), 0, 0) != _T(""))
    {
        return Fail(_T("BuildCopy/emptyText"), _T("expected empty"));
    }
    // 越界裁剪：hi>len 被裁到 len
    if (DuiLabel::BuildCopyText(_T("hello"), 1, 999) != _T("ello"))
    {
        return Fail(_T("BuildCopy/hi_overflow"), _T("expected 'ello'"));
    }
    // 越界裁剪：lo<0 被裁到 0
    if (DuiLabel::BuildCopyText(_T("hello"), -3, 2) != _T("he"))
    {
        return Fail(_T("BuildCopy/lo_negative"), _T("expected 'he'"));
    }
    return OK(_T("BuildCopyText"));
}

// SetSelectable：默认关；开启后获得 tab-stop（接焦点收 Ctrl+C/A）；
// 关闭后恢复（非 Link 模式下）。
static Result Test_SetSelectableTogglesTabStop()
{
    DuiLabel l;

    // 默认状态
    EXPECT_BOOL(l.IsSelectable(), false, _T("Sel/default_off"));
    EXPECT_BOOL(l.IsTabStop(),    false, _T("Sel/default_tabStop_off"));

    // 开启 → 既 selectable 又 tabstop
    l.SetSelectable(true);
    EXPECT_BOOL(l.IsSelectable(), true,  _T("Sel/on"));
    EXPECT_BOOL(l.IsTabStop(),    true,  _T("Sel/on_tabStop"));

    // 关闭(非 Link 模式) → 两者都回 false
    l.SetSelectable(false);
    EXPECT_BOOL(l.IsSelectable(), false, _T("Sel/off"));
    EXPECT_BOOL(l.IsTabStop(),    false, _T("Sel/off_tabStop"));

    // Link 模式下开关 selectable，不可破坏 Link 自带的 tab-stop=true
    DuiLabel link;
    link.SetMode(DuiLabel::ModeLink);
    EXPECT_BOOL(link.IsTabStop(), true, _T("Sel/linkBaseline"));
    link.SetSelectable(true);
    EXPECT_BOOL(link.IsTabStop(), true, _T("Sel/link_on_tabStop"));
    link.SetSelectable(false);
    EXPECT_BOOL(link.IsTabStop(), true, _T("Sel/link_off_tabStop_preserved"));

    // 选区高亮色默认值 + 自定义往返
    EXPECT_INT((int)l.GetSelectionColor(), (int)RGB(217, 232, 252), _T("Sel/defaultColor"));
    l.SetSelectionColor(RGB(255, 200, 0));
    EXPECT_INT((int)l.GetSelectionColor(), (int)RGB(255, 200, 0), _T("Sel/colorRoundtrip"));

    return OK(_T("SetSelectableTogglesTabStop"));
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
        { _T("LinkUpWithoutDownIgnored"),  &Test_LinkUpWithoutDownIgnored  },
        { _T("LinkDownUpOutsideIgnored"),  &Test_LinkDownUpOutsideIgnored  },
        { _T("LinkDisabledDownUpIgnored"), &Test_LinkDisabledDownUpIgnored },
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
        { _T("MeasureWrapLongWord"),    &Test_MeasureWrapLongWord   },
        { _T("WrapPreservesText"),      &Test_WrapPreservesText     },
        // ---- 选中模式（SetSelectable） ----
        { _T("CharIndexFromCumulativeWidths"), &Test_CharIndexFromCumulativeWidths },
        { _T("NormalizeSelection"),            &Test_NormalizeSelection            },
        { _T("BuildCopyText"),                 &Test_BuildCopyText                 },
        { _T("SetSelectableTogglesTabStop"),   &Test_SetSelectableTogglesTabStop   }
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
