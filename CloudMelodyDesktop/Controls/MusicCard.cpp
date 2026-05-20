#include "stdafx.h"
#include "MusicCard.h"
#include "CoverArt.h"
#include "PaintHelpers.h"
#include "../App/CloudMelodyTheme.h"

#include "DuiResMgr.h"
#include "DuiPaintAA.h"

using namespace balloonwjui;

namespace cloudmelody {

// 卡片下方文字区高度（容纳 title + subtitle 两行）。封面 = 卡片宽 - 文字
// 区高度，让封面保持正方形。
static const int kTextAreaHeight = 44;
// title / sub 行高
static const int kTitleLineHeight = 22;
static const int kSubLineHeight   = 18;

// hover 时浮的"播放徽章"半径（占封面短边的比例）
static const float kPlayBadgeRadiusRatio = 0.20f;
// 徽章右下边距（绝对像素，与封面右 / 下边）
static const int kPlayBadgeMargin = 8;

void MusicCard::SetTitle(LPCTSTR sz)
{
    m_title = sz ? sz : _T("");
    Invalidate();
}

void MusicCard::SetSubtitle(LPCTSTR sz)
{
    m_subtitle = sz ? sz : _T("");
    Invalidate();
}

bool MusicCard::OnMouseEnter()
{
    m_hover = true;
    Invalidate();
    return true;
}

bool MusicCard::OnMouseLeave()
{
    m_hover = false;
    Invalidate();
    return true;
}

bool MusicCard::OnLButtonUp(POINT pt, UINT)
{
    if (::PtInRect(&m_rcItem, pt))
    {
        NotifyParent(DUIN_CLICK, 0);
    }
    return true;
}

bool MusicCard::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    return true;
}

void MusicCard::OnPaint(HDC hdc, const RECT&)
{
    if (!m_bVisible)
    {
        return;
    }

    int w = m_rcItem.right - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    int coverSide = h - kTextAreaHeight;
    if (coverSide < 0) { coverSide = 0; }
    if (coverSide > w) { coverSide = w; }

    RECT rcCover = {
        m_rcItem.left,
        m_rcItem.top,
        m_rcItem.left + coverSide,
        m_rcItem.top + coverSide
    };

    // 1) 封面：渐变 + 标题字（用 m_hueSeed 选色板）
    CoverArt::Paint(hdc, rcCover, m_title, m_hueSeed, kRadiusMd);

    // 2) hover 浮 ▶ 播放徽章 —— 圆形半透明白底 + 居中红三角
    if (m_hover && coverSide > 30)
    {
        int radius = (int)(coverSide * kPlayBadgeRadiusRatio);
        if (radius < 12) { radius = 12; }
        int cx = rcCover.right  - kPlayBadgeMargin - radius;
        int cy = rcCover.bottom - kPlayBadgeMargin - radius;
        RECT rcBadge = { cx - radius, cy - radius,
                         cx + radius, cy + radius };
        // 圆形 + 三角都用 GDI+ 抗锯齿
        DuiAA::FillEllipse(hdc, rcBadge,
                           RGB(0xFF, 0xFF, 0xFF));    // 白底
        // 红三角，缩小到徽章直径的 0.42
        float triH = radius * 0.84f;
        float triW = triH * 0.866f;
        float ox = (float)cx + radius * 0.06f;
        POINT tri[3] = {
            { (LONG)(ox - triW * 0.5f), (LONG)(cy - triH * 0.5f) },
            { (LONG)(ox - triW * 0.5f), (LONG)(cy + triH * 0.5f) },
            { (LONG)(ox + triW * 0.5f), (LONG)(cy)               }
        };
        DuiAA::FillPolygon(hdc, tri, 3, kColorPrimaryAccent);
    }

    // 3) 标题 + 副标题
    HFONT hOld = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);

    if (!m_title.IsEmpty())
    {
        RECT rcTitle = {
            m_rcItem.left,
            rcCover.bottom + 4,
            m_rcItem.right,
            rcCover.bottom + 4 + kTitleLineHeight
        };
        COLORREF oldC = ::SetTextColor(hdc, kColorOnSurface);
        ::DrawText(hdc, (LPCTSTR)m_title, -1, &rcTitle,
                   DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
    }
    if (!m_subtitle.IsEmpty())
    {
        RECT rcSub = {
            m_rcItem.left,
            rcCover.bottom + 4 + kTitleLineHeight,
            m_rcItem.right,
            rcCover.bottom + 4 + kTitleLineHeight + kSubLineHeight
        };
        COLORREF oldC = ::SetTextColor(hdc, kColorOnSurfaceVar);
        ::DrawText(hdc, (LPCTSTR)m_subtitle, -1, &rcSub,
                   DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
    }

    ::SetBkMode(hdc, oldBk);
    ::SelectObject(hdc, hOld);
}

} // namespace cloudmelody
