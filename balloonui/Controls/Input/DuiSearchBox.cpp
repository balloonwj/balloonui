#include "stdafx.h"
#include "DuiSearchBox.h"

#if BUI_FEATURE_SEARCHBOX

#include "../../DuiPaintAA.h"

namespace balloonwjui {

namespace {

// Magnifier glyph color: a slightly cool medium gray so the icon reads as
// "search affordance" without competing with whatever the EDIT renders.
const COLORREF kGlyphColor = RGB(110, 110, 120);

// Clear (x) button color: neutral gray so it's visible against the white
// EDIT background but doesn't shout for attention.
const COLORREF kClearColor = RGB(150, 150, 150);

// Left magnifier strip width default (px).
const int kDefaultGlyphW = 24;

// Glyph geometry (intra-strip).
const int kMagnifierRadiusPx = 5;
const int kClearGlyphHalfPx  = 5;

// Painter callback：在 left gutter RECT 内画 magnifier glyph
void PaintMagnifierGlyph(HDC hdc, const RECT& rc)
{
    int gx = (rc.left + rc.right) / 2;
    int gy = (rc.top  + rc.bottom) / 2;
    int r  = kMagnifierRadiusPx;
    RECT circle = { gx - r, gy - r - 1, gx + r, gy + r - 1 };
    DuiAA::FillEllipse(hdc, circle, CLR_INVALID, kGlyphColor, 1.5f);
    DuiAA::DrawLine(hdc, gx + 3, gy + 3, gx + 7, gy + 7,
                    kGlyphColor, 1.5f);
}

// Painter callback：在 right gutter RECT 内画 × glyph
void PaintClearGlyph(HDC hdc, const RECT& rc)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top  + rc.bottom) / 2;
    int sz = kClearGlyphHalfPx;
    DuiAA::DrawLine(hdc, cx - sz, cy - sz, cx + sz, cy + sz,
                    kClearColor, 1.5f);
    DuiAA::DrawLine(hdc, cx + sz, cy - sz, cx - sz, cy + sz,
                    kClearColor, 1.5f);
}

} // anonymous

DuiSearchBox::DuiSearchBox()
{
    SetTabStop(true);   // 与 DuiEditHost 默认一致；明示
    InstallMagnifier_();
    // 右侧 × 不在 ctor 里安装 —— 文字非空时 SyncClear_ 才装。这样空状
    // 态下 × 不画，与重构前的 IsClearShowing 行为一致。
}

void DuiSearchBox::InstallMagnifier_()
{
    SetIcon(LeftIcon, kDefaultGlyphW, &PaintMagnifierGlyph);
    // 放大镜不可点击 —— 是装饰，鼠标穿透到 EDIT 设 caret
    SetIconClickable(LeftIcon, false);
}

void DuiSearchBox::SetGlyphStripWidth(int px)
{
    if (px < 0) { px = 0; }
    SetIcon(LeftIcon, px, px > 0 ? &PaintMagnifierGlyph : nullptr);
}

void DuiSearchBox::SetClearStripWidth(int px)
{
    if (px < 14) { px = 14; }
    if (m_clearW == px) { return; }
    m_clearW = px;
    SyncClear_();   // 当前若已显示，宽度跟着改
}

bool DuiSearchBox::IsClearShowing() const
{
    return GetIconWidth(RightIcon) > 0;
}

RECT DuiSearchBox::GetClearRect() const
{
    if (!IsClearShowing())
    {
        RECT z = { 0, 0, 0, 0 };
        return z;
    }
    // 沿用 DuiEditHost::ComputeIconRect 静态 helper —— 与基类 OnPaint /
    // hit-test 用同一套计算，保证三者一致。
    // border=1 marginV 用 DuiEditHost 默认 m_marginT = 2。
    return ComputeIconRect(GetRect(), RightIcon, m_clearW, 1, 2);
}

void DuiSearchBox::SetText(LPCTSTR sz)
{
    DuiEditHost::SetText(sz);
    // 单测路径（无 HWND）EN_CHANGE 不会触发；显式补一次
    SyncClear_();
}

void DuiSearchBox::OnHwndCommand(UINT enCode)
{
    // 基类先处理：转 cache + 发 DUIN_VALUECHANGED 等
    DuiEditHost::OnHwndCommand(enCode);
    // EN_CHANGE 之后文字内容可能变了，刷 × 显隐
    if (enCode == EN_CHANGE)
    {
        SyncClear_();
    }
}

void DuiSearchBox::SyncClear_()
{
    bool nonEmpty = !GetText().IsEmpty();
    if (nonEmpty)
    {
        SetIcon(RightIcon, m_clearW, &PaintClearGlyph);
        SetIconClickable(RightIcon, true);
    }
    else
    {
        ClearIcon(RightIcon);
    }
}

bool DuiSearchBox::OnIconClicked_(IconSlot slot)
{
    if (slot == RightIcon)
    {
        // × 清除：本类自吞 click，不让 DUIEN_RIGHT_ICON_CLICK 冒泡 ——
        // 父业务关心的是"用户改了搜索文字"，应在 DUIN_VALUECHANGED
        // 里收到（基类 OnHwndCommand EN_CHANGE 会发，下面 SetText("")
        // 会触发）。
        SetText(_T(""));
        // SetText 同步到 HWND → EDIT 发 EN_CHANGE → OnHwndCommand →
        // SyncClear_ → ClearIcon(RightIcon)，自动消失。
        return true;
    }
    return false;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SEARCHBOX
