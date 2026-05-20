// =============================================================================
// DemoNinePatchBg.exe — 9-grid 整窗背景演示
//
// 两种运行模式：
//
// 1. 交互模式（直接双击 demo.exe）：
//    弹一个可缩放 DuiFrameWindow，背景用 BuddyInfoDlgBg.png（580×520） +
//    DuiHost::SetBgImage 上 9-grid。客户区随便拖角放大缩小，四角的圆角
//    和上方粉蓝渐变条都不会被拉变形。
//
// 2. 截图模式（DemoNinePatchBg.exe --capture-all <dir>）：
//    生成文档需要的 PNG：
//      bg-9grid-small.png   小尺寸（280×220）9-grid 渲染
//      bg-9grid-medium.png  中尺寸（440×340）9-grid
//      bg-9grid-large.png   大尺寸（680×500）9-grid
//      bg-naive-medium.png  同 440×340 但用 StretchBlt 整图拉伸（反例）
//      bg-9grid-realdialog.png  类 BuddyInfoDlg 形态：bg + 头像 + 几个
//                              label + 按钮
// =============================================================================

#include "stdafx.h"
#include "BgPaintTile.h"
#include "CaptureMode.h"

#include "DuiDpi.h"
#include "DuiHost.h"
#include "DuiXmlBuilder.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Input/DuiEditHost.h"
#include "Controls/Window/DuiFrameWindow.h"

#include <atlcrack.h>
#include <atlimage.h>     // CImage 加载 PNG
#include <gdiplus.h>      // 备选 — 把 GDI+ Bitmap 转 32bpp DIB section
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

CAppModule _Module;

// 加载 app.ico (IDI_APP=100，灰色 "9") 应用到 OS + DUI 标题栏。本 demo 起
// 4 个 frame（codeDefault / codeThemed / xmlDefault / xmlThemed），共用同
// 一份 icon —— 文件作用域 static helper。
static void ApplyAppIcon_(DuiFrameWindow& f)
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
// 9-grid 内距（针对 BuddyInfoDlgBg.png 587×535 的具体设计）
//
// 源图里渐变带的真实高度是 69px (用 PowerShell 扫像素得到 — 见 guides.html
// 第 10 章)。如果用经典 9-grid 把 srcInsets.top = dstInsets.top = 69，标题
// 栏就要做 69px 高，太高；把它都改成 40 又会让源里 40-69 那 30 行渐变残尾
// 被推到中间区，下面会出一道脏边。
//
// 解决：源 inset 用 69（罩住整条装饰带），目标 inset 用 40（你想要的标题栏
// 高度）。9-grid 把这块装饰带等比压缩到 40px — 渐变完整呈现，下方业务区
// 干净。
// -----------------------------------------------------------------------------
const int kBgInsetLeft       = 10;
const int kBgInsetRight      = 10;
const int kBgInsetBottom     = 10;
const int kBgSrcGradientH    = 69;   // 源图渐变带的真实像素高度
const int kBgDstTitleBarH    = 40;   // 目标里标题栏的高度

// -----------------------------------------------------------------------------
// 全进程共用一份 BuddyInfoDlgBg.png 句柄。Demo 的 caller-owned 模式：
// 我们这里加载一次，挂到全局，让 host / tile 共享。进程退出时操作系统
// 自动回收，不显式 DeleteObject。
// -----------------------------------------------------------------------------
static HBITMAP g_hBgBitmap = nullptr;

// 加载 PNG 为一张"完全不透明"的 HBITMAP。
// Gdiplus::Bitmap::GetHBITMAP(背景色, &hbm) 把图片预合成到指定背景色
// 上，输出一张 alpha 已经 baked 进 RGB 的 DDB —— 后续 BitBlt /
// StretchBlt / DuiNinePatch::Draw 都视它为不透明。这是处理 PNG +
// 各种 GDI 渲染路径最稳的兼容办法（避开了"alpha=0 → 黑屏"和"DIB
// section 字节序差异"两个坑）。
static HBITMAP LoadOpaqueBgPng(LPCTSTR path)
{
    Gdiplus::Bitmap src(path);
    if (src.GetLastStatus() != Gdiplus::Ok)
    {
        return nullptr;
    }
    HBITMAP hbm = nullptr;
    if (src.GetHBITMAP(Gdiplus::Color(255, 255, 255, 255), &hbm) != Gdiplus::Ok)
    {
        return nullptr;
    }
    return hbm;
}

// 从 exe 同目录读 XML 文件，返回 UTF-8 string（去掉 BOM）。失败 → 空。
// XML 自身在编码声明里写不写 UTF-8 都不重要 —— FromFrameXml 内部把
// 整个 buffer 当 UTF-8 处理，能正确解析中文属性值。
//
// 用 ResolveAssetPath 把 fileName 解释成"相对 exe 目录"的绝对路径，
// 与 XML 内 bg-image 属性走同一套路径策略，行为一致。
static std::string LoadXmlFromExeDir(LPCTSTR fileName)
{
    CString abs = balloonwjui::DuiXmlBuilder::ResolveAssetPath(fileName);
    if (abs.IsEmpty())
    {
        return std::string();
    }
    HANDLE h = ::CreateFile(abs, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        return std::string();
    }
    LARGE_INTEGER sz = {};
    if (!::GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (1 << 20))
    {
        // 限 1MB —— XML 配置正常远小于此，超出大概率指错文件
        ::CloseHandle(h);
        return std::string();
    }
    std::string data((size_t)sz.QuadPart, 0);
    DWORD got = 0;
    BOOL ok = ::ReadFile(h, &data[0], (DWORD)sz.QuadPart, &got, NULL);
    ::CloseHandle(h);
    if (!ok || got != (DWORD)sz.QuadPart)
    {
        return std::string();
    }
    // 去 UTF-8 BOM —— Notepad / VS 默认存 UTF-8 with BOM，XML 解析器
    // 会把 BOM 字节当 element 前的非空白字符吐 parse error。
    if (data.size() >= 3
        && (BYTE)data[0] == 0xEF
        && (BYTE)data[1] == 0xBB
        && (BYTE)data[2] == 0xBF)
    {
        data.erase(0, 3);
    }
    return data;
}

static HBITMAP LoadBgFromExeDir()
{
    // 先尝试 exe 同目录 / 项目目录
    TCHAR exePath[MAX_PATH] = {};
    ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    CString dir = exePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0) dir = dir.Left(slash);

    // 候选路径
    CString candidates[] = {
        dir + _T("\\BuddyInfoDlgBg.png"),
        dir + _T("\\Skins\\Skin0\\BuddyInfoDlgBg.png"),
        dir + _T("\\..\\DemoNinePatchBg\\BuddyInfoDlgBg.png"),
        dir + _T("\\..\\Bin\\Skins\\Skin0\\BuddyInfoDlgBg.png"),
    };

    // GDI+ 必须先 startup
    static ULONG_PTR s_token = 0;
    if (!s_token)
    {
        Gdiplus::GdiplusStartupInput gsi;
        Gdiplus::GdiplusStartup(&s_token, &gsi, nullptr);
    }

    for (auto& path : candidates)
    {
        if (::GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
        {
            continue;
        }
        if (HBITMAP hbm = LoadOpaqueBgPng(path))
        {
            return hbm;
        }
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
// 交互模式：构造一个"类 BuddyInfoDlg"客户区
//   - 头部 80px 留白（落在 9-grid 顶部 inset，背景画粉蓝渐变条）
//   - 中部 一行 头像 + 名字 + 帐号
//   - 中部 几行 label + edit
//   - 底部 一行 居中 OK 按钮
// -----------------------------------------------------------------------------
static std::unique_ptr<DuiControl> BuildBuddyInfoContent()
{
    auto root = std::unique_ptr<DuiVBox>(new DuiVBox());
    // 标题栏（DuiFrameWindow 透明标题条）已经占走顶部 80px，
    // 内容区只需要少量上 padding 避开下边的描边阴影。
    root->SetPadding(24 /*L*/, 16 /*T*/, 24 /*R*/, 16 /*B*/);
    root->SetGap(10);

    // 第一行：头像 + 名字 + 帐号
    auto headerRow = std::unique_ptr<DuiHBox>(new DuiHBox());
    headerRow->SetGap(12);
    auto av = std::unique_ptr<DuiAvatar>(new DuiAvatar());
    av->SetName(_T("苏文嘉"));
    av->SetFallbackBgColor(RGB(80, 130, 220));
    headerRow->AddChild(std::move(av), DuiLayout::Hint().Fixed(56));

    auto nameCol = std::unique_ptr<DuiVBox>(new DuiVBox());
    nameCol->SetGap(2);
    auto name = std::unique_ptr<DuiLabel>(new DuiLabel());
    name->SetText(_T("苏文嘉"));
    name->SetTextColor(RGB(20, 20, 20));
    nameCol->AddChild(std::move(name), DuiLayout::Hint().Fixed(28));
    auto handle = std::unique_ptr<DuiLabel>(new DuiLabel());
    handle->SetText(_T("@suwenjia · 设计中心"));
    handle->SetTextColor(RGB(110, 110, 110));
    nameCol->AddChild(std::move(handle), DuiLayout::Hint().Fixed(20));
    headerRow->AddChild(std::move(nameCol), DuiLayout::Hint().Weight(1));

    root->AddChild(std::move(headerRow), DuiLayout::Hint().Fixed(56));

    // 几行 label + edit
    auto makeRow = [](LPCTSTR label, LPCTSTR text) {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(8);
        auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
        l->SetText(label);
        l->SetTextColor(RGB( 80,  80,  80));
        l->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e->SetText(text);
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(72));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        return row;
    };

    root->AddChild(makeRow(_T("帐号"), _T("suwenjia")),
                   DuiLayout::Hint().Fixed(28));
    root->AddChild(makeRow(_T("邮箱"), _T("suwenjia@example.com")),
                   DuiLayout::Hint().Fixed(28));
    root->AddChild(makeRow(_T("电话"), _T("+86 138 0000 1234")),
                   DuiLayout::Hint().Fixed(28));
    root->AddChild(makeRow(_T("签名"), _T("做能让人慢下来的设计")),
                   DuiLayout::Hint().Fixed(28));

    // 弹性占位
    root->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Weight(1));

    // OK 按钮居中
    auto btnRow = std::unique_ptr<DuiHBox>(new DuiHBox());
    btnRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                     DuiLayout::Hint().Weight(1));
    auto bOk = std::unique_ptr<DuiButton>(new DuiButton());
    bOk->SetText(_T("关闭"));
    btnRow->AddChild(std::move(bOk), DuiLayout::Hint().Fixed(96));
    btnRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                     DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(btnRow), DuiLayout::Hint().Fixed(36));

    return root;
}

// -----------------------------------------------------------------------------
// 截图模式：5 个 BgPaintTile 实例，演示多种尺寸 + 反例 + 真实形态
// -----------------------------------------------------------------------------
static std::unique_ptr<BgPaintTile>
MakeTile(BgPaintTile::Mode mode)
{
    auto t = std::unique_ptr<BgPaintTile>(new BgPaintTile());
    t->SetBitmap(g_hBgBitmap);
    t->SetInsets(kBgInsetLeft, kBgSrcGradientH, kBgInsetRight, kBgInsetBottom,
                 kBgInsetLeft, kBgDstTitleBarH, kBgInsetRight, kBgInsetBottom);
    t->SetMode(mode);
    return t;
}

static std::unique_ptr<DuiControl> BuildCaptureContent()
{
    auto page = std::unique_ptr<DuiVBox>(new DuiVBox());
    page->SetPadding(20);
    page->SetGap(16);

    auto title = std::unique_ptr<DuiLabel>(new DuiLabel());
    title->SetText(_T("DuiHost::SetBgImage 9-grid 演示"));
    title->SetTextColor(RGB(20, 20, 20));
    page->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    // ---- 三尺寸：9-grid 渲染 ----
    auto h1 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h1->SetText(_T("9-grid · 不同尺寸 — 角与渐变条都不失真"));
    h1->SetTextColor(RGB(60, 60, 60));
    page->AddChild(std::move(h1), DuiLayout::Hint().Fixed(22));

    // 注：Win32 头文件把 small / large 定义成 char / int — 不能当变量名
    {
        auto sz1 = MakeTile(BgPaintTile::ModeNinePatch);
        CaptureMode::RegisterMark(_T("9grid-small"), sz1.get());
        page->AddChild(std::move(sz1), DuiLayout::Hint().Fixed(220));
    }
    {
        auto sz2 = MakeTile(BgPaintTile::ModeNinePatch);
        CaptureMode::RegisterMark(_T("9grid-medium"), sz2.get());
        page->AddChild(std::move(sz2), DuiLayout::Hint().Fixed(280));
    }
    {
        auto sz3 = MakeTile(BgPaintTile::ModeNinePatch);
        CaptureMode::RegisterMark(_T("9grid-large"), sz3.get());
        page->AddChild(std::move(sz3), DuiLayout::Hint().Fixed(360));
    }

    // ---- 反例：同尺寸 naive StretchBlt ----
    auto h2 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h2->SetText(_T("反例 · naive StretchBlt — 渐变条被压扁，角变形"));
    h2->SetTextColor(RGB(180, 60, 60));
    page->AddChild(std::move(h2), DuiLayout::Hint().Fixed(22));

    {
        auto naive = MakeTile(BgPaintTile::ModeNaiveStretch);
        CaptureMode::RegisterMark(_T("naive-medium"), naive.get());
        page->AddChild(std::move(naive), DuiLayout::Hint().Fixed(280));
    }

    return page;
}

// -----------------------------------------------------------------------------
// CLI parser
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

// Demo 同时开两个 frame（默认样式 vs 9-grid bg 样式），任一关闭都让计数减一；
// 计数归零才 PostQuitMessage —— 这样用户可以单独关掉其中一个对照看，剩下那
// 个还能继续操作。
//
// 全局 + 简单计数；demo 范畴内不需要更复杂的 frame 注册表。
static int g_openFrameCount = 0;

class MainFrame : public DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(MainFrame)
        MSG_WM_DESTROY(OnDestroy)
        CHAIN_MSG_MAP(DuiFrameWindow)
    END_MSG_MAP()

    // WM_DESTROY — 库里 DuiFrameWindow 不主动 PostQuitMessage（一个进程可能
    // 开多个 frame，库不该假设关哪个 = 整体退出），由主窗口负责。本 demo
    // 同时存在 2 个 frame，所以走计数门控：只有最后一个关闭时才退出循环。
    void OnDestroy()
    {
        if (--g_openFrameCount <= 0)
        {
            ::PostQuitMessage(0);
        }
    }
};

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR lpCmdLine, int nCmdShow)
{
    DuiDpi::OptInPerMonitorV2();
    ::OleInitialize(NULL);
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);

    g_hBgBitmap = LoadBgFromExeDir();   // 全局缓存

    CString captureDir;
    if (ParseCaptureMode(lpCmdLine, captureDir))
    {
        int saved = CaptureMode::RunCaptureAll(
            captureDir, _T("bg"), &BuildCaptureContent);
        _Module.Term();
        ::OleUninitialize();
        return saved < 0 ? 1 : 0;
    }

    int ret = 0;
    {
        // ────────────────────────────────────────────────────────────────
        // 4 窗对比：行 = 配置模式（代码 vs XML），列 = 样式（默认 vs 9-grid bg）
        //
        //   [代码 / 默认]    [代码 / 9-grid bg]
        //   [XML  / 默认]    [XML  / 9-grid bg]
        //
        // 同列 = "XML 配置和等价 C++ 调用产出的窗口是否完全一致"。
        // 4 个窗口跑相同的客户区内容 (BuildBuddyInfoContent)，所以视觉差异
        // 全部来自 frame 配置本身。
        // ────────────────────────────────────────────────────────────────

        // ── 行 1 / 列 1：代码 / 默认 ────────────────────────────────────
        //   不调任何标题/边/bg 相关 setter，纯走 DuiFrameWindow 默认：
        //   · 36px 标题栏（浅灰底 + 1px 底分隔）
        //   · COLOR_BTNFACE 客户区背景
        //   · RGB(200,200,200) 1px 客户区外框
        //   · 三按钮：浅灰 hover、Windows 红 close
        MainFrame frame_codeDefault;
        frame_codeDefault.SetButtons(true, true, true);
        frame_codeDefault.SetMinSize(320, 240);
        frame_codeDefault.SetResizable(true);
        if (!frame_codeDefault.Create(NULL, CWindow::rcDefault,
                                      _T("DemoFrame_CodeDefault"),
                                      WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        // SetTitle 必须在 Create 之后调，否则 ::SetWindowText 那行会被
        // m_hWnd 空检查跳过，OS 标题（任务栏 / Alt-Tab 显示）仍是窗口
        // 类名而非业务标题。XML 路径走 ApplyConfig 是 post-Create 的，
        // 自动正确。
        frame_codeDefault.SetTitle(_T("默认样式 / 代码"));
        ApplyAppIcon_(frame_codeDefault);
        frame_codeDefault.SetClientContent(BuildBuddyInfoContent());

        // ── 行 1 / 列 2：代码 / 9-grid bg ───────────────────────────────
        //   ★ 关键调用：让 9-grid 顶部的 40px 渐变条充当标题栏
        //   1) 标题栏高度 = 目标 dst inset top（40，跟 bg-dst-insets 对齐）
        //   2) 标题栏透明 → host 的 9-grid 背景顶部那条直接穿透显示
        //   3) 标题文字 + 三按钮 glyph 改白色（彩色渐变上对比度）
        MainFrame frame_codeThemed;
        frame_codeThemed.SetTitleBarHeight    (kBgDstTitleBarH);
        frame_codeThemed.SetTitleBarTransparent(true);
        frame_codeThemed.SetTitleTextColor    (RGB(255, 255, 255));
        frame_codeThemed.SetCaptionGlyphColor (RGB(255, 255, 255));
        frame_codeThemed.SetButtons (true, true, true);
        frame_codeThemed.SetMinSize (320, 240);
        frame_codeThemed.SetResizable(true);
        if (!frame_codeThemed.Create(NULL, CWindow::rcDefault,
                                     _T("DemoFrame_CodeThemed"),
                                     WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        // SetTitle post-Create —— 同 frame_codeDefault 的注释
        frame_codeThemed.SetTitle(_T("9-grid bg / 代码"));
        ApplyAppIcon_(frame_codeThemed);
        if (g_hBgBitmap)
        {
            RECT srcInsets = { kBgInsetLeft, kBgSrcGradientH,
                               kBgInsetRight, kBgInsetBottom };
            RECT dstInsets = { kBgInsetLeft, kBgDstTitleBarH,
                               kBgInsetRight, kBgInsetBottom };
            frame_codeThemed.SetBgImage(g_hBgBitmap, srcInsets, dstInsets);
        }
        frame_codeThemed.SetClientContent(BuildBuddyInfoContent());

        // ── 行 2 / 列 1：XML / 默认 ─────────────────────────────────────
        //   从 Bin/frame-default.xml 加载，FromFrameXml 解析成 cfg，
        //   ApplyConfig 一行落地到 frame。属性集合等价于上面 frame_codeDefault
        //   的 setter 序列 —— 同列对比应当像素级一致。
        std::string xmlDefault = LoadXmlFromExeDir(_T("frame-default.xml"));
        DuiFrameWindowConfig cfgDefault;
        DuiXmlBuilder::FromFrameXml(xmlDefault.c_str(), cfgDefault);

        MainFrame frame_xmlDefault;
        if (!frame_xmlDefault.Create(NULL, CWindow::rcDefault,
                                     _T("DemoFrame_XmlDefault"),
                                     WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        frame_xmlDefault.ApplyConfig(cfgDefault);                // ★ 一行落地
        ApplyAppIcon_(frame_xmlDefault);
        frame_xmlDefault.SetClientContent(BuildBuddyInfoContent());

        // ── 行 2 / 列 2：XML / 9-grid bg ───────────────────────────────
        //   从 Bin/frame-themed.xml 加载。XML 里 bg-image 写相对路径
        //   "BuddyInfoDlgBg.png"，FromFrameXml 调 ResolveAssetPath 转成
        //   绝对（exe 目录 = Bin），ApplyConfig 调 LoadBgImageFromFile
        //   把 bitmap 加载并交给 frame 持有，析构自动释放。
        //
        //   注意：跟列 2 行 1 (frame_codeThemed) 不同，这个 frame 自己
        //   独立加载一份 bitmap（不共享 g_hBgBitmap），因为 XML 路径
        //   走 host-owned 模式更纯粹 —— 演示"XML 跑起来不依赖 caller 提前
        //   加载任何资源"。
        std::string xmlThemed = LoadXmlFromExeDir(_T("frame-themed.xml"));
        DuiFrameWindowConfig cfgThemed;
        DuiXmlBuilder::FromFrameXml(xmlThemed.c_str(), cfgThemed);

        MainFrame frame_xmlThemed;
        if (!frame_xmlThemed.Create(NULL, CWindow::rcDefault,
                                    _T("DemoFrame_XmlThemed"),
                                    WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        frame_xmlThemed.ApplyConfig(cfgThemed);                  // ★ 一行落地
        ApplyAppIcon_(frame_xmlThemed);
        frame_xmlThemed.SetClientContent(BuildBuddyInfoContent());

        // ── 4 窗 2×2 网格 ──────────────────────────────────────────────
        // 单窗 460×420（Per-Monitor V2 + WS_THICKFRAME 下 ResizeClient
        // 给的是 client 尺寸，实际 window rect 含 frame 装饰会比它略大；
        // 这里直接 SetWindowPos 设 window-rect 尺寸 + 位置，一次到位）。
        // 行间隔 60 ——足够 1366×768 也不上下重叠。
        const int kWinW = 460;
        const int kWinH = 420;
        const int kGap  = 60;
        RECT rcWork = {};
        ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
        int x0 = (rcWork.left + rcWork.right  - (2 * kWinW + kGap)) / 2;
        int y0 = (rcWork.top  + rcWork.bottom - (2 * kWinH + kGap)) / 2;
        int xR = x0 + kWinW + kGap;
        int yB = y0 + kWinH + kGap;

        // SetWindowPos 同时设位置 + 尺寸，覆盖 Create 默认大小。SWP_NOZORDER
        // 不改 z-order；不需要 SWP_NOSIZE（要传 size 进去）。
        frame_codeDefault.SetWindowPos(NULL, x0, y0, kWinW, kWinH, SWP_NOZORDER);
        frame_codeThemed .SetWindowPos(NULL, xR, y0, kWinW, kWinH, SWP_NOZORDER);
        frame_xmlDefault .SetWindowPos(NULL, x0, yB, kWinW, kWinH, SWP_NOZORDER);
        frame_xmlThemed  .SetWindowPos(NULL, xR, yB, kWinW, kWinH, SWP_NOZORDER);

        g_openFrameCount = 4;
        frame_codeDefault.ShowWindow(nCmdShow);  frame_codeDefault.UpdateWindow();
        frame_codeThemed .ShowWindow(nCmdShow);  frame_codeThemed .UpdateWindow();
        frame_xmlDefault .ShowWindow(nCmdShow);  frame_xmlDefault .UpdateWindow();
        frame_xmlThemed  .ShowWindow(nCmdShow);  frame_xmlThemed  .UpdateWindow();

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
