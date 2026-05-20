#include "stdafx.h"
#include "DemoChatBubble.h"
#include "DuiNotify.h"

using namespace balloonwjui;

void DemoChatBubble::SetSide(Side s)
{
    if (m_side == s) return;
    m_side = s;
    Invalidate();
}

void DemoChatBubble::SetText(LPCTSTR t)
{
    CString s = t ? t : _T("");
    if (m_text == s) return;
    m_text = s;
    Invalidate();
}

void DemoChatBubble::SetTimestamp(LPCTSTR ts)
{
    CString s = ts ? ts : _T("");
    if (m_ts == s) return;
    m_ts = s;
    Invalidate();
}

// -----------------------------------------------------------------------------
// MeasureHeight: 给定 availWidth（控件总宽度，含尾巴），算出气泡占用的高度。
// 步骤：扣掉尾巴宽 + 左右 padding 得到文字可用宽度，用 DT_CALCRECT 算出
// WORDBREAK 后所需高度，再加上下 padding + 时间戳行高（如有）。
// -----------------------------------------------------------------------------
int DemoChatBubble::MeasureHeight(int availWidth) const
{
    int textW = availWidth - kTailWidth - kPadX * 2;
    if (textW < 1) textW = 1;

    HDC hdc = ::GetDC(NULL);
    HFONT f = DuiResMgr::Inst().GetDefaultFont();
    HGDIOBJ oldF = ::SelectObject(hdc, f);

    RECT rc = { 0, 0, textW, 1 };
    ::DrawText(hdc, m_text, -1, &rc,
               DT_CALCRECT | DT_WORDBREAK | DT_LEFT);
    int textH = rc.bottom - rc.top;

    ::SelectObject(hdc, oldF);
    ::ReleaseDC(NULL, hdc);

    int total = textH + kPadY * 2;
    if (!m_ts.IsEmpty())
    {
        total += kMetaGap + kMetaH;
    }
    // 气泡至少要罩住尾巴起点 + 尾巴高度。
    int minHeight = kTailOffsetY + kTailHeight + kPadY;
    return total > minHeight ? total : minHeight;
}

// -----------------------------------------------------------------------------
// PaintBubble: 画气泡身（圆角矩形）+ 尾巴（三角），返回气泡内容矩形。
// -----------------------------------------------------------------------------
RECT DemoChatBubble::PaintBubble(HDC hdc) const
{
    // 气泡矩形：扣掉一侧的尾巴宽
    RECT bubbleRc = m_rcItem;
    if (m_side == SideIn)
    {
        bubbleRc.left += kTailWidth;
    }
    else
    {
        bubbleRc.right -= kTailWidth;
    }

    // 配色
    COLORREF bgColor = (m_side == SideOut)
        ? RGB( 90, 200, 150)             // 我方：浅绿
        : RGB(255, 255, 255);            // 对方：白
    COLORREF border  = (m_side == SideOut)
        ? RGB( 60, 170, 120)
        : RGB(220, 222, 226);

    // 画气泡身
    HBRUSH brBg  = ::CreateSolidBrush(bgColor);
    HPEN   pen   = ::CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldB = ::SelectObject(hdc, brBg);
    HGDIOBJ oldP = ::SelectObject(hdc, pen);
    ::RoundRect(hdc,
                bubbleRc.left, bubbleRc.top,
                bubbleRc.right, bubbleRc.bottom,
                kRadius * 2, kRadius * 2);

    // 画尾巴：三角形指向气泡侧
    POINT tri[3];
    if (m_side == SideIn)
    {
        // 尾巴在左：从气泡左边伸出
        tri[0].x = bubbleRc.left;
        tri[0].y = bubbleRc.top + kTailOffsetY;
        tri[1].x = bubbleRc.left - kTailWidth;
        tri[1].y = bubbleRc.top + kTailOffsetY + kTailHeight / 2;
        tri[2].x = bubbleRc.left;
        tri[2].y = bubbleRc.top + kTailOffsetY + kTailHeight;
    }
    else
    {
        // 尾巴在右
        tri[0].x = bubbleRc.right;
        tri[0].y = bubbleRc.top + kTailOffsetY;
        tri[1].x = bubbleRc.right + kTailWidth;
        tri[1].y = bubbleRc.top + kTailOffsetY + kTailHeight / 2;
        tri[2].x = bubbleRc.right;
        tri[2].y = bubbleRc.top + kTailOffsetY + kTailHeight;
    }
    // GDI Polygon 是带边框 + 内填的。我们想边框只在两条斜边、底边遮掉
    // 气泡边框 — 但 Polygon 会画第三条边。简化：尾巴底边正好压在气泡边
    // 上，看起来"无缝"，不完美但足够清晰。
    ::Polygon(hdc, tri, 3);

    ::SelectObject(hdc, oldB);
    ::SelectObject(hdc, oldP);
    ::DeleteObject(brBg);
    ::DeleteObject(pen);

    // 内容矩形
    RECT inner = bubbleRc;
    ::InflateRect(&inner, -kPadX, -kPadY);
    return inner;
}

void DemoChatBubble::OnPaint(HDC hdc, const RECT&)
{
    RECT inner = PaintBubble(hdc);

    // 文字色：对方深墨，我方深绿（在浅绿底上更易读）
    COLORREF textColor = (m_side == SideOut)
        ? RGB( 20,  60,  40)
        : RGB( 30,  30,  30);

    HFONT f = DuiResMgr::Inst().GetDefaultFont();
    HGDIOBJ oldF = ::SelectObject(hdc, f);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, textColor);

    // 文字区：内容矩形减去时间戳行
    RECT textRc = inner;
    if (!m_ts.IsEmpty())
    {
        textRc.bottom -= (kMetaGap + kMetaH);
    }
    ::DrawText(hdc, m_text, -1, &textRc,
               DT_LEFT | DT_TOP | DT_WORDBREAK);

    // 时间戳：右下角灰小字
    if (!m_ts.IsEmpty())
    {
        ::SetTextColor(hdc, RGB(140, 140, 140));
        RECT metaRc = {
            inner.left, inner.bottom - kMetaH,
            inner.right, inner.bottom
        };
        ::DrawText(hdc, m_ts, -1, &metaRc,
                   DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);
    }

    ::SelectObject(hdc, oldF);
}

bool DemoChatBubble::OnLButtonUp(POINT, UINT)
{
    NotifyParent(DUIN_CLICK);
    return true;
}
