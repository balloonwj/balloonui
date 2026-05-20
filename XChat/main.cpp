// =============================================================================
// XChat.exe —— 某信 PC 版界面 demo（用 balloonui 复刻 third_party/wechat/
// 下 10 张截图，纯界面、所有数据 mock）。
//
// 启动流程：
//   Phase 1：LoginFrame（360×500、QR view 3s → SwitchMode → loading view 3s → close）
//            单一 LoginFrame 实例 + SetClientContent 切换 mode；消息循环 1
//            跑到 LoginFrame OnDestroy_ PostQuitMessage 退出。
//   Phase 2+：XChatMainFrame（1100×720、主面板 / 会话 / 聊天）
//            消息循环 2：直到主窗关闭整个进程退出。
//
// 这种"两个 GetMessage 循环串行"的写法比"一个循环 + 多 frame 共存"简单：
// 每个 frame 自己 OnDestroy_ PostQuit 即可，互不干扰。缺点是 LoginFrame
// 关到 MainFrame 起之间会有约 1 帧的视觉空档；做精致转场要换"双 frame
// 共存 + 显式信号关 login + 起 main"的方案，本 demo 阶段不必要。
// =============================================================================

#include "stdafx.h"
#include "LoginFrame.h"
#include "MainFrame.h"
#include "SettingsFrame.h"

#include "DuiDpi.h"
#include "Controls/Window/DuiFrameWindow.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// 嵌入 ComCtl32 v6 manifest 依赖 —— 否则 EDIT 控件用的是 v5 风格，
// EM_SETCUEBANNER 等 Vista+ 消息直接 no-op（DuiEditHost 的 placeholder
// 走 EM_SETCUEBANNER，没这条 manifest cue 字永远显不出来）。这条
// pragma 会让 linker 在 PE manifest 里写 dependentAssembly ComCtl32 v6.0.0.0。
#pragma comment(linker, \
    "/manifestdependency:\"type='win32' " \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

using namespace balloonwjui;

CAppModule _Module;

// 加载 app.ico (IDI_APP=100，绿色 "X") 应用到 OS 任务栏 + DUI 标题栏。
// 本 demo 起 3 个 frame（LoginFrame / SettingsFrame / XChatMainFrame）
// 共用同一份 icon —— 文件作用域 static helper，与 demo 同活同消。
static void ApplyAppIcon_(balloonwjui::DuiFrameWindow& f)
{
    HICON hI = (HICON)::LoadImage(_Module.GetModuleInstance(),
        MAKEINTRESOURCE(100), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (!hI) { return; }
    ::SendMessage(f.m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hI);
    ::SendMessage(f.m_hWnd, WM_SETICON, ICON_BIG,   (LPARAM)hI);
    ICONINFO ii = {};
    if (::GetIconInfo(hI, &ii))
    {
        if (ii.hbmMask) { ::DeleteObject(ii.hbmMask); }
        f.SetIcon(ii.hbmColor);
    }
}

// -----------------------------------------------------------------------------
// 跑一个 GetMessage 循环直到 PostQuitMessage。封一下复用：登录窗、主窗
// 各跑一遍。
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// 入口
// -----------------------------------------------------------------------------
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR cmdLine, int nCmdShow)
{
    DuiDpi::OptInPerMonitorV2();
    ::OleInitialize(NULL);
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);

    // GDI+ 全局初始化（LoginFrame 加载 PNG / Spinner 画 anti-aliased 弧线
    // 都要它）。balloonui 内部的 DuiAA 会自己懒初始化，但 demo 自己直接
    // 调 Gdiplus::Bitmap 时无法依赖那个，需要在 main 里先 Startup。
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gpToken = 0;
    Gdiplus::GdiplusStartup(&gpToken, &gsi, nullptr);

    // --settings 早路径：跳过 login + main，直接弹设置 frame（Phase 8 截图用）
    if (cmdLine && _tcsstr(cmdLine, _T("--settings")) != nullptr)
    {
        xchat::SettingsFrame settings;
        if (settings.Create(NULL, CWindow::rcDefault, _T(""),
                            WS_OVERLAPPEDWINDOW, 0))
        {
            ApplyAppIcon_(settings);
            settings.LoadSettingsXml();
            settings.ResizeClient(800, 680);
            settings.CenterWindow();
            settings.ShowWindow(nCmdShow);
            settings.UpdateWindow();
            settings.RunModal(nullptr);
        }
        Gdiplus::GdiplusShutdown(gpToken);
        _Module.Term();
        ::OleUninitialize();
        return 0;
    }

    // ── Phase 1: 登录窗（单 frame，QR → SwitchMode → Loading） ──
    {
        xchat::LoginFrame login(xchat::LoginFrame::Qr);
        if (!login.Create(NULL, CWindow::rcDefault,
                          _T("XChat"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            Gdiplus::GdiplusShutdown(gpToken);
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        ApplyAppIcon_(login);
        login.ShowAndStartLoginSequence();
        login.ResizeClient(360, 500);
        login.CenterWindow();
        login.ShowWindow(nCmdShow);
        login.UpdateWindow();
        (void)RunMsgLoop();
    }

    // ── Phase 2+: 主窗 ───────────────────────────────────────────────
    // CLI arg 选择初始页面：
    //   --contacts  → 联系人页（Phase 6）
    //   --publics   → 公众号页（Phase 7）
    //   默认无 arg  → main.xml = chat 视图（点击切 单聊/群聊/公众号）
    LPCTSTR initialXml = _T("main.xml");
    if (cmdLine && _tcsstr(cmdLine, _T("--contacts")) != nullptr)
    {
        initialXml = _T("main_contacts.xml");
    }
    else if (cmdLine && _tcsstr(cmdLine, _T("--publics")) != nullptr)
    {
        initialXml = _T("main_publics.xml");
    }

    int ret = 0;
    {
        xchat::XChatMainFrame frame;
        if (!frame.Create(NULL, CWindow::rcDefault,
                          _T("XChat"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            Gdiplus::GdiplusShutdown(gpToken);
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        ApplyAppIcon_(frame);
        // 选定 xml 自带 frame-window 配置（标题 / min/max/close / resizable
        // / min-w/min-h），LoadMainXml 内部 ApplyConfig + SetClientContent。
        frame.LoadMainXml(initialXml);
        // --empty：跳过默认 session-0 选中，直接进空 chat 水印 view。
        if (cmdLine && _tcsstr(cmdLine, _T("--empty")) != nullptr)
        {
            frame.ShowEmptyView();
        }
        frame.ResizeClient(1100, 720);
        frame.CenterWindow();
        frame.ShowWindow(SW_SHOWNORMAL);
        frame.UpdateWindow();
        ret = RunMsgLoop();
    }

    Gdiplus::GdiplusShutdown(gpToken);
    _Module.Term();
    ::OleUninitialize();
    return ret;
}
