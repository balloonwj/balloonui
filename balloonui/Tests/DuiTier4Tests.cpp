#include "stdafx.h"
#include "DuiTier4Tests.h"
#include "../DuiControl.h"

namespace balloonwjui {

namespace DuiTier4Tests {

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

// ----- HighContrast preset -------------------------------------------

struct ThemeSnap
{
    DuiTheme& t;
    COLORREF saved[DuiTheme::SlotCount];
    explicit ThemeSnap(DuiTheme& th) : t(th)
    {
        for (int i = 0; i < DuiTheme::SlotCount; ++i)
        {
            saved[i] = t.Get((DuiTheme::Slot)i);
        }
    }
    ~ThemeSnap()
    {
        for (int i = 0; i < DuiTheme::SlotCount; ++i)
        {
            t.Set((DuiTheme::Slot)i, saved[i]);
        }
    }
};

static Result Test_HighContrastPreset()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();
    t.ApplyPreset(DuiTheme::HighContrast);
    EXPECT_INT((int)t.Get(DuiTheme::SurfaceBg),    (int)RGB(  0,   0,   0), _T("HC/bg"));
    EXPECT_INT((int)t.Get(DuiTheme::TextDefault),  (int)RGB(255, 255, 255), _T("HC/text"));
    EXPECT_INT((int)t.Get(DuiTheme::BrandPrimary), (int)RGB(255, 255,   0), _T("HC/brand"));
    EXPECT_INT((int)t.Get(DuiTheme::BorderHeavy),  (int)RGB(255, 255, 255), _T("HC/border"));
    EXPECT_INT((int)t.Get(DuiTheme::TextLink),     (int)RGB(  0, 255, 255), _T("HC/link"));
    return OK(_T("HighContrastPreset"));
}

// Going Dark -> HighContrast -> Light all just swap palettes cleanly.
static Result Test_PresetCycleBumpsVersion()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();
    unsigned v0 = t.GetVersion();
    t.ApplyPreset(DuiTheme::Dark);
    unsigned v1 = t.GetVersion();
    t.ApplyPreset(DuiTheme::HighContrast);
    unsigned v2 = t.GetVersion();
    t.ApplyPreset(DuiTheme::Light);
    unsigned v3 = t.GetVersion();
    EXPECT_TRUE(v1 > v0 && v2 > v1 && v3 > v2, _T("Cycle/monotonic"));
    return OK(_T("PresetCycleBumpsVersion"));
}

// Unknown preset value falls back to Light (defensive).
static Result Test_UnknownPresetFallsBackLight()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();
    t.ApplyPreset(DuiTheme::Dark);
    t.ApplyPreset((DuiTheme::Preset)99);
    EXPECT_INT((int)t.Get(DuiTheme::SurfaceBg), (int)RGB(255, 255, 255), _T("Unk/light"));
    return OK(_T("UnknownPresetFallsBackLight"));
}

// ----- DuiFocus paint helpers (smoke) --------------------------------

// DrawRing paints into a memory DC without crashing. Verify the outer
// ring's left edge actually got stroked at the expected color.
static Result Test_DrawRingSmoke()
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 40;
    bi.bmiHeader.biHeight   = -40;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dst = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    EXPECT_TRUE(dst != nullptr, _T("DR/dib"));

    // Pre-fill with green so the ring's red stands out.
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < 40 * 40; ++i)
    {
        p[i*4] = 0;
        p[i*4+1] = 200;
        p[i*4+2] = 0;
        p[i*4+3] = 255;
    }

    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, dst);

    // 1-px ring, no inset, pure red.
    DuiFocus::DrawRing(hdc, RECT{ 5, 5, 35, 35 },
                       RGB(255, 0, 0), /*thickness=*/1, /*withInset=*/false);

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);

    // Sample top-left edge of the ring (y=5, x=5..6) for red.
    BYTE* px = (BYTE*)bits + (5 * 40 + 5) * 4;
    bool isRed = (px[2] >= 200 && px[1] <= 60 && px[0] <= 60);
    ::DeleteObject(dst);
    EXPECT_TRUE(isRed, _T("DR/redOnEdge"));
    return OK(_T("DrawRingSmoke"));
}

static Result Test_DrawRingDegenerate()
{
    HDC hdc = ::CreateCompatibleDC(nullptr);
    DuiFocus::DrawRing(hdc, RECT{ 0, 0, 0, 0 }, RGB(255, 0, 0));     // empty
    DuiFocus::DrawRing(nullptr, RECT{ 0, 0, 10, 10 }, RGB(0, 0, 0)); // null hdc
    ::DeleteDC(hdc);
    return OK(_T("DrawRingDegenerate"));
}

// DrawRingThemed pulls color from DuiTheme::BrandPrimary at call time.
static Result Test_DrawRingThemedFollowsTheme()
{
    ThemeSnap snap(DuiTheme::Inst());
    DuiTheme& t = DuiTheme::Inst();

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 20;
    bi.bmiHeader.biHeight   = -20;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dst = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC hdc = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(hdc, dst);

    auto fill = [&](COLORREF c)
    {
        BYTE* p = (BYTE*)bits;
        for (int i = 0; i < 20 * 20; ++i)
        {
            p[i*4] = GetBValue(c);
            p[i*4+1] = GetGValue(c);
            p[i*4+2] = GetRValue(c);
            p[i*4+3] = 255;
        }
    };
    auto sampleTopLeft = [&]() -> COLORREF
    {
        BYTE* px = (BYTE*)bits + (2 * 20 + 2) * 4;
        return RGB(px[2], px[1], px[0]);
    };

    // Light preset: brand = #2D6CDF.
    fill(RGB(0, 0, 0));
    t.ApplyPreset(DuiTheme::Light);
    DuiFocus::DrawRingThemed(hdc, RECT{ 2, 2, 18, 18 }, 1, false);
    COLORREF lightSample = sampleTopLeft();

    // High contrast: brand = pure yellow #FFFF00.
    fill(RGB(0, 0, 0));
    t.ApplyPreset(DuiTheme::HighContrast);
    DuiFocus::DrawRingThemed(hdc, RECT{ 2, 2, 18, 18 }, 1, false);
    COLORREF hcSample = sampleTopLeft();

    ::SelectObject(hdc, old);
    ::DeleteDC(hdc);
    ::DeleteObject(dst);

    EXPECT_TRUE(lightSample != hcSample, _T("DRT/changes"));
    EXPECT_INT((int)hcSample, (int)RGB(255, 255, 0), _T("DRT/hcYellow"));
    return OK(_T("DrawRingThemedFollowsTheme"));
}

// ----- Tree-mutation fuzzer ------------------------------------------

class FuzzNode : public DuiControl
{
public:
    void OnPaint(HDC, const RECT&) override {}
};

// Apply a deterministic stream of AddChild / RemoveChild operations to
// a small tree, asserting nothing crashes and final state is consistent.
// Pure logic: no HWND, no host. Catches the kind of "removed-while-
// iterating" or "stale parent" bug that's plagued similar code paths
// in legacy CSkin* control containers.
static Result Test_TreeMutationFuzz()
{
    // Use a fixed seed so reproductions are easy.
    unsigned seed = 0x01234567u;
    auto rnd = [&]() -> unsigned
    {
        seed = seed * 1664525u + 1013904223u;     // numerical recipes LCG
        return seed;
    };

    DuiControl root;
    std::vector<DuiControl*> live;

    const int kIters = 4000;
    for (int it = 0; it < kIters; ++it)
    {
        unsigned op = rnd() % 100;
        if (op < 60 || live.empty())
        {
            // 60%: AddChild (or always if tree is empty).
            DuiControl* parent = live.empty() ? &root : live[rnd() % live.size()];
            auto child = std::unique_ptr<DuiControl>(new FuzzNode());
            DuiControl* raw = child.get();
            parent->AddChild(std::move(child));
            live.push_back(raw);
        }
        else
        {
            // 40%: RemoveChild on a random live node.
            int i = rnd() % (unsigned)live.size();
            DuiControl* victim = live[i];
            DuiControl* parent = victim->GetParent();
            // Remove tracking entries for the victim and *all of its
            // descendants* up front, since RemoveChild deletes the
            // whole subtree. Detect by walking the live list and
            // dropping anything whose parent chain reaches victim.
            std::vector<DuiControl*> survivors;
            survivors.reserve(live.size());
            for (auto* n : live)
            {
                if (n == victim)
                {
                    continue;
                }
                bool isDescendant = false;
                for (DuiControl* p = n->GetParent(); p; p = p->GetParent())
                {
                    if (p == victim)
                    {
                        isDescendant = true;
                        break;
                    }
                }
                if (!isDescendant)
                {
                    survivors.push_back(n);
                }
            }
            live.swap(survivors);
            if (parent)
            {
                parent->RemoveChild(victim);
            }
        }
    }

    // Sanity: every live node still reports a parent that is either &root
    // or another live entry (or null when the parent IS root, depending
    // on the GetParent semantics - root itself returns null parent).
    for (auto* n : live)
    {
        DuiControl* p = n->GetParent();
        if (p != &root)
        {
            bool found = (p == nullptr);    // becomes true if parent is root-via-null
            for (auto* m : live)
            {
                if (m == p)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return Fail(_T("Fuzz/danglingParent"), _T("parent not in live set"));
            }
        }
    }
    return OK(_T("TreeMutationFuzz"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("HighContrastPreset"),         &Test_HighContrastPreset         },
        { _T("PresetCycleBumpsVersion"),    &Test_PresetCycleBumpsVersion    },
        { _T("UnknownPresetFallsBackLight"),&Test_UnknownPresetFallsBackLight},
        { _T("DrawRingSmoke"),              &Test_DrawRingSmoke              },
        { _T("DrawRingDegenerate"),         &Test_DrawRingDegenerate         },
        { _T("DrawRingThemedFollowsTheme"), &Test_DrawRingThemedFollowsTheme },
        { _T("TreeMutationFuzz"),           &Test_TreeMutationFuzz           },
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
    summary.Format(_T("[summary] DuiTier4Tests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiTier4Tests

} // namespace balloonwjui
