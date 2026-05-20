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

    // 直接设置 / 读取文本（最多 4 字符显示，超出截断）。
    //   text：任意字符串；可中可英；nullptr 视为空。
    void    SetText(LPCTSTR text);
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

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;

    // ---- 静态 helper（暴露给测试）----

    // 把整数计数转成显示文本，不需要构造控件。
    //   n：未读数。
    //   返回：FormatCount 规则的字符串 —— 0 → ""、1-99 → "<n>"、100+ → "99+"。
    static  CString FormatCount(int n);

private:
    CString  m_text;
    COLORREF m_bg = RGB(220, 60, 60);
    COLORREF m_fg = RGB(255, 255, 255);
    bool     m_hideEmpty = true;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_BADGE
