#include "stdafx.h"
#include "MockChat.h"

namespace xchat {

// =============================================================================
// Mock 对话数据 —— 按 session idx (与 MockData.cpp Sessions() 顺序一致)
// 索引返回该会话的消息流。Publics 类 session 返回空 vector，主面板里
// 点它会切到公众号文章 view 而非对话窗。
//
// 总共 20 个 session：8 single + 8 group + 4 publics（具体见 MockData.cpp
// 注释）。每段对话设计成贴合该联系人/群"身份"——HR 谈 offer、家长群
// 接龙、技术群贴代码截图、妈妈视频通话等。
//
// 复用 ChatMessageList 已支持的 5 种 kind：Text / Image / TransferCard /
// KnowledgeCard / DateDivider。不引入新 kind。
// =============================================================================

namespace {

// "我" 自己 = Balloonwj（与 LoginFrame avatar 一致）。所有 self 消息共用
// 这个头像（绿底 + B）。
static const COLORREF kSelfBg     = RGB(  7, 193,  96);
static const TCHAR    kSelfLetter = _T('B');

// 一组品牌色循环用做 peer 头像 fallback bg。和 SessionListItem 头像调色
// 板部分对齐，让"会话列表里那个橙底的人"在对话窗里也是橙底。
static const COLORREF kCBlue   = RGB( 51, 121, 200);
static const COLORREF kCGreen  = RGB(  7, 193,  96);
static const COLORREF kCOrange = RGB(245, 158,  10);
static const COLORREF kCPurple = RGB(168,  85, 247);
static const COLORREF kCCyan   = RGB(  6, 182, 212);
static const COLORREF kCMagent = RGB(217,  70, 239);
static const COLORREF kCDark   = RGB( 22, 163,  74);
static const COLORREF kCGray   = RGB(101, 113, 130);

// ---- 构造单条消息的小 helper ----------------------------------------------

ChatMessage Self(LPCTSTR text)
{
    ChatMessage m{};
    m.kind         = Kind_Text;
    m.fromSelf     = true;
    m.avatarLetter = kSelfLetter;
    m.avatarBg     = kSelfBg;
    m.text         = text;
    return m;
}

ChatMessage Peer(TCHAR letter, COLORREF bg, LPCTSTR text)
{
    ChatMessage m{};
    m.kind         = Kind_Text;
    m.fromSelf     = false;
    m.avatarLetter = letter;
    m.avatarBg     = bg;
    m.text         = text;
    return m;
}

ChatMessage Divider(LPCTSTR text)
{
    ChatMessage m{};
    m.kind = Kind_DateDivider;
    m.text = text;
    return m;
}

ChatMessage SelfImage(ImageKind kind, LPCTSTR caption = _T("[图片]"))
{
    ChatMessage m{};
    m.kind         = Kind_Image;
    m.fromSelf     = true;
    m.avatarLetter = kSelfLetter;
    m.avatarBg     = kSelfBg;
    m.text         = caption;
    m.imageKind    = kind;
    return m;
}

ChatMessage PeerImage(TCHAR letter, COLORREF bg, ImageKind kind, LPCTSTR caption = _T("[图片]"))
{
    ChatMessage m{};
    m.kind         = Kind_Image;
    m.fromSelf     = false;
    m.avatarLetter = letter;
    m.avatarBg     = bg;
    m.text         = caption;
    m.imageKind    = kind;
    return m;
}

ChatMessage SelfTransfer(LPCTSTR amount, LPCTSTR status)
{
    ChatMessage m{};
    m.kind         = Kind_TransferCard;
    m.fromSelf     = true;
    m.avatarLetter = kSelfLetter;
    m.avatarBg     = kSelfBg;
    m.amount       = amount;
    m.statusText   = status;
    return m;
}

ChatMessage PeerTransfer(TCHAR letter, COLORREF bg, LPCTSTR amount, LPCTSTR status)
{
    ChatMessage m{};
    m.kind         = Kind_TransferCard;
    m.fromSelf     = false;
    m.avatarLetter = letter;
    m.avatarBg     = bg;
    m.amount       = amount;
    m.statusText   = status;
    return m;
}

ChatMessage SelfFile(LPCTSTR fileName, LPCTSTR sizeText, LPCTSTR ext)
{
    ChatMessage m{};
    m.kind         = Kind_File;
    m.fromSelf     = true;
    m.avatarLetter = kSelfLetter;
    m.avatarBg     = kSelfBg;
    m.fileName     = fileName;
    m.fileSizeText = sizeText;
    m.fileExt      = ext;
    return m;
}

ChatMessage PeerFile(TCHAR letter, COLORREF bg,
                     LPCTSTR fileName, LPCTSTR sizeText, LPCTSTR ext)
{
    ChatMessage m{};
    m.kind         = Kind_File;
    m.fromSelf     = false;
    m.avatarLetter = letter;
    m.avatarBg     = bg;
    m.fileName     = fileName;
    m.fileSizeText = sizeText;
    m.fileExt      = ext;
    return m;
}

ChatMessage PeerKnowledgeCard(TCHAR letter, COLORREF bg,
                              LPCTSTR title, LPCTSTR subtitle, LPCTSTR tag)
{
    ChatMessage m{};
    m.kind         = Kind_KnowledgeCard;
    m.fromSelf     = false;
    m.avatarLetter = letter;
    m.avatarBg     = bg;
    m.title        = title;
    m.subtitle     = subtitle;
    m.tagText      = tag;
    return m;
}

// ---- 各 session 的对话生成 ------------------------------------------------
//
// 每个 BuildXxx 返回一个 vector<ChatMessage>。索引顺序必须与 MockData.cpp
// 的 Sessions() 严格对齐 —— 改动 MockData session 顺序时，这里也要同步。

std::vector<ChatMessage> Build_FileTransfer()      // [0] 文件传输助手
{
    // 文件传输助手是<u>跨设备</u>传文件场景。这里 self avatar = 当前 PC
    // 端（Balloonwj 绿色 B），peer avatar = 同账号的"手机端"（灰色"机"
    // 字头像），两端的对话记录在同一个 session 里 —— 模拟"我在 PC 上
    // 发文件给自己手机"和"我在手机上推文件到 PC"双向流。
    static const TCHAR    kPhoneLetter = _T('机');
    static const COLORREF kPhoneBg     = kCGray;

    return {
        Divider(_T("昨天 21:30")),
        Self(_T("https://github.com/balloonwj/XChat —— 项目 readme")),
        SelfFile(_T("XChat-PRD-v3.docx"),  _T("428.5 KB"),   _T("docx")),
        Self(_T("提醒：周三 14:00 客户演示，带笔记本")),
        SelfImage(Img_QrCode, _T("[图片] 二维码截图")),

        Divider(_T("今天 08:42")),
        // 手机端推过来：客户发的合同 + 现场照片
        Peer(kPhoneLetter, kPhoneBg, _T("从手机发送：")),
        PeerFile(kPhoneLetter, kPhoneBg, _T("客户XX-技术服务合同-v2.pdf"), _T("1.8 MB"),  _T("pdf")),
        PeerFile(kPhoneLetter, kPhoneBg, _T("现场拍摄-架构图.png"),         _T("3.2 MB"),  _T("png")),
        PeerFile(kPhoneLetter, kPhoneBg, _T("会议录音 5-8.m4a"),            _T("12.4 MB"), _T("m4a")),

        Divider(_T("今天 09:12")),
        // PC 端整理后发回手机端
        Self(_T("会议纪要整理好了，发回你手机：")),
        SelfFile(_T("会议纪要 5-8.md"),                _T("18.3 KB"),  _T("md")),
        SelfFile(_T("Q2-进度同步-final.xlsx"),         _T("87.6 KB"),  _T("xlsx")),
        SelfFile(_T("演示用 deck-final.pptx"),          _T("4.6 MB"),  _T("pptx")),
        SelfFile(_T("交付物压缩包.zip"),                _T("21.7 MB"), _T("zip")),
        Self(_T("把这个发给老板：Q2 进度同步完成，无 blocker。")),
    };
}

std::vector<ChatMessage> Build_ZhengDaSan()        // [1] 正达南校三 (5)
{
    return {
        Divider(_T("昨天 09:30")),
        Peer(_T('郭'), kCOrange, _T("郭承鑫爸爸: #接龙#5月-接龙报名")),
        Peer(_T('李'), kCBlue,   _T("李一萌妈妈: 1. 李一萌")),
        Peer(_T('王'), kCPurple, _T("王梓豪爸爸: 2. 王梓豪")),
        Peer(_T('刘'), kCCyan,   _T("刘子涵妈妈: 3. 刘子涵")),
        Self(_T("4. 范鑫")),
        Peer(_T('陈'), kCGreen,  _T("陈一鸣妈妈: 5. 陈一鸣")),
        Peer(_T('郭'), kCOrange, _T("郭承鑫爸爸: 截止周五 18:00 哦，请家长按格式接龙，谢谢配合 🙏")),
    };
}

std::vector<ChatMessage> Build_HighPerfDev()       // [2] 高性能服务器开发
{
    return {
        Divider(_T("04/30 14:02")),
        Peer(_T('海'), kCBlue,  _T("汉口学院 18机械成海涛: 老师好，请教一下 epoll 的 LT 和 ET 在压测下"
                                   "性能差距大概有多少？")),
        Peer(_T('张'), kCGreen, _T("张老师: 取决于你的业务模式。短连接多用 ET 配合 EPOLLONESHOT，"
                                   "长连接 LT 也够。我之前一组测试 ET 比 LT 大概快 8-12%。")),
        Peer(_T('海'), kCBlue,  _T("汉口学院 18机械成海涛: 收到，那我先用 ET 试一下。")),
        PeerImage(_T('涛'), kCOrange, Img_Chart, _T("[图片] 压测结果对比图")),
        Self(_T("ET 模式下记得每次 read 要循环到 EAGAIN，不然会丢事件。")),
        Peer(_T('海'), kCBlue,  _T("汉口学院 18机械成海涛: 好的，已经按这个写了。再次感谢 🙏")),
    };
}

std::vector<ChatMessage> Build_HighPerfReaders()   // [3] 高性能服务器读者群
{
    return {
        Divider(_T("04/30 11:18")),
        Peer(_T('陈'), kCPurple, _T("北京-陈志刚: 请教一个问题，muduo 这种 reactor 模型在 Windows "
                                    "下 IOCP 该怎么对应？")),
        Peer(_T('张'), kCGreen,  _T("张老师: IOCP 是 proactor 不是 reactor，模型不一样，我下午写一篇对比帖。")),
        Peer(_T('吴'), kCOrange, _T("\"北京-java-吴佳远\" 撤回了一条消息")),
        Peer(_T('陈'), kCPurple, _T("北京-陈志刚: 哈哈哈刚刚那条问题我也在想 :D")),
        Peer(_T('张'), kCGreen,  _T("张老师: 已发：")),
        PeerKnowledgeCard(_T('张'), kCGreen,
            _T("Reactor vs Proactor：从 epoll 到 IOCP 的工程视角"),
            _T("张小方 · 7 分钟阅读"),
            _T("公众号文章")),
        Self(_T("收藏了 👍")),
    };
}

std::vector<ChatMessage> Build_FanXin()            // [5] 范鑫（加长，可滚动）
{
    return {
        Divider(_T("04/27 22:10")),
        Peer(_T('范'), kCDark,  _T("最近忙啥呢？好久没聚了")),
        Self(_T("加班加得头昏，618 大促前要上一波新功能")),
        Peer(_T('范'), kCDark,  _T("理解理解，互联网卷王 :D")),
        Peer(_T('范'), kCDark,  _T("有空一起吃个饭，发泄一下")),
        Self(_T("好啊，明后天都行")),
        Divider(_T("04/28 12:40")),
        Peer(_T('范'), kCDark,  _T("中午吃饭吗，新开了家潮汕牛肉火锅")),
        Self(_T("好啊，几点？")),
        Peer(_T('范'), kCDark,  _T("12:30 老地方等你")),
        Self(_T("[OK]")),
        Divider(_T("04/28 13:50")),
        Peer(_T('范'), kCDark,  _T("这家牛肉真不错，下次还来")),
        PeerImage(_T('范'), kCDark, Img_Hotpot, _T("[图片] 火锅照")),
        Self(_T("看着真馋人，刚刚没吃够")),
        Peer(_T('范'), kCDark,  _T("下次直接两份哈哈")),
        Self(_T("加某信群带我下次组局 🤝")),
        Peer(_T('范'), kCDark,  _T("已拉，群名：大碗喝酒大块吃肉团")),
        Peer(_T('范'), kCDark,  _T("对了，618 你有买什么计划？")),
        Self(_T("想换台显示器，4K 27 寸的，预算 2500 以内")),
        Peer(_T('范'), kCDark,  _T("我刚入了一台 LG 27UP850，体验贼好，色域 DCI-P3 95%")),
        Self(_T("正想问你，链接发我 🙏")),
        Peer(_T('范'), kCDark,  _T("https://item.jd.com/100012345678.html  京东自营，活动价 2399")),
        Self(_T("感谢，已加购物车 ✅")),
        Peer(_T('范'), kCDark,  _T("不客气，下个月再约个饭！")),
    };
}

std::vector<ChatMessage> Build_TodoList()          // [6] AAA待办事项
{
    return {
        Divider(_T("04/28 08:00")),
        Self(_T("□ 9:00 站会")),
        Self(_T("□ 11:00 跟产品过 v3 文档")),
        Self(_T("□ 14:00 客户 demo 演练")),
        Self(_T("□ 17:00 review PR #482")),
        Self(_T("□ 晚上：办健身卡 / 取快递")),
        Divider(_T("04/28 22:10")),
        Self(_T("✅ 已完成 4/5；办健身卡明天再说。")),
    };
}

std::vector<ChatMessage> Build_ZhengDaEr()         // [8] 正达南校二 (5) 班
{
    return {
        Divider(_T("13:30")),
        Peer(_T('班'), kCOrange, _T("班主任王老师: 各位家长下午好，明天春季运动会请按通知准备：")),
        Peer(_T('班'), kCOrange, _T("班主任王老师: ① 红色班服 ② 运动鞋 ③ 自带水壶；上午 8:30 校门口集合。")),
        PeerImage(_T('班'), kCOrange, Img_Sports, _T("[图片] 项目安排表")),
        Divider(_T("13:38")),
        Peer(_T('范'), kCBlue,   _T("范一林爸爸20号: [图片]")),
        PeerImage(_T('范'), kCBlue, Img_Uniform, _T("[图片] 班服已到")),
        Peer(_T('范'), kCBlue,   _T("范一林爸爸20号: 收到！")),
    };
}

std::vector<ChatMessage> Build_ZhangWei()          // [9] 张伟（加长，可滚动）
{
    return {
        Divider(_T("11:45")),
        Peer(_T('张'), kCBlue,  _T("早上提到的那个 redis 缓存问题，我顺手扒了一下源码")),
        Peer(_T('张'), kCBlue,  _T("发现是 LRU eviction 在大 key 场景下抖得比较厉害")),
        Self(_T("嗯，我们 hot key 也有这毛病。打算切到 LFU 试试")),
        Peer(_T('张'), kCBlue,  _T("LFU 的话别忘了 lfu-decay-time 调小一点，否则尾分布跟不上")),
        Self(_T("好，我加到下次发版里")),
        Divider(_T("12:10")),
        Peer(_T('张'), kCBlue,  _T("方便聊吗？关于明天的需求评审")),
        Self(_T("说")),
        Peer(_T('张'), kCBlue,  _T("我把上午原型图拆成 3 个 user story 了，等会发你")),
        Peer(_T('张'), kCBlue,  _T("评审之前你看下，避免现场卡壳")),
        Self(_T("OK，发我邮箱也抄一份给 PM")),
        Peer(_T('张'), kCBlue,  _T("已发 ✓")),
        Divider(_T("12:18")),
        Self(_T("收到，看了再 ping 你")),
        Peer(_T('张'), kCBlue,  _T("好的，明天见")),
        Divider(_T("17:50")),
        Self(_T("看完了，总体没问题。第二个 story 的边界条件再 review 下：")),
        Self(_T("如果用户取消订单到一半，定金退回的时序得跟支付那边对齐")),
        Peer(_T('张'), kCBlue,  _T("👍 我标个评审待办，明天先讨论这条")),
        Self(_T("ok，那先这样，回头见")),
    };
}

std::vector<ChatMessage> Build_ProjectMeeting()    // [10] 项目周会群
{
    return {
        Divider(_T("11:30")),
        Peer(_T('P'), kCOrange, _T("PM李: 各位下周三的需求评审改到周四 14:00，会议室不变。")),
        Peer(_T('P'), kCOrange, _T("PM李: 客户 demo 因为出差延后一天，影响最小。")),
        Peer(_T('赵'), kCBlue,  _T("Tech-赵: 收到")),
        Peer(_T('钱'), kCPurple,_T("UI-钱: 我那边设计稿周四上午能交终稿")),
        Peer(_T('孙'), kCCyan,  _T("QA-孙: 测试用例已起草，评审完一起对一下")),
        Self(_T("收到 +1，周四见")),
        Peer(_T('P'), kCOrange, _T("PM李: 谢谢配合 🙏")),
    };
}

std::vector<ChatMessage> Build_Mom()               // [11] 妈妈（加长，可滚动）
{
    return {
        Divider(_T("04/26 周六 10:00")),
        Peer(_T('妈'), kCMagent, _T("儿子，五一回家不")),
        Self(_T("回，5/1 早上的高铁，5/4 晚上回")),
        Peer(_T('妈'), kCMagent, _T("好嘞，妈给你包饺子，韭菜还是白菜的？")),
        Self(_T("都要 :D")),
        Peer(_T('妈'), kCMagent, _T("行，那就一样包一半")),
        Peer(_T('妈'), kCMagent, _T("对了，你王阿姨家那个姑娘要不要见一下？")),
        Self(_T("妈，我现在没时间想这个，工作太忙")),
        Peer(_T('妈'), kCMagent, _T("光忙工作，对象的事也得上心，30 岁不小了")),
        Self(_T("……知道了，回家再说")),
        Divider(_T("昨天 19:30")),
        Peer(_T('妈'), kCMagent, _T("吃饭了没")),
        Self(_T("吃过啦，今天加班晚一点")),
        Peer(_T('妈'), kCMagent, _T("别太累，按时吃饭")),
        Peer(_T('妈'), kCMagent, _T("天气转凉了，加件外套")),
        Self(_T("好嘞 ❤️")),
        Divider(_T("10:25")),
        Peer(_T('妈'), kCMagent, _T("[视频聊天]")),
        Peer(_T('妈'), kCMagent, _T("视频没接，方便了回")),
        Self(_T("妈我刚开会，下午回你")),
        Peer(_T('妈'), kCMagent, _T("好的，不急")),
    };
}

std::vector<ChatMessage> Build_LinuxKernel()       // [14] Linux 内核源码群
{
    return {
        Divider(_T("昨天 22:05")),
        Peer(_T('博'), kCDark,  _T("陈博士: 5.18 的 io_uring 这块代码改动比较大，"
                                   "想问下大家：SQPOLL 模式下 cqe 的 user_data 字段有人遇到过对不上吗？")),
        Peer(_T('王'), kCBlue,  _T("王工: 我之前遇到过，是 CQE 已 reaped 但 SQPOLL 线程还在写入旧 sqe"
                                   "导致看起来错位 —— 加个 io_uring_cqe_seen 之后就对了。")),
        Peer(_T('博'), kCDark,  _T("陈博士: 嗯我也想到这点，但我已经及时 seen 了 :( 还是错")),
        Peer(_T('刘'), kCOrange,_T("刘工: 你贴下你的 setup flags 看看")),
        Peer(_T('博'), kCDark,  _T("陈博士: IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF | "
                                   "IORING_SETUP_CQSIZE，cq size 设的 4096")),
        Peer(_T('王'), kCBlue,  _T("王工: 是不是 SQ_AFF 把 sqpoll 线程绑死了核但你又有用户态多线程在写？"
                                   "并发竞争 SQ ring 你的 fence 加了吗？")),
        Peer(_T('博'), kCDark,  _T("陈博士: 没加 fence，直接 store。我去补一下试试，回头同步结果。")),
    };
}

std::vector<ChatMessage> Build_WangFang()          // [15] 王芳
{
    return {
        Divider(_T("昨天 16:20")),
        Self(_T("芳姐，合同我盖完章扫描发你邮箱了")),
        Peer(_T('王'), kCOrange, _T("收到，谢谢")),
    };
}

std::vector<ChatMessage> Build_HrZhao()            // [16] HR-赵敏（加长，可滚动）
{
    return {
        Divider(_T("上周五 17:30")),
        Peer(_T('赵'), kCPurple, _T("您好，我是 XX 公司技术 HR 赵敏，方便简单沟通下吗？")),
        Self(_T("您好，方便")),
        Peer(_T('赵'), kCPurple, _T("看了您的简历，想约一下我们后端架构岗的初面，本周内有时间吗？")),
        Self(_T("可以，建议周三下午 3 点")),
        Peer(_T('赵'), kCPurple, _T("好的，已约好。会议链接稍后邮件发您 📧")),
        Divider(_T("周三 17:00")),
        Peer(_T('赵'), kCPurple, _T("初面反馈非常正面，约个二面 + 终面连场行不？")),
        Self(_T("OK，时间您定，避开周一上午就行")),
        Peer(_T('赵'), kCPurple, _T("已约：本周五 14:00-17:00 三轮连面，地点同上")),
        Self(_T("收到 🙏")),
        Divider(_T("星期一 10:00")),
        Peer(_T('赵'), kCPurple, _T("您好，您的终面已通过 ✅")),
        Self(_T("好的，谢谢通知")),
        Peer(_T('赵'), kCPurple, _T("offer 已发，请查收邮箱")),
        Peer(_T('赵'), kCPurple, _T("base 35K × 16，年终绩效 2-4 个月。期权 2000 股，4 年成熟。")),
        Peer(_T('赵'), kCPurple, _T("入职日期建议 5/15 或 5/20，您看哪个合适？")),
        Peer(_T('赵'), kCPurple, _T("有任何问题可以随时联系我，期待您的加入 🌟")),
        Self(_T("收到，我看完今晚回复您")),
        Self(_T("能不能争取下 base 涨到 38K？我现司 stock 还有一年才行权，机会成本算下来差不少")),
        Peer(_T('赵'), kCPurple, _T("我跟用人部门同步下，今晚或明早回您结果")),
    };
}

std::vector<ChatMessage> Build_GymGroup()          // [17] 健身房群
{
    return {
        Divider(_T("星期一 18:00")),
        Peer(_T('教'), kCDark,  _T("教练Tony: 周末团课调整通知 ↓")),
        Peer(_T('教'), kCDark,  _T("教练Tony: 周六 09:00 HIIT / 10:30 普拉提 / 14:00 椭圆机训练营")),
        Peer(_T('教'), kCDark,  _T("教练Tony: 周日 10:00 力量 / 16:00 拉伸放松")),
        Peer(_T('A'), kCBlue,   _T("Alex: 周六 9 点 +1")),
        Peer(_T('M'), kCOrange, _T("Mia: 周日力量 +1")),
        Self(_T("周六 HIIT +1")),
        Peer(_T('教'), kCDark,  _T("教练Tony: 收到 👍 不见不散")),
    };
}

std::vector<ChatMessage> Build_PhotoGroup()        // [19] 摄影爱好者群
{
    return {
        Divider(_T("04/26 19:00")),
        Peer(_T('T'), kCBlue,   _T("Tom: 周末出行约拍人数统计，目的地：奥森公园北门")),
        Peer(_T('T'), kCBlue,   _T("Tom: 周日 7:30 集合，带相机/三脚架，主题：晨曦人像 + 街拍")),
        Peer(_T('A'), kCOrange, _T("Amy: +1")),
        Peer(_T('K'), kCPurple, _T("Ken: +1")),
        Peer(_T('L'), kCCyan,   _T("Leo: +1")),
        Peer(_T('J'), kCMagent, _T("Jane: +1")),
        Peer(_T('R'), kCDark,   _T("Ryan: +1")),
        Peer(_T('S'), kCBlue,   _T("Sara: +1，可以帮忙带反光板")),
        Self(_T("+1")),
        Peer(_T('T'), kCBlue,   _T("Tom: 已 7 人，再来 1 个就开车出发，欢迎报名")),
        PeerImage(_T('T'), kCBlue, Img_GroupPhoto, _T("[图片] 上次出片预览")),
        Peer(_T('A'), kCOrange, _T("Amy: 上次的人像出片真好看 📷")),
    };
}

// =============================================================================
// 公众号 / 服务号风格的"对话流"。某信里这两类账号也是聊天窗 UI，每条
// 消息一般是平台推送的卡片或文本通知，用户基本只看不回。
// =============================================================================

std::vector<ChatMessage> Build_CsdnAcademy()       // [13] CSDN 学院
{
    return {
        Divider(_T("昨天 09:00")),
        PeerKnowledgeCard(_T('C'), kCMagent,
            _T("深入理解 STL：从 unordered_map 的实现看哈希冲突"),
            _T("CSDN 学院 · 推荐阅读"),
            _T("公众号文章")),
        Divider(_T("昨天 11:30")),
        PeerKnowledgeCard(_T('C'), kCMagent,
            _T("Linux 进程调度：CFS 算法 10 分钟读懂"),
            _T("CSDN 学院 · 内核基础"),
            _T("公众号文章")),
        Divider(_T("昨天 16:45")),
        Peer(_T('C'), kCMagent,
            _T("📢 5 月新课上线：《C++20 协程从入门到上手》—— 限时 5 折，券码 CXX2024")),
        Divider(_T("今天 08:30")),
        PeerKnowledgeCard(_T('C'), kCMagent,
            _T("MySQL 索引下推：为什么 explain 里看到 Using index condition？"),
            _T("CSDN 学院 · 性能优化"),
            _T("公众号文章")),
        Peer(_T('C'), kCMagent,
            _T("🎁 老学员专属：完成本周打卡可得「面试题手册」电子版")),
    };
}

std::vector<ChatMessage> Build_WechatPay()         // [18] 某信支付
{
    return {
        Divider(_T("04/27 08:15")),
        Peer(_T('支'), kCGreen,
            _T("【支付通知】您于 04/27 08:15 在 美团 消费 ¥38.50，支付方式：零钱。")),
        Divider(_T("04/27 12:32")),
        Peer(_T('支'), kCGreen,
            _T("【支付通知】您于 04/27 12:32 在 全家便利 消费 ¥21.00，支付方式：建行储蓄卡(8847)。")),
        Divider(_T("04/27 18:45")),
        Peer(_T('支'), kCGreen,
            _T("【退款到账】京东商城 退款 ¥199.00 已原路返还至建行储蓄卡(8847)。")),
        PeerImage(_T('支'), kCGreen, Img_Receipt, _T("[图片] 退款回执")),
        Divider(_T("04/28 09:00")),
        Peer(_T('支'), kCGreen,
            _T("【账单提醒】您本月信用卡待还 ¥3,521.20，最后还款日 5/8。点此前往还款 ›")),
        Divider(_T("04/28 14:20")),
        Peer(_T('支'), kCGreen,
            _T("【收益到账】零钱通昨日收益 ¥0.42，七日年化 1.81%。")),
        Divider(_T("04/29 11:08")),
        Peer(_T('支'), kCGreen,
            _T("【支付通知】您于 04/29 11:08 在 滴滴出行 消费 ¥18.30，已使用 ¥5 优惠券。")),
        Peer(_T('支'), kCGreen,
            _T("【账户安全提醒】您的某信支付已开通指纹验证，未经本人操作请立即修改密码 →")),
    };
}

// ---- 全表 lazy build ------------------------------------------------------
//
// 按 MockData.cpp Sessions() 顺序索引；publics 槽位 = 空 vector。
const std::vector<std::vector<ChatMessage>>& AllSessionMessages()
{
    static std::vector<std::vector<ChatMessage>> kAll = []()
    {
        std::vector<std::vector<ChatMessage>> v;
        v.reserve(20);
        v.push_back(Build_FileTransfer());      // [0]  文件传输助手
        v.push_back(Build_ZhengDaSan());        // [1]  正达南校三 (5)
        v.push_back(Build_HighPerfDev());       // [2]  高性能服务器开发
        v.push_back(Build_HighPerfReaders());   // [3]  高性能服务器读者群
        v.push_back({});                        // [4]  公众平台安全助手 (publics)
        v.push_back(Build_FanXin());            // [5]  范鑫
        v.push_back(Build_TodoList());          // [6]  AAA待办事项
        v.push_back({});                        // [7]  公众号 (publics)
        v.push_back(Build_ZhengDaEr());         // [8]  正达南校二 (5) 班
        v.push_back(Build_ZhangWei());          // [9]  张伟
        v.push_back(Build_ProjectMeeting());    // [10] 项目周会群
        v.push_back(Build_Mom());               // [11] 妈妈
        v.push_back({});                        // [12] 订阅号助手 (publics)
        v.push_back(Build_CsdnAcademy());       // [13] CSDN 学院 (单聊形式：文章 push)
        v.push_back(Build_LinuxKernel());       // [14] Linux 内核源码群
        v.push_back(Build_WangFang());          // [15] 王芳
        v.push_back(Build_HrZhao());            // [16] HR-赵敏
        v.push_back(Build_GymGroup());          // [17] 健身房群
        v.push_back(Build_WechatPay());         // [18] 某信支付 (单聊形式：账单流水)
        v.push_back(Build_PhotoGroup());        // [19] 摄影爱好者群
        return v;
    }();
    return kAll;
}

} // anonymous namespace

const std::vector<ChatMessage>& MessagesForSession(int sessionIdx)
{
    static const std::vector<ChatMessage> kEmpty;
    const auto& all = AllSessionMessages();
    if (sessionIdx < 0 || sessionIdx >= (int)all.size())
    {
        return kEmpty;
    }
    return all[sessionIdx];
}

LPCTSTR CurrentChatTitle()
{
    // 启动时 LoadMainXml 默认调一次，给个友好的初始 title。真正切会话
    // 后由 ApplySessionView 直接用 Sessions()[idx].name 设到 chat-title。
    return _T("文件传输助手");
}

} // namespace xchat
