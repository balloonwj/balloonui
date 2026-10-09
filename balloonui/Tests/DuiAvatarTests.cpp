#include "stdafx.h"
#include "DuiAvatarTests.h"

#include <math.h>     // fabs：像素到中心的距离
#include <stdlib.h>   // abs：颜色分量比较
#include <vector>

#if BUI_FEATURE_AVATAR


namespace balloonwjui {

namespace DuiAvatarTests {

namespace {

struct Result
{
    CString name;
    bool ok;
    CString detail;
};

static Result OK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok = true;
    return r;
}

static Result Fail(const CString& n, const CString& d)
{
    Result r;
    r.name = n;
    r.ok = false;
    r.detail = d;
    return r;
}

#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { return Fail(name, _T("string mismatch: \"") + _a + _T("\" vs \"") + _e + _T("\"")); } \
    } while (0)
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)

// ----- Defaults --------------------------------------------------------

static Result Test_Defaults()
{
    DuiAvatar a;
    EXPECT_INT(a.GetShape(),  DuiAvatar::ShapeCircle, _T("Def/shape"));
    EXPECT_INT(a.GetStatus(), DuiAvatar::StatusNone,  _T("Def/status"));
    EXPECT_INT(a.GetCornerRadius(), 8, _T("Def/corner"));
    EXPECT_TRUE(a.GetBitmap() == nullptr, _T("Def/bitmap"));
    EXPECT_TRUE(a.GetName().IsEmpty(),    _T("Def/name"));
    return OK(_T("Defaults"));
}

// ----- Setter round-trips ---------------------------------------------

static Result Test_SetterRoundTrip()
{
    DuiAvatar a;
    a.SetShape(DuiAvatar::ShapeRoundRect);
    EXPECT_INT(a.GetShape(), DuiAvatar::ShapeRoundRect, _T("RT/shape"));
    a.SetCornerRadius(12);
    EXPECT_INT(a.GetCornerRadius(), 12, _T("RT/corner"));
    a.SetCornerRadius(-5);
    EXPECT_INT(a.GetCornerRadius(), 0, _T("RT/cornerClamp"));
    a.SetStatus(DuiAvatar::StatusBusy);
    EXPECT_INT(a.GetStatus(), DuiAvatar::StatusBusy, _T("RT/status"));
    a.SetName(_T("Alice"));
    EXPECT_STR(a.GetName(), _T("Alice"), _T("RT/name"));
    a.SetName(nullptr);
    EXPECT_STR(a.GetName(), _T(""), _T("RT/nameNull"));
    a.SetFallbackBgColor(RGB(11, 22, 33));
    EXPECT_INT((int)a.GetFallbackBgColor(), (int)RGB(11, 22, 33), _T("RT/bg"));
    a.SetInitialsColor(RGB(44, 55, 66));
    EXPECT_INT((int)a.GetInitialsColor(), (int)RGB(44, 55, 66), _T("RT/initClr"));
    return OK(_T("SetterRoundTrip"));
}

// ----- ComputeInitials -------------------------------------------------

static Result Test_ComputeInitials()
{
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("Alice")),       _T("A"),  _T("Init/oneWord"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("Alice Smith")), _T("AS"), _T("Init/twoWords"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("alice b cox")), _T("AC"), _T("Init/firstAndLast"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("  spaced  ")),  _T("S"),  _T("Init/onlyTrim"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("X")),           _T("X"),  _T("Init/single"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("")),            _T(""),   _T("Init/empty"));
    EXPECT_STR(DuiAvatar::ComputeInitials(nullptr),           _T(""),   _T("Init/null"));
    EXPECT_STR(DuiAvatar::ComputeInitials(_T("\t\n  ")),      _T(""),   _T("Init/onlyWS"));
    return OK(_T("ComputeInitials"));
}

// ----- Status dot color ------------------------------------------------

static Result Test_StatusDotColor()
{
    using A = DuiAvatar;
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusOnline),  (int)RGB( 60,175, 80), _T("Dot/online"));
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusAway),    (int)RGB(240,175, 40), _T("Dot/away"));
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusBusy),    (int)RGB(220, 60, 60), _T("Dot/busy"));
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusOffline), (int)RGB(150,150,150), _T("Dot/offline"));
    EXPECT_INT((int)A::GetStatusDotColor(A::StatusNone),    (int)CLR_INVALID,      _T("Dot/none"));
    return OK(_T("StatusDotColor"));
}

// ----- SetBitmap idempotence ------------------------------------------

static Result Test_SetBitmapStores()
{
    DuiAvatar a;
    HBITMAP fakeOpaque = (HBITMAP)0x1234;   // never dereferenced
    a.SetBitmap(fakeOpaque);
    EXPECT_TRUE(a.GetBitmap() == fakeOpaque, _T("Bm/store"));
    a.SetBitmap(nullptr);
    EXPECT_TRUE(a.GetBitmap() == nullptr, _T("Bm/clear"));
    return OK(_T("SetBitmapStores"));
}

// ----- Paint smoke (no bitmap, with initials) -------------------------

static HBITMAP MakeAvatarSrc()
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 32;
    bi.bmiHeader.biHeight   = -32;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!h)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < 32 * 32; ++i)
    {
        p[i*4 + 0] = 200;     // B
        p[i*4 + 1] = 100;     // G
        p[i*4 + 2] =  50;     // R
        p[i*4 + 3] = 255;     // A
    }
    return h;
}

static HBITMAP MakeDestBitmap(int sz)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = sz;
    bi.bmiHeader.biHeight   = -sz;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!h)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < sz * sz; ++i)
    {
        p[i*4]=255;
        p[i*4+1]=255;
        p[i*4+2]=255;
        p[i*4+3]=255;
    }
    return h;
}

static Result Test_PaintSmoke_Initials()
{
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, 64, 64 });
    a.SetName(_T("Alice Smith"));
    a.SetStatus(DuiAvatar::StatusOnline);

    HBITMAP hbmDst = MakeDestBitmap(64);
    if (!hbmDst)
    {
        return Fail(_T("Smoke/initials"), _T("dest dib failed"));
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hbmDst);

    a.OnPaint(hdc, RECT{ 0, 0, 64, 64 });    // must not crash

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(hbmDst);
    return OK(_T("PaintSmoke_Initials"));
}

static Result Test_PaintSmoke_Bitmap()
{
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, 64, 64 });
    HBITMAP src = MakeAvatarSrc();
    if (!src)
    {
        return Fail(_T("Smoke/bitmapSrc"), _T("src dib failed"));
    }
    a.SetBitmap(src);
    a.SetStatus(DuiAvatar::StatusBusy);
    a.SetShape(DuiAvatar::ShapeRoundRect);
    a.SetCornerRadius(12);

    HBITMAP hbmDst = MakeDestBitmap(64);
    if (!hbmDst)
    {
        ::DeleteObject(src);
        return Fail(_T("Smoke/bitmapDst"), _T("dest dib failed"));
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hbmDst);

    a.OnPaint(hdc, RECT{ 0, 0, 64, 64 });    // must not crash

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(hbmDst);
    ::DeleteObject(src);
    return OK(_T("PaintSmoke_Bitmap"));
}

// Painting into a tiny rect is not expected to crash; corner-radius
// auto-clamps to half the shorter side.
static Result Test_PaintSmoke_TinyRect()
{
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, 6, 6 });
    a.SetShape(DuiAvatar::ShapeRoundRect);
    a.SetCornerRadius(99);   // larger than the rect; expected to clamp
    a.SetStatus(DuiAvatar::StatusOnline);

    HBITMAP hbmDst = MakeDestBitmap(8);
    if (!hbmDst)
    {
        return Fail(_T("Smoke/tinyDst"), _T("dest dib failed"));
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hbmDst);

    a.OnPaint(hdc, RECT{ 0, 0, 6, 6 });

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(hbmDst);
    return OK(_T("PaintSmoke_TinyRect"));
}

// 造一张 32 位源图：显示时上半为红、下半为蓝。topDown 选内存里的行序：为 true 时 biHeight 为负
// （显示的第 0 行在像素数据开头），为 false 时 biHeight 为正（显示的第 0 行在最后）。
static HBITMAP MakeTwoBandSrc(int sz, bool topDown)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = sz;
    bi.bmiHeader.biHeight   = topDown ? -sz : sz;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!h)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int y = 0; y < sz; ++y)
    {
        const int memRow = topDown ? y : (sz - 1 - y);
        const bool upper = (y < sz / 2);
        for (int x = 0; x < sz; ++x)
        {
            BYTE* px = p + (memRow * sz + x) * 4;
            px[0] = upper ? 0 : 255;    // 蓝
            px[1] = 0;                  // 绿
            px[2] = upper ? 255 : 0;    // 红
            px[3] = 255;                // 透明度：完全不透明，预乘与否结果相同
        }
    }
    return h;
}

// 用 DuiAvatar 画一张上红下蓝的源图，判断画出来是否仍是上半红、下半蓝。
static bool PaintKeepsOrientation(bool topDown, CString& detail)
{
    const int kSize = 64;
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, kSize, kSize });
    a.SetShape(DuiAvatar::ShapeRoundRect);
    a.SetCornerRadius(4);
    HBITMAP src = MakeTwoBandSrc(kSize, topDown);
    HBITMAP dst = MakeDestBitmap(kSize);
    if (!src || !dst)
    {
        detail = _T("dib create failed");
        return false;
    }
    a.SetBitmap(src);

    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, dst);
    a.OnPaint(hdc, RECT{ 0, 0, kSize, kSize });
    const COLORREF top = ::GetPixel(hdc, kSize / 2, kSize / 8);
    const COLORREF bottom = ::GetPixel(hdc, kSize / 2, kSize - kSize / 8);
    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(dst);
    a.SetBitmap(nullptr);
    ::DeleteObject(src);

    const bool topRed = GetRValue(top) > 200 && GetBValue(top) < 60;
    const bool bottomBlue = GetBValue(bottom) > 200 && GetRValue(bottom) < 60;
    if (!topRed || !bottomBlue)
    {
        detail.Format(_T("top=(%d,%d,%d) bottom=(%d,%d,%d)"),
                      GetRValue(top), GetGValue(top), GetBValue(top),
                      GetRValue(bottom), GetGValue(bottom), GetBValue(bottom));
        return false;
    }
    return true;
}

// GetObject() 对自上而下存储的 DIBSection 同样返回正的 dsBmih.biHeight，行序不能据此推测。
// 两种行序的源图都必须画得方向正确。
static Result Test_BitmapOrientation()
{
    CString detail;
    if (!PaintKeepsOrientation(true, detail))
    {
        return Fail(_T("BitmapOrientation"), _T("top-down source flipped: ") + detail);
    }
    if (!PaintKeepsOrientation(false, detail))
    {
        return Fail(_T("BitmapOrientation"), _T("bottom-up source flipped: ") + detail);
    }
    return OK(_T("BitmapOrientation"));
}

// ---- 位图头像边缘抗锯齿（2026-10-04）----

// 抗锯齿检查用的头像边长（像素）与源位图颜色（纯色，与白底、透明区域都能区分）。
static const int kAaSize = 48;
static const BYTE kAaSrcRed   = 50;
static const BYTE kAaSrcGreen = 100;
static const BYTE kAaSrcBlue  = 200;

// 判定「纯源色」「纯白」的颜色容差（每个分量）。
static const int kAaTolerance = 3;

// 只检查圆周的斜向部分：离中心横、纵距离都不小于边长除以这个数。上下左右四个切点附近的边缘几乎
// 是水平或竖直的，整行有色紧挨整行白色正是平直边缘的正确画法，不算锯齿。
static const int kAaDiagonalDivisor = 5;

// 判断颜色是否在容差内等于 (r, g, b)。
static bool AaNear(COLORREF c, int r, int g, int b)
{
    return abs((int)GetRValue(c) - r) <= kAaTolerance
        && abs((int)GetGValue(c) - g) <= kAaTolerance
        && abs((int)GetBValue(c) - b) <= kAaTolerance;
}

// 画一张 avatar 到 sz 见方的白底位图上，取出全部像素（按行存放）。
static bool PaintAvatarPixels(DuiAvatar& a, int sz, std::vector<COLORREF>& out)
{
    HBITMAP dst = MakeDestBitmap(sz);
    if (!dst)
    {
        return false;
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, dst);
    a.OnPaint(hdc, RECT{ 0, 0, sz, sz });
    ::GdiFlush();
    out.resize((size_t)sz * sz);
    for (int y = 0; y < sz; ++y)
    {
        for (int x = 0; x < sz; ++x)
        {
            out[(size_t)y * sz + x] = ::GetPixel(hdc, x, y);
        }
    }
    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(dst);
    return true;
}

// 位图头像在圆周斜向部分的硬台阶数：横向或纵向相邻的两个像素一个是纯源色、一个是纯白。用「先设裁剪区
// 再 DrawImage」切出的圆形边缘只有这两种像素，必然有这种台阶；抗锯齿的边缘两者之间总有过渡色。
static int CountDiagonalHardSteps(const std::vector<COLORREF>& px, int sz)
{
    const double c = (sz - 1) / 2.0;
    const double minD = (double)sz / kAaDiagonalDivisor;
    int n = 0;
    for (int y = 0; y < sz; ++y)
    {
        for (int x = 0; x < sz; ++x)
        {
            if (fabs(x - c) < minD || fabs(y - c) < minD)
            {
                continue;
            }
            const COLORREF a = px[(size_t)y * sz + x];
            const int nx[2] = { x + 1, x };
            const int ny[2] = { y, y + 1 };
            for (int k = 0; k < 2; ++k)
            {
                if (nx[k] >= sz || ny[k] >= sz)
                {
                    continue;
                }
                const COLORREF b = px[(size_t)ny[k] * sz + nx[k]];
                const bool aSrc = AaNear(a, kAaSrcRed, kAaSrcGreen, kAaSrcBlue);
                const bool bSrc = AaNear(b, kAaSrcRed, kAaSrcGreen, kAaSrcBlue);
                const bool aWhite = AaNear(a, 255, 255, 255);
                const bool bWhite = AaNear(b, 255, 255, 255);
                if ((aSrc && bWhite) || (bSrc && aWhite))
                {
                    ++n;
                }
            }
        }
    }
    return n;
}

// 位图头像（圆形）的边缘应当抗锯齿：圆周斜向部分没有纯源色紧挨纯白的硬台阶。
static Result Test_BitmapEdgeAntialiased()
{
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, kAaSize, kAaSize });
    a.SetShape(DuiAvatar::ShapeCircle);
    HBITMAP src = MakeAvatarSrc();
    if (!src)
    {
        return Fail(_T("BitmapEdgeAntialiased"), _T("src dib failed"));
    }
    a.SetBitmap(src);
    std::vector<COLORREF> px;
    const bool ok = PaintAvatarPixels(a, kAaSize, px);
    a.SetBitmap(nullptr);
    ::DeleteObject(src);
    if (!ok)
    {
        return Fail(_T("BitmapEdgeAntialiased"), _T("dest dib failed"));
    }
    const COLORREF center = px[(size_t)(kAaSize / 2) * kAaSize + kAaSize / 2];
    EXPECT_TRUE(AaNear(center, kAaSrcRed, kAaSrcGreen, kAaSrcBlue), _T("BitmapEdgeAntialiased/center"));
    EXPECT_INT(CountDiagonalHardSteps(px, kAaSize), 0, _T("BitmapEdgeAntialiased/hardSteps"));
    return OK(_T("BitmapEdgeAntialiased"));
}

// 圆角半径为 1 的圆角矩形仍能画出位图（文件共享详情头部的类型图标就用这个设置，见 bugs.md BUG-70）。
static Result Test_BitmapRadius1Draws()
{
    const int kSize = 32;
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, kSize, kSize });
    a.SetShape(DuiAvatar::ShapeRoundRect);
    a.SetCornerRadius(1);
    HBITMAP src = MakeAvatarSrc();
    if (!src)
    {
        return Fail(_T("BitmapRadius1Draws"), _T("src dib failed"));
    }
    a.SetBitmap(src);
    std::vector<COLORREF> px;
    const bool ok = PaintAvatarPixels(a, kSize, px);
    a.SetBitmap(nullptr);
    ::DeleteObject(src);
    if (!ok)
    {
        return Fail(_T("BitmapRadius1Draws"), _T("dest dib failed"));
    }
    const COLORREF center = px[(size_t)(kSize / 2) * kSize + kSize / 2];
    EXPECT_TRUE(AaNear(center, kAaSrcRed, kAaSrcGreen, kAaSrcBlue), _T("BitmapRadius1Draws/center"));
    return OK(_T("BitmapRadius1Draws"));
}

// 源位图里透明的部分画出来仍透出底色（白），不能变黑。系统图标四角就是透明的。
static Result Test_BitmapTransparentStaysClear()
{
    const int kSize = 32;
    // 源图：左上四分之一完全透明，其余为源色。
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = kSize;
    bi.bmiHeader.biHeight   = -kSize;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP src = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!src)
    {
        return Fail(_T("BitmapTransparentStaysClear"), _T("src dib failed"));
    }
    BYTE* p = (BYTE*)bits;
    for (int y = 0; y < kSize; ++y)
    {
        for (int x = 0; x < kSize; ++x)
        {
            BYTE* px = p + (y * kSize + x) * 4;
            const bool clear = (x < kSize / 2 && y < kSize / 2);
            px[0] = clear ? 0 : kAaSrcBlue;    // 预乘透明通道：透明处三个分量都是 0
            px[1] = clear ? 0 : kAaSrcGreen;
            px[2] = clear ? 0 : kAaSrcRed;
            px[3] = clear ? 0 : 255;
        }
    }
    DuiAvatar a;
    a.SetRect(RECT{ 0, 0, kSize, kSize });
    a.SetShape(DuiAvatar::ShapeRoundRect);
    a.SetCornerRadius(1);
    a.SetBitmap(src);
    std::vector<COLORREF> out;
    const bool ok = PaintAvatarPixels(a, kSize, out);
    a.SetBitmap(nullptr);
    ::DeleteObject(src);
    if (!ok)
    {
        return Fail(_T("BitmapTransparentStaysClear"), _T("dest dib failed"));
    }
    // 透明区域中间与不透明区域中间各取一点。
    const COLORREF clearPx = out[(size_t)(kSize / 4) * kSize + kSize / 4];
    const COLORREF solidPx = out[(size_t)(kSize * 3 / 4) * kSize + kSize * 3 / 4];
    EXPECT_TRUE(AaNear(clearPx, 255, 255, 255), _T("BitmapTransparentStaysClear/clear"));
    EXPECT_TRUE(AaNear(solidPx, kAaSrcRed, kAaSrcGreen, kAaSrcBlue), _T("BitmapTransparentStaysClear/solid"));
    return OK(_T("BitmapTransparentStaysClear"));
}

#undef EXPECT_INT
#undef EXPECT_STR
#undef EXPECT_TRUE

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn fn;
    };
    Entry tests[] = {
        { _T("Defaults"),             &Test_Defaults             },
        { _T("SetterRoundTrip"),      &Test_SetterRoundTrip      },
        { _T("ComputeInitials"),      &Test_ComputeInitials      },
        { _T("StatusDotColor"),       &Test_StatusDotColor       },
        { _T("SetBitmapStores"),      &Test_SetBitmapStores      },
        { _T("PaintSmoke_Initials"),  &Test_PaintSmoke_Initials  },
        { _T("PaintSmoke_Bitmap"),    &Test_PaintSmoke_Bitmap    },
        { _T("PaintSmoke_TinyRect"),  &Test_PaintSmoke_TinyRect  },
        { _T("BitmapOrientation"),    &Test_BitmapOrientation    },
        { _T("BitmapEdgeAntialiased"),       &Test_BitmapEdgeAntialiased       },
        { _T("BitmapRadius1Draws"),          &Test_BitmapRadius1Draws          },
        { _T("BitmapTransparentStaysClear"), &Test_BitmapTransparentStaysClear },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    for (auto& e : tests)
    {
        Result r = e.fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), e.name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), e.name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }
    CString summary;
    summary.Format(_T("[summary] DuiAvatarTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiAvatarTests

} // namespace balloonwjui

#endif // BUI_FEATURE_AVATAR
