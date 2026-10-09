#include "stdafx.h"
#include "DuiAvatar.h"

#if BUI_FEATURE_AVATAR

#include "../../DuiResMgr.h"
#include "../../DuiTheme.h"   // GetDefaultFontFace：缩写字母的字体名
#include <gdiplus.h>
#include <vector>

namespace balloonwjui {

#pragma comment(lib, "gdiplus.lib")

namespace {

// Presence-status dot colors. The four states map to the standard
// IM-client palette (online green / away amber / busy red / offline gray).
// Tweak here and every avatar everywhere updates - status dots also feed
// the contact list and chat-header avatars.
const COLORREF kStatusOnline  = RGB( 60, 175,  80);   // saturated green
const COLORREF kStatusAway    = RGB(240, 175,  40);   // amber / sunflower
const COLORREF kStatusBusy    = RGB(220,  60,  60);   // red, "do not disturb"
const COLORREF kStatusOffline = RGB(150, 150, 150);   // medium gray

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

inline Gdiplus::Color ToGdiColor(COLORREF c)
{
    return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

// 由一张 32 位 DIBSection 构造 Gdiplus::Bitmap，并保留透明度。像素用 GetDIBits() 按自上而下
// 的行序复制，源图实际的行序由 GDI 自己处理。行序不能从 GetObject() 推测：它对自上而下存储的
// DIBSection 同样返回正的 dsBmih.biHeight，原先按它判断，把所有自上而下存储的位图都画颠倒了。
// 返回的位图自己持有像素，由调用方 delete。设备相关位图与其它位深退回 FromHBITMAP。
Gdiplus::Bitmap* MakeGdipBitmapFromHbm(HBITMAP hbm, bool hasAlpha)
{
    if (!hbm)
    {
        return nullptr;
    }
    DIBSECTION ds = {};
    if (::GetObject(hbm, sizeof(ds), &ds) != sizeof(ds))
    {
        return Gdiplus::Bitmap::FromHBITMAP(hbm, nullptr);
    }
    if (ds.dsBm.bmBitsPixel != 32 || ds.dsBm.bmBits == nullptr)
    {
        return Gdiplus::Bitmap::FromHBITMAP(hbm, nullptr);
    }

    const int w = ds.dsBm.bmWidth;
    const int h = ds.dsBm.bmHeight;
    if (w <= 0 || h <= 0)
    {
        return nullptr;
    }

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;        // 负高：要求按自上而下的行序读出
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    Gdiplus::PixelFormat fmt = hasAlpha ? PixelFormat32bppPARGB
                                        : PixelFormat32bppRGB;
    Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(w, h, fmt);
    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData data = {};
    if (bmp->GetLastStatus() != Gdiplus::Ok
        || bmp->LockBits(&rc, Gdiplus::ImageLockModeWrite, fmt, &data) != Gdiplus::Ok)
    {
        delete bmp;
        return Gdiplus::Bitmap::FromHBITMAP(hbm, nullptr);
    }

    // GetDIBits 写出的 32 位像素行是紧密排列的（天然按 DWORD 对齐）。锁定区的行跨度与之相同时
    // 直接写进锁定区，否则先写进临时缓冲区，再逐行复制过去。
    const int rowBytes = w * 4;
    int lines = 0;
    HDC hdc = ::GetDC(nullptr);
    if (data.Stride == rowBytes)
    {
        lines = ::GetDIBits(hdc, hbm, 0, (UINT)h, data.Scan0, &bi, DIB_RGB_COLORS);
    }
    else
    {
        std::vector<BYTE> buf((size_t)rowBytes * (size_t)h);
        lines = ::GetDIBits(hdc, hbm, 0, (UINT)h, buf.data(), &bi, DIB_RGB_COLORS);
        for (int y = 0; y < h; ++y)
        {
            memcpy((BYTE*)data.Scan0 + (LONG_PTR)data.Stride * y,
                   buf.data() + (size_t)rowBytes * (size_t)y, (size_t)rowBytes);
        }
    }
    ::ReleaseDC(nullptr, hdc);
    bmp->UnlockBits(&data);

    if (lines != h)
    {
        delete bmp;
        return Gdiplus::Bitmap::FromHBITMAP(hbm, nullptr);
    }
    return bmp;
}

} // namespace

DuiAvatar::DuiAvatar()
{
    SetTabStop(false);
}

void DuiAvatar::SetBitmap(HBITMAP hbm)
{
    if (m_hBitmap == hbm)
    {
        return;
    }
    m_hBitmap = hbm;
    Invalidate();
}

void DuiAvatar::SetName(LPCTSTR name)
{
    CString n = name ? name : _T("");
    if (m_name == n)
    {
        return;
    }
    m_name = n;
    Invalidate();
}

void DuiAvatar::SetFallbackBgColor(COLORREF c)
{
    if (m_fallbackBg == c)
    {
        return;
    }
    m_fallbackBg = c;
    Invalidate();
}

void DuiAvatar::SetInitialsColor(COLORREF c)
{
    if (m_initialsClr == c)
    {
        return;
    }
    m_initialsClr = c;
    Invalidate();
}

void DuiAvatar::SetShape(Shape s)
{
    if (m_shape == s)
    {
        return;
    }
    m_shape = s;
    Invalidate();
}

void DuiAvatar::SetCornerRadius(int r)
{
    if (r < 0)
    {
        r = 0;
    }
    if (m_cornerR == r)
    {
        return;
    }
    m_cornerR = r;
    Invalidate();
}

void DuiAvatar::SetStatus(Status st)
{
    if (m_status == st)
    {
        return;
    }
    m_status = st;
    Invalidate();
}

COLORREF DuiAvatar::GetStatusDotColor(Status st)
{
    switch (st)
    {
    case StatusOnline:
        return kStatusOnline;
    case StatusAway:
        return kStatusAway;
    case StatusBusy:
        return kStatusBusy;
    case StatusOffline:
        return kStatusOffline;
    case StatusNone:
    default:
        return CLR_INVALID;
    }
}

CString DuiAvatar::ComputeInitials(LPCTSTR name)
{
    if (!name || !*name)
    {
        return CString();
    }

    // Collect first chars of every whitespace-separated word.
    std::vector<TCHAR> firstChars;
    bool inWord = false;
    TCHAR firstWordChar = 0;
    TCHAR lastWordChar  = 0;
    int wordCount = 0;
    for (LPCTSTR p = name; *p; ++p)
    {
        if (_istspace((TCHAR)*p))
        {
            inWord = false;
        }
        else if (!inWord)
        {
            inWord = true;
            ++wordCount;
            TCHAR c = (TCHAR)_totupper((TCHAR)*p);
            if (firstWordChar == 0)
            {
                firstWordChar = c;
            }
            lastWordChar = c;
        }
    }

    if (wordCount == 0)
    {
        return CString();
    }
    if (wordCount == 1)
    {
        // Single word: take just the first char (callers wanting "Al"-style
        // 2-letter initials can pre-format the name with a space).
        CString s;
        s.Append(&firstWordChar, 1);
        return s;
    }
    // 2+ words: first char of first word + first char of last word.
    CString s;
    s.Append(&firstWordChar, 1);
    s.Append(&lastWordChar,  1);
    return s;
}

void DuiAvatar::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }

    int w = m_rcItem.right  - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    EnsureGdiplus();
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // Avatar rect inset by 0 — we let the status dot overshoot below.
    const Gdiplus::REAL X = (Gdiplus::REAL)m_rcItem.left;
    const Gdiplus::REAL Y = (Gdiplus::REAL)m_rcItem.top;
    const Gdiplus::REAL W = (Gdiplus::REAL)w;
    const Gdiplus::REAL H = (Gdiplus::REAL)h;

    // Build a clip path for the avatar shape.
    Gdiplus::GraphicsPath path;
    if (m_shape == ShapeCircle)
    {
        path.AddEllipse(X, Y, W - 1, H - 1);
    }
    else
    {
        // Rounded rect = 4 arcs + 4 sides. Clamp radius to half the
        // shorter side.
        Gdiplus::REAL r = (Gdiplus::REAL)m_cornerR;
        Gdiplus::REAL maxR = (W < H ? W : H) * 0.5f;
        if (r > maxR)
        {
            r = maxR;
        }
        if (r < 0)
        {
            r = 0;
        }
        Gdiplus::REAL d = r * 2;
        path.AddArc(X,             Y,             d, d, 180, 90);
        path.AddArc(X + W - 1 - d, Y,             d, d, 270, 90);
        path.AddArc(X + W - 1 - d, Y + H - 1 - d, d, d,   0, 90);
        path.AddArc(X,             Y + H - 1 - d, d, d,  90, 90);
        path.CloseFigure();
    }

    // Fill path: bitmap (TextureBrush stretched to fit) or solid color.
    Gdiplus::Bitmap* pImg = nullptr;
    if (m_hBitmap)
    {
        pImg = MakeGdipBitmapFromHbm(m_hBitmap, /*hasAlpha=*/true);
    }

    if (pImg)
    {
        // 先把位图缩放到头像尺寸、画进一张中间位图，再拿它作纹理画刷按抗锯齿填充头像形状。
        // 不用「SetClip 到形状再 DrawImage」：GDI+ 的裁剪区按整像素划分，抗锯齿设置对它不起作用，
        // 圆形与圆角的边缘是台阶状的锯齿（2026-10-04 实测）。中间位图带透明通道，源图透明的部分
        // （如系统图标的四角）填充后仍透出底下的内容。
        const INT iw = (INT)W;
        const INT ih = (INT)H;
        Gdiplus::Bitmap scaled(iw, ih, PixelFormat32bppPARGB);
        {
            Gdiplus::Graphics gs(&scaled);
            gs.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
            gs.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            // 镜像平铺取样：插值取到位图边界之外时用边缘像素的镜像而不是透明，位图四边不会变淡
            Gdiplus::ImageAttributes attrs;
            attrs.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
            gs.DrawImage(pImg, Gdiplus::Rect(0, 0, iw, ih),
                         0, 0, (INT)pImg->GetWidth(), (INT)pImg->GetHeight(),
                         Gdiplus::UnitPixel, &attrs);
        }
        // 纹理只做平移：中间位图与头像同尺寸，左上角对齐到头像左上角，像素一一对应
        Gdiplus::TextureBrush brush(&scaled);
        brush.TranslateTransform(X, Y);
        g.FillPath(&brush, &path);
        delete pImg;
    }
    else
    {
        // Solid fallback + initials.
        Gdiplus::SolidBrush bg(ToGdiColor(m_fallbackBg));
        g.FillPath(&bg, &path);

        CString initials = ComputeInitials(m_name);
        if (!initials.IsEmpty())
        {
            // Use the shared default font scaled to ~50% of the shorter
            // side so initials stay legible at any size.
            // Build a temp font object: pick a point size matching height.
            int desiredPx = (w < h ? w : h) / 2;
            if (desiredPx < 9)
            {
                desiredPx = 9;
            }
            // Convert px to point: 1pt = 1/72in, GetDeviceCaps for LOGPIXELSY.
            int dpiY = ::GetDeviceCaps(hdc, LOGPIXELSY);
            if (dpiY <= 0)
            {
                dpiY = 96;
            }
            int ptSize = ::MulDiv(desiredPx, 72, dpiY);
            if (ptSize < 7)
            {
                ptSize = 7;
            }

            LOGFONT lf = {};
            lf.lfHeight = -::MulDiv(ptSize, dpiY, 72);
            lf.lfWeight = FW_SEMIBOLD;
            lf.lfCharSet = DEFAULT_CHARSET;
            //字体名取 DuiTheme 的默认字体（缺省为微软雅黑，宿主可按界面语言改换）
            _tcsncpy_s(lf.lfFaceName, DuiTheme::Inst().GetDefaultFontFace(), _TRUNCATE);
            HFONT hFont = ::CreateFontIndirect(&lf);
            HFONT oldFont = (HFONT)::SelectObject(hdc, hFont);

            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, m_initialsClr);
            ::DrawText(hdc, initials, -1, &m_rcItem,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
            ::SelectObject(hdc, oldFont);
            ::DeleteObject(hFont);
        }
    }

    // Status dot.
    if (m_status != StatusNone)
    {
        COLORREF dotClr = GetStatusDotColor(m_status);
        if (dotClr != CLR_INVALID)
        {
            int side = (w < h ? w : h);
            int dotD = side / 4;
            if (dotD < 8)
            {
                dotD = 8;
            }
            int dotR = dotD / 2;

            // Center of the dot at 75% / 75% of the avatar.
            int cx = m_rcItem.left + (int)(W * 0.78f);
            int cy = m_rcItem.top  + (int)(H * 0.78f);

            // White outer ring (a slightly larger circle behind the dot)
            // gives separation when the photo behind is light too.
            int ringR = dotR + 2;
            Gdiplus::SolidBrush ringBr(Gdiplus::Color(255, 255, 255, 255));
            g.FillEllipse(&ringBr,
                          (Gdiplus::REAL)(cx - ringR),
                          (Gdiplus::REAL)(cy - ringR),
                          (Gdiplus::REAL)(ringR * 2),
                          (Gdiplus::REAL)(ringR * 2));

            Gdiplus::SolidBrush dotBr(ToGdiColor(dotClr));
            g.FillEllipse(&dotBr,
                          (Gdiplus::REAL)(cx - dotR),
                          (Gdiplus::REAL)(cy - dotR),
                          (Gdiplus::REAL)(dotR * 2),
                          (Gdiplus::REAL)(dotR * 2));
        }
    }

    g.Flush(Gdiplus::FlushIntentionSync);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_AVATAR
