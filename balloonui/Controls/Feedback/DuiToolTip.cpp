#include "stdafx.h"
#include "DuiToolTip.h"

#if BUI_FEATURE_TOOLTIP

#include "../../DuiResMgr.h"
#include "../../DuiDpi.h"

namespace balloonwjui {

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

    // Build, size, and show the popup at the given screen position.
    void Show(POINT screenPt, const CString& text)
    {
        m_text = text;
        if (m_text.IsEmpty())
        {
            return;
        }

        // 提示框窗口尚未创建，按将要弹出的位置所在显示器的 DPI 取字体；
        // 测量与绘制都用这一个 DPI，保证尺寸与文字一致。
        m_dpi = DuiDpi::GetDpiForPoint(screenPt);

        // Measure text in the default DUI font (Microsoft YaHei).
        SIZE sz = MeasureText(m_text);
        const int padX = 8, padY = 4;
        int w = sz.cx + padX * 2;
        int h = sz.cy + padY * 2;

        // Anchor near the cursor (slightly below and right).
        int x = screenPt.x + 12;
        int y = screenPt.y + 18;

        Create(NULL, NULL, NULL,
               WS_POPUP,
               WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE);
        if (!m_hWnd)
        {
            return;
        }

        SetWindowPos(NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(SW_SHOWNOACTIVATE);
    }

private:
    BOOL OnEraseBkgnd(CDCHandle dc)
    {
        CRect rc;
        GetClientRect(&rc);
        HBRUSH bg = ::CreateSolidBrush(RGB(255, 255, 225));   // pale yellow tooltip color
        ::FillRect(dc, &rc, bg);
        ::DeleteObject(bg);
        HPEN pen = ::CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
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
        COLORREF oldClr = ::SetTextColor(dc, RGB(40, 40, 40));
        rc.DeflateRect(8, 4);
        //单行与多行（文字含换行符）用不同的格式标志，与 MeasureText 的测量方式一致
        ::DrawText(dc, m_text, -1, &rc, DuiToolTipMgr::TextDrawFlags(m_text));
        ::SetTextColor(dc, oldClr);
        ::SetBkMode(dc, oldBk);
        if (oldFont)
        {
            ::SelectObject(dc, oldFont);
        }
    }

    SIZE MeasureText(const CString& text) const
    {
        HDC hdc = ::GetDC(nullptr);
        HFONT useFont = DuiResMgr::Inst().GetDefaultFontForDpi(m_dpi);
        HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
        const SIZE sz = DuiToolTipMgr::MeasureTextExtent(hdc, text);
        if (oldFont)
        {
            ::SelectObject(hdc, oldFont);
        }
        ::ReleaseDC(nullptr, hdc);
        return sz;
    }

public:
    CString GetText() const { return m_text; }

private:
    CString m_text;
    int     m_dpi = 0;   // Show 时按弹出位置所在显示器取的 DPI；0 = 尚未 Show，按全局 DPI 处理
};

// =====================================================================
// DuiToolTipMgr
// =====================================================================

DuiToolTipMgr& DuiToolTipMgr::Inst()
{
    static DuiToolTipMgr s_inst;
    return s_inst;
}

UINT DuiToolTipMgr::TextDrawFlags(const CString& text)
{
    const UINT flags = DT_LEFT | DT_TOP;
    //含换行符时按多行排版，DrawText 遇到换行符会另起一行
    if (text.Find(_T('\n')) >= 0)
    {
        return flags;
    }
    return flags | DT_SINGLELINE;
}

SIZE DuiToolTipMgr::MeasureTextExtent(HDC hdc, const CString& text)
{
    SIZE sz = { 0, 0 };
    if (text.IsEmpty())
    {
        return sz;
    }
    const UINT flags = TextDrawFlags(text);
    if ((flags & DT_SINGLELINE) != 0)
    {
        ::GetTextExtentPoint32(hdc, text, text.GetLength(), &sz);
        return sz;
    }
    //多行：DT_CALCRECT 只计算、不绘制，算出的矩形宽为最长一行、高为行数 × 行高
    RECT rc = { 0, 0, 0, 0 };
    ::DrawText(hdc, text, text.GetLength(), &rc, flags | DT_CALCRECT);
    sz.cx = rc.right - rc.left;
    sz.cy = rc.bottom - rc.top;
    return sz;
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
    if (m_pending == ctrl)
    {
        m_pending = nullptr;
        StopTimer();
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
    if (!ctrl)
    {
        OnLeave(nullptr);
        return;
    }
    CString s = GetText(ctrl);
    if (s.IsEmpty())                   // no tooltip registered for this ctrl
    {
        HideNow();
        m_pending = nullptr;
        StopTimer();
        return;
    }
    if (m_pending == ctrl)
    {
        return;     // already pending or shown for this ctrl
    }
    HideNow();
    m_pending = ctrl;
    m_pendingPt = screenPt;
    StartTimer();
}

void DuiToolTipMgr::OnLeave(DuiControl* ctrl)
{
    // Leave only matters if it's the control we were tracking.
    if (ctrl != nullptr && ctrl != m_pending && (!m_window))
    {
        return;
    }
    m_pending = nullptr;
    StopTimer();
    HideNow();
}

void DuiToolTipMgr::HideNow()
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

void DuiToolTipMgr::StartTimer()
{
    StopTimer();
    m_timerId = ::SetTimer(nullptr, 0, m_delayMs, &DuiToolTipMgr::TimerProc);
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
    if (m_window)
    {
        HideNow();           // safety
    }
    m_window = new DuiToolTipWnd();
    m_window->Show(m_pendingPt, text);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_TOOLTIP
