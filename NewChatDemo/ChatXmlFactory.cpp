#include "stdafx.h"
#include "ChatXmlFactory.h"
#include "ChatControls.h"

#include "../balloonui/Controls/Layout/DuiLayout.h"

namespace newchatdemo {

using balloonwjui::DuiControl;
using balloonwjui::DuiXmlBuilder;
using balloonwjui::DuiVBox;

namespace {

// ----- attribute helpers ------------------------------------------------

CString ToCString_(const std::string& utf8)
{
    if (utf8.empty())
    {
        return CString();
    }
    int cw = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (cw <= 0)
    {
        return CString();
    }
    std::wstring w(cw, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], cw);
    while (!w.empty() && w.back() == 0)
    {
        w.pop_back();
    }
    return CString(w.c_str());
}

CString GetStr(const DuiXmlBuilder::Node& n, const char* key)
{
    auto it = n.attrs.find(key);
    if (it == n.attrs.end())
    {
        return CString();
    }
    return ToCString_(it->second);
}

int GetInt(const DuiXmlBuilder::Node& n, const char* key, int def = 0)
{
    auto it = n.attrs.find(key);
    if (it == n.attrs.end() || it->second.empty())
    {
        return def;
    }
    return std::atoi(it->second.c_str());
}

bool GetBool(const DuiXmlBuilder::Node& n, const char* key, bool def = false)
{
    auto it = n.attrs.find(key);
    if (it == n.attrs.end())
    {
        return def;
    }
    const std::string& s = it->second;
    return s == "1" || s == "true" || s == "True" || s == "yes";
}

std::string GetStr8(const DuiXmlBuilder::Node& n, const char* key)
{
    auto it = n.attrs.find(key);
    if (it == n.attrs.end())
    {
        return std::string();
    }
    return it->second;
}

// ----- ChatThread: a custom-layout container holding bubbles -----------
//
// Demonstrates "self-drawn layout" — XML lists day-pill / msg-row
// children, ChatThread::Layout stacks them top-to-bottom with a fixed
// gap and lets each child report its own height through dynamic_cast.
class ChatThread : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT&) override
    {
        HBRUSH br = ::CreateSolidBrush(kBgApp);
        ::FillRect(hdc, &m_rcItem, br);
        ::DeleteObject(br);
        DuiControl::OnPaint(hdc, m_rcItem);
    }

    void Layout(const RECT& rcAvail) override
    {
        m_rcItem = rcAvail;
        int x = rcAvail.left + m_padL;
        int y = rcAvail.top  + m_padT;
        int w = rcAvail.right - rcAvail.left - m_padL - m_padR;
        for (auto& c : m_children)
        {
            int h = ChildHeight(c.get(), w);
            RECT r = { x, y, x + w, y + h };
            c->SetRect(r);
            c->Layout(r);
            y += h + m_gap;
        }
    }

    void SetPadding4(int l, int t, int r, int b)
    {
        m_padL = l;
        m_padT = t;
        m_padR = r;
        m_padB = b;
    }
    void SetGapPx(int g)
    {
        m_gap = g;
    }

private:
    static int ChildHeight(DuiControl* c, int width)
    {
        if (auto* mr = dynamic_cast<MessageRow*>(c))
        {
            return mr->MeasureHeight(width);
        }
        // DayPill: small fixed pill height.
        return 22;
    }

    int m_padL = 16;
    int m_padT = 16;
    int m_padR = 16;
    int m_padB = 16;
    int m_gap  = 12;
};

// ----- SearchBoxMock (visual-only) -------------------------------------
class SearchBoxMock : public DuiControl
{
public:
    void SetPlaceholder(LPCTSTR s)
    {
        if (s && *s)
        {
            m_ph = s;
        }
    }

    void OnPaint(HDC hdc, const RECT&) override
    {
        HBRUSH br = ::CreateSolidBrush(kBgSidebar);
        ::FillRect(hdc, &m_rcItem, br);
        ::DeleteObject(br);

        RECT inner = { m_rcItem.left + 12, m_rcItem.top + 10,
                       m_rcItem.right - 12, m_rcItem.bottom - 10 };
        HBRUSH br2 = ::CreateSolidBrush(kBgApp);
        HBRUSH oldBr = (HBRUSH)::SelectObject(hdc, br2);
        HPEN pen = ::CreatePen(PS_SOLID, 1, kLine1);
        HPEN oldPen = (HPEN)::SelectObject(hdc, pen);
        ::RoundRect(hdc, inner.left, inner.top, inner.right, inner.bottom, 6, 6);
        ::SelectObject(hdc, oldBr);
        ::SelectObject(hdc, oldPen);
        ::DeleteObject(br2);
        ::DeleteObject(pen);

        RECT tr = { inner.left + 8, inner.top, inner.right - 4, inner.bottom };
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, kInk3);
        ::DrawText(hdc, m_ph, -1, &tr,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

private:
    CString m_ph = _T("搜索");
};

// ----- RailSpacer: invisible filler ------------------------------------
class RailSpacer : public DuiControl
{
public:
    void OnPaint(HDC, const RECT&) override
    {
    }
};

// ----- per-tag factory functions ---------------------------------------

std::unique_ptr<DuiControl> MakeChatBubble(const DuiXmlBuilder::Node& n)
{
    auto b = std::unique_ptr<ChatBubble>(new ChatBubble());
    b->SetText(GetStr(n, "text"));
    std::string side = GetStr8(n, "side");
    b->SetSide(side == "out" ? ChatBubble::Out : ChatBubble::In);
    return b;
}

std::unique_ptr<DuiControl> MakeImageBubble(const DuiXmlBuilder::Node& n)
{
    auto b = std::unique_ptr<ImageBubble>(new ImageBubble());
    int w  = GetInt(n, "width", 240);
    int ih = GetInt(n, "imgHeight", 160);
    b->SetData(w, ih, GetStr(n, "caption"));
    return b;
}

std::unique_ptr<DuiControl> MakeFileCardBubble(const DuiXmlBuilder::Node& n)
{
    auto b = std::unique_ptr<FileCardBubble>(new FileCardBubble());
    std::string k = GetStr8(n, "kind");
    FileCardBubble::Kind kind = FileCardBubble::Generic;
    if (k == "fig")
    {
        kind = FileCardBubble::Fig;
    }
    else if (k == "pdf")
    {
        kind = FileCardBubble::Pdf;
    }
    else if (k == "doc")
    {
        kind = FileCardBubble::Doc;
    }
    else if (k == "xls")
    {
        kind = FileCardBubble::Xls;
    }
    else if (k == "zip")
    {
        kind = FileCardBubble::Zip;
    }
    b->SetData(kind, GetStr(n, "name"), GetStr(n, "size"));
    return b;
}

std::unique_ptr<DuiControl> MakeTypingDots(const DuiXmlBuilder::Node&)
{
    return std::unique_ptr<TypingDotsBubble>(new TypingDotsBubble());
}

// msg-row factory: only sets metadata. The bubble child is added by the
// generic auto-recursion in DuiXmlBuilder, which dispatches back into
// this factory for the <chat-bubble> / <image-bubble> / etc.
std::unique_ptr<DuiControl> MakeMessageRow(const DuiXmlBuilder::Node& n)
{
    auto row = std::unique_ptr<MessageRow>(new MessageRow());
    std::string side = GetStr8(n, "side");
    int hue = GetInt(n, "hue", 165);
    CString initial = GetStr(n, "initial");
    CString time    = GetStr(n, "time");
    bool readTick   = GetStr8(n, "tick") == "read";
    bool hideAvatar = GetBool(n, "hideAvatar", false);
    row->SetMeta(side == "out" ? MessageRow::Out : MessageRow::In,
                 hue, initial, time, !hideAvatar, readTick);
    return row;
}

} // anonymous

// =====================================================================

DuiXmlBuilder::CustomFactory MakeChatFactory()
{
    return [](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
    {
        if (n.tag == "rail-item")
        {
            auto r = std::unique_ptr<RailItem>(new RailItem());
            r->SetGlyph(GetStr(n, "glyph"));
            r->SetActive(GetBool(n, "active", false));

            // badge: "0" = solid dot, ">=1" = numeric chip, missing = none.
            auto it = n.attrs.find("badge");
            int badge = -1;
            if (it != n.attrs.end())
            {
                badge = std::atoi(it->second.c_str());
                if (it->second == "0")
                {
                    badge = 0;
                }
            }
            r->SetBadge(badge);

            // For kind="profile", paint the avatar's initial as the glyph.
            std::string kind = GetStr8(n, "kind");
            if (kind == "profile")
            {
                r->SetGlyph(GetStr(n, "initial"));
            }
            return r;
        }
        if (n.tag == "rail-spacer")
        {
            return std::unique_ptr<RailSpacer>(new RailSpacer());
        }
        if (n.tag == "search-box")
        {
            auto s = std::unique_ptr<SearchBoxMock>(new SearchBoxMock());
            s->SetPlaceholder(GetStr(n, "placeholder"));
            return s;
        }
        if (n.tag == "conv-row")
        {
            auto r = std::unique_ptr<ConvRow>(new ConvRow());
            int status = GetInt(n, "status", 0);
            r->SetData(GetStr(n, "name"),
                       GetStr(n, "msg"),
                       GetStr(n, "time"),
                       GetInt(n, "hue", 165),
                       GetStr(n, "initial"),
                       GetInt(n, "unread", 0),
                       GetBool(n, "active", false),
                       GetBool(n, "pinned", false),
                       GetBool(n, "muted",  false),
                       GetBool(n, "typing", false),
                       GetBool(n, "draft",  false),
                       GetBool(n, "group",  false),
                       status);
            return r;
        }
        if (n.tag == "header-bar")
        {
            auto h = std::unique_ptr<HeaderBar>(new HeaderBar());
            h->SetData(GetStr(n, "title"),
                       GetStr(n, "sub"),
                       GetBool(n, "online", true));
            return h;
        }
        if (n.tag == "chat-thread")
        {
            auto t = std::unique_ptr<ChatThread>(new ChatThread());
            std::string p = GetStr8(n, "padding");
            if (!p.empty())
            {
                int l = 16;
                int tt = 16;
                int rr = 16;
                int bb = 16;
                if (std::sscanf(p.c_str(), "%d,%d,%d,%d", &l, &tt, &rr, &bb) != 4)
                {
                    int all = std::atoi(p.c_str());
                    l = tt = rr = bb = all;
                }
                t->SetPadding4(l, tt, rr, bb);
            }
            int gap = GetInt(n, "gap", 12);
            t->SetGapPx(gap);
            return t;
        }
        if (n.tag == "day-pill")
        {
            auto d = std::unique_ptr<DayPill>(new DayPill());
            d->SetText(GetStr(n, "text"));
            return d;
        }
        if (n.tag == "msg-row")
        {
            return MakeMessageRow(n);
        }
        if (n.tag == "composer")
        {
            auto c = std::unique_ptr<Composer>(new Composer());
            CString content = GetStr(n, "content");
            CString footer  = GetStr(n, "footer");
            if (!content.IsEmpty())
            {
                c->SetContent(content);
            }
            if (!footer.IsEmpty())
            {
                c->SetFooter(footer);
            }
            return c;
        }
        if (n.tag == "info-panel")
        {
            auto p = std::unique_ptr<InfoPanel>(new InfoPanel());
            p->SetData(GetStr(n, "name"),
                       GetStr(n, "sub"),
                       GetInt(n, "hue", 165),
                       GetStr(n, "initial"));
            return p;
        }
        if (n.tag == "chat-bubble")
        {
            return MakeChatBubble(n);
        }
        if (n.tag == "image-bubble")
        {
            return MakeImageBubble(n);
        }
        if (n.tag == "file-card-bubble")
        {
            return MakeFileCardBubble(n);
        }
        if (n.tag == "typing-dots")
        {
            return MakeTypingDots(n);
        }
        return nullptr;
    };
}

} // namespace newchatdemo
