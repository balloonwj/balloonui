#pragma once

// =============================================================================
// NowPlayingPage —— 「正在播放」覆盖页（参考 music_7）
//
// 触发：用户点击 PlayerBar 左侧封面 → MainFrame 路由到 ContentRouter，
//      切到 NavId_NowPlaying。返回按钮回到上一次 sidebar 主导航页。
//
// 内容：
//   ┌─────────────┬────────────────────────┐
//   │   ← 返回     │                        │
//   │             │   歌词 / 视频 (Tab)     │
//   │  ◯ 旋转黑胶 │                        │
//   │   + 封面    │   多行歌词              │
//   │             │   （当前行红色加大）    │
//   │  歌名       │                        │
//   │  歌手       │                        │
//   │  ♡ 收藏     │                        │
//   └─────────────┴────────────────────────┘
//
// 旋转：内嵌的 RotatingDisc 由 host 60Hz timer 驱动 Tick(deltaMs)。
//      RotatingDisc.SetSpinning(playing) 跟着 PlayerBar 的 m_playing 同步。
// =============================================================================

#include "DuiControl.h"
#include <memory>

namespace cloudmelody {

// 导航 id：>= NavId_*，避开 sidebar / topbar / playerbar 的 id 区间。
//   sidebar nav   100..112
//   topbar        200..204
//   playerbar     300..312
//   nowplay       400..409
enum NowPlayingId
{
    NowPlayId_Disc       = 400,    // RotatingDisc 自身（timer 找它用）
    NowPlayId_Back       = 401,    // 「← 返回」按钮
    NowPlayId_LikeBig    = 402,    // 大爱心
    NowPlayId_FavBig     = 403,    // 大收藏
};

std::unique_ptr<balloonwjui::DuiControl> BuildNowPlayingPage(int trackIdx);

} // namespace cloudmelody
