#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_DOCK

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"
#include <vector>

namespace balloonwjui {

// =================================================================
// DuiDock —— 经典 dock 布局容器（无 HWND）
// =================================================================
//
// 用途：经典"上下左右 + 中央内容"骨架。每个子从一边（top / bottom /
// left / right）占用固定 px 的条带，最后一个 Fill 子（一般 0 个或 1 个）
// 占余下中央。<u>顺序很重要</u> —— 子按 AddChild 的顺序依次"咬"剩余
// 内距。
//
// 例：
//
//      AddDocked(toolbar,  DockTop,    32)
//      AddDocked(status,   DockBottom, 24)
//      AddDocked(navTree,  DockLeft,   180)
//      AddDocked(infoPane, DockRight,  200)
//      AddDocked(content,  DockFill)
//
// 产出"toolbar 在上、status 在下、nav 在左、info 在右、content 在中"
// 的标准 IM 主面板布局。比嵌套三层 HBox / VBox 写法清爽得多。
//
// 工作机制：
//   · 纯布局容器，自身不绘制；子各画各的。
//   · 每个 docked 子按声明顺序从内距矩形里"咬"一条带（Top/Bottom
//     咬高度、Left/Right 咬宽度），下一个子看到的是剩余矩形。
//   · DockFill 子占用所有剩余空间；同一容器一般<u>只挂一个</u> Fill
//     让数学清晰。
//   · padding 整体内缩内距矩形；gap 在同轴相邻 docked 子之间插一段空。
//   · 子的所有权走标准 DuiControl::m_children；slot 数组只记 side / sizePx。
//
// 代码用法：
//
//     auto dock = std::unique_ptr<DuiDock>(new DuiDock());
//     dock->SetPadding(8);
//     dock->SetGap(4);
//     dock->AddDocked(std::move(toolbar),  DuiDock::DockTop,    32);
//     dock->AddDocked(std::move(statusBar),DuiDock::DockBottom, 24);
//     dock->AddDocked(std::move(navTree),  DuiDock::DockLeft,  180);
//     dock->AddDocked(std::move(content),  DuiDock::DockFill);
//     host.SetRoot(std::move(dock));
//
// XML 用法：<u>暂未原生支持</u>。原因：dock 子的"side + sizePx"是 per-
// child 元数据，跟 vbox/hbox 的 weight/fixed 不在一套通用属性上，需要
// 一个新的"side" / "size" XML 属性约定。要 XML 化的话业务自己写
// CustomFactory 注册 <dock> + 解析子的 side="top|bottom|left|right|fill"
// + size="32" 属性即可（详见 guides.html §3.6）。
//
// 事件：无（容器对 WM_DUI_NOTIFY 链透明）。
class BUI_API DuiDock : public DuiControl
{
public:
    enum Side
    {
        DockTop    = 0,
        DockBottom = 1,
        DockLeft   = 2,
        DockRight  = 3,
        DockFill   = 4,    // 占余下中央；一个容器一般只挂一个 Fill
    };

    // 设置容器 padding。
    //   l/t/r/b：>= 0 的整数。
    void  SetPadding(int l, int t, int r, int b);
    void  SetPadding(int all)              { SetPadding(all, all, all, all); }

    // 设置同轴相邻 docked 子之间的间距。
    //   gap：>= 0 的整数。
    void  SetGap(int gap);

    // 添加一个 docked 子。
    //   child：被 move 进来的子控件。
    //   side：DockTop/Bottom/Left/Right/Fill 之一。
    //   sizePx：沿 docking 轴的固定大小（Top/Bottom 是高度、Left/Right 是
    //           宽度；Fill 时忽略此参数）。
    //   返回：裸子指针，方便 caller 后续操作（所有权已在容器里）。
    DuiControl* AddDocked(std::unique_ptr<DuiControl> child, Side side, int sizePx = 0);

    // ---- DuiControl 覆写 ----
    void  Layout(const RECT& rcAvail) override;

    // ---- 测试 / 内省 ----
    int   GetChildCount() const { return (int)m_slots.size(); }
    Side  GetChildSide  (int i) const;
    int   GetChildSize  (int i) const;

private:
    struct Slot { DuiControl* ctrl; Side side; int sizePx; };
    int   FindSlot(DuiControl* c) const;

    int                m_padL = 0, m_padT = 0, m_padR = 0, m_padB = 0;
    int                m_gap  = 0;
    std::vector<Slot>  m_slots;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_DOCK
