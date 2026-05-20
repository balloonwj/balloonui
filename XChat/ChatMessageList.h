#pragma once

// =============================================================================
// ChatMessageList —— 单聊消息流（Phase 4，从头写）
//
// 为什么不用 DuiVirtualList：DuiVirtualList 假设每行<u>等高</u>，可某信单
// 聊里气泡 / 卡片 / 日期分隔线高度差异巨大（文字按宽度换行，转账卡固
// 定 ~88px，日期 ~36px）。所以本控件自己维护一个"per-message 高度缓存
// + 累加 top"的简单 layout 算法。
//
// 工作机制：
//   · 持有一个 `const std::vector<ChatMessage>*` —— 不拥有数据，data 是
//     MockChat 进程级 static。
//   · 控件宽度变化（resize / 首次 Layout）时调 RelayoutMessages_：按当
//     前可绘制内宽（m_rcItem 宽 - padding）+ 每个 kind 的 Measure 函数算
//     出每条消息的高度，累加得到 m_layouts[i].top + .height + 总
//     m_contentH。
//   · OnPaint 二分 / 线性查找当前可见的 first/last 行（按 m_scrollY），
//     调对应 paint 函数。
//   · 滚轮：m_scrollY += zDelta；clamp 到 [0, max(0, m_contentH-viewH)]。
//     Phase 4 没接 DuiScrollBar；纯滚轮即可看效果。Phase 5+ 视需要再加
//     scrollbar。
//
// 不接收点击 / hover —— Phase 4 demo 不需要交互。OnLButtonUp 等留空。
// =============================================================================

#include "DuiControl.h"

namespace balloonwjui { class DuiScrollBar; }

namespace xchat {

struct ChatMessage;     // forward — full def in MockChat.h

class ChatMessageList : public balloonwjui::DuiControl
{
public:
    ChatMessageList();
    ~ChatMessageList() override = default;

    // 数据源：caller 保证生命周期 >= 本控件。
    void SetMessages(const std::vector<ChatMessage>* msgs);

    // ---- DuiControl 覆写 ----
    void  Layout (const RECT& rcAvail) override;
    void  OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool  OnMouseWheel(POINT pt, short zDelta, UINT mkFlags) override;
    bool  OnMouseMove (POINT pt, UINT mkFlags) override;
    bool  OnMouseLeave() override;

private:
    // 重新算每条消息的 top/height（依赖 m_rcItem 宽度）。
    void  RelayoutMessages_();
    // 同步 scrollbar 的 range / page。RelayoutMessages 后调。
    void  UpdateScrollRange_();
    // DuiScrollBar::OnScrollFn 静态 thunk。
    static void OnSbScrolledStub_(void* user, int newPos);

    // 单条消息的 layout 缓存（local-y，相对于内容区顶端）。
    struct MsgLayout
    {
        int top;        // 此条消息 top（local-y）
        int height;     // 此条消息高度
    };

private:
    const std::vector<ChatMessage>* m_msgs = nullptr;
    std::vector<MsgLayout>          m_layouts;     // 同长度 m_msgs->size()
    int                             m_contentH = 0;
    balloonwjui::DuiScrollBar*      m_sb       = nullptr;   // 所有权在 m_children
};

} // namespace xchat
