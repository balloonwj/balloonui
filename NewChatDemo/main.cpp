// NewChatDemo entry point. Brings up a 1080x720 borderless DuiFrameWindow
// (custom-painted title bar + min/max/close from the library) hosting
// the WeChat-style single-chat dialog laid out from chat.xml.

#include "stdafx.h"
#include "ChatXmlFactory.h"

#include "../balloonui/DuiDpi.h"
#include "../balloonui/DuiXmlBuilder.h"
#include "../balloonui/Controls/Window/DuiFrameWindow.h"

CAppModule _Module;

namespace {

// Default fallback XML — used when chat.xml on disk is missing. Kept in
// sync with NewChatDemo/chat.xml; if you tweak the disk file regenerate
// this constant. (No <titlebar/> any more — DuiFrameWindow paints its
// own caption + min/max/close.)
const char* kDefaultXmlUtf8 =
    "<hbox weight=\"1\">"
    "  <vbox fixedWidth=\"60\" padding=\"16,16,16,16\" gap=\"6\">"
    "    <rail-item kind=\"profile\" hue=\"165\" initial=\"\xe9\x99\x86\" fixedHeight=\"36\"/>"
    "    <rail-item kind=\"chat\" glyph=\"C\" badge=\"12\" active=\"true\" fixedHeight=\"40\"/>"
    "    <rail-spacer weight=\"1\"/>"
    "  </vbox>"
    "  <vbox fixedWidth=\"260\">"
    "    <search-box fixedHeight=\"48\" placeholder=\"\xe6\x90\x9c\xe7\xb4\xa2\"/>"
    "  </vbox>"
    "  <vbox weight=\"1\">"
    "    <header-bar fixedHeight=\"52\" title=\"\xe8\x8b\x8f\xe6\x96\x87\xe5\x98\x89\" sub=\"\xe5\x9c\xa8\xe7\xba\xbf\"/>"
    "    <chat-thread weight=\"1\"/>"
    "    <composer fixedHeight=\"120\"/>"
    "  </vbox>"
    "  <info-panel fixedWidth=\"240\" name=\"\xe8\x8b\x8f\xe6\x96\x87\xe5\x98\x89\" hue=\"165\" initial=\"\xe8\x8b\x8f\"/>"
    "</hbox>";

std::string ReadFileUtf8(LPCTSTR path)
{
    HANDLE h = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        return std::string();
    }
    DWORD sz = ::GetFileSize(h, nullptr);
    std::string out;
    if (sz > 0 && sz < (1 << 20))
    {
        out.resize(sz);
        DWORD got = 0;
        ::ReadFile(h, &out[0], sz, &got, nullptr);
        out.resize(got);
        if (out.size() >= 3
            && (unsigned char)out[0] == 0xEF
            && (unsigned char)out[1] == 0xBB
            && (unsigned char)out[2] == 0xBF)
        {
            out.erase(0, 3);
        }
    }
    ::CloseHandle(h);
    return out;
}

CString ResolveXmlPath()
{
    TCHAR exePath[MAX_PATH] = {};
    ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    CString dir = exePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0)
    {
        dir = dir.Left(slash);
    }
    CString a = dir + _T("\\chat.xml");
    if (::GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES)
    {
        return a;
    }
    CString b = dir + _T("\\..\\NewChatDemo\\chat.xml");
    if (::GetFileAttributes(b) != INVALID_FILE_ATTRIBUTES)
    {
        return b;
    }
    return CString();
}

} // anonymous

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow)
{
    balloonwjui::DuiDpi::OptInPerMonitorV2();

    HRESULT hRes = ::OleInitialize(NULL);
    (void)hRes;
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInstance);

    int ret = 0;
    {
        balloonwjui::DuiFrameWindow frame;

        // Configure title strip BEFORE Create so the DuiFrameTitleBar
        // child is built with the right values when SetClientContent
        // first runs.
        frame.SetTitle(_T("NewChatDemo \x2014 FlamingoNewUI"));
        frame.SetTitleBarHeight(28);
        frame.SetButtons(true, true, true);
        frame.SetMinSize(720, 480);

        if (!frame.Create(NULL, CWindow::rcDefault, _T("NewChatDemo"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            ::OleUninitialize();
            return 0;
        }
        // 加载 app.ico (IDI_APP=100，蓝色 "N")
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

        // Build the chat layout from XML and install as client content.
        auto fac = newchatdemo::MakeChatFactory();
        std::string xml = ReadFileUtf8(ResolveXmlPath());
        if (xml.empty())
        {
            xml = kDefaultXmlUtf8;
        }
        auto root = balloonwjui::DuiXmlBuilder::FromString(xml.c_str(), fac);
        if (root)
        {
            frame.SetClientContent(std::move(root));
        }

        frame.ResizeClient(1080, 720);
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
