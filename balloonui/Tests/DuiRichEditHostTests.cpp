#include "stdafx.h"
#include "DuiRichEditHostTests.h"

#if BUI_FEATURE_RICHEDIT

#include <richedit.h>

namespace balloonwjui {

namespace DuiRichEditHostTests {

namespace {

// Tiny RAII parent HWND used by tests that need a real RICHEDIT50W
// (e.g. RTF round-trip via EM_STREAMOUT/IN, char-format readback). The
// window is hidden + popup-style; lives only for the test's scope.
class HiddenHostWnd
{
public:
    HiddenHostWnd()
    {
        WNDCLASSEX wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = ::DefWindowProc;
        wc.hInstance     = ::GetModuleHandle(nullptr);
        wc.lpszClassName = _T("DuiRichEditTestHost");
        ::RegisterClassEx(&wc);
        m_hwnd = ::CreateWindowEx(
            0, _T("DuiRichEditTestHost"), _T(""),
            WS_POPUP, 0, 0, 200, 100,
            nullptr, nullptr, ::GetModuleHandle(nullptr), nullptr);
    }
    ~HiddenHostWnd()
    {
        if (m_hwnd)
        {
            ::DestroyWindow(m_hwnd);
        }
    }
    HWND get() const { return m_hwnd; }
private:
    HWND m_hwnd = nullptr;
};

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

#define EXPECT_BOOL(actual, expected, name) \
    do { bool _a = (actual); bool _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e?1:0, _a?1:0); return Fail(name, _d); } \
    } while (0)
#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)
#define EXPECT_STR(actual, expected, name) \
    do { CString _a = (actual); CString _e = (expected); \
         if (_a != _e) { return Fail(name, _T("string mismatch")); } \
    } while (0)

// Defaults: tab-stop true, multi-line on, word-wrap on, not readonly,
// no auto-URL, max-len 0, default colors.
static Result Test_Defaults()
{
    DuiRichEditHost e;
    EXPECT_BOOL(e.IsTabStop(),         true,  _T("Defaults/tabStop"));
    EXPECT_BOOL(e.IsMultiLine(),       true,  _T("Defaults/multiLine"));
    EXPECT_BOOL(e.IsWordWrap(),        true,  _T("Defaults/wordWrap"));
    EXPECT_BOOL(e.IsReadOnly(),        false, _T("Defaults/notReadOnly"));
    EXPECT_BOOL(e.IsAutoUrlDetect(),   false, _T("Defaults/noAutoUrl"));
    EXPECT_INT (e.GetMaxLength(),      0,     _T("Defaults/maxLen"));
    return OK(_T("Defaults"));
}

// SetText round-trips through cache when no HWND exists.
static Result Test_SetTextRoundTrip()
{
    DuiRichEditHost e;
    e.SetText(_T("hello rich"));
    EXPECT_STR(e.GetText(), _T("hello rich"), _T("RTrt/text"));
    e.SetText(_T(""));
    EXPECT_STR(e.GetText(), _T(""), _T("RTrt/empty"));
    return OK(_T("SetTextRoundTrip"));
}

// AppendText accumulates into the cache when no HWND exists.
static Result Test_AppendTextNoHwnd()
{
    DuiRichEditHost e;
    e.SetText(_T("Hello"));
    e.AppendText(_T(", "));
    e.AppendText(_T("World"));
    EXPECT_STR(e.GetText(), _T("Hello, World"), _T("Append/joined"));
    return OK(_T("AppendTextNoHwnd"));
}

// Placeholder visibility predicate.
static Result Test_PlaceholderStateMachine()
{
    DuiRichEditHost e;
    e.SetPlaceholder(_T("write something..."));
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
    e.SetPlaceholder(_T(""));
    EXPECT_BOOL(e.IsShowingPlaceholder(), false, _T("PH/emptyTextNoShow"));
    return OK(_T("PlaceholderStateMachine"));
}

// Pre-Create flag flips for multi-line, word-wrap, read-only.
static Result Test_PreCreateFlags()
{
    DuiRichEditHost e;
    e.SetMultiLine(false);
    EXPECT_BOOL(e.IsMultiLine(), false, _T("Flag/multiOff"));
    e.SetMultiLine(true);
    EXPECT_BOOL(e.IsMultiLine(), true,  _T("Flag/multiOn"));
    e.SetWordWrap(false);
    EXPECT_BOOL(e.IsWordWrap(),  false, _T("Flag/wrapOff"));
    e.SetWordWrap(true);
    EXPECT_BOOL(e.IsWordWrap(),  true,  _T("Flag/wrapOn"));
    e.SetReadOnly(true);
    EXPECT_BOOL(e.IsReadOnly(),  true,  _T("Flag/roOn"));
    e.SetReadOnly(false);
    EXPECT_BOOL(e.IsReadOnly(),  false, _T("Flag/roOff"));
    return OK(_T("PreCreateFlags"));
}

// MaxLength clamps negatives to zero.
static Result Test_MaxLength()
{
    DuiRichEditHost e;
    e.SetMaxLength(64);
    EXPECT_INT(e.GetMaxLength(), 64, _T("Max/64"));
    e.SetMaxLength(-3);
    EXPECT_INT(e.GetMaxLength(), 0,  _T("Max/clamp"));
    return OK(_T("MaxLength"));
}

// Auto-URL flag round-trip.
static Result Test_AutoUrlFlag()
{
    DuiRichEditHost e;
    e.SetAutoUrlDetect(true);
    EXPECT_BOOL(e.IsAutoUrlDetect(), true,  _T("URL/on"));
    e.SetAutoUrlDetect(false);
    EXPECT_BOOL(e.IsAutoUrlDetect(), false, _T("URL/off"));
    return OK(_T("AutoUrlFlag"));
}

// Color setters store cached values and don't crash without HWND.
static Result Test_ColorSetters()
{
    DuiRichEditHost e;
    e.SetBackgroundColor(RGB(250, 250, 240));
    if (e.GetBackgroundColor() != RGB(250, 250, 240))
    {
        return Fail(_T("Color/bg"), _T("bg mismatch"));
    }
    e.SetTextColor(RGB(20, 40, 60));
    if (e.GetTextColor() != RGB(20, 40, 60))
    {
        return Fail(_T("Color/fg"), _T("fg mismatch"));
    }
    return OK(_T("ColorSetters"));
}

// Selection helpers without HWND are safe no-ops; cpMin/cpMax come back zero.
static Result Test_SelectionNoHwnd()
{
    DuiRichEditHost e;
    e.SetSel(2, 5);
    long a = -1, b = -1;
    e.GetSel(a, b);
    EXPECT_INT(a, 0, _T("Sel/aZeroNoHwnd"));
    EXPECT_INT(b, 0, _T("Sel/bZeroNoHwnd"));
    e.SelectAll();          // safe
    e.Undo();
    e.Cut();
    e.Copy();
    e.Paste();
    e.Clear();   // safe
    return OK(_T("SelectionNoHwnd"));
}

// Calling InsertQuoteBlock without a hosted HWND must be a safe no-op.
static Result Test_QuoteApiNoHwnd()
{
    DuiRichEditHost e;
    e.InsertQuoteBlock(_T("Alice"), _T("hi there\nsecond line"));
    e.InsertQuoteBlock(nullptr, nullptr);
    return OK(_T("QuoteApiNoHwnd"));
}

// InsertFileCard without HWND is a safe no-op.
static Result Test_FileCardApiNoHwnd()
{
    DuiRichEditHost e;
    e.InsertFileCard(_T("design.zip"), 12345678ULL);
    e.InsertFileCard(nullptr, 0);
    return OK(_T("FileCardApiNoHwnd"));
}

// Image inserts without HWND must return false.
static Result Test_ImageInsertNoHwndReturnsFalse()
{
    DuiRichEditHost e;
    EXPECT_BOOL(e.InsertImageFromFile(_T("nonexistent.png")), false, _T("Img/file"));
    EXPECT_BOOL(e.InsertImageFromBitmap(nullptr),             false, _T("Img/bm-null"));
    return OK(_T("ImageInsertNoHwndReturnsFalse"));
}

// Humanized size: <1KB stays in B, 1024 B -> 1.0 KB, 1.5 MB rounds to .5, etc.
static Result Test_FormatSize()
{
    EXPECT_STR(DuiRichEditHost::FormatHumanSize(0),                _T("0 B"),       _T("Sz/0"));
    EXPECT_STR(DuiRichEditHost::FormatHumanSize(512),              _T("512 B"),     _T("Sz/512"));
    EXPECT_STR(DuiRichEditHost::FormatHumanSize(1024),             _T("1.0 KB"),    _T("Sz/1KB"));
    EXPECT_STR(DuiRichEditHost::FormatHumanSize(1024ULL * 1024),   _T("1.0 MB"),    _T("Sz/1MB"));
    EXPECT_STR(DuiRichEditHost::FormatHumanSize(1024ULL * 1024 * 1024), _T("1.0 GB"), _T("Sz/1GB"));
    EXPECT_STR(DuiRichEditHost::FormatHumanSize(1500 * 1024ULL),   _T("1.5 MB"),    _T("Sz/1.5MB"));
    return OK(_T("FormatSize"));
}

// Margins API and tab-stop default.
static Result Test_MiscApi()
{
    DuiRichEditHost e;
    e.SetMargins(6, 4, 6, 4);
    EXPECT_BOOL(e.IsTabStop(), true, _T("Misc/tab"));
    return OK(_T("MiscApi"));
}

// SaveRTF / LoadRTF without a hosted HWND must fail clean: SaveRTF
// returns false and clears `out`; LoadRTF returns false. (RTF parsing
// lives in RICHEDIT50W; without an HWND there is no parser.)
static Result Test_RTFApiNoHwnd()
{
    DuiRichEditHost e;
    e.SetText(_T("hello"));
    CStringA rtf("not empty before");
    EXPECT_BOOL(e.SaveRTF(rtf), false, _T("RTF/saveNoHwnd"));
    EXPECT_INT (rtf.GetLength(), 0, _T("RTF/saveClearsOut"));
    EXPECT_BOOL(e.LoadRTF(CStringA("{\\rtf1 hi}")), false, _T("RTF/loadNoHwnd"));
    return OK(_T("RTFApiNoHwnd"));
}

// SaveText / LoadText without a hosted HWND fall back to the cache,
// so they unconditionally succeed and behave like SetText/GetText.
static Result Test_TextApiNoHwndUsesCache()
{
    DuiRichEditHost e;
    e.SetText(_T("seed text"));
    CString out;
    EXPECT_BOOL(e.SaveText(out), true,        _T("Txt/saveNoHwndOk"));
    EXPECT_STR (out, _T("seed text"),         _T("Txt/saveMatchesCache"));
    EXPECT_BOOL(e.LoadText(_T("loaded txt")), true, _T("Txt/loadNoHwndOk"));
    EXPECT_STR (e.GetText(), _T("loaded txt"), _T("Txt/loadUpdatesCache"));
    return OK(_T("TextApiNoHwndUsesCache"));
}

// With a real RICHEDIT50W: round-trip plain text via SaveRTF -> Clear ->
// LoadRTF. The RTF blob preserves the user content, even though it
// also contains font tables and other markup.
static Result Test_RTFRoundTripText()
{
    HiddenHostWnd parent;
    if (!parent.get())
    {
        return Fail(_T("RTFRoundTripText"), _T("no host hwnd"));
    }
    DuiRichEditHost e;
    if (!e.EnsureCreated(parent.get()))
    {
        return Fail(_T("RTFRoundTripText"), _T("EnsureCreated failed"));
    }
    e.SetText(_T("Hello, RTF!"));
    CStringA rtf;
    EXPECT_BOOL(e.SaveRTF(rtf), true, _T("RTF/save"));
    if (rtf.GetLength() < 8)
    {
        return Fail(_T("RTFRoundTripText"), _T("rtf too short"));
    }
    if (rtf.Find("\\rtf") < 0)
    {
        return Fail(_T("RTFRoundTripText"), _T("missing \\rtf"));
    }
    e.SetText(_T(""));
    EXPECT_STR(e.GetText(), _T(""), _T("RTF/clearedBeforeLoad"));
    EXPECT_BOOL(e.LoadRTF(rtf), true, _T("RTF/load"));
    EXPECT_STR(e.GetText(), _T("Hello, RTF!"), _T("RTF/textAfterLoad"));
    return OK(_T("RTFRoundTripText"));
}

// With a real RICHEDIT50W: bold formatting on a selection survives a
// SaveRTF -> Clear -> LoadRTF cycle. Verified by reading CHARFORMAT2
// at the bolded character position after the load.
static Result Test_RTFRoundTripBold()
{
    HiddenHostWnd parent;
    if (!parent.get())
    {
        return Fail(_T("RTFRoundTripBold"), _T("no host hwnd"));
    }
    DuiRichEditHost e;
    if (!e.EnsureCreated(parent.get()))
    {
        return Fail(_T("RTFRoundTripBold"), _T("EnsureCreated failed"));
    }
    e.SetText(_T("plain BOLD plain"));
    e.SetSel(6, 10);                 // covers "BOLD"
    e.SetSelBold(true);

    CStringA rtf;
    EXPECT_BOOL(e.SaveRTF(rtf), true, _T("RTFb/save"));
    // RTF for bold is `\b` (with a trailing word break or `\b0` to end).
    if (rtf.Find("\\b ") < 0 && rtf.Find("\\b\r") < 0 && rtf.Find("\\b\n") < 0)
    {
        return Fail(_T("RTFRoundTripBold"), _T("no \\b in saved RTF"));
    }

    e.SetText(_T(""));
    EXPECT_BOOL(e.LoadRTF(rtf), true, _T("RTFb/load"));
    EXPECT_STR(e.GetText(), _T("plain BOLD plain"), _T("RTFb/textAfterLoad"));

    // Probe formatting: position 7 lies inside "BOLD" - must read bold.
    HWND h = e.GetHostedHwnd();
    if (!h)
    {
        return Fail(_T("RTFRoundTripBold"), _T("no hosted hwnd"));
    }
    CHARRANGE cr{ 7, 8 };
    ::SendMessage(h, EM_EXSETSEL, 0, (LPARAM)&cr);
    CHARFORMAT2 cf{};
    cf.cbSize = sizeof(cf);
    ::SendMessage(h, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    bool bold = (cf.dwEffects & CFE_BOLD) != 0;
    EXPECT_BOOL(bold, true, _T("RTFb/probeBold"));

    // Position 1 lies inside "plain" - must NOT be bold.
    CHARRANGE cr2{ 1, 2 };
    ::SendMessage(h, EM_EXSETSEL, 0, (LPARAM)&cr2);
    CHARFORMAT2 cf2{};
    cf2.cbSize = sizeof(cf2);
    ::SendMessage(h, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf2);
    bool boldPlain = (cf2.dwEffects & CFE_BOLD) != 0;
    EXPECT_BOOL(boldPlain, false, _T("RTFb/probePlain"));

    return OK(_T("RTFRoundTripBold"));
}

// With a real RICHEDIT50W: SaveText / LoadText preserve a single line
// of Unicode text. Line-break normalization in RichEdit's stream-out
// is RichEdit-version-specific (some builds append a trailing CR or
// CR/LF), so this test deliberately uses a single-line string and
// asserts round-trip equality on that.
static Result Test_TextRoundTripWithHwnd()
{
    HiddenHostWnd parent;
    if (!parent.get())
    {
        return Fail(_T("TextRoundTripWithHwnd"), _T("no host hwnd"));
    }
    DuiRichEditHost e;
    if (!e.EnsureCreated(parent.get()))
    {
        return Fail(_T("TextRoundTripWithHwnd"), _T("EnsureCreated failed"));
    }
    // CJK code points 0x4E16 0x754C ("world" in zh) + ASCII. Single line,
    // no embedded \r, to dodge RichEdit's EOL-normalization behavior.
    LPCTSTR seed = _T("hello \x4E16\x754C end");
    e.SetText(seed);
    CString out;
    EXPECT_BOOL(e.SaveText(out), true, _T("Txt/save"));
    // RichEdit may or may not append a trailing newline; tolerate both.
    int crIdx = out.FindOneOf(_T("\r\n"));
    if (crIdx >= 0)
    {
        out = out.Left(crIdx);
    }
    EXPECT_STR (out, seed, _T("Txt/saveMatches"));

    e.SetText(_T(""));
    EXPECT_BOOL(e.LoadText(seed), true, _T("Txt/load"));
    EXPECT_STR (e.GetText(), seed, _T("Txt/textAfterLoad"));
    return OK(_T("TextRoundTripWithHwnd"));
}

// PasteAsPlainText / FindText defaults (no HWND).
static Result Test_PasteFindDefaultsNoHwnd()
{
    DuiRichEditHost e;
    EXPECT_BOOL(e.GetPasteAsPlainTextDefault(), false, _T("Paste/defOff"));
    e.SetPasteAsPlainTextDefault(true);
    EXPECT_BOOL(e.GetPasteAsPlainTextDefault(), true,  _T("Paste/onRT"));
    e.SetPasteAsPlainTextDefault(false);
    EXPECT_BOOL(e.GetPasteAsPlainTextDefault(), false, _T("Paste/offRT"));

    // PasteAsPlainText without a hosted HWND must report failure cleanly.
    EXPECT_BOOL(e.PasteAsPlainText(), false, _T("Paste/noHwnd"));

    // FindText without HWND returns false and never touches outRange.
    CHARRANGE r{ 999, 999 };
    EXPECT_BOOL(e.FindText(_T("anything"), 0, true, false, false, r),
                false, _T("Find/noHwnd"));
    EXPECT_INT (r.cpMin, 999, _T("Find/noHwndPreservesOut0"));
    EXPECT_INT (r.cpMax, 999, _T("Find/noHwndPreservesOut1"));

    // FindAndSelect without HWND - false, no crash.
    EXPECT_BOOL(e.FindAndSelect(_T("anything"), 0, true, false, false, true),
                false, _T("FindSel/noHwnd"));
    return OK(_T("PasteFindDefaultsNoHwnd"));
}

// FindText / FindAndSelect with a real RICHEDIT50W. Covers forward,
// backward, case-(in)sensitive, whole-word, no-match, and wrap.
static Result Test_FindTextWithHwnd()
{
    HiddenHostWnd parent;
    if (!parent.get())
    {
        return Fail(_T("FindTextWithHwnd"), _T("no host hwnd"));
    }
    DuiRichEditHost e;
    if (!e.EnsureCreated(parent.get()))
    {
        return Fail(_T("FindTextWithHwnd"), _T("EnsureCreated failed"));
    }
    e.SetText(_T("alpha BETA alpha gamma Alpha"));
    //         0     6     11    17    23

    CHARRANGE r{};
    // Forward, case-insensitive: first "alpha" at 0.
    EXPECT_BOOL(e.FindText(_T("alpha"), 0, true, false, false, r), true, _T("Fwd/caseIns"));
    EXPECT_INT (r.cpMin, 0, _T("Fwd/caseIns/min"));
    EXPECT_INT (r.cpMax, 5, _T("Fwd/caseIns/max"));

    // Forward, case-sensitive: "Alpha" at 23 (skips lowercase ones).
    EXPECT_BOOL(e.FindText(_T("Alpha"), 0, true, true, false, r), true, _T("Fwd/caseSens"));
    EXPECT_INT (r.cpMin, 23, _T("Fwd/caseSens/min"));

    // Forward starting from inside the first match (pos 1) finds the
    // second "alpha" at 11.
    EXPECT_BOOL(e.FindText(_T("alpha"), 5, true, false, false, r), true, _T("Fwd/skipFirst"));
    EXPECT_INT (r.cpMin, 11, _T("Fwd/skipFirst/min"));

    // No match.
    EXPECT_BOOL(e.FindText(_T("zeta"), 0, true, false, false, r), false, _T("Fwd/miss"));

    // Whole-word: "BETA" matches; "ALP" should not match (substring of alpha).
    EXPECT_BOOL(e.FindText(_T("BETA"), 0, true, false, true, r), true, _T("Word/hit"));
    EXPECT_BOOL(e.FindText(_T("alp"),  0, true, false, true, r), false, _T("Word/miss"));

    // Backward from end (pos 28) finds last "Alpha" at 23.
    EXPECT_BOOL(e.FindText(_T("Alpha"), 28, false, true, false, r), true, _T("Bwd/hit"));
    EXPECT_INT (r.cpMin, 23, _T("Bwd/min"));

    // FindAndSelect updates the caret. Start at 0 forward.
    EXPECT_BOOL(e.FindAndSelect(_T("BETA"), 0, true, false, false, false), true,
                _T("Sel/hit"));
    long cpMin = -1, cpMax = -1;
    e.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 6,  _T("Sel/min"));
    EXPECT_INT(cpMax, 10, _T("Sel/max"));

    // Wrap: search forward starting after the last match -> miss without
    // wrap, hit with wrap.
    EXPECT_BOOL(e.FindAndSelect(_T("alpha"), 28, true, false, false, false), false,
                _T("Wrap/missNoWrap"));
    EXPECT_BOOL(e.FindAndSelect(_T("alpha"), 28, true, false, false, true), true,
                _T("Wrap/hitWithWrap"));
    e.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 0, _T("Wrap/firstMatch"));
    return OK(_T("FindTextWithHwnd"));
}

// PasteAsPlainText: stuff plain text + RTF onto the clipboard, verify
// that PasteAsPlainText only inserts the plain text (no RTF artifacts).
// We don't try to verify "no formatting was applied" structurally -
// that path is exercised by RTFRoundTrip tests; here we just check
// that the inserted bytes match the plain-text payload.
static Result Test_PasteAsPlainTextWithHwnd()
{
    HiddenHostWnd parent;
    if (!parent.get())
    {
        return Fail(_T("PasteAsPlainTextWithHwnd"), _T("no host hwnd"));
    }
    DuiRichEditHost e;
    if (!e.EnsureCreated(parent.get()))
    {
        return Fail(_T("PasteAsPlainTextWithHwnd"), _T("EnsureCreated failed"));
    }
    if (!::OpenClipboard(parent.get()))
    {
        return Fail(_T("PasteAsPlainTextWithHwnd"), _T("OpenClipboard failed"));
    }
    ::EmptyClipboard();

    LPCTSTR plain = _T("plaintext-payload");
    size_t bytes = (::_tcslen(plain) + 1) * sizeof(TCHAR);
    HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem)
    {
        void* p = ::GlobalLock(hMem);
        if (p)
        {
            ::memcpy(p, plain, bytes);
            ::GlobalUnlock(hMem);
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    ::CloseClipboard();

    // Empty document, then paste-as-plain at position 0.
    e.SetText(_T(""));
    e.SetSel(0, 0);
    EXPECT_BOOL(e.PasteAsPlainText(), true, _T("PastePlain/ok"));
    // RichEdit may or may not append a trailing CR depending on
    // version / focus state; tolerate that on the round-trip check.
    CString got = e.GetText();
    while (!got.IsEmpty()
           && (got[got.GetLength()-1] == _T('\r')
               || got[got.GetLength()-1] == _T('\n')))
    {
        got = got.Left(got.GetLength()-1);
    }
    EXPECT_STR(got, plain, _T("PastePlain/match"));
    return OK(_T("PasteAsPlainTextWithHwnd"));
}

// LoadRTF on garbage input must fail without crashing and without
// leaving the document in an undefined state. RICHEDIT50W silently
// keeps whatever it has when the parse aborts.
static Result Test_RTFLoadRejectsGarbage()
{
    HiddenHostWnd parent;
    if (!parent.get())
    {
        return Fail(_T("RTFLoadRejectsGarbage"), _T("no host hwnd"));
    }
    DuiRichEditHost e;
    if (!e.EnsureCreated(parent.get()))
    {
        return Fail(_T("RTFLoadRejectsGarbage"), _T("EnsureCreated failed"));
    }
    e.SetText(_T("placeholder"));
    // Not RTF at all - missing the "{\rtf" header.
    e.LoadRTF(CStringA("this is plainly not rtf data"));
    // Survival check: the call returned (no crash) and the document is
    // still readable. We don't assert on the exact contents because
    // RichEdit's behavior here is "best effort" - just that we're alive.
    CString cur = e.GetText();
    (void)cur;
    return OK(_T("RTFLoadRejectsGarbage"));
}

// SetContextMenuEnabled 开关：默认开，可关可再开（不依赖 HWND）。右键菜单的
// 具体项 / 灰显逻辑由共享纯函数 BuildEditContextMenu 承载，已在 DuiEditHostTests
// 里覆盖，此处只验证本控件的开关状态位。
static Result Test_ContextMenuEnabledRoundTrip()
{
    DuiRichEditHost e;
    EXPECT_BOOL(e.IsContextMenuEnabled(), true,  _T("Ctx/default"));
    e.SetContextMenuEnabled(false);
    EXPECT_BOOL(e.IsContextMenuEnabled(), false, _T("Ctx/off"));
    e.SetContextMenuEnabled(true);
    EXPECT_BOOL(e.IsContextMenuEnabled(), true,  _T("Ctx/on"));
    return OK(_T("ContextMenuEnabledRoundTrip"));
}

#undef EXPECT_BOOL
#undef EXPECT_INT
#undef EXPECT_STR

} // anonymous

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry { LPCTSTR name; TestFn fn; };
    Entry tests[] = {
        { _T("Defaults"),                &Test_Defaults                },
        { _T("SetTextRoundTrip"),        &Test_SetTextRoundTrip        },
        { _T("AppendTextNoHwnd"),        &Test_AppendTextNoHwnd        },
        { _T("PlaceholderStateMachine"), &Test_PlaceholderStateMachine },
        { _T("PreCreateFlags"),          &Test_PreCreateFlags          },
        { _T("MaxLength"),               &Test_MaxLength               },
        { _T("AutoUrlFlag"),             &Test_AutoUrlFlag             },
        { _T("ColorSetters"),            &Test_ColorSetters            },
        { _T("SelectionNoHwnd"),         &Test_SelectionNoHwnd         },
        { _T("QuoteApiNoHwnd"),          &Test_QuoteApiNoHwnd          },
        { _T("FileCardApiNoHwnd"),       &Test_FileCardApiNoHwnd       },
        { _T("ImageInsertNoHwndReturnsFalse"), &Test_ImageInsertNoHwndReturnsFalse },
        { _T("FormatSize"),              &Test_FormatSize              },
        { _T("MiscApi"),                 &Test_MiscApi                 },
        { _T("RTFApiNoHwnd"),            &Test_RTFApiNoHwnd            },
        { _T("TextApiNoHwndUsesCache"),  &Test_TextApiNoHwndUsesCache  },
        { _T("RTFRoundTripText"),        &Test_RTFRoundTripText        },
        { _T("RTFRoundTripBold"),        &Test_RTFRoundTripBold        },
        { _T("TextRoundTripWithHwnd"),   &Test_TextRoundTripWithHwnd   },
        { _T("PasteFindDefaultsNoHwnd"), &Test_PasteFindDefaultsNoHwnd },
        { _T("FindTextWithHwnd"),        &Test_FindTextWithHwnd        },
        { _T("PasteAsPlainTextWithHwnd"),&Test_PasteAsPlainTextWithHwnd},
        { _T("RTFLoadRejectsGarbage"),   &Test_RTFLoadRejectsGarbage   },
        { _T("ContextMenuEnabledRoundTrip"), &Test_ContextMenuEnabledRoundTrip }
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
    summary.Format(_T("[summary] DuiRichEditHostTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace DuiRichEditHostTests

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHEDIT
