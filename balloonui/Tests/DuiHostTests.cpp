/**
 *  Unit tests for DuiHost::DispatchMouseWheel — the wheel-bubbling rule.
 *  2026-10-14 起另含控件通知投递目标的用例（DUI-N1、DUI-N2）：这两条会建真实窗口。
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"
#include "DuiHostTests.h"

#if BUI_FEATURE_SCROLLBAR
#include "../Controls/Window/DuiScrollBar.h"
#endif

namespace balloonwjui {

namespace DuiHostTests {

namespace {

// Wheel delta of a single detent, in the sign the OS uses for "wheel up".
static const short kWheelUp   =  WHEEL_DELTA;
static const short kWheelDown = -WHEEL_DELTA;

// Modifier bits handed to DispatchMouseWheel; any value works, the tests
// only check that it arrives unchanged at every level.
static const UINT kTestMkFlags = MK_CONTROL;

// Global call counter: each spy stamps its own m_order from it, so the tests
// can assert the chain was walked child-first.
static int s_seq = 0;

// Stub control that records every wheel event it is handed and reports
// "handled" / "not handled" as configured.
class WheelSpy : public DuiControl
{
public:
    WheelSpy()
        : m_handle(false), m_hits(0), m_lastDelta(0), m_lastFlags(0), m_order(0)
    {
        m_lastPt.x = 0;
        m_lastPt.y = 0;
    }

    void OnPaint(HDC, const RECT&) override {}

    bool OnMouseWheel(POINT pt, short zDelta, UINT mkFlags) override
    {
        ++m_hits;
        m_lastPt    = pt;
        m_lastDelta = zDelta;
        m_lastFlags = mkFlags;
        m_order     = ++s_seq;
        return m_handle;
    }

public:
    bool  m_handle;      // what OnMouseWheel returns
    int   m_hits;        // how many wheel events reached this control
    POINT m_lastPt;      // last point received
    short m_lastDelta;   // last wheel delta received
    UINT  m_lastFlags;   // last modifier bits received
    int   m_order;       // value of s_seq at the last hit (0 = never hit)
};

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
    do { int _a = (int)(actual); int _e = (int)(expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)

#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) { return Fail(name, _T("condition false")); } } while (0)

// A control the mouse is over that does not scroll must not swallow the
// event: the first ancestor willing to handle it gets it. This is the whole
// point of the bubbling rule — a scroll container packed with clickable
// children used to be un-scrollable everywhere a child was hit.
static Result Test_BubblesToAncestor()
{
    WheelSpy parent;
    parent.m_handle = true;
    WheelSpy* child = new WheelSpy();
    child->m_handle = false;
    parent.AddChild(std::unique_ptr<DuiControl>(child));

    POINT pt = { 40, 60 };
    bool handled = DuiHost::DispatchMouseWheel(child, pt, kWheelDown, kTestMkFlags);

    EXPECT_TRUE(handled, _T("BubblesToAncestor/handled"));
    EXPECT_INT(child->m_hits, 1, _T("BubblesToAncestor/childHits"));
    EXPECT_INT(parent.m_hits, 1, _T("BubblesToAncestor/parentHits"));
    // Every argument is forwarded untouched.
    EXPECT_INT(parent.m_lastPt.x, pt.x, _T("BubblesToAncestor/ptX"));
    EXPECT_INT(parent.m_lastPt.y, pt.y, _T("BubblesToAncestor/ptY"));
    EXPECT_INT(parent.m_lastDelta, kWheelDown, _T("BubblesToAncestor/delta"));
    EXPECT_INT(parent.m_lastFlags, kTestMkFlags, _T("BubblesToAncestor/flags"));
    // Child is asked before its parent.
    EXPECT_TRUE(child->m_order < parent.m_order, _T("BubblesToAncestor/order"));
    return OK(_T("BubblesToAncestor"));
}

// The walk does not stop at the immediate parent: it keeps going until some
// ancestor handles the event.
static Result Test_BubblesPastMiddleLevels()
{
    WheelSpy root;
    root.m_handle = true;
    WheelSpy* mid = new WheelSpy();
    mid->m_handle = false;
    WheelSpy* leaf = new WheelSpy();
    leaf->m_handle = false;
    root.AddChild(std::unique_ptr<DuiControl>(mid));
    mid->AddChild(std::unique_ptr<DuiControl>(leaf));

    POINT pt = { 1, 2 };
    bool handled = DuiHost::DispatchMouseWheel(leaf, pt, kWheelUp, 0);

    EXPECT_TRUE(handled, _T("BubblesPastMiddleLevels/handled"));
    EXPECT_INT(leaf->m_hits, 1, _T("BubblesPastMiddleLevels/leafHits"));
    EXPECT_INT(mid->m_hits, 1, _T("BubblesPastMiddleLevels/midHits"));
    EXPECT_INT(root.m_hits, 1, _T("BubblesPastMiddleLevels/rootHits"));
    EXPECT_TRUE(leaf->m_order < mid->m_order, _T("BubblesPastMiddleLevels/order1"));
    EXPECT_TRUE(mid->m_order < root.m_order, _T("BubblesPastMiddleLevels/order2"));
    return OK(_T("BubblesPastMiddleLevels"));
}

// When the hit control handles the event itself, no ancestor is disturbed —
// otherwise an inner list and its outer page would both scroll at once.
static Result Test_HandledHitDoesNotBubble()
{
    WheelSpy parent;
    parent.m_handle = true;
    WheelSpy* child = new WheelSpy();
    child->m_handle = true;
    parent.AddChild(std::unique_ptr<DuiControl>(child));

    POINT pt = { 0, 0 };
    bool handled = DuiHost::DispatchMouseWheel(child, pt, kWheelDown, 0);

    EXPECT_TRUE(handled, _T("HandledHitDoesNotBubble/handled"));
    EXPECT_INT(child->m_hits, 1, _T("HandledHitDoesNotBubble/childHits"));
    EXPECT_INT(parent.m_hits, 0, _T("HandledHitDoesNotBubble/parentHits"));
    return OK(_T("HandledHitDoesNotBubble"));
}

// Nobody on the chain handles it: every level is still asked exactly once,
// the walk stops at the root instead of running off the end, and the caller
// is told to pass the message on to DefWindowProc.
static Result Test_NoHandlerReachesRootAndReturnsFalse()
{
    WheelSpy root;
    root.m_handle = false;
    WheelSpy* mid = new WheelSpy();
    mid->m_handle = false;
    WheelSpy* leaf = new WheelSpy();
    leaf->m_handle = false;
    root.AddChild(std::unique_ptr<DuiControl>(mid));
    mid->AddChild(std::unique_ptr<DuiControl>(leaf));

    POINT pt = { 7, 8 };
    bool handled = DuiHost::DispatchMouseWheel(leaf, pt, kWheelDown, 0);

    EXPECT_TRUE(!handled, _T("NoHandler/handled"));
    EXPECT_INT(leaf->m_hits, 1, _T("NoHandler/leafHits"));
    EXPECT_INT(mid->m_hits, 1, _T("NoHandler/midHits"));
    EXPECT_INT(root.m_hits, 1, _T("NoHandler/rootHits"));
    return OK(_T("NoHandlerReachesRootAndReturnsFalse"));
}

// Mouse over no control at all (empty host, or a point outside the root):
// hit-testing yields nullptr and dispatching must simply report "unhandled".
static Result Test_NullHitIsSafe()
{
    POINT pt = { 0, 0 };
    bool handled = DuiHost::DispatchMouseWheel(nullptr, pt, kWheelDown, 0);
    EXPECT_TRUE(!handled, _T("NullHitIsSafe/handled"));
    return OK(_T("NullHitIsSafe"));
}

#if BUI_FEATURE_SCROLLBAR

// Builds "outer spy -> DuiScrollView -> inner content spy" with the content
// twice as tall as the viewport, so the scroll view really can scroll.
// Ownership: the returned raw pointers stay owned by `outer`.
static void BuildScrollViewTree(WheelSpy& outer, DuiScrollView*& sv, WheelSpy*& content)
{
    // Viewport 200x100 with 400px of content: two viewports' worth.
    static const int kViewW      = 200;
    static const int kViewH      = 100;
    static const int kContentH   = 400;

    sv = new DuiScrollView();
    content = new WheelSpy();
    content->m_handle = false;

    RECT rcView = { 0, 0, kViewW, kViewH };
    sv->SetRect(rcView);
    sv->SetContent(std::unique_ptr<DuiControl>(content));
    sv->SetContentHeight(kContentH);

    outer.m_handle = true;
    outer.AddChild(std::unique_ptr<DuiControl>(sv));
}

// The bug this whole change is about, reproduced against the real
// DuiScrollView: the wheel lands on a child of the viewport, the child does
// not scroll, and the viewport must still scroll.
static Result Test_ScrollViewScrollsWhenChildIsHit()
{
    WheelSpy outer;
    DuiScrollView* sv = nullptr;
    WheelSpy* content = nullptr;
    BuildScrollViewTree(outer, sv, content);

    EXPECT_TRUE(sv->GetScrollBar() != nullptr, _T("ScrollViewChild/hasBar"));
    EXPECT_TRUE(sv->GetScrollBar()->GetMax() > 0, _T("ScrollViewChild/hasRange"));
    EXPECT_INT(sv->GetScrollPos(), 0, _T("ScrollViewChild/startsAtTop"));

    POINT pt = { 10, 10 };
    bool handled = DuiHost::DispatchMouseWheel(content, pt, kWheelDown, 0);

    EXPECT_TRUE(handled, _T("ScrollViewChild/handled"));
    EXPECT_INT(content->m_hits, 1, _T("ScrollViewChild/contentHits"));
    EXPECT_TRUE(sv->GetScrollPos() > 0, _T("ScrollViewChild/scrolled"));
    EXPECT_INT(outer.m_hits, 0, _T("ScrollViewChild/outerUntouched"));
    return OK(_T("ScrollViewScrollsWhenChildIsHit"));
}

// Boundary semantics, pinned deliberately: a scroll view that is already at
// the top still reports the wheel as handled, so the event does NOT continue
// up to an outer container. See the long comment on
// DuiHost::DispatchMouseWheel for why "consume at the edge" was chosen over
// "chain to the outer container".
static Result Test_ScrollViewAtTopStillConsumes()
{
    WheelSpy outer;
    DuiScrollView* sv = nullptr;
    WheelSpy* content = nullptr;
    BuildScrollViewTree(outer, sv, content);

    EXPECT_INT(sv->GetScrollPos(), 0, _T("ScrollViewTop/startsAtTop"));

    POINT pt = { 10, 10 };
    bool handled = DuiHost::DispatchMouseWheel(content, pt, kWheelUp, 0);

    EXPECT_TRUE(handled, _T("ScrollViewTop/handled"));
    EXPECT_INT(sv->GetScrollPos(), 0, _T("ScrollViewTop/stillAtTop"));
    EXPECT_INT(outer.m_hits, 0, _T("ScrollViewTop/outerUntouched"));
    return OK(_T("ScrollViewAtTopStillConsumes"));
}

// Same rule at the far end: scrolled all the way down, further wheel-down
// events stay inside the scroll view.
static Result Test_ScrollViewAtBottomStillConsumes()
{
    WheelSpy outer;
    DuiScrollView* sv = nullptr;
    WheelSpy* content = nullptr;
    BuildScrollViewTree(outer, sv, content);

    const int bottom = sv->GetScrollBar()->GetMax();
    sv->SetScrollPos(bottom);
    EXPECT_INT(sv->GetScrollPos(), bottom, _T("ScrollViewBottom/atBottom"));

    POINT pt = { 10, 10 };
    bool handled = DuiHost::DispatchMouseWheel(content, pt, kWheelDown, 0);

    EXPECT_TRUE(handled, _T("ScrollViewBottom/handled"));
    EXPECT_INT(sv->GetScrollPos(), bottom, _T("ScrollViewBottom/stillAtBottom"));
    EXPECT_INT(outer.m_hits, 0, _T("ScrollViewBottom/outerUntouched"));
    return OK(_T("ScrollViewAtBottomStillConsumes"));
}

#endif // BUI_FEATURE_SCROLLBAR

// ----- 控件通知的投递目标（DUI-N1、DUI-N2，2026-10-14） -----
//
// SendNotify 只在宿主带 WS_CHILD 时把 WM_DUI_NOTIFY 发给父窗口，其余（顶层窗口、有所有者的 WS_POPUP 窗口）一律发回
// 宿主自身。此前按 GetParent 判断，而 Windows 对有所有者的 WS_POPUP 窗口，GetParent 返回的是所有者，于是这类弹窗的
// 按钮通知都被发到所有者，弹窗自己一条也收不到。

// 用例里控件的 id（避开 DuiFrameWindow 标题栏保留的 1 / 2 / 3）
static const UINT kRouteCtrlId = 77;

// 一个窗口收到的 WM_DUI_NOTIFY 记录
struct NotifyLog
{
    int  m_count;      // 收到的次数
    UINT m_lastCode;   // 最近一次的通知码
    UINT m_lastId;     // 最近一次的控件 id
};

// 记录 WM_DUI_NOTIFY 的窗口过程：记录写进 GWLP_USERDATA 指向的 NotifyLog（不拥有）
static LRESULT CALLBACK NotifyLogWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DUI_NOTIFY)
    {
        NotifyLog* log = reinterpret_cast<NotifyLog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
        const DuiNotify* n = reinterpret_cast<const DuiNotify*>(lParam);
        if (log != nullptr && n != nullptr)
        {
            ++log->m_count;
            log->m_lastCode = n->code;
            log->m_lastId = n->ctrlId;
        }
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

// 建一个不显示的重叠窗口，充当所有者或父窗口，收到的通知记进 log（不拥有，须比窗口活得久）
static HWND CreateNotifyLogWindow(NotifyLog* log)
{
    static const TCHAR kClass[] = _T("BuiHostTestsNotifyLog");
    WNDCLASS wc = {};
    wc.lpfnWndProc = &NotifyLogWndProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.lpszClassName = kClass;
    //第二次注册会失败，类已存在，不影响建窗
    ::RegisterClass(&wc);
    HWND hWnd = ::CreateWindowEx(0, kClass, _T(""), WS_OVERLAPPEDWINDOW, 0, 0, 200, 200,
                                 nullptr, nullptr, wc.hInstance, nullptr);
    if (hWnd != nullptr)
    {
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(log));
    }
    return hWnd;
}

// 记录自己收到的 WM_DUI_NOTIFY 的宿主
class NotifyLogHost : public DuiHost
{
public:
    NotifyLogHost()
    {
        m_log.m_count = 0;
        m_log.m_lastCode = 0;
        m_log.m_lastId = 0;
    }

    BEGIN_MSG_MAP(NotifyLogHost)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnNotify)
        CHAIN_MSG_MAP(DuiHost)
    END_MSG_MAP()

    LRESULT OnNotify(UINT, WPARAM, LPARAM lParam, BOOL&)
    {
        const DuiNotify* n = reinterpret_cast<const DuiNotify*>(lParam);
        if (n != nullptr)
        {
            ++m_log.m_count;
            m_log.m_lastCode = n->code;
            m_log.m_lastId = n->ctrlId;
        }
        return 0;
    }

    NotifyLog m_log;   // 本宿主收到的通知
};

// DUI-N1：有所有者的 WS_POPUP 宿主，控件通知发回自身，不发给所有者；没有所有者时同样发回自身
static Result Test_OwnedPopupNotifiesItself()
{
    NotifyLog ownerLog = {};
    HWND owner = CreateNotifyLogWindow(&ownerLog);
    EXPECT_TRUE(owner != nullptr, _T("OwnedPopup/ownerCreated"));
    DuiControl ctrl;
    ctrl.SetCtrlId(kRouteCtrlId);

    NotifyLogHost owned;
    RECT rc = { 0, 0, 40, 40 };
    owned.Create(owner, rc, nullptr, WS_POPUP, WS_EX_TOOLWINDOW);
    const bool ownedCreated = owned.m_hWnd != nullptr;
    const bool ownerLinked = ownedCreated && ::GetWindow(owned.m_hWnd, GW_OWNER) == owner;
    if (ownedCreated)
    {
        owned.SendNotify(&ctrl, DUIN_CLICK);
        owned.DestroyWindow();
    }

    NotifyLogHost ownerless;
    ownerless.Create(nullptr, rc, nullptr, WS_POPUP, WS_EX_TOOLWINDOW);
    const bool ownerlessCreated = ownerless.m_hWnd != nullptr;
    if (ownerlessCreated)
    {
        ownerless.SendNotify(&ctrl, DUIN_CLICK);
        ownerless.DestroyWindow();
    }
    ::DestroyWindow(owner);

    EXPECT_TRUE(ownedCreated, _T("OwnedPopup/created"));
    EXPECT_TRUE(ownerLinked, _T("OwnedPopup/hasOwner"));
    EXPECT_INT(owned.m_log.m_count, 1, _T("OwnedPopup/selfCount"));
    EXPECT_INT(owned.m_log.m_lastCode, DUIN_CLICK, _T("OwnedPopup/selfCode"));
    EXPECT_INT(owned.m_log.m_lastId, kRouteCtrlId, _T("OwnedPopup/selfId"));
    EXPECT_INT(ownerLog.m_count, 0, _T("OwnedPopup/ownerUntouched"));
    EXPECT_TRUE(ownerlessCreated, _T("OwnedPopup/ownerlessCreated"));
    EXPECT_INT(ownerless.m_log.m_count, 1, _T("OwnedPopup/ownerlessSelfCount"));
    return OK(_T("OwnedPopupNotifiesItself"));
}

// DUI-N2：WS_CHILD 宿主（嵌在别的窗口里），控件通知仍发给父窗口
static Result Test_ChildHostNotifiesParent()
{
    NotifyLog parentLog = {};
    HWND parent = CreateNotifyLogWindow(&parentLog);
    EXPECT_TRUE(parent != nullptr, _T("ChildHost/parentCreated"));
    DuiControl ctrl;
    ctrl.SetCtrlId(kRouteCtrlId);

    NotifyLogHost child;
    RECT rc = { 0, 0, 40, 40 };
    child.Create(parent, rc, nullptr, WS_CHILD, 0);
    const bool created = child.m_hWnd != nullptr;
    if (created)
    {
        child.SendNotify(&ctrl, DUIN_VALUECHANGED, 5);
        child.DestroyWindow();
    }
    ::DestroyWindow(parent);

    EXPECT_TRUE(created, _T("ChildHost/created"));
    EXPECT_INT(parentLog.m_count, 1, _T("ChildHost/parentCount"));
    EXPECT_INT(parentLog.m_lastCode, DUIN_VALUECHANGED, _T("ChildHost/parentCode"));
    EXPECT_INT(parentLog.m_lastId, kRouteCtrlId, _T("ChildHost/parentId"));
    EXPECT_INT(child.m_log.m_count, 0, _T("ChildHost/selfUntouched"));
    return OK(_T("ChildHostNotifiesParent"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("BubblesToAncestor"),          &Test_BubblesToAncestor          },
        { _T("BubblesPastMiddleLevels"),    &Test_BubblesPastMiddleLevels    },
        { _T("HandledHitDoesNotBubble"),    &Test_HandledHitDoesNotBubble    },
        { _T("NoHandlerReachesRoot"),       &Test_NoHandlerReachesRootAndReturnsFalse },
        { _T("NullHitIsSafe"),              &Test_NullHitIsSafe              },
        { _T("OwnedPopupNotifiesItself"),   &Test_OwnedPopupNotifiesItself   },
        { _T("ChildHostNotifiesParent"),    &Test_ChildHostNotifiesParent    }
#if BUI_FEATURE_SCROLLBAR
      , { _T("ScrollViewScrollsWhenChildIsHit"), &Test_ScrollViewScrollsWhenChildIsHit },
        { _T("ScrollViewAtTopStillConsumes"),     &Test_ScrollViewAtTopStillConsumes    },
        { _T("ScrollViewAtBottomStillConsumes"),  &Test_ScrollViewAtBottomStillConsumes }
#endif
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
    summary.Format(_T("[summary] DuiHostTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiHostTests

} // namespace balloonwjui
