// DuiGallery.exe entry point. Pure DUI demo - no chat / network / login
// code. Runs entirely on the kernel + controls under flamingoclient/balloonui/.
//
// CLI:
//   DuiGallery.exe                       - normal interactive mode
//   DuiGallery.exe --capture-all <dir>   - headless: write one PNG per
//                                          AddVariantRowCapture mark to
//                                          <dir>\ctl-<name>.png and exit.

#include "stdafx.h"
#include "GalleryFrame.h"
#include "CaptureMode.h"
#include "../balloonui/DuiDpi.h"

using namespace balloonwjui;

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpCmdLine, int nCmdShow)
{
    // Opt into per-monitor v2 DPI awareness BEFORE creating any HWND so
    // the title bar metrics + WM_DPICHANGED routing all kick in. No-op
    // on Windows < 10 1703.
    DuiDpi::OptInPerMonitorV2();

    HRESULT hRes = ::OleInitialize(NULL);
    (void)hRes;
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInstance);

    // CLI parse: look for "--capture-all <dir>". The flag must appear
    // first; we don't parse a full grammar here.
    CString cmd = lpCmdLine ? lpCmdLine : _T("");
    cmd.Trim();
    if (cmd.Find(_T("--capture-all")) == 0)
    {
        CString tail = cmd.Mid(static_cast<int>(_tcslen(_T("--capture-all"))));
        tail.Trim();
        // Strip optional surrounding quotes around the path.
        if (tail.GetLength() >= 2 && tail[0] == _T('"')
            && tail[tail.GetLength() - 1] == _T('"'))
        {
            tail = tail.Mid(1, tail.GetLength() - 2);
        }
        if (tail.IsEmpty())
        {
            ::OutputDebugString(_T("DuiGallery: --capture-all needs an output directory\n"));
            _Module.Term();
            ::OleUninitialize();
            return 2;
        }
        int saved = CaptureMode::RunCaptureAll(tail);
        CString msg;
        msg.Format(_T("DuiGallery: --capture-all wrote %d PNG(s) to %s\n"),
                   saved, (LPCTSTR)tail);
        ::OutputDebugString(msg);
        _Module.Term();
        ::OleUninitialize();
        return saved < 0 ? 1 : 0;
    }

    int ret = 0;
    {
        GalleryFrame frame;
        if (!frame.Create(NULL, CWindow::rcDefault, _T("DuiGallery"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            ::OleUninitialize();
            return 0;
        }
        // 加载 app.ico（IDI_APP=100，紫色 "G"）。GalleryFrame 是 CWindowImpl
        // （非 DuiFrameWindow），仅设 OS 层 icon。
        if (HICON hI = (HICON)::LoadImage(_Module.GetModuleInstance(),
                MAKEINTRESOURCE(100), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR))
        {
            ::SendMessage(frame.m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hI);
            ::SendMessage(frame.m_hWnd, WM_SETICON, ICON_BIG,   (LPARAM)hI);
        }
        frame.ResizeClient(900, 700);
        frame.CenterWindow();
        frame.ShowWindow(nCmdShow);
        frame.UpdateWindow();

        MSG msg;
        while (::GetMessage(&msg, NULL, 0, 0))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        ret = (int)msg.wParam;
    }

    _Module.Term();
    ::OleUninitialize();
    return ret;
}
