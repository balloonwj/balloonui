#include "stdafx.h"
#include "NowPlayingPage.h"
#include "../App/CloudMelodyTheme.h"
#include "../App/CloudMelodyFonts.h"
#include "../App/Mock/MockMusic.h"
#include "../Controls/RotatingDisc.h"
#include "../Controls/IconButton.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiSeparator.h"
#include "Controls/List/DuiTab.h"
#include "DuiResMgr.h"

using namespace balloonwjui;

namespace cloudmelody {

namespace {

// 黑胶最大边（左半区 fixed）。窗口 1100×720 时左右各 ~550，给 disc 360
// 留 100 padding。
static const int kDiscSide = 360;

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

// 「← 返回」可点 label —— 简化版自绘（DuiLabel 不直接做 click 通知，
// 这里派生加一层）
class BackLabel : public DuiLabel
{
public:
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
};

// 单行歌词控件 —— current 行加粗放大、红色，其它行普通灰
class LyricLine : public DuiControl
{
public:
    void Set(LPCTSTR text, bool current)
    {
        m_text    = text ? text : _T("");
        m_current = current;
        Invalidate();
    }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible || m_text.IsEmpty()) { return; }
        // current 行用 HeadlineMd（22px Semibold），非 current 用 BodyMd
        // （14px）—— 视觉层级在歌词列表里非常关键。
        HFONT hf = m_current ? Fonts::Get(Fonts::HeadlineMd)
                             : Fonts::Get(Fonts::BodyMd);
        HFONT hOld = (HFONT)::SelectObject(hdc, hf);
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF c = m_current ? kColorPrimaryAccent : kColorOnSurfaceVar;
        COLORREF oldC = ::SetTextColor(hdc, c);
        ::DrawText(hdc, (LPCTSTR)m_text, -1,
                   const_cast<RECT*>(&m_rcItem),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOld);
    }

private:
    CString m_text;
    bool    m_current = false;
};

// 左半区：返回按钮（顶）+ disc（中）+ 标题/歌手/♡（底）
std::unique_ptr<DuiControl> MakeLeftBlock(int trackIdx)
{
    auto col = std::make_unique<DuiVBox>();
    col->SetGap(20);
    col->SetPadding(40, 24, 40, 24);

    // 顶部"← 返回"
    {
        auto back = std::make_unique<BackLabel>();
        back->SetText(_T("← 返回"));
        back->SetTextColor(kColorOnSurfaceVar);
        back->SetCtrlId(NowPlayId_Back);
        col->AddChild(std::move(back), DuiLayout::Hint().Fixed(24));
    }

    // 上 spacer 把 disc 往中间推
    col->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));

    // disc 居中（外面再加一层 hbox 让 disc 水平居中）
    {
        auto row = std::make_unique<DuiHBox>();
        row->SetGap(0);
        row->AddChild(std::make_unique<DuiControl>(),
                      DuiLayout::Hint().Weight(1));
        auto disc = std::make_unique<RotatingDisc>();
        disc->SetCtrlId(NowPlayId_Disc);
        disc->SetCoverHueSeed(trackIdx);
        row->AddChild(std::move(disc),
                      DuiLayout::Hint().Fixed(kDiscSide));
        row->AddChild(std::make_unique<DuiControl>(),
                      DuiLayout::Hint().Weight(1));
        col->AddChild(std::move(row), DuiLayout::Hint().Fixed(kDiscSide));
    }

    // 歌名 / 歌手
    {
        auto title = std::make_unique<DuiLabel>();
        title->SetText(kLatestTracks[trackIdx].title);
        title->SetTextColor(kColorOnSurface);
        title->SetFont(Fonts::Get(Fonts::HeadlineMd));
        col->AddChild(std::move(title), DuiLayout::Hint().Fixed(30));

        auto artist = std::make_unique<DuiLabel>();
        artist->SetText(kLatestTracks[trackIdx].artist);
        artist->SetTextColor(kColorOnSurfaceVar);
        artist->SetFont(Fonts::Get(Fonts::BodyMd));
        col->AddChild(std::move(artist), DuiLayout::Hint().Fixed(20));
    }

    // ♡ 收藏 行
    {
        auto row = std::make_unique<DuiHBox>();
        row->SetGap(16);
        auto like = std::make_unique<IconButton>();
        like->SetGlyph(_T("♡"));
        like->SetHoverShape(IconButton::ShapeCircle);
        like->SetCtrlId(NowPlayId_LikeBig);
        row->AddChild(std::move(like), DuiLayout::Hint().Fixed(36));

        auto fav = std::make_unique<IconButton>();
        fav->SetGlyph(_T("☆"));
        fav->SetHoverShape(IconButton::ShapeCircle);
        fav->SetCtrlId(NowPlayId_FavBig);
        row->AddChild(std::move(fav), DuiLayout::Hint().Fixed(36));

        row->AddChild(std::make_unique<DuiControl>(),
                      DuiLayout::Hint().Weight(1));
        col->AddChild(std::move(row), DuiLayout::Hint().Fixed(36));
    }

    // 下 spacer
    col->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));

    return col;
}

// 右半区：tab + 歌词列表
std::unique_ptr<DuiControl> MakeRightBlock()
{
    auto col = std::make_unique<DuiVBox>();
    col->SetGap(16);
    col->SetPadding(0, 32, 40, 32);

    // 上面填一个空白让 tab 不顶头
    col->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Fixed(8));

    auto tab = std::make_unique<DuiTab>();
    tab->AddTab(_T("歌词"));
    tab->AddTab(_T("视频"));
    tab->SetCurSel(0);
    col->AddChild(std::move(tab), DuiLayout::Hint().Fixed(36));

    auto sep = std::make_unique<DuiSeparator>();
    col->AddChild(std::move(sep), DuiLayout::Hint().Fixed(1));

    // 歌词块（current 行 32px、其它行 24px）
    auto lyricsCol = std::make_unique<DuiVBox>();
    lyricsCol->SetGap(8);
    lyricsCol->SetPadding(0, 16, 0, 0);

    for (int i = 0; i < kMockLyricsCount; ++i)
    {
        auto line = std::make_unique<LyricLine>();
        bool isCur = (i == kMockCurrentLyricIdx);
        line->Set(kMockLyrics[i], isCur);
        lyricsCol->AddChild(std::move(line),
                            DuiLayout::Hint().Fixed(isCur ? 32 : 24));
    }
    lyricsCol->AddChild(std::make_unique<DuiControl>(),
                        DuiLayout::Hint().Weight(1));
    col->AddChild(std::move(lyricsCol), DuiLayout::Hint().Weight(1));

    return col;
}

} // anonymous

std::unique_ptr<DuiControl> BuildNowPlayingPage(int trackIdx)
{
    if (trackIdx < 0 || trackIdx >= kLatestTrackCount)
    {
        trackIdx = 0;
    }

    auto page = std::make_unique<PageHost>();
    page->SetGap(0);
    page->SetPadding(0, 0, 0, 0);

    // 主区：左 disc + 右 lyrics 50/50
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(0);
    row->AddChild(MakeLeftBlock(trackIdx), DuiLayout::Hint().Weight(1));
    row->AddChild(MakeRightBlock(),         DuiLayout::Hint().Weight(1));
    page->AddChild(std::move(row), DuiLayout::Hint().Weight(1));

    return page;
}

} // namespace cloudmelody
