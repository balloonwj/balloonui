#include "stdafx.h"
#include "CaptureMode.h"
#include "DuiHost.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

namespace CaptureMode {

// Writes a single line to %TEMP%\DemoTextBadgeTile.log AND OutputDebugString.
// Append-only — safe to call many times. Used to diagnose capture
// failures since GUI apps don't have a console.
static void DebugLog(LPCTSTR line)
{
    ::OutputDebugString(line);
    TCHAR tmp[MAX_PATH] = {};
    if (!::GetTempPath(MAX_PATH, tmp))
    {
        return;
    }
    CString p = tmp;
    p += _T("DemoTextBadgeTile.log");
    HANDLE h = ::CreateFile(p, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    CStringA a(line);
    DWORD w = 0;
    ::WriteFile(h, (LPCSTR)a, a.GetLength(), &w, NULL);
    ::CloseHandle(h);
}

namespace {

// Canvas 大小 — demo 内容比 DuiGallery 简单，1000×800 足够；过大会多用内存。
const int kCanvasW = 1000;
const int kCanvasH = 800;

// (CaptureOwner 类移到 anonymous 命名空间外，见 namespace 关闭后。
//  ATL 的 DECLARE_WND_CLASS_EX 在匿名命名空间内可能引发类注册问题 —
//  搬到 file scope 后注册稳定。)

// PNG encoder CLSID 查找。GDI+ 需要这个才能把 Bitmap 存成 PNG 格式。
bool GetPngEncoderClsid(CLSID& outClsid)
{
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
    {
        return false;
    }
    BYTE* buf = new BYTE[size];
    auto* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf);
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

// 把 srcDC 中 (x,y,w,h) 区域保存成 PNG。
// 处理细节：BitBlt 的 alpha 是垃圾 → 必须强行 0xFF 否则 PNG 半透明。
bool SaveRegionAsPng(HDC srcDC, int x, int y, int w, int h, LPCTSTR outPath)
{
    if (w <= 0 || h <= 0)
    {
        return false;
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;       // top-down
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

// 合成快照：DUI 后台缓冲 + 各 HWND 子窗口的 PrintWindow 内容。
// 必要的复杂度 — 单纯 PrintWindow 屏外窗口在 Win10+ 会出黑图。
HDC SnapshotHostToDib(HWND host, HDC backDC, int w, int h, HBITMAP& outDib)
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
    HDC scrDC = ::GetDC(nullptr);
    HDC memDC = ::CreateCompatibleDC(scrDC);
    ::ReleaseDC(nullptr, scrDC);
    ::SelectObject(memDC, dib);

    if (backDC)
    {
        ::BitBlt(memDC, 0, 0, w, h, backDC, 0, 0, SRCCOPY);
    }
    else
    {
        RECT rcAll = { 0, 0, w, h };
        ::FillRect(memDC, &rcAll, ::GetSysColorBrush(COLOR_BTNFACE));
    }

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

// Mark 注册表 — 进程级 vector，每个 demo 独立持有自己的副本。
std::vector<Mark>& GetMarks()
{
    static std::vector<Mark> s;
    return s;
}

} // anonymous

// 屏外 owner 窗口类。在 anonymous namespace 之外定义以让 ATL
// DECLARE_WND_CLASS_EX 正常注册。
class CaptureOwner : public CWindowImpl<CaptureOwner, CWindow>
{
public:
    DECLARE_WND_CLASS_EX(_T("__DemoCaptureOwner__"),
                         CS_HREDRAW | CS_VREDRAW, COLOR_WINDOW)

    BEGIN_MSG_MAP(CaptureOwner)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)
    END_MSG_MAP()

    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM, BOOL&) { return 0; }
};

void RegisterMark(LPCTSTR name, balloonwjui::DuiControl* anchor)
{
    if (!name || !anchor)
    {
        return;
    }
    Mark m;
    m.name   = name;
    m.anchor = anchor;
    GetMarks().push_back(m);
}

int RunCaptureAll(LPCTSTR outDir,
                  LPCTSTR filePrefix,
                  std::function<std::unique_ptr<DuiControl>()> buildContent)
{
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gpToken = 0;
    Gdiplus::GdiplusStartup(&gpToken, &gsi, nullptr);

    ::CreateDirectory(outDir, nullptr);

    // Manually register a window class (bypass ATL DECLARE_WND_CLASS_EX
    // which seems to fail to register from a DLL-linked demo TU).
    static bool s_classRegistered = false;
    static const TCHAR kClassName[] = _T("__DemoCaptureOwner__");
    if (!s_classRegistered)
    {
        WNDCLASSEX wc = { sizeof(wc) };
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = ::DefWindowProc;
        wc.hInstance     = ::GetModuleHandle(NULL);
        wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = kClassName;
        ATOM atom = ::RegisterClassEx(&wc);
        if (!atom)
        {
            DWORD err = ::GetLastError();
            CString m;
            m.Format(_T("[CaptureMode] RegisterClassEx FAILED err=%lu\n"), err);
            DebugLog(m);
            Gdiplus::GdiplusShutdown(gpToken);
            return -1;
        }
        s_classRegistered = true;
    }
    HWND hOwner = ::CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, _T("DemoCapture"),
        WS_POPUP | WS_VISIBLE | WS_DISABLED,
        -32000, -32000, kCanvasW, kCanvasH,
        NULL, NULL, ::GetModuleHandle(NULL), NULL);
    if (!hOwner)
    {
        DWORD err = ::GetLastError();
        CString m;
        m.Format(_T("[CaptureMode] CreateWindowEx FAILED err=%lu\n"), err);
        DebugLog(m);
        Gdiplus::GdiplusShutdown(gpToken);
        return -1;
    }
    CWindow owner(hOwner);
    {
        CString m;
        m.Format(_T("[CaptureMode] owner created hwnd=%p\n"), hOwner);
        DebugLog(m);
    }
    owner.SetWindowPos(NULL, -32000, -32000, kCanvasW, kCanvasH,
                       SWP_NOZORDER | SWP_NOACTIVATE);

    DuiHost host;
    RECT rcClient = { 0, 0, kCanvasW, kCanvasH };
    host.Create(owner.m_hWnd, rcClient, nullptr,
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);

    GetMarks().clear();
    auto content = buildContent();   // 这里会注册各个 Mark
    host.SetRoot(std::move(content));

    // pump messages 6 轮 — 让 HWND-hosted 子窗口（若有）完成自身 paint。
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

    HBITMAP fullDib = nullptr;
    HDC fullDC = SnapshotHostToDib(host.m_hWnd,
                                   host.GetBackBufferDC(),
                                   kCanvasW, kCanvasH, fullDib);
    int totalSaved = 0;
    if (fullDC)
    {
        for (const auto& mark : GetMarks())
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
            path.Format(_T("%s\\%s-%s.png"),
                        outDir, filePrefix, (LPCTSTR)mark.name);
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
