// =============================================================================
// ProcessesPage.cpp
// =============================================================================
#include "stdafx.h"
#include "ProcessesPage.h"
#include "MockData.h"
#include <algorithm>

namespace demotaskmgr {

using balloonwjui::DuiTreeView;

// =============================================================================
// 列宽。Win10 任务管理器进程页在 1080p 1080×720 窗口下大约就是这套比例。
// =============================================================================

static const int kColWidthName   = 240;
static const int kColWidthStatus = 80;
static const int kColWidthCpu    = 70;
static const int kColWidthMem    = 90;
static const int kColWidthDisk   = 90;
static const int kColWidthNet    = 90;
static const int kColWidthGpu    = 70;

// =============================================================================
// 进程图标：3 类（应用/后台/Windows）按 ProcessKind 区分。颜色与 menu bar
// accent 一致让整 demo 视觉协调。Lazy init：第一次 EnsureIcons() 时建好，
// file-static 长寿，进程退出 OS 自动回收 GDI handle。
// =============================================================================

static const int      kIconPx          = 16;
static const COLORREF kIconBgApp       = RGB( 45, 108, 223);    // #2D6CDF
static const COLORREF kIconBgBackground= RGB(110, 110, 120);    // 中灰
static const COLORREF kIconBgWindows   = RGB( 70,  70,  80);    // 深灰
static const COLORREF kIconFg          = RGB(255, 255, 255);

static HBITMAP s_iconApp = NULL;
static HBITMAP s_iconBg  = NULL;
static HBITMAP s_iconWin = NULL;
static bool    s_iconsInited = false;

static void EnsureIcons()
{
    if (s_iconsInited) { return; }
    s_iconsInited = true;
    s_iconApp = MakeLetterBitmap(kIconPx, kIconBgApp,        _T('A'), kIconFg);
    s_iconBg  = MakeLetterBitmap(kIconPx, kIconBgBackground, _T('B'), kIconFg);
    s_iconWin = MakeLetterBitmap(kIconPx, kIconBgWindows,    _T('W'), kIconFg);
}

static HBITMAP IconForKind(ProcessKind k)
{
    switch (k)
    {
    case ProcessKind::App:        return s_iconApp;
    case ProcessKind::Background: return s_iconBg;
    case ProcessKind::Windows:    return s_iconWin;
    default:                      return NULL;
    }
}

// =============================================================================
// ctor
// =============================================================================

ProcessesPage::ProcessesPage()
{
    // 默认只读：双击 Text cell 不进 in-place EDIT，给业务侧的 DUIN_DBLCLK
    // 用（弹"属性"框）。库默认 Editable = false，这里显式写出来强调意图。
    SetEditable(false);
}

// =============================================================================
// Init —— 配列 + 收集初始顺序 + 构建树
// =============================================================================

void ProcessesPage::Init()
{
    EnsureIcons();

    // ---- 7 列定义。首列默认就是 tree column（带展开箭头）。----
    AddColumn(_T("名称"),  kColWidthName,   60,  DT_LEFT,    true);
    AddColumn(_T("状态"),  kColWidthStatus, 40,  DT_LEFT,    true);
    AddColumn(_T("CPU"),   kColWidthCpu,    40,  DT_RIGHT,   true);
    AddColumn(_T("内存"),  kColWidthMem,    50,  DT_RIGHT,   true);
    AddColumn(_T("磁盘"),  kColWidthDisk,   50,  DT_RIGHT,   true);
    AddColumn(_T("网络"),  kColWidthNet,    50,  DT_RIGHT,   true);
    AddColumn(_T("GPU"),   kColWidthGpu,    40,  DT_RIGHT,   true);

    // ---- 三个分组按 ProcessKind 收集索引 ----
    const auto& procs = GetProcesses();
    m_appTopOrder.clear();
    m_bgOrder.clear();
    m_winOrder.clear();
    for (size_t i = 0; i < procs.size(); ++i)
    {
        const auto& p = procs[i];
        switch (p.kind)
        {
        case ProcessKind::App:
            if (p.parentIndex < 0)
            {
                m_appTopOrder.push_back((int)i);
            }
            // App 子进程（parentIndex >= 0）由父节点遍历时挂上去，不参与
            // 排序——不进 m_appTopOrder。
            break;
        case ProcessKind::Background:
            m_bgOrder.push_back((int)i);
            break;
        case ProcessKind::Windows:
            m_winOrder.push_back((int)i);
            break;
        }
    }

    BuildTree();
}

// =============================================================================
// BuildTree —— 按 m_appTopOrder/m_bgOrder/m_winOrder 当前顺序整树重建
// =============================================================================

void ProcessesPage::BuildTree()
{
    Clear();
    m_nodeToProc.clear();
    const auto& procs = GetProcesses();

    // ---- root: 应用 (N) ----
    {
        TCHAR rootText[64];
        _stprintf_s(rootText, _T("应用 (%d)"), (int)m_appTopOrder.size());
        int rootId = AddRoot(rootText);
        for (int parentProcIdx : m_appTopOrder)
        {
            int parentItemId = AddProcessNode(rootId, parentProcIdx);
            // helper 子进程跟父走，按 procs 数组里相对顺序原样挂（不排序）
            for (size_t j = 0; j < procs.size(); ++j)
            {
                if (procs[j].parentIndex == parentProcIdx)
                {
                    AddProcessNode(parentItemId, (int)j);
                }
            }
        }
    }

    // ---- root: 后台进程 (N) ----
    {
        TCHAR rootText[64];
        _stprintf_s(rootText, _T("后台进程 (%d)"), (int)m_bgOrder.size());
        int rootId = AddRoot(rootText);
        for (int procIdx : m_bgOrder)
        {
            AddProcessNode(rootId, procIdx);
        }
    }

    // ---- root: Windows 进程 (N) ----
    {
        TCHAR rootText[64];
        _stprintf_s(rootText, _T("Windows 进程 (%d)"), (int)m_winOrder.size());
        int rootId = AddRoot(rootText);
        for (int procIdx : m_winOrder)
        {
            AddProcessNode(rootId, procIdx);
        }
    }

    ExpandAll();
}

// =============================================================================
// AddProcessNode
// =============================================================================

int ProcessesPage::AddProcessNode(int parentItemId, int procIdx)
{
    const auto& procs = GetProcesses();
    const Process& p = procs[procIdx];

    int itemId = AddChild(parentItemId, p.name);
    m_nodeToProc[itemId] = procIdx;

    // 首列 tree icon —— 按 kind 选 3 类位图之一。SetItemIcon 不接管所有
    // 权（库内部存指针），因此 file-static 缓存的 HBITMAP 直接传即可。
    HBITMAP icon = IconForKind(p.kind);
    if (icon)
    {
        SetItemIcon(itemId, icon);
    }

    SetCellText(itemId, ColStatus, p.status);

    UpdateNumberCells(itemId, procIdx);
    return itemId;
}

// =============================================================================
// UpdateNumberCells
// =============================================================================

void ProcessesPage::UpdateNumberCells(int itemId, int procIdx)
{
    const auto& procs = GetProcesses();
    const Process& p = procs[procIdx];

    TCHAR buf[32];

    _stprintf_s(buf, _T("%.1f%%"), p.cpuPct);
    SetCellText(itemId, ColCpu, buf);

    if (p.memMB < 1024.0)
    {
        _stprintf_s(buf, _T("%.1f MB"), p.memMB);
    }
    else
    {
        _stprintf_s(buf, _T("%.2f GB"), p.memMB / 1024.0);
    }
    SetCellText(itemId, ColMem, buf);

    _stprintf_s(buf, _T("%.1f MB/秒"), p.diskMBs);
    SetCellText(itemId, ColDisk, buf);

    _stprintf_s(buf, _T("%.1f Mbps"), p.netMbps);
    SetCellText(itemId, ColNet, buf);

    _stprintf_s(buf, _T("%.1f%%"), p.gpuPct);
    SetCellText(itemId, ColGpu, buf);
}

// =============================================================================
// RefreshNumbers
// =============================================================================

void ProcessesPage::RefreshNumbers()
{
    for (const auto& kv : m_nodeToProc)
    {
        UpdateNumberCells(kv.first, kv.second);
    }
}

// =============================================================================
// GetSelectedProcessIndex
// =============================================================================

int ProcessesPage::GetSelectedProcessIndex() const
{
    int sel = GetCurSel();
    if (sel < 0)
    {
        return -1;
    }
    auto it = m_nodeToProc.find(sel);
    if (it == m_nodeToProc.end())
    {
        return -1;       // 选中的是 group root
    }
    return it->second;
}

// =============================================================================
// SortByColumn —— 表头列名单击触发
// =============================================================================

void ProcessesPage::SortByColumn(int col, bool ascending)
{
    const auto& procs = GetProcesses();
    auto less = [&procs, col](int a, int b) -> bool
    {
        const Process& pa = procs[a];
        const Process& pb = procs[b];
        switch (col)
        {
        case ColName:   return pa.name.CompareNoCase(pb.name) < 0;
        case ColStatus: return pa.status.CompareNoCase(pb.status) < 0;
        case ColCpu:    return pa.cpuPct  < pb.cpuPct;
        case ColMem:    return pa.memMB   < pb.memMB;
        case ColDisk:   return pa.diskMBs < pb.diskMBs;
        case ColNet:    return pa.netMbps < pb.netMbps;
        case ColGpu:    return pa.gpuPct  < pb.gpuPct;
        default:        return a < b;
        }
    };

    auto sortOne = [&](std::vector<int>& v)
    {
        if (ascending)
        {
            std::stable_sort(v.begin(), v.end(), less);
        }
        else
        {
            std::stable_sort(v.begin(), v.end(),
                             [&less](int a, int b) { return less(b, a); });
        }
    };
    sortOne(m_appTopOrder);
    sortOne(m_bgOrder);
    sortOne(m_winOrder);

    BuildTree();
    SetSortIndicator(col, ascending ? +1 : -1);
}

} // namespace demotaskmgr
