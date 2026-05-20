#include "stdafx.h"
#include "DuiNinePatchTests.h"

namespace balloonwjui {

namespace DuiNinePatchTests {

namespace {

struct Result { CString name; bool ok; CString detail; };
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
#define EXPECT_RECT(rc, L, T, R, B, name) \
    do { const RECT& _r = (rc); \
         if (_r.left!=(L)||_r.top!=(T)||_r.right!=(R)||_r.bottom!=(B)) { \
             CString _d; _d.Format(_T("expected=(%d,%d,%d,%d) got=(%d,%d,%d,%d)"), \
                 (L),(T),(R),(B),_r.left,_r.top,_r.right,_r.bottom); \
             return Fail(name, _d); } \
    } while (0)
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)

// ----- ClampInsets ------------------------------------------------------

// Negative insets clamp to 0.
static Result Test_ClampNegatives()
{
    using namespace DuiNinePatch;
    Insets in;
    in.left = -5;
    in.top = -1;
    in.right = -100;
    in.bottom = 0;
    Insets r = ClampInsets(20, 20, in);
    EXPECT_INT(r.left,   0, _T("Clamp/negL"));
    EXPECT_INT(r.top,    0, _T("Clamp/negT"));
    EXPECT_INT(r.right,  0, _T("Clamp/negR"));
    EXPECT_INT(r.bottom, 0, _T("Clamp/zeroB"));
    return OK(_T("ClampNegatives"));
}

// Insets within source dims pass through unchanged.
static Result Test_ClampPassThrough()
{
    using namespace DuiNinePatch;
    Insets in;
    in.left = 4;
    in.top = 3;
    in.right = 4;
    in.bottom = 3;
    Insets r = ClampInsets(20, 20, in);
    EXPECT_INT(r.left,   4, _T("ClampPT/L"));
    EXPECT_INT(r.top,    3, _T("ClampPT/T"));
    EXPECT_INT(r.right,  4, _T("ClampPT/R"));
    EXPECT_INT(r.bottom, 3, _T("ClampPT/B"));
    return OK(_T("ClampPassThrough"));
}

// Horizontal pair overflow: 7+7=14 > srcW=10 -> shrink proportionally
// to roughly (5,5). Same for vertical.
static Result Test_ClampOverflow()
{
    using namespace DuiNinePatch;
    Insets in;
    in.left = 7;
    in.top = 8;
    in.right = 7;
    in.bottom = 8;
    Insets r = ClampInsets(10, 10, in);
    EXPECT_INT(r.left + r.right,  10, _T("ClampOF/Hsum"));
    EXPECT_INT(r.top  + r.bottom, 10, _T("ClampOF/Vsum"));
    EXPECT_TRUE(r.left  > 0 && r.right  > 0, _T("ClampOF/HnonZero"));
    EXPECT_TRUE(r.top   > 0 && r.bottom > 0, _T("ClampOF/VnonZero"));
    return OK(_T("ClampOverflow"));
}

// ----- ComputeCells ------------------------------------------------------

// Standard case: 30x30 src, insets (10,10,10,10), dst (0,0,90,90).
// Center src is 10x10 from (10,10)->(20,20); dst center stretches to
// 70x70 from (10,10)->(80,80).
static Result Test_ComputeCells_Basic()
{
    using namespace DuiNinePatch;
    Cell cells[9];
    Insets ins;
    ins.left = ins.top = ins.right = ins.bottom = 10;
    RECT dst = { 0, 0, 90, 90 };
    ComputeCells(30, 30, dst, ins, cells);

    // TL src=(0,0,10,10) dst=(0,0,10,10)
    EXPECT_RECT(cells[0].src, 0, 0, 10, 10, _T("CC/0src"));
    EXPECT_RECT(cells[0].dst, 0, 0, 10, 10, _T("CC/0dst"));
    // T  src=(10,0,20,10) dst=(10,0,80,10)
    EXPECT_RECT(cells[1].src, 10, 0, 20, 10, _T("CC/1src"));
    EXPECT_RECT(cells[1].dst, 10, 0, 80, 10, _T("CC/1dst"));
    // TR src=(20,0,30,10) dst=(80,0,90,10)
    EXPECT_RECT(cells[2].src, 20, 0, 30, 10, _T("CC/2src"));
    EXPECT_RECT(cells[2].dst, 80, 0, 90, 10, _T("CC/2dst"));
    // C  src=(10,10,20,20) dst=(10,10,80,80)
    EXPECT_RECT(cells[4].src, 10, 10, 20, 20, _T("CC/4src"));
    EXPECT_RECT(cells[4].dst, 10, 10, 80, 80, _T("CC/4dst"));
    // BR src=(20,20,30,30) dst=(80,80,90,90)
    EXPECT_RECT(cells[8].src, 20, 20, 30, 30, _T("CC/8src"));
    EXPECT_RECT(cells[8].dst, 80, 80, 90, 90, _T("CC/8dst"));
    return OK(_T("ComputeCells_Basic"));
}

// Dst smaller than corners: corners shrink proportionally so they don't
// overlap, center collapses to zero size.
static Result Test_ComputeCells_TinyDst()
{
    using namespace DuiNinePatch;
    Cell cells[9];
    Insets ins;
    ins.left = 10;
    ins.top = 10;
    ins.right = 10;
    ins.bottom = 10;
    RECT dst = { 0, 0, 6, 6 };
    ComputeCells(30, 30, dst, ins, cells);

    // L+R corners must add up to dst width = 6; same for top/bottom.
    int Lw = cells[0].dst.right - cells[0].dst.left;
    int Rw = cells[2].dst.right - cells[2].dst.left;
    EXPECT_INT(Lw + Rw, 6, _T("CC/tinyHsum"));
    int Th = cells[0].dst.bottom - cells[0].dst.top;
    int Bh = cells[6].dst.bottom - cells[6].dst.top;
    EXPECT_INT(Th + Bh, 6, _T("CC/tinyVsum"));
    // Center should be empty (right<=left or bottom<=top).
    EXPECT_TRUE(::IsRectEmpty(&cells[4].dst), _T("CC/tinyCenterEmpty"));
    return OK(_T("ComputeCells_TinyDst"));
}

// Zero/negative source dims -> all cells stay zero.
static Result Test_ComputeCells_BadSrc()
{
    using namespace DuiNinePatch;
    Cell cells[9];
    for (int i = 0; i < 9; ++i)
    {
        cells[i].src = RECT{ 99, 99, 99, 99 };
        cells[i].dst = RECT{ 99, 99, 99, 99 };
    }
    Insets ins;
    ins.left = ins.top = ins.right = ins.bottom = 1;
    RECT dst = { 0, 0, 50, 50 };
    ComputeCells(0, 30, dst, ins, cells);
    // Per contract, ComputeCells zeroes out[] before returning on bad src.
    for (int i = 0; i < 9; ++i)
    {
        EXPECT_RECT(cells[i].src, 0, 0, 0, 0, _T("CC/badSrcCleared"));
        EXPECT_RECT(cells[i].dst, 0, 0, 0, 0, _T("CC/badDstCleared"));
    }
    return OK(_T("ComputeCells_BadSrc"));
}

// Empty destination rect -> all cells empty too.
static Result Test_ComputeCells_EmptyDst()
{
    using namespace DuiNinePatch;
    Cell cells[9];
    Insets ins;
    ins.left = ins.top = ins.right = ins.bottom = 5;
    RECT dst = { 10, 10, 10, 10 };  // zero width/height
    ComputeCells(30, 30, dst, ins, cells);
    for (int i = 0; i < 9; ++i)
    {
        EXPECT_TRUE(::IsRectEmpty(&cells[i].dst), _T("CC/emptyDst"));
    }
    return OK(_T("ComputeCells_EmptyDst"));
}

// Insets tiles up perfectly: dst exactly fits all 9 cells with center
// equal to source center (no stretching, identity-ish).
static Result Test_ComputeCells_Identity()
{
    using namespace DuiNinePatch;
    Cell cells[9];
    Insets ins;
    ins.left = 4;
    ins.top = 4;
    ins.right = 4;
    ins.bottom = 4;
    RECT dst = { 0, 0, 10, 10 };
    ComputeCells(10, 10, dst, ins, cells);
    // Identity layout: each cell src == dst (after offsetting dst by (0,0)).
    for (int i = 0; i < 9; ++i)
    {
        const RECT& s = cells[i].src;
        const RECT& d = cells[i].dst;
        if (::IsRectEmpty(&s) || ::IsRectEmpty(&d))
        {
            continue;
        }
        EXPECT_INT(d.right - d.left, s.right - s.left, _T("CC/idW"));
        EXPECT_INT(d.bottom - d.top, s.bottom - s.top, _T("CC/idH"));
    }
    return OK(_T("ComputeCells_Identity"));
}

// ----- Draw smoke -------------------------------------------------------

// Build a 9x9 ARGB DIBSection with 4 distinct corner colors so we can
// later sample the destination DC and verify each corner ended up where
// we expected.
//
// Source layout (9x9 with 3px insets — corners are 3x3 each):
//   TL: red       T:    white  TR: green
//   L : white     C:    white  R : white
//   BL: blue      B:    white  BR: yellow
//
// All "white" regions stay white so we can also check center fill.
struct ARGB { BYTE b, g, r, a; };

static HBITMAP Make9x9Bitmap()
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 9;
    bi.bmiHeader.biHeight   = -9;          // top-down
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hbm || !pBits)
    {
        return nullptr;
    }

    ARGB* px = (ARGB*)pBits;
    auto setRect = [&](int x0, int y0, int x1, int y1, BYTE r, BYTE g, BYTE b)
    {
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                ARGB& p = px[y * 9 + x];
                p.r = r;
                p.g = g;
                p.b = b;
                p.a = 255;
            }
        }
    };
    // White everywhere first.
    setRect(0, 0, 9, 9, 255, 255, 255);
    // Corners.
    setRect(0, 0, 3, 3, 255, 0,   0  );  // TL red
    setRect(6, 0, 9, 3, 0,   200, 0  );  // TR green
    setRect(0, 6, 3, 9, 0,   0,   255);  // BL blue
    setRect(6, 6, 9, 9, 240, 220, 0  );  // BR yellow
    return hbm;
}

// Sample one pixel from a top-down 32bpp DIBSection.
static COLORREF SampleHbm(HBITMAP hbm, int x, int y)
{
    DIBSECTION ds = {};
    if (::GetObject(hbm, sizeof(ds), &ds) != sizeof(ds))
    {
        return CLR_INVALID;
    }
    ARGB* px = (ARGB*)ds.dsBm.bmBits;
    int stride = ds.dsBm.bmWidthBytes / 4;
    int H = ds.dsBmih.biHeight;
    if (H > 0)  // bottom-up: invert y
    {
        y = H - 1 - y;
    }
    ARGB p = px[y * stride + x];
    return RGB(p.r, p.g, p.b);
}

// True if c1 / c2 differ by no more than tol per channel (gives the
// stretch interpolation a little slack).
static bool ColorsClose(COLORREF c1, COLORREF c2, int tol)
{
    int dr = (int)GetRValue(c1) - (int)GetRValue(c2);
    int dg = (int)GetGValue(c1) - (int)GetGValue(c2);
    int db = (int)GetBValue(c1) - (int)GetBValue(c2);
    if (dr < 0)
    {
        dr = -dr;
    }
    if (dg < 0)
    {
        dg = -dg;
    }
    if (db < 0)
    {
        db = -db;
    }
    return dr <= tol && dg <= tol && db <= tol;
}

// Paint the 9x9 source into a 60x60 destination DIB; sample a pixel near
// each corner of the destination and confirm we got the right corner color.
static Result Test_Draw_CornersLandRight()
{
    using namespace DuiNinePatch;

    HBITMAP hbmSrc = Make9x9Bitmap();
    if (!hbmSrc)
    {
        return Fail(_T("Draw/cornersSrc"), _T("CreateDIBSection failed"));
    }

    // Destination: 60x60 top-down 32bpp DIB, white background.
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 60;
    bi.bmiHeader.biHeight   = -60;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pDst = nullptr;
    HBITMAP hbmDst = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &pDst, nullptr, 0);
    if (!hbmDst)
    {
        ::DeleteObject(hbmSrc);
        return Fail(_T("Draw/cornersDst"), _T("CreateDIBSection failed"));
    }

    // Pre-fill destination with white so we can tell if a paint actually landed.
    ARGB* dpx = (ARGB*)pDst;
    for (int i = 0; i < 60 * 60; ++i)
    {
        dpx[i].r = 255;
        dpx[i].g = 255;
        dpx[i].b = 255;
        dpx[i].a = 255;
    }

    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hbmDst);

    Insets ins;
    ins.left = 3;
    ins.top = 3;
    ins.right = 3;
    ins.bottom = 3;
    Options opts;
    opts.hasAlpha = false;  // 9x9 is opaque RGB; FromHBITMAP path is fine
    RECT dst = { 0, 0, 60, 60 };
    bool drew = Draw(hdc, hbmSrc, dst, ins, opts);

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);

    if (!drew)
    {
        ::DeleteObject(hbmSrc);
        ::DeleteObject(hbmDst);
        return Fail(_T("Draw/cornersDrawRet"), _T("Draw returned false"));
    }

    // Sample a pixel at the centre of each destination corner cell
    // (corner cells are 3x3 in dst since src corners are 3x3 and dst is
    // wide enough to keep them at 1:1 scale).
    int tol = 16;  // GDI+ may add a 1-pixel anti-aliased fringe.
    COLORREF tl = SampleHbm(hbmDst, 1,  1 );
    COLORREF tr = SampleHbm(hbmDst, 58, 1 );
    COLORREF bl = SampleHbm(hbmDst, 1,  58);
    COLORREF br = SampleHbm(hbmDst, 58, 58);

    ::DeleteObject(hbmSrc);
    ::DeleteObject(hbmDst);

    auto fmt = [](COLORREF c)
    {
        CString s;
        s.Format(_T("(%d,%d,%d)"), GetRValue(c), GetGValue(c), GetBValue(c));
        return s;
    };
    if (!ColorsClose(tl, RGB(255, 0, 0), tol))
    {
        return Fail(_T("Draw/TLcolor"), _T("TL ") + fmt(tl) + _T(" not red"));
    }
    if (!ColorsClose(tr, RGB(0, 200, 0), tol))
    {
        return Fail(_T("Draw/TRcolor"), _T("TR ") + fmt(tr) + _T(" not green"));
    }
    if (!ColorsClose(bl, RGB(0, 0, 255), tol))
    {
        return Fail(_T("Draw/BLcolor"), _T("BL ") + fmt(bl) + _T(" not blue"));
    }
    if (!ColorsClose(br, RGB(240, 220, 0), tol))
    {
        return Fail(_T("Draw/BRcolor"), _T("BR ") + fmt(br) + _T(" not yellow"));
    }
    return OK(_T("Draw_CornersLandRight"));
}

// Null bitmap returns false without crashing; null DC same.
static Result Test_Draw_GuardsAgainstNulls()
{
    using namespace DuiNinePatch;
    Insets ins;
    ins.left = 1;
    ins.top = 1;
    ins.right = 1;
    ins.bottom = 1;
    RECT dst = { 0, 0, 10, 10 };

    bool a = Draw(nullptr, nullptr, dst, ins);
    EXPECT_TRUE(!a, _T("Draw/nullAll"));

    HDC hdc = ::CreateCompatibleDC(nullptr);
    bool b = Draw(hdc, nullptr, dst, ins);
    ::DeleteDC(hdc);
    EXPECT_TRUE(!b, _T("Draw/nullBmp"));

    return OK(_T("Draw_GuardsAgainstNulls"));
}

// Empty destination rect returns false.
static Result Test_Draw_EmptyDst()
{
    using namespace DuiNinePatch;
    HBITMAP hbm = Make9x9Bitmap();
    if (!hbm)
    {
        return Fail(_T("Draw/emptySrc"), _T("CreateDIBSection failed"));
    }
    HDC hdc = ::CreateCompatibleDC(nullptr);
    Insets ins;
    ins.left = 1;
    ins.top = 1;
    ins.right = 1;
    ins.bottom = 1;
    RECT dst = { 5, 5, 5, 5 };
    bool ok = Draw(hdc, hbm, dst, ins);
    ::DeleteDC(hdc);
    ::DeleteObject(hbm);
    EXPECT_TRUE(!ok, _T("Draw/emptyRet"));
    return OK(_T("Draw_EmptyDst"));
}

#undef EXPECT_INT
#undef EXPECT_RECT
#undef EXPECT_TRUE

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("ClampNegatives"),         &Test_ClampNegatives         },
        { _T("ClampPassThrough"),       &Test_ClampPassThrough       },
        { _T("ClampOverflow"),          &Test_ClampOverflow          },
        { _T("ComputeCells_Basic"),     &Test_ComputeCells_Basic     },
        { _T("ComputeCells_TinyDst"),   &Test_ComputeCells_TinyDst   },
        { _T("ComputeCells_BadSrc"),    &Test_ComputeCells_BadSrc    },
        { _T("ComputeCells_EmptyDst"),  &Test_ComputeCells_EmptyDst  },
        { _T("ComputeCells_Identity"),  &Test_ComputeCells_Identity  },
        { _T("Draw_CornersLandRight"),  &Test_Draw_CornersLandRight  },
        { _T("Draw_GuardsAgainstNulls"),&Test_Draw_GuardsAgainstNulls},
        { _T("Draw_EmptyDst"),          &Test_Draw_EmptyDst          },
    };

    CString out;
    int passed = 0, failed = 0;
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
    summary.Format(_T("[summary] DuiNinePatchTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiNinePatchTests

} // namespace balloonwjui
