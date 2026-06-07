/**
 *  DuiToast 实现 —— 顶层浮出的轻量提示控件
 *  balloonwj@qq.com   2026-05-25
 *
 *  本文件用途:实现 DuiToast 控件:setter / Layout / OnPaint /
 *  动画链路 / 静态 helper。OnPaint 走"先画到 32bpp memDc, 再
 *  AlphaBlend 合到 host(SourceConstantAlpha=255*m_alpha)"二级
 *  渲染, 保证渐入渐出对背景 + 文字 + 图标统一生效。
 */
#include "stdafx.h"
#include "DuiToast.h"

#if BUI_FEATURE_TOAST

#include "../../DuiPaintAA.h"
#include "../../DuiResMgr.h"
#include "../../DuiAnimation.h"
#include "../../DuiDpi.h"
#include <gdiplus.h>

// memDc → host 的合成走 ::AlphaBlend(SourceConstantAlpha + AC_SRC_ALPHA)。
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

namespace {

// ---- 几何常量 ----------------------------------------------------------------

// Toast 内部左右 padding(px):图标 / 文字距 toast 边的水平内距。
const int kHPaddingPx = 14;
// Toast 内部上下 padding(px):上下边距。
const int kVPaddingPx = 8;
// 不透明度浮点对比阈值:小于此值视作"实际不可见", 跳过 OnPaint 整段渲染。
const double kAlphaEpsilon = 0.001;
// 文本截断"..."字符串:用于 ApplyEllipsis。
LPCTSTR const kEllipsis = _T("...");

// 在 path 上构造一个圆角矩形(浮点坐标), 供 OnPaint 的背景与阴影复用,
// 避免两处各写一份四段 AddArc。
//   path:输出路径(调用方传入新建的空 path)。
//   x / y:矩形左上角(像素, 可为负 —— 阴影外扩时会用到)。
//   w / h:矩形宽 / 高(像素)。
//   r:圆角半径(像素);<=0 退化为直角矩形;过大自动夹到 min(w,h)/2。
void BuildRoundRectPathF(Gdiplus::GraphicsPath& path,
                         float x, float y, float w, float h, int r)
{
    int halfMin = (int)((w < h ? w : h) / 2.0f);
    if (r > halfMin)
    {
        r = halfMin;
    }
    if (r < 0)
    {
        r = 0;
    }
    if (r > 0)
    {
        Gdiplus::REAL d = (Gdiplus::REAL)(r * 2);
        path.AddArc(x,         y,         d, d, 180.0f, 90.0f);
        path.AddArc(x + w - d, y,         d, d, 270.0f, 90.0f);
        path.AddArc(x + w - d, y + h - d, d, d,   0.0f, 90.0f);
        path.AddArc(x,         y + h - d, d, d,  90.0f, 90.0f);
        path.CloseFigure();
    }
    else
    {
        path.AddRectangle(Gdiplus::RectF(x, y, w, h));
    }
}

} // anonymous

DuiToast::DuiToast()
{
    SetTabStop(false);
    SetVisible(false);   // 默认隐藏, 由 Show 唤起
}

DuiToast::~DuiToast()
{
    // 让任何还在动画队列里的回调失效(它们捕获了 this 指针)。
    // 实际 DuiAnimMgr 没法主动取消, 但 m_animGen++ 后所有 in-flight callback
    // 第一行检查 gen == m_animGen 都会失败 → 早退、不再触碰本对象。
    ++m_animGen;
}

// ---- setter(同值跳过 Invalidate, 与 balloonui 其它控件一致风格)---------

void DuiToast::SetDurationMs(int ms)
{
    if (ms <= 0)
    {
        ms = 1;    // 至少留 1ms, 避免 0 触发动画立即完成的边界
    }
    m_durationMs = ms;
}

void DuiToast::SetFadeMs(int ms)
{
    if (ms < 0)
    {
        ms = 0;
    }
    m_fadeMs = ms;
}

void DuiToast::SetTextColor(COLORREF c)
{
    if (m_textColor == c)
    {
        return;
    }
    m_textColor = c;
    Invalidate();
}

void DuiToast::SetBgColor(COLORREF c)
{
    if (m_bgColor == c)
    {
        return;
    }
    m_bgColor = c;
    Invalidate();
}

void DuiToast::SetCornerRadius(int px)
{
    if (px < 0)
    {
        px = 0;
    }
    if (m_cornerRadius == px)
    {
        return;
    }
    m_cornerRadius = px;
    Invalidate();
}

void DuiToast::SetShadowEnabled(bool on)
{
    if (m_shadowEnabled == on)
    {
        return;
    }
    m_shadowEnabled = on;
    Invalidate();
}

void DuiToast::SetShadowColor(COLORREF c)
{
    if (m_shadowColor == c)
    {
        return;
    }
    m_shadowColor = c;
    Invalidate();
}

void DuiToast::SetShadowAlpha(int a)
{
    if (a < 0)
    {
        a = 0;       // 负值等效关闭阴影
    }
    if (a > 255)
    {
        a = 255;     // 不透明度上限
    }
    if (m_shadowAlpha == a)
    {
        return;
    }
    m_shadowAlpha = a;
    Invalidate();
}

void DuiToast::SetShadowBlur(int px)
{
    if (px < 1)
    {
        px = 1;      // 至少一层, 近似硬阴影
    }
    if (m_shadowBlur == px)
    {
        return;
    }
    m_shadowBlur = px;
    Invalidate();
}

void DuiToast::SetShadowOffsetY(int px)
{
    if (m_shadowOffsetY == px)
    {
        return;
    }
    m_shadowOffsetY = px;
    Invalidate();
}

void DuiToast::SetIcon(HBITMAP hBmp)
{
    if (m_icon == hBmp)
    {
        return;
    }
    m_icon = hBmp;
    Invalidate();
}

void DuiToast::SetIconSize(int px)
{
    if (px < 1)
    {
        px = 1;
    }
    if (m_iconSize == px)
    {
        return;
    }
    m_iconSize = px;
    Invalidate();
}

void DuiToast::SetIconGap(int px)
{
    if (px < 0)
    {
        px = 0;
    }
    if (m_iconGap == px)
    {
        return;
    }
    m_iconGap = px;
    Invalidate();
}

void DuiToast::SetFont(HFONT hFont)
{
    if (m_font == hFont)
    {
        return;
    }
    m_font = hFont;
    Invalidate();
}

void DuiToast::SetTextPointSize(int pt, bool bold)
{
    // pt <= 0 退化为默认字体(等同 SetFont(nullptr) → 走 toast 默认 AA 字体)。
    // 注意:走 GetAntiAliasedFontByPointSize 而非 GetFontByPointSize, 避免
    // ClearType + AlphaBlend 子像素错位"重影"。
    HFONT hf = (pt <= 0) ? nullptr
                         : DuiResMgr::Inst().GetAntiAliasedFontByPointSize(pt, bold);
    SetFont(hf);
}

void DuiToast::SetTopOffset(int px)
{
    if (m_topOffset == px)
    {
        return;
    }
    m_topOffset = px;
    // 不直接调 Layout —— host 下次 Layout 时会重新分发到本控件。
    Invalidate();
}

void DuiToast::SetMaxWidth(int px)
{
    if (px < 0)
    {
        px = 0;
    }
    if (m_maxWidth == px)
    {
        return;
    }
    m_maxWidth = px;
    Invalidate();
}

// ---- 静态 helper -----------------------------------------------------------

int DuiToast::MeasureWidth(int textPx, bool hasIcon, int iconSize, int iconGap)
{
    if (textPx < 0)
    {
        textPx = 0;
    }
    if (iconSize < 0)
    {
        iconSize = 0;
    }
    if (iconGap < 0)
    {
        iconGap = 0;
    }
    int contentW = textPx;
    if (hasIcon)
    {
        contentW += iconSize + iconGap;
    }
    return kHPaddingPx + contentW + kHPaddingPx;
}

CString DuiToast::ApplyEllipsis(LPCTSTR text, int maxChars)
{
    CString t = text ? text : _T("");
    if (maxChars <= 0)
    {
        return t;
    }
    if (t.GetLength() <= maxChars)
    {
        return t;
    }
    // 截到 maxChars - 3, 末尾加 "..."(确保总长 == maxChars)。
    // 若 maxChars < 3, 退化为单纯硬截(避免负值 Left)。
    int keep = maxChars - 3;
    if (keep <= 0)
    {
        return t.Left(maxChars);
    }
    return t.Left(keep) + kEllipsis;
}

// ---- 显示 / 隐藏 -----------------------------------------------------------

void DuiToast::Show(LPCTSTR text)
{
    CString t = text ? text : _T("");
    if (t.IsEmpty())
    {
        HideNow();
        return;
    }

    m_text = t;
    // 旧动画链路自废:任何 in-flight callback 都会检查 gen 失败、早退。
    ++m_animGen;
    int gen = m_animGen;

    SetVisible(true);
    // 不重置 m_alpha — 从当前值起渐入, 多次 Show 不会有"先消失再出现"。
    // 但首次 Show / HideNow 后再 Show 时 m_alpha 应是 0, 自然从 0 起渐。

    // 关键:toast 初始 Visible=false, 父容器(VBox 等)会跳过 layout 它,
    // m_rcItem 保持全 0 → OnPaint 早退 w/h<=0 → 看不到 toast。
    // 因此 Show 时主动用父 rect 触发一次 Layout, 重算 m_rcItem(顶居中)。
    DuiControl* parent = GetParent();
    if (parent)
    {
        Layout(parent->GetRect());
    }
    Invalidate();

    if (m_fadeMs <= 0)
    {
        // 无渐入动画:alpha 直接 1, 进入 Hold。
        m_alpha = 1.0;
        StartHold(gen);
    }
    else
    {
        StartFadeIn(gen);
    }
}

void DuiToast::HideNow()
{
    ++m_animGen;          // 老 callback 自废
    m_alpha = 0.0;
    if (m_bVisible)
    {
        SetVisible(false);
    }
    Invalidate();
}

void DuiToast::FinishHide()
{
    m_alpha = 0.0;
    if (m_bVisible)
    {
        SetVisible(false);
    }
    Invalidate();
}

// ---- 动画链路:渐入 → 显示等待 → 渐出 → 收尾 -----------------------------

void DuiToast::StartFadeIn(int gen)
{
    // m_alpha 起点(可能不是 0, 见 Show 中"快速连续 Show"语义)。
    double from = m_alpha;
    auto a = std::make_unique<DuiDoubleAnim>(m_fadeMs, from, 1.0,
        [this, gen](double v)
        {
            if (gen != m_animGen)
            {
                return;       // 已被新 Show / HideNow 替换, 早退
            }
            m_alpha = v;
            Invalidate();
        });
    a->SetOnComplete([this, gen]()
        {
            if (gen != m_animGen)
            {
                return;
            }
            StartHold(gen);
        });
    DuiAnimMgr::Inst().Add(std::move(a));
}

void DuiToast::StartHold(int gen)
{
    // 显示等待:占位 anim, setter 啥都不做, 仅靠 OnComplete 跳到下一段。
    auto a = std::make_unique<DuiDoubleAnim>(m_durationMs, 0.0, 0.0,
        [](double){});
    a->SetOnComplete([this, gen]()
        {
            if (gen != m_animGen)
            {
                return;
            }
            if (m_fadeMs <= 0)
            {
                // 无渐出动画:直接收尾。
                FinishHide();
            }
            else
            {
                StartFadeOut(gen);
            }
        });
    DuiAnimMgr::Inst().Add(std::move(a));
}

void DuiToast::StartFadeOut(int gen)
{
    double from = m_alpha;
    auto a = std::make_unique<DuiDoubleAnim>(m_fadeMs, from, 0.0,
        [this, gen](double v)
        {
            if (gen != m_animGen)
            {
                return;
            }
            m_alpha = v;
            Invalidate();
        });
    a->SetOnComplete([this, gen]()
        {
            if (gen != m_animGen)
            {
                return;
            }
            FinishHide();
        });
    DuiAnimMgr::Inst().Add(std::move(a));
}

// ---- Layout(自定位顶居中 + topOffset)-------------------------------------

void DuiToast::Layout(const RECT& rcAvail)
{
    // 关键:toast 是"顶层浮出"控件, 始终基于<u>父容器</u>的完整 rect 算
    // "顶居中 + topOffset"位置 —— 完全忽略 caller(父 layout)传入的
    // rcAvail。因为父 VBox / HBox 可能给 toast 分了 Fixed(0) 高的 rect,
    // 那不是 toast 该有的位置。
    //
    // 无父(单测、root)时退化为 caller 传入的 rcAvail。
    RECT base;
    DuiControl* parent = GetParent();
    if (parent)
    {
        base = parent->GetRect();
    }
    else
    {
        base = rcAvail;
    }

    // 量算文字宽 —— 必须用 GDI+ MeasureString, 与 OnPaint step 1.b 的
    // GDI+ DrawString 一致。<u>不</u>用 GDI ::GetTextExtentPoint32, 否则
    // GDI 量算比 GDI+ 渲染窄, OnPaint 会误判文字超宽触发 ApplyEllipsis
    // 截断, 看起来"文字显示不全"。
    // 触发 GDI+ 懒初始化(借 DuiAA), 否则下面 Gdiplus::Graphics ctor 会失败。
    DuiAA::FillRoundRect(nullptr, RECT{0, 0, 0, 0}, CLR_INVALID, 0);

    int textPx = 0;
    int textHeightPx = 0;
    if (!m_text.IsEmpty())
    {
        HDC dc = ::GetDC(nullptr);
        Gdiplus::Graphics g(dc);
        Gdiplus::Font* font = nullptr;
        if (m_font)
        {
            font = new Gdiplus::Font(dc, m_font);
        }
        else
        {
            LOGFONT lf = {};
            lf.lfHeight = -::MulDiv(9, DuiDpi::GetSystemDpi(), 72);
            lf.lfWeight = FW_NORMAL;
            lf.lfCharSet = GB2312_CHARSET;
            lf.lfQuality = ANTIALIASED_QUALITY;
            lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
            _tcsncpy_s(lf.lfFaceName, _T("Microsoft YaHei"), _TRUNCATE);
            font = new Gdiplus::Font(dc, &lf);
        }
        if (font && font->GetLastStatus() == Gdiplus::Ok)
        {
            Gdiplus::RectF bound;
            Gdiplus::PointF zero(0.0f, 0.0f);
            g.MeasureString(m_text, m_text.GetLength(), font, zero, &bound);
            // 向上取整, 再加 1 防 GDI+ 渲染时与 MeasureString 微小偏差(亚像素)。
            textPx = (int)(bound.Width  + 0.999f) + 1;
            textHeightPx = (int)(bound.Height + 0.999f);
        }
        delete font;
        ::ReleaseDC(nullptr, dc);
    }

    int totalW = MeasureWidth(textPx, m_icon != nullptr, m_iconSize, m_iconGap);
    if (m_maxWidth > 0 && totalW > m_maxWidth)
    {
        totalW = m_maxWidth;
    }
    // toast 高度:max(图标高, 文字行高) + 上下 padding。文字行高优先用实际
    // 量出的 textHeightPx(随字号变), 否则退化为默认近似 18px。
    const int kApproxLineH = 18;
    int lineH = textHeightPx > 0 ? textHeightPx : kApproxLineH;
    int contentH = (m_iconSize > lineH) ? m_iconSize : lineH;
    int totalH = kVPaddingPx + contentH + kVPaddingPx;

    // 阴影留边:绘制位图须在内容盒四周扩出 margin, 容纳羽化阴影 + 竖直偏移。
    // margin 计入 m_rcItem(而非仅在 OnPaint 内部), 这样淡入淡出每帧
    // Invalidate(m_rcItem) 都覆盖阴影区, 阴影不会残留。OnPaint 须用同一
    // 公式反推 margin, 二者必须一致。
    int margin = 0;
    if (m_shadowEnabled && m_shadowAlpha > 0)
    {
        int absOffsetY = m_shadowOffsetY >= 0 ? m_shadowOffsetY : -m_shadowOffsetY;
        margin = m_shadowBlur + absOffsetY;
    }

    int hostCx = (base.left + base.right) / 2;
    // 内容盒左上(顶居中 + topOffset);m_rcItem 再四周外扩 margin 含阴影。
    int contentX = hostCx - totalW / 2;
    int contentY = base.top + m_topOffset;
    m_rcItem.left   = contentX - margin;
    m_rcItem.top    = contentY - margin;
    m_rcItem.right  = contentX + totalW + margin;
    m_rcItem.bottom = contentY + totalH + margin;
}

// ---- OnPaint(PARGB 中转 + AlphaBlend 二级渲染, 支持渐入渐出 + AA 圆角)----

void DuiToast::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }
    if (m_alpha < kAlphaEpsilon)
    {
        return;   // 实际不可见, 跳过整段渲染
    }

    int w = m_rcItem.right - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // ---- 关键流水线 ----
    // 旧方案: 直接画到 BI_RGB 32bpp DIB → GDI+ 不写 alpha 通道 → AlphaBlend
    //   把"圆角外的初始黑色(RGB=0, alpha=0 → 被当 0 alpha 处理)"画成黑边/锯齿。
    // 新方案:
    //   1) GDI+ 在 PARGB Bitmap 上画背景圆角 → alpha 通道正确写(圆角外=0,
    //      内=255, 边缘 AA 半透明)
    //   2) LockBits 复制 PARGB 像素到 BI_RGB DIBSection memDc(alpha 通道
    //      已包含在 32bpp 数据里, BI_RGB 不关心怎么解释这 4 个字节)
    //   3) GDI 在 memDc 上画图标 + 文字(只写 RGB 不改 alpha;图标/文字
    //      所在像素的 alpha 仍是背景 step 1 写的 255, OK)
    //   4) AlphaBlend memDc → host, AlphaFormat=AC_SRC_ALPHA, SourceConstantAlpha=
    //      (BYTE)(255*m_alpha) —— 整体淡入淡出 + 圆角外 alpha=0 不画黑边。

    // 借 DuiAA 任意调用触发 GDI+ 懒初始化(GdiplusStartup token), 否则下面
    // Gdiplus::Bitmap ctor 会失败。0x0 rect 在 DuiAA 内部早退, 无副作用。
    DuiAA::FillRoundRect(hdc, RECT{0, 0, 0, 0}, CLR_INVALID, 0);

    // 1) GDI+ PARGB Bitmap, 画 AA 圆角背景 + 文字(都走 GDI+, alpha 通道正确)。
    //    实证:GDI ::DrawText 在 BI_RGB 32bpp DIB 上写字会出现"重影"(二分
    //    隔离锁定了该路径);改成 GDI+ DrawString 在 PARGB Bitmap 上画, 由
    //    GDI+ 统一管理 alpha 通道, 避免该问题。
    Gdiplus::Bitmap pargb(w, h, PixelFormat32bppPARGB);
    CString showText = m_text;
    int textPxOnPargb = 0;

    // 内容盒在绘制位图内的位置:四周留 cm 阴影边距, 内容盒尺寸 cw×ch。
    // cm 公式须与 Layout 中 margin 完全一致(否则内容盒会偏出位图)。文字段
    // (下方 GDI+ block 内)与图标段(block 外, 走 GDI ::AlphaBlend)都要用
    // cm / cw / ch, 故提到 block 外声明。
    int cm = 0;
    if (m_shadowEnabled && m_shadowAlpha > 0)
    {
        int absOffsetY = m_shadowOffsetY >= 0 ? m_shadowOffsetY : -m_shadowOffsetY;
        cm = m_shadowBlur + absOffsetY;
    }
    int cw = w - 2 * cm;
    int ch = h - 2 * cm;
    if (cw <= 0 || ch <= 0)
    {
        // 极端小尺寸下阴影边距把内容盒挤没了:退化为不留边、不画阴影。
        cm = 0;
        cw = w;
        ch = h;
    }

    {
        Gdiplus::Graphics g(&pargb);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));   // 内容盒 / 阴影外保持全透明

        // 圆角半径以内容盒短边夹取(而非整张位图)。
        int r = m_cornerRadius;
        int halfMin = (cw < ch ? cw : ch) / 2;
        if (r > halfMin)
        {
            r = halfMin;
        }
        if (r < 0)
        {
            r = 0;
        }

        // 1.a 阴影:逐层叠加、由内到外渐淡的羽化圆角。每层等量 alpha, 经
        //     AC_SRC_OVER 合成后内部累积≈m_shadowAlpha, 边缘只被外层覆盖
        //     故渐减 —— 形成柔和羽化。最内层覆盖内容盒, 随后被不透明背景
        //     盖住, 只在内容盒外露出一圈渐变。
        if (m_shadowEnabled && m_shadowAlpha > 0 && cm > 0)
        {
            int blur = m_shadowBlur;
            if (blur < 1)
            {
                blur = 1;
            }
            int aLayer = m_shadowAlpha / blur;
            if (aLayer < 1)
            {
                aLayer = 1;       // 每层至少 1, 避免整圈阴影消失
            }
            // 内容盒在位图内左上 = (cm, cm), 阴影整体下移 m_shadowOffsetY。
            float sx = (float)cm;
            float sy = (float)cm + (float)m_shadowOffsetY;
            int k = blur;
            for (; k >= 1; --k)
            {
                Gdiplus::GraphicsPath spath;
                BuildRoundRectPathF(spath,
                                    sx - (float)k, sy - (float)k,
                                    (float)cw + (float)(2 * k),
                                    (float)ch + (float)(2 * k),
                                    r + k);
                Gdiplus::SolidBrush sbr(Gdiplus::Color((BYTE)aLayer,
                                                       GetRValue(m_shadowColor),
                                                       GetGValue(m_shadowColor),
                                                       GetBValue(m_shadowColor)));
                g.FillPath(&sbr, &spath);
            }
        }

        // 1.b 圆角背景(画在内容盒区, 盖住其下阴影内部, 只留外圈羽化)。
        Gdiplus::GraphicsPath path;
        BuildRoundRectPathF(path, (float)cm, (float)cm,
                            (float)(cw - 1), (float)(ch - 1), r);
        Gdiplus::SolidBrush br(Gdiplus::Color(255,
                                              GetRValue(m_bgColor),
                                              GetGValue(m_bgColor),
                                              GetBValue(m_bgColor)));
        g.FillPath(&br, &path);

        // 1.c 文字(GDI+ DrawString;字体由 HFONT 转 Gdiplus::Font)
        if (!m_text.IsEmpty())
        {
            // Gdiplus::Font 从 HFONT 转需要 hdc;借 host hdc 即可(只用于字体度量)。
            // m_font 是 caller-owned HFONT;nullptr 则构造 LOGFONT 默认。
            Gdiplus::Font* font = nullptr;
            if (m_font)
            {
                font = new Gdiplus::Font(hdc, m_font);
            }
            else
            {
                // 默认 9pt YaHei ANTIALIASED_QUALITY (与之前 Layout 量算一致)。
                LOGFONT lf = {};
                lf.lfHeight = -::MulDiv(9, DuiDpi::GetSystemDpi(), 72);
                lf.lfWeight = FW_NORMAL;
                lf.lfCharSet = GB2312_CHARSET;
                lf.lfQuality = ANTIALIASED_QUALITY;
                lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
                _tcsncpy_s(lf.lfFaceName, _T("Microsoft YaHei"), _TRUNCATE);
                font = new Gdiplus::Font(hdc, &lf);
            }

            if (font && font->GetLastStatus() == Gdiplus::Ok)
            {
                // 量算文字宽 + 应用 MaxWidth 截断逻辑。可用文字宽基于内容盒
                // cw(不含四周阴影留边), 否则会把阴影边距误算进可用宽。
                int availTextW = cw - kHPaddingPx - kHPaddingPx;
                if (m_icon)
                {
                    availTextW -= m_iconSize + m_iconGap;
                }
                Gdiplus::RectF measureBound;
                Gdiplus::PointF zero(0.0f, 0.0f);
                g.MeasureString(m_text, m_text.GetLength(), font, zero, &measureBound);
                int textW = (int)measureBound.Width;
                if (textW > availTextW && availTextW > 0)
                {
                    // 用 ApplyEllipsis 截断, 用 GDI+ MeasureString 量到刚好 fit。
                    int n = m_text.GetLength();
                    while (n > 0)
                    {
                        CString cand = ApplyEllipsis(m_text, n);
                        Gdiplus::RectF candBound;
                        g.MeasureString(cand, cand.GetLength(), font, zero, &candBound);
                        if ((int)candBound.Width <= availTextW)
                        {
                            showText = cand;
                            textW = (int)candBound.Width;
                            break;
                        }
                        --n;
                    }
                }
                textPxOnPargb = textW;

                // 整组(图标 + gap + 文字)在内容盒内居中: 图标区由 step 3 GDI
                // 画, 但这里文字 startX 必须与之配合算对位置(基于内容盒 cm/cw)。
                int totalContentW = (m_icon ? (m_iconSize + m_iconGap) : 0) + textW;
                int startX = cm + (cw - totalContentW) / 2;
                if (m_icon)
                {
                    startX += m_iconSize + m_iconGap;
                }

                Gdiplus::SolidBrush textBrush(Gdiplus::Color(255,
                                                             GetRValue(m_textColor),
                                                             GetGValue(m_textColor),
                                                             GetBValue(m_textColor)));
                Gdiplus::StringFormat sf;
                sf.SetAlignment(Gdiplus::StringAlignmentNear);
                sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap |
                                  Gdiplus::StringFormatFlagsNoClip);
                // 修复"文字尾部被切": 不能把 DrawString 的矩形宽钉成 textW
                // (=floor(MeasureString))。实证 GenericDefault 排版下, 用量出
                // 的宽当矩形宽会稳定裁掉约一个字宽的尾部墨迹(同段文字窄矩形
                // 比足够宽矩形少画 11~13px)。这里把矩形右边放宽到内容盒右内
                // 边距, 文字仍按 startX 左对齐, 故能完整渲染。竖直方向也改用
                // 内容盒 [cm, cm+ch], 配合 LineAlignmentCenter 居中。
                Gdiplus::REAL textRight = (Gdiplus::REAL)(cm + cw - kHPaddingPx);
                Gdiplus::REAL rectW = textRight - (Gdiplus::REAL)startX;
                if (rectW < (Gdiplus::REAL)textW)
                {
                    rectW = (Gdiplus::REAL)textW;   // 兜底:至少给量出的宽
                }
                Gdiplus::RectF textRect((Gdiplus::REAL)startX, (Gdiplus::REAL)cm,
                                        rectW, (Gdiplus::REAL)ch);
                g.DrawString(showText, showText.GetLength(), font, textRect,
                             &sf, &textBrush);
            }
            delete font;
        }
    }

    // 2) LockBits → 复制到 BI_RGB 32bpp DIBSection memDc。
    Gdiplus::Rect lockRc(0, 0, w, h);
    Gdiplus::BitmapData bd;
    if (pargb.LockBits(&lockRc, Gdiplus::ImageLockModeRead,
                       PixelFormat32bppPARGB, &bd) != Gdiplus::Ok)
    {
        return;
    }

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;          // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* destBits = nullptr;
    HBITMAP memBmp = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS,
                                        &destBits, nullptr, 0);
    if (!memBmp || !destBits)
    {
        pargb.UnlockBits(&bd);
        if (memBmp)
        {
            ::DeleteObject(memBmp);
        }
        return;
    }
    {
        const int rowBytes = w * 4;
        BYTE* src = static_cast<BYTE*>(bd.Scan0);
        BYTE* dst = static_cast<BYTE*>(destBits);
        for (int y = 0; y < h; ++y)
        {
            ::memcpy(dst + y * rowBytes, src + y * bd.Stride, rowBytes);
        }
    }
    pargb.UnlockBits(&bd);

    HDC memDc = ::CreateCompatibleDC(hdc);
    HBITMAP oldBmp = (HBITMAP)::SelectObject(memDc, memBmp);

    // 3) 在 memDc 上用 GDI 画图标(alpha 通道由背景保持:圆角内 = 255)。
    //    文字已在 step 1.c 用 GDI+ DrawString 画到 PARGB Bitmap 上(避免
    //    GDI ::DrawText 在 BI_RGB DIB 写文字导致 alpha 通道污染产生重影);
    //    此处只剩图标走 GDI ::AlphaBlend, 整组(图标 + gap + 文字)在内容盒
    //    内居中, 位置由 cm / cw / ch 与 step 1.c 已算好的 textPxOnPargb 决定。
    if (m_icon)
    {
        int totalContentW = m_iconSize + m_iconGap + textPxOnPargb;
        int startX = cm + (cw - totalContentW) / 2;
        int cy     = cm + ch / 2;
        int iconY  = cy - m_iconSize / 2;
        BITMAP bi2 = {};
        ::GetObject(m_icon, sizeof(bi2), &bi2);
        int srcW = bi2.bmWidth  > 0 ? bi2.bmWidth  : m_iconSize;
        int srcH = bi2.bmHeight > 0 ? bi2.bmHeight : m_iconSize;
        HDC iconDc = ::CreateCompatibleDC(memDc);
        HBITMAP oldIcon = (HBITMAP)::SelectObject(iconDc, m_icon);
        BLENDFUNCTION bf;
        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat         = AC_SRC_ALPHA;
        ::AlphaBlend(memDc, startX, iconY, m_iconSize, m_iconSize,
                     iconDc, 0, 0, srcW, srcH, bf);
        ::SelectObject(iconDc, oldIcon);
        ::DeleteDC(iconDc);
    }

    // 4) AlphaBlend memDc → host hdc, 用 AC_SRC_ALPHA + SourceConstantAlpha
    //    实现"AA 边缘 + 整体淡入淡出"。AlphaFormat=AC_SRC_ALPHA 让 AlphaBlend
    //    读 DIB alpha 通道(背景 step 1 写的 0/255/AA 半透明), 圆角外不画黑边。
    BLENDFUNCTION bfHost;
    bfHost.BlendOp             = AC_SRC_OVER;
    bfHost.BlendFlags          = 0;
    bfHost.SourceConstantAlpha = (BYTE)(255 * m_alpha);
    bfHost.AlphaFormat         = AC_SRC_ALPHA;
    ::AlphaBlend(hdc, m_rcItem.left, m_rcItem.top, w, h,
                 memDc, 0, 0, w, h, bfHost);

    // 5) 清理。
    ::SelectObject(memDc, oldBmp);
    ::DeleteDC(memDc);
    ::DeleteObject(memBmp);
}

DuiControl* DuiToast::HitTest(POINT /*pt*/)
{
    // 永远不参与命中:点击穿透到下层控件。
    return nullptr;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_TOAST
