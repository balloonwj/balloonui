#include "stdafx.h"
#include "DuiEditHost.h"

#if BUI_FEATURE_EDIT

#include "../../DuiHost.h"
#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"
#include "EditContextMenu.h"            // 右键菜单的纯逻辑模型
#if defined(BUI_FEATURE_MENU)
#include "../List/DuiMenu.h"            // 自绘弹出菜单（仅启用 MENU 特性时用）
#endif

#include <gdiplus.h>
#include <windowsx.h>                   // GET_X_LPARAM / GET_Y_LPARAM
#include <commctrl.h>                   // SetWindowSubclass / DefSubclassProc / RemoveWindowSubclass
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

namespace balloonwjui {

// Eye toggle visual constants.
namespace {
    const int kEyeBoxW   = 22;          // reserved width on right side of EDIT
    const int kEyeIconW  = 18;          // visible eye icon width
    const int kEyeIconH  = 12;          // visible eye icon height

    // Border colors. The DUI host paints a 1px frame around the OS EDIT;
    // the color tracks input state so the user can see focus / hover /
    // disabled at a glance even though the EDIT itself doesn't recolor.
    const COLORREF kBorderDisabled = RGB(190, 190, 190);   // muted gray, low contrast
    const COLORREF kBorderFocused  = RGB( 80, 130, 200);   // saturated blue when typing
    const COLORREF kBorderHover    = RGB(100, 140, 200);   // slightly lighter than focused
    const COLORREF kBorderNormal   = RGB(150, 150, 150);   // resting medium gray

    // Background fill behind the host strip (1px outside the EDIT). Disabled
    // gets a flat light gray; enabled is system white. We don't extract pure
    // white here since it appears once and reads as "white" at the call site.
    const COLORREF kBgDisabled     = RGB(240, 240, 240);

    // Placeholder text color - matches Windows native placeholder gray so
    // the prompt is clearly distinguishable from real user-typed content.
    const COLORREF kPlaceholderText = RGB(160, 160, 160);

    // Eye toggle (password reveal) icon colors. Hover uses the brand blue
    // documented in CLAUDE.md PushButton style; resting state is medium
    // gray so it doesn't compete with the EDIT's own caret/text.
    const COLORREF kEyeIconHover   = RGB( 45, 108, 223);   // brand blue (#2D6CDF)
    const COLORREF kEyeIconNormal  = RGB(110, 110, 110);   // medium gray
}

namespace {

    // 以 4 倍超采样绘制"睁开的眼睛"图标（椭圆轮廓 + 居中瞳孔），再用
    // 高质量双三次插值缩回目标尺寸 —— 直接在 18x12 这种小尺寸上画抗锯齿
    // 椭圆边缘仍会显得粗糙，超采样后缩小可消除这种锯齿感。
    void DrawEyeIcon(HDC hdc, const RECT& rc, COLORREF clr)
    {
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0)
        {
            return;
        }

        const int kSuperSample = 4;   // 超采样倍数
        Gdiplus::Bitmap bmp(w * kSuperSample, h * kSuperSample,
                            PixelFormat32bppARGB);
        {
            Gdiplus::Graphics gb(&bmp);
            gb.Clear(Gdiplus::Color(0, 0, 0, 0));
            gb.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            gb.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

            const Gdiplus::Color gc(255, GetRValue(clr), GetGValue(clr),
                                    GetBValue(clr));
            // 眼睛椭圆轮廓。
            Gdiplus::Pen pen(gc, 1.5f * kSuperSample);
            gb.DrawEllipse(&pen,
                           1.0f * kSuperSample, 1.0f * kSuperSample,
                           (Gdiplus::REAL)(w - 2) * kSuperSample,
                           (Gdiplus::REAL)(h - 2) * kSuperSample);
            // 居中瞳孔。
            Gdiplus::SolidBrush brush(gc);
            const Gdiplus::REAL pupilR = 2.6f * kSuperSample;
            gb.FillEllipse(&brush,
                           (Gdiplus::REAL)(w * kSuperSample) / 2.0f - pupilR,
                           (Gdiplus::REAL)(h * kSuperSample) / 2.0f - pupilR,
                           pupilR * 2.0f, pupilR * 2.0f);
        }

        Gdiplus::Graphics g(hdc);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.DrawImage(&bmp,
                    (Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.top,
                    (Gdiplus::REAL)w, (Gdiplus::REAL)h);
    }

    // Use a high range for child control ids assigned to DuiEditHost EDITs
    // to avoid colliding with WTL/Resource-based control ids in the parent.
    UINT NextEditCtrlId()
    {
        static UINT s_next = 0xE000;
        return s_next++;
    }

    // 右键菜单子类过程的 uIdSubclass（与 DuiRichEditHost 的 0xB011 区分开，
    // 便于 trace 时分辨是哪个控件的子类过程）。
    const UINT_PTR kCtxMenuSubclassId = 0xB012;

    // 量出指定 EDIT 当前字体的单行像素高（tmHeight = 字符 ascent + descent）。
    // 供单行 EDIT 文字垂直居中用：优先取 HWND 上 WM_SETFONT 设过的字体，没设过
    // 则退回 DuiResMgr 默认字体，在临时 DC 里 GetTextMetrics 量取。测不到返回 0。
    //   h：有效的 EDIT 子 HWND。
    int MeasureEditLineHeight(HWND h)
    {
        HFONT hFont = reinterpret_cast<HFONT>(::SendMessage(h, WM_GETFONT, 0, 0));
        if (hFont == nullptr)
        {
            hFont = DuiResMgr::Inst().GetDefaultFont();
        }
        HDC hdc = ::GetDC(h);
        if (hdc == nullptr)
        {
            return 0;
        }
        HFONT old = nullptr;
        if (hFont != nullptr)
        {
            old = reinterpret_cast<HFONT>(::SelectObject(hdc, hFont));
        }
        TEXTMETRIC tm;
        ::ZeroMemory(&tm, sizeof(tm));
        int lineH = 0;
        if (::GetTextMetrics(hdc, &tm))
        {
            lineH = tm.tmHeight;
        }
        if (old != nullptr)
        {
            ::SelectObject(hdc, old);
        }
        ::ReleaseDC(h, hdc);
        return lineH;
    }
}

DuiEditHost::DuiEditHost()
{
    SetTabStop(true);
}

DuiEditHost::~DuiEditHost()
{
    // 派生类析构先于基类（基类 ~HwndHostControl 才销毁 EDIT 子 HWND），此处
    // 先摘掉右键菜单子类过程，避免子类过程在 this 已半析构时被触达。
    RemoveContextMenuSubclass();

    // 释放 SetCtlFont 建的自定义字体。
    if (m_customFont)
    {
        ::DeleteObject(m_customFont);
        m_customFont = nullptr;
    }
}

void DuiEditHost::SetMultiLine(bool b)
{
    if (m_multiLine == b)
    {
        return;
    }
    m_multiLine = b;
    if (HWND h = GetHostedHwnd())
    {
        // Recreate to honor the new style. Caller must re-Layout.
        HWND parent = ::GetParent(h);
        ::DestroyWindow(h);
        Detach();
        if (parent)
        {
            EnsureCreated(parent);
        }
    }
}

void DuiEditHost::SetWordWrap(bool b)
{
    if (m_wordWrap == b)
    {
        return;
    }
    m_wordWrap = b;
    if (HWND h = GetHostedHwnd())
    {
        // ES_AUTOHSCROLL / WS_VSCROLL 是创建期 baked-in 风格位,改了须销毁重建。
        HWND parent = ::GetParent(h);
        ::DestroyWindow(h);
        Detach();
        if (parent)
        {
            EnsureCreated(parent);
        }
    }
}

void DuiEditHost::SetPassword(bool b)
{
    if (m_password == b)
    {
        return;
    }
    m_password = b;
    if (HWND h = GetHostedHwnd())
    {
        HWND parent = ::GetParent(h);
        ::DestroyWindow(h);
        Detach();
        if (parent)
        {
            EnsureCreated(parent);
        }
    }
}

bool DuiEditHost::EnsureCreated(HWND hwndParent)
{
    if (GetHostedHwnd())
    {
        return true;
    }
    if (!hwndParent || !::IsWindow(hwndParent))
    {
        return false;
    }

    // 横向滚动(ES_AUTOHSCROLL)对单行恒需;多行时仅在"不自动换行"才加 ——
    // 普通 EDIT 一旦带 ES_AUTOHSCROLL 就横向滚动、不按宽度换行。多行 + 自动
    // 换行时改加 WS_VSCROLL,让内容超过框高时出现竖直滚动条。
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (m_multiLine)
    {
        style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
        if (m_wordWrap)
        {
            style |= WS_VSCROLL;          // 自动换行 + 溢出竖直滚动条
        }
        else
        {
            style |= ES_AUTOHSCROLL;      // 不换行,横向滚动(历史行为)
        }
    }
    else
    {
        style |= ES_AUTOHSCROLL;          // 单行横向滚动
    }
    if (m_password)
    {
        style |= ES_PASSWORD;
    }
    if (m_readonly)
    {
        style |= ES_READONLY;
    }

    if (m_ctrlIdHwnd == 0)
    {
        m_ctrlIdHwnd = NextEditCtrlId();
    }

    RECT rc = m_rcItem;
    // Apply margins so the EDIT sits inside our 1px border + margins.
    ::InflateRect(&rc, -1, -1);
    rc.left   += m_marginL;
    rc.top    += m_marginT;
    rc.right  -= m_marginR;
    rc.bottom -= m_marginB;
    if (rc.right < rc.left)
    {
        rc.right = rc.left;
    }
    if (rc.bottom < rc.top)
    {
        rc.bottom = rc.top;
    }

    HWND h = ::CreateWindowEx(
        0,
        _T("EDIT"),
        m_cachedText,
        style,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        hwndParent,
        (HMENU)(UINT_PTR)m_ctrlIdHwnd,
        ::GetModuleHandle(nullptr),
        nullptr);
    if (!h)
    {
        return false;
    }

    // 字体：优先用 SetCtlFont 设过的自定义字体；否则用 DUI 默认（微软雅黑），
    // 保证用户键入的中文正常渲染。
    HFONT hFont = m_customFont ? m_customFont : DuiResMgr::Inst().GetDefaultFont();
    if (!hFont)
    {
        hFont = (HFONT)::SendMessage(hwndParent, WM_GETFONT, 0, 0);
    }
    if (hFont)
    {
        ::SendMessage(h, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(FALSE, 0));
    }

    if (m_maxLen > 0)
    {
        ::SendMessage(h, EM_SETLIMITTEXT, (WPARAM)m_maxLen, 0);
    }

    Attach(h);
    // Attach 之后 GetHostedHwnd() 才返回真 HWND；SyncPlaceholderToHwnd 才能把
    // EnsureCreated 之前调过的 SetPlaceholder 真正应用到新 EDIT。
    SyncPlaceholderToHwnd();
    // 给新建的 EDIT 子 HWND 挂右键菜单子类过程（SetMultiLine / SetPassword 等
    // 触发的销毁重建也会再次走到这里重挂；内部按菜单开关与 MENU 特性判断）。
    InstallContextMenuSubclass();
    return true;
}

void DuiEditHost::SetCtlFont(LPCTSTR family, int sizePx,
                             bool bold, bool italic, bool underline, bool strike)
{
    if (sizePx <= 0)
    {
        sizePx = 14;
    }
    LPCTSTR face = (family && family[0] != _T('\0')) ? family : _T("Microsoft YaHei");

    HFONT hNew = ::CreateFont(
        -sizePx, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        italic ? TRUE : FALSE,
        underline ? TRUE : FALSE,
        strike ? TRUE : FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    if (!hNew)
    {
        return;
    }

    if (m_customFont)
    {
        ::DeleteObject(m_customFont);
    }
    m_customFont = hNew;

    // EDIT 已创建则即时 WM_SETFONT（带重绘）；否则等 EnsureCreated 时采用 m_customFont。
    HWND h = GetHostedHwnd();
    if (h && ::IsWindow(h))
    {
        ::SendMessage(h, WM_SETFONT, (WPARAM)m_customFont, MAKELPARAM(TRUE, 0));
    }
    Invalidate();
}

void DuiEditHost::SetText(LPCTSTR sz)
{
    // 文本相对当前缓存未变化时直接返回，避免无谓的 SetWindowText + Invalidate，
    // 与 SetRect（EqualRect）/ SetCtlTextColor / SetCtlBgColor（颜色未变即 return）
    // 等兄弟 setter 的"未变化则不失效"幂等语义对齐。否则调用方若在每帧 OnPaint
    // 里调 SetText（哪怕传入文本与当前一致），每次都会把本控件矩形标脏、令宿主据
    // 此再次重绘，形成自激重绘死循环（值文本被持续 erase / redraw 而高频闪烁不可见）。
    CString next = sz ? sz : _T("");
    if (m_cachedText == next)
    {
        return;
    }
    m_cachedText = next;
    SyncTextToHwnd();
    Invalidate();
}

CString DuiEditHost::GetText() const
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return m_cachedText;
    }
    int n = ::GetWindowTextLength(h);
    if (n <= 0)
    {
        return CString();
    }
    CString s;
    LPTSTR buf = s.GetBufferSetLength(n);
    ::GetWindowText(h, buf, n + 1);
    s.ReleaseBuffer();
    return s;
}

void DuiEditHost::SetReadOnly(bool b)
{
    m_readonly = b;
    SyncReadOnlyToHwnd();
}

// ============================================================
// 标准编辑命令 + 右键菜单
// ============================================================

void DuiEditHost::Cut()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, WM_CUT, 0, 0);
    }
}

void DuiEditHost::Copy()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, WM_COPY, 0, 0);
    }
}

void DuiEditHost::Paste()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, WM_PASTE, 0, 0);
    }
}

void DuiEditHost::SelectAll()
{
    if (HWND h = GetHostedHwnd())
    {
        // EM_SETSEL(0, -1)：从头选到尾，即全选。
        ::SendMessage(h, EM_SETSEL, 0, (LPARAM)-1);
    }
}

void DuiEditHost::SetContextMenuEnabled(bool b)
{
    if (m_contextMenuEnabled == b)
    {
        return;
    }
    m_contextMenuEnabled = b;
    // EDIT 已建则即时生效：开 → 挂子类过程；关 → 摘掉退回原生菜单。EDIT 未建
    // 时只记标志，等 EnsureCreated 时按标志决定是否安装。
    if (GetHostedHwnd())
    {
        if (b)
        {
            InstallContextMenuSubclass();
        }
        else
        {
            RemoveContextMenuSubclass();
        }
    }
}

#if defined(BUI_FEATURE_MENU)

void DuiEditHost::InstallContextMenuSubclass()
{
    if (!m_contextMenuEnabled)
    {
        return;
    }
    HWND h = GetHostedHwnd();
    if (h == nullptr || !::IsWindow(h))
    {
        return;
    }
    // SetWindowSubclass 对"同一 proc + 同一 uId"幂等：已挂则仅更新 refData。
    if (::SetWindowSubclass(h, &DuiEditHost::ContextMenuSubclassProc,
                            kCtxMenuSubclassId,
                            reinterpret_cast<DWORD_PTR>(this)))
    {
        m_ctxSubclassed = true;
    }
}

void DuiEditHost::RemoveContextMenuSubclass()
{
    if (!m_ctxSubclassed)
    {
        return;
    }
    HWND h = GetHostedHwnd();
    if (h != nullptr && ::IsWindow(h))
    {
        ::RemoveWindowSubclass(h, &DuiEditHost::ContextMenuSubclassProc,
                               kCtxMenuSubclassId);
    }
    m_ctxSubclassed = false;
}

LRESULT CALLBACK DuiEditHost::ContextMenuSubclassProc(HWND hwnd, UINT msg,
                                                      WPARAM wParam, LPARAM lParam,
                                                      UINT_PTR /*uIdSubclass*/,
                                                      DWORD_PTR dwRefData)
{
    DuiEditHost* self = reinterpret_cast<DuiEditHost*>(dwRefData);
    if (msg == WM_CONTEXTMENU && self != nullptr && self->m_contextMenuEnabled)
    {
        // lParam 为屏幕坐标；键盘触发（Shift+F10 / 菜单键）时为 (-1, -1)，
        // 此时取控件中心作为弹出点。
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        if (pt.x == -1 && pt.y == -1)
        {
            RECT rc;
            ::GetWindowRect(hwnd, &rc);
            pt.x = rc.left + (rc.right - rc.left) / 2;
            pt.y = rc.top + (rc.bottom - rc.top) / 2;
        }
        if (self->ShowContextMenu(pt))
        {
            return 0;       // 已弹自绘菜单：消费消息，压掉原生菜单
        }
    }
    return ::DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool DuiEditHost::ShowContextMenu(POINT screenPt)
{
    HWND h = GetHostedHwnd();
    if (h == nullptr || !::IsWindow(h))
    {
        return false;
    }

    // ---- 读取文本框当前状态 ----
    EditContextState state;
    state.m_readOnly   = m_readonly;
    state.m_isPassword = m_password;
    state.m_hasText    = (::GetWindowTextLength(h) > 0);

    // 选区：EM_GETSEL 取起止字符位置，start != end 即有非空选区。
    DWORD selStart = 0;
    DWORD selEnd   = 0;
    ::SendMessage(h, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    state.m_hasSelection = (selStart != selEnd);

    // 剪贴板有无文本：CF_UNICODETEXT / CF_TEXT 任一可用即可粘贴。
    state.m_clipboardHasText =
        (::IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE) ||
        (::IsClipboardFormatAvailable(CF_TEXT) != FALSE);

    // ---- 构建模型并填充 DuiMenu ----
    std::vector<EditContextMenuItem> model = BuildEditContextMenu(state);
    DuiMenu menu;
    for (std::vector<EditContextMenuItem>::const_iterator it = model.begin();
         it != model.end(); ++it)
    {
        if (it->m_cmd == EditCtxCmd_Separator)
        {
            menu.AppendSeparator();
        }
        else if (it->m_enabled)
        {
            menu.AppendItem((UINT)it->m_cmd, EditContextCommandLabel(it->m_cmd));
        }
        else
        {
            menu.AppendDisabled((UINT)it->m_cmd, EditContextCommandLabel(it->m_cmd));
        }
    }

    // ownerHwnd 用 EDIT 的父窗口（即 DUI host 窗口），与 codebase 里其它
    // TrackPopup 调用约定一致（菜单关掉后焦点回该窗口）。
    HWND owner = ::GetParent(h);
    if (owner == nullptr)
    {
        owner = h;
    }
    UINT chosen = menu.TrackPopup(screenPt.x, screenPt.y, owner);

    // 菜单期间焦点在弹出窗口上，关闭后 TrackPopup 把焦点交回的是 owner（宿主
    // 窗口）而非 EDIT 子窗口。执行命令前先把焦点交回 EDIT —— 否则"全选"虽然
    // 用 EM_SETSEL 选中了，但 Win32 EDIT 默认无 ES_NOHIDESEL，失焦时不画选区
    // 高亮（粘贴的光标位置同理需要 EDIT 持有焦点）。
    if (chosen != 0)
    {
        ::SetFocus(h);
    }

    switch (chosen)
    {
    case EditCtxCmd_Cut:
        Cut();
        break;
    case EditCtxCmd_Copy:
        Copy();
        break;
    case EditCtxCmd_Paste:
        Paste();
        break;
    case EditCtxCmd_SelectAll:
        SelectAll();
        break;
    default:
        // 0 = 用户没选（点空白 / ESC dismiss）；无操作。
        break;
    }
    return true;
}

#else  // !BUI_FEATURE_MENU

// MENU 特性被裁掉时不挂子类过程，右键退回 OS 原生 EDIT 菜单；下列为空实现，
// 仅为满足声明（InstallContextMenuSubclass 不会被有效调用）。
void DuiEditHost::InstallContextMenuSubclass()
{
}

void DuiEditHost::RemoveContextMenuSubclass()
{
}

LRESULT CALLBACK DuiEditHost::ContextMenuSubclassProc(HWND hwnd, UINT msg,
                                                      WPARAM wParam, LPARAM lParam,
                                                      UINT_PTR /*uIdSubclass*/,
                                                      DWORD_PTR /*dwRefData*/)
{
    return ::DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool DuiEditHost::ShowContextMenu(POINT /*screenPt*/)
{
    return false;
}

#endif // BUI_FEATURE_MENU

void DuiEditHost::SetMaxLength(int n)
{
    m_maxLen = (n < 0) ? 0 : n;
    SyncMaxLengthToHwnd();
}

void DuiEditHost::SetMargins(int left, int top, int right, int bottom)
{
    m_marginL = left;
    m_marginT = top;
    m_marginR = right;
    m_marginB = bottom;
    Layout(m_rcItem);
    Invalidate();
}

void DuiEditHost::SetPlaceholder(LPCTSTR sz)
{
    m_placeholder = sz ? sz : _T("");
    SyncPlaceholderToHwnd();
    Invalidate();
}

void DuiEditHost::SetShowPlaceholder(bool b)
{
    m_showPlaceholder = b;
    SyncPlaceholderToHwnd();
    Invalidate();
}

bool DuiEditHost::IsShowingPlaceholder() const
{
    return m_showPlaceholder
        && !m_placeholder.IsEmpty()
        && m_cachedText.IsEmpty()
        && !m_bFocused;
}

void DuiEditHost::SyncReadOnlyToHwnd()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, EM_SETREADONLY, m_readonly ? TRUE : FALSE, 0);
    }
}

void DuiEditHost::SyncTextToHwnd()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SetWindowText(h, m_cachedText);
    }
}

void DuiEditHost::SyncMaxLengthToHwnd()
{
    if (HWND h = GetHostedHwnd())
    {
        ::SendMessage(h, EM_SETLIMITTEXT, (WPARAM)m_maxLen, 0);
    }
}

void DuiEditHost::SyncPlaceholderToHwnd()
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    // m_showPlaceholder=false 等价于"清空 cue"，cue 显示由 OS 在 EDIT empty
    // 时自动生效。第二参 FALSE = 获焦时消失（与 Win32 EDIT 默认 cue 行为一致）。
    // EM_SETCUEBANNER lParam 必须是 wchar_t* —— 项目是 _UNICODE 编译，
    // CString 是 wchar_t 串，可直接 cast。
    LPCWSTR cue = m_showPlaceholder ? (LPCWSTR)(LPCTSTR)m_placeholder : L"";
    ::SendMessageW(h, EM_SETCUEBANNER, FALSE, (LPARAM)cue);
}

void DuiEditHost::RefreshCacheFromHwnd()
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    int n = ::GetWindowTextLength(h);
    if (n <= 0)
    {
        m_cachedText.Empty();
        return;
    }
    LPTSTR buf = m_cachedText.GetBufferSetLength(n);
    ::GetWindowText(h, buf, n + 1);
    m_cachedText.ReleaseBuffer();
}

void DuiEditHost::OnEditNotify(UINT enCode)
{
    switch (enCode)
    {
    case EN_CHANGE:
        // Refresh cached text from HWND so GetText() and IsShowingPlaceholder()
        // stay correct without needing another GetWindowText round-trip.
        RefreshCacheFromHwnd();
        Invalidate();
        NotifyParent(DUIN_VALUECHANGED, 0);
        break;
    case EN_SETFOCUS:
        m_bFocused = true;
        Invalidate();
        NotifyParent(DUIN_SETFOCUS);
        break;
    case EN_KILLFOCUS:
        m_bFocused = false;
        Invalidate();
        NotifyParent(DUIN_KILLFOCUS);
        break;
    default:
        break;
    }
}

void DuiEditHost::Layout(const RECT& rcAvail)
{
    // Set our rect; HwndHostControl::Layout would resize the EDIT to fill
    // m_rcItem exactly, but we want margins + border, so override.
    m_rcItem = rcAvail;

    // Lazy-create the underlying EDIT HWND on first Layout once a host
    // is attached. This relieves callers (Pages.cpp / chat dialogs) from
    // having to track when SetRoot has run and call EnsureCreated by hand.
    if (!GetHostedHwnd() && m_pHost && m_pHost->m_hWnd && ::IsWindow(m_pHost->m_hWnd))
    {
        EnsureCreated(m_pHost->m_hWnd);
    }

    if (HWND h = GetHostedHwnd())
    {
        RECT inner = m_rcItem;
        ::InflateRect(&inner, -1, -1);     // skip 1px border
        inner.left   += m_marginL;
        inner.top    += m_marginT;
        inner.right  -= m_marginR;
        inner.bottom -= m_marginB;
        // If the eye toggle is visible, leave a kEyeBoxW strip on the
        // right so clicks there land on the DUI control (not the EDIT).
        if (EyeVisible())
        {
            inner.right -= kEyeBoxW;
        }
        // 内联图标 gutter —— EDIT 文字区收缩对应宽度，让 OnPaint 在外侧
        // 画图标。eye toggle 与 right icon 同时启用时，eye 优先（已经
        // 减过 kEyeBoxW；这里如再减一次会把 EDIT 挤到极小）—— 把右 icon
        // 在这种 corner case 下视为 0 宽（painter 不画）。
        inner.left  += m_iconL.width;
        if (!EyeVisible())
        {
            inner.right -= m_iconR.width;
        }
        if (inner.right  < inner.left)
        {
            inner.right  = inner.left;
        }
        if (inner.bottom < inner.top)
        {
            inner.bottom = inner.top;
        }
        // 单行 EDIT 文字垂直居中（按需开启，见 SetVerticalCenter）：Win32 单行
        // EDIT 的文字恒在客户区【顶部】对齐，框比一行高时下方会留白。开关打开后
        // 把寄宿窗口收成约一行高、在可用纵向区间内居中放置，使文字 / 光标视觉居中。
        // 多行 EDIT 不处理（文字本就自顶向下填充整框）。
        if (m_vCenter && !m_multiLine)
        {
            const int lineH  = MeasureEditLineHeight(h);
            const int availH = inner.bottom - inner.top;
            if (lineH > 0 && lineH < availH)
            {
                const int top = inner.top + (availH - lineH) / 2;
                inner.top    = top;
                inner.bottom = top + lineH;
            }
        }
        ::SetWindowPos(h, nullptr,
                       inner.left, inner.top,
                       inner.right - inner.left,
                       inner.bottom - inner.top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

RECT DuiEditHost::EyeRect() const
{
    if (!EyeVisible())
    {
        return RECT{ 0, 0, 0, 0 };
    }
    int right = m_rcItem.right - 1 - m_marginR;     // skip border + right margin
    int left  = right - kEyeBoxW + (kEyeBoxW - kEyeIconW) / 2;
    int top   = (m_rcItem.top + m_rcItem.bottom - kEyeIconH) / 2;
    int rightIcon = left + kEyeIconW;
    return RECT{ left, top, rightIcon, top + kEyeIconH };
}

COLORREF DuiEditHost::BorderColor() const
{
    if (!m_bEnabled)
    {
        return kBorderDisabled;
    }
    if (m_bFocused)
    {
        return kBorderFocused;
    }
    if (m_bHover)
    {
        return kBorderHover;
    }
    return kBorderNormal;
}

void DuiEditHost::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    // Background outside the EDIT (tiny strip given our margins) —— 用
    // SetBgColor 配置的色（默认白）。enabled=false 时仍走灰色 disabled
    // 底，独立于 m_bgColor。
    HBRUSH hbr = ::CreateSolidBrush(m_bEnabled ? m_bgColor : kBgDisabled);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    // Border —— 由 SetShowBorder 控制，默认 true 兼容旧行为
    if (m_showBorder)
    {
        HPEN hpen = ::CreatePen(PS_SOLID, 1, BorderColor());
        HPEN oldPen = (HPEN)::SelectObject(hdc, hpen);
        HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom);
        ::SelectObject(hdc, oldBr);
        ::SelectObject(hdc, oldPen);
        ::DeleteObject(hpen);
    }

    // Placeholder 由 EDIT 自己通过 EM_SETCUEBANNER 画 —— 见
    // SyncPlaceholderToHwnd。这里曾经在 host backbuffer 画 placeholder，
    // 但 EDIT 是 child HWND，OS 在 host paint 之后才让 EDIT 画自己（白
    // 底）→ EDIT 的白底必然覆盖 host 画的 placeholder，从来都看不见。
    // IsShowingPlaceholder() 仍然保留作为<u>状态查询</u> API（caller 可
    // 用来决定别的 UI 行为，例如"无字时弱化清除按钮"），但不再驱动绘制。

    // 内联图标 —— left icon 在 eye toggle 之前画；right icon 仅在
    // eye toggle 关闭时画（互斥，见 Layout 注释）。
    if (m_iconL.painter && m_iconL.width > 0)
    {
        RECT rcL = ComputeIconRect(m_rcItem, LeftIcon,
                                   m_iconL.width, 1, m_marginT);
        m_iconL.painter(hdc, rcL);
    }
    if (!EyeVisible() && m_iconR.painter && m_iconR.width > 0)
    {
        RECT rcR = ComputeIconRect(m_rcItem, RightIcon,
                                   m_iconR.width, 1, m_marginT);
        m_iconR.painter(hdc, rcR);
    }

    // Eye toggle button.
    if (EyeVisible())
    {
        RECT eye = EyeRect();
        COLORREF clr = m_eyeHover ? kEyeIconHover : kEyeIconNormal;
        // 始终画一只普通"睁开的眼睛"（椭圆轮廓 + 居中瞳孔）。明文 / 密文
        // 两种状态都画成睁眼（与设计稿一致），不用斜杠表示隐藏态；用
        // 超采样绘制以消除小图标边缘锯齿。
        DrawEyeIcon(hdc, eye, clr);
    }
}

bool DuiEditHost::OnSetCursor(POINT pt)
{
    if (!m_bEnabled)
    {
        return false;
    }
    if (EyeVisible() && ::PtInRect(&EyeRect(), pt))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
        return true;
    }
    // 可点击图标 → 手形指针
    if (m_iconL.clickable && m_iconL.width > 0)
    {
        RECT r = ComputeIconRect(m_rcItem, LeftIcon,
                                 m_iconL.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
            return true;
        }
    }
    if (m_iconR.clickable && m_iconR.width > 0 && !EyeVisible())
    {
        RECT r = ComputeIconRect(m_rcItem, RightIcon,
                                 m_iconR.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
            return true;
        }
    }
    // 装了拖拽回调的 gutter（如可拖滚动条）：用普通箭头光标，别显示文本工字形。
    if (m_iconR.dragHandler && m_iconR.width > 0 && !EyeVisible())
    {
        RECT r = ComputeIconRect(m_rcItem, RightIcon, m_iconR.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
            return true;
        }
    }
    if (m_iconL.dragHandler && m_iconL.width > 0)
    {
        RECT r = ComputeIconRect(m_rcItem, LeftIcon, m_iconL.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
            return true;
        }
    }

    // The OS already shows IDC_IBEAM over the EDIT child window. The DUI
    // host only sees cursor queries for the strip outside the EDIT (1px
    // border + margins). Show I-beam there too so the cursor is consistent
    // across the whole text-input rect.
    ::SetCursor(::LoadCursor(nullptr, IDC_IBEAM));
    return true;
}

bool DuiEditHost::OnLButtonDown(POINT pt, UINT)
{
    // gutter 拖拽：命中某侧装了 dragHandler 的 gutter 区 → 进入拖拽态、Capture
    // 鼠标，并回调一次按下。gutter 在 EDIT 子窗之外，故这些按下都落到本控件。
    if (m_iconR.dragHandler && m_iconR.width > 0 && !EyeVisible())
    {
        RECT r = ComputeIconRect(m_rcItem, RightIcon, m_iconR.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            m_dragSlot = RightIcon;
            Capture();
            m_iconR.dragHandler(WM_LBUTTONDOWN, pt, r);
            return true;
        }
    }
    if (m_iconL.dragHandler && m_iconL.width > 0)
    {
        RECT r = ComputeIconRect(m_rcItem, LeftIcon, m_iconL.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            m_dragSlot = LeftIcon;
            Capture();
            m_iconL.dragHandler(WM_LBUTTONDOWN, pt, r);
            return true;
        }
    }
    return false;
}

bool DuiEditHost::OnLButtonUp(POINT pt, UINT)
{
    // gutter 拖拽结束：回调一次抬起、释放捕获。
    if (m_dragSlot != -1)
    {
        IconSlot slot = (m_dragSlot == LeftIcon) ? LeftIcon : RightIcon;
        IconState& st = (slot == LeftIcon) ? m_iconL : m_iconR;
        m_dragSlot = -1;
        ReleaseCapture();
        if (st.dragHandler)
        {
            RECT r = ComputeIconRect(m_rcItem, slot, st.width, 1, m_marginT);
            st.dragHandler(WM_LBUTTONUP, pt, r);
        }
        return true;
    }

    if (EyeVisible() && ::PtInRect(&EyeRect(), pt))
    {
        SetPasswordRevealed(!m_pwdRevealed);
        return true;
    }
    if (m_iconL.clickable && m_iconL.width > 0)
    {
        RECT r = ComputeIconRect(m_rcItem, LeftIcon,
                                 m_iconL.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            // 子类可截：返 true 不发通知（自吞）
            if (!OnIconClicked_(LeftIcon))
            {
                NotifyParent(DUIEN_LEFT_ICON_CLICK, 0);
            }
            return true;
        }
    }
    if (m_iconR.clickable && m_iconR.width > 0 && !EyeVisible())
    {
        RECT r = ComputeIconRect(m_rcItem, RightIcon,
                                 m_iconR.width, 1, m_marginT);
        if (::PtInRect(&r, pt))
        {
            if (!OnIconClicked_(RightIcon))
            {
                NotifyParent(DUIEN_RIGHT_ICON_CLICK, 0);
            }
            return true;
        }
    }
    return false;
}

bool DuiEditHost::OnMouseMove(POINT pt, UINT)
{
    // gutter 拖拽进行中：把移动转给拖拽回调。
    if (m_dragSlot != -1)
    {
        IconSlot slot = (m_dragSlot == LeftIcon) ? LeftIcon : RightIcon;
        IconState& st = (slot == LeftIcon) ? m_iconL : m_iconR;
        if (st.dragHandler)
        {
            RECT r = ComputeIconRect(m_rcItem, slot, st.width, 1, m_marginT);
            st.dragHandler(WM_MOUSEMOVE, pt, r);
        }
        return true;
    }

    bool nowOver = EyeVisible() && ::PtInRect(&EyeRect(), pt) ? true : false;
    if (nowOver != m_eyeHover)
    {
        m_eyeHover = nowOver;
        Invalidate();
    }
    return false;
}

bool DuiEditHost::OnMouseLeave()
{
    if (m_eyeHover)
    {
        m_eyeHover = false;
        Invalidate();
    }
    return DuiControl::OnMouseLeave();
}

void DuiEditHost::SetShowEyeToggle(bool b)
{
    if (m_showEye == b)
    {
        return;
    }
    m_showEye = b;
    // If turning off while plaintext is shown, revert mask.
    if (!m_showEye && m_pwdRevealed)
    {
        SetPasswordRevealed(false);
    }
    Layout(m_rcItem);   // re-position EDIT (its right edge changes)
    Invalidate();
}

void DuiEditHost::SetPasswordRevealed(bool b)
{
    if (!m_password)
    {
        return;
    }
    if (m_pwdRevealed == b)
    {
        return;
    }
    m_pwdRevealed = b;
    ApplyPasswordRevealToHwnd();
    Invalidate();
}

void DuiEditHost::ApplyPasswordRevealToHwnd()
{
    HWND h = GetHostedHwnd();
    if (!h)
    {
        return;
    }
    if (m_savedMaskChar == 0)
    {
        // First call - capture the system default mask character so we
        // can restore it when re-hiding.
        TCHAR cur = (TCHAR)::SendMessage(h, EM_GETPASSWORDCHAR, 0, 0);
        if (cur != 0)
        {
            m_savedMaskChar = cur;
        }
        else
        {
            m_savedMaskChar = (TCHAR)0x25CF;   // BLACK CIRCLE fallback
        }
    }
    TCHAR newCh = m_pwdRevealed ? 0 : m_savedMaskChar;
    ::SendMessage(h, EM_SETPASSWORDCHAR, (WPARAM)newCh, 0);
    ::InvalidateRect(h, nullptr, TRUE);
}

// ─── 内联图标 + 底色 + 边框 API ───────────────────────────────────────

void DuiEditHost::SetShowBorder(bool b)
{
    if (m_showBorder == b) { return; }
    m_showBorder = b;
    Invalidate();
}

void DuiEditHost::SetVerticalCenter(bool b)
{
    if (m_vCenter == b)
    {
        return;
    }
    m_vCenter = b;
    // 开关改变会改变寄宿子窗口的纵向尺寸 / 位置，重排一次令其立即生效。
    Layout(m_rcItem);
    Invalidate();
}

void DuiEditHost::SetBgColor(COLORREF c)
{
    if (m_bgColor == c)
    {
        return;
    }
    m_bgColor = c;
    // 同步给 Win32 EDIT 自身（OS 渲染时背景）：HwndHostControl 已实现
    // SetCtlBgColor + WM_CTLCOLOREDIT 路径，propagate 给它。
    SetCtlBgColor(c);
    Invalidate();
}

void DuiEditHost::SetIcon(IconSlot slot, int gutterWidth, IconPainter painter)
{
    if (gutterWidth < 0) { gutterWidth = 0; }
    IconState& st = (slot == LeftIcon) ? m_iconL : m_iconR;
    bool widthChanged = (st.width != gutterWidth);
    st.width   = painter ? gutterWidth : 0;
    st.painter = std::move(painter);
    if (widthChanged)
    {
        // EDIT 内缩量改变 —— 必须 re-layout 重排 HWND
        Layout(m_rcItem);
    }
    Invalidate();
}

void DuiEditHost::SetIconBitmap(IconSlot slot, int gutterWidth, HBITMAP hbm)
{
    if (!hbm)
    {
        ClearIcon(slot);
        return;
    }
    SetIcon(slot, gutterWidth,
            [hbm](HDC hdc, const RECT& rc)
    {
        BITMAP bi = {};
        if (::GetObject(hbm, sizeof(bi), &bi) == 0) { return; }
        HDC hdcMem = ::CreateCompatibleDC(hdc);
        HBITMAP old = (HBITMAP)::SelectObject(hdcMem, hbm);
        // 居中（不缩放）
        int x = (rc.left + rc.right - bi.bmWidth) / 2;
        int y = (rc.top  + rc.bottom - bi.bmHeight) / 2;
        ::BitBlt(hdc, x, y, bi.bmWidth, bi.bmHeight,
                 hdcMem, 0, 0, SRCCOPY);
        ::SelectObject(hdcMem, old);
        ::DeleteDC(hdcMem);
    });
}

void DuiEditHost::SetIconGlyph(IconSlot slot, int gutterWidth,
                                LPCTSTR glyph, COLORREF color)
{
    if (!glyph || glyph[0] == 0)
    {
        ClearIcon(slot);
        return;
    }
    CString text(glyph);
    SetIcon(slot, gutterWidth,
            [text, color](HDC hdc, const RECT& rc)
    {
        HFONT hOld = (HFONT)::SelectObject(hdc,
                          DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldC = ::SetTextColor(hdc, color);
        RECT r = rc;
        ::DrawText(hdc, (LPCTSTR)text, -1, &r,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOld);
    });
}

void DuiEditHost::ClearIcon(IconSlot slot)
{
    SetIcon(slot, 0, nullptr);
}

void DuiEditHost::SetIconClickable(IconSlot slot, bool clickable)
{
    IconState& st = (slot == LeftIcon) ? m_iconL : m_iconR;
    st.clickable = clickable;
}

bool DuiEditHost::IsIconClickable(IconSlot slot) const
{
    return (slot == LeftIcon) ? m_iconL.clickable : m_iconR.clickable;
}

void DuiEditHost::SetIconDragHandler(IconSlot slot, IconDragHandler handler)
{
    IconState& st = (slot == LeftIcon) ? m_iconL : m_iconR;
    st.dragHandler = std::move(handler);
}

int DuiEditHost::GetIconWidth(IconSlot slot) const
{
    return (slot == LeftIcon) ? m_iconL.width : m_iconR.width;
}

// 单测用纯函数 helper —— 给定 host rect + 边距 + slot + gutter 宽，
// 返回图标 RECT。无副作用，独立测试。
RECT DuiEditHost::ComputeIconRect(const RECT& rc, IconSlot slot,
                                   int gutterWidth, int borderPx,
                                   int marginVPx)
{
    if (gutterWidth <= 0)
    {
        RECT z = { 0, 0, 0, 0 };
        return z;
    }
    RECT r;
    r.top    = rc.top + borderPx + marginVPx;
    r.bottom = rc.bottom - borderPx - marginVPx;
    if (slot == LeftIcon)
    {
        r.left  = rc.left + borderPx;
        r.right = r.left + gutterWidth;
    }
    else
    {
        r.right = rc.right - borderPx;
        r.left  = r.right - gutterWidth;
    }
    return r;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT
