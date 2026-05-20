// ChatControls: every business-layer self-drawn control for the WeChat-
// style single-chat demo lives here. Per project policy these are NOT
// added to SkinLib/Dui (the kernel) — they are app-local composites of
// existing primitives + custom GDI / DuiAA paint.
//
// Visual reference: docs/wechat-demo/screen-chat.jsx + styles.css.

#pragma once

#include "../balloonui/DuiControl.h"
#include "ChatTheme.h"

namespace newchatdemo {

using balloonwjui::DuiControl;

// ----- RailItem (40x40 nav button) -------------------------------------
class RailItem : public DuiControl
{
public:
    void SetActive(bool b)
    {
        m_active = b;
        Invalidate();
    }
    bool IsActive() const
    {
        return m_active;
    }
    void SetGlyph(LPCTSTR g)
    {
        m_glyph = g ? g : _T("");
        Invalidate();
    }
    void SetBadge(int count)
    {
        m_badge = count;
        Invalidate();
    }
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnSetCursor(POINT) override;

private:
    bool    m_active = false;
    CString m_glyph;
    int     m_badge = -1;       // -1 hide, 0 dot only, >=1 numeric chip
};

// ----- DayPill (centered "今天 14:21" pill) ----------------------------
class DayPill : public DuiControl
{
public:
    void SetText(LPCTSTR sz)
    {
        m_text = sz ? sz : _T("");
        Invalidate();
    }
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    CString m_text;
};

// ----- ConvRow (single conversation list row) --------------------------
class ConvRow : public DuiControl
{
public:
    void SetData(LPCTSTR name, LPCTSTR msg, LPCTSTR time,
                 int hue, LPCTSTR initial, int unread = 0,
                 bool active = false, bool pinned = false,
                 bool muted  = false, bool typing  = false,
                 bool draft  = false, bool isGroup = false,
                 int  status = 0);

    void OnPaint(HDC hdc, const RECT& rcDirty) override;

    bool OnMouseEnter() override
    {
        m_hover = true;
        Invalidate();
        return false;
    }
    bool OnMouseLeave() override
    {
        m_hover = false;
        Invalidate();
        return false;
    }
    bool OnSetCursor(POINT) override;

private:
    CString m_name, m_msg, m_time, m_initial;
    int     m_hue    = 165;
    int     m_unread = 0;
    int     m_status = 0;
    bool    m_active  = false;
    bool    m_pinned  = false;
    bool    m_muted   = false;
    bool    m_typing  = false;
    bool    m_draft   = false;
    bool    m_isGroup = false;
    bool    m_hover   = false;
};

// ----- HeaderBar (52px main-panel header) ------------------------------
class HeaderBar : public DuiControl
{
public:
    void SetData(LPCTSTR title, LPCTSTR sub, bool online = true)
    {
        m_title  = title;
        m_sub    = sub;
        m_online = online;
        Invalidate();
    }
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    CString m_title, m_sub;
    bool    m_online = true;
};

// ----- ChatBubble (rounded bubble with one squared corner) -------------
class ChatBubble : public DuiControl
{
public:
    enum Side
    {
        In,
        Out
    };

    void SetText(LPCTSTR sz)
    {
        m_text = sz ? sz : _T("");
        Invalidate();
    }
    void SetSide(Side s)
    {
        m_side = s;
        Invalidate();
    }
    Side GetSide() const
    {
        return m_side;
    }
    int  MeasureHeight(int width) const;
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    CString m_text;
    Side    m_side = In;
};

// ----- ImageBubble (striped placeholder + caption) ---------------------
class ImageBubble : public DuiControl
{
public:
    void SetData(int w, int imgH, LPCTSTR caption)
    {
        m_width     = w;
        m_imgHeight = imgH;
        m_caption   = caption ? caption : _T("");
        Invalidate();
    }
    int  MeasureHeight(int width) const;
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    int     m_width     = 240;
    int     m_imgHeight = 160;
    CString m_caption;
};

// ----- FileCardBubble --------------------------------------------------
class FileCardBubble : public DuiControl
{
public:
    enum Kind
    {
        Generic,
        Fig,
        Pdf,
        Doc,
        Xls,
        Zip
    };

    void SetData(Kind kind, LPCTSTR name, LPCTSTR sizeText)
    {
        m_kind = kind;
        m_name = name;
        m_size = sizeText;
        Invalidate();
    }
    int  MeasureHeight(int width) const;
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    Kind    m_kind = Fig;
    CString m_name, m_size;
};

// ----- TypingDotsBubble (three brand dots, static) ---------------------
class TypingDotsBubble : public DuiControl
{
public:
    int  MeasureHeight(int) const
    {
        return 30;
    }
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
};

// ----- MessageRow ------------------------------------------------------
//
// Wraps a content control (any of the *Bubble types — added as the row's
// first child via XML auto-recursion) with avatar gutter + time + read
// tick. Side dictates left/right alignment.
class MessageRow : public DuiControl
{
public:
    enum Side
    {
        In,
        Out
    };

    void SetMeta(Side s, int hue, LPCTSTR initial, LPCTSTR time,
                 bool showAvatar = true, bool readDoubleTick = false)
    {
        m_side       = s;
        m_hue        = hue;
        m_initial    = initial;
        m_time       = time;
        m_showAvatar = showAvatar;
        m_readTick   = readDoubleTick;
    }

    // Compute the row's height for a given parent (chat-thread) width.
    int  MeasureHeight(int rowWidth) const;

    void Layout(const RECT& rcAvail) override;
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    Side    m_side = In;
    int     m_hue  = 165;
    CString m_initial;
    CString m_time;
    bool    m_showAvatar = true;
    bool    m_readTick   = false;

    // Cached during MeasureHeight() / Layout() so OnPaint can place the
    // meta line at the bubble's bottom edge.
    mutable int m_lastBubbleH = 60;
    mutable int m_lastBubbleW = 280;
};

// ----- Composer --------------------------------------------------------
class Composer : public DuiControl
{
public:
    void SetContent(LPCTSTR sz)
    {
        m_content = sz;
        Invalidate();
    }
    void SetFooter(LPCTSTR sz)
    {
        m_footer = sz;
        Invalidate();
    }
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    CString m_content = _T("发现还可以继续优化卡片层级");
    CString m_footer  = _T("Markdown 已启用 · @ 提及成员");
};

// ----- InfoPanel (right 240px sidebar) ---------------------------------
class InfoPanel : public DuiControl
{
public:
    void SetData(LPCTSTR name, LPCTSTR sub, int hue, LPCTSTR initial)
    {
        m_name    = name;
        m_sub     = sub;
        m_hue     = hue;
        m_initial = initial;
        Invalidate();
    }
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    CString m_name, m_sub, m_initial;
    int     m_hue = 165;
};

} // namespace newchatdemo
