#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_MENU

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

#include "../../DuiControl.h"

// CImageEx 在<u>全局</u>命名空间（ImageEx.h 里定义），所以前向声明放在
// balloonwjui namespace<u>外</u>；否则 namespace 内查找会解析成
// balloonwjui::CImageEx 报"incomplete type"。
class CImageEx;

namespace balloonwjui {

class DuiMenuPopup;

// =================================================================
// DuiMenu —— 自绘上下文 / 弹出菜单
// =================================================================
//
// 用途：右键菜单、托盘菜单、tab 标签的 close-context、聊天 / 联系人列表
// 上的"操作"菜单。OS 原生 TrackPopupMenu 的样式在 Win11 下挺难看，自绘
// 一套与产品调性一致的（白底 + 1px 浅灰边 + 系统阴影 + 32px 高行）。
//
// 视觉风格（参 screenshots/menu.png）：
//   · 白底，1px 浅灰边框，系统 drop shadow。
//   · 黑文字；hover 行底色非常浅的灰（#F2F2F2，<u>不</u>用全局品牌蓝
//     —— 菜单 hover 用强对比反而显眼）。
//   · 32px 行高，分隔条全宽。
//
// 项类型（与 CSkinMenu 冻结契约一致）：
//   · 文本项：可带勾选标记或 icon（互斥）。
//   · 分隔条：横线，不可选中。
//   · 子菜单项：hover / 点击时打开嵌套 DuiMenu。
//   · 禁用项：灰字 + icon 自动变灰，点击 no-op。
//
// 工作机制：
//   · 类本身只是<u>builder + runner</u>：持有 vector<Item>，TrackPopup
//     时弹一个内部 DuiMenuPopup 顶层窗口（WS_POPUP + 阴影）渲染并接收
//     输入；用户点项 / 失焦 / ESC 时关闭。
//   · TrackPopup 是<u>同步</u>的（与 Win32 TrackPopupMenu 语义一致）：
//     返回被点项的 id 或 0（dismissed）。
//   · 子菜单是<u>借用</u>的：caller 持有嵌套 DuiMenu 生命期，必须在整个
//     TrackPopup 调用期间保活。点子菜单<u>父项</u>不返回它的 id（视为
//     "打开子菜单"，返 0），只有<u>叶项</u>点击才返非 0 id。
//   · 静态纯 helper（FindAcceleratorChar / FindAcceleratorMatch /
//     KeyboardNavNext）方便不依赖 HWND 单测。
//   · 不可拷贝 —— 每个可见 popup 一个 DuiMenu 实例。
//
// 代码用法（右键上下文菜单）：
//
//     DuiMenu menu;
//     const UINT ID_OPEN = 1, ID_REMOVE = 2, ID_PROP = 3;
//     menu.AppendItem(ID_OPEN, _T("&打开"));
//     menu.AppendSeparator();
//     menu.AppendDisabled(ID_REMOVE, _T("&删除"));
//     menu.AppendItem(ID_PROP, _T("&属性"));
//     POINT pt; ::GetCursorPos(&pt);
//     UINT chosen = menu.TrackPopup(pt.x, pt.y, hwndOwner);
//     if (chosen == ID_OPEN)      OpenSelected();
//     else if (chosen == ID_PROP) ShowProperties();
//
// XML 用法：<u>暂未原生支持</u>。原因：菜单是事件触发的瞬时弹层（不在
// 静态布局里），且每项有独立 id / icon / 子菜单等结构，跟 XML 的"声明
// 静态树"模型不太契合。业务通常 C++ 直接 new + AppendItem 就足够。
//
// 事件：TrackPopup 同步返回；不通过 WM_DUI_NOTIFY。
//
// 替代关系：CSkinMenu。
class BUI_API DuiMenu
{
public:
    DuiMenu();
    ~DuiMenu();

    DuiMenu(const DuiMenu&) = delete;
    DuiMenu& operator=(const DuiMenu&) = delete;

    // ---- builder API（每个 Append* 返回新项的 index 方便链式 SetItemIcon）----

    // 追加普通文本项。
    //   nID：项 id（点击时 TrackPopup 返此值）。
    //   text：显示文本（"&X" 助记符 → Alt+X 激活）。
    //   icon：可选 icon。caller 持有所有权。
    int     AppendItem    (UINT nID, LPCTSTR text, CImageEx* icon = nullptr);

    // 追加可勾选项。
    //   checked：初始勾选状态。
    int     AppendChecked (UINT nID, LPCTSTR text, bool checked);

    // 追加禁用项（灰显，点击 no-op）。
    int     AppendDisabled(UINT nID, LPCTSTR text, CImageEx* icon = nullptr);

    // 追加分隔条。
    int     AppendSeparator();

    // 追加子菜单项。
    //   subMenu：caller 持有 lifetime；必须在 TrackPopup 期间保活。
    //   nID：点子菜单父项时（视为"打开子菜单"）TrackPopup 不返该 id；
    //        只有叶项点击才返非 0。
    int     AppendSubMenu (UINT nID, LPCTSTR text, DuiMenu* subMenu, CImageEx* icon = nullptr);

    // ---- 已存在项的状态修改（按 id 索引）----
    void    SetCheck(UINT nID, bool checked);
    void    SetEnabled(UINT nID, bool enabled);
    void    SetItemIcon(UINT nID, CImageEx* icon);    // nullptr 清空 icon
    bool    IsChecked(UINT nID) const;
    bool    IsEnabled(UINT nID) const;
    CImageEx* GetItemIcon(UINT nID) const;

    // 清空 / 项数。
    void    Clear();
    int     GetCount() const { return (int)m_items.size(); }

    // ---- 运行时 ----

    // 计算菜单弹出后的客户区像素尺寸（与内部 popup 渲染同口径：宽 = 图标列 +
    // 文字左右内边距 + 最宽项文字（夹在内部上下限）+ 子菜单箭头列；高 = 各行
    // 高度之和，分隔条行更矮）。只读、不创建窗口。
    //   用途：调用方在 TrackPopup <u>之前</u>据此把菜单锚定到按钮上方 / 左侧 ——
    //         内部 popup 把左上角放在传入坐标且<u>不</u>自动翻转，故需先知道
    //         高度才能"向上弹"。空菜单返回 {0,0}。
    SIZE    MeasureSize() const;

    // 在屏幕坐标 (screenX, screenY) 弹菜单。<u>同步阻塞</u>到用户点项 /
    // 失焦 / ESC。
    //   ownerHwnd：所有者窗口 HWND（菜单关掉后焦点回这里）。
    //   返回：被点项的 id；0 = dismissed without selection。
    UINT    TrackPopup(int screenX, int screenY, HWND ownerHwnd);

    // ---- 切换区域 hook（DuiMenuBar 用）----

    // TrackPopupEx 的返回值。chosenId 同 TrackPopup 返值；switchZoneIdx
    // >= 0 表示是被 mouse-move 命中 SetSwitchZones 的某 zone 关掉的 ——
    // 用户没选项目，但意图切换到该 zone（典型场景：menu bar 激活态下鼠
    // 标移到隔壁栏目）。两者互斥：chosenId != 0 时 switchZoneIdx == -1。
    struct TrackPopupResult
    {
        UINT chosenId;
        int  switchZoneIdx;
    };

    // 注册"切换区域"——一组屏幕坐标矩形，popup 期间会以 30ms 间隔轮询
    // 鼠标位置；落入任一 zone 就关闭自身并把 zone idx 报告给 caller。
    //   screenZones / count：caller 持有 lifetime；TrackPopupEx 调用期间
    //                        必须保活。count == 0 时清空已设 zones。
    // 设计目的：让 DuiMenuBar 实现"激活态下 mouse-move 切栏目"，无需
    // 额外加键盘/鼠标 hook。普通弹出菜单（业务侧右键 / 按钮 dropdown）
    // 不调用本方法，行为与 TrackPopup 完全一致。
    void    SetSwitchZones(const RECT* screenZones, int count);

    // TrackPopup 的扩展版。chosenId 与 TrackPopup 返值语义一致；
    // switchZoneIdx 若 >= 0 表示触发关闭的是 SetSwitchZones 注册的某
    // 个矩形（鼠标移入了它）。
    TrackPopupResult TrackPopupEx(int screenX, int screenY, HWND ownerHwnd);

    // 强制关掉本菜单及所有嵌套子菜单（罕用，TrackPopup 会自动清理）。
    void    HideNow();

    // ---- 内省（popup 窗口和测试用）----
    enum ItemKind { ItemText, ItemCheckable, ItemSeparator, ItemSubMenu };

    struct Item
    {
        UINT       id;
        ItemKind   kind;
        bool       enabled;
        bool       checked;
        CString    text;
        DuiMenu*   subMenu;       // 借用；仅 kind == ItemSubMenu 时非 null
        CImageEx*  icon;          // 借用；与勾选标记互斥
    };

    const std::vector<Item>& GetItems() const { return m_items; }

    // ---- 静态纯 helper（不依赖 HWND，方便单测）----

    // 在 text 里找第一个 '&' 后的字母，返回小写形式（无则 0）。
    // "&&" 视为 "&" 转义，跳过。
    //   "&Save"     -> 's'
    //   "Save &As"  -> 'a'
    //   "Save && Quit" -> 0
    static TCHAR FindAcceleratorChar(LPCTSTR text);

    // 在 items 里找第一个 enabled / 非分隔条 / 助记符匹配 ch 的项。
    //   返回：匹配项 index；不匹配返 -1。
    static int   FindAcceleratorMatch(const std::vector<Item>& items, TCHAR ch);

    // 从 fromIdx 起按 dir（+1/-1）方向找下一个"可选中"项（Text /
    // Checkable / SubMenu 且 enabled）。环回。fromIdx == -1 表示
    // "无当前，从头（dir>0）或从尾（dir<0）开始"。
    //   返回：下一个可选项 index；全部不可选返 -1。
    static int   KeyboardNavNext(const std::vector<Item>& items, int fromIdx, int dir);

private:
    int     FindIndexById(UINT nID) const;

private:
    std::vector<Item>  m_items;
    DuiMenuPopup*      m_activePopup = nullptr;   // 借用；关闭时清空
    UINT               m_lastChosenId = 0;        // popup 关闭前写入
    // 切换区域：caller 借出的指针 + 数量。TrackPopupEx 期间 popup 拷贝
    // 一份用，TrackPopupEx 返回后 caller 可释放。-1 = 没切换。
    const RECT*        m_switchZones        = nullptr;
    int                m_switchZoneCount    = 0;
    int                m_lastSwitchZoneIdx  = -1;
    friend class DuiMenuPopup;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_MENU
