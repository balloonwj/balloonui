#pragma once

// =============================================================================
// MockMusic.h —— 假数据：专辑 / 歌单 / 单曲 / 播客 / 排行榜
//
// 整个 demo 没有真实音频引擎 / 后端 API；所有"曲目""卡片"都来自这里。
// 数据是写死的常量数组（指针引用，无堆分配 / 无 RAII），让 page 直接
// for-loop 渲染。
//
// 接真后端时（如果有那一步）：保留本头文件接口形状，把内容换成 fetch 后
// 的 vector；DiscoverPage / FavoritesPage 等的 build 逻辑无需变。
// =============================================================================

namespace cloudmelody {

struct MockAlbum
{
    LPCTSTR title;        // "华语民谣宝藏"
    LPCTSTR subtitle;     // "今夜我想静静地听"
};

struct MockTrack
{
    LPCTSTR title;        // "晴天"
    LPCTSTR artist;       // "周杰伦"
    LPCTSTR album;        // "叶惠美"
    LPCTSTR duration;     // "04:29"（display 用）
    int     durationSec;  // 269（playback timer 用，避免每 tick 解析 mm:ss）
};

// "精选歌单" 5 张
extern const MockAlbum  kFeaturedPlaylists[];
extern const int        kFeaturedPlaylistCount;

// "最新音乐" 6 首
extern const MockTrack  kLatestTracks[];
extern const int        kLatestTrackCount;

// NowPlayingPage 用的歌词（写死 8 行通用）——所有曲目共用此 mock，
// M7 polish 时按 trackIdx 切独立 lyrics 集。
extern const LPCTSTR    kMockLyrics[];
extern const int        kMockLyricsCount;
// 当前"高亮行"索引（0-based）。M6 不接 timer 同步，固定第 3 行。
extern const int        kMockCurrentLyricIdx;

} // namespace cloudmelody
