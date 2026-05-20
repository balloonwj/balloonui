#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_MENUBAR

#include "../../DuiControl.h"
#include "DuiMenu.h"

namespace balloonwjui {

// =================================================================
// DuiMenuBar —— 常驻菜单条（host 客户区里的横向 File / Edit / View 那条）
// =================================================================
//
// 用途：桌面应用主窗口顶部的菜单条。由若干**栏目**组成，每个栏目关联
// 一个 DuiMenu 作为下拉。点击 / Alt-激活 时本控件调 DuiMenu::TrackPopupEx
// 弹下拉，下拉关闭后本控件回到 normal 态。
//
// 视觉风格（对标 Win10 任务管理器顶栏）：
//   · 透明底；普通态文字黑、灰禁用、hover 一格 #E5E5E5、active（下拉打
//     开中）一格 #D7E3F4 浅蓝。
//   · 默认行高 24px；栏目宽度 = 文字宽 + 左右 8px padding（不固定栏宽）。
//   · 默认字体走 DuiResMgr::GetDefaultFont()（微软雅黑 9pt GB2312）。
//   · 助记符（"文件(&F)" 中的 F）下划线**仅 Alt 激活态显示**。
//
// 工作机制：
//   · 类本身只画**栏目标题**，不画下拉项；点 / 键盘激活时调用栏目关
//     联的 DuiMenu::TrackPopupEx 把下拉弹出来。
//   · 激活态下鼠标移到隔壁栏目时自动切换：依靠 DuiMenu::SetSwitchZones
//     把"其它栏目矩形"作为 zone 注册给 popup，popup 检测到鼠标进入
//     zone 就关闭并报告 zone idx，本控件取到 idx 立即在新栏弹下拉 ——
//     一个本地 while 循环驱动这个"切换链"，跟 DuiMenu 一样同步阻塞。
//   · DuiMenu 的 lifetime 由 caller（业务侧）持有；本控件只借用其指针，
//     在 TrackPopupEx 调用栈期间必须保活（通常和 DuiMenuBar 一样长）。
//   · 整体不是 tab stop（Alt 激活由 host 转发 WM_SYSKEYDOWN）。
//
// 代码用法：
//
//     auto bar = std::make_unique<DuiMenuBar>();
//     bar->SetCtrlId(IDC_MENUBAR);
//     bar->AppendItem(IDM_FILE,    _T("文件(&F)"), &m_fileMenu);
//     bar->AppendItem(IDM_OPTIONS, _T("选项(&O)"), &m_optionsMenu);
//     bar->AppendItem(IDM_VIEW,    _T("查看(&V)"), &m_viewMenu);
//     parent->AddChild(std::move(bar), DuiLayout::Hint().Fixed(24));
//     // 父 OnDuiNotify:
//     //   case DUIN_VALUECHANGED:                  // 菜单某项被选中
//     //       HandleMenuChosen((UINT)n.extra);     // extra = chosen id
//     //       break;
//     //   case DUIMBN_DROPDOWN_OPEN:               // (调试 / 埋点用)
//     //       Log("dropdown opened on bar idx %d", (int)n.extra);
//     //       break;
//
// XML 用法：
//
//     <menu-bar id="100" item-height="24">
//       <menu-item id="101" text="文件(&amp;F)"/>
//       <menu-item id="102" text="选项(&amp;O)"/>
//       <menu-item id="103" text="查看(&amp;V)"/>
//     </menu-bar>
//
// XML 不支持声明菜单内容（DuiMenu 是事件触发的瞬时弹层，跟"声明静态
// 树"不契合 —— 见 DuiMenu.h 注释）。业务侧 C++ 自己 new + AppendItem
// 配 DuiMenu 实例，再调 bar->SetDropdown(idx, &menu) 把它绑到对应栏目。
//
// 事件（ctrlId = DuiMenuBar id）：
//   · DUIN_VALUECHANGED       — 用户在某下拉里选中了一项。
//                              extra = 被选项的菜单 id（DuiMenu::AppendItem
//                              的 nID，与 TrackPopupEx 的 chosenId 一致）。
//                              dismissed（用户点空白 / Esc）<u>不</u>发本事件。
//   · DUIMBN_DROPDOWN_OPEN    — 即将弹某栏的下拉；extra = 栏目 index。
//   · DUIMBN_DROPDOWN_CLOSE   — 下拉刚关闭（无论是否有选中、是否切换中）；
//                              extra = 栏目 index。
//
// 替代关系：CSkinMenuBar（如果有的话；旧业务通常用 hbox + button 拼）。
class BUI_API DuiMenuBar : public DuiControl
{
public:
    enum NotifyCode
    {
        DUIMBN_DROPDOWN_OPEN  = DUIN_CUSTOM + 1,
        DUIMBN_DROPDOWN_CLOSE = DUIN_CUSTOM + 2,
    };

    DuiMenuBar();
    ~DuiMenuBar() override;

    DuiMenuBar(const DuiMenuBar&) = delete;
    DuiMenuBar& operator=(const DuiMenuBar&) = delete;

    // ---- 栏目 builder ----

    // 追加一个栏目。
    //   nID：栏目 id（不与下拉项 id 共用空间；下拉项 id 由 DuiMenu 内部
    //        管）。预留给未来"按 id 改栏目"的 setter。
    //   text：栏目文字（"&X" 助记符；Alt+X 激活）。
    //   dropdown：关联的下拉 DuiMenu；caller 持有 lifetime；nullptr 视为
    //             "禁用栏目"（点击 no-op）。
    // 返回新栏目的 index。
    int     AppendItem(UINT nID, LPCTSTR text, DuiMenu* dropdown);

    // 移除某栏目。idx 越界时 no-op。
    void    RemoveItem(int idx);

    // 清空所有栏目。
    void    Clear();

    // 栏目数量。
    int     GetCount() const { return (int)m_items.size(); }

    // ---- 栏目状态 ----

    void    SetItemEnabled(UINT nID, bool enabled);
    bool    IsItemEnabled(UINT nID) const;

    // 替换某栏目的下拉菜单（罕用；通常 AppendItem 时已绑）。idx 越界
    // no-op；dropdown 可为 nullptr（禁用该栏的弹出）。
    void    SetDropdown(int idx, DuiMenu* dropdown);
    DuiMenu* GetDropdown(int idx) const;

    // ---- 运行时 ----

    // 当前激活栏目（弹下拉中那栏）；-1 = 无激活。
    int     GetActiveIndex() const { return m_activeIdx; }

    // 程序化激活某栏 + 弹下拉（同步阻塞到下拉关 / 用户切换 / 选项）。
    // idx 越界或栏目无 dropdown 时 no-op。
    void    OpenDropdown(int idx);

    // 程序化关闭当前下拉（如果在弹）。罕用 —— 用户操作通常自己关。
    void    CloseDropdown();

    // Alt + 助记符路由。host 在 WM_SYSKEYDOWN 时调，vk 是按下字母（A-Z）
    // 的虚键码。命中某栏目则立即弹其下拉，返回 true；否则返 false 让
    // host 走默认 Alt 处理（如 OS 系统菜单）。
    bool    ProcessAltKey(UINT vk);

    // ---- 外观参数 ----

    void    SetItemHeight(int px);
    int     GetItemHeight() const { return m_itemH; }

    // ---- 内省（测试 / 调试）----

    // 单条栏目的可见数据（cachedW 不暴露，是内部缓存）。
    struct Item
    {
        UINT     nID      = 0;
        CString  text;
        DuiMenu* dropdown = nullptr;
        bool     enabled  = true;
    };

    int         GetItemCount() const;
    Item        GetItem(int idx) const;       // idx 越界返默认值

    // ---- DuiControl 覆写 ----
    SIZE        GetDesiredSize() const override;
    void        OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool        OnMouseMove   (POINT pt, UINT mkFlags) override;
    bool        OnMouseLeave  () override;
    bool        OnLButtonDown (POINT pt, UINT mkFlags) override;
    bool        OnKeyDown     (UINT vk, UINT flags) override;
    bool        OnSetCursor   (POINT pt) override;

private:
    // 测算第 i 个栏目的宽度（含左右 padding）。优先返 m_cachedW[i]；
    // 未测过（-1）则用一个临时 DC 测一次写回缓存。
    int     ItemWidth(int i) const;

    // 第 i 个栏目在 host 客户区的 RECT。
    RECT    ItemRect(int i) const;

    // 第 i 个栏目的左下角屏幕坐标（下拉锚点）。
    POINT   ItemAnchorScreen(int i) const;

    // hit-test：客户区坐标 → 栏目 idx；不命中返 -1。
    int     HitTestItem(POINT clientPt) const;

    // 内部：把 idx 设为 active + invalidate（旧/新栏都重画）。-1 = 清除。
    void    SetActiveIndex_(int idx);

    // 把"除当前栏目外"的栏目矩形转成屏幕坐标，写入 outZones / outZoneToItem。
    void    BuildSwitchZones(int curIdx,
                             std::vector<RECT>& outZones,
                             std::vector<int>&  outZoneToItem) const;

    // 同步驱动 dropdown 切换循环。startIdx 越界或 m_loopActive 已为 true
    // 时 no-op（防重入）。
    void    OpenAndDriveDropdown(int startIdx);

    int     FindIndexById(UINT nID) const;

    HWND    HostHwnd() const;

private:
    // ---- 视觉常量 ----------------------------------------------------------
    // 单位：96dpi 逻辑像素。运行期 DPI 缩放交给 host / DuiResMgr 处理；这
    // 里只声明设计值。修改前先在 DuiGallery 里手动确认手感。

    // 默认行高。24px = 微软雅黑 9pt 上下各留 ~7px padding，跟 Win10 任务管
    // 理器顶部菜单条目测宽度一致；改这个值会影响 kItemHeightMin 下限的合
    // 理性。
    static const int  kItemHeightDefault = 24;
    // SetItemHeight 的最小允许值。低于这个高度，9pt 文字 ascender 顶到上
    // 边、descender 顶到下边，肉眼贴边难看；12 是经验下限。
    static const int  kItemHeightMin     = 12;
    // 栏目左右 padding。8px 是 Win10 / Office 菜单条的对应间距，让相邻栏
    // 目之间留出一字距视觉宽度，避免 hover 高亮粘连。
    static const int  kItemPadL          = 8;
    static const int  kItemPadR          = 8;

    std::vector<Item>      m_items;
    // 与 m_items[i] 平行：栏目宽度缓存（含左右 padding）。-1 = 未测过。
    // 与 m_items 同步增删；text / item-height 改时整批清成 -1。
    mutable std::vector<int> m_cachedW;
    int               m_hoverIdx     = -1;
    int               m_activeIdx    = -1;     // 弹下拉中的栏目；-1 = 无
    bool              m_altActive    = false;  // Alt 按下后的"键盘激活态"
    bool              m_loopActive   = false;  // OpenAndDriveDropdown 防重入
    int               m_itemH        = kItemHeightDefault;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_MENUBAR
