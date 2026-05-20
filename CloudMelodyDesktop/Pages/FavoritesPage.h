#pragma once

// =============================================================================
// FavoritesPage —— 「收藏」页（参考 music_3）
//
// 内容：
//   1. 大封面 + PLAYLIST tag + "我喜欢的音乐" 标题 + "张小方·138张专辑"
//      副标 + [▶ 播放全部] [⤴ 分享]
//   2. Section "收藏的歌单与艺人" + Tab "全部 / 歌单 / 艺人"
//   3. 网格 6 张艺人卡（复用 MusicCard）
//   4. 单曲列表
// =============================================================================

#include "DuiControl.h"
#include <memory>

namespace cloudmelody {

std::unique_ptr<balloonwjui::DuiControl> BuildFavoritesPage();

} // namespace cloudmelody
