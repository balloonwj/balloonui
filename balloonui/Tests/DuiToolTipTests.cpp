#include "stdafx.h"
#include "DuiToolTipTests.h"

#if BUI_FEATURE_TOOLTIP

#include "../DuiControl.h"
#include "../DuiHost.h"
#include "../DuiResMgr.h"
#include "../Controls/List/DuiMenuPlacement.h"

#include <memory>

namespace balloonwjui {

namespace DuiToolTipTests {

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

#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("got '%s'"), (LPCTSTR)_a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_INT(actual, expected, name) \
    do { int _a = (int)(actual); int _e = (int)(expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)

// Upper bound for waiting until a tip shows. The pump returns as soon as it
// shows, so a generous cap costs nothing when the case passes.
const DWORD kShowWaitMs = 2000;
// How long a case watches for a tip that must NOT show: ten times the delay
// those cases use (kQuickDelayMs), so a late timer is still caught.
const DWORD kQuietWaitMs = 200;
// Popup delay used by the cases that wait for a tip. Mouse and keyboard
// messages are sent with SendMessage, which runs synchronously, so a press
// always lands before the timer however short the delay is.
const UINT kQuickDelayMs = 20;
// Tooltip text registered by the cases.
const LPCTSTR kTipText = _T("tip");
// Width that never wraps the short test strings, in pixels.
const int kNoWrapWidth = 10000;
// Screen point passed to OnHover by the cases that have no host window.
const POINT kHoverPt = { 100, 100 };

class StubCtrl : public DuiControl
{
public:
    void OnPaint(HDC, const RECT&) override {}
};

// A control that consumes the mouse wheel, like a scroll view with
// something to scroll.
class WheelCtrl : public StubCtrl
{
public:
    bool OnMouseWheel(POINT, short, UINT) override { return true; }
};

// Default empty registry returns empty.
static Result Test_Defaults()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    EXPECT_STR(m.GetText(&c), _T(""), _T("Defaults/empty"));
    EXPECT_BOOL(m.IsShowing(), false, _T("Defaults/notShowing"));
    return OK(_T("Defaults"));
}

// Register / overwrite / Unregister round trip.
static Result Test_RegisterRoundTrip()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    m.Register(&c, _T("hello"));
    EXPECT_STR(m.GetText(&c), _T("hello"), _T("Reg/initial"));
    m.Register(&c, _T("world"));   // overwrite
    EXPECT_STR(m.GetText(&c), _T("world"), _T("Reg/overwrite"));
    m.Unregister(&c);
    EXPECT_STR(m.GetText(&c), _T(""), _T("Reg/afterUnreg"));
    return OK(_T("RegisterRoundTrip"));
}

// Independent entries don't collide.
static Result Test_MultipleControls()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl a, b, c;
    m.Register(&a, _T("A"));
    m.Register(&b, _T("B"));
    m.Register(&c, _T("C"));
    EXPECT_STR(m.GetText(&a), _T("A"), _T("Multi/a"));
    EXPECT_STR(m.GetText(&b), _T("B"), _T("Multi/b"));
    EXPECT_STR(m.GetText(&c), _T("C"), _T("Multi/c"));
    m.Unregister(&b);
    EXPECT_STR(m.GetText(&b), _T(""),  _T("Multi/bGone"));
    EXPECT_STR(m.GetText(&a), _T("A"), _T("Multi/aKeep"));
    EXPECT_STR(m.GetText(&c), _T("C"), _T("Multi/cKeep"));
    m.Unregister(&a);
    m.Unregister(&c);
    return OK(_T("MultipleControls"));
}

// Registering an unrelated text doesn't affect the showing-text query.
static Result Test_NotShowingByDefault()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    m.Register(&c, _T("zzz"));
    EXPECT_BOOL(m.IsShowing(), false, _T("NoShow/initial"));
    EXPECT_STR(m.GetShowingText(), _T(""), _T("NoShow/empty"));
    m.Unregister(&c);
    return OK(_T("NotShowingByDefault"));
}

// Unregister a control we never registered is safe.
static Result Test_UnregisterNoop()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    m.Unregister(&c);  // never registered - safe
    return OK(_T("UnregisterNoop"));
}

// Null arg is safe.
static Result Test_NullSafe()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    m.Register(nullptr, _T("ignored"));
    m.Unregister(nullptr);
    EXPECT_STR(m.GetText(nullptr), _T(""), _T("Null/getText"));
    return OK(_T("NullSafe"));
}

// SetDelay round-trip.
static Result Test_DelayRoundTrip()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    UINT old = m.GetDelay();
    m.SetDelay(123);
    if (m.GetDelay() != 123)
    {
        return Fail(_T("Delay/set123"), _T("not set"));
    }
    m.SetDelay(old);
    return OK(_T("DelayRoundTrip"));
}

// OnLeave for a control with no pending tooltip is safe.
static Result Test_LeaveBeforeHover()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    StubCtrl c;
    m.OnLeave(&c);
    EXPECT_BOOL(m.IsShowing(), false, _T("Leave/notShowing"));
    return OK(_T("LeaveBeforeHover"));
}

// The screen DC with the default DUI font selected, for the layout cases.
class ScreenFontDc
{
public:
    ScreenFontDc()
    {
        m_hdc = ::GetDC(nullptr);
        HFONT font = DuiResMgr::Inst().GetDefaultFont();
        m_oldFont = font ? (HFONT)::SelectObject(m_hdc, font) : nullptr;
    }
    ~ScreenFontDc()
    {
        if (m_oldFont)
        {
            ::SelectObject(m_hdc, m_oldFont);
        }
        ::ReleaseDC(nullptr, m_hdc);
    }
    HDC Get() const { return m_hdc; }

private:
    HDC   m_hdc;       // screen DC, released in the dtor
    HFONT m_oldFont;   // font selected before ours, restored in the dtor
};

// Text with a '\n' is laid out as multiple lines; text without one keeps
// the original single-line flags.
static Result Test_DrawFlagsSingleVsMulti()
{
    ScreenFontDc dc;
    const UINT single = DuiToolTipMgr::LayoutText(dc.Get(), _T("one line"), kNoWrapWidth).m_flags;
    const UINT multi  = DuiToolTipMgr::LayoutText(dc.Get(), _T("line one\nline two"), kNoWrapWidth).m_flags;
    EXPECT_BOOL((single & DT_SINGLELINE) != 0, true, _T("DrawFlags/single"));
    EXPECT_BOOL((multi & DT_SINGLELINE) != 0, false, _T("DrawFlags/multi"));
    EXPECT_BOOL((multi & DT_LEFT) == DT_LEFT && (multi & DT_TOP) == DT_TOP, true, _T("DrawFlags/align"));
    return OK(_T("DrawFlagsSingleVsMulti"));
}

// A two-line tip is as wide as its longest line and twice as tall as one
// line; a single-line tip measures the same as GetTextExtentPoint32.
static Result Test_MeasureMultiLine()
{
    ScreenFontDc dc;
    const CString shortLine = _T("ab");
    const CString longLine  = _T("a much longer second line");
    SIZE expectShort = { 0, 0 };
    SIZE expectLong  = { 0, 0 };
    ::GetTextExtentPoint32(dc.Get(), shortLine, shortLine.GetLength(), &expectShort);
    ::GetTextExtentPoint32(dc.Get(), longLine, longLine.GetLength(), &expectLong);
    const SIZE single = DuiToolTipMgr::LayoutText(dc.Get(), shortLine, kNoWrapWidth).m_size;
    const SIZE multi  = DuiToolTipMgr::LayoutText(dc.Get(), shortLine + _T("\n") + longLine, kNoWrapWidth).m_size;
    const SIZE empty  = DuiToolTipMgr::LayoutText(dc.Get(), CString(), kNoWrapWidth).m_size;

    CString d;
    if (single.cx != expectShort.cx || single.cy != expectShort.cy)
    {
        d.Format(_T("single got %dx%d expected %dx%d"), single.cx, single.cy, expectShort.cx, expectShort.cy);
        return Fail(_T("MeasureMultiLine"), d);
    }
    if (multi.cx != expectLong.cx || multi.cy != expectShort.cy * 2)
    {
        d.Format(_T("multi got %dx%d expected %dx%d"), multi.cx, multi.cy, expectLong.cx, expectShort.cy * 2);
        return Fail(_T("MeasureMultiLine"), d);
    }
    if (empty.cx != 0 || empty.cy != 0)
    {
        return Fail(_T("MeasureMultiLine"), _T("empty text should measure 0x0"));
    }
    return OK(_T("MeasureMultiLine"));
}

// Text wider than the maximum width wraps: between words, and inside a word
// that is wider than a whole line (a file name or path without spaces).
// Text that fits keeps the single-line layout.
static Result Test_WrapLongText()
{
    const int maxWidth = 200;       // narrow enough to wrap both long strings below
    const int wordRepeat = 80;      // "word " x 80 is far wider than maxWidth
    const int tokenLength = 300;    // 300 x 'x' without spaces, also far wider
    ScreenFontDc dc;
    SIZE oneLine = { 0, 0 };
    ::GetTextExtentPoint32(dc.Get(), _T("ab"), 2, &oneLine);

    CString words;
    for (int i = 0; i < wordRepeat; ++i)
    {
        words += _T("word ");
    }
    const DuiToolTipMgr::TextLayout wrapped = DuiToolTipMgr::LayoutText(dc.Get(), words, maxWidth);
    EXPECT_BOOL(wrapped.m_size.cx <= maxWidth, true, _T("Wrap/wordsWidth"));
    EXPECT_BOOL(wrapped.m_size.cy >= oneLine.cy * 2, true, _T("Wrap/wordsLines"));
    EXPECT_BOOL((wrapped.m_flags & DT_SINGLELINE) == 0, true, _T("Wrap/wordsMulti"));
    EXPECT_BOOL((wrapped.m_flags & DT_WORDBREAK) != 0, true, _T("Wrap/wordsBreak"));

    CString token(_T('x'), tokenLength);
    const DuiToolTipMgr::TextLayout broken = DuiToolTipMgr::LayoutText(dc.Get(), token, maxWidth);
    EXPECT_BOOL(broken.m_size.cx <= maxWidth, true, _T("Wrap/tokenWidth"));
    EXPECT_BOOL(broken.m_size.cy >= oneLine.cy * 2, true, _T("Wrap/tokenLines"));

    const DuiToolTipMgr::TextLayout fits = DuiToolTipMgr::LayoutText(dc.Get(), _T("ab"), maxWidth);
    EXPECT_INT(fits.m_size.cx, oneLine.cx, _T("Wrap/shortWidth"));
    EXPECT_INT(fits.m_size.cy, oneLine.cy, _T("Wrap/shortHeight"));
    EXPECT_BOOL((fits.m_flags & DT_SINGLELINE) != 0, true, _T("Wrap/shortSingle"));
    return OK(_T("WrapLongText"));
}

// A '&' in a tip is plain text (file names, nicknames), not a mnemonic prefix:
// drawing with the layout's own flags keeps it, so the drawn text is as wide as
// the measured one, on the single-line and the multi-line path. Before
// 2026-10-14 DrawText swallowed the '&' ("A&B.docx" drew as wide as "AB.docx")
// while the single-line window was sized with it.
static Result Test_LayoutKeepsAmpersand()
{
    ScreenFontDc dc;
    const CString withAmp = _T("A&B.docx");
    SIZE ext = { 0, 0 };
    ::GetTextExtentPoint32(dc.Get(), withAmp, withAmp.GetLength(), &ext);

    const DuiToolTipMgr::TextLayout single = DuiToolTipMgr::LayoutText(dc.Get(), withAmp, kNoWrapWidth);
    RECT drawn = { 0, 0, kNoWrapWidth, 0 };
    ::DrawText(dc.Get(), withAmp, -1, &drawn, single.m_flags | DT_CALCRECT);
    EXPECT_INT(drawn.right - drawn.left, ext.cx, _T("Amp/singleDrawnWidth"));

    const DuiToolTipMgr::TextLayout multi = DuiToolTipMgr::LayoutText(dc.Get(), withAmp + _T("\nx"), kNoWrapWidth);
    EXPECT_INT(multi.m_size.cx, ext.cx, _T("Amp/multiWidth"));
    return OK(_T("LayoutKeepsAmpersand"));
}

// Padding, cursor offset and the other sizes scale with the monitor DPI and
// keep their original pixel values at 96 DPI.
static Result Test_MetricsScaleWithDpi()
{
    const int dpi100 = 96;
    const int dpi150 = 144;
    const DuiToolTipMgr::Metrics m100 = DuiToolTipMgr::MetricsForDpi(dpi100);
    const DuiToolTipMgr::Metrics m150 = DuiToolTipMgr::MetricsForDpi(dpi150);
    const DuiToolTipMgr::Metrics m0   = DuiToolTipMgr::MetricsForDpi(0);
    // the pixel values before 2026-10-14, when they were not scaled
    EXPECT_INT(m100.m_padX, 8, _T("Metrics/padX96"));
    EXPECT_INT(m100.m_padY, 4, _T("Metrics/padY96"));
    EXPECT_INT(m100.m_offsetX, 12, _T("Metrics/offsetX96"));
    EXPECT_INT(m100.m_offsetY, 18, _T("Metrics/offsetY96"));
    // 150% = 3 / 2 of the 96 DPI value
    EXPECT_INT(m150.m_padX, m100.m_padX * 3 / 2, _T("Metrics/padX144"));
    EXPECT_INT(m150.m_padY, m100.m_padY * 3 / 2, _T("Metrics/padY144"));
    EXPECT_INT(m150.m_offsetX, m100.m_offsetX * 3 / 2, _T("Metrics/offsetX144"));
    EXPECT_INT(m150.m_offsetY, m100.m_offsetY * 3 / 2, _T("Metrics/offsetY144"));
    EXPECT_INT(m150.m_gapAbove, m100.m_gapAbove * 3 / 2, _T("Metrics/gap144"));
    EXPECT_INT(m150.m_maxTextWidth, m100.m_maxTextWidth * 3 / 2, _T("Metrics/maxWidth144"));
    // an unknown DPI (0) is treated as 96
    EXPECT_INT(m0.m_padX, m100.m_padX, _T("Metrics/padX0"));
    EXPECT_INT(m0.m_maxTextWidth, m100.m_maxTextWidth, _T("Metrics/maxWidth0"));
    return OK(_T("MetricsScaleWithDpi"));
}

// ClampToolTipOrigin: below and to the right of the cursor when it fits;
// above the cursor when the bottom does not fit; slid left when the right
// edge does not fit; pinned to the area when it fits nowhere.
static Result Test_ClampToolTipOrigin()
{
    const RECT area = { 0, 0, 1000, 800 };
    const SIZE tip = { 200, 40 };
    const int offX = 12;
    const int offY = 18;
    const int gap = 4;

    // fits: unchanged
    POINT cur = { 100, 100 };
    POINT pref = { cur.x + offX, cur.y + offY };
    POINT o = ClampToolTipOrigin(cur, pref, gap, tip, area);
    EXPECT_INT(o.x, 112, _T("ClampTip/fitsX"));
    EXPECT_INT(o.y, 118, _T("ClampTip/fitsY"));

    // bottom does not fit: whole tip above the cursor, gap pixels away
    cur.x = 100;
    cur.y = 780;
    pref.x = cur.x + offX;
    pref.y = cur.y + offY;
    o = ClampToolTipOrigin(cur, pref, gap, tip, area);
    EXPECT_INT(o.x, 112, _T("ClampTip/bottomX"));
    EXPECT_INT(o.y, 780 - gap - 40, _T("ClampTip/bottomY"));

    // right edge does not fit: slid left to the edge, still below
    cur.x = 950;
    cur.y = 100;
    pref.x = cur.x + offX;
    pref.y = cur.y + offY;
    o = ClampToolTipOrigin(cur, pref, gap, tip, area);
    EXPECT_INT(o.x, 1000 - 200, _T("ClampTip/rightX"));
    EXPECT_INT(o.y, 118, _T("ClampTip/rightY"));

    // bottom-right corner: both corrections
    cur.x = 950;
    cur.y = 780;
    pref.x = cur.x + offX;
    pref.y = cur.y + offY;
    o = ClampToolTipOrigin(cur, pref, gap, tip, area);
    EXPECT_INT(o.x, 1000 - 200, _T("ClampTip/cornerX"));
    EXPECT_INT(o.y, 780 - gap - 40, _T("ClampTip/cornerY"));

    // neither below nor above fits: pinned to the bottom edge
    const RECT lowArea = { 0, 0, 1000, 60 };
    cur.x = 100;
    cur.y = 30;
    pref.x = cur.x + offX;
    pref.y = cur.y + offY;
    o = ClampToolTipOrigin(cur, pref, gap, tip, lowArea);
    EXPECT_INT(o.y, 60 - 40, _T("ClampTip/pinBottomY"));

    // larger than the area: top-left of the area
    const SIZE huge = { 1200, 900 };
    cur.x = 100;
    cur.y = 100;
    pref.x = cur.x + offX;
    pref.y = cur.y + offY;
    o = ClampToolTipOrigin(cur, pref, gap, huge, area);
    EXPECT_INT(o.x, 0, _T("ClampTip/hugeX"));
    EXPECT_INT(o.y, 0, _T("ClampTip/hugeY"));
    return OK(_T("ClampToolTipOrigin"));
}

// Pumps this thread's messages (the tip is popped by a thread timer) until the
// tip shows or the wait runs out. Returns whether the tip is showing.
//   hoverHost: WM_MOUSELEAVE sent to this window is dropped instead of
//   dispatched. The real cursor is not over the test window (it may even be on
//   another desktop), so TrackMouseEvent reports a leave at once, which would
//   cancel the tip by itself; dropping it keeps the "cursor stays on the
//   control" case. nullptr when the case has no host window.
static bool PumpUntilTipShows(DWORD waitMs, HWND hoverHost)
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    const ULONGLONG deadline = ::GetTickCount64() + waitMs;
    for (;;)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (hoverHost != nullptr && msg.hwnd == hoverHost && msg.message == WM_MOUSELEAVE)
            {
                continue;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        const ULONGLONG now = ::GetTickCount64();
        if (m.IsShowing() || now >= deadline)
        {
            break;
        }
        // wakes as soon as the timer message (or any other) arrives
        ::MsgWaitForMultipleObjects(0, nullptr, FALSE, (DWORD)(deadline - now), QS_ALLINPUT);
    }
    return m.IsShowing();
}

// A small unactivated DuiHost whose root control has a tip, for the cases
// that drive the host with mouse and keyboard messages. Sets the popup delay
// to kQuickDelayMs; the dtor closes any tip, destroys the window and restores
// the delay.
class TipHostFixture
{
public:
    explicit TipHostFixture(std::unique_ptr<DuiControl> root)
    {
        DuiToolTipMgr& m = DuiToolTipMgr::Inst();
        m_oldDelay = m.GetDelay();
        m.SetDelay(kQuickDelayMs);
        RECT rc;
        ::SetRect(&rc, 0, 0, kHostW, kHostH);
        m_host.Create(nullptr, rc, nullptr, WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
        if (m_host.IsWindow())
        {
            m.Register(root.get(), kTipText);
            m_host.SetRoot(std::move(root));
        }
    }
    ~TipHostFixture()
    {
        DuiToolTipMgr& m = DuiToolTipMgr::Inst();
        m.OnLeave(nullptr);
        if (m_host.IsWindow())
        {
            m_host.DestroyWindow();
        }
        m.SetDelay(m_oldDelay);
    }

    bool IsReady() { return m_host.IsWindow() != FALSE; }
    HWND Hwnd() const { return m_host.m_hWnd; }

    // Sends a message to the host with the cursor over the root control.
    void SendAtCursor(UINT msg, WPARAM wParam)
    {
        ::SendMessage(m_host.m_hWnd, msg, wParam, MAKELPARAM(kCursorX, kCursorY));
    }
    // The cursor position in screen coordinates (WM_MOUSEWHEEL wants these).
    LPARAM ScreenCursor() const
    {
        POINT pt = { kCursorX, kCursorY };
        ::ClientToScreen(m_host.m_hWnd, &pt);
        return MAKELPARAM(pt.x, pt.y);
    }
    // Hovers the root control and waits until its tip shows.
    bool HoverUntilShows()
    {
        SendAtCursor(WM_MOUSEMOVE, 0);
        return PumpUntilTipShows(kShowWaitMs, m_host.m_hWnd);
    }

private:
    static const int kHostW = 200;     // host client width, pixels
    static const int kHostH = 100;     // host client height, pixels
    static const int kCursorX = 50;    // cursor x inside the host, pixels
    static const int kCursorY = 50;    // cursor y inside the host, pixels

    DuiHost m_host;        // the window under test, destroyed in the dtor
    UINT    m_oldDelay;    // popup delay before the fixture, restored in the dtor
};

// A tip that is already showing closes when its control is destroyed (the
// control's dtor unregisters it). Before the fix only a pending tip was
// cancelled, and a visible one stayed on screen after its window was closed
// by the program (e.g. the other side ended a remote desktop session).
static Result Test_DestroyingControlClosesVisibleTip()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    const UINT oldDelay = m.GetDelay();
    m.SetDelay(kQuickDelayMs);
    bool shown = false;
    {
        StubCtrl c;
        m.Register(&c, kTipText);
        m.OnHover(&c, kHoverPt);
        shown = PumpUntilTipShows(kShowWaitMs, nullptr);
    }   // ~DuiControl unregisters c: this is the step under test
    const bool stillShowing = m.IsShowing();
    m.HideNow();            // don't leave a tip to later cases if the fix regresses
    m.SetDelay(oldDelay);
    EXPECT_BOOL(shown, true, _T("DestroyVisible/shown"));
    EXPECT_BOOL(stillShowing, false, _T("DestroyVisible/closed"));
    return OK(_T("DestroyingControlClosesVisibleTip"));
}

// HideNow also stops a tip that is still waiting for its delay, and the
// control stays tracked, so hovering the same control again does not restart
// it. Before 2026-10-14 HideNow only closed a tip that was already showing.
static Result Test_HideNowCancelsPendingTip()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    const UINT oldDelay = m.GetDelay();
    m.SetDelay(kQuickDelayMs);
    StubCtrl c;
    m.Register(&c, kTipText);
    m.OnHover(&c, kHoverPt);
    m.HideNow();
    m.OnHover(&c, kHoverPt);   // cursor still on the same control
    const bool shown = PumpUntilTipShows(kQuietWaitMs, nullptr);
    m.OnLeave(nullptr);
    m.SetDelay(oldDelay);
    EXPECT_BOOL(shown, false, _T("HideNowPending/noTip"));
    return OK(_T("HideNowCancelsPendingTip"));
}

// Pressing the left mouse button on a control whose tip is still waiting for
// its delay cancels the tip: it must not pop up later (over a menu the click
// opened, for instance). Before the fix DuiHost::OnLButtonDown only hid a
// tip that was already showing and left the pending timer running.
static Result Test_PressCancelsPendingTip()
{
    TipHostFixture f(std::unique_ptr<DuiControl>(new StubCtrl()));
    if (!f.IsReady())
    {
        return Fail(_T("PressCancels/host"), _T("cannot create host window"));
    }
    // Precondition: hovering alone pops the tip, so the press case below
    // really measures the press.
    const bool hoverShows = f.HoverUntilShows();
    f.SendAtCursor(WM_MOUSELEAVE, 0);

    // Hover again and press before the delay is over.
    f.SendAtCursor(WM_MOUSEMOVE, 0);
    f.SendAtCursor(WM_LBUTTONDOWN, MK_LBUTTON);
    f.SendAtCursor(WM_LBUTTONUP, 0);
    const bool shownAfterPress = PumpUntilTipShows(kQuietWaitMs, f.Hwnd());
    EXPECT_BOOL(hoverShows, true, _T("PressCancels/hoverShows"));
    EXPECT_BOOL(shownAfterPress, false, _T("PressCancels/noTipAfterPress"));
    return OK(_T("PressCancelsPendingTip"));
}

// Same as PressCancelsPendingTip with the right button: the tip must not pop
// up over the context menu the right click opens. Before 2026-10-14
// DuiHost::OnRButtonDown did not touch the tooltip at all and the tip still
// popped after a right press.
static Result Test_RightPressCancelsPendingTip()
{
    TipHostFixture f(std::unique_ptr<DuiControl>(new StubCtrl()));
    if (!f.IsReady())
    {
        return Fail(_T("RightPressCancels/host"), _T("cannot create host window"));
    }
    const bool hoverShows = f.HoverUntilShows();
    f.SendAtCursor(WM_MOUSELEAVE, 0);

    f.SendAtCursor(WM_MOUSEMOVE, 0);
    f.SendAtCursor(WM_RBUTTONDOWN, MK_RBUTTON);
    f.SendAtCursor(WM_RBUTTONUP, 0);
    const bool shownAfterPress = PumpUntilTipShows(kQuietWaitMs, f.Hwnd());
    EXPECT_BOOL(hoverShows, true, _T("RightPressCancels/hoverShows"));
    EXPECT_BOOL(shownAfterPress, false, _T("RightPressCancels/noTipAfterPress"));
    return OK(_T("RightPressCancelsPendingTip"));
}

// A wheel that scrolls something closes the tip (the content moved, so the
// control under the cursor is no longer the one the tip is about); a wheel
// that nothing consumes leaves it alone.
static Result Test_WheelHidesTip()
{
    {
        TipHostFixture f(std::unique_ptr<DuiControl>(new WheelCtrl()));
        if (!f.IsReady())
        {
            return Fail(_T("Wheel/host"), _T("cannot create host window"));
        }
        const bool shown = f.HoverUntilShows();
        ::SendMessage(f.Hwnd(), WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), f.ScreenCursor());
        const bool stillShowing = DuiToolTipMgr::Inst().IsShowing();
        EXPECT_BOOL(shown, true, _T("Wheel/shown"));
        EXPECT_BOOL(stillShowing, false, _T("Wheel/closed"));
    }
    {
        TipHostFixture f(std::unique_ptr<DuiControl>(new StubCtrl()));
        if (!f.IsReady())
        {
            return Fail(_T("Wheel/host2"), _T("cannot create host window"));
        }
        const bool shown = f.HoverUntilShows();
        ::SendMessage(f.Hwnd(), WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), f.ScreenCursor());
        const bool stillShowing = DuiToolTipMgr::Inst().IsShowing();
        EXPECT_BOOL(shown, true, _T("Wheel/shown2"));
        EXPECT_BOOL(stillShowing, true, _T("Wheel/unhandledKeeps"));
    }
    return OK(_T("WheelHidesTip"));
}

// A key press closes the tip (e.g. the user starts typing while the cursor
// rests on a button).
static Result Test_KeyDownHidesTip()
{
    TipHostFixture f(std::unique_ptr<DuiControl>(new StubCtrl()));
    if (!f.IsReady())
    {
        return Fail(_T("KeyDown/host"), _T("cannot create host window"));
    }
    const bool shown = f.HoverUntilShows();
    ::SendMessage(f.Hwnd(), WM_KEYDOWN, 'A', 0);
    const bool stillShowing = DuiToolTipMgr::Inst().IsShowing();
    EXPECT_BOOL(shown, true, _T("KeyDown/shown"));
    EXPECT_BOOL(stillShowing, false, _T("KeyDown/closed"));
    return OK(_T("KeyDownHidesTip"));
}

// Losing the keyboard focus (e.g. Alt+Tab to another program) closes the
// topmost tip so it does not stay over the other program.
static Result Test_KillFocusHidesTip()
{
    TipHostFixture f(std::unique_ptr<DuiControl>(new StubCtrl()));
    if (!f.IsReady())
    {
        return Fail(_T("KillFocus/host"), _T("cannot create host window"));
    }
    const bool shown = f.HoverUntilShows();
    ::SendMessage(f.Hwnd(), WM_KILLFOCUS, 0, 0);
    const bool stillShowing = DuiToolTipMgr::Inst().IsShowing();
    EXPECT_BOOL(shown, true, _T("KillFocus/shown"));
    EXPECT_BOOL(stillShowing, false, _T("KillFocus/closed"));
    return OK(_T("KillFocusHidesTip"));
}

// Moving on to another control while a tip shows, or within one delay of
// leaving a showing tip, uses a fifth of the delay; a tip closed by HideNow
// (press, key, wheel) or a leave older than one delay gives the full delay.
static Result Test_ReshowUsesShortDelay()
{
    const UINT delayMs = 50;        // long enough to tell 50 from 50 / 5
    const UINT reshowDivisor = 5;   // same as kReshowDelayDivisor in DuiToolTip.cpp
    const DWORD expireMs = delayMs + 30;   // sleep past the one-delay reshow window
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    const UINT oldDelay = m.GetDelay();
    m.SetDelay(delayMs);
    StubCtrl a;
    StubCtrl b;
    m.Register(&a, _T("A"));
    m.Register(&b, _T("B"));

    // leave a showing tip, hover the next control at once: short delay
    m.OnHover(&a, kHoverPt);
    const bool aShown = PumpUntilTipShows(kShowWaitMs, nullptr);
    m.OnLeave(&a);
    m.OnHover(&b, kHoverPt);
    const UINT afterLeave = m.GetScheduledDelay();

    // leave a showing tip, hover the next control after one delay: full delay
    const bool bShown = PumpUntilTipShows(kShowWaitMs, nullptr);
    m.OnLeave(&b);
    ::Sleep(expireMs);
    m.OnHover(&a, kHoverPt);
    const UINT afterExpiry = m.GetScheduledDelay();

    // tip closed by HideNow, then the next control: full delay
    const bool aShownAgain = PumpUntilTipShows(kShowWaitMs, nullptr);
    m.HideNow();
    m.OnLeave(&a);
    m.OnHover(&b, kHoverPt);
    const UINT afterHideNow = m.GetScheduledDelay();

    m.OnLeave(nullptr);
    m.SetDelay(oldDelay);
    EXPECT_BOOL(aShown, true, _T("Reshow/aShown"));
    EXPECT_INT(afterLeave, delayMs / reshowDivisor, _T("Reshow/afterLeave"));
    EXPECT_BOOL(bShown, true, _T("Reshow/bShown"));
    EXPECT_INT(afterExpiry, delayMs, _T("Reshow/afterExpiry"));
    EXPECT_BOOL(aShownAgain, true, _T("Reshow/aShownAgain"));
    EXPECT_INT(afterHideNow, delayMs, _T("Reshow/afterHideNow"));
    return OK(_T("ReshowUsesShortDelay"));
}

// Retarget swaps the text of a showing tip (the cursor moved to another hot
// zone of the same control); an empty text unregisters and closes it.
static Result Test_RetargetSwitchesText()
{
    DuiToolTipMgr& m = DuiToolTipMgr::Inst();
    const UINT oldDelay = m.GetDelay();
    m.SetDelay(kQuickDelayMs);
    StubCtrl c;
    m.Register(&c, _T("one"));
    m.OnHover(&c, kHoverPt);
    const bool firstShown = PumpUntilTipShows(kShowWaitMs, nullptr);
    const CString firstText = m.GetShowingText();

    m.Retarget(&c, _T("two"), kHoverPt);
    const bool secondShown = PumpUntilTipShows(kShowWaitMs, nullptr);
    const CString secondText = m.GetShowingText();
    const CString registered = m.GetText(&c);

    m.Retarget(&c, _T(""), kHoverPt);
    const bool shownAfterEmpty = m.IsShowing();
    const CString registeredAfterEmpty = m.GetText(&c);

    m.OnLeave(nullptr);
    m.SetDelay(oldDelay);
    EXPECT_BOOL(firstShown, true, _T("Retarget/firstShown"));
    EXPECT_STR(firstText, _T("one"), _T("Retarget/firstText"));
    EXPECT_BOOL(secondShown, true, _T("Retarget/secondShown"));
    EXPECT_STR(secondText, _T("two"), _T("Retarget/secondText"));
    EXPECT_STR(registered, _T("two"), _T("Retarget/registered"));
    EXPECT_BOOL(shownAfterEmpty, false, _T("Retarget/emptyCloses"));
    EXPECT_STR(registeredAfterEmpty, _T(""), _T("Retarget/emptyUnregisters"));
    return OK(_T("RetargetSwitchesText"));
}

#undef EXPECT_STR
#undef EXPECT_BOOL
#undef EXPECT_INT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),               &Test_Defaults              },
        { _T("RegisterRoundTrip"),      &Test_RegisterRoundTrip     },
        { _T("MultipleControls"),       &Test_MultipleControls      },
        { _T("NotShowingByDefault"),    &Test_NotShowingByDefault   },
        { _T("UnregisterNoop"),         &Test_UnregisterNoop        },
        { _T("NullSafe"),               &Test_NullSafe              },
        { _T("DelayRoundTrip"),         &Test_DelayRoundTrip        },
        { _T("LeaveBeforeHover"),       &Test_LeaveBeforeHover      },
        { _T("DrawFlagsSingleVsMulti"), &Test_DrawFlagsSingleVsMulti },
        { _T("MeasureMultiLine"),       &Test_MeasureMultiLine      },
        { _T("WrapLongText"),           &Test_WrapLongText          },
        { _T("LayoutKeepsAmpersand"),   &Test_LayoutKeepsAmpersand  },
        { _T("MetricsScaleWithDpi"),    &Test_MetricsScaleWithDpi   },
        { _T("ClampToolTipOrigin"),     &Test_ClampToolTipOrigin    },
        { _T("DestroyingControlClosesVisibleTip"), &Test_DestroyingControlClosesVisibleTip },
        { _T("HideNowCancelsPendingTip"), &Test_HideNowCancelsPendingTip },
        { _T("PressCancelsPendingTip"), &Test_PressCancelsPendingTip },
        { _T("RightPressCancelsPendingTip"), &Test_RightPressCancelsPendingTip },
        { _T("WheelHidesTip"),          &Test_WheelHidesTip         },
        { _T("KeyDownHidesTip"),        &Test_KeyDownHidesTip       },
        { _T("KillFocusHidesTip"),      &Test_KillFocusHidesTip     },
        { _T("ReshowUsesShortDelay"),   &Test_ReshowUsesShortDelay  },
        { _T("RetargetSwitchesText"),   &Test_RetargetSwitchesText  }
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
    summary.Format(_T("[summary] DuiToolTipTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiToolTipTests

} // namespace balloonwjui

#endif // BUI_FEATURE_TOOLTIP
