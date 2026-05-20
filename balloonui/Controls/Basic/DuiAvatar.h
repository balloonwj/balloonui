#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_AVATAR

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiAvatar —— 用户头像控件（无 HWND）
// =================================================================
//
// 用途：聊天列表 / 会话标题 / 个人资料卡上的圆形或圆角矩形头像；
// 可挂图也可以"姓名缩写 + 实色背景"做 fallback；右下角支持在线状态点。
//
// 工作机制：
//   · 源图是 caller 提供的 HBITMAP，控件 <b>不</b>持有所有权，caller 负责
//     在控件存活期间保活 bitmap。配合 DuiAsyncImage 异步加载磁盘 / 网络
//     头像更顺手。
//   · 没设 bitmap 时画 fallback —— 实色圆 / 圆角矩形 + 居中最多 2 字符
//     缩写。缩写算法：
//       "Alice"        -> "A"
//       "Alice Smith"  -> "AS"
//       "alice b cox"  -> "AC"   （首词 + 末词首字母）
//       "苏文嘉"        -> "苏文"  （前两字）
//       "X"            -> "X"
//   · 状态点位于 75% 宽 × 75% 高处，外圈白底（让点在深色头像上仍可见），
//     内圈是按 status 取的品牌色。直径约头像边长的 25%，最小 8px。
//   · 圆 / 圆角矩形 / 状态点的圆都走 DuiAA helper 抗锯齿，对角不锯齿。
//
// 代码用法：
//
//     auto av = std::unique_ptr<DuiAvatar>(new DuiAvatar());
//     av->SetName(_T("Alice Smith"));            // 没图时 fallback 显示 "AS"
//     av->SetBitmap(hAvatarBmp);                 // 可选；caller 持有 hbm
//     av->SetShape(DuiAvatar::ShapeCircle);
//     av->SetStatus(DuiAvatar::StatusOnline);
//     av->SetRect(RECT{ 8, 8, 56, 56 });
//     parent->AddChild(std::move(av));
//
// XML 用法（详细属性见 guides.html §3.3.6）：
//
//     <avatar name="苏文嘉"
//             shape="circle"
//             status="online"
//             fallback-bg-color="45,108,223"
//             initials-color="255,255,255"
//             fixedWidth="48" fixedHeight="48"/>
//
//   注意：XML 不支持 image="path" —— 图位图加载交由 caller 在 builder 返回
//   后用 SetBitmap(hbm) 自己挂上（典型场景：先用 fallback 占位，async
//   拉到真头像再回填）。
//
// 事件：无（纯绘制控件，不发 WM_DUI_NOTIFY）。
class BUI_API DuiAvatar : public DuiControl
{
public:
    // 形状。
    enum Shape
    {
        ShapeCircle    = 0,    // 默认，正圆
        ShapeRoundRect = 1     // 圆角矩形，圆角半径由 SetCornerRadius 控制
    };

    // 在线状态点。
    enum Status
    {
        StatusNone    = 0,     // 不画状态点（默认）
        StatusOnline  = 1,     // 绿
        StatusAway    = 2,     // 黄
        StatusBusy    = 3,     // 红
        StatusOffline = 4      // 灰
    };

    DuiAvatar();

    // 设置 / 读取头像源位图。caller 持有所有权，控件不会 DeleteObject。
    //   hbm：HBITMAP；nullptr 表示清空，回到 fallback 模式。
    void    SetBitmap(HBITMAP hbm);
    HBITMAP GetBitmap() const { return m_hBitmap; }

    // 设置 / 读取 fallback 缩写所用的姓名。SetName 会触发 ComputeInitials
    // 重新算缩写。空字符串只画一个有色形状不带文字。
    //   name：完整姓名（中英文均可）。
    void    SetName(LPCTSTR name);
    CString GetName() const { return m_name; }

    // 设置 / 读取 fallback 模式下的背景色。默认品牌蓝 RGB(45,108,223)
    // （与 DuiButton::StylePushButton normal fill 一致）。
    //   c：COLORREF（用 RGB(r,g,b) 构造）。
    void    SetFallbackBgColor(COLORREF c);
    COLORREF GetFallbackBgColor() const { return m_fallbackBg; }

    // 设置 / 读取缩写文字颜色。默认白。
    //   c：COLORREF。
    void    SetInitialsColor(COLORREF c);
    COLORREF GetInitialsColor() const { return m_initialsClr; }

    // 设置 / 读取形状（圆 / 圆角矩形）。SetShape 触发重绘。
    //   s：ShapeCircle 或 ShapeRoundRect。
    void    SetShape(Shape s);
    Shape   GetShape() const { return m_shape; }

    // 设置 / 读取圆角矩形的角半径（仅 ShapeRoundRect 时生效）。默认 8px。
    //   r：>= 0 的整数。
    void    SetCornerRadius(int r);
    int     GetCornerRadius() const { return m_cornerR; }

    // 设置 / 读取状态点。默认 StatusNone（不画）。
    //   st：5 个 Status 常量之一。
    void    SetStatus(Status st);
    Status  GetStatus() const { return m_status; }

    // ---- 静态 helper（暴露给测试用）----

    // 给定 status 返回对应品牌色。无副作用，单测可直接调，不需要构造控件。
    //   st：Status 常量。
    //   返回：该状态对应的内圈品牌色 COLORREF。
    static COLORREF GetStatusDotColor(Status st);

    // 计算姓名的最多 2 字符大写缩写。纯函数（不需要 DC / GDI）。
    // 见类头部注释顶端的"缩写算法"举例。
    //   name：完整姓名，可中可英可空。
    //   返回：1–2 字符大写缩写；空入 → 空 CString。
    static CString ComputeInitials(LPCTSTR name);

    // ---- DuiControl 覆写 ----

    // 在自身 m_rcItem 内画头像（含 bitmap 或 fallback + 状态点）。
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    HBITMAP   m_hBitmap     = nullptr;
    CString   m_name;
    COLORREF  m_fallbackBg  = RGB(45, 108, 223);    // 品牌蓝
    COLORREF  m_initialsClr = RGB(255, 255, 255);
    Shape     m_shape       = ShapeCircle;
    int       m_cornerR     = 8;                    // px (仅 RoundRect)
    Status    m_status      = StatusNone;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_AVATAR
