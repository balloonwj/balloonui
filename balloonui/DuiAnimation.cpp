#include "stdafx.h"
#include "DuiAnimation.h"

namespace balloonwjui {

namespace DuiEase {

double Linear(double t)
{
    return t;
}

double EaseInQuad(double t)
{
    return t * t;
}

double EaseOutQuad(double t)
{
    double inv = 1.0 - t;
    return 1.0 - inv * inv;
}

double EaseInOutQuad(double t)
{
    if (t < 0.5)
    {
        return 2.0 * t * t;
    }
    double inv = 1.0 - t;
    return 1.0 - 2.0 * inv * inv;
}

double EaseInCubic(double t)
{
    return t * t * t;
}

double EaseOutCubic(double t)
{
    double inv = 1.0 - t;
    return 1.0 - inv * inv * inv;
}

double EaseInOutCubic(double t)
{
    if (t < 0.5)
    {
        return 4.0 * t * t * t;
    }
    double inv = 1.0 - t;
    return 1.0 - 4.0 * inv * inv * inv;
}

} // namespace DuiEase

// =====================================================================
// DuiAnim
// =====================================================================

DuiAnim::DuiAnim(int durationMs)
    : m_durationMs(durationMs < 1 ? 1 : durationMs)
{
}

bool DuiAnim::Tick(unsigned long nowMs)
{
    if (m_done)
    {
        return false;
    }
    if (!m_started)
    {
        m_started = true;
        m_startMs = nowMs;
    }
    int elapsed = (int)(nowMs - m_startMs);
    return TickWithElapsed(elapsed);
}

bool DuiAnim::TickWithElapsed(int elapsedMs)
{
    if (m_done)
    {
        return false;
    }
    if (!m_started)
    {
        m_started = true;
        m_startMs = 0;     // synthetic; nowMs path won't be used
    }

    if (elapsedMs < 0)
    {
        elapsedMs = 0;
    }
    double t = (double)elapsedMs / (double)m_durationMs;
    bool last = (t >= 1.0);
    if (last)
    {
        t = 1.0;
    }

    double eased = m_easing ? m_easing(t) : DuiEase::Linear(t);
    OnTick(eased);

    if (last)
    {
        m_done = true;
        if (m_onDone)
        {
            m_onDone();
        }
        return false;
    }
    return true;
}

void DuiAnim::Finish()
{
    if (m_done)
    {
        return;
    }
    if (!m_started)
    {
        m_started = true;
    }
    OnTick(1.0);
    m_done = true;
    if (m_onDone)
    {
        m_onDone();
    }
}

// =====================================================================
// DuiAnimMgr
// =====================================================================

// 共享脉冲定时器<u>请求</u>的触发间隔，单位毫秒。目标是约 60Hz：hover 渐变 / popup
// 滑入 / 滚动平滑这类 150~300ms 的短动画在 60Hz 下已经看不出台阶。
//
// 取 10 而不是 16（2026-09-30 修改）：系统定时器约 15.6ms 才跳一次，SetTimer 的实际触发
// 时刻会对齐到这个节拍上。请求 16ms 时，16 比一个节拍多出一点点，大多要等到第二个节拍
// （约 31ms）才触发 —— 实测平均间隔 25ms、约 40Hz，滚动动画肉眼可见地发顿。请求 10ms
// 时每个节拍都会触发，实测平均间隔 16.5ms、约 60Hz。10 也是 SetTimer 允许的最小值
// （USER_TIMER_MINIMUM），再小会被系统改回 10。定时器只在有动画时才存在，空闲期不耗电。
static const UINT kAnimPulseIntervalMs = 10;

DuiAnimMgr& DuiAnimMgr::Inst()
{
    static DuiAnimMgr s_inst;
    return s_inst;
}

DuiAnimMgr::~DuiAnimMgr()
{
    // Static destructor at process exit. ::KillTimer on a thread whose
    // message queue is already gone simply returns FALSE, so this is safe
    // even when the UI thread has been torn down before us.
    StopPulse();
}

void DuiAnimMgr::Add(std::unique_ptr<DuiAnim> a)
{
    if (!a)
    {
        return;
    }

    if (m_ticking)
    {
        // Re-entrant add from a completion callback. Pushing into
        // m_active could reallocate the vector TickAll is walking by
        // index, so park it and let TickAll merge it afterwards. That
        // also keeps the new anim out of the current frame, which is what
        // a chained animation wants anyway.
        m_pending.push_back(std::move(a));
        return;
    }

    m_active.push_back(std::move(a));
    StartPulse();
}

void DuiAnimMgr::TickAll(unsigned long nowMs)
{
    if (m_ticking)
    {
        // Nested call from a setter or a completion callback. The outer
        // walk owns m_active; a second walk would tick every anim twice
        // in one frame and, worse, erase entries out from under it.
        return;
    }
    m_ticking = true;

    // Walk in reverse so we can erase in-place. Callbacks fired from
    // Tick() cannot touch m_active while m_ticking is set, so the indices
    // below stay valid for the whole walk.
    for (int i = (int)m_active.size() - 1; i >= 0; --i)
    {
        if (!m_active[i]->Tick(nowMs))
        {
            m_active.erase(m_active.begin() + i);
        }
    }

    m_ticking = false;

    if (m_clearRequested)
    {
        // A callback asked for a full cancel while we were walking. Only
        // m_active is left to drop here: Clear() already emptied m_pending
        // at request time, so anything sitting in it now was queued after
        // the cancel and belongs to the next round.
        m_clearRequested = false;
        m_active.clear();
    }

    for (size_t i = 0; i < m_pending.size(); ++i)
    {
        m_active.push_back(std::move(m_pending[i]));
    }
    m_pending.clear();

    // Drop the timer as soon as there is nothing left to animate, and
    // (re)install it when a callback chained a follow-up animation onto
    // an externally driven tick.
    if (m_active.empty())
    {
        StopPulse();
    }
    else
    {
        StartPulse();
    }
}

void DuiAnimMgr::Clear()
{
    if (m_ticking)
    {
        // Deferred: TickAll is walking m_active and destroying the anim
        // it is currently ticking would pull the ground out from under
        // the callback that asked for the clear. Anims queued earlier in
        // this same tick are cancelled right away; ones queued after this
        // call are a fresh round and survive.
        m_pending.clear();
        m_clearRequested = true;
        return;
    }

    m_pending.clear();
    m_active.clear();
    StopPulse();
}

void CALLBACK DuiAnimMgr::PulseProc(HWND, UINT, UINT_PTR id, DWORD)
{
    DuiAnimMgr& mgr = Inst();
    if (mgr.m_pulseTimerId != id)
    {
        // A WM_TIMER that was already queued when StopPulse ran. Ignore
        // it rather than ticking on behalf of a timer we no longer own.
        return;
    }
    mgr.TickAll(::GetTickCount());
}

void DuiAnimMgr::StartPulse()
{
    if (m_pulseTimerId != 0)
    {
        return;              // one timer per process, never a second one
    }

    // Thread timer: no window, no window class, no WndProc. The WM_TIMER
    // it posts carries hwnd == NULL, and ::DispatchMessage invokes
    // PulseProc directly, so every message loop that pumps unfiltered
    // messages drives it - including the hand-written modal loops in the
    // PC client. Same mechanism DuiToolTipMgr already uses for its
    // hover-delay timer.
    m_pulseTimerId = ::SetTimer(NULL, 0, kAnimPulseIntervalMs, &DuiAnimMgr::PulseProc);
}

void DuiAnimMgr::StopPulse()
{
    if (m_pulseTimerId == 0)
    {
        return;
    }

    ::KillTimer(NULL, m_pulseTimerId);
    m_pulseTimerId = 0;
}

} // namespace balloonwjui
