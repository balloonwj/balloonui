#include "stdafx.h"
#include "DuiAsyncImage.h"
#include <gdiplus.h>
#include <vector>
#include <queue>
#include <process.h>

#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

namespace DuiAsyncImage {

namespace {

// =====================================================================
// Worker side: a single background thread that pulls requests off a
// shared queue and decodes PNGs via GDI+.
// =====================================================================

struct Request
{
    DWORD_PTR id;
    DWORD_PTR userToken;
    CString   path;
    Callback  cb;
    DWORD     ownerThreadId;   // thread that submitted (callback target)
    HWND      ownerWnd;        // message-only window of submitting thread
    bool      cancelled;
};

struct CompletedItem
{
    Result    res;
    Callback  cb;
};

// Process-wide worker state. Initialized lazily on first Submit.
struct State
{
    HANDLE             thread       = nullptr;
    HANDLE             wakeEvent    = nullptr;
    HANDLE             stopEvent    = nullptr;
    CRITICAL_SECTION   cs;
    std::queue<Request> queue;
    DWORD_PTR          nextId       = 1;
    ULONG_PTR          gdipToken    = 0;
    bool               initialized  = false;
};

State& GS()
{
    static State s;
    return s;
}

// Per-thread message-only window class for delivering completion
// callbacks back to the originating thread. The class is registered
// once; each submitting thread owns its own HWND.
const UINT WM_DUIASYNC_DONE = WM_USER + 0x4321;
LPCTSTR    kClsName         = _T("__DuiAsyncImageDeliver__");
ATOM       g_classAtom      = 0;

LRESULT CALLBACK DeliverProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_DUIASYNC_DONE)
    {
        CompletedItem* item = (CompletedItem*)lp;
        if (item)
        {
            if (item->cb)
            {
                item->cb(item->res);
            }
            delete item;
        }
        return 0;
    }
    return ::DefWindowProc(h, m, wp, lp);
}

void EnsureClass()
{
    if (g_classAtom)
    {
        return;
    }
    WNDCLASS wc = {};
    wc.lpfnWndProc   = DeliverProc;
    wc.hInstance     = ::GetModuleHandle(nullptr);
    wc.lpszClassName = kClsName;
    g_classAtom      = ::RegisterClass(&wc);
}

// Per-thread cache of message-only windows.
__declspec(thread) HWND tls_deliverWnd = nullptr;

HWND EnsureDeliverWnd()
{
    if (tls_deliverWnd)
    {
        return tls_deliverWnd;
    }
    EnsureClass();
    tls_deliverWnd = ::CreateWindow(kClsName, _T(""), 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, nullptr,
                                    ::GetModuleHandle(nullptr), nullptr);
    return tls_deliverWnd;
}

// Decode a single file into an HBITMAP via GDI+. Returns null on
// failure (missing file, unsupported format, OOM). HBITMAP returned
// is a DDB; caller owns + DeleteObject.
HBITMAP DecodeFile(LPCTSTR path, int& w, int& h)
{
    w = 0;
    h = 0;
    Gdiplus::Bitmap bm(path);
    if (bm.GetLastStatus() != Gdiplus::Ok)
    {
        return nullptr;
    }
    HBITMAP hbm = nullptr;
    if (bm.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbm) != Gdiplus::Ok || !hbm)
    {
        return nullptr;
    }
    w = (int)bm.GetWidth();
    h = (int)bm.GetHeight();
    return hbm;
}

unsigned __stdcall WorkerThread(void*)
{
    HANDLE waits[2] = { GS().wakeEvent, GS().stopEvent };
    while (true)
    {
        DWORD r = ::WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (r == WAIT_OBJECT_0 + 1)
        {
            break;     // stop
        }
        if (r != WAIT_OBJECT_0)
        {
            continue;
        }

        // Drain queue.
        for (;;)
        {
            Request req;
            bool have = false;
            ::EnterCriticalSection(&GS().cs);
            if (!GS().queue.empty())
            {
                req = std::move(GS().queue.front());
                GS().queue.pop();
                have = true;
            }
            ::LeaveCriticalSection(&GS().cs);
            if (!have)
            {
                break;
            }
            if (req.cancelled)
            {
                continue;
            }

            int w = 0, h = 0;
            HBITMAP hbm = DecodeFile(req.path, w, h);

            CompletedItem* item = new CompletedItem();
            item->cb         = req.cb;
            item->res.ok     = (hbm != nullptr);
            item->res.hbm    = hbm;
            item->res.width  = w;
            item->res.height = h;
            item->res.userToken = req.userToken;

            // Post to owning thread's deliver window.
            if (req.ownerWnd && ::IsWindow(req.ownerWnd))
            {
                ::PostMessage(req.ownerWnd, WM_DUIASYNC_DONE, 0, (LPARAM)item);
            }
            else
            {
                // No deliver window — call inline (worker thread).
                if (item->cb)
                {
                    item->cb(item->res);
                }
                delete item;
            }
        }
    }
    return 0;
}

void EnsureInit()
{
    if (GS().initialized)
    {
        return;
    }
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&GS().gdipToken, &input, nullptr);
    ::InitializeCriticalSection(&GS().cs);
    GS().wakeEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    GS().stopEvent = ::CreateEvent(nullptr, TRUE,  FALSE, nullptr);
    GS().thread = (HANDLE)::_beginthreadex(nullptr, 0, &WorkerThread, nullptr, 0, nullptr);
    GS().initialized = true;
}

} // anonymous

DWORD_PTR Submit(LPCTSTR path, const Callback& cb, DWORD_PTR userToken)
{
    if (!path || !cb)
    {
        return 0;
    }
    EnsureInit();
    HWND deliver = EnsureDeliverWnd();

    Request req;
    req.id            = GS().nextId++;
    req.userToken     = userToken;
    req.path          = path;
    req.cb            = cb;
    req.ownerThreadId = ::GetCurrentThreadId();
    req.ownerWnd      = deliver;
    req.cancelled     = false;

    DWORD_PTR id = req.id;
    ::EnterCriticalSection(&GS().cs);
    GS().queue.push(std::move(req));
    ::LeaveCriticalSection(&GS().cs);
    ::SetEvent(GS().wakeEvent);
    return id;
}

void Cancel(DWORD_PTR requestId)
{
    if (!GS().initialized)
    {
        return;
    }
    ::EnterCriticalSection(&GS().cs);
    // Walk queue: if found, mark cancelled. Worker checks the flag.
    // std::queue lacks an iterator API so rebuild the queue.
    std::queue<Request> q2;
    while (!GS().queue.empty())
    {
        Request r = std::move(GS().queue.front());
        GS().queue.pop();
        if (r.id == requestId)
        {
            r.cancelled = true;
        }
        q2.push(std::move(r));
    }
    GS().queue = std::move(q2);
    ::LeaveCriticalSection(&GS().cs);
}

void PumpForTests()
{
    HWND w = tls_deliverWnd;
    if (!w)
    {
        return;
    }
    MSG m;
    while (::PeekMessage(&m, w, 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&m);
        ::DispatchMessage(&m);
    }
}

void RunSyncForTests(LPCTSTR path, DWORD_PTR userToken, const Callback& cb)
{
    if (!path || !cb)
    {
        return;
    }
    EnsureInit();
    int w = 0, h = 0;
    HBITMAP hbm = DecodeFile(path, w, h);
    Result r;
    r.ok = (hbm != nullptr);
    r.hbm = hbm;
    r.width = w;
    r.height = h;
    r.userToken = userToken;
    cb(r);
}

} // namespace DuiAsyncImage

} // namespace balloonwjui
