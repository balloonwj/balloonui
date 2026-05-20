// =============================================================================
// DemoTaskManager.exe —— Win10 任务管理器界面 demo（用 balloonui 复刻）
//
// Phase 1：主框架由 taskmgr.xml 驱动 —— frame-window + menu bar (hbox) +
//   tab-page (7 个 tab) + status bar (hbox)。<tab-page> / <tab-page-item> /
//   <status-label> 三个自定义 tag 由 demotaskmgr::MakeTaskMgrFactory() 接住。
//
// Phase 2：进程 tab 接上 ProcessesPage（DuiTreeView 7 列），其余 tab 仍是
//   占位。WM_TIMER 200ms：mock 数据 Tick → 状态栏 + 进程页数值列刷一遍。
//   DUIN_DBLCLK   → 弹 mock "属性"框；
//   DUITVN_RCLICK → 弹 DuiMenu（结束任务 / 转到详细信息 / 打开文件位置 / 属性）。
// =============================================================================

#include "stdafx.h"
#include "MockData.h"
#include "XmlFactory.h"
#include "ProcessesPage.h"
#include "PerformancePage.h"
#include "OtherPages.h"

#include "DuiDpi.h"
#include "DuiHost.h"
#include "DuiNotify.h"
#include "DuiXmlBuilder.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Window/DuiFrameWindow.h"
#include "Controls/List/DuiTabPage.h"
#include "Controls/List/DuiTreeView.h"
#include "Controls/List/DuiMenu.h"
#include "Controls/List/DuiMenuBar.h"

using namespace balloonwjui;

CAppModule _Module;

// -----------------------------------------------------------------------------
// 控件 id（DuiFrameWindow 标题栏内置三按钮 1/2/3，业务 id 从 100 起）
// -----------------------------------------------------------------------------
// menu bar 自身的 ctrl id（与 taskmgr.xml 里 <menu-bar id="100"> 一致）
static const UINT kIdMenuBar         = 100;

// 6 个表格 page 的 ctrl id（DUITVN_COLUMNCLICK 路由用）
static const UINT kIdProcessesTree   = 2001;
static const UINT kIdAppHistoryTree  = 2002;
static const UINT kIdStartupTree     = 2003;
static const UINT kIdUsersTree       = 2004;
static const UINT kIdDetailsTree     = 2005;
static const UINT kIdServicesTree    = 2006;

// 进程页右键菜单各项 id（Win10 任务管理器进程页代表性条目）
static const UINT kMenuIdEndTask     = 9101;
static const UINT kMenuIdGotoDetails = 9102;
static const UINT kMenuIdOpenLoc     = 9103;
static const UINT kMenuIdProperties  = 9104;

// 标题栏菜单 ("文件" / "选项" / "查看") 下拉项 id。各 menu 内项 id 区
// 间隔开（File 92xx / Options 93xx / View 94xx），方便归并到一个 switch
// 里且不冲突。menu bar 通过 DUIN_VALUECHANGED 把这些 id 经 extra 透传给
// main 的 OnDuiNotify_。
static const UINT kFileMenuRunNew    = 9201;
static const UINT kFileMenuExit      = 9202;
static const UINT kOptMenuAlwaysTop  = 9301;
static const UINT kOptMenuMinHide    = 9302;
static const UINT kOptMenuShowFront  = 9303;
static const UINT kViewMenuRefresh   = 9401;
static const UINT kViewMenuSpdHigh   = 9402;
static const UINT kViewMenuSpdNormal = 9403;
static const UINT kViewMenuSpdLow    = 9404;
static const UINT kViewMenuSpdPause  = 9405;
static const UINT kViewMenuGroup     = 9406;
static const UINT kViewMenuExpandAll = 9407;

// -----------------------------------------------------------------------------
// 计时器：和 Win10 真任务管理器"正常"刷新档接近的 200ms。
// -----------------------------------------------------------------------------
static const UINT kTimerTick    = 1;
static const UINT kTickPeriodMs = 200;

// -----------------------------------------------------------------------------
// XML 兜底：taskmgr.xml 找不到时塞这串最小骨架，让窗口至少能起来。
// /utf-8 编译开关让源中文按 UTF-8 字节落地，无需 \xXX 转义。
// -----------------------------------------------------------------------------
static const char* kFallbackXmlUtf8 =
    "<frame-window title=\"\xe4\xbb\xbb\xe5\x8a\xa1\xe7\xae\xa1\xe7\x90\x86\xe5\x99\xa8\""
    " min-w=\"800\" min-h=\"600\""
    " has-min-button=\"true\" has-max-button=\"true\" has-close-button=\"true\""
    " resizable=\"true\">"
    "  <vbox>"
    "    <label text=\"taskmgr.xml \xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0\" weight=\"1\"/>"
    "  </vbox>"
    "</frame-window>";

// -----------------------------------------------------------------------------
// 文件 I/O
// -----------------------------------------------------------------------------
namespace {

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
    CString a = dir + _T("\\taskmgr.xml");
    if (::GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES)
    {
        return a;
    }
    CString b = dir + _T("\\..\\DemoTaskManager\\taskmgr.xml");
    if (::GetFileAttributes(b) != INVALID_FILE_ATTRIBUTES)
    {
        return b;
    }
    // x64 build 输出在 Bin\x64\，xml 还在 DemoTaskManager\，往上多走一级
    CString c = dir + _T("\\..\\..\\DemoTaskManager\\taskmgr.xml");
    if (::GetFileAttributes(c) != INVALID_FILE_ATTRIBUTES)
    {
        return c;
    }
    return CString();
}

} // anonymous

// -----------------------------------------------------------------------------
// 主框架
// -----------------------------------------------------------------------------
class TaskManagerFrame : public DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(TaskManagerFrame)
        MESSAGE_HANDLER(WM_DESTROY,    OnDestroy_)
        MESSAGE_HANDLER(WM_TIMER,      OnTimer_)
        MESSAGE_HANDLER(WM_SYSKEYDOWN, OnSysKeyDown_)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify_)
        CHAIN_MSG_MAP(DuiFrameWindow)
    END_MSG_MAP()

    // 暴露给 ScreenshotMode 用（程序化切 tab）。一般业务侧用不到，无需公开。
    const demotaskmgr::TaskMgrXmlOuts& GetOuts() const { return m_outs; }

    void BindOuts(const demotaskmgr::TaskMgrXmlOuts& outs)
    {
        m_outs = outs;
        // 给 6 个表格 page 各一个稳定 ctrl id，以便 WM_DUI_NOTIFY 路由识别。
        if (m_outs.processesPage)   { m_outs.processesPage  ->SetCtrlId(kIdProcessesTree);  }
        if (m_outs.appHistoryPage)  { m_outs.appHistoryPage ->SetCtrlId(kIdAppHistoryTree); }
        if (m_outs.startupPage)     { m_outs.startupPage    ->SetCtrlId(kIdStartupTree);    }
        if (m_outs.usersPage)       { m_outs.usersPage      ->SetCtrlId(kIdUsersTree);      }
        if (m_outs.detailsPage)     { m_outs.detailsPage    ->SetCtrlId(kIdDetailsTree);    }
        if (m_outs.servicesPage)    { m_outs.servicesPage   ->SetCtrlId(kIdServicesTree);   }

        // 标题栏下的 menu bar 由 XML 构造（<menu-bar id="100">）；这里
        // FindCtrlById 拿到指针，把 3 个 DuiMenu 成员关联到对应栏目上。
        // m_*Menu 的 lifetime = TaskManagerFrame；bar 借用指针，满足
        // "TrackPopupEx 调用栈期间保活"的契约（TaskManagerFrame 是顶层
        // 主 frame，主菜单弹出栈始终在它生命期内）。
        if (DuiHost* host = static_cast<DuiHost*>(this))
        {
            DuiControl* root = host->GetRoot();
            DuiControl* found = root ? root->FindCtrlById(kIdMenuBar) : nullptr;
            m_menuBar = dynamic_cast<DuiMenuBar*>(found);
            if (m_menuBar)
            {
                BuildMenus();
                m_menuBar->SetDropdown(0, &m_fileMenu);
                m_menuBar->SetDropdown(1, &m_optionsMenu);
                m_menuBar->SetDropdown(2, &m_viewMenu);
            }
        }
    }

    // 库里 DuiFrameWindow 不主动 PostQuitMessage（一个进程可能多 frame，
    // 关一个不等于退出），单 frame 进程必须自己把"主 frame 关闭"翻译成
    // "消息循环退出"。
    LRESULT OnDestroy_(UINT, WPARAM, LPARAM, BOOL& bHandled)
    {
        ::PostQuitMessage(0);
        bHandled = FALSE;
        return 0;
    }

    LRESULT OnTimer_(UINT, WPARAM wp, LPARAM, BOOL& bHandled)
    {
        if (wp == kTimerTick)
        {
            demotaskmgr::Tick();
            UpdateStatusBar();
            if (m_outs.processesPage)
            {
                m_outs.processesPage->RefreshNumbers();
            }
            if (m_outs.performancePage)
            {
                m_outs.performancePage->OnTick();
            }
        }
        bHandled = FALSE;
        return 0;
    }

    // Alt + letter 系统按键。WM_SYSKEYDOWN 不进 WM_KEYDOWN 路径，需要
    // frame 自己 hook 然后转发到 menu bar 的助记符路由。命中则消费消息。
    LRESULT OnSysKeyDown_(UINT, WPARAM wp, LPARAM, BOOL& bHandled)
    {
        if (m_menuBar && m_menuBar->ProcessAltKey((UINT)wp))
        {
            bHandled = TRUE;
            return 0;
        }
        bHandled = FALSE;
        return 0;
    }

    LRESULT OnDuiNotify_(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        const DuiNotify* n = reinterpret_cast<const DuiNotify*>(lParam);
        UINT ctrlId = static_cast<UINT>(wParam);
        if (!n)
        {
            bHandled = FALSE;
            return 0;
        }

        // ---- 标题栏下 menu bar：用户在某下拉里选中一项 → DUIN_VALUECHANGED ----
        if (ctrlId == kIdMenuBar && n->code == DUIN_VALUECHANGED)
        {
            HandleMenuBarChosen((UINT)n->extra);
            bHandled = TRUE;
            return 0;
        }

        // ---- 进程 tab：双击 / 右键 ----
        if (ctrlId == kIdProcessesTree && m_outs.processesPage)
        {
            if (n->code == DUIN_DBLCLK)
            {
                ShowProcessProperties();
            }
            else if (n->code == DuiTreeView::DUITVN_RCLICK)
            {
                ShowProcessContextMenu();
            }
        }

        // ---- 任意表格 tab：表头列名单击 → 排序 ----
        if (n->code == DuiTreeView::DUITVN_COLUMNCLICK)
        {
            auto* cn = static_cast<const DuiTreeView::DuiTreeCellNotify*>(
                          reinterpret_cast<void*>(n->extra));
            if (cn)
            {
                DispatchColumnSort(ctrlId, cn->col, cn->ascending);
                bHandled = TRUE;
                return 0;
            }
        }

        bHandled = FALSE;       // 让 DuiFrameWindow 仍能处理标题栏按钮等
        return 0;
    }

private:
    void UpdateStatusBar()
    {
        const auto& m = demotaskmgr::GetMetrics();
        TCHAR buf[64];
        if (m_outs.lblProc)
        {
            _stprintf_s(buf, _T("进程数: %d"), m.procCount);
            m_outs.lblProc->SetText(buf);
        }
        if (m_outs.lblCpu)
        {
            _stprintf_s(buf, _T("CPU 使用率: %.1f%%"), m.cpuTotalPct);
            m_outs.lblCpu->SetText(buf);
        }
        if (m_outs.lblMem)
        {
            _stprintf_s(buf, _T("内存: %.1f / %.1f GB"),
                        m.memUsedGB, m.memTotalGB);
            m_outs.lblMem->SetText(buf);
        }
    }

    // 双击进程节点 → 模拟 Win10 任务管理器的"属性"对话框。这里偷懒用
    // ::MessageBox 显示一段 mock 文本（PID / 用户 / CPU / 内存 / kind）；
    // 真做属性页是个独立 dialog 工作量，超出本 demo 的"用 DuiTreeView 跑通
    // 主流程"范围。
    void ShowProcessProperties()
    {
        int procIdx = m_outs.processesPage->GetSelectedProcessIndex();
        if (procIdx < 0)
        {
            return;     // 选中的是分组节点
        }
        const auto& p = demotaskmgr::GetProcesses()[procIdx];
        TCHAR msg[512];
        _stprintf_s(msg,
                    _T("进程: %s\n")
                    _T("PID: %d\n")
                    _T("用户: %s\n")
                    _T("状态: %s\n")
                    _T("CPU: %.1f%%\n")
                    _T("内存: %.1f MB\n")
                    _T("磁盘: %.1f MB/秒\n")
                    _T("网络: %.1f Mbps\n")
                    _T("GPU: %.1f%%"),
                    p.name.GetString(), p.pid, p.user.GetString(),
                    p.status.GetString(), p.cpuPct, p.memMB,
                    p.diskMBs, p.netMbps, p.gpuPct);
        ::MessageBox(m_hWnd, msg, _T("属性 (mock)"), MB_OK | MB_ICONINFORMATION);
    }

    // 表头点击排序的 dispatcher：按触发 tree 的 ctrlId 路由到对应 page。
    // ascending 由库内部根据当前 sort 自动翻转（同一列连点 = 升 → 降）。
    void DispatchColumnSort(UINT ctrlId, int col, bool ascending)
    {
        switch (ctrlId)
        {
        case kIdProcessesTree:
            if (m_outs.processesPage)
            {
                m_outs.processesPage->SortByColumn(col, ascending);
            }
            break;
        case kIdAppHistoryTree:
            if (m_outs.appHistoryPage)
            {
                m_outs.appHistoryPage->SortByColumn(col, ascending);
            }
            break;
        case kIdStartupTree:
            if (m_outs.startupPage)
            {
                m_outs.startupPage->SortByColumn(col, ascending);
            }
            break;
        case kIdUsersTree:
            if (m_outs.usersPage)
            {
                m_outs.usersPage->SortByColumn(col, ascending);
            }
            break;
        case kIdDetailsTree:
            if (m_outs.detailsPage)
            {
                m_outs.detailsPage->SortByColumn(col, ascending);
            }
            break;
        case kIdServicesTree:
            if (m_outs.servicesPage)
            {
                m_outs.servicesPage->SortByColumn(col, ascending);
            }
            break;
        default:
            break;
        }
    }

    // 把 3 个 DuiMenu（File / Options / View）填好。BindOuts 时调一次；
    // bar 通过 SetDropdown 借用这些 menu 的指针。menu 内项的 id 由
    // HandleMenuBarChosen 路由到具体行为。
    void BuildMenus()
    {
        m_fileMenu.Clear();
        m_fileMenu.AppendItem(kFileMenuRunNew,   _T("运行新任务(&N)..."));
        m_fileMenu.AppendSeparator();
        m_fileMenu.AppendItem(kFileMenuExit,     _T("退出(&X)"));

        m_optionsMenu.Clear();
        m_optionsMenu.AppendChecked(kOptMenuAlwaysTop, _T("总在最前(&O)"),         false);
        m_optionsMenu.AppendChecked(kOptMenuMinHide,   _T("最小化时隐藏(&H)"),     true);
        m_optionsMenu.AppendChecked(kOptMenuShowFront, _T("使用时显示在最前(&D)"), false);

        m_viewMenu.Clear();
        m_viewMenu.AppendItem    (kViewMenuRefresh,    _T("刷新(&R)\tF5"));
        m_viewMenu.AppendSeparator();
        // 真任务管理器 "更新速度" 是子菜单；这里平铺为 4 个互斥项简化。
        m_viewMenu.AppendChecked (kViewMenuSpdHigh,    _T("高更新频率"), false);
        m_viewMenu.AppendChecked (kViewMenuSpdNormal,  _T("正常"),        true);
        m_viewMenu.AppendChecked (kViewMenuSpdLow,     _T("低"),          false);
        m_viewMenu.AppendChecked (kViewMenuSpdPause,   _T("暂停"),        false);
        m_viewMenu.AppendSeparator();
        m_viewMenu.AppendChecked (kViewMenuGroup,      _T("按类型分组"),  true);
        m_viewMenu.AppendItem    (kViewMenuExpandAll,  _T("全部展开"));
    }

    // menu bar 发 DUIN_VALUECHANGED 时调，extra 是被选中的下拉项 id（与
    // 三个 m_*Menu 里 AppendItem 的 nID 一致）。File→退出 直接 SC_CLOSE，
    // 其它项 mock 提示。
    void HandleMenuBarChosen(UINT chosen)
    {
        if (chosen == kFileMenuExit)
        {
            ::PostMessage(m_hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
            return;
        }
        TCHAR msg[64];
        _stprintf_s(msg, _T("(mock) 菜单项 id=%u 已选中"), chosen);
        ::MessageBox(m_hWnd, msg, _T("DemoTaskManager"),
                     MB_OK | MB_ICONINFORMATION);
    }

    // 右键进程节点 → 弹 DuiMenu。被选项以 MessageBox 反馈（mock 实现）。
    // 鼠标位置由 GetCursorPos 给屏幕坐标，与 Win10 真菜单一致。
    void ShowProcessContextMenu()
    {
        int procIdx = m_outs.processesPage->GetSelectedProcessIndex();
        // 即便选中的是分组节点（procIdx < 0）也展示菜单 —— Win10 真任务管
        // 理器在分组节点上也能弹菜单，只是部分项是禁用的。本 demo 简化：
        // 无进程上下文时所有项都禁用，避免歧义。
        bool hasProc = (procIdx >= 0);

        DuiMenu menu;
        if (hasProc)
        {
            menu.AppendItem    (kMenuIdEndTask,     _T("结束任务(&E)"));
            menu.AppendItem    (kMenuIdGotoDetails, _T("转到详细信息(&G)"));
            menu.AppendItem    (kMenuIdOpenLoc,     _T("打开文件位置(&O)"));
            menu.AppendSeparator();
            menu.AppendItem    (kMenuIdProperties,  _T("属性(&R)"));
        }
        else
        {
            menu.AppendDisabled(kMenuIdEndTask,     _T("结束任务(&E)"));
            menu.AppendDisabled(kMenuIdGotoDetails, _T("转到详细信息(&G)"));
            menu.AppendDisabled(kMenuIdOpenLoc,     _T("打开文件位置(&O)"));
            menu.AppendSeparator();
            menu.AppendDisabled(kMenuIdProperties,  _T("属性(&R)"));
        }

        POINT pt = {};
        ::GetCursorPos(&pt);
        UINT chosen = menu.TrackPopup(pt.x, pt.y, m_hWnd);
        if (chosen == 0)
        {
            return;     // dismissed
        }

        const auto& p = demotaskmgr::GetProcesses()[procIdx];
        TCHAR msg[256];
        switch (chosen)
        {
        case kMenuIdEndTask:
            _stprintf_s(msg, _T("(mock) 结束任务: %s (PID %d)"),
                        p.name.GetString(), p.pid);
            break;
        case kMenuIdGotoDetails:
            _stprintf_s(msg, _T("(mock) 转到详细信息: %s"), p.name.GetString());
            break;
        case kMenuIdOpenLoc:
            _stprintf_s(msg, _T("(mock) 打开文件位置: %s"), p.name.GetString());
            break;
        case kMenuIdProperties:
            ShowProcessProperties();
            return;
        default:
            return;
        }
        ::MessageBox(m_hWnd, msg, _T("DemoTaskManager"), MB_OK | MB_ICONINFORMATION);
    }

private:
    demotaskmgr::TaskMgrXmlOuts m_outs;

    // 标题栏下方 menu bar：m_menuBar 由 BindOuts 通过 FindCtrlById 拿到
    // （所有权属 host root，frame 借用）；3 个 DuiMenu 是 frame 自己持有
    // 的成员，TaskManagerFrame 析构时一起销毁，满足 menu bar 借用契约。
    DuiMenuBar*  m_menuBar     = nullptr;
    DuiMenu      m_fileMenu;
    DuiMenu      m_optionsMenu;
    DuiMenu      m_viewMenu;
};

// =============================================================================
// 截图模式（命令行 --screenshot <outdir>）
//
// 自动遍历 7 个 tab，每个 tab SetCurSel 后让消息泵 + WM_TIMER 跑 ~400ms
// 让 mock 数据 / 折线图至少跑 2 帧、layout/paint 完成；然后 PrintWindow
// 整窗到 32bpp DIB，GDI+ 编码 PNG 存到 outdir/demo-taskmgr-<tag>.png。
//
// 用于自动化生成 guides.html 用的 demo 截图，避免手动 7 张 Win+Shift+S。
// 模式参考 DuiGallery/CaptureMode.cpp，简化掉了 capture-mark 走 BitBlt
// 子矩形那条路径 —— 这里只截整窗。
// =============================================================================
namespace {

bool GetPngClsid(CLSID& outClsid)
{
    UINT num = 0, sz = 0;
    Gdiplus::GetImageEncodersSize(&num, &sz);
    if (sz == 0)
    {
        return false;
    }
    std::vector<BYTE> buf(sz);
    auto* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(num, sz, info);
    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(info[i].MimeType, L"image/png") == 0)
        {
            outClsid = info[i].Clsid;
            return true;
        }
    }
    return false;
}

bool SaveWindowAsPng(HWND hwnd, LPCTSTR outPath)
{
    RECT rc; ::GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
    {
        return false;
    }

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;        // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS,
                                     &bits, NULL, 0);
    if (!dib)
    {
        return false;
    }
    HDC scrDC = ::GetDC(NULL);
    HDC memDC = ::CreateCompatibleDC(scrDC);
    ::ReleaseDC(NULL, scrDC);
    HGDIOBJ oldBmp = ::SelectObject(memDC, dib);

    // PrintWindow flag = 0 截整窗（含 NC：DuiFrameWindow 自绘标题栏算客户区）
    ::PrintWindow(hwnd, memDC, 0);

    ::SelectObject(memDC, oldBmp);
    ::DeleteDC(memDC);

    // PrintWindow / BitBlt 不写有意义 alpha；强制 0xFF 否则 PNG 透明
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < w * h; ++i)
    {
        p[i * 4 + 3] = 0xFF;
    }

    Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppARGB, p);
    CLSID clsid;
    bool ok = false;
    if (GetPngClsid(clsid))
    {
        ok = (bmp.Save(outPath, &clsid, nullptr) == Gdiplus::Ok);
    }
    ::DeleteObject(dib);
    return ok;
}

// 跑消息泵 ms 毫秒。期间 WM_TIMER / WM_PAINT 都会被 dispatch，让 mock
// 数据推进 + 折线图填充几个采样点 + layout 落地。
void PumpFor(DWORD ms)
{
    DWORD start = ::GetTickCount();
    MSG   msg;
    while (::GetTickCount() - start < ms)
    {
        while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        ::Sleep(10);
    }
}

int RunScreenshotMode(TaskManagerFrame& frame, LPCTSTR outDir)
{
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gpToken = 0;
    Gdiplus::GdiplusStartup(&gpToken, &gsi, nullptr);

    ::CreateDirectory(outDir, nullptr);

    // 把窗口拉到前台再强重画 + 等够 timer 跑满 5 个采样点（200ms × 5）
    ::SetForegroundWindow(frame.m_hWnd);
    ::InvalidateRect(frame.m_hWnd, NULL, TRUE);
    ::UpdateWindow(frame.m_hWnd);
    PumpFor(1500);

    DuiTabPage* tab = frame.GetOuts().tabPage;

    static const struct { int idx; LPCTSTR fname; } kPages[] = {
        { 0, _T("processes")    },
        { 1, _T("performance")  },
        { 2, _T("appHistory")   },
        { 3, _T("startup")      },
        { 4, _T("users")        },
        { 5, _T("details")      },
        { 6, _T("services")     },
    };

    int saved = 0;
    for (auto& p : kPages)
    {
        if (tab)
        {
            // notify=true 让 DuiTabPage 内部走完整 ApplyVisibility + 触发
            // host 重新 layout 当前 page；前一次 SetCurSel(0,false) 之后切
            // 到同一 idx 不重画的 early-return 也被绕开。
            tab->SetCurSel(p.idx, /*notify=*/true);
        }
        // 整窗强失效，让所有控件重新 layout/paint（防 "DUI 子控件 dirty
        // bit 被前一帧消费、新 page 内容未触发自身 InvalidateRect" 的边缘
        // 情况导致截图空白）。
        ::InvalidateRect(frame.m_hWnd, NULL, TRUE);
        ::UpdateWindow(frame.m_hWnd);
        // 切 tab 后等 500ms 让 timer 跑几次 + 新 page paint 完
        PumpFor(500);

        TCHAR path[MAX_PATH];
        _stprintf_s(path, _T("%s\\demo-taskmgr-%s.png"),
                    outDir, p.fname);
        if (SaveWindowAsPng(frame.m_hWnd, path))
        {
            ++saved;
        }
    }

    Gdiplus::GdiplusShutdown(gpToken);
    return saved;
}

// 命令行解析：找 "--screenshot" 后第一个空格分隔参数（剩余整段）作为
// 输出目录。命中返目录字符串；未命中返空 CString。
CString FindScreenshotArg(LPCTSTR cmdLine)
{
    if (!cmdLine)
    {
        return CString();
    }
    LPCTSTR flag = _T("--screenshot");
    LPCTSTR p = _tcsstr(cmdLine, flag);
    if (!p)
    {
        return CString();
    }
    p += _tcslen(flag);
    while (*p == _T(' ') || *p == _T('\t'))
    {
        ++p;
    }
    CString dir = p;
    dir.Trim();
    if (dir.IsEmpty())
    {
        dir = _T(".");
    }
    return dir;
}

} // anonymous

// -----------------------------------------------------------------------------
// 入口
// -----------------------------------------------------------------------------
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR cmdLine, int nCmdShow)
{
    DuiDpi::OptInPerMonitorV2();
    ::OleInitialize(NULL);
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);

    demotaskmgr::Init();

    int ret = 0;
    {
        TaskManagerFrame frame;
        if (!frame.Create(NULL, CWindow::rcDefault,
                          _T("DemoTaskManager"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }

        // 加载 app.ico (IDI_APP=100，青色 "T")
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

        std::string xml = ReadFileUtf8(ResolveXmlPath());
        if (xml.empty())
        {
            xml = kFallbackXmlUtf8;
        }

        demotaskmgr::TaskMgrXmlOuts  outs;
        auto                         factory = demotaskmgr::MakeTaskMgrFactory(&outs);
        DuiFrameWindowConfig         cfg;
        auto root = DuiXmlBuilder::FromFrameXml(xml.c_str(), cfg, factory);

        frame.ApplyConfig(cfg);
        if (root)
        {
            frame.SetClientContent(std::move(root));
        }
        frame.BindOuts(outs);

        frame.ResizeClient(960, 660);
        frame.CenterWindow();
        frame.ShowWindow(nCmdShow);
        frame.UpdateWindow();
        frame.SetTimer(kTimerTick, kTickPeriodMs);

        // --screenshot <dir>：进入截图模式，遍历 7 个 tab 各存一张 PNG，
        // 完成后立即退出（不进入主消息循环）。
        CString ssOut = FindScreenshotArg(cmdLine);
        if (!ssOut.IsEmpty())
        {
            int saved = RunScreenshotMode(frame, ssOut);
            TCHAR dbg[MAX_PATH + 64];
            _stprintf_s(dbg,
                        _T("[demotaskmgr] screenshot saved %d files to %s\n"),
                        saved, ssOut.GetString());
            ::OutputDebugString(dbg);
            ret = saved;
        }
        else
        {
            MSG msg;
            while (::GetMessage(&msg, NULL, 0, 0))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
            ret = static_cast<int>(msg.wParam);
        }
    }

    _Module.Term();
    ::OleUninitialize();
    return ret;
}
