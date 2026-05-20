#include "stdafx.h"
#include "DuiXmlBuilder.h"

#if BUI_FEATURE_XMLBUILDER

// 每个 #include 都用 BUI_FEATURE_XXX 门控 —— feature 关掉时对应控件
// 头文件里的 class 定义会被 #if 包成空，<u>但</u>这里仍可被预处理读
// 入（#include 一个空头无害），为防可能的展开副作用 + 让"feature off
// 时该控件代码完全不参与编译"这个不变式更直观，干脆把 #include 也
// 一起 gate 起来。dispatch 分支会用同一个 BUI_FEATURE_XXX 宏门控。
#include "Controls/Layout/DuiLayout.h"           // 始终 in（基础容器，无 feature 开关）
#if BUI_FEATURE_BUTTON
#  include "Controls/Basic/DuiButton.h"
#endif
#if BUI_FEATURE_LABEL
#  include "Controls/Basic/DuiLabel.h"
#endif
#if BUI_FEATURE_EDIT
#  include "Controls/Input/DuiEditHost.h"
#endif
#if BUI_FEATURE_FRAMEWINDOW
#  include "Controls/Window/DuiFrameWindow.h"    // DuiFrameWindowConfig 完整类型 — FromFrameXml 写字段
#endif
// V2 native XML tags（无 model 简单控件）
#if BUI_FEATURE_AVATAR
#  include "Controls/Basic/DuiAvatar.h"
#endif
#if BUI_FEATURE_BADGE
#  include "Controls/Basic/DuiBadge.h"
#endif
#if BUI_FEATURE_SEPARATOR
#  include "Controls/Basic/DuiSeparator.h"
#endif
#if BUI_FEATURE_SLIDER
#  include "Controls/Input/DuiSlider.h"
#endif
#if BUI_FEATURE_SWITCH
#  include "Controls/Input/DuiSwitch.h"
#endif
#if BUI_FEATURE_PROGRESSBAR
#  include "Controls/Feedback/DuiProgressBar.h"
#endif
#if BUI_FEATURE_SPLITTER
#  include "Controls/Layout/DuiSplitter.h"
#endif
#if BUI_FEATURE_GROUPBOX
#  include "Controls/Basic/DuiGroupBox.h"
#endif
#if BUI_FEATURE_SEARCHBOX
#  include "Controls/Input/DuiSearchBox.h"
#endif
#if BUI_FEATURE_SPINBOX
#  include "Controls/Input/DuiSpinBox.h"
#endif
#if BUI_FEATURE_RICHEDIT
#  include "Controls/Input/DuiRichEditHost.h"
#endif
#if BUI_FEATURE_TREEVIEW
#  include "Controls/List/DuiTreeView.h"
#endif
#if BUI_FEATURE_MENUBAR
#  include "Controls/List/DuiMenuBar.h"
#endif
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace balloonwjui {

// =====================================================================
// Tiny XML tokenizer / parser. Hand-rolled to avoid an msxml dep.
// Accepts: <tag attr="val" attr2='val'>...</tag> and <tag .../>.
// Rejects: DOCTYPE, comments, CDATA, processing instructions, namespaces.
// =====================================================================

namespace {

struct Cursor
{
    const char* p;
    const char* end;
    bool error = false;

    bool atEnd() const
    {
        return p >= end;
    }
    char peek() const
    {
        return atEnd() ? '\0' : *p;
    }
    void skip()
    {
        if (!atEnd())
        {
            ++p;
        }
    }
    void skipWs()
    {
        while (!atEnd() && std::isspace((unsigned char)*p))
        {
            ++p;
        }
    }
};

bool IsNameStart(char c)
{
    return std::isalpha((unsigned char)c) || c == '_';
}

bool IsNameChar(char c)
{
    return std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == ':';
}

std::string ParseName(Cursor& c)
{
    std::string s;
    while (!c.atEnd() && IsNameChar(c.peek()))
    {
        s += c.peek();
        c.skip();
    }
    return s;
}

// Skip XML processing instruction / comment / declaration. Best-effort:
// any '<? ... ?>' or '<!-- ... -->' is consumed and ignored.
void SkipProlog(Cursor& c)
{
    for (;;)
    {
        c.skipWs();
        if (c.atEnd())
        {
            return;
        }
        if (c.peek() != '<')
        {
            return;
        }
        if (c.p + 1 >= c.end)
        {
            return;
        }
        char n = c.p[1];
        if (n == '?')
        {
            // <?xml ... ?>
            while (!c.atEnd())
            {
                if (c.peek() == '?' && c.p + 1 < c.end && c.p[1] == '>')
                {
                    c.p += 2;
                    break;
                }
                c.skip();
            }
            continue;
        }
        if (n == '!')
        {
            // Distinguish `<!-- ... -->` (comment, may contain `>` in
            // the body) from `<!DOCTYPE ...>` (must end at first `>`).
            // Without this, comments like `<!-- see <foo/> below -->`
            // terminate prematurely at the inner `>` and the parser
            // then chokes on the leftover `-->`.
            if (c.p + 3 < c.end
                && c.p[1] == '!'
                && c.p[2] == '-'
                && c.p[3] == '-')
            {
                c.p += 4;
                while (c.p + 2 < c.end)
                {
                    if (c.p[0] == '-' && c.p[1] == '-' && c.p[2] == '>')
                    {
                        c.p += 3;
                        break;
                    }
                    c.skip();
                }
                continue;
            }
            // Non-comment `<!...>` (DOCTYPE etc.) — skip to first `>`.
            while (!c.atEnd() && c.peek() != '>')
            {
                c.skip();
            }
            if (c.peek() == '>')
            {
                c.skip();
            }
            continue;
        }
        return;
    }
}

std::string DecodeAttrEntities(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); )
    {
        if (in[i] == '&')
        {
            // &amp; &lt; &gt; &quot; &apos;
            if (in.compare(i, 5, "&amp;") == 0)
            {
                out += '&';
                i += 5;
                continue;
            }
            if (in.compare(i, 4, "&lt;") == 0)
            {
                out += '<';
                i += 4;
                continue;
            }
            if (in.compare(i, 4, "&gt;") == 0)
            {
                out += '>';
                i += 4;
                continue;
            }
            if (in.compare(i, 6, "&quot;") == 0)
            {
                out += '"';
                i += 6;
                continue;
            }
            if (in.compare(i, 6, "&apos;") == 0)
            {
                out += '\'';
                i += 6;
                continue;
            }
        }
        out += in[i];
        ++i;
    }
    return out;
}

bool ParseElement(Cursor& c, DuiXmlBuilder::Node& out);

// Skip whitespace and any inline `<!-- ... -->` comments. Used between
// child elements inside a parent so authors can annotate the layout
// without breaking the parse. Best-effort: any <! ... > construct is
// consumed (matches SkipProlog's tolerance).
void SkipWsAndComments(Cursor& c)
{
    for (;;)
    {
        c.skipWs();
        if (c.atEnd())
        {
            return;
        }
        if (c.peek() != '<')
        {
            return;
        }
        if (c.p + 3 < c.end
            && c.p[1] == '!'
            && c.p[2] == '-'
            && c.p[3] == '-')
        {
            // <!-- ... -->
            c.p += 4;
            while (c.p + 2 < c.end)
            {
                if (c.p[0] == '-' && c.p[1] == '-' && c.p[2] == '>')
                {
                    c.p += 3;
                    break;
                }
                c.skip();
            }
            continue;
        }
        return;
    }
}

// Parse the inside of <tag ...> after the leading '<' has been
// consumed and the tag name is in `tagName`. Reads attributes until
// '>' (open + content) or '/>' (self-closing). Sets selfClose.
bool ParseAttrsAndCloser(Cursor& c, DuiXmlBuilder::Node& node, bool& selfClose)
{
    selfClose = false;
    while (!c.atEnd())
    {
        c.skipWs();
        if (c.atEnd())
        {
            c.error = true;
            return false;
        }
        char ch = c.peek();
        if (ch == '/')
        {
            c.skip();
            if (c.peek() != '>')
            {
                c.error = true;
                return false;
            }
            c.skip();
            selfClose = true;
            return true;
        }
        if (ch == '>')
        {
            c.skip();
            return true;
        }
        // Attribute name.
        if (!IsNameStart(ch))
        {
            c.error = true;
            return false;
        }
        std::string an = ParseName(c);
        c.skipWs();
        if (c.peek() != '=')
        {
            c.error = true;
            return false;
        }
        c.skip();
        c.skipWs();
        char q = c.peek();
        if (q != '"' && q != '\'')
        {
            c.error = true;
            return false;
        }
        c.skip();
        std::string av;
        while (!c.atEnd() && c.peek() != q)
        {
            av += c.peek();
            c.skip();
        }
        if (c.atEnd())
        {
            c.error = true;
            return false;
        }
        c.skip();   // closing quote
        node.attrs[an] = DecodeAttrEntities(av);
    }
    c.error = true;
    return false;
}

bool ParseElement(Cursor& c, DuiXmlBuilder::Node& out)
{
    c.skipWs();
    if (c.peek() != '<')
    {
        c.error = true;
        return false;
    }
    c.skip();
    out.tag = ParseName(c);
    if (out.tag.empty())
    {
        c.error = true;
        return false;
    }

    bool selfClose = false;
    if (!ParseAttrsAndCloser(c, out, selfClose))
    {
        return false;
    }
    if (selfClose)
    {
        return true;
    }

    // Children until </tag>.
    while (!c.atEnd())
    {
        SkipWsAndComments(c);
        if (c.atEnd())
        {
            c.error = true;
            return false;
        }
        if (c.peek() != '<')
        {
            c.error = true;
            return false;
        }
        if (c.p + 1 < c.end && c.p[1] == '/')
        {
            // closer
            c.p += 2;
            std::string closer = ParseName(c);
            c.skipWs();
            if (c.peek() != '>')
            {
                c.error = true;
                return false;
            }
            c.skip();
            if (closer != out.tag)
            {
                c.error = true;
                return false;
            }
            return true;
        }
        DuiXmlBuilder::Node child;
        if (!ParseElement(c, child))
        {
            return false;
        }
        out.children.push_back(std::move(child));
    }
    c.error = true;
    return false;
}

} // anonymous

bool DuiXmlBuilder::Parse(LPCSTR xmlUtf8, Node& outRoot)
{
    if (!xmlUtf8 || !*xmlUtf8)
    {
        return false;
    }
    Cursor c;
    c.p   = xmlUtf8;
    c.end = xmlUtf8 + std::strlen(xmlUtf8);
    SkipProlog(c);
    if (!ParseElement(c, outRoot))
    {
        return false;
    }
    return !c.error;
}

// =====================================================================
// Builder — Node -> DuiControl
// =====================================================================

namespace {

int ParseInt(const std::string& s, int def)
{
    if (s.empty())
    {
        return def;
    }
    try
    {
        return std::atoi(s.c_str());
    }
    catch (...)
    {
        return def;
    }
}

bool ParseBool(const std::string& s, bool def)
{
    if (s.empty())
    {
        return def;
    }
    return (s == "1" || s == "true" || s == "True" || s == "yes");
}

COLORREF ParseColor(const std::string& s, COLORREF def)
{
    if (s.empty())
    {
        return def;
    }
    int r = 0, g = 0, b = 0;
    if (std::sscanf(s.c_str(), "%d,%d,%d", &r, &g, &b) != 3)
    {
        return def;
    }
    if (r < 0)
    {
        r = 0;
    }
    if (r > 255)
    {
        r = 255;
    }
    if (g < 0)
    {
        g = 0;
    }
    if (g > 255)
    {
        g = 255;
    }
    if (b < 0)
    {
        b = 0;
    }
    if (b > 255)
    {
        b = 255;
    }
    return RGB((BYTE)r, (BYTE)g, (BYTE)b);
}

std::string Get(const DuiXmlBuilder::Node& n, const char* key)
{
    auto it = n.attrs.find(key);
    return it == n.attrs.end() ? std::string() : it->second;
}

CString ToCString(const std::string& utf8)
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

DuiLayout::Hint MakeHint(const DuiXmlBuilder::Node& n, bool parentIsH)
{
    DuiLayout::Hint h;
    int fixedW  = ParseInt(Get(n, "fixedWidth"),  -1);
    int fixedH  = ParseInt(Get(n, "fixedHeight"), -1);
    int weight  = ParseInt(Get(n, "weight"),       1);
    int margin  = ParseInt(Get(n, "margin"),       0);
    h.weight = weight;
    if (margin > 0)
    {
        h.Margin(margin);
    }
    if (parentIsH)
    {
        if (fixedW >= 0)
        {
            h.fixedMain  = fixedW;
        }
        if (fixedH >= 0)
        {
            h.fixedCross = fixedH;
        }
    }
    else
    {
        if (fixedH >= 0)
        {
            h.fixedMain  = fixedH;
        }
        if (fixedW >= 0)
        {
            h.fixedCross = fixedW;
        }
    }

    // alignCross 属性：fixedCross 必须配合非 AlignFill 的 alignCross 才会
    // 真正限制控件交叉轴尺寸 —— DuiLayout 在 alignCross == AlignFill 时
    // 直接忽略 fixedCross 把控件拉满交叉轴。这条解析支持 XML 写
    // `alignCross="center"` / "near" / "far" / "fill"（默认仍 fill 保持
    // 向后兼容）。alignMain 同理但极少用，等真有需求再加。
    {
        std::string v = Get(n, "alignCross");
        if (!v.empty())
        {
            if      (v == "center")             { h.alignCross = DuiLayout::AlignCenter; }
            else if (v == "near"  || v == "start" || v == "left" || v == "top")    { h.alignCross = DuiLayout::AlignNear; }
            else if (v == "far"   || v == "end"   || v == "right"|| v == "bottom") { h.alignCross = DuiLayout::AlignFar; }
            else if (v == "fill")               { h.alignCross = DuiLayout::AlignFill; }
        }
    }

    return h;
}

// Apply common per-control attributes (id, etc.) to a freshly built control.
void ApplyCommon(DuiControl* c, const DuiXmlBuilder::Node& n)
{
    int id = ParseInt(Get(n, "id"), 0);
    if (id != 0)
    {
        c->SetCtrlId((UINT)id);
    }
}

std::unique_ptr<DuiControl> BuildOne(const DuiXmlBuilder::Node& n,
                                     const DuiXmlBuilder::CustomFactory& fac);

void ParsePadding(DuiLayout* layout, const DuiXmlBuilder::Node& n)
{
    std::string s = Get(n, "padding");
    if (s.empty())
    {
        return;
    }
    int l = 0, t = 0, r = 0, b = 0;
    if (std::sscanf(s.c_str(), "%d,%d,%d,%d", &l, &t, &r, &b) == 4)
    {
        layout->SetPadding(l, t, r, b);
    }
    else
    {
        int all = ParseInt(s, 0);
        layout->SetPadding(all);
    }
}

template <typename Container>
std::unique_ptr<DuiControl> BuildLayout(const DuiXmlBuilder::Node& n,
                                       bool parentIsH,
                                       const DuiXmlBuilder::CustomFactory& fac)
{
    auto box = std::unique_ptr<Container>(new Container());
    Container* raw = box.get();
    ParsePadding(raw, n);
    int gap = ParseInt(Get(n, "gap"), 0);
    if (gap > 0)
    {
        raw->SetGap(gap);
    }
    ApplyCommon(raw, n);

    bool meIsH = std::is_same<Container, DuiHBox>::value;

    for (auto& child : n.children)
    {
        auto built = BuildOne(child, fac);
        if (!built)
        {
            continue;
        }
        DuiLayout::Hint hint = MakeHint(child, meIsH);
        raw->AddChild(std::move(built), hint);
    }
    (void)parentIsH;
    return box;
}

std::unique_ptr<DuiControl> BuildGrid(const DuiXmlBuilder::Node& n,
                                      const DuiXmlBuilder::CustomFactory& fac)
{
    auto g = std::unique_ptr<DuiGrid>(new DuiGrid());
    DuiGrid* raw = g.get();
    ParsePadding(raw, n);
    int gap = ParseInt(Get(n, "gap"), 0);
    if (gap > 0)
    {
        raw->SetGap(gap);
    }
    int rows = ParseInt(Get(n, "rows"), 1);
    int cols = ParseInt(Get(n, "cols"), 1);
    raw->SetGrid(rows, cols);
    ApplyCommon(raw, n);

    for (auto& child : n.children)
    {
        auto built = BuildOne(child, fac);
        if (built)
        {
            raw->AddChild(std::move(built), MakeHint(child, true));
        }
    }
    return g;
}

#if BUI_FEATURE_BUTTON
std::unique_ptr<DuiControl> BuildButton(const DuiXmlBuilder::Node& n)
{
    auto b = std::unique_ptr<DuiButton>(new DuiButton());
    DuiButton* raw = b.get();
    ApplyCommon(raw, n);
    raw->SetText(ToCString(Get(n, "text")));
    std::string t = Get(n, "buttonType");
    if (t == "icon")
    {
        raw->SetButtonType(DuiButton::StyleIcon);
    }
    else if (t == "checkbox")
    {
        raw->SetButtonType(DuiButton::StyleCheckbox);
    }
    else if (t == "radio")
    {
        raw->SetButtonType(DuiButton::StyleRadio);
    }
    else
    {
        raw->SetButtonType(DuiButton::StylePushButton);
    }
    return b;
}
#endif // BUI_FEATURE_BUTTON

#if BUI_FEATURE_LABEL
std::unique_ptr<DuiControl> BuildLabel(const DuiXmlBuilder::Node& n)
{
    auto l = std::unique_ptr<DuiLabel>(new DuiLabel());
    DuiLabel* raw = l.get();
    ApplyCommon(raw, n);
    raw->SetText(ToCString(Get(n, "text")));
    std::string c = Get(n, "textColor");
    if (!c.empty())
    {
        raw->SetTextColor(ParseColor(c, RGB(0, 0, 0)));
    }
    return l;
}
#endif // BUI_FEATURE_LABEL

#if BUI_FEATURE_EDIT
std::unique_ptr<DuiControl> BuildEdit(const DuiXmlBuilder::Node& n)
{
    auto e = std::unique_ptr<DuiEditHost>(new DuiEditHost());
    DuiEditHost* raw = e.get();
    ApplyCommon(raw, n);
    raw->SetPlaceholder(ToCString(Get(n, "placeholder")));
    if (ParseBool(Get(n, "password"), false))
    {
        raw->SetPassword(true);
    }
    if (ParseBool(Get(n, "multiline"), false))
    {
        raw->SetMultiLine(true);
    }
    return e;
}
#endif // BUI_FEATURE_EDIT

// =====================================================================
// V2 — 10 个"无 model 简单控件"的 Build* 函数
//
// 设计约定：
//   · 任何属性<u>不写</u> = 不调对应 setter，控件保留 ctor 设的默认值
//   · 颜色都接受 "r,g,b" 三段（0–255）
//   · RECT (margins / padding) 接受 "l,t,r,b" 四段或单值"n"
//   · 布尔 "true" / "1" / "yes" / "True" 视为 true，其它 false
//   · ApplyCommon 已经处理 id 属性，每个 Build* 都会调
//
// HWND-hosted 控件（SearchBox / SpinBox / RichEditHost）：builder 只创建
// 控件不会调 EnsureCreated —— caller 在 host HWND 就绪后必须自己调一次，
// 跟 BuildEdit (DuiEditHost) 是同一个约定。
// =====================================================================

// ─── 枚举值字符串 → enum 的 helper（COLOR / int / bool 已有通用 parser）─

#if BUI_FEATURE_AVATAR
DuiAvatar::Shape ParseAvatarShape(const std::string& s, DuiAvatar::Shape def)
{
    if (s == "circle")
    {
        return DuiAvatar::ShapeCircle;
    }
    if (s == "round-rect" || s == "rounded-rect")
    {
        return DuiAvatar::ShapeRoundRect;
    }
    return def;
}

DuiAvatar::Status ParseAvatarStatus(const std::string& s, DuiAvatar::Status def)
{
    if (s == "none")
    {
        return DuiAvatar::StatusNone;
    }
    if (s == "online")
    {
        return DuiAvatar::StatusOnline;
    }
    if (s == "away")
    {
        return DuiAvatar::StatusAway;
    }
    if (s == "busy")
    {
        return DuiAvatar::StatusBusy;
    }
    if (s == "offline")
    {
        return DuiAvatar::StatusOffline;
    }
    return def;
}
#endif // BUI_FEATURE_AVATAR

#if BUI_FEATURE_SEPARATOR
DuiSeparator::Orientation ParseSepOrient(const std::string& s,
                                         DuiSeparator::Orientation def)
{
    if (s == "horizontal")
    {
        return DuiSeparator::Horizontal;
    }
    if (s == "vertical")
    {
        return DuiSeparator::Vertical;
    }
    return def;
}
#endif // BUI_FEATURE_SEPARATOR

#if BUI_FEATURE_SPLITTER
DuiSplitter::Orientation ParseSplitterOrient(const std::string& s,
                                             DuiSplitter::Orientation def)
{
    if (s == "vertical")
    {
        return DuiSplitter::Vertical;
    }
    if (s == "horizontal")
    {
        return DuiSplitter::Horizontal;
    }
    return def;
}
#endif // BUI_FEATURE_SPLITTER

// ─── Build* 实现 ───────────────────────────────────────────────────

#if BUI_FEATURE_AVATAR
std::unique_ptr<DuiControl> BuildAvatar(const DuiXmlBuilder::Node& n)
{
    auto a = std::unique_ptr<DuiAvatar>(new DuiAvatar());
    DuiAvatar* raw = a.get();
    ApplyCommon(raw, n);

    std::string s = Get(n, "name");
    if (!s.empty())
    {
        raw->SetName(ToCString(s));
    }
    s = Get(n, "fallback-bg-color");
    if (!s.empty())
    {
        raw->SetFallbackBgColor(ParseColor(s, RGB(45, 108, 223)));
    }
    s = Get(n, "initials-color");
    if (!s.empty())
    {
        raw->SetInitialsColor(ParseColor(s, RGB(255, 255, 255)));
    }
    s = Get(n, "shape");
    if (!s.empty())
    {
        raw->SetShape(ParseAvatarShape(s, DuiAvatar::ShapeCircle));
    }
    s = Get(n, "corner-radius");
    if (!s.empty())
    {
        raw->SetCornerRadius(ParseInt(s, 8));
    }
    s = Get(n, "status");
    if (!s.empty())
    {
        raw->SetStatus(ParseAvatarStatus(s, DuiAvatar::StatusNone));
    }
    return a;
}
#endif // BUI_FEATURE_AVATAR

#if BUI_FEATURE_BADGE
std::unique_ptr<DuiControl> BuildBadge(const DuiXmlBuilder::Node& n)
{
    auto b = std::unique_ptr<DuiBadge>(new DuiBadge());
    DuiBadge* raw = b.get();
    ApplyCommon(raw, n);

    // count 优先于 text —— count 走 SetCount（"99+" 处理逻辑）。
    // 都不写时 badge 默认空字符串 + hide-when-empty=true → 不显示。
    std::string countS = Get(n, "count");
    if (!countS.empty())
    {
        raw->SetCount(ParseInt(countS, 0));
    }
    else
    {
        std::string textS = Get(n, "text");
        if (!textS.empty())
        {
            raw->SetText(ToCString(textS));
        }
    }
    std::string s = Get(n, "bg-color");
    if (!s.empty())
    {
        raw->SetBgColor(ParseColor(s, RGB(220, 60, 60)));
    }
    s = Get(n, "text-color");
    if (!s.empty())
    {
        raw->SetTextColor(ParseColor(s, RGB(255, 255, 255)));
    }
    s = Get(n, "hide-when-empty");
    if (!s.empty())
    {
        raw->SetHideWhenEmpty(ParseBool(s, true));
    }
    return b;
}
#endif // BUI_FEATURE_BADGE

#if BUI_FEATURE_SEPARATOR
std::unique_ptr<DuiControl> BuildSeparator(const DuiXmlBuilder::Node& n)
{
    auto sep = std::unique_ptr<DuiSeparator>(new DuiSeparator());
    DuiSeparator* raw = sep.get();
    ApplyCommon(raw, n);

    std::string s = Get(n, "orientation");
    if (!s.empty())
    {
        raw->SetOrientation(ParseSepOrient(s, DuiSeparator::Horizontal));
    }
    s = Get(n, "color");
    if (!s.empty())
    {
        raw->SetColor(ParseColor(s, RGB(220, 220, 224)));
    }
    s = Get(n, "thickness");
    if (!s.empty())
    {
        raw->SetThickness(ParseInt(s, 1));
    }
    s = Get(n, "inset");
    if (!s.empty())
    {
        raw->SetInset(ParseInt(s, 0));
    }
    return sep;
}
#endif // BUI_FEATURE_SEPARATOR

#if BUI_FEATURE_SLIDER
std::unique_ptr<DuiControl> BuildSlider(const DuiXmlBuilder::Node& n)
{
    auto sl = std::unique_ptr<DuiSlider>(new DuiSlider());
    DuiSlider* raw = sl.get();
    ApplyCommon(raw, n);

    // 范围必须先设：SetPos 会按范围 clamp。读 min/max 再读 value。
    std::string sMin = Get(n, "min");
    std::string sMax = Get(n, "max");
    if (!sMin.empty() || !sMax.empty())
    {
        int mn = sMin.empty() ? raw->GetMin() : ParseInt(sMin, 0);
        int mx = sMax.empty() ? raw->GetMax() : ParseInt(sMax, 100);
        raw->SetRange(mn, mx);
    }
    std::string s = Get(n, "value");
    if (!s.empty())
    {
        raw->SetPos(ParseInt(s, 0), false);   // notify=false：构造期不通知
    }
    s = Get(n, "line-size");
    if (!s.empty())
    {
        raw->SetLineSize(ParseInt(s, 1));
    }
    s = Get(n, "track-height");
    if (!s.empty())
    {
        raw->SetTrackHeight(ParseInt(s, 4));
    }
    s = Get(n, "thumb-size");
    if (!s.empty())
    {
        raw->SetThumbSize(ParseInt(s, 14));
    }
    s = Get(n, "track-color");
    if (!s.empty())
    {
        raw->SetTrackColor(ParseColor(s, RGB(220, 220, 226)));
    }
    s = Get(n, "fill-color");
    if (!s.empty())
    {
        raw->SetFillColor(ParseColor(s, RGB(45, 108, 223)));
    }
    s = Get(n, "thumb-color");
    if (!s.empty())
    {
        raw->SetThumbColor(ParseColor(s, RGB(255, 255, 255)));
    }
    s = Get(n, "vertical");
    if (!s.empty())
    {
        raw->SetVertical(ParseBool(s, false));
    }
    s = Get(n, "tick-frequency");
    if (!s.empty())
    {
        raw->SetTickFrequency(ParseInt(s, 0));
    }
    s = Get(n, "tick-color");
    if (!s.empty())
    {
        raw->SetTickColor(ParseColor(s, RGB(150, 150, 160)));
    }
    return sl;
}
#endif // BUI_FEATURE_SLIDER

#if BUI_FEATURE_SWITCH
std::unique_ptr<DuiControl> BuildSwitch(const DuiXmlBuilder::Node& n)
{
    auto sw = std::unique_ptr<DuiSwitch>(new DuiSwitch());
    DuiSwitch* raw = sw.get();
    ApplyCommon(raw, n);

    std::string s = Get(n, "checked");
    if (!s.empty())
    {
        // 构造期：notify=false，避免"刚装上就发一次假事件"。
        raw->SetChecked(ParseBool(s, false), /*animated=*/false, /*notify=*/false);
    }
    s = Get(n, "animated");
    if (!s.empty())
    {
        raw->SetAnimated(ParseBool(s, true));
    }
    s = Get(n, "on-color");
    if (!s.empty())
    {
        raw->SetOnColor(ParseColor(s, RGB(7, 193, 96)));
    }
    s = Get(n, "off-color");
    if (!s.empty())
    {
        raw->SetOffColor(ParseColor(s, RGB(229, 229, 229)));
    }
    s = Get(n, "knob-color");
    if (!s.empty())
    {
        raw->SetKnobColor(ParseColor(s, RGB(255, 255, 255)));
    }
    return sw;
}
#endif // BUI_FEATURE_SWITCH

#if BUI_FEATURE_PROGRESSBAR
std::unique_ptr<DuiControl> BuildProgress(const DuiXmlBuilder::Node& n)
{
    auto p = std::unique_ptr<DuiProgressBar>(new DuiProgressBar());
    DuiProgressBar* raw = p.get();
    ApplyCommon(raw, n);

    std::string sMin = Get(n, "min");
    std::string sMax = Get(n, "max");
    if (!sMin.empty() || !sMax.empty())
    {
        int mn = sMin.empty() ? raw->GetMin() : ParseInt(sMin, 0);
        int mx = sMax.empty() ? raw->GetMax() : ParseInt(sMax, 100);
        raw->SetRange(mn, mx);
    }
    std::string s = Get(n, "value");
    if (!s.empty())
    {
        raw->SetPos(ParseInt(s, 0), false);
    }
    s = Get(n, "text");
    if (!s.empty())
    {
        raw->SetText(ToCString(s));
    }
    s = Get(n, "show-percent");
    if (!s.empty())
    {
        raw->SetShowPercent(ParseBool(s, true));
    }
    s = Get(n, "vertical");
    if (!s.empty())
    {
        raw->SetVertical(ParseBool(s, false));
    }
    s = Get(n, "marquee");
    if (!s.empty())
    {
        raw->SetMarquee(ParseBool(s, false));
    }
    s = Get(n, "bg-color");
    if (!s.empty())
    {
        raw->SetBgColor(ParseColor(s, RGB(232, 232, 236)));
    }
    s = Get(n, "fill-color");
    if (!s.empty())
    {
        raw->SetFillColor(ParseColor(s, RGB(45, 108, 223)));
    }
    s = Get(n, "border-color");
    if (!s.empty())
    {
        raw->SetBorderColor(ParseColor(s, RGB(180, 180, 188)));
    }
    s = Get(n, "text-color");
    if (!s.empty())
    {
        raw->SetTextColor(ParseColor(s, RGB(30, 30, 30)));
    }
    return p;
}
#endif // BUI_FEATURE_PROGRESSBAR

#if BUI_FEATURE_SPLITTER
std::unique_ptr<DuiControl> BuildSplitter(const DuiXmlBuilder::Node& n,
                                          const DuiXmlBuilder::CustomFactory& fac)
{
    auto sp = std::unique_ptr<DuiSplitter>(new DuiSplitter());
    DuiSplitter* raw = sp.get();
    ApplyCommon(raw, n);

    std::string s = Get(n, "orientation");
    if (!s.empty())
    {
        raw->SetOrientation(ParseSplitterOrient(s, DuiSplitter::Vertical));
    }
    s = Get(n, "bar-thickness");
    if (!s.empty())
    {
        raw->SetBarThickness(ParseInt(s, 4));
    }
    std::string sMin0 = Get(n, "min-size-0");
    std::string sMin1 = Get(n, "min-size-1");
    if (!sMin0.empty() || !sMin1.empty())
    {
        int m0 = sMin0.empty() ? raw->GetMinSize0() : ParseInt(sMin0, 40);
        int m1 = sMin1.empty() ? raw->GetMinSize1() : ParseInt(sMin1, 40);
        raw->SetMinSizes(m0, m1);
    }
    s = Get(n, "split-px");
    if (!s.empty())
    {
        raw->SetSplitPx(ParseInt(s, 100));
    }
    else
    {
        s = Get(n, "split-fraction");
        if (!s.empty())
        {
            // 简易 atof（项目里没专门 ParseFloat helper —— 用 sscanf）
            double f = 0.0;
            if (std::sscanf(s.c_str(), "%lf", &f) == 1)
            {
                raw->SetSplitFraction(f);
            }
        }
    }

    // 头两个子元素 → pane[0] / pane[1]，多余的忽略
    int paneIdx = 0;
    for (auto& child : n.children)
    {
        if (paneIdx >= 2)
        {
            break;
        }
        auto built = BuildOne(child, fac);
        if (!built)
        {
            continue;
        }
        raw->SetPane(paneIdx, std::move(built));
        ++paneIdx;
    }
    return sp;
}
#endif // BUI_FEATURE_SPLITTER

#if BUI_FEATURE_GROUPBOX
std::unique_ptr<DuiControl> BuildGroupBox(const DuiXmlBuilder::Node& n,
                                          const DuiXmlBuilder::CustomFactory& fac)
{
    auto gb = std::unique_ptr<DuiGroupBox>(new DuiGroupBox());
    DuiGroupBox* raw = gb.get();
    ApplyCommon(raw, n);

    std::string s = Get(n, "title");
    if (!s.empty())
    {
        raw->SetTitle(ToCString(s));
    }
    s = Get(n, "title-color");
    if (!s.empty())
    {
        raw->SetTitleColor(ParseColor(s, RGB(60, 60, 80)));
    }
    s = Get(n, "border-color");
    if (!s.empty())
    {
        raw->SetBorderColor(ParseColor(s, RGB(200, 200, 208)));
    }
    s = Get(n, "corner-radius");
    if (!s.empty())
    {
        raw->SetCornerRadius(ParseInt(s, 6));
    }
    s = Get(n, "title-strip-height");
    if (!s.empty())
    {
        raw->SetTitleStripHeight(ParseInt(s, 24));
    }
    s = Get(n, "padding");
    if (!s.empty())
    {
        int l = 0, t = 0, r = 0, b = 0;
        if (std::sscanf(s.c_str(), "%d,%d,%d,%d", &l, &t, &r, &b) == 4)
        {
            raw->SetPadding(l, t, r, b);
        }
        else
        {
            raw->SetPadding(ParseInt(s, 12));
        }
    }

    // 第一个子元素 → SetContent；多余的忽略
    for (auto& child : n.children)
    {
        auto built = BuildOne(child, fac);
        if (built)
        {
            raw->SetContent(std::move(built));
            break;
        }
    }
    return gb;
}
#endif // BUI_FEATURE_GROUPBOX

#if BUI_FEATURE_SEARCHBOX
std::unique_ptr<DuiControl> BuildSearchBox(const DuiXmlBuilder::Node& n)
{
    auto sb = std::unique_ptr<DuiSearchBox>(new DuiSearchBox());
    DuiSearchBox* raw = sb.get();
    ApplyCommon(raw, n);

    std::string s = Get(n, "placeholder");
    if (!s.empty())
    {
        raw->SetPlaceholder(ToCString(s));
    }
    s = Get(n, "read-only");
    if (!s.empty())
    {
        raw->SetReadOnly(ParseBool(s, false));
    }
    s = Get(n, "max-length");
    if (!s.empty())
    {
        raw->SetMaxLength(ParseInt(s, 0));
    }
    s = Get(n, "glyph-strip-width");
    if (!s.empty())
    {
        raw->SetGlyphStripWidth(ParseInt(s, 24));
    }
    s = Get(n, "clear-strip-width");
    if (!s.empty())
    {
        raw->SetClearStripWidth(ParseInt(s, 22));
    }
    return sb;
}
#endif // BUI_FEATURE_SEARCHBOX

#if BUI_FEATURE_SPINBOX
std::unique_ptr<DuiControl> BuildSpinBox(const DuiXmlBuilder::Node& n)
{
    auto sp = std::unique_ptr<DuiSpinBox>(new DuiSpinBox());
    DuiSpinBox* raw = sp.get();
    ApplyCommon(raw, n);

    std::string sMin = Get(n, "min");
    std::string sMax = Get(n, "max");
    if (!sMin.empty() || !sMax.empty())
    {
        int mn = sMin.empty() ? raw->GetMinValue() : ParseInt(sMin, 0);
        int mx = sMax.empty() ? raw->GetMaxValue() : ParseInt(sMax, 100);
        raw->SetRange(mn, mx);
    }
    std::string s = Get(n, "step");
    if (!s.empty())
    {
        raw->SetStep(ParseInt(s, 1));
    }
    s = Get(n, "value");
    if (!s.empty())
    {
        raw->SetValue(ParseInt(s, 0), false);
    }
    s = Get(n, "wrap");
    if (!s.empty())
    {
        raw->SetWrap(ParseBool(s, false));
    }
    s = Get(n, "spin-strip-width");
    if (!s.empty())
    {
        raw->SetSpinStripWidth(ParseInt(s, 18));
    }
    return sp;
}
#endif // BUI_FEATURE_SPINBOX

#if BUI_FEATURE_RICHEDIT
std::unique_ptr<DuiControl> BuildRichEdit(const DuiXmlBuilder::Node& n)
{
    auto re = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
    DuiRichEditHost* raw = re.get();
    ApplyCommon(raw, n);

    // multi-line / word-wrap 必须在 EnsureCreated 前设 —— ES_MULTILINE
    // 等是 baked-in 的窗口风格。XML 解析阶段总是 pre-Create 的，所以
    // 这里直接调即可。
    std::string s = Get(n, "multi-line");
    if (!s.empty())
    {
        raw->SetMultiLine(ParseBool(s, true));
    }
    s = Get(n, "word-wrap");
    if (!s.empty())
    {
        raw->SetWordWrap(ParseBool(s, true));
    }
    s = Get(n, "placeholder");
    if (!s.empty())
    {
        raw->SetPlaceholder(ToCString(s));
    }
    s = Get(n, "read-only");
    if (!s.empty())
    {
        raw->SetReadOnly(ParseBool(s, false));
    }
    s = Get(n, "max-length");
    if (!s.empty())
    {
        raw->SetMaxLength(ParseInt(s, 0));
    }
    s = Get(n, "text-color");
    if (!s.empty())
    {
        raw->SetTextColor(ParseColor(s, RGB(30, 30, 30)));
    }
    s = Get(n, "bg-color");
    if (!s.empty())
    {
        raw->SetBackgroundColor(ParseColor(s, RGB(255, 255, 255)));
    }
    s = Get(n, "auto-url-detect");
    if (!s.empty())
    {
        raw->SetAutoUrlDetect(ParseBool(s, false));
    }
    s = Get(n, "margins");
    if (!s.empty())
    {
        int l = 4, t = 2, r = 4, b = 2;
        if (std::sscanf(s.c_str(), "%d,%d,%d,%d", &l, &t, &r, &b) == 4)
        {
            raw->SetMargins(l, t, r, b);
        }
    }
    return re;
}
#endif // BUI_FEATURE_RICHEDIT

#if BUI_FEATURE_TREEVIEW
// ─── DuiTreeView (XML 范围 B：仅 <column> 子元素，节点仍由 C++ 添加) ───
//
// <treeview id="100" row-height="28" header-height="26"
//           indent="18" frozen-cols="1" frozen-rows="0"
//           editable="false" zebra="false">
//     <column title="名称" width="200" min-width="60" align="left" sortable="true" editable="true"/>
//     <column title="大小" width="80"  align="right" sortable="true"/>
//     ...
// </treeview>
std::unique_ptr<DuiControl> BuildTreeView(const DuiXmlBuilder::Node& n)
{
    auto t = std::unique_ptr<DuiTreeView>(new DuiTreeView());
    DuiTreeView* raw = t.get();
    ApplyCommon(raw, n);

    std::string s = Get(n, "row-height");
    if (!s.empty()) { raw->SetRowHeight(ParseInt(s, 28)); }
    s = Get(n, "header-height");
    if (!s.empty()) { raw->SetHeaderHeight(ParseInt(s, 26)); }
    s = Get(n, "indent");
    if (!s.empty()) { raw->SetIndentPx(ParseInt(s, 18)); }
    s = Get(n, "editable");
    if (!s.empty()) { raw->SetEditable(ParseBool(s, false)); }
    s = Get(n, "zebra");
    if (!s.empty()) { raw->SetZebra(ParseBool(s, false)); }

    // First pass: parse all <column> children in order.
    for (auto& child : n.children)
    {
        if (child.tag != "column")
        {
            continue;
        }
        std::string title    = Get(child, "title");
        int         width    = ParseInt(Get(child, "width"),     120);
        int         minW     = ParseInt(Get(child, "min-width"),
                                        DuiTreeView::kColMinWidthDefault);
        std::string align    = Get(child, "align");
        bool        sortable = ParseBool(Get(child, "sortable"), true);
        bool        editable = ParseBool(Get(child, "editable"), true);
        UINT        dt = DT_LEFT;
        if      (align == "center") { dt = DT_CENTER; }
        else if (align == "right")  { dt = DT_RIGHT;  }
        int colIdx = raw->AddColumn(ToCString(title), width, minW, dt, sortable);
        raw->SetColumnEditable(colIdx, editable);
    }

    // Frozen panes / rows are read after columns so SetFrozenColumns can
    // clamp against the number of columns just added.
    s = Get(n, "frozen-cols");
    if (!s.empty()) { raw->SetFrozenColumns(ParseInt(s, 0)); }
    s = Get(n, "frozen-rows");
    if (!s.empty()) { raw->SetFrozenRows(ParseInt(s, 0)); }

    return t;
}
#endif // BUI_FEATURE_TREEVIEW

#if BUI_FEATURE_MENUBAR
// ─── DuiMenuBar (XML 范围：栏目结构 + 文字 + id；dropdown 后置绑) ────
//
// <menu-bar id="100" item-height="24">
//   <menu-item id="101" text="文件(&amp;F)"/>
//   <menu-item id="102" text="选项(&amp;O)"/>
//   <menu-item id="103" text="查看(&amp;V)"/>
// </menu-bar>
//
// 业务侧拿到 builder 返回的 root 后，FindControlById(100) → SetDropdown(idx, &menu)
// 把每栏关联到自己的 DuiMenu 实例。XML 不支持声明菜单项内容（DuiMenu
// 是事件触发的瞬时弹层）。
std::unique_ptr<DuiControl> BuildMenuBar(const DuiXmlBuilder::Node& n)
{
    auto bar = std::unique_ptr<DuiMenuBar>(new DuiMenuBar());
    DuiMenuBar* raw = bar.get();
    ApplyCommon(raw, n);

    std::string s = Get(n, "item-height");
    if (!s.empty()) { raw->SetItemHeight(ParseInt(s, 24)); }

    for (auto& child : n.children)
    {
        if (child.tag != "menu-item")
        {
            continue;
        }
        UINT    nID  = (UINT)ParseInt(Get(child, "id"), 0);
        CString text = ToCString(Get(child, "text"));
        raw->AppendItem(nID, text.GetString(), /*dropdown=*/nullptr);
    }
    return bar;
}
#endif // BUI_FEATURE_MENUBAR

// ─── BuildOne：tag → 控件 dispatch ─────────────────────────────────
//
// 每条分支都用 BUI_FEATURE_XXX 宏门控。feature 关掉时该分支整段消失，
// 对应 tag 走最底部的 "miss" 路径：先尝试 caller 的 CustomFactory，
// 没匹配上再 OutputDebugString 一行警告 + 返回 nullptr。这样业务方
// 在 XML 里不小心写了一个被裁剪掉的 tag 时，不会静默失败 —— 调试器
// Output 窗口会立即提示是哪个 tag、是不是 feature 没开。
std::unique_ptr<DuiControl> BuildOne(const DuiXmlBuilder::Node& n,
                                     const DuiXmlBuilder::CustomFactory& fac)
{
    // Layout 容器始终可用（无 feature 开关）。
    if (n.tag == "vbox")
    {
        return BuildLayout<DuiVBox>(n, false, fac);
    }
    else if (n.tag == "hbox")
    {
        return BuildLayout<DuiHBox>(n, true, fac);
    }
    else if (n.tag == "grid")
    {
        return BuildGrid(n, fac);
    }
#if BUI_FEATURE_BUTTON
    else if (n.tag == "button")
    {
        return BuildButton(n);
    }
#endif
#if BUI_FEATURE_LABEL
    else if (n.tag == "label")
    {
        return BuildLabel(n);
    }
#endif
#if BUI_FEATURE_EDIT
    else if (n.tag == "edit")
    {
        return BuildEdit(n);
    }
#endif
    // V2 — 无 model 简单控件
#if BUI_FEATURE_AVATAR
    else if (n.tag == "avatar")
    {
        return BuildAvatar(n);
    }
#endif
#if BUI_FEATURE_BADGE
    else if (n.tag == "badge")
    {
        return BuildBadge(n);
    }
#endif
#if BUI_FEATURE_SEPARATOR
    else if (n.tag == "separator")
    {
        return BuildSeparator(n);
    }
#endif
#if BUI_FEATURE_SLIDER
    else if (n.tag == "slider")
    {
        return BuildSlider(n);
    }
#endif
#if BUI_FEATURE_SWITCH
    else if (n.tag == "switch")
    {
        return BuildSwitch(n);
    }
#endif
#if BUI_FEATURE_PROGRESSBAR
    else if (n.tag == "progress")
    {
        return BuildProgress(n);
    }
#endif
#if BUI_FEATURE_SPLITTER
    else if (n.tag == "splitter")
    {
        return BuildSplitter(n, fac);
    }
#endif
#if BUI_FEATURE_GROUPBOX
    else if (n.tag == "groupbox")
    {
        return BuildGroupBox(n, fac);
    }
#endif
#if BUI_FEATURE_SEARCHBOX
    else if (n.tag == "searchbox")
    {
        return BuildSearchBox(n);
    }
#endif
#if BUI_FEATURE_SPINBOX
    else if (n.tag == "spinbox")
    {
        return BuildSpinBox(n);
    }
#endif
#if BUI_FEATURE_RICHEDIT
    else if (n.tag == "richedit")
    {
        return BuildRichEdit(n);
    }
#endif
#if BUI_FEATURE_TREEVIEW
    else if (n.tag == "treeview")
    {
        return BuildTreeView(n);
    }
#endif
#if BUI_FEATURE_MENUBAR
    else if (n.tag == "menu-bar")
    {
        return BuildMenuBar(n);
    }
#endif
    // Unknown built-in tag — defer to the caller's custom factory if any.
    if (fac)
    {
        auto built = fac(n);
        if (built)
        {
            ApplyCommon(built.get(), n);
            // 自定义 factory 返 layout 子类（DuiVBox / DuiHBox / DuiGrid /
            // 业务子类如 ColorPanel）时，把 padding / gap 这两个布局属性也
            // 应用上 —— 否则 caller 必须自己在 factory 里手动写 SetPadding /
            // SetGap，重复且容易漏。BuildLayout 也走这套，行为对齐。
            DuiLayout* asLayout = dynamic_cast<DuiLayout*>(built.get());
            if (asLayout)
            {
                ParsePadding(asLayout, n);
                int gap = ParseInt(Get(n, "gap"), 0);
                if (gap > 0)
                {
                    asLayout->SetGap(gap);
                }
            }
            // 给 child 算 hint 时，parentIsH 必须按容器实际方向决定 ——
            // 默认 true（HBox / Grid 的语义），如果 built 是 VBox-derived
            // 才切 false。这样自定义 VBox（如 ColorPanel）的子 fixedHeight
            // 才会正确映射到 fixedMain（不然就被当成 fixedCross，所有 fixed
            // 子都退化成 weight 平分主轴，layout 全乱）。
            bool meIsH = (dynamic_cast<DuiVBox*>(built.get()) == nullptr);
            for (auto& c : n.children)
            {
                auto sub = BuildOne(c, fac);
                if (sub)
                {
                    if (asLayout)
                    {
                        DuiLayout::Hint hint = MakeHint(c, meIsH);
                        asLayout->AddChild(std::move(sub), hint);
                    }
                    else
                    {
                        built->AddChild(std::move(sub));
                    }
                }
            }
            return built;
        }
    }
    // Tag dispatch miss —— 既不是已知内置 tag，也没有被 CustomFactory 接住。
    // 三种典型原因：
    //   1) 业务侧拼写错（"buttn"）
    //   2) 业务侧用了一个 BUI_FEATURE_XXX 关掉的 tag（"treeview"、"richedit"…）
    //   3) 业务侧自定义 tag 但没注册 CustomFactory
    // 任何一种都要让 user 知道；OutputDebugString 落到 VS Output 窗口（或
    // DebugView），不抛异常 / 不 abort —— Release 构建里也能跑、只是 UI
    // 缺一块。
    {
        // 打印类似 "[balloonui] DuiXmlBuilder: unknown tag <treeview>
        // (feature disabled or factory missing)\n" 的一行。tag 是 std::string
        // (UTF-8)，这里不强转 wide —— OutputDebugStringA 可直接吃 UTF-8
        // 经 ANSI 解读，西文 tag 名都是 ASCII，能正确显示。
        std::string msg;
        msg.reserve(n.tag.size() + 96);
        msg += "[balloonui] DuiXmlBuilder: unknown tag <";
        msg += n.tag;
        msg += "> (feature disabled or factory missing)\n";
        ::OutputDebugStringA(msg.c_str());
    }
    return nullptr;
}

} // anonymous

std::unique_ptr<DuiControl> DuiXmlBuilder::Build(const Node& root,
                                                 const CustomFactory& factory)
{
    return BuildOne(root, factory);
}

std::unique_ptr<DuiControl> DuiXmlBuilder::FromString(LPCSTR xmlUtf8,
                                                     const CustomFactory& factory)
{
    Node root;
    if (!Parse(xmlUtf8, root))
    {
        return nullptr;
    }
    return Build(root, factory);
}

std::unique_ptr<DuiControl> DuiXmlBuilder::FromString(LPCWSTR xmlW,
                                                     const CustomFactory& factory)
{
    if (!xmlW)
    {
        return nullptr;
    }
    int cb = ::WideCharToMultiByte(CP_UTF8, 0, xmlW, -1, nullptr, 0, nullptr, nullptr);
    if (cb <= 0)
    {
        return nullptr;
    }
    std::string s(cb, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, xmlW, -1, &s[0], cb, nullptr, nullptr);
    while (!s.empty() && s.back() == 0)
    {
        s.pop_back();
    }
    return FromString(s.c_str(), factory);
}

// =====================================================================
// FromFrameXml — top-level <frame-window> parser
//
// 整段（FillFrameConfig + ParseRect + ParseColorOrNone + 两个 FromFrameXml
// 重载）由 BUI_FEATURE_FRAMEWINDOW 门控。feature 关掉时 DuiFrameWindow
// 头不被 #include、DuiFrameWindowConfig 不存在，连 FromFrameXml 都不
// 能编译；同样 .h 里 FromFrameXml 的声明也已被门控。
// =====================================================================

#if BUI_FEATURE_FRAMEWINDOW

namespace {

// 解析 RECT (insets / margin) — 接受 "l,t,r,b" 或 "n"（4 边相同）。
// 解析失败返回 def。
RECT ParseRect(const std::string& s, RECT def)
{
    if (s.empty())
    {
        return def;
    }
    int l = 0, t = 0, r = 0, b = 0;
    int n = std::sscanf(s.c_str(), "%d,%d,%d,%d", &l, &t, &r, &b);
    if (n == 4)
    {
        return RECT{ l, t, r, b };
    }
    if (n == 1)
    {
        return RECT{ l, l, l, l };
    }
    // 其它情况（部分匹配）按"无效"处理，保留默认。
    return def;
}

// COLORREF + "none" 关键字 — 用于 client-border-color="none" 关闭边框。
// 解析失败返回 def；"none"/"None" 返回 CLR_INVALID。
COLORREF ParseColorOrNone(const std::string& s, COLORREF def)
{
    if (s.empty())
    {
        return def;
    }
    if (s == "none" || s == "None" || s == "NONE")
    {
        return CLR_INVALID;
    }
    return ParseColor(s, def);
}

// 把 cfg 字段从 attr map 抽出来。任何属性不存在 → optional 留空 → 不调
// 对应 setter（保留 frame 默认）。属性存在但 value 解析失败 → 也按
// "未设"处理（避免坏 XML 静默覆盖默认）—— 用 sentinel def 检测。
void FillFrameConfig(const DuiXmlBuilder::Node& n,
                     DuiFrameWindowConfig& cfg)
{
    // ── 标题 / 标题栏 ──
    if (n.attrs.count("title"))
    {
        cfg.title = ToCString(Get(n, "title"));
    }
    if (n.attrs.count("title-bar-height"))
    {
        // -1 sentinel —— 解析失败时不设
        int v = ParseInt(Get(n, "title-bar-height"), -1);
        if (v >= 0)
        {
            cfg.titleBarHeight = v;
        }
    }
    if (n.attrs.count("title-bar-transparent"))
    {
        cfg.titleBarTransparent = ParseBool(Get(n, "title-bar-transparent"), false);
    }
    if (n.attrs.count("title-text-color"))
    {
        // CLR_INVALID sentinel —— 解析失败时不设
        COLORREF c = ParseColorOrNone(Get(n, "title-text-color"), CLR_INVALID);
        if (c != CLR_INVALID)
        {
            cfg.titleTextColor = c;
        }
    }
    if (n.attrs.count("caption-glyph-color"))
    {
        COLORREF c = ParseColorOrNone(Get(n, "caption-glyph-color"), CLR_INVALID);
        if (c != CLR_INVALID)
        {
            cfg.captionGlyphColor = c;
        }
    }

    // ── 三按钮可见性 ──
    if (n.attrs.count("has-min-button"))
    {
        cfg.hasMinButton = ParseBool(Get(n, "has-min-button"), true);
    }
    if (n.attrs.count("has-max-button"))
    {
        cfg.hasMaxButton = ParseBool(Get(n, "has-max-button"), true);
    }
    if (n.attrs.count("has-close-button"))
    {
        cfg.hasCloseButton = ParseBool(Get(n, "has-close-button"), true);
    }

    // ── 尺寸 / 边 ──
    if (n.attrs.count("min-w"))
    {
        int v = ParseInt(Get(n, "min-w"), -1);
        if (v > 0)
        {
            cfg.minW = v;
        }
    }
    if (n.attrs.count("min-h"))
    {
        int v = ParseInt(Get(n, "min-h"), -1);
        if (v > 0)
        {
            cfg.minH = v;
        }
    }
    if (n.attrs.count("resizable"))
    {
        cfg.resizable = ParseBool(Get(n, "resizable"), true);
    }
    if (n.attrs.count("border-px"))
    {
        int v = ParseInt(Get(n, "border-px"), -1);
        if (v >= 0)
        {
            cfg.borderPx = v;
        }
    }
    if (n.attrs.count("client-border-color"))
    {
        // 这里允许 CLR_INVALID 进入 cfg —— "none" 关闭边框是合法语义
        cfg.clientBorderColor =
            ParseColorOrNone(Get(n, "client-border-color"), CLR_INVALID);
    }

    // ── bg image ──
    // bg-image 路径在 caller 端（FromFrameXml）解析；这里只取原始字符串。
    if (n.attrs.count("bg-image"))
    {
        cfg.bgImagePath = ToCString(Get(n, "bg-image"));
    }
    if (n.attrs.count("bg-src-insets"))
    {
        cfg.bgSrcInsets = ParseRect(Get(n, "bg-src-insets"), RECT{ 0, 0, 0, 0 });
    }
    if (n.attrs.count("bg-dst-insets"))
    {
        cfg.bgDstInsets = ParseRect(Get(n, "bg-dst-insets"), RECT{ 0, 0, 0, 0 });
    }
}

} // anonymous

std::unique_ptr<DuiControl> DuiXmlBuilder::FromFrameXml(LPCSTR xmlUtf8,
                                                       DuiFrameWindowConfig& outConfig,
                                                       const CustomFactory& factory)
{
    Node root;
    if (!Parse(xmlUtf8, root))
    {
        return nullptr;
    }
    if (root.tag != "frame-window")
    {
        // 顶层标签不对 —— 拒绝解析。caller 拿到 nullptr + cfg 全空，
        // 应当走 fallback（自己 C++ 配置）或报错给用户。
        return nullptr;
    }

    FillFrameConfig(root, outConfig);

    // 把相对路径解析成绝对（exe 目录基）—— 写回 cfg，让调用方拿到的
    // 是个能直接 LoadBgImageFromFile 的绝对路径。
    if (!outConfig.bgImagePath.IsEmpty())
    {
        outConfig.bgImagePath = ResolveAssetPath(outConfig.bgImagePath);
    }

    // 客户区控件树 = <frame-window> 内的<u>第一个</u>子元素。多个时
    // 取第一个、忽略其余（与 SetClientContent 单一 root 的语义一致）。
    for (auto& child : root.children)
    {
        // Node::children 只可能是 element（解析器跳过了 text/CDATA）
        auto built = Build(child, factory);
        if (built)
        {
            return built;
        }
    }
    return nullptr;
}

std::unique_ptr<DuiControl> DuiXmlBuilder::FromFrameXml(LPCWSTR xmlW,
                                                       DuiFrameWindowConfig& outConfig,
                                                       const CustomFactory& factory)
{
    if (!xmlW)
    {
        return nullptr;
    }
    int cb = ::WideCharToMultiByte(CP_UTF8, 0, xmlW, -1, nullptr, 0, nullptr, nullptr);
    if (cb <= 0)
    {
        return nullptr;
    }
    std::string s(cb, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, xmlW, -1, &s[0], cb, nullptr, nullptr);
    while (!s.empty() && s.back() == 0)
    {
        s.pop_back();
    }
    return FromFrameXml(s.c_str(), outConfig, factory);
}

#endif // BUI_FEATURE_FRAMEWINDOW

CString DuiXmlBuilder::ResolveAssetPath(LPCTSTR userPath)
{
    if (!userPath || !*userPath)
    {
        return CString();
    }

    // 绝对路径检测 —— 不依赖 shlwapi（PathIsRelative）以减少依赖。
    // 三种绝对形式：
    //   · 盘符 + ":\\..."   或盘符 + ":/..."（如 C:\foo, D:/bar）
    //   · UNC "\\server\share\..." 或 "//server/share/..."
    // 单个反斜杠开头如 "\foo" 在 Win32 算"当前盘根"，按相对处理（极少
    // 见，处理成相对让用户自己写完整盘符即可）。
    auto isAbs = [](LPCTSTR p) -> bool
    {
        if (!p || !*p)
        {
            return false;
        }
        // 盘符
        if (((p[0] >= _T('A') && p[0] <= _T('Z'))
             || (p[0] >= _T('a') && p[0] <= _T('z')))
            && p[1] == _T(':')
            && (p[2] == _T('\\') || p[2] == _T('/')))
        {
            return true;
        }
        // UNC
        if ((p[0] == _T('\\') && p[1] == _T('\\'))
            || (p[0] == _T('/') && p[1] == _T('/')))
        {
            return true;
        }
        return false;
    };

    if (isAbs(userPath))
    {
        return CString(userPath);
    }

    // 相对路径 —— 拼到 exe 所在目录后。
    TCHAR exePath[MAX_PATH] = {};
    DWORD got = ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    if (got == 0 || got >= MAX_PATH)
    {
        // GetModuleFileName 失败（罕见）或路径被截断 —— 直接返回
        // userPath，让 LoadBgImageFromFile 做"文件不存在"分支
        return CString(userPath);
    }
    CString dir(exePath);
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0)
    {
        dir = dir.Left(slash);
    }

    CString out = dir;
    out += _T('\\');
    out += userPath;
    return out;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_XMLBUILDER
