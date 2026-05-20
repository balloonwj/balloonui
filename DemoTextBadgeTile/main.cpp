// =============================================================================
// DemoTextBadgeTile.exe — guides.html 自绘控件章节示例 #1
//
// 本 demo 演示：
//   1) 写一个自绘 DuiControl 子类（DemoTextBadgeTile，见 .h/.cpp）
//   2) 在代码中实例化 + 配置属性
//   3) 通过 DuiXmlBuilder + CustomFactory 从 XML 字符串实例化
//   4) 同时把代码版本与 XML 版本并排显示，验证两条路径行为一致
//   5) 提供 --capture-all <dir> 模式，把每个 tile 截成独立 PNG 供文档使用
//
// 运行：
//   DemoTextBadgeTile.exe                    交互模式（看效果）
//   DemoTextBadgeTile.exe --capture-all DIR  截图模式
// =============================================================================

#include "stdafx.h"

#include "DemoTextBadgeTile.h"
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
// 自定义 XML 工厂：把 <text-badge ...> 标签映射到 DemoTextBadgeTile
//
// XML 例：
//   <text-badge text="保存" bgColor="45,108,223" fgColor="255,255,255" radius="6"/>
//
// 属性约定：
//   text       —— 文字内容
//   bgColor    —— "r,g,b" 三元组
//   fgColor    —— "r,g,b" 三元组
//   radius     —— int 圆角半径
//
// 工厂返回 nullptr 表示不识别（让 builder 走默认 fall-through）。
// -----------------------------------------------------------------------------
static bool ParseRgb(const std::string& s, COLORREF& out)
{
    int r = -1, g = -1, b = -1;
    if (sscanf(s.c_str(), "%d,%d,%d", &r, &g, &b) != 3)
    {
        return false;
    }
    if (r < 0 || g < 0 || b < 0 || r > 255 || g > 255 || b > 255)
    {
        return false;
    }
    out = RGB(r, g, b);
    return true;
}

static std::unique_ptr<DuiControl>
TextBadgeFactory(const DuiXmlBuilder::Node& n)
{
    if (n.tag != "text-badge")
    {
        return nullptr;   // 让 builder 看其它工厂或忽略
    }
    auto t = std::unique_ptr<DemoTextBadgeTile>(new DemoTextBadgeTile());

    auto get = [&](const char* key) -> const std::string* {
        auto it = n.attrs.find(key);
        return it == n.attrs.end() ? nullptr : &it->second;
    };

    if (auto* s = get("text"))
    {
        // attrs 是 UTF-8 std::string。CString 自带从 UTF-8 转 wide 的构造，
        // 但需要显式 codepage — 这里走 ATL 的 CA2W 简单转一次。
        CA2W w(s->c_str(), CP_UTF8);
        t->SetText(CString(w));
    }
    if (auto* s = get("bgColor"))
    {
        COLORREF c;
        if (ParseRgb(*s, c)) t->SetBgColor(c);
    }
    if (auto* s = get("fgColor"))
    {
        COLORREF c;
        if (ParseRgb(*s, c)) t->SetFgColor(c);
    }
    if (auto* s = get("radius"))
    {
        t->SetCornerRadius(atoi(s->c_str()));
    }

    // 通用属性 id/fixedWidth/fixedHeight 由 builder 在 AddChild 时统一处理 —
    // 这里不需要解析。
    return t;
}

// -----------------------------------------------------------------------------
// 用代码方式创建一个 tile
// -----------------------------------------------------------------------------
static std::unique_ptr<DemoTextBadgeTile>
MakeTileByCode(LPCTSTR text, COLORREF bg, COLORREF fg = RGB(255, 255, 255))
{
    auto t = std::unique_ptr<DemoTextBadgeTile>(new DemoTextBadgeTile());
    t->SetText(text);
    t->SetBgColor(bg);
    t->SetFgColor(fg);
    return t;
}

// -----------------------------------------------------------------------------
// 用 XML 方式创建一个 tile（演示 XML 路径）
// -----------------------------------------------------------------------------
static std::unique_ptr<DuiControl>
MakeTileByXml(LPCSTR xmlUtf8)
{
    return DuiXmlBuilder::FromString(xmlUtf8, &TextBadgeFactory);
}

// -----------------------------------------------------------------------------
// 构建 demo 内容树。会被两个路径调用：
//   - 交互模式：作为 frame.SetClientContent 的根
//   - 截图模式：作为 host.SetRoot 的根
//
// 内容布局：
//
//   ┌──────────────────────────────────────────────────────┐
//   │  (header) DemoTextBadgeTile 演示                     │
//   │  (sub) 圆角彩色矩形 + 居中文字 — 最简单的自绘控件     │
//   │                                                      │
//   │  代码方式：                                          │
//   │    [默认] [成功] [警告] [危险]                       │
//   │                                                      │
//   │  XML 方式：                                          │
//   │    [from XML]  [自定义半径=2] [自定义半径=14]        │
//   └──────────────────────────────────────────────────────┘
//
// 每个 tile 都注册一个 capture mark — 文档需要每个状态独立 PNG。
// -----------------------------------------------------------------------------
static std::unique_ptr<DuiControl> BuildContent()
{
    auto page = std::unique_ptr<DuiVBox>(new DuiVBox());
    page->SetPadding(20);
    page->SetGap(10);

    // ---- 标题区 ----
    auto title = std::unique_ptr<DuiLabel>(new DuiLabel());
    title->SetText(_T("DemoTextBadgeTile 演示"));
    title->SetTextColor(RGB(20, 20, 20));
    page->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    auto sub = std::unique_ptr<DuiLabel>(new DuiLabel());
    sub->SetText(_T("圆角彩色矩形 + 居中文字 — 最简单的自绘 DuiControl 子类"));
    sub->SetTextColor(RGB(120, 120, 120));
    page->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    // ---- 代码方式：4 个不同色板 ----
    auto h1 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h1->SetText(_T("代码方式"));
    h1->SetTextColor(RGB(60, 60, 60));
    page->AddChild(std::move(h1), DuiLayout::Hint().Fixed(22));

    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(12);

        auto def  = MakeTileByCode(_T("默认"), RGB( 45, 108, 223));
        auto succ = MakeTileByCode(_T("成功"), RGB( 50, 160, 110));
        auto warn = MakeTileByCode(_T("警告"), RGB(220, 170,  60), RGB(60, 40, 0));
        auto dang = MakeTileByCode(_T("危险"), RGB(220,  60,  60));

        // mark 注册：每个 tile 独立成 PNG
        CaptureMode::RegisterMark(_T("default"), def.get());
        CaptureMode::RegisterMark(_T("success"), succ.get());
        CaptureMode::RegisterMark(_T("warning"), warn.get());
        CaptureMode::RegisterMark(_T("danger"),  dang.get());

        row->AddChild(std::move(def),  DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(succ), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(warn), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(dang), DuiLayout::Hint().Fixed(110));

        // 整行也 mark 一下，用于"4 状态拼图"风格的截图（可选）
        auto* rowAnchor = row.get();
        page->AddChild(std::move(row), DuiLayout::Hint().Fixed(36));
        CaptureMode::RegisterMark(_T("code-row"), rowAnchor);
    }

    page->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Fixed(16));

    // ---- XML 方式 ----
    auto h2 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h2->SetText(_T("XML 方式（DuiXmlBuilder + CustomFactory）"));
    h2->SetTextColor(RGB(60, 60, 60));
    page->AddChild(std::move(h2), DuiLayout::Hint().Fixed(22));

    {
        // 一个 hbox 里放 3 个 text-badge：演示 XML 解析 + 工厂派发。
        // 通用属性 fixedWidth 是 builder 内置（控制布局占位），text/bgColor
        // /radius 走 TextBadgeFactory。
        const char* kXml =
            "<hbox gap=\"12\">"
            "  <text-badge text=\"from XML\" bgColor=\"100,80,200\" radius=\"6\" fixedWidth=\"110\"/>"
            "  <text-badge text=\"radius=2\" bgColor=\"40,40,40\"   radius=\"2\" fixedWidth=\"110\"/>"
            "  <text-badge text=\"radius=14\" bgColor=\"40,150,150\" radius=\"14\" fixedWidth=\"110\"/>"
            "</hbox>";

        auto xmlRow = MakeTileByXml(kXml);
        if (xmlRow)
        {
            // 取出 hbox 的子节点们打 mark — hbox 本身有 3 个子。这里我们
            // 直接对整行 mark 一次就够（per-tile mark 也可以做，省事起见
            // 整行截一张供文档使用）。
            auto* xmlAnchor = xmlRow.get();
            page->AddChild(std::move(xmlRow), DuiLayout::Hint().Fixed(36));
            CaptureMode::RegisterMark(_T("xml-row"), xmlAnchor);
        }
    }

    return page;
}

// -----------------------------------------------------------------------------
// CLI parser — 找 "--capture-all <dir>" 模式
// -----------------------------------------------------------------------------
static bool ParseCaptureMode(LPCTSTR cmdLine, CString& outDir)
{
    CString s = cmdLine ? cmdLine : _T("");
    s.Trim();
    if (s.Find(_T("--capture-all")) != 0)
    {
        return false;
    }
    CString tail = s.Mid(static_cast<int>(_tcslen(_T("--capture-all"))));
    tail.Trim();
    if (tail.GetLength() >= 2 &&
        tail[0] == _T('"') && tail[tail.GetLength() - 1] == _T('"'))
    {
        tail = tail.Mid(1, tail.GetLength() - 2);
    }
    if (tail.IsEmpty())
    {
        return false;
    }
    outDir = tail;
    return true;
}

// -----------------------------------------------------------------------------
// 交互模式 frame — 普通 DuiFrameWindow，加客户区
// -----------------------------------------------------------------------------
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

    // ---- 截图模式 ----
    CString captureDir;
    {
        CString dbg;
        dbg.Format(_T("[DemoTextBadgeTile] cmdLine=[%s]\n"),
                   lpCmdLine ? lpCmdLine : _T("(null)"));
        ::OutputDebugString(dbg);
    }
    if (ParseCaptureMode(lpCmdLine, captureDir))
    {
        ::OutputDebugString(_T("[DemoTextBadgeTile] entering capture mode\n"));
        int saved = CaptureMode::RunCaptureAll(
            captureDir,
            _T("demo-textbadge"),
            &BuildContent);
        CString msg;
        msg.Format(_T("[DemoTextBadgeTile] wrote %d PNG(s) to %s\n"),
                   saved, (LPCTSTR)captureDir);
        ::OutputDebugString(msg);
        // CaptureMode internally appends to %TEMP%\DemoTextBadgeTile.log,
        // so just append our summary too rather than rewriting the file.
        TCHAR tmp[MAX_PATH] = {};
        if (::GetTempPath(MAX_PATH, tmp))
        {
            CString p = tmp; p += _T("DemoTextBadgeTile.log");
            HANDLE h = ::CreateFile(p, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h != INVALID_HANDLE_VALUE)
            {
                CStringA a(msg);
                DWORD w = 0;
                ::WriteFile(h, (LPCSTR)a, a.GetLength(), &w, NULL);
                ::CloseHandle(h);
            }
        }
        _Module.Term();
        ::OleUninitialize();
        return saved < 0 ? 1 : 0;
    }

    // ---- 交互模式 ----
    int ret = 0;
    {
        MainFrame frame;
        frame.SetTitle(_T("DemoTextBadgeTile"));
        frame.SetButtons(true, true, true);
        if (!frame.Create(NULL, CWindow::rcDefault, _T("DemoTextBadgeTile"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        // 加载 app.ico (IDI_APP=100，粉色 "B")
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
