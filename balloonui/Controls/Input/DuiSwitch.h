#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_SWITCH

#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiSwitch —— iOS 风格圆角胶囊开关（无 HWND）
// =================================================================
//
// 用途：单个布尔状态的两态切换器，常用于"通知开关 / 静音 / 自动下载 /
// 保留聊天记录"等设置项，比 Checkbox 更"开关感"，色彩对比明确。
//
// 工作机制：
//   · 圆角胶囊外形（半径 = 高度 / 2）；off 态底色浅灰 #E5E5E5，on 态底
//     色品牌绿 #07C160（某信绿），白色圆形滑块嵌在内部、距边 2px。
//   · 切换时滑块从一侧滑到另一侧，底色同步从 off 色 fade 到 on 色，时长
//     150ms、缓动函数 EaseOutCubic。动画通过 DuiAnimMgr / DuiDoubleAnim
//     驱动；host 必须周期性调 DuiAnimMgr::Inst().TickAll(GetTickCount())
//     才看得到中间帧（DuiGallery 的 GalleryFrame 就这么干）。
//   · 鼠标在控件任意位置点松（按下 + 松开都在控件内）即翻转状态；拖出
//     再松不翻转（与 DuiButton::Checkbox 一致）。
//   · 键盘 Space / Enter 翻转；其他键不消费、由 host 上冒。
//   · 是 tab stop —— 接收键盘焦点，但<u>不画</u>焦点虚线框（iOS /
//     某信原生 toggle 也没有），靠胶囊本身的状态色作为唯一视觉反馈。
//   · Disabled 时控件整体 60% 透明，鼠标 / 键盘均不响应。
//   · 默认尺寸 46×24px（layout 时如果给的高度不同，胶囊半径自动等于
//     高度 / 2；travel 距离自动按宽度算 —— SetRect 给什么大小都自适应）。
//
// 代码用法：
//
//     auto sw = std::unique_ptr<DuiSwitch>(new DuiSwitch());
//     sw->SetCtrlId(IDC_MUTE_NOTIF);
//     sw->SetChecked(true, /*animated=*/false);   // 初始化不动画
//     parent->AddChild(std::move(sw));
//     // 父对话框的 OnDuiNotify：
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_MUTE_NOTIF)
//     //       SetMuteFlag(n.extra != 0);
//
// XML 用法：
//
//     <switch id="100" checked="false" enabled="true" animated="true"
//             on-color="7,193,96" off-color="229,229,229"
//             knob-color="255,255,255"
//             fixedWidth="46" fixedHeight="24"/>
//
// 事件：
//   · DUIN_VALUECHANGED — 鼠标点击 / Space / Enter 触发状态翻转时发；
//                          extra = 1（on）或 0（off）。SetChecked()
//                          编程调用<u>不</u>触发，匹配 DuiButton 行为。
//
// Replaces: 无（balloonui 之前没有原生 toggle 控件）。
class BUI_API DuiSwitch : public DuiControl
{
public:
    DuiSwitch();
    ~DuiSwitch() override;

    // 设置 / 读取勾选状态。
    //   checked：true = on；false = off。
    //   animated：true = 走 150ms 动画到目标状态；false = 立即跳变并取消
    //             任何已在跑的动画。<u>不</u>受 SetAnimated 全局开关影响 ——
    //             显式参数永远赢，"我现在就是要瞬间设值"必须有出口。
    //   notify：true = 触发 DUIN_VALUECHANGED；false = 静默改值。
    void    SetChecked(bool checked, bool animated = true, bool notify = false);
    bool    IsChecked() const  { return m_checked; }

    // 全局动画开关（影响"鼠标 / 键盘"路径的默认 animated 值；不影响
    // SetChecked 的显式参数）。默认 true。
    //   b：false = 鼠标 / 键盘翻转也走"瞬间跳变"，节省 CPU；true = 走 150ms
    //      动画。
    void    SetAnimated(bool b) { m_animatedDefault = b; }
    bool    IsAnimated() const  { return m_animatedDefault; }

    // 0..1 之间的当前动画进度（0 = 完全 off 视觉；1 = 完全 on 视觉）。
    // off → on 时从 0 起爬到 1；on → off 时从 1 起降到 0。Paint 用它插
    // 值滑块 X 坐标 + 底色。暴露给测试。
    double  GetProgress() const { return m_progress; }

    // 颜色覆盖。默认 on=#07C160（某信绿）、off=#E5E5E5（浅灰）、knob=
    // #FFFFFF（白）。同色赋值不重画，避免主题刷新触发无效 Invalidate。
    void    SetOnColor  (COLORREF c)
    {
        if (m_clrOn == c) { return; }
        m_clrOn = c;
        Invalidate();
    }
    COLORREF GetOnColor () const     { return m_clrOn; }
    void    SetOffColor (COLORREF c)
    {
        if (m_clrOff == c) { return; }
        m_clrOff = c;
        Invalidate();
    }
    COLORREF GetOffColor() const     { return m_clrOff; }
    void    SetKnobColor(COLORREF c)
    {
        if (m_clrKnob == c) { return; }
        m_clrKnob = c;
        Invalidate();
    }
    COLORREF GetKnobColor() const    { return m_clrKnob; }

    // ---- 几何 helper（暴露给测试）----

    // 当前滑块圆心 X 坐标（host-客户区像素），按 m_progress 在 off / on
    // 两端点之间线性插值。Y 永远是控件中线。
    int     ComputeKnobCenterX() const;

    // ---- DuiControl 覆写 ----
    void    OnPaint     (HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool    OnKeyDown   (UINT vk, UINT flags) override;
    bool    OnSetCursor (POINT pt) override;

private:
    // 内部翻转：把 m_checked / m_progress 朝 target 移动；走或不走动画
    // 由 animated 决定。notify=true 时发 DUIN_VALUECHANGED（只在逻辑状态
    // 变化时才发）。逻辑状态没变时通常 no-op；但 animated=false 且
    // m_progress 还没到端点的情况下会强制 snap，保证"立即到位"语义。
    void    ApplyTarget(bool target, bool animated, bool notify);

    // 取消当前在跑的动画：把现有 token 的 owner 置 null，让 mgr 中残留
    // 的回调下次 fire 走 dead 路径 no-op，再换一个新 token 给后续动画用。
    // 没有未完成动画时跳过 swap。
    void    CancelAnim();

private:
    bool     m_checked         = false;
    double   m_progress        = 0.0;       // 0 = off, 1 = on
    bool     m_animatedDefault = true;
    bool     m_pressed         = false;     // 鼠标按下且仍在控件内
    bool     m_animPending     = false;     // DuiAnimMgr 持有未完成回调

    COLORREF m_clrOn   = RGB(  7, 193,  96);    // 某信绿
    COLORREF m_clrOff  = RGB(229, 229, 229);    // 浅灰
    COLORREF m_clrKnob = RGB(255, 255, 255);    // 白

    // Owner-shared back-pointer for anim setter callbacks. Lambdas capture
    // the shared_ptr by value, so the token outlives this control. Dtor /
    // CancelAnim flips owner = nullptr; subsequent fires read it through
    // the still-alive shared block and no-op safely.
    struct AnimToken
    {
        DuiSwitch* owner = nullptr;
    };
    std::shared_ptr<AnimToken> m_animToken;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SWITCH
