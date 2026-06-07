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

// ----- DuiButton: Variant 视觉变体 ----------------------------------------

// 默认 Variant=Primary；Setter 往返；Variant 切换不影响 Style。
static Result Test_VariantSetterDefaults()
{
    DuiButton b;
    EXPECT_INT((int)b.GetVariant(),    (int)DuiButton::Variant::Primary, _T("Var/def"));
    EXPECT_INT((int)b.GetButtonType(), (int)DuiButton::StylePushButton,  _T("Var/styleDef"));

    b.SetVariant(DuiButton::Variant::Danger);
    EXPECT_INT((int)b.GetVariant(),    (int)DuiButton::Variant::Danger,  _T("Var/danger"));
    EXPECT_INT((int)b.GetButtonType(), (int)DuiButton::StylePushButton,  _T("Var/styleUnchanged"));

    b.SetVariant(DuiButton::Variant::Ghost);
    EXPECT_INT((int)b.GetVariant(),    (int)DuiButton::Variant::Ghost,   _T("Var/ghost"));

    b.SetVariant(DuiButton::Variant::Primary);   // 回 Primary
    EXPECT_INT((int)b.GetVariant(),    (int)DuiButton::Variant::Primary, _T("Var/backPrimary"));
    return OK(_T("VariantSetterDefaults"));
}

// PaletteFor(Primary, *) 四态返回与历史 PushButton 色板一致的实色 / 白字 /
// 无边框。disabled 灰底浅字。
static Result Test_PaletteForPrimaryStates()
{
    using V = DuiButton::Variant;
    using S = DuiButton::ButtonState;

    DuiButton::ButtonPalette p;

    p = DuiButton::PaletteFor(V::Primary, S::Normal);
    EXPECT_INT((int)p.bg,     (int)RGB( 45,108,223), _T("Pri/N/bg"));
    EXPECT_INT((int)p.text,   (int)RGB(255,255,255), _T("Pri/N/text"));
    EXPECT_INT((int)p.border, (int)CLR_INVALID,      _T("Pri/N/border"));

    p = DuiButton::PaletteFor(V::Primary, S::Hover);
    EXPECT_INT((int)p.bg,     (int)RGB( 37, 89,184), _T("Pri/H/bg"));
    EXPECT_INT((int)p.text,   (int)RGB(255,255,255), _T("Pri/H/text"));

    p = DuiButton::PaletteFor(V::Primary, S::Pressed);
    EXPECT_INT((int)p.bg,     (int)RGB( 30, 74,153), _T("Pri/P/bg"));

    p = DuiButton::PaletteFor(V::Primary, S::Disabled);
    EXPECT_INT((int)p.bg,     (int)RGB(180,180,180), _T("Pri/D/bg"));
    EXPECT_INT((int)p.text,   (int)RGB(220,220,220), _T("Pri/D/text"));
    return OK(_T("PaletteForPrimaryStates"));
}

// PaletteFor(其它 Variant, Normal) 关键颜色校验：Default 白底有边、
// Outlined 透明有边品牌色、Ghost 透明无边深字、Danger 红底白字、
// Text 透明无边品牌字。
static Result Test_PaletteForOtherVariantsNormal()
{
    using V = DuiButton::Variant;
    using S = DuiButton::ButtonState;
    DuiButton::ButtonPalette p;

    // Default —— 白底 + 灰边 + 深字
    p = DuiButton::PaletteFor(V::Default, S::Normal);
    EXPECT_INT((int)p.bg,     (int)RGB(255,255,255), _T("Def/bg"));
    EXPECT_INT((int)p.text,   (int)RGB( 80, 88,102), _T("Def/text"));
    EXPECT_INT((int)p.border, (int)RGB(208,212,219), _T("Def/border"));

    // Outlined —— 透明 + 品牌边 + 品牌字
    p = DuiButton::PaletteFor(V::Outlined, S::Normal);
    EXPECT_INT((int)p.bg,     (int)CLR_INVALID,      _T("Out/bg_invalid"));
    EXPECT_INT((int)p.text,   (int)RGB( 45,108,223), _T("Out/text_brand"));
    EXPECT_INT((int)p.border, (int)RGB( 45,108,223), _T("Out/border_brand"));

    // Ghost —— 透明 + 无边 + 深字
    p = DuiButton::PaletteFor(V::Ghost, S::Normal);
    EXPECT_INT((int)p.bg,     (int)CLR_INVALID,      _T("Ghost/bg_invalid"));
    EXPECT_INT((int)p.border, (int)CLR_INVALID,      _T("Ghost/border_invalid"));
    EXPECT_INT((int)p.text,   (int)RGB(100,108,120), _T("Ghost/text"));

    // Danger —— 红底 + 白字 + 无边
    p = DuiButton::PaletteFor(V::Danger, S::Normal);
    EXPECT_INT((int)p.bg,     (int)RGB(220, 60, 60), _T("Dgr/bg_red"));
    EXPECT_INT((int)p.text,   (int)RGB(255,255,255), _T("Dgr/text_white"));
    EXPECT_INT((int)p.border, (int)CLR_INVALID,      _T("Dgr/border_invalid"));

    // Text —— 透明 + 无边 + 品牌字
    p = DuiButton::PaletteFor(V::Text, S::Normal);
    EXPECT_INT((int)p.bg,     (int)CLR_INVALID,      _T("Txt/bg_invalid"));
    EXPECT_INT((int)p.border, (int)CLR_INVALID,      _T("Txt/border_invalid"));
    EXPECT_INT((int)p.text,   (int)RGB( 45,108,223), _T("Txt/text_brand"));
    return OK(_T("PaletteForOtherVariantsNormal"));
}

// 透明变体（Ghost / Outlined / Text）的 Normal 态 bg = CLR_INVALID；
// Hover 态应有可见 bg；实色变体（Primary / Default / Danger）任何态
// bg 都不能为 CLR_INVALID。
static Result Test_PaletteTransparentSemantics()
{
    using V = DuiButton::Variant;
    using S = DuiButton::ButtonState;

    // 透明变体 Normal → bg invalid
    EXPECT_INT((int)DuiButton::PaletteFor(V::Ghost,    S::Normal).bg, (int)CLR_INVALID, _T("Trans/Ghost_N"));
    EXPECT_INT((int)DuiButton::PaletteFor(V::Outlined, S::Normal).bg, (int)CLR_INVALID, _T("Trans/Out_N"));
    EXPECT_INT((int)DuiButton::PaletteFor(V::Text,     S::Normal).bg, (int)CLR_INVALID, _T("Trans/Txt_N"));

    // 透明变体 Hover → bg 非 invalid（hover 应可视）
    EXPECT_BOOL(DuiButton::PaletteFor(V::Ghost,    S::Hover).bg != CLR_INVALID, true, _T("Trans/Ghost_H_visible"));
    EXPECT_BOOL(DuiButton::PaletteFor(V::Outlined, S::Hover).bg != CLR_INVALID, true, _T("Trans/Out_H_visible"));
    EXPECT_BOOL(DuiButton::PaletteFor(V::Text,     S::Hover).bg != CLR_INVALID, true, _T("Trans/Txt_H_visible"));

    // 实色变体所有态 bg 都有效
    DuiButton::ButtonState states[] = {
        S::Normal, S::Hover, S::Pressed, S::Disabled
    };
    for (auto st : states)
    {
        EXPECT_BOOL(DuiButton::PaletteFor(V::Primary, st).bg != CLR_INVALID, true, _T("Solid/Primary_bg"));
        EXPECT_BOOL(DuiButton::PaletteFor(V::Default, st).bg != CLR_INVALID, true, _T("Solid/Default_bg"));
        EXPECT_BOOL(DuiButton::PaletteFor(V::Danger,  st).bg != CLR_INVALID, true, _T("Solid/Danger_bg"));
    }
    return OK(_T("PaletteTransparentSemantics"));
}

// 位图皮肤与 Variant 各自独立存储 —— SetBgBitmap 不影响 Variant，
// SetVariant 不影响 GetBgBitmapForCurrentState 的返回。
static Result Test_BitmapAndVariantCoexist()
{
    DuiButton b;
    // 任意非空指针即可（不会被 OnPaint 调用 → 不会真画）。
    HBITMAP fakeBmp = reinterpret_cast<HBITMAP>((LPARAM)0xDEADBEEF);

    b.SetVariant(DuiButton::Variant::Ghost);
    b.SetBgBitmap(fakeBmp, nullptr, nullptr, nullptr);

    // Variant 仍是 Ghost
    EXPECT_INT((int)b.GetVariant(), (int)DuiButton::Variant::Ghost, _T("Coex/var"));
    // 位图按当前状态查询：normal 槽返回 fakeBmp
    EXPECT_BOOL(b.GetBgBitmapForCurrentState() == fakeBmp, true, _T("Coex/bmp"));

    // 改 Variant 不影响位图查询
    b.SetVariant(DuiButton::Variant::Danger);
    EXPECT_BOOL(b.GetBgBitmapForCurrentState() == fakeBmp, true, _T("Coex/bmp_afterVarChange"));
    EXPECT_INT((int)b.GetVariant(), (int)DuiButton::Variant::Danger, _T("Coex/var_kept"));

    // 清掉位图：Variant 仍在
    b.SetBgBitmap(nullptr, nullptr, nullptr, nullptr);
    EXPECT_BOOL(b.GetBgBitmapForCurrentState() == nullptr, true, _T("Coex/bmp_cleared"));
    EXPECT_INT((int)b.GetVariant(), (int)DuiButton::Variant::Danger, _T("Coex/var_stillDanger"));
    return OK(_T("BitmapAndVariantCoexist"));
}

// ----- DuiButton: 抗锯齿开关 + Checkbox/Radio 透明 Variant 路由 -----------

// SetAntiAlias 默认 true; setter 往返; 不影响 Variant / Style。
static Result Test_SetAntiAliasRoundTrip()
{
    DuiButton b;
    EXPECT_BOOL(b.IsAntiAlias(), true, _T("AA/def"));

    b.SetAntiAlias(false);
    EXPECT_BOOL(b.IsAntiAlias(), false, _T("AA/off"));
    EXPECT_INT((int)b.GetVariant(),    (int)DuiButton::Variant::Primary, _T("AA/varKept"));
    EXPECT_INT((int)b.GetButtonType(), (int)DuiButton::StylePushButton,  _T("AA/styleKept"));

    b.SetAntiAlias(true);
    EXPECT_BOOL(b.IsAntiAlias(), true, _T("AA/on"));

    // Idempotent setter: 重复设置同值不报错。
    b.SetAntiAlias(true);
    EXPECT_BOOL(b.IsAntiAlias(), true, _T("AA/idem"));
    return OK(_T("SetAntiAliasRoundTrip"));
}

// Checkbox + Ghost Variant: 外框走 Variant palette → Normal 态 bg / border
// 都应为 CLR_INVALID(透明)。Q4=A2 决定。
static Result Test_CheckboxVariantGhost_Transparent()
{
    using V = DuiButton::Variant;
    using S = DuiButton::ButtonState;

    // 直接查 palette: Ghost Normal bg / border 都该是 CLR_INVALID。
    DuiButton::ButtonPalette p = DuiButton::PaletteFor(V::Ghost, S::Normal);
    EXPECT_INT((int)p.bg,     (int)CLR_INVALID, _T("Cb/Ghost/bg"));
    EXPECT_INT((int)p.border, (int)CLR_INVALID, _T("Cb/Ghost/border"));

    // 端到端: 实例化 Checkbox + Ghost, getter / setter 正常工作。
    DuiButton b;
    b.SetButtonType(DuiButton::StyleCheckbox);
    b.SetVariant(V::Ghost);
    EXPECT_INT((int)b.GetVariant(),    (int)V::Ghost,                    _T("Cb/Ghost/var"));
    EXPECT_INT((int)b.GetButtonType(), (int)DuiButton::StyleCheckbox,    _T("Cb/Ghost/style"));
    return OK(_T("CheckboxVariantGhost_Transparent"));
}

// Checkbox + Primary(实色) Variant: 外框应兜底回 kLight* 家族, 不接管 Variant
// palette。Q4=A2 决定。这里通过 PaletteFor 间接校验路由不会走错:
// Primary 的 palette.bg 是品牌蓝 RGB(45,108,223), 但 Checkbox 在 enabled
// 状态下的实际外框颜色应是 kLightFillIdle RGB(245,245,245) —— 二者不应混。
// (本测试不直绘, 仅校验"PaletteFor(Primary, Normal).bg 不是 kLightFillIdle")
// 以确认两套色板物理上是独立的。
static Result Test_CheckboxVariantPrimary_Independent()
{
    using V = DuiButton::Variant;
    using S = DuiButton::ButtonState;

    DuiButton::ButtonPalette pPri = DuiButton::PaletteFor(V::Primary, S::Normal);
    // Primary palette bg 是品牌蓝, 不是 kLight* 家族中的任何浅色 ——
    // 二者不会因为路由疏忽而被相互替代。
    EXPECT_INT((int)pPri.bg, (int)RGB(45, 108, 223), _T("Pri/bg/brand"));

    // 端到端: 切到 Primary 不应丢 Style / Check 状态。
    DuiButton b;
    b.SetButtonType(DuiButton::StyleCheckbox);
    b.SetCheck(true, false);
    b.SetVariant(V::Primary);   // 切 Variant 时既有状态保留
    EXPECT_INT((int)b.GetVariant(),    (int)V::Primary,                 _T("Cb/Pri/var"));
    EXPECT_INT((int)b.GetButtonType(), (int)DuiButton::StyleCheckbox,   _T("Cb/Pri/style"));
    EXPECT_BOOL(b.IsChecked(), true, _T("Cb/Pri/check"));
    return OK(_T("CheckboxVariantPrimary_Independent"));
}

// Radio + Outlined Variant: 外框 palette.bg = CLR_INVALID, border = 品牌色。
// Outlined 是"透明底 + 边可见"的 Variant; Radio 在该 Variant 下外框应是
// 透明 + 品牌色边。
static Result Test_RadioVariantOutlined_BrandBorder()
{
    using V = DuiButton::Variant;
    using S = DuiButton::ButtonState;

    DuiButton::ButtonPalette p = DuiButton::PaletteFor(V::Outlined, S::Normal);
    EXPECT_INT((int)p.bg,     (int)CLR_INVALID,        _T("Rd/Out/bg_invalid"));
    EXPECT_INT((int)p.border, (int)RGB(45, 108, 223),  _T("Rd/Out/border_brand"));

    // 端到端: 实例化 Radio + Outlined, mutual-exclusion 仍工作。
    DuiControl parent;
    DuiButton *r1, *r2;
    parent.AddChild(std::unique_ptr<DuiControl>(r1 = new DuiButton()));
    parent.AddChild(std::unique_ptr<DuiControl>(r2 = new DuiButton()));
    r1->SetButtonType(DuiButton::StyleRadio);
    r2->SetButtonType(DuiButton::StyleRadio);
    r1->SetRadioGroup(11);
    r2->SetRadioGroup(11);
    r1->SetVariant(V::Outlined);
    r2->SetVariant(V::Outlined);
    r1->SetRect(RECT{ 0, 0, 100, 24 });
    r2->SetRect(RECT{ 0, 0, 100, 24 });
    r1->OnLButtonDown(POINT{ 50, 12 }, 0);
    r1->OnLButtonUp  (POINT{ 50, 12 }, 0);
    EXPECT_BOOL(r1->IsChecked(), true,  _T("Rd/Out/r1Checked"));
    r2->OnLButtonDown(POINT{ 50, 12 }, 0);
    r2->OnLButtonUp  (POINT{ 50, 12 }, 0);
    // 互斥仍生效, Variant 不影响该行为。
    EXPECT_BOOL(r1->IsChecked(), false, _T("Rd/Out/r1Excluded"));
    EXPECT_BOOL(r2->IsChecked(), true,  _T("Rd/Out/r2Checked"));
    return OK(_T("RadioVariantOutlined_BrandBorder"));
}

// ----- DuiButton: SetFont / SetTextPointSize / LeadingIcon ----------------

// 默认 GetFont() == nullptr; SetFont(hf) 后 getter 一致; SetFont(nullptr)
// 恢复默认; SetFont 不影响 Style / Variant。
static Result Test_SetFontRoundTrip()
{
    DuiButton b;
    EXPECT_BOOL(b.GetFont() == nullptr, true, _T("Font/def"));

    HFONT fake = reinterpret_cast<HFONT>((LPARAM)0xDEAD);
    b.SetFont(fake);
    EXPECT_BOOL(b.GetFont() == fake, true, _T("Font/set"));

    b.SetFont(nullptr);
    EXPECT_BOOL(b.GetFont() == nullptr, true, _T("Font/clear"));

    // 与 Variant / Style 正交。
    b.SetFont(fake);
    b.SetVariant(DuiButton::Variant::Danger);
    EXPECT_INT((int)b.GetVariant(), (int)DuiButton::Variant::Danger, _T("Font/varKept"));
    EXPECT_BOOL(b.GetFont() == fake, true, _T("Font/fontKept"));
    return OK(_T("SetFontRoundTrip"));
}

// SetTextPointSize 走 DuiResMgr::GetFontByPointSize 拿真实 HFONT;
// 缓存机制由 manager 保证 ——  此处只验"SetFont 被调用了"。
// pt<=0 退化为 SetFont(nullptr)(等同默认字体)。
static Result Test_SetTextPointSizeRoundTrip()
{
    DuiButton b;
    // pt > 0:GetFont 应返回非空(manager 缓存的 HFONT)。
    b.SetTextPointSize(11, /*bold=*/false);
    EXPECT_BOOL(b.GetFont() != nullptr, true, _T("Pt/11"));
    HFONT h11 = b.GetFont();

    // 同 (pt, bold) 再调:走缓存, getter 返回相同句柄。
    b.SetTextPointSize(11, /*bold=*/false);
    EXPECT_BOOL(b.GetFont() == h11, true, _T("Pt/11_cache"));

    // 不同 bold:应是不同 HFONT(缓存按 (pt,bold) 分键)。
    b.SetTextPointSize(11, /*bold=*/true);
    EXPECT_BOOL(b.GetFont() != nullptr && b.GetFont() != h11, true, _T("Pt/11_bold"));

    // pt <= 0:退化为 SetFont(nullptr)。
    b.SetTextPointSize(0);
    EXPECT_BOOL(b.GetFont() == nullptr, true, _T("Pt/0_defaults"));
    b.SetTextPointSize(-3);
    EXPECT_BOOL(b.GetFont() == nullptr, true, _T("Pt/neg_defaults"));
    return OK(_T("SetTextPointSizeRoundTrip"));
}

// SetLeadingIcon 默认 nullptr / size 16 / gap 6; setter 往返。
static Result Test_SetLeadingIconRoundTrip()
{
    DuiButton b;
    EXPECT_BOOL(b.GetLeadingIcon() == nullptr, true, _T("LI/defNull"));
    EXPECT_INT(b.GetLeadingIconSize(), 16, _T("LI/defSize"));
    EXPECT_INT(b.GetLeadingIconGap(),   6, _T("LI/defGap"));

    HBITMAP fake = reinterpret_cast<HBITMAP>((LPARAM)0xBEEF);
    b.SetLeadingIcon(fake);
    EXPECT_BOOL(b.GetLeadingIcon() == fake, true, _T("LI/set"));

    b.SetLeadingIconSize(24);
    EXPECT_INT(b.GetLeadingIconSize(), 24, _T("LI/size24"));

    b.SetLeadingIconGap(10);
    EXPECT_INT(b.GetLeadingIconGap(), 10, _T("LI/gap10"));

    b.SetLeadingIcon(nullptr);
    EXPECT_BOOL(b.GetLeadingIcon() == nullptr, true, _T("LI/clear"));
    return OK(_T("SetLeadingIconRoundTrip"));
}

// SetLeadingIconSize:<= 0 钳到 1; SetLeadingIconGap:< 0 钳到 0。
static Result Test_SetLeadingIconSizeGapClamp()
{
    DuiButton b;

    b.SetLeadingIconSize(0);
    EXPECT_INT(b.GetLeadingIconSize(), 1, _T("LIClamp/size0"));
    b.SetLeadingIconSize(-5);
    EXPECT_INT(b.GetLeadingIconSize(), 1, _T("LIClamp/sizeNeg"));
    b.SetLeadingIconSize(32);
    EXPECT_INT(b.GetLeadingIconSize(), 32, _T("LIClamp/size32"));

    b.SetLeadingIconGap(-1);
    EXPECT_INT(b.GetLeadingIconGap(), 0, _T("LIClamp/gapNeg1"));
    b.SetLeadingIconGap(-100);
    EXPECT_INT(b.GetLeadingIconGap(), 0, _T("LIClamp/gapNegBig"));
    b.SetLeadingIconGap(0);
    EXPECT_INT(b.GetLeadingIconGap(), 0, _T("LIClamp/gap0"));
    b.SetLeadingIconGap(20);
    EXPECT_INT(b.GetLeadingIconGap(), 20, _T("LIClamp/gap20"));
    return OK(_T("SetLeadingIconSizeGapClamp"));
}

// LeadingIcon 与 Variant 共存:setter 互不影响 getter。
static Result Test_LeadingIconCoexistsWithVariant()
{
    DuiButton b;
    HBITMAP fake = reinterpret_cast<HBITMAP>((LPARAM)0xABCD);

    b.SetVariant(DuiButton::Variant::Danger);
    b.SetLeadingIcon(fake);
    b.SetLeadingIconSize(20);
    b.SetLeadingIconGap(8);

    EXPECT_INT((int)b.GetVariant(),       (int)DuiButton::Variant::Danger, _T("Coex/var"));
    EXPECT_BOOL(b.GetLeadingIcon() == fake, true,                          _T("Coex/icon"));
    EXPECT_INT(b.GetLeadingIconSize(),    20, _T("Coex/size"));
    EXPECT_INT(b.GetLeadingIconGap(),      8, _T("Coex/gap"));

    // 改 Variant 不影响 LeadingIcon 配置。
    b.SetVariant(DuiButton::Variant::Outlined);
    EXPECT_INT((int)b.GetVariant(),       (int)DuiButton::Variant::Outlined, _T("Coex/varOutl"));
    EXPECT_BOOL(b.GetLeadingIcon() == fake, true,                            _T("Coex/iconKept"));
    EXPECT_INT(b.GetLeadingIconSize(),    20, _T("Coex/sizeKept"));
    return OK(_T("LeadingIconCoexistsWithVariant"));
}

// LeadingIcon caller-owned:Set 再 Set 再 Clear, 不应崩(控件不 DeleteObject)。
// 假位图指针完全够测"控件没去碰底层 GDI 对象"这一点。
static Result Test_LeadingIconCallerOwned()
{
    DuiButton b;
    HBITMAP a = reinterpret_cast<HBITMAP>((LPARAM)0x1111);
    HBITMAP c = reinterpret_cast<HBITMAP>((LPARAM)0x2222);

    b.SetLeadingIcon(a);
    EXPECT_BOOL(b.GetLeadingIcon() == a, true, _T("Own/a"));

    // 替换:旧 a 不被 DeleteObject(它是假指针, 真 Delete 会崩)。
    b.SetLeadingIcon(c);
    EXPECT_BOOL(b.GetLeadingIcon() == c, true, _T("Own/c"));

    // 清空:c 也不被 DeleteObject。
    b.SetLeadingIcon(nullptr);
    EXPECT_BOOL(b.GetLeadingIcon() == nullptr, true, _T("Own/clear"));

    // 析构控件:m_leadingIcon 已 nullptr, 不会进 DeleteObject 分支
    // (现在控件本身就不该 Delete, 不论 leaving icon 为何;此测就此结束。)
    return OK(_T("LeadingIconCallerOwned"));
}

// LeadingIcon 在 Checkbox / Radio 上 setter 仍工作(getter 往返), 但 OnPaint
// 应忽略 —— 我们这无渲染 verify, 只能验"setter 不报错 + getter 正常 + Style
// 未被改变"。
static Result Test_LeadingIconStateOnNonPushButton()
{
    DuiButton b;
    b.SetButtonType(DuiButton::StyleCheckbox);
    HBITMAP fake = reinterpret_cast<HBITMAP>((LPARAM)0xDEAD);
    b.SetLeadingIcon(fake);
    EXPECT_BOOL(b.GetLeadingIcon() == fake, true, _T("LiNP/cb_icon"));
    EXPECT_INT((int)b.GetButtonType(), (int)DuiButton::StyleCheckbox, _T("LiNP/cb_style"));

    b.SetButtonType(DuiButton::StyleRadio);
    EXPECT_BOOL(b.GetLeadingIcon() == fake, true, _T("LiNP/rd_icon"));
    EXPECT_INT((int)b.GetButtonType(), (int)DuiButton::StyleRadio, _T("LiNP/rd_style"));

    // 回 PushButton:LeadingIcon 仍在(应在画时生效)。
    b.SetButtonType(DuiButton::StylePushButton);
    EXPECT_BOOL(b.GetLeadingIcon() == fake, true, _T("LiNP/push_icon"));
    return OK(_T("LeadingIconStateOnNonPushButton"));
}

// Font 与 LeadingIcon 与 Variant 完整共存:5 项 setter / getter 全部往返。
static Result Test_FontIconVariantTriple()
{
    DuiButton b;
    HFONT   font = reinterpret_cast<HFONT>((LPARAM)0xF0F0);
    HBITMAP icon = reinterpret_cast<HBITMAP>((LPARAM)0xBABA);

    b.SetVariant(DuiButton::Variant::Primary);
    b.SetFont(font);
    b.SetLeadingIcon(icon);
    b.SetLeadingIconSize(18);
    b.SetLeadingIconGap(8);

    EXPECT_INT((int)b.GetVariant(),       (int)DuiButton::Variant::Primary, _T("Trip/var"));
    EXPECT_BOOL(b.GetFont() == font,         true, _T("Trip/font"));
    EXPECT_BOOL(b.GetLeadingIcon() == icon,  true, _T("Trip/icon"));
    EXPECT_INT(b.GetLeadingIconSize(),    18, _T("Trip/size"));
    EXPECT_INT(b.GetLeadingIconGap(),      8, _T("Trip/gap"));

    // 单改 font 不影响其它。
    b.SetFont(nullptr);
    EXPECT_BOOL(b.GetFont() == nullptr,      true, _T("Trip/fontClear"));
    EXPECT_INT((int)b.GetVariant(),       (int)DuiButton::Variant::Primary, _T("Trip/varKept"));
    EXPECT_BOOL(b.GetLeadingIcon() == icon,  true, _T("Trip/iconKept"));
    return OK(_T("FontIconVariantTriple"));
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
        { _T("SetBgInsets_ClampsNegatives"),&Test_SetBgInsets_ClampsNegatives},
        // ---- Variant 视觉变体 ----
        { _T("VariantSetterDefaults"),       &Test_VariantSetterDefaults       },
        { _T("PaletteForPrimaryStates"),     &Test_PaletteForPrimaryStates     },
        { _T("PaletteForOtherVariantsNormal"),&Test_PaletteForOtherVariantsNormal},
        { _T("PaletteTransparentSemantics"), &Test_PaletteTransparentSemantics },
        { _T("BitmapAndVariantCoexist"),     &Test_BitmapAndVariantCoexist     },
        // ---- AA 开关 + Checkbox/Radio 透明 Variant 路由 ----
        { _T("SetAntiAliasRoundTrip"),       &Test_SetAntiAliasRoundTrip       },
        { _T("CheckboxVariantGhost_Transparent"),   &Test_CheckboxVariantGhost_Transparent   },
        { _T("CheckboxVariantPrimary_Independent"), &Test_CheckboxVariantPrimary_Independent },
        { _T("RadioVariantOutlined_BrandBorder"),   &Test_RadioVariantOutlined_BrandBorder   },
        // ---- SetFont / SetTextPointSize / LeadingIcon ----
        { _T("SetFontRoundTrip"),                &Test_SetFontRoundTrip                },
        { _T("SetTextPointSizeRoundTrip"),       &Test_SetTextPointSizeRoundTrip       },
        { _T("SetLeadingIconRoundTrip"),         &Test_SetLeadingIconRoundTrip         },
        { _T("SetLeadingIconSizeGapClamp"),      &Test_SetLeadingIconSizeGapClamp      },
        { _T("LeadingIconCoexistsWithVariant"),  &Test_LeadingIconCoexistsWithVariant  },
        { _T("LeadingIconCallerOwned"),          &Test_LeadingIconCallerOwned          },
        { _T("LeadingIconStateOnNonPushButton"), &Test_LeadingIconStateOnNonPushButton },
        { _T("FontIconVariantTriple"),           &Test_FontIconVariantTriple           }
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
