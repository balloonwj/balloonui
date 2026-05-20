#include "stdafx.h"
#include "DuiTab.h"

#if BUI_FEATURE_TAB

#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"

namespace balloonwjui {

namespace {

// ---- Close-button "X" glyph colors -----------------------------------------
// Filled background behind the X when the close button is hovered - red
// signals destructive action consistently with browser tab UIs.
const COLORREF kCloseHoverBg     = RGB(220, 100, 100);
// X stroke when the close button is hovered - white reads well on the
// red hover background.
const COLORREF kCloseStrokeHover = RGB(255, 255, 255);
// X stroke when not hovered - mid-gray, subtle until the user targets it.
const COLORREF kCloseStrokeIdle  = RGB(110, 110, 110);

// ---- Dropdown chevron triangle colors ---------------------------------------
// Chevron color when the dropdown affordance is hovered - blue draws the
// eye to the just-revealed clickable target.
const COLORREF kDropChevronHover = RGB( 80, 130, 200);
// Chevron color when idle - matches close-X idle so both tab affordances
// read at the same prominence level.
const COLORREF kDropChevronIdle  = RGB(110, 110, 110);

// ---- Scroll-arrow strip colors ----------------------------------------------
// Background fill for the small left/right arrow strips that appear when
// the tab strip overflows. Pale blue-gray so the strips are visible without
// competing with the tabs themselves.
const COLORREF kScrollArrowBg       = RGB(238, 240, 244);
// Arrow triangle fill+stroke when the corresponding direction is scrollable.
const COLORREF kScrollArrowEnabled  = RGB( 80,  80, 100);
// Arrow triangle fill+stroke when at the end of scroll range; muted gray
// to read as "you can't scroll further this way".
const COLORREF kScrollArrowDisabled = RGB(190, 190, 200);

// Drag-reorder insertion line color (matches DuiListBox::kAccentBlue).
const COLORREF kDragInsertBlue   = RGB( 45, 108, 223);

// ---- Layout constants -------------------------------------------------------
// Padding (px) inside the close-X button; insets the X stroke from the
// edge of its hit/hover rect.
const int kCloseGlyphPadPx       = 3;
// Half-edge (px) of the dropdown chevron triangle. The triangle inscribes
// a 2*kDropChevronHalfPx-wide bounding span.
const int kDropChevronHalfPx     = 4;
// Stroke width (px) for the close-X anti-aliased segments.
const float kCloseStrokeWidthPx  = 2.0f;
// Stroke width (px) for the drag-reorder insertion indicator line.
const int kDragLineStrokePx      = 2;

} // anonymous namespace

DuiTab::DuiTab()
{
    SetTabStop(true);
}

int DuiTab::AddTab(LPCTSTR text, bool closeable, bool dropdown, LPARAM lParam)
{
    Tab t;
    t.text      = text ? text : _T("");
    t.closeable = closeable;
    t.dropdown  = dropdown;
    t.lParam    = lParam;
    t.widthPx   = 0;
    m_tabs.push_back(t);
    if (m_curSel < 0)
    {
        m_curSel = 0;
    }
    Invalidate();
    return (int)m_tabs.size() - 1;
}

void DuiTab::InsertTab(int index, LPCTSTR text, bool closeable, bool dropdown, LPARAM lParam)
{
    if (index < 0)
    {
        index = 0;
    }
    if (index > (int)m_tabs.size())
    {
        index = (int)m_tabs.size();
    }
    Tab t;
    t.text = text ? text : _T("");
    t.closeable = closeable;
    t.dropdown = dropdown;
    t.lParam = lParam;
    t.widthPx = 0;
    m_tabs.insert(m_tabs.begin() + index, t);
    if (m_curSel >= index)
    {
        ++m_curSel;
    }
    if (m_curSel < 0 && !m_tabs.empty())
    {
        m_curSel = 0;
    }
    Invalidate();
}

void DuiTab::RemoveTab(int index)
{
    if (index < 0 || index >= (int)m_tabs.size())
    {
        return;
    }
    m_tabs.erase(m_tabs.begin() + index);
    // Adjust selection.
    if (m_tabs.empty())
    {
        m_curSel = -1;
    }
    else if (m_curSel == index)
    {
        m_curSel = (index < (int)m_tabs.size()) ? index : (int)m_tabs.size() - 1;
    }
    else if (m_curSel > index)
    {
        --m_curSel;
    }
    if (m_hoverIdx >= (int)m_tabs.size())
    {
        m_hoverIdx = -1;
    }
    Invalidate();
}

void DuiTab::RemoveAllTabs()
{
    m_tabs.clear();
    m_curSel = -1;
    m_hoverIdx = -1;
    m_pressIdx = -1;
    Invalidate();
}

CString DuiTab::GetTabText(int i) const
{
    return (i >= 0 && i < (int)m_tabs.size()) ? m_tabs[i].text : CString();
}

LPARAM DuiTab::GetTabParam(int i) const
{
    return (i >= 0 && i < (int)m_tabs.size()) ? m_tabs[i].lParam : 0;
}

bool DuiTab::IsTabCloseable(int i) const
{
    return (i >= 0 && i < (int)m_tabs.size()) && m_tabs[i].closeable;
}

bool DuiTab::IsTabDropdown(int i) const
{
    return (i >= 0 && i < (int)m_tabs.size()) && m_tabs[i].dropdown;
}

void DuiTab::SetTabText(int i, LPCTSTR t)
{
    if (i < 0 || i >= (int)m_tabs.size())
    {
        return;
    }
    m_tabs[i].text = t ? t : _T("");
    Invalidate();
}

void DuiTab::SetTabParam(int i, LPARAM p)
{
    if (i < 0 || i >= (int)m_tabs.size())
    {
        return;
    }
    m_tabs[i].lParam = p;
}

void DuiTab::SetTabCloseable(int i, bool b)
{
    if (i < 0 || i >= (int)m_tabs.size())
    {
        return;
    }
    m_tabs[i].closeable = b;
    Invalidate();
}

void DuiTab::SetTabDropdown(int i, bool b)
{
    if (i < 0 || i >= (int)m_tabs.size())
    {
        return;
    }
    m_tabs[i].dropdown  = b;
    Invalidate();
}

void DuiTab::SetCurSel(int index, bool notify)
{
    if (index < -1 || index >= (int)m_tabs.size())
    {
        return;
    }
    if (m_curSel == index)
    {
        return;
    }
    m_curSel = index;
    if (m_curSel >= 0)
    {
        EnsureVisible(m_curSel);
    }
    Invalidate();
    if (notify)
    {
        FireSelectionChanged(m_curSel);
    }
}

void DuiTab::FireSelectionChanged(int newIdx)
{
    NotifyParent(DUIN_VALUECHANGED, (LPARAM)newIdx);
}

int DuiTab::FindByParam(LPARAM lParam) const
{
    for (size_t i = 0; i < m_tabs.size(); ++i)
    {
        if (m_tabs[i].lParam == lParam)
        {
            return (int)i;
        }
    }
    return -1;
}

void DuiTab::SetTabHeight(int h)
{
    m_tabH    = (h > 1)  ? h : 1;
    Invalidate();
}

void DuiTab::SetMinTabWidth(int w)
{
    m_minTabW = (w >= 0) ? w : 0;
    Invalidate();
}

void DuiTab::SetMaxTabWidth(int w)
{
    m_maxTabW = (w > 0)  ? w : 1;
    Invalidate();
}

int DuiTab::TextWidthOf(LPCTSTR text) const
{
    if (!text || !*text)
    {
        return 0;
    }
    HDC hdc = ::GetDC(nullptr);
    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    SIZE sz = { 0, 0 };
    ::GetTextExtentPoint32(hdc, text, (int)_tcslen(text), &sz);
    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
    ::ReleaseDC(nullptr, hdc);
    return sz.cx;
}

void DuiTab::MeasureTabs() const
{
    for (auto& t : m_tabs)
    {
        int w = TextWidthOf(t.text) + 2 * m_tabPad;
        if (t.closeable)
        {
            w += m_closeSize + 6;
        }
        if (t.dropdown)
        {
            w += m_dropSize  + 6;
        }
        if (w < m_minTabW)
        {
            w = m_minTabW;
        }
        if (w > m_maxTabW)
        {
            w = m_maxTabW;
        }
        t.widthPx = w;
    }
}

int DuiTab::TotalContentWidth() const
{
    MeasureTabs();
    int total = 0;
    for (size_t i = 0; i < m_tabs.size(); ++i)
    {
        total += m_tabs[i].widthPx;
        if (i + 1 < m_tabs.size())
        {
            total += m_gap;
        }
    }
    return total;
}

bool DuiTab::NeedsScroll() const
{
    int avail = m_rcItem.right - m_rcItem.left;
    return TotalContentWidth() > avail;
}

int DuiTab::ContentLeftEdge() const
{
    return m_rcItem.left + (NeedsScroll() ? kArrowW : 0);
}

int DuiTab::ContentRightEdge() const
{
    return m_rcItem.right - (NeedsScroll() ? kArrowW : 0);
}

int DuiTab::GetMaxScrollOffset() const
{
    int contentW = ContentRightEdge() - ContentLeftEdge();
    int total    = TotalContentWidth();
    int over     = total - contentW;
    return over > 0 ? over : 0;
}

bool DuiTab::IsScrollableLeft() const
{
    return NeedsScroll() && m_scrollOffset > 0;
}

bool DuiTab::IsScrollableRight() const
{
    return NeedsScroll() && m_scrollOffset < GetMaxScrollOffset();
}

RECT DuiTab::LeftArrowRect() const
{
    if (!NeedsScroll())
    {
        return RECT{ 0, 0, 0, 0 };
    }
    int top = m_rcItem.bottom - m_tabH;
    return RECT{ m_rcItem.left, top, m_rcItem.left + kArrowW, m_rcItem.bottom };
}

RECT DuiTab::RightArrowRect() const
{
    if (!NeedsScroll())
    {
        return RECT{ 0, 0, 0, 0 };
    }
    int top = m_rcItem.bottom - m_tabH;
    return RECT{ m_rcItem.right - kArrowW, top, m_rcItem.right, m_rcItem.bottom };
}

void DuiTab::SetScrollStepPx(int px)
{
    if (px < 1)
    {
        px = 1;
    }
    m_scrollStepPx = px;
}

void DuiTab::SetScrollOffset(int px)
{
    int maxOff = GetMaxScrollOffset();
    if (px < 0)
    {
        px = 0;
    }
    if (px > maxOff)
    {
        px = maxOff;
    }
    if (m_scrollOffset == px)
    {
        return;
    }
    m_scrollOffset = px;
    Invalidate();
}

void DuiTab::MoveTab(int from, int to)
{
    int n = (int)m_tabs.size();
    if (from < 0 || from >= n)
    {
        return;
    }
    if (to < 0)
    {
        to = 0;
    }
    if (to > n)
    {
        to = n;
    }
    if (to > from)
    {
        --to;     // post-removal index shift
    }
    if (from == to)
    {
        return;
    }

    Tab t = m_tabs[from];
    m_tabs.erase(m_tabs.begin() + from);
    m_tabs.insert(m_tabs.begin() + to, t);

    // Selection follows the moved tab; otherwise shift relative to the
    // moved range.
    if (m_curSel == from)
    {
        m_curSel = to;
    }
    else if (from < m_curSel && m_curSel <= to)
    {
        --m_curSel;
    }
    else if (to <= m_curSel && m_curSel < from)
    {
        ++m_curSel;
    }

    Invalidate();
    NotifyParent((UINT)DUITN_REORDERED, (LPARAM)to);
}

void DuiTab::EnsureVisible(int idx)
{
    if (idx < 0 || idx >= (int)m_tabs.size())
    {
        return;
    }
    if (!NeedsScroll())
    {
        m_scrollOffset = 0;
        return;
    }

    MeasureTabs();
    int contentL = ContentLeftEdge();
    int contentR = ContentRightEdge();

    // Tab's left x in *content coordinates* (without scroll offset).
    int x0 = 0;
    for (int i = 0; i < idx; ++i)
    {
        x0 += m_tabs[i].widthPx + m_gap;
    }
    int x1 = x0 + m_tabs[idx].widthPx;
    int viewW = contentR - contentL;
    int newOff = m_scrollOffset;
    if (x0 < newOff)
    {
        newOff = x0;
    }
    else if (x1 > newOff + viewW)
    {
        newOff = x1 - viewW;
    }
    SetScrollOffset(newOff);
}

RECT DuiTab::TabRect(int index) const
{
    if (index < 0 || index >= (int)m_tabs.size())
    {
        return RECT{};
    }
    MeasureTabs();
    int x = ContentLeftEdge() - m_scrollOffset;
    for (int i = 0; i < index; ++i)
    {
        x += m_tabs[i].widthPx + m_gap;
    }
    int top = m_rcItem.bottom - m_tabH;
    return RECT{ x, top, x + m_tabs[index].widthPx, m_rcItem.bottom };
}

RECT DuiTab::CloseRect(int index) const
{
    if (!IsTabCloseable(index))
    {
        return RECT{};
    }
    RECT t = TabRect(index);
    int cy = (t.top + t.bottom - m_closeSize) / 2;
    int cx = t.right - m_tabPad - m_closeSize;
    return RECT{ cx, cy, cx + m_closeSize, cy + m_closeSize };
}

RECT DuiTab::DropRect(int index) const
{
    if (!IsTabDropdown(index))
    {
        return RECT{};
    }
    RECT t = TabRect(index);
    int cy = (t.top + t.bottom - m_dropSize) / 2;
    int cx = t.right - m_tabPad - m_dropSize;
    if (IsTabCloseable(index))
    {
        cx -= m_closeSize + 4;
    }
    return RECT{ cx, cy, cx + m_dropSize, cy + m_dropSize };
}

int DuiTab::HitTabIndex(POINT pt) const
{
    if (!::PtInRect(&m_rcItem, pt))
    {
        return -1;
    }
    MeasureTabs();
    int top = m_rcItem.bottom - m_tabH;
    if (pt.y < top)
    {
        return -1;
    }

    // Skip the arrow strips when scroll is on; clicks there are
    // arrow-button clicks, not tab clicks.
    if (NeedsScroll())
    {
        if (pt.x < m_rcItem.left + kArrowW)
        {
            return -1;
        }
        if (pt.x >= m_rcItem.right - kArrowW)
        {
            return -1;
        }
    }

    int x = ContentLeftEdge() - m_scrollOffset;
    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        int w = m_tabs[i].widthPx;
        if (pt.x >= x && pt.x < x + w)
        {
            return i;
        }
        x += w + m_gap;
    }
    return -1;
}

DuiTab::HitZone DuiTab::HitZoneAt(POINT pt, int& outIndex) const
{
    outIndex = HitTabIndex(pt);
    if (outIndex < 0)
    {
        return ZoneNone;
    }
    if (IsTabCloseable(outIndex) && ::PtInRect(&CloseRect(outIndex), pt))
    {
        return ZoneClose;
    }
    if (IsTabDropdown(outIndex) && ::PtInRect(&DropRect(outIndex), pt))
    {
        return ZoneDrop;
    }
    return ZoneTab;
}

void DuiTab::DrawCloseGlyph(HDC hdc, const RECT& rc, bool hover) const
{
    if (hover)
    {
        HBRUSH hbr = ::CreateSolidBrush(kCloseHoverBg);
        ::FillRect(hdc, &rc, hbr);
        ::DeleteObject(hbr);
    }
    COLORREF lineClr = hover ? kCloseStrokeHover : kCloseStrokeIdle;
    DuiAA::DrawLine(hdc, rc.left + kCloseGlyphPadPx,  rc.top + kCloseGlyphPadPx,
                         rc.right - kCloseGlyphPadPx, rc.bottom - kCloseGlyphPadPx, lineClr, kCloseStrokeWidthPx);
    DuiAA::DrawLine(hdc, rc.right - kCloseGlyphPadPx, rc.top + kCloseGlyphPadPx,
                         rc.left + kCloseGlyphPadPx,  rc.bottom - kCloseGlyphPadPx, lineClr, kCloseStrokeWidthPx);
}

void DuiTab::DrawDropGlyph(HDC hdc, const RECT& rc, bool hover) const
{
    POINT pts[3] = {
        { rc.left,  rc.top + kCloseGlyphPadPx },
        { rc.right, rc.top + kCloseGlyphPadPx },
        { (rc.left + rc.right) / 2, rc.bottom - kCloseGlyphPadPx }
    };
    COLORREF clr = hover ? kDropChevronHover : kDropChevronIdle;
    DuiAA::FillPolygon(hdc, pts, 3, clr, clr);
}

void DuiTab::DrawTab(HDC hdc, int index) const
{
    RECT rc = TabRect(index);
    bool sel = (index == m_curSel);
    bool hov = (index == m_hoverIdx) && (m_hoverZone != ZoneNone);

    COLORREF bg = sel ? m_clrTabSel : (hov ? m_clrTabHover : m_clrTab);
    HBRUSH hbr = ::CreateSolidBrush(bg);
    ::FillRect(hdc, &rc, hbr);
    ::DeleteObject(hbr);

    // Border around non-selected tabs; selected tab merges with content area
    HPEN pen = ::CreatePen(PS_SOLID, 1, m_clrBorder);
    HPEN oldPen = (HPEN)::SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    ::Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom + (sel ? 1 : 0));
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pen);

    // Text
    RECT rText = rc;
    rText.left  += m_tabPad;
    rText.right -= m_tabPad;
    if (IsTabCloseable(index))
    {
        rText.right -= m_closeSize + 6;
    }
    if (IsTabDropdown(index))
    {
        rText.right -= m_dropSize  + 6;
    }

    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, sel ? m_clrTextSel : m_clrText);
    ::DrawText(hdc, m_tabs[index].text, -1, &rText,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }

    if (IsTabDropdown(index))
    {
        bool dHover = (index == m_hoverIdx) && (m_hoverZone == ZoneDrop);
        DrawDropGlyph(hdc, DropRect(index), dHover);
    }
    if (IsTabCloseable(index))
    {
        bool cHover = (index == m_hoverIdx) && (m_hoverZone == ZoneClose);
        DrawCloseGlyph(hdc, CloseRect(index), cHover);
    }
}

void DuiTab::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    // Strip background
    HBRUSH hbr = ::CreateSolidBrush(m_clrBg);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    MeasureTabs();

    // Clip tab painting to the content band when scroll is on so tabs
    // don't bleed under the arrow strips on either side.
    bool clipped = NeedsScroll();
    int saved = 0;
    if (clipped)
    {
        saved = ::SaveDC(hdc);
        RECT clip = { ContentLeftEdge(), m_rcItem.bottom - m_tabH,
                      ContentRightEdge(), m_rcItem.bottom };
        HRGN rgn = ::CreateRectRgnIndirect(&clip);
        ::SelectClipRgn(hdc, rgn);
        ::DeleteObject(rgn);
    }
    for (size_t i = 0; i < m_tabs.size(); ++i)
    {
        DrawTab(hdc, (int)i);
    }
    if (clipped)
    {
        ::RestoreDC(hdc, saved);
    }

    // Left / right arrow buttons.
    if (NeedsScroll())
    {
        auto drawArrow = [&](const RECT& r, bool pointLeft, bool enabled)
        {
            HBRUSH bg = ::CreateSolidBrush(kScrollArrowBg);
            ::FillRect(hdc, &r, bg);
            ::DeleteObject(bg);
            int cx = (r.left + r.right) / 2;
            int cy = (r.top  + r.bottom) / 2;
            int s = kDropChevronHalfPx;
            POINT pts[3];
            if (pointLeft)
            {
                pts[0] = { cx + s/2, cy - s };
                pts[1] = { cx + s/2, cy + s };
                pts[2] = { cx - s,   cy     };
            }
            else
            {
                pts[0] = { cx - s/2, cy - s };
                pts[1] = { cx - s/2, cy + s };
                pts[2] = { cx + s,   cy     };
            }
            COLORREF c = enabled ? kScrollArrowEnabled : kScrollArrowDisabled;
            HBRUSH ar = ::CreateSolidBrush(c);
            HPEN ap = ::CreatePen(PS_SOLID, 1, c);
            HBRUSH ob = (HBRUSH)::SelectObject(hdc, ar);
            HPEN   op = (HPEN)  ::SelectObject(hdc, ap);
            ::Polygon(hdc, pts, 3);
            ::SelectObject(hdc, ob);
            ::SelectObject(hdc, op);
            ::DeleteObject(ar);
            ::DeleteObject(ap);
        };
        drawArrow(LeftArrowRect(),  true,  IsScrollableLeft());
        drawArrow(RightArrowRect(), false, IsScrollableRight());
    }

    // Bottom separator line under the strip — single full-width line
    // when scroll is on (we can't easily compute the per-tab gap with
    // shifted coords); per-tab dotted line when no scroll.
    HPEN pen = ::CreatePen(PS_SOLID, 1, m_clrBorder);
    HPEN oldPen = (HPEN)::SelectObject(hdc, pen);
    int y = m_rcItem.bottom - 1;
    if (NeedsScroll())
    {
        ::MoveToEx(hdc, m_rcItem.left,  y, nullptr);
        ::LineTo  (hdc, m_rcItem.right, y);
        // Re-blank the under-selected-tab segment for visual continuity
        // with the content area, mirroring the no-scroll branch.
        if (m_curSel >= 0)
        {
            RECT r = TabRect(m_curSel);
            HPEN bgPen = ::CreatePen(PS_SOLID, 1, m_clrTabSel);
            HPEN op2   = (HPEN)::SelectObject(hdc, bgPen);
            ::MoveToEx(hdc, r.left,  y, nullptr);
            ::LineTo  (hdc, r.right, y);
            ::SelectObject(hdc, op2);
            ::DeleteObject(bgPen);
        }
    }
    else
    {
        int x = m_rcItem.left;
        for (size_t i = 0; i < m_tabs.size(); ++i)
        {
            int w = m_tabs[i].widthPx;
            if ((int)i != m_curSel)
            {
                ::MoveToEx(hdc, x, y, nullptr);
                ::LineTo  (hdc, x + w + m_gap, y);
            }
            x += w + m_gap;
        }
        if (x < m_rcItem.right)
        {
            ::MoveToEx(hdc, x, y, nullptr);
            ::LineTo  (hdc, m_rcItem.right, y);
        }
    }
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pen);

    // Drag-reorder insertion line.
    if (m_dragging && m_dragSrcIdx >= 0 && m_dragDropSlot >= 0)
    {
        MeasureTabs();
        int x = ContentLeftEdge() - m_scrollOffset;
        for (int i = 0; i < m_dragDropSlot; ++i)
        {
            x += m_tabs[i].widthPx + m_gap;
        }
        int top = m_rcItem.bottom - m_tabH;
        HPEN dragPen = ::CreatePen(PS_SOLID, kDragLineStrokePx, kDragInsertBlue);
        HPEN dop = (HPEN)::SelectObject(hdc, dragPen);
        ::MoveToEx(hdc, x, top + 2, nullptr);
        ::LineTo  (hdc, x, m_rcItem.bottom - 2);
        ::SelectObject(hdc, dop);
        ::DeleteObject(dragPen);
    }
}

bool DuiTab::OnLButtonDown(POINT pt, UINT)
{
    // Arrow-button clicks (only when scroll is on).
    if (NeedsScroll())
    {
        if (::PtInRect(&LeftArrowRect(), pt))
        {
            SetScrollOffset(m_scrollOffset - m_scrollStepPx);
            return true;
        }
        if (::PtInRect(&RightArrowRect(), pt))
        {
            SetScrollOffset(m_scrollOffset + m_scrollStepPx);
            return true;
        }
    }

    int idx = -1;
    HitZone z = HitZoneAt(pt, idx);
    m_pressIdx = idx;
    m_pressZone = z;

    // Reorder: arm a potential drag on a tab body (not on close / drop
    // glyphs, which have their own click semantics).
    if (m_reorderEnabled && z == ZoneTab && idx >= 0)
    {
        m_dragSrcIdx   = idx;
        m_dragDropSlot = -1;
        m_pressX       = pt.x;
        m_dragging     = false;
        Capture();
    }
    return true;
}

bool DuiTab::OnLButtonUp(POINT pt, UINT)
{
    int idx = -1;
    HitZone z = HitZoneAt(pt, idx);
    bool sameTarget = (idx == m_pressIdx) && (z == m_pressZone) && (idx >= 0);
    int press = m_pressIdx;
    HitZone pz = m_pressZone;
    m_pressIdx = -1;
    m_pressZone = ZoneNone;

    // Resolve any active drag-reorder before falling through to the
    // normal click semantics.
    bool didReorder = false;
    if (m_dragSrcIdx >= 0)
    {
        int src  = m_dragSrcIdx;
        int slot = m_dragDropSlot;
        bool wasDragging = m_dragging;
        m_dragSrcIdx   = -1;
        m_dragDropSlot = -1;
        m_dragging     = false;
        if (m_bCapture)
        {
            ReleaseCapture();
        }
        if (wasDragging && slot >= 0 && slot != src && slot != src + 1)
        {
            MoveTab(src, slot);
            didReorder = true;
        }
        Invalidate();
    }
    if (didReorder)
    {
        return true;
    }

    if (!sameTarget)
    {
        return true;
    }
    switch (z)
    {
    case ZoneClose:
        // Don't change selection; let parent decide whether to RemoveTab.
        NotifyParent((UINT)DUITN_CLOSE, (LPARAM)idx);
        break;
    case ZoneDrop:
        NotifyParent((UINT)DUITN_DROPDOWN, (LPARAM)idx);
        break;
    case ZoneTab:
        SetCurSel(idx);
        break;
    default:
        break;
    }
    (void)press;
    (void)pz;
    return true;
}

bool DuiTab::OnMouseMove(POINT pt, UINT)
{
    // Drag-reorder tracking: cross threshold then compute insertion slot
    // by sweeping the strip with the cursor's x.
    if (m_dragSrcIdx >= 0)
    {
        if (!m_dragging)
        {
            int dx = pt.x - m_pressX;
            if (dx < 0)
            {
                dx = -dx;
            }
            if (dx >= kDragThresholdPx)
            {
                m_dragging = true;
            }
        }
        if (m_dragging)
        {
            MeasureTabs();
            int x = ContentLeftEdge() - m_scrollOffset;
            int slot = 0;
            for (int i = 0; i < (int)m_tabs.size(); ++i)
            {
                int w = m_tabs[i].widthPx;
                int mid = x + w / 2;
                if (pt.x >= mid)
                {
                    slot = i + 1;
                }
                x += w + m_gap;
            }
            if (slot != m_dragDropSlot)
            {
                m_dragDropSlot = slot;
                Invalidate();
            }
        }
        return true;
    }

    int idx = -1;
    HitZone z = HitZoneAt(pt, idx);
    if (idx == m_hoverIdx && z == m_hoverZone)
    {
        return false;
    }
    m_hoverIdx = idx;
    m_hoverZone = z;
    Invalidate();
    return false;
}

bool DuiTab::OnMouseLeave()
{
    if (m_hoverIdx != -1 || m_hoverZone != ZoneNone)
    {
        m_hoverIdx = -1;
        m_hoverZone = ZoneNone;
        Invalidate();
    }
    return DuiControl::OnMouseLeave();
}

} // namespace balloonwjui

#endif // BUI_FEATURE_TAB
