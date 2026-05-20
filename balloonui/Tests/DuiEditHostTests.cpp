#include "stdafx.h"
#include "DuiEditHostTests.h"

#if BUI_FEATURE_EDIT

#include "../Controls/Layout/DuiLayout.h"


namespace balloonwjui {

namespace DuiEditHostTests {

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

#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)

// Defaults
static Result Test_Defaults()
{
    DuiEditHost e;
    EXPECT_BOOL(e.IsTabStop(),     true,  _T("Defaults/tabStop"));
    EXPECT_BOOL(e.IsMultiLine(),   false, _T("Defaults/singleLine"));
    EXPECT_BOOL(e.IsPassword(),    false, _T("Defaults/notPassword"));
    EXPECT_BOOL(e.IsReadOnly(),    false, _T("Defaults/notReadOnly"));
    EXPECT_INT (e.GetMaxLength(),  0,     _T("Defaults/maxLen"));
    return OK(_T("Defaults"));
}

// SetText round-trips through the cache when no HWND exists.
static Result Test_SetTextRoundTrip()
{
    DuiEditHost e;
    e.SetText(_T("hello"));
    if (e.GetText() != _T("hello"))
    {
        return Fail(_T("SetTextRoundTrip"), _T("text mismatch"));
    }
    e.SetText(_T(""));
    if (e.GetText() != _T(""))
    {
        return Fail(_T("SetTextRoundTrip/empty"), _T("empty mismatch"));
    }
    return OK(_T("SetTextRoundTrip"));
}

// Placeholder shows only when: enabled=true AND shown=true AND text empty AND not focused.
static Result Test_PlaceholderStateMachine()
{
    DuiEditHost e;
    e.SetPlaceholder(_T("type something"));
    EXPECT_BOOL(e.IsShowingPlaceholder(), true,  _T("PH/blankUnfocused"));
    e.Test_SetFocused(true);
    EXPECT_BOOL(e.IsShowingPlaceholder(), false, _T("PH/focusedHides"));
    e.Test_SetFocused(false);
    EXPECT_BOOL(e.IsShowingPlaceholder(), true,  _T("PH/blurShowsAgain"));
    e.SetText(_T("typed"));
    EXPECT_BOOL(e.IsShowingPlaceholder(), false, _T("PH/textHides"));
    e.SetText(_T(""));
    EXPECT_BOOL(e.IsShowingPlaceholder(), true,  _T("PH/clearShowsAgain"));
    e.SetShowPlaceholder(false);
    EXPECT_BOOL(e.IsShowingPlaceholder(), false, _T("PH/disabledHides"));
    e.SetShowPlaceholder(true);
    EXPECT_BOOL(e.IsShowingPlaceholder(), true,  _T("PH/reenable"));
    e.SetPlaceholder(_T(""));
    EXPECT_BOOL(e.IsShowingPlaceholder(), false, _T("PH/emptyTextNoShow"));
    return OK(_T("PlaceholderStateMachine"));
}

// SetMultiLine before HWND created is a no-cost flag flip.
static Result Test_MultiLinePreCreate()
{
    DuiEditHost e;
    e.SetMultiLine(true);
    EXPECT_BOOL(e.IsMultiLine(), true, _T("MultiLine/set"));
    e.SetMultiLine(false);
    EXPECT_BOOL(e.IsMultiLine(), false, _T("MultiLine/clear"));
    return OK(_T("MultiLinePreCreate"));
}

// SetPassword before HWND created is a no-cost flag flip.
static Result Test_PasswordPreCreate()
{
    DuiEditHost e;
    e.SetPassword(true);
    EXPECT_BOOL(e.IsPassword(), true, _T("Pwd/set"));
    e.SetPassword(false);
    EXPECT_BOOL(e.IsPassword(), false, _T("Pwd/clear"));
    return OK(_T("PasswordPreCreate"));
}

// SetReadOnly without HWND is a flag flip; SyncReadOnlyToHwnd is a no-op.
static Result Test_ReadOnlyFlag()
{
    DuiEditHost e;
    e.SetReadOnly(true);
    EXPECT_BOOL(e.IsReadOnly(), true, _T("RO/set"));
    e.SetReadOnly(false);
    EXPECT_BOOL(e.IsReadOnly(), false, _T("RO/clear"));
    return OK(_T("ReadOnlyFlag"));
}

// MaxLength API round-trip.
static Result Test_MaxLength()
{
    DuiEditHost e;
    e.SetMaxLength(20);
    EXPECT_INT(e.GetMaxLength(), 20, _T("MaxLen/20"));
    e.SetMaxLength(0);
    EXPECT_INT(e.GetMaxLength(), 0,  _T("MaxLen/0"));
    e.SetMaxLength(-5);
    EXPECT_INT(e.GetMaxLength(), 0,  _T("MaxLen/clamp"));
    return OK(_T("MaxLength"));
}

// Test_SetTextDirect mirrors what EN_CHANGE would do (cache-only update).
static Result Test_TextDirectInjection()
{
    DuiEditHost e;
    e.Test_SetTextDirect(_T("abc"));
    if (e.Test_GetCachedText() != _T("abc"))
    {
        return Fail(_T("TextDirectInjection"), _T("cache mismatch"));
    }
    return OK(_T("TextDirectInjection"));
}

// Placeholder visibility when EnsureCreated has NOT run still works
// (cached text drives the check; no HWND required for predicate).
static Result Test_NoHwndStillUsable()
{
    DuiEditHost e;
    e.SetText(_T("data"));
    if (e.GetText() != _T("data"))
    {
        return Fail(_T("NoHwnd/getText"), _T("cache miss"));
    }
    e.SetReadOnly(true);    // safe no-op
    e.SetMaxLength(50);     // safe no-op
    e.SetMargins(2, 1, 2, 1);
    return OK(_T("NoHwndStillUsable"));
}

// Tab-stop default is true (so user can Tab into the edit).
static Result Test_TabStopDefault()
{
    DuiEditHost e;
    EXPECT_BOOL(e.IsTabStop(), true, _T("TabStop"));
    return OK(_T("TabStopDefault"));
}

// Eye toggle defaults: hidden, password not revealed.
static Result Test_EyeDefaults()
{
    DuiEditHost e;
    EXPECT_BOOL(e.HasEyeToggle(),       false, _T("Eye/defOff"));
    EXPECT_BOOL(e.IsPasswordRevealed(), false, _T("Eye/notRevealed"));
    return OK(_T("EyeDefaults"));
}

// SetShowEyeToggle round-trip.
static Result Test_EyeFlagRoundTrip()
{
    DuiEditHost e;
    e.SetShowEyeToggle(true);
    EXPECT_BOOL(e.HasEyeToggle(), true,  _T("Eye/on"));
    e.SetShowEyeToggle(false);
    EXPECT_BOOL(e.HasEyeToggle(), false, _T("Eye/off"));
    return OK(_T("EyeFlagRoundTrip"));
}

// SetPasswordRevealed has effect only in password mode.
static Result Test_EyeRevealOnlyForPassword()
{
    DuiEditHost e;
    // Not password mode: SetPasswordRevealed should be a no-op.
    e.SetPasswordRevealed(true);
    EXPECT_BOOL(e.IsPasswordRevealed(), false, _T("Eye/nonPwdNoop"));
    e.SetPassword(true);
    e.SetPasswordRevealed(true);
    EXPECT_BOOL(e.IsPasswordRevealed(), true,  _T("Eye/pwdReveal"));
    e.SetPasswordRevealed(false);
    EXPECT_BOOL(e.IsPasswordRevealed(), false, _T("Eye/pwdHideAgain"));
    return OK(_T("EyeRevealOnlyForPassword"));
}

// Disabling the toggle while revealed reverts to hidden.
static Result Test_EyeOffWhileRevealedReverts()
{
    DuiEditHost e;
    e.SetPassword(true);
    e.SetShowEyeToggle(true);
    e.SetPasswordRevealed(true);
    EXPECT_BOOL(e.IsPasswordRevealed(), true,  _T("Eye/setupReveal"));
    e.SetShowEyeToggle(false);
    EXPECT_BOOL(e.IsPasswordRevealed(), false, _T("Eye/revertedOnDisable"));
    return OK(_T("EyeOffWhileRevealedReverts"));
}

// Placeholder routes through OS EDIT cue banner — verify by creating a real
// EDIT HWND, calling SetPlaceholder before+after EnsureCreated, and reading
// back via EM_GETCUEBANNER. This is the only way to catch regressions of the
// "host-painted placeholder gets covered by EDIT child window" bug — pure
// unit-state checks (Test_PlaceholderStateMachine) won't see it.
static Result Test_PlaceholderRoutesToCueBanner()
{
    // Hidden popup parent for the EDIT.
    HINSTANCE hInst = ::GetModuleHandle(nullptr);
    HWND hParent = ::CreateWindowEx(0, _T("STATIC"), _T(""),
                                    WS_OVERLAPPED | WS_POPUP,
                                    0, 0, 1, 1, nullptr, nullptr, hInst, nullptr);
    if (!hParent)
    {
        return Fail(_T("CueBanner/parent"), _T("CreateWindowEx failed"));
    }

    // (a) SetPlaceholder BEFORE EnsureCreated should be applied at create time.
    DuiEditHost e1;
    e1.SetPlaceholder(_T("hint-before"));
    bool created = e1.EnsureCreated(hParent);
    if (!created)
    {
        ::DestroyWindow(hParent);
        return Fail(_T("CueBanner/create1"), _T("EnsureCreated failed"));
    }
    HWND hEdit = e1.GetHostedHwnd();
    wchar_t buf[64] = {};
    LRESULT got = ::SendMessageW(hEdit, EM_GETCUEBANNER, (WPARAM)buf, _countof(buf));
    if (got == 0 || wcscmp(buf, L"hint-before") != 0)
    {
        ::DestroyWindow(hParent);
        return Fail(_T("CueBanner/preCreate"), _T("cue not synced on EnsureCreated"));
    }

    // (b) SetPlaceholder AFTER EnsureCreated should immediately update cue.
    e1.SetPlaceholder(_T("hint-after"));
    wchar_t buf2[64] = {};
    ::SendMessageW(hEdit, EM_GETCUEBANNER, (WPARAM)buf2, _countof(buf2));
    if (wcscmp(buf2, L"hint-after") != 0)
    {
        ::DestroyWindow(hParent);
        return Fail(_T("CueBanner/postCreate"), _T("cue not updated by SetPlaceholder"));
    }

    // (c) SetShowPlaceholder(false) clears cue (empty string), keeps m_placeholder.
    e1.SetShowPlaceholder(false);
    wchar_t buf3[64] = { L'!', 0 };       // sentinel so we can tell "untouched"
    ::SendMessageW(hEdit, EM_GETCUEBANNER, (WPARAM)buf3, _countof(buf3));
    if (buf3[0] != L'\0')
    {
        ::DestroyWindow(hParent);
        return Fail(_T("CueBanner/showOff"), _T("cue not cleared by SetShowPlaceholder(false)"));
    }
    if (e1.GetPlaceholder() != _T("hint-after"))
    {
        ::DestroyWindow(hParent);
        return Fail(_T("CueBanner/showOffPreserves"), _T("m_placeholder lost"));
    }

    // (d) SetShowPlaceholder(true) restores cue.
    e1.SetShowPlaceholder(true);
    wchar_t buf4[64] = {};
    ::SendMessageW(hEdit, EM_GETCUEBANNER, (WPARAM)buf4, _countof(buf4));
    if (wcscmp(buf4, L"hint-after") != 0)
    {
        ::DestroyWindow(hParent);
        return Fail(_T("CueBanner/showRestore"), _T("cue not restored"));
    }

    ::DestroyWindow(hParent);
    return OK(_T("PlaceholderRoutesToCueBanner"));
}

// ─── 内联图标 + 底色 API（默认全部关闭，行为不变）────────────────

static Result Test_BgColor_Default()
{
    DuiEditHost e;
    COLORREF c = e.GetBgColor();
    if (c != RGB(255, 255, 255))
    {
        return Fail(_T("BgColor/default"), _T("expected white"));
    }
    return OK(_T("BgColorDefault"));
}

static Result Test_BgColor_RoundTrip()
{
    DuiEditHost e;
    e.SetBgColor(RGB(0xF3, 0xF3, 0xF4));
    if (e.GetBgColor() != RGB(0xF3, 0xF3, 0xF4))
    {
        return Fail(_T("BgColor/RT"), _T("set/get mismatch"));
    }
    return OK(_T("BgColorRoundTrip"));
}

// 默认所有图标参数都是 0 / null / false → 行为与本 API 加入前一致
static Result Test_IconDefaults()
{
    DuiEditHost e;
    EXPECT_INT (e.GetIconWidth(DuiEditHost::LeftIcon),  0,
                _T("Icon/defLeftW"));
    EXPECT_INT (e.GetIconWidth(DuiEditHost::RightIcon), 0,
                _T("Icon/defRightW"));
    EXPECT_BOOL(e.IsIconClickable(DuiEditHost::LeftIcon),  false,
                _T("Icon/defLeftClick"));
    EXPECT_BOOL(e.IsIconClickable(DuiEditHost::RightIcon), false,
                _T("Icon/defRightClick"));
    return OK(_T("IconDefaults"));
}

static Result Test_IconSetGetWidth()
{
    DuiEditHost e;
    e.SetIcon(DuiEditHost::LeftIcon, 32,
              [](HDC, const RECT&) {});
    EXPECT_INT(e.GetIconWidth(DuiEditHost::LeftIcon), 32,
               _T("Icon/leftW"));
    e.SetIcon(DuiEditHost::RightIcon, 24,
              [](HDC, const RECT&) {});
    EXPECT_INT(e.GetIconWidth(DuiEditHost::RightIcon), 24,
               _T("Icon/rightW"));
    return OK(_T("IconSetGetWidth"));
}

// SetIcon(slot, width, nullptr) 等价于 ClearIcon —— gutter 收回
static Result Test_IconClearViaNullPainter()
{
    DuiEditHost e;
    e.SetIcon(DuiEditHost::LeftIcon, 24, [](HDC, const RECT&) {});
    EXPECT_INT(e.GetIconWidth(DuiEditHost::LeftIcon), 24,
               _T("Icon/before"));
    e.SetIcon(DuiEditHost::LeftIcon, 24, nullptr);
    EXPECT_INT(e.GetIconWidth(DuiEditHost::LeftIcon), 0,
               _T("Icon/clearedByNull"));
    return OK(_T("IconClearViaNullPainter"));
}

// ClearIcon 显式 API
static Result Test_IconClearExplicit()
{
    DuiEditHost e;
    e.SetIcon(DuiEditHost::RightIcon, 32, [](HDC, const RECT&) {});
    e.ClearIcon(DuiEditHost::RightIcon);
    EXPECT_INT(e.GetIconWidth(DuiEditHost::RightIcon), 0,
               _T("Icon/clearedExplicit"));
    EXPECT_BOOL(e.IsIconClickable(DuiEditHost::RightIcon), false,
                _T("Icon/clickableUnchanged"));
    return OK(_T("IconClearExplicit"));
}

static Result Test_IconClickableRoundTrip()
{
    DuiEditHost e;
    e.SetIconClickable(DuiEditHost::LeftIcon, true);
    EXPECT_BOOL(e.IsIconClickable(DuiEditHost::LeftIcon), true,
                _T("Icon/leftClickTrue"));
    e.SetIconClickable(DuiEditHost::LeftIcon, false);
    EXPECT_BOOL(e.IsIconClickable(DuiEditHost::LeftIcon), false,
                _T("Icon/leftClickFalse"));
    return OK(_T("IconClickableRoundTrip"));
}

// ComputeIconRect 静态 helper：左图标贴左 border 内侧
static Result Test_ComputeIconRect_Left()
{
    RECT host = { 0, 0, 200, 32 };
    RECT r = DuiEditHost::ComputeIconRect(host,
        DuiEditHost::LeftIcon, 32, 1, 2);
    EXPECT_INT(r.left,   1,  _T("Icon/L/L"));
    EXPECT_INT(r.right,  33, _T("Icon/L/R"));
    EXPECT_INT(r.top,    3,  _T("Icon/L/T"));
    EXPECT_INT(r.bottom, 29, _T("Icon/L/B"));
    return OK(_T("ComputeIconRectLeft"));
}

// 右图标贴右 border 内侧
static Result Test_ComputeIconRect_Right()
{
    RECT host = { 0, 0, 200, 32 };
    RECT r = DuiEditHost::ComputeIconRect(host,
        DuiEditHost::RightIcon, 28, 1, 2);
    EXPECT_INT(r.right, 199, _T("Icon/R/R"));    // 200 - 1 border
    EXPECT_INT(r.left,  171, _T("Icon/R/L"));    // 199 - 28
    return OK(_T("ComputeIconRectRight"));
}

// ── IsEffectivelyVisible：自己 + 所有祖先都 visible 才返 true ─────

// 单独控件、默认 m_bVisible=true、无父 → true
static Result Test_EffectivelyVisible_SoloDefault()
{
    DuiControl c;
    EXPECT_BOOL(c.IsEffectivelyVisible(), true,
                _T("EffVis/solo/default"));
    return OK(_T("EffectivelyVisibleSoloDefault"));
}

// 单独控件 SetVisible(false) → false
static Result Test_EffectivelyVisible_SoloHidden()
{
    DuiControl c;
    c.SetVisible(false);
    EXPECT_BOOL(c.IsEffectivelyVisible(), false,
                _T("EffVis/solo/hidden"));
    return OK(_T("EffectivelyVisibleSoloHidden"));
}

// 父隐子显 → 子的 effective 应为 false（要走父链）
static Result Test_EffectivelyVisible_HiddenAncestor()
{
    auto parent = std::make_unique<DuiVBox>();
    auto childUq = std::make_unique<DuiControl>();
    DuiControl* child = childUq.get();
    parent->AddChild(std::move(childUq));

    EXPECT_BOOL(child->IsEffectivelyVisible(), true,
                _T("EffVis/anc/before"));
    parent->SetVisible(false);
    EXPECT_BOOL(child->IsEffectivelyVisible(), false,
                _T("EffVis/anc/after"));
    EXPECT_BOOL(child->IsVisible(), true,
                _T("EffVis/anc/childOwnFlag"));
    parent->SetVisible(true);
    EXPECT_BOOL(child->IsEffectivelyVisible(), true,
                _T("EffVis/anc/restored"));
    return OK(_T("EffectivelyVisibleHiddenAncestor"));
}

// 三层嵌套：grandparent.SetVisible(false) → 中孙都 effective false
static Result Test_EffectivelyVisible_DeepChain()
{
    auto grand = std::make_unique<DuiVBox>();
    auto midUq = std::make_unique<DuiVBox>();
    DuiVBox* mid = midUq.get();
    auto leafUq = std::make_unique<DuiControl>();
    DuiControl* leaf = leafUq.get();
    mid->AddChild(std::move(leafUq));
    grand->AddChild(std::move(midUq));

    EXPECT_BOOL(leaf->IsEffectivelyVisible(), true,
                _T("EffVis/deep/before"));
    grand->SetVisible(false);
    EXPECT_BOOL(mid->IsEffectivelyVisible(), false,
                _T("EffVis/deep/midAfter"));
    EXPECT_BOOL(leaf->IsEffectivelyVisible(), false,
                _T("EffVis/deep/leafAfter"));
    return OK(_T("EffectivelyVisibleDeepChain"));
}

// SetShowBorder 默认 true（行为不变），关掉后 GetIsShowBorder 返 false
static Result Test_ShowBorderDefault()
{
    DuiEditHost e;
    EXPECT_BOOL(e.IsShowBorder(), true, _T("Border/default"));
    return OK(_T("ShowBorderDefault"));
}

static Result Test_ShowBorderRoundTrip()
{
    DuiEditHost e;
    e.SetShowBorder(false);
    EXPECT_BOOL(e.IsShowBorder(), false, _T("Border/false"));
    e.SetShowBorder(true);
    EXPECT_BOOL(e.IsShowBorder(), true,  _T("Border/true"));
    return OK(_T("ShowBorderRoundTrip"));
}

// gutter 宽度 0 → 返回空 RECT（caller 据此跳过 paint / hit-test）
static Result Test_ComputeIconRect_ZeroWidth()
{
    RECT host = { 0, 0, 200, 32 };
    RECT r = DuiEditHost::ComputeIconRect(host,
        DuiEditHost::LeftIcon, 0, 1, 2);
    EXPECT_INT(r.left,   0, _T("Icon/0/L"));
    EXPECT_INT(r.right,  0, _T("Icon/0/R"));
    return OK(_T("ComputeIconRectZeroWidth"));
}

#undef EXPECT_BOOL
#undef EXPECT_INT

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
        { _T("Defaults"),                &Test_Defaults                },
        { _T("SetTextRoundTrip"),        &Test_SetTextRoundTrip        },
        { _T("PlaceholderStateMachine"), &Test_PlaceholderStateMachine },
        { _T("MultiLinePreCreate"),      &Test_MultiLinePreCreate      },
        { _T("PasswordPreCreate"),       &Test_PasswordPreCreate       },
        { _T("ReadOnlyFlag"),            &Test_ReadOnlyFlag            },
        { _T("MaxLength"),               &Test_MaxLength               },
        { _T("TextDirectInjection"),     &Test_TextDirectInjection     },
        { _T("NoHwndStillUsable"),       &Test_NoHwndStillUsable       },
        { _T("TabStopDefault"),          &Test_TabStopDefault          },
        { _T("EyeDefaults"),             &Test_EyeDefaults             },
        { _T("EyeFlagRoundTrip"),        &Test_EyeFlagRoundTrip        },
        { _T("EyeRevealOnlyForPassword"),&Test_EyeRevealOnlyForPassword},
        { _T("EyeOffWhileRevealedReverts"),&Test_EyeOffWhileRevealedReverts},
        { _T("PlaceholderRoutesToCueBanner"),&Test_PlaceholderRoutesToCueBanner},
        { _T("BgColorDefault"),          &Test_BgColor_Default         },
        { _T("BgColorRoundTrip"),        &Test_BgColor_RoundTrip       },
        { _T("IconDefaults"),            &Test_IconDefaults            },
        { _T("IconSetGetWidth"),         &Test_IconSetGetWidth         },
        { _T("IconClearViaNullPainter"), &Test_IconClearViaNullPainter },
        { _T("IconClearExplicit"),       &Test_IconClearExplicit       },
        { _T("IconClickableRoundTrip"),  &Test_IconClickableRoundTrip  },
        { _T("ComputeIconRectLeft"),     &Test_ComputeIconRect_Left    },
        { _T("ComputeIconRectRight"),    &Test_ComputeIconRect_Right   },
        { _T("ComputeIconRectZeroWidth"),&Test_ComputeIconRect_ZeroWidth},
        { _T("ShowBorderDefault"),       &Test_ShowBorderDefault       },
        { _T("ShowBorderRoundTrip"),     &Test_ShowBorderRoundTrip     },
        { _T("EffectivelyVisibleSoloDefault"),
                                         &Test_EffectivelyVisible_SoloDefault   },
        { _T("EffectivelyVisibleSoloHidden"),
                                         &Test_EffectivelyVisible_SoloHidden    },
        { _T("EffectivelyVisibleHiddenAncestor"),
                                         &Test_EffectivelyVisible_HiddenAncestor},
        { _T("EffectivelyVisibleDeepChain"),
                                         &Test_EffectivelyVisible_DeepChain     },
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
    summary.Format(_T("[summary] DuiEditHostTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiEditHostTests

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT
