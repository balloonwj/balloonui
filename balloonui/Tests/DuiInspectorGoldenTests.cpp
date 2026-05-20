#include "stdafx.h"
#include "DuiInspectorGoldenTests.h"

#if BUI_FEATURE_BUTTON && BUI_FEATURE_LABEL

#include "../Controls/Basic/DuiButton.h"
#include "../Controls/Basic/DuiLabel.h"

namespace balloonwjui {

namespace DuiInspectorGoldenTests {

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
#define EXPECT_STR_CONTAINS(s, sub, name) \
    do { CString _s = (s); if (_s.Find(sub) < 0) return Fail(name, _T("substring missing: ") + CString(sub)); } while (0)

// ----- DuiInspector ---------------------------------------------------

static Result Test_InspectorEnableToggles()
{
    DuiInspector::Inst().Enable(true);
    EXPECT_TRUE(DuiInspector::Inst().IsEnabled(), _T("Insp/on"));
    DuiInspector::Inst().Enable(false);
    EXPECT_TRUE(!DuiInspector::Inst().IsEnabled(), _T("Insp/off"));
    return OK(_T("InspectorEnableToggles"));
}

static Result Test_InspectorFormatNullSafe()
{
    CString s = DuiInspector::FormatControlInfo(nullptr);
    EXPECT_TRUE(s.IsEmpty(), _T("Fmt/nullEmpty"));
    return OK(_T("InspectorFormatNullSafe"));
}

static Result Test_InspectorFormatIncludesIdAndRect()
{
    DuiButton b;
    b.SetCtrlId(42);
    b.SetRect(RECT{ 10, 20, 100, 50 });
    CString s = DuiInspector::FormatControlInfo(&b);
    EXPECT_STR_CONTAINS(s, _T("id=42"),         _T("Fmt/id"));
    EXPECT_STR_CONTAINS(s, _T("(10,20,100,50)"),_T("Fmt/rect"));
    return OK(_T("InspectorFormatIncludesIdAndRect"));
}

static Result Test_InspectorCollectVisibleRoot()
{
    DuiButton b;
    b.SetRect(RECT{ 0, 0, 80, 24 });
    std::vector<RECT> rects;
    DuiInspector::CollectVisibleRects(&b, rects);
    EXPECT_INT((int)rects.size(), 1, _T("Coll/oneRect"));
    EXPECT_INT(rects[0].right - rects[0].left, 80, _T("Coll/w"));

    // Invisible root → no rects emitted.
    b.SetVisible(false);
    rects.clear();
    DuiInspector::CollectVisibleRects(&b, rects);
    EXPECT_INT((int)rects.size(), 0, _T("Coll/hiddenSkipped"));
    return OK(_T("InspectorCollectVisibleRoot"));
}

// PaintOverlay with disabled inspector is a no-op; with null host / hdc
// is also safe.
static Result Test_InspectorPaintNoOpWhenDisabled()
{
    DuiInspector::Inst().Enable(false);
    DuiInspector::Inst().PaintOverlay(nullptr, nullptr);
    return OK(_T("InspectorPaintNoOpWhenDisabled"));
}

// ----- DuiGolden helpers ---------------------------------------------

// Build a tiny synthetic 32bpp DIB filled with one solid color.
static HBITMAP MakeFlatHbm(int w, int h, BYTE r, BYTE g, BYTE b)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = w;
    bi.bmiHeader.biHeight   = -h;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int i = 0; i < w * h; ++i)
    {
        p[i*4] = b;
        p[i*4+1] = g;
        p[i*4+2] = r;
        p[i*4+3] = 255;
    }
    return hbm;
}

static Result Test_GoldenCompareIdentical()
{
    HBITMAP a = MakeFlatHbm(20, 10, 100, 150, 200);
    HBITMAP b = MakeFlatHbm(20, 10, 100, 150, 200);
    DuiGolden::DiffSummary s = DuiGolden::CompareHbm(a, b, 0);
    EXPECT_TRUE(!s.sizeMismatch, _T("Cmp/size"));
    EXPECT_INT(s.diffPixels, 0, _T("Cmp/zero"));
    EXPECT_INT(s.maxChannelDelta, 0, _T("Cmp/maxZero"));
    EXPECT_INT(s.totalPixels, 200, _T("Cmp/total"));
    ::DeleteObject(a);
    ::DeleteObject(b);
    return OK(_T("GoldenCompareIdentical"));
}

static Result Test_GoldenCompareWithinTolerance()
{
    HBITMAP a = MakeFlatHbm(8, 8, 100, 100, 100);
    HBITMAP b = MakeFlatHbm(8, 8, 102, 100, 99);
    DuiGolden::DiffSummary s2 = DuiGolden::CompareHbm(a, b, 2);
    EXPECT_INT(s2.diffPixels, 0, _T("Tol/within"));
    EXPECT_INT(s2.maxChannelDelta, 2, _T("Tol/max2"));
    DuiGolden::DiffSummary s0 = DuiGolden::CompareHbm(a, b, 0);
    EXPECT_INT(s0.diffPixels, 64, _T("Tol/exact0"));
    ::DeleteObject(a);
    ::DeleteObject(b);
    return OK(_T("GoldenCompareWithinTolerance"));
}

static Result Test_GoldenCompareSizeMismatch()
{
    HBITMAP a = MakeFlatHbm(10, 10, 0, 0, 0);
    HBITMAP b = MakeFlatHbm(20, 10, 0, 0, 0);
    DuiGolden::DiffSummary s = DuiGolden::CompareHbm(a, b, 0);
    EXPECT_TRUE(s.sizeMismatch, _T("Sz/flag"));
    ::DeleteObject(a);
    ::DeleteObject(b);
    return OK(_T("GoldenCompareSizeMismatch"));
}

// MakeDiffHbm: pixels within tolerance → white; out of tolerance → red.
static Result Test_GoldenDiffMap()
{
    HBITMAP a = MakeFlatHbm(4, 4, 0,   0, 0);
    HBITMAP b = MakeFlatHbm(4, 4, 200, 0, 0);
    HBITMAP diff = DuiGolden::MakeDiffHbm(a, b, 2);
    EXPECT_TRUE(diff != nullptr, _T("Diff/built"));

    // Sample (1,1): expected red marker.
    DIBSECTION ds = {};
    ::GetObject(diff, sizeof(ds), &ds);
    BYTE* p = (BYTE*)ds.dsBm.bmBits;
    int idx = (1 * ds.dsBm.bmWidth + 1) * 4;
    EXPECT_INT((int)p[idx + 2], 255, _T("Diff/red"));   // R
    EXPECT_INT((int)p[idx + 0], 0,   _T("Diff/blue"));  // B
    ::DeleteObject(a);
    ::DeleteObject(b);
    ::DeleteObject(diff);
    return OK(_T("GoldenDiffMap"));
}

// PNG round-trip: render → save → load → compare.
static Result Test_GoldenPngRoundTrip()
{
    HBITMAP a = MakeFlatHbm(12, 8, 50, 100, 200);

    TCHAR tmp[MAX_PATH] = {};
    ::GetTempPath(MAX_PATH, tmp);
    CString path = tmp;
    if (path.IsEmpty() || path[path.GetLength() - 1] != _T('\\'))
    {
        path += _T("\\");
    }
    path += _T("DuiGolden_RoundTrip.png");

    bool saved = DuiGolden::SaveHbmAsPng(a, path);
    if (!saved)
    {
        ::DeleteObject(a);
        return Fail(_T("PngRT/save"), _T("SaveHbmAsPng failed"));
    }

    HBITMAP b = DuiGolden::LoadPngAsHbm(path);
    if (!b)
    {
        ::DeleteObject(a);
        return Fail(_T("PngRT/load"), _T("LoadPngAsHbm failed"));
    }

    DuiGolden::DiffSummary s = DuiGolden::CompareHbm(a, b, 1);
    EXPECT_INT(s.diffPixels, 0, _T("PngRT/equal"));

    ::DeleteObject(a);
    ::DeleteObject(b);
    ::DeleteFile(path);
    return OK(_T("GoldenPngRoundTrip"));
}

// RenderControlToHbm produces a valid HBITMAP for a paint-only control.
static Result Test_GoldenRenderControl()
{
    DuiLabel l;
    l.SetText(_T("Hello"));
    l.SetTextColor(RGB(20, 20, 20));
    HBITMAP h = DuiGolden::RenderControlToHbm(&l, 64, 24);
    EXPECT_TRUE(h != nullptr, _T("Render/notNull"));
    DIBSECTION ds = {};
    ::GetObject(h, sizeof(ds), &ds);
    EXPECT_INT(ds.dsBm.bmWidth,  64, _T("Render/w"));
    EXPECT_INT(ds.dsBm.bmHeight, 24, _T("Render/h"));
    // Note: Windows normalizes dsBmih.biHeight to positive even when the
    // caller passed a negative (top-down) value, so we don't assert
    // direction via DIBSECTION metadata. The actual top-down byte order
    // is verified in the DuiNinePatch corner-color test elsewhere.
    ::DeleteObject(h);
    return OK(_T("GoldenRenderControl"));
}

// ResolveGoldenPath ends in our expected layout.
static Result Test_GoldenResolvePath()
{
    CString p = DuiGolden::ResolveGoldenPath(_T("MyTest"));
    EXPECT_STR_CONTAINS(p, _T("\\Goldens\\"),    _T("Res/dir"));
    EXPECT_STR_CONTAINS(p, _T("MyTest.png"),     _T("Res/file"));
    return OK(_T("GoldenResolvePath"));
}

// End-to-end: RunGoldenTest with an unset DUI_GOLDEN_UPDATE creates the
// file the first time, then the second pass compares-equal.
static Result Test_GoldenRunCreatesThenMatches()
{
    // Use a unique test name so we don't trip over stale goldens.
    LPCTSTR testName = _T("InternalSelfTest_FlatLabel");

    CString golden = DuiGolden::ResolveGoldenPath(testName);
    ::DeleteFile(golden);
    ::SetEnvironmentVariable(_T("DUI_GOLDEN_UPDATE"), nullptr);

    DuiLabel l1;
    l1.SetText(_T(""));         // empty text avoids font-rendering jitter
    l1.SetTextColor(RGB(0, 0, 0));
    bool first = DuiGolden::RunGoldenTest(testName, &l1, 16, 8);
    EXPECT_TRUE(first, _T("Run/firstWrite"));

    DuiLabel l2;
    l2.SetText(_T(""));
    l2.SetTextColor(RGB(0, 0, 0));
    bool second = DuiGolden::RunGoldenTest(testName, &l2, 16, 8);
    EXPECT_TRUE(second, _T("Run/secondMatch"));

    ::DeleteFile(golden);
    return OK(_T("GoldenRunCreatesThenMatches"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE
#undef EXPECT_STR_CONTAINS

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("InspectorEnableToggles"),       &Test_InspectorEnableToggles       },
        { _T("InspectorFormatNullSafe"),      &Test_InspectorFormatNullSafe      },
        { _T("InspectorFormatIncludesIdAndRect"), &Test_InspectorFormatIncludesIdAndRect },
        { _T("InspectorCollectVisibleRoot"),  &Test_InspectorCollectVisibleRoot  },
        { _T("InspectorPaintNoOpWhenDisabled"), &Test_InspectorPaintNoOpWhenDisabled },
        { _T("GoldenCompareIdentical"),       &Test_GoldenCompareIdentical       },
        { _T("GoldenCompareWithinTolerance"), &Test_GoldenCompareWithinTolerance },
        { _T("GoldenCompareSizeMismatch"),    &Test_GoldenCompareSizeMismatch    },
        { _T("GoldenDiffMap"),                &Test_GoldenDiffMap                },
        { _T("GoldenPngRoundTrip"),           &Test_GoldenPngRoundTrip           },
        { _T("GoldenRenderControl"),          &Test_GoldenRenderControl          },
        { _T("GoldenResolvePath"),            &Test_GoldenResolvePath            },
        { _T("GoldenRunCreatesThenMatches"),  &Test_GoldenRunCreatesThenMatches  },
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
    summary.Format(_T("[summary] DuiInspectorGoldenTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiInspectorGoldenTests

} // namespace balloonwjui

#endif // BUI_FEATURE_BUTTON && BUI_FEATURE_LABEL
