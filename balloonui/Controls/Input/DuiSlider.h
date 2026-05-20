#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_SLIDER

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiSlider —— 数值滑块（无 HWND）
// =================================================================
//
// 用途：让用户在 [min, max] 区间里拖动一个圆球选数值。典型场景：音量、
// 亮度、缩放、单字段筛选条件。
//
// 工作机制：
//   · 默认水平方向：rail（轨道）从左到右铺，左侧到 thumb 之间是品牌蓝
//     "已填充"段，剩下是浅灰 track 段。SetVertical(true) 翻成竖向，
//     轨道从上到下、值<u>向下</u>递增（与 Win32 ScrollBar / volume 一致；
//     若想反过来 "下小上大"，自己 SetReverse / 业务侧反转）。
//   · 鼠标点 thumb 抓拖（DUI capture）；点轨道空白则按 m_lineSize 朝
//     点击点跳一步；松键结束拖动并释放 capture。
//   · 鼠标滚轮 1 detent = ±lineSize；键盘 Left/Right（横向）或 Up/Down
//     （竖向）= ±lineSize，Home/End 跳到端点。
//   · 可选刻度线 —— SetTickFrequency(n) 每 n 个值-单位画一根小刻度。
//     0（默认）= 不画。
//   · thumb 是抗锯齿圆（DuiPaintAA），画在轨道之上保平滑。
//   · 是 tab stop —— 接收键盘焦点。
//
// 代码用法：
//
//     auto slider = std::make_unique<DuiSlider>();
//     slider->SetRange(0, 100);
//     slider->SetPos(40);
//     slider->SetLineSize(5);
//     slider->SetTickFrequency(10);
//     slider->SetCtrlId(IDC_VOLUME);
//     parent->AddChild(std::move(slider));
//     // 父对话框的 OnDuiNotify：
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_VOLUME)
//     //       UpdateVolume((int)n.extra);
//
// XML 用法（详细属性见 guides.html §3.3.10）：
//
//     <slider id="100" min="0" max="100" value="40"
//             line-size="5" tick-frequency="10"
//             vertical="false"
//             fill-color="45,108,223" thumb-color="255,255,255"
//             fixedHeight="32"/>
//
// 事件：
//   · DUIN_VALUECHANGED — 拖动 / 轨道点击 / 滚轮 / 键盘调整时触发。
//                          extra = 新位置（int）。
class BUI_API DuiSlider : public DuiControl
{
public:
    DuiSlider();

    // 设置 / 读取取值范围。SetRange 之后当前 m_pos 会被自动 clamp 到新范围。
    //   nMin / nMax：范围下限 / 上限（必须 nMin < nMax）。
    void    SetRange(int nMin, int nMax);
    int     GetMin() const { return m_min; }
    int     GetMax() const { return m_max; }

    // 设置 / 读取当前值。SetPos 自动 clamp 到 [min, max] 区间。
    //   pos：目标值。
    //   notify：true 时触发 DUIN_VALUECHANGED；false 时只更新内部状态
    //           不通知（用于初始化避免"刚 ShowWindow 就发一次假事件"）。
    void    SetPos(int pos, bool notify = true);
    int     GetPos() const { return m_pos; }

    // 设置 / 读取键盘 / 滚轮 / 轨道点击的步长。
    //   n：> 0；<= 0 会被 clamp 到 1。默认 1。
    void    SetLineSize(int n) { m_lineSize = n > 0 ? n : 1; }
    int     GetLineSize() const { return m_lineSize; }

    // 设置轨道粗细（垂直于线方向的厚度，px）。默认 4。
    //   h：> 0；<= 0 会被 clamp 到 1。
    void    SetTrackHeight(int h) { m_trackH = h > 0 ? h : 1; Invalidate(); }

    // 设置 thumb 圆直径（px）。默认 14（半径 7）。
    //   s：> 1；<= 1 会被 clamp 到 1。
    void    SetThumbSize  (int s) { m_thumbR = s > 1 ? s / 2 : 1; Invalidate(); }

    // 颜色覆盖。默认按品牌色：track 浅灰、fill 品牌蓝、thumb 白。
    void    SetTrackColor (COLORREF c) { m_clrTrack = c; Invalidate(); }
    void    SetFillColor  (COLORREF c) { m_clrFill  = c; Invalidate(); }
    void    SetThumbColor (COLORREF c) { m_clrThumb = c; Invalidate(); }

    // 设置 / 读取竖向模式。竖向时轨道从上到下，值<u>向下</u>递增。
    //   b：true = 竖向；false（默认）= 水平。
    void    SetVertical(bool b) { m_vertical = b; Invalidate(); }
    bool    IsVertical() const  { return m_vertical; }

    // 设置 / 读取刻度间隔（值-单位为间）。0 = 不画刻度（默认）。
    //   n：>= 0；< 0 会被 clamp 到 0。
    void    SetTickFrequency(int n) { m_tickFreq = n < 0 ? 0 : n; Invalidate(); }
    int     GetTickFrequency() const { return m_tickFreq; }

    // 设置刻度线颜色。默认 RGB(150, 150, 160)。
    void    SetTickColor(COLORREF c) { m_clrTick = c; Invalidate(); }

    // ---- 几何 helper（暴露给测试）----

    // 计算当前 thumb 圆心（host-客户区坐标）。轴向感知。
    POINT   ComputeThumbCenter() const;

    // 给定 X 像素，反推对应位置值。<u>仅</u>水平场景（保留 legacy 名）。
    int     PosFromPixelX(int x) const;

    // 给定 host-客户区坐标点，反推对应位置值。轴向感知，竖横通用。
    int     PosFromPoint  (POINT pt) const;

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool    OnMouseMove  (POINT pt, UINT mkFlags) override;
    bool    OnMouseWheel (POINT pt, short zDelta, UINT mkFlags) override;
    bool    OnKeyDown    (UINT vk, UINT flags) override;
    bool    OnSetCursor  (POINT pt) override;

private:
    int     ClampPos(int p) const;
    RECT    TrackRect() const;
    RECT    ThumbHitRect() const;     // thumb 圆周围的方形 hit-test 框

private:
    int      m_min       = 0;
    int      m_max       = 100;
    int      m_pos       = 0;
    int      m_lineSize  = 1;

    int      m_trackH    = 4;         // 轨道厚度
    int      m_thumbR    = 7;         // thumb 圆半径
    bool     m_vertical  = false;
    int      m_tickFreq  = 0;         // 0 = 不画刻度

    bool     m_dragging  = false;

    COLORREF m_clrTrack       = RGB(220, 220, 226);
    COLORREF m_clrFill        = RGB( 45, 108, 223);   // 品牌蓝
    COLORREF m_clrThumb       = RGB(255, 255, 255);
    COLORREF m_clrThumbBorder = RGB( 30,  74, 153);
    COLORREF m_clrTick        = RGB(150, 150, 160);
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SLIDER
