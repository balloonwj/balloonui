#include "stdafx.h"
#include "DuiSwitchTests.h"

#if BUI_FEATURE_SWITCH

#include "../DuiAnimation.h"

namespace balloonwjui {

namespace DuiSwitchTests {

namespace {

struct Result
{
    CString name;
    bool    ok;
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
#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_NEAR(actual, expected, tol, name) \
    do { double _a = (actual); double _e = (expected); double _t = (tol); \
         double _diff = _a - _e; if (_diff < 0) _diff = -_diff; \
         if (_diff > _t) { CString _d; _d.Format(_T("expected~%.3f got=%.3f tol=%.3f"), _e, _a, _t); return Fail(name, _d); } \
    } while (0)

// ===== Defaults =======================================================

static Result Test_Defaults()
{
    DuiSwitch s;
    EXPECT_BOOL(s.IsChecked(), false, _T("Defaults/checked"));
    EXPECT_BOOL(s.IsAnimated(), true, _T("Defaults/animated"));
    EXPECT_NEAR(s.GetProgress(), 0.0, 0.001, _T("Defaults/progress"));
    return OK(_T("Defaults"));
}

// ===== SetChecked: state + progress + notify ==========================

// Programmatic SetChecked with notify=false fires no DUIN_VALUECHANGED.
static Result Test_SetChecked_NotifyFalse()
{
    DuiSwitch s;
    s.SetChecked(true, /*animated=*/false);
    EXPECT_BOOL(s.IsChecked(), true, _T("SetCheck/state"));
    EXPECT_NEAR(s.GetProgress(), 1.0, 0.001, _T("SetCheck/progress"));
    return OK(_T("SetChecked_NotifyFalse"));
}

// SetChecked(_, animated=false) snaps progress to target (no animation).
static Result Test_SetChecked_NoAnimSnaps()
{
    DuiSwitch s;
    DuiAnimMgr::Inst().Clear();
    s.SetChecked(true, /*animated=*/false);
    EXPECT_NEAR(s.GetProgress(), 1.0, 0.001, _T("Snap/on"));
    EXPECT_INT(DuiAnimMgr::Inst().GetActiveCount(), 0, _T("Snap/noAnimQueued"));
    s.SetChecked(false, /*animated=*/false);
    EXPECT_NEAR(s.GetProgress(), 0.0, 0.001, _T("Snap/off"));
    EXPECT_INT(DuiAnimMgr::Inst().GetActiveCount(), 0, _T("Snap/noAnimQueuedAgain"));
    return OK(_T("SetChecked_NoAnimSnaps"));
}

// SetChecked(_, animated=true) registers an anim and progress moves over time.
static Result Test_SetChecked_AnimMovesProgress()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();
    DuiSwitch s;
    s.SetChecked(true, /*animated=*/true);
    EXPECT_BOOL(s.IsChecked(), true, _T("Anim/stateImmediate"));
    EXPECT_INT(m.GetActiveCount(), 1, _T("Anim/queued"));

    // First tick records start time; second tick at +75ms = halfway.
    m.TickAll(10000);
    m.TickAll(10075);
    // 75/150 = 0.5; ease-out-cubic(0.5) ≈ 1 - 0.5^3 = 0.875
    EXPECT_NEAR(s.GetProgress(), 0.875, 0.05, _T("Anim/half"));

    // At end, progress should equal 1.0.
    m.TickAll(10150);
    EXPECT_NEAR(s.GetProgress(), 1.0, 0.001, _T("Anim/end"));
    EXPECT_INT(m.GetActiveCount(), 0, _T("Anim/cleanedUp"));
    return OK(_T("SetChecked_AnimMovesProgress"));
}

// Calling SetChecked with the same value is a no-op (no anim, no notify).
static Result Test_SetChecked_Idempotent()
{
    DuiAnimMgr& m = DuiAnimMgr::Inst();
    m.Clear();
    DuiSwitch s;
    s.SetChecked(false, false);
    EXPECT_INT(m.GetActiveCount(), 0, _T("Idem/noAnim"));
    return OK(_T("SetChecked_Idempotent"));
}

// ===== Click flips state ==============================================

static Result Test_ClickToggles()
{
    DuiSwitch s;
    s.SetRect(RECT{ 0, 0, 46, 24 });
    DuiAnimMgr::Inst().Clear();

    s.OnLButtonDown(POINT{ 23, 12 }, 0);
    s.OnLButtonUp  (POINT{ 23, 12 }, 0);
    EXPECT_BOOL(s.IsChecked(), true, _T("Click/firstFlip"));

    s.OnLButtonDown(POINT{ 23, 12 }, 0);
    s.OnLButtonUp  (POINT{ 23, 12 }, 0);
    EXPECT_BOOL(s.IsChecked(), false, _T("Click/secondFlip"));
    return OK(_T("ClickToggles"));
}

// Down inside, Up outside -> no toggle (drag-out cancels, matches DuiButton).
static Result Test_DragOutCancels()
{
    DuiSwitch s;
    s.SetRect(RECT{ 0, 0, 46, 24 });
    s.OnLButtonDown(POINT{ 23, 12 }, 0);
    s.OnLButtonUp  (POINT{ 999, 999 }, 0);
    EXPECT_BOOL(s.IsChecked(), false, _T("DragOut/noToggle"));
    return OK(_T("DragOutCancels"));
}

// Disabled blocks click.
static Result Test_DisabledIgnoresClick()
{
    DuiSwitch s;
    s.SetRect(RECT{ 0, 0, 46, 24 });
    s.SetEnabled(false);
    s.OnLButtonDown(POINT{ 23, 12 }, 0);
    s.OnLButtonUp  (POINT{ 23, 12 }, 0);
    EXPECT_BOOL(s.IsChecked(), false, _T("Disabled/ignored"));
    return OK(_T("DisabledIgnoresClick"));
}

// ===== Keyboard =======================================================

static Result Test_SpaceToggles()
{
    DuiSwitch s;
    s.OnKeyDown(VK_SPACE, 0);
    EXPECT_BOOL(s.IsChecked(), true, _T("Space/flip"));
    s.OnKeyDown(VK_SPACE, 0);
    EXPECT_BOOL(s.IsChecked(), false, _T("Space/flipBack"));
    return OK(_T("SpaceToggles"));
}

static Result Test_EnterToggles()
{
    DuiSwitch s;
    s.OnKeyDown(VK_RETURN, 0);
    EXPECT_BOOL(s.IsChecked(), true, _T("Enter/flip"));
    return OK(_T("EnterToggles"));
}

// Other keys are ignored (return false from handler so host bubbles).
static Result Test_OtherKeysIgnored()
{
    DuiSwitch s;
    bool consumed = s.OnKeyDown(VK_LEFT, 0);
    EXPECT_BOOL(consumed, false, _T("OtherKey/notConsumed"));
    EXPECT_BOOL(s.IsChecked(), false, _T("OtherKey/noToggle"));
    return OK(_T("OtherKeysIgnored"));
}

// Disabled blocks keyboard too.
static Result Test_DisabledIgnoresKey()
{
    DuiSwitch s;
    s.SetEnabled(false);
    s.OnKeyDown(VK_SPACE, 0);
    EXPECT_BOOL(s.IsChecked(), false, _T("DisabledKey/noToggle"));
    return OK(_T("DisabledIgnoresKey"));
}

// ===== Geometry =======================================================

// Knob X position interpolates with progress along the rail.
static Result Test_KnobXFromProgress()
{
    DuiSwitch s;
    s.SetRect(RECT{ 0, 0, 46, 24 });

    s.SetChecked(false, false);
    int xOff = s.ComputeKnobCenterX();
    s.SetChecked(true, false);
    int xOn = s.ComputeKnobCenterX();

    // off knob is on the left of the rail; on knob is on the right.
    if (!(xOn > xOff))
    {
        CString d;
        d.Format(_T("xOff=%d xOn=%d"), xOff, xOn);
        return Fail(_T("Knob/xOnGtxOff"), d);
    }
    return OK(_T("KnobXFromProgress"));
}

// Larger control widens the travel.
static Result Test_KnobTravelScalesWithWidth()
{
    DuiSwitch s1, s2;
    s1.SetRect(RECT{ 0, 0,  46, 24 });
    s2.SetRect(RECT{ 0, 0, 100, 24 });
    s1.SetChecked(false, false);
    s2.SetChecked(false, false);
    int x1off = s1.ComputeKnobCenterX();
    int x2off = s2.ComputeKnobCenterX();
    s1.SetChecked(true, false);
    s2.SetChecked(true, false);
    int x1on = s1.ComputeKnobCenterX();
    int x2on = s2.ComputeKnobCenterX();
    int travel1 = x1on - x1off;
    int travel2 = x2on - x2off;
    if (!(travel2 > travel1))
    {
        CString d;
        d.Format(_T("travel1=%d travel2=%d"), travel1, travel2);
        return Fail(_T("Travel/widerTravelsFurther"), d);
    }
    return OK(_T("KnobTravelScalesWithWidth"));
}

// ===== Custom colors ==================================================

static Result Test_CustomColorsRoundTrip()
{
    DuiSwitch s;
    s.SetOnColor (RGB(7, 193, 96));      // brand green default
    s.SetOffColor(RGB(229, 229, 229));   // gray default
    s.SetKnobColor(RGB(255, 255, 255));
    EXPECT_INT((int)s.GetOnColor(),   (int)RGB(7, 193, 96),     _T("Color/on"));
    EXPECT_INT((int)s.GetOffColor(),  (int)RGB(229, 229, 229),  _T("Color/off"));
    EXPECT_INT((int)s.GetKnobColor(), (int)RGB(255, 255, 255),  _T("Color/knob"));
    return OK(_T("CustomColorsRoundTrip"));
}

// SetAnimated(false) makes subsequent SetChecked snap.
static Result Test_SetAnimatedFlagAffectsDefault()
{
    DuiAnimMgr::Inst().Clear();
    DuiSwitch s;
    s.SetAnimated(false);
    // Click should not enqueue an anim because IsAnimated() is false.
    s.SetRect(RECT{ 0, 0, 46, 24 });
    s.OnLButtonDown(POINT{ 23, 12 }, 0);
    s.OnLButtonUp  (POINT{ 23, 12 }, 0);
    EXPECT_BOOL(s.IsChecked(), true, _T("AnimFlag/state"));
    EXPECT_INT(DuiAnimMgr::Inst().GetActiveCount(), 0, _T("AnimFlag/noAnim"));
    EXPECT_NEAR(s.GetProgress(), 1.0, 0.001, _T("AnimFlag/progressSnapped"));
    return OK(_T("SetAnimatedFlagAffectsDefault"));
}

#undef EXPECT_INT
#undef EXPECT_BOOL
#undef EXPECT_NEAR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn  fn;
    };
    Entry tests[] = {
        { _T("Defaults"),                       &Test_Defaults                       },
        { _T("SetChecked_NotifyFalse"),         &Test_SetChecked_NotifyFalse         },
        { _T("SetChecked_NoAnimSnaps"),         &Test_SetChecked_NoAnimSnaps         },
        { _T("SetChecked_AnimMovesProgress"),   &Test_SetChecked_AnimMovesProgress   },
        { _T("SetChecked_Idempotent"),          &Test_SetChecked_Idempotent          },
        { _T("ClickToggles"),                   &Test_ClickToggles                   },
        { _T("DragOutCancels"),                 &Test_DragOutCancels                 },
        { _T("DisabledIgnoresClick"),           &Test_DisabledIgnoresClick           },
        { _T("SpaceToggles"),                   &Test_SpaceToggles                   },
        { _T("EnterToggles"),                   &Test_EnterToggles                   },
        { _T("OtherKeysIgnored"),               &Test_OtherKeysIgnored               },
        { _T("DisabledIgnoresKey"),             &Test_DisabledIgnoresKey             },
        { _T("KnobXFromProgress"),              &Test_KnobXFromProgress              },
        { _T("KnobTravelScalesWithWidth"),      &Test_KnobTravelScalesWithWidth      },
        { _T("CustomColorsRoundTrip"),          &Test_CustomColorsRoundTrip          },
        { _T("SetAnimatedFlagAffectsDefault"),  &Test_SetAnimatedFlagAffectsDefault  },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    for (auto& e : tests)
    {
        Result r = e.fn();
        if (r.ok)
        {
            ++passed;
            CString line;
            line.Format(_T("[ok]   %s"), e.name);
            if (!out.IsEmpty())
            {
                out += _T("\r\n");
            }
            out += line;
        }
        else
        {
            ++failed;
            CString line;
            line.Format(_T("[FAIL] %s : %s"), e.name, (LPCTSTR)r.detail);
            if (!out.IsEmpty())
            {
                out += _T("\r\n");
            }
            out += line;
        }
    }
    CString summary;
    summary.Format(_T("[summary] DuiSwitchTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiSwitchTests

} // namespace balloonwjui

#endif // BUI_FEATURE_SWITCH
