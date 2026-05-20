#include "stdafx.h"
#include "CaptureMode.h"
#include "Pages.h"
#include "DuiHost.h"
#include "DuiDpi.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

namespace CaptureMode {

namespace {

// Canvas the host is given. Tall enough for the longest current page.
// The DocCaptures page stacks 30+ rows ≈ 120px each, so we need ~4000+
// to fit them without GetRect()-overflowing the back buffer.
// 6000 gives comfortable headroom for future additions; the resulting
// 32bpp DIB is ~26MB which is fine on modern hardware.
const int kCanvasW = 1100;
const int kCanvasH = 6000;

// Owner window class for the headless host. Just provides an HWND that
// the DuiHost can attach to; does not paint anything itself.
class CaptureOwner : public CWindowImpl<CaptureOwner, CWindow>
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiGalleryCapture__"),
                         CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    BEGIN_MSG_MAP(CaptureOwner)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)
    END_MSG_MAP()

    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM, BOOL&) { return 0; }
};

// PNG encoder CLSID for GDI+ Bitmap::Save. Looked up once and cached.
bool GetPngEncoderClsid(CLSID& outClsid)
{
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
    {
        return false;
    }
    BYTE* buf = new BYTE[size];
    Gdiplus::ImageCodecInfo* info =
        reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf);
    Gdiplus::GetImageEncoders(num, size, info);
    bool found = false;
    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(info[i].MimeType, L"image/png") == 0)
        {
            outClsid = info[i].Clsid;
            found = true;
            break;
        }
    }
    delete[] buf;
    return found;
}

// Save the contents of a DIB section (selected into srcDC) inside the
// rectangle (x, y, w, h) to outPath as a 32bpp PNG. Returns true on
// success.
bool SaveRegionAsPng(HDC srcDC, int x, int y, int w, int h, LPCTSTR outPath)
{
    if (w <= 0 || h <= 0)
    {
        return false;
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;        // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS,
                                     &bits, nullptr, 0);
    if (!dib)
    {
        return false;
    }
    HDC memDC = ::CreateCompatibleDC(srcDC);
    HGDIOBJ old = ::SelectObject(memDC, dib);
    ::BitBlt(memDC, 0, 0, w, h, srcDC, x, y, SRCCOPY);
    ::SelectObject(memDC, old);
    ::DeleteDC(memDC);

    // Force opaque alpha - GDI's BitBlt does not write meaningful
    // alpha. Without this, GDI+ saves a partially-transparent PNG.
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < w * h; ++i)
    {
        p[i * 4 + 3] = 0xFF;
    }

    Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppARGB, p);
    CLSID clsid;
    bool ok = false;
    if (GetPngEncoderClsid(clsid))
    {
        ok = (bmp.Save(outPath, &clsid, nullptr) == Gdiplus::Ok);
    }
    ::DeleteObject(dib);
    return ok;
}

} // anonymous

// Snapshot composes:
//   1) BitBlt the host's DUI back buffer (covers all paint-only DUI
//      controls — buttons, labels, avatars, etc.).
//   2) Walk every visible HWND-hosted child (EDIT in DuiEditHost /
//      DuiSearchBox / DuiSpinBox / DuiRichEditHost) and paint each
//      child's content on top of the snapshot in its rect.
//
// PrintWindow on a fully-off-screen window paints black on Win10+, so
// the "snapshot the whole window" path is not viable here; this
// composition trick gets the same end result.
//
// Caller must DeleteDC + DeleteObject the returned pair.
HDC SnapshotHostToDib(HWND host, HDC backDC, int w, int h,
                      HBITMAP& outDib)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS,
                                     &bits, nullptr, 0);
    if (!dib)
    {
        outDib = nullptr;
        return nullptr;
    }
    HDC scrDC  = ::GetDC(nullptr);
    HDC memDC  = ::CreateCompatibleDC(scrDC);
    ::ReleaseDC(nullptr, scrDC);
    ::SelectObject(memDC, dib);

    // 1) DUI back buffer.
    if (backDC)
    {
        ::BitBlt(memDC, 0, 0, w, h, backDC, 0, 0, SRCCOPY);
    }
    else
    {
        RECT rcAll = { 0, 0, w, h };
        ::FillRect(memDC, &rcAll, ::GetSysColorBrush(COLOR_BTNFACE));
    }

    // 2) Walk HWND-hosted children. Each child gets PrintWindow'd
    //    into its rect on top of the DUI paint. PrintWindow on a real
    //    HWND child (with its own paint cycle that already ran) does
    //    work even when the parent is off-screen, because the child
    //    is painting its own DC, not relying on the parent's
    //    composition.
    const UINT PW_CLIENTONLY_FLAG = 0x00000001;
    HWND child = ::GetWindow(host, GW_CHILD);
    while (child)
    {
        if (::IsWindowVisible(child))
        {
            RECT rChild;
            ::GetWindowRect(child, &rChild);
            POINT topLeft = { rChild.left, rChild.top };
            ::ScreenToClient(host, &topLeft);
            int cw = rChild.right  - rChild.left;
            int ch = rChild.bottom - rChild.top;
            if (cw > 0 && ch > 0)
            {
                // Save current DC origin, shift, print, restore.
                POINT oldOrg = {};
                ::GetViewportOrgEx(memDC, &oldOrg);
                ::SetViewportOrgEx(memDC,
                                   oldOrg.x + topLeft.x,
                                   oldOrg.y + topLeft.y,
                                   nullptr);
                ::PrintWindow(child, memDC, PW_CLIENTONLY_FLAG);
                ::SetViewportOrgEx(memDC, oldOrg.x, oldOrg.y, nullptr);
            }
        }
        child = ::GetWindow(child, GW_HWNDNEXT);
    }

    outDib = dib;
    return memDC;
}

int RunCaptureAll(LPCTSTR outDir)
{
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gpToken = 0;
    Gdiplus::GdiplusStartup(&gpToken, &gsi, nullptr);

    ::CreateDirectory(outDir, nullptr);

    CaptureOwner owner;
    // WS_VISIBLE but positioned far off-screen so WM_PAINT actually
    // fires for the host AND its HWND-hosted children. A truly hidden
    // window collapses paint and EDIT controls would never render.
    // WS_EX_TOOLWINDOW + WS_EX_NOACTIVATE keep it out of the taskbar
    // and prevent focus theft. WS_DISABLED stops accidental input.
    if (!owner.Create(NULL, CWindow::rcDefault, _T("DuiGallery Capture"),
                      WS_POPUP | WS_VISIBLE | WS_DISABLED,
                      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE))
    {
        Gdiplus::GdiplusShutdown(gpToken);
        return -1;
    }
    owner.SetWindowPos(NULL, -32000, -32000, kCanvasW, kCanvasH,
                       SWP_NOZORDER | SWP_NOACTIVATE);

    DuiHost host;
    RECT rcClient = { 0, 0, kCanvasW, kCanvasH };
    host.Create(owner.m_hWnd, rcClient, nullptr,
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);

    int totalSaved = 0;
    int nPages = 0;
    const Pages::PageInfo* pages = Pages::GetPages(nPages);

    for (int i = 0; i < nPages; ++i)
    {
        Pages::GetCaptureMarks().clear();
        auto content = pages[i].build();
        host.SetRoot(std::move(content));

        // Pump messages a few times. The first pumps let DuiHost paint
        // its DUI tree; the later pumps let the HWND-hosted children
        // (EDIT, RichEdit) lay out and paint themselves.
        for (int pump = 0; pump < 6; ++pump)
        {
            ::RedrawWindow(host.m_hWnd, NULL, NULL,
                           RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW
                           | RDW_ALLCHILDREN);
            MSG msg;
            while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }

        // Snapshot the entire host's client area (DUI back buffer +
        // PrintWindow each HWND-hosted child) into one big DIB, then
        // crop per-mark.
        HBITMAP fullDib = nullptr;
        HDC fullDC = SnapshotHostToDib(host.m_hWnd,
                                       host.GetBackBufferDC(),
                                       kCanvasW, kCanvasH, fullDib);
        if (!fullDC)
        {
            continue;
        }

        for (const auto& mark : Pages::GetCaptureMarks())
        {
            if (!mark.anchor)
            {
                continue;
            }
            const RECT& r = mark.anchor->GetRect();
            int w = r.right - r.left;
            int h = r.bottom - r.top;
            if (w <= 0 || h <= 0)
            {
                continue;
            }
            CString path;
            path.Format(_T("%s\\ctl-%s.png"), outDir, (LPCTSTR)mark.name);
            if (SaveRegionAsPng(fullDC, r.left, r.top, w, h, path))
            {
                ++totalSaved;
                ::OutputDebugString(path);
                ::OutputDebugString(_T("\n"));
            }
        }

        ::DeleteDC(fullDC);
        ::DeleteObject(fullDib);
    }

    if (host.IsWindow())
    {
        host.DestroyWindow();
    }
    if (owner.IsWindow())
    {
        owner.DestroyWindow();
    }
    Gdiplus::GdiplusShutdown(gpToken);
    return totalSaved;
}

} // namespace CaptureMode
