#include "stdafx.h"
#include "DemoTextBadgeTile.h"
#include "DuiNotify.h"

using namespace balloonwjui;

// 简单的 setter 模式：值未变直接返回，避免无谓重绘。
// 任何视觉相关属性变化必须 Invalidate() 才会重新触发 OnPaint。

void DemoTextBadgeTile::SetText(LPCTSTR sz)
{
    CString s = sz ? sz : _T("");
    if (m_text == s)
    {
        return;
    }
    m_text = s;
    Invalidate();
}

void DemoTextBadgeTile::SetBgColor(COLORREF c)
{
    if (m_bgColor == c)
    {
        return;
    }
    m_bgColor = c;
    Invalidate();
}

void DemoTextBadgeTile::SetFgColor(COLORREF c)
{
    if (m_fgColor == c)
    {
        return;
    }
    m_fgColor = c;
    Invalidate();
}

void DemoTextBadgeTile::SetCornerRadius(int r)
{
    if (r < 0)
    {
        r = 0;
    }
    if (m_radius == r)
    {
        return;
    }
    m_radius = r;
    Invalidate();
}

// ----------------------------------------------------------------------------
// OnPaint — 真正的自绘逻辑
//
// hdc       : host 双缓冲的内存 DC，已经被 host 清成 COLOR_BTNFACE
// rcDirty   : 请求重绘的脏矩形（host 客户区坐标）
//
// 注意：m_rcItem 是父布局算出的本控件矩形（host 客户区坐标系）。所有绘制
// 都在 m_rcItem 内进行，不用自己处理坐标偏移。
// ----------------------------------------------------------------------------
void DemoTextBadgeTile::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    // ---- 1) 画圆角矩形背景 ----
    // 用 GDI 的 RoundRect。它会同时用当前 brush 填充内部、当前 pen 描边。
    // 我们不要描边，所以用 NULL_PEN（不画线）。
    HBRUSH brBg  = ::CreateSolidBrush(m_bgColor);
    HGDIOBJ oldBr = ::SelectObject(hdc, brBg);
    HGDIOBJ oldPn = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));

    // RoundRect 的最后两个参数是椭圆的"宽和高"，等于圆角半径的 2 倍。
    ::RoundRect(hdc,
                m_rcItem.left, m_rcItem.top,
                m_rcItem.right, m_rcItem.bottom,
                m_radius * 2, m_radius * 2);

    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPn);
    ::DeleteObject(brBg);

    // ---- 2) 画居中文字 ----
    if (m_text.IsEmpty())
    {
        return;
    }

    // 用库默认字体（Microsoft YaHei 9pt GB2312）。绝大多数自绘控件不该硬
    // 编码字体 —— 经 DuiResMgr 拿就能跟主题一起切换。
    HFONT hFont = DuiResMgr::Inst().GetDefaultFont();
    HGDIOBJ oldFont = ::SelectObject(hdc, hFont);

    ::SetBkMode(hdc, TRANSPARENT);   // 别盖掉背景
    ::SetTextColor(hdc, m_fgColor);

    RECT rc = m_rcItem;
    ::DrawText(hdc, m_text, -1, &rc,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    ::SelectObject(hdc, oldFont);
    // 注意：DuiResMgr 的字体是它管理的 — 不要 DeleteObject。
}

// ----------------------------------------------------------------------------
// OnLButtonUp — 把鼠标点击转成业务事件
//
// 父 HWND 在 OnDuiNotify 里收到：
//   n->ctrlId == this->GetCtrlId()
//   n->code   == DUIN_CLICK
//   n->extra  == 0
// ----------------------------------------------------------------------------
bool DemoTextBadgeTile::OnLButtonUp(POINT /*pt*/, UINT /*mkFlags*/)
{
    NotifyParent(DUIN_CLICK);
    return true;   // 消费事件
}
