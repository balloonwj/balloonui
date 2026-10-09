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


// ----- ResizeClient：客户区就是整个窗口，按要求的尺寸精确设置 ------------

// 测试窗口创建时的初始尺寸（像素），随后由 ResizeClient 改掉，取值无特殊含义
static const int kRcInitW = 300;
static const int kRcInitH = 200;
// ResizeClient 的目标尺寸（像素），取单聊窗的设计尺寸 640×620
static const int kRcTargetW = 640;
static const int kRcTargetH = 620;
// 只改高度时的新高度（像素），取值无特殊含义
static const int kRcKeepH = 300;
// 最小尺寸（像素），取关闭提示框的整窗尺寸 380×280
static const int kRcMinW = 380;
static const int kRcMinH = 280;
// 低于最小尺寸的请求（像素）
static const int kRcBelowMinW = 300;
static const int kRcBelowMinH = 200;

// 建一个与业务窗口同样式（WS_OVERLAPPEDWINDOW）但不显示的 DuiFrameWindow。
// 不加 WS_VISIBLE，跑测试时屏幕上不会闪过窗口。
//   f：[出参] 尚未创建的窗口对象。
//   返回：创建成功返回 true。
static bool CreateHiddenFrame(DuiFrameWindow& f)
{
    RECT rc = { 0, 0, kRcInitW, kRcInitH };
    return f.Create(nullptr, rc, _T("DuiFrameWindowTests"), WS_OVERLAPPEDWINDOW, 0) != nullptr;
}

// 读一个窗口的整窗尺寸（像素）。
static SIZE WindowSizeOf(HWND hWnd)
{
    RECT rc = { 0, 0, 0, 0 };
    ::GetWindowRect(hWnd, &rc);
    SIZE s = { rc.right - rc.left, rc.bottom - rc.top };
    return s;
}

// 读一个窗口的客户区尺寸（像素）。
static SIZE ClientSizeOf(HWND hWnd)
{
    RECT rc = { 0, 0, 0, 0 };
    ::GetClientRect(hWnd, &rc);
    SIZE s = { rc.right, rc.bottom };
    return s;
}

// ResizeClient(宽, 高) 之后整窗与客户区都正好是宽×高。继承自 ATL 的版本会按
// WS_OVERLAPPEDWINDOW 再叠加一圈系统标题栏与边框（120 DPI 下得到 658×667）。
// 各个尺寸先读出来、销毁窗口之后再断言：断言失败会提前返回，窗口须先销毁。
static Result Test_ResizeClientExact()
{
    DuiFrameWindow f;
    EXPECT_TRUE(CreateHiddenFrame(f), _T("RcExact/create"));
    f.ResizeClient(kRcTargetW, kRcTargetH);
    const SIZE w = WindowSizeOf(f.m_hWnd);
    const SIZE c = ClientSizeOf(f.m_hWnd);
    f.DestroyWindow();
    EXPECT_INT(w.cx, kRcTargetW, _T("RcExact/windowW"));
    EXPECT_INT(w.cy, kRcTargetH, _T("RcExact/windowH"));
    EXPECT_INT(c.cx, kRcTargetW, _T("RcExact/clientW"));
    EXPECT_INT(c.cy, kRcTargetH, _T("RcExact/clientH"));
    return OK(_T("ResizeClientExact"));
}

// 宽或高传 -1 时该方向保持当前尺寸（与 ATL 版本的约定一致）。
static Result Test_ResizeClientKeepsDimension()
{
    DuiFrameWindow f;
    EXPECT_TRUE(CreateHiddenFrame(f), _T("RcKeep/create"));
    f.ResizeClient(kRcTargetW, kRcTargetH);
    f.ResizeClient(-1, kRcKeepH);
    const SIZE onlyH = WindowSizeOf(f.m_hWnd);
    f.ResizeClient(kRcInitW, -1);
    const SIZE onlyW = WindowSizeOf(f.m_hWnd);
    f.DestroyWindow();
    EXPECT_INT(onlyH.cx, kRcTargetW, _T("RcKeep/onlyH/W"));
    EXPECT_INT(onlyH.cy, kRcKeepH,   _T("RcKeep/onlyH/H"));
    EXPECT_INT(onlyW.cx, kRcInitW,   _T("RcKeep/onlyW/W"));
    EXPECT_INT(onlyW.cy, kRcKeepH,   _T("RcKeep/onlyW/H"));
    return OK(_T("ResizeClientKeepsDimension"));
}

// 请求的尺寸低于 SetMinSize 设定的下限时，系统把窗口抬到下限（WS_THICKFRAME 窗口在
// WM_WINDOWPOSCHANGING 里按 WM_GETMINMAXINFO 限制尺寸）。调用方设置尺寸时要连带把下限改对。
static Result Test_ResizeClientHonorsMinSize()
{
    DuiFrameWindow f;
    EXPECT_TRUE(CreateHiddenFrame(f), _T("RcMin/create"));
    f.SetMinSize(kRcMinW, kRcMinH);
    f.ResizeClient(kRcBelowMinW, kRcBelowMinH);
    const SIZE w = WindowSizeOf(f.m_hWnd);
    f.DestroyWindow();
    EXPECT_INT(w.cx, kRcMinW, _T("RcMin/W"));
    EXPECT_INT(w.cy, kRcMinH, _T("RcMin/H"));
    return OK(_T("ResizeClientHonorsMinSize"));
}


// 框架窗口按本类声明的窗口类注册（2026-10-06）：类名为 __DuiFrameWindow__，样式与修复前实际生效的
// __DuiHost__ 相同（改大小整窗重绘、带双击），窗口行为不变。修复前这里取到的类名是 __DuiHost__。
static Result Test_WindowClassIsOwn()
{
    DuiFrameWindow f;
    if (!CreateHiddenFrame(f))
    {
        return Fail(_T("WindowClassIsOwn"), _T("create failed"));
    }
    TCHAR cls[64] = { 0 };
    ::GetClassName(f.m_hWnd, cls, 64);
    const ULONG_PTR style = ::GetClassLongPtr(f.m_hWnd, GCL_STYLE);
    f.DestroyWindow();
    EXPECT_STR(CString(cls), _T("__DuiFrameWindow__"), _T("WindowClassIsOwn/name"));
    EXPECT_TRUE((style & CS_DBLCLKS) != 0, _T("WindowClassIsOwn/dblclks"));
    EXPECT_TRUE((style & CS_HREDRAW) != 0, _T("WindowClassIsOwn/hredraw"));
    EXPECT_TRUE((style & CS_VREDRAW) != 0, _T("WindowClassIsOwn/vredraw"));
    return OK(_T("WindowClassIsOwn"));
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
        { _T("ResizeClientExact"),          &Test_ResizeClientExact          },
        { _T("ResizeClientKeepsDimension"), &Test_ResizeClientKeepsDimension },
        { _T("ResizeClientHonorsMinSize"),  &Test_ResizeClientHonorsMinSize  },
        { _T("WindowClassIsOwn"),           &Test_WindowClassIsOwn           },
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
