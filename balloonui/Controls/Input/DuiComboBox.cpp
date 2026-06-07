#include "stdafx.h"
#include "DuiComboBox.h"

#if BUI_FEATURE_COMBOBOX

#include "../List/DuiListBox.h"
#include "DuiEditHost.h"
#include "../../DuiHost.h"
#include "../../DuiResMgr.h"
#include "../../DuiNotify.h"
#include "../../DuiPaintAA.h"

namespace balloonwjui {

namespace {

// Combo body fill / border colors. The body is rounded with kCornerPx and
// the border tracks input state so the user can see hover / open / focus
// the same way DuiEditHost does. Disabled drops to a muted gray.
const COLORREF kBgDisabled     = RGB(240, 240, 240);   // flat light gray
const COLORREF kBorderDisabled = RGB(190, 190, 190);   // muted gray
const COLORREF kBorderActive   = RGB( 80, 130, 200);   // hover OR popup-open
const COLORREF kBorderNormal   = RGB(150, 150, 150);   // resting medium gray

// Down arrow on the right side. Color follows enabled state; the disabled
// variant is light gray so the arrow visibly fades along with the border.
const COLORREF kArrowEnabled   = RGB( 80, 100, 140);   // dark blue-gray
const COLORREF kArrowDisabled  = RGB(160, 160, 160);   // matches text-disabled

// Selected-item text color (read-only style; editable mode lets the EDIT
// paint itself). Disabled fades to mid-gray so it still reads as text but
// is clearly not interactive.
const COLORREF kTextEnabled    = RGB( 30,  30,  30);   // near-black body text
const COLORREF kTextDisabled   = RGB(140, 140, 140);

// Body geometry.
const int kCornerPx     = 6;    // rounded-rect corner radius for the body
const int kArrowWPx     = 14;   // down-arrow triangle width
const int kArrowHPx     = 8;    // down-arrow triangle height
const int kArrowPadRPx  = 8;    // gap between arrow and right border
const int kTextPadLPx   = 8;    // text indent past left border (read-only)
const int kTextArrowGap = 4;    // min gap between text right edge and arrow

} // namespace

// ---------------------------------------------------------------------------
// DuiComboEdit: the embedded EDIT child used by editable-mode combo. Routes
// EN_CHANGE to the owning combo and suppresses the would-be VALUECHANGED
// bubble from the EDIT's own ctrlId, so callers see exactly one
// VALUECHANGED notification per change (on the combo's ctrlId).
// ---------------------------------------------------------------------------
class DuiComboEdit : public DuiEditHost
{
public:
    void SetCombo(DuiComboBox* c) { m_combo = c; }

    // Override to redirect notifications to the combo instead of bubbling
    // them as the embedded EDIT.
    void OnHwndCommand(UINT enCode) override
    {
        switch (enCode)
        {
        case EN_CHANGE:
            RefreshCacheFromHwnd();
            Invalidate();
            if (m_combo)
            {
                m_combo->OnEditTextChanged();
            }
            break;
        case EN_SETFOCUS:
            Test_SetFocused(true);
            Invalidate();
            // Intentionally NOT calling NotifyParent: the combo decides
            // when (if ever) to bubble focus events on its own ctrlId.
            break;
        case EN_KILLFOCUS:
            Test_SetFocused(false);
            Invalidate();
            break;
        default:
            break;
        }
    }

private:
    DuiComboBox* m_combo = nullptr;
};

// =====================================================================
// DuiComboBoxPopup: a borderless top-level window that owns a DuiHost
// containing a DuiListBox. Lives only while a combo's popup is open.
// =====================================================================

class DuiComboBoxPopup : public CWindowImpl<DuiComboBoxPopup, CWindow>
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiComboBoxPopup__"), 0, COLOR_WINDOW)

    DuiComboBoxPopup() = default;
    ~DuiComboBoxPopup() = default;

    BEGIN_MSG_MAP(DuiComboBoxPopup)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_KILLFOCUS(OnKillFocus)
        MSG_WM_ACTIVATE(OnActivate)
        MESSAGE_HANDLER_EX(WM_DUI_NOTIFY, OnDuiNotify)
    END_MSG_MAP()

    void Open(DuiComboBox* owner, const RECT& screenRc,
              const std::vector<CString>& items, int curSel, int itemH);

    // Standard WTL hook: runs after WM_NCDESTROY, never inside one of our
    // own message handlers, so 'delete this' is safe here.
    void OnFinalMessage(HWND /*hWnd*/) override
    {
        if (m_owner)
        {
            m_owner->OnPopupClosed();
        }
        delete this;
    }

    // The owner sets this to nullptr if it dies before us, so our deferred
    // close path never calls back into freed memory.
    void DetachOwner() { m_owner = nullptr; }

private:
    int     OnCreate(LPCREATESTRUCT);
    void    OnDestroy();
    void    OnKillFocus(CWindow);
    void    OnActivate(UINT nState, BOOL bMin, CWindow other);
    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lParam);

    // Defer destruction: PostMessage(WM_CLOSE) is dispatched AFTER our
    // current handler returns, so DestroyWindow + WM_NCDESTROY +
    // OnFinalMessage (which deletes us) never fires from the same call
    // stack as the handler that triggered the close.
    void RequestClose()
    {
        if (!m_closeRequested && m_hWnd)
        {
            m_closeRequested = true;
            PostMessage(WM_CLOSE);
        }
    }

private:
    DuiComboBox*  m_owner = nullptr;
    DuiHost       m_host;
    DuiListBox*   m_list  = nullptr;
    bool          m_closeRequested = false;
};

void DuiComboBoxPopup::Open(DuiComboBox* owner, const RECT& screenRc,
                            const std::vector<CString>& items, int curSel, int itemH)
{
    m_owner = owner;
    Create(NULL, (RECT*)&screenRc, NULL, WS_POPUP | WS_BORDER, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
    if (!m_hWnd)
    {
        return;
    }
    SetWindowPos(NULL, screenRc.left, screenRc.top,
                 screenRc.right - screenRc.left, screenRc.bottom - screenRc.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    m_host.Create(m_hWnd, CWindow::rcDefault, NULL,
                  WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);
    CRect rcClient;
    GetClientRect(&rcClient);
    m_host.SetWindowPos(NULL, 0, 0, rcClient.Width(), rcClient.Height(),
                        SWP_NOZORDER | SWP_NOACTIVATE);

    auto lb = std::unique_ptr<DuiListBox>(new DuiListBox());
    lb->SetCtrlId(1);
    lb->SetItemHeight(itemH);
    for (size_t i = 0; i < items.size(); ++i)
    {
        lb->AddItem(items[i], (LPARAM)i);
    }
    if (curSel >= 0 && curSel < (int)items.size())
    {
        lb->SetCurSel(curSel, /*notify=*/false);
    }
    m_list = lb.get();
    m_host.SetRoot(std::move(lb));

    ShowWindow(SW_SHOWNA);
    SetForegroundWindow(m_hWnd);
    SetFocus();
}

int DuiComboBoxPopup::OnCreate(LPCREATESTRUCT) { return 0; }

void DuiComboBoxPopup::OnDestroy()
{
    if (m_host.IsWindow())
    {
        m_host.DestroyWindow();
    }
}

void DuiComboBoxPopup::OnActivate(UINT nState, BOOL, CWindow)
{
    // WA_INACTIVE means another window took focus; close (asynchronously).
    if (nState == WA_INACTIVE)
    {
        RequestClose();
    }
}

void DuiComboBoxPopup::OnKillFocus(CWindow)
{
    // Mirror activation behavior - any focus loss kills the popup.
    RequestClose();
}

LRESULT DuiComboBoxPopup::OnDuiNotify(UINT, WPARAM, LPARAM lParam)
{
    DuiNotify* n = reinterpret_cast<DuiNotify*>(lParam);
    if (!n)
    {
        return 0;
    }
    if (n->code == DUIN_VALUECHANGED && m_owner)
    {
        // Write back to combo first; then schedule our own teardown to run
        // after this handler unwinds. Calling DestroyWindow synchronously
        // here would re-enter our own KillFocus / Activate handlers and,
        // via OnFinalMessage delete-self, leave 'this' dangling on return.
        m_owner->OnPopupSelected((int)n->extra);
        RequestClose();
    }
    return 0;
}

// =====================================================================
// DuiComboBox
// =====================================================================

DuiComboBox::DuiComboBox()
{
    SetTabStop(true);
}

DuiComboBox::~DuiComboBox()
{
    ClosePopup();
}

int DuiComboBox::AddString(LPCTSTR sz)
{
    m_items.push_back(sz ? CString(sz) : CString());
    Invalidate();
    return (int)m_items.size() - 1;
}

void DuiComboBox::DeleteString(int index)
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
    Invalidate();
}

void DuiComboBox::ResetContent()
{
    m_items.clear();
    m_curSel = -1;
    Invalidate();
}

CString DuiComboBox::GetItemText(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return CString();
    }
    return m_items[index];
}

void DuiComboBox::SetItemText(int index, LPCTSTR sz)
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return;
    }
    m_items[index] = sz ? sz : _T("");
    Invalidate();
}

void DuiComboBox::SetCurSel(int index, bool notify)
{
    if (index < -1 || index >= (int)m_items.size())
    {
        return;
    }
    if (m_curSel == index)
    {
        return;
    }
    m_curSel = index;
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_curSel);
    }
}

void DuiComboBox::OpenPopup()
{
    if (m_popupOpen || m_items.empty() || !m_pHost)
    {
        return;
    }
    HWND hostHwnd = m_pHost->m_hWnd;
    if (!hostHwnd || !::IsWindow(hostHwnd))
    {
        return;
    }

    // Decide which items to show. Incremental-search filter (when active)
    // narrows the list; the filter map also remaps "selected popup index"
    // back to the original item index in OnPopupSelected.
    std::vector<CString> popupItems;
    int popupCurSel = -1;
    if (m_incSearch && !m_filteredIndices.empty())
    {
        popupItems.reserve(m_filteredIndices.size());
        for (size_t k = 0; k < m_filteredIndices.size(); ++k)
        {
            int realIdx = m_filteredIndices[k];
            if (realIdx >= 0 && realIdx < (int)m_items.size())
            {
                popupItems.push_back(m_items[realIdx]);
                if (realIdx == m_curSel)
                {
                    popupCurSel = (int)k;
                }
            }
        }
    }
    else
    {
        popupItems = m_items;
        popupCurSel = m_curSel;
    }
    if (popupItems.empty())
    {
        // Nothing to show (e.g. filter has zero hits) -> don't open.
        return;
    }

    // Compute popup rect in screen coordinates: just below the combo, full
    // combo width, height = min(rows, maxVisible) * itemH + 2 (border).
    POINT topLeft = { m_rcItem.left, m_rcItem.bottom };
    ::ClientToScreen(hostHwnd, &topLeft);
    int rows = (int)popupItems.size();
    if (rows > m_maxVisible)
    {
        rows = m_maxVisible;
    }
    int popupW = m_rcItem.right - m_rcItem.left;
    int popupH = rows * m_itemH + 2;

    // Heap-allocate; popup deletes itself in OnFinalMessage. We hold a
    // raw pointer for re-entry checks only - ownership is on the popup
    // itself once it's been Open()'d.
    m_popup = new DuiComboBoxPopup();
    RECT rc = { topLeft.x, topLeft.y, topLeft.x + popupW, topLeft.y + popupH };
    m_popup->Open(this, rc, popupItems, popupCurSel, m_itemH);
    m_popupOpen = true;
}

void DuiComboBox::ClosePopup()
{
    // Used by combo dtor and by external callers. The popup may already be
    // mid-teardown; in that case m_popup is a valid HWND but we have to be
    // safe against re-entrancy. Detach the back-pointer first so the
    // popup's deferred close path can't call back into a soon-to-be-freed
    // combo, then synchronously destroy. The popup's OnFinalMessage will
    // run and delete the popup C++ object; it will see m_owner == nullptr
    // and skip OnPopupClosed.
    if (!m_popup)
    {
        return;
    }
    DuiComboBoxPopup* p = m_popup;
    m_popup = nullptr;
    m_popupOpen = false;
    p->DetachOwner();
    if (p->IsWindow())
    {
        p->DestroyWindow();
    }
    // Do NOT delete p here - OnFinalMessage already did (or will).
}

void DuiComboBox::OnPopupSelected(int index)
{
    // Translate popup-relative index back to the m_items index when an
    // incremental-search filter is active. Without filter, the popup's
    // index already lines up 1:1 with m_items.
    int realIdx = index;
    if (m_incSearch && !m_filteredIndices.empty()
        && index >= 0 && index < (int)m_filteredIndices.size())
    {
        realIdx = m_filteredIndices[index];
    }
    SetCurSel(realIdx, /*notify=*/true);
    if (m_edit && realIdx >= 0 && realIdx < (int)m_items.size())
    {
        // Mirror the chosen text into the embedded EDIT. Suppress the
        // OnEditTextChanged callback so this programmatic write doesn't
        // immediately reset m_curSel back to -1 / re-trigger filtering.
        m_suppressEditNotify = true;
        m_edit->SetText(m_items[realIdx]);
        m_suppressEditNotify = false;
    }
    // Done filtering; clear the map so the next manual OpenPopup shows all.
    m_filteredIndices.clear();
}

void DuiComboBox::OnPopupClosed()
{
    // Called by the popup from OnFinalMessage - the popup HWND is gone
    // and the popup C++ object will delete itself immediately after this
    // returns. Just clear our pointer and flag.
    m_popup = nullptr;
    m_popupOpen = false;
    Invalidate();
}

bool DuiComboBox::OnLButtonUp(POINT pt, UINT)
{
    if (!::PtInRect(&m_rcItem, pt))
    {
        return false;
    }
    // In editable mode only the right-side arrow zone toggles the popup;
    // clicks in the text area fall through to the embedded EDIT (the host
    // dispatched there via HitTest before reaching us).
    if (m_style == StyleEditable && !::PtInRect(&ArrowZoneRect(), pt))
    {
        return false;
    }
    if (m_popupOpen)
    {
        ClosePopup();
    }
    else
    {
        OpenPopup();
    }
    return true;
}

void DuiComboBox::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    // Combo body: rounded box filled with m_bgColor, 1px border that
    // brightens on hover/focus. When m_showBorder is false the border pen
    // tracks the fill color so RoundRect's stroke is effectively invisible.
    COLORREF bgClr   = m_bEnabled ? m_bgColor : kBgDisabled;
    COLORREF brdClr;
    if (!m_showBorder)
    {
        brdClr = bgClr;
    }
    else if (!m_bEnabled)
    {
        brdClr = kBorderDisabled;
    }
    else
    {
        brdClr = (m_bHover || m_popupOpen) ? kBorderActive : kBorderNormal;
    }
    HBRUSH br = ::CreateSolidBrush(bgClr);
    HPEN   pn = ::CreatePen(PS_SOLID, 1, brdClr);
    HBRUSH ob = (HBRUSH)::SelectObject(hdc, br);
    HPEN   op = (HPEN)  ::SelectObject(hdc, pn);
    ::RoundRect(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom,
                kCornerPx, kCornerPx);
    ::SelectObject(hdc, ob);
    ::SelectObject(hdc, op);
    ::DeleteObject(br);
    ::DeleteObject(pn);

    // Down arrow on the right side.
    int arrowW = kArrowWPx;
    int arrowH = kArrowHPx;
    int ax = m_rcItem.right - arrowW - kArrowPadRPx;
    int ay = (m_rcItem.top + m_rcItem.bottom - arrowH) / 2;
    if (m_showArrow)
    {
        POINT pts[3] = {
            { ax,           ay },
            { ax + arrowW,  ay },
            { ax + arrowW / 2, ay + arrowH }
        };
        // enabled 态走 m_arrowColor(默认 kArrowEnabled, 业务可 SetArrowColor 改);
        // disabled 态沿用 kArrowDisabled 不变。三角形走 DuiAA::FillPolygon
        // 抗锯齿绘制 —— 斜边在屏上不再有阶梯像素。
        // 不画 outline(实心小三角形 outline 会显得粗糙),outlineRgb 留 CLR_INVALID。
        COLORREF arrowClr = m_bEnabled ? m_arrowColor : kArrowDisabled;
        DuiAA::FillPolygon(hdc, pts, 3, arrowClr);
    }

    if (m_style == StyleReadOnly)
    {
        // Selected item text - we own the rendering.
        CString text = (m_curSel >= 0 && m_curSel < (int)m_items.size())
                       ? m_items[m_curSel] : CString();
        if (!text.IsEmpty())
        {
            HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
            HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, m_bEnabled ? kTextEnabled : kTextDisabled);
            RECT rt = m_rcItem;
            rt.left  += kTextPadLPx;
            rt.right  = ax - kTextArrowGap;     // leave room for arrow
            ::DrawText(hdc, text, -1, &rt,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
            if (oldFont)
            {
                ::SelectObject(hdc, oldFont);
            }
        }
    }
    // Editable mode: the embedded EDIT paints its own text; the arrow
    // already drawn above is enough of a click-target affordance, no
    // separator line needed.

    if (m_bFocused && m_style == StyleReadOnly)
    {
        RECT rf = m_rcItem;
        ::InflateRect(&rf, -3, -3);
        DrawFocusRect(hdc, &rf);
    }
}

// ---------------------------------------------------------------------------
// Editable-mode plumbing
// ---------------------------------------------------------------------------

void DuiComboBox::SetBgColor(COLORREF c)
{
    m_bgColor = c;
    if (m_edit)
    {
        m_edit->SetBgColor(c);
    }
    Invalidate();
}

void DuiComboBox::SetShowBorder(bool b)
{
    m_showBorder = b;
    if (m_edit)
    {
        m_edit->SetShowBorder(b);
    }
    Invalidate();
}

void DuiComboBox::SetShowArrow(bool b)
{
    m_showArrow = b;
    Invalidate();
}

void DuiComboBox::SetArrowColor(COLORREF c)
{
    if (m_arrowColor == c)
    {
        return;
    }
    m_arrowColor = c;
    Invalidate();
}

void DuiComboBox::SetEditable(bool b)
{
    Style next = b ? StyleEditable : StyleReadOnly;
    if (m_style == next)
    {
        return;
    }
    m_style = next;
    EnsureEditChild();
    Layout(m_rcItem);
    Invalidate();
}

CString DuiComboBox::GetText() const
{
    if (m_style == StyleEditable && m_edit)
    {
        return m_edit->GetText();
    }
    if (m_curSel >= 0 && m_curSel < (int)m_items.size())
    {
        return m_items[m_curSel];
    }
    return CString();
}

void DuiComboBox::SetText(LPCTSTR sz)
{
    CString text = sz ? sz : _T("");
    if (m_style == StyleEditable && m_edit)
    {
        m_suppressEditNotify = true;
        m_edit->SetText(text);
        m_suppressEditNotify = false;
        // Try to map to an item
        int idx = FindItemMatching(text);
        if (idx >= 0)
        {
            SetCurSel(idx, /*notify=*/false);
        }
        else
        {
            m_curSel = -1;
        }
        Invalidate();
    }
    else
    {
        int idx = FindItemMatching(text);
        if (idx >= 0)
        {
            SetCurSel(idx, /*notify=*/false);
        }
    }
}

int DuiComboBox::FindItemMatching(LPCTSTR sz) const
{
    if (!sz || !*sz)
    {
        return -1;
    }
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].Compare(sz) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

void DuiComboBox::SetIncrementalSearch(bool b)
{
    if (m_incSearch == b)
    {
        return;
    }
    m_incSearch = b;
    if (!b)
    {
        // Clear stale filter state when turning off.
        m_filteredIndices.clear();
    }
}

std::vector<int> DuiComboBox::ComputeFilteredIndices(LPCTSTR query) const
{
    std::vector<int> out;
    out.reserve(m_items.size());
    // Empty / null query: every item is a match.
    if (!query || !*query)
    {
        for (int i = 0; i < (int)m_items.size(); ++i)
        {
            out.push_back(i);
        }
        return out;
    }
    CString needle(query);
    if (!m_incCaseSensitive)
    {
        needle.MakeLower();
    }
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        CString hay = m_items[i];
        if (!m_incCaseSensitive)
        {
            hay.MakeLower();
        }
        bool hit;
        if (m_incSubstring)
        {
            hit = (hay.Find(needle) >= 0);
        }
        else
        {
            // Prefix: hay starts with needle.
            hit = (hay.GetLength() >= needle.GetLength()
                   && ::_tcsncmp((LPCTSTR)hay,
                                 (LPCTSTR)needle,
                                 needle.GetLength()) == 0);
        }
        if (hit)
        {
            out.push_back((int)i);
        }
    }
    return out;
}

RECT DuiComboBox::ArrowZoneRect() const
{
    int w = ArrowZoneWidth();
    return RECT{ m_rcItem.right - w, m_rcItem.top,
                 m_rcItem.right,     m_rcItem.bottom };
}

RECT DuiComboBox::EditZoneRect() const
{
    return RECT{ m_rcItem.left + 1,                       // skip 1px border
                 m_rcItem.top  + 1,
                 m_rcItem.right - ArrowZoneWidth(),
                 m_rcItem.bottom - 1 };
}

void DuiComboBox::EnsureEditChild()
{
    if (m_style == StyleEditable && m_edit == nullptr)
    {
        auto e = std::unique_ptr<DuiComboEdit>(new DuiComboEdit());
        e->SetCombo(this);
        e->SetCtrlId(GetCtrlId() + 0x1000);   // sub-id offset; not used by parent
        // Keep the embedded EDIT visually consistent with the combo body.
        e->SetBgColor(m_bgColor);
        e->SetShowBorder(m_showBorder);
        m_edit = e.get();
        DuiControl::AddChild(std::unique_ptr<DuiControl>(e.release()));
    }
    else if (m_style == StyleReadOnly && m_edit != nullptr)
    {
        // Detach the edit child. Removing from m_children will free it.
        RemoveChild(m_edit);
        m_edit = nullptr;
    }
}

void DuiComboBox::PositionEditChild()
{
    if (!m_edit)
    {
        return;
    }
    m_edit->SetRect(EditZoneRect());

    // Lazily create the underlying EDIT HWND once we know the host's HWND.
    if (m_pHost && m_pHost->m_hWnd && !m_edit->GetHostedHwnd())
    {
        m_edit->EnsureCreated(m_pHost->m_hWnd);
        // After EnsureCreated, push the rect again so the new HWND
        // adopts the right position immediately.
        m_edit->Layout(m_edit->GetRect());
    }
}

void DuiComboBox::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    EnsureEditChild();
    PositionEditChild();
}

void DuiComboBox::OnEditTextChanged()
{
    if (m_suppressEditNotify)
    {
        return;
    }
    CString typed = m_edit ? m_edit->GetText() : CString();

    // Track exact-match state for VALUECHANGED extra (existing contract).
    int matched = FindItemMatching(typed);
    int oldSel  = m_curSel;
    m_curSel    = matched;          // -1 if no exact match

    if (m_incSearch)
    {
        // Recompute the filter and re-open / refresh the popup so the
        // dropdown narrows in lockstep with typing.
        m_filteredIndices = ComputeFilteredIndices(typed);
        if (m_popupOpen)
        {
            // Cheapest "refresh" that's safe with the existing popup
            // architecture: tear down + open with the new filter. The
            // typed text stays in the EDIT, the filtered list reflects
            // the new query.
            ClosePopup();
            OpenPopup();
        }
        else if (!typed.IsEmpty() && !m_filteredIndices.empty())
        {
            // First keystroke that produces hits: pop the dropdown.
            OpenPopup();
        }
    }

    if (m_curSel != oldSel)
    {
        Invalidate();
    }
    NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_curSel);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_COMBOBOX
