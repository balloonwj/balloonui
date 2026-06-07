#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_BADGE

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiBadge —— 未读计数 / 短标签徽章（无 HWND）
// =================================================================
//
// 用途：叠在头像、tab、列表项右上角，用一颗小红圆 / 椭圆胶囊显示
// 未读消息数（或一段超短文字，如 "NEW"、"VIP"）。
//
// 工作机制：
//   · 文本为 "1"–"99" 时画圆形或胶囊形，高 16–22px、宽自动适配文字
//     （最小正方形边 18px，确保读得清）。100+ 自动显示为 "99+"（4 字符
//     上限：超出截断）。
//   · 空文本 + HideWhenEmpty=true（默认）→ 完全不画；这样业务可以无脑
//     SetCount(unread) ，0 就自动消失。
//   · caller 通过 SetRect 决定徽章<u>位置</u>，徽章按文字内容自计算实际
//     绘制大小并在 m_rcItem 内居中 —— 也就是说 SetRect 给的是定位框，
//     不是实际形状。
//   · 不参与 hit-test、不接键盘焦点；只是叠加层。
//
// 代码用法：
//
//     auto badge = std::unique_ptr<DuiBadge>(new DuiBadge());
//     badge->SetCount(unread);     // 0 隐藏，1-99 数字，100+ 显示 "99+"
//     badge->SetRect(RECT{
//         avatar.right - 18, avatar.top - 4,
//         avatar.right + 4,  avatar.top + 14 });
//     parent->AddChild(std::move(badge));
//     // 收到新消息：
//     m_badge->SetCount(++unread);
//     m_badge->Invalidate();
//
// XML 用法（详细属性见 guides.html §3.3.7）：
//
//     <badge count="3"
//            bg-color="220,60,60"
//            text-color="255,255,255"
//            hide-when-empty="true"/>
//
//     <!-- 或者：用 text 显示自定义文本 -->
//     <badge text="NEW" bg-color="40,140,80"/>
//
// 事件：无（纯绘制控件，不发 WM_DUI_NOTIFY）。
class BUI_API DuiBadge : public DuiControl
{
public:
    DuiBadge();

    // 直接设置 / 读取文本。
    //   text：任意字符串；可中可英；nullptr 视为空。
    // 注意：本 setter 保存的是「原始文本」，不在此处截断。实际显示长度由
    // MaxDisplayChars 决定（默认 4，与历史行为一致 —— 仅"显示时"截到 4
    // 字符，<u>不</u>改写 m_text）。设 MaxDisplayChars(0) 即可呈现长文字标签。
    void    SetText(LPCTSTR text);
    // 返回的是 SetText 时传入的原始文本，<u>不是</u>截断后的显示文本。
    CString GetText() const { return m_text; }

    // 便捷方法：把整数计数转成显示文本（FormatCount 规则）：
    //   n <= 0       → ""    （配合 HideWhenEmpty=true 自动隐藏）
    //   1 <= n <= 99 → "<n>"
    //   n >= 100     → "99+"
    //   n：未读数等。
    void    SetCount(int n);

    // 设置 / 读取背景色（圆 / 胶囊填充色）。默认红 RGB(220,60,60)。
    //   c：COLORREF。
    void    SetBgColor   (COLORREF c);
    COLORREF GetBgColor   () const { return m_bg; }

    // 设置 / 读取文字色。默认白 RGB(255,255,255)。
    //   c：COLORREF。
    void    SetTextColor (COLORREF c);
    COLORREF GetTextColor() const { return m_fg; }

    // 设置 / 读取"空文本时自动隐藏"开关。默认 true。
    //   b：true = 文本空时跳过绘制；false = 即便空也画一个圆。
    void    SetHideWhenEmpty(bool b);
    bool    GetHideWhenEmpty() const { return m_hideEmpty; }

    // 当前是否会画出徽章（综合考虑 m_text 是否空 + HideWhenEmpty）。
    bool    IsShowing() const;

    // ---- 前导小圆点（leading dot） ----
    //
    // 用于状态指示徽章，如「● 运行中」「● 停机」。圆点画在文字左侧、
    // 徽章垂直居中，圆点 + gap 计入宽度（自动撑宽）。HideWhenEmpty=true
    // 且文本空时圆点也不画（整体不显）。圆点颜色独立于 m_bg / m_fg。

    // 设置前导小圆点颜色。c != CLR_INVALID 时显示圆点；CLR_INVALID 清除。
    //   c：填充色，常用 RGB(60,200,120) 绿"运行中" / RGB(220,60,60) 红
    //       "停机" / RGB(220,150,40) 橙"警告"；CLR_INVALID 关闭圆点。
    void     SetLeadingDot(COLORREF c);

    // 当前圆点颜色；无圆点时返回 CLR_INVALID。
    COLORREF GetLeadingDotColor() const  { return m_dotColor; }

    // 是否启用了前导圆点（即 GetLeadingDotColor() != CLR_INVALID）。
    bool     HasLeadingDot() const       { return m_dotColor != CLR_INVALID; }

    // 设置 / 读取圆点半径（像素）。<= 0（默认 0）表示按字体高度自适配，
    // 自适配规则见 AutoDotRadius。
    void     SetLeadingDotRadius(int r);
    int      GetLeadingDotRadius() const { return m_dotRadius; }

    // 设置 / 读取圆点与文字之间的间距（像素）。默认 4。
    void     SetLeadingGap(int gap);
    int      GetLeadingGap() const       { return m_leadingGap; }

    // ---- 圆角半径（形状参数化） ----
    //
    // 默认 -1 = 自动胶囊形（圆角 = 高/2，与历史行为一致：短计数 / 红点 /
    // "99+" 走这条）。>= 0 = 固定圆角半径（像素），用于"标签 chip"形态
    // （如「● 已启用」「待审核」等长文字状态徽章，推荐 4 或 6）。
    // 0 = 直角矩形。最终半径还会被 DuiAA::FillRoundRect 自动夹到
    // min(w,h)/2 —— 设过大也安全。

    // 设置圆角半径。
    //   r：-1 = 胶囊形（默认）；>= 0 = 固定半径；< -1 视作 0。
    void     SetCornerRadius(int r);
    int      GetCornerRadius() const     { return m_cornerRadius; }

    // ---- 文字显示最大字符数（截断） ----
    //
    // 默认 4，与历史"超 4 字符截断"行为一致 —— 现有调用零影响。
    // 0 = 不截（长文字标签 chip 用法，业务侧自己保证 SetRect 够宽）。
    // <0 视作 0（不截，防御性）。
    // 截断时直接砍后缀，不加省略号；按 CString::GetLength() 计算（中文
    // 1 字算 1 个字符）。

    // 设置最大显示字符数。
    //   n：0 = 不截；> 0 = 截到 n；< 0 视作 0。
    void     SetMaxDisplayChars(int n);
    int      GetMaxDisplayChars() const  { return m_maxChars; }

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;

    // ---- 静态 helper（暴露给测试）----

    // 把整数计数转成显示文本，不需要构造控件。
    //   n：未读数。
    //   返回：FormatCount 规则的字符串 —— 0 → ""、1-99 → "<n>"、100+ → "99+"。
    static  CString FormatCount(int n);

    // 算徽章「内容宽度」（不含左右 padding）：dotDiameter + leadingGap + textWidth。
    // hasDot=false 时退化为 textWidth；textWidth<0 视为 0；dotRadius<0 视为 0；
    // leadingGap<0 视为 0。
    //   textWidth：文字宽（像素）；
    //   dotRadius：圆点半径（像素）；
    //   leadingGap：圆点与文字间距（像素）；
    //   hasDot：true 才把圆点 + gap 计入。
    //   返回：内容宽度（像素，>= 0）。
    static int ContentWidth(int textWidth, int dotRadius,
                            int leadingGap, bool hasDot);

    // 自适配圆点半径：fontHeight <= 0 → 3（fallback）；否则 max(3, fontHeight / 4)。
    //   fontHeight：文字像素高度（如 GetTextExtentPoint32 的 sz.cy / 或 tmHeight）。
    //   返回：建议的圆点半径（像素，>= 3）。
    static int AutoDotRadius(int fontHeight);

    // 算「实际生效」的圆角半径。
    //   rawRadius：来自 GetCornerRadius()，-1 表示胶囊形；
    //   height：当前徽章实际高度（像素）。
    //   返回：rawRadius == -1 → height / 2（胶囊）；
    //         rawRadius >= 0 → rawRadius；
    //         rawRadius < -1（误用）→ 0。
    //   注：返回值还会被 DuiAA::FillRoundRect 二次夹到 min(w,h)/2。
    static int EffectiveCornerRadius(int rawRadius, int height);

    // 应用文字截断规则。
    //   text：原始文本（nullptr 视为空）；
    //   maxChars：来自 GetMaxDisplayChars()，<= 0 表示不截。
    //   返回：截断后的字符串；<= 0 / 文本长度 <= maxChars → 原样返回。
    static CString ApplyMaxChars(LPCTSTR text, int maxChars);

private:
    CString  m_text;
    COLORREF m_bg = RGB(220, 60, 60);
    COLORREF m_fg = RGB(255, 255, 255);
    bool     m_hideEmpty = true;

    // ---- 前导小圆点状态（HasLeadingDot()=false 时不被读写） ----
    COLORREF m_dotColor   = CLR_INVALID;   // CLR_INVALID = 不画圆点
    int      m_dotRadius  = 0;             // 0 = 按字体高度自适配
    int      m_leadingGap = 4;             // 圆点与文字间距（像素）

    // ---- 形状参数化（chip 用法）----
    int      m_cornerRadius = -1;          // -1 = 胶囊形（高/2），>= 0 = 固定半径
    int      m_maxChars     = 4;           // 0 = 不截，>0 = 截到 n 字；默认 4 保持现行为
};

} // namespace balloonwjui

#endif // BUI_FEATURE_BADGE
