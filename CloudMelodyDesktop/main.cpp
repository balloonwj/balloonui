// =============================================================================
// CloudMelodyDesktop.exe —— 「方Music」音乐 App 界面 demo（balloonui 复刻
// third_party/cloud_melody_desktop/ 8 张 stitch 截图）。
//
// 启动流程：
//   单 frame：CloudMelodyMainFrame（1100×720，Top + Sidebar + Content + Player）
//   消息循环跑到 OnDestroy_ PostQuitMessage 退出。
//
// M0 阶段：MainFrame 只装 4 块占位 ColorPanel —— 顶栏 / 侧栏 / 内容区 /
// 播放栏。验证 Demos.sln 集成 + DuiFrameWindow 起得来 + 4 区比例正确。
// 后续 M1+ 才往各区填真控件。
// =============================================================================

#include "stdafx.h"
#include "MainFrame.h"
#include "App/CloudMelodyTheme.h"

#include "DuiDpi.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#pragma comment(linker, \
    "/manifestdependency:\"type='win32' " \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

using namespace balloonwjui;

CAppModule _Module;

static int RunMsgLoop()
{
    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR /*cmdLine*/, int /*nCmdShow*/)
{
    DuiDpi::OptInPerMonitorV2();
    ::OleInitialize(NULL);
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);

    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gpToken = 0;
    Gdiplus::GdiplusStartup(&gpToken, &gsi, nullptr);

    int ret = 0;
    {
        cloudmelody::CloudMelodyMainFrame frame;
        if (!frame.Create(NULL, CWindow::rcDefault,
                          _T(""),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            Gdiplus::GdiplusShutdown(gpToken);
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        // Brand 文字搬到标题栏 —— "FangMusic · Your Sound, Your Way"
        // 一行（DuiFrameTitleBar 单行渲染）。
        frame.SetTitle(_T("FangMusic · Your Sound, Your Way"));
        // 加载 app.ico（IDI_APP=100，红色 "F"）。OS 任务栏 / Alt-Tab + DUI 标题栏一并刷
        if (HICON hI = (HICON)::LoadImage(_Module.GetModuleInstance(),
                MAKEINTRESOURCE(100), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR))
        {
            ::SendMessage(frame.m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hI);
            ::SendMessage(frame.m_hWnd, WM_SETICON, ICON_BIG,   (LPARAM)hI);
            ICONINFO ii = {};
            if (::GetIconInfo(hI, &ii))
            {
                if (ii.hbmMask) { ::DeleteObject(ii.hbmMask); }
                frame.SetIcon(ii.hbmColor);
            }
        }

        frame.LoadMainXml(_T("main.xml"));
        frame.ResizeClient(1100, 720);
        frame.CenterWindow();
        frame.ShowWindow(SW_SHOWNORMAL);
        frame.UpdateWindow();
        ret = RunMsgLoop();
    }

    // app.ico 由 LoadImage 加载 —— OS 在进程结束时回收，不需要显式释放

    Gdiplus::GdiplusShutdown(gpToken);
    _Module.Term();
    ::OleUninitialize();
    return ret;
}
