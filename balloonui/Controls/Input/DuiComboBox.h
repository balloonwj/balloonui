#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_COMBOBOX

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

class DuiListBox;
class DuiComboBoxPopup;
class DuiEditHost;
class DuiComboEdit;

// =================================================================
// DuiComboBox —— 下拉选择框（含弹出 popup HWND）
// =================================================================
//
// 用途：从 N 个候选项里选一项的最常用控件。combo 主体 = 当前选中项 +
// 右侧下拉箭头；点箭头弹出列表（独立的 WS_POPUP 顶层 HWND，可超出
// 父对话框客户区，跟原生 combo 一致）。
//
// 两种风格：
//   · StyleReadOnly（默认）：主体自绘，画选中项文本 + 箭头；不可手输入。
//     等价于 Win32 CBS_DROPDOWNLIST。
//   · StyleEditable：主体左侧嵌一个真 EDIT HWND（DuiComboEdit，是
//     DuiEditHost 子类），右侧 ~20px 是箭头区。可手输入（IME 正常工作
//     因 EDIT 是真 HWND）；选 popup 项时把项文本写回 EDIT。
//     等价于 Win32 CBS_DROPDOWN。
//
// 工作机制：
//   · combo 主体本身是 paint-only DUI；editable 风格额外加一个 EDIT
//     HWND 子（通过 DuiComboEdit 管理，挂在 m_children 里）。
//   · popup 是独立 WS_POPUP HWND，可超出对话框边界。关闭时机：选项 /
//     ESC / 失焦 / 又一次 OpenPopup 调用。ClosePopup 是 idempotent。
//   · 选项以 CString 列表存；AddString 返回新索引。SetText 在 editable
//     模式下<u>不</u>会触发 VALUECHANGED（m_suppressEditNotify 抑制）。
//   · SetEditable 任何时候都可调；嵌套 EDIT 按需懒创建 / 销毁。
//   · 增量搜索（incremental search）：editable 模式下用户每键，弹出列表
//     按"前缀 / 子串 + 区分大小写 / 不区分"过滤。默认 prefix +
//     case-insensitive。
//
// 代码用法：
//
//     auto cb = std::unique_ptr<DuiComboBox>(new DuiComboBox());
//     cb->SetCtrlId(IDC_GENDER);
//     cb->AddString(_T("女"));
//     cb->AddString(_T("男"));
//     cb->SetCurSel(0);
//     cb->SetRect(RECT{ 16, 64, 200, 88 });
//     parent->AddChild(std::move(cb));
//     // 父对话框 WM_DUI_NOTIFY:
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_GENDER) {
//     //       int i = (int)n.extra;   // -1 = 用户输入的文本不在列表
//     //   }
//
// XML 用法：<u>暂未原生支持</u>。原因：combo 是带 model（item 列表）的
// 控件，items 静态写在 XML 里实际场景少（多半是运行时拉数据库 / 配置
// 填）。需要 XML 化的话，业务侧自己写 CustomFactory 注册 <combobox>
// 标签 + 处理 <item> 子节点（详见 guides.html §3.6）。
//
// 事件（ctrlId = combo id）：
//   · DUIN_VALUECHANGED — 选项变化或 editable 输入变化时触发。
//                          extra >= 0：从 popup 选了一项，extra = newIndex
//                                      （此时 m_curSel == newIndex）。
//                          extra == -1：editable 模式手输入触发；m_curSel
//                                      被重置为 -1，除非输入文本完全匹配
//                                      某个 item（这种情况会自动选中那个 item）。
//
// 替代关系：CSkinComboBox（冻结 API：AddString / DeleteString /
// ResetContent / GetCount / GetItemText / SetCurSel / GetCurSel；
// 原 SetReadOnly 改名 SetEditable(false)）。
class BUI_API DuiComboBox : public DuiControl
{
public:
    enum Style { StyleReadOnly = 0, StyleEditable = 1 };

    DuiComboBox();
    ~DuiComboBox() override;

    // ---- 模式 ----

    // 切换 read-only / editable。可在挂到父之前或之后调；如果 host 已
    // attached，会按需懒创建 / 销毁嵌入 EDIT 子。
    //   b：true = editable；false（默认）= read-only。
    void    SetEditable(bool b);
    bool    IsEditable() const { return m_style == StyleEditable; }

    // ---- 整体底色 / 边框（与 DuiEditHost 同名 API 对齐）----

    // 设置 combo 主体底色。默认 RGB(255,255,255) 白底。editable 模式下
    // 会一并把该色传给内嵌 EDIT，避免主体与 EDIT 内部出现色差。常用于
    // 把 combo 嵌进自带底色的容器（如圆角输入框）：SetBgColor(浅灰)。
    void     SetBgColor(COLORREF c);
    COLORREF GetBgColor() const { return m_bgColor; }

    // 是否绘制 1px 边框（默认 true）。设为 false 时主体只填充底色、不
    // 描边 —— 把 combo 嵌进自带圆角 / 边框的容器时关掉，避免方框边压在
    // 容器圆角上。editable 模式下一并作用于内嵌 EDIT。
    void     SetShowBorder(bool b);
    bool     IsShowBorder() const { return m_showBorder; }

    // 是否绘制右侧下拉箭头（默认 true）。设为 false 时不画箭头 —— 外观
    // 像纯输入框，但点击右侧箭头区域仍可弹出下拉列表。
    void     SetShowArrow(bool b);
    bool     IsShowArrow() const { return m_showArrow; }

    // 设置 / 读取右侧下拉箭头的颜色（默认 RGB(80,100,140) 蓝灰色）。
    // 仅覆盖 enabled 态;disabled 态沿用内部 kArrowDisabled = RGB(160,160,160)
    // 不变(业务一般不需要单独换 disabled 色)。三角形走 DuiAA::FillPolygon
    // 抗锯齿绘制,设任意颜色都是平滑边。
    void     SetArrowColor(COLORREF c);
    COLORREF GetArrowColor() const { return m_arrowColor; }

    // ---- 文本（editable 模式下用户面对的文本）----

    // 当前文本：editable 模式直接读 EDIT 内容；read-only 模式返回
    // GetItemText(GetCurSel())。
    CString GetText() const;

    // 程序设置文本。<u>不会</u>触发 DUIN_VALUECHANGED（防止初始化时假
    // 通知）。
    void    SetText(LPCTSTR sz);

    // ---- 数据 model ----

    // 在末尾追加一项。返回新项索引。
    int     AddString(LPCTSTR sz);

    // 删除指定索引的项。删除后 m_curSel 会按需调整（删除选中项 → 取消选）。
    void    DeleteString(int index);

    // 清空所有项。
    void    ResetContent();

    // 当前项数。
    int     GetCount() const { return (int)m_items.size(); }

    // 读取 / 修改第 index 个项的文本。索引越界时 GetItemText 返回空，
    // SetItemText 静默失败。
    CString GetItemText(int index) const;
    void    SetItemText(int index, LPCTSTR sz);

    // ---- 选中状态 ----

    // 当前选中项索引。-1 表示未选 / editable 输入未匹配任何项。
    int     GetCurSel() const { return m_curSel; }

    // 设置选中项。索引越界时设为 -1。
    //   index：[0, GetCount()) 或 -1 取消选。
    //   notify：true 时触发 DUIN_VALUECHANGED；false 抑制。
    void    SetCurSel(int index, bool notify = true);

    // ---- popup 行为 ----

    // 弹出列表最多显示几个项（多了滚动）。默认 8；< 1 会被 clamp 到 1。
    void    SetMaxVisibleItems(int n) { m_maxVisible = (n < 1) ? 1 : n; }
    int     GetMaxVisibleItems() const { return m_maxVisible; }

    // popup 单项高度（px）。默认 22；< 8 会被 clamp 到 8。
    void    SetItemHeight(int h) { m_itemH = (h < 8) ? 8 : h; }
    int     GetItemHeight() const { return m_itemH; }

    // 主动打开 / 关闭 popup。调 OpenPopup 会先关掉之前打开的 popup。
    void    OpenPopup();
    void    ClosePopup();
    bool    IsPopupOpen() const { return m_popupOpen; }

    // ---- 增量搜索（仅 editable 模式有意义）----

    // 启用 / 关闭增量搜索。enabled 时用户每键，popup 自动按当前文本过滤。
    // 第一个非空键还会自动 OpenPopup；空文本 → popup 显示全部项。
    void    SetIncrementalSearch(bool b);
    bool    GetIncrementalSearch() const                { return m_incSearch; }

    // 匹配模式：true = 子串包含；false（默认）= 前缀匹配。
    void    SetIncrementalSearchSubstring(bool b)       { m_incSubstring = b; }
    bool    GetIncrementalSearchSubstring() const       { return m_incSubstring; }

    // 匹配大小写：true = 区分；false（默认）= 不区分。
    void    SetIncrementalSearchCaseSensitive(bool b)   { m_incCaseSensitive = b; }
    bool    GetIncrementalSearchCaseSensitive() const   { return m_incCaseSensitive; }

    // 给定查询字符串，按当前匹配模式过滤 m_items 并返回匹配的索引列表。
    // 空查询 → 返回 [0..count) 全部。public 是给单测用，运行时由
    // OnEditTextChanged 调。
    std::vector<int> ComputeFilteredIndices(LPCTSTR query) const;

    // ---- DuiControl 覆写 ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonUp(POINT pt, UINT mkFlags) override;

    // popup 选中某项时回调（popup 内部调）。
    void    OnPopupSelected(int index);

    // popup 关闭时回调。
    void    OnPopupClosed();

    // 嵌入 EDIT 的 EN_CHANGE 触发时回调。重置 m_curSel（除非输入文本
    // 完全匹配某项），并按 combo 的 ctrlId 冒一个 VALUECHANGED 通知。
    void    OnEditTextChanged();

private:
    void    EnsureEditChild();         // 按 mode 创建 / 销毁 m_edit
    void    PositionEditChild();       // Layout 里调
    int     ArrowZoneWidth() const     { return 20; }
    RECT    ArrowZoneRect()  const;
    RECT    EditZoneRect()   const;
    int     FindItemMatching(LPCTSTR sz) const;

private:
    std::vector<CString>  m_items;
    int                   m_curSel       = -1;
    int                   m_maxVisible   = 8;
    int                   m_itemH        = 22;
    Style                 m_style        = StyleReadOnly;

    COLORREF              m_bgColor      = RGB(255, 255, 255);  // 主体底色
    bool                  m_showBorder   = true;                // 是否描 1px 边框
    bool                  m_showArrow    = true;                // 是否画下拉箭头
    COLORREF              m_arrowColor   = RGB( 80, 100, 140);  // 下拉箭头 enabled 态色;默认蓝灰

    DuiComboBoxPopup*     m_popup        = nullptr;
    bool                  m_popupOpen    = false;

    DuiComboEdit*         m_edit         = nullptr;     // 裸指针；存在时所有权在 m_children
    bool                  m_suppressEditNotify = false; // 程序 SetText 时的 guard

    bool                  m_incSearch        = false;
    bool                  m_incSubstring     = false;   // false=prefix, true=contains
    bool                  m_incCaseSensitive = false;
    std::vector<int>      m_filteredIndices;            // 过滤激活时 m_items 的索引映射
};

} // namespace balloonwjui

#endif // BUI_FEATURE_COMBOBOX
