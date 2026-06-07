#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_LABEL

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiLabel —— 文本标签 / 超链接（无 HWND）
// =================================================================
//
// 用途：显示一段静态文本，或当作可点击的超链接（蓝色下划线、hover 变色、
// 点击可发 DUIN_CLICK 或自动 ShellExecute URL）。
//
// 两种模式（SetMode 切换）：
//   · ModeText（默认）：纯文本，颜色 m_clrText，没有交互（仍接收鼠标
//     事件但不可聚焦）。
//   · ModeLink：下划线样式，三色（normal / hover / visited）。点击发
//     DUIN_CLICK 给父；如果 SetAutoNavigate(true) 则同时 ShellExecute
//     m_url（适合直接打开外部 URL）；false 则交给父决定语义（典型如
//     "注册账号"链接 → 父弹注册对话框）。
//
// 工作机制：
//   · ModeText 是纯绘制控件，不可聚焦，无通知。
//   · ModeLink hover 时光标变手型（IDC_HAND），LButton-up 时发
//     DUIN_CLICK；visited 标志跨 paint 持久（caller 想重置调
//     SetVisited(false)）。
//   · GetMnemonicChar 解析 "&X" 助记符（与 DuiButton / DuiMenu 同约定）。
//   · 下划线字体懒构造自 m_hFont；m_hFont 改时下划线版本一并重建；
//     析构时一并清理。
//   · SetWordWrap(true) 让 OnPaint / MeasureHeight 走 DT_WORDBREAK，
//     可在 m_rcItem 内换行；默认 false 单行。
//
// 代码用法（登录对话框里的"注册账号"链接）：
//
//     auto link = std::unique_ptr<DuiLabel>(new DuiLabel());
//     link->SetMode(DuiLabel::ModeLink);
//     link->SetText(_T("注册账号"));
//     link->SetAutoNavigate(false);     // 父决定点击行为
//     link->SetCtrlId(IDC_REGISTER_LINK);
//     parent->AddChild(std::move(link));
//     // 父的 OnDuiNotify：
//     //   if (code == DUIN_CLICK && id == IDC_REGISTER_LINK)
//     //       ShowRegisterDlg();
//
// XML 用法（详细属性见 guides.html §3.3.4）：
//
//     <label text="用户名" textColor="40,40,40" fixedHeight="22"/>
//
//   注：XML 暂不暴露 ModeLink / 字体 / 对齐 / 字号 等高级属性，需要时
//   在 C++ 侧拿到 builder 返回的 root 后用 FindControlById + SetXxx 自己设。
//
// 事件：
//   · ModeText：无。
//   · ModeLink：DUIN_CLICK — 用户在链接上 LButton-up；extra = 0。
//
// 替代关系：
//   · CSkinStatic            → ModeText
//   · CSkinHyperLink         → ModeLink + SetAutoNavigate(true) + SetUrl
//   · CRegisterSkinHyperLink → ModeLink + SetAutoNavigate(false)，父在
//                              DUIN_CLICK 里自己决定打开哪个对话框。
class BUI_API DuiLabel : public DuiControl
{
public:
    enum Mode { ModeText = 0, ModeLink = 1 };

    DuiLabel();
    ~DuiLabel() override;

    // ---- 文本 ----

    // 设置 / 读取文本。SetText 会触发 Invalidate 重绘。
    //   sz：任意字符串；nullptr 视为空；可含 "&X" 助记符（如 "&Save"）。
    void    SetText(LPCTSTR sz);
    CString GetText() const { return m_text; }

    // 解析 m_text 里的助记符字符。
    //   返回："&Save" → 's'；"&&None"（&& 是 "&" 转义，没有助记）→ 0；
    //         没有 & 标记 → 0。
    TCHAR   GetMnemonicChar() const;

    // 设置 / 读取 DrawText 对齐 flags 组合（DT_LEFT/DT_RIGHT/DT_CENTER/
    // DT_VCENTER/DT_SINGLELINE/...）。默认 DT_LEFT|DT_VCENTER|DT_SINGLELINE。
    void    SetTextAlign(DWORD dt) { m_dtFlags = dt; Invalidate(); }
    DWORD   GetTextAlign() const   { return m_dtFlags; }

    // 设置 / 读取文字色。默认 RGB(20,20,20)。
    void    SetTextColor(COLORREF c) { m_clrText = c; Invalidate(); }
    COLORREF GetTextColor() const    { return m_clrText; }

    // 设置 / 读取字体（caller 持有，控件不会 DeleteObject）。
    // 不设时用 DuiResMgr::GetDefaultFont()（微软雅黑 9pt）。
    void    SetFont(HFONT hFont)  { m_hFont = hFont; Invalidate(); }
    HFONT   GetFont() const       { return m_hFont; }

    // 设置 / 读取自动换行模式。
    //   b：true → OnPaint / MeasureHeight 走 DT_WORDBREAK（多行）；
    //     false（默认）→ 走 m_dtFlags（单行）。
    void    SetWordWrap(bool b);
    bool    IsWordWrap() const           { return m_wordWrap; }

    // 计算给定 client 宽度下、把 m_text 完整呈现需要的高度（px）。
    // 用 DrawText(DT_CALCRECT) + 当前字体（m_hFont 或 DuiResMgr 默认）
    // + 当前 word-wrap 标志算。空文本返回 0。
    //   width：>= 1 才有意义；<= 0 时按"无约束自然宽度"测量，等同单行
    //          高度。
    //   返回：所需高度像素。
    int     MeasureHeight(int width) const;

    // ---- 模式 ----

    // 切换 Text / Link 模式。
    void    SetMode(Mode m);
    Mode    GetMode() const { return m_mode; }

    // ---- Link 模式属性 ----

    // 设置 / 读取链接目标 URL。仅 SetAutoNavigate(true) 时被
    // ShellExecute 自动打开；否则只是元数据。
    void    SetUrl(LPCTSTR sz)              { m_url = sz ? sz : _T(""); }
    CString GetUrl() const                  { return m_url; }

    // 点击是否自动 ShellExecute m_url。默认 false（让父在 DUIN_CLICK
    // 里自己决定）。
    void    SetAutoNavigate(bool b)         { m_autoNav = b; }
    bool    GetAutoNavigate() const         { return m_autoNav; }

    // 三态颜色：normal / hover / visited。默认蓝 / 亮蓝 / 紫。
    void    SetLinkColor   (COLORREF c)     { m_clrLink    = c; Invalidate(); }
    void    SetHoverColor  (COLORREF c)     { m_clrHover   = c; Invalidate(); }
    void    SetVisitedColor(COLORREF c)     { m_clrVisited = c; Invalidate(); }

    // 设置 / 读取已访问标志（控制是否用 m_clrVisited 渲染）。
    bool    IsVisited() const               { return m_visited; }
    void    SetVisited(bool b)              { m_visited = b; Invalidate(); }

    // ---- 可选中（只读拖选 + Ctrl+C 复制） ----

    // 开启 / 关闭可选中模式；默认关。开启后本控件可获焦点，鼠标拖选高亮
    // 文字、Ctrl+C 复制选中（空选区时复制整段）、Ctrl+A 全选。**仅单行模式
    // 有效**：与 SetWordWrap(true) 共存时本能力自动失效（以 word-wrap 为准）。
    void    SetSelectable(bool b);
    bool    IsSelectable() const            { return m_selectable; }

    // 设置 / 读取选区高亮色。默认 RGB(217, 232, 252) 淡蓝。
    void    SetSelectionColor(COLORREF c)   { m_clrSel = c; Invalidate(); }
    COLORREF GetSelectionColor() const      { return m_clrSel; }

    // ---- 静态纯函数（暴露便于单测） ----

    // 给定 cumulative 字符宽度数组（dx[i] = 前 i+1 个字符的累计像素宽，
    // 语义同 GetTextExtentExPoint 的 lpnDx 输出），返回 xRelative 命中的字符
    // 索引 [0, len]。x<=0 返回 0；x 大于最末累计宽返回 len；其余取距离最近
    // 的字符边界。dx 为空 / len<=0 时返回 0。
    static int CharIndexFromCumulativeWidths(const int* dx, int len, int xRelative);

    // 选区规范化：把 (anchor, caret) 化为 [lo, hi)，lo <= hi。
    static void NormalizeSelection(int anchor, int caret, int& lo, int& hi);

    // 由文本 + 选区 (anchor, caret) 构造剪贴板内容。选区为空 (lo == hi)
    // 时返回整段文本（常见 Ctrl+C 行为）。
    static CString BuildCopyText(const CString& text, int anchor, int caret);

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnMouseMove (POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp (POINT pt, UINT mkFlags) override;
    bool    OnKeyDown   (UINT vk, UINT flags) override;
    bool    OnSetCursor (POINT pt) override;

private:
    COLORREF EffectiveColor() const;
    HFONT    EffectiveFont (HDC hdc) const;     // 返回 m_hFont，或为 link 模式构建下划线版本

    // ---- 选中模式内部辅助 ----

    // 把 client 坐标点命中到字符索引 [0, len]。考虑当前字体 + DT_LEFT/CENTER/RIGHT
    // 对齐。失败 / 空文本返回 0。
    int  HitTestCharIndex(POINT pt) const;

    // 在 OnPaint 已 SelectObject 字体的 HDC 上，画 [lo, hi) 区间的高亮矩形。
    void PaintSelectionHighlight(HDC hdc, int lo, int hi);

    // 把 text 写入剪贴板（CF_UNICODETEXT）。Open/Close 失败时静默忽略。
    static void CopyToClipboard(const CString& text);

private:
    Mode        m_mode      = ModeText;
    CString     m_text;
    DWORD       m_dtFlags   = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
    COLORREF    m_clrText   = RGB(20, 20, 20);
    HFONT       m_hFont     = nullptr;          // 不持有所有权
    bool        m_wordWrap  = false;            // true 时 OnPaint 走 DT_WORDBREAK

    CString     m_url;
    bool        m_autoNav   = false;
    bool        m_visited   = false;
    COLORREF    m_clrLink     = RGB(  0,  64, 192);
    COLORREF    m_clrHover    = RGB(  0,  96, 224);
    COLORREF    m_clrVisited  = RGB(112,  64, 160);

    // 缓存的下划线字体（持有所有权）。m_hFont 改变时重建。
    mutable HFONT m_hFontUnderline = nullptr;
    mutable HFONT m_lastBaseFont   = nullptr;   // 记录 m_hFontUnderline 是从哪个 base 构出的

    // ---- 选中模式状态（m_selectable=false 时不被读写）----
    bool        m_selectable   = false;          // 是否启用拖选 + Ctrl+C 复制
    int         m_selAnchor    = 0;              // 鼠标按下时的字符索引
    int         m_selCaret     = 0;              // 当前拖到 / 复制截止的字符索引；选区 = [min, max)
    bool        m_selDragging  = false;          // 鼠标正在拖选
    COLORREF    m_clrSel       = RGB(217, 232, 252);  // 选区高亮背景色
};

} // namespace balloonwjui

#endif // BUI_FEATURE_LABEL
