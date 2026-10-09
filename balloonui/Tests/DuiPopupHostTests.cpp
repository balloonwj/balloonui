#include "stdafx.h"
#include "DuiPopupHostTests.h"

#if BUI_FEATURE_POPUPHOST


namespace balloonwjui {

namespace DuiPopupHostTests {

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
#define EXPECT_RECT(rc, L, T, R, B, name) \
    do { const RECT& _r = (rc); \
         if (_r.left!=(L)||_r.top!=(T)||_r.right!=(R)||_r.bottom!=(B)) { \
             CString _d; _d.Format(_T("expected=(%d,%d,%d,%d) got=(%d,%d,%d,%d)"), \
                 (L),(T),(R),(B),_r.left,_r.top,_r.right,_r.bottom); \
             return Fail(name, _d); } \
    } while (0)

// ----- API round-trips ------------------------------------------------

static Result Test_Defaults()
{
    DuiPopupHost p;
    EXPECT_INT(p.GetWidth(),  200, _T("Def/w"));
    EXPECT_INT(p.GetHeight(), 200, _T("Def/h"));
    EXPECT_INT(p.GetEdge(), DuiPopupHost::EdgeBelow, _T("Def/edge"));
    EXPECT_TRUE(!p.IsShowing(), _T("Def/notShowing"));
    return OK(_T("Defaults"));
}

static Result Test_SetterRoundTrip()
{
    DuiPopupHost p;
    p.SetSize(300, 240);
    EXPECT_INT(p.GetWidth(),  300, _T("RT/w"));
    EXPECT_INT(p.GetHeight(), 240, _T("RT/h"));
    p.SetSize(0, -5);                   // clamped to >= 1
    EXPECT_INT(p.GetWidth(),  1, _T("RT/wClamp"));
    EXPECT_INT(p.GetHeight(), 1, _T("RT/hClamp"));
    p.SetEdge(DuiPopupHost::EdgeAbove);
    EXPECT_INT(p.GetEdge(), DuiPopupHost::EdgeAbove, _T("RT/edge"));
    return OK(_T("SetterRoundTrip"));
}

// ----- ComputePositionScreen pure helper ------------------------------

// Below fits: just place at anchor.bottom-left.
static Result Test_PlaceBelowFits()
{
    using P = DuiPopupHost;
    RECT anchor = { 100, 100, 200, 130 };       // 30 tall
    RECT work   = { 0,   0,   1920, 1080 };
    RECT got = P::ComputePositionScreen(anchor, 150, 80, P::EdgeBelow, work);
    EXPECT_RECT(got, 100, 130, 250, 210, _T("Below/fits"));
    return OK(_T("PlaceBelowFits"));
}

// Below would go past bottom -> auto-flip Above.
static Result Test_PlaceBelowFlipsAbove()
{
    using P = DuiPopupHost;
    RECT anchor = { 100, 1000, 200, 1030 };
    RECT work   = { 0,   0,    1920, 1080 };
    RECT got = P::ComputePositionScreen(anchor, 150, 200, P::EdgeBelow, work);
    // Above placement: y = 1000 - 200 = 800.
    EXPECT_RECT(got, 100, 800, 250, 1000, _T("Below/flip"));
    return OK(_T("PlaceBelowFlipsAbove"));
}

// Right would push past right edge -> flip Left.
static Result Test_PlaceRightFlipsLeft()
{
    using P = DuiPopupHost;
    RECT anchor = { 1850, 200, 1900, 230 };
    RECT work   = { 0,    0,   1920, 1080 };
    RECT got = P::ComputePositionScreen(anchor, 200, 100, P::EdgeRight, work);
    // Left placement: x = 1850 - 200 = 1650.
    EXPECT_RECT(got, 1650, 200, 1850, 300, _T("Right/flip"));
    return OK(_T("PlaceRightFlipsLeft"));
}

// Neither side fits: clamps to work area.
static Result Test_PlaceClampsToWorkArea()
{
    using P = DuiPopupHost;
    // Anchor is in the middle of an extremely small work area, popup
    // larger than work height — flip can't help, must clamp.
    RECT anchor = { 50, 50, 60, 60 };
    RECT work   = { 0,  0,  100, 100 };
    RECT got = P::ComputePositionScreen(anchor, 200, 200, P::EdgeBelow, work);
    // Expected: clamped so right<=100 and bottom<=100; left>=0, top>=0.
    // With size 200x200 and work 100x100, the only valid placement is
    // left=0, top=0 (so right=200>100 still — work area is too small).
    // Our clamper picks (0,0,200,200): left/top forced >= work, but
    // right/bottom can exceed when popup is bigger than work.
    EXPECT_INT(got.left, 0, _T("Clamp/left"));
    EXPECT_INT(got.top,  0, _T("Clamp/top"));
    return OK(_T("PlaceClampsToWorkArea"));
}

// Above default fits: place at anchor.top minus popupH.
static Result Test_PlaceAboveFits()
{
    using P = DuiPopupHost;
    RECT anchor = { 100, 500, 200, 530 };
    RECT work   = { 0,   0,   1920, 1080 };
    RECT got = P::ComputePositionScreen(anchor, 150, 80, P::EdgeAbove, work);
    EXPECT_RECT(got, 100, 420, 250, 500, _T("Above/fits"));
    return OK(_T("PlaceAboveFits"));
}

// Left fits: place at anchor.left minus popupW.
static Result Test_PlaceLeftFits()
{
    using P = DuiPopupHost;
    RECT anchor = { 500, 200, 600, 230 };
    RECT work   = { 0,   0,   1920, 1080 };
    RECT got = P::ComputePositionScreen(anchor, 200, 100, P::EdgeLeft, work);
    EXPECT_RECT(got, 300, 200, 500, 300, _T("Left/fits"));
    return OK(_T("PlaceLeftFits"));
}

// Dismiss callback round-trips: programmatic Hide on a not-yet-shown
// popup is a no-op (no callback fires).
static Result Test_DismissCallbackStores()
{
    DuiPopupHost p;
    int hits = 0;
    p.SetDismissCallback(
        [](void* ud, DuiPopupHost::Reason) { ++(*(int*)ud); }, &hits);
    p.Hide();   // not showing -> no fire.
    EXPECT_INT(hits, 0, _T("Cb/notShown"));
    p.SetDismissCallback(nullptr, nullptr);
    return OK(_T("DismissCallbackStores"));
}

// SetContent before Show: content is stashed; GetRoot returns the raw.
static Result Test_SetContentBeforeShow()
{
    class StubRoot : public DuiControl
    {
    public:
        void OnPaint(HDC, const RECT&) override {}
    };
    DuiPopupHost p;
    StubRoot* raw;
    p.SetContent(std::unique_ptr<DuiControl>(raw = new StubRoot()));
    EXPECT_TRUE(p.GetRoot() == raw, _T("SC/storedRaw"));
    return OK(_T("SetContentBeforeShow"));
}

// 浮层窗口用的是本类声明的窗口类（bugs.md BUG-101，2026-10-05）：类名为 __DuiPopupHost__，
// 带阴影（CS_DROPSHADOW）与 CS_SAVEBITS，同时保留原先从 __DuiHost__ 继承来的双击与重绘样式。
// 修复之前浮层实际注册的是基类的 __DuiHost__，声明的阴影样式从未生效。
static Result Test_WindowClassHasDropShadow()
{
    //弹出位置放在屏幕左上角附近、尺寸取最小，测完立即销毁
    const int kProbeSize = 40;
    const RECT anchor = { 0, 0, kProbeSize, kProbeSize };
    DuiPopupHost p;
    p.SetContent(std::unique_ptr<DuiControl>(new DuiControl()));
    p.SetSize(kProbeSize, kProbeSize);
    p.Show(anchor, nullptr);
    EXPECT_TRUE(p.m_hWnd != nullptr, _T("WC/created"));
    TCHAR cls[64] = { 0 };
    ::GetClassName(p.m_hWnd, cls, 64);
    const DWORD style = static_cast<DWORD>(::GetClassLongPtr(p.m_hWnd, GCL_STYLE));
    p.Hide();
    p.DestroyWindow();
    if (_tcscmp(cls, _T("__DuiPopupHost__")) != 0)
    {
        return Fail(_T("WindowClassHasDropShadow"), CString(_T("class=")) + cls);
    }
    EXPECT_TRUE((style & CS_DROPSHADOW) != 0, _T("WC/dropShadow"));
    EXPECT_TRUE((style & CS_SAVEBITS) != 0, _T("WC/saveBits"));
    EXPECT_TRUE((style & CS_DBLCLKS) != 0, _T("WC/dblClks"));
    EXPECT_TRUE((style & CS_HREDRAW) != 0 && (style & CS_VREDRAW) != 0, _T("WC/redraw"));
    return OK(_T("WindowClassHasDropShadow"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_RECT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),               &Test_Defaults               },
        { _T("SetterRoundTrip"),        &Test_SetterRoundTrip        },
        { _T("PlaceBelowFits"),         &Test_PlaceBelowFits         },
        { _T("PlaceBelowFlipsAbove"),   &Test_PlaceBelowFlipsAbove   },
        { _T("PlaceRightFlipsLeft"),    &Test_PlaceRightFlipsLeft    },
        { _T("PlaceClampsToWorkArea"),  &Test_PlaceClampsToWorkArea  },
        { _T("PlaceAboveFits"),         &Test_PlaceAboveFits         },
        { _T("PlaceLeftFits"),          &Test_PlaceLeftFits          },
        { _T("DismissCallbackStores"),  &Test_DismissCallbackStores  },
        { _T("SetContentBeforeShow"),   &Test_SetContentBeforeShow   },
        { _T("WindowClassHasDropShadow"), &Test_WindowClassHasDropShadow },
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
    summary.Format(_T("[summary] DuiPopupHostTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiPopupHostTests

} // namespace balloonwjui

#endif // BUI_FEATURE_POPUPHOST
