#include "stdafx.h"
#include "DuiDpi.h"

namespace balloonwjui {

namespace DuiDpi {

int GetSystemDpi()
{
    HDC hdc = ::GetDC(nullptr);
    if (!hdc)
    {
        return kDefaultDpi;
    }
    int dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
    ::ReleaseDC(nullptr, hdc);
    return dpi > 0 ? dpi : kDefaultDpi;
}

int GetWindowDpi(HWND hwnd)
{
    if (!hwnd)
    {
        return GetSystemDpi();
    }

    // GetDpiForWindow exists on Windows 10 1607+. Resolve dynamically so
    // the binary still loads on older systems.
    typedef UINT (WINAPI *FN_GetDpiForWindow)(HWND);
    static FN_GetDpiForWindow s_pGetDpi = nullptr;
    static bool s_resolved = false;
    if (!s_resolved)
    {
        s_resolved = true;
        HMODULE u = ::GetModuleHandle(_T("user32.dll"));
        if (u)
        {
            s_pGetDpi = (FN_GetDpiForWindow)::GetProcAddress(u, "GetDpiForWindow");
        }
    }
    if (s_pGetDpi)
    {
        UINT d = s_pGetDpi(hwnd);
        if (d > 0)
        {
            return (int)d;
        }
    }
    return GetSystemDpi();
}

int GetDpiForPoint(POINT screenPt)
{
    // GetDpiForMonitor 在 shcore.dll 里（Windows 8.1+）。动态解析，老系统上
    // 二进制照样能加载。第二个参数 0 即 MDT_EFFECTIVE_DPI。
    typedef HRESULT (WINAPI *FN_GetDpiForMonitor)(HMONITOR, int, UINT*, UINT*);
    const int kEffectiveDpi = 0;
    static FN_GetDpiForMonitor s_pGetDpi = nullptr;
    static bool s_resolved = false;
    if (!s_resolved)
    {
        s_resolved = true;
        HMODULE shcore = ::LoadLibrary(_T("shcore.dll"));
        if (shcore)
        {
            s_pGetDpi = (FN_GetDpiForMonitor)::GetProcAddress(shcore, "GetDpiForMonitor");
        }
    }
    if (s_pGetDpi)
    {
        HMONITOR mon = ::MonitorFromPoint(screenPt, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 0;
        UINT dpiY = 0;
        if (mon && SUCCEEDED(s_pGetDpi(mon, kEffectiveDpi, &dpiX, &dpiY)) && dpiX > 0)
        {
            return (int)dpiX;
        }
    }
    return GetSystemDpi();
}

bool OptInPerMonitorV2()
{
    // SetProcessDpiAwarenessContext + DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2:
    // Windows 10 1703+. Resolve dynamically so older systems don't fail.
    typedef BOOL (WINAPI *FN_SetCtx)(HANDLE);
    HMODULE u = ::GetModuleHandle(_T("user32.dll"));
    if (!u)
    {
        return true;
    }
    auto fn = (FN_SetCtx)::GetProcAddress(u, "SetProcessDpiAwarenessContext");
    if (!fn)
    {
        return true;          // older OS: caller shouldn't treat as error
    }
    // -4 == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 sentinel.
    HANDLE ctx = (HANDLE)(LONG_PTR)-4;
    return fn(ctx) != FALSE;
}

} // namespace DuiDpi

} // namespace balloonwjui
