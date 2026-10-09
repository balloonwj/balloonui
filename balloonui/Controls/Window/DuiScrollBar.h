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
// 外观（2026-10-04 起统一为悬浮式细滑块，与 Flamingo 聊天窗的滚动条一致）：
//   · 不画轨道，只画一根 kOverlayThumbPx(5) 像素粗、两端为半圆的半透明灰色滑块
//     （灰 0x5A、不透明度 120/255，再乘以淡入淡出的 alpha），贴在控件矩形的右缘
//     （水平滚动条为下缘）往里 kOverlayThumbMarginPx(2) 像素处。
//   · 控件矩形本身是「命中带」：比滑块宽，方便鼠标抓住；容器（DuiScrollView、
//     DuiListBox 等）把它浮在内容之上、贴着右缘放置，默认宽 kOverlayBandPx(11)，
//     内容按全宽排版。
//   · 滑块沿主轴的最短长度为 kMinThumbPx(36)，内容很长时也抓得住。
//
// 工作机制（行为）：
//   · 按下滑块抓 capture 拖动；按在轨道空白处时滑块中心先跳到点击处、接着可以继续拖
//     （2026-10-04 起，与聊天窗的滚动条一致；此前点击轨道是按整页翻）；滚轮 = line 步：一整格（zDelta 120）
//     滚 3 行，更小的 zDelta（精确式触摸板、高精度滚轮）按比例累积，凑满
//     一行才滚一行，方向反转时清掉累积的余量。
//   · 滚轮的消费与否：范围为空（max == min，内容放得下）时 OnMouseWheel
//     返回 false，让事件沿父链上冒到外层滚动容器；只要范围非空就返回
//     true，即便已经滚到顶 / 底也照样消费（详见
//     DuiHost::DispatchMouseWheel 的约定说明）。
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

    // 悬浮式细滑块的几何常量（像素）。用枚举而不是 static const int：后者被按引用
    // 使用（如传给 std::max）时需要类外定义，以 DLL 方式使用本库时会链接失败。
    enum
    {
        kOverlayThumbPx       = 5,    // 滑块的粗细（竖直滚动条为宽，水平滚动条为高）
        kOverlayThumbMarginPx = 2,    // 滑块离控件矩形右缘（水平滚动条为下缘）的距离
        kOverlayBandPx        = 11,   // 容器放置滚动条时默认的命中带宽度：滑块 5 + 右边距 2 + 左侧外扩 4
        kMinThumbPx           = 36,   // 滑块沿主轴的最短长度，内容很长时也不至于小到抓不住
    };

    // 淡入淡出的时长（毫秒）：滚动或悬停时淡入，停止操作 kIdleHideMs 后淡出。
    enum
    {
        kFadeInMs   = 200,   // 淡入时长
        kFadeOutMs  = 300,   // 淡出时长
        kIdleHideMs = 800,   // 最后一次操作之后、开始淡出之前的等待时长
    };

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

    // DuiControl 覆写（2026-10-04 起）：鼠标进入滚动条（命中带）时淡入显示；悬停期间空闲计时
    // 到期也不淡出。返回值同基类。
    bool    OnMouseEnter() override;

    // DuiControl 覆写（2026-10-04 起）：鼠标离开滚动条时重新开始空闲计时，kIdleHideMs 后淡出。
    // 返回值同基类。
    bool    OnMouseLeave() override;

    // 当前 thumb 矩形（host-客户区坐标）。这是<u>逻辑</u>滑块：沿主轴是滑块的位置与长度，
    // 沿副轴占满整条命中带，拖动与点击轨道都按它判断。public 给单测用。
    RECT    ComputeThumbRect() const;

    // 实际画出来的细滑块矩形（host-客户区坐标，2026-10-04 起）：沿主轴与 ComputeThumbRect
    // 相同；沿副轴只有 kOverlayThumbPx 粗，贴在命中带右缘（水平滚动条为下缘）往里
    // kOverlayThumbMarginPx 处；命中带比这还窄时从命中带左缘（上缘）起画。
    RECT    ComputePaintThumbRect() const;

    // 是否正在拖动滑块。public 给单测与容器判断用。
    bool    IsDragging() const            { return m_dragging; }

    // ---- 渐隐 / auto-hide 支持 ----
    //
    // 2026-10-04 起<u>默认开启</u>：新建的滚动条 alpha=0（不可见，但仍占控件位置、
    // 仍可命中），滚动（滚轮、拖动、点击轨道）或鼠标进入滚动条时用 kFadeInMs 淡入到
    // alpha=1，并启动空闲计时；kIdleHideMs 内没有新的操作就用 kFadeOutMs 淡出回 0。
    // 鼠标悬停在滚动条上、或正在拖动时，空闲计时到期也不淡出，离开 / 松开后重新计时。
    // 容器在自家 OnMouseWheel / 键盘翻页里调 TriggerShow，在鼠标离开容器时调
    // StartFadeOut。动画走 DuiAnimMgr（自带 16ms 脉冲定时器，caller 不需要在 host
    // 里另挂 60Hz pulse）。关闭 auto-hide 时 alpha 恒为 1（总是显示）。
    void    SetAutoHide(bool b);
    bool    IsAutoHide() const            { return m_autoHide; }

    // 把 alpha 拉到 1（用 fade-in 动画），并复位 idle 计时器。
    // 滚动、鼠标进入滚动条时调。
    void    TriggerShow();

    // 立即取消 idle 计时器并启动 fade-out。鼠标离开容器时调；正在拖动时不做任何事。
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

    // 拖动中：把滑块起点移到「鼠标主轴坐标 - 抓点偏移 m_dragOffsetPx」，按此换算并设置滚动位置（会通知）。
    //   mainAxisPx：鼠标在主轴上的 host 客户区坐标（竖直滚动条为 y，水平为 x）
    void    DragThumbTo(int mainAxisPx);

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

    // 尚未换算成整行滚动的滚轮增量（zDelta 单位，带符号，正为向上）。精确式触摸板
    // 与高精度滚轮一次只发很小的 zDelta，累积满一行的量才滚一行，余下的留到下一次；
    // 滚动方向反转时清零。见 OnMouseWheel。
    int         m_wheelRemainder = 0;

    // 拖拽状态。
    bool        m_dragging      = false;
    int         m_dragOffsetPx  = 0;        // 鼠标按下点到 thumb 起点的 px
    int         m_dragStartPos  = 0;

    OnScrollFn  m_fn   = nullptr;
    void*       m_user = nullptr;

    // auto-hide / fade 状态（2026-10-04 起默认开启自动隐藏，新建时不可见）
    bool        m_autoHide  = true;
    float       m_alpha     = 0.0f;     // 0..1；关闭 auto-hide 时恒为 1
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
//   · 悬浮式滚动条（2026-10-04 起）：内容始终按视口<u>全宽</u>排版，滚动条
//     浮在内容之上、贴着右缘，宽 sbWidth（命中带，默认
//     DuiScrollBar::kOverlayBandPx）。此前是内嵌式，内容宽 = 视口宽 - sbWidth。
//   · 内容控件被设到视口的<u>名义</u>矩形（高度 = SetContentHeight 给的
//     总高），按 GetScrollPos OffsetRect 移到当前可见。
//   · 内容超过视口时滚动条出现（默认自动隐藏：滚动或鼠标移进命中带时淡入，
//     停止操作后淡出）；滚动条出现时命中带内的鼠标事件归滚动条，即使它当前
//     处于隐藏态；不超时滚动条不出现、也不拦截任何鼠标事件。
//     SetScrollBarWidth(0) 完全不要滚动条。
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

    // 设置滚动条命中带的宽度（px）。0 = 不要滚动条。默认 DuiScrollBar::kOverlayBandPx(11)。
    // 滚动条浮在内容之上，宽度不影响内容的排版宽度；画出来的滑块粗细固定，见 DuiScrollBar。
    void    SetScrollBarWidth(int w);
    int     GetScrollBarWidth() const { return m_sbWidth; }

    int     GetScrollPos() const;
    void    SetScrollPos(int p);
    DuiScrollBar* GetScrollBar() { return m_sb; }

    // ---- DuiControl 覆写 ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnMouseWheel(POINT pt, short zDelta, UINT mkFlags) override;
    // 命中测试（2026-10-04 起）：滚动条出现时，命中带内的点归滚动条（它浮在内容之上，
    // 而默认实现按子控件加入顺序会先命中后加入的内容）；其余照常下沉到内容子树。
    //   ptHostClient：host 客户区坐标。返回命中的控件，不在本控件内时为 nullptr。
    DuiControl* HitTest(POINT ptHostClient) override;
    // 子孙控件的失效区域限制在本视口矩形（含滚动条那一列）之内：OnPaint 把内容
    // 裁在视口里绘制，滚出视口的部分失效了也画不出来。见 DuiControl::Invalidate。
    bool    GetChildClipRect(RECT& outClip) const override;

private:
    static void OnSbScrolled(void* user, int newPos);
    void        UpdateRange();
    void        ApplyScrollToContent();
    void        DoLayout();

private:
    DuiControl*     m_content = nullptr;       // 裸；所有权在 m_children
    DuiScrollBar*   m_sb      = nullptr;       // 裸；所有权在 m_children
    int             m_contentH = 0;
    int             m_sbWidth  = DuiScrollBar::kOverlayBandPx;   // 滚动条命中带宽（px），0 = 不要滚动条
    bool            m_autoHeight = false;
    RECT            m_contentNominalRect{};    // 内容 scrollPos=0 时的矩形
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SCROLLBAR
