#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_COMBOBOX

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

class DuiListBox;
class DuiComboBoxPopup;
class DuiEdit;
class DuiComboEdit;

// =================================================================
// combopopup —— 下拉浮层的落点计算
//
// 单拎出来是为了能被单元测试直接断言：真正弹浮层要建顶层窗口、要问
// 显示器，测不了；而"该落在哪儿"完全是算术，把工作区当参数传进来就
// 成了纯函数。DuiComboBox::OpenPopup 先查当前显示器的工作区，再调
// 本函数，两边用的是同一段逻辑。
// =================================================================
namespace combopopup {

// 浮层窗口上下边框各占 1 像素，故按整行数算出高度后还要补上这 2 像素，
// 否则最后一行会被边框压掉一截。
const int kPopupBorderThickness = 2;

// 浮层至少要露出的行数。工作区实在太矮（任务栏很高、下拉框又在屏幕中间）
// 时也不该把浮层压成一条缝 —— 列表本身能滚，露一行就还能用。
const int kPopupMinRows = 1;

/**
 *  算下拉浮层最终该落在屏幕的什么位置。
 *
 *  规则：默认贴在下拉框正下方、与其同宽；下方装不下就翻到上方；上下都装不下
 *  就选空间大的那一侧，并把高度压到那一侧能容纳的整行数 —— 浮层内部的列表本身
 *  能滚（见 DuiComboBoxPopup::OnMouseWheel），压矮只是少露几行，不会让哪一项
 *  彻底够不着。左右方向同样夹进工作区，免得下拉框贴着屏幕右缘时浮层探出去。
 *
 *    comboScreen：下拉框自身的矩形，屏幕坐标，即浮层的锚点。
 *    popupW：     浮层期望宽度（逻辑像素），一般与下拉框同宽。
 *    popupH：     浮层期望高度（逻辑像素），= 行数 * itemH + kPopupBorderThickness。
 *                 空间不足时本函数会把它压小。
 *    itemH：      单行行高（逻辑像素），用于把可用高度换算成整行数；须大于 0。
 *    work：       目标显示器的工作区（已排除任务栏），屏幕坐标。
 *  @return 夹取后的浮层矩形，屏幕坐标；保证完全落在 work 之内（work 本身
 *          比一行还矮这种极端情况除外，此时至少保证左上角在 work 内）。
 */
RECT ClampPopupToWorkArea(const RECT& comboScreen, int popupW, int popupH,
                          int itemH, const RECT& work);

// 浮层里那个列表控件（DuiListBox）的 ctrlId。浮层的宿主会把树里<u>所有</u>控件的通知
// 都转给浮层窗口，其中包括列表内部滚动条的 DUIN_VALUECHANGED；浮层据这个 id 认出
// 哪些通知来自列表本身。
const UINT kPopupListCtrlId = 1;

/**
 *  浮层收到一条控件通知后该怎么处理。
 */
enum PopupNotifyAction
{
    kPopupNotifyIgnore     = 0,   // 与选择无关（如列表内部滚动条滚动、鼠标进出），浮层保持打开
    kPopupNotifySelect     = 1,   // 列表选中了某一行：写回下拉框并关闭浮层
    kPopupNotifyItemDelete = 2    // 点了列表某行的删除叉：通知下拉框并关闭浮层
};

/**
 *  判断浮层收到的一条通知属于哪一种。
 *
 *  必须连 ctrlId 一起判：同一个通知码可能来自不同的控件（balloonui 的自定义通知码由各控件
 *  分别从 DUIN_CUSTOM 起编号，数值相同而含义不同；通用通知码则由各控件共用），只比较通知码
 *  就会把别的控件发来的通知误判为本分支要处理的通知。这里的具体情况是：列表内部的滚动条
 *  滚动时同样发 DUIN_VALUECHANGED（extra 是滚动位置），只比较通知码的话，滚一下滚轮就会
 *  被当成选中了某一行，浮层随即关闭，项数超过一屏的下拉框因而无法滚动选择。
 *    code：  通知码（DuiNotify::code）。
 *    ctrlId：发出通知的控件 id（DuiNotify::ctrlId）。
 *  @return 处理方式，取值见 PopupNotifyAction。
 */
PopupNotifyAction ClassifyPopupNotify(UINT code, UINT ctrlId);

} // namespace combopopup

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
//   · StyleEditable：主体左侧嵌一个无窗口输入框（DuiComboEdit，是
//     DuiEdit 子类），右侧 ~20px 是箭头区。可手输入；选 popup 项时
//     把项文本写回该输入框。等价于 Win32 CBS_DROPDOWN。
//
// 工作机制：
//   · combo 主体本身是 paint-only DUI；editable 风格额外加一个无窗口
//     输入框子控件（DuiComboEdit，挂在 m_children 里）。它没有自己的
//     窗口，像素由 combo 的 OnPaint 递归画出 —— 铺完主体底色之后、画
//     下拉箭头之前。
//   · popup 是独立 WS_POPUP HWND，可超出对话框边界。关闭时机：选项 /
//     ESC / 失焦 / 又一次 OpenPopup 调用。ClosePopup 是 idempotent。
//   · 选项以 CString 列表存；AddString 返回新索引。SetText 在 editable
//     模式下<u>不</u>会触发 VALUECHANGED（m_suppressEditNotify 抑制）。
//   · SetEditable 任何时候都可调；内嵌输入框按需创建 / 销毁。无窗口控件
//     构造完就能用，不再有「等宿主窗口就绪之后才能创建」这一步。
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
//   · DUICBN_ITEMDELETE — 下拉里点了某项右侧的删除叉；extra = 项索引
//                          （已映射回 m_items 下标，过滤态下也对得上）。
//                          仅在 SetShowItemDelete(true) 时可能触发。本控件
//                          <u>不删</u>那一项，收到通知的宿主自行决定是否二次
//                          确认、确认后再调 DeleteString。
//                          <b>宿主处理本通知时不得同步弹模态对话框</b>：通知是
//                          在下拉浮层的消息栈里发出的，模态对话框一泵消息，浮层
//                          就会因失去焦点而自我销毁，等模态框返回时浮层对象已经
//                          没了。要弹框请先 PostMessage 给自己、延后一拍再弹。
//
// 替代关系：CSkinComboBox（冻结 API：AddString / DeleteString /
// ResetContent / GetCount / GetItemText / SetCurSel / GetCurSel；
// 原 SetReadOnly 改名 SetEditable(false)）。
class BUI_API DuiComboBox : public DuiControl
{
public:
    enum Style { StyleReadOnly = 0, StyleEditable = 1 };

    enum NotifyCode
    {
        // 下拉项右侧的删除叉被点击；extra = 项索引。
        DUICBN_ITEMDELETE = DUIN_CUSTOM + 1,
    };

    DuiComboBox();
    ~DuiComboBox() override;

    // ---- 模式 ----

    // 切换 read-only / editable。可在挂到父之前或之后调；如果 host 已
    // attached，会按需懒创建 / 销毁嵌入 EDIT 子。
    //   b：true = editable；false（默认）= read-only。
    void    SetEditable(bool b);
    bool    IsEditable() const { return m_style == StyleEditable; }

    // ---- 整体底色 / 边框（与 DuiEdit 同名 API 对齐）----

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

    // 是否在下拉列表每一项右侧画删除叉（默认 false，画出来与不开时完全一致）。
    //
    // 开启后下拉里每行右端出现一个淡灰色的 ×：鼠标移到该行时加深、直接压在叉上
    // 变红；点它发 DUICBN_ITEMDELETE（extra = 项索引）并<u>不改变当前选项</u>。
    // 典型用途是登录框的账号历史下拉 —— 让用户删掉不再用的账号。
    //
    // 本控件收到点击只发通知、不动 model：要不要二次确认、确认后除了 DeleteString
    // 还要清理什么（本地聊天记录、缓存目录…），都属于业务决定。
    void     SetShowItemDelete(bool b);
    bool     IsShowItemDelete() const { return m_showItemDelete; }

    // 设置 / 读取右侧下拉箭头的颜色（默认 RGB(80,100,140) 蓝灰色）。
    // 仅覆盖 enabled 态;disabled 态沿用内部 kArrowDisabled = RGB(160,160,160)
    // 不变(业务一般不需要单独换 disabled 色)。三角形走 DuiAA::FillPolygon
    // 抗锯齿绘制,设任意颜色都是平滑边。
    void     SetArrowColor(COLORREF c);
    COLORREF GetArrowColor() const { return m_arrowColor; }

    // 设置 / 读取 1px 边框的颜色。
    //   normal：常态边框色；
    //   active：鼠标悬停或下拉浮层展开时的边框色。
    // 禁用态沿用内部 kBorderDisabled = RGB(190,190,190)；SetShowBorder(false) 时不画
    // 边框，这两个颜色都不使用。默认 RGB(150,150,150) / RGB(80,130,200)，与引入
    // 本接口之前写死的颜色相同，不调用本方法的地方外观不变。
    void     SetBorderColors(COLORREF normal, COLORREF active);
    COLORREF GetBorderNormalColor() const { return m_borderNormal; }
    COLORREF GetBorderActiveColor() const { return m_borderActive; }

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

    // ---- 每项图标与副文字（2026-10-04 起，登录窗账号下拉列表「头像 - 账号 - 姓名」用）----
    //
    // 只影响下拉浮层里各行的绘制（转交给浮层里的 DuiListBox，见它的同名接口）；下拉框
    // 主体只显示选中项的文字，不画图标与副文字。都不设时与以前完全相同。

    // 给第 index 项设置浮层里的左侧图标。
    //   index：项索引；越界时忽略。
    //   hbm：32 位预乘 alpha 位图，调用方持有，本控件不复制、不释放，须在本控件销毁、
    //        改设或删除该项之前保持有效；NULL 表示不画图标。
    void    SetItemIcon(int index, HBITMAP hbm);

    // 第 index 项的图标；越界或没有设时返回 NULL。
    HBITMAP GetItemIcon(int index) const;

    // 给第 index 项设置浮层里的副文字（画在主文字右侧、弱色）。
    //   index：项索引；越界时忽略。
    //   sz：副文字；NULL 或空串表示没有。
    void    SetItemSubText(int index, LPCTSTR sz);

    // 第 index 项的副文字；越界或没有设时返回空串。
    CString GetItemSubText(int index) const;

    // 浮层里图标的边长（像素），默认 16；小于 1 时按 1。行高另由 SetItemHeight 设置。
    void    SetIconSize(int px)       { m_iconSize = (px < 1) ? 1 : px; }
    int     GetIconSize() const       { return m_iconSize; }

    // 浮层里的一行
    struct PopupItem
    {
        CString m_text;      // 主文字
        CString m_subText;   // 副文字；空串表示没有
        HBITMAP m_icon;      // 图标；NULL 表示没有（调用方持有）
    };

    // 按过滤映射表取出要放进浮层的各行。public 是给单测用，运行时由 OpenPopup 调。
    //   filteredIndices：过滤映射表（第 k 个命中项对应 m_items 的哪个下标，见
    //                    MapPopupIndexWithFilter）；空表示未过滤，取全部各项。
    //   返回：浮层各行，顺序与浮层一致；映射表里越界的下标被跳过。
    std::vector<PopupItem> BuildPopupItems(const std::vector<int>& filteredIndices) const;

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

    // 把下拉浮层里的项下标映射回 m_items 的下标（纯函数版）。
    //
    // 增量搜索过滤激活时，浮层只显示命中的那几项，它报回来的下标是"第几个命中
    // 项"而不是"m_items 的第几项"；不映射就会张冠李戴 —— 选中会选错人、删除
    // 会删错人。过滤未激活（映射表为空）时两者本来就 1:1，原样返回。
    //   popupIndex：浮层里的项下标。
    //   filteredIndices：过滤映射表（第 k 个命中项对应 m_items 的哪个下标）；
    //                    空表示未过滤。
    //   返回：m_items 里的下标；popupIndex 越出映射表范围时原样返回，
    //         由调用方自己判越界。
    static int MapPopupIndexWithFilter(int popupIndex,
                                       const std::vector<int>& filteredIndices);

    // 上面那个的成员版：按本 combo 当前的过滤态做映射。
    // 运行时由 OnPopupSelected / OnPopupItemDelete 调。
    int     MapPopupIndexToItem(int popupIndex) const;

    // ---- DuiControl 覆写 ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonUp(POINT pt, UINT mkFlags) override;

    // 鼠标移入 / 移出：悬停时边框换成 active 色（OnPaint 里按 m_bHover 取色），要重画一次
    // （BUG-105，2026-10-06）。写成内联，理由同 DuiButton::OnMouseEnter。
    bool    OnMouseEnter() override
    {
        const bool handled = DuiControl::OnMouseEnter();
        Invalidate();
        return handled;
    }
    bool    OnMouseLeave() override
    {
        const bool handled = DuiControl::OnMouseLeave();
        Invalidate();
        return handled;
    }

    // popup 选中某项时回调（popup 内部调）。
    void    OnPopupSelected(int index);

    // popup 里点了某项的删除叉时回调（popup 内部调）。把它按本 combo 的 ctrlId
    // 冒成 DUICBN_ITEMDELETE 通知给宿主，自己<u>不删</u>任何项。
    //   index：被点的项在<u>浮层里</u>的下标；过滤激活时本方法会先映射回 m_items
    //          的真实下标，再据此上报。
    void    OnPopupItemDelete(int index);

    // popup 关闭时回调。
    void    OnPopupClosed();

    // 内嵌输入框的文字发生变化时回调（由 DuiComboEdit::OnTextChanged 转来，
    // 用户输入与程序设值都会触发）。重置 m_curSel（除非输入文本完全匹配某项），
    // 并按 combo 的 ctrlId 冒一个 VALUECHANGED 通知。
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
    std::vector<HBITMAP>  m_itemIcons;                  // 与 m_items 一一对应：各项在浮层里的图标（调用方持有），NULL 表示没有
    std::vector<CString>  m_itemSubTexts;               // 与 m_items 一一对应：各项在浮层里的副文字，空串表示没有
    int                   m_iconSize     = 16;          // 浮层里图标的边长（像素）
    int                   m_curSel       = -1;
    int                   m_maxVisible   = 8;
    int                   m_itemH        = 22;
    Style                 m_style        = StyleReadOnly;

    COLORREF              m_bgColor      = RGB(255, 255, 255);  // 主体底色
    bool                  m_showBorder   = true;                // 是否描 1px 边框
    bool                  m_showArrow    = true;                // 是否画下拉箭头
    bool                  m_showItemDelete = false;             // 下拉项右侧是否画删除叉
    COLORREF              m_arrowColor   = RGB( 80, 100, 140);  // 下拉箭头 enabled 态色;默认蓝灰
    COLORREF              m_borderNormal = RGB(150, 150, 150);  // 常态边框色;默认中灰
    COLORREF              m_borderActive = RGB( 80, 130, 200);  // 悬停 / 浮层展开时的边框色;默认蓝

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
