/**
 *  DuiTextHost 的实现。设计意图、单位约定、为什么不直接持有控件指针，
 *  都写在 DuiTextHost.h 的文件头注释里，读本文件之前建议先看那一段。
 *
 *  本文件里每个 ITextHost 方法的注释都按同一个格式写：引擎什么时候来问、
 *  它期望什么、我们答什么、为什么这么答。这个接口的代码本身都很短，
 *  信息量全在"为什么"上。
 *
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT
#include "DuiTextHost.h"
#include "DuiTextServices.h"
#include "../../DuiTrace.h"
#include "../../DuiResMgr.h"

#include <vector>

// 输入法接口的声明与所在的库。TxImmGetContext / TxImmReleaseContext 要用。
// HIMC 这个类型本身由 windows.h 提供，但取上下文的两个函数在这个头里。
#include <imm.h>
#pragma comment(lib, "imm32.lib")

namespace balloonwjui {

namespace {

// 一英寸等于多少个百分之毫米。1 英寸 = 25.4 毫米 = 2540 个百分之毫米。
// 引擎取内边距和取范围这两个方法要求的就是这个单位。
const int kHimetricPerInch = 2540;

// 一英寸等于多少个 twip。字符格式里的字号、段落格式里的制表位用这个单位。
const int kTwipsPerInch = 1440;

// 每英寸点数取不到时的兜底值，同时也是 Windows 的基准值。
// 有了兜底，换算里的除法不会遇到零。
const int kFallbackDpi = 96;

// 默认制表位间隔（twip）。720 twip 正好是半英寸，与系统编辑框一致。
const LONG kDefaultTabTwips = 720;

// 未设置长度上限时回答给引擎的值。引擎要求这里必须给出一个具体数字，
// 接口里没有"不限制"这个取值，所以取一个足够大的数充当不限制。
// 一百万个字符远超任何输入框的实际用量，同时留足余量不会引起整数溢出。
const DWORD kUnlimitedTextLength = 1024 * 1024;

// 默认字号（磅）。与库内 DuiResMgr 的默认字体保持一致，
// 使本控件不设字体时的观感与其它控件统一。
const int kDefaultFontPointSize = 9;

// ─────────────────────────────────────────────────────────────────
// 定时器归属登记表
// ─────────────────────────────────────────────────────────────────
//
// 为什么需要它：我们用的是**线程定时器**（不绑窗口），系统到点后回调一个
// 静态函数，只把系统分配的定时器编号交给我们。静态函数没有 this 指针，
// 必须靠这张表按编号找回对应的实例。
//
// 为什么不用窗口定时器：窗口定时器的编号由调用方指定，而引擎自选的编号
// 在多个控件实例之间必然撞号，走窗口定时器就得再加一层编号重映射；线程
// 定时器的编号由系统分配、全局唯一，从源头避开了这个问题。库内的提示条
// 管理器与动画管理器用的也是线程定时器，有先例可循。
struct TimerOwner
{
    UINT_PTR     m_idSystem;   // 系统分配的定时器编号
    DuiTextHost* m_pHost;      // 归属实例；不持有所有权
};

// 只在 UI 线程访问，不加锁。定时器数量极少（通常每个控件只有光标闪烁一个），
// 线性查找足够。
std::vector<TimerOwner> s_timerOwners;

void RegisterTimerOwner(UINT_PTR idSystem, DuiTextHost* pHost)
{
    TimerOwner rec;
    rec.m_idSystem = idSystem;
    rec.m_pHost    = pHost;
    s_timerOwners.push_back(rec);
}

void UnregisterTimerOwner(UINT_PTR idSystem)
{
    for (std::vector<TimerOwner>::iterator it = s_timerOwners.begin();
         it != s_timerOwners.end(); ++it)
    {
        if (it->m_idSystem == idSystem)
        {
            s_timerOwners.erase(it);
            return;
        }
    }
}

DuiTextHost* FindTimerOwner(UINT_PTR idSystem)
{
    for (size_t i = 0; i < s_timerOwners.size(); ++i)
    {
        if (s_timerOwners[i].m_idSystem == idSystem)
        {
            return s_timerOwners[i].m_pHost;
        }
    }
    return nullptr;
}

// 单色位图每行按多少位对齐。CreateBitmap 要求单色位图的每一行按 16 位（一个
// WORD）对齐，行尾不足的部分要补齐。
const int kMonoBitmapRowAlignBits = 16;

// 一个字节的位数。
const int kBitsPerByte = 8;

// 建一张全零的单色位图，给系统光标当形状用。
//
// 系统画位图光标时只反转位图里置位的像素，全零的位图画下去没有任何像素变化。
// 排版引擎在有选区时就是用这种位图让光标「存在但看不见」的（2026-09-30 实测：
// 8×20、每个字节都是 0）。系统光标用它创建，就算引擎或宿主的 BeginPaint /
// EndPaint 把它显示出来，屏幕上也不会多出第二个光标。
//   width / height：位图尺寸（像素），必须大于 0。
//   返回：位图句柄，调用方负责 DeleteObject；创建失败返回 nullptr。
HBITMAP CreateBlankMonoBitmap(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return nullptr;
    }
    const int bytesPerRow = ((width + kMonoBitmapRowAlignBits - 1) / kMonoBitmapRowAlignBits)
                          * (kMonoBitmapRowAlignBits / kBitsPerByte);
    std::vector<BYTE> zeros((size_t)bytesPerRow * (size_t)height, 0);
    return ::CreateBitmap(width, height, 1, 1, &zeros[0]);
}

// 把系统的滚动条方向常量翻译成"要通知哪几个方向"，逐个通知实现方。
//
// 注意 SB_BOTH 要通知两个方向。参考实现（SOUI）在这里用的是
// `fnBar != SB_HORZ ? 竖直 : 水平` 的写法，SB_BOTH 会被当成只有竖直，
// 水平那一次就丢了。本实现按三档分别处理。
void NotifyScrollBars(IDuiTextHostSite* pSite, INT fnBar)
{
    if (pSite == nullptr)
    {
        return;
    }
    if (fnBar == SB_VERT || fnBar == SB_BOTH)
    {
        pSite->TxSiteScrollInfoChanged(true);
    }
    if (fnBar == SB_HORZ || fnBar == SB_BOTH)
    {
        pSite->TxSiteScrollInfoChanged(false);
    }
}

} // 匿名命名空间

// 当前存活的实例个数。构造加一、析构减一，用于验证不存在引用环。
//
// 不加锁：本库的控件树只在界面线程上创建与销毁，这个计数也只在测试里读。
static int s_nLiveTextHostCount = 0;

int DuiTextHost::Test_GetLiveInstanceCount()
{
    return s_nLiveTextHostCount;
}

// =================================================================
// 构造与析构
// =================================================================

DuiTextHost::DuiTextHost()
    : m_cRef(1)              // 创建者持有这一份，不需要外部再 AddRef
    , m_pServices(nullptr)
    , m_pSite(nullptr)
    , m_dwPropertyBits(0)
    , m_dwScrollBars(0)
    , m_dwMaxLength(kUnlimitedTextLength)
    , m_chPasswordChar(_T('*'))
    , m_bBackTransparent(true)
    , m_crSelBack(CLR_INVALID)
    , m_crSelText(CLR_INVALID)
    , m_bUiActive(false)
    , m_bShowCaret(true)
    , m_bCaretWanted(false)
    , m_bCaretBlinkOn(true)
    , m_idCaretBlinkTimer(0)
    , m_hCaretPaintBmp(nullptr)
    , m_hSysCaretBlankBmp(nullptr)
    , m_bInDraw(false)
    , m_bInScrollNotify(false)
{
    ++s_nLiveTextHostCount;

    ::SetRect(&m_rcClient, 0, 0, 0, 0);
    ::SetRect(&m_rcViewInset, 0, 0, 0, 0);
    ::SetRectEmpty(&m_rcCaretBeforeDraw);
    m_sizeExtent.cx = 0;
    m_sizeExtent.cy = 0;
    m_szCaretPaint.cx = 0;
    m_szCaretPaint.cy = 0;

    for (int i = 0; i < kMaxTimers; ++i)
    {
        m_timers[i].m_bUsed    = false;
        m_timers[i].m_idEngine = 0;
        m_timers[i].m_idSystem = 0;
    }

    // 默认属性位：富文本、多行、自动换行、双击按整词选取。
    //
    // 刻意**不设**"失焦时隐藏选区"这一位，与早先内嵌真子窗口的实现保持
    // 一致（那种实现用的是 ES_NOHIDESEL 风格）。这样在"点到别处后原先
    // 选中的文字还看不看得见"这件事上表现相同，调用方迁过来不会出现
    // 观感变化。
    m_dwPropertyBits = TXTBIT_RICHTEXT
                     | TXTBIT_MULTILINE
                     | TXTBIT_WORDWRAP
                     | TXTBIT_AUTOWORDSEL;

    // 默认滚动条配置：告诉引擎「存在竖直滚动条」并且允许自动竖直滚动。
    //
    // 这两位缺一不可，含义也不同：**「存在滚动条」那一位决定引擎维不维护
    // 滚动范围**，「自动滚动」那一位决定打字超出可视区时会不会自动跟着滚。
    // 只给后者的话，引擎会认为这个控件根本不能滚，于是把滚动范围恒定报成
    // 零 —— 表现就是内容明明超出了，查出来却说没有溢出，滚动条永远不出现。
    //
    // 注意这里说的「存在滚动条」只是给引擎的说法，引擎自己不画任何东西，
    // 它只会通过回调让宿主去摆滚动条。真正显不显示由控件的滚动策略决定。
    //
    // 水平方向默认不开：默认是自动换行，横向不会溢出。控件关掉自动换行时
    // 会调 SetScrollBars 把水平那两位补上。
    m_dwScrollBars = WS_VSCROLL | ES_AUTOVSCROLL;

    // 默认字符格式：先用库内默认字体填一份，控件随后通常会用自己的字体覆盖。
    // 这里必须填出一份可用的值，因为引擎在创建过程中就会回来问，
    // 那时控件还没来得及推任何东西进来。
    ::memset(&m_cfDefault, 0, sizeof(m_cfDefault));
    m_cfDefault.cbSize = sizeof(CHARFORMAT2W);
    m_cfDefault.dwMask = CFM_SIZE | CFM_OFFSET | CFM_FACE | CFM_CHARSET
                       | CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE;
    m_cfDefault.yHeight = kDefaultFontPointSize * (kTwipsPerInch / 72);
    m_cfDefault.crTextColor = RGB(0, 0, 0);
    m_cfDefault.bCharSet = DEFAULT_CHARSET;
    m_cfDefault.bPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    m_cfDefault.szFaceName[0] = L'\0';

    // 默认段落格式：左对齐、一个半英寸的制表位。
    ::memset(&m_pfDefault, 0, sizeof(m_pfDefault));
    m_pfDefault.cbSize = sizeof(PARAFORMAT2);
    m_pfDefault.dwMask = PFM_ALL;
    m_pfDefault.wAlignment = PFA_LEFT;
    m_pfDefault.cTabCount = 1;
    m_pfDefault.rgxTabs[0] = kDefaultTabTwips;

    // 用库内默认字体把字体名等字段补全。取不到字体时保持上面的默认值，
    // 引擎会退回它自己的默认字体，不会出错。
    HFONT hDefault = DuiResMgr::Inst().GetDefaultFont();
    if (hDefault != nullptr)
    {
        SetDefaultFont(hDefault, m_cfDefault.crTextColor);
    }
}

DuiTextHost::~DuiTextHost()
{
    // 兜底清理。正常流程里控件会先调 Shutdown，这里只防漏。
    Shutdown();

    --s_nLiveTextHostCount;
}

// =================================================================
// 生命周期
// =================================================================

bool DuiTextHost::Init(IDuiTextHostSite* pSite)
{
    if (m_pServices != nullptr)
    {
        // 已经初始化过。允许重复调用只更新回调接口，不重建引擎。
        m_pSite = pSite;
        return true;
    }

    m_pSite = pSite;

    if (!DuiTextServices::IsAvailable())
    {
        return false;
    }

    // 注意时序：引擎在创建过程中就会回头调我们的 TxGetPropertyBits、
    // TxGetCharFormat 等方法索取初始配置。因此**构造函数必须已经把所有
    // 成员填成可用的值**，不能等到这里再填。
    IUnknown* pUnk = nullptr;
    HRESULT hr = DuiTextServices::Create(static_cast<ITextHost*>(this), &pUnk);
    if (FAILED(hr) || pUnk == nullptr)
    {
        return false;
    }

    hr = pUnk->QueryInterface(IID_ITextServices,
                             reinterpret_cast<void**>(&m_pServices));
    // 换到目标接口后本地这一份引用就没用了，无论成功失败都要放掉。
    pUnk->Release();

    if (FAILED(hr) || m_pServices == nullptr)
    {
        m_pServices = nullptr;
        return false;
    }

    // 就地激活：告诉引擎可以开始工作了。传空矩形表示"客户区尺寸请回来问我"，
    // 引擎随后会调 TxGetClientRect。
    m_pServices->OnTxInPlaceActivate(nullptr);

    // 把初始状态明确压成"没有键盘焦点"。
    //
    // 这一步看着多余，实际不能省：刚激活的引擎会认为自己处于可交互状态，
    // 于是可能要求显示光标。控件此刻通常还没获得焦点，就会出现一个没有
    // 焦点的输入框在闪光标。参考实现在同一位置做了同样的处理。
    m_bUiActive = false;
    m_pServices->OnTxUIDeactivate();
    m_pServices->TxSendMessage(WM_KILLFOCUS, 0, 0, nullptr);

    return true;
}

void DuiTextHost::Shutdown()
{
    // 先撤掉所有定时器并从归属表里摘掉自己。必须在断开引擎之前做，
    // 否则定时器到点后会回调到一个已经不该被打扰的对象上。
    for (int i = 0; i < kMaxTimers; ++i)
    {
        if (m_timers[i].m_bUsed)
        {
            ::KillTimer(nullptr, m_timers[i].m_idSystem);
            UnregisterTimerOwner(m_timers[i].m_idSystem);
            m_timers[i].m_bUsed    = false;
            m_timers[i].m_idEngine = 0;
            m_timers[i].m_idSystem = 0;
        }
    }

    // 光标闪烁定时器同理，也登记在归属表里，一并撤掉。
    StopCaretBlink();
    m_bCaretWanted = false;

    // 让出系统光标（线程唯一的共享资源，不能一直占着）。
    m_caret.Destroy();

    // 两张位图都由本对象持有。系统光标已经销毁，空白位图不再被引用，可以删除。
    if (m_hSysCaretBlankBmp != nullptr)
    {
        ::DeleteObject(m_hSysCaretBlankBmp);
        m_hSysCaretBlankBmp = nullptr;
    }
    if (m_hCaretPaintBmp != nullptr)
    {
        ::DeleteObject(m_hCaretPaintBmp);
        m_hCaretPaintBmp = nullptr;
    }

    if (m_pServices != nullptr)
    {
        m_pServices->OnTxInPlaceDeactivate();
        m_pServices->Release();
        m_pServices = nullptr;
    }

    // 最后断开与控件的联系。此后所有回调都只会走到空指针判断上，
    // 不会触及可能已经析构的控件。
    m_pSite = nullptr;
    m_bUiActive = false;
}

// =================================================================
// 控件把状态推进来
// =================================================================

void DuiTextHost::SetClientRect(const RECT& rc)
{
    m_rcClient = rc;
    RecalcExtent();
}

void DuiTextHost::SetViewInsetPixels(int left, int top, int right, int bottom)
{
    int dpiX = kFallbackDpi;
    int dpiY = kFallbackDpi;
    GetDevicePixelsPerInch(dpiX, dpiY);

    // 这是全类唯一把像素换算成百分之毫米的地方之一（另一处是 RecalcExtent）。
    // 换算集中在这两处，别处一律像素，是为了不让单位在代码里到处混着走。
    m_rcViewInset.left   = ::MulDiv(left,   kHimetricPerInch, dpiX);
    m_rcViewInset.top    = ::MulDiv(top,    kHimetricPerInch, dpiY);
    m_rcViewInset.right  = ::MulDiv(right,  kHimetricPerInch, dpiX);
    m_rcViewInset.bottom = ::MulDiv(bottom, kHimetricPerInch, dpiY);

    NotifyPropertyChange(TXTBIT_VIEWINSETCHANGE, TXTBIT_VIEWINSETCHANGE);
}

void DuiTextHost::SetDefaultCharFormat(const CHARFORMAT2W& cf)
{
    m_cfDefault = cf;
    m_cfDefault.cbSize = sizeof(CHARFORMAT2W);
    NotifyPropertyChange(TXTBIT_CHARFORMATCHANGE, TXTBIT_CHARFORMATCHANGE);
}

void DuiTextHost::SetDefaultFont(HFONT hFont, COLORREF crText)
{
    if (hFont == nullptr)
    {
        return;
    }

    LOGFONT lf;
    ::memset(&lf, 0, sizeof(lf));
    if (::GetObject(hFont, sizeof(lf), &lf) == 0)
    {
        return;
    }

    int dpiX = kFallbackDpi;
    int dpiY = kFallbackDpi;
    GetDevicePixelsPerInch(dpiX, dpiY);

    // 字体高度换算：LOGFONT 里的高度是设备单位（负值表示字符高度），
    // 而字符格式要求的是 twip。取绝对值再按每英寸点数换算。
    m_cfDefault.yHeight = ::MulDiv(lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight,
                                   kTwipsPerInch, dpiY);

    m_cfDefault.crTextColor     = crText;
    m_cfDefault.bCharSet        = lf.lfCharSet;
    m_cfDefault.bPitchAndFamily = lf.lfPitchAndFamily;

    m_cfDefault.dwEffects = 0;
    if (lf.lfWeight >= FW_BOLD)
    {
        m_cfDefault.dwEffects |= CFE_BOLD;
    }
    if (lf.lfItalic != 0)
    {
        m_cfDefault.dwEffects |= CFE_ITALIC;
    }
    if (lf.lfUnderline != 0)
    {
        m_cfDefault.dwEffects |= CFE_UNDERLINE;
    }

    // 字符格式的字体名字段固定是宽字符，而 LOGFONT 随编译配置可能是窄字符。
    // 统一走一次转换，两种配置下都正确。
#ifdef UNICODE
    ::wcsncpy_s(m_cfDefault.szFaceName, LF_FACESIZE, lf.lfFaceName, LF_FACESIZE - 1);
#else
    ::MultiByteToWideChar(CP_ACP, 0, lf.lfFaceName, -1,
                          m_cfDefault.szFaceName, LF_FACESIZE);
#endif

    NotifyPropertyChange(TXTBIT_CHARFORMATCHANGE, TXTBIT_CHARFORMATCHANGE);
}

void DuiTextHost::SetDefaultParaFormat(const PARAFORMAT2& pf)
{
    m_pfDefault = pf;
    m_pfDefault.cbSize = sizeof(PARAFORMAT2);
    NotifyPropertyChange(TXTBIT_PARAFORMATCHANGE, TXTBIT_PARAFORMATCHANGE);
}

void DuiTextHost::SetPropertyBits(DWORD dwMask, DWORD dwBits)
{
    // 只改掩码覆盖到的那几位，其余保持不动。
    m_dwPropertyBits = (m_dwPropertyBits & ~dwMask) | (dwBits & dwMask);

    // 第二步不能省。引擎不会主动来问属性位，只在被通知时才重新读；
    // 漏了这一步的症状是"改了没反应"，而且不报错，很难往这个方向想。
    NotifyPropertyChange(dwMask, dwBits);
}

void DuiTextHost::SetBackTransparent(bool bTransparent)
{
    if (m_bBackTransparent == bTransparent)
    {
        return;
    }
    m_bBackTransparent = bTransparent;
    NotifyPropertyChange(TXTBIT_BACKSTYLECHANGE, TXTBIT_BACKSTYLECHANGE);
}

void DuiTextHost::SetMaxLength(int nMax)
{
    m_dwMaxLength = (nMax > 0) ? (DWORD)nMax : kUnlimitedTextLength;
    NotifyPropertyChange(TXTBIT_MAXLENGTHCHANGE, TXTBIT_MAXLENGTHCHANGE);
}

void DuiTextHost::SetPasswordChar(TCHAR ch)
{
    m_chPasswordChar = ch;
    // 密码字符本身没有专门的变更通知位，借用密码显示这一位让引擎重读。
    NotifyPropertyChange(TXTBIT_USEPASSWORD, m_dwPropertyBits & TXTBIT_USEPASSWORD);
}

void DuiTextHost::SetScrollBars(DWORD dwScrollBars)
{
    if (m_dwScrollBars == dwScrollBars)
    {
        return;
    }
    m_dwScrollBars = dwScrollBars;
    NotifyPropertyChange(TXTBIT_SCROLLBARCHANGE, TXTBIT_SCROLLBARCHANGE);
}

void DuiTextHost::SetSelectionColors(COLORREF crBack, COLORREF crText)
{
    m_crSelBack = crBack;
    m_crSelText = crText;
    // 选区配色是在引擎向我们查询系统颜色时现取的，改完只需重绘一次即可生效。
    if (m_pSite != nullptr)
    {
        m_pSite->TxSiteInvalidate(nullptr);
    }
}

void DuiTextHost::SetShowCaret(bool b)
{
    if (m_bShowCaret == b)
    {
        return;
    }
    RECT rcBefore;
    GetCaretVisibleRect(rcBefore);
    m_bShowCaret = b;

    // 关掉时立刻失效光标所在的矩形，让已经画出来的光标随下一次绘制消失 ——
    // 不做这一步的话，要等引擎下一次主动来问才生效，而只读控件可能很久都不会
    // 有下一次。重新打开时同理，引擎仍要求显示的话立即画出来。
    OnCaretMaybeChanged(rcBefore);
}

void DuiTextHost::SetUiActive(bool bActive)
{
    if (m_bUiActive == bActive)
    {
        return;
    }
    RECT rcBefore;
    GetCaretVisibleRect(rcBefore);
    m_bUiActive = bActive;

    if (m_pServices != nullptr)
    {
        if (bActive)
        {
            m_pServices->OnTxUIActivate();
            m_pServices->TxSendMessage(WM_SETFOCUS, 0, 0, nullptr);
        }
        else
        {
            m_pServices->OnTxUIDeactivate();
            m_pServices->TxSendMessage(WM_KILLFOCUS, 0, 0, nullptr);
            // 失焦时主动让出系统光标。不做这一步的话，线程唯一的那份光标会
            // 被本控件一直占着，别的控件再想用就得先把它顶掉。
            m_caret.Destroy();
            // 系统光标已经销毁，它用的空白位图不再被引用。
            if (m_hSysCaretBlankBmp != nullptr)
            {
                ::DeleteObject(m_hSysCaretBlankBmp);
                m_hSysCaretBlankBmp = nullptr;
            }
        }
    }

    // 失焦后引擎先前的显示要求作废：下次获得焦点时引擎会重新建光标、重新要求显示。
    if (!bActive)
    {
        m_bCaretWanted = false;
    }

    // 失焦时擦掉控件自己画的光标并停止闪烁；获得焦点时引擎在上面的
    // WM_SETFOCUS 里已经建好并要求显示光标，这里再核对一次即可。
    OnCaretMaybeChanged(rcBefore);
}

// =================================================================
// 拖放
// =================================================================

IDropTarget* DuiTextHost::CreateEngineDropTarget()
{
    if (m_pServices == nullptr)
    {
        return nullptr;
    }
    IDropTarget* pTarget = nullptr;
    HRESULT hr = m_pServices->TxGetDropTarget(&pTarget);
    if (FAILED(hr))
    {
        return nullptr;
    }
    // 引擎已经把引用计数加好了，直接交给调用方，由它负责释放。
    return pTarget;
}

// =================================================================
// 输入法
// =================================================================

bool DuiTextHost::UpdateImeCompositionPos()
{
    if (m_pSite == nullptr)
    {
        return false;
    }
    HWND hwndHost = m_pSite->TxSiteGetHostHwnd();
    if (hwndHost == nullptr)
    {
        return false;
    }

    HIMC himc = ::ImmGetContext(hwndHost);
    if (himc == nullptr)
    {
        // 当前没有输入法上下文（例如用户用的是纯英文键盘布局）。
        // 不是错误，正常返回失败即可。
        return false;
    }

    // 用光标当前位置作为组字窗的落点。坐标是宿主窗口的客户区坐标 ——
    // 与我们交给引擎、引擎又回传给光标的那套坐标一致，直接可用。
    POINT ptCaret = m_caret.GetPos();

    COMPOSITIONFORM cf;
    ::memset(&cf, 0, sizeof(cf));
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos = ptCaret;
    BOOL bOk = ::ImmSetCompositionWindow(himc, &cf);

    ::ImmReleaseContext(hwndHost, himc);
    return bOk != FALSE;
}

// =================================================================
// 滚动
// =================================================================

bool DuiTextHost::QueryScrollInfo(bool bVertical, int& nMin, int& nMax,
                                  int& nPage, int& nPos, bool& bEnabled)
{
    nMin = 0;
    nMax = 0;
    nPage = 0;
    nPos = 0;
    bEnabled = false;

    if (m_pServices == nullptr)
    {
        return false;
    }

    LONG lMin = 0;
    LONG lMax = 0;
    LONG lPos = 0;
    LONG lPage = 0;
    BOOL bEng = FALSE;
    HRESULT hr = bVertical
        ? m_pServices->TxGetVScroll(&lMin, &lMax, &lPos, &lPage, &bEng)
        : m_pServices->TxGetHScroll(&lMin, &lMax, &lPos, &lPage, &bEng);
    if (FAILED(hr))
    {
        return false;
    }

    nMin = (int)lMin;
    nMax = (int)lMax;
    nPage = (int)lPage;
    nPos = (int)lPos;
    bEnabled = (bEng != FALSE);
    return true;
}

bool DuiTextHost::SetScrollPos(bool bVertical, int nPos)
{
    if (m_pServices == nullptr)
    {
        return false;
    }

    // 防重入的第一个入口。用户拖动滚动条时走到这里，若此刻正处在引擎发起的
    // 滚动通知过程中，就不要再把位置写回去 —— 那正是环的另一半。
    if (m_bInScrollNotify)
    {
        return false;
    }

    m_bInScrollNotify = true;
    // 用"拖动到指定位置"的滚动码把目标位置交给引擎。引擎会按行边界对齐，
    // 因此实际落点可能与 nPos 差一行以内，调用方需要回读一次真实位置。
    m_pServices->TxSendMessage(bVertical ? WM_VSCROLL : WM_HSCROLL,
                               MAKEWPARAM(SB_THUMBPOSITION, nPos), 0, nullptr);
    m_bInScrollNotify = false;
    return true;
}

// =================================================================
// IUnknown
// =================================================================

HRESULT STDMETHODCALLTYPE DuiTextHost::QueryInterface(REFIID riid, void** ppvObject)
{
    if (ppvObject == nullptr)
    {
        return E_POINTER;
    }
    *ppvObject = nullptr;

    if (::IsEqualIID(riid, IID_IUnknown) || ::IsEqualIID(riid, IID_ITextHost))
    {
        *ppvObject = static_cast<ITextHost*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DuiTextHost::AddRef()
{
    // 本对象只在 UI 线程使用，不需要原子操作。
    return ++m_cRef;
}

ULONG STDMETHODCALLTYPE DuiTextHost::Release()
{
    ULONG n = --m_cRef;
    if (n == 0)
    {
        delete this;
    }
    return n;
}

// =================================================================
// ITextHost：设备上下文
// =================================================================

HDC DuiTextHost::TxGetDC()
{
    // 引擎什么时候来问：需要度量文字（算字宽、行高、断行位置）时。
    // 它期望什么：一个可以查询字体度量的设备上下文。
    // 我们答什么：屏幕的设备上下文。
    // 为什么：引擎拿这个上下文只做度量，不会往上面画东西 —— 真正的绘制是
    //   控件在自己的绘制函数里调 TxDraw、并把绘制目标显式传进去的。给屏幕
    //   上下文即可，不必（也不应该）把后台缓冲交出来：后台缓冲的生命周期
    //   由宿主的绘制流程管，引擎随时来取会与之冲突。
    return ::GetDC(nullptr);
}

INT DuiTextHost::TxReleaseDC(HDC hdc)
{
    // 与上一个方法配对归还。收支必须平衡，漏了会泄漏设备上下文。
    return ::ReleaseDC(nullptr, hdc);
}

// =================================================================
// ITextHost：滚动条
// =================================================================

BOOL DuiTextHost::TxShowScrollBar(INT fnBar, BOOL /*fShow*/)
{
    // 引擎什么时候来问：内容量变化导致某个方向该出现或该消失滚动条时。
    // 它期望什么：宿主把对应方向的滚动条显示或隐藏。
    // 我们答什么：只把"这个方向的滚动状态变了"通知控件，让它自己去查。
    // 为什么不直接照做：显不显示滚动条由控件的滚动策略（自动 / 总是 /
    //   从不）决定，不能由引擎单方面拍定。控件收到信号后会连同"内容是否
    //   真的溢出"一起判断。
    NotifyScrollBars(m_pSite, fnBar);
    return TRUE;
}

BOOL DuiTextHost::TxEnableScrollBar(INT fuSBFlags, INT /*fuArrowflags*/)
{
    // 同上，启用 / 禁用也归并成一次"状态变了"的通知。
    NotifyScrollBars(m_pSite, fuSBFlags);
    return TRUE;
}

BOOL DuiTextHost::TxSetScrollRange(INT fnBar, LONG /*nMinPos*/, INT /*nMaxPos*/,
                                   BOOL /*fRedraw*/)
{
    // 引擎什么时候来问：重新排版后内容总高度（或总宽度）变了。
    // 它期望什么：宿主更新滚动条的范围。
    // 我们答什么：同样只发信号。范围值控件会通过 QueryScrollInfo 拿到，
    //   而且那样拿到的是连同可视区大小、当前位置在内的完整一组，比这里
    //   零散地接收更不容易出错。
    NotifyScrollBars(m_pSite, fnBar);
    return TRUE;
}

BOOL DuiTextHost::TxSetScrollPos(INT fnBar, INT /*nPos*/, BOOL /*fRedraw*/)
{
    // 引擎什么时候来问：内容滚动了（用户按方向键、引擎自动把光标滚进视野等）。
    //
    // 防重入的第二个入口。这里必须判，且必须与 SetScrollPos 那处**成对存在**：
    // 引擎要求设位置 → 我们通知控件 → 控件更新滚动条 → 滚动条回调 → 控件把
    // 新位置写回引擎 → 引擎又要求设位置……这是一个环；用户拖动滚动条则是同一个
    // 环的反方向。只在一处加判断只能堵住一个方向，另一个方向照样会无限递归。
    if (m_bInScrollNotify)
    {
        return TRUE;
    }

    m_bInScrollNotify = true;
    NotifyScrollBars(m_pSite, fnBar);
    m_bInScrollNotify = false;
    return TRUE;
}

// =================================================================
// ITextHost：重绘
// =================================================================

void DuiTextHost::TxInvalidateRect(LPCRECT prc, BOOL /*fMode*/)
{
    // 引擎什么时候来问：文字、选区、光标等任何视觉内容发生变化。
    // 它期望什么：宿主把该区域标记为需要重画。
    // 我们答什么：转给控件，由它走 balloonui 的失效机制。
    // 为什么原样转发矩形：balloonui 的宿主上有按矩形失效的接口，局部失效
    //   能少画很多东西 —— 光标闪烁一次只需要重画很窄的一条，整控件重画就
    //   浪费了。prc 为空表示整块，控件那边会区分处理。
    if (prc != nullptr)
    {
        BUI_TRACE("ENGINE-INVALIDATE rect=(%d,%d,%d,%d) site=%s",
                     (int)prc->left, (int)prc->top, (int)prc->right, (int)prc->bottom,
                     m_pSite != nullptr ? "yes" : "null");
    }
    else
    {
        BUI_TRACE("ENGINE-INVALIDATE rect=all site=%s",
                     m_pSite != nullptr ? "yes" : "null");
    }

    if (m_pSite != nullptr)
    {
        m_pSite->TxSiteInvalidate(prc);
    }
}

void DuiTextHost::TxViewChange(BOOL fUpdate)
{
    // 引擎什么时候来问：视图整体发生变化（如滚动、重排）。
    // fUpdate 为假时表示"暂时不用立刻更新"，此时不必重绘。
    BUI_TRACE("ENGINE-VIEWCHANGE update=%d", (int)fUpdate);

    if (fUpdate && m_pSite != nullptr)
    {
        m_pSite->TxSiteInvalidate(nullptr);
    }
}

void DuiTextHost::TxScrollWindowEx(INT /*dx*/, INT /*dy*/,
                                   LPCRECT /*lprcScroll*/, LPCRECT /*lprcClip*/,
                                   HRGN /*hrgnUpdate*/, LPRECT /*lprcUpdate*/,
                                   UINT /*fuScroll*/)
{
    // 引擎什么时候来问：滚动时希望宿主用位块搬移把已经画好的像素挪一挪，
    //   省掉重新排版绘制的开销。
    // 我们答什么：不做搬移，直接整块重画。
    // 为什么可以这样：这是纯粹的性能优化接口，不做不影响正确性。而 balloonui
    //   本身是整窗后台缓冲加一次性上屏的模型，在缓冲里做搬移收益有限，
    //   却要处理裁剪、脏区合并等一堆边界情况，不划算。参考实现同样退化处理。
    if (m_pSite != nullptr)
    {
        m_pSite->TxSiteInvalidate(nullptr);
    }
}

// =================================================================
// ITextHost：光标
// =================================================================

BOOL DuiTextHost::TxCreateCaret(HBITMAP hbmp, INT xWidth, INT yHeight)
{
    // 引擎什么时候来问：控件获得焦点、或者行高变化需要换光标尺寸时。
    // 它期望什么：宿主准备好一个该尺寸的插入光标。
    // 我们答什么：交给 DuiCaret 抢占线程的系统光标（用空白位图创建），同时记下
    //   绘制用的尺寸，界面上的光标由控件按这个尺寸自己画。
    // 为什么还要建系统光标：系统光标的位置同时是**输入法候选窗的定位
    //   依据**。只自绘一根竖线的话，系统不知道插入点在哪里，中文输入时
    //   候选条会跑到窗口左上角。详见 DuiCaret.h 的文件头。
    // 为什么不显示它：见 DuiTextHost.h「光标绘制」一节。
    //
    // hbmp 的所有权归引擎，我们只是转交，绝不能销毁它。
    if (m_pSite == nullptr)
    {
        return FALSE;
    }
    HWND hwndHost = m_pSite->TxSiteGetHostHwnd();
    if (hwndHost == nullptr)
    {
        return FALSE;
    }

    RECT rcBefore;
    GetCaretVisibleRect(rcBefore);

    // 绘制尺寸。引擎给的若是位图，系统会忽略宽高参数、按位图尺寸画，这里照同样
    // 的规则取位图的宽高。hbmp 在「逻辑光标」样式下是一组标志位而不是位图（见
    // docs/windowless-richedit.md 10.1），此时 GetObject 取不到位图信息，自然
    // 退回宽高参数。宽或高为 0 时，系统 CreateCaret 的规则是取窗口边框的宽度。
    m_szCaretPaint.cx = xWidth;
    m_szCaretPaint.cy = yHeight;

    // 上一次的形状位图随旧光标一起作废。
    if (m_hCaretPaintBmp != nullptr)
    {
        ::DeleteObject(m_hCaretPaintBmp);
        m_hCaretPaintBmp = nullptr;
    }

    BITMAP bm;
    if (hbmp != nullptr
        && ::GetObject(hbmp, sizeof(bm), &bm) == (int)sizeof(bm)
        && bm.bmWidth > 0 && bm.bmHeight > 0)
    {
        m_szCaretPaint.cx = bm.bmWidth;
        m_szCaretPaint.cy = bm.bmHeight;
        // 复制一份自己保管，绘制时按它的形状反色。复制失败时退回实心矩形 ——
        // 光标形状略有出入，但不影响输入。
        m_hCaretPaintBmp = (HBITMAP)::CopyImage(hbmp, IMAGE_BITMAP, 0, 0, 0);
    }
    if (m_szCaretPaint.cx <= 0)
    {
        m_szCaretPaint.cx = ::GetSystemMetrics(SM_CXBORDER);
    }
    if (m_szCaretPaint.cy <= 0)
    {
        m_szCaretPaint.cy = ::GetSystemMetrics(SM_CYBORDER);
    }

    // 系统光标用一张同尺寸的全零位图创建：位置、尺寸照常报告给输入法与辅助工具，
    // 但不管谁把它显示出来，屏幕上都不会多出像素。引擎自己给的位图（若有）只用来
    // 决定控件画出的形状，不交给系统。
    //
    // 顺序不能颠倒：旧的系统光标还引用着旧位图，必须先销毁光标、再删旧位图。
    m_caret.Destroy();
    if (m_hSysCaretBlankBmp != nullptr)
    {
        ::DeleteObject(m_hSysCaretBlankBmp);
        m_hSysCaretBlankBmp = nullptr;
    }
    m_hSysCaretBlankBmp = CreateBlankMonoBitmap(m_szCaretPaint.cx, m_szCaretPaint.cy);
    if (m_hSysCaretBlankBmp == nullptr)
    {
        // 建不出空白位图时只能退回实心的系统光标。极少发生，记一条跟踪便于排查。
        BUI_TRACE("ENGINE-CREATECARET blank bitmap FAILED, fall back to a solid system caret");
    }
    const bool bCreated = m_caret.Create(hwndHost, m_hSysCaretBlankBmp,
                                         m_szCaretPaint.cx, m_szCaretPaint.cy);

    BUI_TRACE("ENGINE-CREATECARET hbmp=%p w=%d h=%d paint=%dx%d ok=%d",
              (void*)hbmp, (int)xWidth, (int)yHeight,
              (int)m_szCaretPaint.cx, (int)m_szCaretPaint.cy, bCreated ? 1 : 0);
    if (DuiTrace::IsEnabled() && hbmp != nullptr
        && ::GetObject(hbmp, sizeof(bm), &bm) == (int)sizeof(bm))
    {
        // 位图光标的像素构成：系统只反转位图里置位的像素，全零的位图画出来看不见。
        const LONG cbBits = bm.bmWidthBytes * bm.bmHeight;
        std::vector<BYTE> bits(cbBits > 0 ? (size_t)cbBits : 0);
        LONG cbRead = 0;
        int nonZero = 0;
        if (!bits.empty())
        {
            cbRead = ::GetBitmapBits(hbmp, cbBits, &bits[0]);
            for (LONG i = 0; i < cbRead; ++i)
            {
                if (bits[i] != 0)
                {
                    ++nonZero;
                }
            }
        }
        BUI_TRACE("ENGINE-CREATECARET bitmap %dx%d bpp=%d bytes=%d nonZeroBytes=%d",
                  (int)bm.bmWidth, (int)bm.bmHeight, (int)bm.bmBitsPixel,
                  (int)cbRead, nonZero);
    }

    // 系统光标新建出来是隐藏的，引擎随后会再调 TxShowCaret 要求显示。这里照同样
    // 的语义把「引擎要求显示」复位，控件自己画的光标与之保持一致。
    m_bCaretWanted = false;
    OnCaretMaybeChanged(rcBefore);
    return bCreated ? TRUE : FALSE;
}

BOOL DuiTextHost::TxShowCaret(BOOL fShow)
{
    // 引擎什么时候来问：插入点变化、获得焦点、或者它自己认为该显示光标时；
    //   另外它在 TxDraw 里画字之前会隐藏、画完再显示一次（见 BeginDraw 的注释）。
    // 我们答什么：只记下引擎的要求，由控件在绘制时据此决定画不画光标。
    //   本对象**不调用** ShowCaret，原因见 DuiTextHost.h「光标绘制」一节。
    //
    // **显示请求必须把关，不能照收。** 引擎有时会在控件并无键盘焦点的情况下
    // 要求显示光标 —— 典型是它刚收到一条设置文本或设置选区的命令。若原样
    // 执行，界面上就会出现一个没有焦点的输入框在闪光标，看起来像是可以输入。
    // 参考实现在同一位置做了同样的拦截。
    //
    // 调用方关掉光标显示（SetShowCaret(false)）时，引擎的要求照常记录，只是
    // 绘制时不画 —— 这样重新打开时能立即恢复，不必等引擎再来要求一次。
    // 那一档服务的是「只读展示区」：要的是能点击、能拖选、能复制，但不该有
    // 一个闪烁的光标让人以为可以编辑。注意不能改用「不接受焦点」去达到同样
    // 效果：排版引擎在拖选过程中需要控件持有焦点，不给焦点它就不认为自己在
    // 拖选，文字会变得选不中。
    BUI_TRACE("ENGINE-SHOWCARET show=%d uiActive=%d showCaret=%d inDraw=%d",
              (int)fShow, (int)m_bUiActive, (int)m_bShowCaret, (int)m_bInDraw);

    if (fShow && !m_bUiActive)
    {
        return FALSE;
    }

    RECT rcBefore;
    GetCaretVisibleRect(rcBefore);
    m_bCaretWanted = (fShow != FALSE);
    OnCaretMaybeChanged(rcBefore);
    return m_caret.IsCreated() ? TRUE : FALSE;
}

BOOL DuiTextHost::TxSetCaretPos(INT x, INT y)
{
    // 引擎什么时候来问：插入点移动（打字、点击、方向键、重排版）。
    // 坐标是宿主客户区坐标、像素 —— 与我们在 TxGetClientRect 里交出去的
    // 那套坐标一致，所以直接用，不需要换算。
    BUI_TRACE("ENGINE-CARETPOS x=%d y=%d inDraw=%d", (int)x, (int)y, (int)m_bInDraw);

    RECT rcBefore;
    GetCaretVisibleRect(rcBefore);

    // 系统光标照样移动：它虽然不显示，输入法却要靠这个位置决定候选条弹在哪里。
    const bool bMoved = m_caret.SetPos(x, y);

    // 控件自己画的光标跟着移动：失效新旧两处，并从亮的相位重新计时。
    OnCaretMaybeChanged(rcBefore);
    return bMoved ? TRUE : FALSE;
}

// =================================================================
// 光标绘制
// =================================================================

void DuiTextHost::BeginDraw()
{
    if (m_bInDraw)
    {
        return;
    }
    GetCaretVisibleRect(m_rcCaretBeforeDraw);
    m_bInDraw = true;
}

void DuiTextHost::EndDraw()
{
    if (!m_bInDraw)
    {
        return;
    }
    m_bInDraw = false;

    // 引擎在绘制期间的「隐藏 → 设位置 → 显示」通常原样恢复了光标状态，此时
    // 这里比较结果相同、什么都不做；只有状态确实变了才失效并重新计时。
    OnCaretMaybeChanged(m_rcCaretBeforeDraw);
}

bool DuiTextHost::GetCaretPaintRect(RECT& outRc) const
{
    GetCaretVisibleRect(outRc);
    if (::IsRectEmpty(&outRc) || !m_bCaretBlinkOn)
    {
        ::SetRectEmpty(&outRc);
        return false;
    }
    return true;
}

void DuiTextHost::GetCaretVisibleRect(RECT& outRc) const
{
    ::SetRectEmpty(&outRc);
    if (!m_bShowCaret || !m_bUiActive || !m_bCaretWanted || !m_caret.IsCreated())
    {
        return;
    }
    const POINT pt = m_caret.GetPos();
    outRc.left   = pt.x;
    outRc.top    = pt.y;
    outRc.right  = pt.x + m_szCaretPaint.cx;
    outRc.bottom = pt.y + m_szCaretPaint.cy;
}

void DuiTextHost::OnCaretMaybeChanged(const RECT& rcBefore)
{
    // 绘制期间只记录，由 EndDraw 拿绘制开始时的状态统一比较。
    if (m_bInDraw)
    {
        return;
    }

    RECT rcNow;
    GetCaretVisibleRect(rcNow);
    if (::EqualRect(&rcBefore, &rcNow))
    {
        return;
    }

    BUI_TRACE("CARET-CHANGE before=(%d,%d,%d,%d) now=(%d,%d,%d,%d)",
              (int)rcBefore.left, (int)rcBefore.top, (int)rcBefore.right, (int)rcBefore.bottom,
              (int)rcNow.left, (int)rcNow.top, (int)rcNow.right, (int)rcNow.bottom);

    // 旧位置上画过的光标要擦掉，新位置上要画出来，两处都得重画。
    if (m_pSite != nullptr)
    {
        if (!::IsRectEmpty(&rcBefore))
        {
            m_pSite->TxSiteInvalidate(&rcBefore);
        }
        if (!::IsRectEmpty(&rcNow))
        {
            m_pSite->TxSiteInvalidate(&rcNow);
        }
    }

    if (::IsRectEmpty(&rcNow))
    {
        StopCaretBlink();
    }
    else
    {
        RestartCaretBlink();
    }
}

void DuiTextHost::RestartCaretBlink()
{
    StopCaretBlink();
    m_bCaretBlinkOn = true;

    // 闪烁周期跟随系统设置。用户关闭了闪烁时系统返回 INFINITE，此时光标一直亮，
    // 不需要定时器；返回 0 表示取值失败，同样按一直亮处理。
    const UINT blinkMs = ::GetCaretBlinkTime();
    if (blinkMs == 0 || blinkMs == INFINITE)
    {
        return;
    }

    // 与引擎的定时器一样用线程定时器，并登记进同一张归属表，由 TimerProc 找回本实例。
    const UINT_PTR idSystem = ::SetTimer(nullptr, 0, blinkMs, &DuiTextHost::TimerProc);
    if (idSystem == 0)
    {
        // 定时器建不出来时光标一直亮着，不影响输入，只是不闪。
        BUI_TRACE("CARET-BLINK start FAILED period=%u", blinkMs);
        return;
    }
    m_idCaretBlinkTimer = idSystem;
    RegisterTimerOwner(idSystem, this);
}

void DuiTextHost::StopCaretBlink()
{
    if (m_idCaretBlinkTimer != 0)
    {
        ::KillTimer(nullptr, m_idCaretBlinkTimer);
        UnregisterTimerOwner(m_idCaretBlinkTimer);
        m_idCaretBlinkTimer = 0;
    }
    m_bCaretBlinkOn = true;
}

void DuiTextHost::OnCaretBlinkTimer()
{
    RECT rc;
    GetCaretVisibleRect(rc);
    if (::IsRectEmpty(&rc))
    {
        // 光标已经不该显示，定时器本应已被撤掉。保险起见在这里撤掉。
        StopCaretBlink();
        return;
    }

    m_bCaretBlinkOn = !m_bCaretBlinkOn;
    if (m_pSite != nullptr)
    {
        // 整块失效光标矩形：亮、灭切换必须一次重画整个光标，否则会重演只亮
        // 一段的问题。
        m_pSite->TxSiteInvalidate(&rc);
    }
}

// =================================================================
// ITextHost：定时器
// =================================================================

BOOL DuiTextHost::TxSetTimer(UINT idTimer, UINT uTimeout)
{
    // 引擎什么时候来问：需要周期性回调时。主要用途是光标闪烁，以及拖选
    //   到控件边缘时的自动滚动。
    // 它期望什么：到点后给它发一条定时器消息，带上它给的编号。
    // 我们答什么：挂一个线程定时器，到点后由静态回调找回本实例再转交引擎。
    // 为什么用线程定时器：引擎自选的编号在多个控件实例之间会撞号，而线程
    //   定时器的编号由系统分配、全局唯一。用它就不需要在宿主窗口上再做一层
    //   编号重映射，也不需要给 DuiHost 增加定时器消息路由。

    // 引擎可能用同一个编号重设间隔，先把旧的撤掉。
    TxKillTimer(idTimer);

    int slot = -1;
    for (int i = 0; i < kMaxTimers; ++i)
    {
        if (!m_timers[i].m_bUsed)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        // 槽位用满。正常情况下同时存在的定时器只有一两个，走到这里说明
        // 有泄漏（撤销没配对）。返回失败而不是静默丢弃，让问题暴露出来。
        return FALSE;
    }

    UINT_PTR idSystem = ::SetTimer(nullptr, 0, uTimeout, &DuiTextHost::TimerProc);
    if (idSystem == 0)
    {
        BUI_TRACE("ENGINE-SETTIMER id=%u timeout=%u FAILED", idTimer, uTimeout);
        return FALSE;
    }
    BUI_TRACE("ENGINE-SETTIMER id=%u timeout=%u sysid=%u",
                 idTimer, uTimeout, (unsigned)idSystem);

    m_timers[slot].m_bUsed    = true;
    m_timers[slot].m_idEngine = idTimer;
    m_timers[slot].m_idSystem = idSystem;
    RegisterTimerOwner(idSystem, this);
    return TRUE;
}

void DuiTextHost::TxKillTimer(UINT idTimer)
{
    for (int i = 0; i < kMaxTimers; ++i)
    {
        if (m_timers[i].m_bUsed && m_timers[i].m_idEngine == idTimer)
        {
            ::KillTimer(nullptr, m_timers[i].m_idSystem);
            UnregisterTimerOwner(m_timers[i].m_idSystem);
            m_timers[i].m_bUsed    = false;
            m_timers[i].m_idEngine = 0;
            m_timers[i].m_idSystem = 0;
            return;
        }
    }
}

void CALLBACK DuiTextHost::TimerProc(HWND /*hwnd*/, UINT /*uMsg*/,
                                     UINT_PTR idEvent, DWORD /*dwTime*/)
{
    // 线程定时器不绑窗口，系统只把它分配的编号交给我们，没有 this 指针，
    // 所以要先按编号从归属表里找回实例。
    DuiTextHost* pHost = FindTimerOwner(idEvent);
    if (pHost == nullptr)
    {
        return;
    }

    // 光标闪烁定时器是本对象自己的，不转给引擎。
    if (idEvent == pHost->m_idCaretBlinkTimer)
    {
        pHost->OnCaretBlinkTimer();
        return;
    }

    // 再把系统编号翻译回引擎当初给的编号 —— 引擎只认它自己那个。
    UINT idEngine = 0;
    bool bFound = false;
    for (int i = 0; i < kMaxTimers; ++i)
    {
        if (pHost->m_timers[i].m_bUsed && pHost->m_timers[i].m_idSystem == idEvent)
        {
            idEngine = pHost->m_timers[i].m_idEngine;
            bFound = true;
            break;
        }
    }
    if (!bFound || pHost->m_pServices == nullptr)
    {
        return;
    }

    BUI_TRACE("TIMER-FIRE engineId=%u", idEngine);
    pHost->m_pServices->TxSendMessage(WM_TIMER, idEngine, 0, nullptr);
}

// =================================================================
// ITextHost：捕获、焦点、鼠标指针、坐标
// =================================================================

void DuiTextHost::TxSetCapture(BOOL fCapture)
{
    // 引擎什么时候来问：用户按下左键开始拖选文字（要求捕获），松开时释放。
    // 为什么要有：拖选过程中鼠标很可能移出控件甚至移出窗口，没有捕获就
    //   收不到后续的移动消息，选区会停在边界上。
    if (m_pSite != nullptr)
    {
        m_pSite->TxSiteSetCapture(fCapture != FALSE);
    }
}

void DuiTextHost::TxSetFocus()
{
    // 引擎什么时候来问：用户点击文本区，它认为本控件该拿到键盘焦点。
    if (m_pSite != nullptr)
    {
        m_pSite->TxSiteSetFocus();
    }
}

void DuiTextHost::TxSetCursor(HCURSOR hcur, BOOL /*fText*/)
{
    // 引擎什么时候来问：鼠标移到文本上（要求变成竖线形）或移到别处。
    // hcur 是系统光标句柄，不需要释放。
    if (hcur != nullptr)
    {
        ::SetCursor(hcur);
    }
}

BOOL DuiTextHost::TxScreenToClient(LPPOINT lppt)
{
    // 引擎什么时候来问：处理拖放、或者需要把屏幕坐标换算到自己那套坐标时。
    //
    // 注意前提：我们在 TxGetClientRect 里交给引擎的是**宿主客户区坐标**，
    // 所以这里换算的目标坐标系就是宿主窗口的客户区，两者必须一致。
    // 参考实现在这里有一处隐患 —— 它交出去的是容器坐标，换算的却是宿主
    // 客户区坐标，控件嵌在二级容器里时会差一个偏移。本实现两处用同一套
    // 坐标，天然没有这个问题；但如果将来 balloonui 出现嵌套的绘制容器，
    // 这里需要连同 TxGetClientRect 一起补偏移。
    if (lppt == nullptr || m_pSite == nullptr)
    {
        return FALSE;
    }
    HWND hwndHost = m_pSite->TxSiteGetHostHwnd();
    if (hwndHost == nullptr)
    {
        return FALSE;
    }
    return ::ScreenToClient(hwndHost, lppt);
}

BOOL DuiTextHost::TxClientToScreen(LPPOINT lppt)
{
    if (lppt == nullptr || m_pSite == nullptr)
    {
        return FALSE;
    }
    HWND hwndHost = m_pSite->TxSiteGetHostHwnd();
    if (hwndHost == nullptr)
    {
        return FALSE;
    }
    return ::ClientToScreen(hwndHost, lppt);
}

// =================================================================
// ITextHost：激活状态
// =================================================================

HRESULT DuiTextHost::TxActivate(LONG* plOldState)
{
    // 引擎什么时候来问：它自己认为需要进入激活状态时。
    // 我们答什么：返回旧状态并置为激活。真正驱动激活状态的是控件的焦点
    //   变化（走 SetUiActive），这里只是配合引擎的请求。
    if (plOldState != nullptr)
    {
        *plOldState = m_bUiActive ? 1 : 0;
    }
    m_bUiActive = true;
    return S_OK;
}

HRESULT DuiTextHost::TxDeactivate(LONG /*lNewState*/)
{
    m_bUiActive = false;
    return S_OK;
}

// =================================================================
// ITextHost：几何与格式（引擎回来问数据）
// =================================================================

HRESULT DuiTextHost::TxGetClientRect(LPRECT prc)
{
    // 引擎什么时候来问：每次重新排版之前，用来确定可用的排版宽度与高度。
    // 它期望什么：**像素**单位的矩形。
    // 我们答什么：控件推进来的文本区矩形，宿主客户区坐标。
    // 为什么这一套坐标：它会传染 —— 引擎之后回传的失效矩形、光标位置、
    //   命中坐标全都用这一套。选宿主客户区坐标是因为 balloonui 的控件矩形
    //   本来就是这套，光标接口要的也是这套，一路不用换算。
    //
    // 与紧邻的 TxGetViewInset 对比：那个方法要的是**百分之毫米**。两个方法
    // 名字像、位置挨着、都返回矩形，单位却不同，是本类最容易搞错的一处。
    if (prc == nullptr)
    {
        return E_POINTER;
    }
    *prc = m_rcClient;
    return S_OK;
}

HRESULT DuiTextHost::TxGetViewInset(LPRECT prc)
{
    // 引擎什么时候来问：排版时确定文字与排版区域边界之间留多少空白。
    // 它期望什么：**百分之毫米**单位（不是像素！）。
    // 我们答什么：SetViewInsetPixels 里换算好并存下来的那份值。
    if (prc == nullptr)
    {
        return E_POINTER;
    }
    *prc = m_rcViewInset;
    return S_OK;
}

HRESULT DuiTextHost::TxGetCharFormat(const CHARFORMATW** ppCF)
{
    // 引擎什么时候来问：创建过程中、以及每次收到字符格式变更通知之后。
    // 它期望什么：一个指向默认字符格式的指针。
    // 我们答什么：内部成员的地址。
    // 所有权：**指针指向我们的成员，所有权归我们**，引擎只读、不释放。
    //   因此这个成员的生命周期必须覆盖引擎的整个使用期，不能是临时变量。
    //
    // 注意接口声明的是旧版结构指针，我们实际存的是扩展版结构。这是接口的
    // 既定用法 —— 引擎靠结构体开头的 cbSize 字段区分两者，所以那个字段
    // 必须填对，填错会被当成旧版结构解析、颜色和效果位全部错位。
    if (ppCF == nullptr)
    {
        return E_POINTER;
    }
    *ppCF = reinterpret_cast<const CHARFORMATW*>(&m_cfDefault);
    return S_OK;
}

HRESULT DuiTextHost::TxGetParaFormat(const PARAFORMAT** ppPF)
{
    // 同上，段落格式版本。所有权与 cbSize 的注意点完全一致。
    if (ppPF == nullptr)
    {
        return E_POINTER;
    }
    *ppPF = reinterpret_cast<const PARAFORMAT*>(&m_pfDefault);
    return S_OK;
}

COLORREF DuiTextHost::TxGetSysColor(int nIndex)
{
    // 引擎什么时候来问：绘制选区高亮、禁用态文字等需要系统配色的地方。
    // 我们答什么：默认原样转发给系统；调用方设过覆盖色时拦下选区那两项。
    // 为什么默认跟随系统：与系统其它输入框保持一致，符合用户习惯。需要
    //   统一视觉的场景可以用 SetSelectionColors 覆盖。
    if (nIndex == COLOR_HIGHLIGHT && m_crSelBack != CLR_INVALID)
    {
        return m_crSelBack;
    }
    if (nIndex == COLOR_HIGHLIGHTTEXT && m_crSelText != CLR_INVALID)
    {
        return m_crSelText;
    }
    return ::GetSysColor(nIndex);
}

HRESULT DuiTextHost::TxGetBackStyle(TXTBACKSTYLE* pstyle)
{
    // 引擎什么时候来问：绘制前确定要不要自己擦背景。
    // 我们答什么：默认透明。
    // 为什么默认透明：透明时引擎不擦背景，控件可以自己画圆角、渐变、
    //   半透明的底 —— 这是无窗口路线相对真窗口控件的一项关键优势。
    //   代价是控件**每次绘制都必须自己把背景铺满**，漏了会看到上一帧残留。
    if (pstyle == nullptr)
    {
        return E_POINTER;
    }
    *pstyle = m_bBackTransparent ? TXTBACK_TRANSPARENT : TXTBACK_OPAQUE;
    return S_OK;
}

HRESULT DuiTextHost::TxGetMaxLength(DWORD* plength)
{
    if (plength == nullptr)
    {
        return E_POINTER;
    }
    *plength = m_dwMaxLength;
    return S_OK;
}

HRESULT DuiTextHost::TxGetScrollBars(DWORD* pdwScrollBar)
{
    // 引擎什么时候来问：确定该不该维护滚动状态、内容溢出时能不能滚。
    // 注意这与"滚动条要不要画出来"是两件事 —— 画不画由控件的滚动策略决定。
    if (pdwScrollBar == nullptr)
    {
        return E_POINTER;
    }
    *pdwScrollBar = m_dwScrollBars;
    return S_OK;
}

HRESULT DuiTextHost::TxGetPasswordChar(TCHAR* pch)
{
    if (pch == nullptr)
    {
        return E_POINTER;
    }
    *pch = m_chPasswordChar;
    return S_OK;
}

HRESULT DuiTextHost::TxGetAcceleratorPos(LONG* pcp)
{
    // 助记符是"文件(&F)"里那个带下划线的字母。本控件是文本编辑控件，
    // 不涉及助记符，恒返回 -1 表示没有。
    if (pcp == nullptr)
    {
        return E_POINTER;
    }
    *pcp = -1;
    return S_OK;
}

HRESULT DuiTextHost::TxGetExtent(LPSIZEL lpExtent)
{
    // 引擎什么时候来问：需要知道排版区域的整体尺寸时。
    // 它期望什么：**百分之毫米**单位（与 TxGetViewInset 同类，与
    //   TxGetClientRect 不同）。
    // 我们答什么：RecalcExtent 里按客户区尺寸换算好的那份值。
    if (lpExtent == nullptr)
    {
        return E_POINTER;
    }
    *lpExtent = m_sizeExtent;
    return S_OK;
}

HRESULT DuiTextHost::OnTxCharFormatChange(const CHARFORMATW* /*pCF*/)
{
    // 引擎什么时候来问：由引擎侧发起的默认字符格式变更（例如业务通过
    //   设置默认格式的命令改了字体）。
    // 我们答什么：直接成功。
    // 为什么可以不处理：我们自己保存的那份默认格式是"控件推进来的意图"，
    //   引擎内部的变更不需要回写到它上面 —— 否则控件下次重新推送时会与
    //   引擎的改动互相覆盖，谁说了算变得不可预测。
    return S_OK;
}

HRESULT DuiTextHost::OnTxParaFormatChange(const PARAFORMAT* /*pPF*/)
{
    // 同上，段落格式版本。
    return S_OK;
}

HRESULT DuiTextHost::TxGetPropertyBits(DWORD dwMask, DWORD* pdwBits)
{
    // 引擎什么时候来问：创建过程中一次，以及每次收到属性变更通知之后。
    // 它期望什么：只返回掩码里问到的那几位，其余位必须为零。
    if (pdwBits == nullptr)
    {
        return E_POINTER;
    }
    *pdwBits = m_dwPropertyBits & dwMask;
    return S_OK;
}

HRESULT DuiTextHost::TxNotify(DWORD iNotify, void* pv)
{
    // 引擎什么时候来问：内容变化、选区变化、链接被点击、到达长度上限等。
    // 返回值有语义：少数通知返回非成功表示宿主否决这次操作，因此这里要把
    //   控件的返回值原样带回去，不能一律返回成功。
    if (m_pSite != nullptr)
    {
        return m_pSite->TxSiteNotify(iNotify, pv);
    }
    return S_OK;
}

HIMC DuiTextHost::TxImmGetContext()
{
    // 引擎什么时候来问：用户开始用输入法组字时。
    // 它期望什么：一个输入法上下文。
    // 我们答什么：宿主窗口的那个。
    // 为什么：本控件没有自己的窗口，而输入法上下文是挂在窗口上的资源，
    //   只能借宿主窗口的来用。这也是无窗口控件必须由宿主配合才能输入
    //   中文的根本原因。
    if (m_pSite == nullptr)
    {
        return nullptr;
    }
    HWND hwndHost = m_pSite->TxSiteGetHostHwnd();
    if (hwndHost == nullptr)
    {
        return nullptr;
    }
    return ::ImmGetContext(hwndHost);
}

void DuiTextHost::TxImmReleaseContext(HIMC himc)
{
    // 与上一个方法配对归还。漏了会泄漏输入法上下文，表现为一段时间后
    // 输入法工作异常。
    if (himc == nullptr || m_pSite == nullptr)
    {
        return;
    }
    HWND hwndHost = m_pSite->TxSiteGetHostHwnd();
    if (hwndHost == nullptr)
    {
        return;
    }
    ::ImmReleaseContext(hwndHost, himc);
}

HRESULT DuiTextHost::TxGetSelectionBarWidth(LONG* lSelBarWidth)
{
    // 整行选择条是文档编辑器左侧那条"点一下选中整行"的窄带。本控件不提供
    // 该功能，返回 0 即关闭。
    if (lSelBarWidth == nullptr)
    {
        return E_POINTER;
    }
    *lSelBarWidth = 0;
    return S_OK;
}

// =================================================================
// 私有辅助
// =================================================================

void DuiTextHost::NotifyPropertyChange(DWORD dwMask, DWORD dwBits)
{
    if (m_pServices == nullptr)
    {
        return;
    }
    m_pServices->OnTxPropertyBitsChange(dwMask, dwBits);
}

void DuiTextHost::RecalcExtent()
{
    int dpiX = kFallbackDpi;
    int dpiY = kFallbackDpi;
    GetDevicePixelsPerInch(dpiX, dpiY);

    int cx = m_rcClient.right - m_rcClient.left;
    int cy = m_rcClient.bottom - m_rcClient.top;
    if (cx < 0)
    {
        cx = 0;
    }
    if (cy < 0)
    {
        cy = 0;
    }

    m_sizeExtent.cx = ::MulDiv(cx, kHimetricPerInch, dpiX);
    m_sizeExtent.cy = ::MulDiv(cy, kHimetricPerInch, dpiY);

    // 尺寸变化必须同时通知这两位，否则引擎会继续按旧尺寸排版 ——
    // 症状是改了控件宽度但文字的换行位置不变。这是重排的命脉。
    NotifyPropertyChange(TXTBIT_EXTENTCHANGE | TXTBIT_CLIENTRECTCHANGE,
                         TXTBIT_EXTENTCHANGE | TXTBIT_CLIENTRECTCHANGE);
}

void DuiTextHost::GetDevicePixelsPerInch(int& outX, int& outY)
{
    outX = kFallbackDpi;
    outY = kFallbackDpi;

    HDC hdc = ::GetDC(nullptr);
    if (hdc == nullptr)
    {
        return;
    }
    int x = ::GetDeviceCaps(hdc, LOGPIXELSX);
    int y = ::GetDeviceCaps(hdc, LOGPIXELSY);
    ::ReleaseDC(nullptr, hdc);

    // 取到 0 会让后面的换算除零，兜底成基准值。
    if (x > 0)
    {
        outX = x;
    }
    if (y > 0)
    {
        outY = y;
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
