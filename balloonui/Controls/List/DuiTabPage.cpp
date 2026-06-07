#include "stdafx.h"
#include "DuiTabPage.h"

#if BUI_FEATURE_TABPAGE


namespace balloonwjui {

namespace {

// DuiTab subclass that redirects selection-changed events back to its
// owning DuiTabPage instead of letting them bubble straight to the host.
// The page swap happens in DuiTabPage::OnHeaderSelected, which then
// re-fires DUIN_VALUECHANGED with the composite's own ctrlId so external
// observers see one notification with the right control id.
class HeaderTab : public DuiTab
{
public:
    explicit HeaderTab(DuiTabPage* owner) : m_owner(owner) {}
protected:
    void FireSelectionChanged(int newIdx) override;
private:
    DuiTabPage* m_owner;
};

} // namespace

// Trampoline so HeaderTab can call back into DuiTabPage's private
// state without befriending it (would force HeaderTab into the header).
void DuiTabPage_OnHeaderSelected(DuiTabPage* page, int newIdx)
{
    if (page)
    {
        page->SetCurSel(newIdx, /*notify=*/true);
    }
}

void HeaderTab::FireSelectionChanged(int newIdx)
{
    DuiTabPage_OnHeaderSelected(m_owner, newIdx);
}

DuiTabPage::DuiTabPage()
{
    SetTabStop(false);

    // Build the inner DuiTab eagerly — the composite is unusable without
    // it, and giving it a fixed identity (slot 0 of m_children) makes
    // GetHeader() trivial.
    auto hdr = std::unique_ptr<HeaderTab>(new HeaderTab(this));
    m_header = hdr.get();
    AddChild(std::move(hdr));
}

void DuiTabPage::SetHeaderHeight(int px)
{
    if (px < 12)
    {
        px = 12;
    }
    if (m_headerH == px)
    {
        return;
    }
    m_headerH = px;
    Layout(m_rcItem);
    Invalidate();
}

int DuiTabPage::AddPage(LPCTSTR title, std::unique_ptr<DuiControl> page, HBITMAP icon)
{
    int idx = m_header->AddTab(title, /*closeable=*/false, /*dropdown=*/false,
                               /*lParam=*/0, icon);

    DuiControl* raw = nullptr;
    if (page)
    {
        raw = page.get();
        AddChild(std::move(page));
    }
    m_pages.push_back(raw);

    // First-page added implies selection 0.
    if (m_curSel < 0)
    {
        m_curSel = 0;
        m_header->SetCurSel(0, /*notify=*/false);
    }
    ApplyVisibility();
    LayoutContent();
    Invalidate();
    return idx;
}

void DuiTabPage::RemovePage(int index)
{
    if (index < 0 || index >= (int)m_pages.size())
    {
        return;
    }
    if (m_pages[index])
    {
        RemoveChild(m_pages[index]);
        m_pages[index] = nullptr;
    }
    m_pages.erase(m_pages.begin() + index);
    m_header->RemoveTab(index);

    // Clamp current selection.
    if (m_pages.empty())
    {
        m_curSel = -1;
    }
    else
    {
        if (m_curSel >= (int)m_pages.size())
        {
            m_curSel = (int)m_pages.size() - 1;
        }
        else if (m_curSel < 0)
        {
            m_curSel = 0;
        }
        m_header->SetCurSel(m_curSel, /*notify=*/false);
    }
    ApplyVisibility();
    LayoutContent();
    Invalidate();
}

void DuiTabPage::SetPage(int index, std::unique_ptr<DuiControl> page)
{
    if (index < 0 || index >= (int)m_pages.size())
    {
        return;
    }
    if (m_pages[index])
    {
        RemoveChild(m_pages[index]);
        m_pages[index] = nullptr;
    }
    if (page)
    {
        DuiControl* raw = page.get();
        AddChild(std::move(page));
        m_pages[index] = raw;
    }
    ApplyVisibility();
    LayoutContent();
    Invalidate();
}

DuiControl* DuiTabPage::GetPage(int index) const
{
    if (index < 0 || index >= (int)m_pages.size())
    {
        return nullptr;
    }
    return m_pages[index];
}

void DuiTabPage::SetPageTitle(int index, LPCTSTR title)
{
    m_header->SetTabText(index, title);
}

CString DuiTabPage::GetPageTitle(int index) const
{
    return m_header->GetTabText(index);
}

void DuiTabPage::SetPageIcon(int index, HBITMAP hBmp)
{
    m_header->SetTabIcon(index, hBmp);
}

HBITMAP DuiTabPage::GetPageIcon(int index) const
{
    return m_header->GetTabIcon(index);
}

void DuiTabPage::SetIconSize(int px)
{
    m_header->SetIconSize(px);
}

int DuiTabPage::GetIconSize() const
{
    return m_header->GetIconSize();
}

void DuiTabPage::SetIconGap(int px)
{
    m_header->SetIconGap(px);
}

int DuiTabPage::GetIconGap() const
{
    return m_header->GetIconGap();
}

void DuiTabPage::SetAutoFitTabWidth(bool b)
{
    m_header->SetAutoFitTabWidth(b);
}

bool DuiTabPage::GetAutoFitTabWidth() const
{
    return m_header->GetAutoFitTabWidth();
}

void DuiTabPage::SetCurSel(int index, bool notify)
{
    if (m_pages.empty())
    {
        m_curSel = -1;
        return;
    }
    if (index < 0 || index >= (int)m_pages.size())
    {
        return;
    }
    if (m_curSel == index)
    {
        // Still re-sync header (cheap) and visibility — covers the case
        // where a caller forced a stale state through. No notify since
        // nothing actually changed.
        m_header->SetCurSel(index, false);
        ApplyVisibility();
        return;
    }
    m_curSel = index;
    m_header->SetCurSel(index, /*notify=*/false);
    ApplyVisibility();
    LayoutContent();
    Invalidate();
    if (notify)
    {
        NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_curSel);
    }
}

void DuiTabPage::ApplyVisibility()
{
    for (int i = 0; i < (int)m_pages.size(); ++i)
    {
        DuiControl* p = m_pages[i];
        if (!p)
        {
            continue;
        }
        p->SetVisible(i == m_curSel);
    }
}

void DuiTabPage::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;

    // Header strip across the top.
    RECT rcHdr = m_rcItem;
    rcHdr.bottom = rcHdr.top + m_headerH;
    if (rcHdr.bottom > m_rcItem.bottom)
    {
        rcHdr.bottom = m_rcItem.bottom;
    }
    if (m_header)
    {
        m_header->SetRect(rcHdr);
    }

    LayoutContent();
}

void DuiTabPage::LayoutContent()
{
    if (m_curSel < 0 || m_curSel >= (int)m_pages.size())
    {
        return;
    }
    DuiControl* p = m_pages[m_curSel];
    if (!p)
    {
        return;
    }

    RECT rc = m_rcItem;
    rc.top = rc.top + m_headerH;
    if (rc.top > rc.bottom)
    {
        rc.top = rc.bottom;
    }
    p->SetRect(rc);
}

void DuiTabPage::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }

    // Header.
    if (m_header && m_header->IsVisible())
    {
        RECT inter;
        if (::IntersectRect(&inter, &m_header->GetRect(), &rcDirty))
        {
            m_header->OnPaint(hdc, inter);
        }
    }

    // Currently selected page only.
    if (m_curSel >= 0 && m_curSel < (int)m_pages.size())
    {
        DuiControl* p = m_pages[m_curSel];
        if (p && p->IsVisible())
        {
            RECT inter;
            if (::IntersectRect(&inter, &p->GetRect(), &rcDirty))
            {
                p->OnPaint(hdc, inter);
            }
        }
    }
}

DuiControl* DuiTabPage::HitTest(POINT ptHostClient)
{
    if (!m_bVisible || !m_bEnabled)
    {
        return nullptr;
    }
    if (!::PtInRect(&m_rcItem, ptHostClient))
    {
        return nullptr;
    }

    // Header band first (so a tab click is never shadowed by a page that
    // ignored its own bottom area).
    if (m_header && m_header->IsVisible() &&
        ::PtInRect(&m_header->GetRect(), ptHostClient))
    {
        if (DuiControl* hit = m_header->HitTest(ptHostClient))
        {
            return hit;
        }
    }

    // Then current page.
    if (m_curSel >= 0 && m_curSel < (int)m_pages.size())
    {
        DuiControl* p = m_pages[m_curSel];
        if (p && p->IsVisible())
        {
            if (DuiControl* hit = p->HitTest(ptHostClient))
            {
                return hit;
            }
        }
    }
    return this;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_TABPAGE
