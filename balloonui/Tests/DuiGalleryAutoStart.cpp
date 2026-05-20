// DUI Gallery auto-launcher (Debug-only).
//
// Avoids modifying any existing source file (which may be GBK-encoded).
// Strategy: install a thread-local CBT hook at static-init time. The hook
// fires for every window creation; on the first WM_INITDIALOG-like event
// it pops the gallery as a top-level window, then uninstalls itself.
//
// Active only in _DEBUG builds. Has no effect in Release.

#include "stdafx.h"
#include "../BalloonUiFeatures.h"

#if defined(_DEBUG) && defined(BUI_FEATURE_GALLERY)

#include "DuiGalleryDlg.h"

namespace balloonwjui {

namespace {

HHOOK    g_hCbtHook = NULL;
bool     g_bGalleryShown = false;

LRESULT CALLBACK CbtProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HCBT_ACTIVATE && !g_bGalleryShown)
    {
        // First top-level window has been activated. The message loop
        // is running, GDI is up, CSkinManager is initialized. Safe to
        // pop the gallery.
        g_bGalleryShown = true;

        // Heap-allocate; leak on purpose - lives until process exit.
        DuiGalleryDlg* dlg = new DuiGalleryDlg();
        dlg->ShowGallery(NULL);

        // Uninstall ourselves; we only fire once.
        if (g_hCbtHook != NULL)
        {
            ::UnhookWindowsHookEx(g_hCbtHook);
            g_hCbtHook = NULL;
        }
    }
    return ::CallNextHookEx(g_hCbtHook, nCode, wParam, lParam);
}

struct AutoInstaller
{
    AutoInstaller()
    {
        // WH_CBT thread-local hook on the current (main) thread.
        // Static init runs before _tWinMain, but Windows hooks bound
        // to a thread queue activate as soon as the message loop spins.
        g_hCbtHook = ::SetWindowsHookEx(
            WH_CBT,
            CbtProc,
            NULL,
            ::GetCurrentThreadId());
    }
    ~AutoInstaller()
    {
        if (g_hCbtHook != NULL)
        {
            ::UnhookWindowsHookEx(g_hCbtHook);
            g_hCbtHook = NULL;
        }
    }
};

// Static instance triggers SetWindowsHookEx during DLL/EXE load.
AutoInstaller g_autoInstaller;

} // namespace

} // namespace balloonwjui

#endif // _DEBUG
