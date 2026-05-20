#include "stdafx.h"
#include "Pages.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Layout/DuiSplitter.h"
#include "Controls/List/DuiTabPage.h"
#include "Controls/List/DuiTreeView.h"
#include "Controls/Feedback/DuiPopupHost.h"
#include "Controls/Feedback/DuiEmojiPanel.h"
#include "Controls/Window/DuiFrameWindow.h"
#include "Controls/Layout/DuiDock.h"
#include "DuiXmlBuilder.h"
#include "Controls/Basic/DuiSeparator.h"
#include "Controls/Basic/DuiBadge.h"
#include "Controls/Basic/DuiGroupBox.h"
#include "Controls/Input/DuiSearchBox.h"
#include "Controls/Input/DuiSpinBox.h"
#include "DuiTheme.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Input/DuiEditHost.h"
#include "Controls/Input/DuiRichEditHost.h"
#include "DuiPaintAA.h"

#include <functional>
#include "Controls/Input/DuiComboBox.h"
#include "Controls/Input/DuiSlider.h"
#include "Controls/Input/DuiSwitch.h"
#include "Controls/Feedback/DuiProgressBar.h"
#include "Controls/List/DuiTab.h"
#include "Controls/List/DuiListBox.h"
#include "Controls/Window/DuiScrollBar.h"
#include "Controls/List/DuiMenu.h"
#include "Controls/List/DuiMenuBar.h"
#include "Controls/Feedback/DuiToolTip.h"
#include "DuiHost.h"
#include "DuiPaintAA.h"

using namespace balloonwjui;

namespace Pages {

// ===== capture registry ==============================================
//
// The registry is a flat list cleared by the harness between pages.
// AddVariantRowCapture() pushes into it; main.cpp's --capture-all loop
// drains it after each page is built and laid out.

std::vector<CaptureMark>& GetCaptureMarks()
{
    static std::vector<CaptureMark> s;
    return s;
}

void RegisterCapture(LPCTSTR name, balloonwjui::DuiControl* anchor)
{
    if (!name || !anchor)
    {
        return;
    }
    CaptureMark m;
    m.name   = name;
    m.anchor = anchor;
    GetCaptureMarks().push_back(m);
}

// ===== shared helpers =================================================

namespace {

static const int kPageMargin   = 20;
static const int kHeaderH      = 28;
static const int kDescH        = 20;
static const int kVariantGap   = 6;
static const int kSectionGap   = 22;
static const int kRowH         = 36;

// Make a section header label.
static std::unique_ptr<DuiLabel> MakeHeader(LPCTSTR text)
{
    auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
    l->SetText(text);
    l->SetTextColor(RGB(20, 20, 20));
    return l;
}

// Make a description (gray, single line).
static std::unique_ptr<DuiLabel> MakeDesc(LPCTSTR text)
{
    auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
    l->SetText(text);
    l->SetTextColor(RGB(110, 110, 110));
    return l;
}

// Add a section to a parent VBox: header + description (optional).
// Returns the parent for chaining (raw, owned by caller).
static void AddSection(DuiVBox* parent, LPCTSTR header, LPCTSTR desc)
{
    parent->AddChild(MakeHeader(header), DuiLayout::Hint().Fixed(kHeaderH));
    if (desc && *desc)
        parent->AddChild(MakeDesc(desc), DuiLayout::Hint().Fixed(kDescH));
}

// Add a horizontal row of variant controls. Caller passes a pre-built
// DuiHBox; we just stick it into the VBox with a fixed row height.
static DuiControl* AddVariantRow(DuiVBox* parent, std::unique_ptr<DuiHBox> row, int rowH = kRowH)
{
    DuiControl* anchor = row.get();
    parent->AddChild(std::move(row), DuiLayout::Hint().Fixed(rowH));
    return anchor;
}

// Same as AddVariantRow, but also registers a doc-screenshot capture mark
// pointing at the row anchor. Used by --capture-all to emit one PNG per
// row. The capture name becomes the file name (ctl-<name>.png).
static void AddVariantRowCapture(DuiVBox* parent, LPCTSTR captureName,
                                 std::unique_ptr<DuiHBox> row, int rowH = kRowH)
{
    DuiControl* anchor = AddVariantRow(parent, std::move(row), rowH);
    Pages::RegisterCapture(captureName, anchor);
}

// Add a vertical gap before the next section.
static void AddGap(DuiVBox* parent, int h)
{
    parent->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                     DuiLayout::Hint().Fixed(h));
}

// Make an empty page VBox with standard padding.
static std::unique_ptr<DuiVBox> NewPage()
{
    auto v = std::unique_ptr<DuiVBox>(new DuiVBox());
    v->SetPadding(kPageMargin);
    v->SetGap(4);
    return v;
}

} // anonymous

// ===== Button =========================================================

std::unique_ptr<DuiControl> Build_Button()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("PushButton"),
               _T("Brand-blue rounded primary action. White label, hover deepens, disabled grays out."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto bN = std::unique_ptr<DuiButton>(new DuiButton()); bN->SetText(_T("Normal"));
        auto bD = std::unique_ptr<DuiButton>(new DuiButton()); bD->SetText(_T("Disabled")); bD->SetEnabled(false);
        row->AddChild(std::move(bN), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(bD), DuiLayout::Hint().Fixed(120));
        AddVariantRow(page.get(), std::move(row));
    }
    AddGap(page.get(), 8);
    AddSection(page.get(),
               _T("PushButton — 4-state strip (force-state)"),
               _T("Normal / Hover / Pressed / Disabled rendered side-by-side via Debug* APIs."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto bN = std::unique_ptr<DuiButton>(new DuiButton()); bN->SetText(_T("Normal"));
        auto bH = std::unique_ptr<DuiButton>(new DuiButton()); bH->SetText(_T("Hover"));
        bH->DebugSetHover(true);
        auto bP = std::unique_ptr<DuiButton>(new DuiButton()); bP->SetText(_T("Pressed"));
        bP->DebugSetHover(true); bP->DebugSetPressed(true);
        auto bD = std::unique_ptr<DuiButton>(new DuiButton()); bD->SetText(_T("Disabled"));
        bD->SetEnabled(false);
        row->AddChild(std::move(bN), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bH), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bP), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bD), DuiLayout::Hint().Fixed(110));
        AddVariantRowCapture(page.get(),
                             _T("button-pushbutton-states"),
                             std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("IconButton"),
               _T("Light-fill button with a left-side icon glyph. Used for compact actions."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto b = std::unique_ptr<DuiButton>(new DuiButton());
        b->SetButtonType(DuiButton::StyleIcon); b->SetText(_T("Open File"));
        row->AddChild(std::move(b), DuiLayout::Hint().Fixed(160));
        AddVariantRow(page.get(), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Checkbox"),
               _T("Toggle on click. Drag-out cancels. Programmatic SetCheck does not fire UI events."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto c1 = std::unique_ptr<DuiButton>(new DuiButton());
        c1->SetButtonType(DuiButton::StyleCheckbox); c1->SetText(_T("Remember me"));
        auto c2 = std::unique_ptr<DuiButton>(new DuiButton());
        c2->SetButtonType(DuiButton::StyleCheckbox); c2->SetText(_T("Pre-checked"));
        c2->SetCheck(true, false);
        auto c3 = std::unique_ptr<DuiButton>(new DuiButton());
        c3->SetButtonType(DuiButton::StyleCheckbox); c3->SetText(_T("Disabled"));
        c3->SetEnabled(false);
        row->AddChild(std::move(c1), DuiLayout::Hint().Fixed(140));
        row->AddChild(std::move(c2), DuiLayout::Hint().Fixed(140));
        row->AddChild(std::move(c3), DuiLayout::Hint().Fixed(140));
        AddVariantRow(page.get(), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Radio (group=7)"),
               _T("Mutually exclusive within parent + matching radioGroup. Clicking selected is no-op."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto r1 = std::unique_ptr<DuiButton>(new DuiButton());
        r1->SetButtonType(DuiButton::StyleRadio); r1->SetRadioGroup(7);
        r1->SetText(_T("Online")); r1->SetCheck(true, false);
        auto r2 = std::unique_ptr<DuiButton>(new DuiButton());
        r2->SetButtonType(DuiButton::StyleRadio); r2->SetRadioGroup(7);
        r2->SetText(_T("Away"));
        auto r3 = std::unique_ptr<DuiButton>(new DuiButton());
        r3->SetButtonType(DuiButton::StyleRadio); r3->SetRadioGroup(7);
        r3->SetText(_T("Busy"));
        row->AddChild(std::move(r1), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(r2), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(r3), DuiLayout::Hint().Fixed(110));
        AddVariantRow(page.get(), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Skinned PushButton (9-grid bitmap)"),
               _T("DuiButton::SetBgBitmap(normal/hover/pressed/disabled) + SetBgInsets(3,3,3,3). ")
               _T("Hover deepens center, press deepens further; corners stay crisp at any size."));
    {
        // Synthesize 12x12 32bpp DIBSection bitmaps with a 3px solid outer
        // ring and a solid center fill. The 9-grid insets (3,3,3,3) keep
        // those corner pixels at 1:1 scale and stretch only the center,
        // so the rendered button shows a clean 3px border at any size.
        //
        // Bitmaps live in static storage so they outlive the buttons. They
        // leak on process exit, which is fine for a demo harness.
        auto makeSkin = [](BYTE br, BYTE bg, BYTE bb,    // border RGB
                           BYTE cr, BYTE cg, BYTE cb)    // center RGB
        {
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth    = 12;
            bi.bmiHeader.biHeight   = -12;          // top-down
            bi.bmiHeader.biPlanes   = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS,
                                             &bits, nullptr, 0);
            if (!hbm)
            {
                return (HBITMAP)nullptr;
            }
            BYTE* p = (BYTE*)bits;
            for (int y = 0; y < 12; ++y)
                for (int x = 0; x < 12; ++x)
                {
                    BYTE* px = p + (y * 12 + x) * 4;
                    bool ring = (x < 3 || x >= 9 || y < 3 || y >= 9);
                    px[0] = ring ? bb : cb;
                    px[1] = ring ? bg : cg;
                    px[2] = ring ? br : cr;
                    px[3] = 255;
                }
            return hbm;
        };

        struct Theme { HBITMAP n, h, p, d; };
        static Theme blue = {
            makeSkin( 30,  74, 153,   45, 108, 223),   // normal:  border #1E4A99 / fill #2D6CDF
            makeSkin( 30,  74, 153,   37,  89, 184),   // hover:   border #1E4A99 / fill #2559B8
            makeSkin( 20,  56, 120,   30,  74, 153),   // pressed: darker still
            makeSkin(150, 150, 150,  200, 200, 200)    // disabled: gray
        };
        static Theme teal = {
            makeSkin( 14,  90,  90,   30, 160, 160),
            makeSkin( 14,  90,  90,   24, 140, 140),
            makeSkin(  8,  60,  60,   18, 110, 110),
            makeSkin(150, 150, 150,  205, 215, 215)
        };
        static Theme red = {
            makeSkin(140,  40,  40,  210,  60,  60),
            makeSkin(140,  40,  40,  185,  50,  50),
            makeSkin(110,  30,  30,  150,  35,  35),
            makeSkin(150, 150, 150,  220, 200, 200)
        };

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);

        auto bBlue = std::unique_ptr<DuiButton>(new DuiButton());
        bBlue->SetText(_T("Blue"));
        bBlue->SetBgBitmap(blue.n, blue.h, blue.p, blue.d);
        bBlue->SetBgInsets(3, 3, 3, 3);

        auto bTeal = std::unique_ptr<DuiButton>(new DuiButton());
        bTeal->SetText(_T("Teal"));
        bTeal->SetBgBitmap(teal.n, teal.h, teal.p, teal.d);
        bTeal->SetBgInsets(3, 3, 3, 3);

        auto bRed = std::unique_ptr<DuiButton>(new DuiButton());
        bRed->SetText(_T("Red"));
        bRed->SetBgBitmap(red.n, red.h, red.p, red.d);
        bRed->SetBgInsets(3, 3, 3, 3);

        auto bDis = std::unique_ptr<DuiButton>(new DuiButton());
        bDis->SetText(_T("Disabled"));
        bDis->SetEnabled(false);
        bDis->SetBgBitmap(blue.n, blue.h, blue.p, blue.d);
        bDis->SetBgInsets(3, 3, 3, 3);

        row->AddChild(std::move(bBlue), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(bTeal), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(bRed),  DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(bDis),  DuiLayout::Hint().Fixed(120));
        AddVariantRow(page.get(), std::move(row));
    }
    return page;
}

// ===== Avatar =========================================================

namespace {

// Synthesize a 32x32 32bpp DIBSection with a vertical 2-color gradient
// so the rendered avatar shows a visible photo-like fill (not a flat
// disc). Returned HBITMAP is owned by a static cache and lives until
// process exit.
HBITMAP MakeAvatarSourceBitmap(BYTE r0, BYTE g0, BYTE b0,
                               BYTE r1, BYTE g1, BYTE b1)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth    = 32;
    bi.bmiHeader.biHeight   = -32;
    bi.bmiHeader.biPlanes   = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!h)
    {
        return nullptr;
    }
    BYTE* p = (BYTE*)bits;
    for (int y = 0; y < 32; ++y)
    {
        // Lerp t in 0..1 from top to bottom.
        float t = y / 31.0f;
        BYTE r = (BYTE)(r0 + (r1 - r0) * t);
        BYTE g = (BYTE)(g0 + (g1 - g0) * t);
        BYTE b = (BYTE)(b0 + (b1 - b0) * t);
        for (int x = 0; x < 32; ++x)
        {
            BYTE* px = p + (y * 32 + x) * 4;
            px[0] = b; px[1] = g; px[2] = r; px[3] = 255;
        }
    }
    return h;
}

} // anonymous

std::unique_ptr<DuiControl> Build_Avatar()
{
    auto page = NewPage();

    // Static bitmap cache so the avatars in this page share sources
    // and the bitmaps outlive the controls (rebuilt on every tab switch).
    // Intentionally leaks at process exit; matches the Skinned-Button
    // demo above.
    static HBITMAP s_blue   = MakeAvatarSourceBitmap( 80, 130, 220,  30,  60, 130);
    static HBITMAP s_purple = MakeAvatarSourceBitmap(170,  90, 200,  90,  30, 130);
    static HBITMAP s_orange = MakeAvatarSourceBitmap(255, 170,  60, 200,  80,  20);

    AddSection(page.get(),
               _T("Initials fallback"),
               _T("No bitmap. Picks 1-2 uppercase initials from the name; otherwise just a colored disc."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(16);
        struct Demo { LPCTSTR name; COLORREF bg; };
        Demo demos[] = {
            { _T("Alice Smith"),       RGB(45, 108, 223) },
            { _T("bob"),               RGB( 50, 160, 110) },
            { _T("零一"),      RGB(220,  60,  60) },     // 零一
            { _T(""),                  RGB(120, 120, 120) },     // empty -> just disc
        };
        for (auto& d : demos)
        {
            auto a = std::unique_ptr<DuiAvatar>(new DuiAvatar());
            a->SetName(d.name);
            a->SetFallbackBgColor(d.bg);
            row->AddChild(std::move(a), DuiLayout::Hint().Fixed(56));
        }
        AddVariantRow(page.get(), std::move(row), 56);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Bitmap source (circle)"),
               _T("HBITMAP supplied by caller; clipped to a circle, stretched to fit any size."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(16);
        for (int i = 0; i < 3; ++i)
        {
            auto a = std::unique_ptr<DuiAvatar>(new DuiAvatar());
            a->SetBitmap(i == 0 ? s_blue : i == 1 ? s_purple : s_orange);
            row->AddChild(std::move(a), DuiLayout::Hint().Fixed(64));
        }
        AddVariantRow(page.get(), std::move(row), 64);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Bitmap source (rounded rect)"),
               _T("ShapeRoundRect with various corner radii. Radius auto-clamps to half the shorter side."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(16);
        int radii[] = { 4, 12, 24, 999 /* clamps */ };
        for (int i = 0; i < 4; ++i)
        {
            auto a = std::unique_ptr<DuiAvatar>(new DuiAvatar());
            a->SetBitmap(s_purple);
            a->SetShape(DuiAvatar::ShapeRoundRect);
            a->SetCornerRadius(radii[i]);
            row->AddChild(std::move(a), DuiLayout::Hint().Fixed(64));
        }
        AddVariantRow(page.get(), std::move(row), 64);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Status dot (online/away/busy/offline/none)"),
               _T("Brand-colored dot at bottom-right with white outer ring for separation on light photos."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(16);
        DuiAvatar::Status sts[] = {
            DuiAvatar::StatusOnline,
            DuiAvatar::StatusAway,
            DuiAvatar::StatusBusy,
            DuiAvatar::StatusOffline,
            DuiAvatar::StatusNone,
        };
        for (auto s : sts)
        {
            auto a = std::unique_ptr<DuiAvatar>(new DuiAvatar());
            a->SetBitmap(s_blue);
            a->SetStatus(s);
            row->AddChild(std::move(a), DuiLayout::Hint().Fixed(64));
        }
        AddVariantRow(page.get(), std::move(row), 64);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Sizes"),
               _T("Same source bitmap rendered at 24 / 40 / 64 / 96 px. Status dot scales with avatar size."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(16);
        int sizes[] = { 24, 40, 64, 96 };
        for (auto sz : sizes)
        {
            auto a = std::unique_ptr<DuiAvatar>(new DuiAvatar());
            a->SetBitmap(s_orange);
            a->SetStatus(DuiAvatar::StatusOnline);
            row->AddChild(std::move(a), DuiLayout::Hint().Fixed(sz));
        }
        AddVariantRow(page.get(), std::move(row), 96);
    }
    return page;
}

// ===== Label ==========================================================

std::unique_ptr<DuiControl> Build_Label()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Plain text"),
               _T("ModeText. Different colors and alignments via SetTextColor / SetTextAlign."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto a = std::unique_ptr<DuiLabel>(new DuiLabel()); a->SetText(_T("Default black"));
        auto b = std::unique_ptr<DuiLabel>(new DuiLabel()); b->SetText(_T("Brand blue"));
        b->SetTextColor(RGB(45, 108, 223));
        auto c = std::unique_ptr<DuiLabel>(new DuiLabel()); c->SetText(_T("Right aligned"));
        c->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        row->AddChild(std::move(a), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(b), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(c), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 24);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Hyperlink (auto-navigate)"),
               _T("ModeLink + SetAutoNavigate(true). Click opens URL via ShellExecute."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto link = std::unique_ptr<DuiLabel>(new DuiLabel());
        link->SetMode(DuiLabel::ModeLink);
        link->SetText(_T("Open example.com"));
        link->SetUrl(_T("https://example.com"));
        link->SetAutoNavigate(true);
        row->AddChild(std::move(link), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 24);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Hyperlink (no nav)"),
               _T("ModeLink without auto-navigate. Click bubbles DUIN_CLICK; parent decides."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto link = std::unique_ptr<DuiLabel>(new DuiLabel());
        link->SetMode(DuiLabel::ModeLink);
        link->SetText(_T("Register account"));
        link->SetAutoNavigate(false);
        row->AddChild(std::move(link), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 24);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Visited state"),
               _T("After click the link adopts visited color. SetVisited(bool) toggles programmatically."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto link = std::unique_ptr<DuiLabel>(new DuiLabel());
        link->SetMode(DuiLabel::ModeLink);
        link->SetText(_T("This one starts visited"));
        link->SetVisited(true);
        row->AddChild(std::move(link), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 24);
    }
    return page;
}

// ===== Edit ===========================================================

std::unique_ptr<DuiControl> Build_Edit()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Single line + placeholder"),
               _T("HWND-hosted EDIT. Microsoft YaHei 9pt; I-beam cursor. Placeholder shown only when empty + unfocused."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e1 = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e1->SetPlaceholder(_T("type your username..."));
        auto e2 = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e2->SetText(_T("Pre-filled value"));
        row->AddChild(std::move(e1), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(e2), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Password"),
               _T("SetPassword(true) before HWND creation. Bullets mask input."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e->SetPassword(true);
        e->SetPlaceholder(_T("password"));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Password + eye toggle"),
               _T("SetShowEyeToggle(true) adds an eye button on the right side. Click to reveal / hide plaintext."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e->SetPassword(true);
        e->SetShowEyeToggle(true);
        e->SetPlaceholder(_T("type something to hide..."));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Read-only"),
               _T("SetReadOnly(true). User cannot edit; click + selection still works."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e->SetText(_T("This text is read-only"));
        e->SetReadOnly(true);
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Multi-line"),
               _T("SetMultiLine(true) before create. ES_AUTOVSCROLL + ES_WANTRETURN. IME works."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e->SetMultiLine(true);
        e->SetPlaceholder(_T("notes (multi-line, IME, drag to select)"));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 80);
    }
    AddGap(page.get(), kSectionGap);

    // ─── Inline icons ────────────────────────────────────────────────
    AddSection(page.get(),
               _T("Inline icons (left / right gutter)"),
               _T("SetIcon(LeftIcon|RightIcon, gutterW, painter). ")
               _T("Default: gutter=0, behavior identical to legacy. ")
               _T("Painter is std::function<void(HDC, RECT)> -- caller draws icon ")
               _T("with any GDI / GDI+ API. EDIT auto insets to leave room. ")
               _T("Click on a gutter with SetIconClickable(slot,true) fires ")
               _T("DUIEN_LEFT_ICON_CLICK / DUIEN_RIGHT_ICON_CLICK to parent."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);

        // (1) Left icon: magnifier (decorative, not clickable)
        auto eL = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        eL->SetPlaceholder(_T("Search music, podcasts..."));
        eL->SetIcon(DuiEditHost::LeftIcon, 28,
            [](HDC hdc, const RECT& rc) {
                int gx = (rc.left + rc.right) / 2;
                int gy = (rc.top + rc.bottom) / 2;
                const COLORREF c = RGB(140, 140, 140);
                const int r = 6;
                RECT circle = { gx - r, gy - r - 1, gx + r, gy + r - 1 };
                DuiAA::FillEllipse(hdc, circle, CLR_INVALID, c, 1.5f);
                DuiAA::DrawLine(hdc, gx + 3, gy + 3, gx + 7, gy + 7,
                                c, 1.5f);
            });
        row->AddChild(std::move(eL), DuiLayout::Hint().Weight(1));

        // (2) Right icon: clear "x" (clickable)
        auto eR = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        eR->SetText(_T("query text"));
        eR->SetIconGlyph(DuiEditHost::RightIcon, 24,
                         _T("✕"), RGB(140, 140, 140));
        eR->SetIconClickable(DuiEditHost::RightIcon, true);
        row->AddChild(std::move(eR), DuiLayout::Hint().Weight(1));

        // (3) Both: left @ + right ✕
        auto eB = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        eB->SetPlaceholder(_T("email"));
        eB->SetIconGlyph(DuiEditHost::LeftIcon,  24,
                         _T("@"), RGB(140, 140, 140));
        eB->SetIconGlyph(DuiEditHost::RightIcon, 24,
                         _T("✕"), RGB(140, 140, 140));
        eB->SetIconClickable(DuiEditHost::RightIcon, true);
        row->AddChild(std::move(eB), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Pill style (SetBgColor + SetShowBorder(false) + left icon)"),
               _T("Combo: SetBgColor(gray) + SetShowBorder(false) + left magnifier. ")
               _T("Outer container would paint a rounded pill bg of the same gray "
                  "(not shown here -- the gallery row uses the page bg)."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e->SetPlaceholder(_T("Search anything..."));
        e->SetBgColor(RGB(0xF3, 0xF3, 0xF4));
        e->SetShowBorder(false);
        e->SetIcon(DuiEditHost::LeftIcon, 32,
            [](HDC hdc, const RECT& rc) {
                int gx = (rc.left + rc.right) / 2;
                int gy = (rc.top + rc.bottom) / 2;
                const COLORREF c = RGB(140, 140, 140);
                const int r = 6;
                RECT circle = { gx - r, gy - r - 1, gx + r, gy + r - 1 };
                DuiAA::FillEllipse(hdc, circle, CLR_INVALID, c, 1.5f);
                DuiAA::DrawLine(hdc, gx + 3, gy + 3, gx + 7, gy + 7,
                                c, 1.5f);
            });
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 32);
    }
    return page;
}

// ===== RichEdit =======================================================

namespace {

// Local DuiButton subclass that fires a std::function on each click.
// Used by the RichEdit demo to wire Insert* actions without bouncing
// through the gallery frame's WM_DUI_NOTIFY router.
class LambdaButton : public DuiButton
{
public:
    void SetOnClick(std::function<void()> fn) { m_fn = std::move(fn); }
    bool OnLButtonUp(POINT pt, UINT mk) override
    {
        bool consumed = DuiButton::OnLButtonUp(pt, mk);
        if (m_bEnabled && ::PtInRect(&m_rcItem, pt) && m_fn) m_fn();
        return consumed;
    }
private:
    std::function<void()> m_fn;
};

// Synthesize a colorful 240x180 HBITMAP for the image-insert demo so we
// don't depend on any image file on disk.
static HBITMAP MakeDemoBitmap()
{
    const int W = 240, H = 180;
    HDC hdcScreen = ::GetDC(nullptr);
    HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = ::CreateCompatibleBitmap(hdcScreen, W, H);
    HBITMAP old = (HBITMAP)::SelectObject(hdcMem, hbm);
    // Brand-blue gradient via vertical strips.
    for (int y = 0; y < H; ++y)
    {
        int r = 45  + (y * (90  - 45 )) / H;
        int g = 108 + (y * (160 - 108)) / H;
        int b = 223 + (y * (255 - 223)) / H;
        HBRUSH hbr = ::CreateSolidBrush(RGB(r, g, b));
        RECT rcLine = { 0, y, W, y + 1 };
        ::FillRect(hdcMem, &rcLine, hbr);
        ::DeleteObject(hbr);
    }
    // Centered label so the user can recognize it after pasting.
    ::SetBkMode(hdcMem, TRANSPARENT);
    ::SetTextColor(hdcMem, RGB(255, 255, 255));
    HFONT hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldF = (HFONT)::SelectObject(hdcMem, hFont);
    RECT rcText = { 0, 0, W, H };
    ::DrawText(hdcMem, _T("DUI demo image"), -1, &rcText,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ::SelectObject(hdcMem, oldF);
    ::SelectObject(hdcMem, old);
    ::DeleteDC(hdcMem);
    ::ReleaseDC(nullptr, hdcScreen);
    return hbm;
}

} // anonymous

std::unique_ptr<DuiControl> Build_RichEdit()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Multi-line + word wrap"),
               _T("HWND-hosted RICHEDIT50W (msftedit.dll). Default Microsoft YaHei 9pt."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
        e->SetPlaceholder(_T("type a paragraph - it will wrap to fit..."));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 120);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Pre-filled with default text color"),
               _T("SetTextColor(RGB) drives the default CHARFORMAT2 fg. Affects newly typed text."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
        e->SetTextColor(RGB(45, 108, 223));
        e->SetText(_T("Brand-blue default text. Append more characters to confirm the color sticks."));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Auto URL detect"),
               _T("SetAutoUrlDetect(true). RichEdit auto-styles URLs blue+underlined and fires EN_LINK on click. The host forwards as DUIN_RE_LINKCLICK."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
        e->SetAutoUrlDetect(true);
        e->SetText(_T("Visit https://example.com or http://github.com for details."));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 80);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Read-only"),
               _T("SetReadOnly(true). User can select + copy + scroll, but no input. Suitable for the chat-history pane."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
        e->SetText(_T("This pane is read-only. Try to select + copy a phrase: works. Try to type: nothing."));
        e->SetReadOnly(true);
        e->SetBackgroundColor(RGB(248, 248, 250));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Single line"),
               _T("SetMultiLine(false). Caret cannot wrap; ES_AUTOHSCROLL kicks in for overflow."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
        e->SetMultiLine(false);
        e->SetWordWrap(false);
        e->SetPlaceholder(_T("rich single line"));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Rich content insert (image / quote / file)"),
               _T("Click a button to insert at the caret. Quote = U+2503 prefix + faded gray italic. File card = paperclip + size. Image = synthesized 240x180 demo bitmap pasted via clipboard CF_BITMAP round-trip."));
    {
        // The shared edit + the 3 action buttons go in a vertical mini-stack.
        auto stack = std::unique_ptr<DuiVBox>(new DuiVBox()); stack->SetGap(8);

        auto edit = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
        edit->SetPlaceholder(_T("Place caret here, then click a button below."));
        DuiRichEditHost* editPtr = edit.get();
        stack->AddChild(std::move(edit), DuiLayout::Hint().Weight(1));

        auto btnRow = std::unique_ptr<DuiHBox>(new DuiHBox()); btnRow->SetGap(10);

        auto bImg = std::unique_ptr<LambdaButton>(new LambdaButton());
        bImg->SetText(_T("Insert image"));
        bImg->SetOnClick([editPtr]()
        {
            HBITMAP hbm = MakeDemoBitmap();
            editPtr->InsertImageFromBitmap(hbm, 240, 180);
            ::DeleteObject(hbm);
        });
        btnRow->AddChild(std::move(bImg), DuiLayout::Hint().Fixed(140));

        auto bQuote = std::unique_ptr<LambdaButton>(new LambdaButton());
        bQuote->SetText(_T("Insert quote"));
        bQuote->SetOnClick([editPtr]()
        {
            editPtr->InsertQuoteBlock(
                _T("Alice"),
                _T("项目截止周五前完成,\n剩两个测试用例待补."));
        });
        btnRow->AddChild(std::move(bQuote), DuiLayout::Hint().Fixed(140));

        auto bFile = std::unique_ptr<LambdaButton>(new LambdaButton());
        bFile->SetText(_T("Insert file card"));
        bFile->SetOnClick([editPtr]()
        {
            editPtr->InsertFileCard(_T("design_v3_final.zip"), 24ULL * 1024 * 1024 + 567 * 1024);
        });
        btnRow->AddChild(std::move(bFile), DuiLayout::Hint().Fixed(160));

        // Filler so buttons left-align.
        btnRow->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                         DuiLayout::Hint().Weight(1));

        stack->AddChild(std::move(btnRow), DuiLayout::Hint().Fixed(32));

        page->AddChild(std::move(stack), DuiLayout::Hint().Fixed(220));
    }
    return page;
}

// ===== ComboBox =======================================================

std::unique_ptr<DuiControl> Build_ComboBox()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Read-only (CBS_DROPDOWNLIST)"),
               _T("Owner-drawn body shows selected item; click anywhere opens popup."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto cb = std::unique_ptr<DuiComboBox>(new DuiComboBox());
        cb->AddString(_T("Online"));
        cb->AddString(_T("Away"));
        cb->AddString(_T("Busy"));
        cb->AddString(_T("Invisible"));
        cb->AddString(_T("Offline"));
        cb->SetCurSel(0, false);
        row->AddChild(std::move(cb), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Editable (CBS_DROPDOWN)"),
               _T("SetEditable(true). HWND-hosted EDIT for text + arrow zone for popup. IME works."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto cb = std::unique_ptr<DuiComboBox>(new DuiComboBox());
        cb->SetEditable(true);
        cb->AddString(_T("Apple"));
        cb->AddString(_T("Banana"));
        cb->AddString(_T("Cherry"));
        cb->AddString(_T("Durian"));
        cb->SetText(_T("Apple"));
        row->AddChild(std::move(cb), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Long list (popup scrolls)"),
               _T("More items than maxVisible (default 8). Popup grows up to that cap and scrolls."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto cb = std::unique_ptr<DuiComboBox>(new DuiComboBox());
        for (int i = 1; i <= 25; ++i)
        {
            CString s; s.Format(_T("Item %02d"), i);
            cb->AddString(s);
        }
        cb->SetCurSel(0, false);
        row->AddChild(std::move(cb), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    return page;
}

// ===== Slider =========================================================

std::unique_ptr<DuiControl> Build_Slider()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("0..100, lineSize=1"),
               _T("Round thumb on a 4px track. Drag, click on track, wheel, arrow keys all work."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto s = std::unique_ptr<DuiSlider>(new DuiSlider());
        s->SetRange(0, 100); s->SetPos(35, false);
        row->AddChild(std::move(s), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("0..1000, lineSize=50 (large step)"),
               _T("SetLineSize(50). Arrow keys and track-click move in 50-unit chunks."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto s = std::unique_ptr<DuiSlider>(new DuiSlider());
        s->SetRange(0, 1000); s->SetLineSize(50); s->SetPos(500, false);
        row->AddChild(std::move(s), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Disabled"),
               _T("SetEnabled(false). Greyed track, ignores input."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto s = std::unique_ptr<DuiSlider>(new DuiSlider());
        s->SetRange(0, 100); s->SetPos(70, false);
        s->SetEnabled(false);
        row->AddChild(std::move(s), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    return page;
}

// ===== Switch =========================================================

std::unique_ptr<DuiControl> Build_Switch()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("4-state strip (force-state)"),
               _T("Off / On / Disabled-off / Disabled-on rendered side-by-side ")
               _T("for screenshot. Click any of the interactive ones below to toggle live."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(20);
        auto sOff = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        auto sOn  = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        sOn->SetChecked(true, /*animated=*/false, /*notify=*/false);
        auto sDisOff = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        sDisOff->SetEnabled(false);
        auto sDisOn = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        sDisOn->SetChecked(true, /*animated=*/false, /*notify=*/false);
        sDisOn->SetEnabled(false);
        row->AddChild(std::move(sOff),    DuiLayout::Hint().Fixed(46));
        row->AddChild(std::move(sOn),     DuiLayout::Hint().Fixed(46));
        row->AddChild(std::move(sDisOff), DuiLayout::Hint().Fixed(46));
        row->AddChild(std::move(sDisOn),  DuiLayout::Hint().Fixed(46));
        AddVariantRowCapture(page.get(),
                             _T("switch-states"),
                             std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Interactive — animated default"),
               _T("Click toggles. Watch the 150ms ease-out slide + color blend."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(20);
        auto a = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        auto b = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        b->SetChecked(true, false, false);
        row->AddChild(std::move(a), DuiLayout::Hint().Fixed(46));
        row->AddChild(std::move(b), DuiLayout::Hint().Fixed(46));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Interactive — SetAnimated(false)"),
               _T("Same control, but click snaps instantly with no animation."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(20);
        auto a = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        a->SetAnimated(false);
        row->AddChild(std::move(a), DuiLayout::Hint().Fixed(46));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Custom colors"),
               _T("SetOnColor / SetOffColor / SetKnobColor swap palette."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(20);
        auto orange = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        orange->SetOnColor(RGB(255, 140, 0));
        orange->SetChecked(true, false, false);
        auto purple = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        purple->SetOnColor(RGB(155, 89, 182));
        purple->SetChecked(true, false, false);
        auto darkOff = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        darkOff->SetOffColor(RGB(80, 80, 80));
        darkOff->SetKnobColor(RGB(220, 220, 220));
        row->AddChild(std::move(orange),  DuiLayout::Hint().Fixed(46));
        row->AddChild(std::move(purple),  DuiLayout::Hint().Fixed(46));
        row->AddChild(std::move(darkOff), DuiLayout::Hint().Fixed(46));
        AddVariantRow(page.get(), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Larger size (100×32)"),
               _T("SetRect drives geometry; pill radius = h/2 and travel widens."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(20);
        auto big = std::unique_ptr<DuiSwitch>(new DuiSwitch());
        big->SetChecked(true, false, false);
        row->AddChild(std::move(big), DuiLayout::Hint().Fixed(100));
        AddVariantRow(page.get(), std::move(row), 32);
    }
    return page;
}

// ===== ProgressBar ====================================================

std::unique_ptr<DuiControl> Build_ProgressBar()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Default percent overlay"),
               _T("Brand-blue fill grows left-to-right. NN%% drawn centered when no custom text."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        for (int pct : { 0, 25, 60, 100 })
        {
            auto p = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
            p->SetRange(0, 100); p->SetPos(pct, false);
            row->AddChild(std::move(p), DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(row), 22);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Custom text"),
               _T("SetText(\"Uploading...\") replaces the percentage overlay."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto p = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
        p->SetRange(0, 100); p->SetPos(45, false);
        p->SetText(_T("Uploading 45 / 100 MB"));
        row->AddChild(std::move(p), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 22);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Custom colors"),
               _T("SetFillColor + SetBgColor swap palette - here a green / orange / red set."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        struct Spec { COLORREF fill; int pct; };
        Spec specs[3] = {
            { RGB( 60, 170,  90), 80 },
            { RGB(230, 150,  40), 50 },
            { RGB(220,  80,  80), 20 },
        };
        for (auto& s : specs)
        {
            auto p = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
            p->SetRange(0, 100); p->SetPos(s.pct, false);
            p->SetFillColor(s.fill);
            row->AddChild(std::move(p), DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(row), 22);
    }
    return page;
}

// ===== Tab ============================================================

std::unique_ptr<DuiControl> Build_Tab()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Plain tabs"),
               _T("Owner-drawn tab strip. Selected tab has white fill; others share the strip color."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto t = std::unique_ptr<DuiTab>(new DuiTab());
        t->AddTab(_T("Friends"));
        t->AddTab(_T("Groups"));
        t->AddTab(_T("Recent"));
        t->SetCurSel(0, false);
        row->AddChild(std::move(t), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 32);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Closeable + dropdown"),
               _T("AppendTab with closeable=true paints an X; dropdown=true paints a triangle. Both fire custom notify codes (DUITN_CLOSE / DUITN_DROPDOWN)."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto t = std::unique_ptr<DuiTab>(new DuiTab());
        t->AddTab(_T("Settings"), false, true);    // dropdown
        t->AddTab(_T("Chat: Alice"), true);        // closeable
        t->AddTab(_T("Chat: Bob"),   true);
        t->SetCurSel(0, false);
        row->AddChild(std::move(t), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 32);
    }
    return page;
}

// ===== ListBox ========================================================

std::unique_ptr<DuiControl> Build_ListBox()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("10 items, single select"),
               _T("Click selects, hover highlights. Keys: Up/Down/Home/End/PgUp/PgDn auto-scroll."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto lb = std::unique_ptr<DuiListBox>(new DuiListBox());
        LPCTSTR fruits[] = {
            _T("Apple"), _T("Banana"), _T("Cherry"), _T("Durian"), _T("Eggplant"),
            _T("Fig"),   _T("Grape"),  _T("Hazel"),  _T("Iris"),   _T("Jujube") };
        for (auto* s : fruits)
        {
            lb->AddItem(s);
        }
        lb->SetCurSel(0, false);
        row->AddChild(std::move(lb), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 220);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("50 items + scrollbar"),
               _T("Embedded DuiScrollBar appears automatically when contentH > viewH."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto lb = std::unique_ptr<DuiListBox>(new DuiListBox());
        for (int i = 1; i <= 50; ++i)
        {
            CString s; s.Format(_T("Row #%02d"), i);
            lb->AddItem(s);
        }
        lb->SetCurSel(0, false);
        row->AddChild(std::move(lb), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 220);
    }
    return page;
}

// ===== VirtualList ====================================================

std::unique_ptr<DuiControl> Build_VirtualList()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("10000 rows (paint callback)"),
               _T("DuiVirtualList. Only visible rows are painted via the callback - constant cost regardless of count."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto vl = std::unique_ptr<DuiVirtualList>(new DuiVirtualList());
        vl->SetRowCount(10000);
        vl->SetRowHeight(28);
        vl->SetPaintRowCallback(
            [](void*, HDC hdc, int idx, const RECT& rc, bool sel, bool hov)
            {
                COLORREF bg = sel ? RGB(180, 210, 245)
                                  : (hov ? RGB(232, 240, 252)
                                         : ((idx & 1) ? RGB(248, 248, 252) : RGB(255, 255, 255)));
                HBRUSH hbr = ::CreateSolidBrush(bg);
                ::FillRect(hdc, &rc, hbr);
                ::DeleteObject(hbr);
                int oldBk = ::SetBkMode(hdc, TRANSPARENT);
                COLORREF oldClr = ::SetTextColor(hdc, sel ? RGB(10, 30, 90) : RGB(40, 40, 40));
                CString s; s.Format(_T("  Row #%05d   (virtual)"), idx);
                RECT rt = rc; rt.left += 6;
                ::DrawText(hdc, s, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                ::SetTextColor(hdc, oldClr);
                ::SetBkMode(hdc, oldBk);
            }, nullptr);
        row->AddChild(std::move(vl), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 360);
    }
    return page;
}

// ===== ScrollView =====================================================

namespace {

class TallColumn : public DuiControl
{
public:
    enum { BAND_COUNT = 30, BAND_H = 36 };
    int DesiredHeight() const { return BAND_COUNT * BAND_H; }

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }
        for (int i = 0; i < BAND_COUNT; ++i)
        {
            RECT band = { m_rcItem.left,
                          m_rcItem.top + i * BAND_H,
                          m_rcItem.right,
                          m_rcItem.top + (i + 1) * BAND_H };
            RECT inter;
            if (!::IntersectRect(&inter, &band, &rcDirty)) continue;
            int hue = (i * 23) % 256;
            COLORREF clr = RGB(180 + (hue % 60), 200 - (hue % 80), 220 - (hue % 90));
            HBRUSH hbr = ::CreateSolidBrush(clr);
            ::FillRect(hdc, &inter, hbr);
            ::DeleteObject(hbr);
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            CString label; label.Format(_T("  band %02d"), i);
            ::DrawText(hdc, label, -1, &band, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ::SetBkMode(hdc, oldBk);
        }
    }
};

} // anonymous

std::unique_ptr<DuiControl> Build_ScrollView()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Vertical scroll view"),
               _T("DuiScrollView clips a tall content control. Wheel + thumb drag + track click all work."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto sv = std::unique_ptr<DuiScrollView>(new DuiScrollView());
        auto col = std::unique_ptr<TallColumn>(new TallColumn());
        int h = col->DesiredHeight();
        sv->SetContent(std::move(col));
        sv->SetContentHeight(h);
        row->AddChild(std::move(sv), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 360);
    }
    return page;
}

// ===== Menu ===========================================================

namespace {

// Tiny button that opens a menu when clicked. Menu lives in a static so
// it survives the synchronous TrackPopup call.
class MenuButton : public DuiButton
{
public:
    typedef void (*BuildFn)(DuiMenu& menu);
    void SetBuilder(BuildFn fn) { m_fn = fn; }
    bool OnLButtonUp(POINT pt, UINT mkFlags) override
    {
        bool consumed = DuiButton::OnLButtonUp(pt, mkFlags);
        if (consumed && m_fn)
        {
            DuiMenu menu;
            m_fn(menu);
            POINT scr;
            scr.x = m_rcItem.left;
            scr.y = m_rcItem.bottom;
            if (GetHost() && GetHost()->m_hWnd)
                ::ClientToScreen(GetHost()->m_hWnd, &scr);
            menu.TrackPopup(scr.x, scr.y, GetHost() ? GetHost()->m_hWnd : NULL);
        }
        return consumed;
    }
private:
    BuildFn m_fn = nullptr;
};

static void BuildPlainMenu(DuiMenu& menu)
{
    menu.AppendItem    (101, _T("New"));
    menu.AppendItem    (102, _T("Open..."));
    menu.AppendItem    (103, _T("Save"));
    menu.AppendSeparator();
    menu.AppendItem    (104, _T("Exit"));
}

static void BuildFullMenu(DuiMenu& menu)
{
    menu.AppendChecked (201, _T("Stay on top"), true);
    menu.AppendChecked (202, _T("Mute"),        false);
    menu.AppendSeparator();
    static DuiMenu sub;
    if (sub.GetCount() == 0)
    {
        sub.AppendItem(301, _T("English"));
        sub.AppendItem(302, _T("Chinese"));
        sub.AppendItem(303, _T("Japanese"));
    }
    menu.AppendSubMenu (203, _T("Language"), &sub);
    menu.AppendDisabled(204, _T("Cannot click (disabled)"));
    menu.AppendSeparator();
    menu.AppendItem    (299, _T("Quit"));
}

} // anonymous

std::unique_ptr<DuiControl> Build_Menu()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Plain menu"),
               _T("Click button to TrackPopup a 4-item menu. White background, light-gray hover, drop shadow."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto b = std::unique_ptr<MenuButton>(new MenuButton());
        b->SetText(_T("Show plain menu"));
        b->SetBuilder(&BuildPlainMenu);
        row->AddChild(std::move(b), DuiLayout::Hint().Fixed(180));
        AddVariantRow(page.get(), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Checked + sub-menu + disabled"),
               _T("Demonstrates AppendChecked, AppendSubMenu (hover or click to open), and AppendDisabled."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto b = std::unique_ptr<MenuButton>(new MenuButton());
        b->SetText(_T("Show full menu"));
        b->SetBuilder(&BuildFullMenu);
        row->AddChild(std::move(b), DuiLayout::Hint().Fixed(180));
        AddVariantRow(page.get(), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Right-click context menu"),
               _T("Right-click anywhere on this page to pop a 5-item context menu. Page-level hook in MenuButton's parent."));
    return page;
}

// ===== MenuBar ========================================================
//
// Inline menu bar control. Three columns (File / Options / View) each tied
// to a static DuiMenu. The DuiMenu instances must outlive the bar — store
// them as file-static so they survive Build_MenuBar() being called every
// tab switch. Bar itself is rebuilt each time (cheap; only borrows menus).

#if BUI_FEATURE_MENUBAR

static DuiMenu s_mbFileMenu;
static DuiMenu s_mbOptionsMenu;
static DuiMenu s_mbViewMenu;
static bool    s_mbMenusInited = false;

static void EnsureMenuBarMenusInited()
{
    if (s_mbMenusInited)
    {
        return;
    }
    s_mbMenusInited = true;

    s_mbFileMenu.AppendItem    (701, _T("&Open..."));
    s_mbFileMenu.AppendItem    (702, _T("&Save"));
    s_mbFileMenu.AppendSeparator();
    s_mbFileMenu.AppendItem    (703, _T("E&xit"));

    s_mbOptionsMenu.AppendChecked(721, _T("Always on &top"),    false);
    s_mbOptionsMenu.AppendChecked(722, _T("&Hide on minimize"), true);
    s_mbOptionsMenu.AppendChecked(723, _T("Show on &demand"),   false);

    s_mbViewMenu.AppendItem    (741, _T("&Refresh\tF5"));
    s_mbViewMenu.AppendSeparator();
    s_mbViewMenu.AppendChecked (742, _T("Speed: high"),   false);
    s_mbViewMenu.AppendChecked (743, _T("Speed: normal"), true);
    s_mbViewMenu.AppendChecked (744, _T("Speed: low"),    false);
    s_mbViewMenu.AppendChecked (745, _T("Speed: paused"), false);
    s_mbViewMenu.AppendSeparator();
    s_mbViewMenu.AppendChecked (746, _T("&Group by type"), true);
    s_mbViewMenu.AppendItem    (747, _T("&Expand all"));
}

#endif // BUI_FEATURE_MENUBAR

std::unique_ptr<DuiControl> Build_MenuBar()
{
    auto page = NewPage();

#if BUI_FEATURE_MENUBAR
    EnsureMenuBarMenusInited();

    AddSection(page.get(),
               _T("DuiMenuBar — File / Options / View"),
               _T("Inline menu bar. Click a column to drop its menu, or Alt+letter to jump straight there. Once a dropdown is open, mouse over to a sibling column auto-switches (Win10 behaviour)."));
    {
        // 一个直接放在 page 里的 DuiMenuBar。bar lifetime = page lifetime
        // （每次 SwitchToPage 重建）；s_mbXxxMenu 是 file-static 长寿对象，
        // 满足"dropdown 必须保活到 TrackPopupEx 调用栈结束"的契约。
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto bar = std::unique_ptr<DuiMenuBar>(new DuiMenuBar());
        bar->SetCtrlId(700);
        bar->AppendItem(710, _T("&File"),    &s_mbFileMenu);
        bar->AppendItem(720, _T("&Options"), &s_mbOptionsMenu);
        bar->AppendItem(740, _T("&View"),    &s_mbViewMenu);
        row->AddChild(std::move(bar), DuiLayout::Hint().Fixed(180));
        AddVariantRow(page.get(), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Manual verification checklist"),
               _T("(1) hover File 一格变浅灰; (2) 点开 File，鼠标移到 Options/View 自动切换; (3) Alt+F/O/V 直跳; (4) Esc 关闭下拉; (5) DPI 125%/150% 文字 + 下划线仍清晰."));

#else
    AddSection(page.get(),
               _T("DuiMenuBar"),
               _T("BUI_FEATURE_MENUBAR disabled — control unavailable in this build."));
#endif

    return page;
}

// ===== ToolTip ========================================================

std::unique_ptr<DuiControl> Build_ToolTip()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Per-control tooltip"),
               _T("Hover ~500 ms to see a pale-yellow tooltip. Move to next control to see another. Click hides any."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto a = std::unique_ptr<DuiButton>(new DuiButton()); a->SetText(_T("Send"));
        auto b = std::unique_ptr<DuiButton>(new DuiButton()); b->SetText(_T("Open"));
        auto c = std::unique_ptr<DuiButton>(new DuiButton()); c->SetText(_T("Logout"));
        DuiToolTipMgr::Inst().Register(a.get(), _T("Send a chat message"));
        DuiToolTipMgr::Inst().Register(b.get(), _T("Open the file picker"));
        DuiToolTipMgr::Inst().Register(c.get(), _T("Log out and quit"));
        row->AddChild(std::move(a), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(b), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(c), DuiLayout::Hint().Fixed(120));
        AddVariantRow(page.get(), std::move(row));
    }
    return page;
}

// ===== Layout =========================================================

std::unique_ptr<DuiControl> Build_Layout()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("HBox: equal weights"),
               _T("Three children with weight=1 evenly split width."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        for (int i = 0; i < 3; ++i)
        {
            auto b = std::unique_ptr<DuiButton>(new DuiButton());
            CString s; s.Format(_T("w=1 (%d)"), i + 1);
            b->SetText(s);
            row->AddChild(std::move(b), DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("HBox: fixed + weighted"),
               _T("Left fixed=80, middle weight=1, right weight=2. Resize behavior: only middle/right grow."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        auto a = std::unique_ptr<DuiButton>(new DuiButton()); a->SetText(_T("fixed=80"));
        auto b = std::unique_ptr<DuiButton>(new DuiButton()); b->SetText(_T("w=1"));
        auto c = std::unique_ptr<DuiButton>(new DuiButton()); c->SetText(_T("w=2"));
        row->AddChild(std::move(a), DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(b), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(c), DuiLayout::Hint().Weight(2));
        AddVariantRow(page.get(), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Grid 2x3"),
               _T("SetGrid(2, 3) + SetGap(8). Children fill row-major (D, E, F / G, H, I)."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto grid = std::unique_ptr<DuiGrid>(new DuiGrid());
        grid->SetGrid(2, 3); grid->SetGap(8);
        LPCTSTR labels[6] = { _T("D"), _T("E"), _T("F"), _T("G"), _T("H"), _T("I") };
        for (int i = 0; i < 6; ++i)
        {
            auto b = std::unique_ptr<DuiButton>(new DuiButton());
            b->SetText(labels[i]);
            grid->AddChild(std::move(b));
        }
        row->AddChild(std::move(grid), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Padding + gap"),
               _T("Padding is the inner margin between the box and its children; gap is between siblings."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto inner = std::unique_ptr<DuiHBox>(new DuiHBox());
        inner->SetPadding(20); inner->SetGap(20);
        for (int i = 0; i < 3; ++i)
        {
            auto b = std::unique_ptr<DuiButton>(new DuiButton());
            CString s; s.Format(_T("inner %d"), i + 1);
            b->SetText(s);
            inner->AddChild(std::move(b), DuiLayout::Hint().Weight(1));
        }
        row->AddChild(std::move(inner), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 80);
    }
    return page;
}

// ===== Splitter =======================================================

namespace {

// Paint-only stub used as a Splitter pane. Fills its rect with a solid
// color and draws a centered label, so the user can see the panes
// resize while dragging the bar.
class ColorPane : public DuiControl
{
public:
    ColorPane(COLORREF c, LPCTSTR label)
        : m_color(c), m_label(label ? label : _T(""))
    { SetTabStop(false); }

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }
        RECT inter;
        if (!::IntersectRect(&inter, &m_rcItem, &rcDirty))
        {
            return;
        }
        HBRUSH hbr = ::CreateSolidBrush(m_color);
        ::FillRect(hdc, &inter, hbr);
        ::DeleteObject(hbr);

        if (!m_label.IsEmpty())
        {
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
            ::DrawText(hdc, m_label, -1, &m_rcItem,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
        }
    }

private:
    COLORREF m_color;
    CString  m_label;
};

} // anonymous

std::unique_ptr<DuiControl> Build_Splitter()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Vertical (side-by-side, drag the bar)"),
               _T("DuiSplitter::Vertical with two colored panes. Min sizes 60/60. Initial fraction 0.4."));
    {
        auto sp = std::unique_ptr<DuiSplitter>(new DuiSplitter());
        sp->SetOrientation(DuiSplitter::Vertical);
        sp->SetMinSizes(60, 60);
        sp->SetSplitFraction(0.4);
        sp->SetPane(0, std::unique_ptr<DuiControl>(new ColorPane(RGB(45, 108, 223), _T("pane 0"))));
        sp->SetPane(1, std::unique_ptr<DuiControl>(new ColorPane(RGB(60, 175,  80), _T("pane 1"))));
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(sp), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Horizontal (stacked, drag the bar)"),
               _T("DuiSplitter::Horizontal. Bar runs horizontally; panes stack top/bottom. Bar thickness 6."));
    {
        auto sp = std::unique_ptr<DuiSplitter>(new DuiSplitter());
        sp->SetOrientation(DuiSplitter::Horizontal);
        sp->SetBarThickness(6);
        sp->SetMinSizes(40, 40);
        sp->SetSplitFraction(0.5);
        sp->SetPane(0, std::unique_ptr<DuiControl>(new ColorPane(RGB(220, 90, 70),  _T("top"))));
        sp->SetPane(1, std::unique_ptr<DuiControl>(new ColorPane(RGB(255, 160, 50), _T("bottom"))));
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(sp), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 160);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Nested splitters (chat-like 3 panes)"),
               _T("Outer vertical (friend list | chat area). Right side is a horizontal splitter (history / input)."));
    {
        // Inner: chat history on top, input on bottom.
        auto inner = std::unique_ptr<DuiSplitter>(new DuiSplitter());
        inner->SetOrientation(DuiSplitter::Horizontal);
        inner->SetMinSizes(80, 30);
        inner->SetSplitFraction(0.7);
        inner->SetPane(0, std::unique_ptr<DuiControl>(new ColorPane(RGB(245, 245, 250), _T("chat history"))));
        inner->SetPane(1, std::unique_ptr<DuiControl>(new ColorPane(RGB(220, 230, 245), _T("input"))));

        // Outer: friend list on left, the inner splitter on right.
        auto outer = std::unique_ptr<DuiSplitter>(new DuiSplitter());
        outer->SetOrientation(DuiSplitter::Vertical);
        outer->SetMinSizes(80, 120);
        outer->SetSplitFraction(0.3);
        outer->SetPane(0, std::unique_ptr<DuiControl>(new ColorPane(RGB(40, 60, 90), _T("friend list"))));
        outer->SetPane(1, std::move(inner));

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(outer), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 220);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Min-size demo (try dragging far in either direction)"),
               _T("Min sizes 100/100, total ~280px. Bar can travel only across the small middle band."));
    {
        auto sp = std::unique_ptr<DuiSplitter>(new DuiSplitter());
        sp->SetOrientation(DuiSplitter::Vertical);
        sp->SetMinSizes(100, 100);
        sp->SetBarThickness(8);
        sp->SetSplitFraction(0.5);
        sp->SetPane(0, std::unique_ptr<DuiControl>(new ColorPane(RGB(120, 50, 180), _T("min 100"))));
        sp->SetPane(1, std::unique_ptr<DuiControl>(new ColorPane(RGB(180, 50, 120), _T("min 100"))));
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(sp), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 100);
    }
    return page;
}

// ===== TabPage ========================================================

namespace {

// Reused from Splitter section: paint-only colored pane with a label.
class ColorPaneTP : public DuiControl
{
public:
    ColorPaneTP(COLORREF c, LPCTSTR label)
        : m_color(c), m_label(label ? label : _T(""))
    { SetTabStop(false); }
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }
        RECT inter;
        if (!::IntersectRect(&inter, &m_rcItem, &rcDirty))
        {
            return;
        }
        HBRUSH hbr = ::CreateSolidBrush(m_color);
        ::FillRect(hdc, &inter, hbr);
        ::DeleteObject(hbr);
        if (!m_label.IsEmpty())
        {
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
            ::DrawText(hdc, m_label, -1, &m_rcItem,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
        }
    }
private:
    COLORREF m_color; CString m_label;
};

} // anonymous

std::unique_ptr<DuiControl> Build_TabPage()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("DuiTabPage (header strip + content area)"),
               _T("Click a tab to swap the visible page. Header height = 32 px."));
    {
        auto tp = std::unique_ptr<DuiTabPage>(new DuiTabPage());
        tp->AddPage(_T("Friends"),
                    std::unique_ptr<DuiControl>(new ColorPaneTP(RGB(45, 108, 223), _T("friends list"))));
        tp->AddPage(_T("Groups"),
                    std::unique_ptr<DuiControl>(new ColorPaneTP(RGB( 60, 175,  80), _T("group list"))));
        tp->AddPage(_T("Recent"),
                    std::unique_ptr<DuiControl>(new ColorPaneTP(RGB(220, 110,  60), _T("recent chats"))));
        tp->AddPage(_T("Settings"),
                    std::unique_ptr<DuiControl>(new ColorPaneTP(RGB(120,  90, 180), _T("settings"))));
        tp->SetCurSel(0, false);

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(tp), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 220);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Larger header"),
               _T("SetHeaderHeight(48). Same content; header just gets more vertical room."));
    {
        auto tp = std::unique_ptr<DuiTabPage>(new DuiTabPage());
        tp->SetHeaderHeight(48);
        tp->AddPage(_T("Tab A"),
                    std::unique_ptr<DuiControl>(new ColorPaneTP(RGB( 50, 160, 110), _T("page A content"))));
        tp->AddPage(_T("Tab B"),
                    std::unique_ptr<DuiControl>(new ColorPaneTP(RGB(180, 100, 200), _T("page B content"))));

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(tp), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 180);
    }
    return page;
}

// ===== TreeView =======================================================

namespace {

// Status palette matching DuiAvatar; embedded inline so this page does
// not pull DuiAvatar.h into Pages.cpp just for 4 constants.
static const COLORREF kTVOnline  = RGB( 60, 175,  80);
static const COLORREF kTVAway    = RGB(240, 175,  40);
static const COLORREF kTVBusy    = RGB(220,  60,  60);
static const COLORREF kTVOffline = RGB(150, 150, 150);

} // anonymous

std::unique_ptr<DuiControl> Build_TreeView()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("Friend list (click ▶ to expand, click row to select)"),
               _T("Embedded in a DuiScrollView so the tree scrolls when collapsed/expanded changes height. ")
               _T("Row 28 px, indent 18 px, status dot at right."));
    {
        auto tree = std::unique_ptr<DuiTreeView>(new DuiTreeView());
        tree->SetCtrlId(900);

        int gFriends = tree->AddRoot(_T("Friends"));
        int idAlice = tree->AddChild(gFriends, _T("Alice"));     tree->SetItemStatusColor(idAlice, kTVOnline);
        int idBob   = tree->AddChild(gFriends, _T("Bob"));       tree->SetItemStatusColor(idBob,   kTVAway);
        int idCarol = tree->AddChild(gFriends, _T("Carol"));     tree->SetItemStatusColor(idCarol, kTVBusy);
        int idDave  = tree->AddChild(gFriends, _T("Dave"));      tree->SetItemStatusColor(idDave,  kTVOffline);
        (void)idAlice;(void)idBob;(void)idCarol;(void)idDave;

        int gWork = tree->AddRoot(_T("Coworkers"));
        for (int i = 1; i <= 8; ++i)
        {
            CString s; s.Format(_T("Engineer #%d"), i);
            int id = tree->AddChild(gWork, s);
            tree->SetItemStatusColor(id, (i % 4 == 0) ? kTVOffline
                                       : (i % 3 == 0) ? kTVBusy
                                       : (i % 2 == 0) ? kTVAway
                                                       : kTVOnline);
        }

        int gNested = tree->AddRoot(_T("Projects"));
        int pAlpha  = tree->AddChild(gNested, _T("Alpha"));
        tree->AddChild(pAlpha, _T("frontend"));
        tree->AddChild(pAlpha, _T("backend"));
        tree->AddChild(pAlpha, _T("mobile"));
        int pBeta = tree->AddChild(gNested, _T("Beta"));
        tree->AddChild(pBeta, _T("design"));
        tree->AddChild(pBeta, _T("infra"));

        // Embed in scroll view so the row count can outgrow the visible
        // box without the gallery page becoming unmanageable.
        auto sv = std::unique_ptr<DuiScrollView>(new DuiScrollView());
        DuiTreeView* treeRaw = tree.get();
        sv->SetContent(std::move(tree));
        sv->SetContentHeight(treeRaw->GetContentHeight());
        // Force initial relayout once the tree is mounted; the scroll
        // view sets the tree's rect after this and OnPaint reads it.
        DuiScrollView* svRaw = sv.get();
        (void)svRaw;

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(sv), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 320);
    }

    // Multi-column section --------------------------------------------------
    AddSection(page.get(),
               _T("Multi-column mode (drag header / Ctrl+click to multi-select / dbl-click text to edit)"),
               _T("5 columns: Name (tree, frozen) / Done (CheckBox) / Progress / Size / Link. ")
               _T("Editable=on, frozen-cols=1, initial sort indicator on Name (asc)."));
    {
        auto tree = std::unique_ptr<DuiTreeView>(new DuiTreeView());
        tree->SetCtrlId(901);

        // Column layout.
        int colName  = tree->AddColumn(_T("Name"),     200, 80, DT_LEFT);
        int colDone  = tree->AddColumn(_T("Done"),      70, 50, DT_CENTER);
        int colProg  = tree->AddColumn(_T("Progress"), 160, 90, DT_LEFT);
        int colSize  = tree->AddColumn(_T("Size"),      90, 60, DT_RIGHT);
        int colLink  = tree->AddColumn(_T("Link"),     180, 80, DT_LEFT);
        (void)colName;

        tree->SetFrozenColumns(1);
        tree->SetEditable(true);
        tree->SetSortIndicator(colName, +1);

        struct Row
        {
            LPCTSTR name;
            bool    done;
            int     progress;
            LPCTSTR size;
            LPCTSTR link;
            LPCTSTR url;
        };
        const Row rows[] =
        {
            { _T("README.md"),         true,  100, _T("4.2 KB"),  _T("open"),     _T("https://example.com/readme") },
            { _T("design.pdf"),        false,  60, _T("1.1 MB"),  _T("preview"),  _T("https://example.com/design") },
            { _T("logo.png"),          true,  100, _T("87 KB"),   _T("download"), _T("https://example.com/logo")   },
            { _T("notes.txt"),         false,  20, _T("612 B"),   _T("edit"),     _T("https://example.com/notes")  },
            { _T("schema.sql"),        false,  85, _T("32 KB"),   _T("run"),      _T("https://example.com/sql")    },
        };

        int gProj = tree->AddRoot(_T("project-alpha"));
        for (auto& r : rows)
        {
            int id = tree->AddChild(gProj, r.name);
            tree->SetCellChecked(id, colDone, r.done);
            tree->SetCellValue  (id, colProg, r.progress);
            tree->SetCellText   (id, colSize, r.size);
            tree->SetCellLink   (id, colLink, r.link, r.url);
        }
        int gAnother = tree->AddRoot(_T("project-beta"));
        int idDoc = tree->AddChild(gAnother, _T("docs.md"));
        tree->SetCellChecked(idDoc, colDone, false);
        tree->SetCellValue  (idDoc, colProg, 45);
        tree->SetCellText   (idDoc, colSize, _T("8.7 KB"));
        tree->SetCellLink   (idDoc, colLink, _T("open"), _T("https://example.com/docs"));

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(tree), DuiLayout::Hint().Weight(1));
        AddVariantRowCapture(page.get(), _T("treeview-multicol"), std::move(row), 320);
    }

    return page;
}

// ===== PopupHost ======================================================

namespace {

// Tiny DuiButton subclass that fires a std::function<> on click. Lets us
// drive popup demo without weaving extra ctrlId routing into the gallery
// frame's WM_DUI_NOTIFY router. Press tracking shadows the base class's
// own m_pressed (which is private) so we can mirror its click semantics.
class FnButton : public DuiButton
{
public:
    std::function<void(FnButton*)> onClick;
    bool OnLButtonDown(POINT pt, UINT mk) override
    {
        m_localPressed = true;
        return DuiButton::OnLButtonDown(pt, mk);
    }
    bool OnLButtonUp(POINT pt, UINT mk) override
    {
        bool was = m_localPressed;
        m_localPressed = false;
        bool r = DuiButton::OnLButtonUp(pt, mk);
        if (was && IsEnabled() && ::PtInRect(&GetRect(), pt) && onClick)
            onClick(this);
        return r;
    }
private:
    bool m_localPressed = false;
};

// A simple DuiVBox factory that builds a small content tree for the popup.
static std::unique_ptr<DuiControl> MakePopupContent(LPCTSTR title,
                                                   COLORREF accent)
{
    auto box = std::unique_ptr<DuiVBox>(new DuiVBox());
    box->SetPadding(10);
    box->SetGap(6);

    auto h = std::unique_ptr<DuiLabel>(new DuiLabel());
    h->SetText(title);
    h->SetTextColor(accent);
    box->AddChild(std::move(h), DuiLayout::Hint().Fixed(22));

    auto desc = std::unique_ptr<DuiLabel>(new DuiLabel());
    desc->SetText(_T("Click outside or press Esc to dismiss."));
    desc->SetTextColor(RGB(80, 80, 80));
    box->AddChild(std::move(desc), DuiLayout::Hint().Fixed(20));

    for (int i = 1; i <= 3; ++i)
    {
        auto b = std::unique_ptr<DuiButton>(new DuiButton());
        CString s; s.Format(_T("Item %d"), i);
        b->SetText(s);
        box->AddChild(std::move(b), DuiLayout::Hint().Fixed(28));
    }
    return box;
}

// Static popup hosts so they outlive Build_PopupHost re-invocations
// (the gallery rebuilds the page on every tab switch). Each has its
// own preferred edge / content; SetContent runs once on first Show.
static DuiPopupHost s_popBelow;
static DuiPopupHost s_popAbove;
static DuiPopupHost s_popRight;
static bool         s_popInited = false;

static void EnsurePopupsInited()
{
    if (s_popInited)
    {
        return;
    }
    s_popInited = true;

    s_popBelow.SetSize(220, 180);
    s_popBelow.SetEdge(DuiPopupHost::EdgeBelow);
    s_popBelow.SetContent(MakePopupContent(
        _T("Below anchor"), RGB(45, 108, 223)));

    s_popAbove.SetSize(240, 180);
    s_popAbove.SetEdge(DuiPopupHost::EdgeAbove);
    s_popAbove.SetContent(MakePopupContent(
        _T("Above anchor"), RGB(60, 175, 80)));

    s_popRight.SetSize(220, 200);
    s_popRight.SetEdge(DuiPopupHost::EdgeRight);
    s_popRight.SetContent(MakePopupContent(
        _T("Right of anchor"), RGB(220, 90, 70)));
}

} // anonymous

std::unique_ptr<DuiControl> Build_PopupHost()
{
    EnsurePopupsInited();
    auto page = NewPage();

    AddSection(page.get(),
               _T("DuiPopupHost — top-level WS_POPUP that hosts a DUI tree"),
               _T("Clicking a button opens a borderless popup near it. ")
               _T("Click outside the popup or focus a different window to auto-dismiss. ")
               _T("Edge auto-flips when the preferred side would push the popup off the screen."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(12);

        auto btnBelow = std::unique_ptr<FnButton>(new FnButton());
        btnBelow->SetText(_T("Open below"));
        btnBelow->onClick = [](FnButton* b)
        {
            RECT screenRc;
            HWND hHost = ::GetAncestor(b->GetHost() ? b->GetHost()->m_hWnd : nullptr,
                                       GA_ROOT);
            (void)hHost;
            // Get button rect in host-client, then map to screen.
            RECT cr = b->GetRect();
            POINT tl = { cr.left, cr.top };
            POINT br = { cr.right, cr.bottom };
            ::ClientToScreen(b->GetHost()->m_hWnd, &tl);
            ::ClientToScreen(b->GetHost()->m_hWnd, &br);
            screenRc = { tl.x, tl.y, br.x, br.y };
            s_popBelow.Show(screenRc, b->GetHost()->m_hWnd);
        };

        auto btnAbove = std::unique_ptr<FnButton>(new FnButton());
        btnAbove->SetText(_T("Open above"));
        btnAbove->onClick = [](FnButton* b)
        {
            RECT cr = b->GetRect();
            POINT tl = { cr.left, cr.top };
            POINT br = { cr.right, cr.bottom };
            ::ClientToScreen(b->GetHost()->m_hWnd, &tl);
            ::ClientToScreen(b->GetHost()->m_hWnd, &br);
            RECT screenRc = { tl.x, tl.y, br.x, br.y };
            s_popAbove.Show(screenRc, b->GetHost()->m_hWnd);
        };

        auto btnRight = std::unique_ptr<FnButton>(new FnButton());
        btnRight->SetText(_T("Open right"));
        btnRight->onClick = [](FnButton* b)
        {
            RECT cr = b->GetRect();
            POINT tl = { cr.left, cr.top };
            POINT br = { cr.right, cr.bottom };
            ::ClientToScreen(b->GetHost()->m_hWnd, &tl);
            ::ClientToScreen(b->GetHost()->m_hWnd, &br);
            RECT screenRc = { tl.x, tl.y, br.x, br.y };
            s_popRight.Show(screenRc, b->GetHost()->m_hWnd);
        };

        row->AddChild(std::move(btnBelow), DuiLayout::Hint().Fixed(140));
        row->AddChild(std::move(btnAbove), DuiLayout::Hint().Fixed(140));
        row->AddChild(std::move(btnRight), DuiLayout::Hint().Fixed(140));
        AddVariantRow(page.get(), std::move(row));
    }
    return page;
}

// ===== Emoji ==========================================================

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace {

static DuiPopupHost  s_emojiPop;
static DuiEmojiPanel*s_emojiPanel  = nullptr;
static DuiLabel*     s_emojiResult = nullptr;
static bool          s_emojiInited = false;

// Hold the loaded HBITMAPs for the demo's lifetime. They're owned by
// us (we call DeleteObject in a leak-on-exit pattern matching the rest
// of the gallery's static caches).
static std::vector<HBITMAP> s_emojiHbms;

// Number of face*.png assets we'll try to load from Bin\Face\.
static const int kEmojiPngCount = 40;

// Resolve "<exeDir>\Face\faceN.png" without depending on CSkinManager.
static CString MakeFacePngPath(int n)
{
    TCHAR buf[MAX_PATH] = {};
    ::GetModuleFileName(NULL, buf, MAX_PATH);
    CString path = buf;
    int slash = path.ReverseFind(_T('\\'));
    if (slash >= 0)
    {
        path = path.Left(slash);
    }
    CString p;
    p.Format(_T("%s\\Face\\face%d.png"), (LPCTSTR)path, n);
    return p;
}

// Load a single PNG -> HBITMAP via GDI+. Returns nullptr if missing.
static HBITMAP LoadPngAsHbitmap(LPCTSTR path)
{
    Gdiplus::Bitmap bm(path);
    if (bm.GetLastStatus() != Gdiplus::Ok) return nullptr;
    HBITMAP h = nullptr;
    bm.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &h);
    return h;
}

static void EmojiPicked(void* /*ud*/, LPCTSTR seq, int idx)
{
    if (s_emojiResult)
    {
        CString s;
        s.Format(_T("Last picked: %s   (idx=%d)"), seq, idx);
        s_emojiResult->SetText(s);
    }
    s_emojiPop.Hide(DuiPopupHost::ReasonProgrammatic);
}

static void EnsureEmojiPopupInited()
{
    if (s_emojiInited)
    {
        return;
    }
    s_emojiInited = true;

    // Init GDI+ in case nobody else did yet.
    static ULONG_PTR s_token = 0;
    if (!s_token)
    {
        Gdiplus::GdiplusStartupInput in;
        Gdiplus::GdiplusStartup(&s_token, &in, nullptr);
    }

    auto panel = std::unique_ptr<DuiEmojiPanel>(new DuiEmojiPanel());
    panel->SetColumns(8);
    panel->SetCellSize(36);

    // Load color-emoji PNGs that ship with the project at Bin\Face\.
    // Each entry's "sequence" is set to the legacy face-tag form
    // ("[face0]", "[face1]", ...) so chat composers know what to insert.
    s_emojiHbms.reserve(kEmojiPngCount);
    for (int i = 0; i < kEmojiPngCount; ++i)
    {
        CString path = MakeFacePngPath(i);
        HBITMAP h = LoadPngAsHbitmap(path);
        s_emojiHbms.push_back(h);     // may be null if file missing
        CString tag; tag.Format(_T("[face%d]"), i);
        panel->AddEmojiBitmap(tag, h, tag);
    }

    panel->SetPickCallback(&EmojiPicked, nullptr);
    s_emojiPanel = panel.get();

    SIZE want = panel->GetDesiredSize();
    s_emojiPop.SetSize(want.cx + 8, want.cy + 8);
    s_emojiPop.SetEdge(DuiPopupHost::EdgeBelow);
    s_emojiPop.SetContent(std::move(panel));
}

} // anonymous

std::unique_ptr<DuiControl> Build_Emoji()
{
    EnsureEmojiPopupInited();
    auto page = NewPage();

    AddSection(page.get(),
               _T("DuiEmojiPanel inside DuiPopupHost"),
               _T("Click \"Open emoji\" to drop a popup with a Segoe UI Emoji grid. ")
               _T("Picking an emoji hides the popup via the pick callback and updates the label below."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);

        auto btn = std::unique_ptr<FnButton>(new FnButton());
        btn->SetText(_T("Open emoji"));
        btn->onClick = [](FnButton* b)
        {
            RECT cr = b->GetRect();
            POINT tl = { cr.left, cr.top };
            POINT br = { cr.right, cr.bottom };
            ::ClientToScreen(b->GetHost()->m_hWnd, &tl);
            ::ClientToScreen(b->GetHost()->m_hWnd, &br);
            RECT screenRc = { tl.x, tl.y, br.x, br.y };
            s_emojiPop.Show(screenRc, b->GetHost()->m_hWnd);
        };

        auto lbl = std::unique_ptr<DuiLabel>(new DuiLabel());
        lbl->SetText(_T("Last picked: (none)"));
        lbl->SetTextColor(RGB(60, 60, 60));
        s_emojiResult = lbl.get();

        row->AddChild(std::move(btn), DuiLayout::Hint().Fixed(160));
        row->AddChild(std::move(lbl), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }
    return page;
}

// ===== FrameWindow ====================================================

namespace {

// Static frame so its HWND survives across page rebuilds. Created on the
// first click; re-shown on subsequent clicks. SetClientContent installs
// a small sample tree (label + a few buttons) so the demo has something
// inside.
static DuiFrameWindow s_demoFrame;
static bool           s_demoFrameBuilt = false;

static void EnsureDemoFrameBuilt()
{
    if (s_demoFrameBuilt)
    {
        return;
    }
    s_demoFrameBuilt = true;

    s_demoFrame.SetTitle(_T("Custom skin frame demo"));
    s_demoFrame.SetMinSize(360, 240);

    // Sample client content: VBox with header + button row.
    auto root = std::unique_ptr<DuiVBox>(new DuiVBox());
    root->SetPadding(20);
    root->SetGap(10);

    auto h = std::unique_ptr<DuiLabel>(new DuiLabel());
    h->SetText(_T("Drag the title bar to move."));
    h->SetTextColor(RGB(45, 108, 223));
    root->AddChild(std::move(h), DuiLayout::Hint().Fixed(28));

    auto h2 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h2->SetText(_T("Drag any window edge or corner to resize."));
    h2->SetTextColor(RGB(80, 80, 80));
    root->AddChild(std::move(h2), DuiLayout::Hint().Fixed(22));

    auto h3 = std::unique_ptr<DuiLabel>(new DuiLabel());
    h3->SetText(_T("Min / Max / Close all run through WM_SYSCOMMAND."));
    h3->SetTextColor(RGB(80, 80, 80));
    root->AddChild(std::move(h3), DuiLayout::Hint().Fixed(22));

    auto rowBtn = std::unique_ptr<DuiHBox>(new DuiHBox());
    rowBtn->SetGap(8);
    for (int i = 0; i < 3; ++i)
    {
        auto b = std::unique_ptr<DuiButton>(new DuiButton());
        CString s; s.Format(_T("Action %d"), i + 1);
        b->SetText(s);
        rowBtn->AddChild(std::move(b), DuiLayout::Hint().Weight(1));
    }
    root->AddChild(std::move(rowBtn), DuiLayout::Hint().Fixed(36));
    root->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Weight(1));

    s_demoFrame.SetClientContent(std::move(root));
}

static void ShowDemoFrame(HWND hOwner)
{
    EnsureDemoFrameBuilt();
    if (!s_demoFrame.m_hWnd)
    {
        s_demoFrame.Create(hOwner, CWindow::rcDefault,
                           _T("Custom skin frame demo"),
                           WS_OVERLAPPEDWINDOW,
                           0);
        if (s_demoFrame.m_hWnd)
        {
            s_demoFrame.ResizeClient(480, 320);
            s_demoFrame.CenterWindow();
        }
    }
    if (s_demoFrame.m_hWnd)
    {
        s_demoFrame.ShowWindow(SW_SHOW);
        ::SetForegroundWindow(s_demoFrame.m_hWnd);
    }
}

} // anonymous

std::unique_ptr<DuiControl> Build_FrameWindow()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("DuiFrameWindow — system chrome stripped, custom title bar"),
               _T("Click \"Pop frame\" to open a top-level window with no system non-client area. ")
               _T("Drag title bar to move; drag any edge to resize; min/max/close are real DUI buttons routed through WM_SYSCOMMAND."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto btn = std::unique_ptr<FnButton>(new FnButton());
        btn->SetText(_T("Pop frame"));
        btn->onClick = [](FnButton* b)
        {
            ShowDemoFrame(b->GetHost()->m_hWnd);
        };
        row->AddChild(std::move(btn), DuiLayout::Hint().Fixed(160));

        auto note = std::unique_ptr<DuiLabel>(new DuiLabel());
        note->SetText(_T("(Closing the popped frame just hides it; click \"Pop frame\" again to bring it back.)"));
        note->SetTextColor(RGB(110, 110, 110));
        row->AddChild(std::move(note), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row));
    }
    return page;
}

// ===== Dock ===========================================================

namespace {

// Reuse pattern: paint-only colored pane + label, scoped local to this
// section so it doesn't collide with the same names elsewhere.
class ColorPaneDk : public DuiControl
{
public:
    ColorPaneDk(COLORREF c, LPCTSTR label) : m_color(c), m_label(label ? label : _T("")) {}
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }
        RECT inter;
        if (!::IntersectRect(&inter, &m_rcItem, &rcDirty))
        {
            return;
        }
        HBRUSH hbr = ::CreateSolidBrush(m_color);
        ::FillRect(hdc, &inter, hbr);
        ::DeleteObject(hbr);
        if (!m_label.IsEmpty())
        {
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, RGB(255, 255, 255));
            ::DrawText(hdc, m_label, -1, &m_rcItem,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
        }
    }
private:
    COLORREF m_color;
    CString  m_label;
};

} // anonymous

std::unique_ptr<DuiControl> Build_Dock()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("DuiDock — toolbar / status / nav / fill in one container"),
               _T("Children dock to a side with a pixel size; the last \"Fill\" child takes the leftover. ")
               _T("Cheaper than nesting HBox / VBox three deep for the typical IM main panel layout."));
    {
        auto dock = std::unique_ptr<DuiDock>(new DuiDock());
        dock->SetGap(2);
        dock->AddDocked(std::unique_ptr<DuiControl>(new ColorPaneDk(RGB(45, 108, 223), _T("toolbar (top, 32)"))),
                        DuiDock::DockTop, 32);
        dock->AddDocked(std::unique_ptr<DuiControl>(new ColorPaneDk(RGB(60, 175, 80), _T("status (bottom, 24)"))),
                        DuiDock::DockBottom, 24);
        dock->AddDocked(std::unique_ptr<DuiControl>(new ColorPaneDk(RGB(40, 60, 90), _T("nav (left, 140)"))),
                        DuiDock::DockLeft, 140);
        dock->AddDocked(std::unique_ptr<DuiControl>(new ColorPaneDk(RGB(120, 90, 180), _T("info (right, 120)"))),
                        DuiDock::DockRight, 120);
        dock->AddDocked(std::unique_ptr<DuiControl>(new ColorPaneDk(RGB(200, 90, 70), _T("content (fill)"))),
                        DuiDock::DockFill);

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(dock), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 280);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Compact (top + fill only)"),
               _T("Just a header strip + content area — common pattern for chat windows."));
    {
        auto dock = std::unique_ptr<DuiDock>(new DuiDock());
        dock->AddDocked(std::unique_ptr<DuiControl>(new ColorPaneDk(RGB(45, 108, 223), _T("title strip (top, 30)"))),
                        DuiDock::DockTop, 30);
        dock->AddDocked(std::unique_ptr<DuiControl>(new ColorPaneDk(RGB(245, 245, 248), _T("body (fill)"))),
                        DuiDock::DockFill);
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(std::move(dock), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 140);
    }
    return page;
}

// ===== LoginXml — Login dialog reproduced via DuiXmlBuilder ==========
//
// The legacy CLoginDlg in Source/LoginDlg.h drives a CDialogImpl /
// flamingo.rc layout (IDD_LOGINDLG, 187 x 98 dialog units):
//
//     [avatar     ] [combo: username]
//     [status btn ] [edit:  password]
//                   [□ 记住密码]   [注册账号]
//                   [□ 自动登录]   [找回密码]
//     [设置]                         [登录]
//
// The XML below describes the *same* visual layout using nothing but
// the Tier-2 DuiXmlBuilder vocabulary (vbox / hbox / label / edit /
// button). This page parses the XML at runtime, mounts the resulting
// tree into the gallery, and shows the source side-by-side so the
// reader can see what the parser produced.
//
// Differences from the production dialog (intentional, scope cut):
//   * Avatar / login-status are placeholder labels — the real dialog
//     uses CSkinPictureBox + a custom-drawn status button. The XML
//     builder doesn't yet have those tags; the layout slot is what's
//     under test.
//   * "注册账号" / "找回密码" are plain labels, not hyperlinks. Same
//     reason — the builder has no <hyperlink> tag yet.

namespace {

static const char kLoginXml[] =
"<vbox padding=\"20\" gap=\"10\">"
"  <hbox gap=\"14\" fixedHeight=\"80\">"
"    <vbox fixedWidth=\"72\" gap=\"6\">"
"      <label fixedHeight=\"58\" text=\"(avatar)\" textColor=\"60,60,60\"/>"
"      <button buttonType=\"icon\" fixedHeight=\"18\" text=\"online\"/>"
"    </vbox>"
"    <vbox weight=\"1\" gap=\"6\">"
"      <hbox fixedHeight=\"24\" gap=\"6\">"
"        <label fixedWidth=\"54\" text=\"账号\" textColor=\"60,60,60\"/>"
"        <edit  id=\"3001\" weight=\"1\" placeholder=\"UTalk number / email\"/>"
"      </hbox>"
"      <hbox fixedHeight=\"24\" gap=\"6\">"
"        <label fixedWidth=\"54\" text=\"密码\" textColor=\"60,60,60\"/>"
"        <edit  id=\"3002\" weight=\"1\" password=\"true\" placeholder=\"password\"/>"
"      </hbox>"
"      <hbox fixedHeight=\"22\" gap=\"12\">"
"        <button id=\"3003\" buttonType=\"checkbox\" fixedWidth=\"90\" text=\"记住密码\"/>"
"        <button id=\"3004\" buttonType=\"checkbox\" fixedWidth=\"90\" text=\"自动登录\"/>"
"        <label  fixedWidth=\"60\" text=\"注册账号\" textColor=\"45,108,223\"/>"
"        <label  fixedWidth=\"60\" text=\"找回密码\" textColor=\"45,108,223\"/>"
"      </hbox>"
"    </vbox>"
"  </hbox>"
"  <hbox fixedHeight=\"34\" gap=\"10\">"
"    <button id=\"3005\" text=\"设置\"  fixedWidth=\"80\"/>"
"    <label weight=\"1\" text=\"\"/>"
"    <button id=\"3006\" text=\"登录\"  fixedWidth=\"110\"/>"
"  </hbox>"
"</vbox>";

} // anonymous

std::unique_ptr<DuiControl> Build_LoginXml()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("LoginDlg reproduced from XML"),
               _T("DuiXmlBuilder::FromString parses the XML below at runtime and produces ")
               _T("the live login form on the right. The XML source is shown read-only on ")
               _T("the left so you can see what the builder consumed."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(16);

        // Left: source viewer (multi-line read-only edit). We display
        // the same bytes the parser sees, so any trailing whitespace
        // / \" escaping shows up exactly.
        auto srcBox = std::unique_ptr<DuiVBox>(new DuiVBox());
        srcBox->SetGap(4);

        auto srcHdr = std::unique_ptr<DuiLabel>(new DuiLabel());
        srcHdr->SetText(_T("XML source (passed to DuiXmlBuilder::FromString):"));
        srcHdr->SetTextColor(RGB(40, 40, 40));
        srcBox->AddChild(std::move(srcHdr), DuiLayout::Hint().Fixed(20));

        auto src = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        src->SetMultiLine(true);
        src->SetReadOnly(true);
        // ANSI -> CString. The XML uses raw bytes for the Chinese
        // labels; the file's saved as system code page, so reading
        // back via CString(LPCSTR) round-trips correctly.
        src->SetText(CString(kLoginXml));
        srcBox->AddChild(std::move(src), DuiLayout::Hint().Weight(1));

        // Right: built tree.
        auto rightBox = std::unique_ptr<DuiVBox>(new DuiVBox());
        rightBox->SetGap(4);

        auto rightHdr = std::unique_ptr<DuiLabel>(new DuiLabel());
        rightHdr->SetText(_T("Live tree built from the XML above:"));
        rightHdr->SetTextColor(RGB(40, 40, 40));
        rightBox->AddChild(std::move(rightHdr), DuiLayout::Hint().Fixed(20));

        // Parse & build. If parsing fails we substitute a red error
        // label so the demo still has a visible result.
        auto built = DuiXmlBuilder::FromString(kLoginXml);
        if (built)
        {
            rightBox->AddChild(std::move(built), DuiLayout::Hint().Weight(1));
        }
        else
        {
            auto err = std::unique_ptr<DuiLabel>(new DuiLabel());
            err->SetText(_T("DuiXmlBuilder::FromString returned null"));
            err->SetTextColor(RGB(220, 60, 60));
            rightBox->AddChild(std::move(err), DuiLayout::Hint().Weight(1));
        }

        row->AddChild(std::move(srcBox),   DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(rightBox), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), 320);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Recognized tags / attributes"),
               _T("Tags: vbox, hbox, grid, label, button, edit. ")
               _T("Common attrs: id, fixedWidth, fixedHeight, weight, margin. ")
               _T("Layouts also take padding + gap. Per-control: text, textColor=\"r,g,b\", ")
               _T("placeholder, password, multiline, buttonType=push|icon|checkbox|radio. ")
               _T("Unrecognized tags / attrs are silently ignored — extend the builder to add more."));
    return page;
}

// ===== Tier3 (Separator / Badge / GroupBox) ==========================

std::unique_ptr<DuiControl> Build_Tier3()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("DuiSeparator — horizontal + vertical line"),
               _T("Centered 1px line with optional color / thickness / inset."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);

        auto col = std::unique_ptr<DuiVBox>(new DuiVBox()); col->SetGap(6);
        auto a = std::unique_ptr<DuiLabel>(new DuiLabel());     a->SetText(_T("Above"));
        auto sep1 = std::unique_ptr<DuiSeparator>(new DuiSeparator());
        auto b = std::unique_ptr<DuiLabel>(new DuiLabel());     b->SetText(_T("Below"));
        col->AddChild(std::move(a),    DuiLayout::Hint().Fixed(20));
        col->AddChild(std::move(sep1), DuiLayout::Hint().Fixed(8));
        col->AddChild(std::move(b),    DuiLayout::Hint().Fixed(20));
        row->AddChild(std::move(col), DuiLayout::Hint().Weight(1));

        // Vertical separator with thicker line + brand color.
        auto col2 = std::unique_ptr<DuiHBox>(new DuiHBox()); col2->SetGap(8);
        auto la = std::unique_ptr<DuiLabel>(new DuiLabel());    la->SetText(_T("Left"));
        auto sep2 = std::unique_ptr<DuiSeparator>(new DuiSeparator());
        sep2->SetOrientation(DuiSeparator::Vertical);
        sep2->SetThickness(2);
        sep2->SetColor(RGB(45, 108, 223));
        sep2->SetInset(6);
        auto lb = std::unique_ptr<DuiLabel>(new DuiLabel());    lb->SetText(_T("Right"));
        col2->AddChild(std::move(la),   DuiLayout::Hint().Weight(1));
        col2->AddChild(std::move(sep2), DuiLayout::Hint().Fixed(8));
        col2->AddChild(std::move(lb),   DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(col2), DuiLayout::Hint().Weight(1));

        AddVariantRow(page.get(), std::move(row), 90);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("DuiBadge — unread count pill / circle"),
               _T("0 hides; 1-99 shows the number; 100+ shows \"99+\". Custom color also supported."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(28);

        struct Demo { int n; LPCTSTR caption; COLORREF c; };
        Demo demos[] = {
            { 0,    _T("count=0 (hidden)"),   RGB(220, 60, 60) },
            { 1,    _T("count=1"),            RGB(220, 60, 60) },
            { 9,    _T("count=9"),            RGB(220, 60, 60) },
            { 42,   _T("count=42"),           RGB(220, 60, 60) },
            { 240,  _T("count=240 (\"99+\")"),RGB(45, 108, 223) },
        };
        for (auto& d : demos)
        {
            auto col = std::unique_ptr<DuiVBox>(new DuiVBox()); col->SetGap(4);
            auto bdg = std::unique_ptr<DuiBadge>(new DuiBadge());
            bdg->SetCount(d.n);
            bdg->SetBgColor(d.c);
            auto lbl = std::unique_ptr<DuiLabel>(new DuiLabel());
            lbl->SetText(d.caption);
            lbl->SetTextColor(RGB(80, 80, 80));
            col->AddChild(std::move(bdg), DuiLayout::Hint().Fixed(24));
            col->AddChild(std::move(lbl), DuiLayout::Hint().Fixed(18));
            row->AddChild(std::move(col), DuiLayout::Hint().Fixed(110));
        }
        AddVariantRow(page.get(), std::move(row), 60);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("DuiGroupBox — titled rounded-rect border"),
               _T("Title rests on the top edge of a rounded border. Caller's content control is placed inside with a configurable padding."));
    {
        auto outerRow = std::unique_ptr<DuiHBox>(new DuiHBox()); outerRow->SetGap(18);

        // GroupBox 1: a VBox of labels.
        {
            auto gb = std::unique_ptr<DuiGroupBox>(new DuiGroupBox());
            gb->SetTitle(_T("Account"));
            auto inner = std::unique_ptr<DuiVBox>(new DuiVBox());
            inner->SetGap(6);
            auto l1 = std::unique_ptr<DuiLabel>(new DuiLabel()); l1->SetText(_T("UTalk number: 12345"));
            auto l2 = std::unique_ptr<DuiLabel>(new DuiLabel()); l2->SetText(_T("Email: alice@example.com"));
            auto l3 = std::unique_ptr<DuiLabel>(new DuiLabel()); l3->SetText(_T("Status: online"));
            inner->AddChild(std::move(l1), DuiLayout::Hint().Fixed(20));
            inner->AddChild(std::move(l2), DuiLayout::Hint().Fixed(20));
            inner->AddChild(std::move(l3), DuiLayout::Hint().Fixed(20));
            gb->SetContent(std::move(inner));
            outerRow->AddChild(std::move(gb), DuiLayout::Hint().Weight(1));
        }
        // GroupBox 2: a button row, brand-colored title.
        {
            auto gb = std::unique_ptr<DuiGroupBox>(new DuiGroupBox());
            gb->SetTitle(_T("Network proxy"));
            gb->SetTitleColor(RGB(45, 108, 223));
            gb->SetBorderColor(RGB(45, 108, 223));
            auto inner = std::unique_ptr<DuiHBox>(new DuiHBox());
            inner->SetGap(8);
            for (int i = 0; i < 3; ++i)
            {
                auto bt = std::unique_ptr<DuiButton>(new DuiButton());
                bt->SetButtonType(DuiButton::StyleRadio);
                bt->SetRadioGroup(31);
                static LPCTSTR names[] = { _T("Direct"), _T("System"), _T("Custom") };
                bt->SetText(names[i]);
                if (i == 0)
                {
                    bt->SetCheck(true, false);
                }
                inner->AddChild(std::move(bt), DuiLayout::Hint().Weight(1));
            }
            gb->SetContent(std::move(inner));
            outerRow->AddChild(std::move(gb), DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(outerRow), 140);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("DuiSearchBox — magnifier glyph + clear (×) when text present"),
               _T("Type into the field to see the clear button appear; click it to wipe the text."));
    {
        auto sb = std::unique_ptr<DuiSearchBox>(new DuiSearchBox());
        sb->SetPlaceholder(_T("Search friends..."));

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        row->AddChild(std::move(sb), DuiLayout::Hint().Fixed(280));
        AddVariantRow(page.get(), std::move(row), 32);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("DuiSpinBox — numeric field with up/down buttons"),
               _T("Range 0..50, step 5. Click ▲/▼ to increment or decrement."));
    {
        auto sp = std::unique_ptr<DuiSpinBox>(new DuiSpinBox());
        sp->SetRange(0, 50);
        sp->SetStep(5);
        sp->SetValue(20, false);

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        row->AddChild(std::move(sp), DuiLayout::Hint().Fixed(120));
        AddVariantRow(page.get(), std::move(row), 32);
    }
    return page;
}

// ===== Theme =========================================================

namespace {

// Paints a single named theme slot as a colored swatch + label so the
// demo page reflects ApplyPreset(Light/Dark) without each control
// having to subscribe individually.
class ThemeSwatch : public DuiControl
{
public:
    explicit ThemeSwatch(DuiTheme::Slot s, LPCTSTR label)
        : m_slot(s), m_label(label) {}
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }
        RECT inter;
        if (!::IntersectRect(&inter, &m_rcItem, &rcDirty))
        {
            return;
        }

        COLORREF c = DuiTheme::Inst().Get(m_slot);
        HBRUSH br = ::CreateSolidBrush(c);
        ::FillRect(hdc, &m_rcItem, br);
        ::DeleteObject(br);

        BYTE r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
        int luma = (r * 299 + g * 587 + b * 114) / 1000;
        COLORREF txt = luma > 140 ? RGB(20, 20, 20) : RGB(255, 255, 255);
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, txt);
        ::DrawText(hdc, m_label, -1, &m_rcItem,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
    }
private:
    DuiTheme::Slot m_slot;
    CString        m_label;
};

} // anonymous

std::unique_ptr<DuiControl> Build_Theme()
{
    auto page = NewPage();

    AddSection(page.get(),
               _T("DuiTheme — central palette + Light / Dark presets"),
               _T("Click a button to swap the global palette. Switch away and back to ")
               _T("this tab — every swatch reads its slot color live from DuiTheme::Inst()."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        auto bL = std::unique_ptr<FnButton>(new FnButton());
        bL->SetText(_T("Apply Light"));
        bL->onClick = [](FnButton* b) {
            DuiTheme::Inst().ApplyPreset(DuiTheme::Light);
            if (b->GetHost()) b->GetHost()->Invalidate(FALSE);
        };
        auto bD = std::unique_ptr<FnButton>(new FnButton());
        bD->SetText(_T("Apply Dark"));
        bD->onClick = [](FnButton* b) {
            DuiTheme::Inst().ApplyPreset(DuiTheme::Dark);
            if (b->GetHost()) b->GetHost()->Invalidate(FALSE);
        };
        row->AddChild(std::move(bL), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(bD), DuiLayout::Hint().Fixed(120));
        AddVariantRow(page.get(), std::move(row), 32);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(),
               _T("Palette swatches"),
               _T("Live readout of the current palette."));
    {
        struct Pair { DuiTheme::Slot s; LPCTSTR name; };
        Pair items[] = {
            { DuiTheme::BrandPrimary,  _T("Brand")    },
            { DuiTheme::BrandHover,    _T("Hover")    },
            { DuiTheme::BrandPressed,  _T("Pressed")  },
            { DuiTheme::SurfaceBg,     _T("Surface")  },
            { DuiTheme::SurfaceAltBg,  _T("AltSurf")  },
            { DuiTheme::TextDefault,   _T("Text")     },
            { DuiTheme::TextSubtle,    _T("Subtle")   },
            { DuiTheme::BorderHeavy,   _T("Border")   },
            { DuiTheme::RowSel,        _T("RowSel")   },
            { DuiTheme::StatusOnline,  _T("Online")   },
            { DuiTheme::StatusAway,    _T("Away")     },
            { DuiTheme::StatusBusy,    _T("Busy")     },
            { DuiTheme::StatusOffline, _T("Offline")  },
        };
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(4);
        for (auto& it : items)
        {
            row->AddChild(std::unique_ptr<DuiControl>(
                new ThemeSwatch(it.s, it.name)), DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(row), 36);
    }
    return page;
}

// ===== page table =====================================================

const PageInfo* GetPages(int& outCount)
{
    static const PageInfo pages[] = {
        { _T("Button"),       &Build_Button       },
        { _T("Avatar"),       &Build_Avatar       },
        { _T("Label"),        &Build_Label        },
        { _T("Edit"),         &Build_Edit         },
        { _T("RichEdit"),     &Build_RichEdit     },
        { _T("ComboBox"),     &Build_ComboBox     },
        { _T("Slider"),       &Build_Slider       },
        { _T("Switch"),       &Build_Switch       },
        { _T("ProgressBar"),  &Build_ProgressBar  },
        { _T("Tab"),          &Build_Tab          },
        { _T("ListBox"),      &Build_ListBox      },
        { _T("VirtualList"),  &Build_VirtualList  },
        { _T("ScrollView"),   &Build_ScrollView   },
        { _T("Menu"),         &Build_Menu         },
        { _T("MenuBar"),      &Build_MenuBar      },
        { _T("ToolTip"),      &Build_ToolTip      },
        { _T("Layout"),       &Build_Layout       },
        { _T("Splitter"),     &Build_Splitter     },
        { _T("TabPage"),      &Build_TabPage      },
        { _T("TreeView"),     &Build_TreeView     },
        { _T("PopupHost"),    &Build_PopupHost    },
        { _T("Emoji"),        &Build_Emoji        },
        { _T("FrameWindow"),  &Build_FrameWindow  },
        { _T("Dock"),         &Build_Dock         },
        { _T("LoginXml"),     &Build_LoginXml     },
        { _T("Tier3"),        &Build_Tier3        },
        { _T("Theme"),        &Build_Theme        },
        { _T("DocCaptures"),  &Build_DocCaptures  },
        { _T("Layouts"),      &Build_Layouts      },
    };
    outCount = (int)(sizeof(pages) / sizeof(pages[0]));
    return pages;
}

// =====================================================================
// Build_DocCaptures
//
// Single page that defines every screenshot needed by guides.html. Each
// section contains exactly one row registered with AddVariantRowCapture
// so --capture-all writes ctl-<name>.png for it. The page is also
// browsable in the Gallery as the "DocCaptures" tab so the layout can
// be visually verified before re-running the capture command.
//
// Naming convention: ctl-<group>-<variant>.png
// Group names map to the categories in flamingoclient/docs/guides.html.
// =====================================================================

// Tiny helper for the DocCaptures page: paint a flat-color rect with a
// centered text label. DuiLabel does not expose SetBgColor, so for the
// layout-container demos (where each cell needs to be visually
// distinct) we use this minimal subclass instead. Pure local helper -
// not part of the library API.
class DocColorTile : public DuiControl
{
public:
    DocColorTile(LPCTSTR text, COLORREF bg, COLORREF fg = RGB(40, 40, 40))
        : m_text(text ? text : _T("")), m_bg(bg), m_fg(fg)
    {
    }

    void OnPaint(HDC hdc, const RECT&) override
    {
        HBRUSH br = ::CreateSolidBrush(m_bg);
        ::FillRect(hdc, &m_rcItem, br);
        ::DeleteObject(br);
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, m_fg);
        RECT r = m_rcItem;
        ::DrawText(hdc, m_text, -1, &r,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

private:
    CString  m_text;
    COLORREF m_bg;
    COLORREF m_fg;
};

std::unique_ptr<DuiControl> Build_DocCaptures()
{
    auto page = NewPage();

    // ---- 1) Layout ---------------------------------------------------

    AddSection(page.get(), _T("layout-vbox"),
               _T("DuiVBox with 3 children + gap=8 + padding=12."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        auto vb  = std::unique_ptr<DuiVBox>(new DuiVBox());
        vb->SetPadding(12); vb->SetGap(8);
        for (int i = 0; i < 3; ++i)
        {
            auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
            CString s; s.Format(_T("Row %d"), i + 1);
            l->SetText(s);
            l->SetTextColor(RGB(50, 50, 50));
            vb->AddChild(std::move(l), DuiLayout::Hint().Fixed(20));
        }
        row->AddChild(std::move(vb), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("layout-vbox"), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-hbox"),
               _T("DuiHBox with 4 children + gap=8 + padding=12."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        auto hb  = std::unique_ptr<DuiHBox>(new DuiHBox());
        hb->SetPadding(12); hb->SetGap(8);
        for (int i = 0; i < 4; ++i)
        {
            auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
            CString s; s.Format(_T("#%d"), i + 1);
            l->SetText(s);
            l->SetTextColor(RGB(50, 50, 50));
            hb->AddChild(std::move(l), DuiLayout::Hint().Fixed(60));
        }
        row->AddChild(std::move(hb), DuiLayout::Hint().Fixed(380));
        AddVariantRowCapture(page.get(), _T("layout-hbox"), std::move(row), 64);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-grid-equal"),
               _T("DuiGrid 3 columns equal width, 2 rows."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto g   = std::unique_ptr<DuiGrid>(new DuiGrid());
        g->SetGrid(2, 3); g->SetGap(6); g->SetPadding(8);
        COLORREF tints[] = {
            RGB(220,235,255), RGB(225,245,225), RGB(245,225,245),
            RGB(255,235,220), RGB(245,245,210), RGB(220,245,235),
        };
        for (int i = 0; i < 6; ++i)
        {
            CString s; s.Format(_T("Cell %d"), i + 1);
            g->AddChild(std::unique_ptr<DuiControl>(new DocColorTile(s, tints[i])));
        }
        row->AddChild(std::move(g), DuiLayout::Hint().Fixed(360));
        AddVariantRowCapture(page.get(), _T("layout-grid-equal"), std::move(row), 80);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-grid-uneven"),
               _T("DuiGrid 4 columns × 2 rows of varied colored cells."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto g   = std::unique_ptr<DuiGrid>(new DuiGrid());
        g->SetGrid(2, 4); g->SetGap(4); g->SetPadding(6);
        COLORREF tints[] = {
            RGB(220,235,255), RGB(255,235,220),
            RGB(225,245,225), RGB(245,225,245),
            RGB(245,245,210), RGB(225,235,245),
            RGB(235,225,210), RGB(220,245,235),
        };
        for (int i = 0; i < 8; ++i)
        {
            CString s; s.Format(_T("R%d C%d"), i / 4 + 1, i % 4 + 1);
            g->AddChild(std::unique_ptr<DuiControl>(new DocColorTile(s, tints[i])));
        }
        row->AddChild(std::move(g), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("layout-grid-uneven"), std::move(row), 80);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("splitter-horizontal"),
               _T("Horizontal DuiSplitter — drag the central handle to resize panes."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto sp  = std::unique_ptr<DuiSplitter>(new DuiSplitter());
        sp->SetOrientation(DuiSplitter::Horizontal);
        sp->SetPane(0, std::unique_ptr<DuiControl>(
            new DocColorTile(_T(" Left pane "),  RGB(220, 235, 255))));
        sp->SetPane(1, std::unique_ptr<DuiControl>(
            new DocColorTile(_T(" Right pane "), RGB(245, 225, 245))));
        row->AddChild(std::move(sp), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("splitter-horizontal"), std::move(row), 80);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("splitter-vertical"),
               _T("Vertical DuiSplitter."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto sp  = std::unique_ptr<DuiSplitter>(new DuiSplitter());
        sp->SetOrientation(DuiSplitter::Vertical);
        sp->SetPane(0, std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Top pane"),    RGB(220, 235, 255))));
        sp->SetPane(1, std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Bottom pane"), RGB(245, 225, 245))));
        row->AddChild(std::move(sp), DuiLayout::Hint().Fixed(280));
        AddVariantRowCapture(page.get(), _T("splitter-vertical"), std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("dock-five-zones"),
               _T("DuiDock with 5 colored regions (top / bottom / left / right / fill)."));
    {
        auto row  = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto dock = std::unique_ptr<DuiDock>(new DuiDock());
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Top"),    RGB(220, 235, 255))),
            DuiDock::DockTop,    36);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Bottom"), RGB(245, 225, 245))),
            DuiDock::DockBottom, 36);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Left"),   RGB(225, 245, 225))),
            DuiDock::DockLeft,   60);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Right"),  RGB(255, 235, 220))),
            DuiDock::DockRight,  60);
        dock->AddDocked(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Center / Fill"), RGB(245, 245, 210))),
            DuiDock::DockFill);
        row->AddChild(std::move(dock), DuiLayout::Hint().Fixed(380));
        AddVariantRowCapture(page.get(), _T("dock-five-zones"), std::move(row), 200);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 2) Basic ----------------------------------------------------

    AddSection(page.get(), _T("label-single"),
               _T("Single-line label, default 9pt YaHei, ink-1 text."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
        l->SetText(_T("Hello, balloonui — single-line label"));
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(360));
        AddVariantRowCapture(page.get(), _T("label-single"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("label-multiline"),
               _T("Word-wrapped label spanning multiple lines."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
        l->SetText(_T("Multi-line labels wrap on whitespace by default. ")
                   _T("Pass DT_WORDBREAK in the text-align flags to ")
                   _T("opt in. Width is bounded by the parent layout."));
        l->SetTextAlign(DT_LEFT | DT_TOP | DT_WORDBREAK);
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("label-multiline"), std::move(row), 70);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("label-ellipsis"),
               _T("Single-line truncation with trailing ellipsis."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
        l->SetText(_T("This text is a lot longer than the label is wide so it will be truncated with an ellipsis at the end."));
        l->SetTextAlign(DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(280));
        AddVariantRowCapture(page.get(), _T("label-ellipsis"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("label-link-hover"),
               _T("Link-style label (blue underline). Hover state forced via DebugSetHover."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto l1 = std::unique_ptr<DuiLabel>(new DuiLabel());
        l1->SetText(_T("Forgot password?"));
        l1->SetTextColor(RGB(45, 108, 223));

        auto l2 = std::unique_ptr<DuiLabel>(new DuiLabel());
        l2->SetText(_T("Forgot password? (hover)"));
        l2->SetTextColor(RGB(30,  74, 153));
        l2->DebugSetHover(true);

        row->AddChild(std::move(l1), DuiLayout::Hint().Fixed(180));
        row->AddChild(std::move(l2), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("label-link-hover"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("button-styles-overview"),
               _T("4 DuiButton styles: PushButton / Checkbox / Radio / Icon."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(16);
        auto bP = std::unique_ptr<DuiButton>(new DuiButton());
        bP->SetText(_T("Save"));
        auto bC = std::unique_ptr<DuiButton>(new DuiButton());
        bC->SetButtonType(DuiButton::StyleCheckbox);
        bC->SetText(_T("Checked")); bC->SetCheck(true, false);
        auto bR = std::unique_ptr<DuiButton>(new DuiButton());
        bR->SetButtonType(DuiButton::StyleRadio);
        bR->SetText(_T("Radio")); bR->SetCheck(true, false);
        auto bI = std::unique_ptr<DuiButton>(new DuiButton());
        bI->SetButtonType(DuiButton::StyleIcon);
        bI->SetText(_T("Open"));
        row->AddChild(std::move(bP), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bC), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bR), DuiLayout::Hint().Fixed(110));
        row->AddChild(std::move(bI), DuiLayout::Hint().Fixed(110));
        AddVariantRowCapture(page.get(), _T("button-styles-overview"), std::move(row));
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("badge-types"),
               _T("DuiBadge: red dot / count / 99+ / custom color."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto b1 = std::unique_ptr<DuiBadge>(new DuiBadge()); b1->SetText(_T(" "));
        auto b2 = std::unique_ptr<DuiBadge>(new DuiBadge()); b2->SetCount(3);
        auto b3 = std::unique_ptr<DuiBadge>(new DuiBadge()); b3->SetCount(150);
        auto b4 = std::unique_ptr<DuiBadge>(new DuiBadge());
        b4->SetCount(7); b4->SetBgColor(RGB(45, 108, 223));
        row->AddChild(std::move(b1), DuiLayout::Hint().Fixed(40));
        row->AddChild(std::move(b2), DuiLayout::Hint().Fixed(40));
        row->AddChild(std::move(b3), DuiLayout::Hint().Fixed(40));
        row->AddChild(std::move(b4), DuiLayout::Hint().Fixed(40));
        AddVariantRowCapture(page.get(), _T("badge-types"), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("avatar-grid"),
               _T("Avatar variations: bitmap circle / bitmap rounded / initials / status dots."));
    {
        // Pull the avatar bitmaps that Build_Avatar already cached.
        static HBITMAP s_blue   = MakeAvatarSourceBitmap( 80, 130, 220,  30,  60, 130);
        static HBITMAP s_purple = MakeAvatarSourceBitmap(170,  90, 200,  90,  30, 130);

        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(14);

        auto a1 = std::unique_ptr<DuiAvatar>(new DuiAvatar());
        a1->SetBitmap(s_blue);
        auto a2 = std::unique_ptr<DuiAvatar>(new DuiAvatar());
        a2->SetBitmap(s_purple); a2->SetShape(DuiAvatar::ShapeRoundRect); a2->SetCornerRadius(12);
        auto a3 = std::unique_ptr<DuiAvatar>(new DuiAvatar());
        a3->SetName(_T("Alice Smith"));
        a3->SetFallbackBgColor(RGB(45, 108, 223));
        auto a4 = std::unique_ptr<DuiAvatar>(new DuiAvatar());
        a4->SetName(_T("陆星辰"));
        a4->SetFallbackBgColor(RGB( 50, 160, 110));
        auto a5 = std::unique_ptr<DuiAvatar>(new DuiAvatar());
        a5->SetBitmap(s_blue); a5->SetStatus(DuiAvatar::StatusOnline);
        auto a6 = std::unique_ptr<DuiAvatar>(new DuiAvatar());
        a6->SetBitmap(s_blue); a6->SetStatus(DuiAvatar::StatusAway);
        auto a7 = std::unique_ptr<DuiAvatar>(new DuiAvatar());
        a7->SetBitmap(s_blue); a7->SetStatus(DuiAvatar::StatusBusy);
        auto a8 = std::unique_ptr<DuiAvatar>(new DuiAvatar());
        a8->SetBitmap(s_blue); a8->SetStatus(DuiAvatar::StatusOffline);

        row->AddChild(std::move(a1), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(a2), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(a3), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(a4), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(a5), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(a6), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(a7), DuiLayout::Hint().Fixed(56));
        row->AddChild(std::move(a8), DuiLayout::Hint().Fixed(56));
        AddVariantRowCapture(page.get(), _T("avatar-grid"), std::move(row), 56);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("separator-horizontal"),
               _T("Plain 1px horizontal separator."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto s = std::unique_ptr<DuiSeparator>(new DuiSeparator());
        s->SetOrientation(DuiSeparator::Horizontal);
        row->AddChild(std::move(s), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("separator-horizontal"), std::move(row), 16);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("separator-labeled"),
               _T("Vertical separator alongside two labels."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        auto l1 = std::unique_ptr<DuiLabel>(new DuiLabel());
        l1->SetText(_T(" Online")); l1->SetTextColor(RGB(40, 40, 40));
        auto sep = std::unique_ptr<DuiSeparator>(new DuiSeparator());
        sep->SetOrientation(DuiSeparator::Vertical);
        auto l2 = std::unique_ptr<DuiLabel>(new DuiLabel());
        l2->SetText(_T(" Offline")); l2->SetTextColor(RGB(40, 40, 40));
        row->AddChild(std::move(l1),  DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(sep), DuiLayout::Hint().Fixed(2));
        row->AddChild(std::move(l2),  DuiLayout::Hint().Fixed(80));
        AddVariantRowCapture(page.get(), _T("separator-labeled"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("groupbox-sample"),
               _T("DuiGroupBox with title strip and one nested child."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto gb  = std::unique_ptr<DuiGroupBox>(new DuiGroupBox());
        gb->SetTitle(_T(" Account "));
        auto lbl = std::unique_ptr<DuiLabel>(new DuiLabel());
        lbl->SetText(_T("  Username:  alice@example.com"));
        gb->AddChild(std::move(lbl));
        row->AddChild(std::move(gb), DuiLayout::Hint().Fixed(280));
        AddVariantRowCapture(page.get(), _T("groupbox-sample"), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 3) Inputs --------------------------------------------------

    AddSection(page.get(), _T("searchbox-states"),
               _T("Empty + populated DuiSearchBox side-by-side."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(16);
        auto s1 = std::unique_ptr<DuiSearchBox>(new DuiSearchBox());
        s1->SetPlaceholder(_T("Search contacts..."));
        auto s2 = std::unique_ptr<DuiSearchBox>(new DuiSearchBox());
        s2->SetText(_T("alice"));
        row->AddChild(std::move(s1), DuiLayout::Hint().Fixed(220));
        row->AddChild(std::move(s2), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("searchbox-states"), std::move(row), 32);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("spinbox-default"),
               _T("DuiSpinBox showing a numeric value with up/down arrows."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto sp = std::unique_ptr<DuiSpinBox>(new DuiSpinBox());
        sp->SetRange(0, 999); sp->SetValue(42, false);
        row->AddChild(std::move(sp), DuiLayout::Hint().Fixed(120));
        AddVariantRowCapture(page.get(), _T("spinbox-default"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("progressbar-states"),
               _T("Progress bar at 0% / 35% / 100%."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto p0 = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
        p0->SetRange(0, 100); p0->SetPos(0);
        auto p1 = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
        p1->SetRange(0, 100); p1->SetPos(35);
        auto p2 = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
        p2->SetRange(0, 100); p2->SetPos(100);
        row->AddChild(std::move(p0), DuiLayout::Hint().Fixed(140));
        row->AddChild(std::move(p1), DuiLayout::Hint().Fixed(140));
        row->AddChild(std::move(p2), DuiLayout::Hint().Fixed(140));
        AddVariantRowCapture(page.get(), _T("progressbar-states"), std::move(row), 24);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("slider-horizontal"),
               _T("Horizontal slider 0% / 50% / 100% / disabled."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto s0 = std::unique_ptr<DuiSlider>(new DuiSlider());
        s0->SetRange(0, 100); s0->SetPos(0, false);
        auto s1 = std::unique_ptr<DuiSlider>(new DuiSlider());
        s1->SetRange(0, 100); s1->SetPos(50, false);
        auto s2 = std::unique_ptr<DuiSlider>(new DuiSlider());
        s2->SetRange(0, 100); s2->SetPos(100, false);
        auto s3 = std::unique_ptr<DuiSlider>(new DuiSlider());
        s3->SetRange(0, 100); s3->SetPos(70, false); s3->SetEnabled(false);
        row->AddChild(std::move(s0), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(s1), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(s2), DuiLayout::Hint().Fixed(120));
        row->AddChild(std::move(s3), DuiLayout::Hint().Fixed(120));
        AddVariantRowCapture(page.get(), _T("slider-horizontal"), std::move(row), 24);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("slider-vertical"),
               _T("Vertical slider variants."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        for (int v : { 20, 50, 80 })
        {
            auto s = std::unique_ptr<DuiSlider>(new DuiSlider());
            s->SetVertical(true);
            s->SetRange(0, 100); s->SetPos(v, false);
            row->AddChild(std::move(s), DuiLayout::Hint().Fixed(28));
        }
        AddVariantRowCapture(page.get(), _T("slider-vertical"), std::move(row), 140);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("combobox-collapsed"),
               _T("Collapsed DuiComboBox showing the selected item + dropdown arrow."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto cb = std::unique_ptr<DuiComboBox>(new DuiComboBox());
        cb->AddString(_T("English"));
        cb->AddString(_T("中文 简体"));
        cb->AddString(_T("日本語"));
        cb->SetCurSel(1, false);
        row->AddChild(std::move(cb), DuiLayout::Hint().Fixed(180));
        AddVariantRowCapture(page.get(), _T("combobox-collapsed"), std::move(row), 30);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 4) Lists & nav --------------------------------------------

    AddSection(page.get(), _T("listbox-single"),
               _T("Single-select DuiListBox with hover and selected items."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto lb = std::unique_ptr<DuiListBox>(new DuiListBox());
        lb->AddItem(_T("Inbox"));
        lb->AddItem(_T("Drafts"));
        lb->AddItem(_T("Sent"));
        lb->AddItem(_T("Trash"));
        lb->SetCurSel(1, false);
        row->AddChild(std::move(lb), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("listbox-single"), std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("listbox-multi"),
               _T("Multi-select list with several items checked."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto lb = std::unique_ptr<DuiListBox>(new DuiListBox());
        lb->SetMultiSelect(true);
        lb->AddItem(_T("Apple"));
        lb->AddItem(_T("Banana"));
        lb->AddItem(_T("Cherry"));
        lb->AddItem(_T("Durian"));
        lb->SetItemSelected(0, true);
        lb->SetItemSelected(2, true);
        row->AddChild(std::move(lb), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("listbox-multi"), std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("tab-horizontal"),
               _T("Horizontal DuiTab with selected / inactive / disabled tabs."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto t = std::unique_ptr<DuiTab>(new DuiTab());
        t->AddTab(_T("Friends"));
        t->AddTab(_T("Groups"));
        t->AddTab(_T("Recent"));
        t->AddTab(_T("Archive"));
        t->SetCurSel(1, false);
        row->AddChild(std::move(t), DuiLayout::Hint().Fixed(420));
        AddVariantRowCapture(page.get(), _T("tab-horizontal"), std::move(row), 36);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("tab-vertical"),
               _T("Settings-style vertical nav (DuiListBox styled as a sidebar)."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto lb = std::unique_ptr<DuiListBox>(new DuiListBox());
        lb->SetItemHeight(28);
        lb->AddItem(_T("General"));
        lb->AddItem(_T("Account"));
        lb->AddItem(_T("Privacy"));
        lb->AddItem(_T("Appearance"));
        lb->SetCurSel(0, false);
        row->AddChild(std::move(lb), DuiLayout::Hint().Fixed(160));
        AddVariantRowCapture(page.get(), _T("tab-vertical"), std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("treeview-states"),
               _T("DuiTreeView with expanded + collapsed parents."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto tv = std::unique_ptr<DuiTreeView>(new DuiTreeView());
        int root1 = tv->AddRoot(_T("Friends"));
        tv->AddChild(root1, _T("Alice"));
        tv->AddChild(root1, _T("Bob"));
        tv->AddChild(root1, _T("Cindy"));
        tv->Expand(root1);
        int root2 = tv->AddRoot(_T("Groups (collapsed)"));
        tv->AddChild(root2, _T("Dev"));
        tv->AddChild(root2, _T("Design"));
        row->AddChild(std::move(tv), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("treeview-states"), std::move(row), 140);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 5) Theme / paint helpers -----------------------------------

    AddSection(page.get(), _T("theme-swatches"),
               _T("Theme color swatches: brand / ink / status."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(8);
        struct Sw { COLORREF c; LPCTSTR name; };
        Sw sws[] = {
            { RGB( 45, 108, 223), _T("brand")  },
            { RGB( 30,  74, 153), _T("deep")   },
            { RGB( 50, 160, 110), _T("online") },
            { RGB(220, 170,  60), _T("away")   },
            { RGB(220,  60,  60), _T("busy")   },
            { RGB(120, 120, 120), _T("off")    },
        };
        for (auto& s : sws)
        {
            row->AddChild(std::unique_ptr<DuiControl>(
                new DocColorTile(s.name, s.c, RGB(255, 255, 255))),
                DuiLayout::Hint().Fixed(80));
        }
        AddVariantRowCapture(page.get(), _T("theme-swatches"), std::move(row), 36);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 6) PaintAA demo --------------------------------------------

    AddSection(page.get(), _T("paintaa-comparison"),
               _T("Anti-aliased vs plain GDI: triangle, ellipse, diagonal line."));
    {
        // Custom DuiControl that paints two side-by-side glyph rows:
        // top row = plain GDI Polygon/Ellipse/LineTo, bottom row =
        // DuiAA::FillPolygon/FillEllipse/DrawLine. Lets the doc image
        // show the jagged-vs-smooth difference at a glance.
        class AAComparisonTile : public DuiControl
        {
        public:
            void OnPaint(HDC hdc, const RECT&) override
            {
                ::FillRect(hdc, &m_rcItem,
                           ::GetSysColorBrush(COLOR_BTNFACE));

                int midY = (m_rcItem.top + m_rcItem.bottom) / 2;
                int rowH = (m_rcItem.bottom - m_rcItem.top) / 2;
                int x = m_rcItem.left + 20;

                // ---- top row: plain GDI (jaggies) ----
                HPEN penBlack = ::CreatePen(PS_SOLID, 2, RGB(40, 40, 40));
                HBRUSH brBlue = ::CreateSolidBrush(RGB(80, 130, 220));
                HGDIOBJ oldP = ::SelectObject(hdc, penBlack);
                HGDIOBJ oldB = ::SelectObject(hdc, brBlue);

                POINT tri[3] = {
                    { x + 30, m_rcItem.top + 12 },
                    { x +  8, m_rcItem.top + rowH - 12 },
                    { x + 52, m_rcItem.top + rowH - 12 },
                };
                ::Polygon(hdc, tri, 3);

                ::Ellipse(hdc,
                          x + 80,  m_rcItem.top + 12,
                          x + 130, m_rcItem.top + rowH - 12);

                ::MoveToEx(hdc, x + 160, m_rcItem.top + 12, nullptr);
                ::LineTo(hdc,   x + 220, m_rcItem.top + rowH - 12);

                ::SelectObject(hdc, oldP);
                ::SelectObject(hdc, oldB);
                ::DeleteObject(penBlack);
                ::DeleteObject(brBlue);

                ::SetBkMode(hdc, TRANSPARENT);
                ::SetTextColor(hdc, RGB(120, 30, 30));
                RECT rT = { m_rcItem.left + 240, m_rcItem.top + 4,
                            m_rcItem.right,
                            m_rcItem.top + rowH };
                ::DrawText(hdc, _T("plain GDI — jaggies"), -1, &rT,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // ---- bottom row: DuiPaintAA helpers ----
                POINT tri2[3] = {
                    { x + 30, midY + 12 },
                    { x +  8, m_rcItem.bottom - 12 },
                    { x + 52, m_rcItem.bottom - 12 },
                };
                DuiAA::FillPolygon(hdc, tri2, 3,
                                   RGB(80, 130, 220), RGB(40, 40, 40));

                RECT rE = { x + 80, midY + 12,
                            x + 130, m_rcItem.bottom - 12 };
                DuiAA::FillEllipse(hdc, rE,
                                   RGB(80, 130, 220), RGB(40, 40, 40));

                DuiAA::DrawLine(hdc, x + 160, midY + 12,
                                     x + 220, m_rcItem.bottom - 12,
                                     RGB(40, 40, 40), 2);

                ::SetTextColor(hdc, RGB(20, 100, 50));
                RECT rB = { m_rcItem.left + 240, midY + 4,
                            m_rcItem.right, m_rcItem.bottom };
                ::DrawText(hdc, _T("DuiPaintAA — smooth"), -1, &rB,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        };

        auto row  = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto tile = std::unique_ptr<AAComparisonTile>(new AAComparisonTile());
        row->AddChild(std::move(tile), DuiLayout::Hint().Fixed(440));
        AddVariantRowCapture(page.get(), _T("paintaa-comparison"),
                             std::move(row), 140);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 6b) More controls -----------------------------------------

    AddSection(page.get(), _T("edit-states"),
               _T("DuiEditHost — empty placeholder / filled / focused / disabled."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(12);
        auto e1 = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e1->SetPlaceholder(_T("Type here..."));
        auto e2 = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e2->SetText(_T("alice@example.com"));
        auto e3 = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e3->SetText(_T("focused")); e3->DebugSetFocused(true);
        auto e4 = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        e4->SetText(_T("disabled")); e4->SetEnabled(false);
        row->AddChild(std::move(e1), DuiLayout::Hint().Fixed(150));
        row->AddChild(std::move(e2), DuiLayout::Hint().Fixed(150));
        row->AddChild(std::move(e3), DuiLayout::Hint().Fixed(150));
        row->AddChild(std::move(e4), DuiLayout::Hint().Fixed(150));
        AddVariantRowCapture(page.get(), _T("edit-states"), std::move(row), 28);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("richedit-styles"),
               _T("DuiRichEditHost — bold / italic / colored / link runs."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto re = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
        re->SetText(_T("Plain. Bold. Italic. Colored. Link."));
        row->AddChild(std::move(re), DuiLayout::Hint().Fixed(440));
        AddVariantRowCapture(page.get(), _T("richedit-styles"), std::move(row), 50);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("scrollbar-states"),
               _T("DuiScrollBar — vertical / horizontal."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox()); row->SetGap(20);
        auto sbV = std::unique_ptr<DuiScrollBar>(new DuiScrollBar());
        // Vertical is default
        sbV->SetRange(0, 1000); sbV->SetPage(200); sbV->SetPos(300);
        auto sbH = std::unique_ptr<DuiScrollBar>(new DuiScrollBar());
        sbH->SetHorizontal(true);
        sbH->SetRange(0, 1000); sbH->SetPage(200); sbH->SetPos(300);
        row->AddChild(std::move(sbV), DuiLayout::Hint().Fixed(14));
        row->AddChild(std::move(sbH), DuiLayout::Hint().Fixed(280));
        AddVariantRowCapture(page.get(), _T("scrollbar-states"), std::move(row), 100);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("emojipanel-default"),
               _T("DuiEmojiPanel default 8×3 grid."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto ep = std::unique_ptr<DuiEmojiPanel>(new DuiEmojiPanel());
        row->AddChild(std::move(ep), DuiLayout::Hint().Fixed(360));
        AddVariantRowCapture(page.get(), _T("emojipanel-default"), std::move(row), 200);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("tabpage-content"),
               _T("DuiTabPage hosting a small content area under a tab strip."));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto vb  = std::unique_ptr<DuiVBox>(new DuiVBox()); vb->SetGap(0);
        auto t = std::unique_ptr<DuiTab>(new DuiTab());
        t->AddTab(_T("Profile"));
        t->AddTab(_T("Activity"));
        t->AddTab(_T("Notes"));
        t->SetCurSel(0, false);
        vb->AddChild(std::move(t), DuiLayout::Hint().Fixed(36));
        vb->AddChild(std::unique_ptr<DuiControl>(
            new DocColorTile(_T("Profile content panel"),
                             RGB(245, 245, 245))),
            DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(vb), DuiLayout::Hint().Fixed(400));
        AddVariantRowCapture(page.get(), _T("tabpage-content"),
                             std::move(row), 130);
    }
    AddGap(page.get(), kSectionGap);

    // ---- 7) Window / host snapshot ----------------------------------

    AddSection(page.get(), _T("host-tree"),
               _T("DuiHost composing a small tree: root VBox holds a header label and an HBox of buttons."));
    {
        auto row  = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto root = std::unique_ptr<DuiVBox>(new DuiVBox());
        root->SetPadding(10); root->SetGap(8);
        auto title = std::unique_ptr<DuiLabel>(new DuiLabel());
        title->SetText(_T("Login"));
        title->SetTextColor(RGB(20, 20, 20));
        root->AddChild(std::move(title), DuiLayout::Hint().Fixed(24));

        auto buttons = std::unique_ptr<DuiHBox>(new DuiHBox());
        buttons->SetGap(8);
        auto bOk = std::unique_ptr<DuiButton>(new DuiButton());
        bOk->SetText(_T("OK"));
        auto bCancel = std::unique_ptr<DuiButton>(new DuiButton());
        bCancel->SetText(_T("Cancel"));
        buttons->AddChild(std::move(bOk),     DuiLayout::Hint().Fixed(80));
        buttons->AddChild(std::move(bCancel), DuiLayout::Hint().Fixed(80));
        root->AddChild(std::move(buttons), DuiLayout::Hint().Fixed(28));

        row->AddChild(std::move(root), DuiLayout::Hint().Fixed(220));
        AddVariantRowCapture(page.get(), _T("host-tree"),
                             std::move(row), 80);
    }

    return page;
}

// =====================================================================
// Build_Layouts
//
// 5 个完整布局示例 — 每个对应 guides.html "完整布局示例" 章节里的
// 一节。每个布局是真 balloonui 控件组合（不是 mock），通过
// AddVariantRowCapture 注册截图位，--capture-all 模式生成
// layout-<name>.png 供文档使用。
//
// 设计原则：
//   * 用现成控件（DuiVBox/HBox/Splitter/Dock/Label/Button/EditHost）
//     拼接 — 不引入自绘控件，避免和第 8 章重复。
//   * 缺少专门控件时（如聊天气泡），用 DocColorTile 简单代替。
//   * 每个布局尺寸适中（~520×280 ~ 720×400），适合 doc 横排展示。
// =====================================================================

namespace {

// ---- 布局 #1：登录对话框 ---------------------------------------------
//
// 结构：
//   VBox padding=20 gap=10
//   ├─ DocColorTile          (logo, fixed 64)
//   ├─ Label "FlamingoNewUI" (fixed 30)
//   ├─ Label "登录账号"      (fixed 22, 灰色)
//   ├─ EditHost  username    (fixed 32)
//   ├─ EditHost  password    (fixed 32)
//   ├─ HBox: Checkbox 左 + Label 链接右   (fixed 24)
//   ├─ DuiButton "登录"      (fixed 36)
//   └─ Label "v1.0"         (fixed 18, 底部小字)
//
// 等价 XML（写在 guides.html 里的）：
//   <vbox padding="20" gap="10" fixedWidth="320">
//     <label text="FlamingoNewUI" fixedHeight="30"/>
//     ...
//
std::unique_ptr<DuiControl> BuildLayoutLogin()
{
    auto card = std::unique_ptr<DuiVBox>(new DuiVBox());
    card->SetPadding(20);
    card->SetGap(10);

    // Logo（用 DocColorTile 当临时品牌方块）
    auto logo = std::unique_ptr<DocColorTile>(
        new DocColorTile(_T("F"), RGB(50, 160, 110), RGB(255, 255, 255)));
    card->AddChild(std::move(logo), DuiLayout::Hint().Fixed(48));

    // 标题
    auto title = std::unique_ptr<DuiLabel>(new DuiLabel());
    title->SetText(_T("FlamingoNewUI"));
    title->SetTextColor(RGB(20, 20, 20));
    title->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    card->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    auto sub = std::unique_ptr<DuiLabel>(new DuiLabel());
    sub->SetText(_T("登录账号"));
    sub->SetTextColor(RGB(120, 120, 120));
    sub->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    card->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    // 用户名 + 密码
    auto eUser = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    eUser->SetPlaceholder(_T("用户名 / 邮箱"));
    card->AddChild(std::move(eUser), DuiLayout::Hint().Fixed(32));

    auto ePwd = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    ePwd->SetPlaceholder(_T("密码"));
    ePwd->SetPassword(true);
    card->AddChild(std::move(ePwd), DuiLayout::Hint().Fixed(32));

    // 复选框 + 链接 一行
    auto opts = std::unique_ptr<DuiHBox>(new DuiHBox());
    auto cb = std::unique_ptr<DuiButton>(new DuiButton());
    cb->SetButtonType(DuiButton::StyleCheckbox);
    cb->SetText(_T("记住我"));
    auto link = std::unique_ptr<DuiLabel>(new DuiLabel());
    link->SetText(_T("忘记密码？"));
    link->SetMode(DuiLabel::ModeLink);
    link->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    opts->AddChild(std::move(cb),   DuiLayout::Hint().Weight(1));
    opts->AddChild(std::move(link), DuiLayout::Hint().Fixed(80));
    card->AddChild(std::move(opts), DuiLayout::Hint().Fixed(24));

    // 登录按钮
    auto bLogin = std::unique_ptr<DuiButton>(new DuiButton());
    bLogin->SetText(_T("登录"));
    card->AddChild(std::move(bLogin), DuiLayout::Hint().Fixed(36));

    // 版本号
    auto ver = std::unique_ptr<DuiLabel>(new DuiLabel());
    ver->SetText(_T("v1.0   © 2026 FlamingoNewUI"));
    ver->SetTextColor(RGB(170, 170, 170));
    ver->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    card->AddChild(std::move(ver), DuiLayout::Hint().Fixed(18));

    return card;
}

// ---- 布局 #2：表单 ---------------------------------------------------
std::unique_ptr<DuiControl> BuildLayoutForm()
{
    auto col = std::unique_ptr<DuiVBox>(new DuiVBox());
    col->SetPadding(20);
    col->SetGap(12);

    auto title = std::unique_ptr<DuiLabel>(new DuiLabel());
    title->SetText(_T("个人资料"));
    title->SetTextColor(RGB(20, 20, 20));
    col->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    // helper：一行 "label + edit"
    auto makeRow = [](LPCTSTR labelText, LPCTSTR placeholder) {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->SetGap(8);
        auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
        l->SetText(labelText);
        l->SetTextColor(RGB( 60,  60,  60));
        l->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
        if (placeholder) e->SetPlaceholder(placeholder);
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        return row;
    };

    col->AddChild(makeRow(_T("姓名"),   _T("请输入姓名")),  DuiLayout::Hint().Fixed(28));
    col->AddChild(makeRow(_T("昵称"),   _T("可选")),       DuiLayout::Hint().Fixed(28));
    col->AddChild(makeRow(_T("邮箱"),   _T("name@example.com")), DuiLayout::Hint().Fixed(28));
    col->AddChild(makeRow(_T("电话"),   nullptr),          DuiLayout::Hint().Fixed(28));

    // 弹性空白 + 底部按钮组
    col->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                  DuiLayout::Hint().Weight(1));

    auto buttons = std::unique_ptr<DuiHBox>(new DuiHBox());
    buttons->SetGap(8);
    buttons->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
    auto bCancel = std::unique_ptr<DuiButton>(new DuiButton());
    bCancel->SetButtonType(DuiButton::StyleIcon);   // 浅底
    bCancel->SetText(_T("取消"));
    auto bOk = std::unique_ptr<DuiButton>(new DuiButton());
    bOk->SetText(_T("保存"));
    buttons->AddChild(std::move(bCancel), DuiLayout::Hint().Fixed(80));
    buttons->AddChild(std::move(bOk),     DuiLayout::Hint().Fixed(80));
    col->AddChild(std::move(buttons), DuiLayout::Hint().Fixed(32));

    return col;
}

// ---- 布局 #3：三列主窗 -----------------------------------------------
//
// 左侧 60px 图标导航 + 中间 240px 列表 + 右侧 flex 内容
// 用 HBox 而不是 Splitter（布局示意优先，拖动可后期开 Splitter）
std::unique_ptr<DuiControl> BuildLayoutThreePane()
{
    auto root = std::unique_ptr<DuiHBox>(new DuiHBox());

    // 左：60px 导航条（暗背景）
    auto rail = std::unique_ptr<DuiVBox>(new DuiVBox());
    rail->SetPadding(8);
    rail->SetGap(8);
    auto avatar = std::unique_ptr<DuiAvatar>(new DuiAvatar());
    avatar->SetName(_T("陆"));
    avatar->SetFallbackBgColor(RGB(50, 160, 110));
    rail->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(40));
    for (int i = 0; i < 4; ++i)
    {
        // 4 个图标方块占位（实际业务会是 DuiButton(StyleIcon) 或自绘）
        auto t = std::unique_ptr<DocColorTile>(
            new DocColorTile(_T(""), RGB(60, 65, 75)));
        rail->AddChild(std::move(t), DuiLayout::Hint().Fixed(36));
    }
    root->AddChild(std::move(rail), DuiLayout::Hint().Fixed(60));

    // 中：240 列表
    auto mid = std::unique_ptr<DuiVBox>(new DuiVBox());
    auto search = std::unique_ptr<DuiSearchBox>(new DuiSearchBox());
    search->SetPlaceholder(_T("搜索"));
    mid->AddChild(std::move(search), DuiLayout::Hint().Fixed(36));
    auto list = std::unique_ptr<DuiListBox>(new DuiListBox());
    list->SetItemHeight(48);
    list->AddItem(_T("苏文嘉"));
    list->AddItem(_T("# 设计周会"));
    list->AddItem(_T("李静"));
    list->AddItem(_T("F 前端基础设施"));
    list->AddItem(_T("陈聪"));
    list->SetCurSel(0, false);
    mid->AddChild(std::move(list), DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(mid), DuiLayout::Hint().Fixed(240));

    // 右：flex 内容（标题栏 + 内容占位 + 输入栏）
    auto right = std::unique_ptr<DuiVBox>(new DuiVBox());
    auto header = std::unique_ptr<DocColorTile>(
        new DocColorTile(_T(" 苏文嘉   ·   在线"), RGB(245, 245, 248)));
    right->AddChild(std::move(header), DuiLayout::Hint().Fixed(48));
    auto chatArea = std::unique_ptr<DocColorTile>(
        new DocColorTile(_T("(聊天内容区)"), RGB(252, 252, 252)));
    right->AddChild(std::move(chatArea), DuiLayout::Hint().Weight(1));
    auto composer = std::unique_ptr<DocColorTile>(
        new DocColorTile(_T("(输入区)"), RGB(248, 248, 250)));
    right->AddChild(std::move(composer), DuiLayout::Hint().Fixed(80));
    root->AddChild(std::move(right), DuiLayout::Hint().Weight(1));

    return root;
}

// ---- 布局 #4：设置页 -------------------------------------------------
std::unique_ptr<DuiControl> BuildLayoutSettings()
{
    auto root = std::unique_ptr<DuiHBox>(new DuiHBox());

    // 左侧导航：DuiListBox 当 vertical tab
    auto nav = std::unique_ptr<DuiListBox>(new DuiListBox());
    nav->SetItemHeight(32);
    nav->AddItem(_T("通用"));
    nav->AddItem(_T("账号与安全"));
    nav->AddItem(_T("消息与通知"));
    nav->AddItem(_T("隐私"));
    nav->AddItem(_T("聊天偏好"));
    nav->AddItem(_T("音频与视频"));
    nav->AddItem(_T("文件与存储"));
    nav->AddItem(_T("外观"));
    nav->SetCurSel(0, false);
    root->AddChild(std::move(nav), DuiLayout::Hint().Fixed(160));

    // 右侧内容：vbox 几个 section
    auto content = std::unique_ptr<DuiVBox>(new DuiVBox());
    content->SetPadding(20);
    content->SetGap(8);

    auto h = std::unique_ptr<DuiLabel>(new DuiLabel());
    h->SetText(_T("通用"));
    h->SetTextColor(RGB(20, 20, 20));
    content->AddChild(std::move(h), DuiLayout::Hint().Fixed(28));

    auto subH = std::unique_ptr<DuiLabel>(new DuiLabel());
    subH->SetText(_T("启动行为"));
    subH->SetTextColor(RGB(120, 120, 120));
    content->AddChild(std::move(subH), DuiLayout::Hint().Fixed(20));

    // 3 行 "标签 + 控件" 设置项
    auto makeOption = [](LPCTSTR label, std::unique_ptr<DuiControl> right) {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
        l->SetText(label);
        l->SetTextColor(RGB(40, 40, 40));
        row->AddChild(std::move(l), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(right), DuiLayout::Hint().Fixed(120));
        return row;
    };

    auto cb1 = std::unique_ptr<DuiButton>(new DuiButton());
    cb1->SetButtonType(DuiButton::StyleCheckbox);
    cb1->SetText(_T("启用"));
    cb1->SetCheck(true, false);
    content->AddChild(makeOption(_T("开机自动启动"), std::move(cb1)),
                      DuiLayout::Hint().Fixed(28));

    auto cb2 = std::unique_ptr<DuiButton>(new DuiButton());
    cb2->SetButtonType(DuiButton::StyleCheckbox);
    cb2->SetText(_T("启用"));
    content->AddChild(makeOption(_T("启动时静默运行"), std::move(cb2)),
                      DuiLayout::Hint().Fixed(28));

    auto cb3 = std::unique_ptr<DuiComboBox>(new DuiComboBox());
    cb3->AddString(_T("最小化到托盘"));
    cb3->AddString(_T("退出程序"));
    cb3->SetCurSel(0, false);
    content->AddChild(makeOption(_T("关闭主窗口时"), std::move(cb3)),
                      DuiLayout::Hint().Fixed(28));

    content->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));

    root->AddChild(std::move(content), DuiLayout::Hint().Weight(1));
    return root;
}

// ---- 布局 #5：窗口骨架（Dock）---------------------------------------
std::unique_ptr<DuiControl> BuildLayoutSkeleton()
{
    auto dock = std::unique_ptr<DuiDock>(new DuiDock());

    auto top = std::unique_ptr<DocColorTile>(
        new DocColorTile(_T(" 工具栏 (toolbar)"),
                         RGB(245, 245, 248)));
    dock->AddDocked(std::move(top), DuiDock::DockTop, 32);

    auto bottom = std::unique_ptr<DocColorTile>(
        new DocColorTile(_T(" 状态栏 (statusbar) — Ready"),
                         RGB(40, 110, 200), RGB(255, 255, 255)));
    dock->AddDocked(std::move(bottom), DuiDock::DockBottom, 24);

    auto left = std::unique_ptr<DocColorTile>(
        new DocColorTile(_T("Nav"),
                         RGB(245, 245, 248)));
    dock->AddDocked(std::move(left), DuiDock::DockLeft, 140);

    auto center = std::unique_ptr<DocColorTile>(
        new DocColorTile(_T("(主内容区)"),
                         RGB(252, 252, 252)));
    dock->AddDocked(std::move(center), DuiDock::DockFill);

    return dock;
}

} // anonymous

std::unique_ptr<DuiControl> Build_Layouts()
{
    auto page = NewPage();

    // 每个布局：标题 + 描述 + 真实 balloonui 渲染（带 capture mark）

    AddSection(page.get(), _T("layout-login"),
               _T("登录对话框：垂直居中卡片，Logo + 标题 + 用户/密码 + 复选 + 链接 + 按钮 + 版本号"));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        // 居中：左 weight + card fixed + 右 weight
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        row->AddChild(BuildLayoutLogin(), DuiLayout::Hint().Fixed(320));
        row->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        AddVariantRowCapture(page.get(), _T("layout-login"),
                             std::move(row), 360);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-form"),
               _T("表单：4 行 \"label 右对齐 + edit 弹性\" + 底部 取消/保存"));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(BuildLayoutForm(), DuiLayout::Hint().Fixed(480));
        AddVariantRowCapture(page.get(), _T("layout-form"),
                             std::move(row), 280);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-three-pane"),
               _T("三列主窗：60px 左导航 + 240px 中列表 + flex 右内容"));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(BuildLayoutThreePane(), DuiLayout::Hint().Fixed(720));
        AddVariantRowCapture(page.get(), _T("layout-three-pane"),
                             std::move(row), 320);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-settings"),
               _T("设置页：左侧 ListBox 导航 + 右侧 vbox 设置项（标签 + 控件）"));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(BuildLayoutSettings(), DuiLayout::Hint().Fixed(640));
        AddVariantRowCapture(page.get(), _T("layout-settings"),
                             std::move(row), 280);
    }
    AddGap(page.get(), kSectionGap);

    AddSection(page.get(), _T("layout-skeleton"),
               _T("窗口骨架：DuiDock 五区（top toolbar + bottom statusbar + left nav + center fill）"));
    {
        auto row = std::unique_ptr<DuiHBox>(new DuiHBox());
        row->AddChild(BuildLayoutSkeleton(), DuiLayout::Hint().Fixed(640));
        AddVariantRowCapture(page.get(), _T("layout-skeleton"),
                             std::move(row), 220);
    }

    return page;
}

} // namespace Pages
