#pragma once

#include "../BalloonUiFeatures.h"
#if defined(BUI_FEATURE_GALLERY)

// .cpp must include stdafx.h first (project-wide PCH convention).
#include "../DuiHost.h"
#include "../DuiNotify.h"
#if BUI_FEATURE_MENUBAR
#  include "../Controls/List/DuiMenu.h"
#endif

namespace balloonwjui {

class DuiEditHost;
class DuiTab;
class DuiMenuBar;

// Standalone test harness for DUI controls. Debug-only entry point.
//
// Hosted layout:  one DuiHost fills the client area; its root is a vertical
// stack we build per-scenario. Notifications from the host bubble up to
// this window via WM_DUI_NOTIFY and are appended to a log shown at the
// bottom (drawn directly on this HWND, not by the DUI tree).
//
// Phase 1 scenario: three DuiKernelSmokeControl instances side by side.
// Later phases register more scenarios via AddScenario(name, builder).
class DuiGalleryDlg : public CWindowImpl<DuiGalleryDlg>
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiGalleryDlg__"), CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    DuiGalleryDlg();
    ~DuiGalleryDlg();

    // Open the gallery as a top-level window. Returns the HWND.
    HWND ShowGallery(HWND hParent);

    BEGIN_MSG_MAP(DuiGalleryDlg)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MSG_WM_KEYDOWN(OnKeyDown)
        MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnSysKeyDown)
        MESSAGE_HANDLER_EX(WM_DUI_NOTIFY, OnDuiNotify)
    END_MSG_MAP()

    // Debug log line - also called by tests to assert order of events.
    void    AppendLog(LPCTSTR sz);
    CString GetLog() const { return m_log; }
    void    ClearLog() { m_log.Empty(); Invalidate(FALSE); }

protected:
    int     OnCreate(LPCREATESTRUCT lpcs);
    void    OnDestroy();
    void    OnSize(UINT nType, CSize size);
    void    OnPaint(CDCHandle dc);
    BOOL    OnEraseBkgnd(CDCHandle dc);
    void    OnKeyDown(TCHAR vk, UINT nRepCnt, UINT nFlags);
    LRESULT OnSysKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnDuiNotify(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    void    BuildKernelSmokeScenario();
    void    LayoutHost();

private:
    DuiHost      m_host;
    CString      m_log;
    int          m_logHeight = 120;
    DuiEditHost* m_edits[3] = { nullptr, nullptr, nullptr };
    DuiTab*      m_tab      = nullptr;
#if BUI_FEATURE_MENUBAR
    // 3 个 DuiMenu 是 DuiMenuBar 栏目的下拉，lifetime 必须 ≥ menu bar。
    // DuiMenuBar 不持有它们的所有权（只借指针），所以放在本类成员里。
    DuiMenu      m_fileMenu;
    DuiMenu      m_optionsMenu;
    DuiMenu      m_viewMenu;
    DuiMenuBar*  m_menuBar  = nullptr;     // raw；m_host 持有所有权
#endif
};

} // namespace balloonwjui

#endif // BUI_FEATURE_GALLERY
