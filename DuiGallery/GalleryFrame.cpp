#include "stdafx.h"
#include "GalleryFrame.h"
#include "Pages.h"
#include "../balloonui/DuiAnimation.h"
#include "../balloonui/Tests/DuiNinePatchTests.h"
#include "../balloonui/Tests/DuiButtonTests.h"
#include "../balloonui/Tests/DuiSwitchTests.h"
#include "../balloonui/Tests/DuiAvatarTests.h"
#include "../balloonui/Tests/DuiSplitterTests.h"
#include "../balloonui/Tests/DuiTabPageTests.h"
#include "../balloonui/Tests/DuiTreeViewTests.h"
#include "../balloonui/Tests/DuiPopupHostTests.h"
#include "../balloonui/Tests/DuiEmojiPanelTests.h"
#include "../balloonui/Tests/DuiFrameWindowTests.h"
#include "../balloonui/Tests/DuiDpiTests.h"
#include "../balloonui/Tests/DuiImageOleTests.h"
#include "../balloonui/Tests/DuiDockTests.h"
#include "../balloonui/Tests/DuiAnimationTests.h"
#include "../balloonui/Tests/DuiAsyncImageTests.h"
#include "../balloonui/Tests/DuiGifTests.h"
#include "../balloonui/Tests/DuiDropTargetTests.h"
#include "../balloonui/Tests/DuiXmlBuilderTests.h"
#include "../balloonui/Tests/DuiCtlColorTests.h"
#include "../balloonui/Tests/DuiTier3Tests.h"
#include "../balloonui/Tests/DuiThemeTests.h"
#include "../balloonui/Tests/DuiListBoxTests.h"
#include "../balloonui/Tests/DuiMenuTests.h"
#include "../balloonui/Tests/DuiMenuBarTests.h"
#include "../balloonui/Tests/DuiTabTests.h"
#include "../balloonui/Tests/DuiMnemonicTests.h"
#include "../balloonui/Tests/DuiTier4Tests.h"
#include "../balloonui/Tests/DuiInspectorGoldenTests.h"
#include "../balloonui/Tests/DuiRichEditHostTests.h"
#include "../balloonui/Tests/DuiComboBoxTests.h"
#include "../balloonui/Tests/DuiLabelTests.h"
#include "../balloonui/Tests/DuiEditHostTests.h"
#include "../balloonui/Tests/DuiSearchBoxTests.h"
#include "../balloonui/Tests/DuiLayoutTests.h"

using namespace balloonwjui;

GalleryFrame::GalleryFrame()  = default;
GalleryFrame::~GalleryFrame() = default;

// Shared 60Hz pulse driving DuiAnimMgr. Any animated DUI control
// (DuiSwitch, DuiGif, DuiProgressBar marquee, future hover-fades) that
// schedules a DuiAnim depends on this timer firing — without it,
// AnimMgr's active list piles up and never advances. ID is arbitrary
// but stays out of WM_USER range to avoid collisions with control-internal
// timers (DuiToolTip, DuiMenu) that route through ::SetTimer(NULL,...).
static const UINT_PTR kAnimPulseTimerId = 0xA01;
static const UINT     kAnimPulseMs      = 16;   // ~60Hz

int GalleryFrame::OnCreate(LPCREATESTRUCT)
{
    CRect rcClient;
    GetClientRect(&rcClient);
    m_host.Create(m_hWnd, rcClient, nullptr,
                  WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);
    BuildRoot();
    SwitchToPage(0);
    SetTimer(kAnimPulseTimerId, kAnimPulseMs);

    // Dump unit test results to OutputDebugString (always) and, in debug
    // builds, to %TEMP%\DuiGallery_tests.log so headless verification
    // (build agent, CI) can confirm the results without attaching a
    // debugger. The temp-file write is a convenience and uses ANSI for
    // simplicity — the test report is ASCII-safe.
    CString report = DuiNinePatchTests::RunAll();
    report += _T("\r\n");
    report += DuiButtonTests::RunAll();
    report += _T("\r\n");
    report += DuiSwitchTests::RunAll();
    report += _T("\r\n");
    report += DuiAvatarTests::RunAll();
    report += _T("\r\n");
    report += DuiSplitterTests::RunAll();
    report += _T("\r\n");
    report += DuiTabPageTests::RunAll();
    report += _T("\r\n");
    report += DuiTreeViewTests::RunAll();
    report += _T("\r\n");
    report += DuiPopupHostTests::RunAll();
    report += _T("\r\n");
    report += DuiEmojiPanelTests::RunAll();
    report += _T("\r\n");
    report += DuiFrameWindowTests::RunAll();
    report += _T("\r\n");
    report += DuiDpiTests::RunAll();
    report += _T("\r\n");
    report += DuiImageOleTests::RunAll();
    report += _T("\r\n");
    report += DuiDockTests::RunAll();
    report += _T("\r\n");
    report += DuiAnimationTests::RunAll();
    report += _T("\r\n");
    report += DuiAsyncImageTests::RunAll();
    report += _T("\r\n");
    report += DuiGifTests::RunAll();
    report += _T("\r\n");
    report += DuiDropTargetTests::RunAll();
    report += _T("\r\n");
    report += DuiXmlBuilderTests::RunAll();
    report += _T("\r\n");
    report += DuiCtlColorTests::RunAll();
    report += _T("\r\n");
    report += DuiTier3Tests::RunAll();
    report += _T("\r\n");
    report += DuiThemeTests::RunAll();
    report += _T("\r\n");
    report += DuiListBoxTests::RunAll();
    report += _T("\r\n");
    report += DuiMenuTests::RunAll();
    report += _T("\r\n");
    report += DuiMenuBarTests::RunAll();
    report += _T("\r\n");
    report += DuiTabTests::RunAll();
    report += _T("\r\n");
    report += DuiMnemonicTests::RunAll();
    report += _T("\r\n");
    report += DuiTier4Tests::RunAll();
    report += _T("\r\n");
    report += DuiInspectorGoldenTests::RunAll();
    report += _T("\r\n");
    report += DuiRichEditHostTests::RunAll();
    report += _T("\r\n");
    report += DuiComboBoxTests::RunAll();
    report += _T("\r\n");
    report += DuiLabelTests::RunAll();
    report += _T("\r\n");
    report += DuiEditHostTests::RunAll();
    report += _T("\r\n");
    report += DuiSearchBoxTests::RunAll();
    report += _T("\r\n");
    report += DuiLayoutTests::RunAll();

    int start = 0;
    while (start < report.GetLength())
    {
        int crlf = report.Find(_T("\r\n"), start);
        CString line = (crlf < 0)
            ? report.Mid(start)
            : report.Mid(start, crlf - start);
        ::OutputDebugString(line);
        ::OutputDebugString(_T("\n"));
        if (crlf < 0)
        {
            break;
        }
        start = crlf + 2;
    }

    TCHAR tmp[MAX_PATH] = {};
    if (::GetTempPath(MAX_PATH, tmp))
    {
        CString path = tmp;
        if (!path.IsEmpty() && path[path.GetLength()-1] != _T('\\'))
        {
            path += _T("\\");
        }
        path += _T("DuiGallery_tests.log");
        HANDLE h = ::CreateFile(path,
                                GENERIC_WRITE,
                                FILE_SHARE_READ,
                                NULL, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE)
        {
            CStringA ansi(report);
            DWORD wrote = 0;
            ::WriteFile(h, (LPCSTR)ansi, ansi.GetLength(), &wrote, NULL);
            ::CloseHandle(h);
        }
    }
    return 0;
}

void GalleryFrame::OnDestroy()
{
    KillTimer(kAnimPulseTimerId);
    // Anim manager's active list may still hold setter closures pointing
    // at controls inside the host's tree. Clear it before tearing the host
    // down so a stray late-tick can't dereference dead memory.
    DuiAnimMgr::Inst().Clear();
    if (m_host.IsWindow())
    {
        m_host.DestroyWindow();
    }
    ::PostQuitMessage(0);
}

void GalleryFrame::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kAnimPulseTimerId)
    {
        DuiAnimMgr::Inst().TickAll(::GetTickCount());
    }
}

void GalleryFrame::OnSize(UINT, CSize size)
{
    if (m_host.IsWindow())
    {
        m_host.SetWindowPos(NULL, 0, 0, size.cx, size.cy,
                            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

BOOL GalleryFrame::OnEraseBkgnd(CDCHandle) { return TRUE; }

void GalleryFrame::BuildRoot()
{
    auto root = std::unique_ptr<DuiVBox>(new DuiVBox());

    // Tab strip (top).
    auto tab = std::unique_ptr<DuiTab>(new DuiTab());
    tab->SetCtrlId(1);
    int n = 0;
    const Pages::PageInfo* pages = Pages::GetPages(n);
    for (int i = 0; i < n; ++i)
    {
        tab->AddTab(pages[i].title);
    }
    tab->SetCurSel(0, /*notify=*/false);
    m_tab = tab.get();
    root->AddChild(std::move(tab), DuiLayout::Hint().Fixed(36));

    // Scroll view (fills remaining area).
    auto sv = std::unique_ptr<DuiScrollView>(new DuiScrollView());
    sv->SetCtrlId(2);
    m_scroll = sv.get();
    root->AddChild(std::move(sv), DuiLayout::Hint().Weight(1));

    m_host.SetRoot(std::move(root));
}

void GalleryFrame::SwitchToPage(int index)
{
    int n = 0;
    const Pages::PageInfo* pages = Pages::GetPages(n);
    if (index < 0 || index >= n)
    {
        return;
    }
    if (m_currentPage == index)
    {
        return;
    }
    m_currentPage = index;

    auto content = pages[index].build();
    m_scroll->SetContent(std::move(content));
    // Pages are DuiVBox-rooted with all-fixed children (Fixed(rowH) for
    // sections / variant rows / gaps), so the VBox's GetDesiredSize gives
    // an exact total height — no need to hand-pick a generous default.
    m_scroll->SetAutoContentHeight(true);
    m_scroll->SetScrollPos(0);
    m_scroll->Invalidate();
}

LRESULT GalleryFrame::OnDuiNotify(UINT, WPARAM, LPARAM lParam)
{
    DuiNotify* n = reinterpret_cast<DuiNotify*>(lParam);
    if (!n)
    {
        return 0;
    }
    if (m_tab && n->ctrlId == m_tab->GetCtrlId() && n->code == DUIN_VALUECHANGED)
    {
        SwitchToPage((int)n->extra);
    }
    return 0;
}
