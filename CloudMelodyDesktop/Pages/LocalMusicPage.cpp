#include "stdafx.h"
#include "LocalMusicPage.h"
#include "../App/CloudMelodyTheme.h"
#include "../App/CloudMelodyFonts.h"
#include "../App/Mock/MockMusic.h"
#include "../Controls/PillButton.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiSeparator.h"
#include "Controls/List/DuiTab.h"
#include "DuiResMgr.h"

using namespace balloonwjui;

namespace cloudmelody {

namespace {

// =========================================================================
// PageHost —— 简单白底 vbox。M3 里也有同款 PageScrollHost；为避免跨页
// 跨文件的"全局共享"耦合，先各自一份；M7 抽到通用基类。
// =========================================================================
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

// 「全部 1,248 首」对应一行 label。设计稿目测主标题 30px、副标题 13px。
std::unique_ptr<DuiControl> MakeHeader()
{
    auto col = std::make_unique<DuiVBox>();
    col->SetGap(4);

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("本地音乐"));
    title->SetTextColor(kColorOnSurface);
    title->SetFont(Fonts::Get(Fonts::DisplayLg));
    col->AddChild(std::move(title), DuiLayout::Hint().Fixed(38));

    auto sub = std::make_unique<DuiLabel>();
    sub->SetText(_T("共 1,248 首歌曲"));
    sub->SetTextColor(kColorOnSurfaceVar);
    sub->SetFont(Fonts::Get(Fonts::BodySm));
    col->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    return col;
}

// CTA 行（▶ 播放全部 / ✕ 随机播放）+ 右侧弹性 spacer
std::unique_ptr<DuiControl> MakeCtaRow()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(12);

    auto play = std::make_unique<PillButton>();
    play->SetText(_T("▶ 播放全部"));
    row->AddChild(std::move(play), DuiLayout::Hint().Fixed(120));

    // 「随机播放」是次级 —— 灰描边 + 深灰字
    auto rand = std::make_unique<PillButton>();
    rand->SetText(_T("✕ 随机播放"));
    rand->SetStyle(PillButton::StyleSecondary);
    row->AddChild(std::move(rand), DuiLayout::Hint().Fixed(120));

    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

// 单曲表表头（# / 标题 / 歌手 / 专辑 / 时长）。固定列宽，与下面行匹配。
//   col widths（px）:  40 / 220 / 140 / 140 / 60
std::unique_ptr<DuiControl> MakeTrackHeader()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(0);

    auto add = [&](LPCTSTR text, int w)
    {
        auto lbl = std::make_unique<DuiLabel>();
        lbl->SetText(text);
        lbl->SetTextColor(kColorOnSurfaceVar);
        row->AddChild(std::move(lbl), DuiLayout::Hint().Fixed(w));
    };
    add(_T("#"),     40);
    add(_T("标题"),  220);
    add(_T("歌手"),  140);
    add(_T("专辑"),  140);
    add(_T("时长"),  60);
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

// 单曲行（与 header 列宽对齐）
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
    add(num,        40, kColorOnSurfaceVar);
    add(t.title,    220, kColorOnSurface);
    add(t.artist,   140, kColorOnSurfaceVar);
    add(t.album,    140, kColorOnSurfaceVar);
    add(t.duration, 60,  kColorOnSurfaceVar);
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

} // anonymous

std::unique_ptr<DuiControl> BuildLocalMusicPage()
{
    auto page = std::make_unique<PageHost>();
    page->SetGap(20);
    page->SetPadding(kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin);

    // 1) 标题 + 副 label —— 与 CTA 同行，左中右拆。
    auto headerRow = std::make_unique<DuiHBox>();
    headerRow->SetGap(20);
    headerRow->AddChild(MakeHeader(), DuiLayout::Hint().Weight(1));
    headerRow->AddChild(MakeCtaRow(), DuiLayout::Hint().Fixed(280));
    page->AddChild(std::move(headerRow), DuiLayout::Hint().Fixed(64));

    // 2) Tab 行
    auto tab = std::make_unique<DuiTab>();
    tab->AddTab(_T("全部歌曲"));
    tab->AddTab(_T("歌手"));
    tab->AddTab(_T("专辑"));
    tab->AddTab(_T("正在下载"));
    tab->SetCurSel(0);
    page->AddChild(std::move(tab), DuiLayout::Hint().Fixed(36));

    // 3) 分隔线
    auto sep = std::make_unique<DuiSeparator>();
    page->AddChild(std::move(sep), DuiLayout::Hint().Fixed(1));

    // 4) 表头
    page->AddChild(MakeTrackHeader(), DuiLayout::Hint().Fixed(28));

    // 5) 6 行单曲（用 MockMusic 的 latestTracks 充当本地音乐 demo）
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
