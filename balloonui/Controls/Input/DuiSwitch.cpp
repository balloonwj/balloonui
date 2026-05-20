#include "stdafx.h"
#include "DuiSwitch.h"

#if BUI_FEATURE_SWITCH

#include "../../DuiPaintAA.h"
#include "../../DuiAnimation.h"
#include <memory>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

// ---- 设计常量 -------------------------------------------------------

// 切换动画时长。150ms 是 iOS / Material 标准 toggle 时长 —— 比 200ms
// 更紧凑，避免"拖"的感觉；比 100ms 长一点，能看清状态在动而不是闪。
// 修改这个值前先在 DuiGallery 里手动确认手感。
static const int kSwitchAnimMs = 150;

// 滑块距胶囊外缘的内边距（px）。2 是 iOS 系统 toggle 的标准内距 ——
// 给滑块一圈"呼吸空间"，太小会粘边、太大滑块就太瘦。胶囊半径 = 高度/2，
// 滑块半径 = 高度/2 - kKnobInset。
static const int kKnobInset = 2;

// 默认尺寸（DuiResMgr 已经支持 DPI 缩放，这里给的是 96dpi 的逻辑像素）。
// 46×24 与某信 / iOS 默认 toggle 视觉大小一致。layout 不强加这两个值；
// 业务可以 SetRect 给任意 (w, h)，本控件按实际矩形等比缩放。
static const int kDefaultWidth  = 46;
static const int kDefaultHeight = 24;

// ---- helpers --------------------------------------------------------

// COLORREF 三通道线性插值。t in [0, 1]：t=0 返 a，t=1 返 b。
static COLORREF LerpColor(COLORREF a, COLORREF b, double t)
{
    if (t < 0.0)
    {
        t = 0.0;
    }
    if (t > 1.0)
    {
        t = 1.0;
    }
    int ar = GetRValue(a);
    int ag = GetGValue(a);
    int ab = GetBValue(a);
    int br = GetRValue(b);
    int bg = GetGValue(b);
    int bb = GetBValue(b);
    int rr = ar + (int)((br - ar) * t);
    int rg = ag + (int)((bg - ag) * t);
    int rb = ab + (int)((bb - ab) * t);
    return RGB(rr, rg, rb);
}

// 把 8-bit 通道按 alpha (0..1) 朝 255（白底假设）混色，做"60% 透明"
// disabled 视觉效果。"朝白色 lerp 1 - alpha"的近似 —— 对浅色控件足够；
// 真要严格 alpha-blend 得走 GDI+ 复合操作，本控件无必要。
static COLORREF FadeOnWhite(COLORREF c, double alpha)
{
    return LerpColor(RGB(255, 255, 255), c, alpha);
}

// ====================================================================

DuiSwitch::DuiSwitch()
{
    SetTabStop(true);
    // 给一个合理的默认 rect，方便业务"没显式设矩形"也能立即可见 ——
    // 实际进 layout 后会被父容器的 SetRect 覆写。
    m_rcItem = RECT{ 0, 0, kDefaultWidth, kDefaultHeight };
    m_animToken = std::make_shared<AnimToken>();
    m_animToken->owner = this;
}

DuiSwitch::~DuiSwitch()
{
    // DuiAnimMgr 仍可能持有指向本控件的 setter 闭包；把 owner 置 null，
    // 让闭包下次 fire 时走"owner 已死"分支安全 no-op。闭包通过 shared_ptr
    // 保持 token 存活，所以读 owner 字段本身永远安全。
    if (m_animToken)
    {
        m_animToken->owner = nullptr;
    }
}

void DuiSwitch::ApplyTarget(bool target, bool animated, bool notify)
{
    double targetProgress = target ? 1.0 : 0.0;
    bool   stateChanged   = (target != m_checked);
    // 没有逻辑状态变化时也得放过"animated=false 且尚未到端点"的请求 ——
    // 否则在动画中途调 SetChecked(sameTarget, animated=false) 想瞬间钉到
    // 终点，会被早退兜底吞掉。
    if (!stateChanged && (animated || m_progress == targetProgress))
    {
        return;
    }
    m_checked   = target;
    double from = m_progress;
    double to   = targetProgress;

    CancelAnim();

    if (animated && from != to)
    {
        // 拷贝 token 给 anim 闭包；若 owner 已置 null，回调走 dead 路径 no-op。
        auto token = m_animToken;
        auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
            kSwitchAnimMs, from, to,
            [token](double v)
            {
                if (token->owner)
                {
                    token->owner->m_progress = v;
                    token->owner->Invalidate();
                }
            }));
        a->SetEasing(&DuiEase::EaseOutCubic);
        // 完成时把进度精确钉到端点（浮点误差兜底）+ 清 pending 标志。
        a->SetOnComplete([token, to]()
        {
            if (token->owner)
            {
                token->owner->m_progress    = to;
                token->owner->m_animPending = false;
                token->owner->Invalidate();
            }
        });
        DuiAnimMgr::Inst().Add(std::move(a));
        m_animPending = true;
    }
    else
    {
        m_progress = to;
        Invalidate();
    }

    if (notify && stateChanged)
    {
        NotifyParent(DUIN_VALUECHANGED, m_checked ? 1 : 0);
    }
}

void DuiSwitch::CancelAnim()
{
    // DuiAnimMgr 没有 Cancel(token) API；要"取消"只能把 token 的 owner
    // 置 null + 换一个新 token —— 老 token 的回调下次 fire 时会读到
    // owner==null，走 dead 路径 no-op。没有 in-flight 回调时整个 swap
    // 是浪费，跳过即可。
    if (!m_animPending)
    {
        return;
    }
    if (m_animToken)
    {
        m_animToken->owner = nullptr;
    }
    m_animToken = std::make_shared<AnimToken>();
    m_animToken->owner = this;
    m_animPending = false;
}

void DuiSwitch::SetChecked(bool checked, bool animated, bool notify)
{
    ApplyTarget(checked, animated, notify);
}

int DuiSwitch::ComputeKnobCenterX() const
{
    int h     = m_rcItem.bottom - m_rcItem.top;
    int knobR = (h / 2) - kKnobInset;
    if (knobR < 1)
    {
        knobR = 1;
    }
    int leftCx  = m_rcItem.left  + kKnobInset + knobR;
    // OnPaint 用 RECT { cx-knobR, ..., cx+knobR+1, ... } 画滑块（GDI 排他
    // 右边界），所以滑块覆盖像素 [cx-knobR, cx+knobR]。让右内距等于左内距：
    // cx + knobR == m_rcItem.right - kKnobInset - 1 → cx = right - inset - knobR - 1。
    int rightCx = m_rcItem.right - kKnobInset - knobR - 1;
    if (rightCx < leftCx)
    {
        rightCx = leftCx;
    }
    double t = m_progress;
    if (t < 0.0)
    {
        t = 0.0;
    }
    if (t > 1.0)
    {
        t = 1.0;
    }
    return leftCx + (int)((rightCx - leftCx) * t);
}

void DuiSwitch::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    int w = m_rcItem.right - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // Disabled 时 60% alpha-on-white 近似 —— 真实 alpha 要走 GDI+ 合成，
    // 本控件无必要。
    double dimAlpha = m_bEnabled ? 1.0 : 0.4;
    COLORREF bg   = LerpColor(m_clrOff, m_clrOn, m_progress);
    bg            = FadeOnWhite(bg, dimAlpha);
    COLORREF knob = FadeOnWhite(m_clrKnob, dimAlpha);

    // 胶囊背景：用 GDI+ GraphicsPath 一次画完整圆角矩形（半径 = 高/2）。
    // 之前用"左 AA 半圆 + 中间 GDI FillRect + 右 AA 半圆"三块拼接 —— 半
    // 圆边缘是 GDI+ 抗锯齿渐变像素，中间矩形顶/底是 GDI FillRect 的 hard
    // edge，两类像素拼接处看到明显锯齿。整条 path 一次 FillPath，整圈
    // 边缘统一走 AntiAlias，没有接缝。
    int radius = h / 2;
    {
        Gdiplus::Graphics gp(hdc);
        gp.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        gp.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        Gdiplus::REAL x = (Gdiplus::REAL)m_rcItem.left;
        Gdiplus::REAL y = (Gdiplus::REAL)m_rcItem.top;
        Gdiplus::REAL fW = (Gdiplus::REAL)(w - 1);   // -1 把 path 收进整数 rect
        Gdiplus::REAL fH = (Gdiplus::REAL)(h - 1);
        Gdiplus::REAL r = (Gdiplus::REAL)radius;
        if (r * 2 > fH)
        {
            r = fH / 2;                               // 防极端宽高比
        }

        Gdiplus::GraphicsPath path;
        path.AddArc(x,             y,             r * 2, fH, 90,  180); // 左半圆
        path.AddArc(x + fW - r * 2, y,            r * 2, fH, 270, 180); // 右半圆
        path.CloseFigure();

        Gdiplus::SolidBrush br(Gdiplus::Color(255,
                                              GetRValue(bg),
                                              GetGValue(bg),
                                              GetBValue(bg)));
        gp.FillPath(&br, &path);
    }

    // 滑块圆。半径 = 胶囊半径 - inset；圆心 X 由 progress 插值；圆心
    // Y 永远是控件中线。
    int knobR = radius - kKnobInset;
    if (knobR < 1)
    {
        knobR = 1;
    }
    int cx = ComputeKnobCenterX();
    int cy = (m_rcItem.top + m_rcItem.bottom) / 2;
    RECT rcKnob = { cx - knobR, cy - knobR,
                    cx + knobR + 1, cy + knobR + 1 };
    // 滑块给一圈极淡的灰描边，避免在浅色 off 底色上"消失"。
    COLORREF knobBorder = FadeOnWhite(RGB(180, 180, 180), dimAlpha);
    DuiAA::FillEllipse(hdc, rcKnob, knob, knobBorder);
    // 不画焦点虚线框：iOS / 某信原生 toggle 没有焦点指示框，加了反而和
    // 胶囊外缘冲突显脏。键盘焦点仍然可以通过 Space / Enter 触发翻转，
    // tab-stop 行为不受影响 —— 只是没有视觉提示。
}

bool DuiSwitch::OnLButtonDown(POINT pt, UINT)
{
    if (!m_bEnabled)
    {
        return false;
    }
    m_pressed = true;
    Capture();
    SetFocus();
    (void)pt;
    return true;
}

bool DuiSwitch::OnLButtonUp(POINT pt, UINT)
{
    if (!m_bEnabled)
    {
        return false;
    }
    bool wasPressed = m_pressed;
    m_pressed = false;
    if (m_bCapture)
    {
        ReleaseCapture();
    }
    if (!wasPressed)
    {
        return false;
    }
    // Drag-out cancels：只有松开点也在控件内才算一次 click。
    if (!::PtInRect(&m_rcItem, pt))
    {
        Invalidate();
        return true;
    }
    ApplyTarget(!m_checked, m_animatedDefault, /*notify=*/true);
    return true;
}

bool DuiSwitch::OnKeyDown(UINT vk, UINT)
{
    if (!m_bEnabled)
    {
        return false;
    }
    if (vk == VK_SPACE || vk == VK_RETURN)
    {
        ApplyTarget(!m_checked, m_animatedDefault, /*notify=*/true);
        return true;
    }
    return false;
}

bool DuiSwitch::OnSetCursor(POINT pt)
{
    if (m_bEnabled && ::PtInRect(&m_rcItem, pt))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
        return true;
    }
    return false;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SWITCH
