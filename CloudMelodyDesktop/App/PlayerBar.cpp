#include "stdafx.h"
#include "PlayerBar.h"
#include "CloudMelodyTheme.h"
#include "CloudMelodyFonts.h"
#include "Mock/MockMusic.h"
#include "../Controls/IconButton.h"
#include "../Controls/PlayCircleButton.h"
#include "../Controls/PlayerProgressBar.h"
#include "../Controls/CoverArt.h"

#include "Controls/Basic/DuiLabel.h"
#include "Controls/Input/DuiSlider.h"

using namespace balloonwjui;

namespace cloudmelody {

// CoverPlaceholder 提到 anonymous 外（namespace cloudmelody 直属）—— 让
// PlayerBar.h 里前向声明的 CoverPlaceholder* m_cover 能 link。
class CoverPlaceholder : public DuiControl
{
public:
    void SetTitle(LPCTSTR sz)   { m_title = sz ? sz : _T(""); Invalidate(); }
    void SetHueSeed(int s)      { m_seed = s; Invalidate(); }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }
        CoverArt::Paint(hdc, m_rcItem, nullptr, m_seed, kRadiusMd);
    }

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

private:
    CString m_title;
    int     m_seed = 0;
};

namespace {

static const int kLeftBlockWidth   = 280;
static const int kRightBlockWidth  = 260;
static const int kCoverSize        = 60;
static const int kCoverPadding     = 10;
static const int kPlayCircleSize   = 40;
// "00:00" 5 字符在 YaHei 9pt 下约需 38-42px。给 50 留余量避免裁剪。
static const int kTimeLabelWidth   = 50;
// 进度条精度：把曲长映射到 0..kProgressTicks。1000 = 千分比，秒级精度
// 对常见 3-5 分钟曲足够（每 tick 代表 ~0.2s）。
static const int kProgressTicks    = 1000;

std::unique_ptr<IconButton> MakeIcon(LPCTSTR glyph, int ctrlId,
                                     LPCTSTR tooltip = nullptr,
                                     IconButton::HoverShape shape =
                                         IconButton::ShapeSquare)
{
    auto b = std::make_unique<IconButton>();
    b->SetGlyph(glyph);
    b->SetCtrlId((UINT)ctrlId);
    b->SetHoverShape(shape);
    b->SetTooltip(tooltip);
    return b;
}

CString FormatTime(int sec)
{
    if (sec < 0) { sec = 0; }
    int mm = sec / 60;
    int ss = sec % 60;
    CString out;
    out.Format(_T("%02d:%02d"), mm, ss);
    return out;
}

} // anonymous

// =========================================================================
// PlayerBar 实现
// =========================================================================

PlayerBar::PlayerBar()
{
    SetGap(0);
    SetPadding(0, 0, 0, 0);
    Build();
    LoadTrack(m_trackIdx);  // 首次进入加载默认曲目（"十年"）
}

void PlayerBar::Build()
{
    // ─── 左块：封面 + 文字 + 喜欢 ─────────────────────────────────────
    auto leftBox = std::make_unique<DuiHBox>();
    leftBox->SetGap(12);
    leftBox->SetPadding(16, kCoverPadding, 0, kCoverPadding);

    auto cover = std::make_unique<CoverPlaceholder>();
    cover->SetCtrlId(PlayerBarId_Cover);
    m_cover = cover.get();
    leftBox->AddChild(std::move(cover),
                      DuiLayout::Hint().Fixed(kCoverSize));

    auto info = std::make_unique<DuiVBox>();
    info->SetGap(4);
    info->SetPadding(0, 8, 0, 8);

    {
        auto title = std::make_unique<DuiLabel>();
        title->SetTextColor(kColorOnSurface);
        title->SetFont(Fonts::Get(Fonts::TitleSm));
        m_titleLbl = title.get();
        info->AddChild(std::move(title), DuiLayout::Hint().Fixed(22));
    }
    {
        auto sub = std::make_unique<DuiLabel>();
        sub->SetTextColor(kColorOnSurfaceVar);
        sub->SetFont(Fonts::Get(Fonts::LabelXs));
        m_subLbl = sub.get();
        info->AddChild(std::move(sub), DuiLayout::Hint().Fixed(16));
    }

    leftBox->AddChild(std::move(info), DuiLayout::Hint().Weight(1));

    AddChild(std::move(leftBox), DuiLayout::Hint().Fixed(kLeftBlockWidth));

    // ─── 中块：transport + progress ───────────────────────────────────
    auto centerCol = std::make_unique<DuiVBox>();
    centerCol->SetGap(4);
    centerCol->SetPadding(8, 10, 8, 10);

    // transport row
    {
        auto row = std::make_unique<DuiHBox>();
        row->SetGap(6);
        row->AddChild(std::make_unique<DuiControl>(),
                      DuiLayout::Hint().Weight(1));
        // ♡ 喜欢按钮 —— M7-后续 从左 block 移到中 block，"随机播放"按钮左侧
        row->AddChild(MakeIcon(_T("♡"), PlayerBarId_Like, _T("喜欢"),
                               IconButton::ShapeCircle),
                      DuiLayout::Hint().Fixed(kSizeIconBtn));
        // 缓存"随机播放"按钮指针 —— OnShuffleClicked 切模式时改 glyph
        {
            auto shuf = std::make_unique<IconButton>();
            shuf->SetGlyph(_T("⇄"));
            shuf->SetCtrlId((UINT)PlayerBarId_Shuffle);
            shuf->SetTooltip(_T("随机播放"));
            m_shuffleBtn = shuf.get();
            row->AddChild(std::move(shuf),
                          DuiLayout::Hint().Fixed(kSizeIconBtn));
        }
        row->AddChild(MakeIcon(_T("⏮"), PlayerBarId_Prev, _T("上一首")),
                      DuiLayout::Hint().Fixed(kSizeIconBtn));

        auto play = std::make_unique<PlayCircleButton>();
        play->SetCtrlId(PlayerBarId_Play);
        m_playBtn = play.get();
        row->AddChild(std::move(play),
                      DuiLayout::Hint().Fixed(kPlayCircleSize));

        row->AddChild(MakeIcon(_T("⏭"), PlayerBarId_Next, _T("下一首")),
                      DuiLayout::Hint().Fixed(kSizeIconBtn));
        row->AddChild(MakeIcon(_T("⟳"), PlayerBarId_Repeat, _T("循环模式")),
                      DuiLayout::Hint().Fixed(kSizeIconBtn));
        row->AddChild(std::make_unique<DuiControl>(),
                      DuiLayout::Hint().Weight(1));
        centerCol->AddChild(std::move(row),
                            DuiLayout::Hint().Fixed(kPlayCircleSize));
    }

    // progress row
    {
        auto prow = std::make_unique<DuiHBox>();
        prow->SetGap(8);

        auto leftLbl = std::make_unique<DuiLabel>();
        leftLbl->SetTextColor(kColorOnSurfaceVar);
        m_timeLeftLbl = leftLbl.get();
        prow->AddChild(std::move(leftLbl),
                       DuiLayout::Hint().Fixed(kTimeLabelWidth));

        auto pb = std::make_unique<PlayerProgressBar>();
        pb->SetRange(0, kProgressTicks);
        pb->SetCtrlId(PlayerBarId_Progress);
        m_progress = pb.get();
        prow->AddChild(std::move(pb), DuiLayout::Hint().Weight(1));

        auto rightLbl = std::make_unique<DuiLabel>();
        rightLbl->SetTextColor(kColorOnSurfaceVar);
        m_timeRightLbl = rightLbl.get();
        prow->AddChild(std::move(rightLbl),
                       DuiLayout::Hint().Fixed(kTimeLabelWidth));

        centerCol->AddChild(std::move(prow), DuiLayout::Hint().Fixed(16));
    }

    AddChild(std::move(centerCol), DuiLayout::Hint().Weight(1));

    // ─── 右块：音量 + 歌词 / 队列 / 全屏 / 桌面歌词 ───────────────────
    auto rightBox = std::make_unique<DuiHBox>();
    rightBox->SetGap(4);
    rightBox->SetPadding(0, kCoverPadding, 16, kCoverPadding);

    rightBox->AddChild(MakeIcon(_T("🔊"), 0, _T("音量")),
                       DuiLayout::Hint().Fixed(kSizeIconBtn));
    auto vol = std::make_unique<DuiSlider>();
    vol->SetCtrlId(PlayerBarId_Volume);
    // 关闭 tab stop —— DuiSlider 默认 tab stop=true，OnPaint 在 m_bFocused
    // 时画 DrawFocusRect（虚线框）。音量滑竿是次要交互，不需要键盘焦点态，
    // 关掉避免点击边缘时虚线框乱跳。
    vol->SetTabStop(false);
    rightBox->AddChild(std::move(vol), DuiLayout::Hint().Fixed(80));
    rightBox->AddChild(MakeIcon(_T("词"), PlayerBarId_Lyrics, _T("歌词")),
                       DuiLayout::Hint().Fixed(kSizeIconBtn));
    rightBox->AddChild(MakeIcon(_T("≡"), PlayerBarId_Queue, _T("播放队列")),
                       DuiLayout::Hint().Fixed(kSizeIconBtn));
    rightBox->AddChild(MakeIcon(_T("⛶"), PlayerBarId_Fullscreen, _T("全屏")),
                       DuiLayout::Hint().Fixed(kSizeIconBtn));
    rightBox->AddChild(MakeIcon(_T("⊟"), PlayerBarId_DeskLyrics, _T("桌面歌词")),
                       DuiLayout::Hint().Fixed(kSizeIconBtn));

    AddChild(std::move(rightBox), DuiLayout::Hint().Fixed(kRightBlockWidth));
}

void PlayerBar::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible) { return; }
    // 1) 底色（玻璃感近似）
    HBRUSH hbr = ::CreateSolidBrush(kColorPlayerBarBg);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);
    // 2) 顶部 1px 分隔线
    RECT rcLine = m_rcItem;
    rcLine.bottom = rcLine.top + 1;
    HBRUSH hbrL = ::CreateSolidBrush(kColorPlayerBarTop);
    ::FillRect(hdc, &rcLine, hbrL);
    ::DeleteObject(hbrL);
    // 3) child paint
    DuiControl::OnPaint(hdc, rcDirty);
}

void PlayerBar::Refresh()
{
    if (m_cover)
    {
        m_cover->SetTitle(kLatestTracks[m_trackIdx].title);
        m_cover->SetHueSeed(m_trackIdx);
    }
    if (m_titleLbl)
    {
        m_titleLbl->SetText(kLatestTracks[m_trackIdx].title);
    }
    if (m_subLbl)
    {
        CString sub;
        sub.Format(_T("%s · %s"),
                   kLatestTracks[m_trackIdx].artist,
                   kLatestTracks[m_trackIdx].album);
        m_subLbl->SetText(sub);
    }
    if (m_timeLeftLbl)
    {
        m_timeLeftLbl->SetText(FormatTime(m_posMs / 1000));
    }
    if (m_timeRightLbl)
    {
        m_timeRightLbl->SetText(FormatTime(m_durationSec));
    }
    if (m_progress)
    {
        long long durMs = (long long)m_durationSec * 1000;
        int v = (durMs > 0)
            ? (int)((long long)m_posMs * kProgressTicks / durMs)
            : 0;
        m_progress->SetValue(v, false);
    }
    if (m_playBtn)
    {
        m_playBtn->SetPlaying(m_playing);
    }
}

void PlayerBar::LoadTrack(int idx)
{
    int n = kLatestTrackCount;
    if (n <= 0) { return; }
    // wrap 到 [0, n)
    int i = ((idx % n) + n) % n;
    m_trackIdx    = i;
    m_durationSec = kLatestTracks[i].durationSec;
    m_posMs       = 0;
    Refresh();
    Invalidate();
}

void PlayerBar::Tick(int deltaMs)
{
    if (!m_playing)
    {
        return;
    }
    if (m_durationSec <= 0)
    {
        return;
    }
    m_posMs += deltaMs;
    long long durMs = (long long)m_durationSec * 1000;
    if (m_posMs >= durMs)
    {
        // 播完 —— 暂停在末位（不自动续下一曲，简化）。后续真要 auto-advance
        // 在这里调 OnNextClicked()。
        m_posMs = (int)durMs;
        m_playing = false;
    }
    Refresh();
    Invalidate();
}

void PlayerBar::OnPlayClicked()
{
    // PlayCircleButton 自身的 OnLButtonUp 已经把 m_playing 切换 +
    // Invalidate（视觉立即响应），这里同步状态：
    // 1) 若到达末尾 + 用户重新点 ▶，从 0 重启
    // 2) 否则跟着 PlayCircleButton 的视觉切换
    long long durMs = (long long)m_durationSec * 1000;
    if (m_posMs >= durMs && m_playBtn && m_playBtn->IsPlaying())
    {
        m_posMs = 0;
    }
    m_playing = m_playBtn ? m_playBtn->IsPlaying() : !m_playing;
    Refresh();
}

void PlayerBar::OnPrevClicked()
{
    LoadTrack(m_trackIdx - 1);
}

void PlayerBar::OnNextClicked()
{
    LoadTrack(m_trackIdx + 1);
}

void PlayerBar::OnShuffleClicked()
{
    // 4 模式循环：顺序 → 随机 → 列表循环 → 单曲循环 → 顺序 → ...
    int next = ((int)m_playMode + 1) % (int)PlayMode_Count;
    m_playMode = (PlayMode)next;
    if (m_shuffleBtn)
    {
        // glyph + tooltip 跟着变 —— 让用户视觉上知道当前哪种模式
        switch (m_playMode)
        {
        case PlayMode_Sequential:
            m_shuffleBtn->SetGlyph(_T("⇉"));
            m_shuffleBtn->SetTooltip(_T("顺序播放"));
            break;
        case PlayMode_Shuffle:
            m_shuffleBtn->SetGlyph(_T("⇄"));
            m_shuffleBtn->SetTooltip(_T("随机播放"));
            break;
        case PlayMode_RepeatAll:
            m_shuffleBtn->SetGlyph(_T("⟳"));
            m_shuffleBtn->SetTooltip(_T("列表循环"));
            break;
        case PlayMode_RepeatOne:
            m_shuffleBtn->SetGlyph(_T("⥁"));
            m_shuffleBtn->SetTooltip(_T("单曲循环"));
            break;
        default:
            break;
        }
    }
}

void PlayerBar::OnSeek(int value)
{
    if (m_durationSec <= 0)
    {
        return;
    }
    if (value < 0) { value = 0; }
    if (value > kProgressTicks) { value = kProgressTicks; }
    long long durMs = (long long)m_durationSec * 1000;
    m_posMs = (int)((long long)value * durMs / kProgressTicks);
    // 拖动时只更新时间 label（progress 自身已被 caller 改值，再调
    // SetValue 会无谓重绘）
    if (m_timeLeftLbl)
    {
        m_timeLeftLbl->SetText(FormatTime(m_posMs / 1000));
    }
}

std::unique_ptr<DuiControl> BuildPlayerBar()
{
    return std::make_unique<PlayerBar>();
}

} // namespace cloudmelody
