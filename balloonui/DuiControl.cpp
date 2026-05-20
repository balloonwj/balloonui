#include "stdafx.h"
#include "DuiControl.h"
#include "DuiHost.h"
#include "HwndHostControl.h"
#include "BalloonUiFeatures.h"
#if BUI_FEATURE_TOOLTIP
#  include "Controls/Feedback/DuiToolTip.h"
#endif

namespace balloonwjui {

DuiControl::DuiControl()
{
    ::SetRectEmpty(&m_rcItem);
}

DuiControl::~DuiControl()
{
    // Clear all host-held raw pointers to this control so a later mouse
    // move / capture release / focus change doesn't dereference freed
    // memory. Critical for tree-mutation paths (SetContent, SetRoot,
    // RemoveChild) where many controls are destroyed back-to-back.
    if (m_pHost)
    {
        if (m_pHost->GetDuiCapture() == this)
        {
            m_pHost->ReleaseDuiCapture(this);
        }
        if (m_pHost->GetDuiFocus() == this)
        {
            m_pHost->SetDuiFocus(nullptr);
        }
        m_pHost->ClearHoverIfMatches(this);
    }
    // Same logic for the singleton tooltip manager: it stores raw
    // DuiControl* keys + a possibly-pending hover target. Skipped when
    // BUI_FEATURE_TOOLTIP is off (manager doesn't exist).
#if BUI_FEATURE_TOOLTIP
    DuiToolTipMgr::Inst().Unregister(this);
#endif
}

void DuiControl::AddChild(std::unique_ptr<DuiControl> child)
{
    if (!child)
    {
        return;
    }
    child->SetParent_(this);
    if (m_pHost)
    {
        child->AttachToHost(m_pHost);
    }
    m_children.push_back(std::move(child));
}

void DuiControl::RemoveChild(DuiControl* child)
{
    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (it->get() == child)
        {
            // Notify subclass side tables FIRST (pointer still valid),
            // then erase (which destroys the unique_ptr / the child).
            OnChildRemoved_(child);
            m_children.erase(it);
            return;
        }
    }
}

void DuiControl::AttachToHost(DuiHost* host)
{
    SetHost_(host);
    for (auto& c : m_children)
    {
        c->AttachToHost(host);
    }
}

DuiControl* DuiControl::FindCtrlById(UINT id)
{
    if (m_uCtrlId == id)
    {
        return this;
    }
    for (auto& c : m_children)
    {
        if (DuiControl* hit = c->FindCtrlById(id))
        {
            return hit;
        }
    }
    return nullptr;
}

void DuiControl::SetRect(const RECT& rc)
{
    if (::EqualRect(&m_rcItem, &rc))
    {
        return;
    }
    Invalidate();              // old position
    m_rcItem = rc;
    Invalidate();              // new position
    Layout(m_rcItem);
}

void DuiControl::ForceLayout(const RECT& rc)
{
    m_rcItem = rc;
    Layout(m_rcItem);
    Invalidate();
}

bool DuiControl::IsEffectivelyVisible() const
{
    const DuiControl* c = this;
    while (c)
    {
        if (!c->m_bVisible)
        {
            return false;
        }
        c = c->m_pParent;
    }
    return true;
}

void DuiControl::SyncHwndVisibilitySubtree_(DuiControl* node)
{
    if (!node) { return; }
    HwndHostControl* h = dynamic_cast<HwndHostControl*>(node);
    if (h)
    {
        HWND hw = h->GetHostedHwnd();
        if (hw && ::IsWindow(hw))
        {
            BOOL want = h->IsEffectivelyVisible() ? TRUE : FALSE;
            BOOL cur  = ::IsWindowVisible(hw);
            if (want != cur)
            {
                ::ShowWindow(hw, want ? SW_SHOWNOACTIVATE : SW_HIDE);
            }
        }
    }
    for (auto& c : node->m_children)
    {
        SyncHwndVisibilitySubtree_(c.get());
    }
}

void DuiControl::SetVisible(bool b)
{
    if (m_bVisible == b)
    {
        return;
    }
    m_bVisible = b;
    Invalidate();
    // 同步本子树里所有 HwndHostControl 后代的真 HWND 显隐 —— 否则
    // 隐藏一个 layout 父容器后，里面 EDIT / RICHEDIT 子窗仍透出来
    SyncHwndVisibilitySubtree_(this);
}

void DuiControl::SetEnabled(bool b)
{
    if (m_bEnabled == b)
    {
        return;
    }
    m_bEnabled = b;
    Invalidate();
}

void DuiControl::DebugSetHover(bool b)
{
    if (m_bHover == b)
    {
        return;
    }
    m_bHover = b;
    Invalidate();
}

void DuiControl::DebugSetFocused(bool b)
{
    if (m_bFocused == b)
    {
        return;
    }
    m_bFocused = b;
    Invalidate();
}

DuiControl* DuiControl::HitTest(POINT pt)
{
    if (!m_bVisible || !::PtInRect(&m_rcItem, pt))
    {
        return nullptr;
    }
    // top-most first (last added paints on top)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        if (DuiControl* hit = (*it)->HitTest(pt))
        {
            return hit;
        }
    }
    return m_bEnabled ? this : nullptr;
}

void DuiControl::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    for (auto& c : m_children)
    {
        if (!c->m_bVisible)
        {
            continue;
        }
        RECT rcInter;
        if (::IntersectRect(&rcInter, &c->m_rcItem, &rcDirty))
        {
            c->OnPaint(hdc, rcInter);
        }
    }
}

void DuiControl::Layout(const RECT& rcAvail)
{
    // Default: children fill our rect (subclasses do real layout).
    for (auto& c : m_children)
    {
        c->Layout(rcAvail);
    }
}

// --- Default event handlers: don't consume; bubble a notify to parent HWND. ---

bool DuiControl::OnMouseMove(POINT, UINT)
{
    return false;
}

bool DuiControl::OnMouseEnter()
{
    m_bHover = true;
    NotifyParent(DUIN_MOUSEENTER);
    return false;
}

bool DuiControl::OnMouseLeave()
{
    m_bHover = false;
    NotifyParent(DUIN_MOUSELEAVE);
    return false;
}

bool DuiControl::OnLButtonDown(POINT, UINT)
{
    return false;
}

bool DuiControl::OnLButtonUp(POINT pt, UINT)
{
    if (m_bEnabled && ::PtInRect(&m_rcItem, pt))
    {
        NotifyParent(DUIN_CLICK);
        return true;
    }
    return false;
}

bool DuiControl::OnLButtonDblClk(POINT, UINT)
{
    NotifyParent(DUIN_DBLCLK);
    return false;
}

bool DuiControl::OnRButtonDown(POINT, UINT)
{
    NotifyParent(DUIN_RCLICK);
    return false;
}

bool DuiControl::OnMouseWheel(POINT, short, UINT)
{
    return false;
}

bool DuiControl::OnChar(TCHAR)
{
    return false;
}

bool DuiControl::OnKeyDown(UINT, UINT)
{
    return false;
}

bool DuiControl::OnSetFocus()
{
    m_bFocused = true;
    Invalidate();
    NotifyParent(DUIN_SETFOCUS);
    return false;
}

bool DuiControl::OnKillFocus()
{
    m_bFocused = false;
    Invalidate();
    NotifyParent(DUIN_KILLFOCUS);
    return false;
}

bool DuiControl::OnSetCursor(POINT)
{
    return false;
}

void DuiControl::Invalidate()
{
    if (m_pHost)
    {
        m_pHost->InvalidateDuiRect(m_rcItem);
    }
}

void DuiControl::Capture()
{
    if (m_pHost)
    {
        m_pHost->SetDuiCapture(this);
    }
}

void DuiControl::ReleaseCapture()
{
    if (m_pHost)
    {
        m_pHost->ReleaseDuiCapture(this);
    }
}

void DuiControl::SetFocus()
{
    if (m_pHost)
    {
        m_pHost->SetDuiFocus(this);
    }
}

LRESULT DuiControl::NotifyParent(UINT code, LPARAM extra)
{
    if (m_pHost)
    {
        return m_pHost->SendNotify(this, code, extra);
    }
    return 0;
}

} // namespace balloonwjui
