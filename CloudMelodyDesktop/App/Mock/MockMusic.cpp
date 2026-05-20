#include "stdafx.h"
#include "MockMusic.h"

namespace cloudmelody {

const MockAlbum kFeaturedPlaylists[] = {
    { _T("华语民谣宝藏"), _T("今夜我想静静地听") },
    { _T("华语 R&B 流行"), _T("节奏与蓝调的城市夜")  },
    { _T("经典老歌金选"),  _T("重温整张华语黑胶")    },
    { _T("独立流派精选"),  _T("不在主流排行榜里")    },
    { _T("自习室专属白噪"), _T("书桌旁的低分贝陪伴") }
};
const int kFeaturedPlaylistCount =
    sizeof(kFeaturedPlaylists) / sizeof(kFeaturedPlaylists[0]);

const MockTrack kLatestTracks[] = {
    { _T("晴天"),         _T("周杰伦"),  _T("叶惠美"),       _T("04:29"), 4 * 60 + 29 },
    { _T("十年"),         _T("陈奕迅"),  _T("黑·白·灰"),    _T("03:22"), 3 * 60 + 22 },
    { _T("光年之外"),     _T("邓紫棋"),  _T("摩天动物园"),   _T("03:55"), 3 * 60 + 55 },
    { _T("青花瓷"),       _T("周杰伦"),  _T("我很忙"),       _T("03:59"), 3 * 60 + 59 },
    { _T("修炼爱情"),     _T("林俊杰"),  _T("因你而在"),     _T("04:47"), 4 * 60 + 47 },
    { _T("小幸运"),       _T("田馥甄"),  _T("我的少女时代"), _T("04:25"), 4 * 60 + 25 }
};
const int kLatestTrackCount =
    sizeof(kLatestTracks) / sizeof(kLatestTracks[0]);

const LPCTSTR kMockLyrics[] = {
    _T("从出生那年就吵闹著"),
    _T("童年的回忆挥不去"),
    _T("唱不完一首细水长流"),
    _T("随记忆一直晃到现在"),
    _T("Re So So Si Do Si La"),
    _T("So La Si Si Si La Si La So"),
    _T("微微岛屿婴啼夺去"),
    _T("我的心扎根在你那里"),
};
const int kMockLyricsCount =
    sizeof(kMockLyrics) / sizeof(kMockLyrics[0]);

// 当前高亮行 = 第 3 行（0-based） —— 把"随记忆一直晃到现在"显成红色加大。
const int kMockCurrentLyricIdx = 3;

} // namespace cloudmelody
