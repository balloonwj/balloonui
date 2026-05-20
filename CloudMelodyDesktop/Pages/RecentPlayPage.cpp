#include "stdafx.h"
#include "RecentPlayPage.h"
#include "../App/CloudMelodyTheme.h"
#include "../App/CloudMelodyFonts.h"
#include "../App/Mock/MockMusic.h"
#include "../Controls/PillButton.h"
#include "../Controls/CoverArt.h"
#include "../Controls/PaintHelpers.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiSeparator.h"

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

// 红底 Hero（"正在播放的专辑 / 这就是我"）
class NowPlayingHero : public DuiHBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        PaintHelpers::FillRoundedRect(hdc, m_rcItem,
                                      kColorPrimary, kRadiusLg);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

// 灰底"最近播客"卡
class PodcastChip : public DuiHBox
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

class CoverPlaceholder : public DuiControl
{
public:
    explicit CoverPlaceholder(int seed) : m_seed(seed) {}
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }
        CoverArt::Paint(hdc, m_rcItem, nullptr, m_seed, kRadiusMd);
    }
private:
    int m_seed = 0;
};

std::unique_ptr<DuiControl> MakeHero()
{
    auto hero = std::make_unique<NowPlayingHero>();
    hero->SetGap(20);
    hero->SetPadding(20, 18, 20, 18);

    // 左：封面 —— "这就是我"专辑用紫色渐变（hueSeed=1），与红 banner 对比
    hero->AddChild(std::make_unique<CoverPlaceholder>(1),
                   DuiLayout::Hint().Fixed(96));

    // 中：tag + 标题 + 副标
    auto col = std::make_unique<DuiVBox>();
    col->SetGap(4);
    col->SetPadding(0, 8, 0, 0);

    auto tag = std::make_unique<DuiLabel>();
    tag->SetText(_T("正在播放的专辑"));
    tag->SetTextColor(RGB(255, 230, 230));
    col->AddChild(std::move(tag), DuiLayout::Hint().Fixed(18));

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("这就是我"));
    title->SetTextColor(kColorOnPrimary);
    title->SetFont(Fonts::Get(Fonts::HeadlineMd));
    col->AddChild(std::move(title), DuiLayout::Hint().Fixed(32));

    auto sub = std::make_unique<DuiLabel>();
    sub->SetText(_T("陈奕迅 · 2024 · 流行"));
    sub->SetTextColor(RGB(255, 230, 230));
    col->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    col->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    hero->AddChild(std::move(col), DuiLayout::Hint().Weight(1));

    // 右：播放按钮 + +按钮（白色 pill 反色）
    auto play = std::make_unique<PillButton>();
    play->SetText(_T("▶"));
    hero->AddChild(std::move(play), DuiLayout::Hint().Fixed(40));

    auto plus = std::make_unique<PillButton>();
    plus->SetText(_T("+"));
    hero->AddChild(std::move(plus), DuiLayout::Hint().Fixed(40));

    return hero;
}

std::unique_ptr<DuiControl> MakePodcastBanner()
{
    auto chip = std::make_unique<PodcastChip>();
    chip->SetGap(12);
    chip->SetPadding(16, 12, 16, 12);

    // 播客封面 —— 橙色（hueSeed=2）
    chip->AddChild(std::make_unique<CoverPlaceholder>(2),
                   DuiLayout::Hint().Fixed(60));

    auto col = std::make_unique<DuiVBox>();
    col->SetGap(2);
    auto t = std::make_unique<DuiLabel>();
    t->SetText(_T("最近的播客"));
    t->SetTextColor(kColorOnSurface);
    col->AddChild(std::move(t), DuiLayout::Hint().Fixed(20));

    auto s = std::make_unique<DuiLabel>();
    s->SetText(_T("凯叔讲故事 · 第 128 集"));
    s->SetTextColor(kColorOnSurfaceVar);
    col->AddChild(std::move(s), DuiLayout::Hint().Fixed(18));

    col->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    chip->AddChild(std::move(col), DuiLayout::Hint().Weight(1));

    return chip;
}

std::unique_ptr<DuiControl> MakeTrackHeader()
{
    auto row = std::make_unique<DuiHBox>();
    auto add = [&](LPCTSTR text, int w)
    {
        auto lbl = std::make_unique<DuiLabel>();
        lbl->SetText(text);
        lbl->SetTextColor(kColorOnSurfaceVar);
        row->AddChild(std::move(lbl), DuiLayout::Hint().Fixed(w));
    };
    add(_T("# 标题"),    260);
    add(_T("播放时间"),  120);
    add(_T("时长"),      80);
    add(_T("操作"),      80);
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

std::unique_ptr<DuiControl> MakeTrackRow(int idx, const MockTrack& t)
{
    auto row = std::make_unique<DuiHBox>();
    auto add = [&](LPCTSTR text, int w, COLORREF c)
    {
        auto lbl = std::make_unique<DuiLabel>();
        lbl->SetText(text);
        lbl->SetTextColor(c);
        row->AddChild(std::move(lbl), DuiLayout::Hint().Fixed(w));
    };
    CString first;
    first.Format(_T("%02d  %s"), idx, t.title);
    add(first,             260, kColorOnSurface);
    add(_T("2 小时前"),     120, kColorOnSurfaceVar);
    add(t.duration,        80,  kColorOnSurfaceVar);
    add(_T("..."),         80,  kColorOnSurfaceVar);
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

} // anonymous

std::unique_ptr<DuiControl> BuildRecentPlayPage()
{
    auto page = std::make_unique<PageHost>();
    page->SetGap(20);
    page->SetPadding(kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin);

    // 标题 + CTA 按钮
    auto headerRow = std::make_unique<DuiHBox>();
    headerRow->SetGap(20);

    auto titleCol = std::make_unique<DuiVBox>();
    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("最近播放"));
    title->SetTextColor(kColorOnSurface);
    title->SetFont(Fonts::Get(Fonts::DisplayLg));
    titleCol->AddChild(std::move(title), DuiLayout::Hint().Fixed(40));

    auto sub = std::make_unique<DuiLabel>();
    sub->SetText(_T("回顾你最近五次播放与播客的对话"));
    sub->SetTextColor(kColorOnSurfaceVar);
    sub->SetFont(Fonts::Get(Fonts::BodyMd));
    titleCol->AddChild(std::move(sub), DuiLayout::Hint().Fixed(22));

    headerRow->AddChild(std::move(titleCol), DuiLayout::Hint().Weight(1));

    // 「清除全部」是危险次级 —— 用灰描边；「播放全部」是主操作
    auto clear = std::make_unique<PillButton>();
    clear->SetText(_T("✕ 清除全部"));
    clear->SetStyle(PillButton::StyleSecondary);
    headerRow->AddChild(std::move(clear), DuiLayout::Hint().Fixed(120));
    auto playAll = std::make_unique<PillButton>();
    playAll->SetText(_T("▶ 播放全部"));
    headerRow->AddChild(std::move(playAll), DuiLayout::Hint().Fixed(120));
    page->AddChild(std::move(headerRow), DuiLayout::Hint().Fixed(64));

    // hero + 播客 banner（左 hero / 右 chip）
    auto heroRow = std::make_unique<DuiHBox>();
    heroRow->SetGap(20);
    heroRow->AddChild(MakeHero(),         DuiLayout::Hint().Weight(1));
    heroRow->AddChild(MakePodcastBanner(), DuiLayout::Hint().Fixed(280));
    page->AddChild(std::move(heroRow), DuiLayout::Hint().Fixed(140));

    // 单曲列表
    page->AddChild(MakeTrackHeader(), DuiLayout::Hint().Fixed(28));
    auto sep = std::make_unique<DuiSeparator>();
    page->AddChild(std::move(sep), DuiLayout::Hint().Fixed(1));
    for (int i = 0; i < kLatestTrackCount; ++i)
    {
        page->AddChild(MakeTrackRow(i + 1, kLatestTracks[i]),
                       DuiLayout::Hint().Fixed(36));
    }

    page->AddChild(std::make_unique<DuiControl>(),
                   DuiLayout::Hint().Weight(1));
    return page;
}

} // namespace cloudmelody
