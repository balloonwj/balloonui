#include "stdafx.h"
#include "PodcastPage.h"
#include "../App/CloudMelodyTheme.h"
#include "../App/CloudMelodyFonts.h"
#include "../App/Mock/MockMusic.h"
#include "../Controls/PillButton.h"
#include "../Controls/MusicCard.h"
#include "../Controls/PaintHelpers.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"

using namespace balloonwjui;

namespace cloudmelody {

namespace {

class PageHost : public DuiVBox
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

// 红底 banner —— "精选播客 / 声动早咖啡"
class HeroBanner : public DuiVBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        PaintHelpers::FillRoundedRect(hdc, m_rcItem, kColorPrimary, kRadiusLg);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

// 灰底小卡（"随机抽听" / "大爪爱听"）
class ChipCard : public DuiVBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        PaintHelpers::FillRoundedRect(hdc, m_rcItem,
                                      kColorSurfaceContainer, kRadiusLg);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

std::unique_ptr<DuiControl> MakeBanner()
{
    auto banner = std::make_unique<HeroBanner>();
    banner->SetGap(8);
    banner->SetPadding(28, 24, 28, 24);

    auto tag = std::make_unique<DuiLabel>();
    tag->SetText(_T("精选播客"));
    tag->SetTextColor(RGB(255, 230, 230));
    tag->SetFont(Fonts::Get(Fonts::LabelXs));
    banner->AddChild(std::move(tag), DuiLayout::Hint().Fixed(18));

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("声动早咖啡"));
    title->SetTextColor(kColorOnPrimary);
    title->SetFont(Fonts::Get(Fonts::DisplayLg));
    banner->AddChild(std::move(title), DuiLayout::Hint().Fixed(40));

    auto sub = std::make_unique<DuiLabel>();
    sub->SetText(_T("每日早晨，我们陪你聊全球商业与文娱新趋势。"));
    sub->SetTextColor(RGB(255, 230, 230));
    sub->SetFont(Fonts::Get(Fonts::BodyMd));
    banner->AddChild(std::move(sub), DuiLayout::Hint().Fixed(22));

    auto play = std::make_unique<PillButton>();
    play->SetText(_T("立即播放"));
    banner->AddChild(std::move(play), DuiLayout::Hint().Fixed(40));

    banner->AddChild(std::make_unique<DuiControl>(),
                     DuiLayout::Hint().Weight(1));
    return banner;
}

std::unique_ptr<DuiControl> MakeChipCard(LPCTSTR title, LPCTSTR sub)
{
    auto c = std::make_unique<ChipCard>();
    c->SetGap(4);
    c->SetPadding(20, 18, 20, 18);

    auto t = std::make_unique<DuiLabel>();
    t->SetText(title);
    t->SetTextColor(kColorOnSurface);
    c->AddChild(std::move(t), DuiLayout::Hint().Fixed(22));

    auto s = std::make_unique<DuiLabel>();
    s->SetText(sub);
    s->SetTextColor(kColorOnSurfaceVar);
    c->AddChild(std::move(s), DuiLayout::Hint().Fixed(18));

    c->AddChild(std::make_unique<DuiControl>(),
                DuiLayout::Hint().Weight(1));
    return c;
}

// 「全部分类」胶囊按钮组：音乐 / 商业 / 娱乐 / 知识 / 故事 / 人文艺术
std::unique_ptr<DuiControl> MakeCategoryChips()
{
    static const LPCTSTR kCats[] = {
        _T("音乐"), _T("商业"), _T("娱乐"),
        _T("知识"), _T("故事"), _T("人文艺术")
    };
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(8);
    for (auto cat : kCats)
    {
        // 分类胶囊：次级风格（灰描边 + 深灰字），与 banner 上的 OnDark
        // 主按钮形成视觉层级。第一个"音乐"是 active 用主红，其它灰。
        auto b = std::make_unique<PillButton>();
        b->SetText(cat);
        b->SetStyle(cat == kCats[0] ? PillButton::StylePrimary
                                    : PillButton::StyleSecondary);
        row->AddChild(std::move(b), DuiLayout::Hint().Fixed(80));
    }
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

std::unique_ptr<DuiControl> MakePodcastGrid()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(16);
    int n = kFeaturedPlaylistCount;
    for (int i = 0; i < n; ++i)
    {
        auto card = std::make_unique<MusicCard>();
        card->SetTitle(kFeaturedPlaylists[i].title);
        card->SetSubtitle(kFeaturedPlaylists[i].subtitle);
        card->SetHueSeed(i + 1);            // 错开 Discover 起色
        row->AddChild(std::move(card), DuiLayout::Hint().Fixed(140));
    }
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

std::unique_ptr<DuiControl> MakeSectionHeader(LPCTSTR title)
{
    auto lbl = std::make_unique<DuiLabel>();
    lbl->SetText(title);
    lbl->SetTextColor(kColorOnSurface);
    lbl->SetFont(Fonts::Get(Fonts::HeadlineMd));
    return lbl;
}

} // anonymous

std::unique_ptr<DuiControl> BuildPodcastPage()
{
    auto page = std::make_unique<PageHost>();
    page->SetGap(24);
    page->SetPadding(kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin);

    // banner + 右侧两块小卡
    auto topRow = std::make_unique<DuiHBox>();
    topRow->SetGap(20);
    topRow->AddChild(MakeBanner(), DuiLayout::Hint().Weight(1));

    auto rightCol = std::make_unique<DuiVBox>();
    rightCol->SetGap(12);
    rightCol->AddChild(MakeChipCard(_T("随机抽听"),
                                    _T("从你订阅 110 集中挑")),
                       DuiLayout::Hint().Weight(1));
    rightCol->AddChild(MakeChipCard(_T("大爪爱听"),
                                    _T("基于你的常听场景")),
                       DuiLayout::Hint().Weight(1));
    topRow->AddChild(std::move(rightCol), DuiLayout::Hint().Fixed(220));

    page->AddChild(std::move(topRow), DuiLayout::Hint().Fixed(180));

    // 全部分类
    page->AddChild(MakeSectionHeader(_T("全部分类")),
                   DuiLayout::Hint().Fixed(22));
    page->AddChild(MakeCategoryChips(), DuiLayout::Hint().Fixed(36));

    // 热门节目
    page->AddChild(MakeSectionHeader(_T("热门节目")),
                   DuiLayout::Hint().Fixed(22));
    page->AddChild(MakePodcastGrid(),
                   DuiLayout::Hint().Fixed(140 + 44));

    page->AddChild(std::make_unique<DuiControl>(),
                   DuiLayout::Hint().Weight(1));
    return page;
}

} // namespace cloudmelody
