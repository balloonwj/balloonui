#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_BUTTON

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"
#include "../../DuiNinePatch.h"

namespace balloonwjui {

// =================================================================
// DuiButton —— DUI 原生按钮（无 HWND，4 种风格）
// =================================================================
//
// 用途：业务里最高频的可点击控件。4 种风格覆盖几乎所有按钮场景：
// 普通按下、勾选框、单选框、紧凑图标按钮。
//
// 风格（与老 SKIN_BUTTON_TYPE 枚举对应，REFACTOR_NOTES.md 是冻结契约）：
//   · PushButton：普通可按按钮（默认）。品牌蓝实心圆角。
//   · Checkbox：点击 toggle m_checked，左侧画一个勾选框。
//   · Radio：跟 Checkbox 一样，但与父下同 group id 的兄弟<u>互斥</u>
//     —— 选中一个时父会遍历同组兄弟把其它的取消选中。
//   · Icon：紧凑型按钮，左侧画一个 icon glyph + 右侧标签。按下语义同
//     PushButton。
//
// 工作机制：
//   · PushButton 实色背景 + 8px 圆角：normal #2D6CDF、hover #2559B8、
//     pressed #1E4A99；Checkbox / Radio 用浅色填充让勾 / 圆点 visible。
//     按状态可用 SetBgBitmap 用 9-grid 位图替换填充。
//   · 跟踪 m_pressed —— 鼠标按下在按钮内但松开时移出 → 取消（不发
//     DUIN_CLICK）；只有"按下且松开都在按钮内"才算一次有效点击。
//   · Radio 同父同 group 自动互斥：toggle 时遍历父下所有同组 Radio 把
//     其它的取消选。Group 是父-scope。
//   · 所有 HBITMAP 是 caller-owned，按钮永远不 DeleteObject。
//
// 代码用法：
//
//     auto btn = std::unique_ptr<DuiButton>(new DuiButton());
//     btn->SetButtonType(DuiButton::StylePushButton);
//     btn->SetText(_T("&Save"));         // & 是 Alt 助记符
//     btn->SetCtrlId(IDC_SAVE);
//     btn->SetRect(RECT{ 16, 200, 96, 226 });
//     parent->AddChild(std::move(btn));
//     // 父对话框 WM_DUI_NOTIFY:
//     //   if (n.code == DUIN_CLICK && n.ctrlId == IDC_SAVE) Save();
//
// XML 用法（详细属性见 guides.html §3.3.9）：
//
//     <button id="100" text="保存" buttonType="push"/>
//     <button id="101" text="启用通知" buttonType="checkbox"/>
//     <button id="102" text="选项 A" buttonType="radio"/>
//     <button id="103" text="设置" buttonType="icon"/>
//
//   注意：XML 暂不暴露 SetRadioGroup / SetBgBitmap / SetBgPic 等
//   高级属性，需要时拿到 builder 返回的 root 后用 FindControlById +
//   SetXxx 自己设。
//
// 事件（ctrlId = button id）：
//   · DUIN_CLICK         — 任意风格，每次成功点击发一次。extra = 0。
//   · DUIN_VALUECHANGED  — Checkbox / Radio 勾选状态变化时发。
//                          extra = checked (0 = 未选 / 1 = 选中)。
//
// 替代关系：CSkinButton（冻结 API 表面）。SetBgPic / SetIconPic /
// SetCheckBoxPic 保留为空 stub —— 这些走 CSkinManager 的 PNG 路径加载
// 还没接，业务想测皮肤直接用 SetBgBitmap 传 HBITMAP。
class BUI_API DuiButton : public DuiControl
{
public:
    enum Style
    {
        StylePushButton = 0,    // 默认
        StyleCheckbox   = 1,
        StyleRadio      = 2,
        StyleIcon       = 3
    };

    DuiButton();

    // ---- 基本属性 ----

    // 设置 / 读取按钮风格。SetButtonType 会触发 Invalidate 重绘。
    void    SetButtonType(Style s);
    Style   GetButtonType() const { return m_style; }

    // 设置 / 读取按钮文字。可含 "&X" 助记符（如 "&Save"）。
    void    SetText(LPCTSTR sz);
    CString GetText() const { return m_text; }

    // 解析助记符字符。
    //   返回："&Save" → 's'；"&&Plain"（&& 是 & 转义，没有助记）→ 0；
    //         无 & 标记 → 0。
    // 键盘 Alt+letter 激活由 host / DuiFrameWindow 自己 wire；本 accessor
    // 只是让 caller 找到该激活哪个控件。
    TCHAR   GetMnemonicChar() const;

    // 设置 / 读取勾选状态（仅 Checkbox / Radio 风格有意义）。
    //   checked：true = 选中，false = 取消。
    //   notify：true 时触发 DUIN_VALUECHANGED；false 抑制。
    void    SetCheck(bool checked, bool notify = true);
    bool    IsChecked() const { return m_checked; }

    // 设置 / 读取 Radio 风格的 group id —— 父下同非 0 group id 的所有
    // Radio 形成一个互斥组。0 = 不参与互斥（Radio 风格下意义不大）。
    void    SetRadioGroup(int gid) { m_radioGroup = gid; }
    int     GetRadioGroup() const  { return m_radioGroup; }

    // ---- 老 CSkinButton 的 PNG 路径 setter，空 stub（迁移期占位）----
    void    SetBgPic   (LPCTSTR /*n*/, LPCTSTR /*h*/, LPCTSTR /*d*/, LPCTSTR /*f*/) {}
    void    SetIconPic (LPCTSTR /*p*/) {}
    void    SetCheckBoxPic(LPCTSTR /*n*/, LPCTSTR /*h*/, LPCTSTR /*tn*/, LPCTSTR /*th*/) {}

    // ---- 按状态背景位图（4 状态可独立指定）----

    // 设置 / 替换 4 状态的背景位图。caller 持有所有权；本控件不会
    // copy 也不会 DeleteObject。任一槽位传 nullptr → 该状态 fallback
    // 到下一槽（见 GetBgBitmapForCurrentState）。
    //   normal/hover/pressed/disabled：HBITMAP 或 nullptr。
    void    SetBgBitmap(HBITMAP normal, HBITMAP hover,
                        HBITMAP pressed, HBITMAP disabled);

    // 设置背景位图的 9-grid inset 切片。任一 bg bitmap 存在时生效。
    // (0,0,0,0) = 直接 stretch（不保护四角）。
    void    SetBgInsets(int left, int top, int right, int bottom);
    DuiNinePatch::Insets GetBgInsets() const { return m_bgInsets; }

    // 按当前状态选 bitmap 并 fallback 到下一槽：
    //   disabled    -> m_hBgDisabled 或 normal
    //   pressed     -> m_hBgPressed  或 hover  或 normal
    //   hover       -> m_hBgHover    或 normal
    //   default     -> m_hBgNormal
    // 全部都空 → 返回 nullptr，caller 必须画实色 fallback。
    HBITMAP GetBgBitmapForCurrentState() const;

    // ---- 文字外观 ----

    // 设置文字色。
    void    SetTextColor(COLORREF c) { m_clrText = c; Invalidate(); }

    // 设置 DrawText 对齐 flags（DT_CENTER/DT_VCENTER/...）。默认居中。
    void    SetTextAlign(DWORD dt)   { m_dtFlags = dt; Invalidate(); }

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp  (POINT pt, UINT mkFlags) override;
    bool    OnSetCursor  (POINT pt) override;

    // Debug / 截图用：强制 m_pressed = b 让 DuiGallery 抓到按下态。
    // 与 DuiControl::DebugSetHover / DebugSetFocused 配套。下次真鼠标
    // down/up 会覆盖此设置。
    void    DebugSetPressed(bool b);

private:
    void    DoToggle();           // Checkbox / Radio 的 click 切换语义
    void    UncheckRadioPeers();  // 遍历父子，把同组其它 Radio 取消选
    COLORREF FillColor() const;
    COLORREF BorderColor() const;

private:
    Style       m_style       = StylePushButton;
    CString     m_text;
    bool        m_checked     = false;
    bool        m_pressed     = false;     // 鼠标按下在按钮内、还没松开
    int         m_radioGroup  = 0;
    COLORREF    m_clrText     = RGB(20, 20, 20);
    DWORD       m_dtFlags     = DT_CENTER | DT_VCENTER | DT_SINGLELINE;

    HBITMAP     m_hBgNormal   = nullptr;
    HBITMAP     m_hBgHover    = nullptr;
    HBITMAP     m_hBgPressed  = nullptr;
    HBITMAP     m_hBgDisabled = nullptr;
    DuiNinePatch::Insets m_bgInsets;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_BUTTON
