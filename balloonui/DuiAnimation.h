#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiAnim / DuiAnimMgr —— 轻量动画框架
// =================================================================
//
// 用途：让 DUI 控件做 hover 渐变、popup 滑入、滚动平滑、GIF 帧步进等
// 短时长属性 tween。两部分：
//   · DuiAnimMgr：进程单例，自带一个约 60Hz 的线程定时器
//     ::SetTimer(NULL, 0, 10, PulseProc)（请求 10ms，实际随系统定时器节拍约
//     15.6ms 触发一次，取值理由见 DuiAnimation.cpp 的 kAnimPulseIntervalMs）。维护一组活跃 DuiAnim，每次
//     pulse 都 tick 一遍。定时器<u>按需</u>存在：Add 让活跃列表从空变
//     非空时装上，列表清空（全部完成或 Clear）时立刻卸掉，空闲期不会
//     有定时器长期挂着。
//   · DuiAnim：单个 tween 的基类。持有 duration / start time /
//     easing function / 完成回调；子类覆写 OnTick(double t01) 应用进度
//     到具体目标属性（颜色 / opacity / scroll pos / rect ...）。
//
// 故意<u>不</u>用 per-control timer —— 一个进程级 timer 开销小得多，
// 60Hz 对 hover 渐变 / popup 滑入 / 滚动平滑都够用。
//
// 选线程定时器而不是隐藏窗口（message-only window）的原因：不需要注册
// 窗口类、不需要 WndProc，进程退出时静态析构里调 ::KillTimer 即使线程
// 已经结束也只是返回 FALSE，不会崩溃。代价是线程亲和性 —— 定时器归属
// 于第一次 Add 的那个线程，该线程必须泵消息。UI 线程都满足这一点：
// 主消息循环与客户端各处手写的模态循环都用不带窗口过滤的 ::GetMessage
// 加无条件 ::DispatchMessage，而线程定时器的 WM_TIMER 其 hwnd 为 NULL，
// DispatchMessage 会直接回调 PulseProc。
//
// TickAll 仍然是 public 的，主要供单元测试手动驱动（无 HWND、无消息循环
// 也能逐帧推进并断言中间值）；宿主窗口若出于历史原因仍有自己的 60Hz
// pulse，继续调用也不受影响。同一帧被 tick 两次无副作用 —— DuiAnim 按
// 绝对时间算进度（t = (nowMs - startMs) / durationMs），重复传同一个
// nowMs 得到同一个 t。
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

// Process-wide animation scheduler. Owns every running DuiAnim and drives
// them from a single ~60Hz thread timer (requested at 10ms, fires on every
// ~15.6ms system timer tick) that only exists while there is something to
// animate (see IsSelfDriving).
class BUI_API DuiAnimMgr
{
public:
    static DuiAnimMgr& Inst();

    // Add an animation. Manager takes ownership; deletes when complete.
    // Going from "no animation" to "one animation" installs the shared
    // pulse timer, so the caller does not need any timer of its own.
    // Adding from inside a completion callback (DuiToast chains fade-in ->
    // hold -> fade-out that way) is safe: the new anim is parked in a
    // pending list and merged when the current TickAll walk finishes.
    // A null pointer is ignored.
    void  Add(std::unique_ptr<DuiAnim> a);

    // Number of animations the manager holds - the ones being ticked plus
    // the ones queued from inside the current tick.
    int   GetActiveCount() const
    {
        return (int)(m_active.size() + m_pending.size());
    }

    // Drive all active anims at nowMs (a monotonic ms clock, normally
    // ::GetTickCount). Removes finished ones and drops the pulse timer
    // once nothing is left. Public so tests, and hosts that already run
    // their own pulse, can drive it directly. Calling it repeatedly with
    // the same nowMs has no extra effect because progress is computed
    // from absolute time. A nested call from a completion callback is
    // ignored - the outer walk owns the active list.
    void  TickAll(unsigned long nowMs);

    // Cancel all queued animations without firing their OnComplete, and
    // drop the pulse timer. Called from a completion callback it is
    // deferred until the current TickAll walk finishes; anims the same
    // callback queued *before* the Clear are cancelled with the rest,
    // anims queued *after* it start a fresh round and survive.
    void  Clear();

    // True while the shared pulse timer is installed, i.e. while the
    // manager advances its animations on its own. False when idle.
    bool  IsSelfDriving() const { return m_pulseTimerId != 0; }

    // Id of the shared pulse timer, 0 when idle. Exposed for diagnostics
    // and tests: adding several animations in a row must not change it,
    // which is how "one timer per process, not one per animation" is
    // asserted. Not meant to be passed to ::KillTimer by callers.
    UINT_PTR GetPulseTimerId() const { return m_pulseTimerId; }

private:
    DuiAnimMgr() = default;
    ~DuiAnimMgr();
    DuiAnimMgr(const DuiAnimMgr&) = delete;
    DuiAnimMgr& operator=(const DuiAnimMgr&) = delete;

    // Thread-timer callback: ticks every active anim at the current
    // ::GetTickCount. Static because ::SetTimer wants a plain function.
    static void CALLBACK PulseProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD tick);

    // Install / drop the shared pulse timer. Both are idempotent.
    void  StartPulse();
    void  StopPulse();

    // 正在被 TickAll 遍历的动画，管理器持有所有权，完成后立即删除。
    std::vector<std::unique_ptr<DuiAnim>> m_active;

    // TickAll 遍历期间由完成回调新加进来的动画，遍历结束后合并进
    // m_active。单独放一份是因为直接 push_back 到 m_active 可能触发
    // vector 重新分配，而外层循环正拿着它的下标。
    std::vector<std::unique_ptr<DuiAnim>> m_pending;

    // 共享脉冲定时器的 id，0 表示当前没有自驱动的定时器。
    // 由 StartPulse / StopPulse 维护，生命周期与活跃动画一致。
    UINT_PTR m_pulseTimerId = 0;

    // 是否正处于 TickAll 的遍历过程中。为 true 时 Add / Clear / 嵌套的
    // TickAll 都改走延后路径，避免在遍历中改动 m_active。
    bool     m_ticking = false;

    // 遍历期间收到过 Clear 请求。遍历结束后统一清空并卸掉定时器。
    bool     m_clearRequested = false;
};

} // namespace balloonwjui
