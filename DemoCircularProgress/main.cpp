// =============================================================================
// DemoCircularProgress.exe — guides.html 自绘控件章节示例 #2
//
// 演示：圆形进度环 + GDI+ 抗锯齿 + 多状态 + XML 工厂。
// =============================================================================

#include "stdafx.h"
#include "DemoCircularProgress.h"
#include "CaptureMode.h"

#include "DuiDpi.h"
#include "DuiHost.h"
#include "DuiXmlBuilder.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Window/DuiFrameWindow.h"

#include <atlcrack.h>

using namespace balloonwjui;

CAppModule _Module;

// -----------------------------------------------------------------------------
// XML 工厂：<circular-progress percent="60" thickness="8" fillColor="r,g,b"/>
//
// 与 DemoTextBadgeTile 完全同模式 — 只是属性集不同。读懂一个就懂全部。
// -----------------------------------------------------------------------------
static bool ParseRgb(const std::string& s, COLORREF& out)
{
    int r = -1, g = -1, b = -1;
    if (sscanf(s.c_str(), "%d,%d,%d", &r, &g, &b) != 3) return false;
    if (r < 0 || g < 0 || b < 0 || r > 255 || g > 255 || b > 255) return false;
    out = RGB(r, g, b);
    return true;
}

static std::unique_ptr<DuiControl>
CircularProgressFactory(const DuiXmlBuilder::Node& n)
{
    if (n.tag != "circular-progress") return nullptr;
    auto c = std::unique_ptr<DemoCircularProgress>(new DemoCircularProgress());

    auto get = [&](const char* k) -> const std::string* {
        auto it = n.attrs.find(k);
        return it == n.attrs.end() ? nullptr : &it->second;
    };

    if (auto* s = get("percent"))   c->SetPercent(atoi(s->c_str()));
    if (auto* s = get("thickness")) c->SetThickness(atoi(s->c_str()));
    COLORREF col;
    if (auto* s = get("fillColor"))  { if (ParseRgb(*s, col)) c->SetFillColor(col); }
    if (auto* s = get("trackColor")) { if (ParseRgb(*s, col)) c->SetTrackColor(col); }
    if (auto* s = get("textColor"))  { if (ParseRgb(*s, col)) c->SetTextColor(col); }
    if (auto* s = get("showText"))   c->SetShowText(*s == "true" || *s == "1");

    return c;
}

// -----------------------------------------------------------------------------
// 内容树
// -----------------------------------------------------------------------------
static std::unique_ptr<DuiControl> BuildContent()
{
    auto page = std::unique_ptr<DuiVBox>(new DuiVBox());
    page->SetPadding(20);
    page->SetGap(10);

    auto title = std::unique_ptr<DuiLabel>(new DuiLabel());
    title->SetText(_T("DemoCircularProgress 演示"));
    title->SetTextColor(RGB(20, 20, 20));
    page->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    auto sub = std::unique_ptr<DuiLabel>(new DuiLabel());
    sub->SetText(_T("GDI+ 抗锯齿圆环 + 中心百分比 — 演示 Gdiplus::Pen 与 SetSmoothingMode"));
    sub->SetTextColor(RGB(120, 120, 120));
    page->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    // ---- 代码方式：4 种进度 ----
    auto h1 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h1->SetText(_T("代码方式（不同进度）"));
    h1->SetTextColor(RGB(60, 60, 60));
    page->AddChild(std::move(h1), DuiLayout::Hint().Fixed(22));

    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(20);

        auto p0   = std::unique_ptr<DemoCircularProgress>(new DemoCircularProgress());
        p0->SetPercent(0);
        auto p33  = std::unique_ptr<DemoCircularProgress>(new DemoCircularProgress());
        p33->SetPercent(33);
        auto p66  = std::unique_ptr<DemoCircularProgress>(new DemoCircularProgress());
        p66->SetPercent(66);
        auto p100 = std::unique_ptr<DemoCircularProgress>(new DemoCircularProgress());
        p100->SetPercent(100);

        CaptureMode::RegisterMark(_T("p0"),   p0.get());
        CaptureMode::RegisterMark(_T("p33"),  p33.get());
        CaptureMode::RegisterMark(_T("p66"),  p66.get());
        CaptureMode::RegisterMark(_T("p100"), p100.get());

        row->AddChild(std::move(p0),   DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(p33),  DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(p66),  DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(p100), DuiLayout::Hint().Fixed(80));

        auto* rowAnchor = row.get();
        page->AddChild(std::move(row), DuiLayout::Hint().Fixed(80));
        CaptureMode::RegisterMark(_T("code-row"), rowAnchor);
    }

    page->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Fixed(16));

    // ---- XML 方式：自定义颜色 + 厚度 ----
    auto h2 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h2->SetText(_T("XML 方式（自定义配色 + 厚度）"));
    h2->SetTextColor(RGB(60, 60, 60));
    page->AddChild(std::move(h2), DuiLayout::Hint().Fixed(22));

    {
        const char* kXml =
            "<hbox gap=\"20\">"
            "  <circular-progress percent=\"75\" fillColor=\"50,160,110\"  fixedWidth=\"80\"/>"
            "  <circular-progress percent=\"50\" fillColor=\"220,170,60\"  thickness=\"10\" fixedWidth=\"80\"/>"
            "  <circular-progress percent=\"25\" fillColor=\"220,60,60\"   thickness=\"3\"  fixedWidth=\"80\"/>"
            "  <circular-progress percent=\"90\" fillColor=\"100,80,200\"  showText=\"false\" fixedWidth=\"80\"/>"
            "</hbox>";
        auto xmlRow = DuiXmlBuilder::FromString(kXml, &CircularProgressFactory);
        if (xmlRow)
        {
            auto* a = xmlRow.get();
            page->AddChild(std::move(xmlRow), DuiLayout::Hint().Fixed(80));
            CaptureMode::RegisterMark(_T("xml-row"), a);
        }
    }

    return page;
}

// -----------------------------------------------------------------------------
// CLI parse — 与示例 #1 相同
// -----------------------------------------------------------------------------
static bool ParseCaptureMode(LPCTSTR cmdLine, CString& outDir)
{
    CString s = cmdLine ? cmdLine : _T("");
    s.Trim();
    if (s.Find(_T("--capture-all")) != 0) return false;
    CString tail = s.Mid(static_cast<int>(_tcslen(_T("--capture-all"))));
    tail.Trim();
    if (tail.GetLength() >= 2 && tail[0] == _T('"')
        && tail[tail.GetLength() - 1] == _T('"'))
    {
        tail = tail.Mid(1, tail.GetLength() - 2);
    }
    if (tail.IsEmpty()) return false;
    outDir = tail;
    return true;
}

class MainFrame : public DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(MainFrame)
        CHAIN_MSG_MAP(DuiFrameWindow)
    END_MSG_MAP()
};

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR lpCmdLine, int nCmdShow)
{
    DuiDpi::OptInPerMonitorV2();
    ::OleInitialize(NULL);
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);

    CString captureDir;
    if (ParseCaptureMode(lpCmdLine, captureDir))
    {
        int saved = CaptureMode::RunCaptureAll(
            captureDir, _T("demo-circprog"), &BuildContent);
        _Module.Term();
        ::OleUninitialize();
        return saved < 0 ? 1 : 0;
    }

    int ret = 0;
    {
        MainFrame frame;
        frame.SetTitle(_T("DemoCircularProgress"));
        frame.SetButtons(true, true, true);
        if (!frame.Create(NULL, CWindow::rcDefault, _T("DemoCircularProgress"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        // 加载 app.ico (IDI_APP=100，橙色 "P")
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
        frame.SetClientContent(BuildContent());
        frame.ResizeClient(640, 320);
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
