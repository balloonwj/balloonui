// =============================================================================
// DemoFileTypeIcon.exe — guides.html 自绘控件章节示例 #4
//
// 演示：文件类型彩色徽章 + 数据驱动配色 + hover 视觉反馈 + XML 工厂。
// =============================================================================

#include "stdafx.h"
#include "DemoFileTypeIcon.h"
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
// XML 工厂：<file-icon ext="pdf" label="PDF"/>
// -----------------------------------------------------------------------------
static std::unique_ptr<DuiControl>
FileTypeIconFactory(const DuiXmlBuilder::Node& n)
{
    if (n.tag != "file-icon") return nullptr;
    auto c = std::unique_ptr<DemoFileTypeIcon>(new DemoFileTypeIcon());

    auto get = [&](const char* k) -> const std::string* {
        auto it = n.attrs.find(k);
        return it == n.attrs.end() ? nullptr : &it->second;
    };
    if (auto* s = get("ext"))
    {
        CA2W w(s->c_str(), CP_UTF8);
        c->SetExt(CString(w));
    }
    if (auto* s = get("label"))
    {
        CA2W w(s->c_str(), CP_UTF8);
        c->SetLabel(CString(w));
    }
    if (auto* s = get("radius"))
    {
        c->SetCornerRadius(atoi(s->c_str()));
    }
    return c;
}

// -----------------------------------------------------------------------------
// 内容树：8 种常见后缀的色板示例
// -----------------------------------------------------------------------------
static std::unique_ptr<DuiControl> BuildContent()
{
    auto page = std::unique_ptr<DuiVBox>(new DuiVBox());
    page->SetPadding(20);
    page->SetGap(10);

    auto title = std::unique_ptr<DuiLabel>(new DuiLabel());
    title->SetText(_T("DemoFileTypeIcon 演示"));
    title->SetTextColor(RGB(20, 20, 20));
    page->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    auto sub = std::unique_ptr<DuiLabel>(new DuiLabel());
    sub->SetText(_T("数据驱动配色 + 折角阴影 + hover 提亮"));
    sub->SetTextColor(RGB(120, 120, 120));
    page->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    // ---- 8 种代码方式 ----
    auto h1 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h1->SetText(_T("代码方式"));
    h1->SetTextColor(RGB(60, 60, 60));
    page->AddChild(std::move(h1), DuiLayout::Hint().Fixed(22));

    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(10);
        struct Spec { LPCTSTR ext; LPCTSTR mark; };
        Spec specs[] = {
            { _T("pdf"),  _T("pdf")  },
            { _T("doc"),  _T("doc")  },
            { _T("xls"),  _T("xls")  },
            { _T("ppt"),  _T("ppt")  },
            { _T("mp4"),  _T("mp4")  },
            { _T("png"),  _T("png")  },
            { _T("zip"),  _T("zip")  },
            { _T("xyz"),  _T("unknown") },   // 未知后缀 → 灰色 default
        };
        for (auto& s : specs)
        {
            auto c = std::unique_ptr<DemoFileTypeIcon>(new DemoFileTypeIcon());
            c->SetExt(s.ext);
            CaptureMode::RegisterMark(s.mark, c.get());
            row->AddChild(std::move(c), DuiLayout::Hint().Fixed(48));
        }
        auto* a = row.get();
        page->AddChild(std::move(row), DuiLayout::Hint().Fixed(48));
        CaptureMode::RegisterMark(_T("code-row"), a);
    }

    page->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Fixed(16));

    // ---- XML 方式 ----
    auto h2 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h2->SetText(_T("XML 方式 + 自定义 label"));
    h2->SetTextColor(RGB(60, 60, 60));
    page->AddChild(std::move(h2), DuiLayout::Hint().Fixed(22));

    {
        const char* kXml =
            "<hbox gap=\"10\">"
            "  <file-icon ext=\"fig\"    fixedWidth=\"48\"/>"
            "  <file-icon ext=\"psd\"    fixedWidth=\"48\"/>"
            "  <file-icon ext=\"sketch\" label=\"SK\" fixedWidth=\"48\"/>"
            "  <file-icon ext=\"mp3\"    radius=\"14\" fixedWidth=\"48\"/>"
            "</hbox>";
        auto v = DuiXmlBuilder::FromString(kXml, &FileTypeIconFactory);
        if (v)
        {
            auto* a = v.get();
            page->AddChild(std::move(v), DuiLayout::Hint().Fixed(48));
            CaptureMode::RegisterMark(_T("xml-row"), a);
        }
    }

    return page;
}

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
            captureDir, _T("demo-fileicon"), &BuildContent);
        _Module.Term();
        ::OleUninitialize();
        return saved < 0 ? 1 : 0;
    }

    int ret = 0;
    {
        MainFrame frame;
        frame.SetTitle(_T("DemoFileTypeIcon"));
        frame.SetButtons(true, true, true);
        if (!frame.Create(NULL, CWindow::rcDefault, _T("DemoFileTypeIcon"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        // 加载 app.ico (IDI_APP=100，黄色 "I")
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
        frame.ResizeClient(640, 280);
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
