#include "stdafx.h"
#include "DuiLabel.h"

#if BUI_FEATURE_LABEL

#include "../../DuiResMgr.h"
#include "../../DuiMnemonic.h"
#include <shellapi.h>
#include <vector>

namespace balloonwjui {

DuiLabel::DuiLabel()
{
    // Text labels are not focusable; links are (so keyboard users can Tab to them).
    SetTabStop(false);
}

DuiLabel::~DuiLabel()
{
    if (m_hFontUnderline)
    {
        ::DeleteObject(m_hFontUnderline);
        m_hFontUnderline = nullptr;
    }
}

void DuiLabel::SetText(LPCTSTR sz)
{
    m_text = sz ? sz : _T("");
    Invalidate();
}

TCHAR DuiLabel::GetMnemonicChar() const
{
    return DuiMnemonic::FindChar(m_text);
}

void DuiLabel::SetMode(Mode m)
{
    if (m_mode == m)
    {
        return;
    }
    m_mode = m;
    SetTabStop(m == ModeLink);
    Invalidate();
}

COLORREF DuiLabel::EffectiveColor() const
{
    if (!m_bEnabled)
    {
        return RGB(140, 140, 140);
    }
    if (m_mode == ModeLink)
    {
        if (m_bHover)
        {
            return m_clrHover;
        }
        if (m_visited)
        {
            return m_clrVisited;
        }
        return m_clrLink;
    }
    return m_clrText;
}

HFONT DuiLabel::EffectiveFont(HDC hdc) const
{
    HFONT base = m_hFont
                 ? m_hFont
                 : DuiResMgr::Inst().GetDefaultFont();
    if (!base)
    {
        base = (HFONT)::GetCurrentObject(hdc, OBJ_FONT);
    }
    if (m_mode != ModeLink)
    {
        return base;
    }

    // Build / refresh an underlined sibling of the base font on demand.
    if (m_hFontUnderline && m_lastBaseFont == base)
    {
        return m_hFontUnderline;
    }

    if (m_hFontUnderline)
    {
        ::DeleteObject(m_hFontUnderline);
        m_hFontUnderline = nullptr;
    }
    LOGFONT lf;
    ::GetObject(base, sizeof(lf), &lf);
    lf.lfUnderline = TRUE;
    m_hFontUnderline = ::CreateFontIndirect(&lf);
    m_lastBaseFont = base;
    return m_hFontUnderline ? m_hFontUnderline : base;
}

void DuiLabel::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }
    if (m_text.IsEmpty())
    {
        return;
    }

    HFONT useFont = EffectiveFont(hdc);
    HFONT oldFont = (HFONT)::SelectObject(hdc, useFont);
    int      oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, EffectiveColor());

    DWORD dtFlags = m_dtFlags;
    if (m_wordWrap)
    {
        // DT_WORDBREAK + DT_SINGLELINE is undefined behavior; drop the
        // single-line bit and add wordbreak. DT_TOP is enforced because
        // DT_VCENTER is meaningless with multi-line text.
        dtFlags &= ~(DT_SINGLELINE | DT_VCENTER | DT_BOTTOM);
        dtFlags |= DT_WORDBREAK | DT_TOP;
    }

    // 选中模式 + 单行 + 选区非空时，先把选区高亮画在文本下层。
    if (m_selectable && !m_wordWrap)
    {
        int lo = 0;
        int hi = 0;
        NormalizeSelection(m_selAnchor, m_selCaret, lo, hi);
        if (lo < hi)
        {
            PaintSelectionHighlight(hdc, lo, hi);
        }
    }

    RECT rc = m_rcItem;
    ::DrawText(hdc, m_text, -1, &rc, dtFlags);

    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, oldFont);
}

void DuiLabel::SetWordWrap(bool b)
{
    if (m_wordWrap == b)
    {
        return;
    }
    m_wordWrap = b;
    Invalidate();
}

int DuiLabel::MeasureHeight(int width) const
{
    if (m_text.IsEmpty())
    {
        return 0;
    }
    HDC hdc = ::GetDC(nullptr);
    if (!hdc)
    {
        return 0;
    }
    HFONT use = m_hFont ? m_hFont : DuiResMgr::Inst().GetDefaultFont();
    HFONT old = use ? (HFONT)::SelectObject(hdc, use) : nullptr;

    DWORD flags = DT_CALCRECT | DT_LEFT | DT_TOP | DT_NOPREFIX;
    if (m_wordWrap)
    {
        flags |= DT_WORDBREAK;
    }
    else
    {
        flags |= DT_SINGLELINE;
    }
    // DT_CALCRECT requires a non-zero `right` to constrain wrapping.
    // width <= 0 = "natural width" => omit DT_WORDBREAK constraint by
    // measuring single-line; DrawText then ignores `right`.
    int w = (width <= 0) ? 0x7FFF : width;
    RECT rc = { 0, 0, w, 0 };
    ::DrawText(hdc, m_text, -1, &rc, flags);
    int h = rc.bottom - rc.top;

    if (old)
    {
        ::SelectObject(hdc, old);
    }
    ::ReleaseDC(nullptr, hdc);
    return h;
}

bool DuiLabel::OnLButtonUp(POINT pt, UINT /*mkFlags*/)
{
    // 优先处理拖选结束：把最后落点也并入选区，松开 capture 并刷新。
    // 这条独立于 Link 模式 —— selectable 与 Link 互不冲突。
    if (m_selDragging)
    {
        int idx = HitTestCharIndex(pt);
        m_selCaret = idx;
        m_selDragging = false;
        ReleaseCapture();
        Invalidate();
        return true;
    }

    if (m_mode != ModeLink)
    {
        return false;
    }
    if (!m_bEnabled)
    {
        return false;
    }
    if (!::PtInRect(&m_rcItem, pt))
    {
        return false;
    }

    m_visited = true;
    Invalidate();
    NotifyParent(DUIN_CLICK);
    if (m_autoNav && !m_url.IsEmpty())
    {
        ::ShellExecute(nullptr, _T("open"), m_url, nullptr, nullptr, SW_SHOWNORMAL);
    }
    return true;
}

bool DuiLabel::OnSetCursor(POINT pt)
{
    // selectable 模式且鼠标确实落在文字框内时，用 I-beam（编辑光标）。
    // 单行模式才生效；多行（word-wrap）下选中能力本身不启用，光标也不改。
    if (m_selectable && m_bEnabled && !m_wordWrap && ::PtInRect(&m_rcItem, pt))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_IBEAM));
        return true;
    }
    if (m_mode == ModeLink && m_bEnabled)
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
        return true;
    }
    return false;   // let host fall back to arrow
}

// =================================================================
// 选中模式（SetSelectable）实现
// =================================================================

void DuiLabel::SetSelectable(bool b)
{
    if (m_selectable == b)
    {
        return;
    }
    m_selectable = b;
    if (b)
    {
        // 选中模式必须可获焦点（Ctrl+C / Ctrl+A 走 OnKeyDown 需要焦点）。
        SetTabStop(true);
    }
    else
    {
        // 退出选中模式：清状态。Link 模式自身的 tab stop 由 SetMode 管，
        // 这里仅在不是 Link 模式时回到默认不可聚焦。
        m_selAnchor   = 0;
        m_selCaret    = 0;
        m_selDragging = false;
        if (m_mode != ModeLink)
        {
            SetTabStop(false);
        }
    }
    Invalidate();
}

// ---- 静态纯函数（无 HWND / 无 HDC 依赖，可直接单测） ----

int DuiLabel::CharIndexFromCumulativeWidths(const int* dx, int len, int xRelative)
{
    // 空文本或非法入参 → 索引 0。
    if (len <= 0 || dx == nullptr)
    {
        return 0;
    }
    // 左外 / 左端 → 索引 0。
    if (xRelative <= 0)
    {
        return 0;
    }
    // 右外 → 索引 len（落在最后一个字符之后）。
    if (xRelative >= dx[len - 1])
    {
        return len;
    }
    // 在内部区间：边界位置为 0、dx[0]、dx[1] ... dx[len-1]。
    // 找包含 xRelative 的字符 i（其右边界 dx[i] 首次大于 xRelative），
    // 然后看 xRelative 离左边界（prev）还是右边界（dx[i]）更近。
    int prev = 0;
    for (int i = 0; i < len; ++i)
    {
        int next = dx[i];
        if (xRelative < next)
        {
            if (xRelative - prev < next - xRelative)
            {
                return i;       // 距左边界（索引 i）更近
            }
            return i + 1;       // 距右边界（索引 i+1）更近
        }
        prev = next;
    }
    return len;
}

void DuiLabel::NormalizeSelection(int anchor, int caret, int& lo, int& hi)
{
    if (anchor <= caret)
    {
        lo = anchor;
        hi = caret;
    }
    else
    {
        lo = caret;
        hi = anchor;
    }
}

CString DuiLabel::BuildCopyText(const CString& text, int anchor, int caret)
{
    int lo = 0;
    int hi = 0;
    NormalizeSelection(anchor, caret, lo, hi);
    // 空选区 → 复制整段（常见 Ctrl+C 行为，与多数浏览器 / 编辑器一致）。
    if (lo == hi)
    {
        return text;
    }
    int len = text.GetLength();
    // 防御性裁剪：异常输入仍返回整段，保证不抛。
    if (lo < 0)
    {
        lo = 0;
    }
    if (hi > len)
    {
        hi = len;
    }
    if (lo >= hi)
    {
        return text;
    }
    return text.Mid(lo, hi - lo);
}

// ---- 私有辅助 ----

int DuiLabel::HitTestCharIndex(POINT pt) const
{
    int len = m_text.GetLength();
    if (len <= 0)
    {
        return 0;
    }
    HDC hdc = ::GetDC(nullptr);
    if (!hdc)
    {
        return 0;
    }
    HFONT useFont = EffectiveFont(hdc);
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;

    std::vector<int> dx(len, 0);
    SIZE sz = { 0, 0 };
    int hit = 0;
    if (::GetTextExtentExPoint(hdc, m_text, len, 0, nullptr, dx.data(), &sz))
    {
        // 根据对齐方式算出文字左缘的绝对 x。
        int textWidth = sz.cx;
        int rcW       = m_rcItem.right - m_rcItem.left;
        int textLeft  = m_rcItem.left;
        if (m_dtFlags & DT_CENTER)
        {
            textLeft = m_rcItem.left + (rcW - textWidth) / 2;
        }
        else if (m_dtFlags & DT_RIGHT)
        {
            textLeft = m_rcItem.right - textWidth;
        }
        int xRel = pt.x - textLeft;
        hit = CharIndexFromCumulativeWidths(dx.data(), len, xRel);
    }

    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
    ::ReleaseDC(nullptr, hdc);
    return hit;
}

void DuiLabel::PaintSelectionHighlight(HDC hdc, int lo, int hi)
{
    int len = m_text.GetLength();
    if (len <= 0 || lo >= hi)
    {
        return;
    }
    if (lo < 0)
    {
        lo = 0;
    }
    if (hi > len)
    {
        hi = len;
    }

    std::vector<int> dx(len, 0);
    SIZE sz = { 0, 0 };
    // hdc 已由 OnPaint SelectObject 当前字体 —— 量得的 dx 与 DrawText 一致。
    if (!::GetTextExtentExPoint(hdc, m_text, len, 0, nullptr, dx.data(), &sz))
    {
        return;
    }
    int textWidth = sz.cx;
    int rcW       = m_rcItem.right - m_rcItem.left;
    int textLeft  = m_rcItem.left;
    if (m_dtFlags & DT_CENTER)
    {
        textLeft = m_rcItem.left + (rcW - textWidth) / 2;
    }
    else if (m_dtFlags & DT_RIGHT)
    {
        textLeft = m_rcItem.right - textWidth;
    }

    int xLo = (lo == 0) ? 0 : dx[lo - 1];
    int xHi = (hi == 0) ? 0 : dx[hi - 1];

    RECT selRc;
    selRc.left   = textLeft + xLo;
    selRc.right  = textLeft + xHi;
    selRc.top    = m_rcItem.top;
    selRc.bottom = m_rcItem.bottom;

    HBRUSH br = ::CreateSolidBrush(m_clrSel);
    ::FillRect(hdc, &selRc, br);
    ::DeleteObject(br);
}

void DuiLabel::CopyToClipboard(const CString& text)
{
    if (!::OpenClipboard(nullptr))
    {
        return;
    }
    ::EmptyClipboard();
    int len = text.GetLength();
    HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(len + 1) * sizeof(WCHAR));
    if (hMem)
    {
        WCHAR* pMem = (WCHAR*)::GlobalLock(hMem);
        if (pMem)
        {
            // balloonui 工程是 _UNICODE，CString = CStringW，可直接拷贝。
            ::wcscpy_s(pMem, (size_t)(len + 1), (LPCWSTR)(LPCTSTR)text);
            ::GlobalUnlock(hMem);
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
        else
        {
            ::GlobalFree(hMem);
        }
    }
    ::CloseClipboard();
}

// ---- 新增事件覆写 ----

bool DuiLabel::OnLButtonDown(POINT pt, UINT /*mkFlags*/)
{
    // 仅在 selectable + 启用 + 单行 + 点中文字框内时启动拖选。
    if (!m_selectable || !m_bEnabled || m_wordWrap)
    {
        return false;
    }
    if (!::PtInRect(&m_rcItem, pt))
    {
        return false;
    }
    SetFocus();
    int idx = HitTestCharIndex(pt);
    m_selAnchor   = idx;
    m_selCaret    = idx;
    m_selDragging = true;
    Capture();
    Invalidate();
    return true;
}

bool DuiLabel::OnMouseMove(POINT pt, UINT /*mkFlags*/)
{
    // 只有在拖选中才响应；非拖选时不消费事件，让 host 走默认流程。
    if (!m_selDragging)
    {
        return false;
    }
    int idx = HitTestCharIndex(pt);
    if (idx != m_selCaret)
    {
        m_selCaret = idx;
        Invalidate();
    }
    return true;
}

bool DuiLabel::OnKeyDown(UINT vk, UINT /*flags*/)
{
    if (!m_selectable || !m_bEnabled)
    {
        return false;
    }
    bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (!ctrl)
    {
        return false;
    }
    if (vk == 'C')
    {
        // Ctrl+C：把选中文本（空选区时为整段）写剪贴板。
        CString copy = BuildCopyText(m_text, m_selAnchor, m_selCaret);
        CopyToClipboard(copy);
        return true;
    }
    if (vk == 'A')
    {
        // Ctrl+A：选全部。
        m_selAnchor = 0;
        m_selCaret  = m_text.GetLength();
        Invalidate();
        return true;
    }
    return false;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_LABEL
