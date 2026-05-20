#pragma once

// =============================================================================
// CloudMelodyFonts.h —— 排版梯度的 HFONT 工厂
//
// DESIGN.md 的字号梯度：
//   display-lg   30px / 700  —— 页面大标题（"本地音乐" / "我喜欢的音乐"）
//   headline-md  22px / 600  —— section 主标题（"精选歌单" / "听歌时段分析"）
//   title-sm     16px / 600  —— 中标题（卡片标题、按钮文字）
//   body-md      14px / 400  —— 正文 / 列表行
//   body-sm      13px / 400  —— 元数据 / 副文
//   label-xs     11px / 500  —— chip / slogan / section header
//
// HFONT 全局单例（first-call 懒构造），不在每次 SetFont 时建。所有
// HFONT 在进程退出时由 OS 回收，不显式 DeleteObject —— 我们持有的是
// 进程级 HFONT，与控件实例生命周期无关。
//
// 用法：
//   lbl->SetFont(Fonts::Get(Fonts::HeadlineMd));
// =============================================================================

#include <windows.h>
#include <memory>
#include "Controls/Basic/DuiLabel.h"

namespace cloudmelody {

namespace Fonts {

enum Kind
{
    DisplayLg = 0,
    HeadlineMd,
    TitleSm,
    BodyMd,
    BodySm,
    LabelXs,
    KindCount
};

// 返回对应排版梯度的 HFONT。caller <u>不</u>持有所有权 —— 这是进程级
// 全局单例，第一次 Get 时 CreateFont，后续调用直接 return cache。
HFONT Get(Kind kind);

} // namespace Fonts

} // namespace cloudmelody

// ─── 便捷 helper：让 page builder 一行写完 "label + 颜色 + 字号" ───
//
// 用法：
//     col->AddChild(MakeLabel(_T("精选歌单"), kColorOnSurface,
//                             Fonts::HeadlineMd),
//                   DuiLayout::Hint().Fixed(28));
namespace cloudmelody {

inline std::unique_ptr<balloonwjui::DuiLabel>
MakeLabel(LPCTSTR text, COLORREF color, Fonts::Kind kind)
{
    auto l = std::make_unique<balloonwjui::DuiLabel>();
    l->SetText(text);
    l->SetTextColor(color);
    l->SetFont(Fonts::Get(kind));
    return l;
}

} // namespace cloudmelody

