#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiAnim / DuiAnimMgr —— 轻量动画框架
// =================================================================
//
// 用途：让 DUI 控件做 hover 渐变、popup 滑入、滚动平滑、GIF 帧步进等
// 短时长属性 tween。两部分：
//   · DuiAnimMgr：进程单例，由<u>一个</u> 16ms ::SetTimer 在隐藏 HWND
//     上驱动。维护一组活跃 DuiAnim，每次 WM_TIMER pulse 都 tick。
//   · DuiAnim：单个 tween 的基类。持有 duration / start time /
//     easing function / 完成回调；子类覆写 OnTick(double t01) 应用进度
//     到具体目标属性（颜色 / opacity / scroll pos / rect ...）。
//
// 故意<u>不</u>用 per-control timer —— 一个进程级 timer 开销小得多，
// 60Hz 对 hover 渐变 / popup 滑入 / 滚动平滑都够用。
//
// 纯函数（Easing 系列、Tick helper）可脱离 HWND 单测，缓动曲线能离线
// 验证。
//
// 代码用法：
//
//     // 1) 只用 easing 函数（不需要 manager）：
//     double v = balloonwjui::DuiEase::EaseOutCubic(0.5);
//
//     // 2) 200ms 内把属性从 0.0 渐变到 1.0：
//     auto anim = std::make_unique<balloonwjui::DuiDoubleAnim>(
//         /*durationMs=*/200, 0.0, 1.0,
//         [this](double v) { m_alpha = v; Invalidate(); });
//     anim->SetEasing(&balloonwjui::DuiEase::EaseOutCubic);
//     anim->SetOnComplete([this]() { m_alpha = 1.0; });
//     balloonwjui::DuiAnimMgr::Inst().Add(std::move(anim));
//
//     // 3) 单测里手驱动（无 HWND）：
//     balloonwjui::DuiAnimMgr::Inst().TickAll(currentTickMs);
//
// XML 用法：N/A（运行时 API，与静态布局无关）。

#include <windows.h>
#include <functional>
#include <vector>
#include <memory>
#include "BalloonUiApi.h"

namespace balloonwjui {

// Usage:
//   // 1) Easing functions only (no manager needed):
//   double v = balloonwjui::DuiEase::EaseOutCubic(0.5);
//
//   // 2) Drive a property tween from value 0.0 -> 1.0 over 200ms:
//   auto anim = std::make_unique<balloonwjui::DuiDoubleAnim>(
//       /*durationMs=*/200, 0.0, 1.0,
//       [this](double v) { m_alpha = v; Invalidate(); });
//   anim->SetEasing(&balloonwjui::DuiEase::EaseOutCubic);
//   anim->SetOnComplete([this]() { m_alpha = 1.0; });
//   balloonwjui::DuiAnimMgr::Inst().Add(std::move(anim));
//
//   // 3) From a unit test (no HWND):
//   balloonwjui::DuiAnimMgr::Inst().TickAll(currentTickMs);

namespace DuiEase
{
    // All easing functions take t in [0,1] and return progress in [0,1].
    // t=0 → 0, t=1 → 1 (exact). Pure functions.
    double Linear     (double t);
    double EaseInQuad (double t);
    double EaseOutQuad(double t);
    double EaseInOutQuad(double t);
    double EaseInCubic (double t);
    double EaseOutCubic(double t);
    double EaseInOutCubic(double t);
}

class BUI_API DuiAnim
{
public:
    typedef std::function<double(double)> EasingFn;

    explicit DuiAnim(int durationMs);
    virtual ~DuiAnim() = default;

    void  SetEasing(EasingFn fn) { m_easing = fn; }
    void  SetOnComplete(std::function<void()> fn) { m_onDone = fn; }
    int   GetDurationMs() const { return m_durationMs; }

    // Drive one frame. nowMs is a monotonic time source (process-uptime
    // in ms via GetTickCount). Returns true while the anim is still
    // running; false when it has completed (caller should remove it).
    //
    // First call records start time; subsequent calls compute t01 =
    // (nowMs - startMs) / durationMs clamped to [0,1] and call
    // OnTick(eased(t01)).
    bool  Tick(unsigned long nowMs);

    // For tests: advance state without timing. Equivalent to Tick when
    // the elapsed time has been computed externally.
    bool  TickWithElapsed(int elapsedMs);

    // Force-complete: tick at t=1.0 then mark done + fire OnComplete.
    void  Finish();

    bool  IsRunning() const { return m_started && !m_done; }
    bool  IsDone()    const { return m_done; }

protected:
    // Subclass override: apply animation progress (eased value) to its
    // target property. tEased is in [0, 1].
    virtual void OnTick(double tEased) = 0;

private:
    int                    m_durationMs;
    unsigned long          m_startMs   = 0;
    bool                   m_started   = false;
    bool                   m_done      = false;
    EasingFn               m_easing;          // null = Linear
    std::function<void()>  m_onDone;
};

// Simple double-property tween: fires a setter on each tick with the
// current eased value mapped to [from, to].
class BUI_API DuiDoubleAnim : public DuiAnim
{
public:
    typedef std::function<void(double)> SetterFn;

    DuiDoubleAnim(int durationMs, double from, double to, SetterFn setter)
        : DuiAnim(durationMs), m_from(from), m_to(to), m_setter(setter)
    {
    }

    double Current() const { return m_current; }
    double From()    const { return m_from; }
    double To()      const { return m_to; }

protected:
    void OnTick(double t) override
    {
        m_current = m_from + (m_to - m_from) * t;
        if (m_setter)
        {
            m_setter(m_current);
        }
    }

private:
    double   m_from;
    double   m_to;
    double   m_current = 0.0;
    SetterFn m_setter;
};

class BUI_API DuiAnimMgr
{
public:
    static DuiAnimMgr& Inst();

    // Add an animation. Manager takes ownership; deletes when complete.
    void  Add(std::unique_ptr<DuiAnim> a);

    // Number of active animations (after pending removals).
    int   GetActiveCount() const { return (int)m_active.size(); }

    // Drive all active anims at nowMs. Removes finished ones.
    // Public so tests can drive without a real WM_TIMER.
    void  TickAll(unsigned long nowMs);

    // Cancel all queued animations without firing their OnComplete.
    void  Clear();

private:
    DuiAnimMgr() = default;
    ~DuiAnimMgr() = default;
    DuiAnimMgr(const DuiAnimMgr&) = delete;
    DuiAnimMgr& operator=(const DuiAnimMgr&) = delete;

    std::vector<std::unique_ptr<DuiAnim>> m_active;
};

} // namespace balloonwjui
