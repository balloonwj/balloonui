/**
 *  DuiEdit 的单元测试实现。用例说明见 DuiEditTests.h。
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_EDIT
#include "DuiEditTests.h"
#include "../DuiHost.h"
#include "../DuiNotify.h"
#if BUI_FEATURE_SEARCHBOX
#include "../Controls/Input/DuiSearchBox.h"
#endif
#if BUI_FEATURE_COMBOBOX
#include "../Controls/Input/DuiComboBox.h"
#endif

namespace balloonwjui {

namespace DuiEditTests {

namespace {

// =================================================================
// 常量
// =================================================================

// ---- 图标矩形几何用例的输入与期望值 ----
//
// 这一组数值是旧输入框的用例固定下来的基准，这里原样照搬。两套用例用同一组
// 数字的好处是：换算规则一旦被改动，新旧用例会一起变红，能立刻看出这是有意
// 为之的行为变更，而不是某一侧写错了。
const int kGeomHostW       = 200;   // 控件矩形宽度（像素）
const int kGeomHostH       = 32;    // 控件矩形高度（像素）
const int kGeomLeftGutter  = 32;    // 左侧图标栏宽度（像素）
const int kGeomRightGutter = 28;    // 右侧图标栏宽度（像素）
const int kGeomBorder      = 1;     // 边框宽度（像素）
const int kGeomMarginV     = 2;     // 图标矩形相对控件上下边的额外内缩（像素）

const int kGeomLeftRectLeft   = 1;    // 左侧图标矩形左边界 = 0 + 边框
const int kGeomLeftRectRight  = 33;   // 左侧图标矩形右边界 = 1 + 图标栏宽 32
const int kGeomLeftRectTop    = 3;    // 上边界 = 0 + 边框 + 内缩
const int kGeomLeftRectBottom = 29;   // 下边界 = 32 - 边框 - 内缩
const int kGeomRightRectLeft  = 171;  // 右侧图标矩形左边界 = 199 - 图标栏宽 28
const int kGeomRightRectRight = 199;  // 右侧图标矩形右边界 = 200 - 边框

// ---- 布局类用例的控件尺寸 ----
//
// 高度取得比一行文字高出不少，是为了让「单行垂直居中」算出来的上内边距明显
// 大于不居中时的默认上内边距。高度贴着行高时两者只差一两个像素，字体一换就
// 可能因取整而相等，用例会变得不稳定。
const int kCtrlW = 200;
const int kCtrlH = 48;

// ---- 图标栏宽度 ----
//
// 三个互不相同的值，便于在同一条用例里连续改宽度并从文本区的变化上分辨出来。
const int kIconGutter      = 24;
const int kWideIconGutter  = 40;
const int kWiderIconGutter = 60;

// 字形图标的文字颜色。用例不校验绘制结果，取什么颜色都行。
const COLORREF kGlyphColor = RGB(0, 0, 0);

// ---- 通知观察窗口 ----

// 窗口类名。
const TCHAR* const kNotifyWndClassName = _T("DuiEditTestNotifyWnd");

// 窗口落点与尺寸（像素）。必须是可见窗口 —— Win32 的焦点接口对不可见窗口的
// 行为不可靠 —— 所以挪到屏幕外，免得测试运行时在用户眼前闪一下。
const int kOffscreenX = -32000;
const int kOffscreenY = -32000;
const int kOffscreenW = 400;
const int kOffscreenH = 200;

// 宿主子窗口的尺寸（像素）。通知类用例不校验布局，够大即可。
const int kHostW = 300;
const int kHostH = 40;

// ---- 纯文本模式用例的样本文本 ----
//
// 取登录窗账号框的真实形态：6 个字符，末尾不带空白。富文本模式下，第 7 个
// 位置是文档末尾那个看不见的段落结束符，选区能扩进去，界面上表现为文字后面
// 多出一小块高亮。
const TCHAR* const kPlainSample = _T("MY0127");
const long kPlainSampleLen = 6;     // kPlainSample 的字符数

// 选区终点取 -1 表示「一直到文档末尾」，与引擎的约定一致。
const long kSelToEnd = -1;

// =================================================================
// 断言助手（与 DuiRichEditTests 保持一致）
// =================================================================

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

// =================================================================
// 通知观察装置
// =================================================================
//
// 控件的通知是经 DuiHost 同步 SendMessage 到「宿主窗口的父窗口」的，所以要
// 观察通知就必须有一个真的父窗口。下面这一小套东西就是那个父窗口：它只做一
// 件事 —— 把收到的 WM_DUI_NOTIFY 按通知码计数。
//
// 计数放在文件级静态变量里而不是对象成员上，是因为窗口过程是 C 风格回调，
// 拿不到对象上下文。每条用例开始前调一次 ResetNotifyCounters 清零。

//收到的 DUIN_VALUECHANGED 条数
static int s_nNotifyValueChanged = 0;
//收到的 DUIN_EDIT_ENTER 条数
static int s_nNotifyEnter = 0;
//收到的 DUIN_EDIT_ESCAPE 条数
static int s_nNotifyEscape = 0;

static void ResetNotifyCounters()
{
    s_nNotifyValueChanged = 0;
    s_nNotifyEnter        = 0;
    s_nNotifyEscape       = 0;
}

// 计数窗口的窗口过程。
//   hWnd / uMsg / wParam / lParam：标准窗口过程参数。
//   返回：WM_DUI_NOTIFY 一律返回 0，其余消息交给系统默认处理。
static LRESULT CALLBACK NotifyCounterWndProc(HWND hWnd, UINT uMsg,
                                             WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DUI_NOTIFY)
    {
        const DuiNotify* pNotify = (const DuiNotify*)lParam;
        if (pNotify != NULL)
        {
            switch (pNotify->code)
            {
            //文字内容发生变化。SetText 会发这一条，SetTextNoNotify 不发，
            //两者的差别正是靠这个计数分辨出来的。
            case DUIN_VALUECHANGED:
                ++s_nNotifyValueChanged;
                break;

            //单行模式下用户按了回车。多行模式下回车归排版引擎处理，不该发。
            case DuiEdit::DUIN_EDIT_ENTER:
                ++s_nNotifyEnter;
                break;

            //用户按了 Esc。单行多行都应当发。
            case DuiEdit::DUIN_EDIT_ESCAPE:
                ++s_nNotifyEscape;
                break;

            //其余通知（焦点进出、图标点击等）本文件不关心，忽略即可。
            default:
                break;
            }
        }
        return 0;
    }
    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// 临时顶层窗口，用来接收并统计控件发出的通知。
//
// 构造即创建、析构即销毁；创建失败时 get() 返回空句柄，调用方据此让用例
// 报错退出而不是继续往下走。
class NotifyCounterWnd
{
public:
    NotifyCounterWnd()
        : m_hwnd(NULL)
    {
        WNDCLASSEX wc;
        ::memset(&wc, 0, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = &NotifyCounterWndProc;
        wc.hInstance     = ::GetModuleHandle(NULL);
        wc.lpszClassName = kNotifyWndClassName;
        //第二次以后注册会失败，这是预期内的（同一个类名注册一次就够），
        //不必判断返回值。
        ::RegisterClassEx(&wc);

        m_hwnd = ::CreateWindowEx(
            WS_EX_TOOLWINDOW, kNotifyWndClassName, _T(""),
            WS_POPUP | WS_VISIBLE,
            kOffscreenX, kOffscreenY, kOffscreenW, kOffscreenH,
            NULL, NULL, ::GetModuleHandle(NULL), NULL);
    }

    ~NotifyCounterWnd()
    {
        if (m_hwnd != NULL)
        {
            ::DestroyWindow(m_hwnd);
            m_hwnd = NULL;
        }
    }

    HWND get() const { return m_hwnd; }

private:
    HWND m_hwnd;    // 顶层窗口句柄；构造时创建、析构时销毁，为空表示创建失败
};

// 只为统计 OnTextChanged 被调用了几次的测试子类。
//
// OnTextChanged 是 protected 的内部钩子，外部观察不到调用次数，只能靠子类
// 覆写把次数记下来。除计数外行为与基类完全一致。
class CountingEdit : public DuiEdit
{
public:
    CountingEdit()
        : m_nTextChangedCount(0)
    {
    }

    // OnTextChanged 至今被调用了多少次。
    //   返回：调用次数，从 0 开始累加。
    int GetTextChangedCount() const { return m_nTextChangedCount; }

protected:
    void OnTextChanged() override
    {
        DuiEdit::OnTextChanged();
        ++m_nTextChangedCount;
    }

private:
    int m_nTextChangedCount;    // OnTextChanged 的累计调用次数
};

// =================================================================
// 一、默认状态
// =================================================================

//新建出来的控件必须已经是「普通输入框」的样子。
//
//这一条钉住的是构造函数里那几行显式设置不会被误删：基类 DuiRichEdit 的默认值
//是按富文本场景定的（多行 + 自动换行），本类必须显式改成单行 + 不换行。少了
//这一步的症状很隐蔽 —— 登录窗的用户名框看着正常，敲回车却换了行。
static Result Test_Defaults()
{
    DuiEdit e;

    EXPECT_BOOL(e.Test_IsEngineReady(), true, _T("Defaults/engineReady"));

    //单行、不换行。
    EXPECT_BOOL(e.IsMultiLine(), false, _T("Defaults/singleLine"));
    EXPECT_BOOL(e.IsWordWrap(),  false, _T("Defaults/noWordWrap"));

    //非密码框，也没有显隐按钮与明文状态。
    EXPECT_BOOL(e.IsPassword(),         false, _T("Defaults/notPassword"));
    EXPECT_BOOL(e.HasEyeToggle(),       false, _T("Defaults/noEyeToggle"));
    EXPECT_BOOL(e.IsPasswordRevealed(), false, _T("Defaults/notRevealed"));
    EXPECT_BOOL(e.IsPasswordMode(),     false, _T("Defaults/engineMaskOff"));

    //单行垂直居中默认开启。
    EXPECT_BOOL(e.IsVerticalCenter(), true, _T("Defaults/verticalCenterOn"));

    //两侧都没有图标，也都不可点击。
    EXPECT_INT(e.GetIconWidth(DuiEdit::LeftIcon),  0, _T("Defaults/noLeftIcon"));
    EXPECT_INT(e.GetIconWidth(DuiEdit::RightIcon), 0, _T("Defaults/noRightIcon"));
    EXPECT_BOOL(e.IsIconClickable(DuiEdit::LeftIcon),  false, _T("Defaults/leftNotClickable"));
    EXPECT_BOOL(e.IsIconClickable(DuiEdit::RightIcon), false, _T("Defaults/rightNotClickable"));

    //粘贴一律按纯文本处理 —— 普通输入框不该把网页上的字体与颜色带进来。
    EXPECT_BOOL(e.GetPasteAsPlainTextDefault(), true, _T("Defaults/pastePlainText"));
    return OK(_T("Defaults"));
}

// =================================================================
// 二、图标矩形几何
// =================================================================

//静态函数 ComputeIconRect 的换算规则。
//
//它是纯函数，调用方（例如搜索框）要靠它对齐自己画的命中区，所以规则必须稳定。
//这里用旧输入框用例固定下来的那组数值当基准。
static Result Test_ComputeIconRectGeometry()
{
    RECT host;
    ::SetRect(&host, 0, 0, kGeomHostW, kGeomHostH);

    //左侧图标贴着左边框内侧。
    RECT rcLeft = DuiEdit::ComputeIconRect(host, DuiEdit::LeftIcon,
                                           kGeomLeftGutter, kGeomBorder, kGeomMarginV);
    EXPECT_INT(rcLeft.left,   kGeomLeftRectLeft,   _T("Icon/L/left"));
    EXPECT_INT(rcLeft.right,  kGeomLeftRectRight,  _T("Icon/L/right"));
    EXPECT_INT(rcLeft.top,    kGeomLeftRectTop,    _T("Icon/L/top"));
    EXPECT_INT(rcLeft.bottom, kGeomLeftRectBottom, _T("Icon/L/bottom"));

    //右侧图标贴着右边框内侧，宽度朝左侧展开。
    RECT rcRight = DuiEdit::ComputeIconRect(host, DuiEdit::RightIcon,
                                            kGeomRightGutter, kGeomBorder, kGeomMarginV);
    EXPECT_INT(rcRight.right,  kGeomRightRectRight, _T("Icon/R/right"));
    EXPECT_INT(rcRight.left,   kGeomRightRectLeft,  _T("Icon/R/left"));
    //上下与左侧一致 —— 两侧共用同一套纵向换算。
    EXPECT_INT(rcRight.top,    kGeomLeftRectTop,    _T("Icon/R/top"));
    EXPECT_INT(rcRight.bottom, kGeomLeftRectBottom, _T("Icon/R/bottom"));

    //宽度为 0 表示没有图标，返回空矩形。
    RECT rcZero = DuiEdit::ComputeIconRect(host, DuiEdit::LeftIcon,
                                           0, kGeomBorder, kGeomMarginV);
    EXPECT_INT(rcZero.left,   0, _T("Icon/zero/left"));
    EXPECT_INT(rcZero.top,    0, _T("Icon/zero/top"));
    EXPECT_INT(rcZero.right,  0, _T("Icon/zero/right"));
    EXPECT_INT(rcZero.bottom, 0, _T("Icon/zero/bottom"));

    //负宽度同样按「没有图标」处理，不能算出反向矩形。
    RECT rcNeg = DuiEdit::ComputeIconRect(host, DuiEdit::RightIcon,
                                          -kGeomLeftGutter, kGeomBorder, kGeomMarginV);
    EXPECT_INT(rcNeg.left,   0, _T("Icon/negative/left"));
    EXPECT_INT(rcNeg.top,    0, _T("Icon/negative/top"));
    EXPECT_INT(rcNeg.right,  0, _T("Icon/negative/right"));
    EXPECT_INT(rcNeg.bottom, 0, _T("Icon/negative/bottom"));
    return OK(_T("ComputeIconRectGeometry"));
}

// =================================================================
// 三、图标栏与文本区的关系
// =================================================================

// 建一个已经布局过的控件，供图标与内边距类用例使用。
// **必须先给矩形**：文本区是由控件矩形减去边框和内边距算出来的，没有矩形
// 时怎么改内边距都看不出差别。
//   e：待布局的控件。参数类型取基类，是为了让纯文本模式用例里作对照的
//      DuiRichEdit 也能用同一个助手。
static void SetUpLaidOutEdit(DuiRichEdit& e)
{
    RECT rc;
    ::SetRect(&rc, 0, 0, kCtrlW, kCtrlH);
    e.SetRect(rc);
}

//设置左侧图标之后，文本区的左边界要按图标栏宽度整体右移。
//
//图标栏的宽度是从文本区里让出来的 —— 少了这一步的症状是文字压在图标上面。
static Result Test_LeftIconShrinksTextRect()
{
    DuiEdit e;
    SetUpLaidOutEdit(e);

    const int nLeftBefore = (int)e.Test_GetTextRect().left;

    e.SetIconGlyph(DuiEdit::LeftIcon, kIconGutter, _T("Q"), kGlyphColor);
    EXPECT_INT(e.GetIconWidth(DuiEdit::LeftIcon), kIconGutter, _T("LeftIcon/widthRecorded"));

    const int nLeftAfter = (int)e.Test_GetTextRect().left;
    EXPECT_INT(nLeftAfter - nLeftBefore, kIconGutter, _T("LeftIcon/textShiftedRight"));

    //右边界不受左侧图标影响。
    EXPECT_INT(e.GetIconWidth(DuiEdit::RightIcon), 0, _T("LeftIcon/rightUntouched"));
    return OK(_T("LeftIconShrinksTextRect"));
}

//清除图标之后，让出的宽度要还给文本区。
static Result Test_ClearIconRestoresTextRect()
{
    DuiEdit e;
    SetUpLaidOutEdit(e);

    const int nLeftBefore = (int)e.Test_GetTextRect().left;

    e.SetIconGlyph(DuiEdit::LeftIcon, kIconGutter, _T("Q"), kGlyphColor);
    const int nLeftWithIcon = (int)e.Test_GetTextRect().left;
    EXPECT_INT(nLeftWithIcon - nLeftBefore, kIconGutter, _T("ClearIcon/shiftedFirst"));

    e.ClearIcon(DuiEdit::LeftIcon);
    EXPECT_INT(e.GetIconWidth(DuiEdit::LeftIcon), 0, _T("ClearIcon/widthZeroed"));
    EXPECT_INT(e.Test_GetTextRect().left, nLeftBefore, _T("ClearIcon/textRestored"));
    return OK(_T("ClearIconRestoresTextRect"));
}

//传一个空画法等同于清除图标，**并且要重排**。
//
//这一条钉住的是一个刚修掉的缺陷：判断「要不要重排」时如果拿传进来的宽度参数
//去比对，传「与当前相同的宽度 + 空画法」就会得出「宽度没变」的错误结论 ——
//实际上宽度已经被归零了，文本区却停在内缩状态上，左边留着一块永远画不出图标
//的空白。正确的做法是拿最终写进去的宽度去比。
static Result Test_SetIconWithNullPainterClearsGutter()
{
    DuiEdit e;
    SetUpLaidOutEdit(e);

    const int nLeftBefore = (int)e.Test_GetTextRect().left;

    e.SetIconGlyph(DuiEdit::LeftIcon, kIconGutter, _T("Q"), kGlyphColor);
    EXPECT_INT(e.GetIconWidth(DuiEdit::LeftIcon), kIconGutter, _T("NullPainter/iconSet"));

    //宽度传的是「与当前相同的值」，画法传空 —— 这正是旧实现漏掉重排的那条路。
    e.SetIcon(DuiEdit::LeftIcon, kIconGutter, NULL);
    EXPECT_INT(e.GetIconWidth(DuiEdit::LeftIcon), 0, _T("NullPainter/widthZeroed"));
    EXPECT_INT(e.Test_GetTextRect().left, nLeftBefore, _T("NullPainter/textRestored"));
    return OK(_T("SetIconWithNullPainterClearsGutter"));
}

// =================================================================
// 四、密码模式
// =================================================================

//密码模式的开关，以及退出密码模式时「明文显示」一并复位。
//
//不复位的话，下次再把控件设成密码框会直接停在明文状态上 —— 密码裸露在界面
//上，而用户并没有点过显隐按钮。
static Result Test_PasswordToggleResetsReveal()
{
    DuiEdit e;
    e.SetText(_T("secret"));

    e.SetPassword(true);
    EXPECT_BOOL(e.IsPassword(),     true, _T("Password/on"));
    EXPECT_BOOL(e.IsPasswordMode(), true, _T("Password/engineMaskOn"));
    //遮蔽的只是显示，内容读取不受影响。
    EXPECT_STR(e.GetText(), _T("secret"), _T("Password/contentReadable"));

    e.SetPasswordRevealed(true);
    EXPECT_BOOL(e.IsPasswordRevealed(), true,  _T("Password/revealed"));
    EXPECT_BOOL(e.IsPasswordMode(),     false, _T("Password/engineMaskOffWhenRevealed"));

    e.SetPassword(false);
    EXPECT_BOOL(e.IsPassword(),         false, _T("Password/off"));
    EXPECT_BOOL(e.IsPasswordRevealed(), false, _T("Password/revealResetOnOff"));
    EXPECT_BOOL(e.IsPasswordMode(),     false, _T("Password/engineMaskOffAfterOff"));

    //非密码框时设明文无效果 —— 没有可遮蔽的东西，也就没有「明文」一说。
    e.SetPasswordRevealed(true);
    EXPECT_BOOL(e.IsPasswordRevealed(), false, _T("Password/revealIgnoredWhenNotPassword"));
    return OK(_T("PasswordToggleResetsReveal"));
}

//关闭显隐按钮时，若当前正处于明文状态，要先恢复成遮蔽再关。
//
//按钮没了就再也没法把密码收回去，密码会一直裸露在界面上。
static Result Test_HidingEyeToggleRestoresMask()
{
    DuiEdit e;
    e.SetPassword(true);
    e.SetShowEyeToggle(true);
    e.SetPasswordRevealed(true);

    EXPECT_BOOL(e.HasEyeToggle(),       true,  _T("EyeOff/toggleOnFirst"));
    EXPECT_BOOL(e.IsPasswordRevealed(), true,  _T("EyeOff/revealedFirst"));
    EXPECT_BOOL(e.IsPasswordMode(),     false, _T("EyeOff/maskOffWhileRevealed"));

    e.SetShowEyeToggle(false);
    EXPECT_BOOL(e.HasEyeToggle(),       false, _T("EyeOff/toggleOff"));
    EXPECT_BOOL(e.IsPasswordRevealed(), false, _T("EyeOff/revealPulledBack"));
    EXPECT_BOOL(e.IsPasswordMode(),     true,  _T("EyeOff/maskRestored"));
    return OK(_T("HidingEyeToggleRestoresMask"));
}

//密码显隐按钮与右侧图标互斥：按钮可见时，文本区右侧让出的是按钮的宽度，
//与右侧图标的宽度完全无关。
//
//本来最直接的写法是断言「按钮可见时右侧图标矩形为空」，但取图标矩形的方法是
//私有的，外部拿不到。改为从文本区右边界间接验证：先记下只有图标时让出多少，
//再开按钮看让出的量变成了另一个值，然后把图标改宽 —— 右边界纹丝不动才说明
//确实是按钮说了算。按钮保留宽度是 .cpp 里的内部常量，用例刻意不照抄它的数值，
//只验证「与图标宽度无关」这个关系。
static Result Test_EyeToggleWinsOverRightIcon()
{
    DuiEdit e;
    SetUpLaidOutEdit(e);

    const int nRightBare = (int)e.Test_GetTextRect().right;

    //一、只有右侧图标：让出的正是图标宽度。
    e.SetIconGlyph(DuiEdit::RightIcon, kWideIconGutter, _T("x"), kGlyphColor);
    const int nRightWithIcon = (int)e.Test_GetTextRect().right;
    EXPECT_INT(nRightBare - nRightWithIcon, kWideIconGutter, _T("Eye/iconReservesWidth"));

    //二、开出显隐按钮：让出的量换成了按钮那一份，与图标那份不同。
    e.SetPassword(true);
    e.SetShowEyeToggle(true);
    const int nRightWithEye = (int)e.Test_GetTextRect().right;
    if (nRightWithEye == nRightWithIcon)
    {
        CString d;
        d.Format(_T("Eye/stillUsingIconWidth: right=%d — the eye toggle is visible but ")
                 _T("the right side still reserves the icon width %d, so the toggle ")
                 _T("did not take over the slot"),
                 nRightWithEye, kWideIconGutter);
        return Fail(_T("Eye/stillUsingIconWidth"), d);
    }
    //按钮确实占了地方（右边界比完全没有图标时更靠左）。
    if (nRightWithEye >= nRightBare)
    {
        CString d;
        d.Format(_T("Eye/reservedNothing: bare=%d withEye=%d"), nRightBare, nRightWithEye);
        return Fail(_T("Eye/reservedNothing"), d);
    }

    //三、把图标改宽，右边界应当纹丝不动 —— 这一步才真正证明按钮说了算。
    e.SetIconGlyph(DuiEdit::RightIcon, kWiderIconGutter, _T("x"), kGlyphColor);
    EXPECT_INT(e.Test_GetTextRect().right, nRightWithEye, _T("Eye/independentOfIconWidth"));

    //四、关掉按钮，让出的量又回到图标那一份（此时图标已经是更宽的那个值）。
    e.SetShowEyeToggle(false);
    EXPECT_INT(nRightBare - (int)e.Test_GetTextRect().right, kWiderIconGutter,
               _T("Eye/iconTakesOverAgain"));
    return OK(_T("EyeToggleWinsOverRightIcon"));
}

// =================================================================
// 五、单行垂直居中
// =================================================================

//单行模式下开启垂直居中，文本区要从控件顶部往下挪；多行模式下这个开关无效果。
//
//做法是把上下内边距算成「剩余空间的一半」，让文本区恰好只有一行高 —— 排版
//引擎在文本区内永远从顶部开始排，文本区自身居中了，文字也就居中了。
static Result Test_VerticalCenterOnlyAffectsSingleLine()
{
    //一、单行：居中时的上边界必须比不居中时更靠下。
    DuiEdit single;
    SetUpLaidOutEdit(single);
    EXPECT_BOOL(single.IsVerticalCenter(), true, _T("VCenter/onByDefault"));

    const RECT rcCentered = single.Test_GetTextRect();
    const RECT rcItem     = single.GetRect();

    single.SetVerticalCenter(false);
    const RECT rcTopAligned = single.Test_GetTextRect();

    if (rcCentered.top <= rcTopAligned.top)
    {
        CString d;
        d.Format(_T("VCenter/notMovedDown: centeredTop=%d topAlignedTop=%d ")
                 _T("itemHeight=%d lineHeight=%d"),
                 (int)rcCentered.top, (int)rcTopAligned.top,
                 (int)(rcItem.bottom - rcItem.top), single.GetLineHeight());
        return Fail(_T("VCenter/notMovedDown"), d);
    }

    //上下留白应当基本相等（整数除法允许差一个像素），这才叫居中。
    const int nGapTop    = (int)(rcCentered.top - rcItem.top);
    const int nGapBottom = (int)(rcItem.bottom - rcCentered.bottom);
    if (nGapTop - nGapBottom > 1 || nGapBottom - nGapTop > 1)
    {
        CString d;
        d.Format(_T("VCenter/notBalanced: gapTop=%d gapBottom=%d"), nGapTop, nGapBottom);
        return Fail(_T("VCenter/notBalanced"), d);
    }

    //二、多行：开关不该有任何效果，多行永远从顶部开始排。
    DuiEdit multi;
    multi.SetMultiLine(true);
    SetUpLaidOutEdit(multi);

    const int nMultiTopCentered = (int)multi.Test_GetTextRect().top;
    multi.SetVerticalCenter(false);
    const int nMultiTopAligned = (int)multi.Test_GetTextRect().top;
    EXPECT_INT(nMultiTopCentered, nMultiTopAligned, _T("VCenter/multiLineUnaffected"));

    //多行的上边界应当与单行「不居中」时一致 —— 两者用的是同一份默认上内边距。
    EXPECT_INT(nMultiTopAligned, (int)rcTopAligned.top, _T("VCenter/multiLineUsesDefaultPad"));
    return OK(_T("VerticalCenterOnlyAffectsSingleLine"));
}

// =================================================================
// 六、单行的回车与 Esc
// =================================================================

//单行模式下回车不是换行而是「提交」，Esc 则不论单行多行都交给宿主窗口。
//
//两者都必须**消费掉**这次按键，否则事件会继续在 DUI 树里往上冒泡，可能被别的
//控件再处理一遍。
//
//注意：多行模式下按回车的返回值本用例**刻意不断言**。那条路直接转给排版引擎，
//消费与否由引擎当时的状态决定，不是本控件说了算。「多行不把回车转成提交」这层
//语义由下面的 EnterEscapeNotifications 用例从通知的角度钉住。
static Result Test_SingleLineEnterAndEscapeConsumed()
{
    DuiEdit single;
    SetUpLaidOutEdit(single);
    single.SetText(_T("abc"));

    //回车被消费，且一个字符都不许插进文本里。
    EXPECT_BOOL(single.OnKeyDown(VK_RETURN, 0), true, _T("Enter/singleConsumed"));
    EXPECT_STR(single.GetText(), _T("abc"), _T("Enter/singleTextUnchanged"));

    //Esc 同样被消费，文本也不变。
    EXPECT_BOOL(single.OnKeyDown(VK_ESCAPE, 0), true, _T("Esc/singleConsumed"));
    EXPECT_STR(single.GetText(), _T("abc"), _T("Esc/singleTextUnchanged"));

    //多行模式下 Esc 照样被消费 —— 它与行数无关，收起弹出层这类动作在多行
    //输入框里同样需要。
    DuiEdit multi;
    multi.SetMultiLine(true);
    SetUpLaidOutEdit(multi);
    multi.SetText(_T("abc"));

    EXPECT_BOOL(multi.OnKeyDown(VK_ESCAPE, 0), true, _T("Esc/multiConsumed"));
    EXPECT_STR(multi.GetText(), _T("abc"), _T("Esc/multiTextUnchanged"));
    return OK(_T("SingleLineEnterAndEscapeConsumed"));
}

//**字符消息必须与按键处理成对出现。**
//
//这一条很关键：宿主派发按键消息时不看返回值，随后由系统消息泵生成的字符消息
//照样会送进来。只拦按键、不拦字符的话，回车仍然会被排版引擎当成换行插进去 ——
//表现为「明明写了单行输入框，敲回车还是换了行」。
static Result Test_CharMessagesInterceptedTogether()
{
    DuiEdit single;
    SetUpLaidOutEdit(single);
    single.SetText(_T("ab"));
    //把光标放到两个字符中间。放在末尾的话，即便真插进去了换行，读回文本时也会
    //被「去掉尾部换行」的归一化抹平，用例就看不出问题了。
    single.SetSel(1, 1);

    EXPECT_BOOL(single.OnChar(_T('\r')), true, _T("Char/singleCrConsumed"));
    EXPECT_STR(single.GetText(), _T("ab"), _T("Char/singleCrNoInsert"));

    EXPECT_BOOL(single.OnChar(_T('\n')), true, _T("Char/singleLfConsumed"));
    EXPECT_STR(single.GetText(), _T("ab"), _T("Char/singleLfNoInsert"));

    //Esc 的字符消息（0x1B）同样要丢掉，否则文本里会留下一个不可见字符。
    EXPECT_BOOL(single.OnChar((TCHAR)VK_ESCAPE), true, _T("Char/singleEscConsumed"));
    EXPECT_STR(single.GetText(), _T("ab"), _T("Char/singleEscNoInsert"));

    //制表符不分单行多行都要拦：Tab 键已被宿主截去做焦点遍历，不该再往文本里
    //插一个制表符。
    EXPECT_BOOL(single.OnChar(_T('\t')), true, _T("Char/singleTabConsumed"));
    EXPECT_STR(single.GetText(), _T("ab"), _T("Char/singleTabNoInsert"));

    DuiEdit multi;
    multi.SetMultiLine(true);
    SetUpLaidOutEdit(multi);
    multi.SetText(_T("ab"));
    multi.SetSel(1, 1);

    EXPECT_BOOL(multi.OnChar(_T('\t')), true, _T("Char/multiTabConsumed"));
    EXPECT_STR(multi.GetText(), _T("ab"), _T("Char/multiTabNoInsert"));

    //多行下回车归排版引擎：本控件不拦，内容因此变长。
    //
    //按键消息与字符消息必须成对送到 —— 排版引擎会丢弃没有配套按键按下的回车
    //字符（DuiRichEditTests.cpp 的换行用例里对这一点有实测记录）。只发字符消息
    //的话，这里会误判成"本控件把回车拦下来了"。
    multi.OnKeyDown(VK_RETURN, 0);
    multi.OnChar(_T('\r'));
    if (multi.GetTextLength() <= 2)
    {
        CString d;
        d.Format(_T("Char/multiCrShouldInsert: length=%d text=[%s] — in multi-line mode ")
                 _T("the carriage return must reach the layout engine and insert a line ")
                 _T("break, so the length should have grown beyond 2"),
                 multi.GetTextLength(), (LPCTSTR)multi.GetText());
        return Fail(_T("Char/multiCrShouldInsert"), d);
    }
    return OK(_T("CharMessagesInterceptedTogether"));
}

// =================================================================
// 七、通知（需要真窗口）
// =================================================================

//单行按回车发一条 DUIN_EDIT_ENTER，多行按回车一条也不发；Esc 则单行多行都发。
//
//这一条是从通知的角度钉住「多行模式下回车不被本控件截走」—— 直接看
//OnKeyDown 的返回值是看不出来的，多行时那条路转给了排版引擎，返回值由引擎
//当时的状态决定。
static Result Test_EnterEscapeNotifications()
{
    ResetNotifyCounters();

    NotifyCounterWnd top;
    if (top.get() == NULL)
    {
        return Fail(_T("EnterEscapeNotifications"), _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, kHostW, kHostH);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
    if (!host.IsWindow())
    {
        return Fail(_T("EnterEscapeNotifications"), _T("cannot create DuiHost"));
    }

    DuiEdit* pEdit = new DuiEdit();
    host.SetRoot(std::unique_ptr<DuiControl>(pEdit));
    pEdit->SetText(_T("abc"));

    //一、单行回车 → 一条提交通知。
    pEdit->OnKeyDown(VK_RETURN, 0);
    const int nEnterAfterSingle = s_nNotifyEnter;

    //二、单行 Esc → 一条取消通知。
    pEdit->OnKeyDown(VK_ESCAPE, 0);
    const int nEscapeAfterSingle = s_nNotifyEscape;

    //三、切成多行后按回车 → 不再发提交通知，回车归排版引擎。
    pEdit->SetMultiLine(true);
    pEdit->OnKeyDown(VK_RETURN, 0);
    const int nEnterAfterMulti = s_nNotifyEnter;

    //四、多行 Esc 照发。
    pEdit->OnKeyDown(VK_ESCAPE, 0);
    const int nEscapeAfterMulti = s_nNotifyEscape;

    host.DestroyWindow();

    EXPECT_INT(nEnterAfterSingle,  1, _T("Notify/enterOnceInSingleLine"));
    EXPECT_INT(nEscapeAfterSingle, 1, _T("Notify/escapeOnceInSingleLine"));
    EXPECT_INT(nEnterAfterMulti,   1, _T("Notify/noEnterInMultiLine"));
    EXPECT_INT(nEscapeAfterMulti,  2, _T("Notify/escapeAlsoInMultiLine"));
    return OK(_T("EnterEscapeNotifications"));
}

//SetText 会发文字变化通知，SetTextNoNotify 不发；而内部的 OnTextChanged 钩子
//两者都会走到。
//
//这个区别是本类刻意与基类不同的地方：基类只在用户编辑时发通知，本类程序设值
//也发，与旧输入框保持一致（存量业务代码有多处依赖这一点，例如搜索框点清除
//叉号之后要靠这条通知去重新过滤列表）。而 SetTextNoNotify 是留给「回填」这类
//场景的出口 —— 下拉框把选中项写回输入框时不该再触发一次增量搜索。
//
//钩子必须两条路都走：它是控件自身状态的同步点（例如搜索框据此决定清除叉号
//显不显示），压掉会让控件自己的外观停在旧状态上。
static Result Test_SetTextNotifiesAndNoNotifyVariantDoesNot()
{
    ResetNotifyCounters();

    NotifyCounterWnd top;
    if (top.get() == NULL)
    {
        return Fail(_T("SetTextNotifiesAndNoNotifyVariantDoesNot"),
                    _T("cannot create top window"));
    }

    DuiHost host;
    RECT rcHost;
    ::SetRect(&rcHost, 0, 0, kHostW, kHostH);
    host.Create(top.get(), rcHost, nullptr, WS_CHILD | WS_VISIBLE, 0);
    if (!host.IsWindow())
    {
        return Fail(_T("SetTextNotifiesAndNoNotifyVariantDoesNot"),
                    _T("cannot create DuiHost"));
    }

    CountingEdit* pEdit = new CountingEdit();
    host.SetRoot(std::unique_ptr<DuiControl>(pEdit));

    //一、SetText：钩子走一次，通知发一条。
    pEdit->SetText(_T("first"));
    const int nHookAfterSet   = pEdit->GetTextChangedCount();
    const int nNotifyAfterSet = s_nNotifyValueChanged;

    //二、SetTextNoNotify：钩子照样走，通知不再增加。
    pEdit->SetTextNoNotify(_T("second"));
    const int nHookAfterQuiet   = pEdit->GetTextChangedCount();
    const int nNotifyAfterQuiet = s_nNotifyValueChanged;

    //三、压制状态是临时的，下一次 SetText 必须重新发通知。
    pEdit->SetText(_T("third"));
    const int nNotifyAfterThird = s_nNotifyValueChanged;

    const CString strFinal = pEdit->GetText();
    host.DestroyWindow();

    EXPECT_INT(nHookAfterSet,     1, _T("Notify/hookOnSetText"));
    EXPECT_INT(nNotifyAfterSet,   1, _T("Notify/valueChangedOnSetText"));
    EXPECT_INT(nHookAfterQuiet,   2, _T("Notify/hookAlsoOnNoNotify"));
    EXPECT_INT(nNotifyAfterQuiet, 1, _T("Notify/noValueChangedOnNoNotify"));
    EXPECT_INT(nNotifyAfterThird, 2, _T("Notify/suppressionIsTemporary"));
    EXPECT_STR(strFinal, _T("third"), _T("Notify/finalText"));
    return OK(_T("SetTextNotifiesAndNoNotifyVariantDoesNot"));
}

// =================================================================
// 八、纯文本模式
// =================================================================
//
// 2026-09-30 登录窗账号框出现的问题：光标在文字末尾时按 Shift+→、或者拖选
// 越过文字末尾，文字后面会多出一小块高亮，看起来像是账号末尾有空格。实际
// 上文本里并没有空白字符，被选中的是富文本模式下文档末尾那个看不见的段落
// 结束符。纯文本模式下引擎不允许选区越过文本末尾，这一块也就选不中了。
//
// 普通输入框用不到分段格式（字体与颜色都是整体设置的），因此一律切到纯
// 文本模式。下面几条用例都直接向引擎查询，而不是读控件自己记下的标志位，
// 这样引擎若拒绝了切换，用例能如实报出来。

// 引擎当前是否处于纯文本模式。
//   edit：被查询的控件（引用，不转移所有权）。
//   返回：true 表示引擎报告的文本模式带有纯文本标志。
static bool IsEnginePlainText(DuiRichEdit& edit)
{
    const LRESULT mode = edit.SendMessageToEngine(EM_GETTEXTMODE, 0, 0);
    return (mode & TM_PLAINTEXT) != 0;
}

// 模拟按住 Shift 再按一次方向键 →。
//
// 排版引擎判断 Shift 是否按下读的是本线程的键盘状态，所以这里临时改写键盘
// 状态，按键送完立即恢复，不影响后续用例。
//   edit：接收按键的控件（引用，不转移所有权）。
static void PressShiftRight(DuiRichEdit& edit)
{
    BYTE keys[256];
    ::GetKeyboardState(keys);
    const BYTE oldShift = keys[VK_SHIFT];
    //键盘状态里最高位置位表示该键处于按下状态
    keys[VK_SHIFT] = 0x80;
    ::SetKeyboardState(keys);

    edit.OnKeyDown(VK_RIGHT, 0);

    keys[VK_SHIFT] = oldShift;
    ::SetKeyboardState(keys);
}

//新建的普通输入框，引擎处于纯文本模式。
//
//这一条钉住构造函数里的切换：切换只在文档为空时才会被引擎接受，挪到设置
//文字之后就会静默失效，界面上看不出任何异常，直到有人再次选中那个段落标记。
static Result Test_PlainTextModeByDefault()
{
    DuiEdit e;
    EXPECT_BOOL(IsEnginePlainText(e), true, _T("PlainText/engineModeAfterConstruct"));

    //写入文字之后仍然是纯文本模式。
    e.SetText(kPlainSample);
    EXPECT_BOOL(IsEnginePlainText(e), true, _T("PlainText/engineModeAfterSetText"));
    return OK(_T("PlainTextModeByDefault"));
}

//选区不能越过文本末尾：无论是指定终点超出一格，还是用 -1 表示一直选到末尾，
//读回来的终点都应当等于文本长度。
static Result Test_SelectionStopsAtTextEnd()
{
    DuiEdit e;
    e.SetText(kPlainSample);
    EXPECT_INT(e.GetTextLength(), kPlainSampleLen, _T("SelEnd/textLength"));

    long cpMin = 0;
    long cpMax = 0;

    //终点比文本多出一格：多出的那一格正是段落结束符，不该被选中。
    e.SetSel(kPlainSampleLen, kPlainSampleLen + 1);
    e.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, kPlainSampleLen, _T("SelEnd/pastEndMin"));
    EXPECT_INT(cpMax, kPlainSampleLen, _T("SelEnd/pastEndMax"));

    //全选只覆盖文字本身。
    e.SetSel(0, kSelToEnd);
    e.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, 0,               _T("SelEnd/selectAllMin"));
    EXPECT_INT(cpMax, kPlainSampleLen, _T("SelEnd/selectAllMax"));
    return OK(_T("SelectionStopsAtTextEnd"));
}

//光标在文字末尾时按 Shift+→，选区保持为空。
//
//同一套按键先在一个富文本模式的单行 DuiRichEdit 上跑一遍作为对照：那边必须
//真的选中了一格。对照组若没有选中，说明模拟按键没有生效，这时如果只看输入框
//这一侧，用例会在什么都没验证的情况下误报通过。
static Result Test_ShiftRightAtEndSelectsNothing()
{
    long cpMin = 0;
    long cpMax = 0;

    //对照组：富文本模式下，段落结束符可以被选中。
    DuiRichEdit rich;
    rich.SetMultiLine(false);
    SetUpLaidOutEdit(rich);
    rich.SetText(kPlainSample);
    rich.SetSel(kPlainSampleLen, kPlainSampleLen);
    PressShiftRight(rich);
    rich.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMax, kPlainSampleLen + 1, _T("ShiftRight/richControlSelectsEop"));

    //被测对象：普通输入框。
    DuiEdit e;
    SetUpLaidOutEdit(e);
    e.SetText(kPlainSample);
    e.SetSel(kPlainSampleLen, kPlainSampleLen);
    PressShiftRight(e);
    e.GetSel(cpMin, cpMax);
    EXPECT_INT(cpMin, kPlainSampleLen, _T("ShiftRight/editMin"));
    EXPECT_INT(cpMax, kPlainSampleLen, _T("ShiftRight/editMax"));
    return OK(_T("ShiftRightAtEndSelectsNothing"));
}

//密码框在纯文本模式下照常工作：引擎显示掩码，取回的仍是原文。
static Result Test_PasswordWorksInPlainTextMode()
{
    DuiEdit e;
    e.SetPassword(true);
    e.SetText(kPlainSample);

    EXPECT_BOOL(IsEnginePlainText(e), true, _T("Password/plainText"));
    EXPECT_BOOL(e.IsPasswordMode(),   true, _T("Password/engineMaskOn"));
    EXPECT_STR(e.GetText(), kPlainSample,   _T("Password/textReadBack"));
    return OK(_T("PasswordWorksInPlainTextMode"));
}

//派生自 DuiEdit 的输入框同样处于纯文本模式：搜索框本身，以及可编辑下拉框
//内嵌的那个输入框。登录窗的账号框就是后者。
static Result Test_SubclassesInheritPlainTextMode()
{
#if BUI_FEATURE_SEARCHBOX
    DuiSearchBox search;
    EXPECT_BOOL(IsEnginePlainText(search), true, _T("Inherit/searchBox"));
#endif

#if BUI_FEATURE_COMBOBOX
    DuiComboBox combo;
    combo.SetEditable(true);
    DuiRichEdit* pInner = nullptr;
    const std::vector<std::unique_ptr<DuiControl>>& children = combo.Children();
    for (size_t i = 0; i < children.size(); ++i)
    {
        pInner = dynamic_cast<DuiRichEdit*>(children[i].get());
        if (pInner != nullptr)
        {
            break;
        }
    }
    if (pInner == nullptr)
    {
        return Fail(_T("Inherit/comboHasEdit"),
                    _T("Inherit/comboHasEdit: editable combo box has no inner edit child"));
    }
    EXPECT_BOOL(IsEnginePlainText(*pInner), true, _T("Inherit/comboInnerEdit"));
#endif
    return OK(_T("SubclassesInheritPlainTextMode"));
}

} // 匿名命名空间

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        LPCTSTR name;
        TestFn  fn;
    };

    // **每个用例都要在这里登记一行**，漏登记的用例不报错、也不执行。
    Entry tests[] = {
        // ---- 默认状态 ----
        { _T("Defaults"),                         &Test_Defaults                         },
        // ---- 图标几何与文本区 ----
        { _T("ComputeIconRectGeometry"),          &Test_ComputeIconRectGeometry          },
        { _T("LeftIconShrinksTextRect"),          &Test_LeftIconShrinksTextRect          },
        { _T("ClearIconRestoresTextRect"),        &Test_ClearIconRestoresTextRect        },
        { _T("SetIconWithNullPainterClearsGutter"), &Test_SetIconWithNullPainterClearsGutter },
        // ---- 密码模式与显隐按钮 ----
        { _T("PasswordToggleResetsReveal"),       &Test_PasswordToggleResetsReveal       },
        { _T("HidingEyeToggleRestoresMask"),      &Test_HidingEyeToggleRestoresMask      },
        { _T("EyeToggleWinsOverRightIcon"),       &Test_EyeToggleWinsOverRightIcon       },
        // ---- 单行垂直居中 ----
        { _T("VerticalCenterOnlyAffectsSingleLine"), &Test_VerticalCenterOnlyAffectsSingleLine },
        // ---- 键盘 ----
        { _T("SingleLineEnterAndEscapeConsumed"), &Test_SingleLineEnterAndEscapeConsumed },
        { _T("CharMessagesInterceptedTogether"),  &Test_CharMessagesInterceptedTogether  },
        // ---- 通知（需要真窗口）----
        { _T("EnterEscapeNotifications"),         &Test_EnterEscapeNotifications         },
        { _T("SetTextNotifiesAndNoNotifyVariantDoesNot"), &Test_SetTextNotifiesAndNoNotifyVariantDoesNot },
        // ---- 纯文本模式 ----
        { _T("PlainTextModeByDefault"),           &Test_PlainTextModeByDefault           },
        { _T("SelectionStopsAtTextEnd"),          &Test_SelectionStopsAtTextEnd          },
        { _T("ShiftRightAtEndSelectsNothing"),    &Test_ShiftRightAtEndSelectsNothing    },
        { _T("PasswordWorksInPlainTextMode"),     &Test_PasswordWorksInPlainTextMode     },
        { _T("SubclassesInheritPlainTextMode"),   &Test_SubclassesInheritPlainTextMode   },
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
    summary.Format(_T("[summary] DuiEditTests passed=%d failed=%d"), passed, failed);
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

} // namespace DuiEditTests

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT
