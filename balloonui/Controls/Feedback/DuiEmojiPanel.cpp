#include "stdafx.h"
#include "DuiEmojiPanel.h"

#if BUI_FEATURE_EMOJIPANEL

#include <gdiplus.h>

namespace balloonwjui {

#pragma comment(lib, "gdiplus.lib")

namespace {

// Lazy GDI+ init shared with other DUI bitmap paths.
ULONG_PTR& GdipToken()
{
    static ULONG_PTR s_token = 0;
    return s_token;
}

void EnsureGdiplus()
{
    if (GdipToken() != 0)
    {
        return;
    }
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&GdipToken(), &input, nullptr);
}

// Build a Gdiplus::Bitmap from an HBITMAP that is correct for both
// 32bpp top-down DIBSections (PNG decoded into bits) and DDBs returned
// from Gdiplus::Bitmap::GetHBITMAP. See DuiNinePatch for the rationale.
Gdiplus::Bitmap* MakeGdipBitmapFromHbm(HBITMAP hbm)
{
    if (!hbm)
    {
        return nullptr;
    }
    DIBSECTION ds = {};
    if (::GetObject(hbm, sizeof(ds), &ds) != sizeof(ds))
    {
        return Gdiplus::Bitmap::FromHBITMAP(hbm, nullptr);
    }
    if (ds.dsBm.bmBitsPixel != 32 || ds.dsBm.bmBits == nullptr)
    {
        return Gdiplus::Bitmap::FromHBITMAP(hbm, nullptr);
    }

    int stride = ds.dsBm.bmWidthBytes;
    BYTE* bits = (BYTE*)ds.dsBm.bmBits;
    if (ds.dsBmih.biHeight > 0)
    {
        bits   = bits + (LONG_PTR)stride * (ds.dsBmih.biHeight - 1);
        stride = -stride;
    }
    return new Gdiplus::Bitmap(ds.dsBm.bmWidth, ds.dsBm.bmHeight,
                               stride, PixelFormat32bppPARGB, bits);
}

} // namespace

DuiEmojiPanel::DuiEmojiPanel()
{
    SetTabStop(false);
}

void DuiEmojiPanel::AddEmoji(LPCTSTR utf16Sequence, LPCTSTR tooltip)
{
    E e;
    e.seq  = utf16Sequence ? utf16Sequence : _T("");
    e.tip  = tooltip ? tooltip : _T("");
    e.icon = nullptr;
    if (!e.seq.IsEmpty())
    {
        m_emoji.push_back(e);
    }
}

void DuiEmojiPanel::AddEmojiBitmap(LPCTSTR utf16Sequence, HBITMAP icon, LPCTSTR tooltip)
{
    E e;
    e.seq  = utf16Sequence ? utf16Sequence : _T("");
    e.tip  = tooltip ? tooltip : _T("");
    e.icon = icon;
    // Allow empty seq when an icon is supplied — the panel will still
    // bubble its index on click, even if the caller doesn't care about
    // text insertion.
    if (!e.seq.IsEmpty() || icon)
    {
        m_emoji.push_back(e);
    }
}

void DuiEmojiPanel::AddEmojiSet(LPCTSTR const* sequences, int count)
{
    if (!sequences)
    {
        return;
    }
    for (int i = 0; i < count; ++i)
    {
        AddEmoji(sequences[i]);
    }
}

void DuiEmojiPanel::Clear()
{
    m_emoji.clear();
    m_hoverIdx = -1;
    m_pressIdx = -1;
    Invalidate();
}

int DuiEmojiPanel::GetCount() const
{
    return (int)m_emoji.size();
}

CString DuiEmojiPanel::GetEmojiAt(int idx) const
{
    if (idx < 0 || idx >= (int)m_emoji.size())
    {
        return CString();
    }
    return m_emoji[idx].seq;
}

CString DuiEmojiPanel::GetTooltipAt(int idx) const
{
    if (idx < 0 || idx >= (int)m_emoji.size())
    {
        return CString();
    }
    return m_emoji[idx].tip;
}

HBITMAP DuiEmojiPanel::GetIconAt(int idx) const
{
    if (idx < 0 || idx >= (int)m_emoji.size())
    {
        return nullptr;
    }
    return m_emoji[idx].icon;
}

void DuiEmojiPanel::SetCellSize(int px)
{
    if (px < 16)
    {
        px = 16;
    }
    if (m_cellSize == px)
    {
        return;
    }
    m_cellSize = px;
    Invalidate();
}

void DuiEmojiPanel::SetColumns(int n)
{
    if (n < 1)
    {
        n = 1;
    }
    if (m_columns == n)
    {
        return;
    }
    m_columns = n;
    Invalidate();
}

int DuiEmojiPanel::GetRowCount() const
{
    int n = (int)m_emoji.size();
    if (n <= 0)
    {
        return 0;
    }
    return (n + m_columns - 1) / m_columns;
}

SIZE DuiEmojiPanel::GetDesiredSize() const
{
    SIZE s;
    s.cx = m_columns * m_cellSize;
    s.cy = GetRowCount() * m_cellSize;
    return s;
}

void DuiEmojiPanel::SetPickCallback(PickCallback cb, void* userdata)
{
    m_pickCb = cb;
    m_pickUd = userdata;
}

RECT DuiEmojiPanel::CellRect(int idx) const
{
    int col = idx % m_columns;
    int row = idx / m_columns;
    RECT r;
    r.left   = m_rcItem.left + col * m_cellSize;
    r.top    = m_rcItem.top  + row * m_cellSize;
    r.right  = r.left + m_cellSize;
    r.bottom = r.top  + m_cellSize;
    return r;
}

int DuiEmojiPanel::HitTestIndex(POINT pt) const
{
    if (!::PtInRect(&m_rcItem, pt))
    {
        return -1;
    }
    if (m_emoji.empty() || m_columns <= 0 || m_cellSize <= 0)
    {
        return -1;
    }
    int col = (pt.x - m_rcItem.left) / m_cellSize;
    int row = (pt.y - m_rcItem.top)  / m_cellSize;
    if (col < 0 || col >= m_columns)
    {
        return -1;
    }
    int idx = row * m_columns + col;
    if (idx < 0 || idx >= (int)m_emoji.size())
    {
        return -1;
    }
    return idx;
}

HFONT DuiEmojiPanel::EnsureEmojiFont() const
{
    int wantPx = m_cellSize - 6;
    if (wantPx < 12)
    {
        wantPx = 12;
    }
    if (m_hEmojiFont && m_emojiFontPx == wantPx)
    {
        return m_hEmojiFont;
    }
    if (m_hEmojiFont)
    {
        ::DeleteObject(m_hEmojiFont);
    }

    LOGFONT lf = {};
    lf.lfHeight  = -wantPx;
    lf.lfWeight  = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcsncpy_s(lf.lfFaceName, _T("Segoe UI Emoji"), _TRUNCATE);
    m_hEmojiFont = ::CreateFontIndirect(&lf);
    m_emojiFontPx = wantPx;
    return m_hEmojiFont;
}

void DuiEmojiPanel::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    if (m_emoji.empty())
    {
        return;
    }

    EnsureGdiplus();
    HFONT useFont = EnsureEmojiFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);

    // Reuse one Gdiplus::Graphics for all icon cells.
    Gdiplus::Graphics g(hdc);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);

    int n = (int)m_emoji.size();
    for (int i = 0; i < n; ++i)
    {
        RECT cell = CellRect(i);
        RECT inter;
        if (!::IntersectRect(&inter, &cell, &rcDirty))
        {
            continue;
        }

        if (i == m_hoverIdx || i == m_pressIdx)
        {
            HBRUSH hbr = ::CreateSolidBrush(
                (i == m_pressIdx) ? RGB(200, 220, 250) : RGB(232, 240, 252));
            ::FillRect(hdc, &cell, hbr);
            ::DeleteObject(hbr);
        }

        if (m_emoji[i].icon)
        {
            // Color emoji: paint the supplied PNG/HBITMAP fitted into a
            // small inset of the cell so adjacent cells don't visually
            // touch. Aspect-correct: use min(cellW, cellH) - inset for
            // both dimensions and center inside the cell.
            const int inset = 2;
            int side = (m_cellSize - inset * 2);
            if (side < 1)
            {
                side = 1;
            }
            int cx = (cell.left + cell.right ) / 2;
            int cy = (cell.top  + cell.bottom) / 2;
            Gdiplus::Rect dst(cx - side / 2, cy - side / 2, side, side);

            Gdiplus::Bitmap* p = MakeGdipBitmapFromHbm(m_emoji[i].icon);
            if (p)
            {
                g.DrawImage(p, dst,
                            0, 0, p->GetWidth(), p->GetHeight(),
                            Gdiplus::UnitPixel);
                delete p;
            }
        }
        else
        {
            ::DrawText(hdc, m_emoji[i].seq, -1, &cell,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
    g.Flush(Gdiplus::FlushIntentionSync);

    ::SetBkMode(hdc, oldBk);
    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
}

bool DuiEmojiPanel::OnLButtonDown(POINT pt, UINT)
{
    if (!m_bEnabled)
    {
        return false;
    }
    int idx = HitTestIndex(pt);
    if (idx < 0)
    {
        return false;
    }
    m_pressIdx = idx;
    Invalidate();
    return true;
}

bool DuiEmojiPanel::OnLButtonUp(POINT pt, UINT)
{
    int press = m_pressIdx;
    m_pressIdx = -1;
    int idx = HitTestIndex(pt);
    Invalidate();
    if (press >= 0 && idx == press)
    {
        // Bubble standard notify, then fire the optional pick callback
        // so a popup host can Hide() without going through WM_DUI_NOTIFY.
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)idx);
        if (m_pickCb)
        {
            m_pickCb(m_pickUd, (LPCTSTR)m_emoji[idx].seq, idx);
        }
        return true;
    }
    return true;
}

bool DuiEmojiPanel::OnMouseMove(POINT pt, UINT)
{
    int idx = HitTestIndex(pt);
    if (idx != m_hoverIdx)
    {
        m_hoverIdx = idx;
        Invalidate();
    }
    return false;
}

bool DuiEmojiPanel::OnMouseLeave()
{
    if (m_hoverIdx != -1)
    {
        m_hoverIdx = -1;
        Invalidate();
    }
    return false;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_EMOJIPANEL
