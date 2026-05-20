#include "stdafx.h"
#include "DuiMenuBar.h"

#if BUI_FEATURE_MENUBAR

#include "../../DuiHost.h"
#include "../../DuiResMgr.h"
#include "../../DuiNotify.h"
#include <tchar.h>

namespace balloonwjui {

// 状态背景色。透明 = CLR_INVALID 不画底，跟 host 背景一致。
//
// kBgHover = #E5E5E5，跟 Win10 任务管理器 menu bar hover 一格几乎相同；
// 强对比反而显眼，浅灰更"系统感"。
//
// kBgActive = #D7E3F4 浅蓝，下拉打开期间标识"当前栏"（视觉与 Win10 一致：
// 既区分于 hover 也不与 menu 项 hover 的浅灰冲突）。
static const COLORREF kBgNormal            = CLR_INVALID;
static const COLORREF kBgHover             = RGB(229, 229, 229);
static const COLORREF kBgActive            = RGB(215, 227, 244);

static const COLORREF kTextColor           = RGB(  0,   0,   0);
static const COLORREF kTextColorDisabled   = RGB(160, 160, 160);

DuiMenuBar::DuiMenuBar() = default;
DuiMenuBar::~DuiMenuBar() = default;

int DuiMenuBar::AppendItem(UINT nID, LPCTSTR text, DuiMenu* dropdown)
{
    Item it;
    it.nID      = nID;
    it.text     = (text ? text : _T(""));
    it.dropdown = dropdown;
    it.enabled  = true;
    m_items.push_back(it);
    m_cachedW.push_back(-1);
    Invalidate();
    return (int)m_items.size() - 1;
}

void DuiMenuBar::RemoveItem(int idx)
{
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return;
    }
    m_items.erase(m_items.begin() + idx);
    m_cachedW.erase(m_cachedW.begin() + idx);
    if (m_hoverIdx == idx)
    {
        m_hoverIdx = -1;
    }
    if (m_activeIdx == idx)
    {
        m_activeIdx = -1;
    }
    Invalidate();
}

void DuiMenuBar::Clear()
{
    m_items.clear();
    m_cachedW.clear();
    m_hoverIdx  = -1;
    m_activeIdx = -1;
    Invalidate();
}

int DuiMenuBar::GetItemCount() const
{
    return (int)m_items.size();
}

DuiMenuBar::Item DuiMenuBar::GetItem(int idx) const
{
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return Item{};
    }
    return m_items[idx];
}

void DuiMenuBar::SetItemEnabled(UINT nID, bool enabled)
{
    int idx = FindIndexById(nID);
    if (idx < 0)
    {
        return;
    }
    if (m_items[idx].enabled == enabled)
    {
        return;
    }
    m_items[idx].enabled = enabled;
    Invalidate();
}

bool DuiMenuBar::IsItemEnabled(UINT nID) const
{
    int idx = FindIndexById(nID);
    if (idx < 0)
    {
        return false;
    }
    return m_items[idx].enabled;
}

void DuiMenuBar::SetDropdown(int idx, DuiMenu* dropdown)
{
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return;
    }
    m_items[idx].dropdown = dropdown;
}

DuiMenu* DuiMenuBar::GetDropdown(int idx) const
{
    if (idx < 0 || idx >= (int)m_items.size())
    {
        return nullptr;
    }
    return m_items[idx].dropdown;
}

int DuiMenuBar::FindIndexById(UINT nID) const
{
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        if (m_items[i].nID == nID)
        {
            return i;
        }
    }
    return -1;
}

void DuiMenuBar::SetItemHeight(int px)
{
    if (px < kItemHeightMin)
    {
        px = kItemHeightMin;
    }
    if (m_itemH == px)
    {
        return;
    }
    m_itemH = px;
    // 行高变化不影响文字宽度，但保险起见整批失效（DPI 切换 / 字体回调
    // 等情况共用同一条路径）。
    for (int& w : m_cachedW)
    {
        w = -1;
    }
    Invalidate();
}

int DuiMenuBar::ItemWidth(int i) const
{
    if (i < 0 || i >= (int)m_items.size())
    {
        return 0;
    }
    if (m_cachedW[i] >= 0)
    {
        return m_cachedW[i];
    }

    // 用 desktop DC 测一次。DT_CALCRECT + DT_HIDEPREFIX 让 "&" 助记符按
    // "普通态绘制"宽度（&不画）测量；激活态显示下划线时下划线在 baseline
    // 下面，不影响水平宽度，所以同一个 cachedW 两态通用。
    HDC   hdc  = ::GetDC(NULL);
    HFONT font = DuiResMgr::Inst().GetDefaultFont();
    HFONT old  = font ? (HFONT)::SelectObject(hdc, font) : nullptr;
    RECT  r    = { 0, 0, 0, 0 };
    ::DrawText(hdc, m_items[i].text.GetString(), -1, &r,
               DT_CALCRECT | DT_SINGLELINE | DT_HIDEPREFIX);
    if (old)
    {
        ::SelectObject(hdc, old);
    }
    ::ReleaseDC(NULL, hdc);

    int w = (r.right - r.left) + kItemPadL + kItemPadR;
    m_cachedW[i] = w;
    return w;
}

RECT DuiMenuBar::ItemRect(int i) const
{
    RECT r = { m_rcItem.left, m_rcItem.top, m_rcItem.left, m_rcItem.bottom };
    for (int k = 0; k < i; ++k)
    {
        r.left  += ItemWidth(k);
    }
    r.right = r.left + ItemWidth(i);
    return r;
}

POINT DuiMenuBar::ItemAnchorScreen(int i) const
{
    RECT  r  = ItemRect(i);
    POINT pt = { r.left, r.bottom };
    HWND  hw = HostHwnd();
    if (hw)
    {
        ::ClientToScreen(hw, &pt);
    }
    return pt;
}

int DuiMenuBar::HitTestItem(POINT pt) const
{
    if (pt.y < m_rcItem.top || pt.y >= m_rcItem.bottom)
    {
        return -1;
    }
    int x = m_rcItem.left;
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        int w = ItemWidth(i);
        if (pt.x >= x && pt.x < x + w)
        {
            return i;
        }
        x += w;
    }
    return -1;
}

HWND DuiMenuBar::HostHwnd() const
{
    return (m_pHost ? m_pHost->m_hWnd : NULL);
}

void DuiMenuBar::SetActiveIndex_(int idx)
{
    if (m_activeIdx == idx)
    {
        return;
    }
    m_activeIdx = idx;
    Invalidate();
}

SIZE DuiMenuBar::GetDesiredSize() const
{
    SIZE sz = { 0, m_itemH };
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        sz.cx += ItemWidth(i);
    }
    return sz;
}

void DuiMenuBar::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (m_items.empty())
    {
        return;
    }
    HFONT font   = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFnt = font ? (HFONT)::SelectObject(hdc, font) : nullptr;
    int   oldBk  = ::SetBkMode(hdc, TRANSPARENT);

    int x = m_rcItem.left;
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        const Item& it = m_items[i];
        int  w = ItemWidth(i);
        RECT r = { x, m_rcItem.top, x + w, m_rcItem.bottom };

        // 背景
        COLORREF bg = kBgNormal;
        if (i == m_activeIdx)
        {
            bg = kBgActive;
        }
        else if (i == m_hoverIdx && it.enabled)
        {
            bg = kBgHover;
        }
        if (bg != CLR_INVALID)
        {
            HBRUSH br = ::CreateSolidBrush(bg);
            ::FillRect(hdc, &r, br);
            ::DeleteObject(br);
        }

        // 文字
        ::SetTextColor(hdc, it.enabled ? kTextColor : kTextColorDisabled);
        UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        // 助记符下划线：仅 m_altActive 状态显示，对应 Win10 行为（普通
        // 态藏起 "&"，Alt 按下时整个 menu bar 上的助记符都画下划线）。
        if (!m_altActive)
        {
            flags |= DT_HIDEPREFIX;
        }
        ::DrawText(hdc, it.text.GetString(), -1, &r, flags);

        x += w;
    }

    ::SetBkMode(hdc, oldBk);
    if (oldFnt)
    {
        ::SelectObject(hdc, oldFnt);
    }
}

bool DuiMenuBar::OnMouseMove(POINT pt, UINT)
{
    int idx = HitTestItem(pt);
    if (idx != m_hoverIdx)
    {
        m_hoverIdx = idx;
        Invalidate();
    }
    return true;
}

bool DuiMenuBar::OnMouseLeave()
{
    if (m_hoverIdx != -1)
    {
        m_hoverIdx = -1;
        Invalidate();
    }
    return true;
}

bool DuiMenuBar::OnLButtonDown(POINT pt, UINT)
{
    int idx = HitTestItem(pt);
    if (idx < 0)
    {
        return false;
    }
    OpenAndDriveDropdown(idx);
    return true;
}

bool DuiMenuBar::OnKeyDown(UINT, UINT)
{
    // 控件本身没有 keyboard focus（不是 tab stop）；← → 切换由 popup 内
    // 处理（暂未实现，本期限制范围）。Esc 通常也走 popup 自己。
    return false;
}

bool DuiMenuBar::OnSetCursor(POINT)
{
    // 走默认 IDC_ARROW（Win10 menu bar 也是箭头不变手型）
    return false;
}

bool DuiMenuBar::ProcessAltKey(UINT vk)
{
    // 只接受字母 / 数字（VK 码 == ASCII for letters/digits）
    bool isAlpha = (vk >= 'A' && vk <= 'Z') || (vk >= 'a' && vk <= 'z');
    bool isDigit = (vk >= '0' && vk <= '9');
    if (!isAlpha && !isDigit)
    {
        return false;
    }
    // FindAcceleratorChar 返小写；这里用 _totlower 折叠，跟它的小写约定
    // 对齐，并避开手卷 A-Z 范围检查的 locale 偏移。
    TCHAR ch = (TCHAR)_totlower((TCHAR)vk);
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        if (!m_items[i].enabled)
        {
            continue;
        }
        TCHAR mn = DuiMenu::FindAcceleratorChar(m_items[i].text.GetString());
        if (mn == ch)
        {
            // 进入 alt active 态画一次下划线（视觉提示），然后弹下拉
            m_altActive = true;
            Invalidate();
            OpenAndDriveDropdown(i);
            m_altActive = false;
            Invalidate();
            return true;
        }
    }
    return false;
}

void DuiMenuBar::OpenDropdown(int idx)
{
    OpenAndDriveDropdown(idx);
}

void DuiMenuBar::CloseDropdown()
{
    if (m_activeIdx < 0)
    {
        return;
    }
    DuiMenu* dd = m_items[m_activeIdx].dropdown;
    if (dd)
    {
        dd->HideNow();
    }
}

void DuiMenuBar::BuildSwitchZones(int curIdx,
                                  std::vector<RECT>& outZones,
                                  std::vector<int>&  outZoneToItem) const
{
    outZones.clear();
    outZoneToItem.clear();
    HWND hw = HostHwnd();
    if (!hw)
    {
        return;
    }
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        if (i == curIdx)
        {
            continue;
        }
        if (!m_items[i].enabled)
        {
            continue;
        }
        if (!m_items[i].dropdown)
        {
            continue;       // 没下拉的栏不接受切换
        }

        RECT r = ItemRect(i);
        // host 客户区 → 屏幕坐标
        POINT lt = { r.left,  r.top    };
        POINT rb = { r.right, r.bottom };
        ::ClientToScreen(hw, &lt);
        ::ClientToScreen(hw, &rb);
        RECT s = { lt.x, lt.y, rb.x, rb.y };
        outZones.push_back(s);
        outZoneToItem.push_back(i);
    }
}

void DuiMenuBar::OpenAndDriveDropdown(int startIdx)
{
    if (m_loopActive)
    {
        return;     // 防重入
    }
    if (startIdx < 0 || startIdx >= (int)m_items.size())
    {
        return;
    }
    HWND hw = HostHwnd();
    if (!hw)
    {
        return;     // 无 host：没法 TrackPopup
    }
    m_loopActive = true;

    int idx = startIdx;
    while (idx >= 0 && idx < (int)m_items.size())
    {
        Item& cur = m_items[idx];
        if (!cur.enabled || !cur.dropdown)
        {
            break;
        }

        // 把"其它栏目"作为切换 zones 注册给 dropdown
        std::vector<RECT> zones;
        std::vector<int>  zoneToItem;
        BuildSwitchZones(idx, zones, zoneToItem);
        cur.dropdown->SetSwitchZones(
            zones.empty() ? nullptr : zones.data(),
            (int)zones.size());

        SetActiveIndex_(idx);
        NotifyParent((UINT)DUIMBN_DROPDOWN_OPEN, (LPARAM)idx);

        POINT anchor = ItemAnchorScreen(idx);
        DuiMenu::TrackPopupResult result =
            cur.dropdown->TrackPopupEx(anchor.x, anchor.y, hw);

        // 清掉栈上 zones 引用，防止悬空
        cur.dropdown->SetSwitchZones(nullptr, 0);

        NotifyParent((UINT)DUIMBN_DROPDOWN_CLOSE, (LPARAM)idx);

        if (result.switchZoneIdx >= 0
            && result.switchZoneIdx < (int)zoneToItem.size())
        {
            idx = zoneToItem[result.switchZoneIdx];
            continue;
        }

        // 普通关闭（选项 / Esc / 失焦）。chosenId 非 0 时通知父。
        if (result.chosenId != 0)
        {
            NotifyParent((UINT)DUIN_VALUECHANGED, (LPARAM)result.chosenId);
        }
        break;
    }

    SetActiveIndex_(-1);
    m_loopActive = false;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_MENUBAR
