#pragma once

// =============================================================================
// CloudMelodyTheme.h —— 方Music 视觉令牌（design token）集中处
//
// 来源：third_party/cloud_melody_desktop/stitch_cloud_melody_desktop/music/DESIGN.md
//      （Hanken Grotesk + Material 调色板，主色「中国红」）
//
// 使用约定：
//   · 所有颜色、字号、间距、圆角"常量"<u>必须</u>从这里取，不要在业务文件
//     里硬写 RGB(xxx,xxx,xxx) / 数字 px。修改主题改这里一处。
//   · XML（main.xml 等）里的 color/fixedWidth/fixedHeight 数值需与本文件
//     保持一致；XML 不能 #include 本头，靠人肉对照 —— 改值时注意同步。
//   · 字号 / 圆角 / 间距按 DESIGN.md 的 4px 网格走（`spacing.unit = 4px`）。
//
// 命名约定（以下三档前缀按"用途"区分）：
//   kColor*      —— RGB 调色板
//   kSize*       —— 像素 / 间距 / 圆角等几何量
//   kFont*       —— 字体面 / 字号 / 字重
// =============================================================================

namespace cloudmelody {

// ─── Brand / Surface 调色板 ───────────────────────────────────────────────

// Chinese Red 主品牌色（DESIGN.md 里 primary 是 #b61621；卡片视觉上偏向
// 更亮的 #EC4141 —— 实截里的"播放"红圆按钮、爱心高亮、active 指示条都
// 用这个鲜红）。两挡都留：primary 用在大块面积（按钮底 / banner 装饰），
// accent 用在点缀（active bar / 喜欢心形）。
static const COLORREF kColorPrimary       = RGB(0xB6, 0x16, 0x21); // #B61621
static const COLORREF kColorPrimaryHover  = RGB(0xDA, 0x34, 0x36); // #DA3436 (primary-container)
static const COLORREF kColorPrimaryAccent = RGB(0xEC, 0x41, 0x41); // #EC4141 实截鲜红
static const COLORREF kColorOnPrimary     = RGB(0xFF, 0xFF, 0xFF); // 白字

// Surface 体系（content / 容器 / 浮层 三档底色）
static const COLORREF kColorSurface              = RGB(0xFF, 0xFF, 0xFF); // 主内容区
static const COLORREF kColorSurfaceContainerLow  = RGB(0xF3, 0xF3, 0xF4); // sidebar / topbar
static const COLORREF kColorSurfaceContainer     = RGB(0xEE, 0xEE, 0xEE); // hover 浅灰底
static const COLORREF kColorSurfaceContainerHigh = RGB(0xE8, 0xE8, 0xE8); // pressed 深一档
static const COLORREF kColorSurfaceDim           = RGB(0xDA, 0xDA, 0xDA); // 边线 / inactive

// PlayerBar 玻璃感近似底（高亮白 + 1px 顶分隔线）。第二版上 Acrylic 时
// 这里改成 alpha 通道；第一版纯色。
static const COLORREF kColorPlayerBarBg    = RGB(0xFC, 0xFC, 0xFD);
static const COLORREF kColorPlayerBarTop   = RGB(0xE4, 0xE4, 0xE6); // 1px 顶分隔线

// 文字色三档（高对比 / 中 / 低）
static const COLORREF kColorOnSurface     = RGB(0x1A, 0x1C, 0x1C); // primary text
static const COLORREF kColorOnSurfaceVar  = RGB(0x5F, 0x5E, 0x5E); // metadata / 副文
static const COLORREF kColorOutline       = RGB(0x8F, 0x6F, 0x6D); // outline
static const COLORREF kColorOutlineVar    = RGB(0xE4, 0xBE, 0xBA); // outline-variant

// SidebarNavItem 状态色：active 项左侧 4px 红条 + 浅红 tint 底
static const COLORREF kColorNavActiveBar  = kColorPrimary;
static const COLORREF kColorNavActiveTint = RGB(0xFF, 0xEB, 0xEC); // primary 5% 等效
static const COLORREF kColorNavHoverTint  = RGB(0xF0, 0xF0, 0xF2); // 中性灰 hover

// ─── 几何 / 间距 ─────────────────────────────────────────────────────────

// 4px 网格（`spacing.unit`）—— 间距走 4 的倍数。stack-gap=12 / gutter=20 /
// section-gap=40 / container-margin=32 都来自 DESIGN.md。
static const int kSizeUnit            = 4;
static const int kSizeStackGap        = 12;  // 行内紧凑组件间距
static const int kSizeGutter          = 20;  // 12 列网格中列间距
static const int kSizeContainerMargin = 32;  // 主内容区四周外边距
static const int kSizeSectionGap      = 40;  // 「精选歌单」「最新音乐」等段落分隔

// 主框架四区固定尺寸
static const int kSizeTopBarHeight    = 48;
static const int kSizeSidebarWidth    = 220;
static const int kSizeSidebarPadding  = 12;
static const int kSizePlayerBarHeight = 80;

// 圆角
static const int kRadiusSm    = 4;     // 列表 hover 选中
static const int kRadiusMd    = 8;     // 卡片 / 封面（默认）
static const int kRadiusLg    = 12;    // banner
static const int kRadiusPill  = 18;    // pill 按钮 / 搜索框（接近 full）
static const int kRadiusFull  = 9999;  // 圆形

// 控件常用尺寸
static const int kSizeAvatarSm = 28;   // PlayerBar 用户头像
static const int kSizeAvatarMd = 38;   // TopBar 用户头像
static const int kSizeAvatarLg = 96;   // ProfilePage 主头像（120 设计稿；这里取 96 留余）
static const int kSizeIconBtn  = 32;   // TopBar / PlayerBar icon 按钮宽高
static const int kSizeCardSm   = 140;
static const int kSizeCardMd   = 160;
static const int kSizeCardLg   = 180;

// 进度条 track 高度（窄 → hover 加粗）
static const int kSizeProgressTrackIdle  = 2;
static const int kSizeProgressTrackHover = 4;

// ─── 字体 ────────────────────────────────────────────────────────────────
//
// 全应用单一字体：**Microsoft YaHei**（系统自带，中英数字一致渲染）。
// 早期 DESIGN.md 提到 Hanken Grotesk 用于拉丁/数字 —— 已废弃，因为终端
// 用户机器多半没装该字体，且 balloonui 当前没有 mixed-script font fallback，
// 走单一 face 反而视觉一致。CloudMelodyFonts 6 档全部用本 face 名。
//
// balloonui 的 DuiResMgr::GetDefaultFont() 也是 YaHei 9pt（库内置），所以
// "没显式 SetFont 的 label / Win32 EDIT" 也都走 YaHei。

static LPCTSTR const kFontFaceCJK = _T("Microsoft YaHei");

// 字号（DESIGN.md 排版梯度）。CreateFont 用 -px（负号 = 不经 mapping
// 直接用像素高度）。
static const int kFontSizeDisplayLg  = 30;  // 大标题（页头大字）
static const int kFontSizeHeadlineMd = 22;  // section / 用户名 / playlist 标题
static const int kFontSizeTitleSm    = 16;  // section 副标题
static const int kFontSizeBodyMd     = 14;  // 正文 / 列表行
static const int kFontSizeBodySm     = 13;  // 副文 / 元数据
static const int kFontSizeLabelXs    = 11;  // chip / slogan

// 字重（GDI LOGFONT.lfWeight）
static const int kFontWeightRegular  = 400;
static const int kFontWeightMedium   = 500;
static const int kFontWeightSemibold = 600;
static const int kFontWeightBold     = 700;

} // namespace cloudmelody
