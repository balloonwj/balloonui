#include "stdafx.h"
#include "DuiSmallControlsTests.h"

#if BUI_FEATURE_PROGRESSBAR && BUI_FEATURE_SLIDER && BUI_FEATURE_COMBOBOX


namespace balloonwjui {

namespace DuiSmallControlsTests {

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
#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("got '%s'"), (LPCTSTR)_a); return Fail(name, _d); } \
    } while (0)

// ============================ DuiProgressBar =========================

static Result Test_PB_Defaults()
{
    DuiProgressBar p;
    EXPECT_INT(p.GetMin(), 0,   _T("PB/min"));
    EXPECT_INT(p.GetMax(), 100, _T("PB/max"));
    EXPECT_INT(p.GetPos(), 0,   _T("PB/pos"));
    return OK(_T("PB_Defaults"));
}

static Result Test_PB_PosClamp()
{
    DuiProgressBar p;
    p.SetRange(0, 50);
    p.SetPos(25);
    EXPECT_INT(p.GetPos(), 25, _T("PB/inRange"));
    p.SetPos(99);
    EXPECT_INT(p.GetPos(), 50, _T("PB/clampMax"));
    p.SetPos(-5);
    EXPECT_INT(p.GetPos(), 0,  _T("PB/clampMin"));
    return OK(_T("PB_PosClamp"));
}

static Result Test_PB_FillWidth()
{
    DuiProgressBar p;
    p.SetRect(RECT{ 0, 0, 200, 20 });
    p.SetRange(0, 100);
    p.SetPos(0);
    EXPECT_INT(p.ComputeFillWidth(), 0,   _T("PB/fill0"));
    p.SetPos(50);
    EXPECT_INT(p.ComputeFillWidth(), 100, _T("PB/fillMid"));
    p.SetPos(100);
    EXPECT_INT(p.ComputeFillWidth(), 200, _T("PB/fillFull"));
    return OK(_T("PB_FillWidth"));
}

static Result Test_PB_FillRectGeometry()
{
    DuiProgressBar p;
    p.SetRect(RECT{ 10, 10, 110, 30 });
    p.SetRange(0, 100);
    p.SetPos(25);
    RECT f = p.ComputeFillRect();
    EXPECT_INT(f.left,  10, _T("PB/rect.left"));
    EXPECT_INT(f.right, 35, _T("PB/rect.right"));   // 10 + 25%*100 = 35
    EXPECT_INT(f.top,   10, _T("PB/rect.top"));
    EXPECT_INT(f.bottom,30, _T("PB/rect.bottom"));
    return OK(_T("PB_FillRectGeometry"));
}

static Result Test_PB_RangeShrinkClamps()
{
    DuiProgressBar p;
    p.SetRange(0, 100);
    p.SetPos(80);
    p.SetRange(0, 50);
    EXPECT_INT(p.GetPos(), 50, _T("PB/shrinkClamp"));
    return OK(_T("PB_RangeShrinkClamps"));
}

// ============================ DuiSlider ===============================

static Result Test_SL_Defaults()
{
    DuiSlider s;
    EXPECT_INT(s.GetMin(), 0,   _T("SL/min"));
    EXPECT_INT(s.GetMax(), 100, _T("SL/max"));
    EXPECT_INT(s.GetPos(), 0,   _T("SL/pos"));
    EXPECT_INT(s.GetLineSize(), 1, _T("SL/lineSize"));
    EXPECT_BOOL(s.IsTabStop(), true, _T("SL/tabStop"));
    return OK(_T("SL_Defaults"));
}

static Result Test_SL_PosClamp()
{
    DuiSlider s;
    s.SetRange(0, 50);
    s.SetPos(25);
    EXPECT_INT(s.GetPos(), 25, _T("SL/inRange"));
    s.SetPos(99);
    EXPECT_INT(s.GetPos(), 50, _T("SL/clampMax"));
    s.SetPos(-5);
    EXPECT_INT(s.GetPos(), 0,  _T("SL/clampMin"));
    return OK(_T("SL_PosClamp"));
}

static Result Test_SL_ThumbCenter()
{
    DuiSlider s;
    s.SetRect(RECT{ 0, 0, 200, 30 });    // track inset by m_thumbR (7) -> [7,193]
    s.SetRange(0, 100);
    s.SetPos(0);
    POINT c = s.ComputeThumbCenter();
    EXPECT_INT(c.x, 7,   _T("SL/centerLeft"));
    s.SetPos(100);
    c = s.ComputeThumbCenter();
    EXPECT_INT(c.x, 193, _T("SL/centerRight"));
    s.SetPos(50);
    c = s.ComputeThumbCenter();
    EXPECT_INT(c.x, 7 + (193 - 7) / 2, _T("SL/centerMid"));
    return OK(_T("SL_ThumbCenter"));
}

static Result Test_SL_PosFromPixel()
{
    DuiSlider s;
    s.SetRect(RECT{ 0, 0, 200, 30 });
    s.SetRange(0, 100);
    EXPECT_INT(s.PosFromPixelX(7),   0,   _T("SL/leftEdge"));
    EXPECT_INT(s.PosFromPixelX(193), 100, _T("SL/rightEdge"));
    EXPECT_INT(s.PosFromPixelX(0),   0,   _T("SL/clampLeft"));
    EXPECT_INT(s.PosFromPixelX(999), 100, _T("SL/clampRight"));
    return OK(_T("SL_PosFromPixel"));
}

static Result Test_SL_Keyboard()
{
    DuiSlider s;
    s.SetRange(0, 100);
    s.SetLineSize(5);
    s.SetPos(50);
    s.OnKeyDown(VK_RIGHT, 0);
    EXPECT_INT(s.GetPos(), 55, _T("SL/right"));
    s.OnKeyDown(VK_LEFT, 0);
    EXPECT_INT(s.GetPos(), 50, _T("SL/left"));
    s.OnKeyDown(VK_HOME, 0);
    EXPECT_INT(s.GetPos(), 0,  _T("SL/home"));
    s.OnKeyDown(VK_END, 0);
    EXPECT_INT(s.GetPos(), 100,_T("SL/end"));
    s.OnKeyDown(VK_PRIOR, 0);
    EXPECT_INT(s.GetPos(), 50, _T("SL/pgUp"));
    s.OnKeyDown(VK_NEXT, 0);
    EXPECT_INT(s.GetPos(), 100,_T("SL/pgDn"));
    return OK(_T("SL_Keyboard"));
}

static Result Test_SL_MouseWheel()
{
    DuiSlider s;
    s.SetRange(0, 100);
    s.SetLineSize(10);
    s.SetPos(40);
    s.OnMouseWheel(POINT{0,0},  120, 0);
    EXPECT_INT(s.GetPos(), 50, _T("SL/wheelUp"));
    s.OnMouseWheel(POINT{0,0}, -120, 0);
    EXPECT_INT(s.GetPos(), 40, _T("SL/wheelDown"));
    return OK(_T("SL_MouseWheel"));
}

// ============================ DuiComboBox =============================

static Result Test_CB_Defaults()
{
    DuiComboBox c;
    EXPECT_INT(c.GetCount(),       0,  _T("CB/empty"));
    EXPECT_INT(c.GetCurSel(),     -1,  _T("CB/noSel"));
    EXPECT_BOOL(c.IsPopupOpen(), false,_T("CB/popupClosed"));
    return OK(_T("CB_Defaults"));
}

static Result Test_CB_AddDelete()
{
    DuiComboBox c;
    EXPECT_INT(c.AddString(_T("a")), 0, _T("CB/add0"));
    EXPECT_INT(c.AddString(_T("b")), 1, _T("CB/add1"));
    EXPECT_INT(c.AddString(_T("c")), 2, _T("CB/add2"));
    EXPECT_INT(c.GetCount(), 3, _T("CB/count3"));
    EXPECT_STR(c.GetItemText(1), _T("b"), _T("CB/text1"));
    c.DeleteString(1);
    EXPECT_INT(c.GetCount(), 2, _T("CB/count2"));
    EXPECT_STR(c.GetItemText(1), _T("c"), _T("CB/textShifted"));
    c.ResetContent();
    EXPECT_INT(c.GetCount(), 0, _T("CB/reset"));
    return OK(_T("CB_AddDelete"));
}

static Result Test_CB_DeleteAdjustsSel()
{
    DuiComboBox c;
    c.AddString(_T("a"));
    c.AddString(_T("b"));
    c.AddString(_T("c"));
    c.SetCurSel(2, /*notify=*/false);
    c.DeleteString(0);    // sel was 2 -> should now be 1
    EXPECT_INT(c.GetCurSel(), 1, _T("CB/selShiftAfterDel"));
    c.DeleteString(1);    // remove the selected one
    EXPECT_INT(c.GetCurSel(), -1, _T("CB/selResetOnOwnDel"));
    return OK(_T("CB_DeleteAdjustsSel"));
}

static Result Test_CB_SetCurSelClamp()
{
    DuiComboBox c;
    c.AddString(_T("a"));
    c.AddString(_T("b"));
    c.SetCurSel(99, /*notify=*/false);
    EXPECT_INT(c.GetCurSel(), -1, _T("CB/setOOBIgnored"));
    c.SetCurSel(1, /*notify=*/false);
    EXPECT_INT(c.GetCurSel(), 1, _T("CB/set1"));
    c.SetCurSel(1, /*notify=*/false);   // idempotent
    EXPECT_INT(c.GetCurSel(), 1, _T("CB/idem"));
    return OK(_T("CB_SetCurSelClamp"));
}

static Result Test_CB_OOBSafe()
{
    DuiComboBox c;
    c.AddString(_T("only"));
    EXPECT_STR(c.GetItemText(99), _T(""), _T("CB/textOOB"));
    c.DeleteString(-1);                     // safe no-op
    c.DeleteString(99);                     // safe no-op
    EXPECT_INT(c.GetCount(), 1, _T("CB/oobNoop"));
    c.SetItemText(99, _T("ignored"));       // safe no-op
    EXPECT_STR(c.GetItemText(0), _T("only"), _T("CB/setOOBNoChange"));
    return OK(_T("CB_OOBSafe"));
}

// Editable-mode toggling does not break read-only behavior. Item lookup
// works in both modes via the combo's GetText/SetText surface.
static Result Test_CB_EditableDefault()
{
    DuiComboBox c;
    EXPECT_BOOL(c.IsEditable(), false, _T("CB/editDefaultFalse"));
    c.SetEditable(true);
    EXPECT_BOOL(c.IsEditable(), true,  _T("CB/editTrue"));
    c.SetEditable(false);
    EXPECT_BOOL(c.IsEditable(), false, _T("CB/editToggleBack"));
    return OK(_T("CB_EditableDefault"));
}

// In read-only mode, GetText returns selected item; SetText finds and selects.
static Result Test_CB_GetSetTextReadOnly()
{
    DuiComboBox c;
    c.AddString(_T("alpha"));
    c.AddString(_T("beta"));
    c.AddString(_T("gamma"));
    c.SetCurSel(1, /*notify=*/false);
    EXPECT_STR(c.GetText(), _T("beta"), _T("CB/getTextSel"));
    c.SetText(_T("gamma"));   // matches an item -> selects it
    EXPECT_INT(c.GetCurSel(), 2, _T("CB/setTextMatchSel"));
    c.SetText(_T("nonexistent")); // no match in read-only -> selection unchanged
    EXPECT_INT(c.GetCurSel(), 2, _T("CB/setTextNoMatchKeep"));
    return OK(_T("CB_GetSetTextReadOnly"));
}

// Editable mode preserves the items list across mode toggle.
static Result Test_CB_EditableKeepsItems()
{
    DuiComboBox c;
    c.AddString(_T("a"));
    c.AddString(_T("b"));
    c.AddString(_T("c"));
    c.SetCurSel(1, /*notify=*/false);
    c.SetEditable(true);
    EXPECT_INT(c.GetCount(), 3, _T("CB/editKeepCount"));
    EXPECT_STR(c.GetItemText(2), _T("c"), _T("CB/editKeepText"));
    c.SetEditable(false);
    EXPECT_INT(c.GetCount(), 3, _T("CB/roKeepCount"));
    return OK(_T("CB_EditableKeepsItems"));
}

#undef EXPECT_INT
#undef EXPECT_BOOL
#undef EXPECT_STR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("PB_Defaults"),           &Test_PB_Defaults           },
        { _T("PB_PosClamp"),           &Test_PB_PosClamp           },
        { _T("PB_FillWidth"),          &Test_PB_FillWidth          },
        { _T("PB_FillRectGeometry"),   &Test_PB_FillRectGeometry   },
        { _T("PB_RangeShrinkClamps"),  &Test_PB_RangeShrinkClamps  },
        { _T("SL_Defaults"),           &Test_SL_Defaults           },
        { _T("SL_PosClamp"),           &Test_SL_PosClamp           },
        { _T("SL_ThumbCenter"),        &Test_SL_ThumbCenter        },
        { _T("SL_PosFromPixel"),       &Test_SL_PosFromPixel       },
        { _T("SL_Keyboard"),           &Test_SL_Keyboard           },
        { _T("SL_MouseWheel"),         &Test_SL_MouseWheel         },
        { _T("CB_Defaults"),           &Test_CB_Defaults           },
        { _T("CB_AddDelete"),          &Test_CB_AddDelete          },
        { _T("CB_DeleteAdjustsSel"),   &Test_CB_DeleteAdjustsSel   },
        { _T("CB_SetCurSelClamp"),     &Test_CB_SetCurSelClamp     },
        { _T("CB_OOBSafe"),            &Test_CB_OOBSafe            },
        { _T("CB_EditableDefault"),    &Test_CB_EditableDefault    },
        { _T("CB_GetSetTextReadOnly"), &Test_CB_GetSetTextReadOnly },
        { _T("CB_EditableKeepsItems"), &Test_CB_EditableKeepsItems }
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
    summary.Format(_T("[summary] DuiSmallControlsTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiSmallControlsTests

} // namespace balloonwjui

#endif // BUI_FEATURE_PROGRESSBAR && BUI_FEATURE_SLIDER && BUI_FEATURE_COMBOBOX
