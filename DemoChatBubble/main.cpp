// =============================================================================
// DemoChatBubble.exe — guides.html 自绘控件章节示例 #3
//
// 演示：聊天气泡（左右对齐 + 异形尾巴 + MeasureHeight 测高 + XML 工厂）。
// =============================================================================

#include "stdafx.h"
#include "DemoChatBubble.h"
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
// XML 工厂：<chat-bubble side="in|out" text="..." ts="14:21"/>
// -----------------------------------------------------------------------------
static std::unique_ptr<DuiControl>
ChatBubbleFactory(const DuiXmlBuilder::Node& n)
{
    if (n.tag != "chat-bubble") return nullptr;
    auto b = std::unique_ptr<DemoChatBubble>(new DemoChatBubble());

    auto get = [&](const char* k) -> const std::string* {
        auto it = n.attrs.find(k);
        return it == n.attrs.end() ? nullptr : &it->second;
    };

    if (auto* s = get("side"))
    {
        b->SetSide(*s == "out" ? DemoChatBubble::SideOut : DemoChatBubble::SideIn);
    }
    if (auto* s = get("text"))
    {
        CA2W w(s->c_str(), CP_UTF8);
        b->SetText(CString(w));
    }
    if (auto* s = get("ts"))
    {
        CA2W w(s->c_str(), CP_UTF8);
        b->SetTimestamp(CString(w));
    }
    return b;
}

// -----------------------------------------------------------------------------
// 内容树：左右两侧各放几条不同长度的气泡
// -----------------------------------------------------------------------------
static std::unique_ptr<DuiControl> BuildContent()
{
    auto page = std::unique_ptr<DuiVBox>(new DuiVBox());
    page->SetPadding(20);
    page->SetGap(8);

    auto title = std::unique_ptr<DuiLabel>(new DuiLabel());
    title->SetText(_T("DemoChatBubble 演示"));
    title->SetTextColor(RGB(20, 20, 20));
    page->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    auto sub = std::unique_ptr<DuiLabel>(new DuiLabel());
    sub->SetText(_T("圆角矩形 + 三角尾巴 + WORDBREAK 自动换行 + 时间戳"));
    sub->SetTextColor(RGB(120, 120, 120));
    page->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    page->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Fixed(8));

    // ---- 4 条不同形态的气泡 ----
    struct Spec {
        DemoChatBubble::Side side;
        LPCTSTR              text;
        LPCTSTR              ts;
        LPCTSTR              markName;
    };
    Spec specs[] = {
        { DemoChatBubble::SideIn,  _T("好的，我下午把方案发给你，记得查收。"),
          _T("14:21"), _T("in-short") },
        { DemoChatBubble::SideOut, _T("收到。今天预计 5 点前我能整理完，方便的话晚点电话沟通一下细节？"),
          _T("14:23"), _T("out-long") },
        { DemoChatBubble::SideIn,  _T("可以，到时候直接打吧。"),
          _T(""),       _T("in-noTs") },
        { DemoChatBubble::SideOut, _T("好。"),
          _T("14:24"), _T("out-tiny") },
    };

    const int kBubbleW = 320;     // 气泡总宽（含尾巴）

    for (auto& s : specs)
    {
        auto bub = std::unique_ptr<DemoChatBubble>(new DemoChatBubble());
        bub->SetSide(s.side);
        bub->SetText(s.text);
        bub->SetTimestamp(s.ts);

        // 算高度 — 真实业务里这步在排聊天列表前完成。
        int h = bub->MeasureHeight(kBubbleW);

        // 套一个 hbox 控制左 / 右对齐：左侧 = bubble 在前 + 弹性占位在后；
        // 右侧 = 弹性占位在前 + bubble 在后。
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        if (s.side == DemoChatBubble::SideIn)
        {
            auto* a = bub.get();
            row->AddChild(std::move(bub), DuiLayout::Hint().Fixed(kBubbleW));
            row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                          DuiLayout::Hint().Weight(1));
            CaptureMode::RegisterMark(s.markName, a);
        }
        else
        {
            auto* a = bub.get();
            row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                          DuiLayout::Hint().Weight(1));
            row->AddChild(std::move(bub), DuiLayout::Hint().Fixed(kBubbleW));
            CaptureMode::RegisterMark(s.markName, a);
        }

        page->AddChild(std::move(row), DuiLayout::Hint().Fixed(h));
        // 气泡之间留点 gap
        page->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                       DuiLayout::Hint().Fixed(6));
    }

    page->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Fixed(12));

    // ---- XML 方式 ----
    auto h2 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h2->SetText(_T("XML 方式"));
    h2->SetTextColor(RGB(60, 60, 60));
    page->AddChild(std::move(h2), DuiLayout::Hint().Fixed(22));

    {
        // 简单一对：in + out
        const char* kXml =
            "<vbox gap=\"6\">"
            "  <chat-bubble side=\"in\"  text=\"from XML — incoming\" ts=\"15:00\" fixedHeight=\"50\"/>"
            "  <chat-bubble side=\"out\" text=\"from XML — outgoing\" ts=\"15:01\" fixedHeight=\"50\"/>"
            "</vbox>";
        auto v = DuiXmlBuilder::FromString(kXml, &ChatBubbleFactory);
        if (v)
        {
            auto* a = v.get();
            page->AddChild(std::move(v), DuiLayout::Hint().Fixed(108));
            CaptureMode::RegisterMark(_T("xml-pair"), a);
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
            captureDir, _T("demo-chatbubble"), &BuildContent);
        _Module.Term();
        ::OleUninitialize();
        return saved < 0 ? 1 : 0;
    }

    int ret = 0;
    {
        MainFrame frame;
        frame.SetTitle(_T("DemoChatBubble"));
        frame.SetButtons(true, true, true);
        if (!frame.Create(NULL, CWindow::rcDefault, _T("DemoChatBubble"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        // 加载 app.ico (IDI_APP=100，浅绿色 "C")
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
        frame.ResizeClient(720, 520);
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
