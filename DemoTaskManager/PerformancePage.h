// =============================================================================
// PerformancePage —— 任务管理器"性能"tab 的内容
//
// 结构（DuiSplitter，竖向分隔条）：
//
//   ┌──────────┬─────────────────────────────────┐
//   │ sidebar  │  detail                         │
//   │          │  ┌──── title ─────────────────┐ │
//   │ [✓] CPU  │  │ CPU                        │ │
//   │     内存 │  │ Intel Core i7-8700K @ ...  │ │
//   │     磁盘 │  ├────── line chart ──────────┤ │
//   │     网络 │  │                            │ │
//   │     GPU  │  │       /\___/\___/\         │ │
//   │          │  │  ___/                       │ │
//   │          │  ├──── 4 列 stats ────────────┤ │
//   │          │  │ 利用率 12%   速度 3.7 GHz  │ │
//   │          │  └────────────────────────────┘ │
//   └──────────┴─────────────────────────────────┘
//
// 5 个 sidebar item 对应 CPU / 内存 / 磁盘 / 网络 / GPU。点哪个，detail 切
// 到对应度量的 line chart + 标题 + 副标题 + 4 列 stats。
//
// 折线图（TaskMgrLineChart）和 sidebar item（TaskMgrSidebarItem）都是
// **demo 内自绘**控件，不进 balloonui 库（plan §决策 2）。如果证明可复用
// 再考虑后续 PR 升库为 DuiLineChart / DuiSparklineRow。
//
// 数据驱动：父 frame 200ms WM_TIMER 调 OnTick()。
//   · TaskMgrLineChart 整图 InvalidateRect 自身（不局部刷，简单）。
//   · TaskMgrSidebarItem InvalidateRect 自身。
//   · detail 的 4 列 stats label 更新文字（仅 SetText，不调 Invalidate）。
// =============================================================================
#pragma once

#include "../balloonui/DuiControl.h"
#include "../balloonui/Controls/Layout/DuiSplitter.h"
#include "MockData.h"

namespace balloonwjui {
class DuiLabel;
}

namespace demotaskmgr {

// 5 个度量。和 sidebar item / detail title 一一对应。kMetricCount 既是数
// 组容量也是 enum 枚举次数（switch default 用）。
enum class MetricKind
{
    CPU      = 0,
    Memory   = 1,
    Disk     = 2,
    Network  = 3,
    GPU      = 4,
    kMetricCount = 5
};

class PerformancePage;

// =============================================================================
// TaskMgrLineChart —— 单度量实时折线图（自绘，demo 内）
//
// 用途：性能页 detail 区主体。把一个 60 点 ring buffer 画成 Win10 任务管理
// 器风格的折线 —— 浅灰 10×10 网格、accent 色折线 + 折线下半透明填充、左
// 上角当前值大字、右上 100% 标注。
//
// 工作机制：
//   · 数据源是 const MetricRing*（外部 mock；本控件不持有所有权）。
//   · 量程 m_range：CPU/GPU = 100；磁盘/网络 = 100（MB/s 上限近似）；
//     内存按"可用总内存 GB"。Y 轴始终 [0, m_range]。
//   · OnPaint 遍历 ring 60 个点，X 等距、Y 按量程映射，DuiAA::DrawLine 段
//     段相连画 AA 折线；折线下方用浅 accent 色 FillRect 多个 1px 列模拟
//     "渐淡填充"（GDI 没真透明，演不了真半透明，简化为固定浅色）。
//   · 父调 Invalidate() 触发整图重画。
//
// 代码用法（PerformancePage 内部）：
//
//     auto chart = std::unique_ptr<TaskMgrLineChart>(new TaskMgrLineChart());
//     chart->SetSource(&GetMetrics().cpu, 100.0);
//     chart->SetAccent(RGB(45, 108, 223));
//     parent->AddChild(std::move(chart), DuiLayout::Hint().Weight(1));
//
// 替代关系：N/A（demo 私有控件）。
// =============================================================================
class TaskMgrLineChart : public balloonwjui::DuiControl
{
public:
    // 切数据源 + Y 轴量程。ring 必须在控件生命期内保持有效（业务侧 mock
    // 是文件作用域全局，天然满足）。range <= 0 会被 clamp 到 1。
    void SetSource(const MetricRing* ring, double range);

    // 设置 accent 色（折线 + 渐淡填充）。
    void SetAccent(COLORREF c);

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    const MetricRing* m_ring   = nullptr;
    double            m_range  = 100.0;
    COLORREF          m_accent = RGB(45, 108, 223);
};

// =============================================================================
// TaskMgrSidebarItem —— 性能页 sidebar 的一行（自绘，demo 内）
//
// 高 60px。一行内画：
//   · 选中态：左侧 4px accent 竖条 + 整行浅蓝背景
//   · 上半行：accent 圆点 + 标题（"CPU"）+ 行尾百分比当前值
//   · 下半行：副标题（"Intel Core i7-..."）+ 行尾 mini sparkline
//
// 点击 = 通知父 PerformancePage 切到本 metric。点击事件不走 WM_DUI_NOTIFY
// 冒泡（路径长 + 父 PerformancePage 不是 HWND 拥有者），直接调持有的父指
// 针 m_parent->SelectMetric(m_kind)。父子关系稳定，裸指针安全。
// =============================================================================
class TaskMgrSidebarItem : public balloonwjui::DuiControl
{
public:
    // 一次性配置：父指针 / metric 类别 / ring 数据源 / 量程 / 标题 / 副
    // 标题 / accent 色。所有引用 / 指针在控件生命期内必须保持有效。
    void Configure(PerformancePage* parent,
                   MetricKind        kind,
                   const MetricRing* ring,
                   double            range,
                   LPCTSTR           title,
                   LPCTSTR           subtitle,
                   COLORREF          accent);

    // 选中态（左侧条 + 浅蓝底 + 标题加粗）。Invalidate 自动跟。
    void SetSelected(bool b);
    bool IsSelected() const { return m_selected; }

    MetricKind GetKind() const { return m_kind; }

    // ---- DuiControl 覆写 ----
    bool OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool OnSetCursor  (POINT pt) override;
    void OnPaint      (HDC hdc, const RECT& rcDirty) override;

private:
    PerformancePage*  m_parent   = nullptr;
    MetricKind        m_kind     = MetricKind::CPU;
    const MetricRing* m_ring     = nullptr;
    double            m_range    = 100.0;
    CString           m_title;
    CString           m_subtitle;
    COLORREF          m_accent   = RGB(45, 108, 223);
    bool              m_selected = false;
};

// =============================================================================
// PerformancePage —— "性能"tab 主控件
//
// 派生 DuiSplitter（vertical），pane 0 = sidebar，pane 1 = detail。
// 业务由 main.cpp 注入：构造后立即 Init()，再交给 DuiTabPage 持有。
// 200ms WM_TIMER 由 main.cpp 调 OnTick() 驱动。
// =============================================================================
class PerformancePage : public balloonwjui::DuiSplitter
{
public:
    PerformancePage();

    // 配出 splitter 的两个 pane + 5 个 sidebar item + chart + stats。
    // 默认选中 CPU。
    void Init();

    // 外部 200ms 调一次。重画 chart + 当前选中 sidebar item + 重算 detail
    // stats label 文字。其余 4 个未选中的 sidebar item 也重画（曲线和当前
    // 值在动）。
    void OnTick();

    // 切到指定 metric。被某个 TaskMgrSidebarItem 的 OnLButtonDown 调用。
    void SelectMetric(MetricKind k);

    // 重写：先把 detail 区（pane 1）填白底再让 splitter / 子控件叠画上去。
    // detail 区视觉上"分层"——sidebar 浅灰、detail 白底——更接近 Win10
    // 真任务管理器的"性能"页布局。
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    // 把 detail 区的 title / subtitle / chart 数据源 / accent 切到指定
    // metric。stats label 由 RefreshDetailStats() 单独负责，此函数也调
    // 一次它（首屏）。
    void ApplyDetailFor(MetricKind k);

    // 按 m_selected 的 metric，把 4 个 stats label 文字更新一次。
    void RefreshDetailStats();

    // metric 元数据 helper（PerformancePage.cpp 内部 const 表）：标题 /
    // 副标题 / accent / 量程 / 取 ring 指针。
    struct MetricInfo
    {
        LPCTSTR  title;
        LPCTSTR  subtitle;
        COLORREF accent;
        double   range;
        const MetricRing& (*getRing)(const Metrics& m);
    };
    static const MetricInfo& GetInfo(MetricKind k);

private:
    TaskMgrSidebarItem*    m_items[(int)MetricKind::kMetricCount] = {};
    TaskMgrLineChart*      m_chart        = nullptr;
    balloonwjui::DuiLabel* m_lblTitle     = nullptr;
    balloonwjui::DuiLabel* m_lblSubtitle  = nullptr;
    balloonwjui::DuiLabel* m_lblStats[4]  = {};
    MetricKind             m_selected     = MetricKind::CPU;
};

} // namespace demotaskmgr
