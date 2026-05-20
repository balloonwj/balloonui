#include "stdafx.h"
#include "DuiListBox.h"

#if BUI_FEATURE_LISTBOX

#include "../../DuiResMgr.h"

namespace balloonwjui {

namespace {

// ---- Border / chrome colors -------------------------------------------------
// Border around the listbox frame - mid-gray, neutral against any bg color
// the caller may set.
const COLORREF kListBoxBorder       = RGB(180, 180, 180);
// Inner background for the per-item checkbox rectangle - white, so the
// blue tick is clearly visible.
const COLORREF kCheckboxFillWhite   = RGB(255, 255, 255);
// Checkbox border - cooler gray than the listbox border to slightly stand
// out as an interactive element.
const COLORREF kCheckboxBorder      = RGB(150, 150, 160);
// Check tick stroke and drag-reorder insertion line color - the project's
// brand blue (matches DuiButton::kPushButtonFillNormal).
const COLORREF kAccentBlue          = RGB( 45, 108, 223);

// ---- Layout constants -------------------------------------------------------
// Edge of the per-item checkbox glyph (px). 14 leaves enough inner room
// for the 3-segment tick at 9pt without crowding adjacent text.
const int kCheckboxGlyphPx          = 14;
// Horizontal padding (px) between the row left edge and the checkbox.
const int kCheckboxLeftPadPx        = 4;
// Horizontal padding (px) between the left edge and label start when no
// checkbox column is shown.
const int kRowTextLeftPadPx         = 6;
// Stroke width (px) for the check tick when an item is checked.
const int kCheckTickStrokePx        = 2;
// Stroke width (px) for the drag-reorder insertion indicator line.
const int kDragLineStrokePx         = 2;

} // anonymous namespace

// =====================================================================
// DuiListBox
// =====================================================================

DuiListBox::DuiListBox()
{
    SetTabStop(true);
    auto sb = std::unique_ptr<DuiScrollBar>(new DuiScrollBar(/*horizontal=*/false));
    m_sb = sb.get();
    m_sb->SetOnScroll(&DuiListBox::OnSbScrolledStub, this);
    m_sb->SetAutoHide(true);    // 默认 auto-hide：鼠标进 list / 滚轮才显示，离开渐隐
    DuiControl::AddChild(std::move(sb));
}

int DuiListBox::AddItem(LPCTSTR text, LPARAM lParam)
{
    Item it;
    it.text = text ? text : _T("");
    it.lParam = lParam;
    it.selected = false;
    it.checked  = false;
    m_items.push_back(it);
    UpdateScrollRange();
    Invalidate();
    return (int)m_items.size() - 1;
}

void DuiListBox::InsertItem(int index, LPCTSTR text, LPARAM lParam)
{
    if (index < 0)
    {
        index = 0;
    }
    if (index > (int)m_items.size())
    {
        index = (int)m_items.size();
    }
    Item it;
    it.text = text ? text : _T("");
    it.lParam = lParam;
    it.selected = false;
    it.checked  = false;
    m_items.insert(m_items.begin() + index, it);
    if (m_curSel >= index)
    {
        ++m_curSel;
    }
    UpdateScrollRange();
    Invalidate();
}

void DuiListBox::DeleteItem(int index)
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return;
    }
    m_items.erase(m_items.begin() + index);
    if (m_curSel == index)
    {
        m_curSel = -1;
    }
    else if (m_curSel > index)
    {
        --m_curSel;
    }
    if (m_hoverIdx == index)
    {
        m_hoverIdx = -1;
    }
    UpdateScrollRange();
    Invalidate();
}

void DuiListBox::DeleteAllItems()
{
    m_items.clear();
    m_curSel = -1;
    m_hoverIdx = -1;
    UpdateScrollRange();
    Invalidate();
}

CString DuiListBox::GetItemText(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return CString();
    }
    return m_items[index].text;
}

LPARAM DuiListBox::GetItemParam(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return 0;
    }
    return m_items[index].lParam;
}

void DuiListBox::SetItemText(int index, LPCTSTR text)
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return;
    }
    m_items[index].text = text ? text : _T("");
    Invalidate();
}

void DuiListBox::SetItemParam(int index, LPARAM lParam)
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return;
    }
    m_items[index].lParam = lParam;
}

void DuiListBox::SetCurSel(int index, bool notify)
{
    if (index < -1 || index >= (int)m_items.size())
    {
        return;
    }
    bool changed = (m_curSel != index);
    m_curSel = index;
    // Single-select model: collapse the per-item selected flag to just
    // m_curSel. Multi-select model: only the anchor moves; existing
    // selection bits stay put unless the caller clears them explicitly.
    if (!m_multiSelect)
    {
        for (int i = 0; i < (int)m_items.size(); ++i)
        {
            m_items[i].selected = (i == index);
        }
    }
    if (changed)
    {
        Invalidate();
    }
    if (notify && changed)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_curSel);
    }
}

void DuiListBox::SetMultiSelect(bool b)
{
    if (m_multiSelect == b)
    {
        return;
    }
    m_multiSelect = b;
    if (!b)
    {
        // Going single -- keep only m_curSel selected.
        for (int i = 0; i < (int)m_items.size(); ++i)
        {
            m_items[i].selected = (i == m_curSel);
        }
    }
    Invalidate();
}

bool DuiListBox::IsItemSelected(int idx) const
{
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return false;
    }
    return m_items[idx].selected;
}

void DuiListBox::SetItemSelected(int idx, bool sel, bool notify)
{
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return;
    }
    if (m_items[idx].selected == sel)
    {
        return;
    }
    if (sel && !m_multiSelect)
    {
        // Single-select: clear the rest first.
        for (int i = 0; i < (int)m_items.size(); ++i)
        {
            m_items[i].selected = (i == idx);
        }
        m_curSel = idx;
    }
    else
    {
        m_items[idx].selected = sel;
        if (sel)
        {
            m_curSel = idx;
        }
    }
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)idx);
    }
}

int DuiListBox::GetSelectionCount() const
{
    int n = 0;
    for (auto& it : m_items)
    {
        if (it.selected)
        {
            ++n;
        }
    }
    return n;
}

void DuiListBox::GetSelectedIndices(std::vector<int>& out) const
{
    out.clear();
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        if (m_items[i].selected)
        {
            out.push_back(i);
        }
    }
}

void DuiListBox::ClearSelection()
{
    bool any = false;
    for (auto& it : m_items)
    {
        if (it.selected)
        {
            it.selected = false;
            any = true;
        }
    }
    if (any)
    {
        Invalidate();
    }
}

void DuiListBox::SetShowCheckboxes(bool b)
{
    if (m_showCheckboxes == b)
    {
        return;
    }
    m_showCheckboxes = b;
    Invalidate();
}

bool DuiListBox::IsItemChecked(int idx) const
{
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return false;
    }
    return m_items[idx].checked;
}

void DuiListBox::SetItemChecked(int idx, bool checked, bool notify)
{
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return;
    }
    if (m_items[idx].checked == checked)
    {
        return;
    }
    m_items[idx].checked = checked;
    Invalidate();
    if (notify)
    {
        NotifyParent((UINT)DUITN_CHECKED, (LPARAM)idx);
    }
}

void DuiListBox::MoveItem(int from, int to)
{
    int n = (int)m_items.size();
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
    // `to` is the slot the moved item should end up at AFTER removal.
    // Insertion semantics: erase first; if to > from, the target index
    // shifts down by 1.
    if (to > from)
    {
        --to;
    }
    if (from == to)
    {
        return;
    }

    Item it = m_items[from];
    m_items.erase(m_items.begin() + from);
    m_items.insert(m_items.begin() + to, it);

    // Adjust m_curSel: track the moved row when it was the current
    // selection; otherwise shift relative to the move.
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

void DuiListBox::SetItemHeight(int h)
{
    if (h < 1)
    {
        h = 1;
    }
    if (m_itemH == h)
    {
        return;
    }
    m_itemH = h;
    UpdateScrollRange();
    Invalidate();
}

int DuiListBox::GetScrollPos() const
{
    return m_sb ? m_sb->GetPos() : 0;
}

void DuiListBox::SetScrollPos(int p)
{
    if (m_sb)
    {
        m_sb->SetPos(p);
    }
}

void DuiListBox::EnsureVisible(int index)
{
    if (index < 0 || index >= (int)m_items.size() || !m_sb)
    {
        return;
    }
    int top = index * m_itemH;
    int bot = top + m_itemH;
    int viewH = m_rcItem.bottom - m_rcItem.top;
    int pos = m_sb->GetPos();
    if (top < pos)
    {
        pos = top;
    }
    else if (bot > pos + viewH)
    {
        pos = bot - viewH;
    }
    m_sb->SetPos(pos, /*notify=*/false);
    Invalidate();
}

int DuiListBox::BodyWidth() const
{
    int w = m_rcItem.right - m_rcItem.left;
    return (m_sb && m_sb->IsVisible()) ? w - m_sbWidth : w;
}

int DuiListBox::RowsVisible() const
{
    int viewH = m_rcItem.bottom - m_rcItem.top;
    return viewH / m_itemH;
}

void DuiListBox::OnSbScrolledStub(void* user, int)
{
    static_cast<DuiListBox*>(user)->Invalidate();
}

void DuiListBox::UpdateScrollRange()
{
    if (!m_sb)
    {
        return;
    }
    int viewH = m_rcItem.bottom - m_rcItem.top;
    int over  = ContentHeight() - viewH;
    if (over < 0)
    {
        over = 0;
    }
    m_sb->SetRange(0, over);
    m_sb->SetPage(viewH > 0 ? viewH : 1);
    m_sb->SetLineSize(m_itemH);

    bool needBar = ContentHeight() > viewH;
    m_sb->SetVisible(needBar);
}

int DuiListBox::IndexFromPoint(POINT pt) const
{
    if (!::PtInRect(&m_rcItem, pt))
    {
        return -1;
    }
    int rel = (pt.y - m_rcItem.top) + (m_sb ? m_sb->GetPos() : 0);
    int idx = rel / m_itemH;
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return -1;
    }
    if (pt.x >= m_rcItem.left + BodyWidth())
    {
        return -1;     // over scrollbar
    }
    return idx;
}

RECT DuiListBox::ItemRect(int index) const
{
    int top = m_rcItem.top + index * m_itemH - (m_sb ? m_sb->GetPos() : 0);
    return RECT{ m_rcItem.left, top, m_rcItem.left + BodyWidth(), top + m_itemH };
}

void DuiListBox::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    UpdateScrollRange();
    if (m_sb && m_sb->IsVisible())
    {
        RECT rcSb = { m_rcItem.right - m_sbWidth, m_rcItem.top,
                      m_rcItem.right,             m_rcItem.bottom };
        m_sb->SetRect(rcSb);
    }
}

DuiControl* DuiListBox::HitTest(POINT pt)
{
    if (!m_bVisible || !::PtInRect(&m_rcItem, pt))
    {
        return nullptr;
    }
    if (m_sb && m_sb->IsVisible() && ::PtInRect(&m_sb->GetRect(), pt))
    {
        return m_sb;
    }
    return m_bEnabled ? this : nullptr;
}

void DuiListBox::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    HBRUSH hbr = ::CreateSolidBrush(m_clrBg);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    // Border
    HPEN hpen = ::CreatePen(PS_SOLID, 1, kListBoxBorder);
    HPEN oldPen = (HPEN)::SelectObject(hdc, hpen);
    HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    ::Rectangle(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom);
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(hpen);

    // Clip rows to body.
    int bodyW = BodyWidth();
    RECT rcClip = { m_rcItem.left + 1, m_rcItem.top + 1,
                    m_rcItem.left + bodyW, m_rcItem.bottom - 1 };
    HRGN rgn = ::CreateRectRgnIndirect(&rcClip);
    HRGN rgnOld = ::CreateRectRgn(0, 0, 0, 0);
    int hasOld = ::GetClipRgn(hdc, rgnOld);
    ::SelectClipRgn(hdc, rgn);

    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;

    int firstVisible = (m_sb ? m_sb->GetPos() : 0) / m_itemH;
    int lastVisible  = firstVisible + RowsVisible() + 1;
    if (lastVisible > (int)m_items.size())
    {
        lastVisible = (int)m_items.size();
    }

    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    for (int i = firstVisible; i < lastVisible; ++i)
    {
        RECT rc = ItemRect(i);
        RECT inter;
        if (!::IntersectRect(&inter, &rc, &rcDirty))
        {
            continue;
        }

        // Per-item selected flag (multi-select model). For single-select
        // the SetCurSel path keeps the flags in sync so the same code
        // path lights the selected row.
        bool sel = m_items[i].selected;
        bool hov = (i == m_hoverIdx);
        if (sel || hov)
        {
            HBRUSH bg = ::CreateSolidBrush(sel ? m_clrSelBg : m_clrHoverBg);
            ::FillRect(hdc, &rc, bg);
            ::DeleteObject(bg);
        }

        int textLeft = rc.left + kRowTextLeftPadPx;
        if (m_showCheckboxes)
        {
            int cy = (rc.top + rc.bottom - kCheckboxGlyphPx) / 2;
            RECT cb = { rc.left + kCheckboxLeftPadPx, cy,
                        rc.left + kCheckboxLeftPadPx + kCheckboxGlyphPx, cy + kCheckboxGlyphPx };
            HBRUSH cbBg = ::CreateSolidBrush(kCheckboxFillWhite);
            HPEN cbPen = ::CreatePen(PS_SOLID, 1, kCheckboxBorder);
            HPEN op = (HPEN)::SelectObject(hdc, cbPen);
            HBRUSH ob = (HBRUSH)::SelectObject(hdc, cbBg);
            ::Rectangle(hdc, cb.left, cb.top, cb.right, cb.bottom);
            ::SelectObject(hdc, ob);
            ::SelectObject(hdc, op);
            ::DeleteObject(cbBg);
            ::DeleteObject(cbPen);
            if (m_items[i].checked)
            {
                // Plain check tick (axis-aligned, no DuiAA dep here).
                HPEN tickPen = ::CreatePen(PS_SOLID, kCheckTickStrokePx, kAccentBlue);
                HPEN op2 = (HPEN)::SelectObject(hdc, tickPen);
                ::MoveToEx(hdc, cb.left + 3, cb.top + 7, nullptr);
                ::LineTo  (hdc, cb.left + 6, cb.top + 10);
                ::LineTo  (hdc, cb.left + 11, cb.top + 4);
                ::SelectObject(hdc, op2);
                ::DeleteObject(tickPen);
            }
            textLeft = rc.left + kCheckColW + kCheckboxLeftPadPx;
        }

        COLORREF clr = sel ? m_clrSelText : m_clrText;
        COLORREF oldClr = ::SetTextColor(hdc, clr);
        RECT rText = rc;
        rText.left = textLeft;
        ::DrawText(hdc, m_items[i].text, -1, &rText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        ::SetTextColor(hdc, oldClr);
    }
    ::SetBkMode(hdc, oldBk);

    // Drag-reorder insertion line.
    if (m_dragSrcIdx >= 0 && m_dragDropIdx >= 0)
    {
        int y = m_rcItem.top + m_dragDropIdx * m_itemH - (m_sb ? m_sb->GetPos() : 0);
        if (y >= m_rcItem.top && y <= m_rcItem.bottom)
        {
            HPEN pen = ::CreatePen(PS_SOLID, kDragLineStrokePx, kAccentBlue);
            HPEN op  = (HPEN)::SelectObject(hdc, pen);
            ::MoveToEx(hdc, m_rcItem.left + 1, y, nullptr);
            ::LineTo  (hdc, m_rcItem.left + bodyW - 1, y);
            ::SelectObject(hdc, op);
            ::DeleteObject(pen);
        }
    }

    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
    ::SelectClipRgn(hdc, hasOld == 1 ? rgnOld : nullptr);
    ::DeleteObject(rgn);
    ::DeleteObject(rgnOld);

    if (m_sb && m_sb->IsVisible())
    {
        RECT inter;
        if (::IntersectRect(&inter, &m_sb->GetRect(), &rcDirty))
        {
            m_sb->OnPaint(hdc, inter);
        }
    }
}

bool DuiListBox::OnLButtonDown(POINT pt, UINT mkFlags)
{
    int idx = IndexFromPoint(pt);
    if (idx < 0)
    {
        return true;
    }

    // Checkbox column click: toggle, don't move selection.
    if (m_showCheckboxes && pt.x < m_rcItem.left + kCheckColW)
    {
        SetItemChecked(idx, !m_items[idx].checked);
        return true;
    }

    bool ctrl  = (mkFlags & MK_CONTROL) != 0;
    bool shift = (mkFlags & MK_SHIFT)   != 0;

    if (m_multiSelect && shift && m_curSel >= 0)
    {
        // Range-extend from anchor.
        int a = m_curSel, b = idx;
        if (a > b)
        {
            int t = a;
            a = b;
            b = t;
        }
        for (int i = 0; i < (int)m_items.size(); ++i)
        {
            m_items[i].selected = (i >= a && i <= b);
        }
        m_curSel = idx;
        Invalidate();
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)idx);
    }
    else if (m_multiSelect && ctrl)
    {
        // Toggle this item.
        SetItemSelected(idx, !m_items[idx].selected);
    }
    else
    {
        // Plain click -- select only this row.
        SetCurSel(idx);
    }

    // Drag-reorder: arm the source row; actual move waits for OnLButtonUp.
    if (m_dragReorder)
    {
        m_dragSrcIdx  = idx;
        m_dragDropIdx = -1;
        Capture();
    }
    return true;
}

bool DuiListBox::OnLButtonUp(POINT pt, UINT)
{
    if (m_dragSrcIdx >= 0)
    {
        int src  = m_dragSrcIdx;
        int drop = m_dragDropIdx;
        m_dragSrcIdx  = -1;
        m_dragDropIdx = -1;
        if (m_bCapture)
        {
            ReleaseCapture();
        }
        if (drop >= 0 && drop != src && drop != src + 1)
        {
            MoveItem(src, drop);
        }
        Invalidate();
    }
    return true;
}

bool DuiListBox::OnLButtonDblClk(POINT pt, UINT)
{
    int idx = IndexFromPoint(pt);
    if (idx < 0)
    {
        return true;
    }
    SetCurSel(idx);
    NotifyParent(DUIN_DBLCLK, (LPARAM)idx);
    return true;
}

bool DuiListBox::OnMouseMove(POINT pt, UINT)
{
    // 鼠标在 list 内移动 → scrollbar fade in（auto-hide 模式下）
    if (m_sb) { m_sb->TriggerShow(); }

    int idx = IndexFromPoint(pt);

    // Drag-reorder: compute insertion slot — top half of row -- before
    // it; bottom half -- after.
    if (m_dragSrcIdx >= 0)
    {
        int rel = (pt.y - m_rcItem.top) + (m_sb ? m_sb->GetPos() : 0);
        int row = rel / m_itemH;
        if (row < 0)
        {
            row = 0;
        }
        if (row > (int)m_items.size())
        {
            row = (int)m_items.size();
        }
        int slot = row;
        if (row < (int)m_items.size())
        {
            int rowTop = row * m_itemH;
            if (rel - rowTop > m_itemH / 2)
            {
                slot = row + 1;
            }
        }
        if (slot != m_dragDropIdx)
        {
            m_dragDropIdx = slot;
            Invalidate();
        }
        return true;
    }

    if (idx == m_hoverIdx)
    {
        return false;
    }
    m_hoverIdx = idx;
    Invalidate();
    return false;
}

bool DuiListBox::OnMouseLeave()
{
    if (m_hoverIdx != -1)
    {
        m_hoverIdx = -1;
        Invalidate();
    }
    if (m_sb) { m_sb->StartFadeOut(); }   // auto-hide：离开就开始渐隐
    return DuiControl::OnMouseLeave();
}

bool DuiListBox::OnMouseWheel(POINT pt, short zDelta, UINT mkFlags)
{
    if (m_sb)
    {
        m_sb->TriggerShow();
        return m_sb->OnMouseWheel(pt, zDelta, mkFlags);
    }
    return false;
}

bool DuiListBox::OnKeyDown(UINT vk, UINT)
{
    if (m_items.empty())
    {
        return false;
    }
    int next = m_curSel;
    switch (vk)
    {
    case VK_UP:
        next = (m_curSel <= 0) ? 0 : m_curSel - 1;
        break;
    case VK_DOWN:
        next = (m_curSel >= (int)m_items.size() - 1) ? (int)m_items.size() - 1 : m_curSel + 1;
        break;
    case VK_HOME:
        next = 0;
        break;
    case VK_END:
        next = (int)m_items.size() - 1;
        break;
    case VK_PRIOR:
        next = m_curSel - RowsVisible();
        if (next < 0)
        {
            next = 0;
        }
        break;
    case VK_NEXT:
        next = m_curSel + RowsVisible();
        if (next > (int)m_items.size() - 1)
        {
            next = (int)m_items.size() - 1;
        }
        break;
    default:
        return false;
    }
    if (next != m_curSel)
    {
        SetCurSel(next);
        EnsureVisible(next);
    }
    return true;
}

// =====================================================================
// DuiVirtualList
// =====================================================================

DuiVirtualList::DuiVirtualList()
{
    SetTabStop(true);
    auto sb = std::unique_ptr<DuiScrollBar>(new DuiScrollBar(/*horizontal=*/false));
    m_sb = sb.get();
    m_sb->SetOnScroll(&DuiVirtualList::OnSbScrolledStub, this);
    m_sb->SetAutoHide(true);    // 默认 auto-hide
    DuiControl::AddChild(std::move(sb));
}

void DuiVirtualList::SetRowCount(int n)
{
    if (n < 0)
    {
        n = 0;
    }
    m_rowCount = n;
    if (m_curSel >= n)
    {
        m_curSel = -1;
    }
    if (m_hoverIdx >= n)
    {
        m_hoverIdx = -1;
    }
    UpdateScrollRange();
    Invalidate();
}

void DuiVirtualList::SetRowHeight(int h)
{
    if (h < 1)
    {
        h = 1;
    }
    if (m_rowH == h)
    {
        return;
    }
    m_rowH = h;
    UpdateScrollRange();
    Invalidate();
}

void DuiVirtualList::SetCurSel(int idx, bool notify)
{
    if (idx < -1 || idx >= m_rowCount)
    {
        return;
    }
    if (m_curSel == idx)
    {
        return;
    }
    m_curSel = idx;
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_curSel);
    }
}

void DuiVirtualList::EnsureVisible(int idx)
{
    if (idx < 0 || idx >= m_rowCount || !m_sb)
    {
        return;
    }
    int top = idx * m_rowH;
    int bot = top + m_rowH;
    int viewH = m_rcItem.bottom - m_rcItem.top;
    int pos = m_sb->GetPos();
    if (top < pos)
    {
        pos = top;
    }
    else if (bot > pos + viewH)
    {
        pos = bot - viewH;
    }
    m_sb->SetPos(pos, /*notify=*/false);
    Invalidate();
}

int DuiVirtualList::GetScrollPos() const
{
    return m_sb ? m_sb->GetPos() : 0;
}

void DuiVirtualList::SetScrollPos(int p)
{
    if (m_sb)
    {
        m_sb->SetPos(p);
    }
}

int DuiVirtualList::BodyWidth() const
{
    int w = m_rcItem.right - m_rcItem.left;
    return (m_sb && m_sb->IsVisible()) ? w - m_sbWidth : w;
}

int DuiVirtualList::RowsVisible() const
{
    int viewH = m_rcItem.bottom - m_rcItem.top;
    return viewH / m_rowH;
}

void DuiVirtualList::OnSbScrolledStub(void* user, int)
{
    static_cast<DuiVirtualList*>(user)->Invalidate();
}

void DuiVirtualList::UpdateScrollRange()
{
    if (!m_sb)
    {
        return;
    }
    int viewH = m_rcItem.bottom - m_rcItem.top;
    int contentH = m_rowCount * m_rowH;
    int over = contentH - viewH;
    if (over < 0)
    {
        over = 0;
    }
    m_sb->SetRange(0, over);
    m_sb->SetPage(viewH > 0 ? viewH : 1);
    m_sb->SetLineSize(m_rowH);
    m_sb->SetVisible(contentH > viewH);
}

int DuiVirtualList::IndexFromPoint(POINT pt) const
{
    if (!::PtInRect(&m_rcItem, pt))
    {
        return -1;
    }
    if (pt.x >= m_rcItem.left + BodyWidth())
    {
        return -1;
    }
    int rel = (pt.y - m_rcItem.top) + (m_sb ? m_sb->GetPos() : 0);
    int idx = rel / m_rowH;
    if (idx < 0 || idx >= m_rowCount)
    {
        return -1;
    }
    return idx;
}

void DuiVirtualList::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    UpdateScrollRange();
    if (m_sb && m_sb->IsVisible())
    {
        RECT rcSb = { m_rcItem.right - m_sbWidth, m_rcItem.top,
                      m_rcItem.right,             m_rcItem.bottom };
        m_sb->SetRect(rcSb);
    }
}

DuiControl* DuiVirtualList::HitTest(POINT pt)
{
    if (!m_bVisible || !::PtInRect(&m_rcItem, pt))
    {
        return nullptr;
    }
    if (m_sb && m_sb->IsVisible() && ::PtInRect(&m_sb->GetRect(), pt))
    {
        return m_sb;
    }
    return m_bEnabled ? this : nullptr;
}

void DuiVirtualList::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    HBRUSH hbr = ::CreateSolidBrush(kCheckboxFillWhite);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    HPEN hpen = ::CreatePen(PS_SOLID, 1, kListBoxBorder);
    HPEN oldPen = (HPEN)::SelectObject(hdc, hpen);
    HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    ::Rectangle(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom);
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(hpen);

    int bodyW = BodyWidth();
    RECT rcClip = { m_rcItem.left + 1, m_rcItem.top + 1,
                    m_rcItem.left + bodyW, m_rcItem.bottom - 1 };
    HRGN rgn = ::CreateRectRgnIndirect(&rcClip);
    HRGN rgnOld = ::CreateRectRgn(0, 0, 0, 0);
    int hasOld = ::GetClipRgn(hdc, rgnOld);
    ::SelectClipRgn(hdc, rgn);

    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;

    int firstVisible = (m_sb ? m_sb->GetPos() : 0) / m_rowH;
    int lastVisible  = firstVisible + RowsVisible() + 1;
    if (lastVisible > m_rowCount)
    {
        lastVisible = m_rowCount;
    }

    if (m_paint)
    {
        for (int i = firstVisible; i < lastVisible; ++i)
        {
            int top = m_rcItem.top + i * m_rowH - (m_sb ? m_sb->GetPos() : 0);
            RECT rc = { m_rcItem.left + 1, top, m_rcItem.left + bodyW, top + m_rowH };
            RECT inter;
            if (!::IntersectRect(&inter, &rc, &rcDirty))
            {
                continue;
            }
            bool sel = (i == m_curSel);
            bool hov = (i == m_hoverIdx);
            m_paint(m_paintUser, hdc, i, rc, sel, hov);
        }
    }

    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
    ::SelectClipRgn(hdc, hasOld == 1 ? rgnOld : nullptr);
    ::DeleteObject(rgn);
    ::DeleteObject(rgnOld);

    if (m_sb && m_sb->IsVisible())
    {
        RECT inter;
        if (::IntersectRect(&inter, &m_sb->GetRect(), &rcDirty))
        {
            m_sb->OnPaint(hdc, inter);
        }
    }
}

bool DuiVirtualList::OnLButtonDown(POINT pt, UINT)
{
    int idx = IndexFromPoint(pt);
    if (idx < 0)
    {
        return true;
    }
    SetCurSel(idx);
    if (m_click)
    {
        m_click(m_clickUser, idx, /*dbl=*/false);
    }
    return true;
}

bool DuiVirtualList::OnLButtonDblClk(POINT pt, UINT)
{
    int idx = IndexFromPoint(pt);
    if (idx < 0)
    {
        return true;
    }
    SetCurSel(idx);
    if (m_click)
    {
        m_click(m_clickUser, idx, /*dbl=*/true);
    }
    NotifyParent(DUIN_DBLCLK, (LPARAM)idx);
    return true;
}

bool DuiVirtualList::OnMouseMove(POINT pt, UINT)
{
    if (m_sb) { m_sb->TriggerShow(); }
    int idx = IndexFromPoint(pt);
    if (idx == m_hoverIdx)
    {
        return false;
    }
    m_hoverIdx = idx;
    Invalidate();
    return false;
}

bool DuiVirtualList::OnMouseLeave()
{
    if (m_hoverIdx != -1)
    {
        m_hoverIdx = -1;
        Invalidate();
    }
    if (m_sb) { m_sb->StartFadeOut(); }
    return DuiControl::OnMouseLeave();
}

bool DuiVirtualList::OnMouseWheel(POINT pt, short zDelta, UINT mkFlags)
{
    if (m_sb)
    {
        m_sb->TriggerShow();
        return m_sb->OnMouseWheel(pt, zDelta, mkFlags);
    }
    return false;
}

bool DuiVirtualList::OnKeyDown(UINT vk, UINT)
{
    if (m_rowCount <= 0)
    {
        return false;
    }
    int next = m_curSel;
    switch (vk)
    {
    case VK_UP:
        next = (m_curSel <= 0) ? 0 : m_curSel - 1;
        break;
    case VK_DOWN:
        next = (m_curSel >= m_rowCount - 1) ? m_rowCount - 1 : m_curSel + 1;
        break;
    case VK_HOME:
        next = 0;
        break;
    case VK_END:
        next = m_rowCount - 1;
        break;
    case VK_PRIOR:
        next = m_curSel - RowsVisible();
        if (next < 0)
        {
            next = 0;
        }
        break;
    case VK_NEXT:
        next = m_curSel + RowsVisible();
        if (next > m_rowCount - 1)
        {
            next = m_rowCount - 1;
        }
        break;
    default:
        return false;
    }
    if (next != m_curSel)
    {
        SetCurSel(next);
        EnsureVisible(next);
    }
    return true;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_LISTBOX
