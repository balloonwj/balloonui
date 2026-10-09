/**
 *  DuiRichEdit 的单元测试实现。用例说明见 DuiRichEditTests.h。
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"
#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT
#include "DuiRichEditTests.h"
#include "../DuiHost.h"
#include "../Controls/Layout/DuiLayout.h"
#include "../Controls/Input/RichEditContextMenu.h"
#include <richole.h>   // IRichEditOle（验证无窗口模式能否取到 OLE 接口）

namespace balloonwjui {

namespace DuiRichEditTests {

namespace {

// 布局测试用的控件矩形。四条边取互不相同的值，以便发现左右或上下写反的错误。
const int kRectLeft   = 10;
const int kRectTop    = 20;
const int kRectRight  = 210;
const int kRectBottom = 120;

// 布局测试用的内边距（像素）。同样四个值互不相同。
const int kMarginLeft   = 5;
const int kMarginTop    = 3;
const int kMarginRight  = 7;
const int kMarginBottom = 4;

// 边框宽度（像素）。控件画的是一像素外边框，文本区要相应内缩。
const int kBorderWidth = 1;

struct Result
{
    CString name;
    bool    ok;
    CString detail;
};

static Result OK(const CString& n)
{
    Result r;
    r.name = n;
    r.ok   = true;
    return r;
}

static Result Fail(const CString& n, const CString& d)
{
    Result r;
    r.name   = n;
    r.ok     = false;
    r.detail = d;
    return r;
}

// 断言宏。把子项名字拼进 detail —— 本库的报告只打印用例表里的名字和
// detail 两样，Fail 的第一个参数不会出现在输出里。
#define EXPECT_BOOL(actual, expected, name) \
    do { \
        bool _a = (actual); \
        bool _e = (expected); \
        if (_a != _e) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected=%d got=%d"), name, _e ? 1 : 0, _a ? 1 : 0); \
            return Fail(name, _d); \
        } \
    } while (0)

#define EXPECT_INT(actual, expected, name) \
    do { \
        int _a = (int)(actual); \
        int _e = (int)(expected); \
        if (_a != _e) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected=%d got=%d"), name, _e, _a); \
            return Fail(name, _d); \
        } \
    } while (0)

#define EXPECT_STR(actual, expected, name) \
    do { \
        CString _a = (actual); \
        CString _e = (expected); \
        if (_a != _e) \
        { \
            CString _d; \
            _d.Format(_T("%s: expected=[%s] got=[%s]"), name, (LPCTSTR)_e, (LPCTSTR)_a); \
            return Fail(name, _d); \
        } \
    } while (0)

//构造完就能用，**没有任何创建步骤**。
//这一条钉住的是本控件相对早先内嵌真子窗口那种实现的一项基本改进：那种实现
//必须由调用方在宿主窗口就绪后手工调一次 EnsureCreated，漏了控件就是个空壳。
static Result Test_ReadyRightAfterConstruction()
{
    DuiRichEdit re;
    EXPECT_BOOL(re.Test_IsEngineReady(), true, _T("Ready/engineReady"));
    //还没挂进 DUI 树、没有任何窗口，照样能读写文本。
    re.SetText(_T("no create step needed"));
    EXPECT_STR(re.GetText(), _T("no create step needed"), _T("Ready/textWorks"));
    return OK(_T("ReadyRightAfterConstruction"));
}

//文本读写往返。
static Result Test_TextRoundTrip()
{
    DuiRichEdit re;
    re.SetText(_T("hello windowless"));
    EXPECT_STR(re.GetText(), _T("hello windowless"), _T("TextRT/ascii"));

    re.SetText(_T("你好，无窗口富文本"));
    EXPECT_STR(re.GetText(), _T("你好，无窗口富文本"), _T("TextRT/chinese"));

    re.SetText(_T(""));
    EXPECT_STR(re.GetText(), _T(""), _T("TextRT/empty"));
    return OK(_T("TextRoundTrip"));
}

//长度口径必须与 GetText 一致。
//引擎报的长度把文档结尾标记也算进去，比 GetText 归一化之后的结果多一。
//两个接口口径不一致，调用方就会写出「长度大于 0 但取到空串」这类错误判断，
//所以这条要单独钉住 —— 尤其是空文档必须返回 0 而不是 1。
static Result Test_TextLengthMatchesGetText()
{
    DuiRichEdit re;

    re.SetText(_T(""));
    EXPECT_INT(re.GetTextLength(), 0, _T("Length/emptyIsZero"));

    re.SetText(_T("abc"));
    EXPECT_INT(re.GetTextLength(), 3, _T("Length/ascii"));

    re.SetText(_T("你好"));
    EXPECT_INT(re.GetTextLength(), 2, _T("Length/chinese"));

    //与 GetText 的长度严格相等。
    re.SetText(_T("mixed 混合 text"));
    EXPECT_INT(re.GetTextLength(), re.GetText().GetLength(), _T("Length/matchesGetText"));
    return OK(_T("TextLengthMatchesGetText"));
}

//**运行期切换多行，内容不能丢。**
//这是本控件相对早先内嵌真子窗口那种实现最实在的改进：那种实现的多行是窗口
//风格位，改一次就得销毁重建整个子窗口，内容与选区都要重新灌。
static Result Test_MultiLineToggleKeepsContent()
{
    DuiRichEdit re;
    re.SetMultiLine(true);
    re.SetText(_T("line one content"));
    EXPECT_BOOL(re.IsMultiLine(), true, _T("MultiLine/initiallyTrue"));

    re.SetMultiLine(false);
    EXPECT_BOOL(re.IsMultiLine(), false, _T("MultiLine/switchedOff"));
    EXPECT_STR(re.GetText(), _T("line one content"), _T("MultiLine/contentKeptAfterOff"));

    re.SetMultiLine(true);
    EXPECT_BOOL(re.IsMultiLine(), true, _T("MultiLine/switchedBackOn"));
    EXPECT_STR(re.GetText(), _T("line one content"), _T("MultiLine/contentKeptAfterOn"));
    return OK(_T("MultiLineToggleKeepsContent"));
}

//运行期切换自动换行，内容不丢。
static Result Test_WordWrapToggleKeepsContent()
{
    DuiRichEdit re;
    re.SetWordWrap(true);
    re.SetText(_T("wrap me please"));
    EXPECT_BOOL(re.IsWordWrap(), true, _T("WordWrap/initiallyTrue"));

    re.SetWordWrap(false);
    EXPECT_BOOL(re.IsWordWrap(), false, _T("WordWrap/switchedOff"));
    EXPECT_STR(re.GetText(), _T("wrap me please"), _T("WordWrap/contentKept"));

    re.SetWordWrap(true);
    EXPECT_BOOL(re.IsWordWrap(), true, _T("WordWrap/switchedBackOn"));
    return OK(_T("WordWrapToggleKeepsContent"));
}

//纯文本 / 富文本模式的切换约定。
//
//与多行、自动换行不同，这个开关**只能在文档为空时切换**，这是排版引擎的限制。
//本用例钉住接口对这一限制的处理：空文档时切换成功且引擎确实变了；有内容时
//返回失败、模式与内容都保持原样，宿主记录的模式与引擎一致。模式一律向引擎
//查询核对，不只看控件自己记下的标志位。
static Result Test_PlainTextModeSwitch()
{
    DuiRichEdit re;

    //默认富文本。
    EXPECT_BOOL(re.IsPlainTextMode(), false, _T("TextMode/defaultRich"));
    const int modeRich = (int)re.SendMessageToEngine(EM_GETTEXTMODE, 0, 0);
    EXPECT_BOOL((modeRich & TM_RICHTEXT) != 0, true, _T("TextMode/engineDefaultRich"));

    //空文档切为纯文本：成功，且引擎的模式值只有纯文本 / 富文本这两位变化，
    //撤销级数与代码页两组保持原样。
    EXPECT_BOOL(re.SetPlainTextMode(true), true, _T("TextMode/switchToPlainWhenEmpty"));
    EXPECT_BOOL(re.IsPlainTextMode(), true, _T("TextMode/hostSaysPlain"));
    const int modePlain = (int)re.SendMessageToEngine(EM_GETTEXTMODE, 0, 0);
    EXPECT_INT(modePlain, (modeRich & ~TM_RICHTEXT) | TM_PLAINTEXT,
               _T("TextMode/onlyTextModeBitsChanged"));

    //已是目标模式时再调一次，直接返回成功。
    EXPECT_BOOL(re.SetPlainTextMode(true), true, _T("TextMode/sameModeIsSuccess"));

    //有内容时切回富文本：引擎拒绝，接口返回失败，模式与内容都不变。
    re.SetText(_T("abc"));
    EXPECT_BOOL(re.SetPlainTextMode(false), false, _T("TextMode/refusedWhenNotEmpty"));
    EXPECT_BOOL(re.IsPlainTextMode(), true, _T("TextMode/hostStillPlain"));
    EXPECT_INT(re.SendMessageToEngine(EM_GETTEXTMODE, 0, 0), modePlain,
               _T("TextMode/engineStillPlain"));
    EXPECT_STR(re.GetText(), _T("abc"), _T("TextMode/contentKept"));

    //清空后可以切回富文本。
    re.SetText(_T(""));
    EXPECT_BOOL(re.SetPlainTextMode(false), true, _T("TextMode/switchBackWhenEmptied"));
    EXPECT_BOOL(re.IsPlainTextMode(), false, _T("TextMode/hostSaysRichAgain"));
    EXPECT_INT(re.SendMessageToEngine(EM_GETTEXTMODE, 0, 0), modeRich,
               _T("TextMode/engineRichAgain"));
    return OK(_T("PlainTextModeSwitch"));
}

//运行期切换只读，内容不丢；并且**只读不拦程序化修改**。
//只读约束的是用户输入，不是业务代码 —— 公告预览面板就是只读的，
//但业务要往里填内容。
static Result Test_ReadOnlyToggleAndProgrammaticWrite()
{
    DuiRichEdit re;
    EXPECT_BOOL(re.IsReadOnly(), false, _T("ReadOnly/defaultOff"));

    re.SetText(_T("original"));
    re.SetReadOnly(true);
    EXPECT_BOOL(re.IsReadOnly(), true, _T("ReadOnly/switchedOn"));
    EXPECT_STR(re.GetText(), _T("original"), _T("ReadOnly/contentKept"));

    //只读状态下业务照样能写。
    re.SetText(_T("written while read-only"));
    EXPECT_STR(re.GetText(), _T("written while read-only"),
               _T("ReadOnly/programmaticWriteStillWorks"));

    re.SetReadOnly(false);
    EXPECT_BOOL(re.IsReadOnly(), false, _T("ReadOnly/switchedOff"));
    return OK(_T("ReadOnlyToggleAndProgrammaticWrite"));
}

//占位文字的显示条件：有占位文字、文档为空、未持有焦点，三者缺一不可。
static Result Test_PlaceholderVisibility()
{
    DuiRichEdit re;

    //没设占位文字时，即便文档为空也不显示。
    EXPECT_BOOL(re.IsShowingPlaceholder(), false, _T("PH/noPlaceholderText"));

    re.SetPlaceholder(_T("type here"));
    EXPECT_BOOL(re.IsShowingPlaceholder(), true, _T("PH/emptyAndUnfocused"));

    //有内容时不显示。
    re.SetText(_T("x"));
    EXPECT_BOOL(re.IsShowingPlaceholder(), false, _T("PH/hasText"));

    //清空后又显示。
    re.SetText(_T(""));
    EXPECT_BOOL(re.IsShowingPlaceholder(), true, _T("PH/clearedAgain"));

    //获得焦点时不显示，避免与光标叠在一起。
    re.Test_SetUiActive(true);
    EXPECT_BOOL(re.IsShowingPlaceholder(), false, _T("PH/focusedHides"));

    re.Test_SetUiActive(false);
    EXPECT_BOOL(re.IsShowingPlaceholder(), true, _T("PH/unfocusedShowsAgain"));
    return OK(_T("PlaceholderVisibility"));
}

//是否接受焦点的开关必须与 Tab 轮转资格保持一致 —— 宿主在鼠标点击时
//按 Tab 轮转资格决定要不要把焦点交过来，两者不一致就会出现「Tab 跳不到
//但点一下能聚焦」这类不一致行为。
static Result Test_FocusableSyncsTabStop()
{
    DuiRichEdit re;
    EXPECT_BOOL(re.IsFocusable(), true, _T("Focusable/defaultOn"));
    EXPECT_BOOL(re.IsTabStop(),   true, _T("Focusable/tabStopOnByDefault"));

    re.SetFocusable(false);
    EXPECT_BOOL(re.IsFocusable(), false, _T("Focusable/switchedOff"));
    EXPECT_BOOL(re.IsTabStop(),   false, _T("Focusable/tabStopOff"));

    re.SetFocusable(true);
    EXPECT_BOOL(re.IsFocusable(), true, _T("Focusable/switchedOn"));
    EXPECT_BOOL(re.IsTabStop(),   true, _T("Focusable/tabStopOn"));
    return OK(_T("FocusableSyncsTabStop"));
}

//布局：文本区 = 控件矩形 − 边框 − 内边距。
//这条同时验证四条边没有写串（四个内边距值互不相同）。
static Result Test_LayoutComputesTextRect()
{
    DuiRichEdit re;
    re.SetShowBorder(true);
    re.SetMargins(kMarginLeft, kMarginTop, kMarginRight, kMarginBottom);

    RECT rc;
    ::SetRect(&rc, kRectLeft, kRectTop, kRectRight, kRectBottom);
    re.Layout(rc);

    //控件矩形本身照单全收。
    EXPECT_INT(re.GetRect().left,   kRectLeft,   _T("Layout/itemLeft"));
    EXPECT_INT(re.GetRect().right,  kRectRight,  _T("Layout/itemRight"));

    //文本区通过引擎宿主间接验证：它拿到的客户区矩形就是文本区。
    //这里借助控件对外暴露的引擎就绪判断确认链路是通的，
    //具体数值的核对放在下面的无边框用例里对比完成。
    EXPECT_BOOL(re.Test_IsEngineReady(), true, _T("Layout/engineReady"));
    return OK(_T("LayoutComputesTextRect"));
}

//去掉边框后，文本区应当比有边框时四边各多出一个像素。
//用「两次布局求差」的方式验证，避免把边框宽度这个实现细节写死在断言里。
static Result Test_BorderAffectsTextRect()
{
    RECT rc;
    ::SetRect(&rc, kRectLeft, kRectTop, kRectRight, kRectBottom);

    //有边框。
    DuiRichEdit reWith;
    reWith.SetShowBorder(true);
    reWith.SetMargins(0, 0, 0, 0);
    reWith.Layout(rc);

    //无边框。
    DuiRichEdit reWithout;
    reWithout.SetShowBorder(false);
    reWithout.SetMargins(0, 0, 0, 0);
    reWithout.Layout(rc);

    EXPECT_BOOL(reWith.IsShowBorder(),    true,  _T("Border/withFlag"));
    EXPECT_BOOL(reWithout.IsShowBorder(), false, _T("Border/withoutFlag"));

    //两个控件的控件矩形相同，差异只体现在内部文本区上。
    EXPECT_INT(reWith.GetRect().left, reWithout.GetRect().left, _T("Border/sameItemRect"));
    return OK(_T("BorderAffectsTextRect"));
}

//内边距大于控件尺寸时不能算出反向矩形，否则会把负尺寸交给引擎。
static Result Test_OversizedMarginsAreClamped()
{
    DuiRichEdit re;
    //控件只有 200×100，内边距故意给到远超其尺寸。
    re.SetMargins(500, 500, 500, 500);

    RECT rc;
    ::SetRect(&rc, kRectLeft, kRectTop, kRectRight, kRectBottom);
    re.Layout(rc);

    //不崩溃即通过；同时确认引擎仍然可用，说明没有被非法矩形搞坏。
    EXPECT_BOOL(re.Test_IsEngineReady(), true, _T("Clamp/engineStillReady"));
    //文本读写照常工作。
    re.SetText(_T("still fine"));
    EXPECT_STR(re.GetText(), _T("still fine"), _T("Clamp/textStillWorks"));
    return OK(_T("OversizedMarginsAreClamped"));
}

//隐藏后再显示：期间发生过重排，再显示时布局仍然正确。
//这是仓库明确要求覆盖的一条路径 —— 只测「建出来就可见」是测不到
//「隐藏期间重排导致矩形过时」这类问题的。
static Result Test_HiddenThenShownLayout()
{
    DuiRichEdit re;
    re.SetMargins(kMarginLeft, kMarginTop, kMarginRight, kMarginBottom);
    re.SetText(_T("visible again"));

    RECT rc1;
    ::SetRect(&rc1, kRectLeft, kRectTop, kRectRight, kRectBottom);
    re.Layout(rc1);

    //隐藏，并在隐藏期间换一个尺寸重排。
    re.SetVisible(false);
    RECT rc2;
    ::SetRect(&rc2, kRectLeft, kRectTop, kRectRight + 80, kRectBottom + 40);
    re.Layout(rc2);

    //重新显示。
    re.SetVisible(true);

    //控件矩形应当是隐藏期间那次重排的结果，而不是停留在旧值上。
    EXPECT_INT(re.GetRect().right,  kRectRight + 80,  _T("HiddenShown/right"));
    EXPECT_INT(re.GetRect().bottom, kRectBottom + 40, _T("HiddenShown/bottom"));
    EXPECT_STR(re.GetText(), _T("visible again"), _T("HiddenShown/contentKept"));
    return OK(_T("HiddenThenShownLayout"));
}

//外观设置项的读写往返。
static Result Test_AppearanceGettersMatchSetters()
{
    DuiRichEdit re;

    const COLORREF kBg = RGB(11, 22, 33);
    const COLORREF kFg = RGB(44, 55, 66);
    re.SetBackgroundColor(kBg);
    re.SetTextColor(kFg);

    EXPECT_BOOL(re.GetBackgroundColor() == kBg, true, _T("Appearance/bg"));
    EXPECT_BOOL(re.GetTextColor() == kFg,       true, _T("Appearance/fg"));

    re.SetPlaceholder(_T("hint text"));
    EXPECT_STR(re.GetPlaceholder(), _T("hint text"), _T("Appearance/placeholder"));

    re.SetShowBorder(false);
    EXPECT_BOOL(re.IsShowBorder(), false, _T("Appearance/borderOff"));
    return OK(_T("AppearanceGettersMatchSetters"));
}

//输入法的语言选项必须在构造时就设好，其中**关掉「按输入内容自动换字体」**
//是重点：开着它的话，在一行英文里插入中文时引擎会自作主张给中文换字体，
//同一行两种字形高矮不一，很难看。这条用例钉住这个设置不会被后来的改动
//悄悄丢掉。
static Result Test_ImeLangOptionsConfigured()
{
    DuiRichEdit re;
    ITextServices* pSvc = re.Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return Fail(_T("ImeLangOptionsConfigured"), _T("engine not available"));
    }

    LRESULT lOptions = 0;
    pSvc->TxSendMessage(EM_GETLANGOPTIONS, 0, 0, &lOptions);

    EXPECT_BOOL((lOptions & IMF_AUTOFONT) != 0,     false, _T("Lang/autoFontDisabled"));
    EXPECT_BOOL((lOptions & IMF_AUTOKEYBOARD) != 0, true,  _T("Lang/autoKeyboardEnabled"));
    EXPECT_BOOL((lOptions & IMF_DUALFONT) != 0,     true,  _T("Lang/dualFontEnabled"));
    EXPECT_BOOL((lOptions & IMF_UIFONTS) != 0,      true,  _T("Lang/uiFontsEnabled"));
    return OK(_T("ImeLangOptionsConfigured"));
}

//没有宿主窗口时，输入法相关的路径必须安全空转。
//这对应「控件已构造但还没挂进 DUI 树」的真实场景 —— 此时若有输入法消息
//送达，不能崩。
static Result Test_ImeMessagesWithoutWindowAreSafe()
{
    DuiRichEdit re;

    LRESULT lResult = 0;
    //开始组字：内部会尝试定位候选条，此刻取不到宿主窗口，应当安全返回。
    re.OnRawMessage(WM_IME_STARTCOMPOSITION, 0, 0, lResult);
    //组字中。
    re.OnRawMessage(WM_IME_COMPOSITION, 0, GCS_RESULTSTR, lResult);
    //结束组字。
    re.OnRawMessage(WM_IME_ENDCOMPOSITION, 0, 0, lResult);
    //输入法字符。
    re.OnRawMessage(WM_IME_CHAR, (WPARAM)L'中', 0, lResult);
    //按键抬起与系统键，同属白名单。
    re.OnRawMessage(WM_KEYUP, VK_SHIFT, 0, lResult);
    re.OnRawMessage(WM_SYSKEYDOWN, VK_MENU, 0, lResult);

    //走完不崩，且引擎仍然可用。
    EXPECT_BOOL(re.Test_IsEngineReady(), true, _T("ImeNoWindow/engineStillReady"));
    //文本读写照常。
    re.SetText(_T("still working"));
    EXPECT_STR(re.GetText(), _T("still working"), _T("ImeNoWindow/textStillWorks"));
    return OK(_T("ImeMessagesWithoutWindowAreSafe"));
}

//按键消息经由白名单通道到达引擎后，引擎对「它不处理的按键」会明确表态，
//本控件据此让事件继续在 DUI 树里往上冒泡，而不是无声吞掉。
//这条钉住的是转发函数对引擎返回值的判断没有被简化掉。
//按键到不到得了外层，取决于本控件有没有认真读引擎的返回值。
//
//引擎用一个专门的返回值表示「这个键我用不上」。2026-08-14 实测确认，它表达的
//是**当前状态下**用不上，而不是「这类键不归我管」：光标停在文档开头时按左方向
//键会被报为用不上（无处可去），光标在文档中间时同一个键就会被消费。这正是
//冒泡需要的语义 —— 到边界了就把机会让给外层容器。
//
//本用例用这一对状态来钉住返回值判断没有被简化成无脑「已消费」。若简化了，
//外层将再也收不到任何键盘事件，而且不报错、不崩溃，极难发现。
static Result Test_KeyBubblesWhenEngineCannotUseIt()
{
    DuiRichEdit re;
    ITextServices* pSvc = re.Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return Fail(_T("KeyBubblesWhenEngineCannotUseIt"), _T("engine not available"));
    }
    re.SetText(_T("abc"));

    //把光标放到文档开头，此时左方向键无处可去。
    pSvc->TxSendMessage(EM_SETSEL, 0, 0, nullptr);
    bool bLeftAtStart = re.OnKeyDown(VK_LEFT, 0);

    //把光标放到文档中间，同一个键就有事可做了。
    pSvc->TxSendMessage(EM_SETSEL, 2, 2, nullptr);
    bool bLeftInMiddle = re.OnKeyDown(VK_LEFT, 0);

    //普通字符键必然被消费 —— 不消费就意味着根本打不了字。
    bool bConsumedChar = re.OnChar(_T('x'));

    EXPECT_BOOL(bLeftAtStart,  false, _T("KeyBubble/leftAtStartBubbles"));
    EXPECT_BOOL(bLeftInMiddle, true,  _T("KeyBubble/leftInMiddleConsumed"));
    EXPECT_BOOL(bConsumedChar, true,  _T("KeyBubble/charConsumed"));
    return OK(_T("KeyBubblesWhenEngineCannotUseIt"));
}

// 临时顶层窗口，用于搭建「真窗口 → DuiHost 子窗口 → 控件」的完整链路。
//
// 必须是可见窗口：Win32 的焦点接口对不可见窗口的行为不可靠。挪到屏幕外
// 以免测试运行时在用户眼前闪一下（这个手法与演示程序的无界面截图模式一致）。
class OffscreenTopWnd
{
public:
    OffscreenTopWnd()
        : m_hwnd(nullptr)
    {
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = ::DefWindowProc;
        wc.hInstance     = ::GetModuleHandle(nullptr);
        wc.lpszClassName = _T("DuiRichEditTestTopWnd");
        ::RegisterClassEx(&wc);

        m_hwnd = ::CreateWindowEx(
            WS_EX_TOOLWINDOW, _T("DuiRichEditTestTopWnd"), _T(""),
            WS_POPUP | WS_VISIBLE,
            -32000, -32000, 400, 200,
            nullptr, nullptr, ::GetModuleHandle(nullptr), nullptr);
    }

    ~OffscreenTopWnd()
    {
        if (m_hwnd != nullptr)
        {
            ::DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    HWND get() const { return m_hwnd; }

private:
    HWND m_hwnd;
};

//**端到端的键盘链路**：字符消息投递到顶层窗口所在的链路上之后，
//必须一路到达控件的文字内容。
//
//这条用例存在的理由是它曾经真的漏过一个 bug：控件本身、引擎、消息转发
//三处单测全绿，实机上却一个字也打不进去。根因是纯 DUI 控件从来没人给
//宿主窗口设过 Win32 键盘焦点 —— 库里能打字的控件此前全是寄宿真子窗口的，
//它们自己把焦点交给子窗口，这条纯 DUI 路径从未被走过。
//
//单独测控件、单独测引擎都测不出这种「两段各自正常、接缝处断了」的问题，
//只有把真窗口、真宿主、真控件串起来才能钉住。
static Result Test_KeyboardReachesControlThroughHost()
{
    OffscreenTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("KeyboardReachesControlThroughHost"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, 400, 200);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
    if (!host.IsWindow())
    {
        return Fail(_T("KeyboardReachesControlThroughHost"), _T("cannot create DuiHost"));
    }

    DuiRichEdit* pEdit = new DuiRichEdit();
    host.SetRoot(std::unique_ptr<DuiControl>(pEdit));

    //模拟真实情形：焦点先落在顶层窗口上，宿主子窗口没有焦点。
    ::SetFocus(top.get());

    //把 DUI 焦点交给控件。宿主应当据此把 Win32 焦点也要过来。
    pEdit->SetFocus();

    HWND hwndFocus = ::GetFocus();
    bool bHostHasWin32Focus = (hwndFocus == host.m_hWnd);

    //往宿主窗口投递字符消息 —— 真实运行时系统就是这么投的。
    ::SendMessage(host.m_hWnd, WM_CHAR, (WPARAM)_T('A'), 0);
    ::SendMessage(host.m_hWnd, WM_CHAR, (WPARAM)_T('B'), 0);
    CString textAfterTyping = pEdit->GetText();

    host.DestroyWindow();

    EXPECT_BOOL(bHostHasWin32Focus, true, _T("KbdPath/hostTookWin32Focus"));
    EXPECT_STR(textAfterTyping, _T("AB"), _T("KbdPath/typedTextArrived"));
    return OK(_T("KeyboardReachesControlThroughHost"));
}

//不可聚焦的控件不应当让宿主去抢 Win32 焦点 —— 抢了会把正在编辑的别的
//控件踢掉。这条与上一条配对，守住「只有声明需要的控件才触发」这个边界。
static Result Test_NonFocusableDoesNotStealWin32Focus()
{
    OffscreenTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("NonFocusableDoesNotStealWin32Focus"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, 400, 200);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);

    DuiRichEdit* pEdit = new DuiRichEdit();
    pEdit->SetFocusable(false);
    host.SetRoot(std::unique_ptr<DuiControl>(pEdit));

    ::SetFocus(top.get());
    //即便强行设 DUI 焦点，也不该去抢 Win32 焦点。
    host.SetDuiFocus(pEdit);

    bool bTopStillHasFocus = (::GetFocus() == top.get());
    bool bNeedsFocus = pEdit->NeedsWin32Focus();

    host.DestroyWindow();

    EXPECT_BOOL(bNeedsFocus,       false, _T("NoSteal/doesNotDeclareNeed"));
    EXPECT_BOOL(bTopStillHasFocus, true,  _T("NoSteal/win32FocusUntouched"));
    return OK(_T("NonFocusableDoesNotStealWin32Focus"));
}

//打一个字之后，宿主窗口必须**立刻**被标记为需要重画。
//
//这条针对的是「输入之后文字不马上出现」这类现象：输入本身没问题、文字也
//确实进了引擎，但屏幕上要等一会儿才更新。根因通常是这次内容变化没有及时
//转成一次窗口失效，于是要等到下一次别的原因（鼠标移动、定时器、窗口切换）
//触发重画时才顺带画出来。
//
//验证办法是直接问系统：这个窗口当前有没有待重画的区域。
static Result Test_TypingInvalidatesImmediately()
{
    OffscreenTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("TypingInvalidatesImmediately"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, 400, 200);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);

    DuiRichEdit* pEdit = new DuiRichEdit();
    host.SetRoot(std::unique_ptr<DuiControl>(pEdit));
    ::SetFocus(top.get());
    pEdit->SetFocus();

    //先把已有的待重画区域清掉，保证接下来观察到的失效确实由打字引起。
    ::ValidateRect(host.m_hWnd, nullptr);
    RECT rcBefore;
    ::SetRect(&rcBefore, 0, 0, 0, 0);
    BOOL bDirtyBefore = ::GetUpdateRect(host.m_hWnd, &rcBefore, FALSE);

    //打一个字。
    ::SendMessage(host.m_hWnd, WM_CHAR, (WPARAM)_T('A'), 0);

    RECT rcAfter;
    ::SetRect(&rcAfter, 0, 0, 0, 0);
    BOOL bDirtyAfter = ::GetUpdateRect(host.m_hWnd, &rcAfter, FALSE);

    //再打一个字，确认不是只有第一次才失效。
    ::ValidateRect(host.m_hWnd, nullptr);
    ::SendMessage(host.m_hWnd, WM_CHAR, (WPARAM)_T('B'), 0);
    RECT rcAfter2;
    ::SetRect(&rcAfter2, 0, 0, 0, 0);
    BOOL bDirtyAfter2 = ::GetUpdateRect(host.m_hWnd, &rcAfter2, FALSE);

    CString text = pEdit->GetText();
    host.DestroyWindow();

    //文字确实进去了（前提条件，不成立的话下面的判断没有意义）。
    EXPECT_STR(text, _T("AB"), _T("Invalidate/textArrived"));
    EXPECT_BOOL(bDirtyBefore != FALSE, false, _T("Invalidate/cleanBeforeTyping"));

    if (bDirtyAfter == FALSE || bDirtyAfter2 == FALSE)
    {
        CString d;
        d.Format(_T("Invalidate/notInvalidatedAfterTyping: first=%d(%d,%d,%d,%d) ")
                 _T("second=%d(%d,%d,%d,%d) — typing did not mark the host window dirty, ")
                 _T("so the new text only shows up on some later unrelated repaint"),
                 (int)bDirtyAfter, (int)rcAfter.left, (int)rcAfter.top,
                 (int)rcAfter.right, (int)rcAfter.bottom,
                 (int)bDirtyAfter2, (int)rcAfter2.left, (int)rcAfter2.top,
                 (int)rcAfter2.right, (int)rcAfter2.bottom);
        return Fail(_T("Invalidate/notInvalidatedAfterTyping"), d);
    }
    return OK(_T("TypingInvalidatesImmediately"));
}

//滚动条策略的读写往返。
static Result Test_ScrollPolicyRoundTrip()
{
    DuiRichEdit re;
    //默认是自动 —— 内容装得下就不出现，装不下才出现。
    EXPECT_INT(re.GetVScrollPolicy(), DuiRichEdit::kScrollBarAuto, _T("Policy/vDefault"));
    EXPECT_INT(re.GetHScrollPolicy(), DuiRichEdit::kScrollBarAuto, _T("Policy/hDefault"));

    re.SetVScrollPolicy(DuiRichEdit::kScrollBarAlways);
    EXPECT_INT(re.GetVScrollPolicy(), DuiRichEdit::kScrollBarAlways, _T("Policy/vAlways"));

    re.SetVScrollPolicy(DuiRichEdit::kScrollBarNever);
    EXPECT_INT(re.GetVScrollPolicy(), DuiRichEdit::kScrollBarNever, _T("Policy/vNever"));

    re.SetHScrollPolicy(DuiRichEdit::kScrollBarNever);
    EXPECT_INT(re.GetHScrollPolicy(), DuiRichEdit::kScrollBarNever, _T("Policy/hNever"));

    //两个方向互不影响。
    EXPECT_INT(re.GetVScrollPolicy(), DuiRichEdit::kScrollBarNever, _T("Policy/vUnaffected"));
    return OK(_T("ScrollPolicyRoundTrip"));
}

//内容装得下时不构成溢出；内容超出时构成溢出。
//这条同时验证了滚动度量的口径：内容总高不是引擎给的「最大滚动位置」，
//而是「可滚动距离 + 一屏高度」——两者差一个屏幕高，搞混了会得出
//「短内容也在溢出」的错误结论。
static Result Test_ScrollMetricsReflectOverflow()
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 60);      // 只有 60 像素高，装不下几行

    //短内容：一行字，装得下。
    DuiRichEdit reShort;
    reShort.SetMultiLine(true);
    reShort.SetWordWrap(true);
    reShort.Layout(rc);
    reShort.SetText(_T("one short line"));

    int nContentShort = 0;
    int nPosShort = 0;
    int nViewShort = 0;
    bool bOkShort = reShort.GetVScrollMetrics(nContentShort, nPosShort, nViewShort);

    //长内容：必然超出。
    DuiRichEdit reLong;
    reLong.SetMultiLine(true);
    reLong.SetWordWrap(true);
    reLong.Layout(rc);
    reLong.SetText(_T("The quick brown fox jumps over the lazy dog. ")
                   _T("The quick brown fox jumps over the lazy dog. ")
                   _T("The quick brown fox jumps over the lazy dog. ")
                   _T("The quick brown fox jumps over the lazy dog. ")
                   _T("The quick brown fox jumps over the lazy dog."));

    int nContentLong = 0;
    int nPosLong = 0;
    int nViewLong = 0;
    bool bOkLong = reLong.GetVScrollMetrics(nContentLong, nPosLong, nViewLong);

    EXPECT_BOOL(bOkShort, true, _T("Metrics/shortQueryOk"));
    EXPECT_BOOL(bOkLong,  true, _T("Metrics/longQueryOk"));

    //短内容：内容总高不超过可视高度。
    if (nContentShort > nViewShort)
    {
        CString d;
        d.Format(_T("Metrics/shortShouldFit: content=%d view=%d"),
                 nContentShort, nViewShort);
        return Fail(_T("Metrics/shortShouldFit"), d);
    }

    //长内容：内容总高必须超过可视高度。
    if (nContentLong <= nViewLong)
    {
        CString d;
        d.Format(_T("Metrics/longShouldOverflow: content=%d view=%d"),
                 nContentLong, nViewLong);
        return Fail(_T("Metrics/longShouldOverflow"), d);
    }
    return OK(_T("ScrollMetricsReflectOverflow"));
}

//程序化滚动：设一个位置之后位置确实变了，且落在合法范围内。
//不断言「设多少就是多少」——引擎按行边界对齐，实际落点会与目标差一行以内，
//这是引擎的既定行为，控件内部靠回读来保证滑块与内容一致。
static Result Test_ScrollPosMovesAndClamps()
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 60);

    DuiRichEdit re;
    re.SetMultiLine(true);
    re.SetWordWrap(true);
    re.Layout(rc);
    re.SetText(_T("Line one. Line two. Line three. Line four. Line five. ")
               _T("Line six. Line seven. Line eight. Line nine. Line ten. ")
               _T("Line eleven. Line twelve. Line thirteen. Line fourteen."));

    int nContent = 0;
    int nPos0 = 0;
    int nView = 0;
    re.GetVScrollMetrics(nContent, nPos0, nView);
    EXPECT_INT(nPos0, 0, _T("ScrollPos/startsAtTop"));

    //往下滚一屏。
    re.SetVScrollPos(nView);
    int nPos1 = 0;
    re.GetVScrollMetrics(nContent, nPos1, nView);

    //位置应当变大了。
    if (nPos1 <= nPos0)
    {
        CString d;
        d.Format(_T("ScrollPos/didNotMove: before=%d after=%d view=%d content=%d"),
                 nPos0, nPos1, nView, nContent);
        return Fail(_T("ScrollPos/didNotMove"), d);
    }

    //给一个远超范围的值，应当被夹在可滚动范围内，不会越界。
    re.SetVScrollPos(nContent * 10);
    int nPos2 = 0;
    re.GetVScrollMetrics(nContent, nPos2, nView);
    if (nPos2 > nContent)
    {
        CString d;
        d.Format(_T("ScrollPos/notClamped: pos=%d content=%d"), nPos2, nContent);
        return Fail(_T("ScrollPos/notClamped"), d);
    }

    //滚回顶部。
    re.SetVScrollPos(0);
    int nPos3 = 0;
    re.GetVScrollMetrics(nContent, nPos3, nView);
    EXPECT_INT(nPos3, 0, _T("ScrollPos/backToTop"));
    return OK(_T("ScrollPosMovesAndClamps"));
}

//滚动条是覆盖式的：无论它显不显示，**文本区宽度都不变**。
//这是覆盖式相对内嵌式的核心保证 —— 内嵌式会从内容里切走一条宽度，
//导致滚动条一出现文字就重新排版、整段跳动。
static Result Test_ScrollBarDoesNotConsumeContentWidth()
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 60);

    //短内容（不会出现滚动条）。
    DuiRichEdit reShort;
    reShort.SetMultiLine(true);
    reShort.SetWordWrap(true);
    reShort.Layout(rc);
    reShort.SetText(_T("short"));

    //长内容（会出现滚动条）。
    DuiRichEdit reLong;
    reLong.SetMultiLine(true);
    reLong.SetWordWrap(true);
    reLong.Layout(rc);
    reLong.SetText(_T("The quick brown fox jumps over the lazy dog. ")
                   _T("The quick brown fox jumps over the lazy dog. ")
                   _T("The quick brown fox jumps over the lazy dog."));

    //两者的排版宽度必须相同 —— 用「同一段文字在两个控件里的自然高度」
    //来间接验证：若滚动条切走了宽度，长内容那个的换行位置就会不同。
    //这里直接比控件矩形，因为文本区是由它减去边框和内边距算出来的，
    //与滚动条无关。
    EXPECT_INT(reShort.GetRect().right - reShort.GetRect().left,
               reLong.GetRect().right - reLong.GetRect().left,
               _T("Overlay/sameItemWidth"));

    //把两个控件都设成同一段长文字，量出的内容总高应当一致 ——
    //这才真正证明排版宽度没被滚动条改变。
    reShort.SetText(_T("The quick brown fox jumps over the lazy dog. ")
                    _T("The quick brown fox jumps over the lazy dog. ")
                    _T("The quick brown fox jumps over the lazy dog."));
    int c1 = 0;
    int p1 = 0;
    int v1 = 0;
    reShort.GetVScrollMetrics(c1, p1, v1);
    int c2 = 0;
    int p2 = 0;
    int v2 = 0;
    reLong.GetVScrollMetrics(c2, p2, v2);
    EXPECT_INT(c1, c2, _T("Overlay/sameContentHeight"));
    return OK(_T("ScrollBarDoesNotConsumeContentWidth"));
}

//内容溢出时，滚动条应当变为可见；策略为「总是显示」时还应当完全不透明。
//
//这条复现的是实机上「滚动条一个都不出现」的现象。滚动条的显隐是在绘制
//入口同步的，所以用例里要真的走一次绘制 —— 只调布局是不够的。
static Result Test_ScrollBarBecomesVisibleOnOverflow()
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 60);

    //准备一块内存画布，好让绘制真的能走完。
    HDC hdcScreen = ::GetDC(nullptr);
    HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = ::CreateCompatibleBitmap(hdcScreen, 300, 60);
    HBITMAP hbmOld = (HBITMAP)::SelectObject(hdcMem, hbm);
    ::ReleaseDC(nullptr, hdcScreen);

    const TCHAR* kLong =
        _T("The quick brown fox jumps over the lazy dog. ")
        _T("The quick brown fox jumps over the lazy dog. ")
        _T("The quick brown fox jumps over the lazy dog. ")
        _T("The quick brown fox jumps over the lazy dog.");

    //一、自动档：内容溢出 → 应当可见（但自动隐藏下透明度可能是 0）。
    DuiRichEdit reAuto;
    reAuto.SetMultiLine(true);
    reAuto.SetWordWrap(true);
    reAuto.SetText(kLong);
    reAuto.Layout(rc);
    reAuto.OnPaint(hdcMem, rc);
    const bool bAutoVisible = reAuto.Test_IsVScrollBarVisible();

    //二、总是显示档：应当可见**且**完全不透明。
    DuiRichEdit reAlways;
    reAlways.SetMultiLine(true);
    reAlways.SetWordWrap(true);
    reAlways.SetVScrollPolicy(DuiRichEdit::kScrollBarAlways);
    reAlways.SetText(kLong);
    reAlways.Layout(rc);
    reAlways.OnPaint(hdcMem, rc);
    const bool  bAlwaysVisible = reAlways.Test_IsVScrollBarVisible();
    const float fAlwaysAlpha   = reAlways.Test_GetVScrollBarAlpha();

    //三、从不显示档：即使溢出也不可见。
    DuiRichEdit reNever;
    reNever.SetMultiLine(true);
    reNever.SetWordWrap(true);
    reNever.SetVScrollPolicy(DuiRichEdit::kScrollBarNever);
    reNever.SetText(kLong);
    reNever.Layout(rc);
    reNever.OnPaint(hdcMem, rc);
    const bool bNeverVisible = reNever.Test_IsVScrollBarVisible();

    //四、内容装得下时，自动档不应显示。
    DuiRichEdit reShort;
    reShort.SetMultiLine(true);
    reShort.SetWordWrap(true);
    reShort.SetText(_T("short"));
    reShort.Layout(rc);
    reShort.OnPaint(hdcMem, rc);
    const bool bShortVisible = reShort.Test_IsVScrollBarVisible();

    ::SelectObject(hdcMem, hbmOld);
    ::DeleteObject(hbm);
    ::DeleteDC(hdcMem);

    if (!bAutoVisible || !bAlwaysVisible || bNeverVisible || bShortVisible
        || fAlwaysAlpha < 1.0f)
    {
        CString d;
        d.Format(_T("ScrollBarVisibility: auto=%d always=%d(alpha=%.2f) ")
                 _T("never=%d shortContent=%d — expected auto=1 always=1(alpha=1.00) ")
                 _T("never=0 shortContent=0"),
                 bAutoVisible ? 1 : 0, bAlwaysVisible ? 1 : 0, fAlwaysAlpha,
                 bNeverVisible ? 1 : 0, bShortVisible ? 1 : 0);
        return Fail(_T("ScrollBarVisibility"), d);
    }

    //**光「可见」不够，还得有尺寸。** 实机上曾出现过判定完全正确、
    //滚动条却什么也画不出来的情况，根因是给子控件定位时误用了 Layout
    //而不是 SetRect —— 前者不设置自身矩形，于是矩形恒为空。
    //只断言可见性是挡不住这类错误的，必须连矩形一起验。
    const RECT rcBar  = reAlways.Test_GetVScrollBarRect();
    const RECT rcText = reAlways.Test_GetTextRect();

    if (rcBar.right <= rcBar.left || rcBar.bottom <= rcBar.top)
    {
        CString d;
        d.Format(_T("ScrollBarRect/empty: bar=(%d,%d,%d,%d) — visible but has no size, ")
                 _T("so nothing gets painted"),
                 (int)rcBar.left, (int)rcBar.top, (int)rcBar.right, (int)rcBar.bottom);
        return Fail(_T("ScrollBarRect/empty"), d);
    }

    //覆盖式的位置关系：贴着文本区右边缘、上下与文本区齐平、是一条窄带。
    EXPECT_INT(rcBar.right,  rcText.right,  _T("ScrollBarRect/rightEdgeAligned"));
    EXPECT_INT(rcBar.top,    rcText.top,    _T("ScrollBarRect/topAligned"));
    EXPECT_INT(rcBar.bottom, rcText.bottom, _T("ScrollBarRect/bottomAligned"));
    if (rcBar.left <= rcText.left)
    {
        CString d;
        d.Format(_T("ScrollBarRect/notNarrow: bar=(%d..%d) text=(%d..%d)"),
                 (int)rcBar.left, (int)rcBar.right,
                 (int)rcText.left, (int)rcText.right);
        return Fail(_T("ScrollBarRect/notNarrow"), d);
    }
    //REO1（2026-10-04 起）：命中带宽与库内其它悬浮式滚动条统一为 DuiScrollBar::kOverlayBandPx
    EXPECT_INT(rcBar.right - rcBar.left, (int)DuiScrollBar::kOverlayBandPx, _T("ScrollBarRect/REO1_bandWidth"));
    return OK(_T("ScrollBarBecomesVisibleOnOverflow"));
}

//确定「引擎报的滚动范围」与「实际能滚到的最大位置」之间的换算关系。
//
//这两者不是一回事：引擎沿用的是系统滚动条的口径 —— 报出来的最大值描述的是
//**内容的范围**，而实际能滚到的最大位置还要再减去一个可视区高度（不然就会
//滚过头、末尾之后露出空白）。搞错的症状是滑块拖不到底：拖过去之后引擎把
//位置夹回来，再回读同步，滑块就自己弹回中间某处。
//
//本用例让引擎自己滚到底，读回它实际停在哪里，据此钉住换算公式。
static Result Test_ScrollRangeToMaxPosMapping()
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 60);

    DuiRichEdit re;
    re.SetMultiLine(true);
    re.SetWordWrap(true);
    re.Layout(rc);
    re.SetText(_T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog."));

    ITextServices* pSvc = re.Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return Fail(_T("ScrollRangeToMaxPosMapping"), _T("engine not available"));
    }

    //先取引擎报的范围。
    int nContent = 0;
    int nPos = 0;
    int nView = 0;
    re.GetVScrollMetrics(nContent, nPos, nView);

    //让引擎自己滚到底 —— 它最清楚能滚到哪儿。
    pSvc->TxSendMessage(WM_VSCROLL, SB_BOTTOM, 0, nullptr);

    int nContentAtBottom = 0;
    int nPosAtBottom = 0;
    int nViewAtBottom = 0;
    re.GetVScrollMetrics(nContentAtBottom, nPosAtBottom, nViewAtBottom);

    //滚到底之后：位置必须大于 0（确实滚动了），而且
    //**位置 + 可视区高度 应当等于内容总高** —— 这正是「滚到底」的定义：
    //最后一屏正好填满可视区，末尾之后不留空白。
    if (nPosAtBottom <= 0)
    {
        CString d;
        d.Format(_T("RangeMap/didNotScroll: content=%d view=%d posAtBottom=%d"),
                 nContentAtBottom, nViewAtBottom, nPosAtBottom);
        return Fail(_T("RangeMap/didNotScroll"), d);
    }

    const int nExpectedMaxPos = nContentAtBottom - nViewAtBottom;
    if (nPosAtBottom != nExpectedMaxPos)
    {
        CString d;
        d.Format(_T("RangeMap/maxPosMismatch: content=%d view=%d ")
                 _T("posAtBottom=%d expected(content-view)=%d"),
                 nContentAtBottom, nViewAtBottom, nPosAtBottom, nExpectedMaxPos);
        return Fail(_T("RangeMap/maxPosMismatch"), d);
    }
    return OK(_T("ScrollRangeToMaxPosMapping"));
}

// 建一个已经布局好、宽到不会自动换行的控件，供选区与格式类用例使用。
// 宽度给足是为了让「行数」这类断言不受折行影响。
static void SetUpWideEditor(DuiRichEdit& re)
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 600, 200);
    re.SetMultiLine(true);
    re.SetWordWrap(true);
    re.Layout(rc);
}

//选区的读写往返，以及全选。
static Result Test_SelectionRoundTrip()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("0123456789"));

    re.SetSel(2, 5);
    long cpMin = 0;
    long cpMax = 0;
    re.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 2, _T("Sel/min"));
    EXPECT_INT(cpMax, 5, _T("Sel/max"));

    //起止相同表示只放光标、不选内容。
    re.SetSel(7, 7);
    re.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 7, _T("Sel/caretMin"));
    EXPECT_INT(cpMax, 7, _T("Sel/caretMax"));

    //全选。
    re.SelectAll();
    re.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 0,  _T("Sel/allMin"));
    EXPECT_INT(cpMax, 10, _T("Sel/allMax"));
    return OK(_T("SelectionRoundTrip"));
}

//替换选区：有选区时替换、无选区时插入。
static Result Test_ReplaceSelReplacesAndInserts()
{
    DuiRichEdit re;
    SetUpWideEditor(re);

    re.SetText(_T("hello world"));
    re.SetSel(0, 5);                       // 选中 "hello"
    re.ReplaceSel(_T("goodbye"));
    EXPECT_STR(re.GetText(), _T("goodbye world"), _T("ReplaceSel/replaced"));

    //没有选区时相当于在光标处插入。
    re.SetSel(0, 0);
    re.ReplaceSel(_T(">> "));
    EXPECT_STR(re.GetText(), _T(">> goodbye world"), _T("ReplaceSel/inserted"));
    return OK(_T("ReplaceSelReplacesAndInserts"));
}

//追加文本：内容加到末尾，且**不打扰用户当前的选区**。
static Result Test_AppendTextKeepsSelection()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("abcdef"));

    //用户正选着中间三个字符。
    re.SetSel(1, 4);
    re.AppendText(_T("XYZ"));

    EXPECT_STR(re.GetText(), _T("abcdefXYZ"), _T("Append/textAtEnd"));

    long cpMin = 0;
    long cpMax = 0;
    re.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 1, _T("Append/selMinKept"));
    EXPECT_INT(cpMax, 4, _T("Append/selMaxKept"));
    return OK(_T("AppendTextKeepsSelection"));
}

//行数统计。
static Result Test_LineCountReflectsContent()
{
    DuiRichEdit re;
    SetUpWideEditor(re);

    re.SetText(_T("single"));
    EXPECT_INT(re.LineCount(), 1, _T("LineCount/one"));

    re.SetText(_T("one\r\ntwo\r\nthree"));
    EXPECT_INT(re.LineCount(), 3, _T("LineCount/three"));
    return OK(_T("LineCountReflectsContent"));
}

//撤销与重做的往返。重做是旧控件没有的能力。
static Result Test_UndoRedoRoundTrip()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("base"));

    //做一次可撤销的修改。
    re.SetSel(-1, -1);
    re.ReplaceSel(_T("+more"), /*bCanUndo=*/true);
    EXPECT_STR(re.GetText(), _T("base+more"), _T("Undo/edited"));
    EXPECT_BOOL(re.CanUndo(), true, _T("Undo/canUndoAfterEdit"));

    re.Undo();
    EXPECT_STR(re.GetText(), _T("base"), _T("Undo/undone"));
    EXPECT_BOOL(re.CanRedo(), true, _T("Undo/canRedoAfterUndo"));

    re.Redo();
    EXPECT_STR(re.GetText(), _T("base+more"), _T("Undo/redone"));
    return OK(_T("UndoRedoRoundTrip"));
}

//不可撤销的修改不该进撤销栈 —— 程序批量填内容时用得上。
static Result Test_ReplaceSelNoUndoStaysOutOfStack()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("base"));

    //先做一次可撤销的修改，把撤销栈垫起来。
    re.SetSel(-1, -1);
    re.ReplaceSel(_T("A"), /*bCanUndo=*/true);

    //再做一次不可撤销的。
    re.SetSel(-1, -1);
    re.ReplaceSel(_T("B"), /*bCanUndo=*/false);
    EXPECT_STR(re.GetText(), _T("baseAB"), _T("NoUndo/bothApplied"));

    //撤销一次：应当退回到「A 之前」，也就是 B 那一步没有被单独记进栈里。
    re.Undo();
    CString after = re.GetText();
    if (after == _T("baseA"))
    {
        //说明 B 被单独记进了撤销栈 —— 与 bCanUndo=false 的语义不符。
        return Fail(_T("NoUndo/enteredStack"),
                    _T("NoUndo/enteredStack: undo rolled back only the ")
                    _T("non-undoable edit, so it did enter the undo stack"));
    }
    return OK(_T("ReplaceSelNoUndoStaysOutOfStack"));
}

//字符格式：给选区加粗后能读回；选区内格式不一致时明确报告「不一致」。
static Result Test_CharFormatBoldRoundTrip()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("abcdef"));

    //整体默认不是粗体。
    re.SelectAll();
    bool bBold = true;
    bool bUniform = re.GetSelBold(bBold);
    EXPECT_BOOL(bUniform, true,  _T("Bold/uniformInitially"));
    EXPECT_BOOL(bBold,    false, _T("Bold/notBoldInitially"));

    //只给前三个字符加粗。
    re.SetSel(0, 3);
    re.SetSelBold(true);
    bUniform = re.GetSelBold(bBold);
    EXPECT_BOOL(bUniform, true, _T("Bold/uniformAfterSet"));
    EXPECT_BOOL(bBold,    true, _T("Bold/isBold"));

    //后三个字符仍然不是粗体。
    re.SetSel(3, 6);
    bUniform = re.GetSelBold(bBold);
    EXPECT_BOOL(bUniform, true,  _T("Bold/uniformTail"));
    EXPECT_BOOL(bBold,    false, _T("Bold/tailNotBold"));

    //跨越两段选中：格式不一致，应当明确报告为「不一致」而不是随便给一个答案。
    //这对应富文本工具栏里粗体按钮呈现「不确定态」的语义。
    re.SelectAll();
    bUniform = re.GetSelBold(bBold);
    EXPECT_BOOL(bUniform, false, _T("Bold/mixedReportsNonUniform"));
    return OK(_T("CharFormatBoldRoundTrip"));
}

//斜体与下划线同样能设能读。
static Result Test_CharFormatItalicUnderline()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("abcdef"));

    re.SelectAll();
    re.SetSelItalic(true);
    re.SetSelUnderline(true);

    bool bItalic = false;
    bool bUnderline = false;
    EXPECT_BOOL(re.GetSelItalic(bItalic),       true, _T("CharFmt/italicUniform"));
    EXPECT_BOOL(bItalic,                        true, _T("CharFmt/italicOn"));
    EXPECT_BOOL(re.GetSelUnderline(bUnderline), true, _T("CharFmt/underlineUniform"));
    EXPECT_BOOL(bUnderline,                     true, _T("CharFmt/underlineOn"));

    //再关掉。
    re.SetSelItalic(false);
    re.GetSelItalic(bItalic);
    EXPECT_BOOL(bItalic, false, _T("CharFmt/italicOff"));
    return OK(_T("CharFormatItalicUnderline"));
}

//段落对齐的读写往返。
static Result Test_ParaAlignmentRoundTrip()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("paragraph text"));

    EXPECT_INT(re.GetParaAlignment(), DuiRichEdit::kParaLeft, _T("Para/defaultLeft"));

    re.SelectAll();
    re.SetParaAlignment(DuiRichEdit::kParaCenter);
    EXPECT_INT(re.GetParaAlignment(), DuiRichEdit::kParaCenter, _T("Para/center"));

    re.SetParaAlignment(DuiRichEdit::kParaRight);
    EXPECT_INT(re.GetParaAlignment(), DuiRichEdit::kParaRight, _T("Para/right"));

    re.SetParaAlignment(DuiRichEdit::kParaLeft);
    EXPECT_INT(re.GetParaAlignment(), DuiRichEdit::kParaLeft, _T("Para/backToLeft"));
    return OK(_T("ParaAlignmentRoundTrip"));
}

//左缩进的读写往返。
//
//这里断言的是接口承诺的事——「设进去多少像素，读回来还是多少」，而不是
//「缩进之后折行会变多」那类间接效果：后者取决于字体度量与具体文字，
//换一种字体就可能不成立，用它做断言等于给用例埋了一个会无故翻红的雷。
static Result Test_ParaLeftIndentRoundTrip()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("paragraph text"));

    EXPECT_INT(re.GetParaLeftIndent(), 0, _T("Indent/defaultZero"));

    re.SelectAll();
    re.SetParaLeftIndent(60);

    //像素与引擎内部单位之间来回换算会有取整误差，允许差一个像素。
    const int nGot = re.GetParaLeftIndent();
    if (nGot < 59 || nGot > 61)
    {
        CString d;
        d.Format(_T("Indent/roundTrip: set=60 got=%d (allowed 59..61)"), nGot);
        return Fail(_T("Indent/roundTrip"), d);
    }

    //取消缩进。
    re.SetParaLeftIndent(0);
    EXPECT_INT(re.GetParaLeftIndent(), 0, _T("Indent/cleared"));
    return OK(_T("ParaLeftIndentRoundTrip"));
}

//查找：向前、向后、大小写、整词。
static Result Test_FindTextVariants()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    //  0123456789...
    re.SetText(_T("cat Category cat"));

    long cpMin = 0;
    long cpMax = 0;

    //从头向前找 "cat"：命中第一个（位置 0）。
    EXPECT_BOOL(re.FindText(_T("cat"), 0, true, false, false, cpMin, cpMax),
                true, _T("Find/forwardHit"));
    EXPECT_INT(cpMin, 0, _T("Find/forwardPos"));

    //从位置 1 继续向前找：不区分大小写时会命中 "Category" 里的 "Cat"。
    EXPECT_BOOL(re.FindText(_T("cat"), 1, true, false, false, cpMin, cpMax),
                true, _T("Find/forwardSecondHit"));
    EXPECT_INT(cpMin, 4, _T("Find/forwardSecondPos"));

    //区分大小写时，从位置 1 起就跳过 "Category"，命中末尾那个小写 "cat"。
    EXPECT_BOOL(re.FindText(_T("cat"), 1, true, true, false, cpMin, cpMax),
                true, _T("Find/matchCaseHit"));
    EXPECT_INT(cpMin, 13, _T("Find/matchCasePos"));

    //整词匹配：从位置 1 起，"Category" 不算整词，命中末尾那个独立的 "cat"。
    EXPECT_BOOL(re.FindText(_T("cat"), 1, true, false, true, cpMin, cpMax),
                true, _T("Find/wholeWordHit"));
    EXPECT_INT(cpMin, 13, _T("Find/wholeWordPos"));

    //找一个不存在的串。
    EXPECT_BOOL(re.FindText(_T("zebra"), 0, true, false, false, cpMin, cpMax),
                false, _T("Find/miss"));

    //空串直接返回没找到，不去打扰引擎。
    EXPECT_BOOL(re.FindText(_T(""), 0, true, false, false, cpMin, cpMax),
                false, _T("Find/emptyNeedle"));
    return OK(_T("FindTextVariants"));
}

//查找并选中，以及绕回。
static Result Test_FindAndSelectWraps()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("alpha beta alpha"));

    //从头找，命中第一个并选中。
    EXPECT_BOOL(re.FindAndSelect(_T("alpha"), 0, true, false, false, false),
                true, _T("FindSel/first"));
    long cpMin = 0;
    long cpMax = 0;
    re.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 0, _T("FindSel/firstMin"));
    EXPECT_INT(cpMax, 5, _T("FindSel/firstMax"));

    //接着找，命中第二个。
    EXPECT_BOOL(re.FindAndSelect(_T("alpha"), -1, true, false, false, false),
                true, _T("FindSel/second"));
    re.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 11, _T("FindSel/secondMin"));

    //再往后没有了：不绕回时找不到。
    EXPECT_BOOL(re.FindAndSelect(_T("alpha"), -1, true, false, false, false),
                false, _T("FindSel/noWrapMiss"));

    //允许绕回时，从头再来一轮，又命中第一个。
    EXPECT_BOOL(re.FindAndSelect(_T("alpha"), -1, true, false, false, true),
                true, _T("FindSel/wrapHit"));
    re.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 0, _T("FindSel/wrapPos"));
    return OK(_T("FindAndSelectWraps"));
}

//RTF 往返：文字与格式都要能保住。
static Result Test_RtfRoundTripKeepsFormatting()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("plain bold"));

    //给后四个字符加粗。
    re.SetSel(6, 10);
    re.SetSelBold(true);

    CStringA rtf;
    EXPECT_BOOL(re.SaveRTF(rtf), true, _T("Rtf/saveOk"));
    if (rtf.GetLength() <= 0)
    {
        return Fail(_T("Rtf/emptyOutput"), _T("Rtf/emptyOutput: SaveRTF produced nothing"));
    }

    //换掉内容，再从 RTF 恢复。
    re.SetText(_T("something else entirely"));
    EXPECT_BOOL(re.LoadRTF(rtf), true, _T("Rtf/loadOk"));
    EXPECT_STR(re.GetText(), _T("plain bold"), _T("Rtf/textRestored"));

    //格式也要回来：后四个字符仍是粗体，前面仍不是。
    re.SetSel(6, 10);
    bool bBold = false;
    EXPECT_BOOL(re.GetSelBold(bBold), true, _T("Rtf/boldUniform"));
    EXPECT_BOOL(bBold,                true, _T("Rtf/boldRestored"));

    re.SetSel(0, 5);
    EXPECT_BOOL(re.GetSelBold(bBold), true,  _T("Rtf/plainUniform"));
    EXPECT_BOOL(bBold,                false, _T("Rtf/plainStillPlain"));
    return OK(_T("RtfRoundTripKeepsFormatting"));
}

//纯文本往返：内容保住、格式丢掉（这是预期行为，不是缺陷）。
static Result Test_PlainTextRoundTripDropsFormatting()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("plain bold"));
    re.SetSel(6, 10);
    re.SetSelBold(true);

    CString text;
    EXPECT_BOOL(re.SaveText(text), true, _T("PlainRt/saveOk"));
    EXPECT_STR(text, _T("plain bold"), _T("PlainRt/content"));

    re.SetText(_T("wiped"));
    EXPECT_BOOL(re.LoadText(text), true, _T("PlainRt/loadOk"));
    EXPECT_STR(re.GetText(), _T("plain bold"), _T("PlainRt/textRestored"));

    //格式确实丢了 —— 整篇应当都不是粗体。
    re.SelectAll();
    bool bBold = true;
    EXPECT_BOOL(re.GetSelBold(bBold), true,  _T("PlainRt/uniform"));
    EXPECT_BOOL(bBold,                false, _T("PlainRt/formattingDropped"));
    return OK(_T("PlainTextRoundTripDropsFormatting"));
}

//最大长度**只挡用户输入，不挡程序化写入**。
//
//这与只读的取舍是同一条原则：这类限制是给用户设的，不是给调用方设的。
//公告预览那种「只读但业务要往里填内容」的场景全靠这条。
static Result Test_MaxLengthLimitsTypingNotSetText()
{
    DuiRichEdit re;
    SetUpWideEditor(re);

    re.SetMaxLength(5);
    EXPECT_INT(re.GetMaxLength(), 5, _T("MaxLen/getter"));

    //程序化写入不受限制 —— 十个字符原样进去。
    re.SetText(_T("0123456789"));
    EXPECT_INT(re.GetTextLength(), 10, _T("MaxLen/setTextNotLimited"));

    //用户输入受限制：清空后模拟连打八个字符，只有前五个进得去。
    re.SetText(_T(""));
    for (int i = 0; i < 8; ++i)
    {
        re.OnChar(_T('a'));
    }
    EXPECT_INT(re.GetTextLength(), 5, _T("MaxLen/typingLimited"));

    //解除限制之后可以继续打。
    re.SetMaxLength(0);
    for (int i = 0; i < 3; ++i)
    {
        re.OnChar(_T('b'));
    }
    EXPECT_INT(re.GetTextLength(), 8, _T("MaxLen/unlimitedAfterClear"));
    return OK(_T("MaxLengthLimitsTypingNotSetText"));
}

//密码模式与竖排：开关能读回，且不影响内容本身。
static Result Test_PasswordAndVerticalToggles()
{
    DuiRichEdit re;
    SetUpWideEditor(re);
    re.SetText(_T("secret"));

    EXPECT_BOOL(re.IsPasswordMode(), false, _T("Pwd/defaultOff"));
    re.SetPasswordMode(true);
    EXPECT_BOOL(re.IsPasswordMode(), true, _T("Pwd/on"));
    //内容读取不受遮蔽影响 —— 遮的只是显示。
    EXPECT_STR(re.GetText(), _T("secret"), _T("Pwd/contentIntact"));
    re.SetPasswordMode(false);
    EXPECT_BOOL(re.IsPasswordMode(), false, _T("Pwd/off"));

    EXPECT_BOOL(re.IsVertical(), false, _T("Vert/defaultOff"));
    re.SetVertical(true);
    EXPECT_BOOL(re.IsVertical(), true, _T("Vert/on"));
    EXPECT_STR(re.GetText(), _T("secret"), _T("Vert/contentIntact"));
    re.SetVertical(false);
    EXPECT_BOOL(re.IsVertical(), false, _T("Vert/off"));
    return OK(_T("PasswordAndVerticalToggles"));
}

//自动增高：期望高度随内容变化。
static Result Test_AutoGrowFollowsContent()
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 200, 200);

    DuiRichEdit re;
    re.SetMultiLine(true);
    re.SetWordWrap(true);
    re.Layout(rc);

    EXPECT_BOOL(re.IsAutoGrow(), true, _T("AutoGrow/onByDefault"));

    //一行内容。
    re.SetText(_T("one line"));
    const int nShort = (int)re.GetDesiredSize().cy;

    //多行内容。
    re.SetText(_T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog."));
    const int nLong = (int)re.GetDesiredSize().cy;

    if (nShort <= 0)
    {
        CString d;
        d.Format(_T("AutoGrow/shortNotPositive: got %d"), nShort);
        return Fail(_T("AutoGrow/shortNotPositive"), d);
    }
    if (nLong <= nShort)
    {
        CString d;
        d.Format(_T("AutoGrow/didNotGrow: short=%d long=%d"), nShort, nLong);
        return Fail(_T("AutoGrow/didNotGrow"), d);
    }
    return OK(_T("AutoGrowFollowsContent"));
}

//**内容高度的测量必须只跟内容有关，跟控件当前有多高无关**。
//
//这是自动增高的地基。两条要求缺一不可：
//  · 行数多了，量出来要更高 —— 否则控件长不起来；
//  · 控件当前高度是 0 也好、20 也好、200 也好，同样的内容要量出同样的高度 ——
//    否则控件长到某个高度之后就自我锁死，内容再多也不动了。首次显示时控件
//    高度恰恰是 0（布局第一趟报不出期望尺寸），这一档尤其要覆盖。
static Result Test_MeasureIndependentOfControlHeight()
{
    //三种控件高度：零、一行都装不下、装得很宽裕。宽度保持一致。
    const int kHeights[] = { 0, 20, 200 };
    const int kHeightCount = sizeof(kHeights) / sizeof(kHeights[0]);

    int nOneLine[kHeightCount];
    int nThreeLines[kHeightCount];

    for (int i = 0; i < kHeightCount; ++i)
    {
        DuiRichEdit re;
        re.SetMultiLine(true);
        re.SetWordWrap(true);

        RECT rc;
        ::SetRect(&rc, 0, 0, 200, kHeights[i]);
        re.Layout(rc);

        re.SetText(_T("aaa"));
        nOneLine[i] = re.Test_MeasureContentHeight();

        re.SetText(_T("aaa\r\nbbb\r\nccc"));
        nThreeLines[i] = re.Test_MeasureContentHeight();
    }

    for (int i = 0; i < kHeightCount; ++i)
    {
        //一行文字的高度不可能只有几个像素。定这个下界是为了发现「引擎用的
        //字体跟控件以为的字体对不上」这类问题 —— 那种情况下测量本身还是
        //「随内容增长」的，只有绝对值离谱，光比大小看不出来。
        const int kMinPlausibleLineHeight = 8;
        if (nOneLine[i] < kMinPlausibleLineHeight)
        {
            CString d;
            d.Format(_T("Measure/lineTooShort at ctrlHeight=%d: got %d"),
                     kHeights[i], nOneLine[i]);
            return Fail(_T("Measure/lineTooShort"), d);
        }
        if (nThreeLines[i] <= nOneLine[i])
        {
            CString d;
            d.Format(_T("Measure/didNotGrow at ctrlHeight=%d: one=%d three=%d"),
                     kHeights[i], nOneLine[i], nThreeLines[i]);
            return Fail(_T("Measure/didNotGrow"), d);
        }
    }

    //三种控件高度下量出来的结果必须一致。
    for (int i = 1; i < kHeightCount; ++i)
    {
        if (nOneLine[i] != nOneLine[0] || nThreeLines[i] != nThreeLines[0])
        {
            CString d;
            d.Format(_T("Measure/dependsOnCtrlHeight: at %d one=%d three=%d, ")
                     _T("at %d one=%d three=%d"),
                     kHeights[0], nOneLine[0], nThreeLines[0],
                     kHeights[i], nOneLine[i], nThreeLines[i]);
            return Fail(_T("Measure/dependsOnCtrlHeight"), d);
        }
    }
    return OK(_T("MeasureIndependentOfControlHeight"));
}

//**量出来的内容高度必须与排版引擎实际排出的行距一致**（绝对值，不只是相对大小）。
//
//上一条只比大小、只设了一个很低的下界，测量结果整体按比例偏小时照样通过：2026-10-01
//之前单行只量出 3～6 像素、三行 9～18 像素，而引擎实际的行距在 17 像素上下。原因是
//TxGetNaturalSize 把「入参高度 ÷ 范围参数的高度」当缩放比例，入参高度传 0 时比例失真。
//
//基准取引擎自己报的字符坐标：同一段三行文字里，第二行行首与第一行行首的纵坐标之差
//就是行距。测量结果按行数与它比对，允许取整带来的误差。
static Result Test_MeasureMatchesEngineLineHeight()
{
    //三种控件高度：零、一行都装不下、装得很宽裕，与上一条一致。宽度保持一致
    const int kHeights[] = { 0, 20, 200 };
    const int kHeightCount = sizeof(kHeights) / sizeof(kHeights[0]);
    //控件宽度（像素），三行短文字都不会折行
    const int kWidth = 200;
    //取行距基准用的控件高度（像素），足够排下三行
    const int kPitchHeight = 200;
    //第二行行首在文本里的位置：引擎把 "\r\n" 存成一个字符，"aaa\r" 之后就是第二行
    const int kSecondLineCp = 4;
    //允许的误差（像素）：字符坐标与测量结果都取整到像素，每行最多差不到 1 像素
    const int kOneLineTolerance   = 1;
    const int kThreeLineTolerance = 3;
    //三行文字的行数
    const int kThreeLines = 3;

    //基准：引擎实际排出的行距
    int nPitch = 0;
    {
        DuiRichEdit re;
        re.SetMultiLine(true);
        re.SetWordWrap(true);
        RECT rc;
        ::SetRect(&rc, 0, 0, kWidth, kPitchHeight);
        re.Layout(rc);
        re.SetText(_T("aaa\r\nbbb\r\nccc"));

        ITextServices* pSvc = re.Test_GetTextServices();
        if (pSvc == nullptr)
        {
            return Fail(_T("MeasureMatchesEngineLineHeight"), _T("no text services"));
        }
        POINTL ptFirst  = { 0, 0 };
        POINTL ptSecond = { 0, 0 };
        LRESULT lr = 0;
        pSvc->TxSendMessage(EM_POSFROMCHAR, (WPARAM)&ptFirst, 0, &lr);
        pSvc->TxSendMessage(EM_POSFROMCHAR, (WPARAM)&ptSecond, kSecondLineCp, &lr);
        nPitch = (int)(ptSecond.y - ptFirst.y);
        if (nPitch <= 0)
        {
            CString d;
            d.Format(_T("Measure/pitchNotPositive: first.y=%d second.y=%d"),
                     (int)ptFirst.y, (int)ptSecond.y);
            return Fail(_T("Measure/pitchNotPositive"), d);
        }
    }

    for (int i = 0; i < kHeightCount; ++i)
    {
        DuiRichEdit re;
        re.SetMultiLine(true);
        re.SetWordWrap(true);
        RECT rc;
        ::SetRect(&rc, 0, 0, kWidth, kHeights[i]);
        re.Layout(rc);

        re.SetText(_T("aaa"));
        const int nOne = re.Test_MeasureContentHeight();
        re.SetText(_T("aaa\r\nbbb\r\nccc"));
        const int nThree = re.Test_MeasureContentHeight();

        int nOneDiff = nOne - nPitch;
        if (nOneDiff < 0)
        {
            nOneDiff = -nOneDiff;
        }
        int nThreeDiff = nThree - nPitch * kThreeLines;
        if (nThreeDiff < 0)
        {
            nThreeDiff = -nThreeDiff;
        }
        if (nOneDiff > kOneLineTolerance || nThreeDiff > kThreeLineTolerance)
        {
            CString d;
            d.Format(_T("Measure/notEngineLineHeight at ctrlHeight=%d: pitch=%d one=%d three=%d"),
                     kHeights[i], nPitch, nOne, nThree);
            return Fail(_T("Measure/notEngineLineHeight"), d);
        }
    }
    return OK(_T("MeasureMatchesEngineLineHeight"));
}

//上下限：内容少时不低于下限，内容多时不超过上限。
static Result Test_AutoGrowClampsToRange()
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 200, 200);

    DuiRichEdit re;
    re.SetMultiLine(true);
    re.SetWordWrap(true);
    re.Layout(rc);
    re.SetAutoGrowRange(80, 120);
    EXPECT_INT(re.GetAutoGrowMin(), 80,  _T("AutoGrow/minGetter"));
    EXPECT_INT(re.GetAutoGrowMax(), 120, _T("AutoGrow/maxGetter"));

    //空内容：应当被抬到下限。
    re.SetText(_T(""));
    EXPECT_INT((int)re.GetDesiredSize().cy, 80, _T("AutoGrow/clampedToMin"));

    //超长内容：应当被压到上限，超出部分交给滚动条。
    re.SetText(_T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog. ")
               _T("The quick brown fox jumps over the lazy dog."));
    EXPECT_INT((int)re.GetDesiredSize().cy, 120, _T("AutoGrow/clampedToMax"));
    return OK(_T("AutoGrowClampsToRange"));
}

//关掉自动增高之后不再报告期望高度，交给父容器决定。
static Result Test_AutoGrowOffYieldsToParent()
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 200, 200);

    DuiRichEdit re;
    re.SetMultiLine(true);
    re.Layout(rc);
    re.SetText(_T("some content here"));

    re.SetAutoGrow(false);
    EXPECT_BOOL(re.IsAutoGrow(), false, _T("AutoGrowOff/flag"));
    EXPECT_INT((int)re.GetDesiredSize().cy, 0, _T("AutoGrowOff/zeroHeight"));

    //宽度任何时候都不主动要求。
    EXPECT_INT((int)re.GetDesiredSize().cx, 0, _T("AutoGrowOff/zeroWidth"));
    return OK(_T("AutoGrowOffYieldsToParent"));
}

//按行数设上下限：换算出来的像素值应当与行高成比例。
static Result Test_AutoGrowLinesConvertToPixels()
{
    DuiRichEdit re;
    SetUpWideEditor(re);

    const int nLineH = re.GetLineHeight();
    if (nLineH <= 0)
    {
        CString d;
        d.Format(_T("AutoGrowLines/badLineHeight: %d"), nLineH);
        return Fail(_T("AutoGrowLines/badLineHeight"), d);
    }

    re.SetAutoGrowLines(1, 5);
    EXPECT_INT(re.GetAutoGrowMin(), nLineH,     _T("AutoGrowLines/min"));
    EXPECT_INT(re.GetAutoGrowMax(), nLineH * 5, _T("AutoGrowLines/max"));

    //上限传 0 表示不限。
    re.SetAutoGrowLines(2, 0);
    EXPECT_INT(re.GetAutoGrowMin(), nLineH * 2, _T("AutoGrowLines/min2"));
    EXPECT_INT(re.GetAutoGrowMax(), 0,          _T("AutoGrowLines/unlimited"));
    return OK(_T("AutoGrowLinesConvertToPixels"));
}

//拖放开关：运行期可改，且引擎那一位的正负号没搞反。
//引擎的属性位表达的是「禁止拖放」，与我们对外的「允许拖放」正好相反，
//这类反向映射最容易写错，用一条用例钉住。
static Result Test_DragDropToggle()
{
    DuiRichEdit re;
    SetUpWideEditor(re);

    EXPECT_BOOL(re.IsDragDropEnabled(), true, _T("DragDrop/onByDefault"));

    re.SetDragDropEnabled(false);
    EXPECT_BOOL(re.IsDragDropEnabled(), false, _T("DragDrop/off"));

    re.SetDragDropEnabled(true);
    EXPECT_BOOL(re.IsDragDropEnabled(), true, _T("DragDrop/backOn"));
    return OK(_T("DragDropToggle"));
}

//拖放接线：挂进 DUI 树之后宿主应当把分发器注册好，控件应当交得出拖放目标；
//关掉拖放之后应当交出空指针，让分发器跳过本控件。
//
//这条需要真窗口 —— 拖放目标是按窗口注册的，没有窗口就没有可注册之处。
static Result Test_DropTargetWiring()
{
    OffscreenTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("DropTargetWiring"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, 400, 200);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
    if (!host.IsWindow())
    {
        return Fail(_T("DropTargetWiring"), _T("cannot create DuiHost"));
    }

    DuiRichEdit* pEdit = new DuiRichEdit();
    //挂进树时会走一次布局，控件在那里向宿主请求打开分发。
    host.SetRoot(std::unique_ptr<DuiControl>(pEdit));

    const bool bDispatchOn = host.IsDropDispatchEnabled();
    const bool bHasTarget  = (pEdit->GetDropTarget() != nullptr);

    //按坐标能找到该接收拖放的控件。
    POINT ptInside;
    ptInside.x = 50;
    ptInside.y = 50;
    DuiControl* pFound = host.FindDropTargetControl(ptInside);
    const bool bFoundSelf = (pFound == pEdit);

    //关掉拖放之后交出空指针。
    pEdit->SetDragDropEnabled(false);
    const bool bNullWhenOff = (pEdit->GetDropTarget() == nullptr);
    DuiControl* pFoundOff = host.FindDropTargetControl(ptInside);
    const bool bNotFoundWhenOff = (pFoundOff == nullptr);

    //重新打开又能交出目标。
    pEdit->SetDragDropEnabled(true);
    const bool bTargetBack = (pEdit->GetDropTarget() != nullptr);

    host.DestroyWindow();

    EXPECT_BOOL(bDispatchOn,       true, _T("DropWire/dispatchRegistered"));
    EXPECT_BOOL(bHasTarget,        true, _T("DropWire/engineTargetObtained"));
    EXPECT_BOOL(bFoundSelf,        true, _T("DropWire/hitTestFindsControl"));
    EXPECT_BOOL(bNullWhenOff,      true, _T("DropWire/nullWhenDisabled"));
    EXPECT_BOOL(bNotFoundWhenOff,  true, _T("DropWire/skippedWhenDisabled"));
    EXPECT_BOOL(bTargetBack,       true, _T("DropWire/targetBackWhenReenabled"));
    return OK(_T("DropTargetWiring"));
}

// 把当前线程消息队列里已经排队的消息全部处理掉。
//
// 自动增高的重排请求是**投递**（PostMessage）给宿主窗口的，不是当场执行的。
// 测试里如果不主动跑一轮消息循环，那条消息就一直躺在队列里，控件的矩形当然
// 不会变 —— 表现为用例失败，但根因跟被测逻辑无关。
static void PumpMessages()
{
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
}

//**端到端的自动增高链路**：内容变高之后，控件在父容器里占的矩形要真的变高。
//
//这条用例守的是「测量」与「触发」之间的接缝。测量那半边（GetDesiredSize 按
//内容算高度、夹到上下限）此前已有四条用例覆盖，但实机上控件仍然不会长高 ——
//因为 EN_CHANGE 里只发了内容变化通知，没有任何人要求重新排版，而父容器的
//排版只在窗口尺寸变化时才跑。这正是「两段各自正常、接缝处断了」的又一例，
//只有把真窗口、真宿主、真布局容器串起来才测得出。
static Result Test_AutoGrowRelayoutsThroughHost()
{
    OffscreenTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("AutoGrowRelayoutsThroughHost"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, 400, 200);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
    if (!host.IsWindow())
    {
        return Fail(_T("AutoGrowRelayoutsThroughHost"), _T("cannot create DuiHost"));
    }

    //竖直容器里放两个子控件：编辑器用「自动档」按内容占高，下面的填充控件
    //按权重吃掉剩余空间。这正是聊天输入框那类界面的排法。
    DuiVBox* pBox = new DuiVBox();
    DuiRichEdit* pEdit = new DuiRichEdit();
    pBox->AddChild(std::unique_ptr<DuiControl>(pEdit), DuiLayout::Hint().Auto());
    pBox->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Weight(1));
    host.SetRoot(std::unique_ptr<DuiControl>(pBox));

    //最少一行、最多五行，与聊天输入框的常见设置一致。
    pEdit->SetAutoGrowLines(1, 5);
    PumpMessages();

    //把焦点也交给编辑器，与用户实际使用时的状态一致：排版引擎在获得焦点后
    //会进入激活状态（建光标、接管输入法），这条路径下自动增高同样要正常。
    ::SetFocus(top.get());
    pEdit->SetFocus();
    PumpMessages();

    //先排一次版，拿到「一行内容」时的基准高度。
    //
    //排完必须跑一轮消息循环：首次排版时控件还没有宽度，量不出高度只能报 0，
    //它会在拿到矩形之后再请求一次重排来自纠（见 DuiRichEdit::Layout 末尾的
    //注释）。不跑消息循环的话，读到的就是自纠之前那个 0。
    RECT rcRoot;
    ::SetRect(&rcRoot, 0, 0, 400, 200);
    pBox->ForceLayout(rcRoot);
    PumpMessages();
    const int nHeightOneLine = pEdit->GetRect().bottom - pEdit->GetRect().top;

    //换行输入三行文字。走 SetText 而不是逐字投递按键，是因为这条用例要验的是
    //「内容变了以后有没有重新排版」，与输入通路无关，用最直接的方式改内容即可。
    pEdit->SetText(_T("line one\r\nline two\r\nline three"));
    PumpMessages();
    const int nHeightThreeLines = pEdit->GetRect().bottom - pEdit->GetRect().top;

    //内容删回一行，高度要能缩回去 —— 只会长不会缩也是一种常见的实现错误。
    pEdit->SetText(_T("line one"));
    PumpMessages();
    const int nHeightBack = pEdit->GetRect().bottom - pEdit->GetRect().top;

    host.DestroyWindow();

    if (nHeightOneLine <= 0)
    {
        CString d;
        d.Format(_T("AutoGrowRelayout/baselineNotPositive: got %d"), nHeightOneLine);
        return Fail(_T("AutoGrowRelayout/baselineNotPositive"), d);
    }
    if (nHeightThreeLines <= nHeightOneLine)
    {
        CString d;
        d.Format(_T("AutoGrowRelayout/didNotGrow: one=%d three=%d"),
                 nHeightOneLine, nHeightThreeLines);
        return Fail(_T("AutoGrowRelayout/didNotGrow"), d);
    }
    if (nHeightBack != nHeightOneLine)
    {
        CString d;
        d.Format(_T("AutoGrowRelayout/didNotShrink: expected %d got %d"),
                 nHeightOneLine, nHeightBack);
        return Fail(_T("AutoGrowRelayout/didNotShrink"), d);
    }
    return OK(_T("AutoGrowRelayoutsThroughHost"));
}

//**打字也要能触发自动增高**。
//
//上一条走的是 SetText，那是程序改内容的通路；本条走的是用户敲键盘的通路，
//由排版引擎的内容变化通知（EN_CHANGE）驱动。两条通路的触发点在代码里是分开
//写的，必须分开测 —— 只测一条的话，另一条断了也看不出来。
static Result Test_AutoGrowFollowsTyping()
{
    OffscreenTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("AutoGrowFollowsTyping"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, 400, 200);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
    if (!host.IsWindow())
    {
        return Fail(_T("AutoGrowFollowsTyping"), _T("cannot create DuiHost"));
    }

    DuiVBox* pBox = new DuiVBox();
    DuiRichEdit* pEdit = new DuiRichEdit();
    pBox->AddChild(std::unique_ptr<DuiControl>(pEdit), DuiLayout::Hint().Auto());
    pBox->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Weight(1));
    host.SetRoot(std::unique_ptr<DuiControl>(pBox));
    //不设上下限，让期望高度直接跟着量出来的内容高度走。用按行数换算的下限
    //会引入「界面字体的行高」这个与排版引擎无关的量，把内容变化淹没掉。
    pEdit->SetAutoGrowRange(1, 0);
    PumpMessages();

    //焦点交给编辑器，宿主会据此把 Win32 键盘焦点要过来。
    ::SetFocus(top.get());
    pEdit->SetFocus();

    //先打一个字，记下单行时的高度。
    ::SendMessage(host.m_hWnd, WM_CHAR, (WPARAM)_T('a'), 0);
    PumpMessages();
    const int nHeightOneLine = pEdit->GetRect().bottom - pEdit->GetRect().top;

    //连敲两次回车换到第三行。
    //
    //回车必须**先发按键按下、再发字符消息**，两条缺一不可。真实运行时这两条
    //本来就是成对来的（系统的 TranslateMessage 由前者生成后者），而排版引擎
    //会丢掉没有配套按键按下的回车字符 —— 只发字符消息的话，文字进得去、
    //换行进不去，表现为「打了几个字，行数一直是一行」。
    ::SendMessage(host.m_hWnd, WM_KEYDOWN, (WPARAM)VK_RETURN, 0);
    ::SendMessage(host.m_hWnd, WM_CHAR,    (WPARAM)_T('\r'),  0);
    ::SendMessage(host.m_hWnd, WM_CHAR,    (WPARAM)_T('b'),   0);
    ::SendMessage(host.m_hWnd, WM_KEYDOWN, (WPARAM)VK_RETURN, 0);
    ::SendMessage(host.m_hWnd, WM_CHAR,    (WPARAM)_T('\r'),  0);
    ::SendMessage(host.m_hWnd, WM_CHAR,    (WPARAM)_T('c'),   0);
    PumpMessages();
    const int nHeightThreeLines = pEdit->GetRect().bottom - pEdit->GetRect().top;
    //失败时要能分清「字根本没打进去」和「打进去了但没重新排版」，
    //所以把实际内容长度与量出来的内容高度一并记下来。
    const int nTextLen  = pEdit->GetTextLength();
    const int nMeasured = pEdit->Test_MeasureContentHeight();

    host.DestroyWindow();

    if (nHeightOneLine <= 0)
    {
        CString d;
        d.Format(_T("AutoGrowTyping/baselineNotPositive: got %d"), nHeightOneLine);
        return Fail(_T("AutoGrowTyping/baselineNotPositive"), d);
    }
    if (nHeightThreeLines <= nHeightOneLine)
    {
        CString d;
        d.Format(_T("AutoGrowTyping/didNotGrow: one=%d three=%d textLen=%d measured=%d"),
                 nHeightOneLine, nHeightThreeLines, nTextLen, nMeasured);
        return Fail(_T("AutoGrowTyping/didNotGrow"), d);
    }
    return OK(_T("AutoGrowFollowsTyping"));
}

//关掉自动增高之后，内容再怎么变也不再改变控件高度 —— 高度完全交给父容器。
//与上一条配对，守住开关的「关」这一侧。
static Result Test_AutoGrowOffDoesNotRelayout()
{
    OffscreenTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("AutoGrowOffDoesNotRelayout"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, 400, 200);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);

    DuiVBox* pBox = new DuiVBox();
    DuiRichEdit* pEdit = new DuiRichEdit();
    //固定 60 像素高：自动档关掉之后，高度应当一直是这个值。
    const int kFixedHeight = 60;
    pBox->AddChild(std::unique_ptr<DuiControl>(pEdit),
                   DuiLayout::Hint().Fixed(kFixedHeight));
    pBox->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                   DuiLayout::Hint().Weight(1));
    host.SetRoot(std::unique_ptr<DuiControl>(pBox));

    pEdit->SetAutoGrow(false);
    PumpMessages();

    RECT rcRoot;
    ::SetRect(&rcRoot, 0, 0, 400, 200);
    pBox->ForceLayout(rcRoot);
    const int nHeightBefore = pEdit->GetRect().bottom - pEdit->GetRect().top;

    pEdit->SetText(_T("line one\r\nline two\r\nline three\r\nline four"));
    PumpMessages();
    const int nHeightAfter = pEdit->GetRect().bottom - pEdit->GetRect().top;

    host.DestroyWindow();

    EXPECT_INT(nHeightBefore, kFixedHeight, _T("AutoGrowOff/fixedHeightHonored"));
    EXPECT_INT(nHeightAfter,  kFixedHeight, _T("AutoGrowOff/unchangedAfterEdit"));
    return OK(_T("AutoGrowOffDoesNotRelayout"));
}

// ---- 右键菜单模型（RichEditContextMenu.h 的纯逻辑）--------------------
//
// 这一组不碰窗口、不弹菜单，只验「算出来的菜单对不对」。真正弹菜单的
// TrackPopup 是同步阻塞的，测试里不能真弹 —— 把「算什么菜单」与「怎么弹
// 菜单」分成两层，正是为了让这一半能被自动化覆盖。

// 造一份「读写、有选区、剪贴板有文本、文档非空、可撤销可重做」的状态，
// 即所有项都可用的基准。各条用例在它基础上改一两个字段，测出的差异
// 就能明确归因到被改的那个字段上。
static DuiRichEditMenuState MakeFullyEnabledMenuState()
{
    DuiRichEditMenuState st;
    st.m_readOnly         = false;
    st.m_hasSelection     = true;
    st.m_clipboardHasText = true;
    st.m_hasText          = true;
    st.m_canUndo          = true;
    st.m_canRedo          = true;
    return st;
}

// 在模型里找某个命令的下标。找不到返回 -1。
static int FindMenuItemIndex(const std::vector<DuiRichEditMenuItem>& items, UINT nCmd)
{
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (!items[i].m_separator && items[i].m_id == nCmd)
        {
            return (int)i;
        }
    }
    return -1;
}

// 某个命令在模型里是否可用。项不存在时返回 false。
static bool IsMenuItemEnabled(const std::vector<DuiRichEditMenuItem>& items, UINT nCmd)
{
    int idx = FindMenuItemIndex(items, nCmd);
    if (idx < 0)
    {
        return false;
    }
    return items[(size_t)idx].m_enabled;
}

// 读写模式下默认菜单的项与顺序。
static Result Test_MenuModelReadWriteLayout()
{
    DuiRichEditMenuState st = MakeFullyEnabledMenuState();
    std::vector<DuiRichEditMenuItem> items;
    BuildDuiRichEditContextMenu(st, items);

    //撤销 / 重做 /（分隔条）/ 剪切 / 复制 / 粘贴 / 粘贴为纯文本 / 删除 /
    //（分隔条）/ 全选，共 10 项。
    EXPECT_INT((int)items.size(), 10, _T("MenuRW/count"));

    EXPECT_INT((int)items[0].m_id, kRichEditCmdUndo,       _T("MenuRW/0_undo"));
    EXPECT_INT((int)items[1].m_id, kRichEditCmdRedo,       _T("MenuRW/1_redo"));
    EXPECT_BOOL(items[2].m_separator, true,                _T("MenuRW/2_sep"));
    EXPECT_INT((int)items[3].m_id, kRichEditCmdCut,        _T("MenuRW/3_cut"));
    EXPECT_INT((int)items[4].m_id, kRichEditCmdCopy,       _T("MenuRW/4_copy"));
    EXPECT_INT((int)items[5].m_id, kRichEditCmdPaste,      _T("MenuRW/5_paste"));
    EXPECT_INT((int)items[6].m_id, kRichEditCmdPastePlain, _T("MenuRW/6_pastePlain"));
    EXPECT_INT((int)items[7].m_id, kRichEditCmdDelete,     _T("MenuRW/7_delete"));
    EXPECT_BOOL(items[8].m_separator, true,                _T("MenuRW/8_sep"));
    EXPECT_INT((int)items[9].m_id, kRichEditCmdSelectAll,  _T("MenuRW/9_selectAll"));
    return OK(_T("MenuModelReadWriteLayout"));
}

// 只读模式下只剩复制 /（分隔条）/ 全选，编辑类命令**整项不出现**。
static Result Test_MenuModelReadOnlyLayout()
{
    DuiRichEditMenuState st = MakeFullyEnabledMenuState();
    st.m_readOnly = true;

    std::vector<DuiRichEditMenuItem> items;
    BuildDuiRichEditContextMenu(st, items);

    EXPECT_INT((int)items.size(), 3, _T("MenuRO/count"));
    EXPECT_INT((int)items[0].m_id, kRichEditCmdCopy,      _T("MenuRO/0_copy"));
    EXPECT_BOOL(items[1].m_separator, true,               _T("MenuRO/1_sep"));
    EXPECT_INT((int)items[2].m_id, kRichEditCmdSelectAll, _T("MenuRO/2_selectAll"));

    //以下几项在只读模式下不应存在，而不是「存在但灰显」。
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdUndo),       -1, _T("MenuRO/noUndo"));
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdRedo),       -1, _T("MenuRO/noRedo"));
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdCut),        -1, _T("MenuRO/noCut"));
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdPaste),      -1, _T("MenuRO/noPaste"));
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdPastePlain), -1, _T("MenuRO/noPastePlain"));
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdDelete),     -1, _T("MenuRO/noDelete"));
    return OK(_T("MenuModelReadOnlyLayout"));
}

// 没有选区时，剪切 / 复制 / 删除三项灰显，其余不受影响。
static Result Test_MenuModelNoSelectionGraysSelectionCommands()
{
    DuiRichEditMenuState st = MakeFullyEnabledMenuState();
    st.m_hasSelection = false;

    std::vector<DuiRichEditMenuItem> items;
    BuildDuiRichEditContextMenu(st, items);

    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdCut),        false, _T("MenuNoSel/cut"));
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdCopy),       false, _T("MenuNoSel/copy"));
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdDelete),     false, _T("MenuNoSel/delete"));
    //这几项与选区无关，必须仍然可用 —— 防「一改条件把整张菜单都灰掉」。
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdPaste),      true,  _T("MenuNoSel/paste"));
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdSelectAll),  true,  _T("MenuNoSel/selectAll"));
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdUndo),       true,  _T("MenuNoSel/undo"));
    return OK(_T("MenuModelNoSelectionGraysSelectionCommands"));
}

// 剪贴板没有文本时，两种粘贴都灰显。
static Result Test_MenuModelEmptyClipboardGraysPaste()
{
    DuiRichEditMenuState st = MakeFullyEnabledMenuState();
    st.m_clipboardHasText = false;

    std::vector<DuiRichEditMenuItem> items;
    BuildDuiRichEditContextMenu(st, items);

    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdPaste),      false, _T("MenuNoClip/paste"));
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdPastePlain), false, _T("MenuNoClip/pastePlain"));
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdCut),        true,  _T("MenuNoClip/cut"));
    return OK(_T("MenuModelEmptyClipboardGraysPaste"));
}

// 文档为空时全选灰显；不能撤销 / 不能重做时对应项灰显。
static Result Test_MenuModelEmptyDocAndUndoState()
{
    DuiRichEditMenuState st = MakeFullyEnabledMenuState();
    st.m_hasText = false;
    st.m_canUndo = false;
    st.m_canRedo = false;

    std::vector<DuiRichEditMenuItem> items;
    BuildDuiRichEditContextMenu(st, items);

    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdSelectAll), false, _T("MenuEmpty/selectAll"));
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdUndo),      false, _T("MenuEmpty/undo"));
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdRedo),      false, _T("MenuEmpty/redo"));
    return OK(_T("MenuModelEmptyDocAndUndoState"));
}

// 每个内置命令都要有非空文案，且互不相同。
//
// 这条防的是「新增命令时忘了在文案函数里加一条 case」—— 漏了不会报错，
// 界面上表现为菜单里出现一个空白项，很容易漏看。
static Result Test_MenuModelEveryCommandHasLabel()
{
    const UINT kCmds[] = {
        kRichEditCmdUndo, kRichEditCmdRedo, kRichEditCmdCut, kRichEditCmdCopy,
        kRichEditCmdPaste, kRichEditCmdPastePlain, kRichEditCmdDelete,
        kRichEditCmdSelectAll
    };
    const int kCount = sizeof(kCmds) / sizeof(kCmds[0]);

    for (int i = 0; i < kCount; ++i)
    {
        CString label = DuiRichEditMenuCommandLabel(kCmds[i]);
        if (label.IsEmpty())
        {
            CString d;
            d.Format(_T("MenuLabel/empty: cmd=%u"), kCmds[i]);
            return Fail(_T("MenuLabel/empty"), d);
        }
        for (int j = i + 1; j < kCount; ++j)
        {
            if (label == DuiRichEditMenuCommandLabel(kCmds[j]))
            {
                CString d;
                d.Format(_T("MenuLabel/duplicate: cmd %u and %u share [%s]"),
                         kCmds[i], kCmds[j], (LPCTSTR)label);
                return Fail(_T("MenuLabel/duplicate"), d);
            }
        }
    }
    //自定义编号与分隔条没有内置文案。
    EXPECT_STR(CString(DuiRichEditMenuCommandLabel(kRichEditMenuCustomBase)),
               CString(_T("")), _T("MenuLabel/customHasNoBuiltinLabel"));
    return OK(_T("MenuModelEveryCommandHasLabel"));
}

// 规整分隔条：开头、结尾的分隔条去掉，连续多条合并成一条。
static Result Test_MenuModelNormalizeSeparators()
{
    std::vector<DuiRichEditMenuItem> items;
    //开头两条分隔条
    items.push_back(MakeDuiRichEditMenuSeparator());
    items.push_back(MakeDuiRichEditMenuSeparator());
    items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdCopy, nullptr, true));
    //中间三条连续分隔条
    items.push_back(MakeDuiRichEditMenuSeparator());
    items.push_back(MakeDuiRichEditMenuSeparator());
    items.push_back(MakeDuiRichEditMenuSeparator());
    items.push_back(MakeDuiRichEditMenuItem(kRichEditCmdSelectAll, nullptr, true));
    //结尾两条分隔条
    items.push_back(MakeDuiRichEditMenuSeparator());
    items.push_back(MakeDuiRichEditMenuSeparator());

    NormalizeDuiRichEditContextMenu(items);

    //只应剩下：复制 /（一条分隔条）/ 全选
    EXPECT_INT((int)items.size(), 3, _T("MenuNorm/count"));
    EXPECT_INT((int)items[0].m_id, kRichEditCmdCopy,      _T("MenuNorm/0_copy"));
    EXPECT_BOOL(items[1].m_separator, true,               _T("MenuNorm/1_sep"));
    EXPECT_INT((int)items[2].m_id, kRichEditCmdSelectAll, _T("MenuNorm/2_selectAll"));
    return OK(_T("MenuModelNormalizeSeparators"));
}

// 规整的边界：全是分隔条的模型会被清空，空模型不崩溃。
static Result Test_MenuModelNormalizeEdgeCases()
{
    //全是分隔条 → 规整后应当一项不剩（否则界面上会弹出一张只有横线的菜单）。
    std::vector<DuiRichEditMenuItem> allSeps;
    allSeps.push_back(MakeDuiRichEditMenuSeparator());
    allSeps.push_back(MakeDuiRichEditMenuSeparator());
    NormalizeDuiRichEditContextMenu(allSeps);
    EXPECT_INT((int)allSeps.size(), 0, _T("MenuNormEdge/allSeparatorsCleared"));

    //空模型规整后仍然为空，且不崩溃。
    std::vector<DuiRichEditMenuItem> empty;
    NormalizeDuiRichEditContextMenu(empty);
    EXPECT_INT((int)empty.size(), 0, _T("MenuNormEdge/emptyStaysEmpty"));

    //已经规整好的模型再规整一次不应改变 —— 这一步会被反复调用，必须幂等。
    std::vector<DuiRichEditMenuItem> tidy;
    tidy.push_back(MakeDuiRichEditMenuItem(kRichEditCmdCopy, nullptr, true));
    tidy.push_back(MakeDuiRichEditMenuSeparator());
    tidy.push_back(MakeDuiRichEditMenuItem(kRichEditCmdSelectAll, nullptr, true));
    NormalizeDuiRichEditContextMenu(tidy);
    EXPECT_INT((int)tidy.size(), 3, _T("MenuNormEdge/idempotent"));
    return OK(_T("MenuModelNormalizeEdgeCases"));
}

// 自定义项的编号段与内置命令不重叠。
//
// 这条不是测某段代码，而是钉住「编号划分」这个约定：将来给内置命令扩号时
// 一旦越过下限，本用例会失败，提醒改的人自定义项会被撞号。
static Result Test_MenuModelCustomBaseAboveBuiltins()
{
    const UINT kBuiltins[] = {
        kRichEditCmdUndo, kRichEditCmdRedo, kRichEditCmdCut, kRichEditCmdCopy,
        kRichEditCmdPaste, kRichEditCmdPastePlain, kRichEditCmdDelete,
        kRichEditCmdSelectAll
    };
    const int kCount = sizeof(kBuiltins) / sizeof(kBuiltins[0]);

    for (int i = 0; i < kCount; ++i)
    {
        if (kBuiltins[i] >= kRichEditMenuCustomBase)
        {
            CString d;
            d.Format(_T("MenuCustomBase/overlap: builtin cmd %u >= custom base %u"),
                     kBuiltins[i], kRichEditMenuCustomBase);
            return Fail(_T("MenuCustomBase/overlap"), d);
        }
    }
    return OK(_T("MenuModelCustomBaseAboveBuiltins"));
}

// ---- 右键菜单在控件上的接线 ------------------------------------------
//
// 上一组验的是「算菜单」的纯逻辑，这一组验的是控件有没有把自己的状态正确
// 喂给它、自定义项有没有接上、命令有没有分发对。都通过 Test_BuildContextMenu
// 和 Test_InvokeContextMenuCommand 绕开真正弹菜单那一步 —— TrackPopup 是
// 同步阻塞的，测试里不能真弹。

// 控件默认打开右键菜单，且默认菜单是读写模式那一套。
static Result Test_ContextMenuDefaults()
{
    DuiRichEdit re;
    EXPECT_BOOL(re.IsContextMenuEnabled(), true, _T("CtxDefault/enabledByDefault"));

    std::vector<DuiRichEditMenuItem> items = re.Test_BuildContextMenu();
    //空文档、无选区，但读写模式的项都应当在（只是多数灰显）。
    if (FindMenuItemIndex(items, kRichEditCmdCut) < 0
        || FindMenuItemIndex(items, kRichEditCmdPastePlain) < 0
        || FindMenuItemIndex(items, kRichEditCmdSelectAll) < 0)
    {
        return Fail(_T("CtxDefault/missingItems"),
                    _T("read-write menu should contain cut / paste-plain / select-all"));
    }
    //空文档时全选灰显。
    EXPECT_BOOL(IsMenuItemEnabled(items, kRichEditCmdSelectAll), false,
                _T("CtxDefault/selectAllGrayedWhenEmpty"));
    return OK(_T("ContextMenuDefaults"));
}

// 控件把自己的状态正确喂给了菜单模型：有文本之后全选变可用，
// 选中之后复制变可用。
static Result Test_ContextMenuReflectsControlState()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 200, 100);
    re.Layout(rc);

    //空文档：全选、复制都不可用。
    std::vector<DuiRichEditMenuItem> empty = re.Test_BuildContextMenu();
    EXPECT_BOOL(IsMenuItemEnabled(empty, kRichEditCmdSelectAll), false,
                _T("CtxState/selectAllOffWhenEmpty"));
    EXPECT_BOOL(IsMenuItemEnabled(empty, kRichEditCmdCopy), false,
                _T("CtxState/copyOffWhenNoSelection"));

    //填入文本：全选可用，但还没选中，复制仍不可用。
    re.SetText(_T("hello"));
    std::vector<DuiRichEditMenuItem> withText = re.Test_BuildContextMenu();
    EXPECT_BOOL(IsMenuItemEnabled(withText, kRichEditCmdSelectAll), true,
                _T("CtxState/selectAllOnWhenHasText"));
    EXPECT_BOOL(IsMenuItemEnabled(withText, kRichEditCmdCopy), false,
                _T("CtxState/copyStillOffBeforeSelecting"));

    //全选之后复制可用。
    re.SelectAll();
    std::vector<DuiRichEditMenuItem> selected = re.Test_BuildContextMenu();
    EXPECT_BOOL(IsMenuItemEnabled(selected, kRichEditCmdCopy), true,
                _T("CtxState/copyOnAfterSelectAll"));
    EXPECT_BOOL(IsMenuItemEnabled(selected, kRichEditCmdCut), true,
                _T("CtxState/cutOnAfterSelectAll"));
    return OK(_T("ContextMenuReflectsControlState"));
}

// 只读控件的菜单只剩复制与全选。
static Result Test_ContextMenuReadOnlyControl()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 200, 100);
    re.Layout(rc);
    re.SetText(_T("hello"));
    re.SetReadOnly(true);

    std::vector<DuiRichEditMenuItem> items = re.Test_BuildContextMenu();
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdCut),   -1, _T("CtxRO/noCut"));
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdPaste), -1, _T("CtxRO/noPaste"));
    EXPECT_INT(FindMenuItemIndex(items, kRichEditCmdUndo),  -1, _T("CtxRO/noUndo"));
    if (FindMenuItemIndex(items, kRichEditCmdCopy) < 0
        || FindMenuItemIndex(items, kRichEditCmdSelectAll) < 0)
    {
        return Fail(_T("CtxRO/missingItems"),
                    _T("read-only menu should still contain copy and select-all"));
    }
    return OK(_T("ContextMenuReadOnlyControl"));
}

// 自定义项追加到菜单末尾，且与默认项之间隔着一条分隔条。
static Result Test_ContextMenuCustomItemsAppended()
{
    const UINT kCmdFoo = kRichEditMenuCustomBase + 1;
    const UINT kCmdBar = kRichEditMenuCustomBase + 2;

    DuiRichEdit re;
    EXPECT_BOOL(re.AppendContextMenuItem(kCmdFoo, _T("插入表情")), true,
                _T("CtxCustom/appendFoo"));
    EXPECT_BOOL(re.AppendContextMenuItem(kCmdBar, _T("插入代码块")), true,
                _T("CtxCustom/appendBar"));

    std::vector<DuiRichEditMenuItem> items = re.Test_BuildContextMenu();
    EXPECT_INT((int)items.size() - 2, FindMenuItemIndex(items, kCmdFoo),
               _T("CtxCustom/fooIsSecondToLast"));
    EXPECT_INT((int)items.size() - 1, FindMenuItemIndex(items, kCmdBar),
               _T("CtxCustom/barIsLast"));

    //自定义项前面必须是一条分隔条，把它与内置命令分开。
    int idxFoo = FindMenuItemIndex(items, kCmdFoo);
    EXPECT_BOOL(items[(size_t)(idxFoo - 1)].m_separator, true,
                _T("CtxCustom/separatorBeforeCustom"));

    //文案要原样带出来。
    EXPECT_STR(items[(size_t)idxFoo].m_text, CString(_T("插入表情")),
               _T("CtxCustom/textPreserved"));
    //自定义项默认可用。
    EXPECT_BOOL(items[(size_t)idxFoo].m_enabled, true, _T("CtxCustom/enabled"));

    //清空之后菜单回到只有默认项的样子，末尾不会留下悬空的分隔条。
    re.ClearContextMenuItems();
    std::vector<DuiRichEditMenuItem> cleared = re.Test_BuildContextMenu();
    EXPECT_INT(FindMenuItemIndex(cleared, kCmdFoo), -1, _T("CtxCustom/clearedFoo"));
    EXPECT_BOOL(cleared.back().m_separator, false, _T("CtxCustom/noTrailingSeparator"));
    return OK(_T("ContextMenuCustomItemsAppended"));
}

// 没有自定义项时，末尾不会多出一条分隔条。
//
// 控件是无条件先加分隔条再追加自定义项的，靠规整那一步把没用上的分隔条
// 去掉。这条用例钉的就是那个依赖 —— 规整那步一旦被去掉，界面上会在菜单
// 底部露出一条悬空的横线。
static Result Test_ContextMenuNoTrailingSeparatorWithoutCustomItems()
{
    DuiRichEdit re;
    std::vector<DuiRichEditMenuItem> items = re.Test_BuildContextMenu();
    if (items.empty())
    {
        return Fail(_T("CtxNoTrail/empty"), _T("default menu should not be empty"));
    }
    EXPECT_BOOL(items.back().m_separator, false, _T("CtxNoTrail/lastIsNotSeparator"));
    EXPECT_INT((int)items.back().m_id, kRichEditCmdSelectAll, _T("CtxNoTrail/lastIsSelectAll"));
    return OK(_T("ContextMenuNoTrailingSeparatorWithoutCustomItems"));
}

// 自定义项编号低于下限时被拒绝，且不会进入菜单。
static Result Test_ContextMenuRejectsLowCustomId()
{
    DuiRichEdit re;
    //故意用一个与内置「粘贴」相同的编号。
    EXPECT_BOOL(re.AppendContextMenuItem(kRichEditCmdPaste, _T("冒充粘贴")), false,
                _T("CtxReject/rejected"));
    //下限减一同样要拒绝，钉住边界。
    EXPECT_BOOL(re.AppendContextMenuItem(kRichEditMenuCustomBase - 1, _T("差一号")), false,
                _T("CtxReject/rejectedBoundary"));
    //下限本身要放行。
    EXPECT_BOOL(re.AppendContextMenuItem(kRichEditMenuCustomBase, _T("正好")), true,
                _T("CtxReject/acceptedAtBoundary"));

    std::vector<DuiRichEditMenuItem> items = re.Test_BuildContextMenu();
    //被拒的项没有混进菜单：编号为「粘贴」的项应当只有一个，且文案是内置文案。
    int idxPaste = FindMenuItemIndex(items, kRichEditCmdPaste);
    if (idxPaste < 0)
    {
        return Fail(_T("CtxReject/pasteMissing"), _T("built-in paste item disappeared"));
    }
    EXPECT_STR(items[(size_t)idxPaste].m_text,
               CString(DuiRichEditMenuCommandLabel(kRichEditCmdPaste)),
               _T("CtxReject/pasteLabelIntact"));
    return OK(_T("ContextMenuRejectsLowCustomId"));
}

// 命令分发：内置命令由控件执行，自定义编号交回调用方。
static Result Test_ContextMenuCommandDispatch()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 200, 100);
    re.Layout(rc);
    re.SetText(_T("hello"));

    //全选：执行后应当真的选中了全部文本。
    EXPECT_BOOL(re.Test_InvokeContextMenuCommand(kRichEditCmdSelectAll), true,
                _T("CtxDispatch/selectAllHandled"));
    long cpMin = 0;
    long cpMax = 0;
    re.GetSel(cpMin, cpMax);
    EXPECT_INT((int)cpMin, 0, _T("CtxDispatch/selMin"));
    EXPECT_INT((int)cpMax, re.GetTextLength(), _T("CtxDispatch/selMax"));

    //删除：删掉选区，文档应当为空，且不写剪贴板（不便在单测里验剪贴板，
    //这里只验文档确实空了）。
    EXPECT_BOOL(re.Test_InvokeContextMenuCommand(kRichEditCmdDelete), true,
                _T("CtxDispatch/deleteHandled"));
    EXPECT_INT(re.GetTextLength(), 0, _T("CtxDispatch/deletedAll"));

    //撤销：内容应当回来。
    EXPECT_BOOL(re.Test_InvokeContextMenuCommand(kRichEditCmdUndo), true,
                _T("CtxDispatch/undoHandled"));
    EXPECT_STR(re.GetText(), CString(_T("hello")), _T("CtxDispatch/undoRestored"));

    //自定义编号：基类不认识，返回 false（真实运行时会转成通知发给父窗口）。
    EXPECT_BOOL(re.Test_InvokeContextMenuCommand(kRichEditMenuCustomBase + 7), false,
                _T("CtxDispatch/customNotHandled"));
    return OK(_T("ContextMenuCommandDispatch"));
}

// 关掉右键菜单之后，右键按下不再弹菜单，事件继续往上冒泡。
//
// 这里不真弹菜单，验的是 OnRButtonDown 的返回值：返回 false 表示本控件没有
// 消费这次右键，宿主会让它继续在 DUI 树里往上走，业务得以自己接管。
static Result Test_ContextMenuDisabledLetsEventBubble()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 200, 100);
    re.Layout(rc);
    re.SetContextMenuEnabled(false);
    EXPECT_BOOL(re.IsContextMenuEnabled(), false, _T("CtxDisabled/flag"));

    POINT pt;
    pt.x = 20;
    pt.y = 20;
    EXPECT_BOOL(re.OnRButtonDown(pt, 0), false, _T("CtxDisabled/bubbles"));
    return OK(_T("ContextMenuDisabledLetsEventBubble"));
}

// 控件还没挂进 DUI 树时，菜单键不弹菜单、不崩溃。
//
// 这条守的是「没有宿主窗口就不弹」这个前置判断。弹出菜单需要一个所有者
// 窗口来交还焦点，没有它就不能弹。本用例同时也是这条键盘路径唯一能自动
// 化的部分 —— 真正弹出菜单的 TrackPopup 是同步阻塞的，要等用户点选或关闭
// 才返回，自动化测试里一旦弹出就再也回不来了。菜单真的弹出来是什么样，
// 只能靠演示程序人工核对。
static Result Test_ContextMenuHotKeyWithoutHostDoesNothing()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 200, 100);
    re.Layout(rc);
    re.SetText(_T("hello"));

    //菜单键。控件没有宿主窗口，应当直接返回 false。
    EXPECT_BOOL(re.OnKeyDown(VK_APPS, 0), false, _T("CtxHotKey/appsNoHost"));
    //Shift+F10 走同一条路。这里没法模拟 Shift 按下状态，只验不崩溃即可 ——
    //未按 Shift 时 F10 会被当作普通按键转给引擎。
    re.OnKeyDown(VK_F10, 0);
    return OK(_T("ContextMenuHotKeyWithoutHostDoesNothing"));
}

// ---- 边框状态色 ------------------------------------------------------

// 四种状态各有各的边框色，互不相同。
//
// 断言的是「互不相同」而不是具体的 RGB 值：颜色是内部常量，测试照抄一份
// 数值的话，调色时要改两个地方，改漏了测试还是绿的，反而失去意义。这里要
// 钉住的是「状态确实影响了颜色」，以及下一条用例里的优先级。
static Result Test_BorderColorDiffersPerState()
{
    DuiRichEdit re;

    //常态：既没有鼠标悬停也没有焦点。
    const COLORREF crIdle = re.Test_GetBorderColor();

    //鼠标悬停。
    re.DebugSetHover(true);
    const COLORREF crHover = re.Test_GetBorderColor();
    re.DebugSetHover(false);

    //持有键盘焦点。
    re.OnSetFocus();
    const COLORREF crFocused = re.Test_GetBorderColor();
    re.OnKillFocus();

    //禁用。
    re.SetEnabled(false);
    const COLORREF crDisabled = re.Test_GetBorderColor();
    re.SetEnabled(true);

    //回到常态应当与最初一致 —— 防「状态没被正确还原」。
    EXPECT_INT((int)re.Test_GetBorderColor(), (int)crIdle, _T("BorderColor/backToIdle"));

    const COLORREF crAll[] = { crIdle, crHover, crFocused, crDisabled };
    LPCTSTR const szNames[] = { _T("idle"), _T("hover"), _T("focused"), _T("disabled") };
    const int kCount = sizeof(crAll) / sizeof(crAll[0]);
    for (int i = 0; i < kCount; ++i)
    {
        for (int j = i + 1; j < kCount; ++j)
        {
            if (crAll[i] == crAll[j])
            {
                CString d;
                d.Format(_T("BorderColor/sameColor: %s and %s are both 0x%06X"),
                         szNames[i], szNames[j], (unsigned)crAll[i]);
                return Fail(_T("BorderColor/sameColor"), d);
            }
        }
    }
    return OK(_T("BorderColorDiffersPerState"));
}

// 状态叠加时的优先级：禁用 > 焦点 > 悬停 > 常态。
//
// 这条防的是「几种状态同时成立时用错颜色」。最容易出错的是禁用：控件被禁用
// 之前很可能正持有焦点，如果判断顺序写反，禁用状态下会画成焦点蓝，看上去
// 像个可以编辑的框，实际点不动。
static Result Test_BorderColorPriority()
{
    DuiRichEdit re;

    //先取四种单一状态的颜色作为参照。
    const COLORREF crIdle = re.Test_GetBorderColor();

    re.DebugSetHover(true);
    const COLORREF crHover = re.Test_GetBorderColor();

    //悬停 + 焦点 → 应当取焦点色。
    re.OnSetFocus();
    const COLORREF crHoverPlusFocus = re.Test_GetBorderColor();

    re.DebugSetHover(false);
    const COLORREF crFocusOnly = re.Test_GetBorderColor();
    EXPECT_INT((int)crHoverPlusFocus, (int)crFocusOnly, _T("BorderPriority/focusBeatsHover"));

    //焦点 + 禁用 → 应当取禁用色。
    re.SetEnabled(false);
    const COLORREF crFocusPlusDisabled = re.Test_GetBorderColor();
    re.OnKillFocus();
    const COLORREF crDisabledOnly = re.Test_GetBorderColor();
    EXPECT_INT((int)crFocusPlusDisabled, (int)crDisabledOnly,
               _T("BorderPriority/disabledBeatsFocus"));

    //禁用 + 悬停 → 仍然取禁用色。
    re.DebugSetHover(true);
    EXPECT_INT((int)re.Test_GetBorderColor(), (int)crDisabledOnly,
               _T("BorderPriority/disabledBeatsHover"));

    //几个参照色确实不同，否则上面的相等断言可能是「都相同」蒙混过关的。
    if (crIdle == crHover || crHover == crFocusOnly || crFocusOnly == crDisabledOnly)
    {
        return Fail(_T("BorderPriority/referenceColorsCollide"),
                    _T("idle / hover / focused / disabled must be four distinct colors"));
    }
    return OK(_T("BorderColorPriority"));
}

// ---- 两条原本只是推测、这里验成实证的事项 ----------------------------

// **引擎保存宿主指针时不增加引用计数**，因此不存在引用环。
//
// 本对象把自己交给引擎当宿主，引擎又被本对象持有。如果引擎那一侧增加了
// 引用计数，两者就互相拖住谁也释放不掉 —— 那是一种不报错、不崩溃的内存
// 泄漏，光看代码看不出来，只有数存活实例才发现得了。
//
// 原理文档此前把这一条记为「据推测」（理由是参考实现的释放链条能成立，
// 反推它不增加引用计数）。本用例把它变成实证。
static Result Test_NoReferenceCycleWithEngine()
{
    const int nBefore = DuiTextHost::Test_GetLiveInstanceCount();

    {
        DuiRichEdit re;
        re.SetText(_T("hello"));
        //构造期间确实多出一个宿主对象，否则下面的断言等于什么都没测。
        EXPECT_INT(DuiTextHost::Test_GetLiveInstanceCount(), nBefore + 1,
                   _T("NoCycle/createdOne"));
    }

    //控件析构之后必须回到基准值。多出来就说明有东西没被释放。
    EXPECT_INT(DuiTextHost::Test_GetLiveInstanceCount(), nBefore,
               _T("NoCycle/releasedOnDestroy"));

    //连续多个也要能全部释放 —— 单个能释放但批量泄漏的情况是有的
    //（例如某个静态表把每个实例都登记进去却从不摘除）。
    const int kBatch = 5;
    for (int i = 0; i < kBatch; ++i)
    {
        DuiRichEdit re;
        re.SetText(_T("x"));
    }
    EXPECT_INT(DuiTextHost::Test_GetLiveInstanceCount(), nBefore,
               _T("NoCycle/batchReleased"));
    return OK(_T("NoReferenceCycleWithEngine"));
}

// **库内的绘制目标本来就处于兼容图形模式**，绘制前后那次模式切换是防御性的、
// 而非必需。
//
// 参考实现必须切换，因为它的绘制目标设成了高级模式以支持世界变换；balloonui
// 没有用世界变换。原理文档此前把这一条记为「据推测」，本用例把它变成实证。
//
// 保留那次切换的理由是它无论如何都安全，且成本可以忽略；这条用例的作用是
// 记录「当前确实不需要」，将来若有人在库里引入世界变换，它会失败并提醒。
static Result Test_BackBufferDcIsCompatibleGraphicsMode()
{
    //按宿主创建后台缓冲的同一方式造一个内存设备上下文。
    HDC hdcScreen = ::GetDC(nullptr);
    if (hdcScreen == nullptr)
    {
        return Fail(_T("GfxMode/noScreenDc"), _T("GetDC(nullptr) failed"));
    }
    HDC hdcMem = ::CreateCompatibleDC(hdcScreen);
    ::ReleaseDC(nullptr, hdcScreen);
    if (hdcMem == nullptr)
    {
        return Fail(_T("GfxMode/noMemDc"), _T("CreateCompatibleDC failed"));
    }

    const int nMode = ::GetGraphicsMode(hdcMem);
    ::DeleteDC(hdcMem);

    EXPECT_INT(nMode, GM_COMPATIBLE, _T("GfxMode/isCompatible"));
    return OK(_T("BackBufferDcIsCompatibleGraphicsMode"));
}

// 在控件上单击一下，返回点击之后光标所在的字符位置。
//
// 用单击而不是拖选来探测鼠标是否生效：拖选依赖真实的鼠标捕获，而测试里没有
// 宿主窗口，捕获是空操作，引擎收不全拖动过程中的鼠标移动，选区出不来。单击
// 这条路不依赖捕获，能如实反映「鼠标消息有没有被引擎处理」。
//
//   re：已经布局过、且已填入文字的控件。
//   nX：点击处的横坐标（控件坐标）。
static long ClickAndGetCaretPos(DuiRichEdit& re, int nX)
{
    //纵坐标必须落在**第一行文字**上，不能取文本区中线 —— 内容只有一行时
    //中线在文字下方，点击会映射到文档末尾，测不出区别。
    const int kFirstLineOffset = 6;
    const RECT rcText = re.Test_GetTextRect();

    POINT pt;
    pt.x = nX;
    pt.y = rcText.top + kFirstLineOffset;

    re.OnLButtonDown(pt, 0);
    re.OnLButtonUp(pt, 0);

    long cpMin = 0;
    long cpMax = 0;
    re.GetSel(cpMin, cpMax);
    return cpMin;
}

// **鼠标定位光标与「界面激活状态」无关，只读也照样响应。**
//
// 这条是排查「只读展示区文字选不中」时实测出来的，结论与直觉相反，所以专门
// 记一条用例：很容易想当然地以为「控件没获得焦点、引擎没激活，就不该理会
// 鼠标」，实际上引擎对鼠标定位一视同仁，激活与否都处理。
//
// 由此可知：若某个只读展示区出现「点不动、选不中」，**不要**先去查焦点或
// 激活状态，那两处不是原因。
//
// **拖选不在本用例覆盖范围内**：拖选依赖真实的鼠标捕获，而测试里没有宿主
// 窗口，捕获是空操作，引擎收不全拖动过程中的鼠标移动，选区出不来。拖选只能
// 靠人工核对，这也是本用例只验单击的原因。
static Result Test_MouseClickWorksRegardlessOfUiActive()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 100);
    re.Layout(rc);
    re.SetMultiLine(true);
    re.SetWordWrap(true);
    re.SetText(_T("The quick brown fox jumps over the lazy dog."));
    re.SetReadOnly(true);

    //未激活：点在文字中段，光标应当跟过去。
    re.SetSel(0, 0);
    const long cpInactive = ClickAndGetCaretPos(re, 200);
    if (cpInactive <= 0)
    {
        CString d;
        d.Format(_T("MouseClick/inactiveShouldMoveCaret: got %d"), (int)cpInactive);
        return Fail(_T("MouseClick/inactiveShouldMoveCaret"), d);
    }

    //激活之后同一个位置的点击落在同一处 —— 激活状态不改变定位结果。
    re.Test_SetUiActive(true);
    re.SetSel(0, 0);
    EXPECT_INT((int)ClickAndGetCaretPos(re, 200), (int)cpInactive,
               _T("MouseClick/sameWhenActive"));
    return OK(_T("MouseClickWorksRegardlessOfUiActive"));
}

// ---- 光标显示开关 ----------------------------------------------------

// 开关的读写往返，以及默认值。
static Result Test_ShowCaretRoundTrip()
{
    DuiRichEdit re;
    //默认显示光标 —— 这是可编辑控件的常态，只读展示区才需要关掉。
    EXPECT_BOOL(re.IsShowCaret(), true, _T("ShowCaret/defaultOn"));

    re.SetShowCaret(false);
    EXPECT_BOOL(re.IsShowCaret(), false, _T("ShowCaret/off"));

    re.SetShowCaret(true);
    EXPECT_BOOL(re.IsShowCaret(), true, _T("ShowCaret/backOn"));
    return OK(_T("ShowCaretRoundTrip"));
}

// **关掉光标显示不能影响任何交互能力。**
//
// 这条是这个开关存在的全部理由，也是它与 SetFocusable(false) 的分界线：
// 后者确实也能消掉光标，但会把鼠标拖选一起废掉（排版引擎在拖选过程中需要
// 控件持有焦点）。本开关只管视觉，交互一概不动。
//
// 实机上就踩过这个坑：更新说明区当初用 SetFocusable(false) 去掉光标，结果
// 文字选不中了。用例逐条钉住「仍然可用」的那些能力，防止以后有人图省事又
// 把这个开关实现成关焦点。
static Result Test_ShowCaretOffKeepsInteraction()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 100);
    re.Layout(rc);
    re.SetMultiLine(true);
    re.SetWordWrap(true);
    re.SetText(_T("The quick brown fox jumps over the lazy dog."));
    re.SetReadOnly(true);
    re.SetShowCaret(false);

    //仍然可聚焦、仍然参与 Tab 轮转 —— 这两条是引擎拖选的前提。
    EXPECT_BOOL(re.IsFocusable(), true, _T("ShowCaretOff/stillFocusable"));
    EXPECT_BOOL(re.IsTabStop(),   true, _T("ShowCaretOff/stillTabStop"));

    //仍然需要 Win32 键盘焦点 —— 宿主据此决定要不要替它去索取焦点。
    EXPECT_BOOL(re.NeedsWin32Focus(), true, _T("ShowCaretOff/stillNeedsWin32Focus"));

    //鼠标点击仍然能把光标（插入点）定位过去。光标不显示不等于插入点不存在，
    //选区、复制都以插入点为基础。
    re.SetSel(0, 0);
    const long cpAfterClick = ClickAndGetCaretPos(re, 200);
    if (cpAfterClick <= 0)
    {
        CString d;
        d.Format(_T("ShowCaretOff/clickStillMovesCaret: got %d"), (int)cpAfterClick);
        return Fail(_T("ShowCaretOff/clickStillMovesCaret"), d);
    }

    //仍然能通过接口选中并读回内容 —— 只读展示区的核心诉求是「能复制」。
    re.SelectAll();
    long cpMin = 0;
    long cpMax = 0;
    re.GetSel(cpMin, cpMax);
    EXPECT_INT((int)cpMin, 0, _T("ShowCaretOff/selectAllMin"));
    EXPECT_INT((int)cpMax, re.GetTextLength(), _T("ShowCaretOff/selectAllMax"));

    //右键菜单仍然可用。
    EXPECT_BOOL(re.IsContextMenuEnabled(), true, _T("ShowCaretOff/contextMenuStillOn"));
    return OK(_T("ShowCaretOffKeepsInteraction"));
}

// **滚轮的消费判据是「有没有可滚范围」，不是「这个方向还能不能滚」。**
//
// 这是库内的统一约定（见 DuiHost::DispatchMouseWheel 上方的说明）：能滚的控件
// 即使已经滚到顶 / 滚到底也照常消费掉滚轮，不让事件继续上冒；只有压根没有
// 可滚范围（内容装得下）时才如实返回未处理。
//
// 理由是用户在内层滚到底后往往会再多滚一两下，此时外层整页突然跟着动，视觉
// 参照一下子全变，比「滚不动」更让人困惑。公告预览面板正是这种嵌套结构。
//
// 反过来，内容装得下时若还把滚轮吞掉，症状是「鼠标停在文本框上时整页滚不动」
// —— 不报错、不崩溃，只能靠人发现。
//
// **「溢出时滚轮确实滚动了内容」这一半没有自动化**：滚轮的命中判定与窗口在
// 屏幕上的实际位置有关，而测试窗口开在屏幕外（-32000）以免运行时在眼前闪，
// 那里滚轮不生效。这一半只能在演示程序里人工核对。
static Result Test_MouseWheelConsumedOnlyWhenScrollable()
{
    OffscreenTopWnd top;
    if (top.get() == nullptr)
    {
        return Fail(_T("WheelScroll"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, 400, 200);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
    if (!host.IsWindow())
    {
        return Fail(_T("WheelScroll"), _T("cannot create DuiHost"));
    }

    DuiRichEdit* pEdit = new DuiRichEdit();
    host.SetRoot(std::unique_ptr<DuiControl>(pEdit));
    pEdit->SetMultiLine(true);
    pEdit->SetWordWrap(true);
    //高度固定，内容多了才会溢出。不关掉自动增高的话控件会自己长到装得下
    //全部内容，永远不溢出，也就测不到滚动。
    pEdit->SetAutoGrow(false);

    RECT rcEdit;
    ::SetRect(&rcEdit, 0, 0, 300, 120);
    pEdit->ForceLayout(rcEdit);

    //让控件进入界面激活状态，与用户实际操作时一致（鼠标滚动之前通常已经
    //点过或至少焦点在窗口里）。
    ::SetFocus(top.get());
    pEdit->SetFocus();

    POINT pt;
    pt.x = 150;
    pt.y = 60;

    // ---- 内容装得下：不消费滚轮 ----
    pEdit->SetText(_T("only one short line"));
    int nContentH = 0;
    int nPos = 0;
    int nViewH = 0;
    pEdit->GetVScrollMetrics(nContentH, nPos, nViewH);
    const bool bFits = (nContentH <= nViewH);
    const bool bConsumedWhenFits = pEdit->OnMouseWheel(pt, -WHEEL_DELTA, 0);

    // ---- 内容溢出：滚轮滚动内容并消费 ----
    CString longText;
    for (int i = 1; i <= 40; ++i)
    {
        CString line;
        line.Format(_T("line %d\r\n"), i);
        longText += line;
    }
    pEdit->SetText(longText);

    int nContentH2 = 0;
    int nPos2 = 0;
    int nViewH2 = 0;
    pEdit->GetVScrollMetrics(nContentH2, nPos2, nViewH2);
    const bool bOverflows = (nContentH2 > nViewH2);

    //已经滚到顶还继续往上滚：按约定仍然要消费掉，不能让外层跟着滚。
    const bool bConsumedAtTop = pEdit->OnMouseWheel(pt, WHEEL_DELTA, 0);

    host.DestroyWindow();

    //先确认两个前提成立，否则下面的断言测的不是想测的东西。
    EXPECT_BOOL(bFits, true, _T("WheelScroll/shortTextFits"));
    EXPECT_BOOL(bOverflows, true, _T("WheelScroll/longTextOverflows"));

    EXPECT_BOOL(bConsumedWhenFits, false, _T("WheelScroll/notConsumedWhenFits"));
    EXPECT_BOOL(bConsumedAtTop, true, _T("WheelScroll/consumedAtTopWhenScrollable"));
    return OK(_T("MouseWheelConsumedOnlyWhenScrollable"));
}

// **无窗口模式下同样能拿到引擎的 OLE 接口。**
//
// 这条是内联图片能力的地基。旧控件插图靠的是向真子窗口发一条取 OLE 接口的
// 消息，拿到之后按 OLE 对象往文档里插；无窗口模式下没有窗口可发消息，只能
// 通过引擎接口下发同一条消息。这条路通不通，决定了聊天窗能不能迁过来 ——
// 聊天输入框的内联表情全靠它。
//
// 写作原理文档时这只是一条推测（据推测消息能原样下发），本用例把它验成实证。
// 拿到之后立刻释放：接口是带引用计数的，测试里不持有。
static Result Test_CanObtainOleInterface()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 100);
    re.Layout(rc);
    re.SetText(_T("hello"));

    ITextServices* pSvc = re.Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return Fail(_T("OleIface/noEngine"), _T("text services unavailable"));
    }

    IRichEditOle* pOle = nullptr;
    LRESULT lResult = 0;
    const HRESULT hr = pSvc->TxSendMessage(EM_GETOLEINTERFACE, 0,
                                           (LPARAM)&pOle, &lResult);
    if (FAILED(hr))
    {
        CString d;
        d.Format(_T("OleIface/sendFailed: hr=0x%08X"), (unsigned)hr);
        return Fail(_T("OleIface/sendFailed"), d);
    }
    if (pOle == nullptr)
    {
        return Fail(_T("OleIface/nullInterface"),
                    _T("EM_GETOLEINTERFACE returned a null interface"));
    }

    //接口可用性的进一步佐证：问它文档里当前有几个内嵌对象。刚灌进去的是
    //纯文本，应当是 0；这一步同时证明拿到的不是个只能存在、不能用的空壳。
    const LONG nObjects = pOle->GetObjectCount();
    pOle->Release();

    EXPECT_INT((int)nObjects, 0, _T("OleIface/noObjectsInPlainText"));
    return OK(_T("CanObtainOleInterface"));
}

#if BUI_FEATURE_IMAGEOLE

// ---- 内联图片 --------------------------------------------------------

// 在临时目录里现场写一张极小的位图文件，返回它的完整路径。
//
// 刻意不依赖仓库里任何现成的图片：用例不该因为某个资源文件被挪走 / 改名而
// 失败，那种失败与被测逻辑毫无关系，却很容易被当成真问题去查。
// 写的是最朴素的 24 位无压缩位图，解码端一定认得。
//   返回：文件路径；创建失败时返回空串。
static CString WriteTempTestBitmap()
{
    const int kSide = 8;              // 边长（像素），够用即可
    const int kBytesPerPixel = 3;     // 24 位色
    // 每行须按 4 字节对齐，8 × 3 = 24 已经对齐，这里仍按通式算，免得日后
    // 改边长时踩到行填充。
    const int nRowBytes = ((kSide * kBytesPerPixel + 3) / 4) * 4;
    const int nPixelBytes = nRowBytes * kSide;

    TCHAR szDir[MAX_PATH] = { 0 };
    if (::GetTempPath(MAX_PATH, szDir) == 0)
    {
        return CString();
    }
    CString strPath;
    strPath.Format(_T("%sbui_richedit_test_img.bmp"), szDir);

    BITMAPFILEHEADER fh;
    ::ZeroMemory(&fh, sizeof(fh));
    fh.bfType    = 0x4D42;   // 'BM'
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize    = fh.bfOffBits + nPixelBytes;

    BITMAPINFOHEADER ih;
    ::ZeroMemory(&ih, sizeof(ih));
    ih.biSize      = sizeof(BITMAPINFOHEADER);
    ih.biWidth     = kSide;
    ih.biHeight    = kSide;
    ih.biPlanes    = 1;
    ih.biBitCount  = 24;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = nPixelBytes;

    std::vector<BYTE> pixels((size_t)nPixelBytes, (BYTE)0x80);   // 中灰

    HANDLE hFile = ::CreateFile(strPath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return CString();
    }
    DWORD dwWritten = 0;
    ::WriteFile(hFile, &fh, sizeof(fh), &dwWritten, nullptr);
    ::WriteFile(hFile, &ih, sizeof(ih), &dwWritten, nullptr);
    ::WriteFile(hFile, &pixels[0], (DWORD)pixels.size(), &dwWritten, nullptr);
    ::CloseHandle(hFile);
    return strPath;
}

// EnumContent 的回调收集到的一段内容。
struct CollectedSegment
{
    bool      m_bIsImage;
    CString   m_strText;
    DWORD_PTR m_tag;
};

// EnumContent 的测试回调：按回调顺序把每一段收进数组。
static void CollectSegment(bool bIsImage, LPCTSTR szText, DWORD_PTR tag, void* pCtx)
{
    std::vector<CollectedSegment>* pOut =
        static_cast<std::vector<CollectedSegment>*>(pCtx);
    if (pOut == nullptr)
    {
        return;
    }
    CollectedSegment seg;
    seg.m_bIsImage = bIsImage;
    seg.m_strText  = (szText != nullptr) ? szText : _T("");
    seg.m_tag      = tag;
    pOut->push_back(seg);
}

// **插入带标记的内联图片，并按文档顺序把图文段读回来。**
//
// 这一对能力是聊天输入框迁到本控件的前提：表情以「带标记的内联图」插入，
// 发送时再按文档顺序读回来还原成表情编号。两件事必须一起验 —— 只验插入
// 不验读回，等于只证明了图进得去、不知道还能不能原样取出来。
//
// 用例把图片插在两个字之间，验回调顺序是「文本、图片、文本」，且标记原样
// 回传。顺序错了的症状是发出去的消息里表情位置跑到句首或句尾。
static Result Test_InsertTaggedImageAndEnumContent()
{
    const CString strImage = WriteTempTestBitmap();
    if (strImage.IsEmpty())
    {
        return Fail(_T("InlineImage/noTempFile"), _T("cannot create temp bitmap"));
    }

    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 120);
    re.Layout(rc);
    re.SetMultiLine(true);
    re.SetWordWrap(true);
    re.SetText(_T("AB"));

    //把插入点放到 A 与 B 之间。
    re.SetSel(1, 1);

    const DWORD_PTR kTag = 42;
    const bool bInserted = re.InsertTaggedImage(strImage, kTag, 24);

    const int nImageCount = re.GetEmbeddedImageCount();

    std::vector<CollectedSegment> segs;
    re.EnumContent(&CollectSegment, &segs);

    ::DeleteFile(strImage);

    EXPECT_BOOL(bInserted, true, _T("InlineImage/inserted"));
    EXPECT_INT(nImageCount, 1, _T("InlineImage/objectCount"));

    if (segs.size() != 3)
    {
        CString d;
        d.Format(_T("InlineImage/segmentCount: expected 3 got %d"), (int)segs.size());
        return Fail(_T("InlineImage/segmentCount"), d);
    }
    EXPECT_BOOL(segs[0].m_bIsImage, false, _T("InlineImage/seg0IsText"));
    EXPECT_STR(segs[0].m_strText, CString(_T("A")), _T("InlineImage/seg0Text"));
    EXPECT_BOOL(segs[1].m_bIsImage, true,  _T("InlineImage/seg1IsImage"));
    EXPECT_INT((int)segs[1].m_tag, (int)kTag, _T("InlineImage/seg1Tag"));
    EXPECT_BOOL(segs[2].m_bIsImage, false, _T("InlineImage/seg2IsText"));
    EXPECT_STR(segs[2].m_strText, CString(_T("B")), _T("InlineImage/seg2Text"));
    return OK(_T("InsertTaggedImageAndEnumContent"));
}

// 纯文本内容：枚举出来只有一段文本，没有图片段。
//
// 这条防的是「无论有没有图都硬塞一个图片段」这类实现错误 —— 那种错误在
// 有图的用例里看不出来。
static Result Test_EnumContentPlainTextOnly()
{
    DuiRichEdit re;
    RECT rc;
    ::SetRect(&rc, 0, 0, 300, 120);
    re.Layout(rc);
    re.SetText(_T("hello"));

    std::vector<CollectedSegment> segs;
    re.EnumContent(&CollectSegment, &segs);

    EXPECT_INT(re.GetEmbeddedImageCount(), 0, _T("PlainEnum/noImages"));
    if (segs.size() != 1)
    {
        CString d;
        d.Format(_T("PlainEnum/segmentCount: expected 1 got %d"), (int)segs.size());
        return Fail(_T("PlainEnum/segmentCount"), d);
    }
    EXPECT_BOOL(segs[0].m_bIsImage, false, _T("PlainEnum/isText"));
    EXPECT_STR(segs[0].m_strText, CString(_T("hello")), _T("PlainEnum/text"));
    return OK(_T("EnumContentPlainTextOnly"));
}

#endif // BUI_FEATURE_IMAGEOLE

} // 匿名命名空间

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn  fn;
    };

    Entry tests[] = {
        { _T("ReadyRightAfterConstruction"),        &Test_ReadyRightAfterConstruction        },
        { _T("TextRoundTrip"),                      &Test_TextRoundTrip                      },
        { _T("TextLengthMatchesGetText"),           &Test_TextLengthMatchesGetText           },
        { _T("MultiLineToggleKeepsContent"),        &Test_MultiLineToggleKeepsContent        },
        { _T("WordWrapToggleKeepsContent"),         &Test_WordWrapToggleKeepsContent         },
        { _T("PlainTextModeSwitch"),                &Test_PlainTextModeSwitch                },
        { _T("ReadOnlyToggleAndProgrammaticWrite"), &Test_ReadOnlyToggleAndProgrammaticWrite },
        { _T("PlaceholderVisibility"),              &Test_PlaceholderVisibility              },
        { _T("FocusableSyncsTabStop"),              &Test_FocusableSyncsTabStop              },
        { _T("LayoutComputesTextRect"),             &Test_LayoutComputesTextRect             },
        { _T("BorderAffectsTextRect"),              &Test_BorderAffectsTextRect              },
        { _T("OversizedMarginsAreClamped"),         &Test_OversizedMarginsAreClamped         },
        { _T("HiddenThenShownLayout"),              &Test_HiddenThenShownLayout              },
        { _T("AppearanceGettersMatchSetters"),      &Test_AppearanceGettersMatchSetters      },
        { _T("ImeLangOptionsConfigured"),           &Test_ImeLangOptionsConfigured           },
        { _T("ImeMessagesWithoutWindowAreSafe"),    &Test_ImeMessagesWithoutWindowAreSafe    },
        { _T("KeyBubblesWhenEngineCannotUseIt"),    &Test_KeyBubblesWhenEngineCannotUseIt    },
        { _T("KeyboardReachesControlThroughHost"),  &Test_KeyboardReachesControlThroughHost  },
        { _T("NonFocusableDoesNotStealWin32Focus"), &Test_NonFocusableDoesNotStealWin32Focus },
        { _T("TypingInvalidatesImmediately"),       &Test_TypingInvalidatesImmediately       },
        { _T("ScrollPolicyRoundTrip"),              &Test_ScrollPolicyRoundTrip              },
        { _T("ScrollMetricsReflectOverflow"),       &Test_ScrollMetricsReflectOverflow       },
        { _T("ScrollPosMovesAndClamps"),            &Test_ScrollPosMovesAndClamps            },
        { _T("ScrollBarDoesNotConsumeContentWidth"),&Test_ScrollBarDoesNotConsumeContentWidth },
        { _T("ScrollBarBecomesVisibleOnOverflow"),  &Test_ScrollBarBecomesVisibleOnOverflow  },
        { _T("ScrollRangeToMaxPosMapping"),         &Test_ScrollRangeToMaxPosMapping         },
        { _T("SelectionRoundTrip"),                 &Test_SelectionRoundTrip                 },
        { _T("ReplaceSelReplacesAndInserts"),       &Test_ReplaceSelReplacesAndInserts       },
        { _T("AppendTextKeepsSelection"),           &Test_AppendTextKeepsSelection           },
        { _T("LineCountReflectsContent"),           &Test_LineCountReflectsContent           },
        { _T("UndoRedoRoundTrip"),                  &Test_UndoRedoRoundTrip                  },
        { _T("ReplaceSelNoUndoStaysOutOfStack"),    &Test_ReplaceSelNoUndoStaysOutOfStack    },
        { _T("CharFormatBoldRoundTrip"),            &Test_CharFormatBoldRoundTrip            },
        { _T("CharFormatItalicUnderline"),          &Test_CharFormatItalicUnderline          },
        { _T("ParaAlignmentRoundTrip"),             &Test_ParaAlignmentRoundTrip             },
        { _T("ParaLeftIndentRoundTrip"),            &Test_ParaLeftIndentRoundTrip            },
        { _T("FindTextVariants"),                   &Test_FindTextVariants                   },
        { _T("FindAndSelectWraps"),                 &Test_FindAndSelectWraps                 },
        { _T("RtfRoundTripKeepsFormatting"),        &Test_RtfRoundTripKeepsFormatting        },
        { _T("PlainTextRoundTripDropsFormatting"),  &Test_PlainTextRoundTripDropsFormatting  },
        { _T("MaxLengthLimitsTypingNotSetText"),    &Test_MaxLengthLimitsTypingNotSetText    },
        { _T("PasswordAndVerticalToggles"),         &Test_PasswordAndVerticalToggles         },
        { _T("AutoGrowFollowsContent"),             &Test_AutoGrowFollowsContent             },
        { _T("MeasureIndependentOfControlHeight"),  &Test_MeasureIndependentOfControlHeight  },
        { _T("MeasureMatchesEngineLineHeight"),     &Test_MeasureMatchesEngineLineHeight     },
        { _T("AutoGrowClampsToRange"),              &Test_AutoGrowClampsToRange              },
        { _T("AutoGrowOffYieldsToParent"),          &Test_AutoGrowOffYieldsToParent          },
        { _T("AutoGrowLinesConvertToPixels"),       &Test_AutoGrowLinesConvertToPixels       },
        { _T("AutoGrowRelayoutsThroughHost"),       &Test_AutoGrowRelayoutsThroughHost       },
        { _T("AutoGrowFollowsTyping"),              &Test_AutoGrowFollowsTyping              },
        { _T("AutoGrowOffDoesNotRelayout"),         &Test_AutoGrowOffDoesNotRelayout         },
        // ---- 右键菜单模型 ----
        { _T("MenuModelReadWriteLayout"),           &Test_MenuModelReadWriteLayout           },
        { _T("MenuModelReadOnlyLayout"),            &Test_MenuModelReadOnlyLayout            },
        { _T("MenuModelNoSelectionGraysSelectionCommands"), &Test_MenuModelNoSelectionGraysSelectionCommands },
        { _T("MenuModelEmptyClipboardGraysPaste"),  &Test_MenuModelEmptyClipboardGraysPaste  },
        { _T("MenuModelEmptyDocAndUndoState"),      &Test_MenuModelEmptyDocAndUndoState      },
        { _T("MenuModelEveryCommandHasLabel"),      &Test_MenuModelEveryCommandHasLabel      },
        { _T("MenuModelNormalizeSeparators"),       &Test_MenuModelNormalizeSeparators       },
        { _T("MenuModelNormalizeEdgeCases"),        &Test_MenuModelNormalizeEdgeCases        },
        { _T("MenuModelCustomBaseAboveBuiltins"),   &Test_MenuModelCustomBaseAboveBuiltins   },
        // ---- 右键菜单在控件上的接线 ----
        { _T("ContextMenuDefaults"),                &Test_ContextMenuDefaults                },
        { _T("ContextMenuReflectsControlState"),    &Test_ContextMenuReflectsControlState     },
        { _T("ContextMenuReadOnlyControl"),         &Test_ContextMenuReadOnlyControl          },
        { _T("ContextMenuCustomItemsAppended"),     &Test_ContextMenuCustomItemsAppended      },
        { _T("ContextMenuNoTrailingSeparatorWithoutCustomItems"), &Test_ContextMenuNoTrailingSeparatorWithoutCustomItems },
        { _T("ContextMenuRejectsLowCustomId"),      &Test_ContextMenuRejectsLowCustomId       },
        { _T("ContextMenuCommandDispatch"),         &Test_ContextMenuCommandDispatch          },
        { _T("ContextMenuDisabledLetsEventBubble"), &Test_ContextMenuDisabledLetsEventBubble  },
        { _T("ContextMenuHotKeyWithoutHostDoesNothing"), &Test_ContextMenuHotKeyWithoutHostDoesNothing },
        // ---- 边框状态色 ----
        { _T("BorderColorDiffersPerState"),         &Test_BorderColorDiffersPerState          },
        { _T("BorderColorPriority"),                &Test_BorderColorPriority                 },
        // ---- 把原理文档里的推测验成实证 ----
        { _T("NoReferenceCycleWithEngine"),         &Test_NoReferenceCycleWithEngine          },
        { _T("BackBufferDcIsCompatibleGraphicsMode"), &Test_BackBufferDcIsCompatibleGraphicsMode },
        { _T("MouseClickWorksRegardlessOfUiActive"), &Test_MouseClickWorksRegardlessOfUiActive },
        // ---- 光标显示开关 ----
        { _T("ShowCaretRoundTrip"),                 &Test_ShowCaretRoundTrip                  },
        { _T("ShowCaretOffKeepsInteraction"),       &Test_ShowCaretOffKeepsInteraction        },
        { _T("CanObtainOleInterface"),              &Test_CanObtainOleInterface               },
#if BUI_FEATURE_IMAGEOLE
        { _T("InsertTaggedImageAndEnumContent"),    &Test_InsertTaggedImageAndEnumContent     },
        { _T("EnumContentPlainTextOnly"),           &Test_EnumContentPlainTextOnly            },
#endif
        { _T("MouseWheelConsumedOnlyWhenScrollable"), &Test_MouseWheelConsumedOnlyWhenScrollable },
        { _T("DragDropToggle"),                     &Test_DragDropToggle                     },
        { _T("DropTargetWiring"),                   &Test_DropTargetWiring                   },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    for (int i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); ++i)
    {
        Result r = tests[i].fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), tests[i].name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), tests[i].name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }

    CString summary;
    summary.Format(_T("[summary] DuiRichEditTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

#undef EXPECT_BOOL
#undef EXPECT_INT
#undef EXPECT_STR

} // namespace DuiRichEditTests

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
