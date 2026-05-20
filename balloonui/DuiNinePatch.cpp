#include "stdafx.h"
#include "DuiNinePatch.h"
#include <gdiplus.h>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

namespace {

// Lazy GDI+ init shared with DuiPaintAA. Re-declared here instead of
// reaching across module boundaries; GdiplusStartup is safe to call
// multiple times within one process as long as we keep our own token.
ULONG_PTR& GdipToken()
{
    static ULONG_PTR s_token = 0;
    return s_token;
}

void EnsureGdiplus()
{
    if (GdipToken() != 0)
    {
        return;
    }
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&GdipToken(), &input, nullptr);
}

} // namespace

namespace DuiNinePatch {

// Forward declaration — body in this file, below.
static bool DrawCells(HDC hdc, HBITMAP hbm, const BITMAP& bm,
                      const Cell cells[9], const Options& opts);

Insets ClampInsets(int srcW, int srcH, const Insets& ins)
{
    Insets r = ins;
    if (r.left < 0)
    {
        r.left = 0;
    }
    if (r.top < 0)
    {
        r.top = 0;
    }
    if (r.right < 0)
    {
        r.right = 0;
    }
    if (r.bottom < 0)
    {
        r.bottom = 0;
    }

    // If horizontal pair overflows source width, shrink proportionally.
    if (srcW > 0 && r.left + r.right > srcW)
    {
        double sum = (double)(r.left + r.right);
        r.left  = (int)((double)r.left  * srcW / sum);
        r.right = srcW - r.left;
    }
    // Same for vertical.
    if (srcH > 0 && r.top + r.bottom > srcH)
    {
        double sum = (double)(r.top + r.bottom);
        r.top    = (int)((double)r.top    * srcH / sum);
        r.bottom = srcH - r.top;
    }
    return r;
}

// 双 inset 版本：源/目标内距分别给。当 src==dst 时退化为单 inset 行为。
void ComputeCells(int srcW, int srcH,
                  const RECT& dstRect,
                  const Insets& srcInsIn,
                  const Insets& dstInsIn,
                  Cell out[9])
{
    for (int i = 0; i < 9; ++i)
    {
        out[i].src = RECT{ 0, 0, 0, 0 };
        out[i].dst = RECT{ 0, 0, 0, 0 };
    }

    if (srcW <= 0 || srcH <= 0)
    {
        return;
    }

    int dstW = dstRect.right  - dstRect.left;
    int dstH = dstRect.bottom - dstRect.top;
    if (dstW <= 0 || dstH <= 0)
    {
        return;
    }

    // 源内距按源尺寸 clamp（与单 inset 版本一致）
    Insets srcIns = ClampInsets(srcW, srcH, srcInsIn);

    // 目标内距：负值钳到 0；左+右 / 上+下 不能超过 dst 尺寸，溢出时按比例缩
    Insets dstIns = dstInsIn;
    if (dstIns.left   < 0) dstIns.left   = 0;
    if (dstIns.top    < 0) dstIns.top    = 0;
    if (dstIns.right  < 0) dstIns.right  = 0;
    if (dstIns.bottom < 0) dstIns.bottom = 0;
    if (dstIns.left + dstIns.right > dstW)
    {
        double sum = (double)(dstIns.left + dstIns.right);
        dstIns.left  = (int)((double)dstIns.left  * dstW / sum);
        dstIns.right = dstW - dstIns.left;
    }
    if (dstIns.top + dstIns.bottom > dstH)
    {
        double sum = (double)(dstIns.top + dstIns.bottom);
        dstIns.top    = (int)((double)dstIns.top    * dstH / sum);
        dstIns.bottom = dstH - dstIns.top;
    }

    // Source rails (用源 insets)
    int sX0 = 0;
    int sX1 = srcIns.left;
    int sX2 = srcW - srcIns.right;
    int sX3 = srcW;

    int sY0 = 0;
    int sY1 = srcIns.top;
    int sY2 = srcH - srcIns.bottom;
    int sY3 = srcH;

    // Destination rails (用目标 insets)
    int dX0 = dstRect.left;
    int dX1 = dstRect.left + dstIns.left;
    int dX2 = dstRect.right - dstIns.right;
    int dX3 = dstRect.right;

    int dY0 = dstRect.top;
    int dY1 = dstRect.top + dstIns.top;
    int dY2 = dstRect.bottom - dstIns.bottom;
    int dY3 = dstRect.bottom;

    auto setCell = [&](int idx,
                       int sxa, int sya, int sxb, int syb,
                       int dxa, int dya, int dxb, int dyb)
    {
        out[idx].src = RECT{ sxa, sya, sxb, syb };
        out[idx].dst = RECT{ dxa, dya, dxb, dyb };
    };

    // 0 TL  1 T   2 TR
    setCell(0, sX0, sY0, sX1, sY1, dX0, dY0, dX1, dY1);
    setCell(1, sX1, sY0, sX2, sY1, dX1, dY0, dX2, dY1);
    setCell(2, sX2, sY0, sX3, sY1, dX2, dY0, dX3, dY1);
    // 3 L   4 C   5 R
    setCell(3, sX0, sY1, sX1, sY2, dX0, dY1, dX1, dY2);
    setCell(4, sX1, sY1, sX2, sY2, dX1, dY1, dX2, dY2);
    setCell(5, sX2, sY1, sX3, sY2, dX2, dY1, dX3, dY2);
    // 6 BL  7 B   8 BR
    setCell(6, sX0, sY2, sX1, sY3, dX0, dY2, dX1, dY3);
    setCell(7, sX1, sY2, sX2, sY3, dX1, dY2, dX2, dY3);
    setCell(8, sX2, sY2, sX3, sY3, dX2, dY2, dX3, dY3);
}

void ComputeCells(int srcW, int srcH,
                  const RECT& dstRect,
                  const Insets& insIn,
                  Cell out[9])
{
    // Zero everything first so degenerate cells are always safe to skip
    // via IsRectEmpty.
    for (int i = 0; i < 9; ++i)
    {
        out[i].src = RECT{ 0, 0, 0, 0 };
        out[i].dst = RECT{ 0, 0, 0, 0 };
    }

    if (srcW <= 0 || srcH <= 0)
    {
        return;
    }

    int dstW = dstRect.right  - dstRect.left;
    int dstH = dstRect.bottom - dstRect.top;
    if (dstW <= 0 || dstH <= 0)
    {
        return;
    }

    Insets ins = ClampInsets(srcW, srcH, insIn);

    // If the destination is too small to fit corners side-by-side, also
    // shrink corner widths/heights proportionally so the painted shape
    // still looks reasonable (the alternative — clipping corners — looks
    // worse for round-rect borders).
    int cornerL = ins.left;
    int cornerR = ins.right;
    if (cornerL + cornerR > dstW)
    {
        double sum = (double)(cornerL + cornerR);
        cornerL = (int)((double)cornerL * dstW / sum);
        cornerR = dstW - cornerL;
    }
    int cornerT = ins.top;
    int cornerB = ins.bottom;
    if (cornerT + cornerB > dstH)
    {
        double sum = (double)(cornerT + cornerB);
        cornerT = (int)((double)cornerT * dstH / sum);
        cornerB = dstH - cornerT;
    }

    // Source rails (use full insets, NOT corner-shrunk values, so the
    // image's geometry stays correct — the corners themselves will scale
    // to fit the dst corners below).
    int sX0 = 0;
    int sX1 = ins.left;
    int sX2 = srcW - ins.right;
    int sX3 = srcW;

    int sY0 = 0;
    int sY1 = ins.top;
    int sY2 = srcH - ins.bottom;
    int sY3 = srcH;

    // Destination rails.
    int dX0 = dstRect.left;
    int dX1 = dstRect.left + cornerL;
    int dX2 = dstRect.right - cornerR;
    int dX3 = dstRect.right;

    int dY0 = dstRect.top;
    int dY1 = dstRect.top + cornerT;
    int dY2 = dstRect.bottom - cornerB;
    int dY3 = dstRect.bottom;

    auto setCell = [&](int idx,
                       int sxa, int sya, int sxb, int syb,
                       int dxa, int dya, int dxb, int dyb)
    {
        out[idx].src = RECT{ sxa, sya, sxb, syb };
        out[idx].dst = RECT{ dxa, dya, dxb, dyb };
    };

    // 0 TL  1 T   2 TR
    setCell(0, sX0, sY0, sX1, sY1, dX0, dY0, dX1, dY1);
    setCell(1, sX1, sY0, sX2, sY1, dX1, dY0, dX2, dY1);
    setCell(2, sX2, sY0, sX3, sY1, dX2, dY0, dX3, dY1);

    // 3 L   4 C   5 R
    setCell(3, sX0, sY1, sX1, sY2, dX0, dY1, dX1, dY2);
    setCell(4, sX1, sY1, sX2, sY2, dX1, dY1, dX2, dY2);
    setCell(5, sX2, sY1, sX3, sY2, dX2, dY1, dX3, dY2);

    // 6 BL  7 B   8 BR
    setCell(6, sX0, sY2, sX1, sY3, dX0, dY2, dX1, dY3);
    setCell(7, sX1, sY2, sX2, sY3, dX1, dY2, dX2, dY3);
    setCell(8, sX2, sY2, sX3, sY3, dX2, dY2, dX3, dY3);
}

bool Draw(HDC hdc, HBITMAP hbm,
          const RECT& dstRect,
          const Insets& srcInsets,
          const Insets& dstInsets,
          const Options& opts)
{
    if (!hdc || !hbm)
    {
        return false;
    }
    if (dstRect.right <= dstRect.left || dstRect.bottom <= dstRect.top)
    {
        return false;
    }

    BITMAP bm = {};
    if (!::GetObject(hbm, sizeof(bm), &bm))
    {
        return false;
    }
    if (bm.bmWidth <= 0 || bm.bmHeight <= 0)
    {
        return false;
    }

    Cell cells[9];
    ComputeCells(bm.bmWidth, bm.bmHeight, dstRect, srcInsets, dstInsets, cells);

    // 共用底层渲染（与单 inset 路径同样）— 内联以避免重复样板。
    return DrawCells(hdc, hbm, bm, cells, opts);
}

bool Draw(HDC hdc, HBITMAP hbm,
          const RECT& dstRect,
          const Insets& insets,
          const Options& opts)
{
    if (!hdc || !hbm)
    {
        return false;
    }
    if (dstRect.right <= dstRect.left || dstRect.bottom <= dstRect.top)
    {
        return false;
    }

    BITMAP bm = {};
    if (!::GetObject(hbm, sizeof(bm), &bm))
    {
        return false;
    }
    if (bm.bmWidth <= 0 || bm.bmHeight <= 0)
    {
        return false;
    }

    Cell cells[9];
    ComputeCells(bm.bmWidth, bm.bmHeight, dstRect, insets, cells);

    return DrawCells(hdc, hbm, bm, cells, opts);
}

// 把 9 块 (src, dst) 真正画到 hdc 上 — 共用底层。
static bool DrawCells(HDC hdc, HBITMAP hbm, const BITMAP& bm,
                      const Cell cells[9], const Options& opts)
{

    EnsureGdiplus();

    // Wrap the HBITMAP for GDI+ once and reuse for all 9 blits.
    //
    // For 32bpp DIBSections we always go through the explicit-bits path
    // (Bitmap ctor with stride + format + bits pointer). FromHBITMAP is
    // a known-unreliable choice here: it ignores the DIB's biHeight sign,
    // so a top-down DIB gets rendered upside-down. Building the Bitmap
    // directly with a signed stride lets GDI+ read rows in the order the
    // bytes are actually laid out.
    //
    // Format choice: PARGB if opts.hasAlpha (caller promises pre-multiplied
    // alpha), otherwise 32bppRGB so GDI+ does not blend stale alpha bytes.
    Gdiplus::Bitmap* pImg = nullptr;
    Gdiplus::Bitmap* pImgOwned = nullptr;

    DIBSECTION ds = {};
    bool hasDib = (::GetObject(hbm, sizeof(ds), &ds) == sizeof(ds));
    if (hasDib && ds.dsBm.bmBitsPixel == 32 && ds.dsBm.bmBits != nullptr)
    {
        // biHeight > 0 means bottom-up: bits start at the bottom row,
        // memory ascends toward the top. Point GDI+ at the bottom row
        // with a negative stride so it walks upward.
        int stride = ds.dsBm.bmWidthBytes;
        BYTE* bits = (BYTE*)ds.dsBm.bmBits;
        if (ds.dsBmih.biHeight > 0)
        {
            bits   = bits + (LONG_PTR)stride * (ds.dsBmih.biHeight - 1);
            stride = -stride;
        }
        Gdiplus::PixelFormat fmt = opts.hasAlpha
            ? PixelFormat32bppPARGB
            : PixelFormat32bppRGB;
        pImgOwned = new Gdiplus::Bitmap(bm.bmWidth, bm.bmHeight,
                                        stride, fmt, bits);
        pImg = pImgOwned;
    }
    if (!pImg)
    {
        // Non-DIB or non-32bpp source: fall back to FromHBITMAP. Top-down
        // skew is not a concern for DDBs since they have no biHeight to
        // misinterpret.
        pImgOwned = Gdiplus::Bitmap::FromHBITMAP(hbm, nullptr);
        pImg = pImgOwned;
    }
    if (!pImg)
    {
        return false;
    }

    Gdiplus::Graphics g(hdc);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    for (int i = 0; i < 9; ++i)
    {
        const Cell& c = cells[i];
        if (::IsRectEmpty(&c.src) || ::IsRectEmpty(&c.dst))
        {
            continue;
        }

        int dW = c.dst.right - c.dst.left;
        int dH = c.dst.bottom - c.dst.top;
        int sW = c.src.right - c.src.left;
        int sH = c.src.bottom - c.src.top;

        // Corners (TL/TR/BL/BR = even indices 0,2,6,8) blit 1:1 when
        // dst size matches src — use NearestNeighbor to keep them crisp.
        // For all stretched cells use HighQualityBilinear so edges and
        // center don't go blocky.
        bool isCorner = (i == 0 || i == 2 || i == 6 || i == 8);
        if (isCorner && dW == sW && dH == sH)
        {
            g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        }
        else
        {
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
        }

        Gdiplus::Rect dstR(c.dst.left, c.dst.top, dW, dH);
        g.DrawImage(pImg, dstR,
                    c.src.left, c.src.top, sW, sH,
                    Gdiplus::UnitPixel);
    }

    // Force GDI+ to commit pending operations to the underlying DC.
    // Without this, paints into a memory DC backed by a DIBSection may
    // remain inside GDI+'s internal buffer until the Graphics dtor runs,
    // which makes pixel-level off-screen verification flaky.
    g.Flush(Gdiplus::FlushIntentionSync);

    delete pImgOwned;
    return true;
}

} // namespace DuiNinePatch

} // namespace balloonwjui
