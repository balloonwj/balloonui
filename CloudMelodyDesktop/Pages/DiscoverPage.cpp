#include "stdafx.h"
#include "DiscoverPage.h"
#include "../App/CloudMelodyTheme.h"
#include "../App/CloudMelodyFonts.h"
#include "../App/Mock/MockMusic.h"
#include "../Controls/PillButton.h"
#include "../Controls/MusicCard.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"

using namespace balloonwjui;

namespace cloudmelody {

namespace {

// Banner 高度（设计稿目测 ~180）
static const int kBannerHeight     = 180;
// "精选歌单"卡片宽（5 张并排在 ~860 内容区里 = 5*140 + 4*20 = 780，留 80 余）
static const int kCardSize         = 140;
static const int kCardSpacing      = 20;
static const int kCardTotalHeight  = 140 + 44;  // cover + 文字区

// =========================================================================
// PageScrollHost —— 内容页根容器。继承 DuiVBox 加一层背景填充（白色
// surface）。M3 暂不做滚动条，内容长度足够 800 多高的窗口高度内塞下。
//
// M4+ 内容更长（如 RecentPlay 长列表）时把它换成 ListBox 或挂 DuiScrollBar
// 滚动包装。
// =========================================================================
class PageScrollHost : public DuiVBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        HBRUSH hbr = ::CreateSolidBrush(kColorSurface);
        ::FillRect(hdc, &m_rcItem, hbr);
        ::DeleteObject(hbr);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

// =========================================================================
// HeroBanner —— 红底大 banner（"华语流行：城市之声" + 两个 CTA 按钮）。
// 设计稿背景是图片+渐变；M3 第一版用纯红 fallback，M7 polish 接 PNG。
// =========================================================================
class HeroBanner : public DuiHBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        HRGN hrgn = ::CreateRoundRectRgn(m_rcItem.left, m_rcItem.top,
                                         m_rcItem.right + 1, m_rcItem.bottom + 1,
                                         kRadiusLg * 2, kRadiusLg * 2);
        HBRUSH hbr = ::CreateSolidBrush(kColorPrimary);
        ::FillRgn(hdc, hrgn, hbr);
        ::DeleteObject(hbr);
        ::DeleteObject(hrgn);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

std::unique_ptr<DuiControl> MakeBanner()
{
    auto banner = std::make_unique<HeroBanner>();
    banner->SetGap(0);
    banner->SetPadding(32, 28, 32, 28);

    // 左侧：tag + 主标题 + 副标题 + 两个按钮
    auto col = std::make_unique<DuiVBox>();
    col->SetGap(8);

    auto tag = std::make_unique<DuiLabel>();
    tag->SetText(_T("[ 精选 ]  华语流行"));
    tag->SetTextColor(RGB(255, 230, 230));
    tag->SetFont(Fonts::Get(Fonts::LabelXs));
    col->AddChild(std::move(tag), DuiLayout::Hint().Fixed(18));

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("华语流行：城市之声"));
    title->SetTextColor(kColorOnPrimary);
    title->SetFont(Fonts::Get(Fonts::DisplayLg));
    col->AddChild(std::move(title), DuiLayout::Hint().Fixed(38));

    auto sub = std::make_unique<DuiLabel>();
    sub->SetText(_T("捕捉霓虹深夜、雨后街道与城市心跳的最新华语单曲。"));
    sub->SetTextColor(RGB(255, 230, 230));
    sub->SetFont(Fonts::Get(Fonts::BodyMd));
    col->AddChild(std::move(sub), DuiLayout::Hint().Fixed(22));

    // CTA 按钮行
    auto btnRow = std::make_unique<DuiHBox>();
    btnRow->SetGap(12);
    btnRow->SetPadding(0, 16, 0, 0);

    // Banner 在红底上 —— 主按钮反色（白底红字）通过 OnDark 的 pressed 态
    // 暂用，第一刀仍用主品牌红 fill；次按钮"了解更多"用 OnDark 透明白边。
    auto play = std::make_unique<PillButton>();
    play->SetText(_T("立即播放"));
    btnRow->AddChild(std::move(play), DuiLayout::Hint().Fixed(120));

    auto more = std::make_unique<PillButton>();
    more->SetText(_T("了解更多"));
    more->SetStyle(PillButton::StyleOnDark);
    btnRow->AddChild(std::move(more), DuiLayout::Hint().Fixed(120));

    btnRow->AddChild(std::make_unique<DuiControl>(),
                     DuiLayout::Hint().Weight(1));

    col->AddChild(std::move(btnRow), DuiLayout::Hint().Fixed(40));

    banner->AddChild(std::move(col), DuiLayout::Hint().Weight(1));
    return banner;
}

// section header：左主标题 + 右"查看全部"超链接风 label
std::unique_ptr<DuiControl> MakeSectionHeader(LPCTSTR title, LPCTSTR linkText)
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(8);
    row->SetPadding(0, 0, 0, 0);

    auto t = std::make_unique<DuiLabel>();
    t->SetText(title);
    t->SetTextColor(kColorOnSurface);
    t->SetFont(Fonts::Get(Fonts::HeadlineMd));
    row->AddChild(std::move(t), DuiLayout::Hint().Weight(1));

    if (linkText && linkText[0])
    {
        auto link = std::make_unique<DuiLabel>();
        link->SetText(linkText);
        link->SetTextColor(kColorPrimaryAccent);
        link->SetFont(Fonts::Get(Fonts::BodyMd));
        row->AddChild(std::move(link), DuiLayout::Hint().Fixed(72));
    }
    return row;
}

// 5 张 MusicCard 横排
std::unique_ptr<DuiControl> MakeFeaturedRow()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(kCardSpacing);
    for (int i = 0; i < kFeaturedPlaylistCount; ++i)
    {
        auto card = std::make_unique<MusicCard>();
        card->SetTitle(kFeaturedPlaylists[i].title);
        card->SetSubtitle(kFeaturedPlaylists[i].subtitle);
        card->SetHueSeed(i);
        // 卡片 ctrl id = 1000 + idx，便于 host 路由"点击第 N 张"
        card->SetCtrlId(1000 + (UINT)i);
        row->AddChild(std::move(card),
                      DuiLayout::Hint().Fixed(kCardSize));
    }
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

// "最新音乐" 简表 —— M3 第一版用 "# 标题 - 歌手 - 时长" 一行 label，未
// 走 DuiTreeView，让 page 渲染负担轻。M4 起切到正式表格视图。
std::unique_ptr<DuiControl> MakeLatestList()
{
    auto col = std::make_unique<DuiVBox>();
    col->SetGap(8);
    col->SetPadding(0, 0, 0, 0);

    for (int i = 0; i < kLatestTrackCount; ++i)
    {
        const auto& t = kLatestTracks[i];
        CString line;
        line.Format(_T("%02d   %-12s   %-8s   %s"),
                    i + 1, t.title, t.artist, t.duration);
        auto lbl = std::make_unique<DuiLabel>();
        lbl->SetText(line);
        lbl->SetTextColor(kColorOnSurface);
        col->AddChild(std::move(lbl), DuiLayout::Hint().Fixed(28));
    }
    return col;
}

} // anonymous

std::unique_ptr<DuiControl> BuildDiscoverPage()
{
    auto page = std::make_unique<PageScrollHost>();
    page->SetGap(kSizeSectionGap);
    page->SetPadding(kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin);

    page->AddChild(MakeBanner(), DuiLayout::Hint().Fixed(kBannerHeight));

    // 「精选歌单」section
    auto featuredCol = std::make_unique<DuiVBox>();
    featuredCol->SetGap(16);
    featuredCol->AddChild(MakeSectionHeader(_T("精选歌单"), _T("查看全部")),
                          DuiLayout::Hint().Fixed(24));
    featuredCol->AddChild(MakeFeaturedRow(),
                          DuiLayout::Hint().Fixed(kCardTotalHeight));
    page->AddChild(std::move(featuredCol),
                   DuiLayout::Hint().Fixed(24 + 16 + kCardTotalHeight));

    // 「最新音乐」section
    auto latestCol = std::make_unique<DuiVBox>();
    latestCol->SetGap(16);
    latestCol->AddChild(MakeSectionHeader(_T("最新音乐"), _T("更多")),
                        DuiLayout::Hint().Fixed(24));
    latestCol->AddChild(MakeLatestList(), DuiLayout::Hint().Weight(1));
    page->AddChild(std::move(latestCol), DuiLayout::Hint().Weight(1));

    return page;
}

} // namespace cloudmelody
