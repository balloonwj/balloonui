// =============================================================================
// XmlFactory.cpp
// =============================================================================
#include "stdafx.h"
#include "XmlFactory.h"
#include "ProcessesPage.h"
#include "PerformancePage.h"
#include "OtherPages.h"

#include "../balloonui/Controls/Basic/DuiLabel.h"
#include "../balloonui/Controls/List/DuiTabPage.h"

namespace demotaskmgr {

using balloonwjui::DuiControl;
using balloonwjui::DuiLabel;
using balloonwjui::DuiTabPage;
using balloonwjui::DuiXmlBuilder;

// =============================================================================
// 字符串 → PageTag
// =============================================================================

PageTag PageTagFromString(const std::string& s)
{
    if (s == "processes")   { return PageTag::Processes;   }
    if (s == "performance") { return PageTag::Performance; }
    if (s == "appHistory")  { return PageTag::AppHistory;  }
    if (s == "startup")     { return PageTag::Startup;     }
    if (s == "users")       { return PageTag::Users;       }
    if (s == "details")     { return PageTag::Details;     }
    if (s == "services")    { return PageTag::Services;    }
    return PageTag::Unknown;
}

// =============================================================================
// 内部工具
// =============================================================================

namespace {

// UTF-8 std::string → CString（项目宽字符）。和 NewChatDemo 同模式。
CString ToCString_(const std::string& utf8)
{
    if (utf8.empty())
    {
        return CString();
    }
    int cw = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (cw <= 0)
    {
        return CString();
    }
    std::wstring w(cw, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], cw);
    while (!w.empty() && w.back() == 0)
    {
        w.pop_back();
    }
    return CString(w.c_str());
}

std::string GetAttr8(const DuiXmlBuilder::Node& n, const char* key)
{
    auto it = n.attrs.find(key);
    if (it == n.attrs.end())
    {
        return std::string();
    }
    return it->second;
}

// 还没接的 tab 占位：显示 "TODO: <page-tag>"。后续阶段把对应分支替换为
// 真正的 page 实现。
std::unique_ptr<DuiControl> MakePagePlaceholder(const std::string& pageTag)
{
    auto lbl = std::unique_ptr<DuiLabel>(new DuiLabel());
    CString text;
    text.Format(_T("TODO: %s"),
                ToCString_(pageTag).GetString());
    lbl->SetText(text);
    lbl->SetTextColor(RGB(120, 120, 130));
    return lbl;
}

// 按 page-tag 选 page 类型。已实现的返回真实 page；未实现的回 fallback
// 占位。outs 的相关字段在这里写（指针带回 main.cpp）。
std::unique_ptr<DuiControl> MakePageByTag(const std::string& pageTag,
                                          TaskMgrXmlOuts* outs)
{
    if (pageTag == "processes")
    {
        auto page = std::unique_ptr<ProcessesPage>(new ProcessesPage());
        if (outs)
        {
            outs->processesPage = page.get();
        }
        // Init() 加列 + 加节点。在控件挂入 host 树之前调没问题（API 不依
        // 赖 host），让 page 一就位就有内容。
        page->Init();
        return page;
    }
    if (pageTag == "performance")
    {
        auto page = std::unique_ptr<PerformancePage>(new PerformancePage());
        if (outs)
        {
            outs->performancePage = page.get();
        }
        page->Init();
        return page;
    }
    // 其余 5 个 tab：纯 DuiTreeView 子类。指针通过 outs 带回，main.cpp 用
    // 来路由 DUITVN_COLUMNCLICK 到各 page 的 SortByColumn。
    if (pageTag == "appHistory")
    {
        auto page = std::unique_ptr<AppHistoryPage>(new AppHistoryPage());
        if (outs) { outs->appHistoryPage = page.get(); }
        page->Init();
        return page;
    }
    if (pageTag == "startup")
    {
        auto page = std::unique_ptr<StartupPage>(new StartupPage());
        if (outs) { outs->startupPage = page.get(); }
        page->Init();
        return page;
    }
    if (pageTag == "users")
    {
        auto page = std::unique_ptr<UsersPage>(new UsersPage());
        if (outs) { outs->usersPage = page.get(); }
        page->Init();
        return page;
    }
    if (pageTag == "details")
    {
        auto page = std::unique_ptr<DetailsPage>(new DetailsPage());
        if (outs) { outs->detailsPage = page.get(); }
        page->Init();
        return page;
    }
    if (pageTag == "services")
    {
        auto page = std::unique_ptr<ServicesPage>(new ServicesPage());
        if (outs) { outs->servicesPage = page.get(); }
        page->Init();
        return page;
    }
    return MakePagePlaceholder(pageTag);
}

// <tab-page> 工厂分支。同步消费所有 <tab-page-item> 子节点，把每个 item
// 的 title + page-tag 转成一个 page 加进 DuiTabPage。kernel 之后还会
// recurse 子节点（独立调一次 BuildOne），那条路径走 <tab-page-item>
// 工厂分支，返回 0 尺寸占位（见下）。
std::unique_ptr<DuiControl> BuildTabPage(const DuiXmlBuilder::Node& n,
                                         TaskMgrXmlOuts* outs)
{
    auto page = std::unique_ptr<DuiTabPage>(new DuiTabPage());
    DuiTabPage* raw = page.get();

    for (const auto& child : n.children)
    {
        if (child.tag != "tab-page-item")
        {
            continue;
        }
        std::string pageTag = GetAttr8(child, "page-tag");
        CString     title   = ToCString_(GetAttr8(child, "title"));
        raw->AddPage(title, MakePageByTag(pageTag, outs));
    }

    if (raw->GetPageCount() > 0)
    {
        raw->SetCurSel(0, false);
    }
    if (outs)
    {
        outs->tabPage = raw;
    }
    return page;
}

// <tab-page-item> 工厂分支。kernel 已经在 <tab-page> 工厂里消费过这些
// 子节点；这里返回一个无害的 0 尺寸占位 control，纯粹为了不触发 kernel
// 的 "unknown tag" OutputDebugString 警告。占位不参与 layout（被父
// DuiTabPage 当作 m_children 之一，但 LayoutContent 只走 m_pages，所以
// 占位永远不会拿到非空 m_rcItem）。
std::unique_ptr<DuiControl> BuildTabPageItemPlaceholder(const DuiXmlBuilder::Node&)
{
    return std::unique_ptr<DuiControl>(new DuiControl());
}

// <status-label which="proc|cpu|mem" text="..."> 工厂分支。本质是一个加了
// "指针带回"副作用的 <label>：构造 DuiLabel + 应用初始文字 + 写指针到 outs
// 的对应槽位。layout hint（fixedWidth 等）由 kernel 的 MakeHint 处理；id
// 由 ApplyCommon 处理。
std::unique_ptr<DuiControl> BuildStatusLabel(const DuiXmlBuilder::Node& n,
                                             TaskMgrXmlOuts* outs)
{
    auto lbl = std::unique_ptr<DuiLabel>(new DuiLabel());
    lbl->SetTextColor(RGB(60, 60, 70));

    std::string text = GetAttr8(n, "text");
    if (!text.empty())
    {
        lbl->SetText(ToCString_(text));
    }

    if (outs)
    {
        std::string which = GetAttr8(n, "which");
        if (which == "proc")      { outs->lblProc = lbl.get(); }
        else if (which == "cpu")  { outs->lblCpu  = lbl.get(); }
        else if (which == "mem")  { outs->lblMem  = lbl.get(); }
    }
    return lbl;
}

} // anonymous

// =============================================================================
// 工厂入口
// =============================================================================

DuiXmlBuilder::CustomFactory MakeTaskMgrFactory(TaskMgrXmlOuts* outs)
{
    return [outs](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
    {
        if (n.tag == "tab-page")
        {
            return BuildTabPage(n, outs);
        }
        if (n.tag == "tab-page-item")
        {
            return BuildTabPageItemPlaceholder(n);
        }
        if (n.tag == "status-label")
        {
            return BuildStatusLabel(n, outs);
        }
        return nullptr;
    };
}

} // namespace demotaskmgr
