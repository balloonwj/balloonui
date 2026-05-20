#include "stdafx.h"
#include "ProfilePage.h"
#include "../App/CloudMelodyTheme.h"
#include "../App/CloudMelodyFonts.h"
#include "../App/Mock/MockMusic.h"
#include "../Controls/PillButton.h"
#include "../Controls/IconButton.h"
#include "../Controls/MusicCard.h"
#include "../Controls/HoursBarChart.h"
#include "../Controls/PaintHelpers.h"

#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiAvatar.h"

#include "DuiResMgr.h"

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

// 「LEVEL 18」用户等级 chip —— 小号圆角胶囊 + 居中文字。不能用
// DuiBadge —— DuiBadge::SetText 内部硬截到 4 字符（设计给 "99+" 等短
// 数字 angular 标），把 "LEVEL 18" 截成 "LEVE"。这里自己画一个简单的
// chip：红 tint 底 + 主品牌红字，用 LabelXs 字号（11px）。
class LevelChip : public DuiControl
{
public:
    void SetText(LPCTSTR sz) { m_text = sz ? sz : _T(""); Invalidate(); }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }
        int h = m_rcItem.bottom - m_rcItem.top;
        // 圆角胶囊浅红底
        PaintHelpers::FillRoundedRect(hdc, m_rcItem,
                                      kColorNavActiveTint, h / 2);
        if (!m_text.IsEmpty())
        {
            HFONT hOld = (HFONT)::SelectObject(hdc,
                              Fonts::Get(Fonts::LabelXs));
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldC = ::SetTextColor(hdc, kColorPrimary);
            ::DrawText(hdc, (LPCTSTR)m_text, -1,
                       const_cast<RECT*>(&m_rcItem),
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            ::SetTextColor(hdc, oldC);
            ::SetBkMode(hdc, oldBk);
            ::SelectObject(hdc, hOld);
        }
    }

private:
    CString m_text;
};

// 「最爱风格」红色卡 —— 简单红底圆角矩形 + 两行文字
class GenreCard : public DuiVBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        PaintHelpers::FillRoundedRect(hdc, m_rcItem,
                                      kColorPrimary, kRadiusMd);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

// 顶部用户信息卡：头像 + 名字 + 关注/粉丝/听歌 + 编辑/分享 按钮
std::unique_ptr<DuiControl> MakeProfileHeader()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(20);
    row->SetPadding(0, 0, 0, 0);

    auto avatar = std::make_unique<DuiAvatar>();
    avatar->SetName(_T("张小方"));
    avatar->SetFallbackBgColor(kColorPrimary);
    avatar->SetShape(DuiAvatar::ShapeCircle);
    row->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(96));

    auto col = std::make_unique<DuiVBox>();
    col->SetGap(8);
    col->SetPadding(0, 8, 0, 0);

    {
        auto nameRow = std::make_unique<DuiHBox>();
        nameRow->SetGap(12);
        auto name = std::make_unique<DuiLabel>();
        name->SetText(_T("张小方"));
        name->SetTextColor(kColorOnSurface);
        name->SetFont(Fonts::Get(Fonts::HeadlineMd));
        nameRow->AddChild(std::move(name), DuiLayout::Hint().Fixed(100));

        auto chip = std::make_unique<LevelChip>();
        chip->SetText(_T("LEVEL 18"));
        // 高度对齐姓名行（28px），宽度自适应文字
        nameRow->AddChild(std::move(chip), DuiLayout::Hint().Fixed(72));

        nameRow->AddChild(std::make_unique<DuiControl>(),
                          DuiLayout::Hint().Weight(1));
        col->AddChild(std::move(nameRow), DuiLayout::Hint().Fixed(28));
    }

    // 三栏数字
    {
        auto stats = std::make_unique<DuiHBox>();
        stats->SetGap(28);
        auto add = [&](LPCTSTR num, LPCTSTR lbl)
        {
            auto v = std::make_unique<DuiVBox>();
            auto n = std::make_unique<DuiLabel>();
            n->SetText(num);
            n->SetTextColor(kColorOnSurface);
            v->AddChild(std::move(n), DuiLayout::Hint().Fixed(22));
            auto t = std::make_unique<DuiLabel>();
            t->SetText(lbl);
            t->SetTextColor(kColorOnSurfaceVar);
            v->AddChild(std::move(t), DuiLayout::Hint().Fixed(16));
            stats->AddChild(std::move(v), DuiLayout::Hint().Fixed(80));
        };
        add(_T("356"),  _T("关注"));
        add(_T("142"),  _T("粉丝"));
        add(_T("1.2w"), _T("听歌"));
        stats->AddChild(std::make_unique<DuiControl>(),
                        DuiLayout::Hint().Weight(1));
        col->AddChild(std::move(stats), DuiLayout::Hint().Fixed(40));
    }

    row->AddChild(std::move(col), DuiLayout::Hint().Weight(1));

    // 「编辑资料」是次级 —— 设计稿是灰色描边
    auto edit = std::make_unique<PillButton>();
    edit->SetText(_T("编辑资料"));
    edit->SetStyle(PillButton::StyleSecondary);
    row->AddChild(std::move(edit), DuiLayout::Hint().Fixed(108));

    auto share = std::make_unique<IconButton>();
    share->SetGlyph(_T("⤴"));
    share->SetHoverShape(IconButton::ShapeCircle);
    row->AddChild(std::move(share), DuiLayout::Hint().Fixed(36));

    return row;
}

// 「最爱风格」红卡 + 「听歌时段」柱图 一行两块
std::unique_ptr<DuiControl> MakeAnalysisRow()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(20);

    // 左：红色风格卡
    auto genre = std::make_unique<GenreCard>();
    genre->SetGap(8);
    genre->SetPadding(20, 18, 20, 18);

    auto subtle = std::make_unique<DuiLabel>();
    subtle->SetText(_T("最爱风格"));
    subtle->SetTextColor(RGB(255, 230, 230));
    genre->AddChild(std::move(subtle), DuiLayout::Hint().Fixed(18));

    auto big = std::make_unique<DuiLabel>();
    big->SetText(_T("独立流行 & 爵士"));
    big->SetTextColor(kColorOnPrimary);
    big->SetFont(Fonts::Get(Fonts::HeadlineMd));
    genre->AddChild(std::move(big), DuiLayout::Hint().Fixed(32));

    auto hint = std::make_unique<DuiLabel>();
    hint->SetText(_T("基于本月 142 首歌的听歌习惯分析"));
    hint->SetTextColor(RGB(255, 230, 230));
    genre->AddChild(std::move(hint), DuiLayout::Hint().Fixed(20));

    genre->AddChild(std::make_unique<DuiControl>(),
                    DuiLayout::Hint().Weight(1));
    row->AddChild(std::move(genre), DuiLayout::Hint().Fixed(280));

    // 右：柱图 + 标题
    auto right = std::make_unique<DuiVBox>();
    right->SetGap(12);

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("听歌时段分析"));
    title->SetTextColor(kColorOnSurface);
    title->SetFont(Fonts::Get(Fonts::TitleSm));
    right->AddChild(std::move(title), DuiLayout::Hint().Fixed(24));

    auto chart = std::make_unique<HoursBarChart>();
    right->AddChild(std::move(chart), DuiLayout::Hint().Weight(1));

    auto hintR = std::make_unique<DuiLabel>();
    hintR->SetText(_T("最近 7 天 / 总时长 22:00 - 01:00 时段最活跃"));
    hintR->SetTextColor(kColorOnSurfaceVar);
    right->AddChild(std::move(hintR), DuiLayout::Hint().Fixed(18));

    row->AddChild(std::move(right), DuiLayout::Hint().Weight(1));

    return row;
}

// 「我的歌单」卡片网格 + 末尾"+ 创建歌单"占位
class CreatePlaylistCard : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }
        // 灰虚线边框近似：用单色实线 + 浅灰底（GDI 没原生虚线 region），
        // 简化为单色 1px 边框 + 浅灰底。
        HBRUSH hbr = ::CreateSolidBrush(kColorSurfaceContainerLow);
        ::FillRect(hdc, &m_rcItem, hbr);
        ::DeleteObject(hbr);
        // 中央 "+"
        HFONT hOld = (HFONT)::SelectObject(hdc,
                        (HFONT)::GetStockObject(DEFAULT_GUI_FONT));
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldC = ::SetTextColor(hdc, kColorOnSurfaceVar);
        ::DrawText(hdc, _T("+ 创建歌单"), -1,
                   const_cast<RECT*>(&m_rcItem),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldC);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOld);
    }
};

std::unique_ptr<DuiControl> MakeMyPlaylistsRow()
{
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(16);
    int n = (kFeaturedPlaylistCount > 4) ? 4 : kFeaturedPlaylistCount;
    for (int i = 0; i < n; ++i)
    {
        auto card = std::make_unique<MusicCard>();
        card->SetTitle(kFeaturedPlaylists[i].title);
        card->SetSubtitle(kFeaturedPlaylists[i].subtitle);
        card->SetHueSeed(i + 3);
        row->AddChild(std::move(card), DuiLayout::Hint().Fixed(140));
    }
    auto plus = std::make_unique<CreatePlaylistCard>();
    row->AddChild(std::move(plus), DuiLayout::Hint().Fixed(140));
    row->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));
    return row;
}

} // anonymous

std::unique_ptr<DuiControl> BuildProfilePage()
{
    auto page = std::make_unique<PageHost>();
    page->SetGap(28);
    page->SetPadding(kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin,
                     kSizeContainerMargin);

    page->AddChild(MakeProfileHeader(), DuiLayout::Hint().Fixed(96));
    page->AddChild(MakeAnalysisRow(),   DuiLayout::Hint().Fixed(160));

    // 「我的歌单」section
    auto playlistsCol = std::make_unique<DuiVBox>();
    playlistsCol->SetGap(16);

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("我的歌单"));
    title->SetTextColor(kColorOnSurface);
    title->SetFont(Fonts::Get(Fonts::HeadlineMd));
    playlistsCol->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    playlistsCol->AddChild(MakeMyPlaylistsRow(),
                           DuiLayout::Hint().Fixed(140 + 44));
    page->AddChild(std::move(playlistsCol),
                   DuiLayout::Hint().Weight(1));

    return page;
}

} // namespace cloudmelody
