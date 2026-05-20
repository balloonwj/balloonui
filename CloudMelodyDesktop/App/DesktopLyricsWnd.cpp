#include "stdafx.h"
#include "DesktopLyricsWnd.h"
#include "CloudMelodyFonts.h"
#include "CloudMelodyTheme.h"

#include <gdiplus.h>

namespace cloudmelody {

// 浮窗几何（屏幕底部居中、距任务栏上方 kBottomGap）
static const int kLyricsWndW    = 800;
static const int kLyricsWndH    = 90;
static const int kBottomGap     = 60;

// 卡拉 OK 静态进度（无真实 LRC timing）：左 60% 唱过部分用绿色，
// 右 40% 用白色。文字描边黑色 1px 保证可读。
static const float kKaraokePct  = 0.60f;
static const Gdiplus::Color kColorSung   (255, 80, 200, 110);   // 已唱：绿
static const Gdiplus::Color kColorUnsung (255, 255, 255, 255);  // 未唱：白
static const Gdiplus::Color kColorStroke (255, 0,   0,   0);    // 描边：黑

void DesktopLyricsWnd::EnsureCreated()
{
    if (m_hWnd && ::IsWindow(m_hWnd))
    {
        return;
    }

    Create(NULL, CWindow::rcDefault, _T(""),
           WS_POPUP,
           WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE);
    if (!m_hWnd)
    {
        return;
    }

    int sw = ::GetSystemMetrics(SM_CXSCREEN);
    int sh = ::GetSystemMetrics(SM_CYSCREEN);
    int x  = (sw - kLyricsWndW) / 2;
    int y  = sh - kLyricsWndH - kBottomGap;
    SetWindowPos(NULL, x, y, kLyricsWndW, kLyricsWndH,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    // <u>不</u>调 SetLayeredWindowAttributes —— 我们要走 UpdateLayeredWindow
    // 逐像素 alpha 模式（背景完全透、文字完全不透），LWA_ALPHA 是整窗
    // 一致 alpha，无法做到。
}

void DesktopLyricsWnd::Toggle()
{
    EnsureCreated();
    if (!m_hWnd) { return; }
    if (::IsWindowVisible(m_hWnd))
    {
        ::ShowWindow(m_hWnd, SW_HIDE);
    }
    else
    {
        ::ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
        Repaint_();
    }
}

void DesktopLyricsWnd::SetCurrentLyric(LPCTSTR text)
{
    CString t = text ? text : _T("");
    if (m_text == t) { return; }
    m_text = t;
    if (m_hWnd && ::IsWindowVisible(m_hWnd))
    {
        Repaint_();
    }
}

LRESULT DesktopLyricsWnd::OnEraseBkgnd_(UINT, WPARAM, LPARAM, BOOL&)
{
    return 1;   // layered window 不走 erase + paint 路径，直接吞
}

LRESULT DesktopLyricsWnd::OnNcHitTest_(UINT, WPARAM, LPARAM, BOOL&)
{
    return HTCAPTION;
}

LRESULT DesktopLyricsWnd::OnPaint_(UINT, WPARAM, LPARAM, BOOL&)
{
    // layered window 不走 BeginPaint/EndPaint —— 仅作"validate update
    // region"（避免 OS 反复发 WM_PAINT）。真画图在 Repaint_ 里通过
    // UpdateLayeredWindow 推一次。
    PAINTSTRUCT ps;
    ::BeginPaint(m_hWnd, &ps);
    ::EndPaint(m_hWnd, &ps);
    Repaint_();
    return 0;
}

void DesktopLyricsWnd::Repaint_()
{
    if (!m_hWnd) { return; }

    int w = kLyricsWndW;
    int h = kLyricsWndH;

    // 创建 32bpp top-down DIB，用作 UpdateLayeredWindow 的源 surface。
    // 每像素 BGRA pre-multiplied alpha；alpha=0 表全透、255 表不透。
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       =  w;
    bmi.bmiHeader.biHeight      = -h;          // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = ::GetDC(nullptr);
    HDC hdcMem    = ::CreateCompatibleDC(hdcScreen);
    void* pBits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS,
                                     &pBits, nullptr, 0);
    HBITMAP hbmOld = (HBITMAP)::SelectObject(hdcMem, hbm);

    // 全透清零（alpha=0）—— 整张 bitmap 默认就是 0，CreateDIBSection
    // 已 zero-init，但显式 memset 防御。
    ::ZeroMemory(pBits, (size_t)w * h * 4);

    // GDI+ 绘到 hdcMem。它支持 alpha pixel，符合 ULW 要求。
    {
        Gdiplus::Graphics g(hdcMem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

        if (!m_text.IsEmpty())
        {
            // 字号取本期 DisplayLg（30px Bold）—— 画桌面歌词足够大
            Gdiplus::FontFamily ff(L"Microsoft YaHei");
            Gdiplus::Font font(&ff, 30.0f, Gdiplus::FontStyleBold,
                               Gdiplus::UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);

            const wchar_t* str = (LPCWSTR)m_text;
            INT len = (INT)m_text.GetLength();
            Gdiplus::RectF rcF(0, 0, (Gdiplus::REAL)w, (Gdiplus::REAL)h);

            // 1) 黑色描边：8 邻接位置画一遍黑底（模拟 1px stroke）
            Gdiplus::SolidBrush brStroke(kColorStroke);
            for (int dx = -1; dx <= 1; ++dx)
            {
                for (int dy = -1; dy <= 1; ++dy)
                {
                    if (dx == 0 && dy == 0) { continue; }
                    Gdiplus::RectF off(rcF.X + dx, rcF.Y + dy,
                                       rcF.Width, rcF.Height);
                    g.DrawString(str, len, &font, off, &fmt, &brStroke);
                }
            }

            // 2) 主体白字（先全画白）
            Gdiplus::SolidBrush brWhite(kColorUnsung);
            g.DrawString(str, len, &font, rcF, &fmt, &brWhite);

            // 3) 测量主体宽度，clip 到左 60%，画绿色覆盖
            Gdiplus::RectF measured;
            g.MeasureString(str, len, &font, rcF, &fmt, &measured);
            Gdiplus::RectF clip(measured.X, 0,
                                measured.Width * kKaraokePct,
                                (Gdiplus::REAL)h);
            g.SetClip(clip);
            Gdiplus::SolidBrush brGreen(kColorSung);
            g.DrawString(str, len, &font, rcF, &fmt, &brGreen);
            g.ResetClip();
        }
    }

    // GDI 文字 / 普通 GDI+ 路径写入的像素 alpha 通常是 0 —— UpdateLayeredWindow
    // 期望 pre-multiplied alpha，需要把 alpha 通道补成 255 给非透明像素。
    // 简易策略：扫一遍 DIB，凡是 RGB 非 (0,0,0,0) 的像素，alpha=255 +
    // 重新 pre-multiply（RGB 通道乘 alpha/255，alpha=255 时无变化）。
    BYTE* p = (BYTE*)pBits;
    for (int i = 0; i < w * h; ++i, p += 4)
    {
        // GDI+ 写入时若已设了 alpha，会在 BGRA 里写非零的 A 通道。
        // 普通 DrawString 的字符像素 A=255（pre-multiplied 已就绪）。
        // 字符之外的像素 A=0（DIB zero-init 没改）。
        // 这里做一个保守的"非黑非透 →设 alpha=255"：实际多数情况下
        // GDI+ 已正确处理，本扫描相当于兜底。
        if (p[3] == 0 && (p[0] != 0 || p[1] != 0 || p[2] != 0))
        {
            p[3] = 255;
        }
    }

    POINT ptDst;
    ::GetWindowRect(m_hWnd, nullptr);
    RECT wr;
    ::GetWindowRect(m_hWnd, &wr);
    ptDst.x = wr.left;
    ptDst.y = wr.top;
    POINT ptSrc = { 0, 0 };
    SIZE sz = { w, h };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    ::UpdateLayeredWindow(m_hWnd, hdcScreen, &ptDst, &sz,
                          hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);

    ::SelectObject(hdcMem, hbmOld);
    ::DeleteObject(hbm);
    ::DeleteDC(hdcMem);
    ::ReleaseDC(nullptr, hdcScreen);
}

} // namespace cloudmelody
