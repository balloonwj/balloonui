#pragma once

#include "DuiHost.h"
#include "Controls/List/DuiTab.h"
#include "Controls/Window/DuiScrollBar.h"
#include "Controls/Layout/DuiLayout.h"
#include "DuiNotify.h"

// DuiGallery main frame. Hosts a single DuiHost child; the host root is
// a VBox of (DuiTab on top, DuiScrollView below). Switching tabs rebuilds
// the scroll view's content from Pages::Build_*().
class GalleryFrame : public CWindowImpl<GalleryFrame, CWindow>
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiGalleryFrame__"), CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    GalleryFrame();
    ~GalleryFrame();

    BEGIN_MSG_MAP(GalleryFrame)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MSG_WM_TIMER(OnTimer)
        MESSAGE_HANDLER_EX(WM_DUI_NOTIFY, OnDuiNotify)
    END_MSG_MAP()

protected:
    int     OnCreate(LPCREATESTRUCT lpcs);
    void    OnDestroy();
    void    OnSize(UINT nType, CSize size);
    BOOL    OnEraseBkgnd(CDCHandle dc);
    void    OnTimer(UINT_PTR nIDEvent);
    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM);

private:
    void    BuildRoot();
    void    SwitchToPage(int index);

private:
    balloonwjui::DuiHost           m_host;
    balloonwjui::DuiTab*           m_tab        = nullptr;
    balloonwjui::DuiScrollView*    m_scroll     = nullptr;
    int                            m_currentPage = -1;
};
