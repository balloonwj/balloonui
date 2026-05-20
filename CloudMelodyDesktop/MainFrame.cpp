#include "stdafx.h"
#include "MainFrame.h"
#include "App/CloudMelodyTheme.h"
#include "App/Sidebar.h"
#include "App/TopBar.h"
#include "App/PlayerBar.h"
#include "App/ContentRouter.h"
#include "Pages/NowPlayingPage.h"
#include "Controls/RotatingDisc.h"
#include "App/Mock/MockMusic.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// 这些常量在 SDK 里只有 Win11 22H2+ 的 win10sdk 头才定义；老 SDK 会缺。
// 自定义补齐一份，运行时若 OS 不支持 DwmSetWindowAttribute 会直接返
// E_INVALIDARG，对结果无影响。
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#  define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_MAINWINDOW
#  define DWMSBT_MAINWINDOW 2          // Mica（推荐用于 main window）
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#  define DWMSBT_TRANSIENTWINDOW 3     // Acrylic（更模糊，更"玻璃"，但更耗）
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#  define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#  define DWMWCP_ROUND 2
#endif

#include "Controls/Layout/DuiLayout.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

namespace cloudmelody {

// ─── 资源路径解析（XChat 同款，搜 exe 旁 → ../CloudMelodyDesktop → 上两级） ──

namespace {

CString ResolveXmlPath(LPCTSTR fileName)
{
    TCHAR exePath[MAX_PATH] = {};
    ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    CString dir = exePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0)
    {
        dir = dir.Left(slash);
    }
    CString a = dir + _T("\\") + fileName;
    if (::GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES) { return a; }
    CString b = dir + _T("\\..\\CloudMelodyDesktop\\") + fileName;
    if (::GetFileAttributes(b) != INVALID_FILE_ATTRIBUTES) { return b; }
    CString c = dir + _T("\\..\\..\\CloudMelodyDesktop\\") + fileName;
    if (::GetFileAttributes(c) != INVALID_FILE_ATTRIBUTES) { return c; }
    return CString();
}

std::string ReadFileUtf8(LPCTSTR path)
{
    HANDLE h = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { return std::string(); }
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

COLORREF ParseRgbAttr(const std::string& s, COLORREF def)
{
    int r = 0, g = 0, b = 0;
    if (sscanf_s(s.c_str(), "%d,%d,%d", &r, &g, &b) == 3)
    {
        return RGB(r, g, b);
    }
    return def;
}

const std::string& AttrOr(const DuiXmlBuilder::Node& n,
                          const char* key, const std::string& def)
{
    auto it = n.attrs.find(key);
    return (it != n.attrs.end()) ? it->second : def;
}

// =========================================================================
// ColorPanel —— 给 vbox 套上背景色（balloonui 的 layout 不画背景）。
// XML：<color-panel color="r,g,b" ...>...</color-panel>
// =========================================================================
class ColorPanel : public DuiVBox
{
public:
    void SetBg(COLORREF c) { m_bg = c; Invalidate(); }

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        HBRUSH hbr = ::CreateSolidBrush(m_bg);
        ::FillRect(hdc, &m_rcItem, hbr);
        ::DeleteObject(hbr);
        DuiControl::OnPaint(hdc, rcDirty);
    }

private:
    COLORREF m_bg = RGB(255, 255, 255);
};

// =========================================================================
// PlayerBarPanel —— 同 ColorPanel，再加一条顶部 1px 分隔线（玻璃感近似）。
// =========================================================================
class PlayerBarPanel : public ColorPanel
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        ColorPanel::OnPaint(hdc, rcDirty);
        if (!m_bVisible) { return; }
        RECT rcLine = m_rcItem;
        rcLine.bottom = rcLine.top + 1;
        HBRUSH hbr = ::CreateSolidBrush(kColorPlayerBarTop);
        ::FillRect(hdc, &rcLine, hbr);
        ::DeleteObject(hbr);
    }
};

} // anonymous

// =========================================================================
// MakeMainXmlFactory —— XML 自定义 tag 工厂。当前只接 <color-panel> 和
// <player-bar-panel>。每加一个自定义 tag 在这里加一条 if。
// =========================================================================
DuiXmlBuilder::CustomFactory MakeMainXmlFactory()
{
    return [](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
    {
        if (n.tag == "color-panel")
        {
            auto panel = std::make_unique<ColorPanel>();
            panel->SetBg(ParseRgbAttr(AttrOr(n, "color", std::string("255,255,255")),
                                      RGB(255, 255, 255)));
            return panel;
        }
        if (n.tag == "player-bar-panel")
        {
            auto panel = std::make_unique<PlayerBarPanel>();
            panel->SetBg(ParseRgbAttr(AttrOr(n, "color", std::string("255,255,255")),
                                      RGB(255, 255, 255)));
            return panel;
        }
        if (n.tag == "sidebar")
        {
            // BuildSidebar 自带 220 宽 + 浅灰底 + 主导航子树。caller 在
            // XML 里只需 <sidebar/> 一个空 tag。后续要覆盖初始 active 项
            // 时加 nav="discover|podcast|..." attr 解析即可（暂未做）。
            return BuildSidebar(NavId_Discover);
        }
        if (n.tag == "top-bar")
        {
            return BuildTopBar();
        }
        if (n.tag == "player-bar")
        {
            return BuildPlayerBar();
        }
        if (n.tag == "content-router")
        {
            return std::make_unique<ContentRouter>();
        }
        return nullptr;
    };
}

// ─── XChatMainFrame 实现 ───────────────────────────────────────────────

void CloudMelodyMainFrame::LoadMainXml(LPCTSTR xmlName)
{
    LPCTSTR name = xmlName ? xmlName : _T("main.xml");
    std::string xml = ReadFileUtf8(ResolveXmlPath(name));
    if (xml.empty())
    {
        // M0 fallback —— xml 找不到时用内嵌字符串保证 demo 至少能起来。
        // M1 起 xml 文件齐全后这块可以删。
        xml = std::string()
            + "<frame-window min-w=\"900\" min-h=\"640\" resizable=\"true\""
            + " has-min-button=\"true\" has-max-button=\"true\""
            + " has-close-button=\"true\">"
            + "  <vbox padding=\"0\" gap=\"0\">"
            + "    <top-bar fixedHeight=\"48\"/>"
            + "    <hbox weight=\"1\" padding=\"0\" gap=\"0\">"
            + "      <sidebar id=\"20\" fixedWidth=\"220\"/>"
            + "      <content-router id=\"30\" weight=\"1\"/>"
            + "    </hbox>"
            + "    <player-bar fixedHeight=\"80\"/>"
            + "  </vbox>"
            + "</frame-window>";
    }
    DuiFrameWindowConfig cfg;
    auto root = DuiXmlBuilder::FromFrameXml(xml.c_str(), cfg, MakeMainXmlFactory());
    ApplyConfig(cfg);
    if (root)
    {
        SetClientContent(std::move(root));
    }

    // 装好客户区后，把 ContentRouter 切到初始页（发现）。Sidebar 默认
    // active 也是发现，两者一致。
    if (auto* clientRoot = GetClientContent())
    {
        if (auto* r = dynamic_cast<ContentRouter*>(
                clientRoot->FindCtrlById(30)))
        {
            r->Switch(NavId_Discover);
        }
    }
}

// 播放计时 timer：250ms = 4Hz。秒级显示足够；不取 1Hz 是因为进度条粗
// 加位移每秒一跳视觉上不够顺滑，250ms 折衷。
static const UINT_PTR kPlaybackTimerId = 0xC10D;
static const UINT     kPlaybackTimerMs = 250;

// 动画 timer：~30Hz。NowPlayingPage 的旋转黑胶用。60Hz 会更顺但 30Hz
// 已经看不出明显抖；省 CPU。timer 一直在跑，只有 NowPlay 可见时才会
// 找到 disc 控件并 Tick 它。
static const UINT_PTR kAnimTimerId = 0xC11A;
static const UINT     kAnimTimerMs = 33;

LRESULT CloudMelodyMainFrame::OnCreate_(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    SetTimer(kPlaybackTimerId, kPlaybackTimerMs);
    SetTimer(kAnimTimerId,     kAnimTimerMs);

    // Win11 Fluent 视觉打磨 —— DwmSetWindowAttribute 在不支持的 OS 上
    // 返 E_INVALIDARG，调用本身无副作用。我们要的两个属性：
    //   1) DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_MAINWINDOW (Mica)
    //      让窗体背后的桌面颜色渗到非客户区（标题栏 / 边框区）。本 demo
    //      的 DUI 客户区仍用 solid 色，所以视觉效果<u>主要在窗体外缘 +
    //      阴影感</u>。要让 Mica 透到 TopBar / PlayerBar，得让 DuiHost
    //      支持透通 paint —— 那是库级改造，本期不做。
    //   2) DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND
    //      Win11 默认就 round，老 Win10 / 早期 Win11 build 不一定。显式
    //      设成 round 确保一致。
    {
        int backdrop = DWMSBT_MAINWINDOW;
        ::DwmSetWindowAttribute(m_hWnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                &backdrop, sizeof(backdrop));
        int corner = DWMWCP_ROUND;
        ::DwmSetWindowAttribute(m_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                &corner, sizeof(corner));
    }

    bHandled = FALSE;
    return 0;
}

LRESULT CloudMelodyMainFrame::OnDestroy_(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    KillTimer(kPlaybackTimerId);
    KillTimer(kAnimTimerId);
    if (m_lyricsWnd.IsWindow())
    {
        m_lyricsWnd.DestroyWindow();
    }
    ::PostQuitMessage(0);
    bHandled = FALSE;
    return 0;
}

LRESULT CloudMelodyMainFrame::OnTimer_(UINT, WPARAM wp, LPARAM, BOOL& bHandled)
{
    auto* clientRoot = GetClientContent();
    if (!clientRoot)
    {
        bHandled = FALSE;
        return 0;
    }

    if (wp == kPlaybackTimerId)
    {
        if (auto* pb = dynamic_cast<PlayerBar*>(clientRoot->FindCtrlById(40)))
        {
            pb->Tick((int)kPlaybackTimerMs);
        }
        // 桌面歌词同步 —— 简化版只显示 mock 当前行（kMockLyrics 数组的
        // kMockCurrentLyricIdx 行）。真 timing 接入后在这里按 m_posMs 找
        // 对应行。
        if (m_lyricsWnd.IsShowingNow())
        {
            int idx = kMockCurrentLyricIdx;
            if (idx >= 0 && idx < kMockLyricsCount)
            {
                m_lyricsWnd.SetCurrentLyric(kMockLyrics[idx]);
            }
        }
    }
    else if (wp == kAnimTimerId)
    {
        // NowPlayingPage 可见时才有 disc，FindCtrlById null 时静默跳过
        if (auto* d = dynamic_cast<RotatingDisc*>(
                clientRoot->FindCtrlById(NowPlayId_Disc)))
        {
            d->Tick((int)kAnimTimerMs);
        }
    }
    bHandled = FALSE;
    return 0;
}

void CloudMelodyMainFrame::ToggleFullscreen_()
{
    auto* clientRoot = GetClientContent();
    if (!clientRoot) { return; }

    // TopBar id=10、Sidebar id=20 —— 进入全屏隐藏，退出还原。Sidebar
    // 的父是 hbox，hide 后 hbox 把空间让给 ContentRouter；TopBar 的父
    // 是顶层 vbox，hide 后 vbox 把空间让给中部行。
    DuiControl* topBar  = clientRoot->FindCtrlById(10);
    DuiControl* sidebar = clientRoot->FindCtrlById(20);

    if (!m_isFullscreen)
    {
        m_isFullscreen = true;
        if (topBar)  { topBar->SetVisible(false); }
        if (sidebar) { sidebar->SetVisible(false); }
        // 切到 NowPlayingPage 沉浸视图（带当前曲目）
        if (auto* router = dynamic_cast<ContentRouter*>(
                clientRoot->FindCtrlById(30)))
        {
            int trackIdx = 0;
            if (auto* pb = dynamic_cast<PlayerBar*>(
                    clientRoot->FindCtrlById(40)))
            {
                trackIdx = pb->GetTrackIdx();
            }
            router->Switch(NavId_NowPlaying, trackIdx);
        }
        ShowWindow(SW_MAXIMIZE);
    }
    else
    {
        m_isFullscreen = false;
        if (topBar)  { topBar->SetVisible(true); }
        if (sidebar) { sidebar->SetVisible(true); }
        if (auto* router = dynamic_cast<ContentRouter*>(
                clientRoot->FindCtrlById(30)))
        {
            router->Switch(NavId_Discover);
        }
        if (sidebar)
        {
            SetActiveNav(sidebar, NavId_Discover);
        }
        ShowWindow(SW_RESTORE);
    }
    // 子树可见性变化需要 force layout 让 hbox / vbox 重排
    clientRoot->ForceLayout(clientRoot->GetRect());
    clientRoot->Invalidate();
}

LRESULT CloudMelodyMainFrame::OnDuiNotify_(UINT, WPARAM wp, LPARAM lp, BOOL& bHandled)
{
    auto* n = (DuiNotify*)lp;
    UINT srcId = (UINT)wp;

    if (!n)
    {
        bHandled = FALSE;
        return 0;
    }
    auto* clientRoot = GetClientContent();
    if (!clientRoot)
    {
        bHandled = FALSE;
        return 0;
    }

    // ── Sidebar 主导航点击 → 切页 + 同步 active 态 ───────────────────
    if (n->code == DUIN_CLICK
        && srcId >= NavId_Discover && srcId <= NavId_Profile)
    {
        if (auto* r = dynamic_cast<ContentRouter*>(
                clientRoot->FindCtrlById(30)))
        {
            r->Switch((int)srcId);
        }
        if (auto* s = clientRoot->FindCtrlById(20))
        {
            SetActiveNav(s, (int)srcId);
        }
    }

    // ── PlayerBar transport / 进度条 ─────────────────────────────────
    auto* pb = dynamic_cast<PlayerBar*>(clientRoot->FindCtrlById(40));
    if (pb)
    {
        if (n->code == DUIN_CLICK)
        {
            switch (srcId)
            {
            case PlayerBarId_Play:    pb->OnPlayClicked();    break;
            case PlayerBarId_Prev:    pb->OnPrevClicked();    break;
            case PlayerBarId_Next:    pb->OnNextClicked();    break;
            case PlayerBarId_Shuffle: pb->OnShuffleClicked(); break;
            case PlayerBarId_Fullscreen:
                ToggleFullscreen_();
                break;
            case PlayerBarId_DeskLyrics:
                m_lyricsWnd.Toggle();
                break;
            default: break;
            }
        }
        else if (n->code == DUIN_VALUECHANGED
                 && srcId == PlayerBarId_Progress)
        {
            pb->OnSeek((int)n->extra);
        }
    }

    // ── PlayerBar 封面点击 → 进 NowPlayingPage ─────────────────────
    if (n->code == DUIN_CLICK && srcId == PlayerBarId_Cover)
    {
        if (auto* r = dynamic_cast<ContentRouter*>(
                clientRoot->FindCtrlById(30)))
        {
            int trackIdx = pb ? pb->GetTrackIdx() : 0;
            r->Switch(NavId_NowPlaying, trackIdx);
        }
    }

    // ── NowPlayingPage 「← 返回」 ───────────────────────────────────
    if (n->code == DUIN_CLICK && srcId == NowPlayId_Back)
    {
        // 全屏态：直接走 ToggleFullscreen_，它内部会还原 TopBar/Sidebar
        // 可见性 + 切回 Discover + SW_RESTORE。避免"卡在全屏 + Discover"
        // 的半破坏状态（TopBar/Sidebar 都还隐藏着）。
        if (m_isFullscreen)
        {
            ToggleFullscreen_();
        }
        else
        {
            if (auto* r = dynamic_cast<ContentRouter*>(
                    clientRoot->FindCtrlById(30)))
            {
                r->Switch(NavId_Discover);
            }
            if (auto* s = clientRoot->FindCtrlById(20))
            {
                SetActiveNav(s, NavId_Discover);
            }
        }
    }

    bHandled = FALSE;
    return 0;
}

} // namespace cloudmelody
