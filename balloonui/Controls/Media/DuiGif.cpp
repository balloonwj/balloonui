#include "stdafx.h"
#include "DuiGif.h"

#if BUI_FEATURE_GIF

#include "../../DuiAnimation.h"
#include <gdiplus.h>

namespace balloonwjui {

#pragma comment(lib, "gdiplus.lib")

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

} // namespace

// =====================================================================
// DuiGif
// =====================================================================

DuiGif::DuiGif() = default;

DuiGif::~DuiGif()
{
    Clear();
}

void DuiGif::Clear()
{
    for (HBITMAP h : m_frames)
    {
        if (h)
        {
            ::DeleteObject(h);
        }
    }
    m_frames.clear();
    m_delaysMs.clear();
    m_w = m_h = 0;
}

bool DuiGif::LoadFromFile(LPCTSTR path)
{
    Clear();
    if (!path || !*path)
    {
        return false;
    }
    EnsureGdiplus();

    Gdiplus::Bitmap bm(path);
    if (bm.GetLastStatus() != Gdiplus::Ok)
    {
        return false;
    }

    m_w = (int)bm.GetWidth();
    m_h = (int)bm.GetHeight();
    if (m_w <= 0 || m_h <= 0)
    {
        return false;
    }

    UINT count = bm.GetFrameDimensionsCount();
    if (count == 0)
    {
        return false;
    }
    std::vector<GUID> dims(count);
    bm.GetFrameDimensionsList(dims.data(), count);

    UINT frameCount = bm.GetFrameCount(&dims[0]);
    if (frameCount == 0)
    {
        return false;
    }

    // Frame delays via PropertyTagFrameDelay (LONG[] in 10ms units).
    UINT propSize = bm.GetPropertyItemSize(PropertyTagFrameDelay);
    Gdiplus::PropertyItem* prop = nullptr;
    if (propSize > 0)
    {
        prop = (Gdiplus::PropertyItem*)::malloc(propSize);
        if (prop && bm.GetPropertyItem(PropertyTagFrameDelay, propSize, prop) != Gdiplus::Ok)
        {
            ::free(prop);
            prop = nullptr;
        }
    }

    m_frames.reserve(frameCount);
    m_delaysMs.reserve(frameCount);

    for (UINT i = 0; i < frameCount; ++i)
    {
        bm.SelectActiveFrame(&dims[0], i);
        HBITMAP hbm = nullptr;
        if (bm.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbm) != Gdiplus::Ok || !hbm)
        {
            // Stop on first failed frame; previous frames already cached.
            break;
        }
        m_frames.push_back(hbm);

        int delay = 0;
        if (prop && i < (prop->length / sizeof(LONG)))
        {
            LONG v = ((const LONG*)prop->value)[i];
            delay = (int)v * 10;       // 10ms units -> ms
        }
        // Browsers / image decoders treat 0 as 100ms.
        if (delay <= 0)
        {
            delay = 100;
        }
        m_delaysMs.push_back(delay);
    }

    if (prop)
    {
        ::free(prop);
    }
    return !m_frames.empty();
}

int DuiGif::GetFrameDelayMs(int idx) const
{
    if (idx < 0 || idx >= (int)m_delaysMs.size())
    {
        return 0;
    }
    return m_delaysMs[idx];
}

int DuiGif::GetTotalDurationMs() const
{
    int sum = 0;
    for (int d : m_delaysMs)
    {
        sum += d;
    }
    return sum;
}

HBITMAP DuiGif::GetFrameHbitmap(int idx) const
{
    if (idx < 0 || idx >= (int)m_frames.size())
    {
        return nullptr;
    }
    return m_frames[idx];
}

int DuiGif::FrameAt(unsigned long elapsedMs, const int* delaysMs, int count)
{
    if (!delaysMs || count <= 0)
    {
        return 0;
    }

    // Total duration; loop within it.
    long total = 0;
    for (int i = 0; i < count; ++i)
    {
        total += (delaysMs[i] > 0 ? delaysMs[i] : 0);
    }
    if (total <= 0)
    {
        return 0;
    }

    long t = (long)(elapsedMs % (unsigned long)total);
    long acc = 0;
    for (int i = 0; i < count; ++i)
    {
        acc += (delaysMs[i] > 0 ? delaysMs[i] : 0);
        if (t < acc)
        {
            return i;
        }
    }
    return count - 1;     // shouldn't reach with positive total, but safe.
}

int DuiGif::FrameAt(unsigned long elapsedMs) const
{
    if (m_delaysMs.empty())
    {
        return 0;
    }
    return FrameAt(elapsedMs, m_delaysMs.data(), (int)m_delaysMs.size());
}

// =====================================================================
// DuiGifControl
// =====================================================================

namespace {

// One animation that ticks the control's frame index every frame. We
// don't actually use eased values — the gif's own per-frame delays do
// the timing. We just need a heartbeat that drives DuiGifControl::Tick
// at ~60Hz. Implementation: a long-duration anim that's a no-op until
// completed; the manager keeps it alive while we re-read elapsed.
//
// Simpler: bypass DuiAnim entirely and use a SetTimer on the host. But
// per the framework principle ("avoid per-control timers"), tie into
// the existing DuiAnimMgr cadence instead — schedule a long anim and
// have its tick callback advance our gif state.
class GifTickAnim : public DuiAnim
{
public:
    GifTickAnim(DuiGifControl* owner)
        : DuiAnim(60 * 60 * 1000)   // 1 hour — well beyond any single gif loop
        , m_owner(owner)
    {
    }
    DuiGifControl* m_owner;
protected:
    void OnTick(double /*t*/) override;
};

} // anonymous

DuiGifControl::DuiGifControl() = default;
DuiGifControl::~DuiGifControl()
{
    Stop();
}

void DuiGifControl::SetGif(std::unique_ptr<DuiGif> gif)
{
    m_gif = std::move(gif);
    m_frameIdx = 0;
    Invalidate();
}

void DuiGifControl::Start()
{
    if (m_running || !m_gif || m_gif->GetFrameCount() == 0)
    {
        return;
    }
    m_running = true;
    m_startMs = ::GetTickCount();

    // Park a tick anim; DuiAnimMgr drives it ~60Hz when its TickAll is
    // called. The DuiGallery harness calls TickAll from a timer on the
    // gallery frame.
    auto a = std::unique_ptr<GifTickAnim>(new GifTickAnim(this));
    DuiAnimMgr::Inst().Add(std::move(a));
}

void DuiGifControl::Stop()
{
    m_running = false;
    // The parked anim self-times-out after an hour; we don't bother
    // hunting it down. A future DuiAnimMgr::Cancel(token) would be the
    // clean answer.
}

void DuiGifControl::SetFrameIndex(int idx)
{
    if (!m_gif)
    {
        return;
    }
    int n = m_gif->GetFrameCount();
    if (n <= 0)
    {
        return;
    }
    if (idx < 0)
    {
        idx = 0;
    }
    if (idx >= n)
    {
        idx = n - 1;
    }
    if (m_frameIdx == idx)
    {
        return;
    }
    m_frameIdx = idx;
    Invalidate();
}

void DuiGifControl::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible || !m_gif)
    {
        return;
    }
    HBITMAP hbm = m_gif->GetFrameHbitmap(m_frameIdx);
    if (!hbm)
    {
        return;
    }

    RECT inter;
    if (!::IntersectRect(&inter, &m_rcItem, &rcDirty))
    {
        return;
    }

    HDC mem = ::CreateCompatibleDC(hdc);
    HGDIOBJ old = ::SelectObject(mem, hbm);

    int gw = m_gif->GetWidth();
    int gh = m_gif->GetHeight();
    int rw = m_rcItem.right - m_rcItem.left;
    int rh = m_rcItem.bottom - m_rcItem.top;

    if (m_stretch)
    {
        ::SetStretchBltMode(hdc, HALFTONE);
        ::SetBrushOrgEx(hdc, 0, 0, nullptr);
        ::StretchBlt(hdc,
                     m_rcItem.left, m_rcItem.top, rw, rh,
                     mem, 0, 0, gw, gh, SRCCOPY);
    }
    else
    {
        // Center 1:1.
        int x = m_rcItem.left + (rw - gw) / 2;
        int y = m_rcItem.top  + (rh - gh) / 2;
        ::BitBlt(hdc, x, y, gw, gh, mem, 0, 0, SRCCOPY);
    }

    ::SelectObject(mem, old);
    ::DeleteDC(mem);
}

SIZE DuiGifControl::GetDesiredSize() const
{
    if (!m_gif)
    {
        return SIZE{ 0, 0 };
    }
    return SIZE{ m_gif->GetWidth(), m_gif->GetHeight() };
}

void GifTickAnim::OnTick(double /*t*/)
{
    if (!m_owner || !m_owner->IsRunning())
    {
        return;
    }
    DuiGif* g = m_owner->GetGif();
    if (!g || g->GetFrameCount() == 0)
    {
        return;
    }
    unsigned long now = ::GetTickCount();
    unsigned long elapsed = now;     // GetTickCount wraps every ~49 days, fine
    int idx = g->FrameAt(elapsed);
    m_owner->SetFrameIndex(idx);
}

} // namespace balloonwjui

#endif // BUI_FEATURE_GIF
