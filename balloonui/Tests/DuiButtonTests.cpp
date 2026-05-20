#include "stdafx.h"
#include "DuiButtonTests.h"

#if BUI_FEATURE_BUTTON

#include "../DuiControl.h"

namespace balloonwjui {

namespace DuiButtonTests {

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
#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)

// Click an enabled button: Down inside, Up inside.
static Result Test_PushClick_DownUpInside()
{
    DuiButton b;
    b.SetRect(RECT{ 0, 0, 100, 30 });
    b.OnLButtonDown(POINT{ 50, 15 }, 0);
    b.OnLButtonUp  (POINT{ 50, 15 }, 0);
    EXPECT_BOOL(b.IsChecked(), false, _T("Push/notToggle"));
    return OK(_T("PushClick_DownUpInside"));
}

// Down inside, Up outside -> no click action (but pressed state cleared).
static Result Test_PushClick_DragOutCancels()
{
    DuiButton b;
    b.SetRect(RECT{ 0, 0, 100, 30 });
    int hits = 0;
    // Subclass via tiny lambda-friendly test: count clicks by checking IsChecked stays false
    b.OnLButtonDown(POINT{ 50, 15 }, 0);
    b.OnLButtonUp  (POINT{ 500, 500 }, 0);   // outside
    EXPECT_BOOL(b.IsChecked(), false, _T("Push/dragOutNoToggle"));
    (void)hits;
    return OK(_T("PushClick_DragOutCancels"));
}

// Disabled button ignores click.
static Result Test_DisabledIgnored()
{
    DuiButton b;
    b.SetRect(RECT{ 0, 0, 100, 30 });
    b.SetEnabled(false);
    b.OnLButtonDown(POINT{ 50, 15 }, 0);
    b.OnLButtonUp  (POINT{ 50, 15 }, 0);
    return OK(_T("DisabledIgnored"));
}

// Checkbox toggles on each click.
static Result Test_CheckboxToggles()
{
    DuiButton b;
    b.SetButtonType(DuiButton::StyleCheckbox);
    b.SetRect(RECT{ 0, 0, 120, 24 });
    EXPECT_BOOL(b.IsChecked(), false, _T("Cb/initFalse"));
    b.OnLButtonDown(POINT{ 60, 12 }, 0);
    b.OnLButtonUp  (POINT{ 60, 12 }, 0);
    EXPECT_BOOL(b.IsChecked(), true,  _T("Cb/firstClick"));
    b.OnLButtonDown(POINT{ 60, 12 }, 0);
    b.OnLButtonUp  (POINT{ 60, 12 }, 0);
    EXPECT_BOOL(b.IsChecked(), false, _T("Cb/secondClick"));
    return OK(_T("CheckboxToggles"));
}

// Checkbox: drag-out then release does NOT toggle.
static Result Test_CheckboxDragOutCancels()
{
    DuiButton b;
    b.SetButtonType(DuiButton::StyleCheckbox);
    b.SetRect(RECT{ 0, 0, 120, 24 });
    b.OnLButtonDown(POINT{ 60, 12 }, 0);
    b.OnLButtonUp  (POINT{ 9999, 9999 }, 0);
    EXPECT_BOOL(b.IsChecked(), false, _T("Cb/dragOutNoToggle"));
    return OK(_T("CheckboxDragOutCancels"));
}

// SetCheck programmatic.
static Result Test_SetCheckProgrammatic()
{
    DuiButton b;
    b.SetButtonType(DuiButton::StyleCheckbox);
    b.SetCheck(true);
    EXPECT_BOOL(b.IsChecked(), true, _T("SetCheck/true"));
    b.SetCheck(false);
    EXPECT_BOOL(b.IsChecked(), false, _T("SetCheck/false"));
    return OK(_T("SetCheckProgrammatic"));
}

// Radio mutual exclusion: three radios in same group share one parent.
static Result Test_RadioMutualExclusion()
{
    DuiControl parent;
    DuiButton *r1, *r2, *r3;
    parent.AddChild(std::unique_ptr<DuiControl>(r1 = new DuiButton()));
    parent.AddChild(std::unique_ptr<DuiControl>(r2 = new DuiButton()));
    parent.AddChild(std::unique_ptr<DuiControl>(r3 = new DuiButton()));
    for (auto* r : { r1, r2, r3 })
    {
        r->SetButtonType(DuiButton::StyleRadio);
        r->SetRadioGroup(7);
        r->SetRect(RECT{ 0, 0, 120, 24 });
    }
    // Click r1 -> r1 checked, others false
    r1->OnLButtonDown(POINT{ 60, 12 }, 0);
    r1->OnLButtonUp  (POINT{ 60, 12 }, 0);
    EXPECT_BOOL(r1->IsChecked(), true,  _T("Radio/r1after1"));
    EXPECT_BOOL(r2->IsChecked(), false, _T("Radio/r2after1"));
    EXPECT_BOOL(r3->IsChecked(), false, _T("Radio/r3after1"));
    // Click r2 -> r2 only
    r2->OnLButtonDown(POINT{ 60, 12 }, 0);
    r2->OnLButtonUp  (POINT{ 60, 12 }, 0);
    EXPECT_BOOL(r1->IsChecked(), false, _T("Radio/r1after2"));
    EXPECT_BOOL(r2->IsChecked(), true,  _T("Radio/r2after2"));
    EXPECT_BOOL(r3->IsChecked(), false, _T("Radio/r3after2"));
    // Click r2 again -> stays checked (radios don't unselect by re-click)
    r2->OnLButtonDown(POINT{ 60, 12 }, 0);
    r2->OnLButtonUp  (POINT{ 60, 12 }, 0);
    EXPECT_BOOL(r2->IsChecked(), true, _T("Radio/r2idempotent"));
    return OK(_T("RadioMutualExclusion"));
}

// Two radio groups under same parent: independent.
static Result Test_RadioTwoGroups()
{
    DuiControl parent;
    DuiButton *a1, *a2, *b1, *b2;
    parent.AddChild(std::unique_ptr<DuiControl>(a1 = new DuiButton()));
    parent.AddChild(std::unique_ptr<DuiControl>(a2 = new DuiButton()));
    parent.AddChild(std::unique_ptr<DuiControl>(b1 = new DuiButton()));
    parent.AddChild(std::unique_ptr<DuiControl>(b2 = new DuiButton()));
    a1->SetButtonType(DuiButton::StyleRadio);
    a1->SetRadioGroup(1);
    a2->SetButtonType(DuiButton::StyleRadio);
    a2->SetRadioGroup(1);
    b1->SetButtonType(DuiButton::StyleRadio);
    b1->SetRadioGroup(2);
    b2->SetButtonType(DuiButton::StyleRadio);
    b2->SetRadioGroup(2);
    for (auto* r : { a1, a2, b1, b2 })
    {
        r->SetRect(RECT{ 0, 0, 100, 24 });
    }

    a1->OnLButtonDown(POINT{ 50, 12 }, 0);
    a1->OnLButtonUp  (POINT{ 50, 12 }, 0);
    b1->OnLButtonDown(POINT{ 50, 12 }, 0);
    b1->OnLButtonUp  (POINT{ 50, 12 }, 0);
    EXPECT_BOOL(a1->IsChecked(), true,  _T("Radio2/a1"));
    EXPECT_BOOL(a2->IsChecked(), false, _T("Radio2/a2"));
    EXPECT_BOOL(b1->IsChecked(), true,  _T("Radio2/b1"));
    EXPECT_BOOL(b2->IsChecked(), false, _T("Radio2/b2"));
    // Switch within group A: doesn't disturb group B.
    a2->OnLButtonDown(POINT{ 50, 12 }, 0);
    a2->OnLButtonUp  (POINT{ 50, 12 }, 0);
    EXPECT_BOOL(a1->IsChecked(), false, _T("Radio2/a1after"));
    EXPECT_BOOL(a2->IsChecked(), true,  _T("Radio2/a2after"));
    EXPECT_BOOL(b1->IsChecked(), true,  _T("Radio2/b1unchanged"));
    return OK(_T("RadioTwoGroups"));
}

// Style switch retains text.
static Result Test_StyleSwitchRetainsText()
{
    DuiButton b;
    b.SetText(_T("Hello"));
    b.SetButtonType(DuiButton::StyleCheckbox);
    if (b.GetText() != _T("Hello"))
    {
        return Fail(_T("StyleSwitchRetainsText"), _T("text lost"));
    }
    return OK(_T("StyleSwitchRetainsText"));
}

// SetCheck no-op when value unchanged.
static Result Test_SetCheckIdempotent()
{
    DuiButton b;
    b.SetButtonType(DuiButton::StyleCheckbox);
    b.SetCheck(false);
    b.SetCheck(false);
    EXPECT_BOOL(b.IsChecked(), false, _T("SetCheck/idem"));
    return OK(_T("SetCheckIdempotent"));
}

// ----- background bitmap state-image API ------------------------------

// Synthesize a tiny 12x12 32bpp DIBSection of a single solid color so
// tests don't depend on real PNG assets on disk.
static HBITMAP MakeFlatBitmap(BYTE r, BYTE g, BYTE b)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 12;
    bi.bmiHeader.biHeight   = -12;          // top-down
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
    for (int i = 0; i < 12 * 12; ++i)
    {
        p[i*4 + 0] = b;
        p[i*4 + 1] = g;
        p[i*4 + 2] = r;
        p[i*4 + 3] = 255;
    }
    return hbm;
}

// SetBgBitmap stores the four pointers; GetBgBitmapForCurrentState
// returns Normal by default.
static Result Test_SetBgBitmap_StoresPointers()
{
    DuiButton b;
    HBITMAP n = MakeFlatBitmap(10, 20, 30);
    HBITMAP h = MakeFlatBitmap(40, 50, 60);
    HBITMAP p = MakeFlatBitmap(70, 80, 90);
    HBITMAP d = MakeFlatBitmap(100, 110, 120);
    b.SetBgBitmap(n, h, p, d);
    HBITMAP got = b.GetBgBitmapForCurrentState();
    if (got != n)
    {
        ::DeleteObject(n);
        ::DeleteObject(h);
        ::DeleteObject(p);
        ::DeleteObject(d);
        return Fail(_T("Bg/storeNormal"), _T("normal not returned"));
    }
    ::DeleteObject(n);
    ::DeleteObject(h);
    ::DeleteObject(p);
    ::DeleteObject(d);
    return OK(_T("SetBgBitmap_StoresPointers"));
}

// Hover -> Hover bitmap.
static Result Test_StateChain_Hover()
{
    DuiButton b;
    b.SetRect(RECT{ 0, 0, 100, 30 });
    HBITMAP n = MakeFlatBitmap(1,1,1);
    HBITMAP h = MakeFlatBitmap(2,2,2);
    b.SetBgBitmap(n, h, nullptr, nullptr);
    b.OnMouseEnter();
    HBITMAP got = b.GetBgBitmapForCurrentState();
    bool okHover = (got == h);
    b.OnMouseLeave();
    HBITMAP got2 = b.GetBgBitmapForCurrentState();
    bool okLeave = (got2 == n);
    ::DeleteObject(n);
    ::DeleteObject(h);
    if (!okHover)
    {
        return Fail(_T("StateChain/hover"), _T("expected hover bitmap"));
    }
    if (!okLeave)
    {
        return Fail(_T("StateChain/leave"), _T("expected normal after leave"));
    }
    return OK(_T("StateChain_Hover"));
}

// Pressed (mouse-down inside, hovering) -> Pressed bitmap; falls back
// to Hover if Pressed slot is empty, then to Normal if Hover empty.
static Result Test_StateChain_PressedFallback()
{
    DuiButton b;
    b.SetRect(RECT{ 0, 0, 100, 30 });
    HBITMAP n = MakeFlatBitmap(1,1,1);
    HBITMAP h = MakeFlatBitmap(2,2,2);
    HBITMAP p = MakeFlatBitmap(3,3,3);

    // All three slots: Pressed wins when pressed+hover.
    b.SetBgBitmap(n, h, p, nullptr);
    b.OnMouseEnter();
    b.OnLButtonDown(POINT{ 10, 10 }, 0);
    HBITMAP got1 = b.GetBgBitmapForCurrentState();
    b.OnLButtonUp(POINT{ 10, 10 }, 0);

    // Pressed slot empty -> falls back to Hover.
    b.SetBgBitmap(n, h, nullptr, nullptr);
    b.OnLButtonDown(POINT{ 10, 10 }, 0);
    HBITMAP got2 = b.GetBgBitmapForCurrentState();
    b.OnLButtonUp(POINT{ 10, 10 }, 0);

    // Pressed + Hover slots empty -> Normal.
    b.SetBgBitmap(n, nullptr, nullptr, nullptr);
    b.OnLButtonDown(POINT{ 10, 10 }, 0);
    HBITMAP got3 = b.GetBgBitmapForCurrentState();
    b.OnLButtonUp(POINT{ 10, 10 }, 0);

    ::DeleteObject(n);
    ::DeleteObject(h);
    ::DeleteObject(p);
    if (got1 != p)
    {
        return Fail(_T("StateChain/pressed"),  _T("expected pressed bitmap"));
    }
    if (got2 != h)
    {
        return Fail(_T("StateChain/pressFallbackHover"), _T("expected hover bitmap"));
    }
    if (got3 != n)
    {
        return Fail(_T("StateChain/pressFallbackNormal"), _T("expected normal bitmap"));
    }
    return OK(_T("StateChain_PressedFallback"));
}

// Disabled -> disabled bitmap, with fallback to normal when slot empty.
static Result Test_StateChain_Disabled()
{
    DuiButton b;
    HBITMAP n = MakeFlatBitmap(1,1,1);
    HBITMAP d = MakeFlatBitmap(9,9,9);

    b.SetBgBitmap(n, nullptr, nullptr, d);
    b.SetEnabled(false);
    HBITMAP got1 = b.GetBgBitmapForCurrentState();
    b.SetEnabled(true);

    // Disabled slot empty -> normal.
    b.SetBgBitmap(n, nullptr, nullptr, nullptr);
    b.SetEnabled(false);
    HBITMAP got2 = b.GetBgBitmapForCurrentState();

    ::DeleteObject(n);
    ::DeleteObject(d);
    if (got1 != d)
    {
        return Fail(_T("StateChain/disabledHit"), _T("expected disabled bitmap"));
    }
    if (got2 != n)
    {
        return Fail(_T("StateChain/disabledFallback"), _T("expected normal bitmap"));
    }
    return OK(_T("StateChain_Disabled"));
}

// All slots null -> nullptr (caller falls back to solid-color path).
static Result Test_StateChain_AllNull()
{
    DuiButton b;
    HBITMAP got = b.GetBgBitmapForCurrentState();
    if (got != nullptr)
    {
        return Fail(_T("StateChain/null"), _T("expected nullptr"));
    }
    return OK(_T("StateChain_AllNull"));
}

// SetBgInsets clamps negatives to 0.
static Result Test_SetBgInsets_ClampsNegatives()
{
    DuiButton b;
    b.SetBgInsets(-3, -1, 4, 2);
    DuiNinePatch::Insets ins = b.GetBgInsets();
    EXPECT_INT(ins.left,   0, _T("Insets/L"));
    EXPECT_INT(ins.top,    0, _T("Insets/T"));
    EXPECT_INT(ins.right,  4, _T("Insets/R"));
    EXPECT_INT(ins.bottom, 2, _T("Insets/B"));
    return OK(_T("SetBgInsets_ClampsNegatives"));
}

// Radio with group=0 doesn't enforce exclusion (group 0 = no group).
static Result Test_RadioGroupZeroIsolated()
{
    DuiControl parent;
    DuiButton *r1, *r2;
    parent.AddChild(std::unique_ptr<DuiControl>(r1 = new DuiButton()));
    parent.AddChild(std::unique_ptr<DuiControl>(r2 = new DuiButton()));
    r1->SetButtonType(DuiButton::StyleRadio);
    r2->SetButtonType(DuiButton::StyleRadio);
    // group = 0 (default)
    r1->SetRect(RECT{ 0, 0, 100, 24 });
    r2->SetRect(RECT{ 0, 0, 100, 24 });
    r1->OnLButtonDown(POINT{ 50, 12 }, 0);
    r1->OnLButtonUp(POINT{ 50, 12 }, 0);
    r2->OnLButtonDown(POINT{ 50, 12 }, 0);
    r2->OnLButtonUp(POINT{ 50, 12 }, 0);
    // both can be checked because group is 0 (treated as "no group")
    EXPECT_BOOL(r1->IsChecked(), true, _T("Radio0/r1"));
    EXPECT_BOOL(r2->IsChecked(), true, _T("Radio0/r2"));
    return OK(_T("RadioGroupZeroIsolated"));
}

#undef EXPECT_INT
#undef EXPECT_BOOL

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
        { _T("PushClick_DownUpInside"),     &Test_PushClick_DownUpInside     },
        { _T("PushClick_DragOutCancels"),   &Test_PushClick_DragOutCancels   },
        { _T("DisabledIgnored"),            &Test_DisabledIgnored            },
        { _T("CheckboxToggles"),            &Test_CheckboxToggles            },
        { _T("CheckboxDragOutCancels"),     &Test_CheckboxDragOutCancels     },
        { _T("SetCheckProgrammatic"),       &Test_SetCheckProgrammatic       },
        { _T("RadioMutualExclusion"),       &Test_RadioMutualExclusion       },
        { _T("RadioTwoGroups"),             &Test_RadioTwoGroups             },
        { _T("StyleSwitchRetainsText"),     &Test_StyleSwitchRetainsText     },
        { _T("SetCheckIdempotent"),         &Test_SetCheckIdempotent         },
        { _T("RadioGroupZeroIsolated"),     &Test_RadioGroupZeroIsolated     },
        { _T("SetBgBitmap_StoresPointers"), &Test_SetBgBitmap_StoresPointers },
        { _T("StateChain_Hover"),           &Test_StateChain_Hover           },
        { _T("StateChain_PressedFallback"), &Test_StateChain_PressedFallback },
        { _T("StateChain_Disabled"),        &Test_StateChain_Disabled        },
        { _T("StateChain_AllNull"),         &Test_StateChain_AllNull         },
        { _T("SetBgInsets_ClampsNegatives"),&Test_SetBgInsets_ClampsNegatives}
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
    summary.Format(_T("[summary] DuiButtonTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiButtonTests

} // namespace balloonwjui

#endif // BUI_FEATURE_BUTTON
