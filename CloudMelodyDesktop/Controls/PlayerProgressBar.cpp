#include "stdafx.h"
#include "PlayerProgressBar.h"
#include "../App/CloudMelodyTheme.h"

using namespace balloonwjui;

namespace cloudmelody {

void PlayerProgressBar::SetRange(int minV, int maxV)
{
    if (maxV <= minV)
    {
        // 防止除零；caller 给了反向 / 等值范围时降级到 0..1。
        maxV = minV + 1;
    }
    m_min = minV;
    m_max = maxV;
    if (m_value < m_min) { m_value = m_min; }
    if (m_value > m_max) { m_value = m_max; }
    Invalidate();
}

void PlayerProgressBar::SetValue(int v, bool notify)
{
    if (v < m_min) { v = m_min; }
    if (v > m_max) { v = m_max; }
    if (m_value != v)
    {
        m_value = v;
        Invalidate();
        if (notify)
        {
            NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_value);
        }
    }
}

int PlayerProgressBar::ValueFromX(int x) const
{
    int w = m_rcItem.right - m_rcItem.left;
    if (w <= 0)
    {
        return m_min;
    }
    int dx = x - m_rcItem.left;
    if (dx < 0) { dx = 0; }
    if (dx > w) { dx = w; }
    // 线性插值。注意 64 位中间体避免 (max-min)*dx 溢出 —— 默认范围 0..1000
    // 不会溢出，但若 caller 调到秒为单位（max=600s），dx*600 = 几十万还
    // 安全，但时长改大后保不齐。用 long long 一劳永逸。
    long long range = (long long)(m_max - m_min);
    long long add   = (range * dx + w / 2) / w;
    return m_min + (int)add;
}

bool PlayerProgressBar::OnMouseEnter()
{
    m_hover = true;
    Invalidate();
    return true;
}

bool PlayerProgressBar::OnMouseLeave()
{
    m_hover = false;
    if (!m_dragging)
    {
        Invalidate();
    }
    return true;
}

bool PlayerProgressBar::OnLButtonDown(POINT pt, UINT)
{
    m_dragging = true;
    SetValue(ValueFromX(pt.x), true);
    return true;
}

bool PlayerProgressBar::OnLButtonUp(POINT, UINT)
{
    m_dragging = false;
    return true;
}

bool PlayerProgressBar::OnMouseMove(POINT pt, UINT mkFlags)
{
    if (m_dragging && (mkFlags & MK_LBUTTON))
    {
        SetValue(ValueFromX(pt.x), true);
    }
    return true;
}

bool PlayerProgressBar::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    return true;
}

void PlayerProgressBar::OnPaint(HDC hdc, const RECT&)
{
    if (!m_bVisible)
    {
        return;
    }

    // track 高度：idle 2 / hover or drag 4。垂直居中。
    int trackH = (m_hover || m_dragging) ? kSizeProgressTrackHover
                                         : kSizeProgressTrackIdle;
    int cy = (m_rcItem.top + m_rcItem.bottom) / 2;
    int trackTop = cy - trackH / 2;
    int trackBot = trackTop + trackH;

    // remaining 整条（浅灰背景）
    RECT rcAll = { m_rcItem.left, trackTop, m_rcItem.right, trackBot };
    HBRUSH hbrAll = ::CreateSolidBrush(kColorSurfaceDim);
    ::FillRect(hdc, &rcAll, hbrAll);
    ::DeleteObject(hbrAll);

    // elapsed 红色覆盖层
    int w = m_rcItem.right - m_rcItem.left;
    long long range = (long long)(m_max - m_min);
    int elapsedW = 0;
    if (range > 0)
    {
        long long ex = (long long)(m_value - m_min) * w / range;
        elapsedW = (int)ex;
    }
    if (elapsedW > 0)
    {
        RECT rcEl = { m_rcItem.left, trackTop,
                      m_rcItem.left + elapsedW, trackBot };
        HBRUSH hbrEl = ::CreateSolidBrush(kColorPrimaryAccent);
        ::FillRect(hdc, &rcEl, hbrEl);
        ::DeleteObject(hbrEl);
    }
}

} // namespace cloudmelody
