#include "stdafx.h"
#include "SessionListItem.h"
#include "MockData.h"

#include "DuiResMgr.h"
#include "DuiDpi.h"
#include "DuiPaintAA.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace xchat {

// ---- 行布局常量（px，逻辑像素；DPI 缩放靠 ::MulDiv 在 EnsureFonts 里做） --

static const int kRowHeightPx        = 68;     // 行高
static const int kPadLeftPx          = 14;     // 行左内距
static const int kPadRightPx         = 14;     // 行右内距
static const int kAvatarSizePx       = 40;     // 头像方边
static const int kAvatarRadiusPx     = 8;      // 头像圆角
static const int kTextLeftGapPx      = 12;     // 头像 → 文本左距
static const int kNameTopOffsetPx    = 12;     // name baseline ≈ 行顶 + 12
static const int kPreviewTopOffsetPx = 38;     // preview baseline ≈ 行顶 + 38
static const int kRightInfoSizePx    = 14;     // bell/badge 圆形直径
static const int kBadgePadXPx        = 6;      // unread > 9 时 badge 拉宽的两侧内距

// ---- 颜色 -----------------------------------------------------------------

static const COLORREF kRowSelBg     = RGB(228, 228, 228);  // 选中行底；和 panel #F5F5F5 拉开 17 灰阶才看得清
static const COLORREF kRowHoverBg   = RGB(238, 238, 238);  // hover 行底（介于 panel 和 selected 之间）
static const COLORREF kAvatarText   = RGB(255, 255, 255);  // 头像内白字
static const COLORREF kNameColor    = RGB( 26,  26,  26);  // 主标题深灰
static const COLORREF kTimeColor    = RGB(153, 153, 153);  // 时间浅灰
static const COLORREF kPreviewColor = RGB(136, 136, 136);  // 预览文本浅灰（比时间略深一档）
static const COLORREF kMuteRingColor= RGB(170, 170, 170);  // 静音铃灰
static const COLORREF kBadgeBg      = RGB(250,  81,  81);  // 未读 badge 红
static const COLORREF kBadgeText    = RGB(255, 255, 255);

// ---- 字体缓存（进程级）----------------------------------------------------
//
// 默认字体走 DuiResMgr（9pt YaHei），name 大半号 11pt。两个字体进程内只
// 创建一次；exe 关时 OS 自动回收，不显式 DeleteObject —— demo 不需要细致
// 资源管理，加上销毁时机也不好定（GDI 还在用就会泄露，加锁更不值）。

static HFONT EnsureNameFont()
{
    static HFONT s_hNameFont = nullptr;
    if (s_hNameFont)
    {
        return s_hNameFont;
    }
    int dpi = balloonwjui::DuiDpi::GetSystemDpi();
    LOGFONT lf = {};
    lf.lfHeight          = -::MulDiv(11, dpi, 72);
    lf.lfWeight          = FW_NORMAL;             // 某信里 name 不是粗体
    lf.lfCharSet         = GB2312_CHARSET;
    lf.lfQuality         = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily  = DEFAULT_PITCH | FF_SWISS;
    _tcsncpy_s(lf.lfFaceName, _T("Microsoft YaHei"), _TRUNCATE);
    s_hNameFont = ::CreateFontIndirect(&lf);
    return s_hNameFont;
}

// ---- 内部：抗锯齿圆角矩形填充（GDI+ GraphicsPath） -----------------------
//
// 给本文件画头像/badge/铃用。DuiAA 暴露的 helper 没有"抗锯齿圆角矩形"
// 这一种（只有椭圆/多边形/直线），所以本文件自己用 GDI+ GraphicsPath
// 画。GDI+ 在 DuiAA::FillEllipse 等已有调用路径上 lazy 初始化过，无需
// caller 再 GdiplusStartup。
static void FillRoundRectAA(HDC hdc, const RECT& rc, int radiusPx, COLORREF bg)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::REAL r = (Gdiplus::REAL)radiusPx;
    Gdiplus::REAL x = (Gdiplus::REAL)rc.left;
    Gdiplus::REAL y = (Gdiplus::REAL)rc.top;
    Gdiplus::REAL w = (Gdiplus::REAL)(rc.right  - rc.left);
    Gdiplus::REAL h = (Gdiplus::REAL)(rc.bottom - rc.top);

    Gdiplus::GraphicsPath p;
    p.AddArc(x,             y,             r * 2, r * 2, 180, 90);
    p.AddArc(x + w - r * 2, y,             r * 2, r * 2, 270, 90);
    p.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2,   0, 90);
    p.AddArc(x,             y + h - r * 2, r * 2, r * 2,  90, 90);
    p.CloseFigure();

    Gdiplus::SolidBrush brush(Gdiplus::Color(255,
                                             GetRValue(bg),
                                             GetGValue(bg),
                                             GetBValue(bg)));
    g.FillPath(&brush, &p);
}

// ---- 头像绘制 -------------------------------------------------------------
//
// 圆角矩形 + 居中单字。走抗锯齿 GDI+ 路径，让斜角的圆角弧线不出锯齿。
static void PaintAvatar(HDC hdc, int x, int y, int sz,
                        TCHAR letter, COLORREF bg)
{
    RECT rc = { x, y, x + sz, y + sz };
    FillRoundRectAA(hdc, rc, kAvatarRadiusPx, bg);

    // 单字居中。用 name font 同档（11pt），居中靠 DT_CENTER|DT_VCENTER。
    HFONT hOld = (HFONT)::SelectObject(hdc, EnsureNameFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, kAvatarText);
    RECT rcLetter = { x, y, x + sz, y + sz };
    TCHAR ch[2] = { letter, 0 };
    ::DrawText(hdc, ch, 1, &rcLetter,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

// ---- 静音铃 ---------------------------------------------------------------
//
// 真正铃形 glyph 需要矢量；Phase 3 用一个简化记号：14×14 圆环 + 1px 斜
// 划线（"禁止"那种），灰色。圆环 + 斜线都是非轴对齐路径，必须走 GDI+
// 抗锯齿，否则圆周锯齿明显。
static void PaintMutedBell(HDC hdc, int cx, int cy)
{
    int r = kRightInfoSizePx / 2;
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::Pen pen(Gdiplus::Color(255,
                                    GetRValue(kMuteRingColor),
                                    GetGValue(kMuteRingColor),
                                    GetBValue(kMuteRingColor)),
                     1.2f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap  (Gdiplus::LineCapRound);

    // 圆环
    Gdiplus::REAL fL = (Gdiplus::REAL)(cx - r);
    Gdiplus::REAL fT = (Gdiplus::REAL)(cy - r);
    Gdiplus::REAL fS = (Gdiplus::REAL)(r * 2);
    g.DrawEllipse(&pen, fL, fT, fS, fS);

    // 斜划线 "/" 从左下到右上
    g.DrawLine(&pen,
               Gdiplus::PointF((Gdiplus::REAL)(cx - r + 2), (Gdiplus::REAL)(cy + r - 2)),
               Gdiplus::PointF((Gdiplus::REAL)(cx + r - 2), (Gdiplus::REAL)(cy - r + 2)));
}

// ---- 未读 badge -----------------------------------------------------------
//
// 圆形 / 胶囊。> 99 显 "99+"，[10..99] 椭圆，[1..9] 圆。背景红，白字。
static void PaintUnreadBadge(HDC hdc, int rightAnchor, int cy, int n)
{
    CString text;
    if (n > 99)      { text = _T("99+"); }
    else             { text.Format(_T("%d"), n); }

    HFONT hOld = (HFONT)::SelectObject(hdc, balloonwjui::DuiResMgr::Inst().GetDefaultFont());
    SIZE sz = {};
    ::GetTextExtentPoint32(hdc, text, text.GetLength(), &sz);
    int diameter = kRightInfoSizePx + 2;        // badge 比 bell 略大一点
    int badgeW   = (sz.cx + kBadgePadXPx * 2 < diameter) ? diameter : sz.cx + kBadgePadXPx * 2;
    int badgeH   = diameter;

    RECT rc = { rightAnchor - badgeW,
                cy - badgeH / 2,
                rightAnchor,
                cy + badgeH / 2 };

    // 胶囊形：圆角半径 = 高度的一半。GDI+ AA 路径避免左右半圆锯齿。
    FillRoundRectAA(hdc, rc, badgeH / 2, kBadgeBg);

    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, kBadgeText);
    ::DrawText(hdc, text, text.GetLength(), &rc,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

// ---- 截断右侧的文本，给 ellipsis 留位置 ----------------------------------
//
// DrawText 自带 DT_END_ELLIPSIS + DT_SINGLELINE 自动加 "..."。

// ===========================================================================
// PaintSessionRow ——  一行的全部绘制
// ===========================================================================
void PaintSessionRow(HDC hdc, const RECT& rc, const Session& s,
                     bool selected, bool hover)
{
    // 1) 行底色（选中 > hover > 透明）
    if (selected)
    {
        HBRUSH hbr = ::CreateSolidBrush(kRowSelBg);
        ::FillRect(hdc, &rc, hbr);
        ::DeleteObject(hbr);
    }
    else if (hover)
    {
        HBRUSH hbr = ::CreateSolidBrush(kRowHoverBg);
        ::FillRect(hdc, &rc, hbr);
        ::DeleteObject(hbr);
    }

    // 2) 头像（左 14，纵向居中）
    int avX = rc.left + kPadLeftPx;
    int avY = rc.top  + (kRowHeightPx - kAvatarSizePx) / 2;
    PaintAvatar(hdc, avX, avY, kAvatarSizePx, s.avatarLetter, s.avatarBg);

    // 3) 文本左起点 / 右内距
    int textX     = avX + kAvatarSizePx + kTextLeftGapPx;
    int rightEdge = rc.right - kPadRightPx;

    // 4) 时间（右上）+ 名字（左上）。先量时间宽度以便给 name 留出空隙。
    HFONT hOldFont = (HFONT)::SelectObject(hdc, balloonwjui::DuiResMgr::Inst().GetDefaultFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);

    SIZE szTime = {};
    ::GetTextExtentPoint32(hdc, s.timeText, s.timeText.GetLength(), &szTime);
    RECT rcTime = { rightEdge - szTime.cx,
                    rc.top + kNameTopOffsetPx,
                    rightEdge,
                    rc.top + kNameTopOffsetPx + szTime.cy + 4 };
    COLORREF oldClr = ::SetTextColor(hdc, kTimeColor);
    ::DrawText(hdc, s.timeText, s.timeText.GetLength(), &rcTime,
               DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    // Name 用 11pt 字体，画在左侧；右侧留出 time 宽 + 8px 间隙以免撞。
    HFONT hOldFontName = (HFONT)::SelectObject(hdc, EnsureNameFont());
    ::SetTextColor(hdc, kNameColor);
    RECT rcName = { textX,
                    rc.top + kNameTopOffsetPx - 4,    // 11pt 比 9pt 高一点，往上挪 4px 对齐 time baseline
                    rightEdge - szTime.cx - 8,
                    rc.top + kNameTopOffsetPx + 22 };
    ::DrawText(hdc, s.name, s.name.GetLength(), &rcName,
               DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    ::SelectObject(hdc, hOldFontName);

    // 5) Preview（左下）+ Bell/Badge（右下）。同样先算右侧宽度给 preview
    //    留位置。
    int rightInfoW = 0;
    if (s.unreadCount > 0)
    {
        // unread 优先：算 badge 宽度。粗略估：每数字 9px，+ 2*padX。
        int digits = (s.unreadCount > 99) ? 3 : ((s.unreadCount > 9) ? 2 : 1);
        rightInfoW = (digits == 1) ? (kRightInfoSizePx + 2)
                                   : (digits * 9 + kBadgePadXPx * 2);
    }
    else if (s.muted)
    {
        rightInfoW = kRightInfoSizePx;
    }

    RECT rcPreview = { textX,
                       rc.top + kPreviewTopOffsetPx,
                       rightEdge - rightInfoW - (rightInfoW > 0 ? 6 : 0),
                       rc.top + kPreviewTopOffsetPx + 18 };
    ::SetTextColor(hdc, kPreviewColor);
    ::DrawText(hdc, s.preview, s.preview.GetLength(), &rcPreview,
               DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    // 6) 右下：bell 或 badge（互斥）
    int rightCy = rc.top + kPreviewTopOffsetPx + 8;
    if (s.unreadCount > 0)
    {
        PaintUnreadBadge(hdc, rightEdge, rightCy, s.unreadCount);
    }
    else if (s.muted)
    {
        PaintMutedBell(hdc, rightEdge - kRightInfoSizePx / 2, rightCy);
    }

    // 7) 复位 GDI 状态
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOldFont);
}

int GetSessionRowHeight()
{
    return kRowHeightPx;
}

} // namespace xchat
