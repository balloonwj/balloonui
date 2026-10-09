#include "stdafx.h"
#include "DuiThemeTests.h"
#include "../BalloonUiFeatures.h"
#include "../DuiResMgr.h"
#if BUI_FEATURE_TOAST
#  include "../Controls/Basic/DuiToast.h"
#endif

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

// =====================================================================
// 默认字体名 / 默认字号与 DuiResMgr 字体缓存（2026-10-09）
//
// 字体名与字号由 DuiTheme 保存，DuiResMgr 建字体时读取。F1 ~ F6 核对改字体名之后
// 取到的字体跟着变、旧句柄仍然有效；P1 ~ P5 核对默认字号跟随 SetDefaultFontPt。
// 都用显式指定 DPI 的取字体接口（...ForDpi），不改全局 DPI；主题由 ThemeSnap 恢复。
// =====================================================================

// 取字体时传入的 DPI（96 = 100% 缩放）。
const int kFontTestDpi = 96;
// 1 英寸 = 72 磅，字高 lfHeight = -MulDiv(磅值, DPI, 72)。
const int kPointsPerInch = 72;
// 改换字体名用的另一个字体：系统自带、与默认的微软雅黑不同。
const TCHAR kOtherFace[] = _T("Segoe UI");
// 同一字体名的另一种大小写写法，用来核对字体名比较不区分大小写。
const TCHAR kOtherFaceLower[] = _T("segoe ui");
// 默认字体名（DuiTheme 的初始值）。
const TCHAR kYaHeiFace[] = _T("Microsoft YaHei");
// 默认字号（DuiTheme 的初始值，单位磅）。
const int kInitialPt = 9;
// 改换默认字号用的另一个字号（磅）。
const int kOtherPt = 12;
// 按磅值取字体时用的字号（磅），与默认字号不同。
const int kProbePt = 14;

// 读出 hf 的 LOGFONT。hf 为空或已失效时返回 false。
bool ReadLogFont(HFONT hf, LOGFONT& lf)
{
    ::memset(&lf, 0, sizeof(lf));
    if (hf == nullptr)
    {
        return false;
    }
    return ::GetObject(hf, sizeof(lf), &lf) == sizeof(lf);
}

// 字体名是否为 face（不区分大小写）。
bool FaceIs(const LOGFONT& lf, LPCTSTR face)
{
    return _tcsicmp(lf.lfFaceName, face) == 0;
}

// F1：改字体名之后，默认字体换成新字体名。
static Result Test_FontFaceChangeTakesEffect()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontFace(kYaHeiFace);
    HFONT before = DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    DuiTheme::Inst().SetDefaultFontFace(kOtherFace);
    HFONT after = DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    EXPECT_TRUE(after != nullptr, _T("F1/after"));
    EXPECT_TRUE(after != before, _T("F1/newHandle"));
    LOGFONT lf;
    EXPECT_TRUE(ReadLogFont(after, lf), _T("F1/read"));
    EXPECT_TRUE(FaceIs(lf, kOtherFace), _T("F1/face"));
    return OK(_T("FontFaceChangeTakesEffect"));
}

// F2：改回原来的字体名时复用原来的句柄（缓存命中）。
static Result Test_FontFaceSwitchBackReusesHandle()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontFace(kYaHeiFace);
    HFONT first = DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    DuiTheme::Inst().SetDefaultFontFace(kOtherFace);
    DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    DuiTheme::Inst().SetDefaultFontFace(kYaHeiFace);
    HFONT again = DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    EXPECT_TRUE(again == first, _T("F2/sameHandle"));
    return OK(_T("FontFaceSwitchBackReusesHandle"));
}

// F3：字体名只是大小写不同时，取到同一个句柄。
static Result Test_FontFaceCaseInsensitive()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontFace(kOtherFace);
    HFONT upper = DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    DuiTheme::Inst().SetDefaultFontFace(kOtherFaceLower);
    HFONT lower = DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    EXPECT_TRUE(upper != nullptr, _T("F3/upper"));
    EXPECT_TRUE(lower == upper, _T("F3/sameHandle"));
    return OK(_T("FontFaceCaseInsensitive"));
}

// F4：改字体名之后，之前取到的旧句柄仍然有效，内容不变。
static Result Test_FontFaceOldHandleStillValid()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontFace(kYaHeiFace);
    HFONT old = DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    DuiTheme::Inst().SetDefaultFontFace(kOtherFace);
    DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    LOGFONT lf;
    EXPECT_TRUE(ReadLogFont(old, lf), _T("F4/oldValid"));
    EXPECT_TRUE(FaceIs(lf, kYaHeiFace), _T("F4/oldFace"));
    return OK(_T("FontFaceOldHandleStillValid"));
}

// F5：字符集。微软雅黑用 GB2312，其它字体用 DEFAULT_CHARSET（指定 GB2312 会让系统换用别的字体）。
static Result Test_FontFaceCharset()
{
    ThemeSnap snap(DuiTheme::Inst());
    LOGFONT lf;
    DuiTheme::Inst().SetDefaultFontFace(kYaHeiFace);
    EXPECT_TRUE(ReadLogFont(DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi), lf), _T("F5/readYaHei"));
    EXPECT_INT(lf.lfCharSet, GB2312_CHARSET, _T("F5/yaheiCharset"));
    DuiTheme::Inst().SetDefaultFontFace(kOtherFace);
    EXPECT_TRUE(ReadLogFont(DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi), lf), _T("F5/readOther"));
    EXPECT_INT(lf.lfCharSet, DEFAULT_CHARSET, _T("F5/otherCharset"));
    return OK(_T("FontFaceCharset"));
}

// F6：按磅值取的字体与抗锯齿字体同样跟随新字体名。
static Result Test_FontFaceAppliesToPointSizeFonts()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontFace(kYaHeiFace);
    DuiResMgr::Inst().GetFontByPointSizeForDpi(kProbePt, false, kFontTestDpi);
    DuiResMgr::Inst().GetAntiAliasedFontByPointSizeForDpi(kProbePt, false, kFontTestDpi);
    DuiTheme::Inst().SetDefaultFontFace(kOtherFace);
    LOGFONT lf;
    EXPECT_TRUE(ReadLogFont(DuiResMgr::Inst().GetFontByPointSizeForDpi(kProbePt, false, kFontTestDpi), lf),
                _T("F6/readPt"));
    EXPECT_TRUE(FaceIs(lf, kOtherFace), _T("F6/ptFace"));
    EXPECT_TRUE(ReadLogFont(DuiResMgr::Inst().GetAntiAliasedFontByPointSizeForDpi(kProbePt, false, kFontTestDpi), lf),
                _T("F6/readAa"));
    EXPECT_TRUE(FaceIs(lf, kOtherFace), _T("F6/aaFace"));
    EXPECT_INT(lf.lfQuality, ANTIALIASED_QUALITY, _T("F6/aaQuality"));
    return OK(_T("FontFaceAppliesToPointSizeFonts"));
}

// P1：默认字体的字号跟随 SetDefaultFontPt。
static Result Test_DefaultFontPtTakesEffect()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontPt(kOtherPt);
    LOGFONT lf;
    EXPECT_TRUE(ReadLogFont(DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi), lf), _T("P1/read"));
    EXPECT_INT(lf.lfHeight, -::MulDiv(kOtherPt, kFontTestDpi, kPointsPerInch), _T("P1/height"));
    return OK(_T("DefaultFontPtTakesEffect"));
}

// P2：按磅值取字体时磅值 <= 0 表示默认字体，同样跟随 SetDefaultFontPt。
static Result Test_DefaultFontPtAppliesToZeroPt()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontPt(kOtherPt);
    LOGFONT lf;
    EXPECT_TRUE(ReadLogFont(DuiResMgr::Inst().GetFontByPointSizeForDpi(0, false, kFontTestDpi), lf), _T("P2/read"));
    EXPECT_INT(lf.lfHeight, -::MulDiv(kOtherPt, kFontTestDpi, kPointsPerInch), _T("P2/height"));
    return OK(_T("DefaultFontPtAppliesToZeroPt"));
}

// P3：不调用 SetDefaultFontPt 时，默认字号仍是 9 磅（与改动之前相同）。
static Result Test_DefaultFontPtUnchangedByDefault()
{
    ThemeSnap snap(DuiTheme::Inst());
    EXPECT_INT(DuiTheme::Inst().GetDefaultFontPt(), kInitialPt, _T("P3/themePt"));
    LOGFONT lf;
    EXPECT_TRUE(ReadLogFont(DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi), lf), _T("P3/read"));
    EXPECT_INT(lf.lfHeight, -::MulDiv(kInitialPt, kFontTestDpi, kPointsPerInch), _T("P3/height"));
    return OK(_T("DefaultFontPtUnchangedByDefault"));
}

#if BUI_FEATURE_TOAST
// P4：DuiToast 没有设字号时，实际使用的抗锯齿字体跟随默认字号。
static Result Test_ToastDefaultFontFollowsThemePt()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontPt(kOtherPt);
    DuiToast toast;
    LOGFONT lf;
    EXPECT_TRUE(ReadLogFont(toast.Test_ResolveFont(), lf), _T("P4/read"));
    EXPECT_INT(lf.lfHeight, -::MulDiv(kOtherPt, toast.GetDpi(), kPointsPerInch), _T("P4/height"));
    EXPECT_INT(lf.lfQuality, ANTIALIASED_QUALITY, _T("P4/aaQuality"));
    return OK(_T("ToastDefaultFontFollowsThemePt"));
}
#endif

// P5：改默认字号之后，之前取到的旧句柄仍然有效，字号不变。
static Result Test_DefaultFontPtOldHandleStillValid()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme::Inst().SetDefaultFontPt(kInitialPt);
    HFONT old = DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    DuiTheme::Inst().SetDefaultFontPt(kOtherPt);
    DuiResMgr::Inst().GetDefaultFontForDpi(kFontTestDpi);
    LOGFONT lf;
    EXPECT_TRUE(ReadLogFont(old, lf), _T("P5/oldValid"));
    EXPECT_INT(lf.lfHeight, -::MulDiv(kInitialPt, kFontTestDpi, kPointsPerInch), _T("P5/oldHeight"));
    return OK(_T("DefaultFontPtOldHandleStillValid"));
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
        { _T("FontFaceChangeTakesEffect"),       &Test_FontFaceChangeTakesEffect       },
        { _T("FontFaceSwitchBackReusesHandle"),  &Test_FontFaceSwitchBackReusesHandle  },
        { _T("FontFaceCaseInsensitive"),         &Test_FontFaceCaseInsensitive         },
        { _T("FontFaceOldHandleStillValid"),     &Test_FontFaceOldHandleStillValid     },
        { _T("FontFaceCharset"),                 &Test_FontFaceCharset                 },
        { _T("FontFaceAppliesToPointSizeFonts"), &Test_FontFaceAppliesToPointSizeFonts },
        { _T("DefaultFontPtTakesEffect"),        &Test_DefaultFontPtTakesEffect        },
        { _T("DefaultFontPtAppliesToZeroPt"),    &Test_DefaultFontPtAppliesToZeroPt    },
        { _T("DefaultFontPtUnchangedByDefault"), &Test_DefaultFontPtUnchangedByDefault },
#if BUI_FEATURE_TOAST
        { _T("ToastDefaultFontFollowsThemePt"),  &Test_ToastDefaultFontFollowsThemePt  },
#endif
        { _T("DefaultFontPtOldHandleStillValid"), &Test_DefaultFontPtOldHandleStillValid },
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
