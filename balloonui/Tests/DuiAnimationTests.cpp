#include "stdafx.h"
#include "DuiAnimationTests.h"
#include <math.h>

namespace balloonwjui {

namespace DuiAnimationTests {

namespace {

struct Result
{
    CString name;
    bool ok;
    CString detail;
};

static Result OK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok = true;
    return r;
}

static Result Fail(const CString& n, const CString& d)
{
    Result r;
    r.name = n;
    r.ok = false;
    r.detail = d;
    return r;
}

#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) return Fail(name, _T("condition false")); } while (0)
#define EXPECT_NEAR(a, b, eps, name) \
    do { double _a = (a); double _b = (b); double _e = (eps); \
         double diff = _a - _b; if (diff < 0) diff = -diff; \
         if (diff > _e) { CString _d; _d.Format(_T("expected=%.4f got=%.4f"), _b, _a); return Fail(name, _d); } \
    } while (0)

// ----- easing endpoints ----------------------------------------------

static Result Test_EasingEndpoints()
{
    // All curves must hit (0,0) and (1,1) exactly.
    EXPECT_NEAR(DuiEase::Linear(0.0),       0.0, 1e-9, _T("End/lin0"));
    EXPECT_NEAR(DuiEase::Linear(1.0),       1.0, 1e-9, _T("End/lin1"));
    EXPECT_NEAR(DuiEase::EaseInQuad(0.0),   0.0, 1e-9, _T("End/inQ0"));
    EXPECT_NEAR(DuiEase::EaseInQuad(1.0),   1.0, 1e-9, _T("End/inQ1"));
    EXPECT_NEAR(DuiEase::EaseOutQuad(0.0),  0.0, 1e-9, _T("End/outQ0"));
    EXPECT_NEAR(DuiEase::EaseOutQuad(1.0),  1.0, 1e-9, _T("End/outQ1"));
    EXPECT_NEAR(DuiEase::EaseInOutQuad(0.0), 0.0, 1e-9, _T("End/ioQ0"));
    EXPECT_NEAR(DuiEase::EaseInOutQuad(1.0), 1.0, 1e-9, _T("End/ioQ1"));
    EXPECT_NEAR(DuiEase::EaseInCubic(0.0),  0.0, 1e-9, _T("End/inC0"));
    EXPECT_NEAR(DuiEase::EaseInCubic(1.0),  1.0, 1e-9, _T("End/inC1"));
    EXPECT_NEAR(DuiEase::EaseOutCubic(0.0), 0.0, 1e-9, _T("End/outC0"));
    EXPECT_NEAR(DuiEase::EaseOutCubic(1.0), 1.0, 1e-9, _T("End/outC1"));
    EXPECT_NEAR(DuiEase::EaseInOutCubic(0.0), 0.0, 1e-9, _T("End/ioC0"));
    EXPECT_NEAR(DuiEase::EaseInOutCubic(1.0), 1.0, 1e-9, _T("End/ioC1"));
    return OK(_T("EasingEndpoints"));
}

// Specific midpoint values verify the curve shape.
static Result Test_EasingMidpoints()
{
    EXPECT_NEAR(DuiEase::Linear(0.5),        0.5,    1e-9, _T("Mid/lin"));
    EXPECT_NEAR(DuiEase::EaseInQuad(0.5),    0.25,   1e-9, _T("Mid/inQ"));
    EXPECT_NEAR(DuiEase::EaseOutQuad(0.5),   0.75,   1e-9, _T("Mid/outQ"));
    EXPECT_NEAR(DuiEase::EaseInOutQuad(0.5), 0.5,    1e-9, _T("Mid/ioQ"));
    EXPECT_NEAR(DuiEase::EaseInCubic(0.5),   0.125,  1e-9, _T("Mid/inC"));
    EXPECT_NEAR(DuiEase::EaseOutCubic(0.5),  0.875,  1e-9, _T("Mid/outC"));
    EXPECT_NEAR(DuiEase::EaseInOutCubic(0.5), 0.5,   1e-9, _T("Mid/ioC"));
    return OK(_T("EasingMidpoints"));
}

// ----- DuiDoubleAnim Tick semantics -----------------------------------

static Result Test_TickProgress()
{
    double v = 0;
    auto setter = [&](double x)
    {
        v = x;
    };
    DuiDoubleAnim a(1000, 0.0, 100.0, setter);
    EXPECT_TRUE(!a.IsRunning(), _T("Tick/preRun"));

    a.TickWithElapsed(0);     // t=0 -> v=0
    EXPECT_NEAR(v, 0.0, 1e-9, _T("Tick/0"));
    EXPECT_TRUE(a.IsRunning(), _T("Tick/runningAt0"));

    a.TickWithElapsed(250);   // t=0.25 -> v=25 (linear default)
    EXPECT_NEAR(v, 25.0, 1e-9, _T("Tick/.25"));

    a.TickWithElapsed(500);   // t=0.5 -> v=50
    EXPECT_NEAR(v, 50.0, 1e-9, _T("Tick/.5"));

    bool more = a.TickWithElapsed(1000);  // t=1 -> v=100, returns false
    EXPECT_TRUE(!more, _T("Tick/1retFalse"));
    EXPECT_TRUE(a.IsDone(), _T("Tick/done"));
    EXPECT_NEAR(v, 100.0, 1e-9, _T("Tick/atEnd"));

    // Extra ticks after done are ignored.
    bool more2 = a.TickWithElapsed(2000);
    EXPECT_TRUE(!more2, _T("Tick/postDone"));
    EXPECT_NEAR(v, 100.0, 1e-9, _T("Tick/postDoneVal"));
    return OK(_T("TickProgress"));
}

static Result Test_FromToAccessors()
{
    double v = 0;
    DuiDoubleAnim a(500, 30.0, 70.0, [&](double x)
    {
        v = x;
    });
    EXPECT_NEAR(a.From(), 30.0, 1e-9, _T("FT/from"));
    EXPECT_NEAR(a.To(),   70.0, 1e-9, _T("FT/to"));
    a.TickWithElapsed(250);
    EXPECT_NEAR(a.Current(), 50.0, 1e-9, _T("FT/cur"));
    EXPECT_NEAR(v, 50.0, 1e-9, _T("FT/setter"));
    return OK(_T("FromToAccessors"));
}

// EaseInQuad applied to a tween clamps endpoints exactly.
static Result Test_EasingApplied()
{
    double v = 0;
    DuiDoubleAnim a(100, 0.0, 10.0, [&](double x)
    {
        v = x;
    });
    a.SetEasing(&DuiEase::EaseInQuad);
    a.TickWithElapsed(50);    // t=0.5 -> ease=0.25 -> v=2.5
    EXPECT_NEAR(v, 2.5, 1e-9, _T("Eased/mid"));
    a.TickWithElapsed(100);   // t=1 -> v=10
    EXPECT_NEAR(v, 10.0, 1e-9, _T("Eased/end"));
    return OK(_T("EasingApplied"));
}

// OnComplete fires exactly once on the tick that crossed t=1.
static Result Test_OnCompleteFires()
{
    int hits = 0;
    DuiDoubleAnim a(100, 0.0, 1.0, [](double){});
    a.SetOnComplete([&]()
    {
        ++hits;
    });
    a.TickWithElapsed(50);
    EXPECT_INT(hits, 0, _T("Cmp/midNotYet"));
    a.TickWithElapsed(100);
    EXPECT_INT(hits, 1, _T("Cmp/atEnd"));
    a.TickWithElapsed(200);
    EXPECT_INT(hits, 1, _T("Cmp/onceOnly"));
    return OK(_T("OnCompleteFires"));
}

// Finish() jumps to t=1 + fires OnComplete + can be called only once.
static Result Test_FinishJumpsToEnd()
{
    double v = 0;
    int hits = 0;
    DuiDoubleAnim a(1000, 0.0, 100.0, [&](double x)
    {
        v = x;
    });
    a.SetOnComplete([&]()
    {
        ++hits;
    });
    a.Finish();
    EXPECT_NEAR(v, 100.0, 1e-9, _T("Fin/atEnd"));
    EXPECT_INT(hits, 1, _T("Fin/cb"));
    a.Finish();   // idempotent
    EXPECT_INT(hits, 1, _T("Fin/idem"));
    return OK(_T("FinishJumpsToEnd"));
}

// Duration <= 0 clamps to 1ms (avoid divide-by-zero).
static Result Test_DurationClamp()
{
    double v = -1;
    DuiDoubleAnim a(0, 0.0, 5.0, [&](double x)
    {
        v = x;
    });
    EXPECT_INT(a.GetDurationMs(), 1, _T("Dur/clamp"));
    a.TickWithElapsed(1);
    EXPECT_TRUE(a.IsDone(), _T("Dur/instantDone"));
    EXPECT_NEAR(v, 5.0, 1e-9, _T("Dur/atEnd"));
    return OK(_T("DurationClamp"));
}

// ----- DuiAnimMgr -----------------------------------------------------

static Result Test_MgrAddAndTick()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    int finished = 0;
    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    a->SetOnComplete([&]()
    {
        ++finished;
    });
    DuiAnim* raw = a.get();
    m.Add(std::move(a));

    EXPECT_INT(m.GetActiveCount(), 1, _T("Mgr/added"));

    // First tick records start time at nowMs=1000. Anim still running.
    m.TickAll(1000);
    EXPECT_INT(m.GetActiveCount(), 1, _T("Mgr/midRun"));

    // Tick at 1100: elapsed=100 -> done -> mgr removes it.
    m.TickAll(1100);
    EXPECT_INT(m.GetActiveCount(), 0, _T("Mgr/cleanedUp"));
    EXPECT_INT(finished, 1, _T("Mgr/cb"));
    (void)raw;
    return OK(_T("MgrAddAndTick"));
}

// Mgr::Clear cancels without firing OnComplete.
static Result Test_MgrClearCancels()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();

    int finished = 0;
    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        100, 0.0, 1.0, [](double){}));
    a->SetOnComplete([&]()
    {
        ++finished;
    });
    m.Add(std::move(a));

    m.Clear();
    EXPECT_INT(m.GetActiveCount(), 0, _T("MgrClr/empty"));
    EXPECT_INT(finished, 0, _T("MgrClr/noFire"));
    return OK(_T("MgrClearCancels"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_NEAR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn fn;
    };
    Entry tests[] = {
        { _T("EasingEndpoints"), &Test_EasingEndpoints },
        { _T("EasingMidpoints"), &Test_EasingMidpoints },
        { _T("TickProgress"),    &Test_TickProgress    },
        { _T("FromToAccessors"), &Test_FromToAccessors },
        { _T("EasingApplied"),   &Test_EasingApplied   },
        { _T("OnCompleteFires"), &Test_OnCompleteFires },
        { _T("FinishJumpsToEnd"),&Test_FinishJumpsToEnd},
        { _T("DurationClamp"),   &Test_DurationClamp   },
        { _T("MgrAddAndTick"),   &Test_MgrAddAndTick   },
        { _T("MgrClearCancels"), &Test_MgrClearCancels },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    for (auto& e : tests)
    {
        Result r = e.fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), e.name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), e.name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }
    CString summary;
    summary.Format(_T("[summary] DuiAnimationTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiAnimationTests

} // namespace balloonwjui
