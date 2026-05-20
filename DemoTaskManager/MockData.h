// =============================================================================
// MockData —— DemoTaskManager 的全部 mock 数据源
//
// 本 demo 不抓真实系统调用（GetSystemTimes / EnumProcesses / Service Control
// Manager …），所有 5 个 tab 显示的内容由本文件按固定 seed 生成。WM_TIMER
// 每 200ms 调一次 Tick()：
//   · 各 Process / Service 的 cpu / mem / disk / net / gpu 数值随机抖动
//   · Metrics 的 5 个 ring buffer 各 push 一个新采样点（最近 60 个保留）
//   · cpuTotalPct / memUsedGB / procCount 重新汇总
//
// 业务侧只读消费：每页绑定到自己的 view，定时检查脏标记 / 直接重画。
// =============================================================================
#pragma once

#include "stdafx.h"

namespace demotaskmgr {

// -----------------------------------------------------------------------------
// 进程归类。Win10 任务管理器进程页头部按这个分三组：应用 / 后台进程 / Windows 进程。
// -----------------------------------------------------------------------------
enum class ProcessKind
{
    App        = 0,
    Background = 1,
    Windows    = 2,
};

// -----------------------------------------------------------------------------
// 一条进程记录。父子关系靠 parentIndex（-1 表示顶层；非负 = 在 g_processes
// 里指向父）。这样应用页可以做"应用 → 子进程"两层 treeview。
// -----------------------------------------------------------------------------
struct Process
{
    int          pid          = 0;
    int          parentIndex  = -1;
    CString      name;                         // 进程显示名
    CString      user;                         // 用户名（用户页用）
    ProcessKind  kind         = ProcessKind::Background;
    CString      status;                       // "正在运行" / "已挂起" / 空
    double       cpuPct       = 0.0;           // 0..100
    double       memMB        = 0.0;           // 内存（MB）
    double       diskMBs      = 0.0;           // 磁盘 MB/s
    double       netMbps      = 0.0;           // 网络 Mbps
    double       gpuPct       = 0.0;           // GPU 占用
};

// -----------------------------------------------------------------------------
// 启动项（启动 tab）。impact 0..3 = 无 / 低 / 中 / 高。
// -----------------------------------------------------------------------------
struct StartupItem
{
    CString name;
    CString publisher;
    bool    enabled = true;
    int     impact  = 1;   // 0=无 1=低 2=中 3=高
};

// -----------------------------------------------------------------------------
// 服务（服务 tab）。pid == 0 表示当前未运行。
// -----------------------------------------------------------------------------
struct Service
{
    CString name;
    int     pid = 0;
    CString description;
    CString status;       // "正在运行" / "已停止"
    CString group;
};

// -----------------------------------------------------------------------------
// 用户（用户 tab）。processIndices 直接列出该用户拥有的 g_processes 索引。
// -----------------------------------------------------------------------------
struct User
{
    CString          name;
    int              sessionId = 0;
    std::vector<int> processIndices;
};

// -----------------------------------------------------------------------------
// 60 点环形缓冲。性能页折线图 / sidebar sparkline 共用。
//
// 约定：At(0) = 最旧采样点，At(Size()-1) = 最新。Push 一次 = 队尾追加 +
// 满 60 时队头自动淘汰。绘图侧从左到右画 At(0) → At(Size()-1) 即可。
// -----------------------------------------------------------------------------
class MetricRing
{
public:
    static const int kCapacity = 60;

    void   Push(double v);
    int    Size()     const { return m_count; }
    int    Capacity() const { return kCapacity; }
    double At(int i)  const;          // i 越界返回 0
    double Latest()   const;          // 空时返回 0

private:
    double m_buf[kCapacity] = {};
    int    m_head           = 0;     // 下一个要写入的位置
    int    m_count          = 0;     // 已写入数量，封顶 = kCapacity
};

// -----------------------------------------------------------------------------
// 整机 metrics（性能页 + 状态栏共用）。
// -----------------------------------------------------------------------------
struct Metrics
{
    MetricRing cpu;
    MetricRing mem;
    MetricRing disk;
    MetricRing net;
    MetricRing gpu;

    double cpuTotalPct = 0.0;        // 当前整机 CPU 使用率（与 cpu.Latest() 同步）
    double memUsedGB   = 0.0;        // 内存使用量
    double memTotalGB  = 16.0;       // 内存总量（固定）
    int    procCount   = 0;          // = g_processes.size()
};

// -----------------------------------------------------------------------------
// 全局生命周期。Init() 只调一次（固定 seed，结果可复现）；Tick() 由 WM_TIMER
// 200ms 调一次，用来推进折线图、抖动各进程数值。
// -----------------------------------------------------------------------------
void Init();
void Tick();

// 只读访问。返回的引用在下一次 Tick 期间稳定（不会 reallocate）。
const std::vector<Process>&     GetProcesses();
const std::vector<StartupItem>& GetStartupItems();
const std::vector<Service>&     GetServices();
const std::vector<User>&        GetUsers();
const Metrics&                  GetMetrics();

// -----------------------------------------------------------------------------
// 简单"字母 logo"位图生成器：实色背景 + 居中粗体字母。用于给 DuiTreeView
// 首列 SetItemIcon、给主 frame WM_SETICON 提供占位图标 —— 避免引入真实
// .ico 资源文件（demo 范围控制）。
//
// caller 不持有所有权语义：返回的 HBITMAP 通常 file-static 缓存到进程结
// 束，OS 自动回收 GDI handle；如果 caller 真的要释放可以自行 DeleteObject。
// -----------------------------------------------------------------------------
HBITMAP MakeLetterBitmap(int sz, COLORREF bg, TCHAR letter, COLORREF fg);

// 同上，但产物是 HICON（带 mask，可投到 WM_SETICON）。内部走 ICONINFO +
// CreateIconIndirect。
HICON   MakeLetterIcon  (int sz, COLORREF bg, TCHAR letter, COLORREF fg);

} // namespace demotaskmgr
