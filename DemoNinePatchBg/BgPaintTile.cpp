#include "stdafx.h"
#include "BgPaintTile.h"
#include "DuiNinePatch.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

void BgPaintTile::SetBitmap(HBITMAP hbm)
{
    if (m_hbm == hbm) return;
    m_hbm = hbm;
    Invalidate();
}

void BgPaintTile::SetInsets(int l, int t, int r, int b)
{
    m_srcL = m_dstL = l;
    m_srcT = m_dstT = t;
    m_srcR = m_dstR = r;
    m_srcB = m_dstB = b;
    Invalidate();
}

void BgPaintTile::SetInsets(int srcL, int srcT, int srcR, int srcB,
                            int dstL, int dstT, int dstR, int dstB)
{
    m_srcL = srcL; m_srcT = srcT; m_srcR = srcR; m_srcB = srcB;
    m_dstL = dstL; m_dstT = dstT; m_dstR = dstR; m_dstB = dstB;
    Invalidate();
}

void BgPaintTile::SetMode(Mode m)
{
    if (m_mode == m) return;
    m_mode = m;
    Invalidate();
}

void BgPaintTile::OnPaint(HDC hdc, const RECT&)
{
    if (!m_hbm)
    {
        return;
    }

    if (m_mode == ModeNinePatch)
    {
        // 走真正的 9-grid — 与 DuiHost::SetBgImage 内部用同一个 helper。
        // 默认双 inset 路径；当 src == dst 时退化为经典 9-grid。
        DuiNinePatch::Insets srcIns;
        srcIns.left   = m_srcL;
        srcIns.top    = m_srcT;
        srcIns.right  = m_srcR;
        srcIns.bottom = m_srcB;
        DuiNinePatch::Insets dstIns;
        dstIns.left   = m_dstL;
        dstIns.top    = m_dstT;
        dstIns.right  = m_dstR;
        dstIns.bottom = m_dstB;
        DuiNinePatch::Draw(hdc, m_hbm, m_rcItem, srcIns, dstIns);
    }
    else
    {
        // 反例 — 整图等比缩放到目标矩形（StretchBlt）。
        // 角落装饰 / 顶部渐变都会被按"目标 / 源"比例压扁拉伸，
        // 与 9-grid 形成对比。
        BITMAP bm = {};
        ::GetObject(m_hbm, sizeof(bm), &bm);
        HDC memDC = ::CreateCompatibleDC(hdc);
        HGDIOBJ old = ::SelectObject(memDC, m_hbm);
        ::SetStretchBltMode(hdc, COLORONCOLOR);
        ::StretchBlt(hdc,
                     m_rcItem.left, m_rcItem.top,
                     m_rcItem.right  - m_rcItem.left,
                     m_rcItem.bottom - m_rcItem.top,
                     memDC, 0, 0, bm.bmWidth, bm.bmHeight,
                     SRCCOPY);
        ::SelectObject(memDC, old);
        ::DeleteDC(memDC);
    }
}
