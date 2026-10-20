/**
 *  「反馈与浮层」分组的四个演示页面：浮层宿主（DuiPopupHost）、工具提示
 *  （DuiToolTipMgr）、进度条（DuiProgressBar）、表情面板（DuiEmojiPanel）。
 *  本文件只负责把这四个页面的控件树拼出来，版式由 PageKit 提供、文案由
 *  GalleryText 的 Txt() 按当前语言挑选；页面列表在文件末尾的
 *  GetFeedbackPages() 里导出，供 PageRegistry 汇总到导航树上。
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "PageKit.h"
#include "PageRegistry.h"

#include "DuiHost.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/Feedback/DuiPopupHost.h"
#include "Controls/Feedback/DuiToolTip.h"
#include "Controls/Feedback/DuiProgressBar.h"
#include "Controls/Feedback/DuiEmojiPanel.h"

#include <vector>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

namespace Gallery {

namespace {

// =====================================================================
// 本文件共用的版式常量
// =====================================================================

// 演示行内相邻控件之间的水平间距（像素）。
const int kRowGap = 12;
// 并排进度条之间的水平间距（像素）。进度条比按钮细，排得紧一点更好看出对比。
const int kBarRowGap = 8;
// 同一段落内两组演示行之间的竖直间距（像素）。
const int kInnerRowGap = 8;
// 演示按钮的标准宽度（像素）。
const int kButtonWidth = 140;
// 文字较长的演示按钮宽度（像素）。
const int kWideButtonWidth = 180;
// 只写几个字的演示按钮宽度（像素），例如切换延时用的那一组。
const int kSmallButtonWidth = 100;

// =====================================================================
// 浮层宿主（DuiPopupHost）
// =====================================================================

// 浮层的外部尺寸（像素）。四个演示浮层装的是同一份内容，用同一组尺寸。
// 高度按内容算得出来：上下内边距 10×2 + 标题 22 + 说明 20 + 三个条目 28×3
// + 四道 6 像素间距 = 170，取 180 留一点余量。
const int kPopupWidth = 220;
const int kPopupHeight = 180;
// 浮层内容容器的内边距与元素间距（像素）。
const int kPopupPadding = 10;
const int kPopupGap = 6;
// 浮层内容里标题行、说明行、条目按钮的高度（像素）。
const int kPopupTitleH = 22;
const int kPopupDescH = 20;
const int kPopupItemH = 28;
// 浮层内容里演示用的条目个数。
const int kPopupItemCount = 3;
// 浮层说明文字的颜色。比标题浅，拉开层级。
const COLORREF kPopupDescColor = RGB(80, 80, 80);
// 四个浮层各自的标题颜色，用来一眼分辨弹出来的是哪一个。
const COLORREF kPopupAccentBelow = RGB(45, 108, 223);
const COLORREF kPopupAccentAbove = RGB(60, 175, 80);
const COLORREF kPopupAccentRight = RGB(220, 90, 70);
const COLORREF kPopupAccentLeft = RGB(150, 90, 200);

// 生成一棵演示用的浮层内容子树：一行标题、一行说明，再加若干条目按钮。
//   title：浮层标题文字，由调用方保证是静态存储期的字符串。
//   accent：标题的文字颜色，用于区分是哪一个浮层。
// 返回：内容子树的根控件，所有权交给调用方，随后交给 DuiPopupHost::SetContent。
std::unique_ptr<DuiControl> MakePopupContent(LPCTSTR title, COLORREF accent)
{
    std::unique_ptr<DuiVBox> box(new DuiVBox());
    box->SetPadding(kPopupPadding);
    box->SetGap(kPopupGap);

    std::unique_ptr<DuiLabel> titleLabel(new DuiLabel());
    titleLabel->SetText(title);
    titleLabel->SetTextColor(accent);
    box->AddChild(std::move(titleLabel), DuiLayout::Hint().Fixed(kPopupTitleH));

    std::unique_ptr<DuiLabel> descLabel(new DuiLabel());
    descLabel->SetText(Txt(_T("点浮层外面或按 ESC 关闭。"),
                           _T("Click outside or press Esc to dismiss.")));
    descLabel->SetTextColor(kPopupDescColor);
    box->AddChild(std::move(descLabel), DuiLayout::Hint().Fixed(kPopupDescH));

    for (int i = 1; i <= kPopupItemCount; ++i)
    {
        std::unique_ptr<DuiButton> item(new DuiButton());
        CString text;
        text.Format(Txt(_T("条目 %d"), _T("Item %d")), i);
        item->SetText(text);
        box->AddChild(std::move(item), DuiLayout::Hint().Fixed(kPopupItemH));
    }
    return std::unique_ptr<DuiControl>(box.release());
}

// 四个演示浮层，分别对应 DuiPopupHost 的四种边缘偏好。
//
// 定义成静态对象而不是页面的成员，是因为画廊每切换一次页面就会重建整棵页面
// 控件树，而 DuiPopupHost 自己持有一个顶层窗口、并不属于页面控件树；跟着页面
// 反复构造销毁既没有必要，也会把已经创建好的窗口丢掉。这四个对象在进程内只构造
// 一次，页面重建时只更新它们的内容。
DuiPopupHost s_popBelow;
DuiPopupHost s_popAbove;
DuiPopupHost s_popRight;
DuiPopupHost s_popLeft;

// 给四个浮层配好尺寸、边缘偏好与内容。
//
// 每次构建本页面时都调用一次，而不是只在首次调用时初始化：浮层内容里有几行
// 需要按当前语言取的文字，而画廊切换语言之后会重建当前页面，这时把内容一并
// 重建，浮层里的文字才会跟着换。DuiHost::SetRoot 会先清掉悬停 / 捕获 / 焦点
// 指针再换根控件，所以即使浮层此刻正显示着，替换内容也是安全的。
void ConfigurePopups()
{
    s_popBelow.SetSize(kPopupWidth, kPopupHeight);
    s_popBelow.SetEdge(DuiPopupHost::EdgeBelow);
    s_popBelow.SetContent(MakePopupContent(Txt(_T("锚点下方"), _T("Below anchor")),
                                           kPopupAccentBelow));

    s_popAbove.SetSize(kPopupWidth, kPopupHeight);
    s_popAbove.SetEdge(DuiPopupHost::EdgeAbove);
    s_popAbove.SetContent(MakePopupContent(Txt(_T("锚点上方"), _T("Above anchor")),
                                           kPopupAccentAbove));

    s_popRight.SetSize(kPopupWidth, kPopupHeight);
    s_popRight.SetEdge(DuiPopupHost::EdgeRight);
    s_popRight.SetContent(MakePopupContent(Txt(_T("锚点右侧"), _T("Right of anchor")),
                                           kPopupAccentRight));

    s_popLeft.SetSize(kPopupWidth, kPopupHeight);
    s_popLeft.SetEdge(DuiPopupHost::EdgeLeft);
    s_popLeft.SetContent(MakePopupContent(Txt(_T("锚点左侧"), _T("Left of anchor")),
                                          kPopupAccentLeft));
}

// 取一个控件在屏幕坐标系下的矩形，用作浮层的锚点。
//   pCtrl：目标控件。本函数只读取它的矩形与宿主窗口句柄，不持有所有权。
//   rcScreen：输出参数，成功时填入控件的屏幕矩形。
//   hWndOwner：输出参数，成功时填入控件所在宿主窗口的句柄，用作浮层的 owner。
// 返回：控件为空或者还没有挂到宿主窗口上时返回 false，此时两个输出参数保持原值。
bool GetControlScreenRect(DuiControl* pCtrl, RECT& rcScreen, HWND& hWndOwner)
{
    if (pCtrl == NULL || pCtrl->GetHost() == NULL)
    {
        return false;
    }
    HWND hWndHost = pCtrl->GetHost()->m_hWnd;
    if (hWndHost == NULL)
    {
        return false;
    }
    const RECT& rcCtrl = pCtrl->GetRect();
    POINT ptTopLeft = { rcCtrl.left, rcCtrl.top };
    POINT ptBottomRight = { rcCtrl.right, rcCtrl.bottom };
    ::ClientToScreen(hWndHost, &ptTopLeft);
    ::ClientToScreen(hWndHost, &ptBottomRight);

    rcScreen.left = ptTopLeft.x;
    rcScreen.top = ptTopLeft.y;
    rcScreen.right = ptBottomRight.x;
    rcScreen.bottom = ptBottomRight.y;
    hWndOwner = hWndHost;
    return true;
}

// 以按钮自身的屏幕矩形为锚点弹出指定的浮层。四个按钮的处理只差用哪一个浮层，
// 所以共用这一段。
//   popup：要弹出的浮层。
//   pButton：被点击的按钮，用它的屏幕矩形当锚点、用它所在的窗口当 owner。
void ShowPopupAtButton(DuiPopupHost& popup, FnButton* pButton)
{
    RECT rcAnchor = { 0, 0, 0, 0 };
    HWND hWndOwner = NULL;
    if (!GetControlScreenRect(pButton, rcAnchor, hWndOwner))
    {
        return;
    }
    popup.Show(rcAnchor, hWndOwner);
}

// 下面四个是四个演示按钮的点击处理。
void OnOpenPopupBelow(FnButton* pButton)
{
    ShowPopupAtButton(s_popBelow, pButton);
}

void OnOpenPopupAbove(FnButton* pButton)
{
    ShowPopupAtButton(s_popAbove, pButton);
}

void OnOpenPopupRight(FnButton* pButton)
{
    ShowPopupAtButton(s_popRight, pButton);
}

void OnOpenPopupLeft(FnButton* pButton)
{
    ShowPopupAtButton(s_popLeft, pButton);
}

// =====================================================================
// 工具提示（DuiToolTipMgr）
// =====================================================================

// DuiToolTipMgr 的默认弹出延时（毫秒），取自 DuiToolTip.h 里 m_delayMs 的默认值。
const UINT kTipDefaultDelayMs = 500;
// 演示用的两档非默认延时（毫秒）：几乎立刻弹出、以及明显偏慢。
const UINT kTipInstantDelayMs = 0;
const UINT kTipSlowDelayMs = 1500;

// 显示当前弹出延时的标签。指向本页面里的控件，每次重建页面时重新赋值；
// 只在本页面按钮的点击处理里使用，页面销毁之后不会再被访问。
DuiLabel* s_pTipDelayLabel = NULL;
// 演示注册与注销的目标控件，以及显示它当前注册状态的标签。生命期同上。
DuiControl* s_pTipTarget = NULL;
DuiLabel* s_pTipStateLabel = NULL;

// 把管理器当前的弹出延时写到标签上。
void UpdateTipDelayLabel()
{
    if (s_pTipDelayLabel == NULL)
    {
        return;
    }
    CString text;
    text.Format(Txt(_T("当前延时：%u 毫秒"), _T("Current delay: %u ms")),
                DuiToolTipMgr::Inst().GetDelay());
    s_pTipDelayLabel->SetText(text);
}

// 改变弹出延时并刷新标签。
//   delayMs：新的延时（毫秒）。0 表示鼠标停下即弹出。
void ApplyTipDelay(UINT delayMs)
{
    DuiToolTipMgr::Inst().SetDelay(delayMs);
    UpdateTipDelayLabel();
}

void OnTipDelayInstant(FnButton* /*pButton*/)
{
    ApplyTipDelay(kTipInstantDelayMs);
}

void OnTipDelayDefault(FnButton* /*pButton*/)
{
    ApplyTipDelay(kTipDefaultDelayMs);
}

void OnTipDelaySlow(FnButton* /*pButton*/)
{
    ApplyTipDelay(kTipSlowDelayMs);
}

// 立刻收起提示：关闭正在显示的浮窗，连同还在等延时的弹出计时。宿主窗口在按下鼠标左键或右键、
// 按键、失去焦点、滚轮滚动了内容时也是这么做的。
void OnTipHideNow(FnButton* /*pButton*/)
{
    DuiToolTipMgr::Inst().HideNow();
}

// 刷新「注册状态」标签：登记过就显示登记的文字，没登记就说明悬停不会有反应。
void UpdateTipRegistrationLabel()
{
    if (s_pTipStateLabel == NULL || s_pTipTarget == NULL)
    {
        return;
    }
    CString registered = DuiToolTipMgr::Inst().GetText(s_pTipTarget);
    CString text;
    if (registered.IsEmpty())
    {
        text = Txt(_T("当前状态：未注册，悬停不会弹出提示"),
                   _T("State: not registered, hovering shows nothing"));
    }
    else
    {
        text.Format(Txt(_T("当前状态：已注册「%s」"), _T("State: registered \"%s\"")),
                    (LPCTSTR)registered);
    }
    s_pTipStateLabel->SetText(text);
}

// 切换目标控件的注册状态，并把按钮文字改成下一次点击会做的事。
void OnTipToggleRegistration(FnButton* pButton)
{
    if (pButton == NULL || s_pTipTarget == NULL)
    {
        return;
    }
    if (DuiToolTipMgr::Inst().GetText(s_pTipTarget).IsEmpty())
    {
        DuiToolTipMgr::Inst().Register(s_pTipTarget,
                                       Txt(_T("这条提示是刚刚重新注册上去的"),
                                           _T("This tip was just registered again")));
        pButton->SetText(Txt(_T("取消注册"), _T("Unregister")));
    }
    else
    {
        DuiToolTipMgr::Inst().Unregister(s_pTipTarget);
        pButton->SetText(Txt(_T("重新注册"), _T("Register")));
    }
    UpdateTipRegistrationLabel();
}

// =====================================================================
// 进度条（DuiProgressBar）
// =====================================================================

// 进度条演示行的高度（像素）。进度条自己没有内在高度，画多高由行高决定。
const int kProgressRowH = 22;
// 演示统一使用的取值范围上限。用 100 是为了让进度值与覆盖文字里的百分比一致。
const int kProgressMax = 100;
// 百分比覆盖文字那一段用到的几个进度值，覆盖空、少量、过半、满四种情形。
const int kProgressDemoValues[] = { 0, 25, 60, kProgressMax };
const int kProgressDemoValueCount =
    (int)(sizeof(kProgressDemoValues) / sizeof(kProgressDemoValues[0]));
// 自定义覆盖文字那一段演示用的进度值。
const int kProgressCustomTextValue = 45;

// 自定义配色演示里的一项：一种填充色配一个进度值。
struct ProgressColorSpec
{
    // 填充色。
    COLORREF fillColor;
    // 进度值，取值范围 0 到 kProgressMax。
    int percent;
};

// 三种状态各一条：进展顺利用绿色、需要留意用橙色、出错用红色。
const ProgressColorSpec kProgressColorSpecs[] = {
    { RGB( 60, 170,  90), 80 },
    { RGB(230, 150,  40), 50 },
    { RGB(220,  80,  80), 20 },
};
const int kProgressColorSpecCount =
    (int)(sizeof(kProgressColorSpecs) / sizeof(kProgressColorSpecs[0]));

// =====================================================================
// 表情面板（DuiEmojiPanel）
// =====================================================================

// 装表情面板的浮层。与上面四个演示浮层同理，做成静态对象只初始化一次。
DuiPopupHost s_emojiPopup;
// 显示最近一次选中结果的标签。指向本页面里的控件，每次重建页面时重新赋值。
DuiLabel* s_pEmojiResultLabel = NULL;
// 表情浮层是否已经初始化过。
bool s_emojiInited = false;

// 已经加载的表情位图。DuiEmojiPanel 只借用 HBITMAP、不接管所有权，所以必须由
// 本文件保存到进程结束；这些位图随进程退出一并释放，不单独回收。
std::vector<HBITMAP> s_emojiBitmaps;

// GDI+ 初始化返回的令牌。为 0 表示本文件还没有初始化过 GDI+。
ULONG_PTR s_gdiplusToken = 0;

// 尝试从可执行文件目录下的 Face 子目录加载多少张 faceN.png。
const int kEmojiPngCount = 40;
// 表情面板的列数与单格边长（像素）。
const int kEmojiColumns = 8;
const int kEmojiCellSize = 36;
// 浮层比面板本身多留出的边距（像素），上下左右各占一半。
const int kEmojiPopupMargin = 8;

// 直接嵌在页面里的那一段演示所用的单格边长（像素）。
const int kInlineEmojiCellSize = 32;

// 两块表情面板的控件编号。表情面板选中时发出的是通用的 DUIN_VALUECHANGED，
// 这个通知码所有控件共用，接收方只能靠控件编号区分，所以不能留成默认的 0。
// 取值避开画廊窗口自己用掉的 1（标签条）与 2（滚动视图）。
const UINT kIdPopupEmojiPanel = 9401;
const UINT kIdInlineEmojiPanel = 9402;

// 不依赖任何外部图片的表情序列，用于「直接嵌在页面里」的那一段演示。
// 每一项都写成 UTF-16 码元的十六进制转义，超出基本多文种平面的字符写成代理对，
// 这样源文件里不会出现依赖编辑器字体才能看清的字符。
const LPCTSTR kInlineEmojiSequences[] = {
    _T("\xD83D\xDE00"),   // U+1F600 咧嘴笑
    _T("\xD83D\xDE02"),   // U+1F602 笑出眼泪
    _T("\xD83D\xDC4D"),   // U+1F44D 点赞
    _T("\xD83D\xDC4C"),   // U+1F44C OK 手势
    _T("\xD83C\xDF89"),   // U+1F389 庆祝彩带
    _T("\xD83D\xDD25"),   // U+1F525 火焰
    _T("\x2764"),         // U+2764  红心
    _T("\x2B50"),         // U+2B50  星星
};
const int kInlineEmojiCount =
    (int)(sizeof(kInlineEmojiSequences) / sizeof(kInlineEmojiSequences[0]));

// 拼出 <可执行文件目录>\Face\faceN.png 的完整路径。
// 这里不经由 CSkinManager，是为了让画廊不依赖客户端的皮肤管理模块。
//   n：表情序号，对应文件名里的数字。
// 返回：完整路径；取不到可执行文件路径时会退化成 \Face\faceN.png，加载必然失败，
//       对应的格子退回到用文字绘制。
CString MakeFacePngPath(int n)
{
    TCHAR szModulePath[MAX_PATH] = { 0 };
    ::GetModuleFileName(NULL, szModulePath, MAX_PATH);
    CString dir = szModulePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash >= 0)
    {
        dir = dir.Left(slash);
    }
    CString path;
    path.Format(_T("%s\\Face\\face%d.png"), (LPCTSTR)dir, n);
    return path;
}

// 用 GDI+ 把一张 PNG 解码成 HBITMAP。
//   path：图片文件的完整路径。
// 返回：解码得到的位图，所有权交给调用方；文件不存在或格式不对时返回 NULL。
HBITMAP LoadPngAsHbitmap(LPCTSTR path)
{
    Gdiplus::Bitmap bitmap(path);
    if (bitmap.GetLastStatus() != Gdiplus::Ok)
    {
        return NULL;
    }
    HBITMAP hBitmap = NULL;
    bitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hBitmap);
    return hBitmap;
}

// 确保 GDI+ 已经初始化。画廊里可能已经有别的模块初始化过，重复初始化没有害处，
// 但这里用令牌自行判断，只做一次。
void EnsureGdiplusStarted()
{
    if (s_gdiplusToken != 0)
    {
        return;
    }
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&s_gdiplusToken, &input, NULL);
}

// 表情面板的选中回调。
//   userdata：注册回调时传入的指针，本演示不需要，忽略。
//   sequence：选中项的插入序列，聊天输入框应当插入的正是这段文字。
//   index：选中项在面板里的序号。
void OnEmojiPicked(void* /*userdata*/, LPCTSTR sequence, int index)
{
    if (s_pEmojiResultLabel != NULL)
    {
        CString text;
        text.Format(Txt(_T("最近选中：%s   （序号 %d）"),
                        _T("Last picked: %s   (index %d)")),
                    sequence, index);
        s_pEmojiResultLabel->SetText(text);
    }
    // 选中之后立刻收起浮层，这是选择面板的常规做法。走 pick 回调而不是
    // DUIN_VALUECHANGED 通知，就是为了不必等通知绕一圈再关闭。
    s_emojiPopup.Hide(DuiPopupHost::ReasonProgrammatic);
}

// 首次使用前建好表情面板并装进浮层。重复调用时直接返回。
void EnsureEmojiPopupInited()
{
    if (s_emojiInited)
    {
        return;
    }
    s_emojiInited = true;
    EnsureGdiplusStarted();

    std::unique_ptr<DuiEmojiPanel> panel(new DuiEmojiPanel());
    panel->SetCtrlId(kIdPopupEmojiPanel);
    panel->SetColumns(kEmojiColumns);
    panel->SetCellSize(kEmojiCellSize);

    // 加载随项目一起发布的彩色表情图片。每一项的插入序列取聊天正文里的表情
    // 标签形式（[face0]、[face1] ……），聊天输入框拿到它就知道该插入什么。
    // 图片文件缺失时 LoadPngAsHbitmap 返回 NULL，该格会退回到用文字绘制。
    s_emojiBitmaps.reserve(kEmojiPngCount);
    for (int i = 0; i < kEmojiPngCount; ++i)
    {
        CString path = MakeFacePngPath(i);
        HBITMAP hBitmap = LoadPngAsHbitmap(path);
        s_emojiBitmaps.push_back(hBitmap);
        CString tag;
        tag.Format(_T("[face%d]"), i);
        panel->AddEmojiBitmap(tag, hBitmap, tag);
    }
    panel->SetPickCallback(&OnEmojiPicked, NULL);

    // 面板的期望尺寸就是 列数 × 单格边长，浮层照着它加上一圈边距即可。
    SIZE desired = panel->GetDesiredSize();
    s_emojiPopup.SetSize(desired.cx + kEmojiPopupMargin, desired.cy + kEmojiPopupMargin);
    s_emojiPopup.SetEdge(DuiPopupHost::EdgeBelow);
    s_emojiPopup.SetContent(std::unique_ptr<DuiControl>(panel.release()));
}

// 「打开表情面板」按钮的点击处理。
void OnOpenEmojiPopup(FnButton* pButton)
{
    ShowPopupAtButton(s_emojiPopup, pButton);
}

} // 匿名命名空间

// ===== 浮层宿主 =======================================================

std::unique_ptr<DuiControl> Build_PopupHost()
{
    ConfigurePopups();

    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("按边缘偏好弹出浮层"), _T("Edge preference")),
               Txt(_T("DuiPopupHost 是一个顶层 WS_POPUP 窗口，里面装的仍然是一棵普通的 DUI 控件树，")
                   _T("因此下拉框、菜单、提示条、表情面板都可以共用它作为基座。点击下面任意一个按钮，")
                   _T("都会以这个按钮的屏幕矩形作为锚点弹出浮层：EdgeBelow 放在锚点下方，EdgeAbove 放在上方，")
                   _T("EdgeRight 与 EdgeLeft 放在左右两侧。把画廊窗口拖到屏幕边缘再点一次，")
                   _T("可以看到首选的一侧放不下时浮层会自动翻到对面。"),
                   _T("DuiPopupHost is a top-level WS_POPUP window that hosts an ordinary DUI control tree, ")
                   _T("so dropdowns, menus, tips and emoji panels can all share it. Each button below opens ")
                   _T("the popup anchored to its own screen rectangle: EdgeBelow, EdgeAbove, EdgeRight and ")
                   _T("EdgeLeft pick the preferred side. Drag the gallery window against a screen edge and ")
                   _T("click again to see the popup flip to the opposite side when the preferred one does not fit.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> btnBelow(new FnButton());
        btnBelow->SetText(Txt(_T("在下方弹出"), _T("Open below")));
        btnBelow->onClick = &OnOpenPopupBelow;

        std::unique_ptr<FnButton> btnAbove(new FnButton());
        btnAbove->SetText(Txt(_T("在上方弹出"), _T("Open above")));
        btnAbove->onClick = &OnOpenPopupAbove;

        std::unique_ptr<FnButton> btnRight(new FnButton());
        btnRight->SetText(Txt(_T("在右侧弹出"), _T("Open right")));
        btnRight->onClick = &OnOpenPopupRight;

        std::unique_ptr<FnButton> btnLeft(new FnButton());
        btnLeft->SetText(Txt(_T("在左侧弹出"), _T("Open left")));
        btnLeft->onClick = &OnOpenPopupLeft;

        row->AddChild(std::move(btnBelow), DuiLayout::Hint().Fixed(kButtonWidth));
        row->AddChild(std::move(btnAbove), DuiLayout::Hint().Fixed(kButtonWidth));
        row->AddChild(std::move(btnRight), DuiLayout::Hint().Fixed(kButtonWidth));
        row->AddChild(std::move(btnLeft), DuiLayout::Hint().Fixed(kButtonWidth));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("关闭方式与窗口复用"), _T("Dismiss and reuse")),
               Txt(_T("浮层有三条关闭路径：按下 ESC 键（原因为 ReasonEscape）、窗口失去激活状态")
                   _T("（ReasonLostFocus，点在浮层外面时走的就是这一条）、以及业务代码显式调用 Hide")
                   _T("（ReasonProgrammatic）。因为失焦关闭发生在新目标收到鼠标点击之前，")
                   _T("所以点浮层外面的按钮这一下既关掉了浮层，也照常点到了那个按钮。")
                   _T("Hide 只是隐藏窗口，窗口句柄与内容都保留下来，下一次 Show 直接复用，")
                   _T("因此本页面的四个浮层都是只构造一次的静态对象。关闭原因可以通过 ")
                   _T("SetDismissCallback 拿到，常见用途是把触发浮层的那个按钮恢复成未按下状态。"),
                   _T("A popup closes in three ways: the Esc key (ReasonEscape), losing window activation ")
                   _T("(ReasonLostFocus, which is what a click outside produces), and an explicit Hide call ")
                   _T("(ReasonProgrammatic). Focus-loss dismissal happens before the new target receives the ")
                   _T("click, so clicking a button outside both dismisses the popup and activates that button. ")
                   _T("Hide only hides the window - the HWND and its content stay alive for the next Show, ")
                   _T("which is why the four popups on this page are constructed once and reused. ")
                   _T("SetDismissCallback reports the reason, typically used to un-press the button that opened it.")));

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 工具提示 =======================================================

std::unique_ptr<DuiControl> Build_ToolTip()
{
    // 弹出延时是进程级设置，下面有一段演示会改它。每次进入本页面先恢复默认值，
    // 免得上一次演示留下的 1500 毫秒影响画廊其它页面。
    DuiToolTipMgr::Inst().SetDelay(kTipDefaultDelayMs);

    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("逐个控件注册提示文字"), _T("Per-control tooltips")),
               Txt(_T("DuiToolTipMgr 是进程级单例，业务侧把控件指针和一段文字登记进去，")
                   _T("鼠标在该控件上停留到设定的延时（默认 500 毫秒）就会弹出一个淡黄色浮窗。")
                   _T("提示浮窗出现在鼠标位置的右下方，不随控件移动。")
                   _T("把鼠标依次移过下面三个按钮，提示会换成各自的文字；按下鼠标左键或右键会立刻关闭当前提示。"),
                   _T("DuiToolTipMgr is a process-wide singleton: register a control pointer together with ")
                   _T("a string, and hovering that control for the configured delay (500 ms by default) pops ")
                   _T("a pale-yellow tip window. The tip appears below and right of the cursor and does not ")
                   _T("follow the control. Move across the three buttons below to see each tip in turn; ")
                   _T("pressing the left or right mouse button hides the current one immediately.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiButton> btnSend(new DuiButton());
        btnSend->SetText(Txt(_T("发送"), _T("Send")));
        DuiButton* pSend = btnSend.get();

        std::unique_ptr<DuiButton> btnOpen(new DuiButton());
        btnOpen->SetText(Txt(_T("打开"), _T("Open")));
        DuiButton* pOpen = btnOpen.get();

        std::unique_ptr<DuiButton> btnLogout(new DuiButton());
        btnLogout->SetText(Txt(_T("退出登录"), _T("Log out")));
        DuiButton* pLogout = btnLogout.get();

        DuiToolTipMgr::Inst().Register(pSend,
                                       Txt(_T("发送一条消息"), _T("Send a chat message")));
        DuiToolTipMgr::Inst().Register(pOpen,
                                       Txt(_T("打开文件选择框"), _T("Open the file picker")));
        DuiToolTipMgr::Inst().Register(pLogout,
                                       Txt(_T("退出登录并关闭程序"), _T("Log out and quit")));

        row->AddChild(std::move(btnSend), DuiLayout::Hint().Fixed(kButtonWidth));
        row->AddChild(std::move(btnOpen), DuiLayout::Hint().Fixed(kButtonWidth));
        row->AddChild(std::move(btnLogout), DuiLayout::Hint().Fixed(kButtonWidth));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("提示文字的长度"), _T("Length of the tip text")),
               Txt(_T("浮窗的宽度按文字实际测出来的宽度算，但不超过最大宽度（96 DPI 下 400 像素，")
                   _T("随显示器 DPI 缩放，也不超过屏幕工作区）。文字更长时自动换行：在词与词之间断开，")
                   _T("比一行还宽的长词（如没有空格的文件名、路径）按字符断开。")
                   _T("下面三个按钮分别注册了短、中、长三段文字，把鼠标依次移过去就能看出差别：")
                   _T("一句话说不完的内容不适合放进提示里，应当放到界面上或者帮助文档里。"),
                   _T("The tip window is sized from the measured width of its text, up to a maximum width ")
                   _T("(400 pixels at 96 DPI, scaled with the monitor DPI and never wider than the work area). ")
                   _T("Longer text wraps between words, and a word wider than a line - a file name or a path ")
                   _T("without spaces - is broken between characters. The three buttons below carry a short, ")
                   _T("a medium and a long string. Anything that does not fit in one sentence belongs on the ")
                   _T("page itself or in the documentation, not in a tooltip.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiButton> btnShort(new DuiButton());
        btnShort->SetText(Txt(_T("短提示"), _T("Short")));
        DuiButton* pShort = btnShort.get();

        std::unique_ptr<DuiButton> btnMedium(new DuiButton());
        btnMedium->SetText(Txt(_T("中等提示"), _T("Medium")));
        DuiButton* pMedium = btnMedium.get();

        std::unique_ptr<DuiButton> btnLong(new DuiButton());
        btnLong->SetText(Txt(_T("长提示"), _T("Long")));
        DuiButton* pLong = btnLong.get();

        DuiToolTipMgr::Inst().Register(pShort, Txt(_T("保存"), _T("Save")));
        DuiToolTipMgr::Inst().Register(pMedium,
                                       Txt(_T("保存当前草稿到本地"),
                                           _T("Save the current draft locally")));
        DuiToolTipMgr::Inst().Register(pLong,
                                       Txt(_T("保存当前草稿到本地，下次打开这个窗口时会自动恢复，")
                                           _T("草稿只保留最近一次的内容，重新编辑会把它覆盖掉"),
                                           _T("Save the current draft locally; it is restored the next time ")
                                           _T("this window opens. Only the most recent draft is kept and ")
                                           _T("editing again overwrites it")));

        row->AddChild(std::move(btnShort), DuiLayout::Hint().Fixed(kButtonWidth));
        row->AddChild(std::move(btnMedium), DuiLayout::Hint().Fixed(kButtonWidth));
        row->AddChild(std::move(btnLong), DuiLayout::Hint().Fixed(kButtonWidth));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("弹出延时与消失时机"), _T("Delay and dismissal")),
               Txt(_T("SetDelay 调整的是鼠标停下到浮窗出现之间的等待时间，默认 500 毫秒。")
                   _T("这个设置对整个进程生效，所以每次进入本页面都会先把它恢复成默认值。")
                   _T("已有提示正在显示、或刚离开一个正在显示的提示时，移到下一个带提示的控件只等五分之一的延时。")
                   _T("提示弹出之后不会自己超时消失，只有在鼠标移到别的控件上、移出窗口、按下鼠标左键或右键、")
                   _T("按键、窗口失去焦点、滚轮滚动了内容，或者业务代码调用 HideNow 时才会关闭。")
                   _T("下面第一行切换延时，第二行是一个带提示的控件和一个立即关闭提示的按钮。"),
                   _T("SetDelay controls how long the cursor must rest before the tip appears; the default ")
                   _T("is 500 ms. The setting is process-wide, so this page restores the default every time ")
                   _T("it is opened. While a tip is showing, or just after leaving one, moving on to another ")
                   _T("control with a tip waits only a fifth of the delay. Once shown, a tip never times out ")
                   _T("on its own - it closes when the cursor moves to another control, leaves the window, the ")
                   _T("left or right mouse button goes down, a key is pressed, the window loses the focus, the wheel ")
                   _T("scrolls the content, or the application calls HideNow. The first row below switches ")
                   _T("the delay; the second row carries a hover target and a button that closes the tip at once.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> btnInstant(new FnButton());
        btnInstant->SetText(_T("0 ms"));
        btnInstant->onClick = &OnTipDelayInstant;

        std::unique_ptr<FnButton> btnDefault(new FnButton());
        btnDefault->SetText(_T("500 ms"));
        btnDefault->onClick = &OnTipDelayDefault;

        std::unique_ptr<FnButton> btnSlow(new FnButton());
        btnSlow->SetText(_T("1500 ms"));
        btnSlow->onClick = &OnTipDelaySlow;

        std::unique_ptr<DuiLabel> delayLabel(new DuiLabel());
        s_pTipDelayLabel = delayLabel.get();

        row->AddChild(std::move(btnInstant), DuiLayout::Hint().Fixed(kSmallButtonWidth));
        row->AddChild(std::move(btnDefault), DuiLayout::Hint().Fixed(kSmallButtonWidth));
        row->AddChild(std::move(btnSlow), DuiLayout::Hint().Fixed(kSmallButtonWidth));
        row->AddChild(std::move(delayLabel), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
        // 标签是本次重建页面时新建的，先把当前延时写上去。
        UpdateTipDelayLabel();
    }
    AddGap(page.get(), kInnerRowGap);
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiButton> btnHover(new DuiButton());
        btnHover->SetText(Txt(_T("把鼠标停在这里"), _T("Rest the cursor here")));
        DuiButton* pHover = btnHover.get();
        DuiToolTipMgr::Inst().Register(pHover,
                                       Txt(_T("提示按当前延时弹出"),
                                           _T("The tip pops after the current delay")));

        std::unique_ptr<FnButton> btnHide(new FnButton());
        btnHide->SetText(Txt(_T("立即关闭提示"), _T("Hide now")));
        btnHide->onClick = &OnTipHideNow;

        row->AddChild(std::move(btnHover), DuiLayout::Hint().Fixed(kWideButtonWidth));
        row->AddChild(std::move(btnHide), DuiLayout::Hint().Fixed(kWideButtonWidth));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("注册、注销与注册表的共享"), _T("Register, unregister and the shared registry")),
               Txt(_T("全进程共用同一份注册表，条目按控件指针匹配：对同一个控件重复 Register 会覆盖原来的文字，")
                   _T("Unregister 之后悬停就不再弹出。DuiControl 的析构函数里已经调用了 Unregister，")
                   _T("所以控件销毁不会在注册表里留下失效指针；显式调用 Unregister 用于控件还在、")
                   _T("但要临时取消提示的场景，例如按钮进入禁用状态时。")
                   _T("点右边的按钮可以反复切换左边那个按钮的注册状态，标签上显示的是用 GetText 读回来的结果。"),
                   _T("All controls share one registry, keyed by control pointer: registering the same control ")
                   _T("again replaces its text, and after Unregister hovering does nothing. ~DuiControl already ")
                   _T("calls Unregister, so destroyed controls never leave stale pointers behind; calling it ")
                   _T("explicitly is for controls that stay alive but should temporarily lose their tip, such as ")
                   _T("a button entering the disabled state. The button on the right toggles the registration of ")
                   _T("the one on the left, and the label shows what GetText reads back.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiButton> btnTarget(new DuiButton());
        btnTarget->SetText(Txt(_T("带提示的按钮"), _T("Target button")));
        s_pTipTarget = btnTarget.get();
        DuiToolTipMgr::Inst().Register(s_pTipTarget,
                                       Txt(_T("页面建好时就注册上去的提示"),
                                           _T("Registered when the page was built")));

        std::unique_ptr<FnButton> btnToggle(new FnButton());
        btnToggle->SetText(Txt(_T("取消注册"), _T("Unregister")));
        btnToggle->onClick = &OnTipToggleRegistration;

        std::unique_ptr<DuiLabel> stateLabel(new DuiLabel());
        s_pTipStateLabel = stateLabel.get();

        row->AddChild(std::move(btnTarget), DuiLayout::Hint().Fixed(kWideButtonWidth));
        row->AddChild(std::move(btnToggle), DuiLayout::Hint().Fixed(kButtonWidth));
        row->AddChild(std::move(stateLabel), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
        // 两个指针刚刚指向本次新建的控件，据此刷新一次状态文字。
        UpdateTipRegistrationLabel();
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 进度条 =========================================================

std::unique_ptr<DuiControl> Build_ProgressBar()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("默认的百分比覆盖文字"), _T("Default percent overlay")),
               Txt(_T("默认是水平的确定进度模式：品牌蓝的填充按当前值在 [min, max] 里的比例从左往右长。")
                   _T("没有设置过覆盖文字时，控件会在正中间画出百分比（例如 60%），")
                   _T("SetShowPercent(false) 可以把它关掉。下面四条分别是 0、25、60、100。")
                   _T("SetPos 会把传入的值限制在取值范围内，值真的发生变化时发出 DUIN_VALUECHANGED 通知，")
                   _T("初始化阶段可以传 notify 为 false 抑制这次通知。"),
                   _T("The default mode is a horizontal determinate bar: the brand-blue fill grows from the ")
                   _T("left in proportion to the current value within [min, max]. With no custom text the ")
                   _T("control paints the percentage in the middle (60%, for instance); SetShowPercent(false) ")
                   _T("turns that off. The four bars below sit at 0, 25, 60 and 100. SetPos clamps its argument ")
                   _T("to the range and fires DUIN_VALUECHANGED when the value actually changes; pass ")
                   _T("notify = false to suppress that during initialization.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kBarRowGap);
        for (int i = 0; i < kProgressDemoValueCount; ++i)
        {
            std::unique_ptr<DuiProgressBar> bar(new DuiProgressBar());
            bar->SetRange(0, kProgressMax);
            bar->SetPos(kProgressDemoValues[i], false);
            row->AddChild(std::move(bar), DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(row), kProgressRowH);
    }

    AddSection(page.get(),
               Txt(_T("自定义覆盖文字"), _T("Custom text")),
               Txt(_T("SetText 设置的文字优先于百分比显示，适合把已完成量和总量一起写出来，")
                   _T("读的人不必再拿百分比去换算。把文字设成空串就回到显示百分比的状态。"),
                   _T("Text set with SetText takes precedence over the percentage, which suits progress that ")
                   _T("is better read as an amount than as a ratio. Setting it back to an empty string ")
                   _T("restores the percentage overlay.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiProgressBar> bar(new DuiProgressBar());
        bar->SetRange(0, kProgressMax);
        bar->SetPos(kProgressCustomTextValue, false);
        bar->SetText(Txt(_T("正在上传 45 / 100 MB"), _T("Uploading 45 / 100 MB")));
        row->AddChild(std::move(bar), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row), kProgressRowH);
    }

    AddSection(page.get(),
               Txt(_T("自定义配色"), _T("Custom colors")),
               Txt(_T("填充色、底色、边框色、文字色都可以单独覆盖，用来表达进度之外的含义。")
                   _T("下面三条用绿、橙、红分别表示进展顺利、需要留意、已经出错。"),
                   _T("Fill, background, border and text colors can each be overridden to carry meaning ")
                   _T("beyond the value itself. The three bars below use green, orange and red for healthy, ")
                   _T("needs-attention and failed.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kBarRowGap);
        for (int i = 0; i < kProgressColorSpecCount; ++i)
        {
            std::unique_ptr<DuiProgressBar> bar(new DuiProgressBar());
            bar->SetRange(0, kProgressMax);
            bar->SetPos(kProgressColorSpecs[i].percent, false);
            bar->SetFillColor(kProgressColorSpecs[i].fillColor);
            row->AddChild(std::move(bar), DuiLayout::Hint().Weight(1));
        }
        AddVariantRow(page.get(), std::move(row), kProgressRowH);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 表情面板 =======================================================

std::unique_ptr<DuiControl> Build_Emoji()
{
    EnsureEmojiPopupInited();

    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(),
               Txt(_T("放在浮层里的表情面板"), _T("Emoji panel inside a popup")),
               Txt(_T("DuiEmojiPanel 是一块按行列排布的表情网格，最常见的用法是把它交给 DuiPopupHost ")
                   _T("作为内容，由输入框旁边的按钮弹出。点「打开表情面板」会在按钮下方弹出面板，")
                   _T("选中一个表情后，pick 回调把该项的插入序列写到右边的标签上并立刻收起浮层。")
                   _T("面板的期望尺寸等于 列数 × 单格边长，浮层照着它设置大小即可。")
                   _T("这一段的图片取自可执行文件目录下的 Face\\face0.png 到 face39.png，")
                   _T("文件缺失时对应的格子会退回到用文字绘制。"),
                   _T("DuiEmojiPanel is a grid of emoji cells, most often handed to a DuiPopupHost as its ")
                   _T("content and opened by a button next to the composer. \"Open emoji panel\" drops the ")
                   _T("panel below the button; picking a cell writes its insert sequence into the label on the ")
                   _T("right through the pick callback and hides the popup at once. The panel's desired size is ")
                   _T("columns x cell size, which is what the popup is sized from. The images come from ")
                   _T("Face\\face0.png through face39.png next to the executable; missing files fall back to ")
                   _T("text rendering.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<FnButton> btnOpen(new FnButton());
        btnOpen->SetText(Txt(_T("打开表情面板"), _T("Open emoji panel")));
        btnOpen->onClick = &OnOpenEmojiPopup;

        std::unique_ptr<DuiLabel> resultLabel(new DuiLabel());
        resultLabel->SetText(Txt(_T("最近选中：（无）"), _T("Last picked: (none)")));
        s_pEmojiResultLabel = resultLabel.get();

        row->AddChild(std::move(btnOpen), DuiLayout::Hint().Fixed(kWideButtonWidth));
        row->AddChild(std::move(resultLabel), DuiLayout::Hint().Weight(1));
        AddVariantRow(page.get(), std::move(row));
    }

    AddSection(page.get(),
               Txt(_T("不带图片的表情项"), _T("Text-only entries")),
               Txt(_T("AddEmoji 只登记要插入的 UTF-16 序列，不带图片，控件用 Segoe UI Emoji 字体")
                   _T("直接把序列画出来。GDI 的 DrawText 即使遇到彩色字体也只画灰阶字形，")
                   _T("所以要显示彩色表情必须走 AddEmojiBitmap 的图片路径。")
                   _T("下面这一行不依赖任何外部图片，可以直接看到网格排布、悬停高亮与按下效果。")
                   _T("选中时控件先发出 DUIN_VALUECHANGED 通知（extra 是选中项的序号），")
                   _T("再调用 SetPickCallback 注册的回调，这一行没有注册回调，因此选中只发通知。"),
                   _T("AddEmoji registers only the UTF-16 sequence to insert, with no bitmap; the control ")
                   _T("draws the sequence with the Segoe UI Emoji font. GDI's DrawText paints color fonts as ")
                   _T("grayscale glyphs, so color emoji require the AddEmojiBitmap path. The row below needs ")
                   _T("no external assets, which makes the grid layout, hover highlight and pressed state ")
                   _T("visible on their own. A pick first fires DUIN_VALUECHANGED with the index in extra and ")
                   _T("then calls the callback registered through SetPickCallback - this row registers none, ")
                   _T("so a pick only sends the notification.")));
    {
        std::unique_ptr<DuiHBox> row(new DuiHBox());
        row->SetGap(kRowGap);

        std::unique_ptr<DuiEmojiPanel> panel(new DuiEmojiPanel());
        panel->SetCtrlId(kIdInlineEmojiPanel);
        // 列数取表情个数，这一段就只排成一行，正好嵌在演示行里。
        panel->SetColumns(kInlineEmojiCount);
        panel->SetCellSize(kInlineEmojiCellSize);
        for (int i = 0; i < kInlineEmojiCount; ++i)
        {
            panel->AddEmoji(kInlineEmojiSequences[i]);
        }
        row->AddChild(std::move(panel),
                      DuiLayout::Hint().Fixed(kInlineEmojiCount * kInlineEmojiCellSize));
        AddVariantRow(page.get(), std::move(row), kInlineEmojiCellSize);
    }

    return std::unique_ptr<DuiControl>(page.release());
}

// ===== 本分组的页面列表 ===============================================

const PageEntry* GetFeedbackPages(int& outCount)
{
    static const PageEntry s_pages[] = {
        { _T("popup-host"),   _T("DuiPopupHost　浮层宿主"),   _T("DuiPopupHost"),   &Build_PopupHost,   true },
        { _T("tooltip"),      _T("DuiToolTip　工具提示"),     _T("DuiToolTip"),     &Build_ToolTip,     true },
        { _T("progress-bar"), _T("DuiProgressBar　进度条"),   _T("DuiProgressBar"), &Build_ProgressBar, true },
        { _T("emoji-panel"),  _T("DuiEmojiPanel　表情面板"),  _T("DuiEmojiPanel"),  &Build_Emoji,       true },
    };
    outCount = (int)(sizeof(s_pages) / sizeof(s_pages[0]));
    return s_pages;
}

} // namespace Gallery
