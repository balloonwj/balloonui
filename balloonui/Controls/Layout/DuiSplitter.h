#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_SPLITTER

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiSplitter —— 2-pane 可拖动分隔容器（无 HWND）
// =================================================================
//
// 用途：把客户区分成两块、中间一根可拖的分隔条，让用户改变两块的相对
// 大小。典型场景：聊天主窗（联系人列表 | 消息区）、邮件三栏的两栏部分、
// 设置页（导航树 | 详情）。
//
//  Orientation::Vertical    Orientation::Horizontal
//   +------+----+              +-----------+
//   |      |    |              |   pane 0  |
//   |pane 0| p1 |              +-----------+   ← 分隔条
//   |      |    |              |           |
//   |      |    |              |   pane 1  |
//   +------+----+              +-----------+
//          ↑                          ↑
//      分隔条                     分隔条
//
// 命名约定：与 Win32 splitter 一致 ——
//   "Vertical"   = 分隔条<u>纵向</u>跑（两 pane 左右排）；
//   "Horizontal" = 分隔条<u>横向</u>跑（两 pane 上下叠）。
//
// 工作机制：
//   · pane 通过 SetPane(0|1, ...) 安装，<b>不</b>走通用 AddChild —— 因为
//     splitter 要明确"slot 语义"和不需要枚举式 layout。
//   · 自绘分隔条；hover 时光标变 NS-resize / WE-resize；按下抓 DUI
//     capture 启动拖动；mouse-move 更新 m_targetPx，松键提交并发一次
//     DUIN_VALUECHANGED。
//   · pane 尺寸"<u>用户意图</u>" m_targetPx 跨容器尺寸变化保持 ——
//     即使容器临时缩小到塞不下，等容器再放大时，原比例自动恢复。
//   · SetSplitFraction(f) 把"按比例分"挂起，等下一次 Layout 拿到实尺寸
//     再换成 px（适合在 splitter 还未挂载到已知尺寸前预设比例）。
//     SetSplitPx 写死 px 后，挂起的 fraction 失效。
//   · 自身不接键盘焦点；pane 内的子控件各自参与 tab。
//
// 代码用法：
//
//     auto split = std::make_unique<DuiSplitter>();
//     split->SetOrientation(DuiSplitter::Vertical);
//     split->SetMinSizes(120, 200);
//     split->SetSplitPx(180);
//     split->SetPane(0, std::make_unique<DuiTreeView>());
//     split->SetPane(1, std::make_unique<DuiLabel>());
//     split->SetCtrlId(IDC_MAIN_SPLIT);
//     parent->AddChild(std::move(split));
//     // 父对话框的 OnDuiNotify：
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_MAIN_SPLIT)
//     //       SaveLayout((int)n.extra);
//
// XML 用法（详细属性见 guides.html §3.3.2）：
//
//     <splitter id="200" orientation="vertical" bar-thickness="6"
//               split-px="240" min-size-0="120" min-size-1="200">
//       <vbox padding="8">  <!-- 自动作为 pane 0 -->
//         ...左侧导航...
//       </vbox>
//       <vbox padding="8">  <!-- 自动作为 pane 1 -->
//         ...右侧详情...
//       </vbox>
//     </splitter>
//
// 事件：
//   · DUIN_VALUECHANGED — 用户拖完分隔条松手时一次。
//                          extra = pane 0 当前像素尺寸。
class BUI_API DuiSplitter : public DuiControl
{
public:
    enum Orientation
    {
        Vertical   = 0,    // 分隔条纵向、pane 左右排
        Horizontal = 1,    // 分隔条横向、pane 上下排
    };

    DuiSplitter();

    // 设置 / 读取分隔条方向。
    //   o：Vertical 或 Horizontal。
    void        SetOrientation(Orientation o);
    Orientation GetOrientation() const { return m_orient; }

    // 设置 / 读取分隔条粗细（沿分割轴方向）。默认 4px。
    //   px：> 0；<= 0 会被 clamp 到 1。
    void        SetBarThickness(int px);
    int         GetBarThickness() const { return m_barThick; }

    // 设置 / 读取每 pane 沿分割轴的最小尺寸 —— 拖到这里会被挡住。
    // 默认 40 / 40。
    //   min0 / min1：>= 0 的整数。
    void        SetMinSizes(int min0, int min1);
    int         GetMinSize0() const { return m_min0; }
    int         GetMinSize1() const { return m_min1; }

    // 设置 / 读取分割位置 —— 即 pane 0 沿分割轴的尺寸（px）。
    // 写 px 会清掉此前 SetSplitFraction 挂起的请求。
    //   px0：>= min0 且 <= AvailAlong - barThick - min1。
    //         超出会在 Layout 时 clamp。
    void        SetSplitPx      (int px0);
    int         GetSplitPx      () const { return m_splitPx; }

    // 按比例分 —— 当下不立即落地，等下一次 Layout 算出实尺寸再换 px。
    //   f：0.0 .. 1.0；超出区间也接受但 clamp 时按 SetMinSizes 限制。
    void        SetSplitFraction(double f);
    double      GetSplitFraction() const { return m_splitFrac; }

    // 安装 / 替换 pane。pane 之前已存在的会被销毁。pane 自身可以是
    // 任何 DuiControl 子类（VBox / HBox / 业务控件 / 嵌套 splitter）。
    //   idx：0 或 1。
    //   pane：要挂的子控件；nullptr 表示清空。
    void        SetPane(int idx, std::unique_ptr<DuiControl> pane);
    DuiControl* GetPane(int idx) const;

    // ---- DuiControl 覆写 ----
    void  Layout(const RECT& rcAvail) override;
    void  OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool  OnMouseMove   (POINT pt, UINT mkFlags) override;
    bool  OnLButtonDown (POINT pt, UINT mkFlags) override;
    bool  OnLButtonUp   (POINT pt, UINT mkFlags) override;
    bool  OnSetCursor   (POINT pt) override;
    DuiControl* HitTest (POINT ptHostClient) override;

    // ---- 几何 helper（测试用）----

    // 分隔条自身在 host-客户区的矩形。
    RECT GetBarRect() const;

    // 给定坐标点是否在分隔条上。
    bool IsPointInBar(POINT pt) const;

private:
    int   AvailAlong() const;       // m_rcItem 沿分割轴的尺寸
    int   ClampSplit(int v) const;  // clamp 到 [m_min0, AvailAlong()-barThick-m_min1]
    void  ApplyPendingFraction();   // 若 fraction 挂起则换 px

private:
    Orientation m_orient   = Vertical;
    int         m_barThick = 4;
    int         m_min0     = 40;
    int         m_min1     = 40;

    // m_targetPx 是用户<u>意图中</u>的 pane 0 尺寸（被 SetSplitPx 或拖动
    // 设置）。它跨 Layout 调用保持 —— 当容器尺寸临时小到容不下意图值时，
    // 容器再放大后能自动恢复用户的请求尺寸。
    // m_splitPx 是当前 Layout <u>实际应用</u>后的尺寸（已 clamp）。
    int         m_targetPx = 100;
    int         m_splitPx  = 100;
    double      m_splitFrac = -1.0;     // < 0 表示没有挂起的 fraction
    bool        m_dragging  = false;
    int         m_dragStartCursor = 0;  // LDown 时鼠标沿轴坐标
    int         m_dragStartSplit  = 0;  // LDown 时的 m_splitPx

    // pane 裸指针；所有权在 DuiControl::m_children 里。每次 SetPane 切换
    // slot 时 RemoveChild 旧的 + AddChild 新的，再更新 raw 指针。
    DuiControl* m_pane[2] = { nullptr, nullptr };
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SPLITTER
