#include "stdafx.h"
#include "TopBar.h"
#include "CloudMelodyTheme.h"
#include "../Controls/IconButton.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Input/DuiEditHost.h"
#include "../Controls/PaintHelpers.h"
#include "DuiPaintAA.h"
#include "DuiNotify.h"

using namespace balloonwjui;

namespace cloudmelody {

namespace {

// 搜索框宽度（设计稿目测主搜索框约 360px）。fixed —— 让 TopBar 大部分
// 空间给左侧 nav arrow 和右侧 user cluster；窗口宽度变化时搜索框不抢空间。
static const int kSearchBoxWidth = 360;
// 搜索框高度（pill；TopBar 总高 48 内留 14px 上下边）。
static const int kSearchBoxHeight = 32;
// 头像直径（TopBar 用小号 28px，留出对齐余量）。
static const int kAvatarDiameter = 28;

// =========================================================================
// SearchPill —— TopBar 搜索框（pill 胶囊 + 左侧放大镜 + 真 EDIT 输入）
//
// 用 balloonui 新加的 DuiEditHost 内联图标 + 底色 + 无边框 API 实现：
//   - SetBgColor(浅灰)：EDIT 底色 = pill 灰，"EDIT 框样式不明显"
//   - SetShowBorder(false)：去掉 EDIT 默认 1px 边框，避免压在 pill 圆角上
//   - SetIcon(LeftIcon, 32, 放大镜 painter)：左 gutter 内画 GDI+ 抗锯齿放大镜
// 外层 DuiVBox 自己 OnPaint 画 pill 圆角灰底（圆角 = 高度/2）。EDIT 子
// 控件铺满中间，与 pill 同色 → 视觉上"放大镜浮在灰胶囊里"。
// =========================================================================
class SearchPill : public DuiVBox
{
public:
    // pill bg 比 TopBar 底色（kColorSurfaceContainerLow #F3F3F4）深一档，
    // 这样自然有"独立 pill"对比度；border 再叠一层更深的灰，描边明显。
    static const COLORREF kPillBg     = RGB(0xEA, 0xEA, 0xEC);
    static const COLORREF kPillBorder = RGB(0xC0, 0xC0, 0xC4);

    SearchPill()
    {
        SetGap(0);
        SetPadding(0, 0, 0, 0);

        auto edit = std::make_unique<DuiEditHost>();
        edit->SetPlaceholder(_T("搜索音乐、视频、播客..."));
        edit->SetBgColor(kPillBg);
        edit->SetShowBorder(false);
        edit->SetMargins(0, 0, 8, 0);
        // 左 gutter 32px，里面画 GDI+ 抗锯齿放大镜。clickable=false：放大镜
        // 是装饰，鼠标穿透到 EDIT 设 caret。
        edit->SetIcon(DuiEditHost::LeftIcon, 32,
            [](HDC hdc, const RECT& rc)
            {
                // 整体图标视觉中心要与 EDIT 文字水平中线一致。
                //
                // 关键观察：Win32 EDIT 单行文字<u>不</u>严格居中 —— 默认有
                // ~2px 顶部 caret 留白，文字中心比 HWND 几何中心偏上约
                // 4-5px。把图标整体上移 ~4px 抵消。
                //
                // 把手做短：起点 (gx+3, gy+3) → 终点 (gx+5, gy+5) 仅 2px 长，
                // 减小下侧 bbox 扩展。
                int gx = (rc.left + rc.right) / 2;
                int gy = (rc.top  + rc.bottom) / 2 - 4;
                const COLORREF c = kColorOnSurfaceVar;
                const int r = 5;
                RECT circle = { gx - r, gy - r, gx + r, gy + r };
                DuiAA::FillEllipse(hdc, circle, CLR_INVALID, c, 1.5f);
                DuiAA::DrawLine(hdc, gx + 3, gy + 3, gx + 5, gy + 5,
                                c, 1.5f);
            });
        AddChild(std::move(edit), DuiLayout::Hint().Weight(1));
    }

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        // 圆角 pill 底（比 TopBar 深一档）+ 1px 较深灰描边
        int h = m_rcItem.bottom - m_rcItem.top;
        int radius = h / 2;
        PaintHelpers::FillRoundedRect(hdc, m_rcItem, kPillBg, radius);
        PaintHelpers::DrawRoundedRectBorder(hdc, m_rcItem,
                                            kPillBorder, radius, 1.0f);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

// =========================================================================
// TopBarPanel —— TopBar 整体容器。继承 DuiHBox 加一层背景填充 + 底部
// 1px 分隔线，视觉上把 TopBar 与下方 ContentHost 区分开。
// =========================================================================
class TopBarPanel : public DuiHBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        // 1) 底色
        HBRUSH hbr = ::CreateSolidBrush(kColorSurfaceContainerLow);
        ::FillRect(hdc, &m_rcItem, hbr);
        ::DeleteObject(hbr);
        // 2) 底部 1px 分隔线（与 PlayerBar 顶部对称）
        RECT rcLine = m_rcItem;
        rcLine.top = rcLine.bottom - 1;
        HBRUSH hbrL = ::CreateSolidBrush(kColorPlayerBarTop);
        ::FillRect(hdc, &rcLine, hbrL);
        ::DeleteObject(hbrL);
        // 3) child paint
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

std::unique_ptr<IconButton> MakeIconBtn(LPCTSTR glyph, int ctrlId,
                                         LPCTSTR tooltip = nullptr)
{
    auto b = std::make_unique<IconButton>();
    b->SetGlyph(glyph);
    b->SetCtrlId((UINT)ctrlId);
    b->SetTooltip(tooltip);
    return b;
}

} // anonymous

std::unique_ptr<DuiControl> BuildTopBar()
{
    auto root = std::make_unique<TopBarPanel>();
    root->SetGap(8);
    // 左右内边距 16；上下 0（让分隔线贴底）。
    root->SetPadding(16, 8, 16, 8);

    // 1) ‹ › 后退 / 前进
    root->AddChild(MakeIconBtn(_T("‹"), TopBarId_Back,    _T("后退")),
                   DuiLayout::Hint().Fixed(kSizeIconBtn));
    root->AddChild(MakeIconBtn(_T("›"), TopBarId_Forward, _T("前进")),
                   DuiLayout::Hint().Fixed(kSizeIconBtn));

    // 2) 与搜索框间的间距 spacer
    root->AddChild(std::make_unique<DuiControl>(),
                   DuiLayout::Hint().Fixed(8));

    // 3) 搜索框（固定宽 360）—— 左对齐到 nav arrow 后，右侧弹性 spacer
    //    把 user cluster 推到最右。
    auto sb = std::make_unique<SearchPill>();
    sb->SetCtrlId(TopBarId_Search);
    root->AddChild(std::move(sb), DuiLayout::Hint().Fixed(kSearchBoxWidth));

    // 4) 弹性 spacer
    root->AddChild(std::make_unique<DuiControl>(),
                   DuiLayout::Hint().Weight(1));

    // 5) 头像（圆形 28px，fallback 显示「张」字）
    auto avatar = std::make_unique<DuiAvatar>();
    avatar->SetName(_T("张小方"));
    avatar->SetFallbackBgColor(kColorPrimary);
    avatar->SetShape(DuiAvatar::ShapeCircle);
    avatar->SetCtrlId(TopBarId_Avatar);
    root->AddChild(std::move(avatar),
                   DuiLayout::Hint().Fixed(kAvatarDiameter));

    // 6) 用户名（DuiLabel，固定宽留给 4 个汉字 + 余量）
    auto name = std::make_unique<DuiLabel>();
    name->SetText(_T("张小方"));
    name->SetTextColor(kColorOnSurface);
    root->AddChild(std::move(name), DuiLayout::Hint().Fixed(72));

    // 7) ⚙ 设置
    root->AddChild(MakeIconBtn(_T("⚙"), TopBarId_Settings, _T("设置")),
                   DuiLayout::Hint().Fixed(kSizeIconBtn));

    return root;
}

} // namespace cloudmelody
