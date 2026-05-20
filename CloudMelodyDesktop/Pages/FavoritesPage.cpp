#include "stdafx.h"
#include "FavoritesPage.h"
#include "../App/CloudMelodyTheme.h"
#include "../App/CloudMelodyFonts.h"
#include "../App/Mock/MockMusic.h"
#include "../Controls/PillButton.h"
#include "../Controls/MusicCard.h"
#include "../Controls/CoverArt.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiSeparator.h"
#include "Controls/List/DuiTab.h"

using namespace balloonwjui;

namespace cloudmelody {

namespace {

// 歌单大封面尺寸（设计稿目测 180×180）
static const int kPlaylistCoverSize = 180;
// 艺人卡尺寸
static const int kArtistCardSize = 130;

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

// "我喜欢的音乐"歌单大封面 —— 红色（hue 0）渐变 + 标题字
class BigCoverPlaceholder : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }
        CoverArt::Paint(hdc, m_rcItem, _T("我喜欢的音乐"), 0, kRadiusMd);
    }
};

// 顶部 hero 行（封面 + 文字块 + 按钮）
std::unique_ptr<DuiControl> MakeHero()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(28);

    row->AddChild(std::make_unique<BigCoverPlaceholder>(),
                  DuiLayout::Hint().Fixed(kPlaylistCoverSize));

    auto col = std::make_unique<DuiVBox>();
    col->SetGap(6);
    col->SetPadding(0, 24, 0, 0);

    auto tag = std::make_unique<DuiLabel>();
    tag->SetText(_T("PLAYLIST"));
    tag->SetTextColor(kColorOnSurfaceVar);
    tag->SetFont(Fonts::Get(Fonts::LabelXs));
    col->AddChild(std::move(tag), DuiLayout::Hint().Fixed(18));

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("我喜欢的音乐"));
    title->SetTextColor(kColorOnSurface);
    title->SetFont(Fonts::Get(Fonts::DisplayLg));
    col->AddChild(std::move(title), DuiLayout::Hint().Fixed(40));

    auto sub = std::make_unique<DuiLabel>();
    sub->SetText(_T("张小方 · 138 张专辑 · 7 个时段"));
    sub->SetTextColor(kColorOnSurfaceVar);
    sub->SetFont(Fonts::Get(Fonts::BodySm));
    col->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    // 按钮行
    auto btnRow = std::make_unique<DuiHBox>();
    btnRow->SetGap(12);
    btnRow->SetPadding(0, 12, 0, 0);

    auto play = std::make_unique<PillButton>();
    play->SetText(_T("▶ 播放全部"));
    btnRow->AddChild(std::move(play), DuiLayout::Hint().Fixed(120));

    auto share = std::make_unique<PillButton>();
    share->SetText(_T("⤴ 分享"));
    share->SetStyle(PillButton::StyleSecondary);
    btnRow->AddChild(std::move(share), DuiLayout::Hint().Fixed(96));

    btnRow->AddChild(std::make_unique<DuiControl>(),
                     DuiLayout::Hint().Weight(1));
    col->AddChild(std::move(btnRow), DuiLayout::Hint().Fixed(40));

    row->AddChild(std::move(col), DuiLayout::Hint().Weight(1));
    return row;
}

// 6 张艺人 / 歌手卡（用 MusicCard 复用）—— 数据走 latestTracks，每首歌
// 当成"一位艺人"渲染。用 artist 作主标题、duration / album 当副。
std::unique_ptr<DuiControl> MakeArtistGrid()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(16);
    int n = (kLatestTrackCount > 6) ? 6 : kLatestTrackCount;
    for (int i = 0; i < n; ++i)
    {
        auto card = std::make_unique<MusicCard>();
        card->SetTitle(kLatestTracks[i].artist);
        card->SetSubtitle(kLatestTracks[i].album);
        card->SetHueSeed(i + 2);            // +2 让本页色调跟 Discover 错开
        card->SetCtrlId(2000 + (UINT)i);
        row->AddChild(std::move(card),
                      DuiLayout::Hint().Fixed(kArtistCardSize));
    }
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

// 单曲列表行（与 LocalMusicPage 同 schema 简化版）
std::unique_ptr<DuiControl> MakeTrackRow(int idx, const MockTrack& t)
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(0);
    auto add = [&](LPCTSTR text, int w, COLORREF c)
    {
        auto lbl = std::make_unique<DuiLabel>();
        lbl->SetText(text);
        lbl->SetTextColor(c);
        row->AddChild(std::move(lbl), DuiLayout::Hint().Fixed(w));
    };
    CString num;
    num.Format(_T("%02d"), idx);
    add(num,        40,  kColorOnSurfaceVar);
    add(t.title,    220, kColorOnSurface);
    add(t.album,    180, kColorOnSurfaceVar);
    add(t.duration, 60,  kColorOnSurfaceVar);
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

std::unique_ptr<DuiControl> MakeSectionHeader(LPCTSTR text)
{
    auto lbl = std::make_unique<DuiLabel>();
    lbl->SetText(text);
    lbl->SetTextColor(kColorOnSurface);
    lbl->SetFont(Fonts::Get(Fonts::HeadlineMd));
    return lbl;
}

} // anonymous

std::unique_ptr<DuiControl> BuildFavoritesPage()
{
    auto page = std::make_unique<PageHost>();
    page->SetGap(28);
    page->SetPadding(kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin);

    // 1) Hero
    page->AddChild(MakeHero(), DuiLayout::Hint().Fixed(kPlaylistCoverSize));

    // 2) "收藏的歌单与艺人" + Tab + 网格
    auto artistsCol = std::make_unique<DuiVBox>();
    artistsCol->SetGap(16);

    // 标题行：左 section header + 右 tab
    auto titleRow = std::make_unique<DuiHBox>();
    titleRow->SetGap(20);
    titleRow->AddChild(MakeSectionHeader(_T("收藏的歌单与艺人")),
                       DuiLayout::Hint().Weight(1));

    auto tab = std::make_unique<DuiTab>();
    tab->AddTab(_T("全部"));
    tab->AddTab(_T("歌单"));
    tab->AddTab(_T("艺人"));
    tab->SetCurSel(0);
    titleRow->AddChild(std::move(tab), DuiLayout::Hint().Fixed(180));

    artistsCol->AddChild(std::move(titleRow), DuiLayout::Hint().Fixed(36));

    // 艺人卡网格
    artistsCol->AddChild(MakeArtistGrid(),
                         DuiLayout::Hint().Fixed(kArtistCardSize + 44));

    page->AddChild(std::move(artistsCol),
                   DuiLayout::Hint().Fixed(36 + 16 + kArtistCardSize + 44));

    // 3) 单曲列表
    auto songsCol = std::make_unique<DuiVBox>();
    songsCol->SetGap(8);
    songsCol->AddChild(MakeSectionHeader(_T("单曲列表")),
                       DuiLayout::Hint().Fixed(28));
    auto sep = std::make_unique<DuiSeparator>();
    songsCol->AddChild(std::move(sep), DuiLayout::Hint().Fixed(1));
    for (int i = 0; i < kLatestTrackCount; ++i)
    {
        songsCol->AddChild(MakeTrackRow(i + 1, kLatestTracks[i]),
                           DuiLayout::Hint().Fixed(34));
    }
    page->AddChild(std::move(songsCol), DuiLayout::Hint().Weight(1));

    return page;
}

} // namespace cloudmelody
