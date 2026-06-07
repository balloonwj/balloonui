/**
 *  DuiToast —— 顶层浮出的轻量提示控件
 *  balloonwj@qq.com   2026-05-25
 *
 *  本文件用途:声明 DuiToast 控件 + 配套 API。Toast 是"非交互、定时
 *  自隐"的轻量提示, 典型形态如「⚠ 请先在左侧选择一个 agent」。
 *  默认布局把自己放到 host 客户区顶部水平居中, 距顶 m_topOffset。
 *  使用方:Show(text) 一行触发, 内部走 DuiAnimMgr 自驱:渐入 → 显示 → 渐出。
 */
#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_TOAST

// .cpp 必须先 include stdafx.h(项目 PCH 约定)。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiToast —— 顶部浮出的轻量提示条(无 HWND, 顶层显示, 不抢焦)
// =================================================================
//
// 用途:成功 / 失败 / 警告 / 信息提示("已保存"、"请先在左侧选择..."
// 之类), 不阻塞用户当前操作。视觉形态参考:深底 + 圆角 + 左侧图标
// + 文字。色板由 caller 完全自定义(SetBgColor / SetTextColor /
// SetIcon)。
//
// 工作机制:
//   · Show(text):设文本 → SetVisible(true) → m_alpha=0 → 启动渐入
//     动画(200ms, alpha 0→1)→ 显示等待 m_durationMs → 启动渐出
//     动画(200ms, alpha 1→0) → SetVisible(false)。整条链全部走
//     balloonui DuiAnimMgr, caller 不需要 SetTimer / Tick。
//   · 多次 Show:m_animGen++ 让旧链路 callback 失效;新文本立刻从
//     当前 alpha 开始渐入,无视觉跳变。
//   · HideNow:m_animGen++ + SetVisible(false), 立刻 hard cancel。
//   · OnPaint:先在 memDc(32bpp DIB)上画完整 toast(背景 + 图标 +
//     文字), 再用 ::AlphaBlend(SourceConstantAlpha=255*m_alpha) 一次
//     性合到 host DC —— 这样背景圆角 + 文字 + 图标都自动半透明,
//     不需要每个绘制 API 单独支持 alpha。
//   · Layout:忽略父分配的 rcAvail, 自己算"顶居中 + topOffset", 实际
//     宽度由文本宽 + 图标宽 + padding 决定, 不超过 m_maxWidth(0 = 不限)。
//   · HitTest 永远返回 nullptr —— toast 不参与点击命中。
//
// 父容器约定:把 DuiToast 加在 m_children <u>末尾</u> —— 基类 OnPaint
// 按子顺序绘制, 末尾即最上层, 自然顶层显示。typical:RootContainer
// 把 toast 作最后一个子。
//
// 代码用法:
//
//     auto toast = std::make_unique<balloonwjui::DuiToast>();
//     toast->SetBgColor(RGB(245, 158, 11));         // 警告橙
//     toast->SetTextColor(RGB(255, 255, 255));      // 白字
//     toast->SetIcon(myWarningBitmap);              // caller-owned 32bpp PARGB
//     toast->SetDurationMs(3000);                   // 3 秒
//     m_root->AddChild(std::move(toast));           // 放父子序列末尾
//     // 业务事件触发:
//     m_toast->Show(_T("请先在左侧选择一个 agent"));
//
// XML 用法:N/A(toast 是运行时浮出, 不在静态布局描述里)。
//
// 事件:无(纯展示, 不参与 WM_DUI_NOTIFY)。
class BUI_API DuiToast : public DuiControl
{
public:
    DuiToast();
    ~DuiToast() override;

    // ---- 显示 / 隐藏 ----

    // 显示一条提示。多次 Show:文本替换 + 计时重置;新链路从当前 m_alpha
    // 起继续渐入, 视觉无跳变。空文本 → 退化为 HideNow。
    //   text:提示文字。
    void Show(LPCTSTR text);

    // 立即隐藏(取消未完成动画), m_alpha 归 0。
    void HideNow();

    // 当前是否处于显示态(含渐入 / 显示 / 渐出)。
    bool IsActive() const { return m_bVisible; }

    // ---- 时长 ----

    // 显示时长(毫秒, 不含淡入 / 淡出)。默认 3000(3 秒)。
    // <= 0 视作 1(瞬间过, 仅一次 paint)。
    void SetDurationMs(int ms);
    int  GetDurationMs() const { return m_durationMs; }

    // 渐入 / 渐出动画时长(毫秒)。默认 200。0 = 硬切, 无动画。
    // 负值钳到 0。
    void SetFadeMs(int ms);
    int  GetFadeMs() const { return m_fadeMs; }

    // ---- 视觉(文本 / 底色 / 圆角)----

    // 文字色;默认白 RGB(255,255,255)。
    void     SetTextColor(COLORREF c);
    COLORREF GetTextColor() const { return m_textColor; }

    // 背景色;默认深灰 RGB(50,50,50)。
    void     SetBgColor(COLORREF c);
    COLORREF GetBgColor() const { return m_bgColor; }

    // 圆角半径(像素);默认 16。走 DuiAA::FillRoundRect 抗锯齿;最终半径
    // 会被 DuiAA 再次夹到 min(w,h)/2, 设过大也安全。
    void SetCornerRadius(int px);
    int  GetCornerRadius() const { return m_cornerRadius; }

    // ---- 阴影(投影, 让 toast 从背景"浮起")----
    //
    // 实现:OnPaint 在内容盒四周扩出留边的绘制位图上, 先画一圈"逐层叠加、
    // 由内到外渐淡"的羽化圆角阴影, 再画背景 / 文字 / 图标。留边宽度由
    // m_shadowBlur + |m_shadowOffsetY| 决定, 已计入 Layout 的 m_rcItem,
    // 故淡入淡出时阴影区一并被 Invalidate, 不残留。

    // 是否启用阴影;默认 true。关闭后 toast 退化为纯内容盒、无外扩留边。
    void SetShadowEnabled(bool on);
    bool IsShadowEnabled() const { return m_shadowEnabled; }

    // 阴影颜色;默认黑 RGB(0,0,0)。浓淡由 m_shadowAlpha 决定, 此处只定色相。
    void     SetShadowColor(COLORREF c);
    COLORREF GetShadowColor() const { return m_shadowColor; }

    // 阴影最大不透明度[0,255];默认 90。<=0 等效关闭阴影;>255 钳到 255。
    void SetShadowAlpha(int a);
    int  GetShadowAlpha() const { return m_shadowAlpha; }

    // 阴影羽化半径(像素);默认 16。决定阴影向外扩散的宽度与柔和度, 同时
    // 决定绘制位图四周留边。<=0 钳到 1(至少一层、近似硬阴影)。
    void SetShadowBlur(int px);
    int  GetShadowBlur() const { return m_shadowBlur; }

    // 阴影竖直偏移(像素, 正值向下);默认 6。让光感来自正上方, toast 下方
    // 阴影更重, 浮起感更自然。
    void SetShadowOffsetY(int px);
    int  GetShadowOffsetY() const { return m_shadowOffsetY; }

    // ---- 左侧图标 ----

    // 图标位图。HBITMAP caller-owned, 控件不 copy 不释放;nullptr(默认) =
    // 无图标。与 DuiButton::SetLeadingIcon 同模式;走 ::AlphaBlend
    // (32bpp 预乘 alpha)。
    void    SetIcon(HBITMAP hBmp);
    HBITMAP GetIcon() const { return m_icon; }

    // 图标边长(像素, 正方形);默认 16。<= 0 钳到 1。
    void SetIconSize(int px);
    int  GetIconSize() const { return m_iconSize; }

    // 图标与文字间距(像素);默认 8。< 0 钳到 0。
    void SetIconGap(int px);
    int  GetIconGap() const { return m_iconGap; }

    // ---- 字体(toast 走灰度 AA 避免 ClearType+AlphaBlend 子像素错位重影)----

    // 设置自定义字体。HFONT caller-owned, 控件不 copy 不释放;nullptr(默认)
    // 走 toast 内部默认字体(YaHei 9pt + ANTIALIASED_QUALITY)。
    // 注意:由于 toast 走 PARGB + AlphaBlend 合成, ClearType 字体会出现
    // 子像素错位"重影", 推荐 caller 传 lfQuality=ANTIALIASED_QUALITY 的字体;
    // 或直接走 SetTextPointSize wrapper, 内部自动用 AA 字体。
    void    SetFont(HFONT hFont);
    HFONT   GetFont() const { return m_font; }

    // 便捷 setter:按磅值 + 粗细从 DuiResMgr 拿 AA 缓存字体并 SetFont。
    //   pt:磅值(如 9 / 11 / 14)。pt <= 0 退化为默认(等同 SetFont(nullptr))。
    //   bold:true=FW_BOLD;false(默认)=FW_NORMAL。
    void    SetTextPointSize(int pt, bool bold = false);

    // ---- 位置 + 尺寸 ----

    // 距 host 客户区顶的偏移(像素);默认 40。
    void SetTopOffset(int px);
    int  GetTopOffset() const { return m_topOffset; }

    // 最大宽度(像素);默认 0 = 不限, 由文本宽决定 toast 宽度。
    // > 0 时, 文本超过 (maxWidth - icon - gap - 左右 padding) 后截断加 "..."。
    void SetMaxWidth(int px);
    int  GetMaxWidth() const { return m_maxWidth; }

    // ---- DuiControl 覆写 ----

    // 忽略父分配, 自定位到顶居中 + topOffset。
    void        Layout(const RECT& rcAvail) override;

    // 绘制 toast(走 memDc + AlphaBlend 二级渲染, 支持半透明)。
    void        OnPaint(HDC hdc, const RECT& rcDirty) override;

    // 永远返回 nullptr —— toast 不参与点击命中, 点击穿透到下层控件。
    DuiControl* HitTest(POINT pt) override;

    // ---- 静态 helper ----

    // 量算 toast 内容宽(图标 + gap + text + 左右 padding), 供测试 / 业务
    // 预估 toast 显示宽度。textPx:文字像素宽;hasIcon:是否带图标;
    // iconSize:图标边长;iconGap:图标与文字间距。
    static int MeasureWidth(int textPx, bool hasIcon, int iconSize, int iconGap);

    // 按 maxChars 截断文本(超过末尾加 "...")。maxChars <= 0 → 不截。
    // 主要给 SetMaxWidth 路径用:caller 算出文字宽超限时, 调本 helper 截到适合长度。
    static CString ApplyEllipsis(LPCTSTR text, int maxChars);

private:
    // 动画链路启动器, 每个对应一段(渐入 / 显示等待 / 渐出 / 收尾)。
    // 用代际号 m_animGen 让旧 callback 自动失效, 避免 cancel 不掉的 lambda。
    void StartFadeIn(int gen);
    void StartHold  (int gen);
    void StartFadeOut(int gen);
    void FinishHide();

private:
    CString  m_text;
    int      m_durationMs   = 3000;          // 显示时长(ms)
    int      m_fadeMs       = 200;           // 渐入 / 渐出时长(ms)
    COLORREF m_textColor    = RGB(255, 255, 255);
    COLORREF m_bgColor      = RGB( 50,  50,  50);
    int      m_cornerRadius = 16;            // 圆角半径(px)

    bool     m_shadowEnabled = true;         // 是否画阴影
    COLORREF m_shadowColor   = RGB(0, 0, 0); // 阴影色相
    int      m_shadowAlpha   = 90;           // 阴影最大不透明度[0,255]
    int      m_shadowBlur    = 16;           // 阴影羽化半径(px), 也是绘制位图四周留边
    int      m_shadowOffsetY = 6;            // 阴影竖直偏移(px, 正=向下)

    HBITMAP  m_icon         = nullptr;       // caller-owned;nullptr = 无图标
    int      m_iconSize     = 16;            // 图标边长(px)
    int      m_iconGap      = 8;             // 图标与文字间距(px)
    int      m_topOffset    = 40;            // 距 host 客户区顶(px)
    int      m_maxWidth     = 0;             // 最大宽度(px);0 = 不限
    HFONT    m_font         = nullptr;       // caller-owned;nullptr = 走 toast 内部 AA 默认字体

    double   m_alpha        = 0.0;           // 当前不透明度 [0,1], 0 = 全透明
    int      m_animGen      = 0;             // 动画代际号:Show / HideNow 自增, 老 callback 据此自废
};

} // namespace balloonwjui

#endif // BUI_FEATURE_TOAST
