#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_SEARCHBOX

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "DuiEditHost.h"

namespace balloonwjui {

// =================================================================
// DuiSearchBox —— 带放大镜 + 清除按钮的搜索框（HWND-hosted）
// =================================================================
//
// 本控件是 <code>DuiEditHost</code> 的<u>预设包装</u>（thin preset）：
// 直接继承 DuiEditHost，ctor 里把内联图标 API（左侧放大镜 + 右侧 ×）
// 配置好。所有的 EDIT 行为（输入 / IME / placeholder / 占位符 cue
// banner / 多行 / 密码 / 边框 / 底色等）都从基类继承，没有任何重复
// 实现。
//
// 历史：本控件早期是独立类，自己 paint magnifier + × glyph、自己
// HitTest、自己 Layout 把 EDIT 内嵌。M7 中 DuiEditHost 加了原生左/右
// 内联图标 API 后，DuiSearchBox 重构成 thin preset；旧的 paint /
// Layout / HitTest 代码全删（>150 行 → ~80 行）。对 caller 的 API
// 表面保持不变，零回归。
//
// 工作机制：
//   · ctor 通过 SetIcon(LeftIcon, ...) 安装抗锯齿 magnifier glyph。
//   · 内部监听 EN_CHANGE，文字变化时切右图标：空 → ClearIcon(RightIcon)；
//     非空 → SetIcon(RightIcon, ..., ×) + SetIconClickable(true)。
//   · 拦截 OnIconClicked_(RightIcon) → SetText("") + 让父收到
//     DUIN_VALUECHANGED。"× 是搜索框内部行为，不冒泡 DUIEN_RIGHT_ICON_CLICK。
//
// 用法（与重构前一致）：
//
//     auto sb = std::unique_ptr<DuiSearchBox>(new DuiSearchBox());
//     sb->SetPlaceholder(_T("搜索联系人..."));
//     sb->SetMaxLength(64);
//     sb->SetCtrlId(IDC_SEARCH);
//     parent->AddChild(std::move(sb));
//     // 等 host HWND 就绪：
//     raw->EnsureCreated(host->m_hWnd);
//     // 父对话框 OnDuiNotify：
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_SEARCH)
//     //       FilterContacts(raw->GetText());
//
// XML 用法（详细属性见 guides.html §3.3.13）：
//
//     <searchbox id="100"
//                placeholder="搜索联系人..."
//                max-length="64"
//                fixedHeight="28"/>
//
// 事件：
//   · DUIN_VALUECHANGED — 文字变化（含 × 清空）；extra = 0。
class BUI_API DuiSearchBox : public DuiEditHost
{
public:
    DuiSearchBox();

    // 设置 / 读取左侧 magnifier 区域宽度（px）。默认 24。改 0 等效于
    // 隐藏左 glyph。本调用透传到 DuiEditHost::SetIcon(LeftIcon, ...)。
    void    SetGlyphStripWidth(int px);
    int     GetGlyphStripWidth() const { return GetIconWidth(LeftIcon); }

    // 设置 / 读取右侧 × 清除按钮区域宽度（px）。默认 22；clamp >= 14。
    // 仅"目标宽度" —— 实际是否显示由文字是否非空决定（IsClearShowing）。
    void    SetClearStripWidth(int px);
    int     GetClearStripWidth() const { return m_clearW; }

    // 当前 × 清除按钮是否显示（即文字非空）。
    bool    IsClearShowing() const;

    // × 清除按钮的矩形（host 客户区坐标系）；隐藏时返回空矩形。
    RECT    GetClearRect() const;

    // 直接拿到内部 EDIT 子控件指针。<u>历史 API</u>：本类 M7 之前是
    // 聚合（持有 m_edit 子控件），返回的是<u>那个</u> 子。重构后本类
    // 自己就是 EDIT，直接返回 this 即可（保持向后兼容）。
    DuiEditHost* GetEdit() { return this; }

    // SetText 后同步 × 显隐 —— 没 HWND 时 EN_CHANGE 不触发，必须显式
    // 在 SetText 路径上钩，否则 IsClearShowing 在 SetText("xx") 后仍是
    // false（破坏单测以及"程序设值"语义）。
    void    SetText(LPCTSTR sz) override;

    // EN_CHANGE / EN_SETFOCUS / EN_KILLFOCUS —— 转发给基类 +
    // 文字变时同步 × 显隐
    void    OnHwndCommand(UINT enCode) override;

protected:
    // 拦右侧 × 点击：自吞 + 清空文字 + 发 VALUECHANGED 给父
    bool    OnIconClicked_(IconSlot slot) override;

private:
    void    InstallMagnifier_();    // ctor 用：安装左 magnifier 图标
    void    SyncClear_();            // 文字非空 → 显 × ；空 → 隐

    int     m_clearW = 22;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SEARCHBOX
