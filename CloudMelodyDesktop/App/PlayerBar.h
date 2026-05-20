#pragma once

// =============================================================================
// PlayerBar —— 方Music 底部 80px 播放栏（常驻）
//
// 视觉布局见 .cpp 顶部注释。本头只暴露：
//
//   class PlayerBar
//     · Tick(deltaMs)        —— host 定时器调用，按 deltaMs 推进 m_posSec
//     · OnPlayClicked()      —— host 路由 PlayerBarId_Play 的 DUIN_CLICK 调
//     · OnSeek(int value)    —— host 路由 PlayerBarId_Progress 的
//                               DUIN_VALUECHANGED 调（进度条拖动改值）
//     · LoadTrack(int idx)   —— 切到 MockMusic 的第 idx 首曲目
//
// 注意：是<u>类</u>+<u>builder 函数</u>组合 —— builder 返 PlayerBar 实例
// （以 unique_ptr<DuiControl> 形式给 XML factory 用）。host 通过
// FindCtrlById(40) 拿到根，dynamic_cast 成 PlayerBar* 即可调上述方法。
// =============================================================================

#include "Controls/Layout/DuiLayout.h"
#include <memory>

namespace balloonwjui {
    class DuiLabel;
}

namespace cloudmelody {

class PlayCircleButton;
class PlayerProgressBar;
class CoverPlaceholder;
class IconButton;

// 播放模式 —— 用户点 "随机播放" 按钮在这 4 种模式间循环切。glyph 跟着变。
enum PlayMode
{
    PlayMode_Sequential = 0,    // 顺序播放
    PlayMode_Shuffle    = 1,    // 随机播放
    PlayMode_RepeatAll  = 2,    // 列表循环
    PlayMode_RepeatOne  = 3,    // 单曲循环
    PlayMode_Count      = 4     // 哨兵
};

// PlayerBar 内 ctrl id（300 起）—— 给外部 host 路由用。
enum PlayerBarId
{
    PlayerBarId_Cover     = 300,    // 封面（点击 → NowPlayingPage）
    PlayerBarId_Like      = 301,    // ♡ 喜欢
    PlayerBarId_Shuffle   = 302,
    PlayerBarId_Prev      = 303,
    PlayerBarId_Play      = 304,
    PlayerBarId_Next      = 305,
    PlayerBarId_Repeat    = 306,
    PlayerBarId_Progress  = 307,
    PlayerBarId_Volume    = 308,
    PlayerBarId_Lyrics    = 309,
    PlayerBarId_Queue     = 310,
    PlayerBarId_Fullscreen = 311,
    PlayerBarId_DeskLyrics = 312,
};

class PlayerBar : public balloonwjui::DuiHBox
{
public:
    PlayerBar();

    // 切到 MockMusic.kLatestTracks[idx]。idx 越界自动 wrap。
    // 切到新曲目后 m_posSec 重置为 0；m_playing 不变（如原本在播则继续）。
    void LoadTrack(int idx);

    // ---- 由 host 周期调用 ----
    // host 的 WM_TIMER 在 m_posSec < m_durationSec 时把 deltaMs 传进来；
    // 内部仅当 m_playing 时累加 m_posMs。到达终点 → 自动停止 + Refresh。
    void Tick(int deltaMs);

    // ---- 由 host 通过 OnDuiNotify_ 路由的事件 ----
    void OnPlayClicked();           // 切换播放 / 暂停
    void OnPrevClicked();           // 上一曲
    void OnNextClicked();           // 下一曲
    void OnSeek(int value);         // 进度条拖动（0..1000 千分比）
    void OnShuffleClicked();        // 切换播放模式（4 模式循环）

    int  GetTrackIdx() const { return m_trackIdx; }
    PlayMode GetPlayMode() const { return m_playMode; }

    // 自有底色 + 顶部 1px 分隔线（layout 容器自身不画背景）
    void OnPaint(HDC hdc, const RECT& rcDirty) override;

private:
    void Refresh();                 // 把当前状态投到 UI 上
    void Build();                   // ctor 调；建子树 + 缓存子指针

    // 缓存的子指针（owned by parent layout's child vector，不要 delete）
    CoverPlaceholder*         m_cover       = nullptr;
    balloonwjui::DuiLabel*    m_titleLbl    = nullptr;
    balloonwjui::DuiLabel*    m_subLbl      = nullptr;
    balloonwjui::DuiLabel*    m_timeLeftLbl = nullptr;
    balloonwjui::DuiLabel*    m_timeRightLbl= nullptr;
    PlayCircleButton*         m_playBtn     = nullptr;
    PlayerProgressBar*        m_progress    = nullptr;
    IconButton*               m_shuffleBtn  = nullptr;

    // 状态
    int  m_trackIdx     = 1;        // MockMusic 的下标，1 = "十年"
    int  m_durationSec  = 0;        // 当前曲目总秒数（来自 mock）
    int  m_posMs        = 0;        // 当前位置（毫秒精度，避免 250ms 跳秒）
    bool m_playing      = false;
    PlayMode m_playMode = PlayMode_Sequential;
};

std::unique_ptr<balloonwjui::DuiControl> BuildPlayerBar();

} // namespace cloudmelody
