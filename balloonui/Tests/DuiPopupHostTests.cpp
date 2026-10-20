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

// 浮层窗口用的是本类声明的窗口类（2026-10-05）：类名为 __DuiPopupHost__，
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

// ----- 浮层内容的控件通知（DUI-N3、DUI-N4，2026-10-14） -----
//
// DuiHost::SendNotify 自 2026-10-14 起只在宿主带 WS_CHILD 时发给父窗口，其余发回宿主自身；浮层是有所有者的 WS_POPUP
// 窗口，通知先到浮层自己。DuiPopupHost 把自己（含子类）没处理的通知转给所有者，「浮层内容的通知发给所有者」的约定
// 因此不变；子类自己处理了的通知不再转。

// 用例里控件的 id、浮层边长（像素）、DUIN_VALUECHANGED 带的值
static const UINT kNotifyCtrlId = 88;
static const int  kNotifyProbeSize = 40;
static const int  kNotifyValue = 55;

// 一个窗口收到的 WM_DUI_NOTIFY 记录
struct NotifyLog
{
    int    m_count;      // 收到的次数
    UINT   m_lastCode;   // 最近一次的通知码
    UINT   m_lastId;     // 最近一次的控件 id
    LPARAM m_lastExtra;  // 最近一次的附加值
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
            log->m_lastExtra = n->extra;
        }
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

// 建一个不显示的重叠窗口充当浮层的所有者，收到的通知记进 log（不拥有，须比窗口活得久）
static HWND CreateNotifyLogOwner(NotifyLog* log)
{
    static const TCHAR kClass[] = _T("BuiPopupTestsNotifyLog");
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

// 自己处理 DUIN_CLICK、其余通知交给基类的浮层子类（与客户端里 DuiPopupHost 子类的写法相同）
class ClickHandlingPopup : public DuiPopupHost
{
public:
    ClickHandlingPopup()
        : m_clicks(0)
    {
    }

    BEGIN_MSG_MAP(ClickHandlingPopup)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnNotify)
        CHAIN_MSG_MAP(DuiPopupHost)
    END_MSG_MAP()

    LRESULT OnNotify(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
    {
        const DuiNotify* n = reinterpret_cast<const DuiNotify*>(lParam);
        if (n != nullptr && n->code == static_cast<UINT>(DUIN_CLICK))
        {
            ++m_clicks;
            return 0;
        }
        bHandled = FALSE;
        return 0;
    }

    int m_clicks;   // 自己处理了的点击次数
};

// DUI-N3：浮层（直接用 DuiPopupHost，不派生子类）内容的通知转给所有者，通知码、控件 id 与附加值原样带过去
static Result Test_UnhandledNotifyGoesToOwner()
{
    NotifyLog ownerLog = {};
    HWND owner = CreateNotifyLogOwner(&ownerLog);
    EXPECT_TRUE(owner != nullptr, _T("N3/ownerCreated"));
    const RECT anchor = { 0, 0, kNotifyProbeSize, kNotifyProbeSize };
    DuiPopupHost p;
    std::unique_ptr<DuiControl> content(new DuiControl());
    DuiControl* raw = content.get();
    raw->SetCtrlId(kNotifyCtrlId);
    p.SetContent(std::move(content));
    p.SetSize(kNotifyProbeSize, kNotifyProbeSize);
    p.Show(anchor, owner);
    const bool created = p.m_hWnd != nullptr;
    const bool ownerLinked = created && ::GetWindow(p.m_hWnd, GW_OWNER) == owner;
    if (created)
    {
        p.SendNotify(raw, DUIN_VALUECHANGED, kNotifyValue);
        p.Hide();
        p.DestroyWindow();
    }
    ::DestroyWindow(owner);

    EXPECT_TRUE(created, _T("N3/created"));
    EXPECT_TRUE(ownerLinked, _T("N3/hasOwner"));
    EXPECT_INT(ownerLog.m_count, 1, _T("N3/ownerCount"));
    EXPECT_INT((int)ownerLog.m_lastCode, (int)DUIN_VALUECHANGED, _T("N3/ownerCode"));
    EXPECT_INT((int)ownerLog.m_lastId, (int)kNotifyCtrlId, _T("N3/ownerId"));
    EXPECT_INT((int)ownerLog.m_lastExtra, kNotifyValue, _T("N3/ownerExtra"));
    return OK(_T("UnhandledNotifyGoesToOwner"));
}

// DUI-N4：浮层子类自己处理了的通知留在浮层，不再转给所有者；没处理的照样转
static Result Test_HandledNotifyStaysInPopup()
{
    NotifyLog ownerLog = {};
    HWND owner = CreateNotifyLogOwner(&ownerLog);
    EXPECT_TRUE(owner != nullptr, _T("N4/ownerCreated"));
    const RECT anchor = { 0, 0, kNotifyProbeSize, kNotifyProbeSize };
    ClickHandlingPopup p;
    std::unique_ptr<DuiControl> content(new DuiControl());
    DuiControl* raw = content.get();
    raw->SetCtrlId(kNotifyCtrlId);
    p.SetContent(std::move(content));
    p.SetSize(kNotifyProbeSize, kNotifyProbeSize);
    p.Show(anchor, owner);
    const bool created = p.m_hWnd != nullptr;
    int ownerAfterClick = -1;
    if (created)
    {
        p.SendNotify(raw, DUIN_CLICK);
        ownerAfterClick = ownerLog.m_count;
        p.SendNotify(raw, DUIN_VALUECHANGED, kNotifyValue);
        p.Hide();
        p.DestroyWindow();
    }
    ::DestroyWindow(owner);

    EXPECT_TRUE(created, _T("N4/created"));
    EXPECT_INT(p.m_clicks, 1, _T("N4/popupHandledClick"));
    EXPECT_INT(ownerAfterClick, 0, _T("N4/ownerNoClick"));
    EXPECT_INT(ownerLog.m_count, 1, _T("N4/ownerGotUnhandled"));
    EXPECT_INT((int)ownerLog.m_lastCode, (int)DUIN_VALUECHANGED, _T("N4/ownerCode"));
    return OK(_T("HandledNotifyStaysInPopup"));
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
        { _T("UnhandledNotifyGoesToOwner"), &Test_UnhandledNotifyGoesToOwner },
        { _T("HandledNotifyStaysInPopup"),  &Test_HandledNotifyStaysInPopup  },
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
