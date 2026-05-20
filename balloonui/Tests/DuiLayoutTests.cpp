#include "stdafx.h"
#include "DuiLayoutTests.h"

namespace balloonwjui {

namespace DuiLayoutTests {

namespace {

// Stub control: records the rect it was placed at, without painting.
class StubChild : public DuiControl
{
public:
    void OnPaint(HDC, const RECT&) override {}
};

// Test helpers
struct Result { CString name; bool ok; CString detail; };

static Result MakeOK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok = true;
    return r;
}
static Result MakeFail(const CString& n, const CString& d)
{
    Result r;
    r.name = n;
    r.ok = false;
    r.detail = d;
    return r;
}

static CString FmtRect(const RECT& r)
{
    CString s;
    s.Format(_T("(%d,%d,%d,%d)"), r.left, r.top, r.right, r.bottom);
    return s;
}

#define EXPECT_RECT(child, L, T, R, B, name) \
    do { \
        const RECT& _r = (child)->GetRect(); \
        if (_r.left != (L) || _r.top != (T) || _r.right != (R) || _r.bottom != (B)) { \
            CString _d; _d.Format(_T("expected (%d,%d,%d,%d) got %s"), \
                                  (int)(L),(int)(T),(int)(R),(int)(B), (LPCTSTR)FmtRect(_r)); \
            return MakeFail(name, _d); \
        } \
    } while (0)

// Compare a SIZE returned by GetDesiredSize against (expectedCx, expectedCy).
// Used by the *_Desired_* tests below to lock in DuiVBox/DuiHBox/DuiGrid's
// intrinsic-size formula (sum of fixed main + margins + gaps + padding;
// max of cross + padding).
#define EXPECT_DESIRED(layout, EXPCX, EXPCY, name) \
    do { \
        SIZE _s = (layout).GetDesiredSize(); \
        if (_s.cx != (EXPCX) || _s.cy != (EXPCY)) { \
            CString _d; _d.Format(_T("expected (%d,%d) got (%d,%d)"), \
                                  (int)(EXPCX),(int)(EXPCY),(int)_s.cx,(int)_s.cy); \
            return MakeFail(name, _d); \
        } \
    } while (0)

// A child that reports its own intrinsic size — needed to test the cross-axis
// max-rollup path in DuiVBox/DuiHBox::GetDesiredSize() (the main-axis path
// uses Hint::fixedMain, no child measurement involved).
class MeasuredChild : public DuiControl
{
public:
    MeasuredChild(int cx, int cy) : m_w(cx), m_h(cy) {}
    void OnPaint(HDC, const RECT&) override {}
    SIZE GetDesiredSize() const override { return SIZE{ m_w, m_h }; }
private:
    int m_w, m_h;
};

// ----- individual tests -------------------------------------------------

// HBox: 3 weighted children fill 300px equally, no padding/gap.
static Result Test_HBox_EqualWeights()
{
    DuiHBox box;
    StubChild *a, *b, *c;
    box.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()));
    box.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()));
    box.AddChild(std::unique_ptr<DuiControl>(c = new StubChild()));
    box.Layout(RECT{0, 0, 300, 50});
    EXPECT_RECT(a,   0, 0, 100, 50, _T("HBox_EqualWeights/a"));
    EXPECT_RECT(b, 100, 0, 200, 50, _T("HBox_EqualWeights/b"));
    EXPECT_RECT(c, 200, 0, 300, 50, _T("HBox_EqualWeights/c"));
    return MakeOK(_T("HBox_EqualWeights"));
}

// HBox: fixed + flex. Fixed takes 50, remaining 250 split 1:4.
static Result Test_HBox_FixedAndWeight()
{
    DuiHBox box;
    StubChild *a, *b, *c;
    box.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()), DuiLayout::Hint().Fixed(50));
    box.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()), DuiLayout::Hint().Weight(1));
    box.AddChild(std::unique_ptr<DuiControl>(c = new StubChild()), DuiLayout::Hint().Weight(4));
    box.Layout(RECT{0, 0, 300, 50});
    // fixed=50, gap=0, flex=250, weight 1+4=5 -> 50, 200
    EXPECT_RECT(a,   0, 0,  50, 50, _T("HBox_FixedAndWeight/a"));
    EXPECT_RECT(b,  50, 0, 100, 50, _T("HBox_FixedAndWeight/b"));
    EXPECT_RECT(c, 100, 0, 300, 50, _T("HBox_FixedAndWeight/c"));
    return MakeOK(_T("HBox_FixedAndWeight"));
}

// HBox: padding + gap.
static Result Test_HBox_PaddingGap()
{
    DuiHBox box;
    box.SetPadding(10);
    box.SetGap(5);
    StubChild *a, *b;
    box.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()));
    box.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()));
    box.Layout(RECT{0, 0, 100, 40});
    // inner 10..90 width=80, minus gap5 -> 75 for two children -> 37 + 38 (integer div: 75/2=37 each, then last absorbs remainder via x increment)
    // Actually with weights 1:1 and weightSum=2, flexAvail=75, each gets 75*1/2=37 -> a:37, b:37
    // Layout walks: x=10; a width=37, x->47; gap5 -> x=52; b width=37, x->89.
    EXPECT_RECT(a, 10, 10, 47, 30, _T("HBox_PaddingGap/a"));
    EXPECT_RECT(b, 52, 10, 89, 30, _T("HBox_PaddingGap/b"));
    return MakeOK(_T("HBox_PaddingGap"));
}

// HBox: invisible child takes no space and no gap.
static Result Test_HBox_InvisibleSkipped()
{
    DuiHBox box;
    box.SetGap(10);
    StubChild *a, *b, *c;
    box.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()));
    box.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()));
    box.AddChild(std::unique_ptr<DuiControl>(c = new StubChild()));
    b->SetVisible(false);
    box.Layout(RECT{0, 0, 210, 30});
    // 2 visible children, gap 10. flex = 210 - 10 = 200, /2 = 100 each.
    // x=0, a=0..100, gap -> 110, c=110..210.
    EXPECT_RECT(a,   0, 0, 100, 30, _T("HBox_InvisibleSkipped/a"));
    EXPECT_RECT(c, 110, 0, 210, 30, _T("HBox_InvisibleSkipped/c"));
    return MakeOK(_T("HBox_InvisibleSkipped"));
}

// VBox: 2 weighted children fill height.
static Result Test_VBox_EqualWeights()
{
    DuiVBox box;
    StubChild *a, *b;
    box.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()));
    box.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()));
    box.Layout(RECT{0, 0, 100, 100});
    EXPECT_RECT(a, 0,  0, 100,  50, _T("VBox_EqualWeights/a"));
    EXPECT_RECT(b, 0, 50, 100, 100, _T("VBox_EqualWeights/b"));
    return MakeOK(_T("VBox_EqualWeights"));
}

// VBox with fixed + margin + AlignNear cross-axis.
static Result Test_VBox_MarginAlignNear()
{
    DuiVBox box;
    StubChild *a;
    DuiLayout::Hint h;
    h.Fixed(20, 30).Margin(2, 3, 4, 5).AlignC(DuiLayout::AlignNear);
    box.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()), h);
    box.Layout(RECT{0, 0, 100, 100});
    // VBox cell: inner=full, child cellY=0..28 (margin top3+main20+bottom5=28).
    // After margins: r.left=2, r.right=96 (100-4), r.top=3, r.bottom=23 (28-5).
    // wantCross=30, availW=94 -> 30 < 94 so apply align. Cross axis is X
    // (mainIsHorizontal=false), AlignNear -> x=2, w=30 -> 2..32.
    // Main axis Y: wantMain=20, availH=20 -> equal -> fill -> 3..23.
    EXPECT_RECT(a, 2, 3, 32, 23, _T("VBox_MarginAlignNear"));
    return MakeOK(_T("VBox_MarginAlignNear"));
}

// Grid: 2x3, 6 children fill cells row-major.
static Result Test_Grid_RowMajor()
{
    DuiGrid grid;
    grid.SetGrid(2, 3);
    StubChild* c[6];
    for (int i = 0; i < 6; ++i)
    {
        grid.AddChild(std::unique_ptr<DuiControl>(c[i] = new StubChild()));
    }
    grid.Layout(RECT{0, 0, 300, 200});
    // cellW=100, cellH=100 (no gap).
    // Row-major: c[0]=0,0,100,100; c[1]=100,0,200,100; c[2]=200,0,300,100;
    //            c[3]=0,100,100,200; c[4]=100,100,200,200; c[5]=200,100,300,200
    EXPECT_RECT(c[0],   0,   0, 100, 100, _T("Grid/0"));
    EXPECT_RECT(c[1], 100,   0, 200, 100, _T("Grid/1"));
    EXPECT_RECT(c[2], 200,   0, 300, 100, _T("Grid/2"));
    EXPECT_RECT(c[3],   0, 100, 100, 200, _T("Grid/3"));
    EXPECT_RECT(c[4], 100, 100, 200, 200, _T("Grid/4"));
    EXPECT_RECT(c[5], 200, 100, 300, 200, _T("Grid/5"));
    return MakeOK(_T("Grid_RowMajor"));
}

// Grid with gap.
static Result Test_Grid_Gap()
{
    DuiGrid grid;
    grid.SetGrid(2, 2);
    grid.SetGap(10);
    StubChild* c[4];
    for (int i = 0; i < 4; ++i)
    {
        grid.AddChild(std::unique_ptr<DuiControl>(c[i] = new StubChild()));
    }
    grid.Layout(RECT{0, 0, 110, 110});
    // gapsX=10, gapsY=10, cellW=(110-10)/2=50, cellH=50
    // c[0]=0,0,50,50; c[1]=60,0,110,50; c[2]=0,60,50,110; c[3]=60,60,110,110
    EXPECT_RECT(c[0],  0,  0,  50,  50, _T("Grid_Gap/0"));
    EXPECT_RECT(c[1], 60,  0, 110,  50, _T("Grid_Gap/1"));
    EXPECT_RECT(c[2],  0, 60,  50, 110, _T("Grid_Gap/2"));
    EXPECT_RECT(c[3], 60, 60, 110, 110, _T("Grid_Gap/3"));
    return MakeOK(_T("Grid_Gap"));
}

// Resize triggers re-layout.
static Result Test_Resize_Reflows()
{
    DuiHBox box;
    StubChild *a, *b;
    box.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()));
    box.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()));
    box.Layout(RECT{0, 0, 100, 50});
    EXPECT_RECT(a,   0, 0,  50, 50, _T("Resize/initial-a"));
    EXPECT_RECT(b,  50, 0, 100, 50, _T("Resize/initial-b"));
    box.Layout(RECT{0, 0, 200, 50});   // grow
    EXPECT_RECT(a,   0, 0, 100, 50, _T("Resize/grown-a"));
    EXPECT_RECT(b, 100, 0, 200, 50, _T("Resize/grown-b"));
    return MakeOK(_T("Resize_Reflows"));
}

// Empty container doesn't crash.
static Result Test_Empty_NoCrash()
{
    DuiHBox box;
    box.Layout(RECT{0, 0, 100, 100});
    DuiVBox v;
    v.Layout(RECT{0, 0, 100, 100});
    DuiGrid g;
    g.SetGrid(3, 3);
    g.Layout(RECT{0, 0, 100, 100});
    return MakeOK(_T("Empty_NoCrash"));
}

// =====================================================================
// GetDesiredSize tests for DuiVBox / DuiHBox / DuiGrid
// =====================================================================
//
// Why these exist:
//   DuiScrollView::SetAutoContentHeight(true) reads m_content->GetDesiredSize().cy
//   to decide its scroll range. Containers that didn't override GetDesiredSize
//   returned {0,0} (DuiControl base default) which silently disabled the
//   auto-height path — DuiGallery had to fall back to a hardcoded 1500.
//
//   Formula (kept identical for all three containers along their main axis):
//     main_intrinsic = padMain1 + padMain2
//                    + sum_over_visible_children(max(0, hint.fixedMain)
//                                                + hint.marginMain1
//                                                + hint.marginMain2)
//                    + gap * max(0, visN - 1)
//   Cross axis takes max over children of (max(hint.fixedCross,
//   child.GetDesiredSize().cross_dim) + cross margins) + cross padding.
//
//   Weighted children (hint.fixedMain < 0) contribute 0 to main intrinsic.
//   That's "shrink to fit fixed content" — matches the semantic
//   ScrollView wants when it asks "what's the smallest height that fits".
//
// What's covered:
//   - Empty container reports just padding.
//   - Sum of fixed children + gaps + padding is exact.
//   - marginT/B (or L/R) participate in main intrinsic.
//   - Hidden children skipped from both sum and gap count.
//   - Weighted children contribute 0 to main intrinsic.
//   - Cross intrinsic rolls up max child cross size + padding.
//   - Grid intrinsic = rows*cellH + gaps + padding (and cols*cellW).

// ---- DuiVBox::GetDesiredSize -----------------------------------------

// Empty VBox with padding only: cx = padL+padR, cy = padT+padB.
static Result Test_VBox_Desired_Empty()
{
    DuiVBox v;
    v.SetPadding(7, 11, 5, 13);
    EXPECT_DESIRED(v, 12 /*7+5*/, 24 /*11+13*/, _T("VBox_Desired_Empty"));
    return MakeOK(_T("VBox_Desired_Empty"));
}

// Three Fixed children + gap + padding: height is exact sum.
static Result Test_VBox_Desired_FixedSum()
{
    DuiVBox v;
    v.SetPadding(10);
    v.SetGap(5);
    StubChild *a, *b, *c;
    v.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()),
               DuiLayout::Hint().Fixed(40));
    v.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()),
               DuiLayout::Hint().Fixed(60));
    v.AddChild(std::unique_ptr<DuiControl>(c = new StubChild()),
               DuiLayout::Hint().Fixed(80));
    // cy = padT + a + gap + b + gap + c + padB = 10+40+5+60+5+80+10 = 210
    // cx = padL + padR = 20 (no child reports cross intrinsic).
    EXPECT_DESIRED(v, 20, 210, _T("VBox_Desired_FixedSum"));
    return MakeOK(_T("VBox_Desired_FixedSum"));
}

// Margin participates on both axes: T/B add to main intrinsic; L/R add to
// cross intrinsic even when the child reports 0 cross size (the container
// still has to reserve the child's frame).
static Result Test_VBox_Desired_MarginIncluded()
{
    DuiVBox v;
    StubChild *a;
    v.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()),
               DuiLayout::Hint().Fixed(40).Margin(8, 4, 8, 6));
    // cy = 0 + (40 + marginT4 + marginB6) + 0 = 50
    // cx = 0 + (childCx0 + marginL8 + marginR8) + 0 = 16
    EXPECT_DESIRED(v, 16, 50, _T("VBox_Desired_MarginIncluded"));
    return MakeOK(_T("VBox_Desired_MarginIncluded"));
}

// Hidden children don't contribute to sum and don't count toward gap*(visN-1).
static Result Test_VBox_Desired_HiddenSkipped()
{
    DuiVBox v;
    v.SetGap(10);
    StubChild *a, *b, *c;
    v.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()),
               DuiLayout::Hint().Fixed(20));
    v.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()),
               DuiLayout::Hint().Fixed(30));
    v.AddChild(std::unique_ptr<DuiControl>(c = new StubChild()),
               DuiLayout::Hint().Fixed(40));
    b->SetVisible(false);
    // Visible: a + c. cy = 20 + gap10 + 40 = 70 (NOT 20+10+30+10+40).
    EXPECT_DESIRED(v, 0, 70, _T("VBox_Desired_HiddenSkipped"));
    return MakeOK(_T("VBox_Desired_HiddenSkipped"));
}

// Weighted child (fixedMain < 0) contributes 0 to main intrinsic — its size
// only materializes when there's leftover space to share. Mixed with a fixed
// child the intrinsic is just the fixed-child's height + the gap.
static Result Test_VBox_Desired_WeightedZeroContribution()
{
    DuiVBox v;
    v.SetGap(8);
    StubChild *fixedC, *weightC;
    v.AddChild(std::unique_ptr<DuiControl>(fixedC = new StubChild()),
               DuiLayout::Hint().Fixed(50));
    v.AddChild(std::unique_ptr<DuiControl>(weightC = new StubChild()),
               DuiLayout::Hint().Weight(1));
    // cy = 50 + gap8 + 0 (weighted) = 58. visN=2 so 1 gap counted.
    EXPECT_DESIRED(v, 0, 58, _T("VBox_Desired_WeightedZeroContribution"));
    return MakeOK(_T("VBox_Desired_WeightedZeroContribution"));
}

// Cross axis (X for VBox) rolls up max(child fixedCross, child.GetDesiredSize().cx)
// across visible children; per-child marginL/R included; padding added once.
static Result Test_VBox_Desired_CrossMaxRollup()
{
    DuiVBox v;
    v.SetPadding(3, 0, 5, 0);
    // Child A: fixedCross=100, marginL=2, marginR=4 → cross contribution 106
    // Child B: GetDesiredSize.cx=70  (no margin)            → cross contribution 70
    // Container intrinsic cx = max(106, 70) + padL3 + padR5 = 114
    auto a = std::unique_ptr<StubChild>(new StubChild());
    v.AddChild(std::move(a),
               DuiLayout::Hint().Fixed(20, /*cross*/100).Margin(2, 0, 4, 0));
    auto b = std::unique_ptr<MeasuredChild>(new MeasuredChild(70, 30));
    v.AddChild(std::move(b), DuiLayout::Hint().Fixed(30));
    EXPECT_DESIRED(v, 114, 50 /*20+30 main; no margins on b*/,
                   _T("VBox_Desired_CrossMaxRollup"));
    return MakeOK(_T("VBox_Desired_CrossMaxRollup"));
}

// ---- DuiHBox::GetDesiredSize (mirror of VBox along the X axis) -------

static Result Test_HBox_Desired_Empty()
{
    DuiHBox h;
    h.SetPadding(7, 11, 5, 13);
    EXPECT_DESIRED(h, 12, 24, _T("HBox_Desired_Empty"));
    return MakeOK(_T("HBox_Desired_Empty"));
}

static Result Test_HBox_Desired_FixedSum()
{
    DuiHBox h;
    h.SetPadding(10);
    h.SetGap(5);
    StubChild *a, *b, *c;
    h.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()),
               DuiLayout::Hint().Fixed(40));
    h.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()),
               DuiLayout::Hint().Fixed(60));
    h.AddChild(std::unique_ptr<DuiControl>(c = new StubChild()),
               DuiLayout::Hint().Fixed(80));
    // cx = 10+40+5+60+5+80+10 = 210
    // cy = 20 (no child reports cross intrinsic)
    EXPECT_DESIRED(h, 210, 20, _T("HBox_Desired_FixedSum"));
    return MakeOK(_T("HBox_Desired_FixedSum"));
}

static Result Test_HBox_Desired_MarginIncluded()
{
    DuiHBox h;
    StubChild *a;
    h.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()),
               DuiLayout::Hint().Fixed(40).Margin(4, 8, 6, 8));
    // cx = 0 + (40 + marginL4 + marginR6) + 0 = 50
    // cy = 0 + (childCy0 + marginT8 + marginB8) + 0 = 16
    EXPECT_DESIRED(h, 50, 16, _T("HBox_Desired_MarginIncluded"));
    return MakeOK(_T("HBox_Desired_MarginIncluded"));
}

static Result Test_HBox_Desired_HiddenSkipped()
{
    DuiHBox h;
    h.SetGap(10);
    StubChild *a, *b, *c;
    h.AddChild(std::unique_ptr<DuiControl>(a = new StubChild()),
               DuiLayout::Hint().Fixed(20));
    h.AddChild(std::unique_ptr<DuiControl>(b = new StubChild()),
               DuiLayout::Hint().Fixed(30));
    h.AddChild(std::unique_ptr<DuiControl>(c = new StubChild()),
               DuiLayout::Hint().Fixed(40));
    b->SetVisible(false);
    // Visible a + c. cx = 20 + gap10 + 40 = 70.
    EXPECT_DESIRED(h, 70, 0, _T("HBox_Desired_HiddenSkipped"));
    return MakeOK(_T("HBox_Desired_HiddenSkipped"));
}

static Result Test_HBox_Desired_WeightedZeroContribution()
{
    DuiHBox h;
    h.SetGap(8);
    StubChild *fixedC, *weightC;
    h.AddChild(std::unique_ptr<DuiControl>(fixedC = new StubChild()),
               DuiLayout::Hint().Fixed(50));
    h.AddChild(std::unique_ptr<DuiControl>(weightC = new StubChild()),
               DuiLayout::Hint().Weight(1));
    // cx = 50 + gap8 + 0 = 58
    EXPECT_DESIRED(h, 58, 0, _T("HBox_Desired_WeightedZeroContribution"));
    return MakeOK(_T("HBox_Desired_WeightedZeroContribution"));
}

static Result Test_HBox_Desired_CrossMaxRollup()
{
    DuiHBox h;
    h.SetPadding(0, 3, 0, 5);
    // Child A: fixedMain=20, fixedCross=100 (Y), marginT=2, marginB=4 → cross 106
    // Child B: MeasuredChild(70,30) → cross 30 (cy)
    // Container intrinsic cy = max(106, 30) + padT3 + padB5 = 114
    h.AddChild(std::unique_ptr<DuiControl>(new StubChild()),
               DuiLayout::Hint().Fixed(20, /*cross*/100).Margin(0, 2, 0, 4));
    h.AddChild(std::unique_ptr<DuiControl>(new MeasuredChild(70, 30)),
               DuiLayout::Hint().Fixed(30));
    EXPECT_DESIRED(h, 50 /*20+30 main; no margins on b*/, 114,
                   _T("HBox_Desired_CrossMaxRollup"));
    return MakeOK(_T("HBox_Desired_CrossMaxRollup"));
}

// ---- DuiGrid::GetDesiredSize -----------------------------------------
//
// For Grid, "intrinsic" is rows*cellH + gaps + padding (and cols*cellW).
// cellW = max(child fixedCross-or-DesiredSize.cx + marginL+R) across children.
// cellH = max(child fixedMain-or-DesiredSize.cy + marginT+B) across children.
//
// Note: in DuiGrid::Layout (existing), each child is addressed by row/col
// position in rcAvail, not by hint sums. Hints only affect within-cell
// alignment. So GetDesiredSize is the only place that asks "what cell size
// would children naturally want".

static Result Test_Grid_Desired_FixedCellSize()
{
    DuiGrid g;
    g.SetGrid(2, 3);
    // 6 children, each Fixed(50, 30) — fixedMain=50, fixedCross=30.
    // For Grid, "main" = column-direction width; treat fixedMain as cellW
    // contribution and fixedCross as cellH contribution.
    // Wait — that's HBox-axis convention. For Grid we use the same rule
    // as HBox: main = X, cross = Y. cellW from main, cellH from cross.
    for (int i = 0; i < 6; ++i)
    {
        g.AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                   DuiLayout::Hint().Fixed(50, /*cross=*/30));
    }
    // cellW = 50, cellH = 30. cols=3, rows=2, no gap, no padding.
    // cx = 3*50 = 150; cy = 2*30 = 60
    EXPECT_DESIRED(g, 150, 60, _T("Grid_Desired_FixedCellSize"));
    return MakeOK(_T("Grid_Desired_FixedCellSize"));
}

static Result Test_Grid_Desired_GapPadding()
{
    DuiGrid g;
    g.SetGrid(2, 3);
    g.SetGap(5);
    g.SetPadding(10);
    for (int i = 0; i < 6; ++i)
    {
        g.AddChild(std::unique_ptr<DuiControl>(new StubChild()),
                   DuiLayout::Hint().Fixed(20, 20));
    }
    // cellW=20 cellH=20, cols=3 rows=2.
    // cx = padL10 + 3*cellW20 + (cols-1)*gap5 = 10 + 60 + 10 + 10 = 90
    // cy = padT10 + 2*cellH20 + (rows-1)*gap5 = 10 + 40 + 5 + 10 = 65
    EXPECT_DESIRED(g, 90, 65, _T("Grid_Desired_GapPadding"));
    return MakeOK(_T("Grid_Desired_GapPadding"));
}

static Result Test_Grid_Desired_NoChildren()
{
    DuiGrid g;
    g.SetGrid(2, 3);
    g.SetPadding(7);
    // No children -> cellW = cellH = 0; intrinsic = padding only.
    EXPECT_DESIRED(g, 14, 14, _T("Grid_Desired_NoChildren"));
    return MakeOK(_T("Grid_Desired_NoChildren"));
}

#undef EXPECT_DESIRED
#undef EXPECT_RECT

} // anonymous namespace

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("HBox_EqualWeights"),    &Test_HBox_EqualWeights    },
        { _T("HBox_FixedAndWeight"),  &Test_HBox_FixedAndWeight  },
        { _T("HBox_PaddingGap"),      &Test_HBox_PaddingGap      },
        { _T("HBox_InvisibleSkipped"),&Test_HBox_InvisibleSkipped},
        { _T("VBox_EqualWeights"),    &Test_VBox_EqualWeights    },
        { _T("VBox_MarginAlignNear"), &Test_VBox_MarginAlignNear },
        { _T("Grid_RowMajor"),        &Test_Grid_RowMajor        },
        { _T("Grid_Gap"),             &Test_Grid_Gap             },
        { _T("Resize_Reflows"),       &Test_Resize_Reflows       },
        { _T("Empty_NoCrash"),        &Test_Empty_NoCrash        },
        // Intrinsic-size (GetDesiredSize) tests — drive
        // DuiScrollView::SetAutoContentHeight(true) for VBox-rooted pages.
        { _T("VBox_Desired_Empty"),                  &Test_VBox_Desired_Empty                  },
        { _T("VBox_Desired_FixedSum"),               &Test_VBox_Desired_FixedSum               },
        { _T("VBox_Desired_MarginIncluded"),         &Test_VBox_Desired_MarginIncluded         },
        { _T("VBox_Desired_HiddenSkipped"),          &Test_VBox_Desired_HiddenSkipped          },
        { _T("VBox_Desired_WeightedZeroContribution"),&Test_VBox_Desired_WeightedZeroContribution},
        { _T("VBox_Desired_CrossMaxRollup"),         &Test_VBox_Desired_CrossMaxRollup         },
        { _T("HBox_Desired_Empty"),                  &Test_HBox_Desired_Empty                  },
        { _T("HBox_Desired_FixedSum"),               &Test_HBox_Desired_FixedSum               },
        { _T("HBox_Desired_MarginIncluded"),         &Test_HBox_Desired_MarginIncluded         },
        { _T("HBox_Desired_HiddenSkipped"),          &Test_HBox_Desired_HiddenSkipped          },
        { _T("HBox_Desired_WeightedZeroContribution"),&Test_HBox_Desired_WeightedZeroContribution},
        { _T("HBox_Desired_CrossMaxRollup"),         &Test_HBox_Desired_CrossMaxRollup         },
        { _T("Grid_Desired_FixedCellSize"),          &Test_Grid_Desired_FixedCellSize          },
        { _T("Grid_Desired_GapPadding"),             &Test_Grid_Desired_GapPadding             },
        { _T("Grid_Desired_NoChildren"),             &Test_Grid_Desired_NoChildren             },
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
    summary.Format(_T("[summary] DuiLayoutTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiLayoutTests

} // namespace balloonwjui
