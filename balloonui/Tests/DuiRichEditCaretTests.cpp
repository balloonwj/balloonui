/**
 *  DuiRichEdit 文本插入光标的单元测试实现。用例说明见 DuiRichEditCaretTests.h。
 *  balloonwj@qq.com   2026-09-30
 */

#include "stdafx.h"
#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT
#include "DuiRichEditCaretTests.h"
#include "../DuiHost.h"
#include "../Controls/Input/DuiEdit.h"
#include <vector>

// CT2 截屏之前要等桌面合成完成（DwmFlush）。
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace balloonwjui {

namespace DuiRichEditCaretTests {

namespace {

// 宿主窗口（也就是文本框）的宽度（像素）。取一个普通单行文本框的宽度即可。
const int kHostWidth = 200;

// 宿主窗口（也就是文本框）的高度（像素）。与客户端资料窗「备注名」文本框的
// 高度 28 一致 —— 本组用例针对的问题就是在那个文本框上发现的。
const int kHostHeight = 28;

// 内存画布的初始底色。控件绘制时会自己铺背景，这里只是让画布的初值确定。
const COLORREF kCanvasBgColor = RGB(255, 255, 255);

// 32 位像素里 RGB 三个通道所占的位。最高字节是保留字节，内容不确定，比较
// 像素时必须屏蔽掉。
const DWORD kPixelRgbMask = 0x00FFFFFF;

// CT4：不处理消息、连续绘制的次数。
const int kRepaintPasses = 5;

// CT3：观察闪烁的时长 = 闪烁周期 × kBlinkObserveNum / kBlinkObserveDen，
// 即 2.5 个周期 —— 保证至少经历一次亮、灭切换，并留有余量。
const int kBlinkObserveNum = 5;
const int kBlinkObserveDen = 2;

// CT2：上屏采样的时长 = 闪烁周期 × kSysCaretObserveNum / kSysCaretObserveDen，
// 即 1.5 个周期。系统光标一旦被显示，就按闪烁周期在屏幕上反色；1.5 个周期内
// 它必然经历一次「亮」，若它会在屏幕上留下像素，这段时间里一定采得到。
const int kSysCaretObserveNum = 3;
const int kSysCaretObserveDen = 2;

// 测试窗口放在屏幕外时的坐标（像素）。远离任何显示器，窗口可见但不会出现在
// 用户眼前（与 DuiRichEditTests 的同名做法一致）。
const int kOffscreenPos = -32000;

// CT2 把测试窗口放到屏幕上时，窗口左上角的屏幕坐标（像素）。取主显示器左上
// 角附近、避开屏幕边缘的一点；窗口置顶，不会被别的窗口挡住。
const int kOnScreenLeft = 40;
const int kOnScreenTop  = 40;

// Windows 10 1703 起定义的「每显示器感知 V2」DPI 感知上下文的取值。为了在较老
// 的 SDK 与系统上也能编译运行，不直接引用系统头文件里的宏，按取值动态使用。
const LONG_PTR kDpiContextPerMonitorAwareV2 = -4;

// 观察时长的上限（毫秒）。用户把闪烁周期调得很长时，避免用例跑得太久。
const DWORD kObserveCapMs = 4000;

// 用户在系统设置里关闭了光标闪烁（GetCaretBlinkTime 返回 INFINITE）时，
// 观察多长时间（毫秒）。此时光标应当一直亮着，看一小段即可。
const DWORD kNoBlinkObserveMs = 300;

// 观察循环里相邻两次采样的间隔（毫秒）。远小于闪烁周期（系统默认 530），
// 保证每个相位都能采到好几帧。
const DWORD kFrameIntervalMs = 15;

// CT5：打进去的两个字符。
const TCHAR kTypedChar1 = _T('A');
const TCHAR kTypedChar2 = _T('B');

// CT8：取光标右侧第几列作为「背景」参照列（像素）。空文本框里插入点右边
// 就是空白，隔开一列是为了避开光标本身可能的宽度误差。
const int kBackgroundProbeOffset = 2;

struct Result
{
    CString name;
    bool    ok;
    CString detail;
};

static Result OK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok   = true;
    return r;
}

static Result Fail(const CString& n, const CString& d)
{
    Result r;
    r.name   = n;
    r.ok     = false;
    r.detail = d;
    return r;
}

// 断言宏。把子项名字拼进 detail —— 本库的报告只打印用例表里的名字和
// detail 两样，Fail 的第一个参数不会出现在输出里。
#define EXPECT_BOOL(actual, expected, name) \
    do { \
        bool _a = (actual); \
        bool _e = (expected); \
        if (_a != _e) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected=%d got=%d"), name, _e ? 1 : 0, _a ? 1 : 0); \
            return Fail(name, _d); \
        } \
    } while (0)

#define EXPECT_INT(actual, expected, name) \
    do { \
        int _a = (int)(actual); \
        int _e = (int)(expected); \
        if (_a != _e) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected=%d got=%d"), name, _e, _a); \
            return Fail(name, _d); \
        } \
    } while (0)

// 临时顶层窗口，用于搭建「真窗口 → DuiHost 子窗口 → 控件」的完整链路。
//
// 必须是可见窗口：Win32 的焦点接口对不可见窗口的行为不可靠。默认挪到屏幕外，
// 以免测试运行时在用户眼前闪一下。只有 CT2 需要把窗口放到屏幕上：系统光标
// 在屏幕外的窗口上根本不画，从窗口自己的设备上下文里也读不到它的像素
// （2026-09-30 实测），要观察它只能真的上屏再截屏。
class TestTopWnd
{
public:
    // 创建窗口。
    //   bOnScreen：true 放到屏幕左上角并置顶（CT2 截屏用）；false 放到屏幕外。
    explicit TestTopWnd(bool bOnScreen)
        : m_hwnd(nullptr)
    {
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = ::DefWindowProc;
        wc.hInstance     = ::GetModuleHandle(nullptr);
        wc.lpszClassName = _T("DuiRichEditCaretTestTopWnd");
        ::RegisterClassEx(&wc);

        const DWORD exStyle = bOnScreen ? (WS_EX_TOOLWINDOW | WS_EX_TOPMOST) : WS_EX_TOOLWINDOW;
        const int   left    = bOnScreen ? kOnScreenLeft : kOffscreenPos;
        const int   top     = bOnScreen ? kOnScreenTop : kOffscreenPos;
        m_hwnd = ::CreateWindowEx(
            exStyle, _T("DuiRichEditCaretTestTopWnd"), _T(""),
            WS_POPUP | WS_VISIBLE,
            left, top, kHostWidth, kHostHeight,
            nullptr, nullptr, ::GetModuleHandle(nullptr), nullptr);
    }

    ~TestTopWnd()
    {
        if (m_hwnd != nullptr)
        {
            ::DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    TestTopWnd(const TestTopWnd&) = delete;
    TestTopWnd& operator=(const TestTopWnd&) = delete;

    HWND get() const { return m_hwnd; }

private:
    HWND m_hwnd;    // 顶层窗口句柄；创建失败时为 nullptr。随本对象析构而销毁
};

// 在本对象的生存期内，把当前线程的 DPI 感知切换为「每显示器感知 V2」，析构时
// 恢复原值。
//
// CT2 要截屏并与控件的绘制结果逐像素比较。进程若不感知 DPI 而系统缩放又不是
// 100%，截到的画面是被系统缩放过的，比较必然失败。在本对象生存期内创建的窗口
// 会沿用这个感知方式，截屏坐标与像素也都按物理像素计算。系统不支持时（早于
// Windows 10 1703）什么都不做。
class ScopedPerMonitorDpi
{
public:
    ScopedPerMonitorDpi()
        : m_pfnSet(nullptr)
        , m_hOldContext(nullptr)
    {
        HMODULE hUser = ::GetModuleHandle(_T("user32.dll"));
        if (hUser == nullptr)
        {
            return;
        }
        m_pfnSet = (FnSetThreadDpiContext)::GetProcAddress(hUser, "SetThreadDpiAwarenessContext");
        if (m_pfnSet != nullptr)
        {
            m_hOldContext = m_pfnSet((HANDLE)kDpiContextPerMonitorAwareV2);
        }
    }

    ~ScopedPerMonitorDpi()
    {
        if (m_pfnSet != nullptr && m_hOldContext != nullptr)
        {
            m_pfnSet(m_hOldContext);
        }
    }

    ScopedPerMonitorDpi(const ScopedPerMonitorDpi&) = delete;
    ScopedPerMonitorDpi& operator=(const ScopedPerMonitorDpi&) = delete;

private:
    typedef HANDLE (WINAPI *FnSetThreadDpiContext)(HANDLE);

    FnSetThreadDpiContext m_pfnSet;       // 系统的 SetThreadDpiAwarenessContext；取不到时为 nullptr
    HANDLE                m_hOldContext;  // 切换之前的 DPI 感知上下文，析构时恢复；切换失败时为 nullptr
};

// 32 位、自上而下排列的内存画布，用来把控件绘制出来后逐像素检查。
// 与 DuiHost 的后台缓冲同一种格式，控件在这里画出来的结果与实机一致。
class Canvas
{
public:
    // 按给定尺寸（像素）建画布。失败时 IsValid() 返回 false。
    Canvas(int width, int height)
        : m_hdc(nullptr)
        , m_hbm(nullptr)
        , m_hbmOld(nullptr)
        , m_pBits(nullptr)
        , m_width(width)
        , m_height(height)
    {
        BITMAPINFO bmi;
        ::ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = width;
        bmi.bmiHeader.biHeight      = -height;     // 负高度表示自上而下排列
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC hdcScreen = ::GetDC(nullptr);
        m_hdc = ::CreateCompatibleDC(hdcScreen);
        ::ReleaseDC(nullptr, hdcScreen);
        if (m_hdc == nullptr)
        {
            return;
        }

        void* pBits = nullptr;
        m_hbm = ::CreateDIBSection(m_hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        if (m_hbm == nullptr)
        {
            return;
        }
        m_pBits  = (const DWORD*)pBits;
        m_hbmOld = (HBITMAP)::SelectObject(m_hdc, m_hbm);
    }

    ~Canvas()
    {
        if (m_hdc != nullptr && m_hbmOld != nullptr)
        {
            ::SelectObject(m_hdc, m_hbmOld);
        }
        if (m_hbm != nullptr)
        {
            ::DeleteObject(m_hbm);
        }
        if (m_hdc != nullptr)
        {
            ::DeleteDC(m_hdc);
        }
    }

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    // 画布是否建成功。
    bool IsValid() const { return m_pBits != nullptr; }

    // 画布的设备上下文，供控件绘制。所有权归本对象。
    HDC GetDC() const { return m_hdc; }

    // 把整块画布铺成初始底色。
    void Clear()
    {
        RECT rc;
        ::SetRect(&rc, 0, 0, m_width, m_height);
        HBRUSH hbr = ::CreateSolidBrush(kCanvasBgColor);
        ::FillRect(m_hdc, &rc, hbr);
        ::DeleteObject(hbr);
        // GDI 的绘制可能被批量缓存，读像素之前先让它落到位图上。
        ::GdiFlush();
    }

    // 取 (x, y) 处的像素，只保留 RGB 三个通道。坐标越界返回 0。
    DWORD Pixel(int x, int y) const
    {
        if (m_pBits == nullptr || x < 0 || y < 0 || x >= m_width || y >= m_height)
        {
            return 0;
        }
        return m_pBits[y * m_width + x] & kPixelRgbMask;
    }

    // 把 rc 范围内的像素按行依次取出（只保留 RGB 三个通道）。rc 超出画布的
    // 部分被裁掉。
    //   out：出参，先清空再填入。
    void Capture(const RECT& rc, std::vector<DWORD>& out) const
    {
        out.clear();
        ::GdiFlush();
        RECT rcCanvas;
        ::SetRect(&rcCanvas, 0, 0, m_width, m_height);
        RECT rcClip;
        if (!::IntersectRect(&rcClip, &rc, &rcCanvas))
        {
            return;
        }
        for (int y = rcClip.top; y < rcClip.bottom; ++y)
        {
            for (int x = rcClip.left; x < rcClip.right; ++x)
            {
                out.push_back(Pixel(x, y));
            }
        }
    }

private:
    HDC          m_hdc;       // 内存设备上下文；随本对象析构而删除
    HBITMAP      m_hbm;       // 画布位图（DIB section）；随本对象析构而删除
    HBITMAP      m_hbmOld;    // 选入画布位图之前设备上下文里原有的位图，析构时换回去
    const DWORD* m_pBits;     // 位图像素区首地址，由系统分配、随 m_hbm 一起释放
    int          m_width;     // 画布宽度（像素）
    int          m_height;    // 画布高度（像素）
};

// 一条完整的「顶层窗口 → DuiHost → DuiEdit」链路。文本框就是宿主的根控件，
// 因此占满宿主客户区（kHostWidth × kHostHeight）。
class EditFixture
{
public:
    // 搭建链路。
    //   bOnScreen：true 把顶层窗口放到屏幕上（只有 CT2 需要）；false 放到屏幕外。
    explicit EditFixture(bool bOnScreen = false)
        : m_top(bOnScreen)
        , m_pEdit(nullptr)
    {
        if (m_top.get() == nullptr)
        {
            return;
        }
        RECT rcHost;
        ::SetRect(&rcHost, 0, 0, kHostWidth, kHostHeight);
        m_host.Create(m_top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
        if (!m_host.IsWindow())
        {
            return;
        }
        m_pEdit = new DuiEdit();
        m_host.SetRoot(std::unique_ptr<DuiControl>(m_pEdit));
    }

    ~EditFixture()
    {
        // 先销毁宿主窗口（连同控件树），顶层窗口随后由成员析构销毁。
        if (m_host.IsWindow())
        {
            m_host.DestroyWindow();
        }
        m_pEdit = nullptr;
    }

    EditFixture(const EditFixture&) = delete;
    EditFixture& operator=(const EditFixture&) = delete;

    // 链路是否搭建成功。
    bool IsReady() const { return m_pEdit != nullptr; }

    // 文本框。所有权归宿主的控件树。
    DuiEdit* Edit() const { return m_pEdit; }

    // 宿主。
    DuiHost& Host() { return m_host; }

    // 模拟真实情形给文本框焦点：Win32 焦点先落在顶层窗口上，再把 DUI 焦点
    // 交给文本框，由宿主据此把 Win32 焦点要过去。
    void Focus()
    {
        ::SetFocus(m_top.get());
        m_pEdit->SetFocus();
    }

    // 把文本框完整绘制到画布上。不处理任何消息，因此不会改变闪烁相位。
    void Render(Canvas& canvas)
    {
        canvas.Clear();
        RECT rc;
        ::SetRect(&rc, 0, 0, kHostWidth, kHostHeight);
        m_pEdit->OnPaint(canvas.GetDC(), rc);
        ::GdiFlush();
    }

    // 参照绘制：临时关闭光标显示后绘制一次，再恢复。得到的是「同一时刻、
    // 不画光标」的画面，用来与正常绘制逐像素比较。
    void RenderWithoutCaret(Canvas& canvas)
    {
        m_pEdit->SetShowCaret(false);
        Render(canvas);
        m_pEdit->SetShowCaret(true);
    }

private:
    TestTopWnd      m_top;     // 顶层窗口；必须先于 m_host 构造，因为宿主是它的子窗口
    DuiHost         m_host;    // DUI 宿主窗口；随本对象析构而销毁
    DuiEdit*        m_pEdit;   // 文本框；所有权归 m_host 的控件树，这里只保留访问指针
};

// 把屏幕上 rcScreen 范围内的画面截到画布的 (0, 0) 处。
//   rcScreen：屏幕坐标（像素）；宽高不得超过画布。
//   返回：截屏成功返回 true。
bool CaptureScreen(const RECT& rcScreen, Canvas& canvas)
{
    canvas.Clear();
    HDC hdcScreen = ::GetDC(nullptr);
    if (hdcScreen == nullptr)
    {
        return false;
    }
    const BOOL bOk = ::BitBlt(canvas.GetDC(), 0, 0,
                              rcScreen.right - rcScreen.left, rcScreen.bottom - rcScreen.top,
                              hdcScreen, rcScreen.left, rcScreen.top, SRCCOPY);
    ::ReleaseDC(nullptr, hdcScreen);
    ::GdiFlush();
    return bOk != FALSE;
}

// 当前线程的系统光标状态。
struct SystemCaret
{
    HWND m_hwnd;       // 光标所属的窗口；本线程没有光标时为 nullptr
    RECT m_rc;         // 光标矩形，m_hwnd 的客户区坐标（像素）
    bool m_bShowing;   // GUI_CARETBLINKING 标志：系统此刻认为光标处于显示中的「亮」相位
};

// 读当前线程的系统光标状态。
//   返回：GetGUIThreadInfo 调用成功返回 true。
bool QuerySystemCaret(SystemCaret& out)
{
    out.m_hwnd = nullptr;
    ::SetRectEmpty(&out.m_rc);
    out.m_bShowing = false;

    GUITHREADINFO gti;
    ::ZeroMemory(&gti, sizeof(gti));
    gti.cbSize = sizeof(gti);
    if (!::GetGUIThreadInfo(::GetCurrentThreadId(), &gti))
    {
        return false;
    }
    out.m_hwnd     = gti.hwndCaret;
    out.m_rc       = gti.rcCaret;
    out.m_bShowing = (gti.flags & GUI_CARETBLINKING) != 0;
    return true;
}

// 取「光标应当画在哪里」：系统光标矩形与文本框文字区的交集。光标被裁剪在
// 文字区内，检查范围也跟着裁。
//   返回：本线程没有光标、或交集为空时返回 false。
bool GetCaretCheckRect(DuiEdit* pEdit, RECT& outRc)
{
    ::SetRectEmpty(&outRc);
    SystemCaret caret;
    if (!QuerySystemCaret(caret) || caret.m_hwnd == nullptr)
    {
        return false;
    }
    const RECT rcText = pEdit->Test_GetTextRect();
    return ::IntersectRect(&outRc, &caret.m_rc, &rcText) != FALSE;
}

// 处理本线程积压的消息，持续 ms 毫秒。线程定时器（光标闪烁就靠它）要靠这一步
// 才会触发。
void PumpMessagesFor(DWORD ms)
{
    const DWORD start = ::GetTickCount();
    MSG msg;
    for (;;)
    {
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        const DWORD elapsed = ::GetTickCount() - start;
        if (elapsed >= ms)
        {
            break;
        }
        ::MsgWaitForMultipleObjects(0, nullptr, FALSE, ms - elapsed, QS_ALLINPUT);
    }
}

// 按系统光标闪烁周期算观察时长（毫秒）= 周期 × num / den，封顶 kObserveCapMs。
//   outBlinks：出参。用户在系统设置里关闭了闪烁时为 false，此时返回 kNoBlinkObserveMs。
DWORD ObserveDurationMs(int num, int den, bool& outBlinks)
{
    const UINT blink = ::GetCaretBlinkTime();
    if (blink == 0 || blink == INFINITE)
    {
        outBlinks = false;
        return kNoBlinkObserveMs;
    }
    outBlinks = true;
    DWORD ms = (DWORD)(((unsigned long long)blink * (unsigned long long)num) / (unsigned long long)den);
    if (ms > kObserveCapMs)
    {
        ms = kObserveCapMs;
    }
    return ms;
}

// 两份同尺寸像素里不相同的个数。尺寸不同时返回 -1。
int CountDiffPixels(const std::vector<DWORD>& a, const std::vector<DWORD>& b)
{
    if (a.size() != b.size())
    {
        return -1;
    }
    int diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
        {
            ++diff;
        }
    }
    return diff;
}

// 比较两块画布在 rc 范围内的像素。
//   outBounds：出参，所有不相同像素的外接矩形；全部相同时为空矩形。失败时靠它
//              判断差异落在哪里（光标、滚动条、选区……）。
//   返回：不相同的像素个数。
int DiffCanvasInRect(const Canvas& a, const Canvas& b, const RECT& rc, RECT& outBounds)
{
    ::SetRectEmpty(&outBounds);
    ::GdiFlush();
    int diff = 0;
    for (int y = rc.top; y < rc.bottom; ++y)
    {
        for (int x = rc.left; x < rc.right; ++x)
        {
            if (a.Pixel(x, y) == b.Pixel(x, y))
            {
                continue;
            }
            if (diff == 0)
            {
                ::SetRect(&outBounds, x, y, x + 1, y + 1);
            }
            else
            {
                RECT rcPixel;
                ::SetRect(&rcPixel, x, y, x + 1, y + 1);
                ::UnionRect(&outBounds, &outBounds, &rcPixel);
            }
            ++diff;
        }
    }
    return diff;
}

// 光标在一次绘制结果里的状态。
enum CaretPaintState
{
    kCaretAbsent  = 0,   // 光标矩形内与参照绘制完全相同：这一帧没有画光标（灭）
    kCaretPresent = 1,   // 光标矩形内每个像素都与参照绘制不同：完整地画出了光标（亮）
    kCaretPartial = 2,   // 只有一部分像素不同：光标只画出了一段 —— 本组用例要杜绝的现象
    kCaretInvalid = 3,   // 比较范围为空或两份像素尺寸不一致，无法判定
};

// 比较同一范围内的一帧与参照绘制，判定光标状态。
//   outDiff：出参，不相同的像素个数。
CaretPaintState ClassifyCaret(const std::vector<DWORD>& frame,
                              const std::vector<DWORD>& reference,
                              int& outDiff)
{
    outDiff = CountDiffPixels(frame, reference);
    if (outDiff < 0 || frame.empty())
    {
        return kCaretInvalid;
    }
    if (outDiff == 0)
    {
        return kCaretAbsent;
    }
    if (outDiff == (int)frame.size())
    {
        return kCaretPresent;
    }
    return kCaretPartial;
}

// 光标状态的文字说明，拼进失败信息里。
LPCTSTR CaretStateName(CaretPaintState state)
{
    switch (state)
    {
    //这一帧没有画光标
    case kCaretAbsent:
        return _T("absent");
    //完整地画出了光标
    case kCaretPresent:
        return _T("present");
    //只画出了一段
    case kCaretPartial:
        return _T("partial");
    //无法判定
    default:
        return _T("invalid");
    }
}

// 矩形的文字形式，拼进失败信息里。
CString RectText(const RECT& rc)
{
    CString s;
    s.Format(_T("(%d,%d)-(%d,%d)"), rc.left, rc.top, rc.right, rc.bottom);
    return s;
}

} // anonymous namespace

//CT1 获得焦点后，光标画进后台缓冲。
//
//刚获得焦点时光标处于「亮」的相位。此时绘制一次，光标矩形内的每个像素都应当
//与「不画光标」的参照绘制不同；紧挨着光标的左右两列则完全相同 —— 光标没有
//画宽，也没有画偏。
static Result Test_CaretPaintedIntoBufferOnFocus()
{
    EditFixture fx;
    if (!fx.IsReady())
    {
        return Fail(_T("CT1"), _T("CT1: cannot create host chain"));
    }
    fx.Focus();

    RECT rcCheck;
    if (!GetCaretCheckRect(fx.Edit(), rcCheck))
    {
        return Fail(_T("CT1"), _T("CT1/noCaretRect: no system caret inside the text rect after focus"));
    }

    Canvas frame(kHostWidth, kHostHeight);
    Canvas reference(kHostWidth, kHostHeight);
    if (!frame.IsValid() || !reference.IsValid())
    {
        return Fail(_T("CT1"), _T("CT1: cannot create canvas"));
    }
    fx.Render(frame);
    fx.RenderWithoutCaret(reference);

    std::vector<DWORD> pixFrame;
    std::vector<DWORD> pixRef;
    frame.Capture(rcCheck, pixFrame);
    reference.Capture(rcCheck, pixRef);
    int diff = 0;
    const CaretPaintState state = ClassifyCaret(pixFrame, pixRef, diff);
    if (state != kCaretPresent)
    {
        CString d;
        d.Format(_T("CT1/caretPainted: caret rect %s state=%s diff=%d of %d"),
                 (LPCTSTR)RectText(rcCheck), CaretStateName(state), diff, (int)pixFrame.size());
        return Fail(_T("CT1"), d);
    }

    // 光标左侧紧挨着的一列。
    RECT rcLeft = rcCheck;
    rcLeft.right = rcCheck.left;
    rcLeft.left  = rcCheck.left - 1;
    frame.Capture(rcLeft, pixFrame);
    reference.Capture(rcLeft, pixRef);
    EXPECT_INT(CountDiffPixels(pixFrame, pixRef), 0, _T("CT1/leftColumnUntouched"));

    // 光标右侧紧挨着的一列。
    RECT rcRight = rcCheck;
    rcRight.left  = rcCheck.right;
    rcRight.right = rcCheck.right + 1;
    frame.Capture(rcRight, pixFrame);
    reference.Capture(rcRight, pixRef);
    EXPECT_INT(CountDiffPixels(pixFrame, pixRef), 0, _T("CT1/rightColumnUntouched"));
    return OK(_T("CaretPaintedIntoBufferOnFocus"));
}

//CT2 系统光标仍然跟踪插入点，但不在屏幕上留下任何像素。
//
//系统光标不能不建：输入法候选窗按它的位置弹出。但它不能在屏幕上画出东西 ——
//它靠在屏幕上反色来闪烁，与后台缓冲拷上屏的绘制方式相互干扰，正是本组用例
//针对的问题的根源。
//
//单靠「宿主不调用 ShowCaret」做不到这一点：排版引擎会绕过宿主直接调用系统的
//光标接口，与宿主 BeginPaint / EndPaint 自带的隐藏、恢复光标叠加之后，系统光标
//会被显示出来（2026-09-30 实测）。所以本用例不看系统光标是否处于显示状态，而是
//直接看屏幕：把测试窗口放到屏幕上，在 1.5 个闪烁周期内反复「处理消息 → 强制宿主
//重绘 → 等桌面合成完成 → 截屏」，每次都与控件此刻的自绘结果逐像素比较。屏幕上
//只要多出系统光标的像素，就会与自绘结果不同。
//
//同时核对系统光标始终属于宿主窗口、矩形与文字区相交 —— 它确实跟着插入点。
static Result Test_SystemCaretLeavesNoPixelsOnScreen()
{
    // 截屏要与控件的绘制结果逐像素比较，必须按物理像素工作，见 ScopedPerMonitorDpi。
    ScopedPerMonitorDpi dpiScope;
    EditFixture fx(true);
    if (!fx.IsReady())
    {
        return Fail(_T("CT2"), _T("CT2: cannot create host chain"));
    }
    fx.Focus();

    // 文字区在屏幕上的位置。截屏只截这一块，系统光标只可能出现在这里。
    const RECT rcText = fx.Edit()->Test_GetTextRect();
    POINT ptOrigin;
    ptOrigin.x = rcText.left;
    ptOrigin.y = rcText.top;
    ::ClientToScreen(fx.Host().m_hWnd, &ptOrigin);
    RECT rcScreen;
    ::SetRect(&rcScreen, ptOrigin.x, ptOrigin.y,
              ptOrigin.x + (rcText.right - rcText.left),
              ptOrigin.y + (rcText.bottom - rcText.top));

    Canvas screen(kHostWidth, kHostHeight);
    Canvas frame(kHostWidth, kHostHeight);
    if (!screen.IsValid() || !frame.IsValid())
    {
        return Fail(_T("CT2"), _T("CT2: cannot create canvas"));
    }

    bool bBlinks = false;
    const DWORD observeMs = ObserveDurationMs(kSysCaretObserveNum, kSysCaretObserveDen, bBlinks);

    int samples = 0;
    const DWORD start = ::GetTickCount();
    for (;;)
    {
        // 让闪烁定时器、引擎、系统光标都有机会运转。
        PumpMessagesFor(kFrameIntervalMs);
        // 把待重画的内容画完，屏幕上的控件画面才与此刻的状态一致。
        ::UpdateWindow(fx.Host().m_hWnd);
        // 等桌面合成把刚画的内容（连同系统光标可能留下的反色）送上屏幕。
        // 合成不可用时退回等一小段时间。
        if (FAILED(::DwmFlush()))
        {
            ::Sleep(kFrameIntervalMs);
        }

        SystemCaret caret;
        if (!QuerySystemCaret(caret))
        {
            return Fail(_T("CT2"), _T("CT2: GetGUIThreadInfo failed"));
        }
        ++samples;

        // 系统光标必须属于宿主窗口 —— 否则说明它根本没建，或者建到了别处。
        if (caret.m_hwnd != fx.Host().m_hWnd)
        {
            CString d;
            d.Format(_T("CT2/ownedByHost: sample %d hwndCaret=%p host=%p"),
                     samples, caret.m_hwnd, fx.Host().m_hWnd);
            return Fail(_T("CT2"), d);
        }

        // 系统光标矩形非空且与文字区相交 —— 它确实跟着插入点。
        RECT rcInter;
        if (::IsRectEmpty(&caret.m_rc) || !::IntersectRect(&rcInter, &caret.m_rc, &rcText))
        {
            CString d;
            d.Format(_T("CT2/insideTextRect: sample %d caret=%s text=%s"),
                     samples, (LPCTSTR)RectText(caret.m_rc), (LPCTSTR)RectText(rcText));
            return Fail(_T("CT2"), d);
        }

        // 截屏与自绘逐像素比较。截屏画布的 (0, 0) 对应文字区的左上角。
        if (!CaptureScreen(rcScreen, screen))
        {
            return Fail(_T("CT2"), _T("CT2: screen capture failed"));
        }
        fx.Render(frame);
        int diff = 0;
        RECT rcDiff;
        ::SetRectEmpty(&rcDiff);
        for (int y = rcText.top; y < rcText.bottom; ++y)
        {
            for (int x = rcText.left; x < rcText.right; ++x)
            {
                if (screen.Pixel(x - rcText.left, y - rcText.top) == frame.Pixel(x, y))
                {
                    continue;
                }
                RECT rcPixel;
                ::SetRect(&rcPixel, x, y, x + 1, y + 1);
                if (diff == 0)
                {
                    rcDiff = rcPixel;
                }
                else
                {
                    ::UnionRect(&rcDiff, &rcDiff, &rcPixel);
                }
                ++diff;
            }
        }
        if (diff != 0)
        {
            CString d;
            d.Format(_T("CT2/noSystemCaretPixels: sample %d, %d pixels on screen differ from the ")
                     _T("control's own painting within %s, system caret %s showing=%d"),
                     samples, diff, (LPCTSTR)RectText(rcDiff),
                     (LPCTSTR)RectText(caret.m_rc), caret.m_bShowing ? 1 : 0);
            return Fail(_T("CT2"), d);
        }

        if (::GetTickCount() - start >= observeMs)
        {
            break;
        }
    }
    return OK(_T("SystemCaretLeavesNoPixelsOnScreen"));
}

//CT3 光标会闪烁，且每一帧都是完整的。
//
//在 2.5 个闪烁周期内每隔约 15 毫秒绘制一次：必须既看到亮帧也看到灭帧；每一帧
//光标矩形要么整块亮、要么整块灭，**不允许只亮一段** —— 这正是 2026-09-30
//在资料窗「备注名」文本框上看到的现象。用户关闭了系统光标闪烁时，光标应当一直亮。
static Result Test_CaretBlinksAndIsNeverPartial()
{
    EditFixture fx;
    if (!fx.IsReady())
    {
        return Fail(_T("CT3"), _T("CT3: cannot create host chain"));
    }
    fx.Focus();

    RECT rcCheck;
    if (!GetCaretCheckRect(fx.Edit(), rcCheck))
    {
        return Fail(_T("CT3"), _T("CT3/noCaretRect: no system caret inside the text rect after focus"));
    }

    Canvas frame(kHostWidth, kHostHeight);
    Canvas reference(kHostWidth, kHostHeight);
    if (!frame.IsValid() || !reference.IsValid())
    {
        return Fail(_T("CT3"), _T("CT3: cannot create canvas"));
    }

    // 参照绘制只需一次：观察期间文字与选区都不变，变的只有光标。
    fx.RenderWithoutCaret(reference);
    std::vector<DWORD> pixRef;
    reference.Capture(rcCheck, pixRef);

    bool bBlinks = false;
    const DWORD observeMs = ObserveDurationMs(kBlinkObserveNum, kBlinkObserveDen, bBlinks);

    int frames = 0;
    int framesOn = 0;
    int framesOff = 0;
    std::vector<DWORD> pixFrame;
    const DWORD start = ::GetTickCount();
    for (;;)
    {
        fx.Render(frame);
        frame.Capture(rcCheck, pixFrame);
        ++frames;

        int diff = 0;
        const CaretPaintState state = ClassifyCaret(pixFrame, pixRef, diff);
        switch (state)
        {
        //完整地画出了光标：亮帧
        case kCaretPresent:
            ++framesOn;
            break;
        //完全没画：灭帧
        case kCaretAbsent:
            ++framesOff;
            break;
        //只画出了一段，或无法判定：立即判失败并报出是第几帧
        default:
            {
                CString d;
                d.Format(_T("CT3/neverPartial: frame %d state=%s diff=%d of %d, caret rect %s"),
                         frames, CaretStateName(state), diff, (int)pixFrame.size(),
                         (LPCTSTR)RectText(rcCheck));
                return Fail(_T("CT3"), d);
            }
        }

        if (::GetTickCount() - start >= observeMs)
        {
            break;
        }
        PumpMessagesFor(kFrameIntervalMs);
    }

    if (bBlinks)
    {
        if (framesOn == 0 || framesOff == 0)
        {
            CString d;
            d.Format(_T("CT3/blinks: blink=%u ms, %d frames, on=%d off=%d (both must be > 0)"),
                     ::GetCaretBlinkTime(), frames, framesOn, framesOff);
            return Fail(_T("CT3"), d);
        }
    }
    else
    {
        EXPECT_INT(framesOff, 0, _T("CT3/noBlinkAlwaysOn"));
    }
    return OK(_T("CaretBlinksAndIsNeverPartial"));
}

//CT4 同一闪烁相位内反复重绘，光标像素保持不变。
//
//不处理任何消息、连续绘制 5 次（闪烁定时器不会触发，相位不变）：光标所在
//范围的像素每次都必须完全相同，且不是「只亮一段」。反色一类的画法若直接作用
//在上一帧的结果上，第二次就会把光标擦掉，这条用例把这类写法挡在外面。
static Result Test_RepaintKeepsCaretPixelsStable()
{
    EditFixture fx;
    if (!fx.IsReady())
    {
        return Fail(_T("CT4"), _T("CT4: cannot create host chain"));
    }
    fx.Focus();

    RECT rcCheck;
    if (!GetCaretCheckRect(fx.Edit(), rcCheck))
    {
        return Fail(_T("CT4"), _T("CT4/noCaretRect: no system caret inside the text rect after focus"));
    }

    Canvas frame(kHostWidth, kHostHeight);
    Canvas reference(kHostWidth, kHostHeight);
    if (!frame.IsValid() || !reference.IsValid())
    {
        return Fail(_T("CT4"), _T("CT4: cannot create canvas"));
    }

    std::vector<DWORD> pixFirst;
    fx.Render(frame);
    frame.Capture(rcCheck, pixFirst);

    std::vector<DWORD> pixFrame;
    for (int pass = 1; pass < kRepaintPasses; ++pass)
    {
        fx.Render(frame);
        frame.Capture(rcCheck, pixFrame);
        const int diff = CountDiffPixels(pixFrame, pixFirst);
        if (diff != 0)
        {
            CString d;
            d.Format(_T("CT4/stableAcrossRepaints: pass %d differs from pass 0 in %d of %d pixels"),
                     pass, diff, (int)pixFirst.size());
            return Fail(_T("CT4"), d);
        }
    }

    std::vector<DWORD> pixRef;
    fx.RenderWithoutCaret(reference);
    reference.Capture(rcCheck, pixRef);
    int diff = 0;
    const CaretPaintState state = ClassifyCaret(pixFirst, pixRef, diff);
    EXPECT_BOOL(state == kCaretPresent || state == kCaretAbsent, true, _T("CT4/notPartial"));
    return OK(_T("RepaintKeepsCaretPixelsStable"));
}

//CT5 打字后，光标移到新的插入点并立即显示。
//
//打入两个字符：系统光标矩形应当右移；紧接着绘制（不等闪烁定时器），新位置
//上是完整的光标，旧位置上没有光标 —— 插入点一动光标就要立刻亮出来，这是
//所有文本框的惯例，否则用户会觉得输入有延迟。
static Result Test_CaretFollowsTyping()
{
    EditFixture fx;
    if (!fx.IsReady())
    {
        return Fail(_T("CT5"), _T("CT5: cannot create host chain"));
    }
    fx.Focus();

    RECT rcBefore;
    if (!GetCaretCheckRect(fx.Edit(), rcBefore))
    {
        return Fail(_T("CT5"), _T("CT5/noCaretRectBefore: no system caret inside the text rect after focus"));
    }

    ::SendMessage(fx.Host().m_hWnd, WM_CHAR, (WPARAM)kTypedChar1, 0);
    ::SendMessage(fx.Host().m_hWnd, WM_CHAR, (WPARAM)kTypedChar2, 0);

    RECT rcAfter;
    if (!GetCaretCheckRect(fx.Edit(), rcAfter))
    {
        return Fail(_T("CT5"), _T("CT5/noCaretRectAfter: no system caret inside the text rect after typing"));
    }
    EXPECT_BOOL(rcAfter.left > rcBefore.left, true, _T("CT5/caretMovedRight"));

    Canvas frame(kHostWidth, kHostHeight);
    Canvas reference(kHostWidth, kHostHeight);
    if (!frame.IsValid() || !reference.IsValid())
    {
        return Fail(_T("CT5"), _T("CT5: cannot create canvas"));
    }
    fx.Render(frame);
    fx.RenderWithoutCaret(reference);

    std::vector<DWORD> pixFrame;
    std::vector<DWORD> pixRef;
    int diff = 0;

    // 新位置：完整的光标。
    frame.Capture(rcAfter, pixFrame);
    reference.Capture(rcAfter, pixRef);
    const CaretPaintState stateNew = ClassifyCaret(pixFrame, pixRef, diff);
    if (stateNew != kCaretPresent)
    {
        CString d;
        d.Format(_T("CT5/caretAtNewPos: rect %s state=%s diff=%d of %d"),
                 (LPCTSTR)RectText(rcAfter), CaretStateName(stateNew), diff, (int)pixFrame.size());
        return Fail(_T("CT5"), d);
    }

    // 旧位置：没有光标。
    frame.Capture(rcBefore, pixFrame);
    reference.Capture(rcBefore, pixRef);
    EXPECT_INT(CountDiffPixels(pixFrame, pixRef), 0, _T("CT5/noCaretAtOldPos"));
    return OK(_T("CaretFollowsTyping"));
}

//CT6 有选区时不画光标。
//
//选中一段文字时，排版引擎会要求隐藏光标（插入点被选区取代）。此时的绘制结果
//在整个文字区内应当与「不画光标」的参照绘制逐像素相同。
static Result Test_NoCaretWhileSelectionActive()
{
    EditFixture fx;
    if (!fx.IsReady())
    {
        return Fail(_T("CT6"), _T("CT6: cannot create host chain"));
    }
    fx.Edit()->SetText(_T("hello"));
    fx.Focus();
    fx.Edit()->SelectAll();

    Canvas frame(kHostWidth, kHostHeight);
    Canvas reference(kHostWidth, kHostHeight);
    if (!frame.IsValid() || !reference.IsValid())
    {
        return Fail(_T("CT6"), _T("CT6: cannot create canvas"));
    }
    fx.Render(frame);
    fx.RenderWithoutCaret(reference);

    const RECT rcText = fx.Edit()->Test_GetTextRect();
    std::vector<DWORD> pixFrame;
    std::vector<DWORD> pixRef;
    frame.Capture(rcText, pixFrame);
    reference.Capture(rcText, pixRef);
    EXPECT_BOOL(!pixFrame.empty(), true, _T("CT6/textRectNotEmpty"));
    RECT rcDiff;
    const int diff = DiffCanvasInRect(frame, reference, rcText, rcDiff);
    if (diff != 0)
    {
        SystemCaret caret;
        QuerySystemCaret(caret);
        CString d;
        d.Format(_T("CT6/noCaretWithSelection: %d pixels differ within %s, text rect %s, system caret %s"),
                 diff, (LPCTSTR)RectText(rcDiff), (LPCTSTR)RectText(rcText),
                 (LPCTSTR)RectText(caret.m_rc));
        return Fail(_T("CT6"), d);
    }
    return OK(_T("NoCaretWhileSelectionActive"));
}

//CT7 失去焦点后不画光标，系统光标被销毁。
//
//系统光标是每个线程独一份的资源，失焦时必须让出来（见 DuiCaret.h）；控件自己
//画的光标也必须随之消失，否则界面上会同时出现两个闪烁的输入框。
static Result Test_NoCaretAfterKillFocus()
{
    EditFixture fx;
    if (!fx.IsReady())
    {
        return Fail(_T("CT7"), _T("CT7: cannot create host chain"));
    }
    fx.Focus();

    // 前提：获得焦点时确实建了光标，否则后面「光标已销毁」的断言没有意义。
    SystemCaret caret;
    EXPECT_BOOL(QuerySystemCaret(caret), true, _T("CT7/queryBefore"));
    EXPECT_BOOL(caret.m_hwnd == fx.Host().m_hWnd, true, _T("CT7/caretCreatedOnFocus"));

    fx.Host().SetDuiFocus(nullptr);

    EXPECT_BOOL(QuerySystemCaret(caret), true, _T("CT7/queryAfter"));
    EXPECT_BOOL(caret.m_hwnd == nullptr, true, _T("CT7/systemCaretDestroyed"));

    Canvas frame(kHostWidth, kHostHeight);
    Canvas reference(kHostWidth, kHostHeight);
    if (!frame.IsValid() || !reference.IsValid())
    {
        return Fail(_T("CT7"), _T("CT7: cannot create canvas"));
    }
    fx.Render(frame);
    fx.RenderWithoutCaret(reference);

    const RECT rcText = fx.Edit()->Test_GetTextRect();
    std::vector<DWORD> pixFrame;
    std::vector<DWORD> pixRef;
    frame.Capture(rcText, pixFrame);
    reference.Capture(rcText, pixRef);
    EXPECT_BOOL(!pixFrame.empty(), true, _T("CT7/textRectNotEmpty"));
    RECT rcDiff;
    const int diff = DiffCanvasInRect(frame, reference, rcText, rcDiff);
    if (diff != 0)
    {
        CString d;
        d.Format(_T("CT7/noCaretAfterKillFocus: %d pixels differ within %s, text rect %s"),
                 diff, (LPCTSTR)RectText(rcDiff), (LPCTSTR)RectText(rcText));
        return Fail(_T("CT7"), d);
    }
    return OK(_T("NoCaretAfterKillFocus"));
}

//CT8 关闭光标显示（SetShowCaret(false)）时不画光标，但系统光标仍跟踪位置。
//
//只读展示区靠这个开关去掉闪烁的光标，同时保留点击定位、拖选、复制。关掉的只是
//「画」，系统光标照样要建、要跟着插入点走 —— 可编辑控件上关掉光标显示时，输入法
//候选窗仍须落在正确位置。
static Result Test_ShowCaretOffPaintsNothingButTracksPosition()
{
    EditFixture fx;
    if (!fx.IsReady())
    {
        return Fail(_T("CT8"), _T("CT8: cannot create host chain"));
    }
    fx.Edit()->SetShowCaret(false);
    fx.Focus();

    SystemCaret caret;
    EXPECT_BOOL(QuerySystemCaret(caret), true, _T("CT8/query"));
    EXPECT_BOOL(caret.m_hwnd == fx.Host().m_hWnd, true, _T("CT8/systemCaretStillCreated"));
    EXPECT_BOOL(::IsRectEmpty(&caret.m_rc) == FALSE, true, _T("CT8/systemCaretHasRect"));

    RECT rcCheck;
    if (!GetCaretCheckRect(fx.Edit(), rcCheck))
    {
        return Fail(_T("CT8"), _T("CT8/noCaretRect: system caret does not intersect the text rect"));
    }

    Canvas frame(kHostWidth, kHostHeight);
    if (!frame.IsValid())
    {
        return Fail(_T("CT8"), _T("CT8: cannot create canvas"));
    }
    fx.Render(frame);

    // 文本框是空的：光标所在范围应当与右侧的空白处是同一种背景色。
    RECT rcBackground = rcCheck;
    ::OffsetRect(&rcBackground, (rcCheck.right - rcCheck.left) + kBackgroundProbeOffset, 0);
    std::vector<DWORD> pixCaret;
    std::vector<DWORD> pixBackground;
    frame.Capture(rcCheck, pixCaret);
    frame.Capture(rcBackground, pixBackground);
    EXPECT_BOOL(!pixCaret.empty(), true, _T("CT8/caretRectNotEmpty"));
    EXPECT_INT(CountDiffPixels(pixCaret, pixBackground), 0, _T("CT8/noCaretPainted"));
    return OK(_T("ShowCaretOffPaintsNothingButTracksPosition"));
}

CString RunAll()
{
    struct Entry
    {
        LPCTSTR name;
        Result (*fn)();
    };

    Entry tests[] = {
        { _T("CaretPaintedIntoBufferOnFocus"),              &Test_CaretPaintedIntoBufferOnFocus              },
        { _T("SystemCaretLeavesNoPixelsOnScreen"),          &Test_SystemCaretLeavesNoPixelsOnScreen          },
        { _T("CaretBlinksAndIsNeverPartial"),               &Test_CaretBlinksAndIsNeverPartial               },
        { _T("RepaintKeepsCaretPixelsStable"),              &Test_RepaintKeepsCaretPixelsStable              },
        { _T("CaretFollowsTyping"),                         &Test_CaretFollowsTyping                         },
        { _T("NoCaretWhileSelectionActive"),                &Test_NoCaretWhileSelectionActive                },
        { _T("NoCaretAfterKillFocus"),                      &Test_NoCaretAfterKillFocus                      },
        { _T("ShowCaretOffPaintsNothingButTracksPosition"), &Test_ShowCaretOffPaintsNothingButTracksPosition },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    for (int i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); ++i)
    {
        Result r = tests[i].fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), tests[i].name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), tests[i].name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }

    CString summary;
    summary.Format(_T("[summary] DuiRichEditCaretTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

#undef EXPECT_BOOL
#undef EXPECT_INT

} // namespace DuiRichEditCaretTests

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
