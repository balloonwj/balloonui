#include "stdafx.h"
#include "DuiGolden.h"
#include <gdiplus.h>
#include <shlwapi.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

namespace balloonwjui {

namespace DuiGolden {

namespace {

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

// Resolve PNG encoder CLSID. Looked up once per process.
bool GetPngEncoderClsid(CLSID& clsid)
{
    UINT count = 0, bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok)
    {
        return false;
    }
    if (bytes == 0)
    {
        return false;
    }
    std::vector<BYTE> buf(bytes);
    Gdiplus::ImageCodecInfo* codecs = (Gdiplus::ImageCodecInfo*)buf.data();
    if (Gdiplus::GetImageEncoders(count, bytes, codecs) != Gdiplus::Ok)
    {
        return false;
    }
    for (UINT i = 0; i < count; ++i)
    {
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0)
        {
            clsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

CString ExeDir()
{
    TCHAR buf[MAX_PATH] = {};
    ::GetModuleFileName(NULL, buf, MAX_PATH);
    CString p = buf;
    int slash = p.ReverseFind(_T('\\'));
    return slash >= 0 ? p.Left(slash) : p;
}

void EnsureDirExists(LPCTSTR dir)
{
    if (!dir || !*dir)
    {
        return;
    }
    if (::PathIsDirectory(dir))
    {
        return;
    }
    ::CreateDirectory(dir, nullptr);
}

// 32bpp top-down DIBSection helpers — uniform pixel access pattern.
HBITMAP CreateBlankDib(int w, int h, void** outBits)
{
    if (w <= 0 || h <= 0)
    {
        *outBits = nullptr;
        return nullptr;
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = w;
    bi.bmiHeader.biHeight   = -h;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    *outBits = bits;
    return hbm;
}

bool DibInfo(HBITMAP hbm, int& w, int& h, BYTE*& bits, int& strideBytes)
{
    DIBSECTION ds = {};
    if (::GetObject(hbm, sizeof(ds), &ds) != sizeof(ds))
    {
        return false;
    }
    if (ds.dsBm.bmBitsPixel != 32 || !ds.dsBm.bmBits)
    {
        return false;
    }
    w = ds.dsBm.bmWidth;
    h = ds.dsBm.bmHeight;
    if (h < 0)
    {
        h = -h;
    }
    bits = (BYTE*)ds.dsBm.bmBits;
    strideBytes = ds.dsBm.bmWidthBytes;

    // For bottom-up DIBs (positive biHeight), advance bits to the bottom
    // row and use negative stride so callers always read top-down.
    if (ds.dsBmih.biHeight > 0)
    {
        bits = bits + (LONG_PTR)strideBytes * (ds.dsBmih.biHeight - 1);
        strideBytes = -strideBytes;
    }
    return true;
}

} // namespace

CString ResolveGoldenPath(LPCTSTR name)
{
    CString p = ExeDir();
    p += _T("\\..\\Goldens\\");
    p += name;
    p += _T(".png");
    return p;
}

CString GetArtifactDir()
{
    TCHAR tmp[MAX_PATH] = {};
    ::GetTempPath(MAX_PATH, tmp);
    CString p = tmp;
    if (p.IsEmpty() || p[p.GetLength() - 1] != _T('\\'))
    {
        p += _T("\\");
    }
    p += _T("DuiGolden\\");
    return p;
}

HBITMAP RenderControlToHbm(DuiControl* c, int w, int h)
{
    if (!c || w <= 0 || h <= 0)
    {
        return nullptr;
    }
    void* bits = nullptr;
    HBITMAP hbm = CreateBlankDib(w, h, &bits);
    if (!hbm)
    {
        return nullptr;
    }

    // Pre-fill with white so transparent / unfilled regions render as
    // white in the golden, matching how DuiHost sets up its back buffer.
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < w * h; ++i)
    {
        p[i*4 + 0] = 255;
        p[i*4 + 1] = 255;
        p[i*4 + 2] = 255;
        p[i*4 + 3] = 255;
    }

    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, hbm);

    c->SetRect(RECT{ 0, 0, w, h });
    c->OnPaint(hdc, RECT{ 0, 0, w, h });

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    return hbm;
}

bool SaveHbmAsPng(HBITMAP hbm, LPCTSTR path)
{
    if (!hbm || !path)
    {
        return false;
    }
    EnsureGdiplus();

    // Use FromHBITMAP — we intentionally accept the alpha-flatten since
    // goldens are always opaque RGB samples.
    Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromHBITMAP(hbm, nullptr);
    if (!bmp)
    {
        return false;
    }

    // Make sure the parent directory exists.
    CString dir = path;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash >= 0)
    {
        dir = dir.Left(slash);
        EnsureDirExists(dir);
    }

    CLSID clsid = {};
    bool ok = GetPngEncoderClsid(clsid);
    Gdiplus::Status st = Gdiplus::GenericError;
    if (ok)
    {
#ifdef _UNICODE
        st = bmp->Save(path, &clsid, nullptr);
#else
        // Convert path to wide.
        int cw = ::MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
        std::vector<WCHAR> w(cw, 0);
        ::MultiByteToWideChar(CP_ACP, 0, path, -1, w.data(), cw);
        st = bmp->Save(w.data(), &clsid, nullptr);
#endif
    }
    delete bmp;
    return st == Gdiplus::Ok;
}

HBITMAP LoadPngAsHbm(LPCTSTR path)
{
    if (!path)
    {
        return nullptr;
    }
    EnsureGdiplus();
#ifdef _UNICODE
    Gdiplus::Bitmap bmp(path);
#else
    int cw = ::MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
    std::vector<WCHAR> w(cw, 0);
    ::MultiByteToWideChar(CP_ACP, 0, path, -1, w.data(), cw);
    Gdiplus::Bitmap bmp(w.data());
#endif
    if (bmp.GetLastStatus() != Gdiplus::Ok)
    {
        return nullptr;
    }
    HBITMAP hbm = nullptr;
    if (bmp.GetHBITMAP(Gdiplus::Color(255, 255, 255), &hbm) != Gdiplus::Ok)
    {
        return nullptr;
    }
    return hbm;
}

DiffSummary CompareHbm(HBITMAP actual, HBITMAP golden, int tolerance)
{
    DiffSummary s = { 0, 0, 0, false };
    int aw = 0, ah = 0, gw = 0, gh = 0;
    BYTE* ab = nullptr;
    BYTE* gb = nullptr;
    int aStride = 0, gStride = 0;
    if (!DibInfo(actual, aw, ah, ab, aStride))
    {
        s.sizeMismatch = true;
        return s;
    }
    if (!DibInfo(golden, gw, gh, gb, gStride))
    {
        s.sizeMismatch = true;
        return s;
    }
    if (aw != gw || ah != gh)
    {
        s.sizeMismatch = true;
        return s;
    }

    s.totalPixels = aw * ah;
    if (tolerance < 0)
    {
        tolerance = 0;
    }
    for (int y = 0; y < ah; ++y)
    {
        BYTE* ar = ab + (LONG_PTR)aStride * y;
        BYTE* gr = gb + (LONG_PTR)gStride * y;
        for (int x = 0; x < aw; ++x)
        {
            int dB = (int)ar[x*4 + 0] - (int)gr[x*4 + 0];
            int dG = (int)ar[x*4 + 1] - (int)gr[x*4 + 1];
            int dR = (int)ar[x*4 + 2] - (int)gr[x*4 + 2];
            if (dB < 0)
            {
                dB = -dB;
            }
            if (dG < 0)
            {
                dG = -dG;
            }
            if (dR < 0)
            {
                dR = -dR;
            }
            int worst = dB;
            if (dG > worst)
            {
                worst = dG;
            }
            if (dR > worst)
            {
                worst = dR;
            }
            if (worst > s.maxChannelDelta)
            {
                s.maxChannelDelta = worst;
            }
            if (worst > tolerance)
            {
                ++s.diffPixels;
            }
        }
    }
    return s;
}

HBITMAP MakeDiffHbm(HBITMAP actual, HBITMAP golden, int tolerance)
{
    int aw = 0, ah = 0, gw = 0, gh = 0;
    BYTE* ab = nullptr;
    BYTE* gb = nullptr;
    int aStride = 0, gStride = 0;
    if (!DibInfo(actual, aw, ah, ab, aStride))
    {
        return nullptr;
    }
    if (!DibInfo(golden, gw, gh, gb, gStride))
    {
        return nullptr;
    }
    if (aw != gw || ah != gh)
    {
        return nullptr;
    }

    void* bits = nullptr;
    HBITMAP out = CreateBlankDib(aw, ah, &bits);
    if (!out)
    {
        return nullptr;
    }

    BYTE* ob = (BYTE*)bits;
    int oStride = aw * 4;          // top-down, no negative-stride trick
    if (tolerance < 0)
    {
        tolerance = 0;
    }
    for (int y = 0; y < ah; ++y)
    {
        BYTE* ar = ab + (LONG_PTR)aStride * y;
        BYTE* gr = gb + (LONG_PTR)gStride * y;
        BYTE* op = ob + (LONG_PTR)oStride * y;
        for (int x = 0; x < aw; ++x)
        {
            int dB = (int)ar[x*4 + 0] - (int)gr[x*4 + 0];
            int dG = (int)ar[x*4 + 1] - (int)gr[x*4 + 1];
            int dR = (int)ar[x*4 + 2] - (int)gr[x*4 + 2];
            if (dB < 0)
            {
                dB = -dB;
            }
            if (dG < 0)
            {
                dG = -dG;
            }
            if (dR < 0)
            {
                dR = -dR;
            }
            int worst = dB;
            if (dG > worst)
            {
                worst = dG;
            }
            if (dR > worst)
            {
                worst = dR;
            }
            if (worst > tolerance)
            {
                op[x*4 + 0] = 0;     // B
                op[x*4 + 1] = 0;     // G
                op[x*4 + 2] = 255;   // R
                op[x*4 + 3] = 255;
            }
            else
            {
                op[x*4 + 0] = 255;
                op[x*4 + 1] = 255;
                op[x*4 + 2] = 255;
                op[x*4 + 3] = 255;
            }
        }
    }
    return out;
}

bool RunGoldenTest(LPCTSTR testName, DuiControl* c, int w, int h, int tolerance)
{
    if (!testName || !c)
    {
        return false;
    }
    HBITMAP actual = RenderControlToHbm(c, w, h);
    if (!actual)
    {
        return false;
    }

    CString goldenPath = ResolveGoldenPath(testName);
    bool isUpdate = (::GetEnvironmentVariable(_T("DUI_GOLDEN_UPDATE"),
                                              nullptr, 0) > 0);
    bool exists = ::PathFileExists(goldenPath) != FALSE;

    if (isUpdate || !exists)
    {
        bool saved = SaveHbmAsPng(actual, goldenPath);
        ::DeleteObject(actual);
        return saved;
    }

    HBITMAP golden = LoadPngAsHbm(goldenPath);
    if (!golden)
    {
        ::DeleteObject(actual);
        return false;
    }

    DiffSummary s = CompareHbm(actual, golden, tolerance);
    bool passed = !s.sizeMismatch && s.diffPixels == 0;

    if (!passed)
    {
        // Drop artifacts so a human can diff visually.
        CString dir = GetArtifactDir();
        EnsureDirExists(dir);
        CString actualPath = dir + testName + _T(".actual.png");
        CString diffPath   = dir + testName + _T(".diff.png");
        SaveHbmAsPng(actual, actualPath);
        if (!s.sizeMismatch)
        {
            HBITMAP diff = MakeDiffHbm(actual, golden, tolerance);
            if (diff)
            {
                SaveHbmAsPng(diff, diffPath);
                ::DeleteObject(diff);
            }
        }
    }

    ::DeleteObject(actual);
    ::DeleteObject(golden);
    return passed;
}

} // namespace DuiGolden

} // namespace balloonwjui
