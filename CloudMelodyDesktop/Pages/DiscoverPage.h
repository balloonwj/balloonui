#pragma once

// =============================================================================
// DiscoverPage —— 「发现」页（参考 cloud_melody_desktop/.../music_2）
//
// 内容（顶 → 底）：
//   1. Banner ("华语流行：城市之声" 红底 + 立即播放 / 了解更多)
//   2. Section "精选歌单"  + "查看全部"链接，5 张 MusicCard
//   3. Section "最新音乐"  + "更多"链接，简表（M3 第一版 6 行）
//
// 数据：全部从 App/Mock/MockMusic.h 取（写死的常量数组）。
//
// 通知：内部 MusicCard / PillButton 点击都 bubble 到 host（MainFrame），
//      暂不做路由（M5 起接 NowPlayingPage / 详情页跳转）。
// =============================================================================

#include "DuiControl.h"
#include <memory>

namespace cloudmelody {

std::unique_ptr<balloonwjui::DuiControl> BuildDiscoverPage();

} // namespace cloudmelody
