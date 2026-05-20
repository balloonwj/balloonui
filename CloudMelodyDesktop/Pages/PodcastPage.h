#pragma once

#include "DuiControl.h"
#include <memory>

namespace cloudmelody {

// 播客页（参考 music_4）
std::unique_ptr<balloonwjui::DuiControl> BuildPodcastPage();

} // namespace cloudmelody
