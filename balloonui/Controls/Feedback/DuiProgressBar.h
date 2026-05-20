#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_PROGRESSBAR

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiProgressBar —— 进度条（无 HWND）
// =================================================================
//
// 用途：显示一个长任务进度。三种工作模式：
//   1) 水平 determinate（默认）：实色品牌蓝填充从左到右长，按
//      m_pos / (max - min) 比例。
//   2) 竖向 determinate：填充从上往下长。
//   3) Marquee（不确定 / 流光条）：一条 MarqueeBlockPx 长的品牌蓝条
//      从一头滑到另一头。<u>本控件不自带定时器</u>，caller 用 timer 或
//      DuiAnimMgr 持续 SetMarqueePhase(0..MarqueePeriod-1)，控件按
//      phase 重画位置。
//
// 工作机制：
//   · FillRect 背景 + Rectangle 边框 + DrawText 覆盖文字。
//   · 覆盖文字优先级：m_text 非空 → 直接显示 m_text；否则若
//     m_showPercent=true 显示 "NN%"；否则不显示。字体走
//     DuiResMgr::GetDefaultFont()。
//   · SetPos 把目标值 clamp 到 [min, max]，无变化时静默返回；变化时
//     发 DUIN_VALUECHANGED（extra = 新值），可用 notify=false 抑制。
//
// 代码用法（determinate，默认 % 覆盖）：
//
//     auto pb = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
//     pb->SetRange(0, 100);
//     pb->SetPos(40);
//     row->AddChild(std::move(pb));
//     // 上传回调里：
//     m_pb->SetPos(percent);
//
// 代码用法（marquee，30Hz 定时器驱动）：
//
//     m_pb->SetMarquee(true);
//     SetTimer(IDT_MARQUEE, 33);
//     // OnTimer:
//     m_pb->SetMarqueePhase(m_pb->GetMarqueePhase() + 33);
//
// XML 用法（详细属性见 guides.html §3.3.8）：
//
//     <progress min="0" max="100" value="40"
//               show-percent="true"
//               fill-color="45,108,223"
//               fixedHeight="22"/>
//
//     <!-- marquee 模式（caller 仍需自己驱动 phase）：-->
//     <progress marquee="true" fixedHeight="22"/>
//
// 事件：
//   · DUIN_VALUECHANGED — SetPos(v, true) 改变了值时触发；
//                          extra = 新值（int）。
//                          notify=false 抑制。
class BUI_API DuiProgressBar : public DuiControl
{
public:
    enum
    {
        MarqueeBlockPx = 60,    // marquee 流光条长度（px）
        MarqueePeriod  = 1200   // marquee 一个完整循环的相位单位
    };

    DuiProgressBar();

    // 设置 / 读取取值范围。
    //   nMin / nMax：必须 nMin < nMax；当前 m_pos 不会自动 clamp。
    void    SetRange(int nMin, int nMax);
    int     GetMin() const { return m_min; }
    int     GetMax() const { return m_max; }

    // 设置 / 读取当前进度值。clamp 到 [min, max]。
    //   pos：目标值。
    //   notify：true 时触发 DUIN_VALUECHANGED；false 抑制（用于初始化）。
    void    SetPos(int pos, bool notify = true);
    int     GetPos() const { return m_pos; }

    // 设置覆盖文字。空字符串 + m_showPercent=true 时显示 "NN%"。
    //   sz：任意文字。
    void    SetText(LPCTSTR sz);

    // 当 m_text 为空时是否自动显示 "NN%"。默认 true。
    void    SetShowPercent(bool b) { m_showPercent = b; Invalidate(); }
    bool    GetShowPercent() const { return m_showPercent; }

    // 设置竖向模式。
    //   b：true = 进度从上往下长；false（默认）= 从左往右。
    void    SetVertical(bool b) { m_vertical = b; Invalidate(); }
    bool    IsVertical() const  { return m_vertical; }

    // 启用 / 关闭 marquee（不确定模式）。<u>本控件不自驱动</u> —— caller
    // 必须用 timer / DuiAnimMgr 定时调 SetMarqueePhase 才看得到流光。
    void    SetMarquee(bool b) { m_marquee = b; Invalidate(); }
    bool    IsMarquee() const  { return m_marquee; }

    // 设置 marquee 当前相位。会自动模 MarqueePeriod。
    //   phase：任意整数。
    void    SetMarqueePhase(int phase);
    int     GetMarqueePhase() const { return m_marqueePhase; }

    // 颜色覆盖。
    void    SetBgColor    (COLORREF c) { m_clrBg     = c; Invalidate(); }
    void    SetFillColor  (COLORREF c) { m_clrFill   = c; Invalidate(); }
    void    SetBorderColor(COLORREF c) { m_clrBorder = c; Invalidate(); }
    void    SetTextColor  (COLORREF c) { m_clrText   = c; Invalidate(); }

    // ---- 几何 helper（测试用）----

    // 仅水平、determinate 模式下的填充宽度（legacy 名，建议用 ComputeFillRect）。
    int     ComputeFillWidth() const;

    // determinate 模式下的填充矩形（轴感知）。
    RECT    ComputeFillRect()  const;

    // marquee 当前 phase 对应的流光条矩形。
    RECT    ComputeMarqueeRect() const;

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    int      ClampPos(int p) const;

private:
    int      m_min   = 0;
    int      m_max   = 100;
    int      m_pos   = 0;
    CString  m_text;
    bool     m_showPercent  = true;
    bool     m_vertical     = false;
    bool     m_marquee      = false;
    int      m_marqueePhase = 0;

    COLORREF m_clrBg     = RGB(232, 232, 236);
    COLORREF m_clrFill   = RGB( 45, 108, 223);   // 品牌蓝 #2D6CDF
    COLORREF m_clrBorder = RGB(180, 180, 188);
    COLORREF m_clrText   = RGB( 30,  30,  30);
};

} // namespace balloonwjui

#endif // BUI_FEATURE_PROGRESSBAR
