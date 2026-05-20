#include "stdafx.h"
#include "DemoCircularProgress.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

// ----- setter 群（标准模式） ---------------------------------------------------

void DemoCircularProgress::SetPercent(int p)
{
    if (p < 0)   p = 0;
    if (p > 100) p = 100;
    if (m_percent == p)
    {
        return;
    }
    m_percent = p;
    Invalidate();
}

void DemoCircularProgress::SetThickness(int t)
{
    if (t < 0) t = 0;
    if (m_thickness == t)
    {
        return;
    }
    m_thickness = t;
    Invalidate();
}

void DemoCircularProgress::SetTrackColor(COLORREF c)
{
    if (m_trackColor == c) return;
    m_trackColor = c;
    Invalidate();
}

void DemoCircularProgress::SetFillColor(COLORREF c)
{
    if (m_fillColor == c) return;
    m_fillColor = c;
    Invalidate();
}

void DemoCircularProgress::SetTextColor(COLORREF c)
{
    if (m_textColor == c) return;
    m_textColor = c;
    Invalidate();
}

void DemoCircularProgress::SetShowText(bool b)
{
    if (m_showText == b) return;
    m_showText = b;
    Invalidate();
}

// ----- OnPaint：GDI+ 抗锯齿弧 + 中心 GDI 文字 -----------------------------

void DemoCircularProgress::OnPaint(HDC hdc, const RECT&)
{
    // 取最小边作为直径，保持环为正圆 + 居中。
    int w = m_rcItem.right  - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    int diameter = (w < h ? w : h) - 2;     // 留 1px 安全边
    if (diameter <= 0)
    {
        return;
    }
    int cx = (m_rcItem.left + m_rcItem.right)  / 2;
    int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;

    // 环外接矩形（GDI+ Pen 沿矩形中线画 → 把矩形内缩 thickness/2）。
    float halfPen = m_thickness / 2.0f;
    float left    = cx - diameter / 2.0f + halfPen;
    float top     = cy - diameter / 2.0f + halfPen;
    float side    = diameter - m_thickness;
    if (side <= 0)
    {
        return;
    }

    // ---- 画环 ----
    // 直接用 GDI+：它带 SmoothingMode::AntiAlias，画弧无锯齿。
    // 绑定到 hdc 上 — Graphics 仅是壳，hdc 仍是 caller 拥有。
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // 1) 灰色轨道：完整圆环
    Gdiplus::Pen trackPen(
        Gdiplus::Color(255,
                       GetRValue(m_trackColor),
                       GetGValue(m_trackColor),
                       GetBValue(m_trackColor)),
        (Gdiplus::REAL)m_thickness);
    g.DrawEllipse(&trackPen, left, top, side, side);

    // 2) 进度填充：从 12 点钟方向（-90°）顺时针画弧，sweep = 3.6° × percent
    if (m_percent > 0)
    {
        Gdiplus::Pen fillPen(
            Gdiplus::Color(255,
                           GetRValue(m_fillColor),
                           GetGValue(m_fillColor),
                           GetBValue(m_fillColor)),
            (Gdiplus::REAL)m_thickness);
        // GDI+ Pen 默认平头 — 进度起止处看着秃。换圆头更现代。
        fillPen.SetStartCap(Gdiplus::LineCapRound);
        fillPen.SetEndCap  (Gdiplus::LineCapRound);

        Gdiplus::REAL startDeg = -90.0f;
        Gdiplus::REAL sweepDeg = 3.6f * (Gdiplus::REAL)m_percent;
        g.DrawArc(&fillPen, left, top, side, side, startDeg, sweepDeg);
    }

    // ---- 中心文字 "<n>%" ----
    if (m_showText)
    {
        HFONT f = DuiResMgr::Inst().GetDefaultFont();
        HGDIOBJ oldF = ::SelectObject(hdc, f);
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, m_textColor);
        CString s; s.Format(_T("%d%%"), m_percent);
        RECT rc = m_rcItem;
        ::DrawText(hdc, s, -1, &rc,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ::SelectObject(hdc, oldF);
    }
}
