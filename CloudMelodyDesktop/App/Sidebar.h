#pragma once

// =============================================================================
// Sidebar —— 方Music 左侧 220px 导航栏
//
// 视觉（参考 cloud_melody_desktop/.../music_2/screen.png）：
//   ┌──────────────────────┐
//   │ 方Music              │  品牌 + slogan（DuiLabel ×2）
//   │ Your Sound, Your Way │
//   │                      │
//   │ ▌发现                │  active：4px 红条 + 浅红 tint
//   │  播客                │  idle
//   │  音乐                │
//   │   我的音乐           │  section header（小灰）
//   │  收藏                │
//   │  最近播放            │
//   │  个人中心            │
//   │  …                   │  spacer（weight=1）
//   │ [ + 创建歌单 ]       │  PillButton（红）
//   │  设置                │
//   │  帮助                │
//   └──────────────────────┘
//
// 通知：每个 SidebarNavItem 携带 ctrl id（NavId 枚举）。点击发 DUIN_CLICK
//      给 host 父；MainFrame 在 OnDuiNotify_ 里按 wp 路由切页。
// =============================================================================

#include "DuiControl.h"
#include <memory>

namespace cloudmelody {

// 主导航 ctrl id —— MainFrame 路由用。100 起避免与 layout 占位 id 冲突。
enum NavId
{
    NavId_Discover    = 100,
    NavId_Podcast     = 101,
    NavId_Music       = 102,
    NavId_Favorites   = 103,
    NavId_RecentPlay  = 104,
    NavId_Profile     = 105,

    NavId_CreatePlaylist = 110,
    NavId_Settings       = 111,
    NavId_Help           = 112,

    // 不在 sidebar 主导航的"特殊页"id —— 由 PlayerBar 封面点击 / 其它
    // 入口触发。SetActiveNav 只处理 100..105，这里 120+ 不会影响 sidebar
    // active 视觉。
    NavId_NowPlaying     = 120,
};

// 工厂：构造完整 sidebar 子树。caller 把它 AddChild 到 MainFrame 的
// 客户区即可；自带 220 宽 + 浅灰底 + 内 padding。
std::unique_ptr<balloonwjui::DuiControl> BuildSidebar(int initialNav = NavId_Discover);

// 切换 active 项 —— MainFrame 在路由完页面切换后调一次，让对应 nav
// item 显示 active 态（左侧红条 + tint 底）。other items 自动取消激活。
//   sidebar：BuildSidebar 返回的根
//   navId：NavId_Discover .. NavId_Profile 之一（次主导航 + 我的音乐 6 项）
void SetActiveNav(balloonwjui::DuiControl* sidebar, int navId);

} // namespace cloudmelody
