#include "stdafx.h"
#include "MockData.h"
#include "MockChat.h"

namespace xchat {

// 头像背景色调色板。色相分散一些，明度都偏中（白字在上面读得清）。前两个
// 是某信品牌绿 + 公众平台蓝，跟参考截图里"文件传输助手 / 公众号"对得上。
static const COLORREF kAvatarPalette[] = {
    RGB(  7, 193,  96),    // 0  某信绿
    RGB( 51,  90, 196),    // 1  公众平台蓝（"公众号"那条）
    RGB(245, 158,  10),    // 2  橘
    RGB(217,  70, 239),    // 3  品红
    RGB(  6, 182, 212),    // 4  青
    RGB(168,  85, 247),    // 5  紫
    RGB(220,  38,  38),    // 6  红
    RGB(101, 163,  13),    // 7  橄榄
    RGB( 22, 163,  74),    // 8  深绿
    RGB(120, 113, 108),    // 9  暖灰（多用作群头像 fallback）
};

// 群成员 helper：构造 4 字段 GroupMember 字面量，让 push 数据 inline 显得清晰。
static GroupMember M(TCHAR letter, COLORREF bg, LPCTSTR name)
{
    GroupMember m{};
    m.avatarLetter = letter;
    m.avatarBg     = bg;
    m.name         = name;
    return m;
}

const std::vector<Session>& Sessions()
{
    // 进程内一次性构造；返回 const&，业务永远拿同一份指针。函数局部
    // static 在 C++11 起线程安全（Magic Statics），demo 单线程也无虞。
    static const std::vector<Session> kAll = []() {
        std::vector<Session> v;
        v.reserve(21);
        // 字段顺序：name, preview, timeText, avatarLetter, avatarBg, isGroup, muted, unread, pageType, members
        // ── 截图里出现过的几条（前 9 条对照 third_party/wechat/主面板.png） ──
        v.push_back({_T("文件传输助手"),    _T("[视频号] 姜幺郎的动态"),                _T("星期二"),     _T('文'), kAvatarPalette[0], false, false, 0,  Page_Single, {} });
        v.push_back({_T("正达南校三 (5)"),   _T("郭承鑫爸爸: #接龙#5月-接龙报名"),     _T("昨天 09:56"), _T('正'), kAvatarPalette[9], true,  true,  0,  Page_Group,  {} });
        v.push_back({_T("高性能服务器开发"), _T("汉口学院 18机械成海涛: 老师好..."),    _T("04/30"),      _T('高'), kAvatarPalette[3], true,  true,  0,  Page_Group,  {} });
        v.push_back({_T("高性能服务器读者群"),_T("\"北京-java-吴佳远\" 撤回了一条消息"), _T("04/30"),      _T('高'), kAvatarPalette[5], true,  true,  0,  Page_Group,  {} });
        v.push_back({_T("公众平台安全助手"), _T("已群发通知一条【图文】消息..."),        _T("04/29"),      _T('公'), kAvatarPalette[8], false, false, 0,  Page_Publics, {} });
        v.push_back({_T("范鑫"),             _T(""),                                       _T("04/28"),      _T('范'), kAvatarPalette[2], false, false, 0,  Page_Single, {} });
        v.push_back({_T("AAA待办事项"),     _T(""),                                       _T("04/28"),      _T('A'),  kAvatarPalette[4], false, false, 0,  Page_Single, {} });
        v.push_back({_T("公众号"),           _T("[37条] C语言与CPP编程: C++..."),        _T("13:57"),      _T('公'), kAvatarPalette[1], false, false, 0,  Page_Publics, {} });
        v.push_back({_T("正达南校二 (5) 班"),_T("范一林爸爸20号: [图片]"),               _T("13:41"),      _T('正'), kAvatarPalette[9], true,  false, 3,  Page_Group,  {} });

        // ── 凑到 21 条，验证滚动条 + 体现 muted / unread badge 多种状态 ──
        v.push_back({_T("张伟"),             _T("好的，明天见"),                           _T("12:18"),      _T('张'), kAvatarPalette[6], false, false, 0,  Page_Single, {} });
        v.push_back({_T("项目周会群"),       _T("PM李: 下周三的需求评审改到周四"),         _T("11:42"),      _T('项'), kAvatarPalette[2], true,  true,  0,  Page_Group,  {} });
        v.push_back({_T("妈妈"),             _T("视频聊天"),                               _T("10:25"),      _T('妈'), kAvatarPalette[6], false, false, 1,  Page_Single, {} });
        v.push_back({_T("订阅号助手"),       _T("有 2 条新消息"),                          _T("09:50"),      _T('订'), kAvatarPalette[1], false, false, 2,  Page_Publics, {} });
        v.push_back({_T("CSDN 学院"),        _T("[文章] 深入理解 STL"),                    _T("08:30"),      _T('C'),  kAvatarPalette[7], false, false, 0,  Page_Single, {} });
        v.push_back({_T("Linux 内核源码群"), _T("陈博士: 5.18 的 io_uring 这块代码..."), _T("昨天"),       _T('L'),  kAvatarPalette[5], true,  false, 7,  Page_Group,  {} });
        v.push_back({_T("王芳"),             _T("收到，谢谢"),                             _T("昨天"),       _T('王'), kAvatarPalette[3], false, false, 0,  Page_Single, {} });
        v.push_back({_T("HR-赵敏"),          _T("offer 已发，请查收邮箱"),                 _T("星期一"),     _T('H'),  kAvatarPalette[2], false, false, 1,  Page_Single, {} });
        v.push_back({_T("健身房群"),         _T("教练: 周末团课调整通知"),                 _T("星期一"),     _T('健'), kAvatarPalette[7], true,  true,  0,  Page_Group,  {} });
        v.push_back({_T("某信支付"),         _T("您的账户有一笔退款到账"),                 _T("04/27"),      _T('支'), kAvatarPalette[0], false, false, 0,  Page_Single, {} });
        v.push_back({_T("摄影爱好者群"),     _T("Tom: 周末出行约拍人数统计"),              _T("04/26"),      _T('摄'), kAvatarPalette[4], true,  false, 99, Page_Group,  {} });

        // ── 各群独立 mock 8 名成员（贴合身份）。idx 跟上面 push 顺序一致。
        v[1].members  = { // 正达南校三 (5) — 家长群
            M(_T('郭'), kAvatarPalette[2], _T("郭爸爸")),
            M(_T('李'), kAvatarPalette[3], _T("李妈妈")),
            M(_T('王'), kAvatarPalette[4], _T("王爸爸")),
            M(_T('刘'), kAvatarPalette[5], _T("刘妈妈")),
            M(_T('陈'), kAvatarPalette[8], _T("陈妈妈")),
            M(_T('赵'), kAvatarPalette[6], _T("赵老师")),
            M(_T('B'), kAvatarPalette[1], _T("我")),
            M(_T('周'), kAvatarPalette[7], _T("周爸爸")),
        };
        v[2].members  = { // 高性能服务器开发 — 技术学习群
            M(_T('张'), kAvatarPalette[1], _T("张老师")),
            M(_T('海'), kAvatarPalette[3], _T("成海涛")),
            M(_T('王'), kAvatarPalette[2], _T("王工")),
            M(_T('刘'), kAvatarPalette[4], _T("刘工")),
            M(_T('博'), kAvatarPalette[8], _T("陈博士")),
            M(_T('B'), kAvatarPalette[1], _T("我")),
            M(_T('小'), kAvatarPalette[7], _T("小李")),
            M(_T('阿'), kAvatarPalette[5], _T("阿凯")),
        };
        v[3].members  = { // 高性能服务器读者群 — 同上但偏读者
            M(_T('张'), kAvatarPalette[1], _T("张老师")),
            M(_T('陈'), kAvatarPalette[3], _T("北京-陈志刚")),
            M(_T('吴'), kAvatarPalette[2], _T("北京-吴佳远")),
            M(_T('赵'), kAvatarPalette[4], _T("上海-赵子健")),
            M(_T('林'), kAvatarPalette[8], _T("深圳-林志远")),
            M(_T('B'), kAvatarPalette[1], _T("我")),
            M(_T('郑'), kAvatarPalette[7], _T("广州-郑伟")),
            M(_T('马'), kAvatarPalette[5], _T("成都-马俊")),
        };
        v[8].members  = { // 正达南校二 (5) 班 — 家长群
            M(_T('班'), kAvatarPalette[2], _T("班主任王老师")),
            M(_T('范'), kAvatarPalette[3], _T("范一林爸爸20号")),
            M(_T('张'), kAvatarPalette[4], _T("张华妈妈18号")),
            M(_T('李'), kAvatarPalette[5], _T("李明爸爸22号")),
            M(_T('赵'), kAvatarPalette[8], _T("赵晨妈妈15号")),
            M(_T('B'), kAvatarPalette[1], _T("我")),
            M(_T('英'), kAvatarPalette[6], _T("英语老师")),
            M(_T('数'), kAvatarPalette[7], _T("数学老师")),
        };
        v[10].members = { // 项目周会群 — 工作群
            M(_T('P'), kAvatarPalette[2], _T("PM李")),
            M(_T('赵'), kAvatarPalette[1], _T("Tech-赵")),
            M(_T('钱'), kAvatarPalette[3], _T("UI-钱")),
            M(_T('孙'), kAvatarPalette[4], _T("QA-孙")),
            M(_T('B'), kAvatarPalette[1], _T("我")),
            M(_T('马'), kAvatarPalette[5], _T("Lead-马总")),
            M(_T('周'), kAvatarPalette[7], _T("PO-周敏")),
            M(_T('郑'), kAvatarPalette[8], _T("SM-郑磊")),
        };
        v[14].members = { // Linux 内核源码群 — 技术深度群
            M(_T('博'), kAvatarPalette[8], _T("陈博士")),
            M(_T('王'), kAvatarPalette[1], _T("王工")),
            M(_T('刘'), kAvatarPalette[3], _T("刘工")),
            M(_T('B'), kAvatarPalette[1], _T("我")),
            M(_T('赵'), kAvatarPalette[5], _T("赵泽华")),
            M(_T('谢'), kAvatarPalette[2], _T("谢林")),
            M(_T('丁'), kAvatarPalette[4], _T("丁智超")),
            M(_T('K'), kAvatarPalette[7], _T("Kevin Lin")),
        };
        v[17].members = { // 健身房群 — 教练 + 会员
            M(_T('教'), kAvatarPalette[8], _T("教练Tony")),
            M(_T('A'), kAvatarPalette[1], _T("Alex")),
            M(_T('M'), kAvatarPalette[2], _T("Mia")),
            M(_T('B'), kAvatarPalette[1], _T("我")),
            M(_T('J'), kAvatarPalette[3], _T("Jerry")),
            M(_T('S'), kAvatarPalette[4], _T("Sophie")),
            M(_T('R'), kAvatarPalette[5], _T("Ryan")),
            M(_T('L'), kAvatarPalette[7], _T("Lucy")),
        };
        v[19].members = { // 摄影爱好者群 — 影友
            M(_T('T'), kAvatarPalette[1], _T("Tom")),
            M(_T('A'), kAvatarPalette[2], _T("Amy")),
            M(_T('K'), kAvatarPalette[3], _T("Ken")),
            M(_T('L'), kAvatarPalette[4], _T("Leo")),
            M(_T('J'), kAvatarPalette[5], _T("Jane")),
            M(_T('R'), kAvatarPalette[8], _T("Ryan")),
            M(_T('S'), kAvatarPalette[7], _T("Sara")),
            M(_T('B'), kAvatarPalette[1], _T("我")),
        };

        // ── 自动用每个 session 最后一条消息的内容覆盖 preview 字段 ──
        // 之前 preview 是手写死的，时间一长容易和 mock 对话流水 drift。
        // 这里 lazy 同步一次：取 MessagesForSession(i) 倒着找第一条非
        // DateDivider 的消息，按 kind 生成 preview 文本。messages 为空
        // （3 个纯 publics）的 session 保留写死值。
        for (size_t i = 0; i < v.size(); ++i)
        {
            const auto& msgs = MessagesForSession((int)i);
            if (msgs.empty()) { continue; }
            for (auto it = msgs.rbegin(); it != msgs.rend(); ++it)
            {
                if (it->kind == Kind_DateDivider) { continue; }
                CString prefix;
                if (it->fromSelf && v[i].isGroup)
                {
                    prefix = _T("我: ");
                }
                CString body;
                switch (it->kind)
                {
                case Kind_Text:
                    body = it->text;
                    break;
                case Kind_Image:
                    body = _T("[图片]");
                    break;
                case Kind_TransferCard:
                    body = _T("[转账] ") + it->amount;
                    break;
                case Kind_KnowledgeCard:
                    body = _T("[文章] ") + it->title;
                    break;
                case Kind_File:
                    body = _T("[文件] ") + it->fileName;
                    break;
                default:
                    break;
                }
                if (!body.IsEmpty())
                {
                    v[i].preview = prefix + body;
                }
                break;
            }
        }

        return v;
    }();
    return kAll;
}

} // namespace xchat
