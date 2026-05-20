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

DuiAnimMgr& DuiAnimMgr::Inst()
{
    static DuiAnimMgr s_inst;
    return s_inst;
}

void DuiAnimMgr::Add(std::unique_ptr<DuiAnim> a)
{
    if (a)
    {
        m_active.push_back(std::move(a));
    }
}

void DuiAnimMgr::TickAll(unsigned long nowMs)
{
    // Walk in reverse so we can erase in-place.
    for (int i = (int)m_active.size() - 1; i >= 0; --i)
    {
        if (!m_active[i]->Tick(nowMs))
        {
            m_active.erase(m_active.begin() + i);
        }
    }
}

void DuiAnimMgr::Clear()
{
    m_active.clear();
}

} // namespace balloonwjui
