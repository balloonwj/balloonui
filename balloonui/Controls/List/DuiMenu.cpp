#include "stdafx.h"
#include "DuiMenu.h"

#if BUI_FEATURE_MENU

#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"
#include "../../ImageEx.h"
#include <gdiplus.h>

namespace balloonwjui {

namespace {
    // Visual constants - matched to screenshots/menu.png.
    const int kRowH        = 32;     // roomy row (was 24)
    const int kIconColW    = 24;     // left-side icon / check column
    const int kArrowColW   = 16;     // sub-menu right arrow column
    const int kPadL        = 0;      // separator goes full width
    const int kPadR        = 0;
    const int kTextPadL    = 12;     // text left padding past icon column
    const int kTextPadR    = 14;
    const int kSepRowH     = 12;     // separator row height (was 8)
    const int kMinTextW    = 80;
    const int kMaxTextW    = 320;
    const int kIconSize    = 16;     // 16x16 px

    // Colors (screenshots/menu.png style).
    const COLORREF kClrBg          = RGB(255, 255, 255);
    const COLORREF kClrText        = RGB( 30,  30,  30);   // 默认 text：深灰，比纯黑略柔和
    const COLORREF kClrTextHover   = RGB(  0,   0,   0);   // hover 时字色加重到纯黑
    const COLORREF kClrTextDisabled= RGB(160, 160, 160);
    // hover 底色：原值 RGB(242,242,242) 太浅，跟白底差异只 13 灰阶，hover
    // 状态视觉上"几乎没变化"，加上字色没强化 → 用户感觉"字反而被浅底淡化"。
    // 改成 OS Vista+ dropdown menu 标准的浅蓝 highlight（与 Win Explorer 右
    // 键菜单 hover 一致），让 hover 行成为视觉焦点，字色同时加深到纯黑加
    // 强对比。
    const COLORREF kClrHoverBg     = RGB(229, 241, 251);   // Win 标准 menu hover 浅蓝
    const COLORREF kClrSeparator   = RGB(220, 220, 222);
    const COLORREF kClrBorder      = RGB(200, 200, 204);
    const COLORREF kClrCheckTick   = RGB( 40, 120,  40);
    const COLORREF kClrArrow       = RGB(110, 110, 110);
}

// Forward decl — DuiMenu uses CImageEx* but the icon paint helper is below.
static void DrawMenuIcon(HDC dc, CImageEx* icon, int x, int y, bool disabled);

// =====================================================================
// DuiMenuPopup: borderless top-level window, owner-draws the menu items.
// Uses the same self-deleting OnFinalMessage + PostMessage(WM_CLOSE)
// teardown pattern as DuiComboBoxPopup.
// =====================================================================

class DuiMenuPopup : public CWindowImpl<DuiMenuPopup, CWindow>
{
public:
    // CS_DROPSHADOW gives popups the OS-rendered soft drop shadow seen
    // in screenshots/menu.png without us having to alpha-blend one.
    DECLARE_WND_CLASS_EX(_T("__DuiMenuPopup__"), CS_DROPSHADOW, COLOR_MENU)

    DuiMenuPopup() = default;
    ~DuiMenuPopup() = default;

    BEGIN_MSG_MAP(DuiMenuPopup)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MSG_WM_LBUTTONUP(OnLButtonUp)
        MSG_WM_MOUSEMOVE(OnMouseMove)
        MSG_WM_KEYDOWN(OnKeyDown)
        MSG_WM_CHAR(OnChar)
        MSG_WM_TIMER(OnTimer)
        MSG_WM_KILLFOCUS(OnKillFocus)
        MSG_WM_ACTIVATE(OnActivate)
    END_MSG_MAP()

    // Open at screen position; receives ownership of selecting an ID via
    // m_menu->m_lastChosenId. When the user dismisses, sets m_dismissed
    // and posts WM_CLOSE.
    void Open(DuiMenu* menu, int screenX, int screenY, DuiMenuPopup* parent)
    {
        m_menu   = menu;
        m_parent = parent;

        SIZE sz = MeasureBody();
        Create(NULL, NULL, NULL,
               WS_POPUP,                  // border drawn manually in OnPaint
               WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
        if (!m_hWnd)
        {
            return;
        }

        SetWindowPos(NULL, screenX, screenY, sz.cx, sz.cy,
                     SWP_NOZORDER);
        ShowWindow(SW_SHOWNORMAL);
        SetForegroundWindow(m_hWnd);
        SetFocus();

        if (m_menu)
        {
            m_menu->m_activePopup = this;
        }

        // 仅 root popup（无 parent）且 menu 设了 switch zones 时启动 30ms
        // 鼠标位置轮询，鼠标移入任一 zone → 关闭 popup（含所有子 popup）
        // 并把 zone idx 写到 m_menu->m_lastSwitchZoneIdx。
        if (!parent
            && m_menu
            && m_menu->m_switchZones
            && m_menu->m_switchZoneCount > 0)
        {
            ::SetTimer(m_hWnd, kSwitchPollTimerId, kSwitchPollMs, nullptr);
        }
    }

    void OnFinalMessage(HWND) override
    {
        // Detach from neighbors before destruction so any Activate/KillFocus
        // dispatched to a still-alive sibling does NOT walk through this
        // about-to-be-freed object.
        if (m_parent && m_parent->m_child == this)
        {
            m_parent->m_child = nullptr;
        }
        if (m_child && m_child->m_parent == this)
        {
            m_child->m_parent = nullptr;
        }
        if (m_menu && m_menu->m_activePopup == this)
        {
            m_menu->m_activePopup = nullptr;
        }
        delete this;
    }

    void RequestClose()
    {
        if (m_closed || !m_hWnd)
        {
            return;
        }
        m_closed = true;
        PostMessage(WM_CLOSE);
    }

    bool WasItemChosen() const { return m_chosen; }

private:
    int     OnCreate(LPCREATESTRUCT) { return 0; }

    BOOL    OnEraseBkgnd(CDCHandle dc)
    {
        CRect rc;
        GetClientRect(&rc);
        HBRUSH bg = ::CreateSolidBrush(kClrBg);
        ::FillRect(dc, &rc, bg);
        ::DeleteObject(bg);
        return TRUE;
    }

    void    OnPaint(CDCHandle)
    {
        CPaintDC dc(m_hWnd);
        if (!m_menu)
        {
            return;
        }

        // 先用菜单底色铺满整个客户区，再画各行内容。OnPaint 不能依赖
        // OnEraseBkgnd 铺底：hover 变化是用 Invalidate(FALSE) 触发的（不发
        // WM_ERASEBKGND），若这里不自铺底，上一次 hover 行的背景与文字会残留，
        // 造成 hover 背景清不掉、文字重影。
        CRect rcClient;
        GetClientRect(&rcClient);
        HBRUSH bgBrush = ::CreateSolidBrush(kClrBg);
        ::FillRect(dc, &rcClient, bgBrush);
        ::DeleteObject(bgBrush);

        const auto& items = m_menu->GetItems();

        HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
        HFONT oldFont = useFont ? (HFONT)::SelectObject(dc, useFont) : nullptr;
        int oldBk = ::SetBkMode(dc, TRANSPARENT);

        int y = 0;
        for (int i = 0; i < (int)items.size(); ++i)
        {
            const auto& it = items[i];
            int rowH = (it.kind == DuiMenu::ItemSeparator) ? kSepRowH : kRowH;
            CRect rcRow(0, y, m_bodyW, y + rowH);

            if (it.kind == DuiMenu::ItemSeparator)
            {
                // Full-width 1px line, vertically centered in the sep row.
                int my = y + kSepRowH / 2;
                HPEN pen = ::CreatePen(PS_SOLID, 1, kClrSeparator);
                HPEN op  = (HPEN)::SelectObject(dc, pen);
                ::MoveToEx(dc, 0,        my, nullptr);
                ::LineTo  (dc, m_bodyW,  my);
                ::SelectObject(dc, op);
                ::DeleteObject(pen);
            }
            else
            {
                bool hover = (i == m_hoverIdx) && it.enabled;
                if (hover)
                {
                    HBRUSH hb = ::CreateSolidBrush(kClrHoverBg);
                    ::FillRect(dc, &rcRow, hb);
                    ::DeleteObject(hb);
                }

                // Left icon / check column.
                int iconX = (kIconColW - kIconSize) / 2;
                int iconY = y + (kRowH - kIconSize) / 2;
                if (it.icon)
                {
                    DrawMenuIcon(dc, it.icon, iconX, iconY, !it.enabled);
                }
                else if (it.kind == DuiMenu::ItemCheckable && it.checked)
                {
                    int cy = y + (kRowH - 12) / 2;
                    int cx = (kIconColW - 12) / 2;
                    DuiAA::DrawLine(dc, cx + 1,  cy + 6,  cx + 5,  cy + 11, kClrCheckTick, 2.0f);
                    DuiAA::DrawLine(dc, cx + 5,  cy + 11, cx + 11, cy + 1,  kClrCheckTick, 2.0f);
                }

                // Text 颜色三档：disabled 灰；enabled+hover 纯黑（强化）；enabled+rest 深灰。
                COLORREF textClr = !it.enabled ? kClrTextDisabled
                                 : (hover ? kClrTextHover : kClrText);
                COLORREF oldClr = ::SetTextColor(dc, textClr);
                CRect rcText(kIconColW + kTextPadL, y,
                             m_bodyW - kArrowColW - kTextPadR, y + rowH);
                ::DrawText(dc, it.text, -1, &rcText,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                ::SetTextColor(dc, oldClr);

                // Sub-menu arrow.
                if (it.kind == DuiMenu::ItemSubMenu)
                {
                    int ax = m_bodyW - kArrowColW;
                    int ay = y + (kRowH - 8) / 2;
                    POINT pts[3] = {
                        { ax,     ay },
                        { ax,     ay + 8 },
                        { ax + 6, ay + 4 }
                    };
                    DuiAA::FillPolygon(dc, pts, 3, kClrArrow, kClrArrow);
                }
            }

            y += rowH;
        }

        if (oldFont)
        {
            ::SelectObject(dc, oldFont);
        }
        ::SetBkMode(dc, oldBk);

        // 1px outer border on top of the white fill.
        CRect rcAll;
        GetClientRect(&rcAll);
        HPEN borderPen = ::CreatePen(PS_SOLID, 1, kClrBorder);
        HPEN obp = (HPEN)::SelectObject(dc, borderPen);
        HBRUSH ob = (HBRUSH)::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(dc, rcAll.left, rcAll.top, rcAll.right, rcAll.bottom);
        ::SelectObject(dc, ob);
        ::SelectObject(dc, obp);
        ::DeleteObject(borderPen);
    }

    void    OnLButtonUp(UINT, CPoint pt)
    {
        if (!m_menu)
        {
            return;
        }
        int idx = HitTest(pt);
        if (idx < 0)
        {
            return;
        }
        const auto& items = m_menu->GetItems();
        const auto& it = items[idx];
        if (!it.enabled || it.kind == DuiMenu::ItemSeparator)
        {
            return;
        }

        if (it.kind == DuiMenu::ItemSubMenu)
        {
            // Click on submenu item == open it (treat as the hover trigger).
            OpenSubMenuAt(idx);
            return;
        }

        // Terminal click: report ID up the chain and dismiss everything.
        m_chosen = true;
        DuiMenu* root = m_menu;
        DuiMenuPopup* p = m_parent;
        while (p && p->m_menu)
        {
            root = p->m_menu;
            p = p->m_parent;
        }
        // Whoever owns the root menu reads m_lastChosenId.
        root->m_lastChosenId = it.id;
        // Close from leaf upward.
        DuiMenuPopup* cur = this;
        while (cur)
        {
            DuiMenuPopup* up = cur->m_parent;
            cur->RequestClose();
            cur = up;
        }
    }

    void    OnMouseMove(UINT, CPoint pt)
    {
        int idx = HitTest(pt);
        if (idx == m_hoverIdx)
        {
            return;
        }
        m_hoverIdx = idx;
        Invalidate(FALSE);

        // Hover-to-open on submenu items uses a 250ms delay so users
        // sweeping past adjacent submenu rows don't get popups flickering
        // open and closed. Non-submenu hover cancels any pending timer
        // and closes any child that was opened by a previous hover.
        ::KillTimer(m_hWnd, kSubmenuTimerId);
        if (m_menu && idx >= 0)
        {
            const auto& items = m_menu->GetItems();
            if (items[idx].kind == DuiMenu::ItemSubMenu && items[idx].enabled)
            {
                m_pendingSubmenuIdx = idx;
                ::SetTimer(m_hWnd, kSubmenuTimerId, kSubmenuDelayMs, nullptr);
            }
            else
            {
                m_pendingSubmenuIdx = -1;
                CloseChildIfAny();
            }
        }
    }

    void    OnTimer(UINT_PTR id)
    {
        if (id == kSubmenuTimerId)
        {
            ::KillTimer(m_hWnd, kSubmenuTimerId);
            if (m_pendingSubmenuIdx >= 0 && m_pendingSubmenuIdx == m_hoverIdx)
            {
                OpenSubMenuAt(m_pendingSubmenuIdx);
            }
            m_pendingSubmenuIdx = -1;
            return;
        }
        if (id == kSwitchPollTimerId)
        {
            // 只有 root popup 在跑这条；m_menu 必非空且 zones 已设。每帧
            // 取 GetCursorPos（屏幕坐标）逐 zone hit-test；命中即关闭整
            // 个 popup 链（含子菜单），把 zone idx 写到 root menu 的
            // m_lastSwitchZoneIdx 让 TrackPopupEx 取走。
            if (m_menu && m_menu->m_switchZones && m_menu->m_switchZoneCount > 0)
            {
                POINT pt = {};
                ::GetCursorPos(&pt);
                for (int i = 0; i < m_menu->m_switchZoneCount; ++i)
                {
                    if (::PtInRect(&m_menu->m_switchZones[i], pt))
                    {
                        m_menu->m_lastSwitchZoneIdx = i;
                        if (m_child)
                        {
                            m_child->RequestClose();
                            m_child = nullptr;
                        }
                        ::KillTimer(m_hWnd, kSwitchPollTimerId);
                        RequestClose();
                        return;
                    }
                }
            }
            return;
        }
    }

    void    OnKeyDown(TCHAR vk, UINT, UINT)
    {
        if (!m_menu)
        {
            return;
        }
        const auto& items = m_menu->GetItems();
        switch (vk)
        {
        case VK_ESCAPE:
            RequestClose();
            return;
        case VK_DOWN:
        {
            int next = DuiMenu::KeyboardNavNext(items, m_hoverIdx, +1);
            if (next >= 0)
            {
                m_hoverIdx = next;
                Invalidate(FALSE);
            }
            return;
        }
        case VK_UP:
        {
            int next = DuiMenu::KeyboardNavNext(items, m_hoverIdx, -1);
            if (next >= 0)
            {
                m_hoverIdx = next;
                Invalidate(FALSE);
            }
            return;
        }
        case VK_RIGHT:
            // Open submenu under hover, no hover-delay path.
            if (m_hoverIdx >= 0 && m_hoverIdx < (int)items.size() &&
                items[m_hoverIdx].kind == DuiMenu::ItemSubMenu &&
                items[m_hoverIdx].enabled)
            {
                ::KillTimer(m_hWnd, kSubmenuTimerId);
                OpenSubMenuAt(m_hoverIdx);
            }
            return;
        case VK_LEFT:
            // Close this level if we have a parent; root popup just no-ops.
            if (m_parent)
            {
                RequestClose();
            }
            return;
        case VK_RETURN:
        case VK_SPACE:
            ActivateHover();
            return;
        }
    }

    void    OnChar(TCHAR ch, UINT, UINT)
    {
        if (!m_menu)
        {
            return;
        }
        const auto& items = m_menu->GetItems();
        int idx = DuiMenu::FindAcceleratorMatch(items, ch);
        if (idx < 0)
        {
            return;
        }
        m_hoverIdx = idx;
        Invalidate(FALSE);
        // Match the standard Win32 behavior: a unique mnemonic activates
        // the item immediately (vs. just moving focus to it).
        ActivateHover();
    }

    void    OnKillFocus(CWindow newFocus)
    {
        // We're already in the close path: a queued WM_CLOSE will tear us
        // down; do nothing now. Also guards against accessing siblings
        // that may have already self-destructed earlier in the cascade.
        if (m_closed)
        {
            return;
        }

        // If focus is moving to one of our descendants (a child submenu
        // popup), don't dismiss; otherwise tear down.
        HWND h = newFocus.m_hWnd;
        for (DuiMenuPopup* c = m_child; c; c = c->m_child)
        {
            if (c->m_hWnd == h)
            {
                return;
            }
        }
        // Also don't dismiss if focus moved to our parent (clicking back up).
        for (DuiMenuPopup* p = m_parent; p; p = p->m_parent)
        {
            if (p->m_hWnd == h)
            {
                return;
            }
        }

        // Tear down from the leaf upwards so each popup unwinds cleanly.
        DuiMenuPopup* leaf = this;
        while (leaf->m_child)
        {
            leaf = leaf->m_child;
        }
        DuiMenuPopup* cur = leaf;
        while (cur)
        {
            DuiMenuPopup* up = cur->m_parent;
            cur->RequestClose();
            if (cur == this)
            {
                break;
            }
            cur = up;
        }
    }

    void    OnActivate(UINT nState, BOOL, CWindow other)
    {
        // Same self-protection as OnKillFocus: once WM_CLOSE has been
        // posted, ignore the WA_INACTIVE that may arrive during teardown.
        // Without this we'd walk m_child / m_parent chains that earlier
        // cascade-closes may already have left dangling.
        if (m_closed)
        {
            return;
        }
        if (nState == WA_INACTIVE)
        {
            HWND h = other.m_hWnd;
            for (DuiMenuPopup* c = m_child; c; c = c->m_child)
            {
                if (c->m_hWnd == h)
                {
                    return;
                }
            }
            for (DuiMenuPopup* p = m_parent; p; p = p->m_parent)
            {
                if (p->m_hWnd == h)
                {
                    return;
                }
            }
            RequestClose();
        }
    }

    // ----- helpers -----

    SIZE MeasureBody()
    {
        if (!m_menu)
        {
            m_bodyW = m_bodyH = 0;
            return SIZE{0, 0};
        }
        const auto& items = m_menu->GetItems();
        // Measure widest text using default font.
        HDC hdc = ::GetDC(nullptr);
        HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
        HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
        int textMax = kMinTextW;
        for (const auto& it : items)
        {
            if (it.kind == DuiMenu::ItemSeparator)
            {
                continue;
            }
            SIZE sz = { 0, 0 };
            ::GetTextExtentPoint32(hdc, it.text, it.text.GetLength(), &sz);
            if (sz.cx > textMax)
            {
                textMax = sz.cx;
            }
        }
        if (oldFont)
        {
            ::SelectObject(hdc, oldFont);
        }
        ::ReleaseDC(nullptr, hdc);
        if (textMax > kMaxTextW)
        {
            textMax = kMaxTextW;
        }

        m_bodyW = kIconColW + kTextPadL + textMax + kTextPadR + kArrowColW;
        int totalH = 0;
        for (const auto& it : items)
        {
            totalH += (it.kind == DuiMenu::ItemSeparator) ? kSepRowH : kRowH;
        }
        m_bodyH = totalH;
        return SIZE{ m_bodyW, m_bodyH };
    }

    int HitTest(POINT pt)
    {
        if (!m_menu)
        {
            return -1;
        }
        if (pt.x < 0 || pt.x >= m_bodyW || pt.y < 0 || pt.y >= m_bodyH)
        {
            return -1;
        }
        const auto& items = m_menu->GetItems();
        int y = 0;
        for (int i = 0; i < (int)items.size(); ++i)
        {
            int rowH = (items[i].kind == DuiMenu::ItemSeparator) ? kSepRowH : kRowH;
            if (pt.y >= y && pt.y < y + rowH)
            {
                return i;
            }
            y += rowH;
        }
        return -1;
    }

    void OpenSubMenuAt(int idx)
    {
        if (!m_menu)
        {
            return;
        }
        const auto& items = m_menu->GetItems();
        if (idx < 0 || idx >= (int)items.size())
        {
            return;
        }
        const auto& it = items[idx];
        if (it.kind != DuiMenu::ItemSubMenu || !it.subMenu)
        {
            return;
        }

        // If the submenu we'd be opening is already open, no-op.
        if (m_child && m_child->m_menu == it.subMenu)
        {
            return;
        }

        // Close any existing child first.
        CloseChildIfAny();

        // Anchor: right edge of this row, vertically aligned to row top.
        RECT rcWnd;
        GetWindowRect(&rcWnd);
        int y = 0;
        for (int i = 0; i < idx; ++i)
        {
            y += (items[i].kind == DuiMenu::ItemSeparator) ? kSepRowH : kRowH;
        }
        int sx = rcWnd.right - 1;
        int sy = rcWnd.top + y;

        m_child = new DuiMenuPopup();
        m_child->Open(it.subMenu, sx, sy, /*parent=*/this);
    }

    void CloseChildIfAny()
    {
        if (!m_child)
        {
            return;
        }
        // The child will null itself out when it closes; we just request.
        m_child->RequestClose();
        m_child = nullptr;
    }

private:
    static const UINT_PTR kSubmenuTimerId    = 0xD3;
    static const UINT     kSubmenuDelayMs    = 250;
    // mouse-move 切换轮询：30ms ≈ 60Hz 与可感知延迟（50ms+）的中点。
    // 仅 root popup 启 timer；子 popup 的 m_menu 通常没设 switchZones。
    static const UINT_PTR kSwitchPollTimerId = 0xD4;
    static const UINT     kSwitchPollMs      = 30;

    void ActivateHover()
    {
        if (!m_menu || m_hoverIdx < 0)
        {
            return;
        }
        const auto& items = m_menu->GetItems();
        if (m_hoverIdx >= (int)items.size())
        {
            return;
        }
        const auto& it = items[m_hoverIdx];
        if (!it.enabled || it.kind == DuiMenu::ItemSeparator)
        {
            return;
        }
        if (it.kind == DuiMenu::ItemSubMenu)
        {
            ::KillTimer(m_hWnd, kSubmenuTimerId);
            OpenSubMenuAt(m_hoverIdx);
            return;
        }
        // Terminal: same path as click.
        m_chosen = true;
        DuiMenu* root = m_menu;
        DuiMenuPopup* p = m_parent;
        while (p && p->m_menu)
        {
            root = p->m_menu;
            p = p->m_parent;
        }
        root->m_lastChosenId = it.id;
        DuiMenuPopup* cur = this;
        while (cur)
        {
            DuiMenuPopup* up = cur->m_parent;
            cur->RequestClose();
            cur = up;
        }
    }

    DuiMenu*       m_menu     = nullptr;
    DuiMenuPopup*  m_parent   = nullptr;
    DuiMenuPopup*  m_child    = nullptr;
    int            m_bodyW    = 0;
    int            m_bodyH    = 0;
    int            m_hoverIdx = -1;
    int            m_pendingSubmenuIdx = -1;
    bool           m_chosen   = false;
    bool           m_closed   = false;
};

// Stub: the original DrawMenuIcon was elided in the recovered source.
// Caller invokes it from OnPaint; provide a minimal implementation that
// uses CImageEx's Draw method when available, otherwise no-ops.
static void DrawMenuIcon(HDC dc, CImageEx* icon, int x, int y, bool /*disabled*/)
{
    if (!icon || !dc)
    {
        return;
    }
    // CImageEx::Draw signature varies; the legacy shim below is best-effort.
    // If a future code path needs grayscale-on-disabled, add a hook here.
    icon->Draw(dc, x, y, kIconSize, kIconSize, 0, 0, kIconSize, kIconSize);
}

// =====================================================================
// DuiMenu
// =====================================================================

DuiMenu::DuiMenu()  = default;

DuiMenu::~DuiMenu()
{
    HideNow();
}

int DuiMenu::AppendItem(UINT nID, LPCTSTR text, CImageEx* icon)
{
    Item it;
    it.id = nID;
    it.kind = ItemText;
    it.enabled = true;
    it.checked = false;
    it.text = text ? text : _T("");
    it.subMenu = nullptr;
    it.icon = icon;
    m_items.push_back(it);
    return (int)m_items.size() - 1;
}

int DuiMenu::AppendChecked(UINT nID, LPCTSTR text, bool checked)
{
    Item it;
    it.id = nID;
    it.kind = ItemCheckable;
    it.enabled = true;
    it.checked = checked;
    it.text = text ? text : _T("");
    it.subMenu = nullptr;
    it.icon = nullptr;     // checkable item uses the check tick, not an icon
    m_items.push_back(it);
    return (int)m_items.size() - 1;
}

int DuiMenu::AppendDisabled(UINT nID, LPCTSTR text, CImageEx* icon)
{
    Item it;
    it.id = nID;
    it.kind = ItemText;
    it.enabled = false;
    it.checked = false;
    it.text = text ? text : _T("");
    it.subMenu = nullptr;
    it.icon = icon;
    m_items.push_back(it);
    return (int)m_items.size() - 1;
}

int DuiMenu::AppendSeparator()
{
    Item it;
    it.id = 0;
    it.kind = ItemSeparator;
    it.enabled = false;
    it.checked = false;
    it.subMenu = nullptr;
    it.icon = nullptr;
    m_items.push_back(it);
    return (int)m_items.size() - 1;
}

int DuiMenu::AppendSubMenu(UINT nID, LPCTSTR text, DuiMenu* subMenu, CImageEx* icon)
{
    Item it;
    it.id = nID;
    it.kind = ItemSubMenu;
    it.enabled = (subMenu != nullptr);
    it.checked = false;
    it.text = text ? text : _T("");
    it.subMenu = subMenu;
    it.icon = icon;
    m_items.push_back(it);
    return (int)m_items.size() - 1;
}

int DuiMenu::FindIndexById(UINT nID) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].id == nID && m_items[i].kind != ItemSeparator)
        {
            return (int)i;
        }
    }
    return -1;
}

// ---- pure helpers --------------------------------------------------

TCHAR DuiMenu::FindAcceleratorChar(LPCTSTR text)
{
    if (!text)
    {
        return 0;
    }
    for (LPCTSTR p = text; *p; ++p)
    {
        if (*p != _T('&'))
        {
            continue;
        }
        TCHAR next = *(p + 1);
        if (next == 0)
        {
            return 0;
        }
        if (next == _T('&'))
        {
            // Escaped literal '&' — skip the second one and keep scanning.
            ++p;
            continue;
        }
        return (TCHAR)_totlower(next);
    }
    return 0;
}

int DuiMenu::FindAcceleratorMatch(const std::vector<Item>& items, TCHAR ch)
{
    if (ch == 0)
    {
        return -1;
    }
    TCHAR target = (TCHAR)_totlower(ch);
    for (int i = 0; i < (int)items.size(); ++i)
    {
        const Item& it = items[i];
        if (it.kind == ItemSeparator || !it.enabled)
        {
            continue;
        }
        TCHAR a = FindAcceleratorChar(it.text);
        if (a != 0 && a == target)
        {
            return i;
        }
    }
    return -1;
}

int DuiMenu::KeyboardNavNext(const std::vector<Item>& items, int fromIdx, int dir)
{
    int n = (int)items.size();
    if (n <= 0)
    {
        return -1;
    }
    if (dir == 0)
    {
        dir = 1;
    }

    auto isSelectable = [&](int i)
    {
        if (i < 0 || i >= n)
        {
            return false;
        }
        const Item& it = items[i];
        if (it.kind == ItemSeparator)
        {
            return false;
        }
        if (!it.enabled)
        {
            return false;
        }
        return true;
    };

    int start = fromIdx;
    if (start < 0)
    {
        start = (dir > 0) ? -1 : n;        // wrap into range below
    }
    int i = start;
    for (int step = 0; step < n; ++step)
    {
        i += dir;
        if (i < 0)
        {
            i = n - 1;
        }
        if (i >= n)
        {
            i = 0;
        }
        if (isSelectable(i))
        {
            return i;
        }
    }
    return -1;
}

void DuiMenu::SetCheck(UINT nID, bool checked)
{
    int i = FindIndexById(nID);
    if (i >= 0)
    {
        m_items[i].checked = checked;
    }
}

void DuiMenu::SetEnabled(UINT nID, bool enabled)
{
    int i = FindIndexById(nID);
    if (i >= 0)
    {
        m_items[i].enabled = enabled;
    }
}

bool DuiMenu::IsChecked(UINT nID) const
{
    int i = FindIndexById(nID);
    return (i >= 0) && m_items[i].checked;
}

bool DuiMenu::IsEnabled(UINT nID) const
{
    int i = FindIndexById(nID);
    return (i >= 0) && m_items[i].enabled;
}

void DuiMenu::SetItemIcon(UINT nID, CImageEx* icon)
{
    int i = FindIndexById(nID);
    if (i >= 0)
    {
        m_items[i].icon = icon;
    }
}

CImageEx* DuiMenu::GetItemIcon(UINT nID) const
{
    int i = FindIndexById(nID);
    return (i >= 0) ? m_items[i].icon : nullptr;
}

SIZE DuiMenu::MeasureSize() const
{
    // 与 DuiMenuPopup::MeasureBody 完全同口径，单独抽出供"弹出前定位"使用。
    // 两处共用本文件作用域的 kRowH / kSepRowH / kIconColW 等常量，改一处即同步。
    SIZE sz = { 0, 0 };

    // 空菜单没有可显示内容，直接返回 {0,0}（也不会被 TrackPopup 弹出）。
    if (m_items.empty())
    {
        return sz;
    }

    // ---- 宽：取最宽一项文字（用默认菜单字体测量），夹在 [kMinTextW, kMaxTextW] ----
    HDC   hdc     = ::GetDC(nullptr);
    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    int   textMax = kMinTextW;
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].kind == ItemSeparator)
        {
            continue;
        }
        SIZE ts = { 0, 0 };
        ::GetTextExtentPoint32(hdc, m_items[i].text, m_items[i].text.GetLength(), &ts);
        if (ts.cx > textMax)
        {
            textMax = ts.cx;
        }
    }
    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
    ::ReleaseDC(nullptr, hdc);
    if (textMax > kMaxTextW)
    {
        textMax = kMaxTextW;
    }
    sz.cx = kIconColW + kTextPadL + textMax + kTextPadR + kArrowColW;

    // ---- 高：各行高度之和（分隔条 kSepRowH，其余 kRowH）----
    int totalH = 0;
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        totalH += (m_items[i].kind == ItemSeparator) ? kSepRowH : kRowH;
    }
    sz.cy = totalH;
    return sz;
}

void DuiMenu::Clear()
{
    HideNow();
    m_items.clear();
    m_lastChosenId = 0;
}

void DuiMenu::HideNow()
{
    if (m_activePopup)
    {
        m_activePopup->RequestClose();
    }
}

void DuiMenu::SetSwitchZones(const RECT* screenZones, int count)
{
    m_switchZones     = (count > 0) ? screenZones : nullptr;
    m_switchZoneCount = (count > 0) ? count       : 0;
}

DuiMenu::TrackPopupResult DuiMenu::TrackPopupEx(int screenX, int screenY,
                                                HWND ownerHwnd)
{
    TrackPopupResult result = { 0u, -1 };
    if (m_items.empty())
    {
        return result;
    }
    m_lastChosenId      = 0;
    m_lastSwitchZoneIdx = -1;

    DuiMenuPopup* p = new DuiMenuPopup();
    p->Open(this, screenX, screenY, /*parent=*/nullptr);
    if (!p->IsWindow())
    {
        return result;   // p deleted itself in OnFinalMessage
    }

    // Pump messages until the popup destroys itself.
    MSG msg;
    while (m_activePopup != nullptr && ::GetMessage(&msg, NULL, 0, 0))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    (void)ownerHwnd;   // kept for API parity with legacy CSkinMenu

    result.chosenId      = m_lastChosenId;
    result.switchZoneIdx = m_lastSwitchZoneIdx;
    return result;
}

UINT DuiMenu::TrackPopup(int screenX, int screenY, HWND ownerHwnd)
{
    return TrackPopupEx(screenX, screenY, ownerHwnd).chosenId;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_MENU
