#include "stdafx.h"
#include "ChatControls.h"

#include "../balloonui/DuiResMgr.h"
#include "../balloonui/DuiPaintAA.h"
#include "../balloonui/Controls/Basic/DuiAvatar.h"

namespace newchatdemo {

using balloonwjui::DuiAA::FillEllipse;
using balloonwjui::DuiAA::FillPolygon;
using balloonwjui::DuiAA::DrawLine;

// ===== shared paint helpers ============================================

namespace {

// Default UI font shared by all custom controls — same font as the rest
// of the DUI library so the demo blends with the kernel widgets.
HFONT DefaultFont()
{
    return balloonwjui::DuiResMgr::Inst().GetDefaultFont();
}

// Build a font on demand. Caller must DeleteObject when done.
HFONT MakeFont(int pt, bool bold = false)
{
    LOGFONT lf{};
    HDC hdc = ::GetDC(nullptr);
    int dpi = ::GetDeviceCaps(hdc, LOGPIXELSY);
    ::ReleaseDC(nullptr, hdc);
    lf.lfHeight  = -MulDiv(pt, dpi, 72);
    lf.lfWeight  = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = GB2312_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, _T("Microsoft YaHei"));
    return ::CreateFontIndirect(&lf);
}

void FillRectColor(HDC hdc, const RECT& rc, COLORREF clr)
{
    HBRUSH br = ::CreateSolidBrush(clr);
    ::FillRect(hdc, &rc, br);
    ::DeleteObject(br);
}

void DrawHLine(HDC hdc, int x1, int x2, int y, COLORREF clr)
{
    RECT r = { x1, y, x2, y + 1 };
    FillRectColor(hdc, r, clr);
}

void DrawText_(HDC hdc, LPCTSTR sz, const RECT& rc, COLORREF clr,
               UINT flags, HFONT font = nullptr)
{
    HFONT old = font ? (HFONT)::SelectObject(hdc, font) : nullptr;
    int oldBk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oldClr = ::SetTextColor(hdc, clr);
    RECT r = rc;
    ::DrawText(hdc, sz, -1, &r, flags);
    ::SetTextColor(hdc, oldClr);
    ::SetBkMode(hdc, oldBk);
    if (font)
    {
        ::SelectObject(hdc, old);
    }
}

// Compute a rounded-rect path that "squares" one corner. side picks
// which corner is squared (-1 none, 0 top-left, 1 top-right, 2 bottom-
// right, 3 bottom-left). Used for chat bubbles' "ear" corner.
void FillBubble(HDC hdc, const RECT& rc, COLORREF fill, COLORREF border,
                int radius, int sharpCorner)
{
    HBRUSH brFill = ::CreateSolidBrush(fill);
    HBRUSH oldBr  = (HBRUSH)::SelectObject(hdc, brFill);
    HPEN   penBd  = (border == CLR_INVALID)
                    ? (HPEN)::GetStockObject(NULL_PEN)
                    : ::CreatePen(PS_SOLID, 1, border);
    HPEN   oldPen = (HPEN)::SelectObject(hdc, penBd);

    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom,
                radius * 2, radius * 2);

    if (sharpCorner >= 0)
    {
        const int sq = radius;
        RECT corner = { 0, 0, 0, 0 };
        switch (sharpCorner)
        {
            case 0:
                corner = { rc.left,       rc.top,         rc.left + sq, rc.top + sq };
                break;
            case 1:
                corner = { rc.right - sq, rc.top,         rc.right,     rc.top + sq };
                break;
            case 2:
                corner = { rc.right - sq, rc.bottom - sq, rc.right,     rc.bottom };
                break;
            case 3:
                corner = { rc.left,       rc.bottom - sq, rc.left + sq, rc.bottom };
                break;
        }
        FillRectColor(hdc, corner, fill);
        if (border != CLR_INVALID)
        {
            switch (sharpCorner)
            {
                case 0:
                {
                    DrawHLine(hdc, corner.left, corner.right, corner.top, border);
                    RECT vr = { corner.left, corner.top, corner.left + 1, corner.bottom };
                    FillRectColor(hdc, vr, border);
                    break;
                }
                case 1:
                {
                    DrawHLine(hdc, corner.left, corner.right, corner.top, border);
                    RECT vr = { corner.right - 1, corner.top, corner.right, corner.bottom };
                    FillRectColor(hdc, vr, border);
                    break;
                }
                case 2:
                {
                    DrawHLine(hdc, corner.left, corner.right, corner.bottom - 1, border);
                    RECT vr = { corner.right - 1, corner.top, corner.right, corner.bottom };
                    FillRectColor(hdc, vr, border);
                    break;
                }
                case 3:
                {
                    DrawHLine(hdc, corner.left, corner.right, corner.bottom - 1, border);
                    RECT vr = { corner.left, corner.top, corner.left + 1, corner.bottom };
                    FillRectColor(hdc, vr, border);
                    break;
                }
            }
        }
    }

    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(brFill);
    if (border != CLR_INVALID)
    {
        ::DeleteObject(penBd);
    }
}

// Paint an avatar tile with initial char(s) on a hue-tinted square with
// rounded corners. Used by every "small avatar" location in the design
// (rail profile, conv list rows, message rows, info panel hero).
void PaintAvatarTile(HDC hdc, const RECT& rc, int hue,
                     LPCTSTR initial, int fontPt, int radius)
{
    COLORREF bg = AvatarColorByHue(hue);
    HBRUSH br = ::CreateSolidBrush(bg);
    HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, br);
    HPEN penNull = (HPEN)::GetStockObject(NULL_PEN);
    HPEN oldPen  = (HPEN)::SelectObject(hdc, penNull);
    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(br);

    if (initial && *initial)
    {
        HFONT f = MakeFont(fontPt, true);
        DrawText_(hdc, initial, rc, kInkInv,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, f);
        ::DeleteObject(f);
    }
}

// Status dot in the bottom-right corner of an avatar.
// status: 1=online, 2=away, 3=offline.
void PaintAvatarStatusDot(HDC hdc, const RECT& rcAvatar, int status,
                          COLORREF ringColor)
{
    if (status == 0)
    {
        return;
    }
    int dia = 10;
    RECT outer = { rcAvatar.right - dia,
                   rcAvatar.bottom - dia,
                   rcAvatar.right,
                   rcAvatar.bottom };
    COLORREF dot;
    if (status == 1)
    {
        dot = kOnline;
    }
    else if (status == 2)
    {
        dot = kAmber;
    }
    else
    {
        dot = kInk4;
    }
    FillEllipse(hdc, outer, ringColor);
    RECT inner = { outer.left + 2,  outer.top + 2,
                   outer.right - 2, outer.bottom - 2 };
    FillEllipse(hdc, inner, dot);
}

void PaintRoundChip(HDC hdc, const RECT& rc, COLORREF fill, int radius)
{
    HBRUSH br = ::CreateSolidBrush(fill);
    HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, br);
    HPEN penNull = (HPEN)::GetStockObject(NULL_PEN);
    HPEN oldPen  = (HPEN)::SelectObject(hdc, penNull);
    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(br);
}

SIZE MeasureText(HDC hdc, LPCTSTR sz, HFONT font)
{
    SIZE out{};
    HDC mdc = ::CreateCompatibleDC(hdc);
    HFONT old = (HFONT)::SelectObject(mdc, font);
    ::GetTextExtentPoint32(mdc, sz, (int)_tcslen(sz), &out);
    ::SelectObject(mdc, old);
    ::DeleteDC(mdc);
    return out;
}

} // anonymous

// ===== RailItem ========================================================

void RailItem::OnPaint(HDC hdc, const RECT&)
{
    RECT inner = m_rcItem;

    if (m_active)
    {
        PaintRoundChip(hdc, inner, kBgActive, 6);

        // Left accent strip (3px x 18px).
        int sy = inner.top + (inner.bottom - inner.top - 18) / 2;
        RECT strip = { inner.left - 6, sy, inner.left - 3, sy + 18 };
        FillRectColor(hdc, strip, kBrandDeep);
    }

    COLORREF glyphClr = m_active ? kBrandDeep : kInk3;
    if (!m_glyph.IsEmpty())
    {
        HFONT f = MakeFont(11, true);
        DrawText_(hdc, m_glyph, inner, glyphClr,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, f);
        ::DeleteObject(f);
    }

    if (m_badge == 0)
    {
        int d = 8;
        RECT b = { inner.right - 6 - d, inner.top + 4,
                   inner.right - 6,     inner.top + 4 + d };
        FillEllipse(hdc, b, kRed);
    }
    else if (m_badge > 0)
    {
        TCHAR buf[16];
        if (m_badge > 99)
        {
            _tcscpy_s(buf, _T("99+"));
        }
        else
        {
            _stprintf_s(buf, _T("%d"), m_badge);
        }

        HFONT f = MakeFont(8, true);
        SIZE sz = MeasureText(hdc, buf, f);

        int chipH = 14;
        int chipW = sz.cx + 8;
        if (chipW < chipH)
        {
            chipW = chipH;
        }
        RECT chip = { inner.right - 4 - chipW, inner.top + 2,
                      inner.right - 4,         inner.top + 2 + chipH };
        PaintRoundChip(hdc, chip, kRed, chipH / 2);
        DrawText_(hdc, buf, chip, kInkInv,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, f);
        ::DeleteObject(f);
    }
}

bool RailItem::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    return true;
}

// ===== DayPill =========================================================

void DayPill::OnPaint(HDC hdc, const RECT&)
{
    if (m_text.IsEmpty())
    {
        return;
    }

    HFONT f = MakeFont(8);
    SIZE sz = MeasureText(hdc, m_text, f);

    int padX  = 10;
    int pillW = sz.cx + padX * 2;
    int pillH = m_rcItem.bottom - m_rcItem.top;
    int x0    = m_rcItem.left + ((m_rcItem.right - m_rcItem.left) - pillW) / 2;
    int y0    = m_rcItem.top;
    RECT pill = { x0, y0, x0 + pillW, y0 + pillH };

    PaintRoundChip(hdc, pill, kBgSidebar2, pillH / 2);
    DrawText_(hdc, m_text, pill, kInk4,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, f);
    ::DeleteObject(f);
}

// ===== ConvRow =========================================================

void ConvRow::SetData(LPCTSTR name, LPCTSTR msg, LPCTSTR time,
                      int hue, LPCTSTR initial, int unread,
                      bool active, bool pinned, bool muted,
                      bool typing, bool draft, bool isGroup,
                      int  status)
{
    m_name    = name;
    m_msg     = msg;
    m_time    = time;
    m_hue     = hue;
    m_initial = initial;
    m_unread  = unread;
    m_active  = active;
    m_pinned  = pinned;
    m_muted   = muted;
    m_typing  = typing;
    m_draft   = draft;
    m_isGroup = isGroup;
    m_status  = status;
    Invalidate();
}

void ConvRow::OnPaint(HDC hdc, const RECT&)
{
    COLORREF bg;
    if (m_active)
    {
        bg = kBgActive;
    }
    else if (m_hover)
    {
        bg = kBgHover;
    }
    else
    {
        bg = kBgSidebar;
    }
    FillRectColor(hdc, m_rcItem, bg);

    // Avatar (36x36) at left.
    const int padL = 12;
    const int padT = 11;
    RECT av = { m_rcItem.left + padL,        m_rcItem.top + padT,
                m_rcItem.left + padL + 36,   m_rcItem.top + padT + 36 };
    PaintAvatarTile(hdc, av, m_hue, m_initial, 12, 6);
    if (m_status > 0)
    {
        PaintAvatarStatusDot(hdc, av, m_status, bg);
    }

    int bodyL   = av.right + 10;
    int bodyR   = m_rcItem.right - 10;
    int unreadW = 0;

    // Unread chip.
    if (m_unread > 0)
    {
        TCHAR buf[16];
        if (m_unread > 99)
        {
            _tcscpy_s(buf, _T("99+"));
        }
        else
        {
            _stprintf_s(buf, _T("%d"), m_unread);
        }

        HFONT f = MakeFont(8, true);
        SIZE sz = MeasureText(hdc, buf, f);
        int chipH = 16;
        int chipW = sz.cx + 10;
        if (chipW < chipH)
        {
            chipW = chipH;
        }
        RECT chip = { bodyR - chipW, m_rcItem.top + 12,
                      bodyR,         m_rcItem.top + 12 + chipH };
        PaintRoundChip(hdc, chip, m_muted ? kInk4 : kRed, chipH / 2);
        DrawText_(hdc, buf, chip, kInkInv,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, f);
        ::DeleteObject(f);
        unreadW = chipW + 6;
    }

    // Time text (top-right, left of unread chip).
    {
        HFONT f = MakeFont(8);
        RECT tr = { bodyL,           m_rcItem.top + 11,
                    bodyR - unreadW, m_rcItem.top + 28 };
        DrawText_(hdc, m_time, tr, kInk4,
                  DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, f);
        ::DeleteObject(f);
    }

    // Name (top line).
    {
        CString nm;
        if (m_isGroup)
        {
            nm += _T("# ");
        }
        nm += m_name;
        HFONT f = MakeFont(10, true);
        RECT nr = { bodyL,                   m_rcItem.top + 10,
                    bodyR - unreadW - 60,    m_rcItem.top + 28 };
        DrawText_(hdc, nm, nr, kInk1,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, f);
        ::DeleteObject(f);
    }

    // Preview / typing / draft (second line).
    {
        HFONT f = MakeFont(9);
        RECT pr = { bodyL,     m_rcItem.top + 30,
                    bodyR - 4, m_rcItem.top + 50 };

        if (m_typing)
        {
            DrawText_(hdc, _T("对方正在输入…"), pr, kBrandDeep,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, f);
        }
        else if (m_draft)
        {
            CString prefix = _T("[草稿] ");
            SIZE sz = MeasureText(hdc, prefix, f);
            RECT pr1 = pr;
            pr1.right = pr1.left + sz.cx;
            DrawText_(hdc, prefix, pr1, kRed,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, f);
            RECT pr2 = pr;
            pr2.left = pr1.right;
            DrawText_(hdc, m_msg, pr2, kInk3,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, f);
        }
        else
        {
            DrawText_(hdc, m_msg, pr, kInk3,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, f);
        }
        ::DeleteObject(f);
    }
}

bool ConvRow::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
    return true;
}

// ===== HeaderBar =======================================================

void HeaderBar::OnPaint(HDC hdc, const RECT&)
{
    FillRectColor(hdc, m_rcItem, kBgPanel);
    DrawHLine(hdc, m_rcItem.left, m_rcItem.right, m_rcItem.bottom - 1, kLine1);

    int x = m_rcItem.left + 16;
    int y = m_rcItem.top;

    if (m_online)
    {
        int d  = 7;
        int yd = y + 14;
        RECT dot = { x, yd, x + d, yd + d };
        FillEllipse(hdc, dot, kOnline);
        x += d + 6;
    }

    HFONT fT = MakeFont(11, true);
    SIZE szT = MeasureText(hdc, m_title, fT);
    RECT trT = { x, y + 8, x + szT.cx + 4, y + 26 };
    DrawText_(hdc, m_title, trT, kInk1,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fT);
    ::DeleteObject(fT);

    if (!m_sub.IsEmpty())
    {
        HFONT fS = MakeFont(8);
        int subL = m_rcItem.left + 16 + (m_online ? 13 : 0);
        RECT trS = { subL, y + 28, m_rcItem.right - 100, y + 44 };
        DrawText_(hdc, m_sub, trS, kInk3,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fS);
        ::DeleteObject(fS);
    }

    // Right-aligned action icon glyphs.
    LPCTSTR icons[] = { _T("📞"), _T("🎥"), _T("◧"), _T("⋯") };
    int n = sizeof(icons) / sizeof(icons[0]);
    int btnSize = 28;
    int gap = 4;
    int total = n * btnSize + (n - 1) * gap;
    int xb = m_rcItem.right - 16 - total;
    int yb = y + (m_rcItem.bottom - m_rcItem.top - btnSize) / 2;
    HFONT fI = MakeFont(11);
    for (int i = 0; i < n; ++i)
    {
        RECT br = { xb, yb, xb + btnSize, yb + btnSize };
        DrawText_(hdc, icons[i], br, kInk2,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fI);
        xb += btnSize + gap;
    }
    ::DeleteObject(fI);
}

// ===== ChatBubble ======================================================

int ChatBubble::MeasureHeight(int width) const
{
    if (m_text.IsEmpty())
    {
        return 36;
    }
    HDC hdc = ::GetDC(nullptr);
    HFONT old = (HFONT)::SelectObject(hdc, DefaultFont());
    RECT rc = { 0, 0, width - 24, 0 };       // 12px padding both sides
    ::DrawText(hdc, m_text, -1, &rc,
               DT_CALCRECT | DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    ::SelectObject(hdc, old);
    ::ReleaseDC(nullptr, hdc);
    int h = rc.bottom - rc.top + 16;          // 8px padding top + bottom
    if (h < 32)
    {
        h = 32;
    }
    return h;
}

void ChatBubble::OnPaint(HDC hdc, const RECT&)
{
    COLORREF fill   = (m_side == In) ? kBgBubbleIn : kBgBubbleOut;
    COLORREF border = (m_side == In) ? kLine1      : CLR_INVALID;
    int sharp = (m_side == In) ? 0 : 1;

    FillBubble(hdc, m_rcItem, fill, border, 8, sharp);

    if (!m_text.IsEmpty())
    {
        RECT tr = { m_rcItem.left + 12,  m_rcItem.top + 8,
                    m_rcItem.right - 12, m_rcItem.bottom - 8 };
        COLORREF txtClr = (m_side == In) ? kInk1 : RGB(20, 60, 45);
        DrawText_(hdc, m_text, tr, txtClr,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX, DefaultFont());
    }
}

// ===== ImageBubble =====================================================

int ImageBubble::MeasureHeight(int) const
{
    return m_imgHeight + 32;       // image + caption strip
}

void ImageBubble::OnPaint(HDC hdc, const RECT&)
{
    FillBubble(hdc, m_rcItem, kBgBubbleIn, kLine1, 8, 0);

    RECT img = { m_rcItem.left + 1,  m_rcItem.top + 1,
                 m_rcItem.right - 1, m_rcItem.top + m_imgHeight };
    FillRectColor(hdc, img, RGB(218, 226, 232));

    HPEN pen = ::CreatePen(PS_SOLID, 1, RGB(204, 215, 222));
    HPEN oldP = (HPEN)::SelectObject(hdc, pen);
    int span = img.right - img.left;
    for (int y = img.top - span; y < img.bottom; y += 12)
    {
        ::MoveToEx(hdc, img.left, y, nullptr);
        ::LineTo  (hdc, img.right, y + span);
    }
    ::SelectObject(hdc, oldP);
    ::DeleteObject(pen);

    HFONT fI = MakeFont(11);
    DrawText_(hdc, _T("📷 设计参考截图 · 1280×800"), img, RGB(80, 105, 120),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fI);
    ::DeleteObject(fI);

    if (!m_caption.IsEmpty())
    {
        RECT cr = { m_rcItem.left + 10,  m_rcItem.top + m_imgHeight + 4,
                    m_rcItem.right - 10, m_rcItem.bottom - 4 };
        DrawText_(hdc, m_caption, cr, kInk3,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS,
                  DefaultFont());
    }
}

// ===== FileCardBubble ==================================================

int FileCardBubble::MeasureHeight(int) const
{
    return 56;
}

void FileCardBubble::OnPaint(HDC hdc, const RECT&)
{
    FillBubble(hdc, m_rcItem, kBgBubbleIn, kLine1, 8, 0);

    RECT ico = { m_rcItem.left + 10, m_rcItem.top + 10,
                 m_rcItem.left + 46, m_rcItem.top + 46 };
    COLORREF icoBg, icoFg;
    LPCTSTR  icoText = _T("FILE");
    switch (m_kind)
    {
        case Fig:
            icoBg = RGB(232, 215, 240);
            icoFg = RGB(115,  50, 145);
            icoText = _T("FIG");
            break;
        case Pdf:
            icoBg = RGB(245, 220, 215);
            icoFg = RGB(165,  50,  35);
            icoText = _T("PDF");
            break;
        case Doc:
            icoBg = RGB(220, 230, 250);
            icoFg = RGB( 40,  85, 175);
            icoText = _T("DOC");
            break;
        case Xls:
            icoBg = RGB(220, 240, 225);
            icoFg = RGB( 30, 130,  60);
            icoText = _T("XLS");
            break;
        case Zip:
            icoBg = RGB(245, 235, 215);
            icoFg = RGB(150, 110,  40);
            icoText = _T("ZIP");
            break;
        default:
            icoBg = RGB(240, 240, 245);
            icoFg = RGB( 90,  94,  98);
            break;
    }
    PaintRoundChip(hdc, ico, icoBg, 4);

    HFONT fI = MakeFont(8, true);
    DrawText_(hdc, icoText, ico, icoFg,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fI);
    ::DeleteObject(fI);

    int textL = ico.right + 10;
    int textR = m_rcItem.right - 30;

    HFONT fN = MakeFont(10, true);
    RECT nr = { textL, m_rcItem.top + 12, textR, m_rcItem.top + 28 };
    DrawText_(hdc, m_name, nr, kInk1,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, fN);
    ::DeleteObject(fN);

    HFONT fS = MakeFont(8);
    RECT sr = { textL, m_rcItem.top + 30, textR, m_rcItem.top + 46 };
    DrawText_(hdc, m_size, sr, kInk3,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fS);
    ::DeleteObject(fS);

    HFONT fC = MakeFont(11);
    RECT cr = { m_rcItem.right - 22, m_rcItem.top, m_rcItem.right - 4, m_rcItem.bottom };
    DrawText_(hdc, _T("›"), cr, kInk3,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fC);
    ::DeleteObject(fC);
}

// ===== TypingDotsBubble ================================================

void TypingDotsBubble::OnPaint(HDC hdc, const RECT&)
{
    FillBubble(hdc, m_rcItem, kBgBubbleIn, kLine1, 8, 0);
    int cx = (m_rcItem.left + m_rcItem.right) / 2;
    int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;
    int d  = 4;
    int gap = 4;
    int total = d * 3 + gap * 2;
    int x = cx - total / 2;
    for (int i = 0; i < 3; ++i)
    {
        RECT dot = { x, cy - d / 2, x + d, cy + d / 2 };
        FillEllipse(hdc, dot, kBrandDeep);
        x += d + gap;
    }
}

// ===== MessageRow ======================================================

namespace {

int MeasureBubble(DuiControl* bubble, int w)
{
    if (!bubble)
    {
        return 50;
    }
    if (auto* cb = dynamic_cast<ChatBubble*>(bubble))
    {
        return cb->MeasureHeight(w);
    }
    if (auto* ib = dynamic_cast<ImageBubble*>(bubble))
    {
        return ib->MeasureHeight(w);
    }
    if (auto* fb = dynamic_cast<FileCardBubble*>(bubble))
    {
        return fb->MeasureHeight(w);
    }
    if (auto* td = dynamic_cast<TypingDotsBubble*>(bubble))
    {
        return td->MeasureHeight(w);
    }
    return 50;
}

int ChooseBubbleWidth(DuiControl* bubble, int rowWidth)
{
    const int avGutter = 32 + 8;        // avatar + gap
    int maxBub = rowWidth - avGutter - 16;
    if (maxBub < 80)
    {
        maxBub = 80;
    }
    if (maxBub > 360)
    {
        maxBub = 360;
    }
    if (dynamic_cast<ImageBubble*>(bubble))
    {
        return 240;
    }
    if (dynamic_cast<FileCardBubble*>(bubble))
    {
        return 280;
    }
    if (dynamic_cast<TypingDotsBubble*>(bubble))
    {
        return 56;
    }
    return maxBub;
}

} // anonymous

int MessageRow::MeasureHeight(int rowWidth) const
{
    if (m_children.empty())
    {
        return 50;
    }
    DuiControl* bub = m_children[0].get();
    int w = ChooseBubbleWidth(bub, rowWidth);
    int h = MeasureBubble(bub, w);
    m_lastBubbleW = w;
    m_lastBubbleH = h;
    return h + 18;
}

void MessageRow::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    if (m_children.empty())
    {
        return;
    }

    DuiControl* bubble = m_children[0].get();
    int rowWidth = rcAvail.right - rcAvail.left;
    int bw = ChooseBubbleWidth(bubble, rowWidth);
    int bh = MeasureBubble(bubble, bw);
    m_lastBubbleW = bw;
    m_lastBubbleH = bh;

    const int avSize = 32;
    const int gap    = 8;
    int bubL, bubR;
    if (m_side == In)
    {
        bubL = rcAvail.left + avSize + gap;
        bubR = bubL + bw;
    }
    else
    {
        bubR = rcAvail.right - avSize - gap;
        bubL = bubR - bw;
    }

    RECT br = { bubL, rcAvail.top, bubR, rcAvail.top + bh };
    bubble->SetRect(br);
    bubble->Layout(br);
}

void MessageRow::OnPaint(HDC hdc, const RECT& rcDirty)
{
    const int avSize = 32;
    int avL;
    if (m_side == In)
    {
        avL = m_rcItem.left;
    }
    else
    {
        avL = m_rcItem.right - avSize;
    }
    int avT = m_rcItem.top;
    RECT av = { avL, avT, avL + avSize, avT + avSize };
    if (m_showAvatar)
    {
        PaintAvatarTile(hdc, av, m_hue, m_initial, 11, 5);
    }

    DuiControl::OnPaint(hdc, rcDirty);

    if (!m_time.IsEmpty())
    {
        int metaY = m_rcItem.top + m_lastBubbleH + 2;
        int textL, textR;
        UINT align;
        if (m_side == In)
        {
            textL = m_rcItem.left + avSize + 8 + 4;
            textR = m_rcItem.right;
            align = DT_LEFT;
        }
        else
        {
            textL = m_rcItem.left;
            textR = m_rcItem.right - avSize - 8 - 4;
            align = DT_RIGHT;
        }

        CString meta = m_time;
        if (m_side == Out && m_readTick)
        {
            meta += _T("  \x2713\x2713");
        }

        HFONT f = MakeFont(8);
        RECT mr = { textL, metaY, textR, metaY + 14 };
        DrawText_(hdc, meta, mr, m_readTick ? kBrandDeep : kInk4,
                  align | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, f);
        ::DeleteObject(f);
    }
}

// ===== Composer ========================================================

void Composer::OnPaint(HDC hdc, const RECT&)
{
    FillRectColor(hdc, m_rcItem, kBgPanel);
    DrawHLine(hdc, m_rcItem.left, m_rcItem.right, m_rcItem.top, kLine1);

    int padX = 16;
    int y    = m_rcItem.top + 8;

    LPCTSTR tools[] = { _T("😊"), _T("🎁"), _T("🖼"),
                        _T("📎"), _T("🎤"), _T("📞"), _T("🎥") };
    int n = sizeof(tools) / sizeof(tools[0]);
    int btnSize = 24;
    int gap = 2;
    int xb = m_rcItem.left + padX;
    HFONT fI = MakeFont(11);
    for (int i = 0; i < n; ++i)
    {
        RECT br = { xb, y, xb + btnSize, y + btnSize };
        DrawText_(hdc, tools[i], br, kInk2,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fI);
        xb += btnSize + gap;
    }
    ::DeleteObject(fI);
    y += btnSize + 6;

    int inputBottom = m_rcItem.bottom - 28;
    RECT ir = { m_rcItem.left + padX,  y,
                m_rcItem.right - padX, inputBottom };
    DrawText_(hdc, m_content, ir, kInk1,
              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX, DefaultFont());

    int footY = inputBottom + 6;
    HFONT fF = MakeFont(8);
    RECT fr = { m_rcItem.left + padX, footY,
                m_rcItem.right - 100, footY + 16 };
    DrawText_(hdc, m_footer, fr, kInk4,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fF);
    ::DeleteObject(fF);

    int btnW = 64;
    int btnH = 22;
    RECT sb = { m_rcItem.right - padX - btnW, footY - 2,
                m_rcItem.right - padX,         footY - 2 + btnH };
    PaintRoundChip(hdc, sb, kBrand, 4);
    HFONT fB = MakeFont(9, true);
    DrawText_(hdc, _T("发送"), sb, kInkInv,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fB);
    ::DeleteObject(fB);
}

// ===== InfoPanel =======================================================

void InfoPanel::OnPaint(HDC hdc, const RECT&)
{
    FillRectColor(hdc, m_rcItem, kBgSidebar);
    RECT lb = { m_rcItem.left, m_rcItem.top, m_rcItem.left + 1, m_rcItem.bottom };
    FillRectColor(hdc, lb, kLine1);

    int cx = (m_rcItem.left + m_rcItem.right) / 2;
    int y  = m_rcItem.top + 24;

    int avSize = 88;
    RECT av = { cx - avSize / 2, y, cx + avSize / 2, y + avSize };
    PaintAvatarTile(hdc, av, m_hue, m_initial, 22, 10);
    y = av.bottom + 10;

    HFONT fN = MakeFont(11, true);
    RECT nr = { m_rcItem.left, y, m_rcItem.right, y + 18 };
    DrawText_(hdc, m_name, nr, kInk1,
              DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fN);
    ::DeleteObject(fN);
    y += 18 + 2;

    HFONT fS = MakeFont(8);
    RECT sr = { m_rcItem.left, y, m_rcItem.right, y + 14 };
    DrawText_(hdc, m_sub, sr, kInk3,
              DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fS);
    ::DeleteObject(fS);
    y += 14 + 14;

    int btnW = 40;
    int btnH = 28;
    int btnGap = 6;
    int totalW = btnW * 3 + btnGap * 2;
    int xb = cx - totalW / 2;
    LPCTSTR icons[] = { _T("📞"), _T("🎥"), _T("✉") };
    HFONT fB = MakeFont(11);
    for (int i = 0; i < 3; ++i)
    {
        RECT br = { xb, y, xb + btnW, y + btnH };
        PaintRoundChip(hdc, br, kBgApp, 5);
        DrawText_(hdc, icons[i], br, kInk2,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fB);
        xb += btnW + btnGap;
    }
    ::DeleteObject(fB);
    y += btnH + 16;

    DrawHLine(hdc, m_rcItem.left + 1, m_rcItem.right, y, kLine1);
    y += 12;
    HFONT fH = MakeFont(8, true);
    RECT hr = { m_rcItem.left + 16, y, m_rcItem.right - 16, y + 14 };
    DrawText_(hdc, _T("备注与标签"), hr, kInk3,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fH);
    ::DeleteObject(fH);
    y += 16;

    LPCTSTR chips[] = { _T("设计搭子"), _T("年度OKR"), _T("+") };
    HFONT fC = MakeFont(8);
    int xc = m_rcItem.left + 16;
    for (int i = 0; i < 3; ++i)
    {
        SIZE sz = MeasureText(hdc, chips[i], fC);
        int cw = sz.cx + 18;
        int ch = 22;
        RECT cr = { xc, y, xc + cw, y + ch };
        PaintRoundChip(hdc, cr, kBgSidebar2, ch / 2);
        DrawText_(hdc, chips[i], cr, kInk2,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, fC);
        xc += cw + 4;
    }
    ::DeleteObject(fC);
    y += 22 + 14;

    DrawHLine(hdc, m_rcItem.left + 1, m_rcItem.right, y, kLine1);
    y += 12;
    HFONT fH2 = MakeFont(8, true);
    RECT hr2 = { m_rcItem.left + 16, y, m_rcItem.right - 16, y + 14 };
    DrawText_(hdc, _T("共同群组 (3)"), hr2, kInk3,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fH2);
    ::DeleteObject(fH2);
    y += 18;

    int avMini = 28;
    int xm = m_rcItem.left + 16;
    int hues[3] = { 200, 240, 320 };
    LPCTSTR ins[3] = { _T("周"), _T("F"), _T("设") };
    for (int i = 0; i < 3; ++i)
    {
        RECT mav = { xm, y, xm + avMini, y + avMini };
        PaintAvatarTile(hdc, mav, hues[i], ins[i], 9, 4);
        xm += avMini + 6;
    }
    y += avMini + 14;

    DrawHLine(hdc, m_rcItem.left + 1, m_rcItem.right, y, kLine1);
    y += 12;
    HFONT fH3 = MakeFont(8, true);
    RECT hr3 = { m_rcItem.left + 16, y, m_rcItem.right - 16, y + 14 };
    DrawText_(hdc, _T("共享文件 (12)"), hr3, kInk3,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fH3);
    ::DeleteObject(fH3);
    y += 32;

    DrawHLine(hdc, m_rcItem.left + 1, m_rcItem.right, y, kLine1);
    y += 8;
    HFONT fT = MakeFont(8);
    LPCTSTR labels[] = { _T("消息免打扰"), _T("置顶聊天") };
    bool ons[2]      = { false, true };
    for (int i = 0; i < 2; ++i)
    {
        RECT lr = { m_rcItem.left + 16, y + 4, m_rcItem.right - 50, y + 22 };
        DrawText_(hdc, labels[i], lr, kInk3,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX, fT);

        int tw = 32;
        int th = 18;
        RECT tr = { m_rcItem.right - 16 - tw, y + 4,
                    m_rcItem.right - 16,      y + 4 + th };
        PaintRoundChip(hdc, tr, ons[i] ? kBrand : kInk4, th / 2);

        int dia = 14;
        int tx;
        if (ons[i])
        {
            tx = tr.right - 2 - dia;
        }
        else
        {
            tx = tr.left + 2;
        }
        RECT thumb = { tx, tr.top + 2, tx + dia, tr.top + 2 + dia };
        FillEllipse(hdc, thumb, RGB(255, 255, 255));

        y += 26;
    }
    ::DeleteObject(fT);
}

} // namespace newchatdemo
