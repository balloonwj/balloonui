#include "stdafx.h"
#include "DuiDpiTests.h"
#include "../BalloonUiFeatures.h"
#include "../DuiResMgr.h"
#include "../DuiHost.h"
#include "../DuiControl.h"
#include "../Controls/Layout/DuiLayout.h"
#include "../Controls/Basic/DuiLabel.h"
#include "../Controls/Basic/DuiButton.h"
#include "../Controls/Basic/DuiToast.h"
#include "../Controls/Input/DuiRichEdit.h"

#include <richedit.h>

#include <memory>

namespace balloonwjui {

namespace DuiDpiTests {

namespace {

struct Result
{
    CString name;
    bool ok;
    CString detail;
};

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

// ----- Scale / Unscale math -------------------------------------------

static Result Test_ScaleIdentity96()
{
    EXPECT_INT(DuiDpi::Scale(100, 96), 100, _T("Scl/96id"));
    EXPECT_INT(DuiDpi::Unscale(100, 96), 100, _T("Scl/96idU"));
    return OK(_T("ScaleIdentity96"));
}

static Result Test_Scale120pct()
{
    // 120dpi = 125%
    EXPECT_INT(DuiDpi::Scale(80, 120),  100, _T("Scl/125pct"));
    EXPECT_INT(DuiDpi::Unscale(100, 120), 80, _T("Scl/125pctU"));
    return OK(_T("Scale120pct"));
}

static Result Test_Scale144pct()
{
    // 144dpi = 150%
    EXPECT_INT(DuiDpi::Scale(40, 144),   60, _T("Scl/150pct"));
    EXPECT_INT(DuiDpi::Unscale(60, 144), 40, _T("Scl/150pctU"));
    return OK(_T("Scale144pct"));
}

static Result Test_ScaleZeroDpiFallsBack()
{
    EXPECT_INT(DuiDpi::Scale(123, 0),   123, _T("Scl/zeroFallback"));
    EXPECT_INT(DuiDpi::Scale(123, -10), 123, _T("Scl/negFallback"));
    return OK(_T("ScaleZeroDpiFallsBack"));
}

// ----- runtime queries ------------------------------------------------

static Result Test_GetSystemDpiPositive()
{
    int d = DuiDpi::GetSystemDpi();
    EXPECT_TRUE(d >= 72,  _T("Sys/lowerSane"));
    EXPECT_TRUE(d <= 600, _T("Sys/upperSane"));
    return OK(_T("GetSystemDpiPositive"));
}

static Result Test_GetWindowDpiNullFallback()
{
    int d = DuiDpi::GetWindowDpi(nullptr);
    EXPECT_TRUE(d > 0, _T("Win/nullFallback"));
    return OK(_T("GetWindowDpiNullFallback"));
}

// ----- DuiResMgr DPI plumbing -----------------------------------------

// Fonts are cached per DPI, so after SetDpi the next GetDefaultFont()
// returns one sized for the new DPI. Two calls at different DPIs should
// not return the *same* handle (each DPI gets its own lazily built font;
// the old one stays alive, see the RMF group below).
static Result Test_DpiInvalidatesFont()
{
    DuiResMgr& rm = DuiResMgr::Inst();
    rm.SetDpi(96);
    HFONT a = rm.GetDefaultFont();
    EXPECT_TRUE(a != nullptr, _T("Font/atDpi96"));
    rm.SetDpi(192);
    HFONT b = rm.GetDefaultFont();
    EXPECT_TRUE(b != nullptr, _T("Font/atDpi192"));
    EXPECT_TRUE(a != b, _T("Font/handleChanged"));
    EXPECT_INT(rm.GetDpi(), 192, _T("Font/getDpi"));
    // Restore for downstream tests + ambient code.
    rm.SetDpi(DuiDpi::GetSystemDpi());
    return OK(_T("DpiInvalidatesFont"));
}

// SetDpi to the same value is a no-op (HFONT handle stays the same).
static Result Test_DpiIdempotent()
{
    DuiResMgr& rm = DuiResMgr::Inst();
    rm.SetDpi(96);
    HFONT a = rm.GetDefaultFont();
    rm.SetDpi(96);
    HFONT b = rm.GetDefaultFont();
    EXPECT_TRUE(a == b, _T("Idem/sameHandle"));
    rm.SetDpi(DuiDpi::GetSystemDpi());
    return OK(_T("DpiIdempotent"));
}

// ----- DuiResMgr 字体句柄在 DPI 切换后的有效性（RMF 组）-------------------
//
// 控件会把 GetFontByPointSize 等取到的句柄长期保存（DuiButton::SetTextPointSize、
// DuiLabel::SetFont 等），所以 SetDpi 不得销毁已经交出去的句柄。本组用例覆盖：
// 切换后旧句柄仍有效、新句柄按新 DPI 定字号、切回旧 DPI 时复用原句柄、反复切换
// 不持续新建 GDI 对象。

// 用例里切换的两档 DPI：96 对应 100% 缩放，192 对应 200% 缩放。
static const int kRmfLowDpi  = 96;
static const int kRmfHighDpi = 192;
// 1 英寸 = 72 磅。DuiResMgr 按 lfHeight = -MulDiv(pt, dpi, 72) 由磅值算字高。
static const int kRmfPointsPerInch = 72;
// 用例取指定字号字体时用的磅值。
static const int kRmfProbePt = 12;
// DuiResMgr 默认字体的磅值（见 DuiResMgr.cpp 的 GetDefaultFont，固定 9 磅）。
static const int kRmfDefaultFontPt = 9;
// RMF4 在两档 DPI 之间来回切换的轮数。
static const int kRmfToggleRounds = 20;
// 每档 DPI 下用例会取的字体种类数：默认字体、指定字号字体、指定字号抗锯齿字体。
static const int kRmfFontKinds = 3;

// 在作用域结束时把 DuiResMgr 的 DPI 恢复为进入时的值。用例中途断言失败会提前
// return，靠析构保证不把改过的 DPI 留给后续用例与界面。
class RmfDpiRestorer
{
public:
    RmfDpiRestorer()
        : m_savedDpi(DuiResMgr::Inst().GetDpi())
    {
    }

    ~RmfDpiRestorer()
    {
        // GetDpi 返回 0 表示进入时尚未设置过 DPI，此时恢复为系统 DPI。
        DuiResMgr::Inst().SetDpi(m_savedDpi > 0 ? m_savedDpi : DuiDpi::GetSystemDpi());
    }

private:
    RmfDpiRestorer(const RmfDpiRestorer&) = delete;
    RmfDpiRestorer& operator=(const RmfDpiRestorer&) = delete;

    int m_savedDpi; // 进入作用域时 DuiResMgr 的 DPI，析构时据此恢复
};

// 取字体的 lfHeight；句柄无效时返回 0。
static int RmfFontHeight(HFONT hf)
{
    LOGFONT lf;
    ::memset(&lf, 0, sizeof(lf));
    if (hf == nullptr || ::GetObject(hf, sizeof(lf), &lf) == 0)
    {
        return 0;
    }
    return lf.lfHeight;
}

// 判断句柄是否仍是一个有效的字体对象（已被 DeleteObject 的句柄返回 false）。
// 不能用 GetObjectType 判断：字体被 DeleteObject 之后它仍返回 OBJ_FONT（2026-10-01
// 实测），而 GetObject 读取 LOGFONT 会失败、返回 0。
static bool RmfIsLiveFont(HFONT hf)
{
    LOGFONT lf;
    ::memset(&lf, 0, sizeof(lf));
    return hf != nullptr && ::GetObject(hf, sizeof(lf), &lf) != 0;
}

// RMF1：DPI 切换之前取到的三种字体句柄，切换之后仍是有效的字体对象。
static Result Test_FontHandlesSurviveDpiChange()
{
    RmfDpiRestorer restore;
    DuiResMgr& rm = DuiResMgr::Inst();
    rm.SetDpi(kRmfLowDpi);
    HFONT def = rm.GetDefaultFont();
    HFONT pt  = rm.GetFontByPointSize(kRmfProbePt, false);
    HFONT aa  = rm.GetAntiAliasedFontByPointSize(kRmfProbePt, false);
    EXPECT_TRUE(RmfIsLiveFont(def) && RmfIsLiveFont(pt) && RmfIsLiveFont(aa),
                _T("RMF1/liveBefore"));

    rm.SetDpi(kRmfHighDpi);
    EXPECT_TRUE(RmfIsLiveFont(def), _T("RMF1/defaultAlive"));
    EXPECT_TRUE(RmfIsLiveFont(pt),  _T("RMF1/pointSizeAlive"));
    EXPECT_TRUE(RmfIsLiveFont(aa),  _T("RMF1/antiAliasedAlive"));
    return OK(_T("FontHandlesSurviveDpiChange"));
}

// RMF2：切到新 DPI 后再取字体，字号按新 DPI 计算（lfHeight = -MulDiv(pt, dpi, 72)）。
static Result Test_FontsSizedForNewDpi()
{
    RmfDpiRestorer restore;
    DuiResMgr& rm = DuiResMgr::Inst();
    rm.SetDpi(kRmfLowDpi);
    rm.SetDpi(kRmfHighDpi);
    EXPECT_INT(RmfFontHeight(rm.GetDefaultFont()),
               -::MulDiv(kRmfDefaultFontPt, kRmfHighDpi, kRmfPointsPerInch),
               _T("RMF2/defaultHeight"));
    EXPECT_INT(RmfFontHeight(rm.GetFontByPointSize(kRmfProbePt, false)),
               -::MulDiv(kRmfProbePt, kRmfHighDpi, kRmfPointsPerInch),
               _T("RMF2/pointSizeHeight"));
    EXPECT_INT(RmfFontHeight(rm.GetAntiAliasedFontByPointSize(kRmfProbePt, false)),
               -::MulDiv(kRmfProbePt, kRmfHighDpi, kRmfPointsPerInch),
               _T("RMF2/antiAliasedHeight"));
    return OK(_T("FontsSizedForNewDpi"));
}

// RMF3：切走再切回原来的 DPI，取到的是与最初相同的句柄。
static Result Test_SwitchBackReusesHandles()
{
    RmfDpiRestorer restore;
    DuiResMgr& rm = DuiResMgr::Inst();
    rm.SetDpi(kRmfLowDpi);
    HFONT def = rm.GetDefaultFont();
    HFONT pt  = rm.GetFontByPointSize(kRmfProbePt, false);
    HFONT aa  = rm.GetAntiAliasedFontByPointSize(kRmfProbePt, false);

    rm.SetDpi(kRmfHighDpi);
    rm.GetDefaultFont();
    rm.GetFontByPointSize(kRmfProbePt, false);
    rm.GetAntiAliasedFontByPointSize(kRmfProbePt, false);

    rm.SetDpi(kRmfLowDpi);
    EXPECT_TRUE(rm.GetDefaultFont() == def, _T("RMF3/defaultReused"));
    EXPECT_TRUE(rm.GetFontByPointSize(kRmfProbePt, false) == pt, _T("RMF3/pointSizeReused"));
    EXPECT_TRUE(rm.GetAntiAliasedFontByPointSize(kRmfProbePt, false) == aa,
                _T("RMF3/antiAliasedReused"));
    return OK(_T("SwitchBackReusesHandles"));
}

// RMF4：在两档 DPI 之间反复切换并每次取字体，进程 GDI 对象数的增量不超过
// 「DPI 档数 × 字体种类数」，不随切换次数增长。
static Result Test_DpiTogglingDoesNotGrowGdi()
{
    RmfDpiRestorer restore;
    DuiResMgr& rm = DuiResMgr::Inst();
    const DWORD before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < kRmfToggleRounds; ++i)
    {
        rm.SetDpi((i % 2 == 0) ? kRmfLowDpi : kRmfHighDpi);
        rm.GetDefaultFont();
        rm.GetFontByPointSize(kRmfProbePt, false);
        rm.GetAntiAliasedFontByPointSize(kRmfProbePt, false);
    }
    const DWORD after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    // 两档 DPI 各最多新建一份三种字体。
    const int kMaxGrowth = 2 * kRmfFontKinds;
    EXPECT_TRUE((int)after - (int)before <= kMaxGrowth, _T("RMF4/boundedGrowth"));
    return OK(_T("DpiTogglingDoesNotGrowGdi"));
}

// ----- 控件按所在窗口的 DPI 取字体（RMD 组）---------------------------------
//
// 不同缩放比例的显示器上，窗口各有各的 DPI。控件经 DuiControl::GetDpi 取所在
// 宿主窗口的 DPI，再据此取字体；宿主收到 WM_DPICHANGED 时把新 DPI 广播给整棵
// 控件树并重新布局。用例里向测试宿主发送 WM_DPICHANGED（lParam 为空，不移动窗口）
// 来模拟窗口被拖到另一块显示器。

// RMD 组用到的第三档 DPI：144 对应 150% 缩放。
static const int kRmdMidDpi = 144;
// 测试宿主窗口的宽、高（像素）。
static const int kRmdHostW = 300;
static const int kRmdHostH = 200;
// 测试控件在竖直布局里的固定高度（像素）。
static const int kRmdRowH = 30;

// 承载测试宿主的顶层窗口，放在屏幕可见范围之外，析构时销毁。
class RmdTopWnd
{
public:
    RmdTopWnd()
        : m_hwnd(nullptr)
    {
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = ::DefWindowProc;
        wc.hInstance     = ::GetModuleHandle(nullptr);
        wc.lpszClassName = _T("DuiDpiTestsTopWnd");
        ::RegisterClassEx(&wc);

        m_hwnd = ::CreateWindowEx(
            WS_EX_TOOLWINDOW, _T("DuiDpiTestsTopWnd"), _T(""),
            WS_POPUP | WS_VISIBLE,
            -32000, -32000, kRmdHostW, kRmdHostH,
            nullptr, nullptr, ::GetModuleHandle(nullptr), nullptr);
    }

    ~RmdTopWnd()
    {
        if (m_hwnd != nullptr)
        {
            ::DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    RmdTopWnd(const RmdTopWnd&) = delete;
    RmdTopWnd& operator=(const RmdTopWnd&) = delete;

    HWND get() const { return m_hwnd; }

private:
    HWND m_hwnd;   // 顶层窗口句柄，析构时销毁
};

// 记录布局与 DPI 变化回调的测试控件，不画任何东西。
class RmdProbe : public DuiControl
{
public:
    int m_layouts         = 0;   // Layout 被调用的次数
    int m_dpiAtLastLayout = 0;   // 最近一次 Layout 时 GetDpi() 的返回值
    int m_dpiChangedCalls = 0;   // OnDpiChanged 被调用的次数
    int m_lastDpiChanged  = 0;   // 最近一次 OnDpiChanged 收到的 DPI

    void Layout(const RECT& rcAvail) override
    {
        DuiControl::Layout(rcAvail);
        ++m_layouts;
        m_dpiAtLastLayout = GetDpi();
    }

    void OnDpiChanged(int dpi) override
    {
        ++m_dpiChangedCalls;
        m_lastDpiChanged = dpi;
    }
};

// 测试宿主：竖直布局的根容器下放一个探测控件（子），再放一个内层容器，内层容器里
// 放另一个探测控件（孙）。
struct RmdHostFixture
{
    RmdTopWnd top;
    DuiHost   host;
    RmdProbe* child      = nullptr;   // 根容器的直接子控件（借用，归控件树所有）
    RmdProbe* grandchild = nullptr;   // 内层容器里的孙控件（借用，归控件树所有）

    RmdHostFixture()
    {
        if (top.get() == nullptr)
        {
            return;
        }
        RECT rcHost;
        ::SetRect(&rcHost, 0, 0, kRmdHostW, kRmdHostH);
        host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);

        std::unique_ptr<DuiVBox> root(new DuiVBox());
        std::unique_ptr<RmdProbe> c(new RmdProbe());
        child = c.get();
        root->AddChild(std::move(c), DuiLayout::Hint().Fixed(kRmdRowH));

        std::unique_ptr<DuiVBox> inner(new DuiVBox());
        std::unique_ptr<RmdProbe> g(new RmdProbe());
        grandchild = g.get();
        inner->AddChild(std::move(g), DuiLayout::Hint().Fixed(kRmdRowH));
        root->AddChild(std::move(inner), DuiLayout::Hint().Weight(1));

        host.SetRoot(std::move(root));
    }

    ~RmdHostFixture()
    {
        if (host.IsWindow())
        {
            host.DestroyWindow();
        }
    }

    RmdHostFixture(const RmdHostFixture&) = delete;
    RmdHostFixture& operator=(const RmdHostFixture&) = delete;

    bool Ready() const { return host.IsWindow() && child != nullptr && grandchild != nullptr; }
};

// 向宿主发送 WM_DPICHANGED，模拟窗口被拖到 DPI 为 dpi 的显示器。lParam 为空，
// 宿主不会移动或缩放窗口，客户区矩形保持不变。
static void RmdSendDpiChanged(DuiHost& host, int dpi)
{
    ::SendMessage(host.m_hWnd, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), 0);
}

// RMD1：带 DPI 参数的取字体接口按参数给的 DPI 定字号，与全局 DPI 无关，也不改全局 DPI；
// 与全局 DPI 恰好相同时，两条路径取到的是同一个句柄。
static Result Test_ExplicitDpiFontIgnoresGlobalDpi()
{
    RmfDpiRestorer restore;
    DuiResMgr& rm = DuiResMgr::Inst();
    rm.SetDpi(kRmfLowDpi);
    EXPECT_INT(RmfFontHeight(rm.GetDefaultFontForDpi(kRmdMidDpi)),
               -::MulDiv(kRmfDefaultFontPt, kRmdMidDpi, kRmfPointsPerInch),
               _T("RMD1/defaultHeight"));
    HFONT pt = rm.GetFontByPointSizeForDpi(kRmfProbePt, false, kRmdMidDpi);
    EXPECT_INT(RmfFontHeight(pt), -::MulDiv(kRmfProbePt, kRmdMidDpi, kRmfPointsPerInch),
               _T("RMD1/pointSizeHeight"));
    EXPECT_INT(RmfFontHeight(rm.GetAntiAliasedFontByPointSizeForDpi(kRmfProbePt, false, kRmdMidDpi)),
               -::MulDiv(kRmfProbePt, kRmdMidDpi, kRmfPointsPerInch),
               _T("RMD1/antiAliasedHeight"));
    EXPECT_INT(rm.GetDpi(), kRmfLowDpi, _T("RMD1/globalUntouched"));

    rm.SetDpi(kRmdMidDpi);
    EXPECT_TRUE(rm.GetFontByPointSize(kRmfProbePt, false) == pt, _T("RMD1/sameHandleAsGlobalPath"));
    return OK(_T("ExplicitDpiFontIgnoresGlobalDpi"));
}

// RMD2：控件未挂到宿主上时 GetDpi 返回全局 DPI；挂上之后返回宿主窗口的 DPI，不受全局
// DPI 影响；宿主收到 WM_DPICHANGED 后跟着变。按控件 DPI 取字体的便捷函数用的也是它。
static Result Test_ControlDpiFollowsHost()
{
    RmfDpiRestorer restore;
    DuiResMgr::Inst().SetDpi(kRmdMidDpi);
    RmdProbe detached;
    EXPECT_INT(detached.GetDpi(), kRmdMidDpi, _T("RMD2/detachedUsesGlobal"));

    RmdHostFixture fx;
    EXPECT_TRUE(fx.Ready(), _T("RMD2/fixture"));
    RmdSendDpiChanged(fx.host, kRmfLowDpi);
    DuiResMgr::Inst().SetDpi(kRmdMidDpi);
    EXPECT_INT(fx.child->GetDpi(), kRmfLowDpi, _T("RMD2/attachedUsesHost"));

    RmdSendDpiChanged(fx.host, kRmfHighDpi);
    EXPECT_INT(fx.child->GetDpi(), kRmfHighDpi, _T("RMD2/followsDpiChange"));
    EXPECT_INT(RmfFontHeight(fx.child->GetFontByPointSize(kRmfProbePt, false)),
               -::MulDiv(kRmfProbePt, kRmfHighDpi, kRmfPointsPerInch),
               _T("RMD2/fontAtControlDpi"));
    EXPECT_INT(RmfFontHeight(fx.grandchild->GetDefaultFont()),
               -::MulDiv(kRmfDefaultFontPt, kRmfHighDpi, kRmfPointsPerInch),
               _T("RMD2/defaultFontAtControlDpi"));
    return OK(_T("ControlDpiFollowsHost"));
}

// RMD7：WM_DPICHANGED 即使不改变客户区矩形，也会让控件树里的每个控件重新布局，
// 且布局时控件看到的已经是新 DPI。
static Result Test_DpiChangeRelayoutsWholeTree()
{
    RmfDpiRestorer restore;
    RmdHostFixture fx;
    EXPECT_TRUE(fx.Ready(), _T("RMD7/fixture"));
    RmdSendDpiChanged(fx.host, kRmfLowDpi);
    const int childBefore      = fx.child->m_layouts;
    const int grandchildBefore = fx.grandchild->m_layouts;

    RmdSendDpiChanged(fx.host, kRmfHighDpi);
    EXPECT_TRUE(fx.child->m_layouts > childBefore, _T("RMD7/childRelaidOut"));
    EXPECT_TRUE(fx.grandchild->m_layouts > grandchildBefore, _T("RMD7/grandchildRelaidOut"));
    EXPECT_INT(fx.child->m_dpiAtLastLayout, kRmfHighDpi, _T("RMD7/childSawNewDpi"));
    EXPECT_INT(fx.grandchild->m_dpiAtLastLayout, kRmfHighDpi, _T("RMD7/grandchildSawNewDpi"));
    return OK(_T("DpiChangeRelayoutsWholeTree"));
}

// RMD8：WM_DPICHANGED 把新 DPI 经 OnDpiChanged 广播到控件树的每一层，每个控件只收到一次。
static Result Test_DpiChangeBroadcastsToDescendants()
{
    RmfDpiRestorer restore;
    RmdHostFixture fx;
    EXPECT_TRUE(fx.Ready(), _T("RMD8/fixture"));
    const int childBefore      = fx.child->m_dpiChangedCalls;
    const int grandchildBefore = fx.grandchild->m_dpiChangedCalls;

    RmdSendDpiChanged(fx.host, kRmfHighDpi);
    EXPECT_INT(fx.child->m_dpiChangedCalls - childBefore, 1, _T("RMD8/childOnce"));
    EXPECT_INT(fx.grandchild->m_dpiChangedCalls - grandchildBefore, 1, _T("RMD8/grandchildOnce"));
    EXPECT_INT(fx.child->m_lastDpiChanged, kRmfHighDpi, _T("RMD8/childDpi"));
    EXPECT_INT(fx.grandchild->m_lastDpiChanged, kRmfHighDpi, _T("RMD8/grandchildDpi"));
    return OK(_T("DpiChangeBroadcastsToDescendants"));
}

#if BUI_FEATURE_LABEL && BUI_FEATURE_BUTTON && BUI_FEATURE_TOAST

// 测试用标签的文字。
static LPCTSTR const kRmdLabelText = _T("DPI 测试文字");

// 按内容报告期望高度的标签：期望高度 = 当前宽度下完整显示文字所需的高度。库里的
// DuiLabel 本身不报告期望尺寸，这里补上，用来观察「DPI 变化 → 字号变化 → 重新布局
// 后控件变高」这一整条链路。
class RmdAutoLabel : public DuiLabel
{
public:
    SIZE GetDesiredSize() const override
    {
        SIZE sz;
        sz.cx = 0;
        sz.cy = MeasureHeight(m_rcItem.right - m_rcItem.left);
        return sz;
    }
};

// 放字体相关控件的测试宿主：竖直布局里依次是按磅值设字号的标签、按钮、提示条，
// 以及一个按内容定高的标签。
struct RmdFontFixture
{
    RmdTopWnd     top;
    DuiHost       host;
    DuiLabel*     label     = nullptr;   // 固定行高的标签（借用，归控件树所有）
    DuiButton*    button    = nullptr;   // 固定行高的按钮（借用，归控件树所有）
    DuiToast*     toast     = nullptr;   // 提示条，不显示，只取字体（借用，归控件树所有）
    RmdAutoLabel* autoLabel = nullptr;   // 按内容定高的标签（借用，归控件树所有）

    RmdFontFixture()
    {
        if (top.get() == nullptr)
        {
            return;
        }
        RECT rcHost;
        ::SetRect(&rcHost, 0, 0, kRmdHostW, kRmdHostH);
        host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);

        std::unique_ptr<DuiVBox> root(new DuiVBox());
        std::unique_ptr<DuiLabel> l(new DuiLabel());
        label = l.get();
        label->SetText(kRmdLabelText);
        root->AddChild(std::move(l), DuiLayout::Hint().Fixed(kRmdRowH));

        std::unique_ptr<DuiButton> b(new DuiButton());
        button = b.get();
        root->AddChild(std::move(b), DuiLayout::Hint().Fixed(kRmdRowH));

        std::unique_ptr<DuiToast> t(new DuiToast());
        toast = t.get();
        root->AddChild(std::move(t), DuiLayout::Hint().Fixed(kRmdRowH));

        std::unique_ptr<RmdAutoLabel> a(new RmdAutoLabel());
        autoLabel = a.get();
        autoLabel->SetText(kRmdLabelText);
        root->AddChild(std::move(a), DuiLayout::Hint().Auto());

        host.SetRoot(std::move(root));
    }

    ~RmdFontFixture()
    {
        if (host.IsWindow())
        {
            host.DestroyWindow();
        }
    }

    RmdFontFixture(const RmdFontFixture&) = delete;
    RmdFontFixture& operator=(const RmdFontFixture&) = delete;

    bool Ready() const
    {
        return host.IsWindow() && label != nullptr && button != nullptr
            && toast != nullptr && autoLabel != nullptr;
    }
};

// RMD3：两个窗口分别处于 96 与 192 DPI，各自的标签按磅值设字号后，同时按各自窗口的
// DPI 取到字体；之后改全局 DPI，两者都不受影响。
static Result Test_LabelFontPerWindowDpi()
{
    RmfDpiRestorer restore;
    RmdFontFixture low;
    RmdFontFixture high;
    EXPECT_TRUE(low.Ready() && high.Ready(), _T("RMD3/fixture"));
    RmdSendDpiChanged(low.host, kRmfLowDpi);
    RmdSendDpiChanged(high.host, kRmfHighDpi);
    low.label->SetTextPointSize(kRmfProbePt, false);
    high.label->SetTextPointSize(kRmfProbePt, false);

    const int lowHeight  = -::MulDiv(kRmfProbePt, kRmfLowDpi, kRmfPointsPerInch);
    const int highHeight = -::MulDiv(kRmfProbePt, kRmfHighDpi, kRmfPointsPerInch);
    EXPECT_INT(RmfFontHeight(low.label->GetFont()), lowHeight, _T("RMD3/lowWindow"));
    EXPECT_INT(RmfFontHeight(high.label->GetFont()), highHeight, _T("RMD3/highWindow"));

    DuiResMgr::Inst().SetDpi(kRmdMidDpi);
    EXPECT_INT(RmfFontHeight(low.label->GetFont()), lowHeight, _T("RMD3/lowIgnoresGlobal"));
    EXPECT_INT(RmfFontHeight(high.label->GetFont()), highHeight, _T("RMD3/highIgnoresGlobal"));
    EXPECT_INT(low.label->GetTextPointSize(), kRmfProbePt, _T("RMD3/pointSizeKept"));
    return OK(_T("LabelFontPerWindowDpi"));
}

// RMD4：按钮按磅值设字号后，所在窗口 DPI 由 96 变为 192，取到的字体字高随之加倍。
// 按钮绘制时直接使用 GetFont() 返回的字体，因此断言它即可。
static Result Test_ButtonFontFollowsDpiChange()
{
    RmfDpiRestorer restore;
    RmdFontFixture fx;
    EXPECT_TRUE(fx.Ready(), _T("RMD4/fixture"));
    RmdSendDpiChanged(fx.host, kRmfLowDpi);
    fx.button->SetTextPointSize(kRmfProbePt, true);
    EXPECT_INT(RmfFontHeight(fx.button->GetFont()),
               -::MulDiv(kRmfProbePt, kRmfLowDpi, kRmfPointsPerInch), _T("RMD4/before"));

    RmdSendDpiChanged(fx.host, kRmfHighDpi);
    EXPECT_INT(RmfFontHeight(fx.button->GetFont()),
               -::MulDiv(kRmfProbePt, kRmfHighDpi, kRmfPointsPerInch), _T("RMD4/after"));
    EXPECT_TRUE(fx.button->IsTextBold(), _T("RMD4/boldKept"));
    return OK(_T("ButtonFontFollowsDpiChange"));
}

// RMD5：标签用 SetFont 设了调用方自己创建的字体时，DPI 变化后仍是同一个句柄
// （这类字体由调用方负责，库不替换）。
static Result Test_CallerFontUnaffectedByDpi()
{
    RmfDpiRestorer restore;
    LOGFONT lf;
    ::memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -kRmdRowH / 2;
    _tcsncpy_s(lf.lfFaceName, _T("Microsoft YaHei"), _TRUNCATE);
    HFONT custom = ::CreateFontIndirect(&lf);
    EXPECT_TRUE(custom != nullptr, _T("RMD5/createFont"));

    Result r = OK(_T("CallerFontUnaffectedByDpi"));
    {
        RmdFontFixture fx;
        if (!fx.Ready())
        {
            r = Fail(_T("RMD5/fixture"), _T("condition false"));
        }
        else
        {
            RmdSendDpiChanged(fx.host, kRmfLowDpi);
            fx.label->SetFont(custom);
            RmdSendDpiChanged(fx.host, kRmfHighDpi);
            if (fx.label->GetFont() != custom)
            {
                r = Fail(_T("RMD5/sameHandle"), _T("condition false"));
            }
            else if (fx.label->GetTextPointSize() != 0)
            {
                r = Fail(_T("RMD5/pointSizeCleared"), _T("condition false"));
            }
        }
    }
    // 宿主销毁之后再释放字体，保证释放时没有控件还在使用它。
    ::DeleteObject(custom);
    return r;
}

// RMD6：提示条按磅值设字号后，取到的是所在窗口 DPI 下的抗锯齿字体。
static Result Test_ToastAntiAliasedFontAtWindowDpi()
{
    RmfDpiRestorer restore;
    RmdFontFixture fx;
    EXPECT_TRUE(fx.Ready(), _T("RMD6/fixture"));
    RmdSendDpiChanged(fx.host, kRmfHighDpi);
    DuiResMgr::Inst().SetDpi(kRmfLowDpi);
    fx.toast->SetTextPointSize(kRmfProbePt, false);

    LOGFONT lf;
    ::memset(&lf, 0, sizeof(lf));
    EXPECT_TRUE(::GetObject(fx.toast->GetFont(), sizeof(lf), &lf) != 0, _T("RMD6/liveFont"));
    EXPECT_INT(lf.lfHeight, -::MulDiv(kRmfProbePt, kRmfHighDpi, kRmfPointsPerInch),
               _T("RMD6/height"));
    EXPECT_INT(lf.lfQuality, ANTIALIASED_QUALITY, _T("RMD6/antiAliased"));
    return OK(_T("ToastAntiAliasedFontAtWindowDpi"));
}

// RMD7 补充：按内容定高的标签在 DPI 由 96 变为 192 后，经重新布局变高。
static Result Test_AutoHeightLabelGrowsWithDpi()
{
    RmfDpiRestorer restore;
    RmdFontFixture fx;
    EXPECT_TRUE(fx.Ready(), _T("RMD7b/fixture"));
    fx.autoLabel->SetTextPointSize(kRmfProbePt, false);
    RmdSendDpiChanged(fx.host, kRmfLowDpi);
    const RECT before = fx.autoLabel->GetRect();
    const int heightBefore = before.bottom - before.top;
    EXPECT_TRUE(heightBefore > 0, _T("RMD7b/measuredBefore"));

    RmdSendDpiChanged(fx.host, kRmfHighDpi);
    const RECT after = fx.autoLabel->GetRect();
    EXPECT_TRUE(after.bottom - after.top > heightBefore, _T("RMD7b/grew"));
    return OK(_T("AutoHeightLabelGrowsWithDpi"));
}

#endif // BUI_FEATURE_LABEL && BUI_FEATURE_BUTTON && BUI_FEATURE_TOAST

#if BUI_FEATURE_RICHTEXT

// 读取富文本控件引擎的默认字符格式字号（单位：1/1440 英寸）；读取失败返回 0。
static LONG RmdRichEditDefaultHeight(DuiRichEdit& edit)
{
    CHARFORMAT2W cf;
    ::memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE;
    edit.SendMessageToEngine(EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
    return ((cf.dwMask & CFM_SIZE) != 0) ? cf.yHeight : 0;
}

// RMD9：富文本控件用库内默认字体时，所在窗口 DPI 由 96 变为 192 后，引擎默认字符
// 格式的字号随之加倍（字号按像素高度换算，像素高度随 DPI 加倍）。
static Result Test_RichEditDefaultFontFollowsDpi()
{
    RmfDpiRestorer restore;
    RmdTopWnd top;
    EXPECT_TRUE(top.get() != nullptr, _T("RMD9/topWnd"));
    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, kRmdHostW, kRmdHostH);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
    EXPECT_TRUE(host.IsWindow(), _T("RMD9/host"));

    std::unique_ptr<DuiVBox> root(new DuiVBox());
    std::unique_ptr<DuiRichEdit> e(new DuiRichEdit());
    DuiRichEdit* edit = e.get();
    root->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
    host.SetRoot(std::move(root));

    RmdSendDpiChanged(host, kRmfLowDpi);
    const LONG before = RmdRichEditDefaultHeight(*edit);
    RmdSendDpiChanged(host, kRmfHighDpi);
    const LONG after = RmdRichEditDefaultHeight(*edit);
    host.DestroyWindow();

    EXPECT_TRUE(before > 0, _T("RMD9/readBefore"));
    EXPECT_TRUE(after > before, _T("RMD9/grew"));
    // 由像素高度换算字号有取整误差，按「约为两倍」判断：与两倍之差不超过 1 磅
    // （20 个 1/1440 英寸单位）。
    const LONG kTwipsPerPoint = 20;
    const LONG diff = after - before * 2;
    EXPECT_TRUE(diff <= kTwipsPerPoint && diff >= -kTwipsPerPoint, _T("RMD9/doubled"));
    return OK(_T("RichEditDefaultFontFollowsDpi"));
}

#endif // BUI_FEATURE_RICHTEXT

#undef EXPECT_INT
#undef EXPECT_TRUE

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn fn;
    };
    Entry tests[] = {
        { _T("ScaleIdentity96"),        &Test_ScaleIdentity96        },
        { _T("Scale120pct"),            &Test_Scale120pct            },
        { _T("Scale144pct"),            &Test_Scale144pct            },
        { _T("ScaleZeroDpiFallsBack"),  &Test_ScaleZeroDpiFallsBack  },
        { _T("GetSystemDpiPositive"),   &Test_GetSystemDpiPositive   },
        { _T("GetWindowDpiNullFallback"), &Test_GetWindowDpiNullFallback },
        { _T("DpiInvalidatesFont"),     &Test_DpiInvalidatesFont     },
        { _T("DpiIdempotent"),          &Test_DpiIdempotent          },
        { _T("FontHandlesSurviveDpiChange"), &Test_FontHandlesSurviveDpiChange },
        { _T("FontsSizedForNewDpi"),         &Test_FontsSizedForNewDpi         },
        { _T("SwitchBackReusesHandles"),     &Test_SwitchBackReusesHandles     },
        { _T("DpiTogglingDoesNotGrowGdi"),   &Test_DpiTogglingDoesNotGrowGdi   },
        { _T("ExplicitDpiFontIgnoresGlobalDpi"),  &Test_ExplicitDpiFontIgnoresGlobalDpi  },
        { _T("ControlDpiFollowsHost"),            &Test_ControlDpiFollowsHost            },
        { _T("DpiChangeRelayoutsWholeTree"),      &Test_DpiChangeRelayoutsWholeTree      },
        { _T("DpiChangeBroadcastsToDescendants"), &Test_DpiChangeBroadcastsToDescendants },
#if BUI_FEATURE_LABEL && BUI_FEATURE_BUTTON && BUI_FEATURE_TOAST
        { _T("LabelFontPerWindowDpi"),            &Test_LabelFontPerWindowDpi            },
        { _T("ButtonFontFollowsDpiChange"),       &Test_ButtonFontFollowsDpiChange       },
        { _T("CallerFontUnaffectedByDpi"),        &Test_CallerFontUnaffectedByDpi        },
        { _T("ToastAntiAliasedFontAtWindowDpi"),  &Test_ToastAntiAliasedFontAtWindowDpi  },
        { _T("AutoHeightLabelGrowsWithDpi"),      &Test_AutoHeightLabelGrowsWithDpi      },
#endif
#if BUI_FEATURE_RICHTEXT
        { _T("RichEditDefaultFontFollowsDpi"),    &Test_RichEditDefaultFontFollowsDpi    },
#endif
    };

    CString out;
    int passed = 0;
    int failed = 0;
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
    summary.Format(_T("[summary] DuiDpiTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiDpiTests

} // namespace balloonwjui
