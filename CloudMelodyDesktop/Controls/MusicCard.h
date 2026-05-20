#pragma once

// =============================================================================
// MusicCard —— 通用"封面 + 标题 + 副标题"方卡（专辑 / 歌单 / 播客通用）
//
// 视觉：
//   ┌────────┐
//   │        │  封面（占满卡片宽度，正方形，圆角 8px）
//   │  cover │  hover 时右下角浮一个圆形 ▶ 播放徽章（半透明白底 + 红三角）
//   │        │
//   ├────────┤
//   │ Title  │  14px 主文（kColorOnSurface）
//   │ Sub    │  11px 副文（kColorOnSurfaceVar）
//   └────────┘
//
// 实现：DuiControl 派生，OnPaint 自绘整个卡片。把封面 / 标题 / 副标题画成
// 三块即可，不引入 child 控件，避免布局开销 / 状态同步。封面图未挂时画
// 默认深灰圆角矩形占位（M3 第一版 ship 不带 cover PNG）。
//
// API：
//   SetTitle / SetSubtitle / SetSize（卡片边长，正方形封面 + 文字区）
//
// 通知：DUIN_CLICK 整卡点击。
//
// 替代 / 用途：DiscoverPage 的"精选歌单"网格、Podcast 的"热门节目"、
//            Favorites 的"喜欢的歌手 & 艺人"等所有正方形封面卡。
// =============================================================================

#include "DuiControl.h"
#include "DuiNotify.h"

namespace cloudmelody {

class MusicCard : public balloonwjui::DuiControl
{
public:
    MusicCard() = default;

    void SetTitle(LPCTSTR sz);
    void SetSubtitle(LPCTSTR sz);
    void SetHueSeed(int seed) { m_hueSeed = seed; Invalidate(); }

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnMouseEnter() override;
    bool OnMouseLeave() override;
    bool OnLButtonUp(POINT pt, UINT mkFlags) override;
    bool OnSetCursor(POINT pt) override;

private:
    CString m_title;
    CString m_subtitle;
    int     m_hueSeed = 0;
    bool    m_hover = false;
};

} // namespace cloudmelody
