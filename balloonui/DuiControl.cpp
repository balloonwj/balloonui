#include "stdafx.h"
#include "DuiControl.h"
#include "DuiHost.h"
#include "DuiResMgr.h"
#include "DuiDpi.h"
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
    // 把宿主持有的、指向本控件的裸指针全部清掉，否则之后的一次鼠标移动、
    // 释放捕获或焦点变化就会访问已释放的内存。在整棵子树连续析构的场景
    // （DuiScrollView::SetContent、DuiHost::SetRoot、RemoveChild）里尤其关键。
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

int DuiControl::GetDpi() const
{
    // 宿主窗口创建之前它的 DPI 还是构造时的默认值，不能代表窗口将来所在的显示器，
    // 此时与未挂宿主一样按全局 DPI 处理。
    if (m_pHost != nullptr && m_pHost->HasWindowDpi())
    {
        return m_pHost->GetDpi();
    }
    const int globalDpi = DuiResMgr::Inst().GetDpi();
    return (globalDpi > 0) ? globalDpi : DuiDpi::GetSystemDpi();
}

HFONT DuiControl::GetDefaultFont() const
{
    return DuiResMgr::Inst().GetDefaultFontForDpi(GetDpi());
}

HFONT DuiControl::GetFontByPointSize(int pt, bool bold) const
{
    return DuiResMgr::Inst().GetFontByPointSizeForDpi(pt, bold, GetDpi());
}

HFONT DuiControl::GetAntiAliasedFontByPointSize(int pt, bool bold) const
{
    return DuiResMgr::Inst().GetAntiAliasedFontByPointSizeForDpi(pt, bold, GetDpi());
}

void DuiControl::DispatchDpiChanged_(int dpi)
{
    OnDpiChanged(dpi);
    for (size_t i = 0; i < m_children.size(); ++i)
    {
        m_children[i]->DispatchDpiChanged_(dpi);
    }
}

void DuiControl::RelayoutDescendants_()
{
    for (size_t i = 0; i < m_children.size(); ++i)
    {
        DuiControl* child = m_children[i].get();
        child->Layout(child->m_rcItem);
        child->RelayoutDescendants_();
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

void DuiControl::SetVisible(bool b)
{
    if (m_bVisible == b)
    {
        return;
    }
    m_bVisible = b;
    Invalidate();
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

bool DuiControl::OnRawMessage(UINT, WPARAM, LPARAM, LRESULT&)
{
    // 默认不处理，宿主会把消息交回系统默认处理。只有需要完整窗口消息序列的
    // 控件（如无窗口富文本控件）才覆写本方法。
    return false;
}

void DuiControl::Invalidate()
{
    if (!m_pHost)
    {
        return;
    }

    // 与祖先链上每个把子控件裁剪在自己矩形内绘制的容器依次求交（理由见头文件）。
    // 普通容器的 GetChildClipRect 返回 false，不参与求交。
    RECT rc = m_rcItem;
    for (const DuiControl* p = m_pParent; p != nullptr; p = p->m_pParent)
    {
        RECT clip;
        if (!p->GetChildClipRect(clip))
        {
            continue;
        }
        RECT visible;
        if (!::IntersectRect(&visible, &rc, &clip))
        {
            return;   // 本控件此刻完全落在可见区之外，没有需要重画的地方
        }
        rc = visible;
    }
    m_pHost->InvalidateDuiRect(rc);
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
