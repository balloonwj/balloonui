#pragma once

// =============================================================================
// MockData —— XChat 的假数据源（Phase 3+）。
//
// 不连服务器、不读磁盘 —— 全是写死在 .cpp 里的 `static const std::vector`。
// 业务侧拿 `MockData::Sessions()` 一个 const 引用，永远拿到同一份数据。
// 后续阶段（4 单聊 / 5 群聊侧栏 / 6 联系人 / 7 公众号）按需在这里加新结
// 构 + getter；不引入运行时增删改 —— demo 不需要。
// =============================================================================

#include <vector>
#include <atlstr.h>

namespace xchat {

// 点击会话后，右栏切到哪种 view。Phase 8 之前是 CLI arg 切换的；点击交
// 互上线后由本 enum 驱动。
enum SessionPageType
{
    Page_Single  = 0,    // 单聊：chat 区，无右侧信息栏
    Page_Group   = 1,    // 群聊：chat 区 + 270px 信息栏
    Page_Publics = 2,    // 公众号 / 订阅号：右栏换成"常看的号 + 文章 list"
};

// 群成员小卡数据（Page_Group session 才有）。
struct GroupMember
{
    TCHAR    avatarLetter;
    COLORREF avatarBg;
    CString  name;
};

// 一条会话的展示数据。Phase 3 的 SessionListItem 一行吃一个 Session。
struct Session
{
    CString         name;          // 会话名（用户昵称 / 群名 / "文件传输助手"）
    CString         preview;       // 上次消息摘要（已包含发送者前缀如 "Alice: ..."）
    CString         timeText;      // 显示用时间："13:57" / "昨天 09:56" / "04/30" / "星期二"
    TCHAR           avatarLetter;  // 头像 fallback 单字母
    COLORREF        avatarBg;      // 头像背景色
    bool            isGroup;       // 群会话 vs 单聊
    bool            muted;         // "免打扰"
    int             unreadCount;   // > 0 显示红 badge；与 muted 互斥
    SessionPageType pageType;      // 点击时切到哪种右栏 view
    std::vector<GroupMember> members;  // 仅 Page_Group session 有意义；其它为空
};

// 返回 20 条 mock 会话；引用在进程生命周期内有效。
const std::vector<Session>& Sessions();

} // namespace xchat
