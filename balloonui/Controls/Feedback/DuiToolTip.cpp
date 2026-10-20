#include "stdafx.h"
#include "DuiToolTip.h"

#if BUI_FEATURE_TOOLTIP

#include "../../DuiResMgr.h"
#include "../../DuiDpi.h"
#include "../List/DuiMenuPlacement.h"

namespace balloonwjui {

namespace {

// ---- 浮窗尺寸的逻辑值（96 DPI 下的像素），由 MetricsForDpi 按显示器 DPI 换算 ----
const int kPadXLogical         = 8;     // 文字与浮窗左右边框之间的距离
const int kPadYLogical         = 4;     // 文字与浮窗上下边框之间的距离
const int kOffsetXLogical      = 12;    // 浮窗左边缘相对光标热点向右的偏移
const int kOffsetYLogical      = 18;    // 浮窗上边缘相对光标热点向下的偏移，让开光标箭头
const int kGapAboveLogical     = 4;     // 浮窗移到光标上方时，下边缘与光标热点之间的间隙
const int kMaxTextWidthLogical = 400;   // 文字区域的最大宽度，超过时自动换行

// ---- 浮窗配色 ----
const COLORREF kBackColor   = RGB(255, 255, 225);   // 底色：传统 Windows 提示的浅黄色
const COLORREF kBorderColor = RGB(120, 120, 120);   // 1 像素边框的颜色
const COLORREF kTextColor   = RGB(40, 40, 40);      // 文字颜色

// 连续悬停时的短延时 = 正常延时 / 本值。取 5 与 Windows 自带提示一致（TTDT_RESHOW 的默认值
// 是 TTDT_INITIAL 的五分之一）。
const UINT kReshowDelayDivisor = 5;

} // namespace

// =====================================================================
// DuiToolTipWnd: borderless top-level popup that paints text on a pale
// yellow background and auto-deletes via OnFinalMessage. Identical
// teardown discipline to DuiComboBoxPopup.
// =====================================================================

class DuiToolTipWnd : public CWindowImpl<DuiToolTipWnd, CWindow>
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiToolTipWnd__"), 0, COLOR_INFOBK)

    DuiToolTipWnd() = default;
    ~DuiToolTipWnd() = default;

    BEGIN_MSG_MAP(DuiToolTipWnd)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
    END_MSG_MAP()

    void OnFinalMessage(HWND) override
    {
        // Notify the manager so it clears its back-pointer, then delete.
        DuiToolTipMgr::Inst().DetachWindow();
        delete this;
    }

    // 按光标所在显示器的 DPI 排版文字，在光标附近、工作区之内建窗并显示。
    //   cursorPt：光标热点的屏幕坐标。
    //   text：提示文字。
    // 返回是否建出了窗口。文字为空或建窗失败时返回 false，此时不会有 OnFinalMessage，
    // 由调用方释放本对象。
    bool Show(POINT cursorPt, const CString& text)
    {
        m_text = text;
        if (m_text.IsEmpty())
        {
            return false;
        }

        // 提示框窗口尚未创建，按将要弹出的位置所在显示器的 DPI 取字体与尺寸参数；
        // 测量与绘制都用这一个 DPI，保证尺寸与文字一致。
        m_dpi = DuiDpi::GetDpiForPoint(cursorPt);
        m_metrics = DuiToolTipMgr::MetricsForDpi(m_dpi);

        // 文字区域的最大宽度不超过工作区去掉左右内边距之后的宽度，保证换行后的浮窗放得下
        const RECT area = GetMenuPlacementArea(cursorPt);
        int maxTextWidth = m_metrics.m_maxTextWidth;
        const int areaTextWidth = (area.right - area.left) - m_metrics.m_padX * 2;
        if (areaTextWidth > 0 && areaTextWidth < maxTextWidth)
        {
            maxTextWidth = areaTextWidth;
        }
        m_layout = LayoutText(maxTextWidth);

        SIZE tipSize;
        tipSize.cx = m_layout.m_size.cx + m_metrics.m_padX * 2;
        tipSize.cy = m_layout.m_size.cy + m_metrics.m_padY * 2;

        // 默认放在光标右下方；放不下时移到光标上方或向左平移，整体留在工作区内
        POINT preferred;
        preferred.x = cursorPt.x + m_metrics.m_offsetX;
        preferred.y = cursorPt.y + m_metrics.m_offsetY;
        const POINT origin = ClampToolTipOrigin(cursorPt, preferred, m_metrics.m_gapAbove, tipSize, area);

        Create(NULL, NULL, NULL,
               WS_POPUP,
               WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE);
        if (!m_hWnd)
        {
            return false;
        }

        SetWindowPos(NULL, origin.x, origin.y, tipSize.cx, tipSize.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(SW_SHOWNOACTIVATE);
        return true;
    }

private:
    BOOL OnEraseBkgnd(CDCHandle dc)
    {
        CRect rc;
        GetClientRect(&rc);
        HBRUSH bg = ::CreateSolidBrush(kBackColor);
        ::FillRect(dc, &rc, bg);
        ::DeleteObject(bg);
        HPEN pen = ::CreatePen(PS_SOLID, 1, kBorderColor);
        HPEN op  = (HPEN)::SelectObject(dc, pen);
        HBRUSH ob = (HBRUSH)::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
        ::SelectObject(dc, ob);
        ::SelectObject(dc, op);
        ::DeleteObject(pen);
        return TRUE;
    }

    void OnPaint(CDCHandle)
    {
        CPaintDC dc(m_hWnd);
        CRect rc;
        GetClientRect(&rc);

        // Background already painted by OnEraseBkgnd; draw the text.
        HFONT useFont = DuiResMgr::Inst().GetDefaultFontForDpi(m_dpi);
        HFONT oldFont = useFont ? (HFONT)::SelectObject(dc, useFont) : nullptr;
        int oldBk = ::SetBkMode(dc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(dc, kTextColor);
        rc.DeflateRect(m_metrics.m_padX, m_metrics.m_padY);
        //格式标志取 Show 时排版的结果，与测量方式一致：文字区域正好是排版算出的尺寸
        ::DrawText(dc, m_text, -1, &rc, m_layout.m_flags);
        ::SetTextColor(dc, oldClr);
        ::SetBkMode(dc, oldBk);
        if (oldFont)
        {
            ::SelectObject(dc, oldFont);
        }
    }

    // 用 m_dpi 对应的默认字体排版 m_text，文字区域宽度不超过 maxTextWidth 像素
    DuiToolTipMgr::TextLayout LayoutText(int maxTextWidth) const
    {
        HDC hdc = ::GetDC(nullptr);
        HFONT useFont = DuiResMgr::Inst().GetDefaultFontForDpi(m_dpi);
        HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
        const DuiToolTipMgr::TextLayout layout = DuiToolTipMgr::LayoutText(hdc, m_text, maxTextWidth);
        if (oldFont)
        {
            ::SelectObject(hdc, oldFont);
        }
        ::ReleaseDC(nullptr, hdc);
        return layout;
    }

public:
    CString GetText() const { return m_text; }

private:
    CString m_text;                           // 提示文字
    int     m_dpi = 0;                        // Show 时按弹出位置所在显示器取的 DPI；0 = 尚未 Show，按全局 DPI 处理
    DuiToolTipMgr::Metrics    m_metrics = {}; // Show 时按 m_dpi 换算的尺寸参数，绘制时取内边距
    DuiToolTipMgr::TextLayout m_layout = {};  // Show 时的排版结果，绘制时取格式标志
};

// =====================================================================
// DuiToolTipMgr
// =====================================================================

DuiToolTipMgr& DuiToolTipMgr::Inst()
{
    static DuiToolTipMgr s_inst;
    return s_inst;
}

DuiToolTipMgr::Metrics DuiToolTipMgr::MetricsForDpi(int dpi)
{
    // DuiDpi::Scale 在 dpi <= 0 时按 96 处理
    Metrics m;
    m.m_padX         = DuiDpi::Scale(kPadXLogical, dpi);
    m.m_padY         = DuiDpi::Scale(kPadYLogical, dpi);
    m.m_offsetX      = DuiDpi::Scale(kOffsetXLogical, dpi);
    m.m_offsetY      = DuiDpi::Scale(kOffsetYLogical, dpi);
    m.m_gapAbove     = DuiDpi::Scale(kGapAboveLogical, dpi);
    m.m_maxTextWidth = DuiDpi::Scale(kMaxTextWidthLogical, dpi);
    return m;
}

DuiToolTipMgr::TextLayout DuiToolTipMgr::LayoutText(HDC hdc, const CString& text, int maxWidth)
{
    TextLayout layout;
    layout.m_size.cx = 0;
    layout.m_size.cy = 0;
    //提示文字是普通文本（常含文件名、昵称），'&' 要原样显示，不当成助记符前缀：两组标志都带
    //DT_NOPREFIX，否则 DrawText 不画 '&'、并给下一个字符加下划线
    layout.m_flags = DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX;
    if (text.IsEmpty())
    {
        return layout;
    }

    //不含换行符、单行放得下：按单行排版，尺寸与原先一样取 GetTextExtentPoint32
    if (text.Find(_T('\n')) < 0)
    {
        ::GetTextExtentPoint32(hdc, text, text.GetLength(), &layout.m_size);
        if (layout.m_size.cx <= maxWidth)
        {
            return layout;
        }
    }

    //多行：DT_WORDBREAK 在换行符处分行、行宽超过 maxWidth 时在词间换行；DT_EDITCONTROL 让
    //比一行还宽的长词按字符断开（不加它时长词会超出 maxWidth）。DT_CALCRECT 只计算、不绘制，
    //算出的宽为最长的一行、高为行数 × 行高
    layout.m_flags = DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;
    RECT rc = { 0, 0, maxWidth, 0 };
    ::DrawText(hdc, text, text.GetLength(), &rc, layout.m_flags | DT_CALCRECT);
    layout.m_size.cx = rc.right - rc.left;
    layout.m_size.cy = rc.bottom - rc.top;
    return layout;
}

void DuiToolTipMgr::Register(DuiControl* ctrl, LPCTSTR text)
{
    if (!ctrl)
    {
        return;
    }
    CString s = text ? text : _T("");
    for (auto& kv : m_entries)
    {
        if (kv.first == ctrl)
        {
            kv.second = s;
            return;
        }
    }
    m_entries.push_back({ ctrl, s });
}

void DuiToolTipMgr::Unregister(DuiControl* ctrl)
{
    if (!ctrl)
    {
        return;
    }
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
    {
        if (it->first == ctrl)
        {
            m_entries.erase(it);
            break;
        }
    }
    // 提示弹出后 m_pending 仍指向该控件，所以这一个判断同时覆盖等待中与已显示两种情况
    if (m_pending == ctrl)
    {
        m_pending = nullptr;
        HideNow();
    }
}

CString DuiToolTipMgr::GetText(DuiControl* ctrl) const
{
    if (!ctrl)
    {
        return CString();
    }
    for (const auto& kv : m_entries)
    {
        if (kv.first == ctrl)
        {
            return kv.second;
        }
    }
    return CString();
}

CString DuiToolTipMgr::GetShowingText() const
{
    return m_window ? m_window->GetText() : CString();
}

void DuiToolTipMgr::OnHover(DuiControl* ctrl, POINT screenPt)
{
    //没有控件、或控件没有登记提示文字：等同于离开原来跟踪的控件
    if (!ctrl || GetText(ctrl).IsEmpty())
    {
        OnLeave(nullptr);
        return;
    }
    if (m_pending == ctrl)
    {
        return;     // already pending or shown for this ctrl
    }
    //要在关闭当前浮窗之前判断：正有提示显示、或刚离开一个正在显示的提示时，按短延时弹出
    const bool reshow = IsReshowWindowOpen();
    DestroyTipWindow();
    m_pending = ctrl;
    m_pendingPt = screenPt;
    StartTimer(reshow ? m_delayMs / kReshowDelayDivisor : m_delayMs);
}

void DuiToolTipMgr::OnLeave(DuiControl* ctrl)
{
    // Leave only matters if it's the control we were tracking (or a tip is showing).
    if (ctrl != nullptr && ctrl != m_pending && (!m_window))
    {
        return;
    }
    const bool wasShowing = (m_window != nullptr);
    m_pending = nullptr;
    HideNow();
    //离开的是一个正在显示的提示：一个正常延时之内悬停到别的控件，按短延时弹出
    if (wasShowing)
    {
        m_reshowUntil = ::GetTickCount64() + m_delayMs;
    }
}

void DuiToolTipMgr::Retarget(DuiControl* ctrl, LPCTSTR text, POINT screenPt)
{
    if (!ctrl)
    {
        return;
    }
    //先复位：OnHover 对当前跟踪的控件直接返回，不复位就不会按新文字重新计时
    if (m_pending == ctrl)
    {
        OnLeave(ctrl);
    }
    if (text == nullptr || text[0] == _T('\0'))
    {
        Unregister(ctrl);
        return;
    }
    Register(ctrl, text);
    OnHover(ctrl, screenPt);
}

void DuiToolTipMgr::HideNow()
{
    StopTimer();
    DestroyTipWindow();
    //主动收起（按下鼠标、按键、滚轮等）之后的悬停按正常延时
    m_reshowUntil = 0;
}

void DuiToolTipMgr::DestroyTipWindow()
{
    if (m_window && m_window->IsWindow())
    {
        m_window->DestroyWindow();
    }
    // m_window will be cleared by DetachWindow() via OnFinalMessage.
}

void DuiToolTipMgr::DetachWindow()
{
    m_window = nullptr;
}

bool DuiToolTipMgr::IsReshowWindowOpen() const
{
    if (m_window != nullptr)
    {
        return true;
    }
    return m_reshowUntil != 0 && ::GetTickCount64() <= m_reshowUntil;
}

void DuiToolTipMgr::StartTimer(UINT delayMs)
{
    StopTimer();
    m_scheduledDelayMs = delayMs;
    m_timerId = ::SetTimer(nullptr, 0, delayMs, &DuiToolTipMgr::TimerProc);
}

void DuiToolTipMgr::StopTimer()
{
    if (m_timerId)
    {
        ::KillTimer(nullptr, m_timerId);
        m_timerId = 0;
    }
}

VOID CALLBACK DuiToolTipMgr::TimerProc(HWND, UINT, UINT_PTR id, DWORD)
{
    DuiToolTipMgr& mgr = Inst();
    if (mgr.m_timerId != id)
    {
        return;
    }
    mgr.StopTimer();
    mgr.PopShow();
}

void DuiToolTipMgr::PopShow()
{
    if (!m_pending)
    {
        return;
    }
    CString text = GetText(m_pending);
    if (text.IsEmpty())
    {
        return;
    }
    DestroyTipWindow();      // safety
    DuiToolTipWnd* wnd = new DuiToolTipWnd();
    m_window = wnd;
    //建窗失败（如系统窗口句柄耗尽）时不会有 OnFinalMessage，由这里释放对象并清空指针；否则
    //IsShowing() 会一直为真、下次弹出时旧对象泄漏。m_window 已不等于 wnd 说明对象已经在
    //OnFinalMessage 里自行释放，不能再 delete
    if (!wnd->Show(m_pendingPt, text) && m_window == wnd)
    {
        m_window = nullptr;
        delete wnd;
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_TOOLTIP
