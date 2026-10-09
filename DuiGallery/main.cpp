// DuiGallery.exe entry point. Pure DUI demo - no chat / network / login
// code. Runs entirely on the kernel + controls under flamingoclient/balloonui/.
//
// CLI (parsed by GalleryCmdLine.h; options in any order, values case-insensitive):
//   DuiGallery.exe                       - normal interactive mode
//   DuiGallery.exe --lang en|zh          - UI language (default zh); applies to
//                                          both modes
//   DuiGallery.exe --capture-all <dir>   - headless: write one PNG per
//                                          AddVariantRowCapture mark to
//                                          <dir>\ctl-<name>.png and exit.
//                                          Quote <dir> if it contains spaces.
//   The documentation screenshots are English:
//     DuiGallery.exe --lang en --capture-all docs\images

#include "stdafx.h"
#include "GalleryFrame.h"
#include "CaptureMode.h"
#include "GalleryCmdLine.h"
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

    // 命令行：[--lang en|zh] [--capture-all <目录>]，语法见 GalleryCmdLine.h。
    // 无效时：截图模式直接退出（返回 2）；正常启动忽略无效的部分照常启动。
    Gallery::GalleryCmdLine cmdLine = Gallery::ParseGalleryCmdLine(lpCmdLine);
    if (!cmdLine.m_valid)
    {
        CString err;
        err.Format(_T("DuiGallery: %s\n"), (LPCTSTR)cmdLine.m_error);
        ::OutputDebugString(err);
    }
    // 界面语言要在建任何页面之前设好：页面里的文字在构建时按当前语言取
    if (cmdLine.m_langGiven)
    {
        Gallery::SetCurrentLanguage(cmdLine.m_lang);
    }
    if (cmdLine.m_captureAll)
    {
        if (!cmdLine.m_valid)
        {
            _Module.Term();
            ::OleUninitialize();
            return 2;
        }
        int saved = CaptureMode::RunCaptureAll(cmdLine.m_captureDir);
        CString msg;
        msg.Format(_T("DuiGallery: --capture-all wrote %d PNG(s) to %s\n"),
                   saved, (LPCTSTR)cmdLine.m_captureDir);
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
