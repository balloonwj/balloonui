#include "stdafx.h"
#include "DuiBadge.h"

#if BUI_FEATURE_BADGE

#include "../../DuiPaintAA.h"
#include "../../DuiResMgr.h"

namespace balloonwjui {

DuiBadge::DuiBadge()
{
    SetTabStop(false);
}

void DuiBadge::SetText(LPCTSTR text)
{
    // 保存原始文本, 不在此处截断 —— 截断在 OnPaint 里走 ApplyMaxChars,
    // 由当前 m_maxChars 决定。这样:
    //   1. GetText() 返回的是 caller 设的原始值, 语义更直观。
    //   2. SetMaxDisplayChars 后改变截断行为, 无需 caller 重设 text。
    //   3. SetMaxDisplayChars(0) 后能完整呈现长文字标签。
    // 兼容性: m_maxChars 默认 4, OnPaint 仍截到 4 字 → 显示行为不变。
    CString t = text ? text : _T("");
    if (m_text == t)
    {
        return;
    }
    m_text = t;
    Invalidate();
}

CString DuiBadge::FormatCount(int n)
{
    if (n <= 0)
    {
        return CString();
    }
    if (n > 99)
    {
        return CString(_T("99+"));
    }
    CString s;
    s.Format(_T("%d"), n);
    return s;
}

void DuiBadge::SetCount(int n)
{
    SetText(FormatCount(n));
}

void DuiBadge::SetBgColor(COLORREF c)
{
    if (m_bg == c)
    {
        return;
    }
    m_bg = c;
    Invalidate();
}

void DuiBadge::SetTextColor(COLORREF c)
{
    if (m_fg == c)
    {
        return;
    }
    m_fg = c;
    Invalidate();
}

void DuiBadge::SetHideWhenEmpty(bool b)
{
    if (m_hideEmpty == b)
    {
        return;
    }
    m_hideEmpty = b;
    Invalidate();
}

// ---- 前导圆点 setters ----

void DuiBadge::SetLeadingDot(COLORREF c)
{
    if (m_dotColor == c)
    {
        return;
    }
    m_dotColor = c;
    Invalidate();
}

void DuiBadge::SetLeadingDotRadius(int r)
{
    if (m_dotRadius == r)
    {
        return;
    }
    m_dotRadius = r;
    Invalidate();
}

void DuiBadge::SetLeadingGap(int gap)
{
    if (m_leadingGap == gap)
    {
        return;
    }
    m_leadingGap = gap;
    Invalidate();
}

// ---- 形状参数化 setter ----

void DuiBadge::SetCornerRadius(int r)
{
    if (m_cornerRadius == r)
    {
        return;
    }
    m_cornerRadius = r;
    Invalidate();
}

void DuiBadge::SetMaxDisplayChars(int n)
{
    if (m_maxChars == n)
    {
        return;
    }
    m_maxChars = n;
    Invalidate();
}

// ---- 静态纯函数（与 OnPaint 内宽度算法对应；可单测） ----

int DuiBadge::ContentWidth(int textWidth, int dotRadius,
                           int leadingGap, bool hasDot)
{
    // 防御性：负值都按 0 处理。
    if (textWidth  < 0) textWidth  = 0;
    if (dotRadius  < 0) dotRadius  = 0;
    if (leadingGap < 0) leadingGap = 0;
    if (!hasDot)
    {
        return textWidth;
    }
    return 2 * dotRadius + leadingGap + textWidth;
}

int DuiBadge::AutoDotRadius(int fontHeight)
{
    // fallback：字体高度未知 / 异常时给最小可视半径 3px。
    if (fontHeight <= 0)
    {
        return 3;
    }
    int r = fontHeight / 4;
    return (r < 3) ? 3 : r;
}

int DuiBadge::EffectiveCornerRadius(int rawRadius, int height)
{
    if (rawRadius == -1)
    {
        // 胶囊形:圆角 = 高/2;height<=0 时退化为 0,避免负值。
        if (height <= 0)
        {
            return 0;
        }
        return height / 2;
    }
    if (rawRadius < 0)
    {
        // 误用:< -1 视作 0(直角),不报错。
        return 0;
    }
    return rawRadius;
}

CString DuiBadge::ApplyMaxChars(LPCTSTR text, int maxChars)
{
    CString t = text ? text : _T("");
    // maxChars <= 0 表示不截;否则按字符数砍后缀,不加省略号。
    if (maxChars <= 0)
    {
        return t;
    }
    if (t.GetLength() > maxChars)
    {
        return t.Left(maxChars);
    }
    return t;
}

bool DuiBadge::IsShowing() const
{
    if (!m_bVisible)
    {
        return false;
    }
    if (m_text.IsEmpty() && m_hideEmpty)
    {
        return false;
    }
    return true;
}

void DuiBadge::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!IsShowing())
    {
        return;
    }
    int w = m_rcItem.right  - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // 截断:m_text 是原始文本, 显示前按 m_maxChars 截。
    // 默认 m_maxChars=4 → 行为与历史一致;chip 用法 SetMaxDisplayChars(0)。
    CString displayText = ApplyMaxChars(m_text, m_maxChars);

    // Pill rect: vertically centered, height = min(h, 20).
    // Width = max(height, contentWidth + 12), contentWidth 由 ContentWidth()
    // 算(含可选前导圆点 + gap)。
    int pillH = h < 20 ? h : 20;
    int textW = 0;
    int fontH = 0;

    HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
    HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
    if (!displayText.IsEmpty())
    {
        SIZE sz = {};
        ::GetTextExtentPoint32(hdc, displayText, displayText.GetLength(), &sz);
        textW = sz.cx;
        fontH = sz.cy;
    }
    else if (HasLeadingDot())
    {
        // 空文本 + 有圆点(纯指示点):仍需字体高度去自适配圆点。
        TEXTMETRIC tm = {};
        ::GetTextMetrics(hdc, &tm);
        fontH = tm.tmHeight;
    }

    // 解析圆点半径:显式 m_dotRadius > 0 优先,否则按字体高度自适配。
    int dotR = 0;
    if (HasLeadingDot())
    {
        dotR = (m_dotRadius > 0) ? m_dotRadius : AutoDotRadius(fontH);
    }

    int contentW = ContentWidth(textW, dotR, m_leadingGap, HasLeadingDot());
    int pillW = contentW + 10;
    if (pillW < pillH)
    {
        pillW = pillH;          // 默认胶囊形最小是个圆
    }
    if (pillW > w)
    {
        pillW = w;
    }
    if (pillH > h)
    {
        pillH = h;
    }

    int cx = (m_rcItem.left + m_rcItem.right) / 2;
    int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;
    RECT pill = { cx - pillW / 2, cy - pillH / 2,
                  cx + pillW / 2, cy + pillH / 2 };

    // ----- 背景:统一走 DuiAA::FillRoundRect -----
    // EffectiveCornerRadius: rawRadius=-1 → 高/2(胶囊/圆);rawRadius>=0 →
    // 固定半径(chip 形态)。DuiAA::FillRoundRect 内部会再夹到 min(w,h)/2,
    // 所以胶囊形 + 极窄 / 极宽都能自动退化为圆 / 胶囊。
    int radius = EffectiveCornerRadius(m_cornerRadius, pillH);
    DuiAA::FillRoundRect(hdc, pill, m_bg, radius);

    // ----- 前导圆点 + 文字 -----
    if (HasLeadingDot())
    {
        // pill 内部左右 padding 各 5px(与 pillW = contentW + 10 一致),
        // 整组(dot + gap + text)在 pill 内水平居中。
        int groupLeft  = pill.left + 5;
        int groupRight = pill.right - 5;
        // 兜底:极窄场景下 groupLeft 可能 > groupRight,跳过绘制。
        if (groupRight > groupLeft)
        {
            int dotCX = groupLeft + dotR;
            int dotCY = (pill.top + pill.bottom) / 2;
            RECT dotRc = { dotCX - dotR, dotCY - dotR,
                           dotCX + dotR, dotCY + dotR };
            DuiAA::FillEllipse(hdc, dotRc, m_dotColor, CLR_INVALID);

            if (!displayText.IsEmpty())
            {
                int textLeft = groupLeft + 2 * dotR + m_leadingGap;
                if (textLeft < groupRight)
                {
                    RECT textRc = { textLeft, pill.top,
                                    groupRight, pill.bottom };
                    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
                    COLORREF oldClr = ::SetTextColor(hdc, m_fg);
                    ::DrawText(hdc, displayText, -1, &textRc,
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE
                               | DT_NOPREFIX);
                    ::SetTextColor(hdc, oldClr);
                    ::SetBkMode(hdc, oldBk);
                }
            }
        }
    }
    else if (!displayText.IsEmpty())
    {
        // 既有行为:无圆点时文字 DT_CENTER 居中。
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, m_fg);
        ::DrawText(hdc, displayText, -1, &pill,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
    }

    if (oldFont)
    {
        ::SelectObject(hdc, oldFont);
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_BADGE
