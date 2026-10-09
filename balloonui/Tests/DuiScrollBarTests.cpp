#include "stdafx.h"
#include "DuiScrollBarTests.h"

#if BUI_FEATURE_SCROLLBAR

#include "../DuiHost.h"
#include "../Controls/Layout/DuiLayout.h"
#include "../DuiAnimation.h"       // OV 组：用 DuiAnimMgr::TickAll 推进淡入淡出
#include <gdiplus.h>                // OV1：滑块用 GDI+ 绘制

#include <memory>
#include <vector>


namespace balloonwjui {

namespace DuiScrollBarTests {

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

#define EXPECT_RECT(r, L, T, R, B, name) \
    do { const RECT& _rr = (r); \
         if (_rr.left != (L) || _rr.top != (T) || _rr.right != (R) || _rr.bottom != (B)) { \
             CString _d; _d.Format(_T("expected (%d,%d,%d,%d) got (%d,%d,%d,%d)"), \
                 (int)(L),(int)(T),(int)(R),(int)(B), _rr.left, _rr.top, _rr.right, _rr.bottom); \
             return Fail(name, _d); } \
    } while (0)

// Defaults: vertical, range [0,0], page 1, pos 0.
static Result Test_Defaults_NoCrash()
{
    DuiScrollBar sb;
    EXPECT_INT(sb.GetMin(), 0, _T("Defaults/min"));
    EXPECT_INT(sb.GetMax(), 0, _T("Defaults/max"));
    EXPECT_INT(sb.GetPos(), 0, _T("Defaults/pos"));
    EXPECT_INT(sb.GetPage(), 1, _T("Defaults/page"));
    return OK(_T("Defaults_NoCrash"));
}

// Pos clamps into [min, max].
static Result Test_PosClamp()
{
    DuiScrollBar sb;
    sb.SetRange(0, 100);
    sb.SetPage(10);
    sb.SetPos(50);
    EXPECT_INT(sb.GetPos(), 50,  _T("PosClamp/in-range"));
    sb.SetPos(200);
    EXPECT_INT(sb.GetPos(), 100, _T("PosClamp/over-max"));
    sb.SetPos(-5);
    EXPECT_INT(sb.GetPos(), 0,   _T("PosClamp/below-min"));
    return OK(_T("PosClamp"));
}

// Range shrink moves pos.
static Result Test_RangeShrinkClamps()
{
    DuiScrollBar sb;
    sb.SetRange(0, 100);
    sb.SetPos(80);
    sb.SetRange(0, 50);   // shrink
    EXPECT_INT(sb.GetPos(), 50, _T("RangeShrink"));
    return OK(_T("RangeShrink"));
}

// Thumb at top with pos=min.
static Result Test_ThumbAtTop()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });    // 200px vertical track
    sb.SetRange(0, 800);                   // 800-px scrollable
    sb.SetPage(200);                       // 200-px viewport
    sb.SetPos(0);
    RECT t = sb.ComputeThumbRect();
    // thumb size: 200 * 200 / (800+200) = 40 px, at top: 0..40.
    EXPECT_RECT(t, 0, 0, 12, 40, _T("ThumbAtTop"));
    return OK(_T("ThumbAtTop"));
}

// Thumb at bottom with pos=max.
static Result Test_ThumbAtBottom()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(800);
    RECT t = sb.ComputeThumbRect();
    // trackUsable = 200 - 40 = 160; offset = 160 * 800 / 800 = 160.
    EXPECT_RECT(t, 0, 160, 12, 200, _T("ThumbAtBottom"));
    return OK(_T("ThumbAtBottom"));
}

// Thumb mid-position.
static Result Test_ThumbMid()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(400);
    RECT t = sb.ComputeThumbRect();
    // offset = 160 * 400 / 800 = 80 -> thumb 80..120.
    EXPECT_RECT(t, 0, 80, 12, 120, _T("ThumbMid"));
    return OK(_T("ThumbMid"));
}

// MIN_THUMB enforced when content is huge.
static Result Test_ThumbMinimumSize()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 1000000);     // huge
    sb.SetPage(200);
    sb.SetPos(0);
    RECT t = sb.ComputeThumbRect();
    int thumbH = t.bottom - t.top;
    if (thumbH < DuiScrollBar::kMinThumbPx)
    {
        // 2026-10-04 起最短 36（与聊天窗的滚动条一致），原先是 18
        CString d;
        d.Format(_T("thumb=%d expected>=%d"), thumbH, (int)DuiScrollBar::kMinThumbPx);
        return Fail(_T("ThumbMinimumSize"), d);
    }
    return OK(_T("ThumbMinimumSize"));
}

// 点在滑块下方的轨道上（2026-10-04 起与聊天窗一致，此前按整页翻）：滑块中心跳到点击处，并进入拖动。
static Result Test_TrackClickJumpsDown()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(0);
    // 滑块 0..40；点在 y=100：滑块起点 = 100 - 40/2 = 80，pos = 800 * 80 / (200 - 40) = 400。
    sb.OnLButtonDown(POINT{ 6, 100 }, 0);
    EXPECT_INT(sb.GetPos(), 400, _T("TrackClickJumpsDown"));
    if (!sb.IsDragging())
    {
        return Fail(_T("TrackClickJumpsDown/dragging"), _T("track click should start dragging"));
    }
    // 接着按住往下拖 40 像素：滑块起点 120，pos = 800 * 120 / 160 = 600。
    sb.OnMouseMove(POINT{ 6, 140 }, 0);
    EXPECT_INT(sb.GetPos(), 600, _T("TrackClickJumpsDown/thenDrag"));
    sb.OnLButtonUp(POINT{ 6, 140 }, 0);
    return OK(_T("TrackClickJumpsDown"));
}

// 点在滑块上方的轨道上：同样是滑块中心跳到点击处。
static Result Test_TrackClickJumpsUp()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(600);     // 滑块 120..160
    sb.OnLButtonDown(POINT{ 6, 50 }, 0);
    // 滑块起点 = 50 - 20 = 30，pos = 800 * 30 / 160 = 150。
    EXPECT_INT(sb.GetPos(), 150, _T("TrackClickJumpsUp"));
    sb.OnLButtonUp(POINT{ 6, 50 }, 0);
    return OK(_T("TrackClickJumpsUp"));
}

// Drag thumb: mousedown on thumb, mousemove updates pos linearly.
static Result Test_DragThumb()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(0);                                // thumb 0..40
    sb.OnLButtonDown(POINT{ 6, 10 }, 0);          // grab inside thumb (offset 10)
    sb.OnMouseMove (POINT{ 6, 90 }, 0);          // newThumbStart = 90 - 0 - 10 = 80
    // pos = 0 + 800 * 80 / 160 = 400.
    EXPECT_INT(sb.GetPos(), 400, _T("DragThumb/mid"));
    sb.OnMouseMove(POINT{ 6, 5000 }, 0);         // far below -> clamp to bottom
    EXPECT_INT(sb.GetPos(), 800, _T("DragThumb/clampBottom"));
    sb.OnMouseMove(POINT{ 6, -5000 }, 0);        // far above -> clamp to top
    EXPECT_INT(sb.GetPos(), 0, _T("DragThumb/clampTop"));
    sb.OnLButtonUp(POINT{ 6, 0 }, 0);
    return OK(_T("DragThumb"));
}

// Mouse wheel scrolls by lineSize * 3 in opposite direction.
static Result Test_MouseWheel()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 1000);
    sb.SetPage(200);
    sb.SetLineSize(10);
    sb.SetPos(100);
    sb.OnMouseWheel(POINT{ 0, 0 }, -120, 0);   // scroll down 30
    EXPECT_INT(sb.GetPos(), 130, _T("MouseWheel/down"));
    sb.OnMouseWheel(POINT{ 0, 0 },  120, 0);   // scroll up 30
    EXPECT_INT(sb.GetPos(), 100, _T("MouseWheel/up"));
    return OK(_T("MouseWheel"));
}

// Notify callback fires on pos change.
struct CallbackCounter { int hits = 0; int lastPos = -1; };
static void CountCb(void* user, int pos)
{
    auto* c = static_cast<CallbackCounter*>(user);
    c->hits++;
    c->lastPos = pos;
}
static Result Test_OnScrollCallback()
{
    DuiScrollBar sb;
    sb.SetRange(0, 100);
    CallbackCounter c;
    sb.SetOnScroll(&CountCb, &c);
    sb.SetPos(50);
    EXPECT_INT(c.hits, 1, _T("Callback/firstHit"));
    EXPECT_INT(c.lastPos, 50, _T("Callback/firstPos"));
    sb.SetPos(50);   // no-op; same pos
    EXPECT_INT(c.hits, 1, _T("Callback/dedup"));
    sb.SetPos(75);
    EXPECT_INT(c.hits, 2, _T("Callback/secondHit"));
    sb.SetPos(0, /*notify=*/false);
    EXPECT_INT(c.hits, 2, _T("Callback/silentSetPos"));
    return OK(_T("OnScrollCallback"));
}

// Empty range (nothing to scroll): paint and clicks are safe; pos stays at min.
static Result Test_EmptyRange()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 0);
    sb.SetPage(200);
    sb.OnLButtonDown(POINT{ 6, 100 }, 0);
    EXPECT_INT(sb.GetPos(), 0, _T("EmptyRange/clickNoMove"));
    return OK(_T("EmptyRange"));
}

// ---------------------------------------------------------------------
// Mouse-wheel consumption contract (see DuiHost::DispatchMouseWheel).
//
//   * no scrollable range at all (max <= min) -> return false so the
//     wheel keeps bubbling to an outer scroll container;
//   * scrollable range exists but pos is already at the edge -> return
//     true (consume), matching native Win32 controls.
//
// The three tests below pin both halves down.
// ---------------------------------------------------------------------

// Empty range: the bar cannot scroll, so it must not swallow the wheel.
static Result Test_WheelEmptyRangeNotConsumed()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 0);          // nothing to scroll
    sb.SetPage(200);
    sb.SetLineSize(16);
    bool handled = sb.OnMouseWheel(POINT{ 6, 100 }, -WHEEL_DELTA, 0);
    EXPECT_INT((int)handled, 0, _T("WheelEmptyRange/notConsumed"));
    EXPECT_INT(sb.GetPos(), 0,  _T("WheelEmptyRange/posUnchanged"));
    return OK(_T("WheelEmptyRangeNotConsumed"));
}

// Scrollable range that is already at its edge: still consumed. Users
// habitually over-scroll past the end, and letting the page jump instead
// is more disorienting than simply not moving.
static Result Test_WheelAtEdgeStillConsumed()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetLineSize(16);

    sb.SetPos(800);             // already at the bottom
    bool downAtBottom = sb.OnMouseWheel(POINT{ 6, 100 }, -WHEEL_DELTA, 0);
    EXPECT_INT((int)downAtBottom, 1, _T("WheelAtEdge/downAtBottom"));
    EXPECT_INT(sb.GetPos(), 800,    _T("WheelAtEdge/bottomPosUnchanged"));

    sb.SetPos(0);               // already at the top
    bool upAtTop = sb.OnMouseWheel(POINT{ 6, 100 }, WHEEL_DELTA, 0);
    EXPECT_INT((int)upAtTop, 1, _T("WheelAtEdge/upAtTop"));
    EXPECT_INT(sb.GetPos(), 0,  _T("WheelAtEdge/topPosUnchanged"));
    return OK(_T("WheelAtEdgeStillConsumed"));
}

// Same contract seen through DuiScrollView, the way callers meet it:
// content shorter than the viewport leaves the inner bar with an empty
// range, so a nested view must let the wheel through to its parent.
static Result Test_ScrollViewWheelPassThroughWhenContentFits()
{
    DuiScrollView sv;
    sv.SetContentHeight(80);                       // shorter than the viewport
    sv.Layout(RECT{ 0, 0, 300, 200 });
    EXPECT_INT(sv.GetScrollBar()->GetMax(), 0, _T("ScrollViewFits/emptyRange"));
    bool handled = sv.OnMouseWheel(POINT{ 100, 100 }, -WHEEL_DELTA, 0);
    EXPECT_INT((int)handled, 0, _T("ScrollViewFits/notConsumed"));

    // And the overflowing case still consumes, edge included.
    DuiScrollView tall;
    tall.SetContentHeight(1000);
    tall.Layout(RECT{ 0, 0, 300, 200 });
    bool tallHandled = tall.OnMouseWheel(POINT{ 100, 100 }, -WHEEL_DELTA, 0);
    EXPECT_INT((int)tallHandled, 1, _T("ScrollViewOverflow/consumed"));
    tall.SetScrollPos(tall.GetScrollBar()->GetMax());
    bool atBottom = tall.OnMouseWheel(POINT{ 100, 100 }, -WHEEL_DELTA, 0);
    EXPECT_INT((int)atBottom, 1, _T("ScrollViewOverflow/consumedAtBottom"));
    return OK(_T("ScrollViewWheelPassThroughWhenContentFits"));
}

// Horizontal: same math, X-axis instead of Y.
static Result Test_Horizontal()
{
    DuiScrollBar sb(/*horizontal=*/true);
    sb.SetRect(RECT{ 0, 0, 200, 12 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(400);
    RECT t = sb.ComputeThumbRect();
    // Same as ThumbMid but on X.
    EXPECT_RECT(t, 80, 0, 120, 12, _T("Horizontal/thumb"));
    return OK(_T("Horizontal"));
}

// =====================================================================
// SVI：滚动视口里子控件的失效区域限制在视口之内（DuiControl::Invalidate
//      与 DuiScrollView::GetChildClipRect）
// =====================================================================
//
// 背景：DuiControl::SetRect 会对新旧两个矩形各失效一次，而滚动视口里的内容
// 控件高度往往远超视口，滚动后它的矩形伸到视口上下方。宿主不做裁剪的话，每
// 滚一格都会让视口上下方的其它控件跟着重画（实测主面板每格滚轮都把表头、页签、
// 底部按钮整个重画一遍）。这组用例用真窗口 + 真宿主，直接向系统询问待重绘区域。

// 测试窗口与布局的尺寸（像素）。
static const int kSviHostW   = 400;   // 宿主客户区宽
static const int kSviHostH   = 300;   // 宿主客户区高
static const int kSviBandH   = 50;    // 视口上方、下方两条控件的高度
static const int kSviRowH    = 100;   // 内容里每一行的高度
static const int kSviRows    = 20;    // 内容的行数（总高 2000，远超视口）
static const int kSviScrollTo = 500;  // SVI1 把内容滚到的位置
static const int kSviWheelSteps = 3;  // SVI2 连滚的格数
static const int kSviInnerTopPad = 100;  // SVI4 外层内容里内层视口上方的留白
static const int kSviInnerH  = 150;   // SVI4 内层视口的高度

// 离屏顶层窗口：宿主的父窗口。必须可见才会产生待重绘区域与绘制，挪到屏幕外以免
// 测试运行时在用户眼前闪一下（与 DuiRichEditTests 的同名辅助类同一手法）。
class SviTopWnd
{
public:
    SviTopWnd()
        : m_hwnd(nullptr)
    {
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = ::DefWindowProc;
        wc.hInstance     = ::GetModuleHandle(nullptr);
        wc.lpszClassName = _T("DuiScrollViewInvalidateTestTopWnd");
        ::RegisterClassEx(&wc);

        m_hwnd = ::CreateWindowEx(
            WS_EX_TOOLWINDOW, _T("DuiScrollViewInvalidateTestTopWnd"), _T(""),
            WS_POPUP | WS_VISIBLE,
            -32000, -32000, kSviHostW, kSviHostH,
            nullptr, nullptr, ::GetModuleHandle(nullptr), nullptr);
    }

    ~SviTopWnd()
    {
        if (m_hwnd != nullptr)
        {
            ::DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    HWND get() const { return m_hwnd; }

private:
    HWND m_hwnd;   // 顶层窗口句柄，析构时销毁
};

// 记录 OnPaint 被调用次数的测试控件，不画任何东西。
class SviPaintCounter : public DuiControl
{
public:
    int m_paints = 0;   // OnPaint 被调用的次数
    void OnPaint(HDC /*hdc*/, const RECT& /*rcDirty*/) override
    {
        ++m_paints;
    }
};

// ---- SVI2 用的窗口过程钩子：在 WM_PAINT 到达时记录系统的待重绘区域 ----
//
// 为什么不直接数视口上下方控件的绘制次数：放到屏幕外的测试窗口，BeginPaint 给出的
// rcPaint 并不等于待重绘区域。实测只让视口里一行失效（待重绘区域准确地是那一行），
// 视口上方的控件照样被整块画了一次。按绘制次数判断会误报，只能在 BeginPaint 之前
// 向系统询问待重绘区域。
static WNDPROC g_sviPrevProc   = nullptr;   // 被钩住之前的窗口过程，钩子把消息原样转给它
static RECT    g_sviPaintUnion = { 0, 0, 0, 0 };   // 钩住期间各次 WM_PAINT 待重绘区域的并集
static int     g_sviPaintCount = 0;         // 钩住期间收到的、带待重绘区域的 WM_PAINT 次数

static LRESULT CALLBACK SviPaintSpyProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_PAINT)
    {
        RECT rc;
        if (::GetUpdateRect(hwnd, &rc, FALSE))
        {
            ++g_sviPaintCount;
            RECT merged;
            ::UnionRect(&merged, &g_sviPaintUnion, &rc);
            g_sviPaintUnion = merged;
        }
    }
    return ::CallWindowProc(g_sviPrevProc, hwnd, msg, wParam, lParam);
}

// 在宿主窗口上挂钩子并清零记录。必须与 SviUnhookPaint 成对调用，且在宿主窗口销毁之前
// 摘掉：ATL 在窗口销毁时会检查窗口过程是否仍是它自己的。
static void SviHookPaint(HWND hwnd)
{
    g_sviPaintCount = 0;
    ::SetRectEmpty(&g_sviPaintUnion);
    g_sviPrevProc = (WNDPROC)::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)&SviPaintSpyProc);
}

static void SviUnhookPaint(HWND hwnd)
{
    ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)g_sviPrevProc);
    g_sviPrevProc = nullptr;
}

// 造一个装着 kSviRows 行计数控件的竖排内容；各行指针写入 rows（借用，树持有）。
static std::unique_ptr<DuiVBox> SviMakeRows(std::vector<SviPaintCounter*>& rows)
{
    std::unique_ptr<DuiVBox> content(new DuiVBox());
    content->SetPadding(0);
    content->SetGap(0);
    for (int i = 0; i < kSviRows; ++i)
    {
        std::unique_ptr<SviPaintCounter> row(new SviPaintCounter());
        rows.push_back(row.get());
        content->AddChild(std::move(row), DuiLayout::Hint().Fixed(kSviRowH));
    }
    return content;
}

// 测试布局：上方一条计数控件、中间滚动视口、下方一条计数控件，竖排铺满宿主。
// 视口矩形为 (0, 50)-(400, 250)，内容 20 行共 2000 高。
struct SviFixture
{
    SviTopWnd                     top;
    DuiHost                       host;
    SviPaintCounter*              header  = nullptr;   // 视口上方的控件（借用）
    SviPaintCounter*              footer  = nullptr;   // 视口下方的控件（借用）
    DuiScrollView*                sv      = nullptr;   // 滚动视口（借用）
    DuiVBox*                      content = nullptr;   // 视口的内容（借用）
    std::vector<SviPaintCounter*> rows;                // 内容的各行（借用）

    SviFixture()
    {
        if (top.get() == nullptr)
        {
            return;
        }
        RECT rcHost;
        ::SetRect(&rcHost, 0, 0, kSviHostW, kSviHostH);
        host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);

        std::unique_ptr<DuiVBox> root(new DuiVBox());
        root->SetPadding(0);
        root->SetGap(0);

        std::unique_ptr<SviPaintCounter> h(new SviPaintCounter());
        header = h.get();
        root->AddChild(std::move(h), DuiLayout::Hint().Fixed(kSviBandH));

        std::unique_ptr<DuiScrollView> view(new DuiScrollView());
        sv = view.get();
        std::unique_ptr<DuiVBox> c = SviMakeRows(rows);
        content = c.get();
        view->SetContent(std::move(c));
        view->SetContentHeight(kSviRows * kSviRowH);
        root->AddChild(std::move(view), DuiLayout::Hint().Weight(1));

        std::unique_ptr<SviPaintCounter> f(new SviPaintCounter());
        footer = f.get();
        root->AddChild(std::move(f), DuiLayout::Hint().Fixed(kSviBandH));

        host.SetRoot(std::move(root));
        ::UpdateWindow(host.m_hWnd);
    }

    ~SviFixture()
    {
        if (host.IsWindow())
        {
            host.DestroyWindow();
        }
    }

    bool Ready() const { return host.IsWindow() && sv != nullptr; }

    // 清掉宿主当前的全部待重绘区域，之后观察到的失效都来自被测操作。
    void ClearDirty()
    {
        ::ValidateRect(host.m_hWnd, nullptr);
    }

    // 取宿主当前的待重绘区域外接矩形；没有待重绘区域时返回 false。
    bool Dirty(RECT& rc) const
    {
        ::SetRect(&rc, 0, 0, 0, 0);
        return ::GetUpdateRect(host.m_hWnd, &rc, FALSE) != FALSE;
    }
};

// 把矩形格式化进失败信息。
static CString SviRectText(const RECT& rc)
{
    CString s;
    s.Format(_T("(%d,%d)-(%d,%d)"), rc.left, rc.top, rc.right, rc.bottom);
    return s;
}

// =====================================================================
// SVW：滚动量随 zDelta 成比例，不足一行的余量累积到下一次
// =====================================================================
//
// 普通鼠标每格 zDelta = 120（WHEEL_DELTA），滚 3 行，与原来一致；精确式触摸板与
// 高精度滚轮一次只发很小的 zDelta，原来每条消息都按整格滚 3 行，滚得飞快。

static const int kSvwLineSize   = 10;     // 测试用每行像素数
static const int kSvwRangeMax   = 1000;   // 测试用滚动范围上限
static const int kSvwStartPos   = 500;    // 起始位置：在范围中间，上下都滚得动
static const int kSvwDeltaPerLine = WHEEL_DELTA / 3;   // 滚一行所需的 zDelta（每格 3 行）

// 造一条处于范围中间、每行 kSvwLineSize 像素的滚动条。
static void SvwSetup(DuiScrollBar& sb)
{
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, kSvwRangeMax);
    sb.SetPage(200);
    sb.SetLineSize(kSvwLineSize);
    sb.SetPos(kSvwStartPos);
}

// SVW1：普通鼠标一格（120）仍然滚 3 行，向上向下对称。
static Result Test_SVW1_OneNotchStillScrollsThreeLines()
{
    DuiScrollBar sb;
    SvwSetup(sb);
    sb.OnMouseWheel(POINT{ 0, 0 }, -WHEEL_DELTA, 0);
    EXPECT_INT(sb.GetPos(), kSvwStartPos + 3 * kSvwLineSize, _T("SVW1/down"));
    sb.OnMouseWheel(POINT{ 0, 0 }, WHEEL_DELTA, 0);
    EXPECT_INT(sb.GetPos(), kSvwStartPos, _T("SVW1/up"));
    return OK(_T("SVW1_OneNotchStillScrollsThreeLines"));
}

// SVW2：四次 30 的总滚动量等于一次 120。
static Result Test_SVW2_SmallDeltasAddUpToOneNotch()
{
    const short kQuarterNotch = (short)(WHEEL_DELTA / 4);   // 30
    DuiScrollBar sb;
    SvwSetup(sb);
    for (int i = 0; i < 4; ++i)
    {
        sb.OnMouseWheel(POINT{ 0, 0 }, (short)-kQuarterNotch, 0);
    }
    EXPECT_INT(sb.GetPos(), kSvwStartPos + 3 * kSvwLineSize, _T("SVW2/fourQuarterNotches"));
    return OK(_T("SVW2_SmallDeltasAddUpToOneNotch"));
}

// SVW3：方向反转时清掉上一方向累积的余量。
//   先向上 30（不足一行，不动）；再向下 30：若沿用余量会正好抵消成 0，反转清零后
//   则记作向下 30；再向下 10 凑满向下 40，滚下一行。
static Result Test_SVW3_DirectionChangeDropsRemainder()
{
    const short kSmall = 30;   // 不足一行的增量
    const short kTiny  = 10;   // 与 kSmall 相加正好凑满一行
    DuiScrollBar sb;
    SvwSetup(sb);
    sb.OnMouseWheel(POINT{ 0, 0 }, kSmall, 0);
    EXPECT_INT(sb.GetPos(), kSvwStartPos, _T("SVW3/upPartialNoMove"));
    sb.OnMouseWheel(POINT{ 0, 0 }, (short)-kSmall, 0);
    EXPECT_INT(sb.GetPos(), kSvwStartPos, _T("SVW3/downPartialNoMove"));
    sb.OnMouseWheel(POINT{ 0, 0 }, (short)-kTiny, 0);
    EXPECT_INT(sb.GetPos(), kSvwStartPos + kSvwLineSize, _T("SVW3/downOneLine"));
    return OK(_T("SVW3_DirectionChangeDropsRemainder"));
}

// SVW4：若干次 10 累积到一行的量（40）才滚出第一行。
static Result Test_SVW4_TinyDeltasAccumulate()
{
    const short kTiny = 10;
    const int   kTinyPerLine = kSvwDeltaPerLine / kTiny;   // 4 次凑满一行
    DuiScrollBar sb;
    SvwSetup(sb);
    for (int i = 0; i < kTinyPerLine - 1; ++i)
    {
        sb.OnMouseWheel(POINT{ 0, 0 }, (short)-kTiny, 0);
    }
    EXPECT_INT(sb.GetPos(), kSvwStartPos, _T("SVW4/notYetOneLine"));
    sb.OnMouseWheel(POINT{ 0, 0 }, (short)-kTiny, 0);
    EXPECT_INT(sb.GetPos(), kSvwStartPos + kSvwLineSize, _T("SVW4/oneLine"));
    return OK(_T("SVW4_TinyDeltasAccumulate"));
}

// SVI1：内容滚动之后整体失效，待重绘区域不得超出滚动视口。
static Result Test_SVI1_ScrolledContentInvalidateStaysInViewport()
{
    SviFixture fx;
    if (!fx.Ready())
    {
        return Fail(_T("SVI1"), _T("cannot create host window"));
    }
    fx.sv->SetScrollPos(kSviScrollTo);
    fx.ClearDirty();
    fx.content->Invalidate();

    RECT dirty;
    const bool any = fx.Dirty(dirty);
    const RECT& view = fx.sv->GetRect();
    if (!any || dirty.top < view.top || dirty.bottom > view.bottom)
    {
        return Fail(_T("SVI1"), _T("dirty ") + SviRectText(dirty) + _T(" escapes viewport ")
                                + SviRectText(view));
    }
    return OK(_T("SVI1_ScrolledContentInvalidateStaysInViewport"));
}

// SVI2：连滚几格滚轮，每次重绘时系统的待重绘区域都不超出滚动视口；内容确实滚动并重画了。
// 待重绘区域在 WM_PAINT 到达时由窗口过程钩子记录（理由见 SviPaintSpyProc 上方的说明）。
static Result Test_SVI2_WheelRepaintStaysInViewport()
{
    SviFixture fx;
    if (!fx.Ready())
    {
        return Fail(_T("SVI2"), _T("cannot create host window"));
    }
    const RECT view = fx.sv->GetRect();
    POINT pt = { (view.left + view.right) / 2, (view.top + view.bottom) / 2 };
    ::ClientToScreen(fx.host.m_hWnd, &pt);

    fx.ClearDirty();
    SviHookPaint(fx.host.m_hWnd);
    for (int i = 0; i < kSviWheelSteps; ++i)
    {
        ::SendMessage(fx.host.m_hWnd, WM_MOUSEWHEEL, MAKEWPARAM(0, (WORD)(short)-WHEEL_DELTA),
                      MAKELPARAM(pt.x, pt.y));
        ::UpdateWindow(fx.host.m_hWnd);
    }
    SviUnhookPaint(fx.host.m_hWnd);

    // 前提：滚轮确实滚动了内容，也确实发生了重绘，否则下面的判断没有意义
    if (fx.sv->GetScrollPos() <= 0)
    {
        return Fail(_T("SVI2"), _T("precondition: wheel did not scroll the view"));
    }
    if (g_sviPaintCount <= 0)
    {
        return Fail(_T("SVI2"), _T("precondition: no WM_PAINT with a dirty region was seen"));
    }
    if (g_sviPaintUnion.top < view.top || g_sviPaintUnion.bottom > view.bottom)
    {
        CString d;
        d.Format(_T("%d paints, dirty union %s escapes viewport %s"), g_sviPaintCount,
                 (LPCTSTR)SviRectText(g_sviPaintUnion), (LPCTSTR)SviRectText(view));
        return Fail(_T("SVI2"), d);
    }
    return OK(_T("SVI2_WheelRepaintStaysInViewport"));
}

// SVI3：位于视口之外（但仍在宿主客户区内）的内容子控件失效时，不产生任何待重绘区域。
static Result Test_SVI3_InvisibleChildInvalidateIsNoop()
{
    SviFixture fx;
    if (!fx.Ready())
    {
        return Fail(_T("SVI3"), _T("cannot create host window"));
    }
    // 滚动位置为 0 时第 3 行在 (250, 350)：位于视口下边沿之下，与下方控件重叠
    SviPaintCounter* below = fx.rows[2];
    const RECT& view = fx.sv->GetRect();
    if (below->GetRect().top < view.bottom)
    {
        return Fail(_T("SVI3"), _T("precondition: row 2 is not below the viewport"));
    }
    fx.ClearDirty();
    below->Invalidate();

    RECT dirty;
    if (fx.Dirty(dirty))
    {
        return Fail(_T("SVI3"), _T("invisible row produced dirty ") + SviRectText(dirty));
    }
    return OK(_T("SVI3_InvisibleChildInvalidateIsNoop"));
}

// SVI4：两层滚动视口嵌套，内层内容的失效限制在两层视口的交集之内。
static Result Test_SVI4_NestedViewportsIntersect()
{
    SviTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("SVI4"), _T("cannot create top window"));
    }
    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, kSviHostW, kSviHostH);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);

    // 外层：上方一条 50 高的控件 + 外层视口 (50..250) + 下方一条 50 高的控件。
    // 外层内容：100 高留白 + 150 高的内层视口 + 足够高的留白；滚动位置 0 时内层视口
    // 落在 (150..300)，下半截伸出外层视口的下边沿。
    std::unique_ptr<DuiVBox> root(new DuiVBox());
    root->SetPadding(0);
    root->SetGap(0);
    root->AddChild(std::unique_ptr<DuiControl>(new SviPaintCounter()), DuiLayout::Hint().Fixed(kSviBandH));

    std::unique_ptr<DuiScrollView> outer(new DuiScrollView());
    DuiScrollView* outerRaw = outer.get();
    std::unique_ptr<DuiVBox> outerContent(new DuiVBox());
    outerContent->SetPadding(0);
    outerContent->SetGap(0);
    outerContent->AddChild(std::unique_ptr<DuiControl>(new SviPaintCounter()),
                           DuiLayout::Hint().Fixed(kSviInnerTopPad));

    std::unique_ptr<DuiScrollView> inner(new DuiScrollView());
    DuiScrollView* innerRaw = inner.get();
    std::vector<SviPaintCounter*> innerRows;
    std::unique_ptr<DuiVBox> innerContent = SviMakeRows(innerRows);
    DuiVBox* innerContentRaw = innerContent.get();
    inner->SetContent(std::move(innerContent));
    inner->SetContentHeight(kSviRows * kSviRowH);
    outerContent->AddChild(std::move(inner), DuiLayout::Hint().Fixed(kSviInnerH));
    outerContent->AddChild(std::unique_ptr<DuiControl>(new SviPaintCounter()),
                           DuiLayout::Hint().Fixed(kSviRows * kSviRowH));

    outer->SetContent(std::move(outerContent));
    outer->SetContentHeight(kSviInnerTopPad + kSviInnerH + kSviRows * kSviRowH);
    root->AddChild(std::move(outer), DuiLayout::Hint().Weight(1));
    root->AddChild(std::unique_ptr<DuiControl>(new SviPaintCounter()), DuiLayout::Hint().Fixed(kSviBandH));

    host.SetRoot(std::move(root));
    ::UpdateWindow(host.m_hWnd);

    const RECT outerRect = outerRaw->GetRect();
    const RECT innerRect = innerRaw->GetRect();
    RECT expected;
    ::IntersectRect(&expected, &outerRect, &innerRect);

    ::ValidateRect(host.m_hWnd, nullptr);
    innerContentRaw->Invalidate();
    RECT dirty;
    ::SetRect(&dirty, 0, 0, 0, 0);
    const bool any = ::GetUpdateRect(host.m_hWnd, &dirty, FALSE) != FALSE;
    host.DestroyWindow();

    // 前提：内层视口确实有一截伸出了外层视口，否则测不出"两层求交"
    if (innerRect.bottom <= outerRect.bottom)
    {
        return Fail(_T("SVI4"), _T("precondition: inner viewport does not overflow the outer one"));
    }
    if (!any || dirty.top < expected.top || dirty.bottom > expected.bottom)
    {
        return Fail(_T("SVI4"), _T("dirty ") + SviRectText(dirty) + _T(" escapes ")
                                + SviRectText(expected));
    }
    return OK(_T("SVI4_NestedViewportsIntersect"));
}

// SVI5：滚动视口自己的滚动条（宽度大于 0）失效时照样重画那一列，不能被裁掉。
static Result Test_SVI5_ScrollBarColumnStillRepaints()
{
    SviFixture fx;
    if (!fx.Ready())
    {
        return Fail(_T("SVI5"), _T("cannot create host window"));
    }
    DuiScrollBar* sb = fx.sv->GetScrollBar();
    const RECT sbRect = sb->GetRect();
    if (!sb->IsVisible() || ::IsRectEmpty(&sbRect))
    {
        return Fail(_T("SVI5"), _T("precondition: scroll bar is not visible"));
    }
    fx.ClearDirty();
    sb->Invalidate();

    RECT dirty;
    if (!fx.Dirty(dirty) || dirty.left > sbRect.left || dirty.right < sbRect.right
        || dirty.top > sbRect.top || dirty.bottom < sbRect.bottom)
    {
        return Fail(_T("SVI5"), _T("scroll bar ") + SviRectText(sbRect) + _T(" not fully dirty, got ")
                                + SviRectText(dirty));
    }
    return OK(_T("SVI5_ScrollBarColumnStillRepaints"));
}

// SVI6：普通布局容器不裁剪：子控件超出父容器的部分照样失效。
static Result Test_SVI6_PlainContainerDoesNotClip()
{
    SviFixture fx;
    if (!fx.Ready())
    {
        return Fail(_T("SVI6"), _T("cannot create host window"));
    }
    // 根是普通的 DuiVBox；把上方那条控件的矩形手工拉到 120 高（超出它在布局里
    // 分到的 50 高、也超出根之外的任何裁剪），它的失效必须原样上报。
    const int kTallBottom = 120;   // 拉高后的下边沿
    RECT tall = fx.header->GetRect();
    tall.bottom = kTallBottom;
    fx.header->SetRect(tall);
    fx.ClearDirty();
    fx.header->Invalidate();

    RECT dirty;
    if (!fx.Dirty(dirty) || dirty.bottom < kTallBottom)
    {
        return Fail(_T("SVI6"), _T("expected dirty down to 120, got ") + SviRectText(dirty));
    }
    return OK(_T("SVI6_PlainContainerDoesNotClip"));
}

// =====================================================================
// OV 组（2026-10-04 起）：悬浮式细滑块的外观与淡入淡出
// =====================================================================

// 测试用的命中带宽度与轨道长度（像素）
const int kOvBandW  = 11;
const int kOvTrackH = 200;
// 颜色比较的容差（每个分量）
const int kOvTol = 4;
// 白底上画半透明灰滑块后的期望灰度：255 - (255 - 0x5A) * 120 / 255 ≈ 177
const int kOvThumbGrayOnWhite = 177;
// 动画时间轴的起点（毫秒，任取）
const DWORD kOvT0 = 100000;

// 进程内启动 / 关闭 GDI+（滚动条用 GDI+ 画滑块）
class OvScopedGdiplus
{
public:
    OvScopedGdiplus() : m_token(0)
    {
        Gdiplus::GdiplusStartupInput input;
        m_ok = (Gdiplus::GdiplusStartup(&m_token, &input, NULL) == Gdiplus::Ok);
    }
    ~OvScopedGdiplus()
    {
        if (m_ok)
        {
            Gdiplus::GdiplusShutdown(m_token);
        }
    }
    bool Ok() const { return m_ok; }
private:
    ULONG_PTR m_token;   // GdiplusStartup 返回的令牌
    bool      m_ok;      // 是否启动成功
};

// 把滚动条画到白底内存位图上，读出若干点的灰度（取红色分量）。位图大小取滚动条矩形的右下角。
static bool OvPaintAndSample(DuiScrollBar& sb, const std::vector<POINT>& pts, std::vector<int>& out)
{
    const RECT rcItem = sb.GetRect();
    const int w = rcItem.right;
    const int h = rcItem.bottom;
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HDC dc = ::CreateCompatibleDC(NULL);
    HBITMAP bmp = ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (dc == NULL || bmp == NULL || bits == NULL)
    {
        return false;
    }
    HGDIOBJ old = ::SelectObject(dc, bmp);
    RECT rc = { 0, 0, w, h };
    ::FillRect(dc, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH));
    sb.OnPaint(dc, rc);
    const DWORD* px = static_cast<const DWORD*>(bits);
    out.clear();
    for (size_t i = 0; i < pts.size(); ++i)
    {
        out.push_back((int)((px[pts[i].y * w + pts[i].x] >> 16) & 0xFF));
    }
    ::SelectObject(dc, old);
    ::DeleteObject(bmp);
    ::DeleteDC(dc);
    return true;
}

// 一根竖直滚动条：命中带 11 宽、轨道 200 高、内容 1000（视口 200），滑块 0..40
static void OvSetup(DuiScrollBar& sb)
{
    sb.SetRect(RECT{ 0, 0, kOvBandW, kOvTrackH });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(0, /*notify=*/false);
}

static bool OvNear(float a, float b)
{
    const float d = a - b;
    return d < 0.01f && d > -0.01f;
}

// OV1：只在离右缘 2 像素、宽 5 像素的竖条里画半透明灰色；命中带其余部分不画；两端是圆角。
static Result Test_OV1_ThinOverlayThumb()
{
    OvScopedGdiplus gdiplus;
    if (!gdiplus.Ok())
    {
        return Fail(_T("OV1"), _T("GdiplusStartup failed"));
    }
    DuiScrollBar sb;
    OvSetup(sb);
    sb.SetAutoHide(false);   // alpha = 1，便于取色

    const RECT paint = sb.ComputePaintThumbRect();
    EXPECT_RECT(paint, kOvBandW - 2 - 5, 0, kOvBandW - 2, 40, _T("OV1/paintRect"));

    std::vector<POINT> pts;
    pts.push_back(POINT{ 6, 20 });     // 滑块中部
    pts.push_back(POINT{ 1, 20 });     // 命中带左侧（滑块左边）
    pts.push_back(POINT{ 10, 20 });    // 右边距内
    pts.push_back(POINT{ 6, 120 });    // 滑块下方的轨道
    pts.push_back(POINT{ 4, 0 });      // 滑块左上角（圆角之外）
    std::vector<int> g;
    if (!OvPaintAndSample(sb, pts, g))
    {
        return Fail(_T("OV1"), _T("cannot create DIB"));
    }
    CString d;
    d.Format(_T("center=%d left=%d margin=%d below=%d corner=%d"), g[0], g[1], g[2], g[3], g[4]);
    if (g[0] < kOvThumbGrayOnWhite - kOvTol || g[0] > kOvThumbGrayOnWhite + kOvTol)
    {
        return Fail(_T("OV1/thumbColor"), d);
    }
    if (g[1] != 255 || g[2] != 255 || g[3] != 255)
    {
        return Fail(_T("OV1/noTrack"), d);
    }
    if (g[4] <= g[0] + 20)
    {
        return Fail(_T("OV1/roundCorner"), d);
    }
    return OK(_T("OV1_ThinOverlayThumb"));
}

// OV2：滑块沿主轴最短 36 像素；水平滚动条的细滑块贴下缘。
static Result Test_OV2_MinThumbAndHorizontal()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, kOvBandW, kOvTrackH });
    sb.SetRange(0, 1000000);
    sb.SetPage(200);
    const RECT t = sb.ComputeThumbRect();
    EXPECT_INT(t.bottom - t.top, DuiScrollBar::kMinThumbPx, _T("OV2/minThumb"));

    DuiScrollBar hb(/*horizontal=*/true);
    hb.SetRect(RECT{ 0, 0, kOvTrackH, kOvBandW });
    hb.SetRange(0, 800);
    hb.SetPage(200);
    const RECT hp = hb.ComputePaintThumbRect();
    EXPECT_RECT(hp, 0, kOvBandW - 2 - 5, 40, kOvBandW - 2, _T("OV2/horizontalPaintRect"));
    return OK(_T("OV2_MinThumbAndHorizontal"));
}

// OV3：新建的滚动条默认自动隐藏，不透明度为 0。
static Result Test_OV3_DefaultAutoHide()
{
    DuiScrollBar sb;
    if (!sb.IsAutoHide() || !OvNear(sb.GetAlpha(), 0.0f))
    {
        return Fail(_T("OV3"), _T("expected auto-hide on and alpha 0"));
    }
    return OK(_T("OV3_DefaultAutoHide"));
}

// OV4：滚轮后淡入（200ms 到 1）；停 800ms 后开始淡出，再过 300ms 回到 0。
static Result Test_OV4_WheelFadeInThenIdleFadeOut()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();
    DuiScrollBar sb;
    OvSetup(sb);
    sb.SetLineSize(10);
    sb.OnMouseWheel(POINT{ 5, 5 }, -WHEEL_DELTA, 0);
    m.TickAll(kOvT0);                                   // 记下动画起点
    m.TickAll(kOvT0 + DuiScrollBar::kFadeInMs);         // 淡入结束
    if (!OvNear(sb.GetAlpha(), 1.0f))
    {
        return Fail(_T("OV4/fadeIn"), _T("alpha not 1 after fade-in"));
    }
    m.TickAll(kOvT0 + DuiScrollBar::kIdleHideMs - 1);   // 空闲计时还没到
    if (!OvNear(sb.GetAlpha(), 1.0f))
    {
        return Fail(_T("OV4/beforeIdle"), _T("faded before idle delay"));
    }
    m.TickAll(kOvT0 + DuiScrollBar::kIdleHideMs);       // 空闲计时到期，开始淡出
    m.TickAll(kOvT0 + DuiScrollBar::kIdleHideMs + 1);   // 记下淡出动画起点
    m.TickAll(kOvT0 + DuiScrollBar::kIdleHideMs + 1 + DuiScrollBar::kFadeOutMs);
    if (!OvNear(sb.GetAlpha(), 0.0f))
    {
        return Fail(_T("OV4/fadeOut"), _T("alpha not 0 after fade-out"));
    }
    m.Clear();
    return OK(_T("OV4_WheelFadeInThenIdleFadeOut"));
}

// OV5：鼠标悬停在滚动条上时，空闲计时到期也不淡出；移开后再过 800ms 才淡出。
static Result Test_OV5_HoverKeepsVisible()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();
    DuiScrollBar sb;
    OvSetup(sb);
    sb.OnMouseEnter();                                  // 进入命中带：淡入
    m.TickAll(kOvT0);
    m.TickAll(kOvT0 + DuiScrollBar::kFadeInMs);
    if (!OvNear(sb.GetAlpha(), 1.0f))
    {
        return Fail(_T("OV5/enterShows"), _T("alpha not 1 after mouse enter"));
    }
    // 悬停期间连续走过三轮空闲计时，仍然可见
    DWORD t = kOvT0 + DuiScrollBar::kFadeInMs;
    for (int round = 0; round < 3; ++round)
    {
        t += DuiScrollBar::kIdleHideMs + DuiScrollBar::kFadeOutMs;
        m.TickAll(t);
        m.TickAll(t + 1);
    }
    if (!OvNear(sb.GetAlpha(), 1.0f))
    {
        return Fail(_T("OV5/hoverKeeps"), _T("faded while hovered"));
    }
    // 移开：重新计时，到期后淡出
    sb.OnMouseLeave();
    t += 2;
    m.TickAll(t);
    m.TickAll(t + DuiScrollBar::kIdleHideMs - 1);
    if (!OvNear(sb.GetAlpha(), 1.0f))
    {
        return Fail(_T("OV5/leaveWaits"), _T("faded right after leave"));
    }
    m.TickAll(t + DuiScrollBar::kIdleHideMs);
    m.TickAll(t + DuiScrollBar::kIdleHideMs + 1);
    m.TickAll(t + DuiScrollBar::kIdleHideMs + 1 + DuiScrollBar::kFadeOutMs);
    if (!OvNear(sb.GetAlpha(), 0.0f))
    {
        return Fail(_T("OV5/leaveFades"), _T("alpha not 0 after leave + idle + fade-out"));
    }
    m.Clear();
    return OK(_T("OV5_HoverKeepsVisible"));
}

// OV6：拖动滑块期间不淡出（容器的鼠标离开也不行）；松开后重新计时再淡出。
static Result Test_OV6_DragKeepsVisible()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();
    DuiScrollBar sb;
    OvSetup(sb);
    sb.OnLButtonDown(POINT{ 6, 10 }, 0);                // 按在滑块上（滑块 0..40）
    if (!sb.IsDragging())
    {
        return Fail(_T("OV6"), _T("precondition: not dragging"));
    }
    m.TickAll(kOvT0);
    m.TickAll(kOvT0 + DuiScrollBar::kFadeInMs);
    sb.StartFadeOut();                                  // 容器的鼠标离开：拖动中应忽略
    DWORD t = kOvT0 + DuiScrollBar::kFadeInMs;
    for (int round = 0; round < 3; ++round)
    {
        t += DuiScrollBar::kIdleHideMs + DuiScrollBar::kFadeOutMs;
        m.TickAll(t);
        m.TickAll(t + 1);
    }
    if (!OvNear(sb.GetAlpha(), 1.0f))
    {
        return Fail(_T("OV6/dragKeeps"), _T("faded while dragging"));
    }
    sb.OnLButtonUp(POINT{ 6, 10 }, 0);                  // 松开：重新计时
    t += 2;
    m.TickAll(t);
    m.TickAll(t + DuiScrollBar::kIdleHideMs);
    m.TickAll(t + DuiScrollBar::kIdleHideMs + 1);
    m.TickAll(t + DuiScrollBar::kIdleHideMs + 1 + DuiScrollBar::kFadeOutMs);
    if (!OvNear(sb.GetAlpha(), 0.0f))
    {
        return Fail(_T("OV6/releaseFades"), _T("alpha not 0 after release + idle + fade-out"));
    }
    m.Clear();
    return OK(_T("OV6_DragKeepsVisible"));
}

// OV7：关闭自动隐藏后不透明度恒为 1，滚动与空闲计时都不改变它。
static Result Test_OV7_AutoHideOffAlwaysVisible()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();
    DuiScrollBar sb;
    OvSetup(sb);
    sb.SetAutoHide(false);
    sb.OnMouseWheel(POINT{ 5, 5 }, -WHEEL_DELTA, 0);
    sb.StartFadeOut();
    m.TickAll(kOvT0);
    m.TickAll(kOvT0 + DuiScrollBar::kIdleHideMs + DuiScrollBar::kFadeOutMs + 1);
    if (!OvNear(sb.GetAlpha(), 1.0f))
    {
        return Fail(_T("OV7"), _T("alpha changed with auto-hide off"));
    }
    m.Clear();
    return OK(_T("OV7_AutoHideOffAlwaysVisible"));
}


// =====================================================================
// SVO 组（2026-10-04 起）：DuiScrollView 的悬浮式滚动条
// =====================================================================

// 视口尺寸与内容高度（像素）
const int kSvoW = 200;
const int kSvoH = 100;
const int kSvoTallContent  = 500;   // 超出视口，出现滚动条
const int kSvoShortContent = 60;    // 放得下，不出现滚动条

// 铺满自身矩形、画纯红色的内容控件，用来检查滑块是否盖在内容之上
class SvoRedControl : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT& /*rcDirty*/) override
    {
        HBRUSH br = ::CreateSolidBrush(RGB(255, 0, 0));
        ::FillRect(hdc, &m_rcItem, br);
        ::DeleteObject(br);
    }
};

// 建一个视口：内容是红色控件，内容高 contentH，排在 (0,0)-(kSvoW,kSvoH)
static std::unique_ptr<DuiScrollView> SvoMake(int contentH, DuiControl** contentOut)
{
    std::unique_ptr<DuiScrollView> sv(new DuiScrollView());
    std::unique_ptr<DuiControl> content(new SvoRedControl());
    *contentOut = content.get();
    sv->SetContent(std::move(content));
    sv->SetContentHeight(contentH);
    sv->SetRect(RECT{ 0, 0, kSvoW, kSvoH });
    return sv;
}

// SVO1：出现滚动条时内容仍按视口全宽排版，滚动条（命中带）贴右缘、宽 11。
static Result Test_SVO1_ContentKeepsFullWidth()
{
    DuiControl* content = nullptr;
    std::unique_ptr<DuiScrollView> sv = SvoMake(kSvoTallContent, &content);
    DuiScrollBar* sb = sv->GetScrollBar();
    if (!sb->IsVisible())
    {
        return Fail(_T("SVO1"), _T("precondition: scroll bar not visible"));
    }
    const RECT rc = content->GetRect();
    EXPECT_INT(rc.right - rc.left, kSvoW, _T("SVO1/contentWidth"));
    EXPECT_RECT(sb->GetRect(), kSvoW - DuiScrollBar::kOverlayBandPx, 0, kSvoW, kSvoH, _T("SVO1/bandRect"));
    EXPECT_INT(sv->GetScrollBarWidth(), DuiScrollBar::kOverlayBandPx, _T("SVO1/defaultWidth"));
    return OK(_T("SVO1_ContentKeepsFullWidth"));
}

// SVO2：内容溢出时命中带里的点归滚动条（即使它还是透明的），带外归内容；不溢出时带里也归内容。
static Result Test_SVO2_BandHitPriority()
{
    DuiControl* content = nullptr;
    std::unique_ptr<DuiScrollView> sv = SvoMake(kSvoTallContent, &content);
    DuiScrollBar* sb = sv->GetScrollBar();
    const POINT inBand = { kSvoW - 3, kSvoH / 2 };
    const POINT outBand = { kSvoW / 2, kSvoH / 2 };
    if (sv->HitTest(inBand) != sb)
    {
        return Fail(_T("SVO2/inBand"), _T("band point did not hit the scroll bar"));
    }
    if (sv->HitTest(outBand) != content)
    {
        return Fail(_T("SVO2/outBand"), _T("content point did not hit the content"));
    }
    DuiControl* shortContent = nullptr;
    std::unique_ptr<DuiScrollView> sv2 = SvoMake(kSvoShortContent, &shortContent);
    if (sv2->HitTest(inBand) != shortContent)
    {
        return Fail(_T("SVO2/noOverflow"), _T("band point should hit the content when nothing overflows"));
    }
    return OK(_T("SVO2_BandHitPriority"));
}

// SVO3：滑块画在内容之上：滑块处是红底上叠的灰色，命中带其余部分仍是红色。
static Result Test_SVO3_ThumbPaintsOverContent()
{
    OvScopedGdiplus gdiplus;
    if (!gdiplus.Ok())
    {
        return Fail(_T("SVO3"), _T("GdiplusStartup failed"));
    }
    DuiControl* content = nullptr;
    std::unique_ptr<DuiScrollView> sv = SvoMake(kSvoTallContent, &content);
    sv->GetScrollBar()->SetAutoHide(false);   // 不透明度 1，便于取色
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = kSvoW;
    bi.bmiHeader.biHeight = -kSvoH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HDC dc = ::CreateCompatibleDC(NULL);
    HBITMAP bmp = ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (dc == NULL || bmp == NULL || bits == NULL)
    {
        return Fail(_T("SVO3"), _T("cannot create DIB"));
    }
    HGDIOBJ old = ::SelectObject(dc, bmp);
    RECT rc = { 0, 0, kSvoW, kSvoH };
    sv->OnPaint(dc, rc);
    const DWORD* px = static_cast<const DWORD*>(bits);
    const RECT thumb = sv->GetScrollBar()->ComputePaintThumbRect();
    const int tx = (thumb.left + thumb.right) / 2;
    const int ty = (thumb.top + thumb.bottom) / 2;
    const DWORD onThumb = px[ty * kSvoW + tx];
    const DWORD bandLeft = px[ty * kSvoW + (kSvoW - DuiScrollBar::kOverlayBandPx + 1)];
    ::SelectObject(dc, old);
    ::DeleteObject(bmp);
    ::DeleteDC(dc);
    const int thumbR = (int)((onThumb >> 16) & 0xFF);
    const int thumbG = (int)((onThumb >> 8) & 0xFF);
    CString d;
    d.Format(_T("thumb=0x%06X bandLeft=0x%06X"), onThumb & 0xFFFFFF, bandLeft & 0xFFFFFF);
    // 红底叠半透明灰：红色变暗、绿色从 0 升上来
    if (thumbR > 230 || thumbG < 20)
    {
        return Fail(_T("SVO3/thumbOverContent"), d);
    }
    if ((bandLeft & 0xFFFFFF) != 0xFF0000)
    {
        return Fail(_T("SVO3/noTrack"), d);
    }
    return OK(_T("SVO3_ThumbPaintsOverContent"));
}

// SVO4：宽度设为 0 时不出现滚动条，命中带位置的点归内容。
static Result Test_SVO4_ZeroWidthMeansNoBar()
{
    DuiControl* content = nullptr;
    std::unique_ptr<DuiScrollView> sv = SvoMake(kSvoTallContent, &content);
    sv->SetScrollBarWidth(0);
    if (sv->GetScrollBar()->IsVisible())
    {
        return Fail(_T("SVO4/hidden"), _T("scroll bar visible with width 0"));
    }
    const POINT edge = { kSvoW - 3, kSvoH / 2 };
    if (sv->HitTest(edge) != content)
    {
        return Fail(_T("SVO4/hit"), _T("edge point should hit the content"));
    }
    const RECT rc = content->GetRect();
    EXPECT_INT(rc.right - rc.left, kSvoW, _T("SVO4/contentWidth"));
    return OK(_T("SVO4_ZeroWidthMeansNoBar"));
}


#undef EXPECT_INT
#undef EXPECT_RECT


} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults_NoCrash"),     &Test_Defaults_NoCrash     },
        { _T("PosClamp"),             &Test_PosClamp             },
        { _T("RangeShrinkClamps"),    &Test_RangeShrinkClamps    },
        { _T("ThumbAtTop"),           &Test_ThumbAtTop           },
        { _T("ThumbAtBottom"),        &Test_ThumbAtBottom        },
        { _T("ThumbMid"),             &Test_ThumbMid             },
        { _T("ThumbMinimumSize"),     &Test_ThumbMinimumSize     },
        { _T("TrackClickJumpsDown"),  &Test_TrackClickJumpsDown  },
        { _T("TrackClickJumpsUp"),    &Test_TrackClickJumpsUp    },
        { _T("DragThumb"),            &Test_DragThumb            },
        { _T("MouseWheel"),           &Test_MouseWheel           },
        { _T("OnScrollCallback"),     &Test_OnScrollCallback     },
        { _T("EmptyRange"),           &Test_EmptyRange           },
        { _T("WheelEmptyRangeNotConsumed"), &Test_WheelEmptyRangeNotConsumed },
        { _T("WheelAtEdgeStillConsumed"),   &Test_WheelAtEdgeStillConsumed   },
        { _T("ScrollViewWheelPassThroughWhenContentFits"),
                                      &Test_ScrollViewWheelPassThroughWhenContentFits },
        { _T("Horizontal"),           &Test_Horizontal           },
        { _T("SVI1_ScrolledContentInvalidateStaysInViewport"),
                                      &Test_SVI1_ScrolledContentInvalidateStaysInViewport },
        { _T("SVI2_WheelRepaintStaysInViewport"),
                                      &Test_SVI2_WheelRepaintStaysInViewport },
        { _T("SVI3_InvisibleChildInvalidateIsNoop"),
                                      &Test_SVI3_InvisibleChildInvalidateIsNoop },
        { _T("SVI4_NestedViewportsIntersect"),
                                      &Test_SVI4_NestedViewportsIntersect },
        { _T("SVI5_ScrollBarColumnStillRepaints"),
                                      &Test_SVI5_ScrollBarColumnStillRepaints },
        { _T("SVI6_PlainContainerDoesNotClip"),
                                      &Test_SVI6_PlainContainerDoesNotClip },
        { _T("SVW1_OneNotchStillScrollsThreeLines"),
                                      &Test_SVW1_OneNotchStillScrollsThreeLines },
        { _T("SVW2_SmallDeltasAddUpToOneNotch"),
                                      &Test_SVW2_SmallDeltasAddUpToOneNotch },
        { _T("SVW3_DirectionChangeDropsRemainder"),
                                      &Test_SVW3_DirectionChangeDropsRemainder },
        { _T("SVW4_TinyDeltasAccumulate"),
                                      &Test_SVW4_TinyDeltasAccumulate },
        { _T("OV1_ThinOverlayThumb"),          &Test_OV1_ThinOverlayThumb          },
        { _T("OV2_MinThumbAndHorizontal"),     &Test_OV2_MinThumbAndHorizontal     },
        { _T("OV3_DefaultAutoHide"),           &Test_OV3_DefaultAutoHide           },
        { _T("OV4_WheelFadeInThenIdleFadeOut"), &Test_OV4_WheelFadeInThenIdleFadeOut },
        { _T("OV5_HoverKeepsVisible"),         &Test_OV5_HoverKeepsVisible         },
        { _T("OV6_DragKeepsVisible"),          &Test_OV6_DragKeepsVisible          },
        { _T("OV7_AutoHideOffAlwaysVisible"),  &Test_OV7_AutoHideOffAlwaysVisible  },
        { _T("SVO1_ContentKeepsFullWidth"),    &Test_SVO1_ContentKeepsFullWidth    },
        { _T("SVO2_BandHitPriority"),          &Test_SVO2_BandHitPriority          },
        { _T("SVO3_ThumbPaintsOverContent"),   &Test_SVO3_ThumbPaintsOverContent   },
        { _T("SVO4_ZeroWidthMeansNoBar"),      &Test_SVO4_ZeroWidthMeansNoBar      }
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
    summary.Format(_T("[summary] DuiScrollBarTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiScrollBarTests

} // namespace balloonwjui

#endif // BUI_FEATURE_SCROLLBAR
