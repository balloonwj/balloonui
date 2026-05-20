#include "stdafx.h"
#include "ContentRouter.h"
#include "Sidebar.h"             // NavId_*
#include "CloudMelodyTheme.h"

#include "../Pages/DiscoverPage.h"
#include "../Pages/LocalMusicPage.h"
#include "../Pages/FavoritesPage.h"
#include "../Pages/NowPlayingPage.h"
#include "../Pages/PodcastPage.h"
#include "../Pages/RecentPlayPage.h"
#include "../Pages/ProfilePage.h"
#include "PlayerBar.h"

#include "Controls/Basic/DuiLabel.h"

using namespace balloonwjui;

namespace cloudmelody {

namespace {

// 「即将推出」占位 —— 还没实现的 nav 项（播客 / 最近播放 / 个人中心 /
// 搜索结果）切到这里。一行居中灰字 + 白底。
class PlaceholderPage : public DuiVBox
{
public:
    explicit PlaceholderPage(LPCTSTR text) : m_text(text ? text : _T("")) {}

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        HBRUSH hbr = ::CreateSolidBrush(kColorSurface);
        ::FillRect(hdc, &m_rcItem, hbr);
        ::DeleteObject(hbr);
        DuiVBox::OnPaint(hdc, rcDirty);
    }

private:
    CString m_text;
    // child 已通过 ctor 加。
};

std::unique_ptr<DuiControl> BuildPlaceholder(LPCTSTR text)
{
    auto box = std::make_unique<PlaceholderPage>(text);
    box->SetGap(0);
    box->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));

    auto lbl = std::make_unique<DuiLabel>();
    lbl->SetText(text);
    lbl->SetTextColor(kColorOnSurfaceVar);
    box->AddChild(std::move(lbl), DuiLayout::Hint().Fixed(28));

    box->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return box;
}

std::unique_ptr<DuiControl> BuildPageFor(int navId, int extra)
{
    switch (navId)
    {
    case NavId_Discover:
        return BuildDiscoverPage();
    case NavId_Music:
        return BuildLocalMusicPage();
    case NavId_Favorites:
        return BuildFavoritesPage();
    case NavId_NowPlaying:
        return BuildNowPlayingPage(extra);   // extra = trackIdx
    case NavId_Podcast:
        return BuildPodcastPage();
    case NavId_RecentPlay:
        return BuildRecentPlayPage();
    case NavId_Profile:
        return BuildProfilePage();
    default:
        return BuildPlaceholder(_T("未知页面"));
    }
}

} // anonymous

void ContentRouter::Switch(int navId, int extra)
{
    // 同 navId 短路 —— 但 NowPlaying 不短路，每次切回都要重建以反映
    // 当前 trackIdx（曲目可能变了）。
    if (navId == m_currentNav && navId != NavId_NowPlaying)
    {
        return;
    }

    // 1) 移除当前 page 子树（如有）
    if (m_currentPage)
    {
        RemoveChild(m_currentPage);
        m_currentPage = nullptr;
    }

    // 2) 构建并挂上目标 page，weight=1 占满整个 router 区
    auto page = BuildPageFor(navId, extra);
    m_currentPage = page.get();
    AddChild(std::move(page), DuiLayout::Hint().Weight(1));

    m_currentNav = navId;

    // 3) 重新布局 —— 子树结构变了，SetRect 会因 EqualRect 早 return；
    //    必须 ForceLayout 强制重排。
    ForceLayout(GetRect());
    Invalidate();
}

} // namespace cloudmelody
