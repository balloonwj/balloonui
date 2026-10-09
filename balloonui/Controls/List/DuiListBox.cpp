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

// ---- Per-item delete cross ---------------------------------------------------
// Idle color of the delete cross - light gray, present but visually quiet so
// a list full of rows does not read as a wall of crosses.
const COLORREF kDeleteCrossIdle     = RGB(176, 176, 182);
// Delete cross on the hovered row - darker, hinting it is actionable.
const COLORREF kDeleteCrossRowHover = RGB(120, 120, 128);
// Delete cross with the mouse directly over it - red, the usual "this
// destroys something" cue.
const COLORREF kDeleteCrossHot      = RGB(214,  70,  70);

// ---- 每行图标与副文字（2026-10-04 起）----------------------------------------
// 图标与其右侧文字之间的间距（像素）。
const int kIconTextGapPx            = 8;
// 主文字与其右侧副文字之间的间距（像素）。
const int kSubTextGapPx             = 12;
// AlphaBlend 的整体不透明度：图标自带逐像素 alpha，整体不再叠加透明度。
const BYTE kIconConstantAlpha       = 255;

// 把一个 32 位预乘 alpha 位图按 px × px 画到 (x, y)。位图取不到尺寸时什么都不画。
//   hdc：目标设备上下文；hbm：图标位图（调用方持有）；x、y：左上角；px：边长（像素）。
void DrawItemIcon(HDC hdc, HBITMAP hbm, int x, int y, int px)
{
    BITMAP bm;
    if (::GetObject(hbm, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight <= 0)
    {
        return;
    }
    HDC mem = ::CreateCompatibleDC(hdc);
    if (mem == NULL)
    {
        return;
    }
    HGDIOBJ old = ::SelectObject(mem, hbm);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, kIconConstantAlpha, AC_SRC_ALPHA };
    ::AlphaBlend(hdc, x, y, px, px, mem, 0, 0, bm.bmWidth, bm.bmHeight, bf);
    ::SelectObject(mem, old);
    ::DeleteDC(mem);
}

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
    m_sb->SetAutoHide(true);    // 默认 auto-hide：滚动时淡入、鼠标悬停在滚动条上保持可见，停手后淡出
    DuiControl::AddChild(std::move(sb));
}

int DuiListBox::AddItem(LPCTSTR text, LPARAM lParam)
{
    Item it;
    it.text = text ? text : _T("");
    it.lParam = lParam;
    it.selected = false;
    it.checked  = false;
    it.icon     = NULL;
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
    it.icon     = NULL;
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

void DuiListBox::SetItemIcon(int index, HBITMAP hbm)
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return;
    }
    m_items[index].icon = hbm;
    Invalidate();
}

HBITMAP DuiListBox::GetItemIcon(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return NULL;
    }
    return m_items[index].icon;
}

void DuiListBox::SetItemSubText(int index, LPCTSTR text)
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return;
    }
    m_items[index].subText = text ? text : _T("");
    Invalidate();
}

CString DuiListBox::GetItemSubText(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return CString();
    }
    return m_items[index].subText;
}

void DuiListBox::SetIconSize(int px)
{
    m_iconSize = (px < 1) ? 1 : px;
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

void DuiListBox::SetShowItemDelete(bool b)
{
    if (m_showItemDelete == b)
    {
        return;
    }
    m_showItemDelete = b;
    m_deleteHoverIdx = -1;
    Invalidate();
}

RECT DuiListBox::DeleteButtonRect(int index) const
{
    if (!m_showItemDelete)
    {
        return RECT{ 0, 0, 0, 0 };
    }

    // 贴着行的右边界往左让出一个删除列。滚动条是悬浮式的（2026-10-04 起行按全宽排版），
    // 出现时整条命中带都归滚动条，删除叉再往左让开命中带，否则点不到也会被滑块压住。
    const RECT rc = ItemRect(index);
    const int right = (m_sb && m_sb->IsVisible()) ? rc.right - m_sbWidth : rc.right;
    return RECT{ right - kDeleteColW, rc.top, right, rc.bottom };
}

bool DuiListBox::HitDeleteButton(POINT pt, int index) const
{
    if (!m_showItemDelete || index < 0 || index >= (int)m_items.size())
    {
        return false;
    }

    const RECT rc = DeleteButtonRect(index);
    return pt.x >= rc.left && pt.x < rc.right && pt.y >= rc.top && pt.y < rc.bottom;
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
    // 真的滚动了才淡入滚动条（键盘翻页、选中项滚到可见）；本来就可见时不惊动它
    if (pos != m_sb->GetPos())
    {
        m_sb->SetPos(pos, /*notify=*/false);
        m_sb->TriggerShow();
    }
    Invalidate();
}

int DuiListBox::BodyWidth() const
{
    // 悬浮式滚动条（2026-10-04 起）：行按全宽排版，滚动条浮在右缘之上，不再扣掉它的宽度
    return m_rcItem.right - m_rcItem.left;
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
    // 滚动条变为可见时同时放到列表右侧。本函数在排版之后也会被调用（添加 / 删除项、
    // 改行高），若只切换显隐，滚动条会停在空矩形上：画不出来，也命中不到、无法拖动
    //（2026-10-03 修复）。隐藏时保留原矩形即可，HitTest 与绘制都先判断可见性。
    if (needBar)
    {
        RECT rcSb = { m_rcItem.right - m_sbWidth, m_rcItem.top,
                      m_rcItem.right,             m_rcItem.bottom };
        m_sb->SetRect(rcSb);
    }
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
    // 滚动范围、显隐与滚动条的矩形都在 UpdateScrollRange 里一并处理
    UpdateScrollRange();
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

    // 把行裁到 body。用 SaveDC + IntersectClipRect + RestoreDC 而不是
    // SelectClipRgn —— 后者是替换语义,嵌套在外层 ScrollView 里时会撤销外层
    // 设的 clip,导致 ListBox 内容能画到外层 m_rcItem 之外(运行时已复现)。
    int bodyW = BodyWidth();
    int dcSaved = ::SaveDC(hdc);
    ::IntersectClipRect(hdc, m_rcItem.left + 1, m_rcItem.top + 1,
                        m_rcItem.left + bodyW, m_rcItem.bottom - 1);

    HFONT useFont = GetDefaultFont();
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

        // 行左侧图标：按图标边长在行内垂直居中，文字随之右移。没设图标的行不受影响。
        if (m_items[i].icon != NULL)
        {
            const int iconTop = (rc.top + rc.bottom - m_iconSize) / 2;
            DrawItemIcon(hdc, m_items[i].icon, textLeft, iconTop, m_iconSize);
            textLeft += m_iconSize + kIconTextGapPx;
        }

        COLORREF clr = sel ? m_clrSelText : m_clrText;
        COLORREF oldClr = ::SetTextColor(hdc, clr);
        RECT rText = rc;
        rText.left = textLeft;
        // 开了删除列就把文字右边界收到删除叉左边，否则长文本会被省略号盖到叉底下。
        if (m_showItemDelete)
        {
            rText.right = DeleteButtonRect((int)i).left;
        }
        if (m_items[i].subText.IsEmpty())
        {
            ::DrawText(hdc, m_items[i].text, -1, &rText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        else
        {
            // 有副文字：主文字只占它实际需要的宽度（放不下时以省略号截断），副文字接在右边
            const CString& mainText = m_items[i].text;
            SIZE mainSize = { 0, 0 };
            ::GetTextExtentPoint32(hdc, mainText, mainText.GetLength(), &mainSize);
            RECT rMain = rText;
            if (rMain.left + mainSize.cx < rMain.right)
            {
                rMain.right = rMain.left + mainSize.cx;
            }
            ::DrawText(hdc, mainText, -1, &rMain, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT rSub = rText;
            rSub.left = rMain.right + kSubTextGapPx;
            if (rSub.left < rSub.right)
            {
                ::SetTextColor(hdc, sel ? m_clrSelText : m_clrSubText);
                ::DrawText(hdc, m_items[i].subText, -1, &rSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
        ::SetTextColor(hdc, oldClr);

        // 行右侧的删除叉。三档配色：平时淡灰（不喧宾夺主）、鼠标在本行时加深
        // （提示可点）、鼠标正压在叉上时变红（提示这一下会删东西）。
        if (m_showItemDelete)
        {
            COLORREF crossColor = kDeleteCrossIdle;
            if ((int)i == m_deleteHoverIdx)
            {
                crossColor = kDeleteCrossHot;
            }
            else if (hov)
            {
                crossColor = kDeleteCrossRowHover;
            }

            const RECT rcDel = DeleteButtonRect((int)i);
            const int cx = (rcDel.left + rcDel.right) / 2;
            const int cy = (rcDel.top + rcDel.bottom) / 2;
            const int half = kDeleteGlyphPx / 2;

            HPEN crossPen = ::CreatePen(PS_SOLID, kDeleteStrokePx, crossColor);
            HPEN oldPen = (HPEN)::SelectObject(hdc, crossPen);
            ::MoveToEx(hdc, cx - half, cy - half, nullptr);
            ::LineTo  (hdc, cx + half + 1, cy + half + 1);
            ::MoveToEx(hdc, cx + half, cy - half, nullptr);
            ::LineTo  (hdc, cx - half - 1, cy + half + 1);
            ::SelectObject(hdc, oldPen);
            ::DeleteObject(crossPen);
        }
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
    ::RestoreDC(hdc, dcSaved);

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

    // 删除列点击：只发通知，不动选中行、也不动自己的 model。要不要弹二次确认、
    // 确认后删哪些东西，全由业务侧决定（见头文件 SetShowItemDelete 的说明）。
    if (HitDeleteButton(pt, idx))
    {
        NotifyParent((UINT)DUITN_ITEMDELETE, (LPARAM)idx);
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
    // 鼠标在行上移动不唤出滚动条（2026-10-04 起按 UI 约定：滚动时、或鼠标移进滚动条的
    // 命中带时才淡入，后者由滚动条自己的 OnMouseEnter 处理）
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

    // 鼠标是否正压在某一行的删除叉上 —— 只影响那个叉的颜色，与行 hover 分开记，
    // 因为在同一行里横向移进 / 移出叉也要重画。
    const int deleteHover = (m_showItemDelete && idx >= 0 && HitDeleteButton(pt, idx))
                            ? idx : -1;
    if (deleteHover != m_deleteHoverIdx)
    {
        m_deleteHoverIdx = deleteHover;
        Invalidate();
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
    if (m_deleteHoverIdx != -1)
    {
        m_deleteHoverIdx = -1;
        Invalidate();
    }
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
    // 真的滚动了才淡入滚动条（键盘翻页、选中项滚到可见）；本来就可见时不惊动它
    if (pos != m_sb->GetPos())
    {
        m_sb->SetPos(pos, /*notify=*/false);
        m_sb->TriggerShow();
    }
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
    // 悬浮式滚动条（2026-10-04 起）：行按全宽排版，滚动条浮在右缘之上，不再扣掉它的宽度
    return m_rcItem.right - m_rcItem.left;
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
    bool needBar = contentH > viewH;
    m_sb->SetVisible(needBar);
    // 与 DuiListBox::UpdateScrollRange 相同：排版之后再设行数或行高时，滚动条变为可见的
    // 同时放到列表右侧，否则它停在空矩形上，画不出来也无法拖动
    if (needBar)
    {
        RECT rcSb = { m_rcItem.right - m_sbWidth, m_rcItem.top,
                      m_rcItem.right,             m_rcItem.bottom };
        m_sb->SetRect(rcSb);
    }
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
    // 滚动范围、显隐与滚动条的矩形都在 UpdateScrollRange 里一并处理
    UpdateScrollRange();
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

    // 把行裁到 body。同 DuiListBox::OnPaint 的说明:必须用 SaveDC +
    // IntersectClipRect + RestoreDC,不能用 SelectClipRgn(替换语义会撤
    // 销外层 ScrollView 的 clip,运行时已复现 VL 行越界画到 tab 上)。
    int bodyW = BodyWidth();
    int dcSaved = ::SaveDC(hdc);
    ::IntersectClipRect(hdc, m_rcItem.left + 1, m_rcItem.top + 1,
                        m_rcItem.left + bodyW, m_rcItem.bottom - 1);

    HFONT useFont = GetDefaultFont();
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
    ::RestoreDC(hdc, dcSaved);

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
    // 与 DuiListBox::OnMouseMove 相同：鼠标在行上移动不唤出滚动条
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
