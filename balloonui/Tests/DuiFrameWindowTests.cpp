#include "stdafx.h"
#include "DuiFrameWindowTests.h"

#if BUI_FEATURE_FRAMEWINDOW


namespace balloonwjui {

namespace DuiFrameWindowTests {

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
         if (_a != _e) { return Fail(name, _T("string mismatch")); } \
    } while (0)
#define EXPECT_HT(actual, expected, name) \
    do { UINT _a = (UINT)(actual); UINT _e = (UINT)(expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%u got=%u"), _e, _a); return Fail(name, _d); } \
    } while (0)

// ----- API round-trips ------------------------------------------------

static Result Test_Defaults()
{
    DuiFrameWindow f;
    EXPECT_INT(f.GetTitleBarHeight(), 32, _T("Def/titleH"));
    EXPECT_INT(f.GetBorderPx(),       6,  _T("Def/border"));
    EXPECT_TRUE(f.HasMinButton  (), _T("Def/min"));
    EXPECT_TRUE(f.HasMaxButton  (), _T("Def/max"));
    EXPECT_TRUE(f.HasCloseButton(), _T("Def/close"));
    EXPECT_TRUE(f.IsResizable(),    _T("Def/resizable"));
    SIZE s = f.GetMinSize();
    EXPECT_INT(s.cx, 200, _T("Def/minW"));
    EXPECT_INT(s.cy, 150, _T("Def/minH"));
    return OK(_T("Defaults"));
}

static Result Test_SetterRoundTrip()
{
    DuiFrameWindow f;
    f.SetTitle(_T("Hello"));
    EXPECT_STR(f.GetTitle(), _T("Hello"), _T("RT/title"));
    f.SetTitleBarHeight(48);
    EXPECT_INT(f.GetTitleBarHeight(), 48, _T("RT/titleH"));
    f.SetTitleBarHeight(5);                   // clamps to 18
    EXPECT_INT(f.GetTitleBarHeight(), 18, _T("RT/titleHClamp"));
    f.SetBorderPx(10);
    EXPECT_INT(f.GetBorderPx(), 10, _T("RT/border"));
    f.SetBorderPx(-3);                        // clamps to 0
    EXPECT_INT(f.GetBorderPx(), 0,  _T("RT/borderClamp"));
    f.SetButtons(false, true, false);
    EXPECT_TRUE(!f.HasMinButton  (), _T("RT/min"));
    EXPECT_TRUE( f.HasMaxButton  (), _T("RT/max"));
    EXPECT_TRUE(!f.HasCloseButton(), _T("RT/close"));
    f.SetResizable(false);
    EXPECT_TRUE(!f.IsResizable(), _T("RT/notResizable"));
    f.SetMinSize(0, -10);                      // clamps to 1
    SIZE s = f.GetMinSize();
    EXPECT_INT(s.cx, 1, _T("RT/minW"));
    EXPECT_INT(s.cy, 1, _T("RT/minH"));
    return OK(_T("SetterRoundTrip"));
}

// ----- ComputeNcHitTest pure helper -----------------------------------

static RECT MakeBtnRc(int l, int t, int r, int b)
{
    RECT x = { l, t, r, b };
    return x;
}

// Outside the window rect → HTNOWHERE.
static Result Test_HtOutside()
{
    using F = DuiFrameWindow;
    RECT wr = { 0, 0, 400, 300 };
    RECT noBtns[3] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
    EXPECT_HT(F::ComputeNcHitTest(POINT{ -5, 50 }, wr, 32, 6, true, noBtns),
              HTNOWHERE, _T("HT/outside"));
    return OK(_T("HtOutside"));
}

// 8-direction resize edges + corners.
static Result Test_HtResizeEdges()
{
    using F = DuiFrameWindow;
    RECT wr = { 0, 0, 400, 300 };
    RECT noBtns[3] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 1, 1 }, wr, 32, 6, true, noBtns),       HTTOPLEFT,     _T("HT/TL"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 398, 1 }, wr, 32, 6, true, noBtns),     HTTOPRIGHT,    _T("HT/TR"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 1, 298 }, wr, 32, 6, true, noBtns),     HTBOTTOMLEFT,  _T("HT/BL"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 398, 298 }, wr, 32, 6, true, noBtns),   HTBOTTOMRIGHT, _T("HT/BR"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 200, 1 }, wr, 32, 6, true, noBtns),     HTTOP,         _T("HT/T"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 200, 298 }, wr, 32, 6, true, noBtns),   HTBOTTOM,      _T("HT/B"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 1, 150 }, wr, 32, 6, true, noBtns),     HTLEFT,        _T("HT/L"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 398, 150 }, wr, 32, 6, true, noBtns),   HTRIGHT,       _T("HT/R"));
    return OK(_T("HtResizeEdges"));
}

// Title bar empty area → HTCAPTION.
static Result Test_HtTitleBarCaption()
{
    using F = DuiFrameWindow;
    RECT wr = { 0, 0, 400, 300 };
    RECT noBtns[3] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 100, 16 }, wr, 32, 6, true, noBtns),
              HTCAPTION, _T("HT/caption"));
    return OK(_T("HtTitleBarCaption"));
}

// Title bar over button → HTCLIENT (so DuiButton can handle the click).
static Result Test_HtTitleBarOverButton()
{
    using F = DuiFrameWindow;
    RECT wr = { 0, 0, 400, 300 };
    RECT btns[3] = {
        MakeBtnRc(292, 0, 328, 32),    // min
        MakeBtnRc(328, 0, 364, 32),    // max
        MakeBtnRc(364, 0, 400, 32),    // close
    };
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 380, 16 }, wr, 32, 6, true, btns),
              HTCLIENT, _T("HT/overClose"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 300, 16 }, wr, 32, 6, true, btns),
              HTCLIENT, _T("HT/overMin"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 100, 16 }, wr, 32, 6, true, btns),
              HTCAPTION, _T("HT/notOverButtons"));
    return OK(_T("HtTitleBarOverButton"));
}

// Below title bar → HTCLIENT.
static Result Test_HtClient()
{
    using F = DuiFrameWindow;
    RECT wr = { 0, 0, 400, 300 };
    RECT noBtns[3] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 100, 100 }, wr, 32, 6, true, noBtns),
              HTCLIENT, _T("HT/client"));
    return OK(_T("HtClient"));
}

// Non-resizable: borders fall through to HTCLIENT (or HTCAPTION when in title).
static Result Test_HtNonResizable()
{
    using F = DuiFrameWindow;
    RECT wr = { 0, 0, 400, 300 };
    RECT noBtns[3] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 1, 150 }, wr, 32, 6, false, noBtns),
              HTCLIENT, _T("HTnr/leftEdge"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 1, 16 },  wr, 32, 6, false, noBtns),
              HTCAPTION, _T("HTnr/leftEdgeOverTitle"));
    return OK(_T("HtNonResizable"));
}

// borderPx == 0 disables resize-edge hit-testing entirely.
static Result Test_HtZeroBorder()
{
    using F = DuiFrameWindow;
    RECT wr = { 0, 0, 400, 300 };
    RECT noBtns[3] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 0, 0 },     wr, 32, 0, true, noBtns), HTCAPTION, _T("HT0/topLeft"));
    EXPECT_HT(F::ComputeNcHitTest(POINT{ 200, 100 }, wr, 32, 0, true, noBtns), HTCLIENT,  _T("HT0/client"));
    return OK(_T("HtZeroBorder"));
}

// ----- ComputeMaximizedClientInsets pure helper ------------------------
//
// 普通窗口态 → 全 0，OnNcCalcSize 走"客户区 = 提议矩形"老路径。
static Result Test_MaxInsets_NotMaximized()
{
    using F = DuiFrameWindow;
    RECT r = F::ComputeMaximizedClientInsets(false, 8, 8);
    EXPECT_INT(r.left,   0, _T("MaxInset/normal/L"));
    EXPECT_INT(r.top,    0, _T("MaxInset/normal/T"));
    EXPECT_INT(r.right,  0, _T("MaxInset/normal/R"));
    EXPECT_INT(r.bottom, 0, _T("MaxInset/normal/B"));
    return OK(_T("MaxInsetsNotMaximized"));
}

// 最大化态 → 四边内缩 borderX/borderY，对应 OS 推出工作区的偏移。
// 故意把 X/Y 设成不同值，验证对称分配（不是只用一个轴）。
static Result Test_MaxInsets_Maximized()
{
    using F = DuiFrameWindow;
    RECT r = F::ComputeMaximizedClientInsets(true, 8, 7);
    EXPECT_INT(r.left,   8, _T("MaxInset/zoomed/L"));
    EXPECT_INT(r.top,    7, _T("MaxInset/zoomed/T"));
    EXPECT_INT(r.right,  8, _T("MaxInset/zoomed/R"));
    EXPECT_INT(r.bottom, 7, _T("MaxInset/zoomed/B"));
    return OK(_T("MaxInsetsMaximized"));
}

// 0 边框（极端情况，比如 SetBorderPx(0) + 罕见 OS）→ 即便最大化也无内缩。
// 防止有人误把 inset 钉成"非 0"硬编码值。
static Result Test_MaxInsets_ZeroBorder()
{
    using F = DuiFrameWindow;
    RECT r = F::ComputeMaximizedClientInsets(true, 0, 0);
    EXPECT_INT(r.left,   0, _T("MaxInset/zero/L"));
    EXPECT_INT(r.top,    0, _T("MaxInset/zero/T"));
    EXPECT_INT(r.right,  0, _T("MaxInset/zero/R"));
    EXPECT_INT(r.bottom, 0, _T("MaxInset/zero/B"));
    return OK(_T("MaxInsetsZeroBorder"));
}


#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_STR
#undef EXPECT_HT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),             &Test_Defaults             },
        { _T("SetterRoundTrip"),      &Test_SetterRoundTrip      },
        { _T("HtOutside"),            &Test_HtOutside            },
        { _T("HtResizeEdges"),        &Test_HtResizeEdges        },
        { _T("HtTitleBarCaption"),    &Test_HtTitleBarCaption    },
        { _T("HtTitleBarOverButton"), &Test_HtTitleBarOverButton },
        { _T("HtClient"),             &Test_HtClient             },
        { _T("HtNonResizable"),       &Test_HtNonResizable       },
        { _T("HtZeroBorder"),         &Test_HtZeroBorder         },
        { _T("MaxInsetsNotMaximized"),&Test_MaxInsets_NotMaximized},
        { _T("MaxInsetsMaximized"),   &Test_MaxInsets_Maximized   },
        { _T("MaxInsetsZeroBorder"),  &Test_MaxInsets_ZeroBorder  },
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
    summary.Format(_T("[summary] DuiFrameWindowTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiFrameWindowTests

} // namespace balloonwjui

#endif // BUI_FEATURE_FRAMEWINDOW
