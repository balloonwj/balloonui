// =============================================================================
// XmlFactory —— DuiXmlBuilder 的 CustomFactory 入口
//
// 接住 taskmgr.xml 里三类自定义 tag：
//
//   <tab-page>       构造 DuiTabPage，并在工厂内即把所有 <tab-page-item>
//                    子节点用 AddPage 加上（暂用 DuiLabel("TODO: <page-tag>")
//                    占位，后续阶段按 page-tag 替换为各自的 page 内容）。
//                    构造好的 DuiTabPage* 写到传入的 outs->tabPage，main.cpp
//                    后续阶段用它做 SetPage 替换 / 切页。
//
//   <tab-page-item>  返回一个 0 尺寸不可见的 DuiControl 占位。理由：kernel
//                    在 factory 返回非空 control 后会自动 recurse 子节点
//                    并 AddChild，所以即便 <tab-page> 自身已经把这些 item
//                    全消费掉、kernel 还会再把它们当独立子节点构造一次；
//                    返回 nullptr 会触发 OutputDebugString 警告，返回一个
//                    无害的占位最干净。占位无 Layout 不画不接事件。
//
//   <status-label>   状态栏的 3 个数字 label。属性 which="proc|cpu|mem"
//                    决定写到 outs 的哪个槽位；text 是初始文字（业务侧
//                    WM_TIMER 200ms 会重写）。本质是一个加了"指针带回"
//                    side effect 的 <label>。
//
// 用法（main.cpp）：
//
//     demotaskmgr::TaskMgrXmlOuts outs;
//     auto factory = demotaskmgr::MakeTaskMgrFactory(&outs);
//     balloonwjui::DuiFrameWindowConfig cfg;
//     auto root = balloonwjui::DuiXmlBuilder::FromFrameXml(
//         xmlUtf8.c_str(), cfg, factory);
//     frame.ApplyConfig(cfg);
//     frame.SetClientContent(std::move(root));
//     // 之后 outs.tabPage / outs.lblProc / lblCpu / lblMem 可用。
// =============================================================================
#pragma once

#include "../balloonui/DuiXmlBuilder.h"

// 前置声明，避免在 header 里拖入完整定义
namespace balloonwjui {
class DuiTabPage;
class DuiLabel;
}

namespace demotaskmgr {

class ProcessesPage;
class PerformancePage;
class AppHistoryPage;
class StartupPage;
class UsersPage;
class DetailsPage;
class ServicesPage;

// 7 个 tab 的 page tag。和 taskmgr.xml 的 page-tag 属性值一一对应。
enum class PageTag
{
    Unknown     = 0,
    Processes,
    Performance,
    AppHistory,
    Startup,
    Users,
    Details,
    Services,
};

PageTag PageTagFromString(const std::string& s);

// 工厂构建过程中"带回"的关键控件指针。owner 是 main.cpp（栈上变量），
// factory 只是写指针不持有。所有指针的生命期跟 frame 客户区子树一致。
struct TaskMgrXmlOuts
{
    balloonwjui::DuiTabPage* tabPage         = nullptr;
    balloonwjui::DuiLabel*   lblProc         = nullptr;
    balloonwjui::DuiLabel*   lblCpu          = nullptr;
    balloonwjui::DuiLabel*   lblMem          = nullptr;
    ProcessesPage*           processesPage   = nullptr;
    PerformancePage*         performancePage = nullptr;
    AppHistoryPage*          appHistoryPage  = nullptr;
    StartupPage*             startupPage     = nullptr;
    UsersPage*               usersPage       = nullptr;
    DetailsPage*             detailsPage     = nullptr;
    ServicesPage*            servicesPage    = nullptr;
};

// 创建 CustomFactory。outs 可为 nullptr（不需要带回时）。
balloonwjui::DuiXmlBuilder::CustomFactory MakeTaskMgrFactory(TaskMgrXmlOuts* outs);

} // namespace demotaskmgr
