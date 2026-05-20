#include "stdafx.h"
#include "DuiTier3Tests.h"

#if BUI_FEATURE_SEPARATOR && BUI_FEATURE_BADGE && BUI_FEATURE_GROUPBOX \
 && BUI_FEATURE_SEARCHBOX && BUI_FEATURE_SPINBOX && BUI_FEATURE_SLIDER \
 && BUI_FEATURE_PROGRESSBAR && BUI_FEATURE_SCROLLBAR

// For the HwndHostControl-clipped-to-ScrollView regression test.
#include "../Controls/Input/DuiEditHost.h"
#include "../DuiHost.h"

namespace balloonwjui {

namespace DuiTier3Tests {

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
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) return Fail(name, _T("string mismatch")); \
    } while (0)
#define EXPECT_RECT(rc, L, T, R, B, name) \
    do { const RECT& _r = (rc); \
         if (_r.left!=(L)||_r.top!=(T)||_r.right!=(R)||_r.bottom!=(B)) { \
             CString _d; _d.Format(_T("expected=(%d,%d,%d,%d) got=(%d,%d,%d,%d)"), \
                 (L),(T),(R),(B),_r.left,_r.top,_r.right,_r.bottom); \
             return Fail(name, _d); } \
    } while (0)

class StubChild : public DuiControl
{
public:
    void OnPaint(HDC, const RECT&) override {}
};

// ----- DuiSeparator ---------------------------------------------------

static Result Test_SepDefaults()
{
    DuiSeparator s;
    EXPECT_INT(s.GetOrientation(), DuiSeparator::Horizontal, _T("Sep/orient"));
    EXPECT_INT(s.GetThickness(), 1, _T("Sep/thick"));
    EXPECT_INT(s.GetInset(),     0, _T("Sep/inset"));
    EXPECT_INT((int)s.GetColor(), (int)RGB(220, 220, 224), _T("Sep/color"));
    return OK(_T("SepDefaults"));
}

static Result Test_SepRoundTrip()
{
    DuiSeparator s;
    s.SetOrientation(DuiSeparator::Vertical);
    EXPECT_INT(s.GetOrientation(), DuiSeparator::Vertical, _T("SepRT/orient"));
    s.SetThickness(3);
    EXPECT_INT(s.GetThickness(), 3, _T("SepRT/thick"));
    s.SetThickness(0);                       // clamp >= 1
    EXPECT_INT(s.GetThickness(), 1, _T("SepRT/thickClamp"));
    s.SetInset(8);
    EXPECT_INT(s.GetInset(), 8, _T("SepRT/inset"));
    s.SetInset(-3);                           // clamp >= 0
    EXPECT_INT(s.GetInset(), 0, _T("SepRT/insetClamp"));
    s.SetColor(RGB(50, 60, 70));
    EXPECT_INT((int)s.GetColor(), (int)RGB(50, 60, 70), _T("SepRT/color"));
    return OK(_T("SepRoundTrip"));
}

// ----- DuiBadge -------------------------------------------------------

static Result Test_BadgeDefaults()
{
    DuiBadge b;
    EXPECT_TRUE(b.GetText().IsEmpty(), _T("Bdg/text"));
    EXPECT_TRUE(b.GetHideWhenEmpty(),  _T("Bdg/hideEmpty"));
    EXPECT_TRUE(!b.IsShowing(),        _T("Bdg/notShowing"));
    EXPECT_INT((int)b.GetBgColor(),  (int)RGB(220, 60, 60), _T("Bdg/bg"));
    EXPECT_INT((int)b.GetTextColor(),(int)RGB(255, 255, 255), _T("Bdg/fg"));
    return OK(_T("BadgeDefaults"));
}

static Result Test_BadgeFormatCount()
{
    EXPECT_TRUE(DuiBadge::FormatCount(0).IsEmpty(),    _T("Fmt/0"));
    EXPECT_TRUE(DuiBadge::FormatCount(-3).IsEmpty(),   _T("Fmt/neg"));
    EXPECT_STR(DuiBadge::FormatCount(1),   _T("1"),   _T("Fmt/1"));
    EXPECT_STR(DuiBadge::FormatCount(99),  _T("99"),  _T("Fmt/99"));
    EXPECT_STR(DuiBadge::FormatCount(100), _T("99+"), _T("Fmt/100"));
    EXPECT_STR(DuiBadge::FormatCount(9999),_T("99+"), _T("Fmt/9999"));
    return OK(_T("BadgeFormatCount"));
}

static Result Test_BadgeSetCountUpdatesShowing()
{
    DuiBadge b;
    EXPECT_TRUE(!b.IsShowing(), _T("Cnt/init"));
    b.SetCount(5);
    EXPECT_STR(b.GetText(), _T("5"), _T("Cnt/5"));
    EXPECT_TRUE(b.IsShowing(), _T("Cnt/showing"));
    b.SetCount(0);
    EXPECT_TRUE(b.GetText().IsEmpty(), _T("Cnt/0"));
    EXPECT_TRUE(!b.IsShowing(), _T("Cnt/hideOn0"));
    b.SetHideWhenEmpty(false);
    EXPECT_TRUE(b.IsShowing(), _T("Cnt/showWhenForceShow"));
    return OK(_T("BadgeSetCountUpdatesShowing"));
}

static Result Test_BadgeTruncates()
{
    DuiBadge b;
    b.SetText(_T("abcdef"));
    EXPECT_STR(b.GetText(), _T("abcd"), _T("Trunc/4"));
    return OK(_T("BadgeTruncates"));
}

// ----- DuiGroupBox ----------------------------------------------------

static Result Test_GroupBoxDefaults()
{
    DuiGroupBox g;
    EXPECT_TRUE(g.GetTitle().IsEmpty(), _T("GB/title"));
    EXPECT_INT(g.GetTitleStripHeight(), 24, _T("GB/strip"));
    EXPECT_INT(g.GetCornerRadius(),     6,  _T("GB/r"));
    EXPECT_TRUE(g.GetContent() == nullptr, _T("GB/content"));
    return OK(_T("GroupBoxDefaults"));
}

static Result Test_GroupBoxComputeContent()
{
    RECT outer = { 0, 0, 200, 150 };
    RECT inner = DuiGroupBox::ComputeContentRect(outer, 24, 12, 12, 12, 12);
    // top inset = 24 (strip) + 12 (pad) = 36; sides = 12; bottom = 12
    EXPECT_RECT(inner, 12, 36, 188, 138, _T("GBcr/std"));
    return OK(_T("GroupBoxComputeContent"));
}

static Result Test_GroupBoxComputeTinyOuter()
{
    // Outer too small for the title strip + padding combo; inner
    // clamps to a zero-size rect anchored at the inner-top-left so
    // a content control gets a valid (if empty) rect to lay out into.
    RECT outer = { 0, 0, 10, 10 };
    RECT inner = DuiGroupBox::ComputeContentRect(outer, 24, 12, 12, 12, 12);
    EXPECT_INT(inner.right, inner.left, _T("GBcr/tinyW"));   // both clamped
    EXPECT_INT(inner.bottom, inner.top, _T("GBcr/tinyH"));
    return OK(_T("GroupBoxComputeTinyOuter"));
}

static Result Test_GroupBoxSetContentLaysOut()
{
    DuiGroupBox g;
    StubChild* c;
    g.SetContent(std::unique_ptr<DuiControl>(c = new StubChild()));
    g.Layout(RECT{ 0, 0, 200, 150 });
    EXPECT_TRUE(g.GetContent() == c, _T("GBsc/raw"));
    EXPECT_RECT(c->GetRect(), 12, 36, 188, 138, _T("GBsc/rect"));
    return OK(_T("GroupBoxSetContentLaysOut"));
}

static Result Test_GroupBoxReplaceContent()
{
    DuiGroupBox g;
    StubChild* c1;
    StubChild* c2;
    g.SetContent(std::unique_ptr<DuiControl>(c1 = new StubChild()));
    g.SetContent(std::unique_ptr<DuiControl>(c2 = new StubChild()));
    EXPECT_TRUE(g.GetContent() == c2, _T("GBrc/swapped"));
    g.SetContent(nullptr);
    EXPECT_TRUE(g.GetContent() == nullptr, _T("GBrc/cleared"));
    (void)c1;
    return OK(_T("GroupBoxReplaceContent"));
}

// ----- DuiSearchBox ---------------------------------------------------

static Result Test_SearchBoxDefaults()
{
    DuiSearchBox sb;
    EXPECT_TRUE(sb.GetEdit() != nullptr,    _T("SB/edit"));
    EXPECT_TRUE(sb.GetText().IsEmpty(),     _T("SB/text"));
    EXPECT_TRUE(!sb.IsClearShowing(),       _T("SB/clearHidden"));
    EXPECT_INT(sb.GetGlyphStripWidth(), 24, _T("SB/glyphW"));
    EXPECT_INT(sb.GetClearStripWidth(), 22, _T("SB/clearW"));
    return OK(_T("SearchBoxDefaults"));
}

static Result Test_SearchBoxClearVisibility()
{
    DuiSearchBox sb;
    EXPECT_TRUE(!sb.IsClearShowing(), _T("SBcv/init"));
    sb.SetText(_T("alice"));
    EXPECT_TRUE(sb.IsClearShowing(),  _T("SBcv/typed"));
    sb.SetText(_T(""));
    EXPECT_TRUE(!sb.IsClearShowing(), _T("SBcv/cleared"));
    return OK(_T("SearchBoxClearVisibility"));
}

static Result Test_SearchBoxLayoutCarves()
{
    // M7+ 后 DuiSearchBox 是 DuiEditHost 子类，"内嵌 EDIT 子控件" 没了 ——
    // GetEdit() 返 this，GetEdit()->GetRect() 返 sb 自己的 rect。新的
    // "carve" 是内部 EDIT HWND 在 Layout 时被 inset 到 (left+border+
    // marginL+iconL.width, top+..., right-border-marginR-iconR.width,
    // bottom-...)。无 HWND 单测里只能验证 icon 宽度被 setter 正确记
    // 录、IsClearShowing 行为正确。
    DuiSearchBox sb;
    sb.SetText(_T("xx"));
    sb.SetGlyphStripWidth(20);
    sb.SetClearStripWidth(18);
    sb.Layout(RECT{ 0, 0, 200, 24 });
    EXPECT_INT(sb.GetGlyphStripWidth(), 20, _T("SBl/glyphW"));
    EXPECT_INT(sb.GetClearStripWidth(), 18, _T("SBl/clearW"));
    EXPECT_TRUE(sb.IsClearShowing(),         _T("SBl/withClear"));
    sb.SetText(_T(""));
    sb.Layout(RECT{ 0, 0, 200, 24 });
    EXPECT_TRUE(!sb.IsClearShowing(),        _T("SBl/noClear"));
    return OK(_T("SearchBoxLayoutCarves"));
}

static Result Test_SearchBoxClearClick()
{
    DuiSearchBox sb;
    sb.SetText(_T("hi"));
    sb.Layout(RECT{ 0, 0, 200, 24 });
    RECT cr = sb.GetClearRect();
    POINT mid = { (cr.left + cr.right) / 2, (cr.top + cr.bottom) / 2 };
    bool consumed = sb.OnLButtonUp(mid, 0);
    EXPECT_TRUE(consumed, _T("SBcc/consumed"));
    EXPECT_TRUE(sb.GetText().IsEmpty(), _T("SBcc/cleared"));
    EXPECT_TRUE(!sb.IsClearShowing(),   _T("SBcc/hideAfter"));
    return OK(_T("SearchBoxClearClick"));
}

static Result Test_SearchBoxClearWidthClamps()
{
    DuiSearchBox sb;
    sb.SetClearStripWidth(2);
    EXPECT_INT(sb.GetClearStripWidth(), 14, _T("SBcw/clamp"));
    return OK(_T("SearchBoxClearWidthClamps"));
}

// ----- DuiSpinBox -----------------------------------------------------

static Result Test_SpinBoxDefaults()
{
    DuiSpinBox sp;
    EXPECT_TRUE(sp.GetEdit() != nullptr,    _T("Sp/edit"));
    EXPECT_INT(sp.GetMinValue(), 0,         _T("Sp/min"));
    EXPECT_INT(sp.GetMaxValue(), 100,       _T("Sp/max"));
    EXPECT_INT(sp.GetStep(),     1,         _T("Sp/step"));
    EXPECT_INT(sp.GetValue(),    0,         _T("Sp/value"));
    EXPECT_INT(sp.GetSpinStripWidth(), 18,  _T("Sp/strip"));
    return OK(_T("SpinBoxDefaults"));
}

static Result Test_SpinBoxClampOrWrap()
{
    EXPECT_INT(DuiSpinBox::ClampOrWrap(5,   0, 10, false),  5,  _T("CW/in"));
    EXPECT_INT(DuiSpinBox::ClampOrWrap(-3,  0, 10, false),  0,  _T("CW/under"));
    EXPECT_INT(DuiSpinBox::ClampOrWrap(99,  0, 10, false), 10,  _T("CW/over"));
    EXPECT_INT(DuiSpinBox::ClampOrWrap(11,  0, 10, true),   0,  _T("CW/wrap+"));
    EXPECT_INT(DuiSpinBox::ClampOrWrap(-1,  0, 10, true),  10,  _T("CW/wrap-"));
    EXPECT_INT(DuiSpinBox::ClampOrWrap(22,  0, 10, true),   0,  _T("CW/wrap+2"));
    EXPECT_INT(DuiSpinBox::ClampOrWrap(5, 10, 0, false),    5,  _T("CW/swap"));
    return OK(_T("SpinBoxClampOrWrap"));
}

static Result Test_SpinBoxSetValueClamps()
{
    DuiSpinBox sp;
    sp.SetRange(10, 20);
    sp.SetValue(5,  false);
    EXPECT_INT(sp.GetValue(), 10, _T("SVc/lo"));
    sp.SetValue(99, false);
    EXPECT_INT(sp.GetValue(), 20, _T("SVc/hi"));
    sp.SetValue(15, false);
    EXPECT_INT(sp.GetValue(), 15, _T("SVc/in"));
    return OK(_T("SpinBoxSetValueClamps"));
}

static Result Test_SpinBoxRangeClampsCurrent()
{
    DuiSpinBox sp;
    sp.SetValue(50, false);
    sp.SetRange(0, 10);
    EXPECT_INT(sp.GetValue(), 10, _T("Rng/clampDown"));
    sp.SetValue(0, false);
    sp.SetRange(50, 100);
    EXPECT_INT(sp.GetValue(), 50, _T("Rng/clampUp"));
    return OK(_T("SpinBoxRangeClampsCurrent"));
}

static Result Test_SpinBoxClick()
{
    DuiSpinBox sp;
    sp.SetRange(0, 10);
    sp.SetStep(2);
    sp.SetValue(4, false);
    sp.Layout(RECT{ 0, 0, 100, 24 });

    RECT up = sp.GetUpRect();
    POINT upMid = { (up.left + up.right) / 2, (up.top + up.bottom) / 2 };
    sp.OnLButtonDown(upMid, 0);
    sp.OnLButtonUp  (upMid, 0);
    EXPECT_INT(sp.GetValue(), 6, _T("Sc/up"));

    RECT dn = sp.GetDownRect();
    POINT dnMid = { (dn.left + dn.right) / 2, (dn.top + dn.bottom) / 2 };
    sp.OnLButtonDown(dnMid, 0);
    sp.OnLButtonUp  (dnMid, 0);
    EXPECT_INT(sp.GetValue(), 4, _T("Sc/down"));

    sp.OnLButtonDown(upMid, 0);
    sp.OnLButtonUp  (POINT{ 5, 5 }, 0);
    EXPECT_INT(sp.GetValue(), 4, _T("Sc/dragOut"));
    return OK(_T("SpinBoxClick"));
}

static Result Test_SpinBoxStepClamp()
{
    DuiSpinBox sp;
    sp.SetStep(0);
    EXPECT_INT(sp.GetStep(), 1, _T("Step/zero"));
    sp.SetStep(-5);
    EXPECT_INT(sp.GetStep(), 1, _T("Step/neg"));
    sp.SetStep(7);
    EXPECT_INT(sp.GetStep(), 7, _T("Step/ok"));
    return OK(_T("SpinBoxStepClamp"));
}

// ----- DuiSlider vertical + ticks -------------------------------------

static Result Test_SliderVerticalGeometry()
{
    DuiSlider s;
    s.SetVertical(true);
    s.SetRange(0, 100);
    s.SetPos(50, false);
    s.SetRect(RECT{ 0, 0, 30, 200 });
    POINT thumb = s.ComputeThumbCenter();
    EXPECT_INT(thumb.x, 15, _T("VS/cx"));
    // Track top = m_thumbR (7), bottom = 200 - 7 = 193, span = 186.
    // pos=50/100 -> y = 7 + 186 * 0.5 = 100.
    EXPECT_INT(thumb.y, 100, _T("VS/cy"));
    return OK(_T("SliderVerticalGeometry"));
}

static Result Test_SliderPosFromPointVertical()
{
    DuiSlider s;
    s.SetVertical(true);
    s.SetRange(0, 100);
    s.SetRect(RECT{ 0, 0, 30, 200 });
    EXPECT_INT(s.PosFromPoint(POINT{ 15, 7 }),    0,   _T("PfV/top"));
    EXPECT_INT(s.PosFromPoint(POINT{ 15, 193 }),  100, _T("PfV/bot"));
    EXPECT_INT(s.PosFromPoint(POINT{ 15, 100 }),  50,  _T("PfV/mid"));
    EXPECT_INT(s.PosFromPoint(POINT{ 15, -50 }),  0,   _T("PfV/clampMin"));
    EXPECT_INT(s.PosFromPoint(POINT{ 15, 999 }),  100, _T("PfV/clampMax"));
    return OK(_T("SliderPosFromPointVertical"));
}

static Result Test_SliderTickFreqRoundTrip()
{
    DuiSlider s;
    EXPECT_INT(s.GetTickFrequency(), 0, _T("Tick/default"));
    s.SetTickFrequency(10);
    EXPECT_INT(s.GetTickFrequency(), 10, _T("Tick/10"));
    s.SetTickFrequency(-5);
    EXPECT_INT(s.GetTickFrequency(), 0, _T("Tick/clamp"));
    return OK(_T("SliderTickFreqRoundTrip"));
}

// ----- DuiProgressBar vertical + marquee -----------------------------

static Result Test_ProgressBarVerticalFill()
{
    DuiProgressBar p;
    p.SetVertical(true);
    p.SetRange(0, 100);
    p.SetPos(40, false);
    p.SetRect(RECT{ 0, 0, 20, 100 });
    RECT f = p.ComputeFillRect();
    EXPECT_INT(f.top,    0,  _T("PbV/top"));
    EXPECT_INT(f.bottom, 40, _T("PbV/bot"));
    EXPECT_INT(f.left,   0,  _T("PbV/l"));
    EXPECT_INT(f.right,  20, _T("PbV/r"));
    return OK(_T("ProgressBarVerticalFill"));
}

static Result Test_ProgressBarMarqueeWraps()
{
    DuiProgressBar p;
    p.SetMarquee(true);
    p.SetRect(RECT{ 0, 0, 200, 20 });
    p.SetMarqueePhase(0);
    EXPECT_INT(p.GetMarqueePhase(), 0, _T("Mq/0"));
    p.SetMarqueePhase(DuiProgressBar::MarqueePeriod);
    EXPECT_INT(p.GetMarqueePhase(), 0, _T("Mq/wrap"));
    p.SetMarqueePhase(-50);
    EXPECT_INT(p.GetMarqueePhase(), DuiProgressBar::MarqueePeriod - 50, _T("Mq/negWrap"));
    return OK(_T("ProgressBarMarqueeWraps"));
}

static Result Test_ProgressBarMarqueeRectClips()
{
    DuiProgressBar p;
    p.SetMarquee(true);
    p.SetRect(RECT{ 0, 0, 100, 20 });
    p.SetMarqueePhase(0);
    RECT r0 = p.ComputeMarqueeRect();
    EXPECT_INT(r0.left,  0, _T("Mqr/0Left"));
    EXPECT_INT(r0.right, 0, _T("Mqr/0Right"));
    p.SetMarqueePhase(DuiProgressBar::MarqueePeriod / 2);
    RECT rMid = p.ComputeMarqueeRect();
    EXPECT_TRUE(rMid.right > rMid.left, _T("Mqr/midNonEmpty"));
    return OK(_T("ProgressBarMarqueeRectClips"));
}

// ----- DuiScrollView auto content height -----------------------------

class MeasuringChild : public DuiControl
{
public:
    int wantH = 300;
    void OnPaint(HDC, const RECT&) override {}
    SIZE GetDesiredSize() const override { return SIZE{ 0, wantH }; }
};

static Result Test_ScrollViewAutoHeight()
{
    DuiScrollView sv;
    MeasuringChild* c;
    sv.SetContent(std::unique_ptr<DuiControl>(c = new MeasuringChild()));
    sv.SetAutoContentHeight(true);
    sv.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_INT(sv.GetContentHeight(), 300, _T("SVa/h300"));

    c->wantH = 50;
    sv.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_INT(sv.GetContentHeight(), 50, _T("SVa/h50"));
    return OK(_T("ScrollViewAutoHeight"));
}

static Result Test_ScrollViewAutoHeightZeroIgnored()
{
    DuiScrollView sv;
    MeasuringChild* c;
    sv.SetContent(std::unique_ptr<DuiControl>(c = new MeasuringChild()));
    c->wantH = 0;
    sv.SetContentHeight(123);
    sv.SetAutoContentHeight(true);
    sv.Layout(RECT{ 0, 0, 200, 100 });
    EXPECT_INT(sv.GetContentHeight(), 123, _T("SVa/zeroSkip"));
    return OK(_T("ScrollViewAutoHeightZeroIgnored"));
}

// Integration: a real DuiVBox (not the MeasuringChild stub) drives
// SetAutoContentHeight via its GetDesiredSize. Mirrors how DuiGallery's
// page-VBox feeds the gallery scroll view after the contentH=1500
// hardcoded fallback is removed. The expected height matches the formula
// asserted in DuiLayoutTests::Test_VBox_Desired_FixedSum and friends:
// padT + sum(fixedMain + marginT/B) + gap*(visN-1) + padB.
static Result Test_VBoxDrivesScrollViewAutoHeight()
{
    DuiVBox* page = new DuiVBox();
    page->SetPadding(20);          // mimics DuiGallery NewPage()'s kPageMargin
    page->SetGap(4);               // mimics NewPage() gap
    // 3 fixed-height "rows" sized like a section header + body + button row.
    page->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                   DuiLayout::Hint().Fixed(28));   // section header
    page->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                   DuiLayout::Hint().Fixed(60));   // body / variant row
    page->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                   DuiLayout::Hint().Fixed(36));   // button row

    DuiScrollView sv;
    sv.SetContent(std::unique_ptr<DuiControl>(page));
    sv.SetAutoContentHeight(true);
    sv.Layout(RECT{ 0, 0, 400, 200 });

    // Expected = padT20 + 28 + gap4 + 60 + gap4 + 36 + padB20 = 172
    EXPECT_INT(sv.GetContentHeight(), 172,
               _T("VBoxDrivesScrollViewAutoHeight/contentH"));
    return OK(_T("VBoxDrivesScrollViewAutoHeight"));
}

// ----- HwndHostControl-inside-ScrollView clipping regression --------
//
// What this guards: when a DuiScrollView contains a DUI subtree with an
// HwndHostControl descendant (e.g. DuiEditHost / DuiRichEditHost), scrolling
// changes the descendant's DUI rect to track the scrolled content. The real
// Win32 EDIT/RICHEDIT HWND must be repositioned/clipped so its visible
// pixels stay within the ScrollView's viewport — otherwise the slice of the
// HWND that pokes above (or below) the viewport overpaints whatever sibling
// is up there, and WS_CLIPCHILDREN on the host blocks the host's back-buffer
// from cleaning it up. The visible bug for DuiGallery is on the
// RichEdit/Edit pages: scrolling makes the EDIT's white background overdraw
// the top of each tab label in the tab strip above the scroll view.
//
// The test is observable-property based, not implementation-shape based —
// it asserts only "after scrolling, the EDIT's effective on-screen rect
// stays inside the ScrollView's screen rect." A fix can ship as
// SetWindowRgn, ShowWindow(SW_HIDE) when fully out, SetWindowPos to a
// clipped rect, etc. — any of those make this test pass.
//
// "Effective on-screen rect" = HWND's screen window rect intersected with
// its window region (if any), and considered empty if the HWND is hidden.

static RECT GetEffectiveScreenRect_(HWND hwnd)
{
    RECT empty = { 0, 0, 0, 0 };
    if (!::IsWindowVisible(hwnd))
    {
        return empty;
    }
    RECT rcWindow;
    ::GetWindowRect(hwnd, &rcWindow);    // screen coords
    HRGN rgn = ::CreateRectRgn(0, 0, 0, 0);
    int rgnRes = ::GetWindowRgn(hwnd, rgn);

    // GetWindowRgn return values to distinguish:
    //   ERROR        — no window region set; the OS clips to the full HWND
    //                  rect, so effective rect = rcWindow.
    //   NULLREGION   — region is set AND empty; the HWND paints nothing,
    //                  so effective rect is empty (NOT rcWindow!). Earlier
    //                  bug here: lumped NULLREGION with ERROR and reported
    //                  full rcWindow → "fully out" tests passed vacuously
    //                  before the lib fix and reported false-leak after.
    //   SIMPLEREGION / COMPLEXREGION — region has area; clip rcWindow to
    //                  the region's bbox (translated from HWND-local to
    //                  screen coords).
    RECT result;
    if (rgnRes == ERROR)
    {
        result = rcWindow;
    }
    else if (rgnRes == NULLREGION)
    {
        result = empty;
    }
    else
    {
        RECT rgnBox = empty;
        ::GetRgnBox(rgn, &rgnBox);   // HWND-local
        RECT rgnScreen = { rcWindow.left + rgnBox.left,
                           rcWindow.top  + rgnBox.top,
                           rcWindow.left + rgnBox.right,
                           rcWindow.top  + rgnBox.bottom };
        if (!::IntersectRect(&result, &rcWindow, &rgnScreen))
        {
            result = empty;
        }
    }
    ::DeleteObject(rgn);
    return result;
}

// Returns true iff `inner` is fully contained in `outer` (or `inner` is empty,
// in which case there's trivially nothing to leak).
static bool RectContains_(const RECT& outer, const RECT& inner)
{
    if (::IsRectEmpty(&inner))
    {
        return true;
    }
    return inner.left   >= outer.left
        && inner.top    >= outer.top
        && inner.right  <= outer.right
        && inner.bottom <= outer.bottom;
}

// RAII fixture: hidden off-screen popup + DuiHost + a 36-px tab stub at top
// and a DuiScrollView below. Caller fills `content` with whatever child mix
// they want, then calls Realize() to attach the tree and run Layout. The
// popup is shown off-screen (SW_SHOWNOACTIVATE at -20000,-20000) so chain
// visibility evaluates TRUE for the EDIT — otherwise IsWindowVisible would
// be FALSE and our effective-rect assertions would pass vacuously.
struct HwndHostInScrollViewFixture
{
    HWND               hPopup  = nullptr;
    DuiHost            host;
    DuiScrollView*     svPtr   = nullptr;
    DuiVBox*           content = nullptr;
    // Hold ownership of the un-attached pieces between Init() and Realize().
    // Without these, the local unique_ptrs in Init() would destroy the
    // controls at end-of-scope and leave svPtr/content dangling.
    std::unique_ptr<DuiVBox>       rootHolder;
    std::unique_ptr<DuiScrollView> svHolder;
    std::unique_ptr<DuiVBox>       contentHolder;

    bool Init()
    {
        HINSTANCE hInst = ::GetModuleHandle(nullptr);
        hPopup = ::CreateWindowEx(0, _T("STATIC"), _T(""),
                                  WS_OVERLAPPED | WS_POPUP,
                                  -20000, -20000, 400, 400, nullptr, nullptr,
                                  hInst, nullptr);
        if (!hPopup)
        {
            return false;
        }
        ::ShowWindow(hPopup, SW_SHOWNOACTIVATE);
        host.Create(hPopup, RECT{ 0, 0, 400, 400 }, nullptr,
                    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);

        rootHolder.reset(new DuiVBox());
        rootHolder->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(36));
        svHolder.reset(new DuiScrollView());
        svPtr = svHolder.get();
        contentHolder.reset(new DuiVBox());
        content = contentHolder.get();
        return true;
    }

    // Hand `content` (caller-populated) over to the ScrollView, then the
    // ScrollView over to the root, then root to the host. After this, layout
    // has run and the EDIT(s) are at their initial positions.
    void Realize()
    {
        svPtr->SetContent(std::move(contentHolder));
        svPtr->SetAutoContentHeight(true);
        rootHolder->AddChild(std::move(svHolder), DuiLayout::Hint().Weight(1));
        host.SetRoot(std::move(rootHolder));
        host.SetWindowPos(nullptr, 0, 0, 400, 400,
                          SWP_NOZORDER | SWP_NOACTIVATE);
    }

    RECT SvScreenRect() const
    {
        RECT r = svPtr->GetRect();
        POINT lt = { r.left, r.top };
        POINT rb = { r.right, r.bottom };
        ::ClientToScreen(host.m_hWnd, &lt);
        ::ClientToScreen(host.m_hWnd, &rb);
        return { lt.x, lt.y, rb.x, rb.y };
    }

    ~HwndHostInScrollViewFixture()
    {
        // host's CWindow dtor doesn't tear down the HWND for us; rely on
        // popup destruction to nuke the whole subtree.
        if (hPopup)
        {
            ::DestroyWindow(hPopup);
        }
    }
};

// Wrong fixture init (CreateWindowEx fail) is rare but flag it instead of
// silently degrading to vacuous-pass results.
#define INIT_FIXTURE(fx, name) \
    if (!fx.Init()) { return Fail(name, _T("fixture init failed")); }

// Convenience: format a rect for the failure detail string.
static CString FmtRectT3_(const RECT& r)
{
    CString s;
    s.Format(_T("(%d,%d,%d,%d)"), r.left, r.top, r.right, r.bottom);
    return s;
}

// ===== Case 1: fully scrolled out (top) ===============================
//
// EDIT is the FIRST content child; scroll-to-bottom puts it far above the
// ScrollView. Without the fix, the OS still paints the EDIT at its full
// position — overlapping siblings (in DuiGallery, the tab strip above sv).

static Result Test_HwndHostInSV_FullyOut_Above()
{
    HwndHostInScrollViewFixture fx;
    INIT_FIXTURE(fx, _T("FullyOutAbove/init"));

    auto edit = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    DuiEditHost* editPtr = edit.get();
    fx.content->AddChild(std::move(edit), DuiLayout::Hint().Fixed(100));
    for (int i = 0; i < 12; ++i)
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(100));
    }
    fx.Realize();
    fx.svPtr->SetScrollPos(fx.svPtr->GetScrollBar()->GetMax());

    HWND hEdit = editPtr->GetHostedHwnd();
    if (!hEdit) { return Fail(_T("FullyOutAbove/noHwnd"), _T("EDIT not created")); }

    RECT editScreen = GetEffectiveScreenRect_(hEdit);
    RECT svScreen   = fx.SvScreenRect();
    if (!RectContains_(svScreen, editScreen))
    {
        CString d;
        d.Format(_T("editScreen=%s leaks outside svScreen=%s"),
                 (LPCTSTR)FmtRectT3_(editScreen), (LPCTSTR)FmtRectT3_(svScreen));
        return Fail(_T("FullyOutAbove/leaks"), d);
    }
    return OK(_T("HwndHostInSV_FullyOut_Above"));
}

// ===== Case 2: partially out (top half above viewport) ================
//
// This is the exact shape of the DuiGallery RichEdit-page bug: the EDIT's
// upper half projects into the tab-strip area above the ScrollView. The
// HWND must be clipped (region) so its visible band stays inside the
// ScrollView — without the fix, partial overlap leaks anyway because
// the part of the HWND that's still in-view paints at full size.

static Result Test_HwndHostInSV_PartiallyOut_Top()
{
    HwndHostInScrollViewFixture fx;
    INIT_FIXTURE(fx, _T("PartialTop/init"));

    // Insert ~250 px of fillers BEFORE the EDIT so a moderate scroll puts
    // the EDIT half-in / half-above the viewport.
    for (int i = 0; i < 5; ++i)   // 5 × 50 = 250 px
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(50));
    }
    auto edit = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    DuiEditHost* editPtr = edit.get();
    fx.content->AddChild(std::move(edit), DuiLayout::Hint().Fixed(100));
    for (int i = 0; i < 10; ++i)  // fill the rest so content > viewport
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(100));
    }
    fx.Realize();

    // Compute scrollPos that places EDIT straddling the viewport top.
    // EDIT content-y = 250 (after the 5 fillers); viewport top in content
    // coords = scrollPos. We want scrollPos = 250 + 50 = 300, so EDIT goes
    // from content-y 250..350, viewport sees content-y 300..(300+364)=664,
    // EDIT visible portion: 50 px (content-y 300..350) → host-y 36..86
    // EDIT's full host-y: 36 - (300-250) .. 36 + (350-300) = -14 .. 86
    fx.svPtr->SetScrollPos(300);

    HWND hEdit = editPtr->GetHostedHwnd();
    if (!hEdit) { return Fail(_T("PartialTop/noHwnd"), _T("EDIT not created")); }

    RECT editDui = editPtr->GetRect();
    // Precondition: EDIT must straddle viewport top — its top above sv top,
    // its bottom below sv top. Otherwise this test's premise is wrong.
    RECT svDui = fx.svPtr->GetRect();
    if (!(editDui.top < svDui.top && editDui.bottom > svDui.top))
    {
        CString d;
        d.Format(_T("expected editDui to straddle svDui.top=%d; got editDui=%s"),
                 svDui.top, (LPCTSTR)FmtRectT3_(editDui));
        return Fail(_T("PartialTop/badPrecondition"), d);
    }

    RECT editScreen = GetEffectiveScreenRect_(hEdit);
    RECT svScreen   = fx.SvScreenRect();
    if (!RectContains_(svScreen, editScreen))
    {
        CString d;
        d.Format(_T("editScreen=%s leaks outside svScreen=%s"),
                 (LPCTSTR)FmtRectT3_(editScreen), (LPCTSTR)FmtRectT3_(svScreen));
        return Fail(_T("PartialTop/leaks"), d);
    }
    // Stronger: the EDIT must still be visibly painting *something* (the
    // 50 px in-view portion). Empty effectiveRect would mean the fix went
    // too far and hid an EDIT that should still show its bottom half.
    if (::IsRectEmpty(&editScreen))
    {
        return Fail(_T("PartialTop/overClipped"),
                    _T("EDIT effective rect is empty but bottom half should still show"));
    }
    return OK(_T("HwndHostInSV_PartiallyOut_Top"));
}

// ===== Case 3: fully visible — must NOT be clipped ====================
//
// At scrollPos=0 with the EDIT in clear viewport space, the fix must not
// over-clip: effective rect should equal the EDIT's full DUI rect (in
// screen coords). Catches the bug class "fix always sets a region even
// when child is fully visible" — wasteful and easy to mess up.

static Result Test_HwndHostInSV_FullyVisible_NotClipped()
{
    HwndHostInScrollViewFixture fx;
    INIT_FIXTURE(fx, _T("FullyVisible/init"));

    auto edit = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    DuiEditHost* editPtr = edit.get();
    fx.content->AddChild(std::move(edit), DuiLayout::Hint().Fixed(100));
    // Add fillers so a scroll bar appears (otherwise auto-height collapses
    // content to viewport height and there's nothing to test).
    for (int i = 0; i < 12; ++i)
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(100));
    }
    fx.Realize();
    fx.svPtr->SetScrollPos(0);

    HWND hEdit = editPtr->GetHostedHwnd();
    if (!hEdit) { return Fail(_T("FullyVisible/noHwnd"), _T("EDIT not created")); }

    RECT editDui = editPtr->GetRect();
    RECT svDui   = fx.svPtr->GetRect();
    // Precondition: EDIT fully inside svDui.
    RECT inter;
    if (!::IntersectRect(&inter, &editDui, &svDui) || !::EqualRect(&inter, &editDui))
    {
        return Fail(_T("FullyVisible/badPrecondition"),
                    _T("EDIT not fully inside ScrollView"));
    }

    // Translate editDui to screen coords for comparison with effective rect.
    POINT lt = { editDui.left, editDui.top };
    POINT rb = { editDui.right, editDui.bottom };
    ::ClientToScreen(fx.host.m_hWnd, &lt);
    ::ClientToScreen(fx.host.m_hWnd, &rb);
    RECT editDuiScreen = { lt.x, lt.y, rb.x, rb.y };

    RECT editScreen = GetEffectiveScreenRect_(hEdit);
    // EditHost shrinks the EDIT slightly inside its DUI rect for border /
    // margin (see DuiEditHost::Layout — InflateRect(-1,-1) + margins). So
    // we don't require exact-match; we require the EDIT to occupy at least
    // the inner portion (which is editDui inflated by -1 — generous floor).
    RECT minEdit = editDuiScreen;
    ::InflateRect(&minEdit, -8, -8);   // generous: real inset is 1+marginAny
    if (!RectContains_(editScreen, minEdit))
    {
        CString d;
        d.Format(_T("editScreen=%s does not contain minEdit=%s (over-clipped?)"),
                 (LPCTSTR)FmtRectT3_(editScreen), (LPCTSTR)FmtRectT3_(minEdit));
        return Fail(_T("FullyVisible/overClipped"), d);
    }
    return OK(_T("HwndHostInSV_FullyVisible_NotClipped"));
}

// ===== Case 4: scroll out then back — region must be cleared ===========
//
// Scroll-down clips the EDIT; scroll-back-to-top must restore it (no
// stale region from the previous scroll position). Catches "fix sets
// region but never clears it" bug class.

static Result Test_HwndHostInSV_RegionRestoredOnScrollBack()
{
    HwndHostInScrollViewFixture fx;
    INIT_FIXTURE(fx, _T("ScrollBack/init"));

    auto edit = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    DuiEditHost* editPtr = edit.get();
    fx.content->AddChild(std::move(edit), DuiLayout::Hint().Fixed(100));
    for (int i = 0; i < 12; ++i)
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(100));
    }
    fx.Realize();

    // Step 1: scroll to bottom (clips the EDIT)
    fx.svPtr->SetScrollPos(fx.svPtr->GetScrollBar()->GetMax());
    // Step 2: scroll back to top (must un-clip)
    fx.svPtr->SetScrollPos(0);

    HWND hEdit = editPtr->GetHostedHwnd();
    if (!hEdit) { return Fail(_T("ScrollBack/noHwnd"), _T("EDIT not created")); }

    // Same assertion as FullyVisible — after scrolling back, EDIT must be
    // visibly the same as if we'd never scrolled.
    RECT editDui = editPtr->GetRect();
    POINT lt = { editDui.left, editDui.top };
    POINT rb = { editDui.right, editDui.bottom };
    ::ClientToScreen(fx.host.m_hWnd, &lt);
    ::ClientToScreen(fx.host.m_hWnd, &rb);
    RECT editDuiScreen = { lt.x, lt.y, rb.x, rb.y };
    RECT minEdit = editDuiScreen;
    ::InflateRect(&minEdit, -8, -8);

    RECT editScreen = GetEffectiveScreenRect_(hEdit);
    if (!RectContains_(editScreen, minEdit))
    {
        CString d;
        d.Format(_T("after scroll-back: editScreen=%s does not contain minEdit=%s (stale region)"),
                 (LPCTSTR)FmtRectT3_(editScreen), (LPCTSTR)FmtRectT3_(minEdit));
        return Fail(_T("ScrollBack/staleRegion"), d);
    }
    return OK(_T("HwndHostInSV_RegionRestoredOnScrollBack"));
}

// ===== Case 5: multiple HwndHostControl siblings clipped independently
//
// Two EDITs at different content y's; scroll to a position where one is
// fully out (above) and the other is fully in. Catches bug class "fix
// only clips the first/last/wrong child".

// Pump any pending paint messages for the popup so the post-scroll
// GetUpdateRect check isn't polluted by leftover startup paint regions.
static void DrainPaintMessages_(HWND hwnd)
{
    MSG msg;
    while (::PeekMessage(&msg, hwnd, WM_PAINT, WM_PAINT, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    // UpdateWindow flushes any remaining update region synchronously.
    ::UpdateWindow(hwnd);
}

// ===== Scroll-driven HWND ghost: paint must be flushed synchronously ===
//
// Without sync paint after a scroll-driven HWND move, the EDIT moves to
// its new screen position via SetWindowPos but the parent's old-EDIT-area
// stays unrepainted until the next async WM_PAINT. WS_CLIPCHILDREN +
// suppressed WM_ERASEBKGND on the host means the OS doesn't auto-erase the
// exposed region, so the EDIT's previous pixels visibly linger as a
// "ghost" / drag trail until the next paint cycle catches up.
//
// Invariant we lock in: after SetScrollPos returns, the host's update
// region is empty — meaning the paint that covers the OLD EDIT location
// has already been processed synchronously, no async lag window left.
//
// Without the fix, OnSbScrolled only calls Invalidate (async); GetUpdateRect
// still reports a non-empty region. With the fix (UpdateWindow / similar),
// the region is processed inline before SetScrollPos returns.

static Result Test_ScrollSync_FlushesPendingPaint()
{
    HwndHostInScrollViewFixture fx;
    INIT_FIXTURE(fx, _T("ScrollSync/init"));

    auto edit = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    fx.content->AddChild(std::move(edit), DuiLayout::Hint().Fixed(100));
    for (int i = 0; i < 12; ++i)
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(100));
    }
    fx.Realize();
    DrainPaintMessages_(fx.host.m_hWnd);

    // Sanity: nothing pending before our scroll call. If this fails, our
    // scroll-flushing assertion below is meaningless.
    if (::GetUpdateRect(fx.host.m_hWnd, nullptr, FALSE))
    {
        return Fail(_T("ScrollSync/preBaseline"),
                    _T("update region not empty before SetScrollPos"));
    }

    fx.svPtr->SetScrollPos(300);

    // Hard assertion: SetScrollPos must leave the host with no pending
    // paint. Without the lib fix, Invalidate just marks dirty + returns;
    // GetUpdateRect == TRUE here means async paint was deferred → ghost.
    RECT updRect = {};
    if (::GetUpdateRect(fx.host.m_hWnd, &updRect, FALSE))
    {
        CString d;
        d.Format(_T("update region pending after SetScrollPos: %s"),
                 (LPCTSTR)FmtRectT3_(updRect));
        return Fail(_T("ScrollSync/notFlushed"), d);
    }
    return OK(_T("ScrollSync_FlushesPendingPaint"));
}

// Repeated SetScrollPos calls (simulating thumb drag / mouse wheel spam):
// every call must flush; no per-call pending paint accumulating.
static Result Test_ScrollSync_RepeatedScrollsAllFlushed()
{
    HwndHostInScrollViewFixture fx;
    INIT_FIXTURE(fx, _T("ScrollSyncRep/init"));

    auto edit = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    fx.content->AddChild(std::move(edit), DuiLayout::Hint().Fixed(100));
    for (int i = 0; i < 12; ++i)
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(100));
    }
    fx.Realize();
    DrainPaintMessages_(fx.host.m_hWnd);

    // Walk through 6 different scroll positions, simulate "user dragging
    // the thumb fast". After EACH, update region must be empty.
    int positions[] = { 50, 150, 250, 400, 200, 0 };
    for (int p : positions)
    {
        fx.svPtr->SetScrollPos(p);
        if (::GetUpdateRect(fx.host.m_hWnd, nullptr, FALSE))
        {
            CString d;
            d.Format(_T("update region pending after SetScrollPos(%d)"), p);
            return Fail(_T("ScrollSyncRep/notFlushedAt"), d);
        }
    }
    return OK(_T("ScrollSync_RepeatedScrollsAllFlushed"));
}

// Sanity: confirm the scroll actually moves the EDIT's HWND. Otherwise
// the FlushesPendingPaint test could pass trivially if SetScrollPos was
// a no-op (e.g. range too small, content fits viewport). Catches setup
// bugs in our fixture rather than in the library.
static Result Test_ScrollSync_ActuallyMovedEditHwnd()
{
    HwndHostInScrollViewFixture fx;
    INIT_FIXTURE(fx, _T("ScrollSyncMove/init"));

    auto edit = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    DuiEditHost* editPtr = edit.get();
    fx.content->AddChild(std::move(edit), DuiLayout::Hint().Fixed(100));
    for (int i = 0; i < 12; ++i)
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(100));
    }
    fx.Realize();

    HWND hEdit = editPtr->GetHostedHwnd();
    if (!hEdit) { return Fail(_T("ScrollSyncMove/noHwnd"), _T("EDIT not created")); }

    RECT before;
    ::GetWindowRect(hEdit, &before);

    fx.svPtr->SetScrollPos(200);

    RECT after;
    ::GetWindowRect(hEdit, &after);
    if (before.top == after.top)
    {
        CString d;
        d.Format(_T("EDIT did not move; before.top=%d after.top=%d (test setup may be wrong)"),
                 before.top, after.top);
        return Fail(_T("ScrollSyncMove/noMove"), d);
    }
    return OK(_T("ScrollSync_ActuallyMovedEditHwnd"));
}

static Result Test_HwndHostInSV_MultipleSiblingsClippedIndependently()
{
    HwndHostInScrollViewFixture fx;
    INIT_FIXTURE(fx, _T("MultiSib/init"));

    auto eA = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    DuiEditHost* eAPtr = eA.get();
    fx.content->AddChild(std::move(eA), DuiLayout::Hint().Fixed(80));
    for (int i = 0; i < 8; ++i)   // 8 × 50 = 400 px filler between
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(50));
    }
    auto eB = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    DuiEditHost* eBPtr = eB.get();
    fx.content->AddChild(std::move(eB), DuiLayout::Hint().Fixed(80));
    for (int i = 0; i < 6; ++i)
    {
        fx.content->AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                             DuiLayout::Hint().Fixed(50));
    }
    fx.Realize();

    // Scroll so eA is way above viewport, eB is in viewport.
    // content layout: eA y=0..80, fillers y=80..480, eB y=480..560, fillers y=560..860
    // viewport is 364 px tall; pick scrollPos = 480 → viewport sees 480..844
    // eA host-y = 36 + 0 - 480 = -444 .. -364 (well above)
    // eB host-y = 36 + 480 - 480 = 36 .. 116 (top of viewport)
    fx.svPtr->SetScrollPos(480);

    RECT svScreen = fx.SvScreenRect();
    HWND hA = eAPtr->GetHostedHwnd();
    HWND hB = eBPtr->GetHostedHwnd();
    if (!hA || !hB)
    {
        return Fail(_T("MultiSib/noHwnd"), _T("one or both EDITs not created"));
    }

    RECT screenA = GetEffectiveScreenRect_(hA);
    RECT screenB = GetEffectiveScreenRect_(hB);

    // eA must not leak outside viewport
    if (!RectContains_(svScreen, screenA))
    {
        CString d;
        d.Format(_T("eA=%s leaks outside svScreen=%s"),
                 (LPCTSTR)FmtRectT3_(screenA), (LPCTSTR)FmtRectT3_(svScreen));
        return Fail(_T("MultiSib/eA_leaks"), d);
    }
    // eB must not leak either
    if (!RectContains_(svScreen, screenB))
    {
        CString d;
        d.Format(_T("eB=%s leaks outside svScreen=%s"),
                 (LPCTSTR)FmtRectT3_(screenB), (LPCTSTR)FmtRectT3_(svScreen));
        return Fail(_T("MultiSib/eB_leaks"), d);
    }
    // eB should still be visible (it's the one that's in viewport)
    if (::IsRectEmpty(&screenB))
    {
        return Fail(_T("MultiSib/eB_overClipped"),
                    _T("eB is in viewport but effective rect is empty"));
    }
    return OK(_T("HwndHostInSV_MultipleSiblingsClippedIndependently"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_STR
#undef EXPECT_RECT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("SepDefaults"),                &Test_SepDefaults                },
        { _T("SepRoundTrip"),               &Test_SepRoundTrip               },
        { _T("BadgeDefaults"),              &Test_BadgeDefaults              },
        { _T("BadgeFormatCount"),           &Test_BadgeFormatCount           },
        { _T("BadgeSetCountUpdatesShowing"),&Test_BadgeSetCountUpdatesShowing},
        { _T("BadgeTruncates"),             &Test_BadgeTruncates             },
        { _T("GroupBoxDefaults"),           &Test_GroupBoxDefaults           },
        { _T("GroupBoxComputeContent"),     &Test_GroupBoxComputeContent     },
        { _T("GroupBoxComputeTinyOuter"),   &Test_GroupBoxComputeTinyOuter   },
        { _T("GroupBoxSetContentLaysOut"),  &Test_GroupBoxSetContentLaysOut  },
        { _T("GroupBoxReplaceContent"),     &Test_GroupBoxReplaceContent     },
        { _T("SearchBoxDefaults"),          &Test_SearchBoxDefaults          },
        { _T("SearchBoxClearVisibility"),   &Test_SearchBoxClearVisibility   },
        { _T("SearchBoxLayoutCarves"),      &Test_SearchBoxLayoutCarves      },
        { _T("SearchBoxClearClick"),        &Test_SearchBoxClearClick        },
        { _T("SearchBoxClearWidthClamps"),  &Test_SearchBoxClearWidthClamps  },
        { _T("SpinBoxDefaults"),            &Test_SpinBoxDefaults            },
        { _T("SpinBoxClampOrWrap"),         &Test_SpinBoxClampOrWrap         },
        { _T("SpinBoxSetValueClamps"),      &Test_SpinBoxSetValueClamps      },
        { _T("SpinBoxRangeClampsCurrent"),  &Test_SpinBoxRangeClampsCurrent  },
        { _T("SpinBoxClick"),               &Test_SpinBoxClick               },
        { _T("SpinBoxStepClamp"),           &Test_SpinBoxStepClamp           },
        { _T("SliderVerticalGeometry"),     &Test_SliderVerticalGeometry     },
        { _T("SliderPosFromPointVertical"), &Test_SliderPosFromPointVertical },
        { _T("SliderTickFreqRoundTrip"),    &Test_SliderTickFreqRoundTrip    },
        { _T("ProgressBarVerticalFill"),    &Test_ProgressBarVerticalFill    },
        { _T("ProgressBarMarqueeWraps"),    &Test_ProgressBarMarqueeWraps    },
        { _T("ProgressBarMarqueeRectClips"),&Test_ProgressBarMarqueeRectClips},
        { _T("ScrollViewAutoHeight"),       &Test_ScrollViewAutoHeight       },
        { _T("ScrollViewAutoHeightZeroIgnored"), &Test_ScrollViewAutoHeightZeroIgnored },
        { _T("VBoxDrivesScrollViewAutoHeight"),  &Test_VBoxDrivesScrollViewAutoHeight  },
        { _T("HwndHostInSV_FullyOut_Above"),                       &Test_HwndHostInSV_FullyOut_Above                       },
        { _T("HwndHostInSV_PartiallyOut_Top"),                     &Test_HwndHostInSV_PartiallyOut_Top                     },
        { _T("HwndHostInSV_FullyVisible_NotClipped"),              &Test_HwndHostInSV_FullyVisible_NotClipped              },
        { _T("HwndHostInSV_RegionRestoredOnScrollBack"),           &Test_HwndHostInSV_RegionRestoredOnScrollBack           },
        { _T("HwndHostInSV_MultipleSiblingsClippedIndependently"), &Test_HwndHostInSV_MultipleSiblingsClippedIndependently },
        { _T("ScrollSync_FlushesPendingPaint"),                    &Test_ScrollSync_FlushesPendingPaint                    },
        { _T("ScrollSync_RepeatedScrollsAllFlushed"),              &Test_ScrollSync_RepeatedScrollsAllFlushed              },
        { _T("ScrollSync_ActuallyMovedEditHwnd"),                  &Test_ScrollSync_ActuallyMovedEditHwnd                  },
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
    summary.Format(_T("[summary] DuiTier3Tests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiTier3Tests

} // namespace balloonwjui

#endif // tier3 features all on
