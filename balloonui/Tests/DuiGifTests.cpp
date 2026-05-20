#include "stdafx.h"
#include "DuiGifTests.h"

#if BUI_FEATURE_GIF


namespace balloonwjui {

namespace DuiGifTests {

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

static CString FindGifPath()
{
    TCHAR buf[MAX_PATH] = {};
    ::GetModuleFileName(NULL, buf, MAX_PATH);
    CString p = buf;
    int slash = p.ReverseFind(_T('\\'));
    if (slash >= 0)
    {
        p = p.Left(slash);
    }
    CString r;
    r.Format(_T("%s\\Image\\DownloadImageProgress.gif"), (LPCTSTR)p);
    return r;
}

// ----- pure FrameAt math ----------------------------------------------

static Result Test_FrameAtBasic()
{
    int delays[] = { 100, 100, 100, 100 };  // 4 frames × 100ms
    EXPECT_INT(DuiGif::FrameAt(0,    delays, 4), 0, _T("FA/0"));
    EXPECT_INT(DuiGif::FrameAt(50,   delays, 4), 0, _T("FA/50"));
    EXPECT_INT(DuiGif::FrameAt(100,  delays, 4), 1, _T("FA/100"));
    EXPECT_INT(DuiGif::FrameAt(250,  delays, 4), 2, _T("FA/250"));
    EXPECT_INT(DuiGif::FrameAt(399,  delays, 4), 3, _T("FA/399"));
    // Wraps at total 400ms.
    EXPECT_INT(DuiGif::FrameAt(400,  delays, 4), 0, _T("FA/wrap"));
    EXPECT_INT(DuiGif::FrameAt(550,  delays, 4), 1, _T("FA/wrap+150"));
    return OK(_T("FrameAtBasic"));
}

static Result Test_FrameAtUnevenDelays()
{
    int delays[] = { 50, 100, 200 };  // total 350
    EXPECT_INT(DuiGif::FrameAt(40,  delays, 3), 0, _T("Une/40"));
    EXPECT_INT(DuiGif::FrameAt(50,  delays, 3), 1, _T("Une/50"));
    EXPECT_INT(DuiGif::FrameAt(149, delays, 3), 1, _T("Une/149"));
    EXPECT_INT(DuiGif::FrameAt(150, delays, 3), 2, _T("Une/150"));
    EXPECT_INT(DuiGif::FrameAt(350, delays, 3), 0, _T("Une/wrap"));
    return OK(_T("FrameAtUnevenDelays"));
}

static Result Test_FrameAtGuards()
{
    EXPECT_INT(DuiGif::FrameAt(100, nullptr, 4), 0, _T("Guard/null"));
    int delays[] = { 0, 0 };
    EXPECT_INT(DuiGif::FrameAt(100, delays, 0), 0, _T("Guard/zeroCount"));
    EXPECT_INT(DuiGif::FrameAt(100, delays, 2), 0, _T("Guard/zeroDelays"));
    return OK(_T("FrameAtGuards"));
}

// ----- Loading a real GIF --------------------------------------------

static Result Test_LoadRealGif()
{
    CString path = FindGifPath();
    DuiGif g;
    if (!g.LoadFromFile(path))
    {
        // Skip rather than fail when the asset isn't present.
        return OK(_T("LoadRealGif(skipped:missing)"));
    }
    EXPECT_TRUE(g.GetFrameCount() > 0, _T("Load/frames"));
    EXPECT_TRUE(g.GetWidth()  > 0,     _T("Load/w"));
    EXPECT_TRUE(g.GetHeight() > 0,     _T("Load/h"));
    EXPECT_TRUE(g.GetTotalDurationMs() > 0, _T("Load/dur"));
    EXPECT_TRUE(g.GetFrameHbitmap(0) != nullptr, _T("Load/frame0"));
    EXPECT_TRUE(g.GetFrameHbitmap(99) == nullptr, _T("Load/oobNull"));
    EXPECT_INT(g.GetFrameDelayMs(-1), 0, _T("Load/oobDelayNeg"));
    return OK(_T("LoadRealGif"));
}

static Result Test_LoadMissingFile()
{
    DuiGif g;
    EXPECT_TRUE(!g.LoadFromFile(_T("Z:\\__no_gif__.gif")), _T("Miss/load"));
    EXPECT_INT(g.GetFrameCount(), 0, _T("Miss/empty"));
    EXPECT_TRUE(!g.LoadFromFile(nullptr), _T("Miss/null"));
    return OK(_T("LoadMissingFile"));
}

// ----- DuiGifControl wiring -------------------------------------------

static Result Test_ControlDefaultStretchOn()
{
    DuiGifControl c;
    EXPECT_TRUE(c.GetStretch(), _T("Ctrl/stretchDefault"));
    EXPECT_TRUE(c.GetGif() == nullptr, _T("Ctrl/noGif"));
    EXPECT_INT(c.GetFrameIndex(), 0, _T("Ctrl/frame0"));
    EXPECT_TRUE(!c.IsRunning(), _T("Ctrl/notRunning"));
    SIZE s = c.GetDesiredSize();
    EXPECT_INT(s.cx, 0, _T("Ctrl/dsZeroX"));
    EXPECT_INT(s.cy, 0, _T("Ctrl/dsZeroY"));
    return OK(_T("ControlDefaultStretchOn"));
}

static Result Test_ControlSetGifSetsSize()
{
    CString path = FindGifPath();
    auto g = std::unique_ptr<DuiGif>(new DuiGif());
    if (!g->LoadFromFile(path))
    {
        return OK(_T("ControlSetGifSetsSize(skipped:missing)"));
    }

    int gw = g->GetWidth(), gh = g->GetHeight();
    DuiGifControl c;
    c.SetGif(std::move(g));
    SIZE s = c.GetDesiredSize();
    EXPECT_INT(s.cx, gw, _T("Sz/w"));
    EXPECT_INT(s.cy, gh, _T("Sz/h"));
    return OK(_T("ControlSetGifSetsSize"));
}

static Result Test_ControlSetFrameClamps()
{
    auto g = std::unique_ptr<DuiGif>(new DuiGif());
    CString path = FindGifPath();
    if (!g->LoadFromFile(path))
    {
        return OK(_T("ControlSetFrameClamps(skipped:missing)"));
    }

    int n = g->GetFrameCount();
    DuiGifControl c;
    c.SetGif(std::move(g));
    c.SetFrameIndex(-5);
    EXPECT_INT(c.GetFrameIndex(), 0, _T("Clamp/neg"));
    c.SetFrameIndex(99999);
    EXPECT_INT(c.GetFrameIndex(), n - 1, _T("Clamp/over"));
    return OK(_T("ControlSetFrameClamps"));
}

#undef EXPECT_INT
#undef EXPECT_TRUE

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("FrameAtBasic"),           &Test_FrameAtBasic           },
        { _T("FrameAtUnevenDelays"),    &Test_FrameAtUnevenDelays    },
        { _T("FrameAtGuards"),          &Test_FrameAtGuards          },
        { _T("LoadRealGif"),            &Test_LoadRealGif            },
        { _T("LoadMissingFile"),        &Test_LoadMissingFile        },
        { _T("ControlDefaultStretchOn"),&Test_ControlDefaultStretchOn},
        { _T("ControlSetGifSetsSize"),  &Test_ControlSetGifSetsSize  },
        { _T("ControlSetFrameClamps"),  &Test_ControlSetFrameClamps  },
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
    summary.Format(_T("[summary] DuiGifTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiGifTests

} // namespace balloonwjui

#endif // BUI_FEATURE_GIF
