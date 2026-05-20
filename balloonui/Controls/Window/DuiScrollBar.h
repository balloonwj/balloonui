#pragma once

#include "../../BalloonUiFeatures.h"
#include <memory>
#if BUI_FEATURE_SCROLLBAR

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiScrollBar —— DUI 原生滚动条（无 HWND）
// =================================================================
//
// 用途：列表 / 文档 / 自定义 view 需要"内容比可视区大"时挂的滚动条。
// 与 DuiScrollView 配合是最常见用法（自动同步范围 + 内容偏移）；也
// 可独立使用接其它 OnScroll 回调。
//
// 工作机制（几何模型）：
//
//      nMin .................. nMax
//      |____________[thumb]____|
//                   ^pos       ^pos+page
//      thumbPx = max(MIN_THUMB, trackPx * page / (max - min + page))
//
// "max" 是<u>最大有效 pos</u>，<u>不</u>是内容尺寸 —— 1000px 内容显示
// 在 200px 视窗里时，应 SetRange(0, 800) + SetPage(200)，而不是
// SetRange(0, 1000)。
//
// 工作机制（行为）：
//   · track + thumb 实色绘制（暂无皮肤资产）。点击轨道 = 该方向 page
//     步；按下 thumb 抓 capture 拖动；滚轮 = line 步。
//   · 每次位置变化触发两路通知：
//     a) C 风格 OnScrollFn 回调（DuiScrollView 用，零 round-trip 直接
//        改内容偏移）；
//     b) DUIN_VALUECHANGED WM_DUI_NOTIFY 上冒（extra = newPos）给
//        通用消费者。
//   · 仅 host 线程访问。
//
// 代码用法（直接挂自定义内容控件）：
//
//     auto sb = std::unique_ptr<DuiScrollBar>(new DuiScrollBar(/*horizontal=*/false));
//     sb->SetRange(0, 800);
//     sb->SetPage(200);
//     sb->SetLineSize(20);
//     sb->SetOnScroll([](void* ud, int pos) {
//         static_cast<MyContent*>(ud)->ScrollTo(pos);
//     }, m_content);
//     m_root->AddChild(std::move(sb));
//
// XML 用法：<u>暂未原生支持</u>。一般业务直接用 DuiScrollView（它内嵌
// scrollbar），无需 XML 暴露 scrollbar 自身。
//
// 事件：
//   · DUIN_VALUECHANGED — 任何 SetPos(_, true) 触发；extra = 新 pos。
//
// 替代关系：CSkinScrollBar（仅在控件树整体迁移到无 HWND DUI 树时替换；
// 老对话框里的 HWND-hosted scrollbar 暂不动）。
class BUI_API DuiScrollBar : public DuiControl
{
public:
    typedef void (*OnScrollFn)(void* user, int newPos);

    explicit DuiScrollBar(bool horizontal = false);
    ~DuiScrollBar() override;

    bool    IsHorizontal() const { return m_horizontal; }

    // 切换水平 / 垂直。
    void    SetHorizontal(bool h);

    // 设置范围 + 页大小 + 当前位置。注意 max 是"最大有效 pos"语义。
    void    SetRange(int nMin, int nMax);
    int     GetMin()  const { return m_min; }
    int     GetMax()  const { return m_max; }

    // page 决定 thumb 大小和"轨道点击"步长。
    void    SetPage(int page);
    int     GetPage() const { return m_page; }

    // 当前滚动位置；自动 clamp 到 [min, max]。
    //   notify：true 时触发 DUIN_VALUECHANGED；false 抑制。
    void    SetPos(int pos, bool notify = true);
    int     GetPos()  const { return m_pos; }

    // 一次 line 步（滚轮 / 上下键 / "↑↓" 按钮）的步长。默认 1。
    void    SetLineSize(int n) { m_lineSize = n > 0 ? n : 1; }
    int     GetLineSize() const { return m_lineSize; }

    // 注册 C 风格快速回调（DuiScrollView 用，避免 std::function 头依赖）。
    //   fn：回调函数；nullptr 取消。
    //   user：透传给回调第一个参数。
    void    SetOnScroll(OnScrollFn fn, void* user) { m_fn = fn; m_user = user; }

    // 步进辅助（鼠标滚轮 / Up-Down 键 / 上下按钮调）。
    void    LineUp()    { SetPos(m_pos - m_lineSize); }
    void    LineDown()  { SetPos(m_pos + m_lineSize); }
    void    PageUp()    { SetPos(m_pos - (m_page > 0 ? m_page : m_lineSize)); }
    void    PageDown()  { SetPos(m_pos + (m_page > 0 ? m_page : m_lineSize)); }

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool    OnMouseMove  (POINT pt, UINT mkFlags) override;
    bool    OnMouseWheel (POINT pt, short zDelta, UINT mkFlags) override;

    // 当前 thumb 矩形（host-客户区坐标）。public 给单测用。
    RECT    ComputeThumbRect() const;

    // ---- 渐隐 / auto-hide 支持 ----
    //
    // 开启 auto-hide 后，scrollbar 默认 alpha=0（不可见，但仍占控件位置），
    // 仅在 caller 主动触发 TriggerShow() 后渐入到 alpha=1，并启动 idle
    // 计时器（默认 800ms 没活动就渐出回 0）。caller 通常在自家 OnMouse
    // Move / OnMouseWheel / OnMouseLeave 里调，让"鼠标在 list 区域时显
    // 示，离开短延迟后隐藏"。FadeIn / FadeOut 跑 200ms 线性 alpha 动画
    // （走 DuiAnimMgr，需 caller 在 host 里挂 60Hz pulse —— XChat MainFrame
    // 的 kAnimPulseTimerId 已挂）。
    void    SetAutoHide(bool b);
    bool    IsAutoHide() const            { return m_autoHide; }

    // 立即把 alpha 拉到 1（用 fade-in 动画），并复位 idle 计时器。
    // 通常在 OnMouseEnter / OnMouseMove / OnMouseWheel 时调。
    void    TriggerShow();

    // 立即取消 idle 计时器并启动 fade-out。OnMouseLeave 时调。
    void    StartFadeOut();

    // 直接设 alpha（跳过动画）。0 = 完全透明，1 = 完全不透明。
    void    SetAlpha(float a);
    float   GetAlpha() const              { return m_alpha; }

public:
    // ---- auto-hide 内部 anim helper（实现细节，public 是因为 anim 闭包要 friend 不方便） ----
    struct FadeToken
    {
        DuiScrollBar* owner = nullptr;
        bool          alive = false;
    };

private:
    void    Notify();
    int     ClampPos(int p) const;
    int     TrackPixels() const;            // 主轴可移动轨道大小
    int     ThumbPixels() const;            // 主轴 thumb 大小
    int     PixelsPerUnit() const;          // (track - thumb) / (max - min)；分母 0 时 -1
    int     PixelOriginAlongMain() const;   // 轨道在 host 坐标的起点（主轴）

    // 启 fade alpha 动画；durMs 一般 200。重新启动时取消老 anim 防累加。
    void    StartFadeAnim_(float to, int durMs);
    // 启"空动画"作 idle delay timer：delayMs 后回调 StartFadeOut。
    void    StartIdleTimer_(int delayMs);
    // 把当前 token alive=false，让所有 in-flight anim 回调走 no-op。
    void    CancelFadeAnims_();

private:
    bool        m_horizontal;
    int         m_min       = 0;
    int         m_max       = 0;
    int         m_page      = 1;
    int         m_pos       = 0;
    int         m_lineSize  = 1;

    // 拖拽状态。
    bool        m_dragging      = false;
    int         m_dragOffsetPx  = 0;        // 鼠标按下点到 thumb 起点的 px
    int         m_dragStartPos  = 0;

    OnScrollFn  m_fn   = nullptr;
    void*       m_user = nullptr;

    // auto-hide / fade 状态
    bool        m_autoHide  = false;
    float       m_alpha     = 1.0f;     // 0..1；m_autoHide=true 初始拉到 0
    // shared_ptr 模式：anim 闭包持 token 副本；切 anim 时把 token alive
    // 翻 false，老闭包下次 fire 走 dead 路径。同时 idle/fade 共用一个
    // token，两条 anim 都被一起取消（这正是 TriggerShow 重置时想要的语义）。
    std::shared_ptr<FadeToken> m_fadeToken;
};

// =================================================================
// DuiScrollView —— 单子节点 + 纵向滚动条的视口
// =================================================================
//
// 用途：装一个长内容（高度大于视口）让用户滚着看。最常用是装一个 VBox
// 装很多条目；也可装自绘 view（聊天历史 / 长表格）。
//
// 工作机制：
//   · 视口宽度 = m_rcItem.Width - sbWidth；高度 = m_rcItem.Height。
//     内容控件被设到视口的<u>名义</u>矩形（高度 = SetContentHeight 给的
//     总高），按 GetScrollPos OffsetRect 移到当前可见。
//   · 内容超过视口时滚动条出现；正好/不超时滚动条 range 收缩，配合
//     SetScrollBarWidth(0) 可隐藏。
//   · 视口内任何地方滚轮都触发滚；track 点击 / thumb 拖也都正常工作。
//   · 两种高度模式：
//     - 显式：SetContentHeight(h) caller 给固定数值；
//     - 自动：SetAutoContentHeight(true) 每次 Layout 询问内容
//       GetDesiredSize().cy。
//   · 仅 host 线程访问。
//
// 代码用法：
//
//     auto sv = std::unique_ptr<DuiScrollView>(new DuiScrollView());
//     sv->SetContent(BuildLongList());
//     sv->SetAutoContentHeight(true);
//     sv->SetScrollBarWidth(12);
//     m_root->AddChild(std::move(sv));
//
// XML 用法：<u>暂未原生支持</u>。要用的话业务侧 CustomFactory 注册
// <scrollview> 标签 + 第一个子做内容（详见 §3.6）。
//
// 事件：内容子的 WM_DUI_NOTIFY 正常上冒（视口对消息透明）；scrollbar
// 自身的 DUIN_VALUECHANGED 通过 OnSbScrolled 内部回调消化，不冒到业务。
class BUI_API DuiScrollView : public DuiControl
{
public:
    DuiScrollView();

    // 安装 / 替换内容子。拿走所有权，旧内容销毁。
    //   content：DuiControl 子类；nullptr 表示清空。
    void    SetContent(std::unique_ptr<DuiControl> content);
    DuiControl* GetContent() const { return m_content; }

    // 显式告知内容总高度（用于算 scrollbar 范围）。
    void    SetContentHeight(int h);
    int     GetContentHeight() const { return m_contentH; }

    // 启用 / 关闭自动测高。开启后每次 Layout 询问 content
    // GetDesiredSize().cy；caller 不需要手动同步。默认 false。
    void    SetAutoContentHeight(bool b);
    bool    GetAutoContentHeight() const { return m_autoHeight; }

    // 设置滚动条宽度（px）。0 = 隐藏滚动条。默认 12。
    void    SetScrollBarWidth(int w);
    int     GetScrollBarWidth() const { return m_sbWidth; }

    int     GetScrollPos() const;
    void    SetScrollPos(int p);
    DuiScrollBar* GetScrollBar() { return m_sb; }

    // ---- DuiControl 覆写 ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnMouseWheel(POINT pt, short zDelta, UINT mkFlags) override;

private:
    static void OnSbScrolled(void* user, int newPos);
    void        UpdateRange();
    void        ApplyScrollToContent();
    void        DoLayout();

private:
    DuiControl*     m_content = nullptr;       // 裸；所有权在 m_children
    DuiScrollBar*   m_sb      = nullptr;       // 裸；所有权在 m_children
    int             m_contentH = 0;
    int             m_sbWidth  = 12;
    bool            m_autoHeight = false;
    RECT            m_contentNominalRect{};    // 内容 scrollPos=0 时的矩形
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SCROLLBAR
