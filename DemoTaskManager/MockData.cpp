// =============================================================================
// MockData.cpp —— 固定 seed 的 mock 生成 + WM_TIMER 抖动驱动
// =============================================================================
#include "stdafx.h"
#include "MockData.h"
#include <random>
#include <algorithm>

namespace demotaskmgr {

// =============================================================================
// 容量 / 配置常量
// =============================================================================

// Plan §阶段 0 钦定：进程 200 / 启动 30 / 服务 150 / 用户 2。
// 实际 split：80 应用、80 后台、40 Windows = 200。
static const int kProcessCount       = 200;
static const int kAppProcCount       = 80;
static const int kBackgroundProcCount = 80;
static const int kWindowsProcCount   = 40;

static const int kStartupItemCount   = 30;
static const int kServiceCount       = 150;
static const int kUserCount          = 2;

// 固定 seed，让多次启动 demo 看到一样的进程列表（便于截图对比 / 调试）。
// 0xFLAM1NG0 不是合法 16 进制，凑成 0xF1A1... 风格的字面量即可。
static const uint32_t kFixedSeed     = 0xF1A1106U;

// PID 起始值。Win10 真机一般从 4 起（System），1xxx-30xxx 是 user 进程的常见
// 区段。这里直接从 1024 起，避开 Idle/System/svchost 的特殊低位 pid。
static const int kPidStart           = 1024;
static const int kPidStride          = 4;        // 真实 Windows pid 是 4 的倍数

// 抖动幅度（每 Tick 加到当前值上、再 clamp 到 [min, max]）。
static const double kCpuJitter       = 3.0;      // 单进程 CPU 抖动 ±3%
static const double kMemJitter       = 6.0;      // 单进程内存抖动 ±6 MB
static const double kDiskJitter      = 0.4;      // 单进程磁盘抖动 ±0.4 MB/s
static const double kNetJitter       = 0.5;      // 单进程网络抖动 ±0.5 Mbps
static const double kGpuJitter       = 1.5;      // 单进程 GPU 抖动 ±1.5%

// 整机 ring buffer 抖动。比单进程抖得平稳一些，避免折线图过于跳动。
static const double kSysCpuJitter    = 2.0;
static const double kSysMemJitter    = 0.05;     // GB
static const double kSysDiskJitter   = 1.0;
static const double kSysNetJitter    = 0.8;
static const double kSysGpuJitter    = 1.2;

// =============================================================================
// 进程名候选池 —— 三类各有一组真实感的程序名 / svc 名，循环取用。
// =============================================================================

static LPCTSTR kAppNames[] = {
    _T("chrome.exe"), _T("Code.exe"), _T("explorer.exe"), _T("WeChat.exe"),
    _T("QQ.exe"), _T("Notepad.exe"), _T("WINWORD.EXE"), _T("EXCEL.EXE"),
    _T("Photoshop.exe"), _T("devenv.exe"), _T("steam.exe"), _T("Discord.exe"),
    _T("Spotify.exe"), _T("vlc.exe"), _T("PowerPnt.exe"), _T("Acrobat.exe"),
    _T("Postman.exe"), _T("notion.exe"), _T("Slack.exe"), _T("Zoom.exe"),
};

static LPCTSTR kBgNames[] = {
    _T("RuntimeBroker.exe"), _T("dllhost.exe"), _T("svchost.exe"),
    _T("ctfmon.exe"), _T("SearchIndexer.exe"), _T("ShellExperienceHost.exe"),
    _T("SecurityHealthService.exe"), _T("MsMpEng.exe"), _T("OneDrive.exe"),
    _T("YourPhone.exe"), _T("CompPkgSrv.exe"), _T("backgroundTaskHost.exe"),
    _T("audiodg.exe"), _T("smartscreen.exe"), _T("conhost.exe"),
    _T("WMIRegistrationService.exe"), _T("WmiPrvSE.exe"),
    _T("StartMenuExperienceHost.exe"), _T("dwm.exe"), _T("fontdrvhost.exe"),
};

static LPCTSTR kWinNames[] = {
    _T("System"), _T("Idle"), _T("csrss.exe"), _T("services.exe"),
    _T("lsass.exe"), _T("winlogon.exe"), _T("wininit.exe"), _T("smss.exe"),
    _T("Registry"), _T("Memory Compression"), _T("spoolsv.exe"),
    _T("LSAIso"), _T("SecureSystem"),
};

static LPCTSTR kPublishers[] = {
    _T("Microsoft Corporation"), _T("Google LLC"), _T("Adobe Systems"),
    _T("Tencent"), _T("Valve Corporation"), _T("Discord Inc."),
    _T("Spotify AB"), _T("Slack Technologies"), _T("Zoom Video"),
    _T("Notion Labs"), _T("Postdot Technologies"),
};

static LPCTSTR kServiceNames[] = {
    _T("AppXSvc"), _T("Audiosrv"), _T("BFE"), _T("BITS"), _T("Browser"),
    _T("CryptSvc"), _T("DcomLaunch"), _T("Dhcp"), _T("Dnscache"),
    _T("EventLog"), _T("EventSystem"), _T("FontCache"), _T("gpsvc"),
    _T("LanmanServer"), _T("LanmanWorkstation"), _T("LSM"), _T("MpsSvc"),
    _T("netprofm"), _T("NlaSvc"), _T("nsi"), _T("PlugPlay"), _T("Power"),
    _T("ProfSvc"), _T("RpcEptMapper"), _T("RpcSs"), _T("SamSs"),
    _T("Schedule"), _T("SENS"), _T("SessionEnv"), _T("Spooler"),
    _T("StorSvc"), _T("SysMain"), _T("Themes"), _T("TimeBrokerSvc"),
    _T("UserManager"), _T("WinDefend"), _T("Winmgmt"), _T("WlanSvc"),
    _T("WSearch"), _T("wuauserv"), _T("WpnService"), _T("XblAuthManager"),
};

static LPCTSTR kServiceGroups[] = {
    _T("LocalServiceNetworkRestricted"), _T("LocalSystemNetworkRestricted"),
    _T("netsvcs"), _T("NetworkService"), _T("LocalService"),
    _T("LocalServiceNoNetwork"), _T("DcomLaunch"), _T("RPCSS"),
    _T("UnistackSvcGroup"),
};

template <int N>
static LPCTSTR PickName(LPCTSTR const (&pool)[N], int i)
{
    return pool[i % N];
}

// =============================================================================
// 全局存储（文件作用域），通过 GetXxx() 暴露 const 引用
// =============================================================================

static std::vector<Process>     g_processes;
static std::vector<StartupItem> g_startup;
static std::vector<Service>     g_services;
static std::vector<User>        g_users;
static Metrics                  g_metrics;
static std::mt19937             g_rng(kFixedSeed);
static bool                     g_initialized = false;

// =============================================================================
// MetricRing
// =============================================================================

void MetricRing::Push(double v)
{
    m_buf[m_head] = v;
    m_head = (m_head + 1) % kCapacity;
    if (m_count < kCapacity)
    {
        ++m_count;
    }
}

double MetricRing::At(int i) const
{
    if (i < 0 || i >= m_count)
    {
        return 0.0;
    }
    // 当 m_count < kCapacity 时，最旧的样本就在 buf[0]；
    // 当 buffer 满了之后，最旧的样本在 buf[m_head]（环回起点）。
    int oldestIdx;
    if (m_count < kCapacity)
    {
        oldestIdx = 0;
    }
    else
    {
        oldestIdx = m_head;
    }
    int idx = (oldestIdx + i) % kCapacity;
    return m_buf[idx];
}

double MetricRing::Latest() const
{
    if (m_count == 0)
    {
        return 0.0;
    }
    int idx = (m_head + kCapacity - 1) % kCapacity;
    return m_buf[idx];
}

// =============================================================================
// 工具：均匀 / 正态抖动 + clamp
// =============================================================================

static double UniformRange(double lo, double hi)
{
    std::uniform_real_distribution<double> d(lo, hi);
    return d(g_rng);
}

static double Jitter(double cur, double amplitude, double lo, double hi)
{
    double v = cur + UniformRange(-amplitude, amplitude);
    if (v < lo) { v = lo; }
    if (v > hi) { v = hi; }
    return v;
}

// =============================================================================
// 初始化 —— 一次性生成所有静态 mock 数据
// =============================================================================

static void GenerateProcesses()
{
    g_processes.clear();
    g_processes.reserve(kProcessCount);

    int pid = kPidStart;
    int idx = 0;

    auto emit = [&](LPCTSTR name, ProcessKind kind, LPCTSTR user, int parent)
    {
        Process p;
        p.pid          = pid;
        p.parentIndex  = parent;
        p.name         = name;
        p.kind         = kind;
        p.user         = user;
        p.status       = _T("正在运行");
        p.cpuPct       = UniformRange(0.0, 5.0);
        p.memMB        = UniformRange(20.0, 350.0);
        p.diskMBs      = UniformRange(0.0, 1.5);
        p.netMbps      = UniformRange(0.0, 2.0);
        p.gpuPct       = UniformRange(0.0, 4.0);
        g_processes.push_back(p);
        pid += kPidStride;
        ++idx;
    };

    // --- 应用进程：80 个 ---
    // 前 16 个挑出来当父进程（chrome、Code 这种），剩下的在它们底下做子进程（模拟
    // chrome 多 tab、Code 多 renderer），让进程页能展示两层树结构。
    int appsCreated = 0;
    int appParents[16] = {};
    for (int i = 0; i < 16 && appsCreated < kAppProcCount; ++i)
    {
        appParents[i] = idx;
        emit(PickName(kAppNames, i), ProcessKind::App,
             (i % 2 == 0) ? _T("balloonwj") : _T("admin"), -1);
        ++appsCreated;
    }
    int childCounter = 0;
    while (appsCreated < kAppProcCount)
    {
        int parent = appParents[childCounter % 16];
        TCHAR childName[64];
        // 子进程显示名 = 父进程名 + " (Helper N)"
        _stprintf_s(childName, _T("%s (Helper %d)"),
                    g_processes[parent].name.GetString(),
                    childCounter / 16 + 1);
        emit(childName, ProcessKind::App,
             g_processes[parent].user.GetString(), parent);
        ++appsCreated;
        ++childCounter;
    }

    // --- 后台进程：80 个 ---
    for (int i = 0; i < kBackgroundProcCount; ++i)
    {
        TCHAR n[64];
        _stprintf_s(n, _T("%s"), PickName(kBgNames, i));
        emit(n, ProcessKind::Background,
             (i % 3 == 0) ? _T("SYSTEM") :
             (i % 3 == 1) ? _T("LOCAL SERVICE") : _T("NETWORK SERVICE"), -1);
    }

    // --- Windows 进程：40 个 ---
    for (int i = 0; i < kWindowsProcCount; ++i)
    {
        emit(PickName(kWinNames, i), ProcessKind::Windows,
             _T("SYSTEM"), -1);
    }

    // 让头几个有"工作量"的进程数值高一些，给截图一个像样的初始视觉。
    for (size_t i = 0; i < g_processes.size() && i < 8; ++i)
    {
        g_processes[i].cpuPct  = UniformRange(5.0, 20.0);
        g_processes[i].memMB   = UniformRange(300.0, 1200.0);
        g_processes[i].diskMBs = UniformRange(0.5, 5.0);
    }
}

static void GenerateStartupItems()
{
    g_startup.clear();
    g_startup.reserve(kStartupItemCount);
    for (int i = 0; i < kStartupItemCount; ++i)
    {
        StartupItem s;
        s.name      = PickName(kAppNames, i);
        s.publisher = PickName(kPublishers, i);
        s.enabled   = (i % 4 != 0);            // 大约 25% 禁用
        s.impact    = i % 4;                   // 0..3 循环
        g_startup.push_back(s);
    }
}

static void GenerateServices()
{
    g_services.clear();
    g_services.reserve(kServiceCount);
    int pid = 1500;
    for (int i = 0; i < kServiceCount; ++i)
    {
        Service s;
        s.name        = PickName(kServiceNames, i);
        // 名字超出名单时加 "_N" 后缀，避免 150 个服务全是重复的 42 个名
        if (i >= (int)(sizeof(kServiceNames) / sizeof(kServiceNames[0])))
        {
            CString suffix;
            suffix.Format(_T("_%d"),
                          i / (sizeof(kServiceNames) / sizeof(kServiceNames[0])));
            s.name += suffix;
        }
        bool running  = (i % 5 != 0);          // 80% 在跑
        s.pid         = running ? pid : 0;
        s.status      = running ? _T("正在运行") : _T("已停止");
        s.description = _T("Windows 服务（mock 描述）");
        s.group       = PickName(kServiceGroups, i);
        g_services.push_back(s);
        if (running) { pid += 4; }
    }
}

static void GenerateUsers()
{
    g_users.clear();
    g_users.reserve(kUserCount);

    User a; a.name = _T("balloonwj"); a.sessionId = 1;
    User b; b.name = _T("admin");     b.sessionId = 2;

    for (size_t i = 0; i < g_processes.size(); ++i)
    {
        const Process& p = g_processes[i];
        // SYSTEM / LOCAL SERVICE / NETWORK SERVICE 不归两个交互式用户；
        // 用户页只列两个交互式用户拥有的进程。
        if (p.user == _T("balloonwj"))
        {
            a.processIndices.push_back((int)i);
        }
        else if (p.user == _T("admin"))
        {
            b.processIndices.push_back((int)i);
        }
    }

    g_users.push_back(a);
    g_users.push_back(b);
}

void Init()
{
    if (g_initialized) { return; }
    g_rng.seed(kFixedSeed);

    GenerateProcesses();
    GenerateStartupItems();
    GenerateServices();
    GenerateUsers();

    // 为 5 个 ring buffer 各 push 一个起始采样点，避免折线图刚启动就空白
    g_metrics.cpu.Push(UniformRange(8.0, 18.0));
    g_metrics.mem.Push(UniformRange(7.0, 9.0));
    g_metrics.disk.Push(UniformRange(2.0, 6.0));
    g_metrics.net.Push(UniformRange(1.0, 4.0));
    g_metrics.gpu.Push(UniformRange(3.0, 12.0));

    g_metrics.cpuTotalPct = g_metrics.cpu.Latest();
    g_metrics.memUsedGB   = g_metrics.mem.Latest();
    g_metrics.procCount   = (int)g_processes.size();

    g_initialized = true;
}

// =============================================================================
// Tick —— WM_TIMER 200ms 一次
// =============================================================================

void Tick()
{
    if (!g_initialized) { Init(); }

    // 整机 ring buffer
    double cpu  = Jitter(g_metrics.cpu.Latest(),  kSysCpuJitter,  0.0, 100.0);
    double mem  = Jitter(g_metrics.mem.Latest(),  kSysMemJitter,  0.5, g_metrics.memTotalGB);
    double disk = Jitter(g_metrics.disk.Latest(), kSysDiskJitter, 0.0, 100.0);
    double net  = Jitter(g_metrics.net.Latest(),  kSysNetJitter,  0.0, 100.0);
    double gpu  = Jitter(g_metrics.gpu.Latest(),  kSysGpuJitter,  0.0, 100.0);

    g_metrics.cpu.Push(cpu);
    g_metrics.mem.Push(mem);
    g_metrics.disk.Push(disk);
    g_metrics.net.Push(net);
    g_metrics.gpu.Push(gpu);

    g_metrics.cpuTotalPct = cpu;
    g_metrics.memUsedGB   = mem;

    // 单进程数值抖动
    for (auto& p : g_processes)
    {
        p.cpuPct  = Jitter(p.cpuPct,  kCpuJitter,  0.0, 100.0);
        p.memMB   = Jitter(p.memMB,   kMemJitter, 10.0, 4096.0);
        p.diskMBs = Jitter(p.diskMBs, kDiskJitter, 0.0, 50.0);
        p.netMbps = Jitter(p.netMbps, kNetJitter,  0.0, 50.0);
        p.gpuPct  = Jitter(p.gpuPct,  kGpuJitter,  0.0, 100.0);
    }
}

// =============================================================================
// 只读访问
// =============================================================================

const std::vector<Process>&     GetProcesses()    { Init(); return g_processes; }
const std::vector<StartupItem>& GetStartupItems() { Init(); return g_startup;   }
const std::vector<Service>&     GetServices()     { Init(); return g_services;  }
const std::vector<User>&        GetUsers()        { Init(); return g_users;     }
const Metrics&                  GetMetrics()      { Init(); return g_metrics;   }

// =============================================================================
// 字母位图 / 图标生成器
// =============================================================================

// 字体高度按方块尺寸的 70% 取，让字母醒目又不贴边。
static const double kLetterFontHeightRatio = 0.70;

HBITMAP MakeLetterBitmap(int sz, COLORREF bg, TCHAR letter, COLORREF fg)
{
    HDC     screen = ::GetDC(NULL);
    HDC     mem    = ::CreateCompatibleDC(screen);
    HBITMAP bmp    = ::CreateCompatibleBitmap(screen, sz, sz);
    HBITMAP old    = (HBITMAP)::SelectObject(mem, bmp);

    // 实色背景
    RECT   rc = { 0, 0, sz, sz };
    HBRUSH bgBr = ::CreateSolidBrush(bg);
    ::FillRect(mem, &rc, bgBr);
    ::DeleteObject(bgBr);

    // 居中粗体字母
    if (letter)
    {
        LOGFONT lf = {};
        lf.lfHeight  = -(int)(sz * kLetterFontHeightRatio);
        lf.lfWeight  = FW_BOLD;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = ANTIALIASED_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        HFONT font   = ::CreateFontIndirect(&lf);
        HFONT oldFnt = (HFONT)::SelectObject(mem, font);
        int   oldBk  = ::SetBkMode(mem, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(mem, fg);

        TCHAR s[2] = { letter, 0 };
        ::DrawText(mem, s, 1, &rc,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        ::SetTextColor(mem, oldClr);
        ::SetBkMode(mem, oldBk);
        ::SelectObject(mem, oldFnt);
        ::DeleteObject(font);
    }

    ::SelectObject(mem, old);
    ::DeleteDC(mem);
    ::ReleaseDC(NULL, screen);
    return bmp;
}

HICON MakeLetterIcon(int sz, COLORREF bg, TCHAR letter, COLORREF fg)
{
    // hbmColor + hbmMask：mask 全 0 = 整张图都不透明（实色背景占满），
    // 等价于"方形不带透明边"的 icon —— 任务栏 / Alt-Tab 都能识别。
    HBITMAP color = MakeLetterBitmap(sz, bg, letter, fg);
    HBITMAP mask  = ::CreateBitmap(sz, sz, 1, 1, NULL);  // 全 0
    if (!color || !mask)
    {
        if (color) { ::DeleteObject(color); }
        if (mask)  { ::DeleteObject(mask);  }
        return NULL;
    }

    ICONINFO ii = {};
    ii.fIcon    = TRUE;
    ii.hbmColor = color;
    ii.hbmMask  = mask;
    HICON hIcon = ::CreateIconIndirect(&ii);

    // CreateIconIndirect 复制位图，原 color/mask 可释放
    ::DeleteObject(color);
    ::DeleteObject(mask);
    return hIcon;
}

} // namespace demotaskmgr
