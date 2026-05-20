#pragma once

#include "DuiControl.h"
#include <memory>

namespace cloudmelody {

// 最近播放页（参考 music_6）
std::unique_ptr<balloonwjui::DuiControl> BuildRecentPlayPage();

} // namespace cloudmelody
