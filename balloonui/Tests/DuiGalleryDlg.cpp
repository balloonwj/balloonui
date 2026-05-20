#include "stdafx.h"
#include "DuiGalleryDlg.h"

#if defined(BUI_FEATURE_GALLERY)

#include "DuiKernelSmokeControl.h"
#include "DuiLayoutTests.h"
#include "DuiScrollBarTests.h"
#include "DuiButtonTests.h"
#include "DuiLabelTests.h"
#include "DuiEditHostTests.h"
#include "DuiRichEditHostTests.h"
#include "DuiNinePatchTests.h"
#include "DuiListBoxTests.h"
#include "DuiTabTests.h"
#include "DuiSmallControlsTests.h"
#include "DuiToolTipTests.h"
#include "DuiMenuTests.h"
#include "DuiMenuBarTests.h"
#include "../Controls/List/DuiMenu.h"
#include "../Controls/List/DuiMenuBar.h"
#include "../Controls/Layout/DuiLayout.h"
#include "../Controls/Window/DuiScrollBar.h"
#include "../Controls/Basic/DuiButton.h"
#include "../Controls/Basic/DuiLabel.h"
#include "../Controls/Input/DuiEditHost.h"
#include "../Controls/List/DuiListBox.h"
#include "../Controls/List/DuiTab.h"
#include "../Controls/Input/DuiComboBox.h"
#include "../Controls/Feedback/DuiProgressBar.h"
#include "../Controls/Input/DuiSlider.h"
#include "../Controls/Feedback/DuiToolTip.h"
#include "../DuiResMgr.h"
#include "../../../ImageEx.h"

namespace balloonwjui {

DuiGalleryDlg::DuiGalleryDlg() = default;
DuiGalleryDlg::~DuiGalleryDlg() = default;

HWND DuiGalleryDlg::ShowGallery(HWND hParent)
{
    if (m_hWnd)
    {
        ::SetForegroundWindow(m_hWnd);
        return m_hWnd;
    }
    HWND h = Create(hParent, CWindow::rcDefault, _T("DUI Gallery"),
                    WS_OVERLAPPEDWINDOW, 0);
    if (h)
    {
        ResizeClient(800, 500);
        ShowWindow(SW_SHOW);
        UpdateWindow();
    }
    return h;
}

int DuiGalleryDlg::OnCreate(LPCREATESTRUCT)
{
    CRect rcClient;
    GetClientRect(&rcClient);
    rcClient.bottom -= m_logHeight;

    m_host.Create(m_hWnd, rcClient, nullptr,
                  WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                  0);

    BuildKernelSmokeScenario();
    LayoutHost();

    // Run unit tests at startup; dump results into the log.
    AppendLog(_T("[gallery] smoke + HBox + ScrollView + Button + Label + Edit scenario loaded"));
    AppendLog(DuiLayoutTests::RunAll());
    AppendLog(DuiScrollBarTests::RunAll());
    AppendLog(DuiButtonTests::RunAll());
    AppendLog(DuiLabelTests::RunAll());
    AppendLog(DuiEditHostTests::RunAll());
    AppendLog(DuiRichEditHostTests::RunAll());
    AppendLog(DuiNinePatchTests::RunAll());
    AppendLog(DuiListBoxTests::RunAll());
    AppendLog(DuiTabTests::RunAll());
    AppendLog(DuiSmallControlsTests::RunAll());
    AppendLog(DuiToolTipTests::RunAll());
    AppendLog(DuiMenuTests::RunAll());
    AppendLog(DuiMenuBarTests::RunAll());
    return 0;
}

void DuiGalleryDlg::OnDestroy()
{
    if (m_host.IsWindow())
    {
        m_host.DestroyWindow();
    }
}

void DuiGalleryDlg::OnSize(UINT, CSize)
{
    LayoutHost();
    Invalidate(FALSE);
}

void DuiGalleryDlg::LayoutHost()
{
    if (!m_host.IsWindow())
    {
        return;
    }
    CRect rcClient;
    GetClientRect(&rcClient);
    int hostBottom = rcClient.bottom - m_logHeight;
    if (hostBottom < rcClient.top)
    {
        hostBottom = rcClient.top;
    }
    m_host.SetWindowPos(nullptr, 0, 0,
                        rcClient.right, hostBottom,
                        SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL DuiGalleryDlg::OnEraseBkgnd(CDCHandle)
{
    return TRUE;
}

void DuiGalleryDlg::OnPaint(CDCHandle)
{
    CPaintDC dc(m_hWnd);
    CRect rcClient;
    GetClientRect(&rcClient);

    // Fill log strip area only (host paints its own area).
    CRect rcLog = rcClient;
    rcLog.top = rcClient.bottom - m_logHeight;
    if (rcLog.top < rcClient.top)
    {
        rcLog.top = rcClient.top;
    }

    HBRUSH hbr = ::GetSysColorBrush(COLOR_INFOBK);
    ::FillRect(dc, &rcLog, hbr);
    HPEN hpen = ::CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
    HPEN hOld = (HPEN)::SelectObject(dc, hpen);
    ::MoveToEx(dc, rcLog.left, rcLog.top, nullptr);
    ::LineTo  (dc, rcLog.right, rcLog.top);
    ::SelectObject(dc, hOld);
    ::DeleteObject(hpen);

    rcLog.DeflateRect(8, 4);
    int oldBk = ::SetBkMode(dc, TRANSPARENT);
    ::DrawText(dc, m_log, -1, &rcLog,
               DT_LEFT | DT_TOP | DT_NOPREFIX | DT_EDITCONTROL | DT_WORDBREAK);
    ::SetBkMode(dc, oldBk);
}

void DuiGalleryDlg::OnKeyDown(TCHAR vk, UINT, UINT)
{
    if (vk == 'C' && (::GetKeyState(VK_CONTROL) & 0x8000))
    {
        ClearLog();
    }
    if (vk == VK_ESCAPE)
    {
        DestroyWindow();
    }
}

void DuiGalleryDlg::AppendLog(LPCTSTR sz)
{
    if (!m_log.IsEmpty())
    {
        m_log += _T("\r\n");
    }
    m_log += sz;
    Invalidate(FALSE);
}

LRESULT DuiGalleryDlg::OnSysKeyDown(UINT, WPARAM wParam, LPARAM)
{
#if BUI_FEATURE_MENUBAR
    // Alt + letter 触发 WM_SYSKEYDOWN（系统级），转给 menu bar 走助记符路由。
    // 命中则消费消息（返 0），不命中让 DefWindowProc 走默认 Alt 处理。
    if (m_menuBar && m_menuBar->ProcessAltKey((UINT)wParam))
    {
        return 0;
    }
#else
    (void)wParam;
#endif
    SetMsgHandled(FALSE);
    return 0;
}

LRESULT DuiGalleryDlg::OnDuiNotify(UINT, WPARAM wParam, LPARAM lParam)
{
    DuiNotify* pn = reinterpret_cast<DuiNotify*>(lParam);
    if (!pn)
    {
        return 0;
    }
    CString line;
    LPCTSTR codeName = _T("?");
    switch (pn->code)
    {
    case DUIN_CLICK:        codeName = _T("CLICK");        break;
    case DUIN_DBLCLK:       codeName = _T("DBLCLK");       break;
    case DUIN_RCLICK:       codeName = _T("RCLICK");       break;
    case DUIN_MOUSEENTER:   codeName = _T("MOUSEENTER");   break;
    case DUIN_MOUSELEAVE:   codeName = _T("MOUSELEAVE");   break;
    case DUIN_SETFOCUS:     codeName = _T("SETFOCUS");     break;
    case DUIN_KILLFOCUS:    codeName = _T("KILLFOCUS");    break;
    case DUIN_VALUECHANGED: codeName = _T("VALUECHANGED"); break;
    case DuiTab::DUITN_CLOSE:    codeName = _T("TAB_CLOSE");    break;
    case DuiTab::DUITN_DROPDOWN: codeName = _T("TAB_DROPDOWN"); break;
#if BUI_FEATURE_MENUBAR
    case DuiMenuBar::DUIMBN_DROPDOWN_OPEN:
        codeName = _T("MBAR_OPEN");  break;
    case DuiMenuBar::DUIMBN_DROPDOWN_CLOSE:
        codeName = _T("MBAR_CLOSE"); break;
#endif
    default: break;
    }
    line.Format(_T("[notify] ctrlId=%u code=%s extra=%d"),
                (unsigned)pn->ctrlId, codeName, (int)pn->extra);
    AppendLog(line);

    // Honor tab close: actually remove the tab so the user sees the strip change.
    if (pn->code == DuiTab::DUITN_CLOSE && m_tab)
    {
        m_tab->RemoveTab((int)pn->extra);
    }

    // Tab dropdown: pop a small DuiMenu so the dropdown actually does
    // something. Anchor the menu under the tab.
    if (pn->code == DuiTab::DUITN_DROPDOWN && m_tab)
    {
        // Try to load a few small icons for demo. Failed loads return
        // nullptr - menu falls back to icon-less rendering automatically.
        CImageEx* iconPin   = DuiResMgr::Inst().AcquireImage(_T("Menu\\menu_check.png"));
        CImageEx* iconClose = DuiResMgr::Inst().AcquireImage(_T("Menu\\close_normal.png"));

        DuiMenu menu;
        menu.AppendItem    (1001, _T("Pin tab"), iconPin);
        menu.AppendChecked (1002, _T("Mute notifications"), false);
        menu.AppendSeparator();
        DuiMenu sub;
        sub.AppendItem(2001, _T("English"));
        sub.AppendItem(2002, _T("Chinese"));
        sub.AppendItem(2003, _T("Japanese"));
        menu.AppendSubMenu (1003, _T("Language"), &sub);
        menu.AppendDisabled(1004, _T("Move to (disabled)"), iconClose);   // disabled icon: auto-grayed
        menu.AppendSeparator();
        menu.AppendItem    (1099, _T("Close other tabs"), iconClose);

        // Anchor: bottom-left of the tab strip, offset by the tab's index.
        RECT rcTab;
        m_host.GetWindowRect(&rcTab);
        int x = rcTab.left + 100 + (int)pn->extra * 80;
        int y = rcTab.bottom - 200;
        UINT chosen = menu.TrackPopup(x, y, m_hWnd);
        CString s;
        s.Format(_T("[menu] chosen=%u"), chosen);
        AppendLog(s);
    }

    // Right-click anywhere -> demo context menu so the right-click path
    // is exercised too.
    if (pn->code == DUIN_RCLICK)
    {
        POINT pt;
        ::GetCursorPos(&pt);
        DuiMenu ctx;
        ctx.AppendItem    (3001, _T("Cut"));
        ctx.AppendItem    (3002, _T("Copy"));
        ctx.AppendDisabled(3003, _T("Paste"));
        ctx.AppendSeparator();
        ctx.AppendChecked (3004, _T("Word wrap"), true);
        UINT chosen = ctx.TrackPopup(pt.x, pt.y, m_hWnd);
        CString s;
        s.Format(_T("[ctxmenu] chosen=%u"), chosen);
        AppendLog(s);
    }
    return 0;
}

// Tall column control used inside DuiScrollView demo.
// Draws 30 horizontal colored bands of fixed height; total height is fixed.
class TallColumn : public DuiControl
{
public:
    enum { BAND_COUNT = 30, BAND_H = 36 };
    int DesiredHeight() const { return BAND_COUNT * BAND_H; }

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }
        // Paint each band that intersects rcDirty.
        for (int i = 0; i < BAND_COUNT; ++i)
        {
            RECT band = { m_rcItem.left,
                          m_rcItem.top + i * BAND_H,
                          m_rcItem.right,
                          m_rcItem.top + (i + 1) * BAND_H };
            RECT inter;
            if (!::IntersectRect(&inter, &band, &rcDirty))
            {
                continue;
            }
            int hue = (i * 23) % 256;
            COLORREF clr = RGB(180 + (hue % 60), 200 - (hue % 80), 220 - (hue % 90));
            HBRUSH hbr = ::CreateSolidBrush(clr);
            ::FillRect(hdc, &inter, hbr);
            ::DeleteObject(hbr);

            HPEN hpen = ::CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
            HPEN oldPen = (HPEN)::SelectObject(hdc, hpen);
            ::MoveToEx(hdc, band.left, band.bottom - 1, NULL);
            ::LineTo(hdc, band.right, band.bottom - 1);
            ::SelectObject(hdc, oldPen);
            ::DeleteObject(hpen);

            CString label;
            label.Format(_T("band %02d"), i);
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            RECT rText = band;
            rText.left += 8;
            ::DrawText(hdc, label, -1, &rText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ::SetBkMode(hdc, oldBk);
        }
    }
};

// Visual layout demo using DuiHBox + DuiVBox + DuiGrid + DuiScrollView.
//
// Top-level VBox:
//   - Row 1 (HBox, weight 1) : three smoke controls (A weight 1, B fixed 80, C weight 2)
//   - Row 2 (Grid 2x3, weight 1) : six smoke controls D..I in a grid with gap=8
//
// This proves: weighted vs. fixed, padding, gap, nested layouts.
void DuiGalleryDlg::BuildKernelSmokeScenario()
{
    auto root = std::unique_ptr<DuiVBox>(new DuiVBox());
    root->SetPadding(12);
    root->SetGap(12);

#if BUI_FEATURE_MENUBAR
    // ---- Row 0: DuiMenuBar 演示（File / Options / View）----
    // 3 个 DuiMenu 是 m_*Menu 成员（lifetime ≥ menu bar），bar 借用指针。
    m_fileMenu.Clear();
    m_fileMenu.AppendItem    (701, _T("&Open..."));
    m_fileMenu.AppendItem    (702, _T("&Save"));
    m_fileMenu.AppendSeparator();
    m_fileMenu.AppendItem    (703, _T("E&xit"));

    m_optionsMenu.Clear();
    m_optionsMenu.AppendChecked(721, _T("Always on &top"),    false);
    m_optionsMenu.AppendChecked(722, _T("&Hide on minimize"), true);
    m_optionsMenu.AppendChecked(723, _T("Show on &demand"),   false);

    m_viewMenu.Clear();
    m_viewMenu.AppendItem    (741, _T("&Refresh\tF5"));
    m_viewMenu.AppendSeparator();
    m_viewMenu.AppendChecked (742, _T("Update speed: high"),   false);
    m_viewMenu.AppendChecked (743, _T("Update speed: normal"), true);
    m_viewMenu.AppendChecked (744, _T("Update speed: low"),    false);
    m_viewMenu.AppendChecked (745, _T("Update speed: paused"), false);
    m_viewMenu.AppendSeparator();
    m_viewMenu.AppendChecked (746, _T("&Group by type"), true);
    m_viewMenu.AppendItem    (747, _T("&Expand all"));

    auto bar = std::unique_ptr<DuiMenuBar>(new DuiMenuBar());
    bar->SetCtrlId(700);
    bar->AppendItem(710, _T("&File"),    &m_fileMenu);
    bar->AppendItem(720, _T("&Options"), &m_optionsMenu);
    bar->AppendItem(740, _T("&View"),    &m_viewMenu);
    m_menuBar = bar.get();
    root->AddChild(std::move(bar), DuiLayout::Hint().Fixed(24));
#endif

    // ---- Row 1: HBox (A | B | C) with mixed sizing ----
    auto row1 = std::unique_ptr<DuiHBox>(new DuiHBox());
    row1->SetGap(8);
    DuiHBox* row1Raw = row1.get();

    auto a = std::unique_ptr<DuiKernelSmokeControl>(new DuiKernelSmokeControl());
    a->SetCtrlId(101);
    a->SetLabel(_T("A weight=1"));
    auto b = std::unique_ptr<DuiKernelSmokeControl>(new DuiKernelSmokeControl());
    b->SetCtrlId(102);
    b->SetLabel(_T("B fixed=80"));
    auto c = std::unique_ptr<DuiKernelSmokeControl>(new DuiKernelSmokeControl());
    c->SetCtrlId(103);
    c->SetLabel(_T("C weight=2"));

    row1Raw->AddChild(std::move(a), DuiLayout::Hint().Weight(1));
    row1Raw->AddChild(std::move(b), DuiLayout::Hint().Fixed(80));
    row1Raw->AddChild(std::move(c), DuiLayout::Hint().Weight(2));

    // ---- Row 1.2: DuiTab strip (closeable + dropdown variants) ----
    auto tab = std::unique_ptr<DuiTab>(new DuiTab());
    tab->SetCtrlId(140);
    tab->AddTab(_T("Friends"));
    tab->AddTab(_T("Groups"),  /*closeable=*/false, /*dropdown=*/true);
    tab->AddTab(_T("Recent"),  /*closeable=*/false, /*dropdown=*/false);
    tab->AddTab(_T("Chat: Alice"), /*closeable=*/true);
    tab->AddTab(_T("Chat: Bob"),   /*closeable=*/true);
    tab->SetCurSel(0, /*notify=*/false);
    m_tab = tab.get();

    // ---- Row 1.3: DuiEditHost parade (single line + password + multi-line) ----
    auto rowEdit = std::unique_ptr<DuiHBox>(new DuiHBox());
    rowEdit->SetGap(8);
    DuiHBox* rowEditRaw = rowEdit.get();

    auto eName = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    eName->SetCtrlId(130);
    eName->SetPlaceholder(_T("username"));
    DuiEditHost* eNameRaw = eName.get();

    auto ePwd = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    ePwd->SetCtrlId(131);
    ePwd->SetPassword(true);
    ePwd->SetPlaceholder(_T("password"));
    DuiEditHost* ePwdRaw = ePwd.get();

    auto eMulti = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    eMulti->SetCtrlId(132);
    eMulti->SetMultiLine(true);
    eMulti->SetPlaceholder(_T("notes (multi-line, IME, drag to select, etc.)"));
    DuiEditHost* eMultiRaw = eMulti.get();

    rowEditRaw->AddChild(std::move(eName),  DuiLayout::Hint().Weight(1));
    rowEditRaw->AddChild(std::move(ePwd),   DuiLayout::Hint().Weight(1));
    rowEditRaw->AddChild(std::move(eMulti), DuiLayout::Hint().Weight(2));

    // Stash raw pointers in members so we can EnsureCreated after the
    // host owns them.
    m_edits[0] = eNameRaw;
    m_edits[1] = ePwdRaw;
    m_edits[2] = eMultiRaw;

    // ---- Row 1.4: DuiLabel parade (text label + auto-nav link + non-nav "register" link) ----
    auto rowLbl = std::unique_ptr<DuiHBox>(new DuiHBox());
    rowLbl->SetGap(12);
    DuiHBox* rowLblRaw = rowLbl.get();

    auto lText = std::unique_ptr<DuiLabel>(new DuiLabel());
    lText->SetCtrlId(120);
    lText->SetText(_T("Status: ready"));
    lText->SetTextColor(RGB(60, 60, 60));

    auto lLink = std::unique_ptr<DuiLabel>(new DuiLabel());
    lLink->SetCtrlId(121);
    lLink->SetMode(DuiLabel::ModeLink);
    lLink->SetText(_T("Open example.com"));
    lLink->SetUrl(_T("https://example.com"));
    lLink->SetAutoNavigate(true);

    auto lReg = std::unique_ptr<DuiLabel>(new DuiLabel());
    lReg->SetCtrlId(122);
    lReg->SetMode(DuiLabel::ModeLink);
    lReg->SetText(_T("Register account"));
    lReg->SetAutoNavigate(false);   // parent handles DUIN_CLICK

    rowLblRaw->AddChild(std::move(lText), DuiLayout::Hint().Weight(2));
    rowLblRaw->AddChild(std::move(lLink), DuiLayout::Hint().Weight(2));
    rowLblRaw->AddChild(std::move(lReg),  DuiLayout::Hint().Weight(2));

    // ---- Row 1.45: small controls (DuiComboBox + DuiSlider + DuiProgressBar) ----
    auto rowSmall = std::unique_ptr<DuiHBox>(new DuiHBox());
    rowSmall->SetGap(8);
    DuiHBox* rowSmallRaw = rowSmall.get();

    // Read-only combo (CBS_DROPDOWNLIST equivalent)
    auto cb = std::unique_ptr<DuiComboBox>(new DuiComboBox());
    cb->SetCtrlId(150);
    cb->AddString(_T("Online"));
    cb->AddString(_T("Away"));
    cb->AddString(_T("Busy"));
    cb->AddString(_T("Invisible"));
    cb->AddString(_T("Offline"));
    cb->SetCurSel(0, /*notify=*/false);

    // Editable combo (CBS_DROPDOWN equivalent)
    auto cbEd = std::unique_ptr<DuiComboBox>(new DuiComboBox());
    cbEd->SetCtrlId(153);
    cbEd->SetEditable(true);
    cbEd->AddString(_T("Apple"));
    cbEd->AddString(_T("Banana"));
    cbEd->AddString(_T("Cherry"));
    cbEd->AddString(_T("Durian"));
    cbEd->SetText(_T("Apple"));   // start with prefilled text + matching sel

    auto sl = std::unique_ptr<DuiSlider>(new DuiSlider());
    sl->SetCtrlId(151);
    sl->SetRange(0, 100);
    sl->SetLineSize(5);
    sl->SetPos(35, /*notify=*/false);

    auto pb = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
    pb->SetCtrlId(152);
    pb->SetRange(0, 100);
    pb->SetPos(60, /*notify=*/false);

    DuiToolTipMgr::Inst().Register(cb.get(),   _T("Read-only status combo (CBS_DROPDOWNLIST)"));
    DuiToolTipMgr::Inst().Register(cbEd.get(), _T("Editable combo - type or pick from list"));
    DuiToolTipMgr::Inst().Register(sl.get(),   _T("Slider: drag thumb or use arrow keys"));
    DuiToolTipMgr::Inst().Register(pb.get(),   _T("Progress bar (60%)"));

    rowSmallRaw->AddChild(std::move(cb),   DuiLayout::Hint().Weight(1));
    rowSmallRaw->AddChild(std::move(cbEd), DuiLayout::Hint().Weight(1));
    rowSmallRaw->AddChild(std::move(sl),   DuiLayout::Hint().Weight(1));
    rowSmallRaw->AddChild(std::move(pb),   DuiLayout::Hint().Weight(1));

    // ---- Row 1.5: DuiButton parade (push, icon, checkbox, two radios in group 7) ----
    auto rowBtn = std::unique_ptr<DuiHBox>(new DuiHBox());
    rowBtn->SetGap(8);
    DuiHBox* rowBtnRaw = rowBtn.get();

    auto bPush = std::unique_ptr<DuiButton>(new DuiButton());
    bPush->SetCtrlId(110);
    bPush->SetText(_T("Push"));
    auto bIcon = std::unique_ptr<DuiButton>(new DuiButton());
    bIcon->SetCtrlId(111);
    bIcon->SetButtonType(DuiButton::StyleIcon);
    bIcon->SetText(_T("Icon"));
    auto bCheck = std::unique_ptr<DuiButton>(new DuiButton());
    bCheck->SetCtrlId(112);
    bCheck->SetButtonType(DuiButton::StyleCheckbox);
    bCheck->SetText(_T("Remember"));
    auto bRadio1 = std::unique_ptr<DuiButton>(new DuiButton());
    bRadio1->SetCtrlId(113);
    bRadio1->SetButtonType(DuiButton::StyleRadio);
    bRadio1->SetRadioGroup(7);
    bRadio1->SetText(_T("Online"));
    bRadio1->SetCheck(true, false);
    auto bRadio2 = std::unique_ptr<DuiButton>(new DuiButton());
    bRadio2->SetCtrlId(114);
    bRadio2->SetButtonType(DuiButton::StyleRadio);
    bRadio2->SetRadioGroup(7);
    bRadio2->SetText(_T("Away"));

    // Tooltip registrations (hover ~500ms to see them).
    DuiToolTipMgr& tt = DuiToolTipMgr::Inst();
    tt.Register(bPush.get(),   _T("Send a chat message"));
    tt.Register(bIcon.get(),   _T("Open the file picker"));
    tt.Register(bCheck.get(),  _T("Remember sign-in for next launch"));
    tt.Register(bRadio1.get(), _T("Show as online to friends"));
    tt.Register(bRadio2.get(), _T("Show as away (auto-reply enabled)"));

    rowBtnRaw->AddChild(std::move(bPush),   DuiLayout::Hint().Weight(1));
    rowBtnRaw->AddChild(std::move(bIcon),   DuiLayout::Hint().Weight(1));
    rowBtnRaw->AddChild(std::move(bCheck),  DuiLayout::Hint().Weight(1));
    rowBtnRaw->AddChild(std::move(bRadio1), DuiLayout::Hint().Weight(1));
    rowBtnRaw->AddChild(std::move(bRadio2), DuiLayout::Hint().Weight(1));

    // ---- Row 2: Grid 2x3 with smoke controls ----
    auto row2 = std::unique_ptr<DuiGrid>(new DuiGrid());
    row2->SetGrid(2, 3);
    row2->SetGap(8);
    DuiGrid* row2Raw = row2.get();

    LPCTSTR labels[6] = { _T("D"), _T("E"), _T("F"), _T("G"), _T("H"), _T("I") };
    for (int i = 0; i < 6; ++i)
    {
        auto cell = std::unique_ptr<DuiKernelSmokeControl>(new DuiKernelSmokeControl());
        cell->SetCtrlId(200 + i);
        cell->SetLabel(labels[i]);
        row2Raw->AddChild(std::move(cell));
    }

    // ---- Row 3: HBox combining ScrollView + ListBox + VirtualList ----
    auto row3 = std::unique_ptr<DuiHBox>(new DuiHBox());
    row3->SetGap(8);

    auto sv = std::unique_ptr<DuiScrollView>(new DuiScrollView());
    DuiScrollView* svRaw = sv.get();
    auto col = std::unique_ptr<TallColumn>(new TallColumn());
    int contentH = col->DesiredHeight();
    svRaw->SetContent(std::move(col));
    svRaw->SetContentHeight(contentH);
    svRaw->SetCtrlId(300);

    auto lb = std::unique_ptr<DuiListBox>(new DuiListBox());
    lb->SetCtrlId(310);
    LPCTSTR sample[10] = {
        _T("Apple"), _T("Banana"), _T("Cherry"), _T("Durian"), _T("Eggplant"),
        _T("Fig"),   _T("Grape"),  _T("Hazel"),  _T("Iris"),   _T("Jujube")
    };
    for (int i = 0; i < 10; ++i)
    {
        lb->AddItem(sample[i], (LPARAM)i);
    }
    lb->SetCurSel(0, /*notify=*/false);

    auto vl = std::unique_ptr<DuiVirtualList>(new DuiVirtualList());
    vl->SetCtrlId(320);
    vl->SetRowCount(10000);
    vl->SetRowHeight(28);
    vl->SetPaintRowCallback(
        [](void*, HDC hdc, int idx, const RECT& rc, bool sel, bool hov)
        {
            COLORREF bgClr = sel ? RGB(180, 210, 245)
                                 : (hov ? RGB(232, 240, 252)
                                        : ((idx & 1) ? RGB(248, 248, 252) : RGB(255, 255, 255)));
            HBRUSH hbr = ::CreateSolidBrush(bgClr);
            ::FillRect(hdc, &rc, hbr);
            ::DeleteObject(hbr);
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, sel ? RGB(10, 30, 90) : RGB(40, 40, 40));
            CString s;
            s.Format(_T("  Row #%05d   (virtual)"), idx);
            RECT rt = rc;
            rt.left += 6;
            ::DrawText(hdc, s, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
        }, nullptr);

    row3->AddChild(std::move(sv), DuiLayout::Hint().Weight(2));
    row3->AddChild(std::move(lb), DuiLayout::Hint().Weight(1));
    row3->AddChild(std::move(vl), DuiLayout::Hint().Weight(2));

    root->AddChild(std::move(row1),    DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(tab),     DuiLayout::Hint().Fixed(32));  // tab strip row
    root->AddChild(std::move(rowEdit), DuiLayout::Hint().Fixed(72));  // edit row: 1-line + 1-line + multiline
    root->AddChild(std::move(rowLbl),  DuiLayout::Hint().Fixed(24));  // fixed height for labels
    root->AddChild(std::move(rowSmall),DuiLayout::Hint().Fixed(28));  // combo + slider + progress
    root->AddChild(std::move(rowBtn),  DuiLayout::Hint().Fixed(34));  // fixed height for buttons
    root->AddChild(std::move(row2),    DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(row3),    DuiLayout::Hint().Weight(2));  // taller area for scrolling

    m_host.SetRoot(std::move(root));

    // Now that the layout has run and the host is wired, create the actual
    // EDIT HWNDs (their parent is the host's HWND so WM_COMMAND routes back).
    HWND hParent = m_host.m_hWnd;
    for (int i = 0; i < 3; ++i)
    {
        if (m_edits[i])
        {
            m_edits[i]->EnsureCreated(hParent);
            // Re-Layout once more so the freshly-created EDIT picks up its
            // current rect (EnsureCreated uses m_rcItem at create time, but
            // SetRect during Layout was called before the HWND existed).
            m_edits[i]->Layout(m_edits[i]->GetRect());
        }
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_GALLERY
