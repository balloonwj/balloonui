#include "stdafx.h"
#include "DuiScrollBarTests.h"

#if BUI_FEATURE_SCROLLBAR


namespace balloonwjui {

namespace DuiScrollBarTests {

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

#define EXPECT_RECT(r, L, T, R, B, name) \
    do { const RECT& _rr = (r); \
         if (_rr.left != (L) || _rr.top != (T) || _rr.right != (R) || _rr.bottom != (B)) { \
             CString _d; _d.Format(_T("expected (%d,%d,%d,%d) got (%d,%d,%d,%d)"), \
                 (int)(L),(int)(T),(int)(R),(int)(B), _rr.left, _rr.top, _rr.right, _rr.bottom); \
             return Fail(name, _d); } \
    } while (0)

// Defaults: vertical, range [0,0], page 1, pos 0.
static Result Test_Defaults_NoCrash()
{
    DuiScrollBar sb;
    EXPECT_INT(sb.GetMin(), 0, _T("Defaults/min"));
    EXPECT_INT(sb.GetMax(), 0, _T("Defaults/max"));
    EXPECT_INT(sb.GetPos(), 0, _T("Defaults/pos"));
    EXPECT_INT(sb.GetPage(), 1, _T("Defaults/page"));
    return OK(_T("Defaults_NoCrash"));
}

// Pos clamps into [min, max].
static Result Test_PosClamp()
{
    DuiScrollBar sb;
    sb.SetRange(0, 100);
    sb.SetPage(10);
    sb.SetPos(50);
    EXPECT_INT(sb.GetPos(), 50,  _T("PosClamp/in-range"));
    sb.SetPos(200);
    EXPECT_INT(sb.GetPos(), 100, _T("PosClamp/over-max"));
    sb.SetPos(-5);
    EXPECT_INT(sb.GetPos(), 0,   _T("PosClamp/below-min"));
    return OK(_T("PosClamp"));
}

// Range shrink moves pos.
static Result Test_RangeShrinkClamps()
{
    DuiScrollBar sb;
    sb.SetRange(0, 100);
    sb.SetPos(80);
    sb.SetRange(0, 50);   // shrink
    EXPECT_INT(sb.GetPos(), 50, _T("RangeShrink"));
    return OK(_T("RangeShrink"));
}

// Thumb at top with pos=min.
static Result Test_ThumbAtTop()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });    // 200px vertical track
    sb.SetRange(0, 800);                   // 800-px scrollable
    sb.SetPage(200);                       // 200-px viewport
    sb.SetPos(0);
    RECT t = sb.ComputeThumbRect();
    // thumb size: 200 * 200 / (800+200) = 40 px, at top: 0..40.
    EXPECT_RECT(t, 0, 0, 12, 40, _T("ThumbAtTop"));
    return OK(_T("ThumbAtTop"));
}

// Thumb at bottom with pos=max.
static Result Test_ThumbAtBottom()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(800);
    RECT t = sb.ComputeThumbRect();
    // trackUsable = 200 - 40 = 160; offset = 160 * 800 / 800 = 160.
    EXPECT_RECT(t, 0, 160, 12, 200, _T("ThumbAtBottom"));
    return OK(_T("ThumbAtBottom"));
}

// Thumb mid-position.
static Result Test_ThumbMid()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(400);
    RECT t = sb.ComputeThumbRect();
    // offset = 160 * 400 / 800 = 80 -> thumb 80..120.
    EXPECT_RECT(t, 0, 80, 12, 120, _T("ThumbMid"));
    return OK(_T("ThumbMid"));
}

// MIN_THUMB enforced when content is huge.
static Result Test_ThumbMinimumSize()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 1000000);     // huge
    sb.SetPage(200);
    sb.SetPos(0);
    RECT t = sb.ComputeThumbRect();
    int thumbH = t.bottom - t.top;
    if (thumbH < 18)
    {
        // MIN_THUMB_PX = 18
        CString d;
        d.Format(_T("thumb=%d expected>=18"), thumbH);
        return Fail(_T("ThumbMinimumSize"), d);
    }
    return OK(_T("ThumbMinimumSize"));
}

// Click on track below thumb -> PageDown by `page`.
static Result Test_TrackClickPageDown()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(0);
    // Thumb at 0..40; click at y=100 (below) -> PageDown -> pos = 0 + 200 = 200.
    sb.OnLButtonDown(POINT{ 6, 100 }, 0);
    EXPECT_INT(sb.GetPos(), 200, _T("TrackClickPageDown"));
    return OK(_T("TrackClickPageDown"));
}

// Click on track above thumb when pos>min -> PageUp.
static Result Test_TrackClickPageUp()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(600);     // thumb at 120..160
    sb.OnLButtonDown(POINT{ 6, 50 }, 0);
    // PageUp: pos = 600 - 200 = 400.
    EXPECT_INT(sb.GetPos(), 400, _T("TrackClickPageUp"));
    return OK(_T("TrackClickPageUp"));
}

// Drag thumb: mousedown on thumb, mousemove updates pos linearly.
static Result Test_DragThumb()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(0);                                // thumb 0..40
    sb.OnLButtonDown(POINT{ 6, 10 }, 0);          // grab inside thumb (offset 10)
    sb.OnMouseMove (POINT{ 6, 90 }, 0);          // newThumbStart = 90 - 0 - 10 = 80
    // pos = 0 + 800 * 80 / 160 = 400.
    EXPECT_INT(sb.GetPos(), 400, _T("DragThumb/mid"));
    sb.OnMouseMove(POINT{ 6, 5000 }, 0);         // far below -> clamp to bottom
    EXPECT_INT(sb.GetPos(), 800, _T("DragThumb/clampBottom"));
    sb.OnMouseMove(POINT{ 6, -5000 }, 0);        // far above -> clamp to top
    EXPECT_INT(sb.GetPos(), 0, _T("DragThumb/clampTop"));
    sb.OnLButtonUp(POINT{ 6, 0 }, 0);
    return OK(_T("DragThumb"));
}

// Mouse wheel scrolls by lineSize * 3 in opposite direction.
static Result Test_MouseWheel()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 1000);
    sb.SetPage(200);
    sb.SetLineSize(10);
    sb.SetPos(100);
    sb.OnMouseWheel(POINT{ 0, 0 }, -120, 0);   // scroll down 30
    EXPECT_INT(sb.GetPos(), 130, _T("MouseWheel/down"));
    sb.OnMouseWheel(POINT{ 0, 0 },  120, 0);   // scroll up 30
    EXPECT_INT(sb.GetPos(), 100, _T("MouseWheel/up"));
    return OK(_T("MouseWheel"));
}

// Notify callback fires on pos change.
struct CallbackCounter { int hits = 0; int lastPos = -1; };
static void CountCb(void* user, int pos)
{
    auto* c = static_cast<CallbackCounter*>(user);
    c->hits++;
    c->lastPos = pos;
}
static Result Test_OnScrollCallback()
{
    DuiScrollBar sb;
    sb.SetRange(0, 100);
    CallbackCounter c;
    sb.SetOnScroll(&CountCb, &c);
    sb.SetPos(50);
    EXPECT_INT(c.hits, 1, _T("Callback/firstHit"));
    EXPECT_INT(c.lastPos, 50, _T("Callback/firstPos"));
    sb.SetPos(50);   // no-op; same pos
    EXPECT_INT(c.hits, 1, _T("Callback/dedup"));
    sb.SetPos(75);
    EXPECT_INT(c.hits, 2, _T("Callback/secondHit"));
    sb.SetPos(0, /*notify=*/false);
    EXPECT_INT(c.hits, 2, _T("Callback/silentSetPos"));
    return OK(_T("OnScrollCallback"));
}

// Empty range (nothing to scroll): paint and clicks are safe; pos stays at min.
static Result Test_EmptyRange()
{
    DuiScrollBar sb;
    sb.SetRect(RECT{ 0, 0, 12, 200 });
    sb.SetRange(0, 0);
    sb.SetPage(200);
    sb.OnLButtonDown(POINT{ 6, 100 }, 0);
    EXPECT_INT(sb.GetPos(), 0, _T("EmptyRange/clickNoMove"));
    return OK(_T("EmptyRange"));
}

// Horizontal: same math, X-axis instead of Y.
static Result Test_Horizontal()
{
    DuiScrollBar sb(/*horizontal=*/true);
    sb.SetRect(RECT{ 0, 0, 200, 12 });
    sb.SetRange(0, 800);
    sb.SetPage(200);
    sb.SetPos(400);
    RECT t = sb.ComputeThumbRect();
    // Same as ThumbMid but on X.
    EXPECT_RECT(t, 80, 0, 120, 12, _T("Horizontal/thumb"));
    return OK(_T("Horizontal"));
}

#undef EXPECT_INT
#undef EXPECT_RECT

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults_NoCrash"),     &Test_Defaults_NoCrash     },
        { _T("PosClamp"),             &Test_PosClamp             },
        { _T("RangeShrinkClamps"),    &Test_RangeShrinkClamps    },
        { _T("ThumbAtTop"),           &Test_ThumbAtTop           },
        { _T("ThumbAtBottom"),        &Test_ThumbAtBottom        },
        { _T("ThumbMid"),             &Test_ThumbMid             },
        { _T("ThumbMinimumSize"),     &Test_ThumbMinimumSize     },
        { _T("TrackClickPageDown"),   &Test_TrackClickPageDown   },
        { _T("TrackClickPageUp"),     &Test_TrackClickPageUp     },
        { _T("DragThumb"),            &Test_DragThumb            },
        { _T("MouseWheel"),           &Test_MouseWheel           },
        { _T("OnScrollCallback"),     &Test_OnScrollCallback     },
        { _T("EmptyRange"),           &Test_EmptyRange           },
        { _T("Horizontal"),           &Test_Horizontal           }
    };

    CString out;
    int passed = 0, failed = 0;
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
    summary.Format(_T("[summary] DuiScrollBarTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiScrollBarTests

} // namespace balloonwjui

#endif // BUI_FEATURE_SCROLLBAR
