#pragma once

// =============================================================================
// MockChat —— 单聊气泡 mock 数据（Phase 4）。
//
// Phase 3 的 MockData 是会话<u>列表</u>的 mock；本文件是<u>单条会话内消息流</u>
// 的 mock。两边数据相互独立 —— Phase 4 的演示只关心右栏聊天区，没去把
// session 跟 message 关联起来。后续阶段需要按会话切聊天内容时再扩 API
// （如 `MessagesForSession(sessionIdx)` 之类）。
// =============================================================================

#include <vector>
#include <atlstr.h>

namespace xchat {

// 单条聊天 item 的种类。每种 kind 在 ChatMessageList 走不同的 paint 路径
// 和高度计算。Phase 4 阶段实现这 5 类；Phase 5+ 加群聊 system 提示 / 图
// 片缩略图等再扩枚举。
enum ChatItemKind
{
    Kind_Text          = 0,    // 文字气泡
    Kind_Image         = 1,    // 图片缩略
    Kind_TransferCard  = 2,    // 某信转账卡（橙底白字）
    Kind_KnowledgeCard = 3,    // 知识星球邀请卡（白底 + footer 标签条）
    Kind_DateDivider   = 4,    // 居中日期分隔线（如 "4月29日 09:18"）
    Kind_File          = 5,    // 文件卡（左侧彩色 ext icon + 文件名 + 大小）
};

// Kind_Image 用：要画的"假照片"种类。每个 kind 走一段独立的 GDI+
// 程序化绘制（食物 / 风景 / 操场 / ...），不依赖外部 PNG 资源。
// ImgNone 表示走老占位（灰矩形 + 文本）的兼容路径。
enum ImageKind
{
    Img_None     = 0,
    Img_Hotpot,         // 火锅照（暖色背景 + 食材小圆）
    Img_Landscape,      // 晨曦风景（渐变天 + 太阳 + 山）
    Img_Sports,         // 操场（跑道椭圆环 + 草坪）
    Img_Uniform,        // 班服（衣服剪影）
    Img_QrCode,         // 二维码（黑白 modules 网格）
    Img_GroupPhoto,     // 出片预览（多人剪影）
    Img_Chart,          // 压测图表（网格 + 折线）
    Img_Receipt,        // 账单截图（白底 + 行）
};

// 一条消息的展示数据。不同 kind 用到的字段不一样 —— 跟 Session 一样平铺
// 在一个 struct 里没用到的字段为空字符串。这是 demo 阶段的简化做法；真
// 业务里会按 kind 单独 union / variant。
struct ChatMessage
{
    ChatItemKind kind;
    bool         fromSelf;        // true = 我发出 → 右侧；false = 对方发出 → 左侧
    TCHAR        avatarLetter;    // 头像 fallback 单字
    COLORREF     avatarBg;        // 头像背景色

    // ---- 不同 kind 用到的内容字段 ----
    CString      text;            // Text / DateDivider 用：要显示的字符串
    CString      title;           // KnowledgeCard 用：邀请标题
    CString      subtitle;        // KnowledgeCard 用：副标题（如 "张小方创建"）
    CString      tagText;         // KnowledgeCard footer 标签文本（如 "知识星球"）
    CString      amount;          // TransferCard 用：金额（如 "¥1500.00"）
    CString      statusText;      // TransferCard 用：状态（如 "已被接收" / "已收款"）
    ImageKind    imageKind = Img_None;   // Kind_Image 用：选哪种程序化"假图"
    CString      fileName;        // Kind_File 用：完整文件名（含后缀）
    CString      fileSizeText;    // Kind_File 用：人类可读大小（"1.2 MB"）
    CString      fileExt;         // Kind_File 用：小写后缀（"docx"/"pdf"/...）决定 icon 颜色
};

// 返回 sessionIdx 对应的对话 mock 消息数组。Publics 类 session（公众
// 号 / 订阅号 / 某信支付 / CSDN 学院 / 安全助手）返回空 vector ——
// publics 不走"对话"流程，主面板里点它会切到公众号文章 view。
// 越界 idx 也返回空。返回引用进程内有效。
const std::vector<ChatMessage>& MessagesForSession(int sessionIdx);

// 当前打开会话的标题（用于 chat header 显示）。LoadMainXml 时给一个
// 默认值；ApplySessionView(idx) 切会话后用 Sessions()[idx].name 直接
// 设到 chat-title label，不再走这条 API。
LPCTSTR CurrentChatTitle();

} // namespace xchat
