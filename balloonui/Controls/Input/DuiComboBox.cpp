#include "stdafx.h"
#include "DuiComboBox.h"

#if BUI_FEATURE_COMBOBOX

#include "../List/DuiListBox.h"
#include "DuiEdit.h"
#include "../../DuiHost.h"
#include "../../DuiResMgr.h"
#include "../../DuiNotify.h"
#include "../../DuiPaintAA.h"

namespace balloonwjui {

namespace {

// Combo body fill / border colors. The body is rounded with kCornerPx and
// the border tracks input state so the user can see hover / open / focus
// the same way DuiEdit does. Disabled drops to a muted gray.
// The normal / active border colors are per-instance members (m_borderNormal /
// m_borderActive, see SetBorderColors); their defaults live in DuiComboBox.h.
const COLORREF kBgDisabled     = RGB(240, 240, 240);   // flat light gray
const COLORREF kBorderDisabled = RGB(190, 190, 190);   // muted gray

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

// 取一块屏幕矩形所在显示器的工作区（已排除任务栏，多显示器安全）。
// 取不到显示器信息时退回主显示器的工作区，再取不到就给一个常见分辨率兜底。
RECT WorkAreaOfRect(const RECT& anchorScreen)
{
    HMONITOR mon = ::MonitorFromRect(&anchorScreen, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    ::memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (mon != NULL && ::GetMonitorInfo(mon, &mi))
    {
        return mi.rcWork;
    }

    RECT work;
    if (::SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0))
    {
        return work;
    }

    RECT fallback = { 0, 0, 1920, 1080 };
    return fallback;
}

} // namespace

namespace combopopup {

RECT ClampPopupToWorkArea(const RECT& comboScreen, int popupW, int popupH,
                          int itemH, const RECT& work)
{
    // 下拉框上方 / 下方各自的可用高度。
    const int spaceBelow = work.bottom - comboScreen.bottom;
    const int spaceAbove = comboScreen.top - work.top;

    int top = 0;
    if (popupH <= spaceBelow)
    {
        // 常规情形：正下方装得下。
        top = comboScreen.bottom;
    }
    else if (popupH <= spaceAbove)
    {
        // 下方不够、上方够 —— 翻到下拉框上方展开。
        top = comboScreen.top - popupH;
    }
    else
    {
        // 上下都不够：挑空间大的那侧，并把高度压到该侧能容纳的整行数。
        const int space = (spaceBelow >= spaceAbove) ? spaceBelow : spaceAbove;
        int fitRows = (space - kPopupBorderThickness) / ((itemH > 0) ? itemH : 1);
        if (fitRows < kPopupMinRows)
        {
            fitRows = kPopupMinRows;
        }
        popupH = fitRows * itemH + kPopupBorderThickness;

        top = (spaceBelow >= spaceAbove) ? comboScreen.bottom
                                         : (comboScreen.top - popupH);
    }

    // 竖直方向再兜一次底：上面按"压到可用空间"算过之后仍越界（例如工作区比
    // 一行还矮）时，直接贴住工作区边缘，保证浮层左上角始终落在桌面内。
    if (top + popupH > work.bottom)
    {
        top = work.bottom - popupH;
    }
    if (top < work.top)
    {
        top = work.top;
    }

    // 水平方向：默认与下拉框左对齐，右侧越界就往左挪，挪到左边界为止。
    int left = comboScreen.left;
    if (left + popupW > work.right)
    {
        left = work.right - popupW;
    }
    if (left < work.left)
    {
        left = work.left;
    }

    RECT rc;
    rc.left   = left;
    rc.top    = top;
    rc.right  = left + popupW;
    rc.bottom = top + popupH;
    return rc;
}

PopupNotifyAction ClassifyPopupNotify(UINT code, UINT ctrlId)
{
    //只认列表本身发来的通知：列表内部滚动条滚动时同样发 DUIN_VALUECHANGED，
    //它的 ctrlId 不是 kPopupListCtrlId，不能当成选中
    if (ctrlId != kPopupListCtrlId)
    {
        return kPopupNotifyIgnore;
    }
    if (code == DUIN_VALUECHANGED)
    {
        return kPopupNotifySelect;
    }
    if (code == (UINT)DuiListBox::DUITN_ITEMDELETE)
    {
        return kPopupNotifyItemDelete;
    }
    return kPopupNotifyIgnore;
}

} // namespace combopopup

// ---------------------------------------------------------------------------
// DuiComboEdit —— 可编辑风格的下拉框内嵌的那个输入框。
//
// 它把内部的文字变化转交给宿主下拉框，由下拉框按<u>自己的</u>控件编号对外上报，
// 从而保证宿主窗口对一次改动只收到一条通知。
//
// 「对外只发一份通知」这个契约由两件事共同保证：一是本类把文字变化转成对下拉框
// 的内部回调，二是下拉框在创建本控件时调 SetNotificationsSuppressed(true)，把本
// 控件对外发出的通知整体关闭。后者无法由本类的覆写代劳 —— 无窗口输入框的通知
// 是直接送到宿主窗口的，不沿控件树逐级上传，外层控件无从拦截，只能在内嵌的这一
// 个控件上从源头关闭。该开关不影响下面这些内部钩子，下拉框照常能感知输入框内部
// 的变化。
// ---------------------------------------------------------------------------
class DuiComboEdit : public DuiEdit
{
public:
    // 记下宿主下拉框，供下面的内部钩子回调。
    //   c：宿主下拉框指针；所有权不在本控件。本控件是它的子控件，生存期短于它，
    //      因此不必考虑该指针失效。
    void SetCombo(DuiComboBox* c) { m_combo = c; }

    // 取得焦点。焦点标志位、文本光标的激活与重绘全部由基类完成 —— 无窗口实现
    // 下 DUI 层的标志位就是唯一的一份，不存在无窗口化之前「真正的焦点在内部子
    // 窗口上、DUI 层的标志位无人更新」的问题，因此这里不再手工设置。
    //   返回：基类的处理结果（不消费该事件）。
    bool OnSetFocus() override
    {
        return DuiEdit::OnSetFocus();
    }

    // 失去焦点，理由同 OnSetFocus。
    //   返回：基类的处理结果（不消费该事件）。
    bool OnKillFocus() override
    {
        return DuiEdit::OnKillFocus();
    }

protected:
    // 文本内容发生变化。用户输入与程序调用 SetText 都会走到这里，这一点与无
    // 窗口化之前由系统转发文字变化通知的行为一致。
    void OnTextChanged() override
    {
        DuiEdit::OnTextChanged();
        if (m_combo != nullptr)
        {
            m_combo->OnEditTextChanged();
        }
    }

private:
    DuiComboBox* m_combo = nullptr;    // 宿主下拉框；本控件不持有其所有权
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
        MSG_WM_MOUSEWHEEL(OnMouseWheel)
        MESSAGE_HANDLER_EX(WM_DUI_NOTIFY, OnDuiNotify)
    END_MSG_MAP()

    // 打开浮层。items 为各行（文字、副文字、图标，见 DuiComboBox::BuildPopupItems），
    // iconSize 为图标边长（像素），其余参数含义见下拉框的同名设置。
    void Open(DuiComboBox* owner, const RECT& screenRc,
              const std::vector<DuiComboBox::PopupItem>& items, int curSel, int itemH,
              int iconSize, bool showItemDelete);

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
    BOOL    OnMouseWheel(UINT mkFlags, short zDelta, CPoint pt);
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
                            const std::vector<DuiComboBox::PopupItem>& items, int curSel, int itemH,
                            int iconSize, bool showItemDelete)
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
    //OnDuiNotify 据这个 id 区分列表本身与列表内部滚动条发来的通知
    lb->SetCtrlId(combopopup::kPopupListCtrlId);
    lb->SetItemHeight(itemH);
    // 每行右侧的删除叉由 combo 决定开不开；关着时 DuiListBox 的绘制与命中
    // 与从前完全一致。
    lb->SetShowItemDelete(showItemDelete);
    // 每行的图标与副文字（没设的行为 NULL / 空串，绘制与以前相同）
    lb->SetIconSize(iconSize);
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int row = lb->AddItem(items[i].m_text, (LPARAM)i);
        lb->SetItemIcon(row, items[i].m_icon);
        lb->SetItemSubText(row, items[i].m_subText);
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

BOOL DuiComboBoxPopup::OnMouseWheel(UINT mkFlags, short zDelta, CPoint pt)
{
    // 滚轮消息由系统发给<u>焦点窗口</u>，而 Open 时焦点设在本浮层窗口上、不是内部
    // 那个 DuiHost 子窗口，所以不转发的话滚轮会一路走到 DefWindowProc 里没下文 ——
    // 表现就是项数超过 m_maxVisible 时，露不出来的那些项彻底够不着。
    //
    // 原样转发给 m_host：DuiHost::OnMouseWheel 按鼠标位置命中滚动容器（不走 focus），
    // 里面的 DuiListBox 自带滚动条，接上就能滚。lParam 本就是屏幕坐标，DuiHost 会
    // 自己 ScreenToClient，这里不必换算。
    if (m_host.IsWindow())
    {
        return (BOOL)::SendMessage(m_host.m_hWnd, WM_MOUSEWHEEL,
                                   MAKEWPARAM(mkFlags, zDelta),
                                   MAKELPARAM(pt.x, pt.y));
    }
    SetMsgHandled(FALSE);
    return FALSE;
}

LRESULT DuiComboBoxPopup::OnDuiNotify(UINT, WPARAM, LPARAM lParam)
{
    DuiNotify* n = reinterpret_cast<DuiNotify*>(lParam);
    if (!n)
    {
        return 0;
    }
    //宿主会把树里所有控件的通知都转过来，包括列表内部滚动条的 DUIN_VALUECHANGED（extra 是
    //滚动位置）。只比较通知码的话，滚一下滚轮就被当成选中、浮层随即关闭，故先按 ctrlId 分类
    const combopopup::PopupNotifyAction action = combopopup::ClassifyPopupNotify(n->code, n->ctrlId);
    if (action == combopopup::kPopupNotifySelect && m_owner)
    {
        // Write back to combo first; then schedule our own teardown to run
        // after this handler unwinds. Calling DestroyWindow synchronously
        // here would re-enter our own KillFocus / Activate handlers and,
        // via OnFinalMessage delete-self, leave 'this' dangling on return.
        m_owner->OnPopupSelected((int)n->extra);
        RequestClose();
    }
    else if (action == combopopup::kPopupNotifyItemDelete && m_owner)
    {
        // 点了某行的删除叉。先把通知冒给 combo 的宿主（业务侧多半要弹二次确认），
        // 再收掉浮层 —— 与选中那一支同样的理由：不能在本 handler 里同步销毁自己。
        // 关浮层放在通知之后，业务侧才好在确认框里安心地改 combo 的 items。
        m_owner->OnPopupItemDelete((int)n->extra);
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
    // 图标与副文字两个数组始终与 m_items 等长
    m_itemIcons.push_back(NULL);
    m_itemSubTexts.push_back(CString());
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
    if (index < (int)m_itemIcons.size())
    {
        m_itemIcons.erase(m_itemIcons.begin() + index);
    }
    if (index < (int)m_itemSubTexts.size())
    {
        m_itemSubTexts.erase(m_itemSubTexts.begin() + index);
    }
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
    m_itemIcons.clear();
    m_itemSubTexts.clear();
    m_curSel = -1;
    Invalidate();
}

void DuiComboBox::SetItemIcon(int index, HBITMAP hbm)
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return;
    }
    m_itemIcons.resize(m_items.size(), NULL);
    m_itemIcons[index] = hbm;
}

HBITMAP DuiComboBox::GetItemIcon(int index) const
{
    if (index < 0 || index >= (int)m_items.size() || index >= (int)m_itemIcons.size())
    {
        return NULL;
    }
    return m_itemIcons[index];
}

void DuiComboBox::SetItemSubText(int index, LPCTSTR sz)
{
    if (index < 0 || index >= (int)m_items.size())
    {
        return;
    }
    m_itemSubTexts.resize(m_items.size());
    m_itemSubTexts[index] = sz ? sz : _T("");
}

CString DuiComboBox::GetItemSubText(int index) const
{
    if (index < 0 || index >= (int)m_items.size() || index >= (int)m_itemSubTexts.size())
    {
        return CString();
    }
    return m_itemSubTexts[index];
}

std::vector<DuiComboBox::PopupItem> DuiComboBox::BuildPopupItems(const std::vector<int>& filteredIndices) const
{
    std::vector<PopupItem> out;
    const bool filtering = !filteredIndices.empty();
    const size_t count = filtering ? filteredIndices.size() : m_items.size();
    out.reserve(count);
    for (size_t k = 0; k < count; ++k)
    {
        const int realIdx = filtering ? filteredIndices[k] : (int)k;
        if (realIdx < 0 || realIdx >= (int)m_items.size())
        {
            continue;
        }
        PopupItem item;
        item.m_text = m_items[realIdx];
        item.m_subText = GetItemSubText(realIdx);
        item.m_icon = GetItemIcon(realIdx);
        out.push_back(item);
    }
    return out;
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
    // 各行的文字、副文字与图标由 BuildPopupItems 按过滤映射表取出（未过滤时取全部）。
    const bool filtering = m_incSearch && !m_filteredIndices.empty();
    const std::vector<int> noFilter;
    std::vector<PopupItem> popupItems = BuildPopupItems(filtering ? m_filteredIndices : noFilter);
    int popupCurSel = -1;
    if (filtering)
    {
        // 浮层里的第几行：只数映射表里有效的下标（与 BuildPopupItems 跳过越界下标的口径一致）
        int row = 0;
        for (size_t k = 0; k < m_filteredIndices.size(); ++k)
        {
            const int realIdx = m_filteredIndices[k];
            if (realIdx < 0 || realIdx >= (int)m_items.size())
            {
                continue;
            }
            if (realIdx == m_curSel)
            {
                popupCurSel = row;
            }
            ++row;
        }
    }
    else
    {
        popupCurSel = m_curSel;
    }
    if (popupItems.empty())
    {
        // Nothing to show (e.g. filter has zero hits) -> don't open.
        return;
    }

    // 浮层与下拉框同宽，默认贴在它正下方，高度 = min(项数, m_maxVisible) 行。
    // 落点<u>必须</u>夹进锚点所在显示器的工作区：项数多时浮层可以很高，直接贴在
    // 下方会掉出屏幕下缘，用户既看不见也够不着（与 DuiMenu 的落点约定同理）。
    POINT ptTopLeft     = { m_rcItem.left,  m_rcItem.top };
    POINT ptBottomRight = { m_rcItem.right, m_rcItem.bottom };
    ::ClientToScreen(hostHwnd, &ptTopLeft);
    ::ClientToScreen(hostHwnd, &ptBottomRight);
    RECT comboScreen;
    comboScreen.left   = ptTopLeft.x;
    comboScreen.top    = ptTopLeft.y;
    comboScreen.right  = ptBottomRight.x;
    comboScreen.bottom = ptBottomRight.y;

    int rows = (int)popupItems.size();
    if (rows > m_maxVisible)
    {
        rows = m_maxVisible;
    }
    int popupW = comboScreen.right - comboScreen.left;
    int popupH = rows * m_itemH + combopopup::kPopupBorderThickness;

    const RECT rc = combopopup::ClampPopupToWorkArea(comboScreen, popupW, popupH,
                                                     m_itemH,
                                                     WorkAreaOfRect(comboScreen));

    // Heap-allocate; popup deletes itself in OnFinalMessage. We hold a
    // raw pointer for re-entry checks only - ownership is on the popup
    // itself once it's been Open()'d.
    m_popup = new DuiComboBoxPopup();
    m_popup->Open(this, rc, popupItems, popupCurSel, m_itemH, m_iconSize, m_showItemDelete);
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

int DuiComboBox::MapPopupIndexWithFilter(int popupIndex,
                                         const std::vector<int>& filteredIndices)
{
    // 过滤激活时，浮层里的下标是"第几个命中项"，要经映射表换回 m_items 的下标。
    // 映射表为空（未过滤）时两者 1:1，原样返回。
    if (!filteredIndices.empty()
        && popupIndex >= 0 && popupIndex < (int)filteredIndices.size())
    {
        return filteredIndices[popupIndex];
    }
    return popupIndex;
}

int DuiComboBox::MapPopupIndexToItem(int popupIndex) const
{
    // 增量搜索没开时映射表不作数（可能是上一次留下的残留），直接按 1:1 处理。
    if (!m_incSearch)
    {
        return popupIndex;
    }
    return MapPopupIndexWithFilter(popupIndex, m_filteredIndices);
}

void DuiComboBox::OnPopupSelected(int index)
{
    // Translate popup-relative index back to the m_items index when an
    // incremental-search filter is active. Without filter, the popup's
    // index already lines up 1:1 with m_items.
    int realIdx = MapPopupIndexToItem(index);
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

void DuiComboBox::OnPaint(HDC hdc, const RECT& rcDirty)
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
        brdClr = (m_bHover || m_popupOpen) ? m_borderActive : m_borderNormal;
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

    // 绘制子控件 —— 可编辑风格下的内嵌输入框就是在这一步画出来的。
    //
    // 输入框无窗口化之前是一个真正的 Win32 子窗口，它的像素由系统在整个界面
    // 之上补齐，本方法不画它也照样看得见；无窗口实现完全依赖这一遍绘制，少了
    // 这一步用户看到的就只是一个空白圆角框加一个下拉箭头。
    //
    // 必须排在上面填充底色之后：底色铺满整个控件矩形，先画输入框会被它整个盖掉。
    // 下面的箭头与只读风格的文字则可以排在后面 —— 内嵌输入框的矩形已经把右侧
    // 箭头区让出来了（见 EditZoneRect），两者不重叠；只读风格下没有子控件，
    // 本次调用直接返回。
    DuiControl::OnPaint(hdc, rcDirty);

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
            HFONT useFont = GetDefaultFont();
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
    // 只控制下拉框主体那一圈边框。内嵌输入框的边框始终关闭，理由见
    // EnsureEditChild 里的说明：两圈边框会在圆角框内侧多出一个方框。
    m_showBorder = b;
    Invalidate();
}

void DuiComboBox::SetShowArrow(bool b)
{
    m_showArrow = b;
    Invalidate();
}

void DuiComboBox::SetShowItemDelete(bool b)
{
    if (m_showItemDelete == b)
    {
        return;
    }
    m_showItemDelete = b;
    // 只影响下次打开的浮层；已经开着的那个不动它 —— 开关一般在建控件时设一次，
    // 为这种边角情形去重建浮层不值当。
}

void DuiComboBox::OnPopupItemDelete(int index)
{
    // 与 OnPopupSelected 同理：增量搜索过滤激活时，浮层里的下标是过滤后的序号，
    // 必须映射回 m_items 的真实下标再上报 —— 否则宿主按这个序号去删，删掉的是
    // 另一个人（用户敲了几个字过滤之后点删除叉，最容易撞上）。
    const int realIdx = MapPopupIndexToItem(index);
    if (realIdx < 0 || realIdx >= (int)m_items.size())
    {
        return;
    }

    // 过滤到此结束（浮层随即关闭），与 OnPopupSelected 一样把映射表清掉，
    // 免得下次手动打开浮层时还停在上一次的过滤结果上。
    m_filteredIndices.clear();

    // 只上报，不删自己的 items：删不删、还要连带删掉什么，由宿主决定。
    //
    // 注意宿主收到这条通知后<u>不能</u>在它的 handler 里同步弹模态对话框：
    // 本调用还在浮层窗口的消息处理栈里，模态对话框会泵消息，浮层的
    // KillFocus / Activate 随即触发自我销毁，等模态框返回时浮层对象已经没了，
    // 栈回到 DuiComboBoxPopup::OnDuiNotify 就是在访问野指针。宿主应当
    // PostMessage 给自己，把确认框推迟到消息栈完全展开之后再弹。
    NotifyParent((UINT)DUICBN_ITEMDELETE, (LPARAM)realIdx);
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

void DuiComboBox::SetBorderColors(COLORREF normal, COLORREF active)
{
    if (m_borderNormal == normal && m_borderActive == active)
    {
        return;
    }
    m_borderNormal = normal;
    m_borderActive = active;
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
        // 关闭内嵌输入框对外发出的全部通知：宿主窗口对一次改动只应当收到一条
        // 通知，且携带的是下拉框自己的控件编号。无窗口输入框的通知直接送到宿主
        // 窗口、不沿控件树逐级上传，外层控件无从拦截，只能在这里从源头关闭。
        // 该开关不影响 DuiComboEdit 的内部钩子，本控件照常能感知输入框的变化。
        e->SetNotificationsSuppressed(true);
        // 底色与下拉框主体保持一致。
        e->SetBgColor(m_bgColor);
        // 内嵌的输入框一律不画自己的边框：下拉框主体已经画了一圈圆角边框，
        // 输入框再画一圈直角边框，就会在圆角框内侧多出一个方框。
        //
        // 无窗口化之前看不到这个问题，是因为当时下拉框根本不绘制子控件（输入
        // 框的像素由它自己的子窗口画在界面之上）。现在补上了子控件绘制，这一圈
        // 边框才会显现出来。
        e->SetShowBorder(false);
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
    // 无窗口输入框构造完就能用，不存在「尚未创建」这个状态，摆好矩形即可。
    // 无窗口化之前这里还要等宿主窗口就绪后再把内部子窗口创建出来，并因此补一次
    // 布局；那一步连同它依赖的窗口句柄查询一并去掉了。
    m_edit->SetRect(EditZoneRect());
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
