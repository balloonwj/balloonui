#include "stdafx.h"
#include "Sidebar.h"
#include "CloudMelodyTheme.h"
#include "CloudMelodyFonts.h"
#include "../Controls/PillButton.h"
#include "../Controls/PaintHelpers.h"

#include <gdiplus.h>

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiSeparator.h"
#include "DuiResMgr.h"
#include "DuiNotify.h"

using namespace balloonwjui;

namespace cloudmelody {

namespace {

// SidebarNavItem 行高（"发现 / 播客 / …" 单行项）。设计稿目测 36–40px；
// 取 36 与默认按钮高一致。
static const int kNavItemHeight    = 36;
// 左侧 active 红条尺寸：宽 4px、高比 cell 略短，留 6px 上下边。
static const int kActiveBarWidth   = 4;
static const int kActiveBarMargin  = 6;
// 图标尺寸 + 距离 active bar 的左边距 + 文字距离图标的间距
static const int kNavIconSize      = 16;
static const int kNavIconLeft      = 14;       // active bar (4) + 10 余位
static const int kNavTextAfterIcon = 10;
// nav item 圆角（hover / active 背景的圆角；4px 视觉接近 small radius）。
static const int kNavItemRadius    = 4;

// 图标种类
enum NavIconKind
{
    NavIcon_None       = 0,
    NavIcon_Discover   = 1,
    NavIcon_Podcast    = 2,
    NavIcon_Music      = 3,
    NavIcon_Favorites  = 4,
    NavIcon_Recent     = 5,
    NavIcon_Profile    = 6,
    NavIcon_Settings   = 7,
    NavIcon_Help       = 8,
};

// 在 rc 内画一个 GDI+ 抗锯齿的 16×16 stroke icon。所有图标用 1.5px 圆头
// 笔画，颜色由 caller 给（active 用主品牌 / idle 用 surface-variant）。
void PaintNavIcon(HDC hdc, const RECT& rc, NavIconKind kind, COLORREF color)
{
    if (kind == NavIcon_None) { return; }

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::Color c(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(c, 1.5f);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound,
                   Gdiplus::DashCapFlat);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush brush(c);

    Gdiplus::REAL X = (Gdiplus::REAL)rc.left;
    Gdiplus::REAL Y = (Gdiplus::REAL)rc.top;
    Gdiplus::REAL W = (Gdiplus::REAL)(rc.right  - rc.left);
    Gdiplus::REAL H = (Gdiplus::REAL)(rc.bottom - rc.top);
    // 让 icon 内容在 cell 里 inset 1px，避免笔画顶到 cell 外
    Gdiplus::REAL inset = 1.0f;
    Gdiplus::REAL L = X + inset, T = Y + inset;
    Gdiplus::REAL R = X + W - inset, B = Y + H - inset;
    Gdiplus::REAL CX = X + W * 0.5f;
    Gdiplus::REAL CY = Y + H * 0.5f;

    switch (kind)
    {
    case NavIcon_Discover:
        // 圆环 + 内部菱形指针（compass）
        {
            g.DrawEllipse(&pen, L, T, R - L, B - T);
            // 上半 fill 三角 + 下半描边三角，模拟指针
            Gdiplus::PointF up[3] = {
                { CX,             T + (B - T) * 0.20f },
                { CX + W * 0.13f, CY                  },
                { CX - W * 0.13f, CY                  },
            };
            g.FillPolygon(&brush, up, 3);
            Gdiplus::PointF dn[3] = {
                { CX,             B - (B - T) * 0.20f },
                { CX + W * 0.13f, CY                  },
                { CX - W * 0.13f, CY                  },
            };
            g.DrawPolygon(&pen, dn, 3);
        }
        break;
    case NavIcon_Podcast:
        // 麦克风：胶囊主体 + 底座 U + 立柱
        {
            Gdiplus::REAL micW = W * 0.32f;
            Gdiplus::REAL micH = H * 0.46f;
            Gdiplus::RectF mic(CX - micW * 0.5f, T + H * 0.10f, micW, micH);
            // 胶囊：圆角矩形 (R = micW/2)
            Gdiplus::GraphicsPath path;
            Gdiplus::REAL d = micW;
            path.AddArc(mic.X, mic.Y, d, d, 180, 180);
            path.AddArc(mic.X, mic.Y + mic.Height - d, d, d, 0, 180);
            path.CloseFigure();
            g.FillPath(&brush, &path);
            // 底座：半圆 + 立柱 + 底横线
            Gdiplus::REAL baseY = T + H * 0.65f;
            Gdiplus::REAL baseW = W * 0.55f;
            g.DrawArc(&pen, CX - baseW * 0.5f, baseY - baseW * 0.5f,
                      baseW, baseW, 0, 180);
            g.DrawLine(&pen, CX, baseY + baseW * 0.4f, CX, B - H * 0.04f);
            g.DrawLine(&pen, CX - W * 0.18f, B - H * 0.04f,
                       CX + W * 0.18f, B - H * 0.04f);
        }
        break;
    case NavIcon_Music:
        // 音符：主梁 + 二音符头
        {
            Gdiplus::REAL stemL = L + W * 0.22f;
            Gdiplus::REAL stemR = R - W * 0.10f;
            Gdiplus::REAL topY  = T + H * 0.10f;
            // 顶部水平桥
            g.DrawLine(&pen, stemL, topY, stemR, topY + H * 0.10f);
            // 左 stem
            g.DrawLine(&pen, stemL, topY, stemL, B - H * 0.20f);
            // 右 stem
            g.DrawLine(&pen, stemR, topY + H * 0.10f, stemR, B - H * 0.30f);
            // 左 noteHead
            Gdiplus::REAL hd = W * 0.25f;
            g.FillEllipse(&brush, stemL - hd, B - H * 0.20f - hd * 0.7f,
                          hd, hd * 0.7f);
            // 右 noteHead
            g.FillEllipse(&brush, stemR - hd, B - H * 0.30f - hd * 0.7f,
                          hd, hd * 0.7f);
        }
        break;
    case NavIcon_Favorites:
        // 心形：两个上半圆 + V 收口
        {
            Gdiplus::GraphicsPath path;
            Gdiplus::REAL hx = W * 0.50f;
            Gdiplus::REAL hy = H * 0.36f;
            // 左半圆
            path.AddArc(L,            T + H * 0.18f, hx, hy, 180, 180);
            // 右半圆
            path.AddArc(L + W * 0.50f, T + H * 0.18f, hx, hy, 180, 180);
            // V 底部
            path.AddLine(R - W * 0.04f, T + H * 0.40f, CX, B - H * 0.06f);
            path.AddLine(CX, B - H * 0.06f, L + W * 0.04f, T + H * 0.40f);
            path.CloseFigure();
            g.DrawPath(&pen, &path);
        }
        break;
    case NavIcon_Recent:
        // 时钟：圆 + 两根针
        {
            g.DrawEllipse(&pen, L, T, R - L, B - T);
            // 时针（短，向上）
            g.DrawLine(&pen, CX, CY, CX, CY - H * 0.20f);
            // 分针（长，向右）
            g.DrawLine(&pen, CX, CY, CX + W * 0.28f, CY);
        }
        break;
    case NavIcon_Profile:
        // 头像：圆头 + U 形肩
        {
            Gdiplus::REAL hd = W * 0.36f;
            g.DrawEllipse(&pen, CX - hd * 0.5f, T + H * 0.08f, hd, hd);
            // 肩：半圆开口向上
            Gdiplus::REAL bw = W * 0.70f;
            Gdiplus::REAL by = T + H * 0.55f;
            g.DrawArc(&pen, CX - bw * 0.5f, by, bw, bw, 180, 180);
        }
        break;
    case NavIcon_Settings:
        // 齿轮：6 角星形 + 中心圆
        {
            // 简化：外八角描边圆 + 中心圆
            // 不画真齿数，画八向小线段示意"齿"
            g.DrawEllipse(&pen, L + W * 0.18f, T + H * 0.18f,
                          (R - L) - W * 0.36f, (B - T) - H * 0.36f);
            g.DrawEllipse(&pen, CX - W * 0.10f, CY - H * 0.10f,
                          W * 0.20f, H * 0.20f);
            // 4 向短"齿"
            g.DrawLine(&pen, CX, T + H * 0.04f, CX, T + H * 0.18f);
            g.DrawLine(&pen, CX, B - H * 0.04f, CX, B - H * 0.18f);
            g.DrawLine(&pen, L + W * 0.04f, CY, L + W * 0.18f, CY);
            g.DrawLine(&pen, R - W * 0.04f, CY, R - W * 0.18f, CY);
        }
        break;
    case NavIcon_Help:
        // 圆 + "?"（用 path 画问号比 DrawText 笔画更可控）
        {
            g.DrawEllipse(&pen, L, T, R - L, B - T);
            // ? 上半圆
            Gdiplus::REAL qx = CX, qy = T + H * 0.32f;
            g.DrawArc(&pen, qx - W * 0.18f, qy - H * 0.10f,
                      W * 0.36f, H * 0.32f, 200, 230);
            // ? 立柱
            g.DrawLine(&pen, qx, qy + H * 0.20f, qx, qy + H * 0.32f);
            // ? 底点
            g.FillEllipse(&brush, qx - W * 0.04f, B - H * 0.22f,
                          W * 0.08f, W * 0.08f);
        }
        break;
    default:
        break;
    }
}

// =========================================================================
// SidebarNavItem —— 一行可点击的导航项。
//
// 视觉 / 交互：
//   active：左 4px 红竖条 + 浅红 tint 底（kColorNavActiveTint）+ 文字深色
//   hover （非 active）：浅灰 tint 底（kColorNavHoverTint）
//   idle：透明
//   click → NotifyParent(DUIN_CLICK, 0)
//
// 说明：本控件<u>只暴露 SetText / SetActive</u>。icon 和 section-header
// 风格在 sidebar 工厂内通过单独的 DuiLabel 实现，避免本类承担多个责任。
// =========================================================================
class SidebarNavItem : public DuiControl
{
public:
    void SetText(LPCTSTR sz)
    {
        m_text = sz ? sz : _T("");
        Invalidate();
    }

    void SetIcon(NavIconKind k) { m_icon = k; Invalidate(); }

    void SetActive(bool a)
    {
        if (m_active != a)
        {
            m_active = a;
            Invalidate();
        }
    }
    bool IsActive() const { return m_active; }

    bool OnMouseEnter() override { m_hover = true;  Invalidate(); return true; }
    bool OnMouseLeave() override { m_hover = false; Invalidate(); return true; }

    bool OnLButtonUp(POINT, UINT) override
    {
        NotifyParent(DUIN_CLICK, 0);
        return true;
    }

    bool OnSetCursor(POINT) override
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
        return true;
    }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible)
        {
            return;
        }

        // 1) 背景：active > hover > idle。Active 时 tint 比 hover 更"主题色"
        //    化（浅红），hover 是中性灰。两者都 4px 圆角。
        COLORREF bg = 0;
        bool fill = false;
        if (m_active)
        {
            bg   = kColorNavActiveTint;
            fill = true;
        }
        else if (m_hover)
        {
            bg   = kColorNavHoverTint;
            fill = true;
        }
        if (fill)
        {
            PaintHelpers::FillRoundedRect(hdc, m_rcItem, bg,
                                          kNavItemRadius);
        }

        // 2) Active 左侧 4px 红竖条。垂直居中、上下各留 kActiveBarMargin。
        if (m_active)
        {
            RECT rcBar = {
                m_rcItem.left,
                m_rcItem.top    + kActiveBarMargin,
                m_rcItem.left   + kActiveBarWidth,
                m_rcItem.bottom - kActiveBarMargin
            };
            HBRUSH hbr = ::CreateSolidBrush(kColorNavActiveBar);
            ::FillRect(hdc, &rcBar, hbr);
            ::DeleteObject(hbr);
        }

        // 3) 图标 —— active 用主色，否则灰
        COLORREF iconColor = m_active ? kColorPrimary : kColorOnSurfaceVar;
        int iconLeft = m_rcItem.left + kNavIconLeft;
        int iconTop  = m_rcItem.top + ((m_rcItem.bottom - m_rcItem.top)
                       - kNavIconSize) / 2;
        RECT rcIcon = { iconLeft, iconTop,
                        iconLeft + kNavIconSize, iconTop + kNavIconSize };
        if (m_icon != NavIcon_None)
        {
            PaintNavIcon(hdc, rcIcon, m_icon, iconColor);
        }

        // 4) 文字：active 用主色文字、idle 用 onSurfaceVar（中等灰）。
        //    text 起点 = icon 右边 + kNavTextAfterIcon；如无 icon 退回原
        //    缩进。
        if (!m_text.IsEmpty())
        {
            COLORREF txt = m_active ? kColorOnSurface : kColorOnSurfaceVar;
            RECT rcText = m_rcItem;
            rcText.left += (m_icon != NavIcon_None)
                ? (kNavIconLeft + kNavIconSize + kNavTextAfterIcon)
                : 16;
            HFONT hOld    = (HFONT)::SelectObject(hdc,
                                DuiResMgr::Inst().GetDefaultFont());
            int   oldBk   = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldC = ::SetTextColor(hdc, txt);
            ::DrawText(hdc, (LPCTSTR)m_text, -1, &rcText,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            ::SetTextColor(hdc, oldC);
            ::SetBkMode(hdc, oldBk);
            ::SelectObject(hdc, hOld);
        }
    }

private:
    CString     m_text;
    NavIconKind m_icon   = NavIcon_None;
    bool        m_active = false;
    bool        m_hover  = false;
};

// =========================================================================
// SidebarPanel —— Sidebar 整体容器。继承 DuiVBox 加一层背景填充
// （balloonui layout 容器自身不画背景）。
// =========================================================================
class SidebarPanel : public DuiVBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        HBRUSH hbr = ::CreateSolidBrush(kColorSurfaceContainerLow);
        ::FillRect(hdc, &m_rcItem, hbr);
        ::DeleteObject(hbr);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

// 一行 nav item 的工厂：分配 + 设 text / icon / id / active 初值。
std::unique_ptr<SidebarNavItem> MakeNavItem(LPCTSTR text, int navId,
                                            NavIconKind icon, bool active)
{
    auto item = std::make_unique<SidebarNavItem>();
    item->SetText(text);
    item->SetIcon(icon);
    item->SetCtrlId(navId);
    item->SetActive(active);
    return item;
}

// 段头 label（如「我的音乐」）—— 小号灰字，左缩进对齐 nav item 文字。
std::unique_ptr<DuiLabel> MakeSectionHeader(LPCTSTR text)
{
    auto lbl = std::make_unique<DuiLabel>();
    lbl->SetText(text);
    lbl->SetTextColor(kColorOnSurfaceVar);
    lbl->SetFont(Fonts::Get(Fonts::LabelXs));
    return lbl;
}

} // anonymous

std::unique_ptr<DuiControl> BuildSidebar(int initialNav)
{
    auto root = std::make_unique<SidebarPanel>();
    root->SetGap(2);
    // 上下左右 padding：左右 8（让 active 红条贴近左边）；上 16（品牌
    // 移到标题栏后顶部需要适当呼吸位）；下 12。
    root->SetPadding(8, 16, 8, 12);

    // 1) 主导航三项（发现 / 播客 / 音乐）—— 每项配对应 icon
    root->AddChild(MakeNavItem(_T("发现"), NavId_Discover, NavIcon_Discover,
                               initialNav == NavId_Discover),
                   DuiLayout::Hint().Fixed(kNavItemHeight));
    root->AddChild(MakeNavItem(_T("播客"), NavId_Podcast, NavIcon_Podcast,
                               initialNav == NavId_Podcast),
                   DuiLayout::Hint().Fixed(kNavItemHeight));
    root->AddChild(MakeNavItem(_T("音乐"), NavId_Music, NavIcon_Music,
                               initialNav == NavId_Music),
                   DuiLayout::Hint().Fixed(kNavItemHeight));

    // 3) 「我的音乐」段头（与 nav text 起点对齐，需要让 text-left =
    //    icon 区右边 = kNavIconLeft + kNavIconSize + kNavTextAfterIcon =
    //    14+16+10 = 40px。这里直接用空格字符近似。）
    auto sec = MakeSectionHeader(_T("       我的音乐"));
    root->AddChild(std::move(sec), DuiLayout::Hint().Fixed(28));

    // 4) 我的音乐三项
    root->AddChild(MakeNavItem(_T("收藏"), NavId_Favorites, NavIcon_Favorites,
                               initialNav == NavId_Favorites),
                   DuiLayout::Hint().Fixed(kNavItemHeight));
    root->AddChild(MakeNavItem(_T("最近播放"), NavId_RecentPlay, NavIcon_Recent,
                               initialNav == NavId_RecentPlay),
                   DuiLayout::Hint().Fixed(kNavItemHeight));
    root->AddChild(MakeNavItem(_T("个人中心"), NavId_Profile, NavIcon_Profile,
                               initialNav == NavId_Profile),
                   DuiLayout::Hint().Fixed(kNavItemHeight));

    // 5) 弹性 spacer —— 把下面的 PillButton + 设置 / 帮助挤到底部
    auto spacer = std::make_unique<DuiControl>();
    root->AddChild(std::move(spacer), DuiLayout::Hint().Weight(1));

    // 6) PillButton 「+ 创建歌单」 —— 全宽减 padding，两侧 4px 内边距让
    //    胶囊不贴 active bar 的位置。
    auto pill = std::make_unique<PillButton>();
    pill->SetText(_T("+ 创建歌单"));
    pill->SetCtrlId(NavId_CreatePlaylist);
    root->AddChild(std::move(pill), DuiLayout::Hint().Fixed(36));

    // 7) 底部 设置 / 帮助
    root->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Fixed(8));
    root->AddChild(MakeNavItem(_T("设置"), NavId_Settings,
                               NavIcon_Settings, false),
                   DuiLayout::Hint().Fixed(kNavItemHeight));
    root->AddChild(MakeNavItem(_T("帮助"), NavId_Help,
                               NavIcon_Help, false),
                   DuiLayout::Hint().Fixed(kNavItemHeight));

    return root;
}

void SetActiveNav(DuiControl* sidebar, int navId)
{
    if (!sidebar)
    {
        return;
    }
    static const int kNavs[] = {
        NavId_Discover, NavId_Podcast, NavId_Music,
        NavId_Favorites, NavId_RecentPlay, NavId_Profile
    };
    for (int id : kNavs)
    {
        if (auto* c = sidebar->FindCtrlById((UINT)id))
        {
            if (auto* item = dynamic_cast<SidebarNavItem*>(c))
            {
                item->SetActive(id == navId);
            }
        }
    }
}

} // namespace cloudmelody
