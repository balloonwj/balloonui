#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"
#include "../../BalloonUiApi.h"

namespace balloonwjui {

// =================================================================
// DuiLayout / DuiHBox / DuiVBox / DuiGrid —— 布局容器（无 HWND）
// =================================================================
//
// 用途：把若干 DuiControl 子控件按"横向 / 纵向 / 网格"自动布局，最常用的
// 顶层结构件。三种容器都共一个 DuiLayout 基类，调用方可以把它们当
// DuiLayout* 传来传去。
//
// 工作机制：
//   · 子控件按声明顺序布局。每个子的"layout hint"（fixedMain / weight /
//     margin / align）存在容器的 side table 里，按子裸指针 keyed。子可以
//     无 hint —— 此时用默认值（自动尺寸、weight=1、零 margin、Fill 对齐）。
//   · 坐标系都是 host-客户区坐标（与 DuiControl 一致）。容器
//     Layout(rcAvail) 把自身 m_rcItem 设为 rcAvail，然后把内距
//     (m_rcItem - padding) 区域分给子。
//   · 容器自身不绘制任何东西、不发任何 DUIN_*；它对 WM_DUI_NOTIFY 链
//     是透明的。
//
// 尺寸规则：
//   · HBox / VBox：每个子要么沿主轴有固定尺寸（Hint.fixedMain >= 0），
//     要么按 weight 分剩余空间。交叉轴尺寸 = rcAvail 的交叉轴尺寸 -
//     margin（除非 Hint.fixedCross >= 0）。
//   · Grid：rows × cols 等大格子，每个子填一个格子（减去 margin），
//     默认 Fill 对齐。
//
// 代码用法（登录表单：标题 + 滚动区 + 按钮行）：
//
//     auto root = std::unique_ptr<DuiVBox>(new DuiVBox());
//     root->SetPadding(12);
//     root->SetGap(8);
//     root->AddChild(std::move(header),     DuiLayout::Hint().Fixed(40));
//     root->AddChild(std::move(scrollArea), DuiLayout::Hint().Weight(1));
//     auto buttons = std::unique_ptr<DuiHBox>(new DuiHBox());
//     buttons->AddChild(std::move(okBtn),     DuiLayout::Hint().Fixed(80));
//     buttons->AddChild(std::move(cancelBtn), DuiLayout::Hint().Fixed(80));
//     root->AddChild(std::move(buttons), DuiLayout::Hint().Fixed(32));
//
// XML 用法（详细属性见 guides.html §3.3.1）：
//
//     <vbox padding="12" gap="8">
//       <label text="登录" fixedHeight="40"/>
//       <vbox weight="1">...滚动区...</vbox>
//       <hbox gap="6" fixedHeight="32">
//         <control weight="1"/>     <!-- 弹性占位 -->
//         <button text="取消" fixedWidth="80"/>
//         <button text="确定" fixedWidth="80" id="100"/>
//       </hbox>
//     </vbox>
//
// 事件：无（容器对 WM_DUI_NOTIFY 链透明）。
class BUI_API DuiLayout : public DuiControl
{
public:
    // 主 / 交叉轴对齐方式。Near = 左 / 上；Far = 右 / 下；Fill = 撑满。
    enum Align : unsigned
    {
        AlignNear   = 0,    // 左 / 上
        AlignCenter = 1,
        AlignFar    = 2,    // 右 / 下
        AlignFill   = 3
    };

    // 单个子控件的 layout 提示。Fluent builder 风格：
    //   DuiLayout::Hint().Fixed(80).Margin(4).AlignC(AlignCenter)
    struct Hint
    {
        int     fixedMain  = -1;    // 主轴像素；-1 = 用 weight
        int     fixedCross = -1;    // 交叉轴像素；-1 = 撑满交叉轴
        int     weight     = 1;     // 主轴剩余空间的份数
        int     marginL    = 0;
        int     marginT    = 0;
        int     marginR    = 0;
        int     marginB    = 0;
        Align   alignMain  = AlignFill;
        Align   alignCross = AlignFill;

        // 主轴固定 main 像素；可选指定交叉轴 cross 像素。
        Hint& Fixed(int main, int cross = -1) { fixedMain = main; fixedCross = cross; return *this; }
        // 主轴权重 w（剩余空间按所有非固定子的 weight 总和按比例分）。
        Hint& Weight(int w)                   { weight = w; return *this; }
        // 4 边 margin。
        Hint& Margin(int l, int t, int r, int b) { marginL = l; marginT = t; marginR = r; marginB = b; return *this; }
        Hint& Margin(int all)                 { marginL = marginT = marginR = marginB = all; return *this; }
        // 主轴对齐（在固定尺寸 < 主轴可用空间时生效）。
        Hint& AlignM(Align a)                 { alignMain = a; return *this; }
        // 交叉轴对齐（在固定尺寸 < 交叉轴可用空间时生效）。
        Hint& AlignC(Align a)                 { alignCross = a; return *this; }
    };

    // 设置容器内部 padding（内距），子控件在此基础上才开始排。
    //   l/t/r/b：>= 0 的整数。
    void    SetPadding(int l, int t, int r, int b);

    // 4 边相同 padding 的便捷重载。
    void    SetPadding(int all)              { SetPadding(all, all, all, all); }

    // 设置子控件之间的间距（HBox 列间距 / VBox 行间距 / Grid 行列共用）。
    //   gap：>= 0 的整数。
    void    SetGap(int gap);

    // 添加一个子控件 + 它的 layout hint。Hint 会被复制存到 side table；
    // 后续改 hint 必须调 SetHint(child, ...) 才生效。
    //   child：要添加的子控件（被 move 进来）。
    //   hint：layout 提示（Fluent builder 构造）。
    void    AddChild(std::unique_ptr<DuiControl> child, const Hint& hint);
    using DuiControl::AddChild;            // 保留无 hint 重载

    // 修改已存在子控件的 hint。
    void    SetHint(DuiControl* child, const Hint& hint);

    // 读取已存在子控件的 hint；不存在则返回默认 Hint。
    Hint    GetHint(DuiControl* child) const;

protected:
    // 子类实现具体布局算法。
    void    Layout(const RECT& rcAvail) override = 0;

    // 钩子：base RemoveChild 调到这里，scrub m_hints 里对应条目，避免
    // 死指针残留导致后续 HintFor 命中 stale 入口。
    void    OnChildRemoved_(DuiControl* child) override;

    // helper：给定一个 cell 矩形 + 子的 hint，套用 margin + 对齐 + 固定
    // 尺寸算出子的真实矩形。
    static RECT ApplyHint(const RECT& cell, const Hint& h, bool mainIsHorizontal);

    // helper：查 child 的 hint，未设置时返回默认 Hint。
    Hint    HintFor(DuiControl* c) const;

protected:
    int     m_padL = 0, m_padT = 0, m_padR = 0, m_padB = 0;
    int     m_gap  = 0;
    // side table：key 是 raw 子指针。m_children 持有所有权，这里只按
    // 地址存 hint。child 被 RemoveChild 时一并清掉。
    std::vector<std::pair<DuiControl*, Hint>> m_hints;
};

// =================================================================
// DuiHBox —— 横向布局容器
// =================================================================
//
// 用途：让子控件横着一排排开。典型场景：toolbar、表单的 label + edit
// 行、按钮组。
//
// 工作机制：
//   · 子按声明顺序沿 X 轴铺。
//   · fixedMain >= 0 的子占用固定 px 宽；剩余空间按非固定子的
//     Hint.weight 比例分。
//   · 交叉轴（Y）默认撑满容器内距高 - margin；alignCross 可以让较矮
//     的子靠 Near / Center / Far。
//
// 代码用法：
//
//     auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
//     row->SetGap(6);
//     row->AddChild(std::move(label), DuiLayout::Hint().Fixed(80));
//     row->AddChild(std::move(edit),  DuiLayout::Hint().Weight(1));
//     row->AddChild(std::move(go),    DuiLayout::Hint().Fixed(60));
//
// XML 用法：见 DuiLayout 类的 XML 用法说明。
//
// 事件：无。
class BUI_API DuiHBox : public DuiLayout
{
public:
    // 把子控件沿 X 轴布局到 rcAvail 内距。
    void    Layout(const RECT& rcAvail) override;

    // 收缩到适合的固有尺寸：cx = padL + Σ可见子(max(0,fixedMain) + marginL+R)
    // + gap*(visN-1) + padR；cy = padT + max可见子(max(fixedCross,
    // child.GetDesiredSize().cy) + marginT+B) + padB。Weight 子（fixedMain<0）
    // 在主轴上贡献 0 —— 它们等 ScrollView 给的剩余空间，不参与 intrinsic。
    // 用途：DuiScrollView::SetAutoContentHeight(true) 通过这条拿到内容自然
    // 高度；任何对 HBox 调 GetDesiredSize 的 caller 也走同一公式。
    SIZE    GetDesiredSize() const override;
};

// =================================================================
// DuiVBox —— 纵向布局容器
// =================================================================
//
// 用途：让子控件竖着一列叠放。典型场景：对话框主体、表单、设置页。
//
// 工作机制：
//   · 子按声明顺序沿 Y 轴叠。
//   · fixedMain >= 0 的子占用固定 px 高；剩余高按 Hint.weight 比例分。
//   · 交叉轴（X）默认撑满容器内距宽 - margin；alignCross 可以让较窄
//     的子靠 Near / Center / Far。
//
// 代码用法：
//
//     auto col = std::unique_ptr<DuiVBox>(new DuiVBox());
//     col->SetPadding(8);
//     col->SetGap(4);
//     col->AddChild(std::move(title),   DuiLayout::Hint().Fixed(28));
//     col->AddChild(std::move(content), DuiLayout::Hint().Weight(1));
//     col->AddChild(std::move(footer),  DuiLayout::Hint().Fixed(32));
//
// XML 用法：见 DuiLayout 类的 XML 用法说明。
//
// 事件：无。
class BUI_API DuiVBox : public DuiLayout
{
public:
    // 把子控件沿 Y 轴布局到 rcAvail 内距。
    void    Layout(const RECT& rcAvail) override;

    // 收缩到适合的固有尺寸：cy = padT + Σ可见子(max(0,fixedMain) + marginT+B)
    // + gap*(visN-1) + padB；cx = padL + max可见子(max(fixedCross,
    // child.GetDesiredSize().cx) + marginL+R) + padR。Weight 子（fixedMain<0）
    // 在主轴上贡献 0。DuiScrollView::SetAutoContentHeight(true) 走这条；
    // DuiGallery 各 page-VBox 由此报准实际高度，避免之前硬编码 1500。
    SIZE    GetDesiredSize() const override;
};

// =================================================================
// DuiGrid —— 等大网格容器
// =================================================================
//
// 用途：固定 rows × cols 的等大格子，子控件按行优先填入。典型场景：
// emoji 选择面板、文件类型图标网格、设置项的 N×N 选项。
//
// 工作机制：
//   · 内距矩形被切成 rows × cols 个等大格子（先扣 padding 和 gap）；
//     子控件按行优先（row-major）顺序填入。
//   · 没设 Hint 的子撑满整格；设了 Hint 可以指定 Near/Center/Far
//     对齐或 fixedMain/fixedCross 在格子里靠某一角。
//   · 子超过 rows*cols 个时，多余的<u>不显示也不溢出滚动</u>。
//
// 代码用法（3 行 × 2 列 emoji 面板）：
//
//     auto grid = std::unique_ptr<DuiGrid>(new DuiGrid());
//     grid->SetGrid(3, 2);
//     grid->SetGap(4);
//     for (auto& g : gifs)
//     {
//         grid->AddChild(std::move(g));
//     }
//
// XML 用法（详细属性见 guides.html §3.3.1）：
//
//     <grid rows="3" cols="2" gap="4" padding="8">
//       <button text="A"/> <button text="B"/>
//       <button text="C"/> <button text="D"/>
//       <button text="E"/> <button text="F"/>
//     </grid>
//
// 事件：无。
class BUI_API DuiGrid : public DuiLayout
{
public:
    // 设置网格行数 × 列数。必须在 AddChild 之前调，否则按 1×1 排。
    //   rows / cols：>= 1 的整数。
    void    SetGrid(int rows, int cols);
    int     GetRows() const { return m_rows; }
    int     GetCols() const { return m_cols; }

    // 把每个子放到对应格子里（行优先）。
    void    Layout(const RECT& rcAvail) override;

    // 固有尺寸：cellW × cols + gap*(cols-1) + padL+R；cellH × rows + gap*(rows-1)
    // + padT+B。cellW = max可见子(max(fixedMain, child.GetDesiredSize().cx)
    // + marginL+R)；cellH 同理走 fixedCross / cy / marginT+B。无可见子时
    // cellW=cellH=0，只剩 padding。
    SIZE    GetDesiredSize() const override;

private:
    int m_rows = 1;
    int m_cols = 1;
};

} // namespace balloonwjui
