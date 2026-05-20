// =============================================================================
// ProcessesPage —— 任务管理器"进程"tab 的内容
//
// 用 DuiTreeView 多列模式表现 Win10 进程页：3 个 root（应用 / 后台进程 /
// Windows 进程）下挂各进程节点；首列名称带展开箭头 + 子进程在父进程下，
// 后续 6 列是状态 / CPU / 内存 / 磁盘 / 网络 / GPU。
//
// 数据源：demotaskmgr::GetProcesses()。Init() 把所有进程节点一次性建好，
// RefreshNumbers() 由 main.cpp 200ms WM_TIMER 调用，只刷新 5 个数值列文
// 字（Status / Name 列保持不变）—— 这避免每帧重建整棵树。
//
// 事件由父 DuiHost / DuiFrameWindow 接住后冒泡 WM_DUI_NOTIFY 给 main.cpp：
//   · DUIN_DBLCLK              — 双击节点 → 弹 mock 属性框
//   · DuiTreeView::DUITVN_RCLICK — 右键节点 → 弹 DuiMenu（结束任务等）
//   · DuiTreeView::DUITVN_BLANK_RCLICK — 表体空白处右键 → 暂忽略
// =============================================================================
#pragma once

#include "../balloonui/Controls/List/DuiTreeView.h"
#include <map>

namespace demotaskmgr {

class ProcessesPage : public balloonwjui::DuiTreeView
{
public:
    // 列顺序，和 Init() 里 AddColumn 调用顺序一致。外部代码可用这些常量
    // 索引列（如 SetCellText(id, ColCpu, ...)），避免硬编 0..6。
    enum Column
    {
        ColName    = 0,    // 进程名（首列：tree column，自带 ▶/折叠箭头）
        ColStatus  = 1,    // "正在运行" / "已挂起" 等
        ColCpu     = 2,    // CPU %
        ColMem     = 3,    // 内存（MB / GB 自适应）
        ColDisk    = 4,    // 磁盘 MB/s
        ColNet     = 5,    // 网络 Mbps
        ColGpu     = 6,    // GPU %
        kColumnCount
    };

    ProcessesPage();

    // 一次性配列定义 + 注入所有节点 + 全展开。MUST 在控件已挂入 host 的
    // 控件树之后调（否则 invalidate 没意义，但 AddColumn / AddRoot 本身不
    // 依赖 host）。约定 main.cpp 在 SetClientContent 之后立即调。
    void Init();

    // 200ms 刷一次：遍历所有 process 节点，把 5 个数值列文字按当前 mock
    // 数据更新。不动 Status / Name 列（它们不抖）。零分配热路径：所有
    // SetCellText 用栈上 TCHAR[] + _stprintf_s。
    void RefreshNumbers();

    // 当前选中节点对应的进程在 demotaskmgr::GetProcesses() 里的索引；
    //   返回 -1 表示无选中、或选中的是 group root（"应用 (12)" 之类的
    //   分组节点本身不对应任何进程）。
    int GetSelectedProcessIndex() const;

    // 表头点击排序。3 个 group root 顺序保持（应用 / 后台 / Windows），只
    // 对每个 root 下的子节点按 col 排序。"应用"组里若某父进程下还有 helper
    // 子进程，helper 跟父进程一起移动（不参与排序，保持原相对顺序）。
    void SortByColumn(int col, bool ascending);

private:
    // 把单个 mock 进程节点加为 parentItemId 的子节点，并把 itemId →
    // procIdx 写进 m_nodeToProc。返回新节点 itemId。
    int AddProcessNode(int parentItemId, int procIdx);

    // 把 procIdx 对应的 5 个数值列文字按 mock 数据写一次。供 Init 和
    // RefreshNumbers 共用。
    void UpdateNumberCells(int itemId, int procIdx);

    // 按当前 m_appTopOrder / m_bgOrder / m_winOrder 重建整棵树（Clear +
    // 三个 root + 各自子节点）。Init 和 SortByColumn 都走这条。
    void BuildTree();

private:
    // 节点 id → 进程数组索引。group root 节点不在表里 —— GetSelected
    // ProcessIndex 借此区分 "选中的是进程" vs "选中的是分组"。
    std::map<int, int> m_nodeToProc;

    // 三个 group 内部的展示顺序。每个元素是 GetProcesses() 的 idx；不含
    // helper 子进程（helper 跟父走，不参与排序）。
    std::vector<int> m_appTopOrder;
    std::vector<int> m_bgOrder;
    std::vector<int> m_winOrder;
};

} // namespace demotaskmgr
