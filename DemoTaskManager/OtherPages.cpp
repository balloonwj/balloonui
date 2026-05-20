// =============================================================================
// OtherPages.cpp
// =============================================================================
#include "stdafx.h"
#include "OtherPages.h"
#include "MockData.h"
#include <algorithm>

namespace demotaskmgr {

using balloonwjui::DuiTreeView;

// =============================================================================
// 共用 helper
// =============================================================================

namespace {

LPCTSTR kImpactLabels[4] = { _T("无"), _T("低"), _T("中"), _T("高") };

LPCTSTR ImpactLabel(int impact)
{
    if (impact < 0) { impact = 0; }
    if (impact > 3) { impact = 3; }
    return kImpactLabels[impact];
}

// 把 procIdx 派生 deterministic 字段（mock，固定 seed 复现）
CString FakeCpuTime(int procIdx)
{
    int totalSec = (procIdx * 73 + 41) % (24 * 3600);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    CString out; out.Format(_T("%02d:%02d:%02d"), h, m, s);
    return out;
}
int FakeCpuTimeSec(int procIdx) { return (procIdx * 73 + 41) % (24 * 3600); }

double FakeNetTrafficMB(int procIdx)
{
    return ((procIdx * 113 + 7) % 5000) / 10.0;
}

CString FakeNetTrafficStr(int procIdx)
{
    CString out; out.Format(_T("%.1f MB"), FakeNetTrafficMB(procIdx));
    return out;
}

double FakeMeteredMB(int procIdx)  { return ((procIdx * 31 + 5) % 100) / 10.0; }
double FakeTileUpdMB(int procIdx)  { return ((procIdx * 17 + 11) % 50) / 10.0; }

CString FakeStartTime(int procIdx)
{
    int h = (procIdx * 17) % 24;
    int m = (procIdx * 31) % 60;
    int s = (procIdx * 53) % 60;
    CString out; out.Format(_T("%02d:%02d:%02d"), h, m, s);
    return out;
}
int FakeStartTimeSec(int procIdx)
{
    return ((procIdx * 17) % 24) * 3600
         + ((procIdx * 31) % 60) * 60
         + ((procIdx * 53) % 60);
}

// 比较器封装：根据 ascending 方向把 less<T> 翻转。stable_sort 比 sort 更友
// 好（保留同 col 同值的相对顺序，避免视觉跳变）。
template <typename T, typename Cmp>
void SortStable(std::vector<T>& v, bool ascending, Cmp lessFn)
{
    if (ascending)
    {
        std::stable_sort(v.begin(), v.end(), lessFn);
    }
    else
    {
        std::stable_sort(v.begin(), v.end(),
                         [&lessFn](const T& a, const T& b) { return lessFn(b, a); });
    }
}

} // anonymous

// =============================================================================
// AppHistoryPage
//   col 0=名称 / 1=CPU 时间 / 2=网络 / 3=按流量计费 / 4=磁贴更新
// =============================================================================

void AppHistoryPage::AddColumnsOnce()
{
    AddColumn(_T("名称"),         220, 60, DT_LEFT,  true);
    AddColumn(_T("CPU 时间"),      90, 60, DT_RIGHT, true);
    AddColumn(_T("网络"),          90, 50, DT_RIGHT, true);
    AddColumn(_T("按流量计费"),   100, 60, DT_RIGHT, true);
    AddColumn(_T("磁贴更新"),     100, 60, DT_RIGHT, true);
}

void AppHistoryPage::BuildNodes()
{
    Clear();
    const auto& procs = GetProcesses();
    for (int idx : m_order)
    {
        const Process& p = procs[idx];
        int id = AddRoot(p.name);
        SetCellText(id, 1, FakeCpuTime(idx));
        SetCellText(id, 2, FakeNetTrafficStr(idx));
        CString meter; meter.Format(_T("%.2f MB"), FakeMeteredMB(idx));
        SetCellText(id, 3, meter);
        CString tile;  tile.Format(_T("%.2f MB"),  FakeTileUpdMB(idx));
        SetCellText(id, 4, tile);
    }
}

void AppHistoryPage::Init()
{
    AddColumnsOnce();
    const auto& procs = GetProcesses();
    m_order.clear();
    for (size_t i = 0; i < procs.size(); ++i)
    {
        if (procs[i].kind == ProcessKind::App && procs[i].parentIndex < 0)
        {
            m_order.push_back((int)i);
        }
    }
    BuildNodes();
}

void AppHistoryPage::SortByColumn(int col, bool asc)
{
    const auto& procs = GetProcesses();
    auto less = [&procs, col](int a, int b) -> bool
    {
        switch (col)
        {
        case 0: return procs[a].name.CompareNoCase(procs[b].name) < 0;
        case 1: return FakeCpuTimeSec(a)   < FakeCpuTimeSec(b);
        case 2: return FakeNetTrafficMB(a) < FakeNetTrafficMB(b);
        case 3: return FakeMeteredMB(a)    < FakeMeteredMB(b);
        case 4: return FakeTileUpdMB(a)    < FakeTileUpdMB(b);
        default: return a < b;
        }
    };
    SortStable(m_order, asc, less);
    BuildNodes();
    SetSortIndicator(col, asc ? +1 : -1);
}

// =============================================================================
// StartupPage
//   col 0=名称 / 1=发布者 / 2=状态(CHECKBOX) / 3=启动影响
// =============================================================================

void StartupPage::AddColumnsOnce()
{
    AddColumn(_T("名称"),       220, 60, DT_LEFT,  true);
    AddColumn(_T("发布者"),     220, 60, DT_LEFT,  true);
    AddColumn(_T("状态"),        90, 60, DT_LEFT,  true);
    AddColumn(_T("启动影响"),   100, 60, DT_LEFT,  true);
}

void StartupPage::BuildNodes()
{
    Clear();
    const auto& items = GetStartupItems();
    for (int idx : m_order)
    {
        const StartupItem& s = items[idx];
        int id = AddRoot(s.name);
        SetCellText(id, 1, s.publisher);
        SetCellChecked(id, 2, s.enabled);
        SetCellText   (id, 2, s.enabled ? _T("已启用") : _T("已禁用"));
        SetCellText   (id, 3, ImpactLabel(s.impact));
    }
}

void StartupPage::Init()
{
    AddColumnsOnce();
    const auto& items = GetStartupItems();
    m_order.clear();
    m_order.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i) { m_order.push_back((int)i); }
    BuildNodes();
}

void StartupPage::SortByColumn(int col, bool asc)
{
    const auto& items = GetStartupItems();
    auto less = [&items, col](int a, int b) -> bool
    {
        const StartupItem& sa = items[a];
        const StartupItem& sb = items[b];
        switch (col)
        {
        case 0: return sa.name.CompareNoCase(sb.name) < 0;
        case 1: return sa.publisher.CompareNoCase(sb.publisher) < 0;
        case 2: return (sa.enabled ? 1 : 0) > (sb.enabled ? 1 : 0); // enabled 在前
        case 3: return sa.impact < sb.impact;
        default: return a < b;
        }
    };
    SortStable(m_order, asc, less);
    BuildNodes();
    SetSortIndicator(col, asc ? +1 : -1);
}

// =============================================================================
// UsersPage
//   两层结构。Sort 只动每个 user root 下的 child 顺序，user root 自身保持
//   [balloonwj, admin] 创建顺序（Win10 真任务管理器也不让用户对 user 行排）。
//   col 0=名称 / 1=CPU
// =============================================================================

void UsersPage::AddColumnsOnce()
{
    AddColumn(_T("名称"),  220, 60, DT_LEFT,  true);
    AddColumn(_T("CPU"),    90, 50, DT_RIGHT, true);
}

void UsersPage::BuildNodes()
{
    Clear();
    const auto& users = GetUsers();
    const auto& procs = GetProcesses();
    for (size_t u = 0; u < users.size(); ++u)
    {
        const User& usr = users[u];
        double totalCpu = 0.0;
        for (int idx : usr.processIndices) { totalCpu += procs[idx].cpuPct; }

        CString rootText; rootText.Format(_T("%s (%d)"),
                                          usr.name.GetString(),
                                          (int)usr.processIndices.size());
        int rootId = AddRoot(rootText);
        CString rcpu; rcpu.Format(_T("%.1f%%"), totalCpu);
        SetCellText(rootId, 1, rcpu);

        const std::vector<int>& order = m_userChildOrder[u];
        for (int idx : order)
        {
            const Process& p = procs[idx];
            int childId = AddChild(rootId, p.name);
            CString cpu; cpu.Format(_T("%.1f%%"), p.cpuPct);
            SetCellText(childId, 1, cpu);
        }
    }
    ExpandAll();
}

void UsersPage::Init()
{
    AddColumnsOnce();
    const auto& users = GetUsers();
    m_userChildOrder.clear();
    m_userChildOrder.resize(users.size());
    for (size_t u = 0; u < users.size(); ++u)
    {
        m_userChildOrder[u] = users[u].processIndices;   // 初始顺序 = 原始
    }
    BuildNodes();
}

void UsersPage::SortByColumn(int col, bool asc)
{
    const auto& procs = GetProcesses();
    auto less = [&procs, col](int a, int b) -> bool
    {
        switch (col)
        {
        case 0: return procs[a].name.CompareNoCase(procs[b].name) < 0;
        case 1: return procs[a].cpuPct < procs[b].cpuPct;
        default: return a < b;
        }
    };
    for (auto& v : m_userChildOrder)
    {
        SortStable(v, asc, less);
    }
    BuildNodes();
    SetSortIndicator(col, asc ? +1 : -1);
}

// =============================================================================
// DetailsPage —— 14 列 + 首列冻结
// =============================================================================

void DetailsPage::AddColumnsOnce()
{
    AddColumn(_T("名称"),                  200, 60, DT_LEFT,  true);
    AddColumn(_T("PID"),                    70, 40, DT_RIGHT, true);
    AddColumn(_T("状态"),                   80, 40, DT_LEFT,  true);
    AddColumn(_T("用户名"),                120, 60, DT_LEFT,  true);
    AddColumn(_T("CPU"),                    60, 40, DT_RIGHT, true);
    AddColumn(_T("内存(专用工作集)"),      130, 60, DT_RIGHT, true);
    AddColumn(_T("内存(提交大小)"),        130, 60, DT_RIGHT, true);
    AddColumn(_T("UAC 虚拟化"),            100, 60, DT_LEFT,  true);
    AddColumn(_T("命令行"),                280, 80, DT_LEFT,  false);
    AddColumn(_T("平台"),                   60, 40, DT_LEFT,  true);
    AddColumn(_T("描述"),                  220, 60, DT_LEFT,  true);
    AddColumn(_T("公司"),                  180, 60, DT_LEFT,  true);
    AddColumn(_T("操作"),                   80, 60, DT_LEFT,  true);
    AddColumn(_T("启动时间"),              100, 60, DT_LEFT,  true);
    SetFrozenColumns(1);
}

void DetailsPage::BuildNodes()
{
    Clear();
    const auto& procs = GetProcesses();
    for (int idx : m_order)
    {
        const Process& p = procs[idx];
        int id = AddRoot(p.name);

        CString s;
        s.Format(_T("%d"), p.pid);                      SetCellText(id, 1, s);
        SetCellText(id, 2, p.status);
        SetCellText(id, 3, p.user);
        s.Format(_T("%02.0f"), p.cpuPct);               SetCellText(id, 4, s);
        s.Format(_T("%.0f K"),    p.memMB * 1024.0);    SetCellText(id, 5, s);
        s.Format(_T("%.0f K"),    p.memMB * 1024.0 + 256.0);
                                                         SetCellText(id, 6, s);
        SetCellText(id, 7, _T("已禁用"));

        CString cmd;
        cmd.Format(_T("\"C:\\Windows\\System32\\%s\" /Embedding"),
                   p.name.GetString());
        SetCellText(id, 8, cmd);
        SetCellText(id, 9, _T("64 位"));
        SetCellText(id, 10, _T("Windows 进程（mock 描述）"));
        SetCellText(id, 11, _T("Microsoft Corporation"));
        SetCellText(id, 12, _T("正在运行"));
        SetCellText(id, 13, FakeStartTime(idx));
    }
}

void DetailsPage::Init()
{
    AddColumnsOnce();
    const auto& procs = GetProcesses();
    m_order.clear();
    m_order.reserve(procs.size());
    for (size_t i = 0; i < procs.size(); ++i) { m_order.push_back((int)i); }
    BuildNodes();
}

void DetailsPage::SortByColumn(int col, bool asc)
{
    const auto& procs = GetProcesses();
    auto less = [&procs, col](int a, int b) -> bool
    {
        const Process& pa = procs[a];
        const Process& pb = procs[b];
        switch (col)
        {
        case 0:  return pa.name.CompareNoCase(pb.name) < 0;
        case 1:  return pa.pid < pb.pid;
        case 2:  return pa.status.CompareNoCase(pb.status) < 0;
        case 3:  return pa.user.CompareNoCase(pb.user) < 0;
        case 4:  return pa.cpuPct < pb.cpuPct;
        case 5:  return pa.memMB  < pb.memMB;
        case 6:  return pa.memMB  < pb.memMB;
        case 13: return FakeStartTimeSec(a) < FakeStartTimeSec(b);
        default: return a < b;
        }
    };
    SortStable(m_order, asc, less);
    BuildNodes();
    SetSortIndicator(col, asc ? +1 : -1);
}

// =============================================================================
// ServicesPage
// =============================================================================

namespace {

// 服务图标：绿底白 'S'。lazy file-static，进程退出 OS 自动回收。
static const int      kSvcIconPx = 16;
static const COLORREF kSvcIconBg = RGB(60, 160, 110);   // 绿 #3CA06E
static const COLORREF kSvcIconFg = RGB(255, 255, 255);
static HBITMAP        s_iconService = NULL;
static bool           s_svcIconInited = false;

static HBITMAP EnsureServiceIcon()
{
    if (!s_svcIconInited)
    {
        s_svcIconInited = true;
        s_iconService = MakeLetterBitmap(kSvcIconPx, kSvcIconBg,
                                         _T('S'), kSvcIconFg);
    }
    return s_iconService;
}

} // anonymous

void ServicesPage::AddColumnsOnce()
{
    AddColumn(_T("名称"),  150, 60, DT_LEFT,  true);
    AddColumn(_T("PID"),    70, 40, DT_RIGHT, true);
    AddColumn(_T("描述"),  280, 60, DT_LEFT,  true);
    AddColumn(_T("状态"),   90, 60, DT_LEFT,  true);
    AddColumn(_T("组"),    180, 60, DT_LEFT,  true);
    AddColumn(_T("备注"),  120, 60, DT_LEFT,  false);
}

void ServicesPage::BuildNodes()
{
    Clear();
    HBITMAP icon = EnsureServiceIcon();
    const auto& services = GetServices();
    for (int idx : m_order)
    {
        const Service& s = services[idx];
        int id = AddRoot(s.name);
        if (icon)
        {
            SetItemIcon(id, icon);
        }
        if (s.pid > 0)
        {
            CString p; p.Format(_T("%d"), s.pid);
            SetCellText(id, 1, p);
        }
        else
        {
            SetCellText(id, 1, _T("--"));
        }
        SetCellText(id, 2, s.description);
        SetCellText(id, 3, s.status);
        SetCellText(id, 4, s.group);
        SetCellText(id, 5, _T(""));
    }
}

void ServicesPage::Init()
{
    AddColumnsOnce();
    const auto& services = GetServices();
    m_order.clear();
    m_order.reserve(services.size());
    for (size_t i = 0; i < services.size(); ++i) { m_order.push_back((int)i); }
    BuildNodes();
}

void ServicesPage::SortByColumn(int col, bool asc)
{
    const auto& services = GetServices();
    auto less = [&services, col](int a, int b) -> bool
    {
        const Service& sa = services[a];
        const Service& sb = services[b];
        switch (col)
        {
        case 0: return sa.name.CompareNoCase(sb.name) < 0;
        case 1: return sa.pid < sb.pid;
        case 2: return sa.description.CompareNoCase(sb.description) < 0;
        case 3: return sa.status.CompareNoCase(sb.status) < 0;
        case 4: return sa.group.CompareNoCase(sb.group) < 0;
        default: return a < b;
        }
    };
    SortStable(m_order, asc, less);
    BuildNodes();
    SetSortIndicator(col, asc ? +1 : -1);
}

} // namespace demotaskmgr
