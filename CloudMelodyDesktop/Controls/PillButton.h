#pragma once

// =============================================================================
// PillButton —— 方Music 品牌红胶囊按钮
//
// 用途：侧栏「+ 创建歌单」、Banner「立即播放」、LocalMusic「播放全部」、
//      Profile「编辑资料」等"主操作"按钮。
//
// 视觉：圆角 18px（pill），品牌红 #B61621 填充，白字。hover 切到稍亮的
//       primary-container #DA3436，pressed 切回 #B61621 略暗。
//
// 与 balloonui DuiButton 的关系：未继承 —— DuiButton 的 FillColor /
// BorderColor / kCornerRadiusPx 是 private + 硬编码的（蓝色 + 8px 圆角），
// 没暴露 setter。继承再"覆盖"反而要 friend 一堆。直接派生 DuiControl 自
// 绘 30 行 paint 代码更干净，也避免动 balloonui 库（test-first 流程）。
//
// API：
//   SetText / GetText      — 按钮文字
//   SetEnabled (基类)      — 启用 / 禁用
//
// 通知：DUIN_CLICK，extra=0。
//
// 用法：
//   auto btn = std::make_unique<PillButton>();
//   btn->SetText(_T("+ 创建歌单"));
//   btn->SetCtrlId(70);
//   sidebar->AddChild(std::move(btn));
//   // 父侧 OnDuiNotify_:
//   //   case 70 (DUIN_CLICK): OnCreatePlaylist(); break;
// =============================================================================

#include "DuiControl.h"
#include "DuiNotify.h"

namespace cloudmelody {

class PillButton : public balloonwjui::DuiControl
{
public:
    enum Style
    {
        StylePrimary   = 0,    // 红实心 + 白字（默认）
        StyleSecondary = 1,    // 灰描边 + 深灰字（"随机播放" / "分享"）
        StyleOnDark    = 2     // 透明 + 白描边 + 白字（红 banner 上的次级按钮）
    };

    PillButton() = default;

    void SetText(LPCTSTR sz);
    CString GetText() const { return m_text; }

    void SetStyle(Style s);

    // ---- DuiControl 覆写 ----
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnMouseEnter() override;
    bool OnMouseLeave() override;
    bool OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool OnSetCursor  (POINT pt) override;

private:
    CString m_text;
    Style   m_style   = StylePrimary;
    bool    m_hover   = false;
    bool    m_pressed = false;
};

} // namespace cloudmelody
