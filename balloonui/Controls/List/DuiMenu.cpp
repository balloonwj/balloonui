#include "stdafx.h"
#include "DuiMenu.h"

#if BUI_FEATURE_MENU

#include "../../DuiResMgr.h"
#include "../../DuiDpi.h"
#include "../../DuiPaintAA.h"
#include "../../ImageEx.h"
#include "DuiMenuPlacement.h"
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
    // 分隔条一行的高度（像素）：1px 线上下各留 2px。原为 12，与上下两行各自约 8px 的
    // 文字上下留白叠加后，分隔条两侧出现明显的空白，2026-10-04 按用户要求收窄。
    const int kSepRowH     = 5;
    // 分组标题行（2026-10-05 起）：行高（像素）比普通行矮，文字用小一号的字、靠下对齐，
    // 让标题贴近它下面的那一组；文字左边距小于普通项的文字起点（kIconColW + kTextPadL），
    // 与普通项错开，不像一项被禁用的功能
    const int kHeaderRowH      = 24;
    const int kHeaderFontPt    = 8;      // 标题文字的磅值，普通项是默认字体的 9 磅
    const int kHeaderTextPadL  = 10;     // 标题文字左边距（像素）
    const int kHeaderTextPadB  = 3;      // 标题文字到行底的距离（像素）
    const int kMinTextW    = 80;
    const int kMaxTextW    = 320;
    const int kShortcutGap = 24;     // 文字列与快捷键列之间的最小间距（像素，2026-10-07 起）
    const COLORREF kClrShortcut = RGB(120, 120, 120);   // 快捷键文字：灰色，比项文字淡
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
    const COLORREF kClrHeaderText  = RGB(128, 128, 134);   // 分组标题文字：中灰，比禁用项的字深一些
    const COLORREF kClrBorder      = RGB(200, 200, 204);
    const COLORREF kClrCheckTick   = RGB( 40, 120,  40);
    const COLORREF kClrArrow       = RGB(110, 110, 110);

    // 一行菜单项的高度（像素）：分隔条 kSepRowH，分组标题 kHeaderRowH，其余 kRowH
    int RowHeightOf(const DuiMenu::Item& it)
    {
        if (it.kind == DuiMenu::ItemSeparator)
        {
            return kSepRowH;
        }
        if (it.kind == DuiMenu::ItemHeader)
        {
            return kHeaderRowH;
        }
        return kRowH;
    }

    /**
     *  测量一组菜单项里最宽的文字（像素），不做上下限处理。分隔条不计；分组标题用标题字体测量，
     *  其余项用普通字体测量。普通项写成「文字\t快捷键」时，左段计入返回值，快捷键另计入 widestShortcut。
     *    hdc：测量用的 DC，调用前后选入的字体保持不变。
     *    itemFont / headerFont：普通项与分组标题的字体，为空时用 DC 当前的字体。
     *    widestShortcut：[出参，可为 nullptr] 最宽的快捷键文字（像素）；没有快捷键时为 0。
     */
    int MeasureWidestText(HDC hdc, const std::vector<DuiMenu::Item>& items, HFONT itemFont, HFONT headerFont,
                          int* widestShortcut)
    {
        HFONT oldFont = (HFONT)::GetCurrentObject(hdc, OBJ_FONT);
        int widest = 0;
        int shortcutWidest = 0;
        for (size_t i = 0; i < items.size(); ++i)
        {
            const DuiMenu::Item& it = items[i];
            if (it.kind == DuiMenu::ItemSeparator)
            {
                continue;
            }
            HFONT useFont = (it.kind == DuiMenu::ItemHeader) ? headerFont : itemFont;
            ::SelectObject(hdc, useFont ? useFont : oldFont);
            //分组标题原样显示，不拆快捷键
            CString label = it.text;
            CString shortcut;
            if (it.kind != DuiMenu::ItemHeader)
            {
                DuiMenu::SplitShortcut(it.text, label, shortcut);
            }
            SIZE sz = { 0, 0 };
            ::GetTextExtentPoint32(hdc, label, label.GetLength(), &sz);
            if (sz.cx > widest)
            {
                widest = sz.cx;
            }
            if (!shortcut.IsEmpty())
            {
                SIZE keySz = { 0, 0 };
                ::GetTextExtentPoint32(hdc, shortcut, shortcut.GetLength(), &keySz);
                if (keySz.cx > shortcutWidest)
                {
                    shortcutWidest = keySz.cx;
                }
            }
        }
        ::SelectObject(hdc, oldFont);
        if (widestShortcut)
        {
            *widestShortcut = shortcutWidest;
        }
        return widest;
    }

    /**
     *  菜单的宽度：图标列、文字列（左段宽度限制在 [kMinTextW, kMaxTextW]）、有快捷键时的间距与快捷键列、
     *  右边距、子菜单箭头列。没有快捷键时与引入快捷键列之前相同。
     *    textMax：最宽的左段（像素）。
     *    shortcutMax：最宽的快捷键文字（像素），0 表示没有快捷键。
     */
    int MenuBodyWidth(int textMax, int shortcutMax)
    {
        if (textMax < kMinTextW)
        {
            textMax = kMinTextW;
        }
        if (textMax > kMaxTextW)
        {
            textMax = kMaxTextW;
        }
        int width = kIconColW + kTextPadL + textMax + kTextPadR + kArrowColW;
        if (shortcutMax > 0)
        {
            width += kShortcutGap + shortcutMax;
        }
        return width;
    }
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
        // 菜单窗口尚未创建，按弹出位置所在显示器的 DPI 取字体；测量与绘制都用
        // 这一个 DPI，保证窗口尺寸与文字一致。
        POINT anchor;
        anchor.x = screenX;
        anchor.y = screenY;
        m_dpi = DuiDpi::GetDpiForPoint(anchor);

        SIZE sz = MeasureBody();
        Create(NULL, NULL, NULL,
               WS_POPUP,                  // border drawn manually in OnPaint
               WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
        if (!m_hWnd)
        {
            return;
        }

        // 落点修正：菜单窗口左上角原本直接放在传入坐标上，锚点靠近屏幕右 / 下
        // 边缘时整张菜单会跑到桌面工作区外。约定任何弹出菜单都必须整体落在锚点所在
        // 显示器的工作区内。在这里统一处理一次，所有入口（右键菜单 / 菜单栏下拉 /
        // 子菜单）都自动受保护，调用方不必也不应各写一份。这一步是幂等的，调用方
        // 自己算好的落点不会被挪动。
        POINT origin;
        origin.x = screenX;
        origin.y = screenY;
        const RECT area = GetMenuPlacementArea(origin);
        if (parent == nullptr)
        {
            // 顶层菜单：默认从锚点向右下展开，放不下就翻向左 / 上。
            origin = ClampMenuOriginToRect(origin, sz, area);
        }
        else
        {
            // 子菜单：必须始终贴着父菜单的某一侧，不能按顶层菜单那套翻转，
            // 否则会盖住父菜单或跑得离对应父项很远。
            RECT rcParent;
            parent->GetWindowRect(&rcParent);
            origin = ClampSubMenuOrigin(rcParent, origin, sz, area);
        }

        SetWindowPos(NULL, origin.x, origin.y, sz.cx, sz.cy,
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
        CPaintDC paintDc(m_hWnd);
        if (!m_menu)
        {
            return;
        }
        // 双缓冲（2026-10-06）：整张菜单先画在与客户区同样大小的内存位图上，再一次贴到屏幕。
        // 原先直接画在屏幕上：每次悬停行变化都先把整个客户区刷成底色、再逐行画图标与文字，
        // 两步之间的空档在鼠标来回移动时反复出现，表现为菜单闪烁。
        CRect rcClient;
        GetClientRect(&rcClient);
        const int width = rcClient.Width();
        const int height = rcClient.Height();
        if (width <= 0 || height <= 0)
        {
            return;
        }
        HDC memDc = ::CreateCompatibleDC(paintDc);
        HBITMAP memBmp = (memDc != nullptr) ? ::CreateCompatibleBitmap(paintDc, width, height) : nullptr;
        if (memBmp == nullptr)
        {
            // 建不出内存位图（GDI 资源不足）：退回直接画在屏幕上，只是会闪
            if (memDc != nullptr)
            {
                ::DeleteDC(memDc);
            }
            PaintContent(paintDc);
            return;
        }
        HGDIOBJ oldBmp = ::SelectObject(memDc, memBmp);
        PaintContent(memDc);
        ::BitBlt(paintDc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
        ::SelectObject(memDc, oldBmp);
        ::DeleteObject(memBmp);
        ::DeleteDC(memDc);
    }

    // 画整张菜单（底色、各行、外框）。dc 为内存 DC（双缓冲）或屏幕 DC（内存位图建不出来时）。
    void    PaintContent(HDC dc)
    {
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

        HFONT useFont = DuiResMgr::Inst().GetDefaultFontForDpi(m_dpi);
        HFONT oldFont = useFont ? (HFONT)::SelectObject(dc, useFont) : nullptr;
        int oldBk = ::SetBkMode(dc, TRANSPARENT);

        int y = 0;
        for (int i = 0; i < (int)items.size(); ++i)
        {
            const auto& it = items[i];
            int rowH = RowHeightOf(it);
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
            else if (it.kind == DuiMenu::ItemHeader)
            {
                //分组标题：小一号的灰字靠下对齐，没有图标列、悬停高亮与子菜单箭头；'&' 原样显示
                HFONT headerFont = DuiResMgr::Inst().GetFontByPointSizeForDpi(kHeaderFontPt, false, m_dpi);
                HFONT prevFont = headerFont ? (HFONT)::SelectObject(dc, headerFont) : nullptr;
                COLORREF oldClr = ::SetTextColor(dc, kClrHeaderText);
                CRect rcText(kHeaderTextPadL, y, m_bodyW - kTextPadR, y + rowH - kHeaderTextPadB);
                ::DrawText(dc, it.text, -1, &rcText,
                           DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                ::SetTextColor(dc, oldClr);
                if (prevFont)
                {
                    ::SelectObject(dc, prevFont);
                }
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
                //「文字\t快捷键」：左段画在文字列，快捷键以灰色靠右画在快捷键列（没有快捷键的菜单 m_shortcutW 为 0，
                //文字列与改动前相同）
                CString label;
                CString shortcut;
                DuiMenu::SplitShortcut(it.text, label, shortcut);
                const int keyRight = m_bodyW - kArrowColW - kTextPadR;
                const int textRight = (m_shortcutW > 0) ? keyRight - m_shortcutW - kShortcutGap : keyRight;
                CRect rcText(kIconColW + kTextPadL, y, textRight, y + rowH);
                ::DrawText(dc, label, -1, &rcText,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                if (!shortcut.IsEmpty())
                {
                    ::SetTextColor(dc, it.enabled ? kClrShortcut : kClrTextDisabled);
                    CRect rcKey(keyRight - m_shortcutW, y, keyRight, y + rowH);
                    ::DrawText(dc, shortcut, -1, &rcKey,
                               DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }
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
        // 取最宽一项文字：普通项用默认字体，分组标题用标题字体，都按弹出位置的 DPI
        HDC hdc = ::GetDC(nullptr);
        int shortcutMax = 0;
        int textMax = MeasureWidestText(hdc, items, DuiResMgr::Inst().GetDefaultFontForDpi(m_dpi),
                                        DuiResMgr::Inst().GetFontByPointSizeForDpi(kHeaderFontPt, false, m_dpi),
                                        &shortcutMax);
        ::ReleaseDC(nullptr, hdc);

        m_shortcutW = shortcutMax;
        m_bodyW = MenuBodyWidth(textMax, shortcutMax);
        int totalH = 0;
        for (const auto& it : items)
        {
            totalH += RowHeightOf(it);
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
            int rowH = RowHeightOf(items[i]);
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
            y += RowHeightOf(items[i]);
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
    int            m_dpi      = 0;        // Open 时按弹出位置所在显示器取的 DPI；0 = 尚未 Open，按全局 DPI 处理
    DuiMenuPopup*  m_child    = nullptr;
    int            m_bodyW    = 0;
    int            m_shortcutW = 0;   // 快捷键列的宽度（像素）：MeasureBody 时取最宽的快捷键文字；没有快捷键时为 0
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

int DuiMenu::AppendHeader(LPCTSTR text)
{
    //分组标题不可选中：置为不可用后，悬停高亮、点击、回车与助记符各路径都会跳过它
    Item it;
    it.id = 0;
    it.kind = ItemHeader;
    it.enabled = false;
    it.checked = false;
    it.text = text ? text : _T("");
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
        //分隔条与分组标题的 id 都是 0，不参与按 id 查找，SetEnabled(0, ...) 之类不会改到它们
        if (m_items[i].id == nID && m_items[i].kind != ItemSeparator && m_items[i].kind != ItemHeader)
        {
            return (int)i;
        }
    }
    return -1;
}

// ---- pure helpers --------------------------------------------------

void DuiMenu::SplitShortcut(LPCTSTR text, CString& label, CString& shortcut)
{
    label.Empty();
    shortcut.Empty();
    if (!text)
    {
        return;
    }
    LPCTSTR tab = _tcschr(text, _T('\t'));
    if (!tab)
    {
        label = text;
        return;
    }
    label.SetString(text, (int)(tab - text));
    shortcut = tab + 1;
}

TCHAR DuiMenu::FindAcceleratorChar(LPCTSTR text)
{
    if (!text)
    {
        return 0;
    }
    //'\t' 之后是快捷键文字，不参与助记符
    for (LPCTSTR p = text; *p && *p != _T('\t'); ++p)
    {
        if (*p != _T('&'))
        {
            continue;
        }
        TCHAR next = *(p + 1);
        if (next == 0 || next == _T('\t'))
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
        if (it.kind == ItemSeparator || it.kind == ItemHeader || !it.enabled)
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
        if (it.kind == ItemSeparator || it.kind == ItemHeader)
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

    // ---- 宽：取最宽一项文字（用默认菜单字体测量），限制在 [kMinTextW, kMaxTextW] 之间 ----
    // 本函数在弹出之前调用，还不知道菜单最终落在哪块显示器，这里按全局 DPI 估算；
    // 实际的窗口尺寸以 DuiMenuPopup::MeasureBody 按弹出位置的 DPI 算出的为准。
    //分组标题用标题字体测量
    HDC   hdc     = ::GetDC(nullptr);
    int   shortcutMax = 0;
    int   textMax = MeasureWidestText(hdc, m_items, DuiResMgr::Inst().GetDefaultFont(),
                                      DuiResMgr::Inst().GetFontByPointSize(kHeaderFontPt), &shortcutMax);
    ::ReleaseDC(nullptr, hdc);
    sz.cx = MenuBodyWidth(textMax, shortcutMax);

    // ---- 高：各行高度之和（分隔条 kSepRowH，分组标题 kHeaderRowH，其余 kRowH）----
    int totalH = 0;
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        totalH += RowHeightOf(m_items[i]);
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
