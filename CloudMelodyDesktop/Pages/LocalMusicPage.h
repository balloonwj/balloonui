#pragma once

// =============================================================================
// LocalMusicPage —— 「音乐 / 本地音乐」页（参考 music_8）
//
// 内容：
//   1. 页头：「本地音乐」+「共 1,248 首歌曲」
//   2. CTA 行：[▶ 播放全部] + [✕ 随机播放]
//   3. Tab 行：全部歌曲 / 歌手 / 专辑 / 正在下载（DuiTab）
//   4. 单曲表：# / 标题 / 歌手 / 专辑 / 时长（M4 第一版用 label 模拟，未走
//      DuiTreeView；M7 polish 切到正式表格）
// =============================================================================

#include "DuiControl.h"
#include <memory>

namespace cloudmelody {

std::unique_ptr<balloonwjui::DuiControl> BuildLocalMusicPage();

} // namespace cloudmelody
