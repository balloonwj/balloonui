// =============================================================================
// OtherPages —— 任务管理器除"进程 / 性能"外的 5 个 tab
//
// 每个 page 都是 DuiTreeView 多列模式，单层（用户页除外，user → 进程两层）。
// 数据全部来自 demotaskmgr 的静态 mock，Init() 一次性注入。
//
//   AppHistoryPage   应用历史记录 (5 列)
//   StartupPage      启动           (4 列，状态列 CHECKBOX cell)
//   UsersPage        用户           (2 列 / 两层 user → 该用户进程)
//   DetailsPage      详细信息       (14 列，首列冻结)
//   ServicesPage     服务           (6 列)
//
// 每个 page 都暴露 SortByColumn(col, asc)：业务侧维护 m_order（数据 idx 顺
// 序数组），SortByColumn 只重排这个数组并 Clear() + 按新顺序重建节点。
// main.cpp 接 DUITVN_COLUMNCLICK 后路由到对应 page。
// =============================================================================
#pragma once

#include "../balloonui/Controls/List/DuiTreeView.h"

namespace demotaskmgr {

// ---- AppHistoryPage —— 应用历史记录 ----
class AppHistoryPage : public balloonwjui::DuiTreeView
{
public:
    void Init();
    void SortByColumn(int col, bool ascending);
private:
    void AddColumnsOnce();
    void BuildNodes();
    std::vector<int> m_order;     // m_order[i] = 第 i 行展示的 process idx
};

// ---- StartupPage ----
class StartupPage : public balloonwjui::DuiTreeView
{
public:
    void Init();
    void SortByColumn(int col, bool ascending);
private:
    void AddColumnsOnce();
    void BuildNodes();
    std::vector<int> m_order;     // m_order[i] = 第 i 行的 startup item idx
};

// ---- UsersPage ----
// 两层结构：user → 该用户进程。SortByColumn 只对每个 user root 内的子节点
// 排序（root 顺序保持 [balloonwj, admin]）。
class UsersPage : public balloonwjui::DuiTreeView
{
public:
    void Init();
    void SortByColumn(int col, bool ascending);
private:
    void AddColumnsOnce();
    void BuildNodes();
    // 每个 user root 下的 process idx 顺序（外层 vector 一项 = 一个 user）
    std::vector<std::vector<int>> m_userChildOrder;
};

// ---- DetailsPage ----
// 14 列、首列冻结。SortByColumn 改 m_order 后整表重建。
class DetailsPage : public balloonwjui::DuiTreeView
{
public:
    void Init();
    void SortByColumn(int col, bool ascending);
private:
    void AddColumnsOnce();
    void BuildNodes();
    std::vector<int> m_order;     // m_order[i] = 第 i 行的 process idx
};

// ---- ServicesPage ----
class ServicesPage : public balloonwjui::DuiTreeView
{
public:
    void Init();
    void SortByColumn(int col, bool ascending);
private:
    void AddColumnsOnce();
    void BuildNodes();
    std::vector<int> m_order;     // m_order[i] = 第 i 行的 service idx
};

} // namespace demotaskmgr
