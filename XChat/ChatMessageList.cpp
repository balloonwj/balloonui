#include "stdafx.h"
#include "ChatMessageList.h"
#include "MockChat.h"

#include "DuiResMgr.h"
#include "DuiDpi.h"
#include "DuiPaintAA.h"
#include "Controls/Window/DuiScrollBar.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

static const int kSbWidth = 10;

namespace xchat {

// ---- 颜色 -----------------------------------------------------------------

static const COLORREF kBgRgb              = RGB(238, 238, 238);   // 聊天区底（与 main.xml 右栏对齐）
static const COLORREF kBubblePeerBg       = RGB(255, 255, 255);   // peer 文本气泡白底
static const COLORREF kBubbleSelfBg       = RGB(149, 236, 105);   // self 文本气泡 某信经典亮绿
static const COLORREF kBubbleText         = RGB( 26,  26,  26);
static const COLORREF kAvatarText         = RGB(255, 255, 255);

static const COLORREF kTransferBg         = RGB(244, 178,  90);   // 转账卡橙底（接近某信实截）
static const COLORREF kTransferText       = RGB(255, 255, 255);
static const COLORREF kTransferSubText    = RGB(255, 245, 235);   // 卡底部 "某信转账" 浅化白

static const COLORREF kKnowledgeBg        = RGB(255, 255, 255);   // 知识星球卡白底
static const COLORREF kKnowledgeTitle     = RGB( 26,  26,  26);
static const COLORREF kKnowledgeSub       = RGB(136, 136, 136);
static const COLORREF kKnowledgeFooterBg  = RGB(245, 245, 245);   // 卡底部 "知识星球" 浅灰条
static const COLORREF kKnowledgeFooterTxt = RGB(120, 120, 120);
static const COLORREF kKnowledgeIconBg    = RGB( 90, 142, 196);   // 卡内书图标占位

static const COLORREF kDividerText        = RGB(170, 170, 170);   // 日期分隔线灰

static const COLORREF kImagePlaceholderBg = RGB(220, 220, 220);   // 图片占位灰底
static const COLORREF kImagePlaceholderTx = RGB(140, 140, 140);

// ---- 几何常量 -------------------------------------------------------------

static const int kListPadX           = 16;     // 列表左右内距
static const int kListPadTop         = 12;
static const int kListPadBot         = 12;

static const int kAvatarSizePx       = 36;
static const int kAvatarRadiusPx     = 6;
static const int kAvatarGapPx        = 8;      // 头像 → 气泡的水平间距

static const int kRowGapPx           = 8;      // 同一会话内相邻消息的纵向间距

static const int kDateDividerHeight  = 36;
static const int kImageHeightPx      = 100;
static const int kImageMaxWidthPx    = 200;

static const int kTransferCardW      = 240;
static const int kTransferCardH      = 88;

static const int kKnowledgeCardW     = 280;
static const int kKnowledgeCardH     = 130;
static const int kKnowledgeFooterH   = 32;

static const int kBubblePadX         = 12;
static const int kBubblePadY         = 9;
static const int kBubbleRadius       = 8;

static const int kScrollWheelPx      = 60;     // 每 notch 滚 60px

// ---- 字体（chat 区比 9pt 默认略大半档，正文 10pt） ----------------------

static HFONT EnsureChatTextFont()
{
    static HFONT s_h = nullptr;
    if (s_h)
    {
        return s_h;
    }
    int dpi = balloonwjui::DuiDpi::GetSystemDpi();
    LOGFONT lf = {};
    lf.lfHeight         = -::MulDiv(10, dpi, 72);
    lf.lfWeight         = FW_NORMAL;
    lf.lfCharSet        = GB2312_CHARSET;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    _tcsncpy_s(lf.lfFaceName, _T("Microsoft YaHei"), _TRUNCATE);
    s_h = ::CreateFontIndirect(&lf);
    return s_h;
}

// ---- 内部：圆角矩形填充 ---------------------------------------------------

static void FillRoundRect(HDC hdc, const RECT& rc, int radiusPx, COLORREF bg)
{
    HBRUSH hbr = ::CreateSolidBrush(bg);
    HRGN hrgn = ::CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1,
                                     radiusPx * 2, radiusPx * 2);
    ::FillRgn(hdc, hrgn, hbr);
    ::DeleteObject(hrgn);
    ::DeleteObject(hbr);
}

static void FillSolidRect(HDC hdc, const RECT& rc, COLORREF bg)
{
    HBRUSH hbr = ::CreateSolidBrush(bg);
    ::FillRect(hdc, &rc, hbr);
    ::DeleteObject(hbr);
}

// ---- 内部：头像绘制 -------------------------------------------------------
//
// chat 区头像用<u>圆形</u>（FillEllipse，DuiAA 抗锯齿），与某信 PC 实截
// 一致；session-list 那边继续用圆角矩形，因为 session 头像背景是品牌色块
// + 单字 fallback，圆角矩形配单字视觉上比圆形更稳。

static void PaintAvatar(HDC hdc, int x, int y, int sz,
                        TCHAR letter, COLORREF bg)
{
    RECT rc = { x, y, x + sz, y + sz };
    balloonwjui::DuiAA::FillEllipse(hdc, rc, bg);

    HFONT hOld = (HFONT)::SelectObject(hdc, EnsureChatTextFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, kAvatarText);
    TCHAR ch[2] = { letter, 0 };
    ::DrawText(hdc, ch, 1, &rc,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

// ---- 文本宽度测算（DT_CALCRECT 包装） ------------------------------------

static SIZE MeasureWrappedText(LPCTSTR text, int wrapW, HFONT hFont)
{
    HDC hdc = ::GetDC(nullptr);
    HFONT hOld = (HFONT)::SelectObject(hdc, hFont);
    RECT r = { 0, 0, wrapW, 0 };
    ::DrawText(hdc, text, -1, &r,
               DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT);
    ::SelectObject(hdc, hOld);
    ::ReleaseDC(nullptr, hdc);
    SIZE sz = { r.right - r.left, r.bottom - r.top };
    return sz;
}

// ===========================================================================
// 每种 kind 的 Measure / Paint
// ===========================================================================
//
// Measure(contentWidth) → 单条消息总高（含 avatar + bubble + 行间空白）
// Paint(hdc, rcRow, msg)：rcRow.top..rcRow.bottom 是该条消息的纵向区段；
// 横向用 m_rcItem.left + kListPadX..m_rcItem.right - kListPadX 作为可绘制
// 横向区段。

// ---- 文本气泡 -------------------------------------------------------------

static int MeasureTextBubble(const ChatMessage& m, int contentW)
{
    int maxBubbleInnerW = contentW - kAvatarSizePx - kAvatarGapPx
                          - 60                              // 留对侧空隙
                          - kBubblePadX * 2;
    if (maxBubbleInnerW < 60) { maxBubbleInnerW = 60; }
    SIZE sz = MeasureWrappedText(m.text, maxBubbleInnerW, EnsureChatTextFont());
    return sz.cy + kBubblePadY * 2;
}

static void PaintTextBubble(HDC hdc, const RECT& rcRow, int contentLeft, int contentRight,
                            const ChatMessage& m)
{
    int avX, avY, bubbleX, bubbleW;
    int maxBubbleInnerW = (contentRight - contentLeft) - kAvatarSizePx - kAvatarGapPx
                          - 60 - kBubblePadX * 2;
    if (maxBubbleInnerW < 60) { maxBubbleInnerW = 60; }
    SIZE sz = MeasureWrappedText(m.text, maxBubbleInnerW, EnsureChatTextFont());
    bubbleW = sz.cx + kBubblePadX * 2;

    avY = rcRow.top;
    if (m.fromSelf)
    {
        avX     = contentRight - kAvatarSizePx;
        bubbleX = avX - kAvatarGapPx - bubbleW;
    }
    else
    {
        avX     = contentLeft;
        bubbleX = avX + kAvatarSizePx + kAvatarGapPx;
    }

    PaintAvatar(hdc, avX, avY, kAvatarSizePx, m.avatarLetter, m.avatarBg);

    RECT rcBubble = { bubbleX, avY,
                      bubbleX + bubbleW,
                      avY + sz.cy + kBubblePadY * 2 };
    COLORREF bubbleBg = m.fromSelf ? kBubbleSelfBg : kBubblePeerBg;
    FillRoundRect(hdc, rcBubble, kBubbleRadius, bubbleBg);

    HFONT hOld = (HFONT)::SelectObject(hdc, EnsureChatTextFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, kBubbleText);
    RECT rcText = { rcBubble.left + kBubblePadX,
                    rcBubble.top  + kBubblePadY,
                    rcBubble.right - kBubblePadX,
                    rcBubble.bottom - kBubblePadY };
    ::DrawText(hdc, m.text, -1, &rcText,
               DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

// ---- 转账卡（橙底）-------------------------------------------------------

static int MeasureTransferCard(const ChatMessage&, int /*contentW*/)
{
    return kTransferCardH;
}

static void PaintTransferCard(HDC hdc, const RECT& rcRow, int contentLeft, int contentRight,
                              const ChatMessage& m)
{
    int avX, avY, cardX;
    avY = rcRow.top;
    if (m.fromSelf)
    {
        avX   = contentRight - kAvatarSizePx;
        cardX = avX - kAvatarGapPx - kTransferCardW;
    }
    else
    {
        avX   = contentLeft;
        cardX = avX + kAvatarSizePx + kAvatarGapPx;
    }
    PaintAvatar(hdc, avX, avY, kAvatarSizePx, m.avatarLetter, m.avatarBg);

    RECT rcCard = { cardX, avY, cardX + kTransferCardW, avY + kTransferCardH };
    FillRoundRect(hdc, rcCard, kBubbleRadius, kTransferBg);

    // 左侧 ✓ 圆圈（占位：填白圆）
    int circleSz = 36;
    int circleCx = rcCard.left + 18 + circleSz / 2;
    int circleCy = (rcCard.top + rcCard.bottom) / 2 - 6;
    RECT rcCircle = { circleCx - circleSz / 2, circleCy - circleSz / 2,
                      circleCx + circleSz / 2, circleCy + circleSz / 2 };
    balloonwjui::DuiAA::FillEllipse(hdc, rcCircle, RGB(255, 255, 255));
    // ✓ 用一段两点折线
    HPEN hPen = ::CreatePen(PS_SOLID, 2, kTransferBg);
    HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);
    POINT pts[3] = {
        { circleCx - 8, circleCy + 1 },
        { circleCx - 2, circleCy + 7 },
        { circleCx + 8, circleCy - 5 }
    };
    ::Polyline(hdc, pts, 3);
    ::SelectObject(hdc, hOldPen);
    ::DeleteObject(hPen);

    // 右侧两行文字：金额（大）+ 状态（小）
    HFONT hOld = (HFONT)::SelectObject(hdc, EnsureChatTextFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, kTransferText);

    int textX = rcCard.left + 18 + circleSz + 16;
    RECT rcAmount = { textX, rcCard.top + 16, rcCard.right - 12, rcCard.top + 40 };
    ::DrawText(hdc, m.amount, -1, &rcAmount,
               DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    RECT rcStatus = { textX, rcCard.top + 38, rcCard.right - 12, rcCard.top + 60 };
    ::DrawText(hdc, m.statusText, -1, &rcStatus,
               DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    // 卡底"某信转账"小字（浅化白色）
    ::SetTextColor(hdc, kTransferSubText);
    RECT rcFooter = { rcCard.left + 18, rcCard.bottom - 22, rcCard.right - 12, rcCard.bottom - 6 };
    ::DrawText(hdc, _T("某信转账"), -1, &rcFooter,
               DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

// ---- 知识星球邀请卡 -------------------------------------------------------

static int MeasureKnowledgeCard(const ChatMessage&, int /*contentW*/)
{
    return kKnowledgeCardH;
}

static void PaintKnowledgeCard(HDC hdc, const RECT& rcRow, int contentLeft, int contentRight,
                               const ChatMessage& m)
{
    int avX, avY, cardX;
    avY = rcRow.top;
    if (m.fromSelf)
    {
        avX   = contentRight - kAvatarSizePx;
        cardX = avX - kAvatarGapPx - kKnowledgeCardW;
    }
    else
    {
        avX   = contentLeft;
        cardX = avX + kAvatarSizePx + kAvatarGapPx;
    }
    PaintAvatar(hdc, avX, avY, kAvatarSizePx, m.avatarLetter, m.avatarBg);

    RECT rcCard = { cardX, avY, cardX + kKnowledgeCardW, avY + kKnowledgeCardH };
    // 卡主体白底（圆角）
    FillRoundRect(hdc, rcCard, kBubbleRadius, kKnowledgeBg);

    // 主体内容：左上 title（多行）+ 副标题；右下书图标占位
    HFONT hOldText = (HFONT)::SelectObject(hdc, EnsureChatTextFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, kKnowledgeTitle);

    // title 区：左 14、上 12、右 80（给图标留位）
    RECT rcTitle = { rcCard.left + 14, rcCard.top + 12,
                     rcCard.right - 80, rcCard.top + 70 };
    ::DrawText(hdc, m.title, -1, &rcTitle,
               DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK | DT_END_ELLIPSIS);

    ::SetTextColor(hdc, kKnowledgeSub);
    RECT rcSub = { rcCard.left + 14, rcCard.bottom - kKnowledgeFooterH - 24,
                   rcCard.right - 80, rcCard.bottom - kKnowledgeFooterH - 6 };
    ::DrawText(hdc, m.subtitle, -1, &rcSub,
               DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    // 右下封面缩略图。原来是 40×40 蓝色实心圆角矩形，过于占位。这里改
    // 成"封面" 视觉：蓝色渐变圆角矩形 + 内部白色"星球"图（中心圆 +
    // 倾斜椭圆环），呼应"知识星球"卡片身份。GDI+ 路径：
    //   1. LinearGradientBrush 填充 vertical 渐变蓝
    //   2. SolidBrush 白填中心圆
    //   3. 白色 Pen 画椭圆环（旋转 -25°）
    {
        int iconSz = 56;
        RECT rcIcon = { rcCard.right - 14 - iconSz,
                        rcCard.bottom - kKnowledgeFooterH - 14 - iconSz,
                        rcCard.right - 14,
                        rcCard.bottom - kKnowledgeFooterH - 14 };

        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        // 渐变背景：上 #6A9BD4 → 下 #4978B5
        Gdiplus::RectF rcGp((Gdiplus::REAL)rcIcon.left, (Gdiplus::REAL)rcIcon.top,
                            (Gdiplus::REAL)(rcIcon.right - rcIcon.left),
                            (Gdiplus::REAL)(rcIcon.bottom - rcIcon.top));
        Gdiplus::LinearGradientBrush gb(rcGp,
                                        Gdiplus::Color(255, 106, 155, 212),
                                        Gdiplus::Color(255,  73, 120, 181),
                                        Gdiplus::LinearGradientModeVertical);

        // 圆角矩形 path
        Gdiplus::GraphicsPath rrPath;
        Gdiplus::REAL rr = 4.0f;
        rrPath.AddArc(rcGp.X,                 rcGp.Y,                 rr * 2, rr * 2, 180, 90);
        rrPath.AddArc(rcGp.GetRight() - rr * 2, rcGp.Y,               rr * 2, rr * 2, 270, 90);
        rrPath.AddArc(rcGp.GetRight() - rr * 2, rcGp.GetBottom() - rr * 2, rr * 2, rr * 2, 0, 90);
        rrPath.AddArc(rcGp.X,                 rcGp.GetBottom() - rr * 2, rr * 2, rr * 2, 90, 90);
        rrPath.CloseFigure();
        g.FillPath(&gb, &rrPath);

        // 中心圆（"星球")
        float cx = rcGp.X + rcGp.Width  * 0.5f;
        float cy = rcGp.Y + rcGp.Height * 0.5f;
        float planetR = (rcGp.Width < rcGp.Height ? rcGp.Width : rcGp.Height) * 0.22f;
        Gdiplus::SolidBrush whiteBr(Gdiplus::Color(255, 255, 255, 255));
        g.FillEllipse(&whiteBr, cx - planetR, cy - planetR, planetR * 2, planetR * 2);

        // 椭圆环（围着星球，倾斜 -25°）
        Gdiplus::Pen ringPen(Gdiplus::Color(255, 255, 255, 255), 1.5f);
        Gdiplus::Matrix saveMx;
        g.GetTransform(&saveMx);
        g.TranslateTransform(cx, cy);
        g.RotateTransform(-25.0f);
        float ringRx = planetR * 1.85f;
        float ringRy = planetR * 0.55f;
        g.DrawEllipse(&ringPen, -ringRx, -ringRy, ringRx * 2, ringRy * 2);
        g.SetTransform(&saveMx);
    }

    // footer 浅灰条 + tag 文本
    RECT rcFooter = { rcCard.left, rcCard.bottom - kKnowledgeFooterH,
                      rcCard.right, rcCard.bottom };
    // footer 不能用 FillRoundRect 因为只有底部圆角；简单 FillRect 接 separator 线
    FillSolidRect(hdc, rcFooter, kKnowledgeFooterBg);
    // 1px 分隔线
    HPEN hSep = ::CreatePen(PS_SOLID, 1, RGB(225, 225, 225));
    HPEN hOldPen = (HPEN)::SelectObject(hdc, hSep);
    ::MoveToEx(hdc, rcFooter.left, rcFooter.top, nullptr);
    ::LineTo  (hdc, rcFooter.right, rcFooter.top);
    ::SelectObject(hdc, hOldPen);
    ::DeleteObject(hSep);

    ::SetTextColor(hdc, kKnowledgeFooterTxt);
    RECT rcTag = { rcFooter.left + 14, rcFooter.top, rcFooter.right - 14, rcFooter.bottom };
    ::DrawText(hdc, m.tagText, -1, &rcTag,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOldText);
}

// ---- 图片占位 -------------------------------------------------------------

static int MeasureImage(const ChatMessage&, int /*contentW*/)
{
    return kImageHeightPx;
}

// =============================================================================
// PaintMockPhoto —— 用 GDI+ 程序化生成"看起来像真照片"的图片，避免依赖
// 外部 PNG 资源也能让聊天记录有视觉密度。每个 ImageKind 用一段独立的
// 几何画法（渐变 + 多边形 + 圆等），全部自带圆角裁剪到 rect 内。
// =============================================================================

namespace {

inline Gdiplus::Color GpCol(int r, int g, int b, int a = 255)
{
    return Gdiplus::Color(a, r, g, b);
}

// 把 path 限制在 rect 圆角内 —— 避免画的内容溢出气泡。
void ClipToRoundRect(Gdiplus::Graphics& g, const Gdiplus::RectF& rc, float r)
{
    Gdiplus::GraphicsPath p;
    p.AddArc(rc.X,                  rc.Y,                  r * 2, r * 2, 180, 90);
    p.AddArc(rc.GetRight() - r * 2, rc.Y,                  r * 2, r * 2, 270, 90);
    p.AddArc(rc.GetRight() - r * 2, rc.GetBottom() - r * 2, r * 2, r * 2,   0, 90);
    p.AddArc(rc.X,                  rc.GetBottom() - r * 2, r * 2, r * 2,  90, 90);
    p.CloseFigure();
    g.SetClip(&p, Gdiplus::CombineModeReplace);
}

void PaintHotpot(Gdiplus::Graphics& g, const Gdiplus::RectF& rc)
{
    // 红汤底（暖色径向感渐变近似：竖向线性渐变 #C92A1F → #E8421F）
    Gdiplus::LinearGradientBrush br(rc, GpCol(201, 42, 31), GpCol(232, 66, 31),
                                    Gdiplus::LinearGradientModeVertical);
    g.FillRectangle(&br, rc);
    // 漂浮食材：白菜（绿）/ 牛肉片（褐红）/ 香菜（深绿）+ 油花（亮黄圆）
    Gdiplus::SolidBrush green(GpCol(122, 184, 76));
    Gdiplus::SolidBrush meat(GpCol(140, 56, 36));
    Gdiplus::SolidBrush dark (GpCol( 60, 110, 50));
    Gdiplus::SolidBrush oil  (GpCol(255, 220, 100, 220));
    struct D { float cx, cy, r; Gdiplus::SolidBrush* br; };
    D dots[] = {
        { rc.X + rc.Width * 0.18f, rc.Y + rc.Height * 0.30f, 14, &green },
        { rc.X + rc.Width * 0.42f, rc.Y + rc.Height * 0.55f, 12, &meat  },
        { rc.X + rc.Width * 0.62f, rc.Y + rc.Height * 0.32f, 10, &green },
        { rc.X + rc.Width * 0.80f, rc.Y + rc.Height * 0.55f, 11, &dark  },
        { rc.X + rc.Width * 0.30f, rc.Y + rc.Height * 0.78f,  9, &meat  },
        { rc.X + rc.Width * 0.55f, rc.Y + rc.Height * 0.78f,  8, &dark  },
        { rc.X + rc.Width * 0.72f, rc.Y + rc.Height * 0.80f, 12, &green },
        { rc.X + rc.Width * 0.20f, rc.Y + rc.Height * 0.55f,  6, &oil   },
        { rc.X + rc.Width * 0.50f, rc.Y + rc.Height * 0.20f,  5, &oil   },
        { rc.X + rc.Width * 0.85f, rc.Y + rc.Height * 0.20f,  7, &oil   },
    };
    for (auto& d : dots)
    {
        g.FillEllipse(d.br, d.cx - d.r, d.cy - d.r, d.r * 2, d.r * 2);
    }
}

void PaintLandscape(Gdiplus::Graphics& g, const Gdiplus::RectF& rc)
{
    // 渐变天空：上 #FFB570（橙） → 下 #FFE0B0（暖白）
    Gdiplus::RectF rcSky(rc.X, rc.Y, rc.Width, rc.Height * 0.65f);
    Gdiplus::LinearGradientBrush sky(rcSky, GpCol(255, 181, 112), GpCol(255, 224, 176),
                                     Gdiplus::LinearGradientModeVertical);
    g.FillRectangle(&sky, rcSky);
    // 草地
    Gdiplus::RectF rcGrass(rc.X, rc.Y + rc.Height * 0.78f, rc.Width, rc.Height * 0.22f);
    Gdiplus::SolidBrush grass(GpCol(64, 130, 70));
    g.FillRectangle(&grass, rcGrass);
    // 太阳
    float sunR = rc.Height * 0.18f;
    Gdiplus::SolidBrush sun(GpCol(255, 240, 180));
    g.FillEllipse(&sun, rc.X + rc.Width * 0.65f - sunR,
                        rc.Y + rc.Height * 0.20f - sunR, sunR * 2, sunR * 2);
    // 远山三角
    Gdiplus::SolidBrush mt1(GpCol(110, 80, 100));
    Gdiplus::SolidBrush mt2(GpCol( 80, 60,  80));
    Gdiplus::PointF tri1[3] = {
        { rc.X + rc.Width * 0.10f, rc.Y + rc.Height * 0.78f },
        { rc.X + rc.Width * 0.40f, rc.Y + rc.Height * 0.45f },
        { rc.X + rc.Width * 0.65f, rc.Y + rc.Height * 0.78f },
    };
    Gdiplus::PointF tri2[3] = {
        { rc.X + rc.Width * 0.45f, rc.Y + rc.Height * 0.78f },
        { rc.X + rc.Width * 0.78f, rc.Y + rc.Height * 0.55f },
        { rc.X + rc.Width * 1.00f, rc.Y + rc.Height * 0.78f },
    };
    g.FillPolygon(&mt1, tri1, 3);
    g.FillPolygon(&mt2, tri2, 3);
}

void PaintSports(Gdiplus::Graphics& g, const Gdiplus::RectF& rc)
{
    // 草地底
    Gdiplus::SolidBrush grass(GpCol(74, 138, 76));
    g.FillRectangle(&grass, rc);
    // 跑道椭圆环（橙红）+ 内部白线
    Gdiplus::Pen lane1(GpCol(208, 100, 60), 8.0f);
    Gdiplus::Pen lane2(GpCol(255, 255, 255), 1.5f);
    float padX = rc.Width  * 0.08f;
    float padY = rc.Height * 0.18f;
    Gdiplus::RectF rcTrack(rc.X + padX, rc.Y + padY,
                           rc.Width - padX * 2, rc.Height - padY * 2);
    g.DrawEllipse(&lane1, rcTrack);
    g.DrawEllipse(&lane2, rcTrack);
    // 中央旗杆 + 国旗
    float poleX = rc.X + rc.Width * 0.5f;
    Gdiplus::Pen pole(GpCol(80, 80, 80), 2.0f);
    g.DrawLine(&pole,
               Gdiplus::PointF(poleX, rc.Y + rc.Height * 0.30f),
               Gdiplus::PointF(poleX, rc.Y + rc.Height * 0.70f));
    Gdiplus::SolidBrush flag(GpCol(220, 40, 40));
    g.FillRectangle(&flag, poleX, rc.Y + rc.Height * 0.30f, 22.0f, 14.0f);
}

void PaintUniform(Gdiplus::Graphics& g, const Gdiplus::RectF& rc)
{
    // 浅灰背景
    Gdiplus::SolidBrush bg(GpCol(240, 240, 240));
    g.FillRectangle(&bg, rc);
    // T 恤剪影：领口梯形 + 主体多边形
    Gdiplus::SolidBrush red(GpCol(220, 50, 50));
    float cx = rc.X + rc.Width * 0.5f;
    float top = rc.Y + rc.Height * 0.20f;
    float bot = rc.Y + rc.Height * 0.85f;
    Gdiplus::PointF body[8] = {
        { cx - rc.Width * 0.18f, top },                 // 左肩
        { cx - rc.Width * 0.06f, top + 10 },            // 左领口下
        { cx + rc.Width * 0.06f, top + 10 },            // 右领口下
        { cx + rc.Width * 0.18f, top },                 // 右肩
        { cx + rc.Width * 0.32f, top + 28 },            // 右袖
        { cx + rc.Width * 0.22f, bot },                 // 右下摆
        { cx - rc.Width * 0.22f, bot },                 // 左下摆
        { cx - rc.Width * 0.32f, top + 28 },            // 左袖
    };
    g.FillPolygon(&red, body, 8);
    // 校徽白圆
    Gdiplus::SolidBrush white(GpCol(255, 255, 255));
    float badgeR = rc.Width * 0.05f;
    g.FillEllipse(&white, cx - badgeR, top + 30, badgeR * 2, badgeR * 2);
}

void PaintQrCode(Gdiplus::Graphics& g, const Gdiplus::RectF& rc)
{
    // 白底 + 黑色 21×21 modules（伪二维码视觉）
    Gdiplus::SolidBrush white(GpCol(255, 255, 255));
    g.FillRectangle(&white, rc);
    Gdiplus::SolidBrush black(GpCol(20, 20, 20));
    int N = 21;
    float pad = rc.Width * 0.05f;
    float side = (rc.Height < rc.Width ? rc.Height : rc.Width) - pad * 2;
    float cell = side / N;
    float ox = rc.X + (rc.Width  - side) * 0.5f;
    float oy = rc.Y + (rc.Height - side) * 0.5f;
    // 三个角的 finder pattern (7×7 with inner white 5×5 + center black 3×3)
    auto finder = [&](int gx, int gy) {
        g.FillRectangle(&black, ox + gx * cell, oy + gy * cell, cell * 7, cell * 7);
        g.FillRectangle(&white, ox + (gx + 1) * cell, oy + (gy + 1) * cell, cell * 5, cell * 5);
        g.FillRectangle(&black, ox + (gx + 2) * cell, oy + (gy + 2) * cell, cell * 3, cell * 3);
    };
    finder(0, 0);
    finder(N - 7, 0);
    finder(0, N - 7);
    // 伪随机数据 modules（用质数序列让看起来像真二维码）
    for (int gy = 0; gy < N; ++gy)
    {
        for (int gx = 0; gx < N; ++gx)
        {
            // 跳过 finder 区
            if ((gx < 8 && gy < 8) || (gx >= N - 8 && gy < 8) || (gx < 8 && gy >= N - 8))
            {
                continue;
            }
            // 用 (gx*31 + gy*17 + gx*gy*7) 模奇偶决定黑白 —— 伪随机
            int h = (gx * 31 + gy * 17 + gx * gy * 7) ^ (gx + gy * 3);
            if ((h & 1) == 0)
            {
                g.FillRectangle(&black, ox + gx * cell, oy + gy * cell, cell, cell);
            }
        }
    }
}

void PaintGroupPhoto(Gdiplus::Graphics& g, const Gdiplus::RectF& rc)
{
    // 渐变天 + 草地 + 几个并排剪影
    Gdiplus::RectF rcSky(rc.X, rc.Y, rc.Width, rc.Height * 0.55f);
    Gdiplus::LinearGradientBrush sky(rcSky, GpCol(180, 215, 245), GpCol(220, 235, 250),
                                     Gdiplus::LinearGradientModeVertical);
    g.FillRectangle(&sky, rcSky);
    Gdiplus::SolidBrush grass(GpCol(120, 170, 100));
    g.FillRectangle(&grass, rc.X, rc.Y + rc.Height * 0.68f, rc.Width, rc.Height * 0.32f);
    // 太阳
    Gdiplus::SolidBrush sun(GpCol(255, 240, 200));
    g.FillEllipse(&sun, rc.X + rc.Width * 0.78f, rc.Y + rc.Height * 0.10f, 22.0f, 22.0f);
    // 5 个人形剪影（头圆 + 身梯形）—— SolidBrush 不可拷贝，用 Color
    // 数组循环里临时构造。
    Gdiplus::Color figCols[5] = {
        GpCol( 60,  60,  90),
        GpCol( 90,  60,  60),
        GpCol( 50,  80,  70),
        GpCol( 80,  70,  50),
        GpCol( 70,  50,  80),
    };
    for (int i = 0; i < 5; ++i)
    {
        Gdiplus::SolidBrush br(figCols[i]);
        float cx = rc.X + rc.Width * (0.15f + i * 0.18f);
        float headY = rc.Y + rc.Height * 0.45f;
        float headR = rc.Height * 0.07f;
        g.FillEllipse(&br, cx - headR, headY, headR * 2, headR * 2);
        Gdiplus::PointF body[4] = {
            { cx - rc.Width * 0.07f, headY + headR * 2 },
            { cx + rc.Width * 0.07f, headY + headR * 2 },
            { cx + rc.Width * 0.10f, rc.Y + rc.Height * 0.92f },
            { cx - rc.Width * 0.10f, rc.Y + rc.Height * 0.92f },
        };
        g.FillPolygon(&br, body, 4);
    }
}

void PaintChart(Gdiplus::Graphics& g, const Gdiplus::RectF& rc)
{
    // 白底 + 浅网格 + 两条折线（蓝 / 橙）
    Gdiplus::SolidBrush white(GpCol(252, 252, 252));
    g.FillRectangle(&white, rc);
    Gdiplus::Pen grid(GpCol(225, 225, 225), 1.0f);
    int gridLines = 6;
    for (int i = 1; i < gridLines; ++i)
    {
        float y = rc.Y + rc.Height * i / (float)gridLines;
        g.DrawLine(&grid, rc.X, y, rc.GetRight(), y);
        float x = rc.X + rc.Width * i / (float)gridLines;
        g.DrawLine(&grid, x, rc.Y, x, rc.GetBottom());
    }
    // 蓝折线 (更平稳)
    Gdiplus::Pen blue(GpCol(45, 108, 223), 2.2f);
    blue.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::PointF blueP[] = {
        { rc.X + rc.Width * 0.05f, rc.Y + rc.Height * 0.85f },
        { rc.X + rc.Width * 0.20f, rc.Y + rc.Height * 0.65f },
        { rc.X + rc.Width * 0.40f, rc.Y + rc.Height * 0.55f },
        { rc.X + rc.Width * 0.60f, rc.Y + rc.Height * 0.45f },
        { rc.X + rc.Width * 0.80f, rc.Y + rc.Height * 0.30f },
        { rc.X + rc.Width * 0.95f, rc.Y + rc.Height * 0.20f },
    };
    g.DrawLines(&blue, blueP, 6);
    // 橙折线 (波动更大)
    Gdiplus::Pen orange(GpCol(245, 158, 10), 2.2f);
    orange.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::PointF orangeP[] = {
        { rc.X + rc.Width * 0.05f, rc.Y + rc.Height * 0.70f },
        { rc.X + rc.Width * 0.20f, rc.Y + rc.Height * 0.55f },
        { rc.X + rc.Width * 0.40f, rc.Y + rc.Height * 0.65f },
        { rc.X + rc.Width * 0.60f, rc.Y + rc.Height * 0.40f },
        { rc.X + rc.Width * 0.80f, rc.Y + rc.Height * 0.50f },
        { rc.X + rc.Width * 0.95f, rc.Y + rc.Height * 0.25f },
    };
    g.DrawLines(&orange, orangeP, 6);
}

void PaintReceipt(Gdiplus::Graphics& g, const Gdiplus::RectF& rc)
{
    // 模拟账单截图：白底 + 顶部品牌色 header + 几行灰色"信息行"
    Gdiplus::SolidBrush white(GpCol(255, 255, 255));
    g.FillRectangle(&white, rc);
    Gdiplus::SolidBrush header(GpCol(7, 193, 96));
    g.FillRectangle(&header, rc.X, rc.Y, rc.Width, rc.Height * 0.20f);
    // 中央大金额"¥...."（用矩形 stub 表示）
    Gdiplus::SolidBrush dark(GpCol(40, 40, 40));
    g.FillRectangle(&dark,
                    rc.X + rc.Width * 0.20f, rc.Y + rc.Height * 0.30f,
                    rc.Width * 0.60f, rc.Height * 0.18f);
    // 4 行"信息行"
    Gdiplus::SolidBrush gray(GpCol(220, 220, 220));
    for (int i = 0; i < 4; ++i)
    {
        float y = rc.Y + rc.Height * (0.55f + i * 0.10f);
        g.FillRectangle(&gray, rc.X + rc.Width * 0.10f, y, rc.Width * 0.80f, 6.0f);
    }
}

void PaintMockPhoto(HDC hdc, const RECT& rcRoot, ImageKind kind)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::RectF rc((Gdiplus::REAL)rcRoot.left, (Gdiplus::REAL)rcRoot.top,
                      (Gdiplus::REAL)(rcRoot.right - rcRoot.left),
                      (Gdiplus::REAL)(rcRoot.bottom - rcRoot.top));
    ClipToRoundRect(g, rc, (float)kBubbleRadius);

    switch (kind)
    {
    case Img_Hotpot:     PaintHotpot    (g, rc); break;
    case Img_Landscape:  PaintLandscape (g, rc); break;
    case Img_Sports:     PaintSports    (g, rc); break;
    case Img_Uniform:    PaintUniform   (g, rc); break;
    case Img_QrCode:     PaintQrCode    (g, rc); break;
    case Img_GroupPhoto: PaintGroupPhoto(g, rc); break;
    case Img_Chart:      PaintChart     (g, rc); break;
    case Img_Receipt:    PaintReceipt   (g, rc); break;
    default: break;
    }
    g.ResetClip();
}

} // anonymous namespace

static void PaintImage(HDC hdc, const RECT& rcRow, int contentLeft, int contentRight,
                       const ChatMessage& m)
{
    int avX, avY, imgX;
    avY = rcRow.top;
    if (m.fromSelf)
    {
        avX  = contentRight - kAvatarSizePx;
        imgX = avX - kAvatarGapPx - kImageMaxWidthPx;
    }
    else
    {
        avX  = contentLeft;
        imgX = avX + kAvatarSizePx + kAvatarGapPx;
    }
    PaintAvatar(hdc, avX, avY, kAvatarSizePx, m.avatarLetter, m.avatarBg);

    RECT rcImg = { imgX, avY,
                   imgX + kImageMaxWidthPx,
                   avY + kImageHeightPx };

    if (m.imageKind != Img_None)
    {
        // 真"假图"路径：GDI+ 程序化画一个 8 种风格之一的小图。
        FillRoundRect(hdc, rcImg, kBubbleRadius, RGB(255, 255, 255));   // 圆角白底打底
        PaintMockPhoto(hdc, rcImg, m.imageKind);
    }
    else
    {
        // 兼容老路径：灰矩形 + caption 文本。Mock 数据全用上面新路径的话
        // 这条分支不会触发；保留是为了"caller 不设 imageKind 时也能画
        // 出来"的优雅退化。
        FillRoundRect(hdc, rcImg, kBubbleRadius, kImagePlaceholderBg);
        HFONT hOld = (HFONT)::SelectObject(hdc, EnsureChatTextFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, kImagePlaceholderTx);
        ::DrawText(hdc, m.text, -1, &rcImg,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOld);
    }
}

// ---- 日期分隔线 -----------------------------------------------------------

static int MeasureDateDivider(const ChatMessage&, int /*contentW*/)
{
    return kDateDividerHeight;
}

static void PaintDateDivider(HDC hdc, const RECT& rcRow, int contentLeft, int contentRight,
                             const ChatMessage& m)
{
    HFONT hOld = (HFONT)::SelectObject(hdc, balloonwjui::DuiResMgr::Inst().GetDefaultFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, kDividerText);
    RECT rc = { contentLeft, rcRow.top, contentRight, rcRow.bottom };
    ::DrawText(hdc, m.text, -1, &rc,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

// ---- 文件卡片 -------------------------------------------------------------
//
// 某信 PC 风格：白底圆角卡 + 左侧 48×56 ext-icon (按 .docx/.pdf/.zip 等
// 后缀决定颜色) + 右侧两行（文件名 + 大小）。卡片宽 ~280。peer / self
// 走 chatMessage.fromSelf 决定卡贴左侧 / 右侧（与文本气泡同侧）。
//
// ext-icon 不依赖外部资源 —— 程序化画：
//   · 整体一个 ICON_W × ICON_H 圆角矩形，颜色按 ext 映射；
//   · 顶部右上角 8×8 三角"折角"；
//   · 居中白色 ext 大字（最多 4 字符，如 "DOC" "PDF" "XLSX"）。

static const int kFileCardW   = 280;
static const int kFileCardH   = 78;
static const int kFileIconW   = 48;
static const int kFileIconH   = 56;
static const int kFileCardPad = 12;

static COLORREF FileExtColor(const CString& ext)
{
    // 后缀 → 类型色（小写比较）。常见办公 / 多媒体 / 压缩 / 通用。
    CString lo = ext; lo.MakeLower();
    if (lo == _T("doc")  || lo == _T("docx")) { return RGB( 45, 108, 223); }   // 蓝 — Word
    if (lo == _T("xls")  || lo == _T("xlsx")) { return RGB( 30, 142,  62); }   // 绿 — Excel
    if (lo == _T("ppt")  || lo == _T("pptx")) { return RGB(218,  92,  46); }   // 橙 — PowerPoint
    if (lo == _T("pdf"))                       { return RGB(208,  44,  44); }   // 红 — PDF
    if (lo == _T("zip")  || lo == _T("rar")
     || lo == _T("7z")   || lo == _T("tar"))   { return RGB(180, 130,  60); }   // 棕 — 压缩
    if (lo == _T("png")  || lo == _T("jpg")
     || lo == _T("jpeg") || lo == _T("gif"))   { return RGB(168,  85, 247); }   // 紫 — 图片
    if (lo == _T("mp3")  || lo == _T("m4a")
     || lo == _T("wav"))                       { return RGB( 14, 165, 165); }   // 青 — 音频
    if (lo == _T("mp4")  || lo == _T("mov")
     || lo == _T("avi"))                       { return RGB(101, 113, 240); }   // 蓝紫 — 视频
    if (lo == _T("md")   || lo == _T("txt"))   { return RGB(101, 113, 130); }   // 灰 — 文本
    return RGB(120, 120, 130);                                                  // 通用
}

static void PaintFileExtIcon(HDC hdc, int x, int y, const CString& ext)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    COLORREF clr = FileExtColor(ext);
    Gdiplus::Color base(255, GetRValue(clr), GetGValue(clr), GetBValue(clr));
    Gdiplus::Color light(255,
                         (BYTE)((255 + GetRValue(clr) * 2) / 3),
                         (BYTE)((255 + GetGValue(clr) * 2) / 3),
                         (BYTE)((255 + GetBValue(clr) * 2) / 3));

    // 主圆角矩形
    Gdiplus::REAL fX = (Gdiplus::REAL)x;
    Gdiplus::REAL fY = (Gdiplus::REAL)y;
    Gdiplus::REAL fW = (Gdiplus::REAL)kFileIconW;
    Gdiplus::REAL fH = (Gdiplus::REAL)kFileIconH;
    Gdiplus::REAL r  = 6.0f;
    Gdiplus::GraphicsPath body;
    body.AddArc(fX,            fY,            r * 2, r * 2, 180, 90);
    body.AddArc(fX + fW - r*2, fY,            r * 2, r * 2, 270, 90);
    body.AddArc(fX + fW - r*2, fY + fH - r*2, r * 2, r * 2,   0, 90);
    body.AddArc(fX,            fY + fH - r*2, r * 2, r * 2,  90, 90);
    body.CloseFigure();
    Gdiplus::SolidBrush brBody(base);
    g.FillPath(&brBody, &body);

    // 顶部 12 高一条更亮的"上沿"，模拟纸张的 "header band"
    Gdiplus::GraphicsPath topBand;
    topBand.AddArc(fX,            fY, r * 2, r * 2, 180, 90);
    topBand.AddArc(fX + fW - r*2, fY, r * 2, r * 2, 270, 90);
    topBand.AddLine(fX + fW, fY + 12, fX, fY + 12);
    topBand.CloseFigure();
    Gdiplus::SolidBrush brTop(light);
    g.FillPath(&brTop, &topBand);

    // 中央 ext 大字（白）
    HFONT hFont = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
    HFONT hOld  = (HFONT)::SelectObject(hdc, hFont);
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
    CString upper = ext; upper.MakeUpper();
    if (upper.GetLength() > 4) { upper = upper.Left(4); }
    RECT rcExt = { x, y + 14, x + kFileIconW, y + kFileIconH - 4 };
    ::DrawText(hdc, upper, -1, &rcExt,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

static int MeasureFile(const ChatMessage&, int /*contentW*/)
{
    return kFileCardH;
}

static void PaintFile(HDC hdc, const RECT& rcRow, int contentLeft, int contentRight,
                      const ChatMessage& m)
{
    int avX, avY, cardX;
    avY = rcRow.top;
    if (m.fromSelf)
    {
        avX   = contentRight - kAvatarSizePx;
        cardX = avX - kAvatarGapPx - kFileCardW;
    }
    else
    {
        avX   = contentLeft;
        cardX = avX + kAvatarSizePx + kAvatarGapPx;
    }
    PaintAvatar(hdc, avX, avY, kAvatarSizePx, m.avatarLetter, m.avatarBg);

    RECT rcCard = { cardX, avY, cardX + kFileCardW, avY + kFileCardH };
    FillRoundRect(hdc, rcCard, kBubbleRadius, RGB(255, 255, 255));

    // 左侧 ext icon
    int iconX = rcCard.left + kFileCardPad;
    int iconY = rcCard.top  + (kFileCardH - kFileIconH) / 2;
    PaintFileExtIcon(hdc, iconX, iconY, m.fileExt);

    // 右侧文字区：上面文件名（深色一行 + 必要时省略），下面大小（小灰）
    int textX = iconX + kFileIconW + 12;
    int textRight = rcCard.right - kFileCardPad;
    HFONT hOld = (HFONT)::SelectObject(hdc, EnsureChatTextFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);

    COLORREF oldClr = ::SetTextColor(hdc, RGB(26, 26, 26));
    RECT rcName = { textX, rcCard.top + 12, textRight, rcCard.top + 36 };
    ::DrawText(hdc, m.fileName, -1, &rcName,
               DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    ::SetTextColor(hdc, RGB(140, 140, 140));
    RECT rcSz = { textX, rcCard.bottom - 26, textRight, rcCard.bottom - 8 };
    ::DrawText(hdc, m.fileSizeText, -1, &rcSz,
               DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

// ---- 派发 -----------------------------------------------------------------

static int MeasureMessage(const ChatMessage& m, int contentW)
{
    switch (m.kind)
    {
    case Kind_Text:          return MeasureTextBubble(m, contentW);
    case Kind_Image:         return MeasureImage      (m, contentW);
    case Kind_TransferCard:  return MeasureTransferCard(m, contentW);
    case Kind_KnowledgeCard: return MeasureKnowledgeCard(m, contentW);
    case Kind_DateDivider:   return MeasureDateDivider(m, contentW);
    case Kind_File:          return MeasureFile       (m, contentW);
    }
    return 0;
}

static void PaintMessage(HDC hdc, const RECT& rcRow, int contentLeft, int contentRight,
                         const ChatMessage& m)
{
    switch (m.kind)
    {
    case Kind_Text:          PaintTextBubble    (hdc, rcRow, contentLeft, contentRight, m); break;
    case Kind_Image:         PaintImage         (hdc, rcRow, contentLeft, contentRight, m); break;
    case Kind_TransferCard:  PaintTransferCard  (hdc, rcRow, contentLeft, contentRight, m); break;
    case Kind_KnowledgeCard: PaintKnowledgeCard (hdc, rcRow, contentLeft, contentRight, m); break;
    case Kind_DateDivider:   PaintDateDivider   (hdc, rcRow, contentLeft, contentRight, m); break;
    case Kind_File:          PaintFile          (hdc, rcRow, contentLeft, contentRight, m); break;
    }
}

// ===========================================================================
// ChatMessageList 实现
// ===========================================================================

ChatMessageList::ChatMessageList()
{
    SetTabStop(false);

    // 内嵌纵向 scrollbar，启用 auto-hide：默认隐藏，鼠标 hover / 滚动时
    // 渐入，离开 800ms 后渐出。回调让 thumb 拖动时立刻 Invalidate 重画
    // 内容（不靠 DUI_NOTIFY 转一圈）。
    auto sb = std::unique_ptr<balloonwjui::DuiScrollBar>(
        new balloonwjui::DuiScrollBar(/*horizontal=*/false));
    sb->SetLineSize(40);
    sb->SetAutoHide(true);
    sb->SetOnScroll(&ChatMessageList::OnSbScrolledStub_, this);
    m_sb = sb.get();
    AddChild(std::move(sb));
}

// 静态 thunk —— DuiScrollBar::OnScrollFn 是 C 风格函数指针。
void ChatMessageList::OnSbScrolledStub_(void* user, int /*newPos*/)
{
    if (auto* self = static_cast<ChatMessageList*>(user))
    {
        self->Invalidate();
    }
}

void ChatMessageList::SetMessages(const std::vector<ChatMessage>* msgs)
{
    m_msgs = msgs;
    if (m_sb)
    {
        m_sb->SetPos(0, /*notify*/false);   // 切会话回顶部
    }
    RelayoutMessages_();
    UpdateScrollRange_();
    Invalidate();
}

void ChatMessageList::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    // scrollbar 占右侧 kSbWidth；内容区在左侧
    if (m_sb)
    {
        RECT rcSb = { rcAvail.right - kSbWidth, rcAvail.top,
                      rcAvail.right,            rcAvail.bottom };
        m_sb->SetRect(rcSb);
    }
    RelayoutMessages_();
    UpdateScrollRange_();
}

void ChatMessageList::UpdateScrollRange_()
{
    if (!m_sb) { return; }
    int viewH    = m_rcItem.bottom - m_rcItem.top;
    int maxPos   = m_contentH - viewH;
    if (maxPos < 0) { maxPos = 0; }
    m_sb->SetRange(0, maxPos);
    m_sb->SetPage(viewH);
    m_sb->SetPos(m_sb->GetPos(), /*notify*/false);    // 自动 clamp
}

void ChatMessageList::RelayoutMessages_()
{
    m_layouts.clear();
    m_contentH = 0;
    if (!m_msgs)
    {
        return;
    }
    int contentW = (m_rcItem.right - m_rcItem.left) - kListPadX * 2 - kSbWidth;
    if (contentW <= 0)
    {
        return;
    }
    m_layouts.reserve(m_msgs->size());
    int y = kListPadTop;
    for (const auto& msg : *m_msgs)
    {
        int h = MeasureMessage(msg, contentW);
        MsgLayout L = { y, h };
        m_layouts.push_back(L);
        y += h + kRowGapPx;
    }
    if (y > kListPadTop)
    {
        y -= kRowGapPx;     // 最后一条不要尾随 gap
    }
    m_contentH = y + kListPadBot;
}

void ChatMessageList::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    // 1) 底色
    FillSolidRect(hdc, m_rcItem, kBgRgb);

    if (m_msgs && !m_layouts.empty())
    {
        // 2) clip 到 m_rcItem 内（避免超出聊天区影响相邻控件）
        HRGN clip = ::CreateRectRgnIndirect(&m_rcItem);
        HRGN oldClip = ::CreateRectRgn(0, 0, 0, 0);
        int hasOld = ::GetClipRgn(hdc, oldClip);
        ::SelectClipRgn(hdc, clip);

        int scrollY = m_sb ? m_sb->GetPos() : 0;
        int contentLeft  = m_rcItem.left + kListPadX;
        int contentRight = m_rcItem.right - kListPadX - kSbWidth;   // 给 sb 留位置
        int viewTop      = m_rcItem.top;

        // 3) 找到第一条可见消息（线性扫描；几条至几十条规模无所谓）
        for (size_t i = 0; i < m_layouts.size(); ++i)
        {
            int absTop    = viewTop + m_layouts[i].top    - scrollY;
            int absBottom = absTop  + m_layouts[i].height;
            if (absBottom < m_rcItem.top)    { continue; }   // 已滚出顶
            if (absTop    > m_rcItem.bottom) { break;    }   // 已滚出底
            RECT rcRow = { m_rcItem.left, absTop, m_rcItem.right, absBottom };
            RECT inter;
            if (::IntersectRect(&inter, &rcRow, &rcDirty))
            {
                PaintMessage(hdc, rcRow, contentLeft, contentRight, (*m_msgs)[i]);
            }
        }

        // 4) 还原 clip
        ::SelectClipRgn(hdc, hasOld == 1 ? oldClip : nullptr);
        ::DeleteObject(clip);
        ::DeleteObject(oldClip);
    }

    // 让基类把 scrollbar child 画在最上层
    DuiControl::OnPaint(hdc, rcDirty);
}

bool ChatMessageList::OnMouseWheel(POINT, short zDelta, UINT)
{
    if (!m_sb) { return false; }
    // zDelta 一档 = 120；正方向往上滚，往上滚 = scrollY 减小
    int delta = (zDelta / 120) * kScrollWheelPx;
    m_sb->TriggerShow();
    m_sb->SetPos(m_sb->GetPos() - delta, /*notify*/true);
    return true;
}

bool ChatMessageList::OnMouseMove(POINT, UINT)
{
    if (m_sb) { m_sb->TriggerShow(); }
    return false;
}

bool ChatMessageList::OnMouseLeave()
{
    if (m_sb) { m_sb->StartFadeOut(); }
    return false;
}

} // namespace xchat
