#include "stdafx.h"
#include "DuiThemeTests.h"

namespace balloonwjui {

namespace DuiThemeTests {

namespace {

struct Result { CString name; bool ok; CString detail; };
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

// The theme is process-wide; each test snapshots / restores so tests
// don't bleed into one another or into the gallery's runtime state.
struct ThemeSnap
{
    DuiTheme&            t;
    COLORREF             saved[DuiTheme::SlotCount];
    int                  pt;
    CString              face;
    explicit ThemeSnap(DuiTheme& th) : t(th)
    {
        for (int i = 0; i < DuiTheme::SlotCount; ++i)
        {
            saved[i] = t.Get((DuiTheme::Slot)i);
        }
        pt   = t.GetDefaultFontPt();
        face = t.GetDefaultFontFace();
    }
    ~ThemeSnap()
    {
        for (int i = 0; i < DuiTheme::SlotCount; ++i)
        {
            t.Set((DuiTheme::Slot)i, saved[i]);
        }
        t.SetDefaultFontPt(pt);
        t.SetDefaultFontFace(face);
    }
};

// Default palette is the Light preset; brand-blue must be #2D6CDF.
static Result Test_LightPreset()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();
    t.ApplyPreset(DuiTheme::Light);
    EXPECT_INT((int)t.Get(DuiTheme::BrandPrimary), (int)RGB(45,108,223), _T("Lt/brand"));
    EXPECT_INT((int)t.Get(DuiTheme::TextOnPrimary),(int)RGB(255,255,255),_T("Lt/textOn"));
    EXPECT_INT((int)t.Get(DuiTheme::SurfaceBg),    (int)RGB(255,255,255),_T("Lt/surface"));
    EXPECT_INT((int)t.Get(DuiTheme::StatusOnline), (int)RGB( 60,175, 80),_T("Lt/online"));
    return OK(_T("LightPreset"));
}

// Dark preset swaps surface bg + text colors.
static Result Test_DarkPreset()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();
    t.ApplyPreset(DuiTheme::Dark);
    EXPECT_INT((int)t.Get(DuiTheme::SurfaceBg),  (int)RGB( 30, 32, 38), _T("Dk/bg"));
    EXPECT_INT((int)t.Get(DuiTheme::TextDefault),(int)RGB(230,230,235), _T("Dk/text"));
    return OK(_T("DarkPreset"));
}

// Set updates the slot + bumps version.
static Result Test_SetBumpsVersion()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();
    unsigned v0 = t.GetVersion();
    t.Set(DuiTheme::TextLink, RGB(20, 60, 200));
    EXPECT_TRUE(t.GetVersion() > v0, _T("Set/bump"));
    EXPECT_INT((int)t.Get(DuiTheme::TextLink), (int)RGB(20, 60, 200), _T("Set/value"));
    // Setting the same value again must NOT bump.
    unsigned v1 = t.GetVersion();
    t.Set(DuiTheme::TextLink, RGB(20, 60, 200));
    EXPECT_INT((int)t.GetVersion(), (int)v1, _T("Set/idempotent"));
    return OK(_T("SetBumpsVersion"));
}

// Out-of-range Set is a safe no-op.
static Result Test_SetOutOfRangeIgnored()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();
    unsigned v = t.GetVersion();
    t.Set((DuiTheme::Slot)999, RGB(1, 2, 3));
    EXPECT_INT((int)t.GetVersion(), (int)v, _T("OOR/noBump"));
    return OK(_T("SetOutOfRangeIgnored"));
}

// SubscribeChange / Unsubscribe lifecycle.
static Result Test_SubscriptionFires()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();

    int hits = 0;
    int tok = t.SubscribeChange(
        [](void* u) { ++(*(int*)u); }, &hits);
    EXPECT_TRUE(tok > 0, _T("Sub/token"));
    int subsBefore = t.GetSubscriberCount();

    t.Set(DuiTheme::TextSubtle, RGB(0, 0, 0));
    EXPECT_INT(hits, 1, _T("Sub/firedOnSet"));

    t.ApplyPreset(DuiTheme::Dark);
    EXPECT_INT(hits, 2, _T("Sub/firedOnPreset"));

    t.Unsubscribe(tok);
    EXPECT_INT(t.GetSubscriberCount(), subsBefore - 1, _T("Sub/unsub"));

    t.Set(DuiTheme::TextSubtle, RGB(1, 1, 1));
    EXPECT_INT(hits, 2, _T("Sub/silentAfterUnsub"));
    return OK(_T("SubscriptionFires"));
}

// Null callback Subscribe returns 0 + no leak.
static Result Test_SubscribeNull()
{
    DuiTheme& t = DuiTheme::Inst();
    int before = t.GetSubscriberCount();
    int tok = t.SubscribeChange(nullptr, nullptr);
    EXPECT_INT(tok, 0, _T("SubN/zeroToken"));
    EXPECT_INT(t.GetSubscriberCount(), before, _T("SubN/noLeak"));
    return OK(_T("SubscribeNull"));
}

// Font hooks round-trip + clamp.
static Result Test_FontHooks()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();
    t.SetDefaultFontPt(13);
    EXPECT_INT(t.GetDefaultFontPt(), 13, _T("Fnt/13"));
    t.SetDefaultFontPt(0);                 // clamp to 6
    EXPECT_INT(t.GetDefaultFontPt(), 6, _T("Fnt/clampLow"));
    t.SetDefaultFontPt(999);               // clamp to 96
    EXPECT_INT(t.GetDefaultFontPt(), 96, _T("Fnt/clampHigh"));
    t.SetDefaultFontFace(_T("Consolas"));
    if (t.GetDefaultFontFace() != _T("Consolas"))
    {
        return Fail(_T("Fnt/face"), _T("face mismatch"));
    }
    return OK(_T("FontHooks"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("LightPreset"),         &Test_LightPreset         },
        { _T("DarkPreset"),          &Test_DarkPreset          },
        { _T("SetBumpsVersion"),     &Test_SetBumpsVersion     },
        { _T("SetOutOfRangeIgnored"),&Test_SetOutOfRangeIgnored},
        { _T("SubscriptionFires"),   &Test_SubscriptionFires   },
        { _T("SubscribeNull"),       &Test_SubscribeNull       },
        { _T("FontHooks"),           &Test_FontHooks           },
    };

    CString out;
    int passed = 0, failed = 0;
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
    summary.Format(_T("[summary] DuiThemeTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiThemeTests

} // namespace balloonwjui
