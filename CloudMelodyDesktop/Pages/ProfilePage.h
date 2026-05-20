#pragma once

#include "DuiControl.h"
#include <memory>

namespace cloudmelody {

// 个人中心页（参考 music_5）
std::unique_ptr<balloonwjui::DuiControl> BuildProfilePage();

} // namespace cloudmelody
