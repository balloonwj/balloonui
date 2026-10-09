/**
 *  DuiRichEdit 的实现。设计背景与选用建议见 DuiRichEdit.h 的文件头注释；
 *  引擎与宿主之间的协作机制见 DuiTextHost.h。
 *  balloonwj@qq.com   2026-08-14
 */

#include "stdafx.h"

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT
#include "DuiRichEdit.h"
#include "../../DuiTrace.h"
#include "../../DuiHost.h"
#include "../../DuiResMgr.h"
#if BUI_FEATURE_IMAGEOLE
#include "../Media/DuiImageOle.h"   // 内联图片对象（仅启用 IMAGEOLE 特性时用）
#include <richole.h>
#include <algorithm>
#endif

#if defined(BUI_FEATURE_MENU)
#include "../List/DuiMenu.h"   // 自绘弹出菜单（仅启用 MENU 特性时用）
#endif

#include <vector>

namespace balloonwjui {

namespace {

// 边框颜色的四种状态。取值与库内其它文本输入控件保持一致，
// 多个输入控件放在同一个界面上时观感统一。
const COLORREF kBorderIdle     = RGB(205, 210, 216);   // 常态
const COLORREF kBorderHover    = RGB(170, 178, 188);   // 鼠标悬停
const COLORREF kBorderFocused  = RGB( 45, 108, 223);   // 持有键盘焦点
const COLORREF kBorderDisabled = RGB(228, 231, 235);   // 禁用

// 禁用态的背景填充色。
const COLORREF kFillDisabled = RGB(246, 247, 249);

// 占位文字的颜色。
const COLORREF kPlaceholderText = RGB(160, 166, 173);

// 按单色位图画光标时，位图里的 0 位与 1 位分别换成的颜色。单色位图拷到彩色目标
// 上时，0 位取目标的文字色、1 位取目标的背景色；再与目标异或，黑色不改变像素，
// 白色把像素取反。见 DuiRichEdit::PaintCaret。
const COLORREF kCaretShapeZeroBitColor = RGB(0, 0, 0);
const COLORREF kCaretShapeOneBitColor  = RGB(255, 255, 255);

// 覆盖式滚动条命中带的宽度（像素）。2026-10-04 起与库内其它悬浮式滚动条统一取
// DuiScrollBar::kOverlayBandPx(11)：画出来的只是其中一根 5 像素的细滑块（见
// DuiScrollBar），命中带比滑块宽，便于鼠标抓住，又不会明显遮挡文字。
#if BUI_FEATURE_SCROLLBAR
const int kOverlayScrollBarThickness = DuiScrollBar::kOverlayBandPx;
#endif

// 默认内边距（像素）。沿用库内文本输入控件一贯的默认值。
const int kDefaultMarginLeft   = 4;
const int kDefaultMarginTop    = 2;
const int kDefaultMarginRight  = 4;
const int kDefaultMarginBottom = 2;

// 量内容高度时临时撑给引擎的排版高度（像素）。
//
// 引擎量出来的内容高度不会超过它当前的排版区域，所以测量前要先把文本区
// 撑到一个远大于任何实际控件高度的值（见 MeasureContentHeight）。取值只需
// 「不构成限制」，本身没有精确含义。
const int kMeasureRoomHeight = 10000;

// GetKeyState 返回值里表示「该键当前处于按下状态」的那一位。
const SHORT kKeyDownMask = (SHORT)0x8000;

// 判断某个虚拟键是不是「唤出右键菜单」的快捷键。
//
// 认两个：菜单键（VK_APPS，键盘右侧 Ctrl 旁画着菜单图案的那个键），以及
// Shift+F10 —— 后者是前者的等价组合，给没有菜单键的键盘留的路，笔记本上
// 缺这个键很常见。
//   vk：虚拟键码。
//   返回：是唤出菜单的按键返回 true。
bool IsContextMenuHotKey(UINT vk)
{
    if (vk == VK_APPS)
    {
        return true;
    }
    if (vk == VK_F10 && (::GetKeyState(VK_SHIFT) & kKeyDownMask) != 0)
    {
        return true;
    }
    return false;
}

#if BUI_FEATURE_IMAGEOLE

// 一个内联图片在文档里的位置与标记，按位置升序排序。
//
// 枚举图片对象得到的次序未必是文档顺序，所以要收集起来自己排一遍，
// 否则拼出来的图文序列会乱序。
struct EmbeddedImagePos
{
    long      m_cp;    // 图片在文档里的字符位置
    DWORD_PTR m_tag;   // 插入时附着的标记

    bool operator<(const EmbeddedImagePos& other) const
    {
        return m_cp < other.m_cp;
    }
};

// 等比缩放后的目标尺寸 —— 把源尺寸塞进约束框里，保持宽高比，**只缩不放**。
//   nSrcW / nSrcH：源尺寸（像素）。任一 <= 0 时原样返回。
//   nMaxW / nMaxH：约束框（像素）；<= 0 表示该方向不限。
//   返回：目标尺寸（像素），每个分量至少为 1。
SIZE FitImageSize(int nSrcW, int nSrcH, int nMaxW, int nMaxH)
{
    SIZE out;
    out.cx = nSrcW;
    out.cy = nSrcH;
    if (nSrcW <= 0 || nSrcH <= 0)
    {
        return out;
    }
    // 只缩不放：两个方向都已经在框内就原样返回。
    if ((nMaxW <= 0 || nSrcW <= nMaxW) && (nMaxH <= 0 || nSrcH <= nMaxH))
    {
        return out;
    }

    const double dScaleX = (nMaxW > 0) ? ((double)nMaxW / nSrcW) : 1.0;
    const double dScaleY = (nMaxH > 0) ? ((double)nMaxH / nSrcH) : 1.0;
    const double dScale  = (dScaleX < dScaleY) ? dScaleX : dScaleY;

    // **必须四舍五入而不是截断。** 举例：源宽 56、上限 24 时比例是
    // 0.428571428571428549…，乘回 56 得 23.999999999999996，截断会得到 23 ——
    // 比要求的尺寸小一个像素，与调用方约定的排版尺寸对不上，绘制时就变成
    // 「先缩小再放大」，图会发糊。
    const double kRoundHalf = 0.5;
    int nW = (int)(nSrcW * dScale + kRoundHalf);
    int nH = (int)(nSrcH * dScale + kRoundHalf);
    out.cx = (nW > 1) ? nW : 1;
    out.cy = (nH > 1) ? nH : 1;
    return out;
}

#endif // BUI_FEATURE_IMAGEOLE

// 取当前屏幕的每英寸点数。取不到时返回基准值，保证后续换算不会除零。
void GetScreenDpi(int& outX, int& outY)
{
    const int kFallbackDpi = 96;
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
    if (x > 0)
    {
        outX = x;
    }
    if (y > 0)
    {
        outY = y;
    }
}

// 去掉文档末尾的回车与换行。
//
// 引擎会在文档尾部维护一个结尾标记，读回来的文本因此比写进去的多一个
// 回车符。这是引擎的既定行为（2026-08-14 实测确认），不是缺陷。本控件
// 统一在读取出口处归一化，保证调用方看到的是「设进去什么就读回什么」。
CString TrimTrailingEol(const wchar_t* psz)
{
    if (psz == nullptr)
    {
        return CString();
    }
    CString s(psz);
    while (!s.IsEmpty())
    {
        TCHAR ch = s[s.GetLength() - 1];
        if (ch != _T('\r') && ch != _T('\n'))
        {
            break;
        }
        s.Delete(s.GetLength() - 1);
    }
    return s;
}

} // 匿名命名空间

// =================================================================
// 构造与析构
// =================================================================

DuiRichEdit::DuiRichEdit()
    : m_pTextHost(nullptr)
    , m_crBackground(RGB(255, 255, 255))
    , m_crText(RGB(30, 30, 30))
    , m_bShowBorder(true)
    , m_nMarginLeft(kDefaultMarginLeft)
    , m_nMarginTop(kDefaultMarginTop)
    , m_nMarginRight(kDefaultMarginRight)
    , m_nMarginBottom(kDefaultMarginBottom)
    , m_bFocusable(true)
    , m_bPastePlainText(false)
    , m_nMaxLength(0)
    , m_pDropTarget(nullptr)
    , m_bDropDispatchRequested(false)
    , m_bAutoGrow(true)
    , m_nAutoGrowMin(0)
    , m_nAutoGrowMax(0)
    , m_bContextMenuEnabled(true)
    , m_nLastGrowReqW(0)
    , m_nLastGrowReqH(0)
    , m_hDefaultFont(nullptr)
    , m_bScrollDirtyV(false)
    , m_bScrollDirtyH(false)
    , m_vScrollPolicy(kScrollBarAuto)
    , m_hScrollPolicy(kScrollBarAuto)
#if BUI_FEATURE_SCROLLBAR
    , m_pScrollV(nullptr)
    , m_pScrollH(nullptr)
#endif
{
    ::SetRect(&m_rcText, 0, 0, 0, 0);

    SetTabStop(true);

#if BUI_FEATURE_SCROLLBAR
    // 两个方向的滚动条都建出来，用不用得上由策略和内容是否溢出决定。
    //
    // 作为**子控件**加入，好处是自动参与命中测试：用户去拖滑块时，鼠标
    // 事件会先命中滚动条而不是文本区，不需要自己写坐标判断。
    //
    // 默认开启自动隐藏。隐藏状态下滚动条完全不绘制（不是画成透明），
    // 所以覆盖在文字上也不会挡住任何东西。
    std::unique_ptr<DuiScrollBar> spScrollV(new DuiScrollBar(/*horizontal=*/false));
    m_pScrollV = spScrollV.get();
    m_pScrollV->SetAutoHide(true);
    m_pScrollV->SetVisible(false);
    m_pScrollV->SetOnScroll(&DuiRichEdit::OnVScrollBarMoved, this);
    AddChild(std::move(spScrollV));

    std::unique_ptr<DuiScrollBar> spScrollH(new DuiScrollBar(/*horizontal=*/true));
    m_pScrollH = spScrollH.get();
    m_pScrollH->SetAutoHide(true);
    m_pScrollH->SetVisible(false);
    m_pScrollH->SetOnScroll(&DuiRichEdit::OnHScrollBarMoved, this);
    AddChild(std::move(spScrollH));
#endif

    // 引擎在构造函数里就建出来 —— 它不依赖任何窗口（2026-08-14 实测确认），
    // 所以本控件不需要早先内嵌真子窗口的实现那样的「创建」步骤。构造完即可用。
    //
    // 这里把 this 作为回调接口交出去。此刻控件还没挂进 DUI 树，
    // 取宿主窗口句柄会得到空值，宿主实现那边对此有判空处理。
    m_pTextHost = new DuiTextHost();
    if (!m_pTextHost->Init(this))
    {
        // 系统缺少引擎库。控件退化成一个只会画背景和边框的空框，
        // 不崩溃、也不影响界面其余部分。
        m_pTextHost->Release();
        m_pTextHost = nullptr;
        return;
    }

    m_pTextHost->SetDefaultFont(GetDefaultFont(), m_crText);

    // 订阅内容变化与选区变化通知。不订阅的话引擎不会发，
    // 业务就收不到「文字改变了」的事件。
    ITextServices* pSvc = m_pTextHost->GetTextServices();
    if (pSvc != nullptr)
    {
        pSvc->TxSendMessage(EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE, nullptr);

        // 调整语言选项，主要是为了**关掉「按输入内容自动换字体」**。
        //
        // 开着它的话，用户在一行英文里插入中文，引擎会自作主张给中文换一种
        // 字体，同一行里两种字体并存，字形高矮不一，很难看。关掉之后所有
        // 文字统一用我们设的默认字体。
        //
        // 另外三项按参考实现的做法打开：随输入语言切换键盘布局、允许中英文
        // 分别使用不同字体的内部机制、以及用界面字体渲染输入法候选内容。
        LRESULT lLangOptions = 0;
        pSvc->TxSendMessage(EM_GETLANGOPTIONS, 0, 0, &lLangOptions);
        lLangOptions |= (IMF_AUTOKEYBOARD | IMF_DUALFONT | IMF_UIFONTS);
        lLangOptions &= ~IMF_AUTOFONT;
        pSvc->TxSendMessage(EM_SETLANGOPTIONS, 0, (LPARAM)lLangOptions, nullptr);
    }
}

DuiRichEdit::~DuiRichEdit()
{
    if (m_pDropTarget != nullptr)
    {
        m_pDropTarget->Release();
        m_pDropTarget = nullptr;
    }

    if (m_pTextHost != nullptr)
    {
        // 先断开与本控件的联系，再让出引用。顺序不能反 —— 反了的话，
        // 引擎在释放过程中若还回调进来，就会访问到正在析构的控件。
        m_pTextHost->Shutdown();
        m_pTextHost->Release();
        m_pTextHost = nullptr;
    }
}

// =================================================================
// 文本
// =================================================================

void DuiRichEdit::SetText(LPCTSTR sz)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }
    ITextServices* pSvc = m_pTextHost->GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }

    CStringW wide(sz != nullptr ? sz : _T(""));
    pSvc->TxSetText(wide);

    // 内容换了，期望高度多半也变了，请求重新排版。
    //
    // 这一步不能指望 EN_CHANGE 那条路 —— 那是引擎在**用户编辑**内容时发出的
    // 通知，程序直接调 TxSetText 换内容并不会走到那里。少了这一步的症状是：
    // 用代码往自动增高的编辑器里填几行文字，控件高度纹丝不动。
    RequestAutoGrowRelayout();
    Invalidate();
}

CString DuiRichEdit::GetText() const
{
    if (m_pTextHost == nullptr)
    {
        return CString();
    }
    ITextServices* pSvc = m_pTextHost->GetTextServices();
    if (pSvc == nullptr)
    {
        return CString();
    }

    BSTR bstr = nullptr;
    if (FAILED(pSvc->TxGetText(&bstr)) || bstr == nullptr)
    {
        return CString();
    }
    CString result = TrimTrailingEol(bstr);
    ::SysFreeString(bstr);
    return result;
}

int DuiRichEdit::GetTextLength() const
{
    // 走 GetText 而不是问引擎要长度，是为了让长度口径与 GetText 严格一致 ——
    // 引擎报的长度把文档结尾标记也算进去了，与 GetText 归一化之后的结果差一。
    // 两个接口口径不一致会让调用方写出「长度大于 0 但取到空串」这类判断错误。
    return GetText().GetLength();
}

// =================================================================
// 基本属性
// =================================================================

void DuiRichEdit::SetReadOnly(bool b)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }
    m_pTextHost->SetPropertyBits(TXTBIT_READONLY, b ? TXTBIT_READONLY : 0);
    Invalidate();
}

bool DuiRichEdit::IsReadOnly() const
{
    if (m_pTextHost == nullptr)
    {
        return false;
    }
    return (m_pTextHost->GetPropertyBits_() & TXTBIT_READONLY) != 0;
}

void DuiRichEdit::SetMultiLine(bool b)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }
    // 运行期随时可改，内容不会丢 —— 这正是无窗口路线相对真窗口控件的
    // 关键改进：后者的多行是窗口风格位，改一次就得销毁重建整个子窗口。
    m_pTextHost->SetPropertyBits(TXTBIT_MULTILINE, b ? TXTBIT_MULTILINE : 0);
    Invalidate();
}

bool DuiRichEdit::IsMultiLine() const
{
    if (m_pTextHost == nullptr)
    {
        return false;
    }
    return (m_pTextHost->GetPropertyBits_() & TXTBIT_MULTILINE) != 0;
}

void DuiRichEdit::SetWordWrap(bool b)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }
    m_pTextHost->SetPropertyBits(TXTBIT_WORDWRAP, b ? TXTBIT_WORDWRAP : 0);

    // 换行开关直接决定横向会不会溢出，所以要连带更新告诉引擎的滚动条配置：
    // 开着自动换行时横向永远不溢出，没必要让引擎维护水平滚动范围。
    UpdateEngineScrollBars();

    m_bScrollDirtyV = true;
    m_bScrollDirtyH = true;
    Invalidate();
}

void DuiRichEdit::UpdateEngineScrollBars()
{
    if (m_pTextHost == nullptr)
    {
        return;
    }

    // 竖直方向恒定开着 —— 多行文本总有可能超出高度。
    DWORD dwBars = WS_VSCROLL | ES_AUTOVSCROLL;

    // 水平方向只在关掉自动换行时才开。开着换行的话内容永远不会横向超出，
    // 让引擎去维护一份恒为零的水平滚动范围没有意义。
    if (!IsWordWrap())
    {
        dwBars |= (WS_HSCROLL | ES_AUTOHSCROLL);
    }
    m_pTextHost->SetScrollBars(dwBars);
}

bool DuiRichEdit::IsWordWrap() const
{
    if (m_pTextHost == nullptr)
    {
        return false;
    }
    return (m_pTextHost->GetPropertyBits_() & TXTBIT_WORDWRAP) != 0;
}

bool DuiRichEdit::SetPlainTextMode(bool bPlain)
{
    // 引擎不可用时下发的消息一律返回 0，会被误当成切换成功，所以先把它排除。
    if (m_pTextHost == nullptr || Test_GetTextServices() == nullptr)
    {
        return false;
    }
    if (IsPlainTextMode() == bPlain)
    {
        return true;
    }

    // 先让引擎切换，成功后再同步宿主记录的属性位，顺序不能反过来。经属性位
    // 通知引擎时，宿主的通知函数不向调用方返回引擎的处理结果（见
    // DuiTextHost::NotifyPropertyChange），引擎拒绝了也无从得知，宿主与引擎
    // 的记录就会不一致；这条消息则明确返回成败，文档非空时返回非 0。
    //
    // 只给出文本模式这一组标志。撤销级数、代码页这两组不给，引擎保持当前的
    // 设置不变（2026-09-30 用同一引擎实测：模式值由 0x2A 变为 0x29，只有
    // 纯文本 / 富文本这两位发生变化；用例 PlainTextModeSwitch 钉住了这一点）。
    const LRESULT lResult = SendMessageToEngine(EM_SETTEXTMODE,
                                                bPlain ? TM_PLAINTEXT : TM_RICHTEXT, 0);
    if (lResult != 0)
    {
        return false;
    }

    // 引擎已经切换完成，这里只是让宿主的属性位与之一致。引擎之后再询问属性位
    // 时拿到的是与自身状态相符的值，通知下去也不会引起任何变化。
    m_pTextHost->SetPropertyBits(TXTBIT_RICHTEXT, bPlain ? 0 : TXTBIT_RICHTEXT);
    Invalidate();
    return true;
}

bool DuiRichEdit::IsPlainTextMode() const
{
    if (m_pTextHost == nullptr)
    {
        return false;
    }
    return (m_pTextHost->GetPropertyBits_() & TXTBIT_RICHTEXT) == 0;
}

void DuiRichEdit::SetFocusable(bool b)
{
    m_bFocusable = b;

    // 同步 Tab 轮转资格。宿主在鼠标点击时也按这个标志决定要不要把焦点
    // 交过来，所以两者必须一致。
    SetTabStop(b);

    // 关掉可聚焦时，若当前正持有焦点，要主动交出去并撤掉光标，
    // 否则界面上会留下一个不该存在的闪烁光标。
    if (!b && m_bFocused && m_pTextHost != nullptr)
    {
        m_pTextHost->SetUiActive(false);
    }
    Invalidate();
}

// =================================================================
// 外观
// =================================================================

void DuiRichEdit::SetShowCaret(bool b)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }
    m_pTextHost->SetShowCaret(b);
    Invalidate();
}

bool DuiRichEdit::IsShowCaret() const
{
    if (m_pTextHost == nullptr)
    {
        // 引擎不可用时控件退化成空框，本来就画不出光标。
        return false;
    }
    return m_pTextHost->IsShowCaret();
}

void DuiRichEdit::SetBackgroundColor(COLORREF cr)
{
    m_crBackground = cr;
    Invalidate();
}

void DuiRichEdit::SetTextColor(COLORREF cr)
{
    m_crText = cr;
    if (m_pTextHost != nullptr)
    {
        HFONT hFont = (m_hDefaultFont != nullptr)
                    ? m_hDefaultFont
                    : GetDefaultFont();
        m_pTextHost->SetDefaultFont(hFont, m_crText);
    }
    Invalidate();
}

void DuiRichEdit::SetShowBorder(bool b)
{
    if (m_bShowBorder == b)
    {
        return;
    }
    m_bShowBorder = b;
    // 边框占一像素，去掉之后文本区会变大，必须重算并通知引擎重排。
    UpdateTextRect();
    Invalidate();
}

void DuiRichEdit::SetMargins(int left, int top, int right, int bottom)
{
    m_nMarginLeft   = (left   >= 0) ? left   : 0;
    m_nMarginTop    = (top    >= 0) ? top    : 0;
    m_nMarginRight  = (right  >= 0) ? right  : 0;
    m_nMarginBottom = (bottom >= 0) ? bottom : 0;
    UpdateTextRect();
    Invalidate();
}

void DuiRichEdit::SetDefaultFontFromHFONT(HFONT font)
{
    m_hDefaultFont = font;
    if (m_pTextHost != nullptr)
    {
        HFONT hUse = (font != nullptr) ? font : GetDefaultFont();
        m_pTextHost->SetDefaultFont(hUse, m_crText);
    }
    Invalidate();
}

void DuiRichEdit::OnDpiChanged(int /*dpi*/)
{
    if (m_pTextHost != nullptr && m_hDefaultFont == nullptr)
    {
        m_pTextHost->SetDefaultFont(GetDefaultFont(), m_crText);
    }
    Invalidate();
}

void DuiRichEdit::SetPlaceholder(LPCTSTR sz)
{
    m_strPlaceholder = (sz != nullptr) ? sz : _T("");
    Invalidate();
}

bool DuiRichEdit::IsShowingPlaceholder() const
{
    // 三个条件缺一不可：有占位文字、文档为空、当前未持有焦点。
    // 聚焦时不显示，是为了不让占位文字与光标叠在一起。
    return !m_strPlaceholder.IsEmpty() && !m_bFocused && GetTextLength() == 0;
}

void DuiRichEdit::SetSelectionColors(COLORREF crBack, COLORREF crText)
{
    if (m_pTextHost != nullptr)
    {
        m_pTextHost->SetSelectionColors(crBack, crText);
    }
    Invalidate();
}

// =================================================================
// 布局与绘制
// =================================================================

void DuiRichEdit::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    UpdateTextRect();

    // 挂进 DUI 树之后，向宿主请求打开拖放分发。
    //
    // 为什么放在布局里而不是构造函数里：构造时控件还没挂进树，取不到宿主。
    // 布局是控件挂好之后必然会走到的第一个时机。用标志位保证只请求一次 ——
    // 布局会被反复调用，而请求本身涉及系统注册，不该每次都做。
    //
    // 请求失败（例如这个窗口已经被别的代码注册过拖放目标）不作处理：
    // 拖入功能用不了，其余一切照常。
    //
    // **拖放被关掉时不请求**：开启分发会让宿主在窗口上注册一个拖放目标，而
    // 一个窗口只能注册一个。调用方若打算自己在同一个窗口上注册（典型是聊天窗
    // 那种「拖文件进来就发送」的整窗接收），这里抢先注册会把它挡在外面 ——
    // 症状是拖放功能整个失效，且第二次注册失败没有任何提示。
    if (!m_bDropDispatchRequested && m_pHost != nullptr && IsDragDropEnabled())
    {
        m_bDropDispatchRequested = true;
        m_pHost->EnableDropDispatch(true);
    }

#if BUI_FEATURE_SCROLLBAR
    // 滚动条**覆盖**在文本区之上，不从文本区里切走宽度 —— 这正是覆盖式与
    // 库内列表那种内嵌式的区别：内容始终按全宽排版，滚动条只是浮在上面。
    //
    // 给子控件定位必须用 SetRect，**不能用 Layout**：在本库里这两者分工
    // 明确 —— SetRect 设置的是「自身矩形」（内部会再调 Layout 把区域分给
    // 它自己的子控件），而 Layout 只负责分配、根本不碰自身矩形。调错的
    // 症状是子控件矩形恒为空，于是什么也画不出来，且不报错。
    if (m_pScrollV != nullptr)
    {
        RECT rc = m_rcText;
        rc.left = rc.right - kOverlayScrollBarThickness;
        if (rc.left < m_rcText.left)
        {
            rc.left = m_rcText.left;
        }
        m_pScrollV->SetRect(rc);
    }
    if (m_pScrollH != nullptr)
    {
        RECT rc = m_rcText;
        rc.top = rc.bottom - kOverlayScrollBarThickness;
        if (rc.top < m_rcText.top)
        {
            rc.top = m_rcText.top;
        }
        // 右下角那一小块留给竖直滚动条，两条不叠在一起。
        // 无条件让出而不看竖直滚动条当前是否可见 —— 可见性随内容随时变化，
        // 跟着变会导致水平滚动条的长度来回抖动。
        rc.right -= kOverlayScrollBarThickness;
        if (rc.right < rc.left)
        {
            rc.right = rc.left;
        }
        m_pScrollH->SetRect(rc);
    }

    // 尺寸变了，排版结果随之变化，滚动范围也要重算。
    m_bScrollDirtyV = true;
    m_bScrollDirtyH = true;

    BUI_TRACE("RE-LAYOUT item=(%d,%d,%d,%d) text=(%d,%d,%d,%d) sbV=%s",
              (int)m_rcItem.left, (int)m_rcItem.top,
              (int)m_rcItem.right, (int)m_rcItem.bottom,
              (int)m_rcText.left, (int)m_rcText.top,
              (int)m_rcText.right, (int)m_rcText.bottom,
              m_pScrollV != nullptr ? "yes" : "null");
#endif

    // 自动增高的「自纠」一步：排完版之后回头看一眼，本控件拿到的高度是不是
    // 就是它想要的；不是就再请求一次重排。
    //
    // **为什么非要有这一步**：父容器的排版分两趟走 —— 第一趟问每个子控件
    // 「你想要多高」，第二趟才把矩形分下去。第一趟发生时，本控件的矩形还是
    // 上一轮的旧值；界面刚建出来那次更是全零。而富文本的高度是**按宽度算
    // 出来的**（同样的文字，框窄了就要多折几行、也就更高），宽度未知时根本
    // 量不出高度，只能报 0。于是首次显示会出现「自动档的编辑器高度为零、
    // 界面上完全看不见」。
    //
    // 第二趟分下来的矩形里宽度是对的（横向由父容器决定，与本控件无关），
    // 所以在这里再量一次就能得到正确的高度，请求重排后下一轮即可正确显示。
    // 代价是首次显示多排一次版，属于可接受的开销。
    //
    // 同一机制还顺带覆盖了「窗口变宽 / 变窄导致折行数变化」的情形 —— 那同样
    // 是「宽度变了、高度才跟着变」，与首次显示是同一回事。
    //
    // 这里不会因为反复请求而打转：RequestAutoGrowRelayout 内部记着上一次
    // 请求的宽高，父容器不采纳时不会重复请求，详见该函数的注释。
    RequestAutoGrowRelayout();
}

void DuiRichEdit::UpdateTextRect()
{
    RECT rc = m_rcItem;
    if (m_bShowBorder)
    {
        ::InflateRect(&rc, -1, -1);
    }
    rc.left   += m_nMarginLeft;
    rc.top    += m_nMarginTop;
    rc.right  -= m_nMarginRight;
    rc.bottom -= m_nMarginBottom;

    // 内边距大于控件尺寸时会算出反向矩形，夹回去，避免把负尺寸交给引擎。
    if (rc.right < rc.left)
    {
        rc.right = rc.left;
    }
    if (rc.bottom < rc.top)
    {
        rc.bottom = rc.top;
    }
    // 矩形没有变化时不再交给引擎。每设置一次客户区矩形，引擎都要重新计算排版
    // 范围，而本函数在每次布局、每次修改内边距时都会被调用，其中绝大多数调用
    // 算出的矩形与上一次完全相同。
    //
    // 更重要的是，本函数有可能是在引擎的通知回调中被间接触发的（例如子类在
    // "内容已改变"的回调里调整内边距），此时再去修改引擎状态属于重入调用。
    // 这项判断可以避免其中的大部分情况。
    const bool bSameRect = (rc.left == m_rcText.left && rc.top == m_rcText.top
                         && rc.right == m_rcText.right && rc.bottom == m_rcText.bottom);
    m_rcText = rc;
    if (bSameRect)
    {
        return;
    }

    // 内边距在这里就从矩形里扣掉了，因此引擎那边的内边距保持为零。
    // 两种做法（扣在矩形里、或交给引擎的内边距参数）效果等价，选前者是
    // 因为它不涉及单位换算 —— 引擎的内边距参数要求的是百分之毫米，
    // 而矩形是像素，少一次换算就少一处可能搞错的地方。
    if (m_pTextHost != nullptr)
    {
        m_pTextHost->SetClientRect(m_rcText);
    }
}

COLORREF DuiRichEdit::BorderColor() const
{
    if (!m_bEnabled)
    {
        return kBorderDisabled;
    }
    if (m_bFocused)
    {
        return kBorderFocused;
    }
    if (m_bHover)
    {
        return kBorderHover;
    }
    return kBorderIdle;
}

COLORREF DuiRichEdit::FillColor() const
{
    return m_bEnabled ? m_crBackground : kFillDisabled;
}

void DuiRichEdit::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }

    BUI_TRACE("PAINT dirty=(%d,%d,%d,%d) textRect=(%d,%d,%d,%d)",
                 (int)rcDirty.left, (int)rcDirty.top,
                 (int)rcDirty.right, (int)rcDirty.bottom,
                 (int)m_rcText.left, (int)m_rcText.top,
                 (int)m_rcText.right, (int)m_rcText.bottom);

    // 绘制入口是同步滚动条的安全时机之一：此刻不在引擎的回调栈里，
    // 可以放心地反过来查询引擎的滚动状态。
    SyncScrollBars();

    // ---- 第一步：自己铺背景 ----
    //
    // 这一步不能省。我们对引擎答的是「背景透明、你别管」，换来的是可以
    // 自己画圆角、渐变、半透明的底；代价就是每一帧都得自己把背景铺满，
    // 漏了会看到上一帧的残留。
    HBRUSH hbr = ::CreateSolidBrush(FillColor());
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    // ---- 第二步：边框 ----
    if (m_bShowBorder)
    {
        HPEN hpen = ::CreatePen(PS_SOLID, 1, BorderColor());
        HPEN oldPen = (HPEN)::SelectObject(hdc, hpen);
        HBRUSH oldBrush = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom);
        ::SelectObject(hdc, oldBrush);
        ::SelectObject(hdc, oldPen);
        ::DeleteObject(hpen);
    }

    // ---- 第三步：文字 ----
    ITextServices* pSvc = (m_pTextHost != nullptr)
                        ? m_pTextHost->GetTextServices()
                        : nullptr;
    if (pSvc != nullptr && !IsShowingPlaceholder())
    {
        // 裁剪必须自己做：balloonui 的宿主与基类的绘制路径都不设裁剪区，
        // 而引擎会按它自己的排版结果画，不裁就会画出控件边界。
        //
        // 用 SaveDC / RestoreDC 成对保护，而不是手工记录再恢复各项状态 ——
        // 引擎在 TxDraw 内部会改动画笔、画刷、字体、文字颜色等一大堆
        // 设备上下文状态，逐项恢复既啰嗦又容易漏。
        int nSaved = ::SaveDC(hdc);

        // 必须用求交而不是替换语义的那个接口：替换会把外层容器（如滚动
        // 视图）已经设好的裁剪一起撤掉，表现为控件画到了容器外面。
        ::IntersectClipRect(hdc, m_rcText.left, m_rcText.top,
                            m_rcText.right, m_rcText.bottom);

        // 把图形模式压成兼容模式。
        //
        // 参考实现必须这么做，因为它的渲染目标开着高级图形模式以支持世界
        // 变换，而排版引擎在高级模式下的定位与裁剪会出偏差。balloonui 的
        // 后台缓冲**据推测**一直是默认的兼容模式（库内没有用过世界变换），
        // 所以这一步很可能是多余的。仍然保留，是因为它幂等、代价极小，
        // 而一旦将来有人给绘制路径引入世界变换，缺了它的症状（文字位置
        // 莫名偏移）极难联想到根因。
        int nOldGraphicsMode = ::SetGraphicsMode(hdc, GM_COMPATIBLE);

        RECT rcUpdate;
        if (!::IntersectRect(&rcUpdate, &rcDirty, &m_rcText))
        {
            rcUpdate = m_rcText;
        }
        RECTL rcBounds;
        rcBounds.left   = m_rcText.left;
        rcBounds.top    = m_rcText.top;
        rcBounds.right  = m_rcText.right;
        rcBounds.bottom = m_rcText.bottom;

        // 引擎在 TxDraw 里会隐藏再显示一次光标、重设一次位置，这些请求只需
        // 记录、不能触发重绘，否则每次绘制都会引发下一次绘制。见 BeginDraw 的注释。
        m_pTextHost->BeginDraw();
        pSvc->TxDraw(
            DVASPECT_CONTENT,   // 绘制方式：正常内容
            0,                  // 保留参数
            nullptr,            // 绘制优化信息，不用
            nullptr,            // 目标设备描述，不用
            hdc,                // 绘制目标
            nullptr,            // 目标设备信息上下文，不用
            &rcBounds,          // 排版边界（宿主客户区坐标、像素）
            nullptr,            // 图元文件专用，不用
            &rcUpdate,          // 需要更新的区域
            nullptr,            // 中断回调，不用
            0,                  // 中断回调参数
            TXTVIEW_ACTIVE);    // 活动视图
        m_pTextHost->EndDraw();

        // 光标画在文字之上，并且仍在上面设好的文字区裁剪之内。
        PaintCaret(hdc, rcDirty);

        if (nOldGraphicsMode != 0)
        {
            ::SetGraphicsMode(hdc, nOldGraphicsMode);
        }
        ::RestoreDC(hdc, nSaved);
    }

    // ---- 第四步：占位文字 ----
    if (IsShowingPlaceholder())
    {
        HFONT hFont = (m_hDefaultFont != nullptr)
                    ? m_hDefaultFont
                    : GetDefaultFont();
        HFONT hOldFont = (hFont != nullptr) ? (HFONT)::SelectObject(hdc, hFont) : nullptr;
        int nOldBkMode = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF crOld = ::SetTextColor(hdc, kPlaceholderText);

        RECT rcPlaceholder = m_rcText;
        UINT uFormat = IsMultiLine()
                     ? (DT_LEFT | DT_TOP | DT_WORDBREAK)
                     : (DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        ::DrawText(hdc, m_strPlaceholder, -1, &rcPlaceholder, uFormat);

        ::SetTextColor(hdc, crOld);
        ::SetBkMode(hdc, nOldBkMode);
        if (hOldFont != nullptr)
        {
            ::SelectObject(hdc, hOldFont);
        }
    }

    // ---- 第五步：滚动条 ----
    //
    // **必须放在最后画**：覆盖式滚动条浮在内容之上，先画就会被文字盖住。
    // 这里显式画子控件而不调用基类的默认实现，正是为了掌握这个顺序。
    // 隐藏状态下滚动条内部会直接返回，不会画出任何东西。
#if BUI_FEATURE_SCROLLBAR
    if (m_pScrollV != nullptr && m_pScrollV->IsVisible())
    {
        m_pScrollV->OnPaint(hdc, rcDirty);
    }
    if (m_pScrollH != nullptr && m_pScrollH->IsVisible())
    {
        m_pScrollH->OnPaint(hdc, rcDirty);
    }
#endif
}

void DuiRichEdit::PaintCaret(HDC hdc, const RECT& rcDirty)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }

    // 此刻不该画（没有焦点、引擎要求隐藏、关闭了光标显示、或闪烁正处于灭的相位）。
    RECT rcCaret;
    if (!m_pTextHost->GetCaretPaintRect(rcCaret))
    {
        return;
    }

    // 裁剪到文字区与本次重画区域之内。只画重画区域内的那一部分不会让光标只亮
    // 一段：光标的每次亮灭切换与移动都会整块失效光标矩形，重画区域之外的像素
    // 与本次的亮灭状态必然一致。
    RECT rcDraw;
    if (!::IntersectRect(&rcDraw, &rcCaret, &m_rcText))
    {
        return;
    }
    if (!::IntersectRect(&rcDraw, &rcDraw, &rcDirty))
    {
        return;
    }

    const int drawWidth  = rcDraw.right - rcDraw.left;
    const int drawHeight = rcDraw.bottom - rcDraw.top;

    // 实心竖线光标：用白色画刷做异或，每个像素的 RGB 三个通道全部取反，效果与
    // 系统光标相同。不用 DSTINVERT，是因为它连 32 位后台缓冲里的保留字节也一起
    // 取反；画刷的保留字节为 0，异或之后保持原值，不会影响后续按透明度合成的绘制。
    HBITMAP hShape = m_pTextHost->GetCaretPaintBitmap();
    if (hShape == nullptr)
    {
        HBRUSH hOldBrush = (HBRUSH)::SelectObject(hdc, ::GetStockObject(WHITE_BRUSH));
        ::PatBlt(hdc, rcDraw.left, rcDraw.top, drawWidth, drawHeight, PATINVERT);
        ::SelectObject(hdc, hOldBrush);
        return;
    }

    // 位图光标：把位图异或到缓冲上，与系统画位图光标的方式一致。单色位图拷到
    // 彩色目标上时，0 位换成目标的文字色、1 位换成目标的背景色；这里把两者设为
    // 黑与白，于是 0 位异或黑色（不变）、1 位异或白色（取反）。引擎有选区时给的
    // 是全零的位图，画下去没有任何变化，光标也就看不见 —— 这正是引擎要的效果。
    HDC hdcShape = ::CreateCompatibleDC(hdc);
    if (hdcShape == nullptr)
    {
        return;
    }
    HBITMAP hOldShape = (HBITMAP)::SelectObject(hdcShape, hShape);
    const COLORREF crOldText = ::SetTextColor(hdc, kCaretShapeZeroBitColor);
    const COLORREF crOldBk   = ::SetBkColor(hdc, kCaretShapeOneBitColor);
    ::BitBlt(hdc, rcDraw.left, rcDraw.top, drawWidth, drawHeight,
             hdcShape, rcDraw.left - rcCaret.left, rcDraw.top - rcCaret.top, SRCINVERT);
    ::SetBkColor(hdc, crOldBk);
    ::SetTextColor(hdc, crOldText);
    ::SelectObject(hdcShape, hOldShape);
    ::DeleteDC(hdcShape);
}

// =================================================================
// 消息转发
// =================================================================

bool DuiRichEdit::ForwardToEngine(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                  LRESULT* plResult)
{
    ITextServices* pSvc = (m_pTextHost != nullptr)
                        ? m_pTextHost->GetTextServices()
                        : nullptr;
    if (pSvc == nullptr)
    {
        return false;
    }

    LRESULT lr = 0;
    HRESULT hr = pSvc->TxSendMessage(uMsg, wParam, lParam, &lr);
    if (plResult != nullptr)
    {
        *plResult = lr;
    }

    // 引擎用一个专门的返回值表示「这个按键我没处理」。据此让事件继续在
    // DUI 树里往上冒泡（例如 Esc 关对话框、方向键交给外层列表），
    // 而不是被本控件无声吞掉。
    if (hr == S_MSG_KEY_IGNORED)
    {
        return false;
    }
    return SUCCEEDED(hr);
}

bool DuiRichEdit::ForwardMouse(UINT uMsg, POINT pt, UINT mkFlags)
{
    // 坐标直接用，不做换算：交给引擎的文本区矩形用的就是宿主客户区坐标，
    // 而 balloonui 派发过来的鼠标坐标也是这一套，前后一致。
    return ForwardToEngine(uMsg, (WPARAM)mkFlags, MAKELPARAM(pt.x, pt.y), nullptr);
}

bool DuiRichEdit::OnMouseMove(POINT pt, UINT mkFlags)
{
    return ForwardMouse(WM_MOUSEMOVE, pt, mkFlags);
}

bool DuiRichEdit::OnLButtonDown(POINT pt, UINT mkFlags)
{
    // 宿主在派发点击时会按 Tab 轮转资格自动设焦点，这里不必重复处理；
    // 不可聚焦时那一步也不会发生，正是我们想要的。
    return ForwardMouse(WM_LBUTTONDOWN, pt, mkFlags);
}

bool DuiRichEdit::OnLButtonUp(POINT pt, UINT mkFlags)
{
    return ForwardMouse(WM_LBUTTONUP, pt, mkFlags);
}

bool DuiRichEdit::OnLButtonDblClk(POINT pt, UINT mkFlags)
{
    // 双击选词。注意宿主窗口类默认没有注册双击，业务需要先调宿主的
    // EnableDoubleClick 打开双击合成，本方法才会被调到。
    return ForwardMouse(WM_LBUTTONDBLCLK, pt, mkFlags);
}

bool DuiRichEdit::OnChar(TCHAR ch)
{
    BUI_TRACE("CHAR ch=%d", (int)ch);
    bool bHandled = ForwardToEngine(WM_CHAR, (WPARAM)ch, 0, nullptr);
    BUI_TRACE("CHAR done handled=%d", bHandled ? 1 : 0);
    return bHandled;
}

bool DuiRichEdit::OnKeyDown(UINT vk, UINT flags)
{
    BUI_TRACE("KEYDOWN vk=%u", vk);

    // 键盘唤出右键菜单。
    //
    // 两个键都要认：菜单键（键盘右侧 Ctrl 旁那个画着菜单图案的键）是 Windows
    // 的标准做法；Shift+F10 是它的等价组合，给没有菜单键的键盘（笔记本上很
    // 常见）留的路。不认这两个键的话，只用键盘的用户就完全够不着右键菜单。
    //
    // 拦在转给引擎之前：引擎自己不处理这两个键，交给它只会白走一趟。
    if (m_bContextMenuEnabled && IsContextMenuHotKey(vk))
    {
        POINT ptScreen;
        if (!GetContextMenuAnchorFromCaret(ptScreen))
        {
            return false;
        }
        return ShowContextMenu(ptScreen);
    }

    return ForwardToEngine(WM_KEYDOWN, (WPARAM)vk, (LPARAM)flags, nullptr);
}

bool DuiRichEdit::OnRawMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
                               LRESULT& lResult)
{
    // 宿主白名单里的按键抬起、系统键、输入法消息都从这里进来，原样转给引擎。
    // 中文输入能不能工作，全靠这条通道 —— 本控件没有自己的窗口，系统投递给
    // 宿主窗口的输入法消息只能这样才能到达引擎。
    BUI_TRACE("RAWMSG msg=0x%04X wp=%u", uMsg, (unsigned)wParam);
    bool bHandled = ForwardToEngine(uMsg, wParam, lParam, &lResult);

    // 组字开始与组字过程中，把输入法的候选条重新定位到光标处。
    //
    // 顺序很重要：**必须放在转发之后**。引擎处理完这条消息才会把光标移到
    // 新的插入点上，在那之前取到的还是上一个位置，候选条会慢一拍。
    if (uMsg == WM_IME_STARTCOMPOSITION || uMsg == WM_IME_COMPOSITION)
    {
        if (m_pTextHost != nullptr)
        {
            m_pTextHost->UpdateImeCompositionPos();
        }
    }
    return bHandled;
}

// =================================================================
// 焦点与鼠标指针
// =================================================================

bool DuiRichEdit::OnSetFocus()
{
    DuiControl::OnSetFocus();

    // 界面激活状态直接决定光标显不显示、选区高不高亮，必须如实同步。
    if (m_pTextHost != nullptr && m_bFocusable)
    {
        m_pTextHost->SetUiActive(true);
    }
    Invalidate();
    return false;
}

bool DuiRichEdit::OnKillFocus()
{
    DuiControl::OnKillFocus();

    if (m_pTextHost != nullptr)
    {
        // 内部会连带销毁系统光标，把线程唯一的那份资源让出来。
        m_pTextHost->SetUiActive(false);
    }
    Invalidate();
    return false;
}

bool DuiRichEdit::OnSetCursor(POINT pt)
{
    ITextServices* pSvc = (m_pTextHost != nullptr)
                        ? m_pTextHost->GetTextServices()
                        : nullptr;
    if (pSvc == nullptr)
    {
        return false;
    }
    if (!::PtInRect(&m_rcText, pt))
    {
        return false;
    }

    // 由引擎判断该显示什么形状 —— 文字上是竖线形、链接上是手形。
    // 它会通过 TxSetCursor 回调把结果告诉我们。
    HDC hdc = ::GetDC(nullptr);
    pSvc->OnTxSetCursor(DVASPECT_CONTENT, 0, nullptr, nullptr, hdc, nullptr,
                        &m_rcText, pt.x, pt.y);
    ::ReleaseDC(nullptr, hdc);
    return true;
}

// =================================================================
// 选区
// =================================================================

void DuiRichEdit::SetSel(long cpMin, long cpMax)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }
    CHARRANGE cr;
    cr.cpMin = cpMin;
    cr.cpMax = cpMax;
    // 用扩展版的设置选区命令而不是老版本：老版本用两个 16 位参数传位置，
    // 文档超过 32767 个字符就会溢出。
    pSvc->TxSendMessage(EM_EXSETSEL, 0, (LPARAM)&cr, nullptr);
    Invalidate();
}

void DuiRichEdit::GetSel(long& cpMin, long& cpMax) const
{
    cpMin = 0;
    cpMax = 0;

    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }
    CHARRANGE cr;
    cr.cpMin = 0;
    cr.cpMax = 0;
    pSvc->TxSendMessage(EM_EXGETSEL, 0, (LPARAM)&cr, nullptr);
    cpMin = cr.cpMin;
    cpMax = cr.cpMax;
}

void DuiRichEdit::SelectAll()
{
    // 刻意**不用**「终点传 -1」那种写法。-1 表示选到引擎眼里的文档末尾，
    // 而引擎在末尾维护着一个结尾标记，会被一并选中 —— 于是选区长度比
    // 用户看到的文字多一个字符，「全选再复制」出来会多一个换行。
    //
    // 这里按本控件对外统一的长度口径来选，与 GetText / GetTextLength 一致。
    SetSel(0, GetTextLength());
}

void DuiRichEdit::ReplaceSel(LPCTSTR text, bool bCanUndo)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }
    CStringW wide(text != nullptr ? text : _T(""));
    pSvc->TxSendMessage(EM_REPLACESEL, bCanUndo ? TRUE : FALSE,
                        (LPARAM)(LPCWSTR)wide, nullptr);
    Invalidate();
}

void DuiRichEdit::AppendText(LPCTSTR text)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr || text == nullptr || *text == _T('\0'))
    {
        return;
    }

    // 先记下用户当前的选区，追加完再还原 —— 追加是程序行为，不该把用户
    // 正在编辑的位置搬走。
    long cpMinSaved = 0;
    long cpMaxSaved = 0;
    GetSel(cpMinSaved, cpMaxSaved);

    SetSel(-1, -1);            // 光标移到文档末尾
    ReplaceSel(text, /*bCanUndo=*/true);

    SetSel(cpMinSaved, cpMaxSaved);
}

int DuiRichEdit::LineCount() const
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return 0;
    }
    LRESULT lResult = 0;
    pSvc->TxSendMessage(EM_GETLINECOUNT, 0, 0, &lResult);
    return (int)lResult;
}

// =================================================================
// 编辑命令
// =================================================================

void DuiRichEdit::Undo()
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc != nullptr)
    {
        pSvc->TxSendMessage(EM_UNDO, 0, 0, nullptr);
        Invalidate();
    }
}

void DuiRichEdit::Redo()
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc != nullptr)
    {
        pSvc->TxSendMessage(EM_REDO, 0, 0, nullptr);
        Invalidate();
    }
}

bool DuiRichEdit::CanUndo() const
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return false;
    }
    LRESULT lResult = 0;
    pSvc->TxSendMessage(EM_CANUNDO, 0, 0, &lResult);
    return lResult != 0;
}

bool DuiRichEdit::CanRedo() const
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return false;
    }
    LRESULT lResult = 0;
    pSvc->TxSendMessage(EM_CANREDO, 0, 0, &lResult);
    return lResult != 0;
}

void DuiRichEdit::Cut()
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc != nullptr)
    {
        pSvc->TxSendMessage(WM_CUT, 0, 0, nullptr);
        Invalidate();
    }
}

void DuiRichEdit::Copy()
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc != nullptr)
    {
        pSvc->TxSendMessage(WM_COPY, 0, 0, nullptr);
    }
}

void DuiRichEdit::Paste()
{
    // 按调用方设定的策略决定粘不粘格式。
    if (m_bPastePlainText)
    {
        PasteAsPlainText();
        return;
    }
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc != nullptr)
    {
        pSvc->TxSendMessage(WM_PASTE, 0, 0, nullptr);
        Invalidate();
    }
}

void DuiRichEdit::Clear()
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc != nullptr)
    {
        pSvc->TxSendMessage(WM_CLEAR, 0, 0, nullptr);
        Invalidate();
    }
}

bool DuiRichEdit::PasteAsPlainText()
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return false;
    }
    // 剪贴板里没有文本就直接返回，不要盲目下发 —— 否则会把剪贴板里的
    // 图片之类按「特殊粘贴」的规则插进来。
    if (!::IsClipboardFormatAvailable(CF_UNICODETEXT)
        && !::IsClipboardFormatAvailable(CF_TEXT))
    {
        return false;
    }

    // 指定只接受纯文本格式，源内容的字体、颜色、链接、内嵌对象全部丢弃。
    pSvc->TxSendMessage(EM_PASTESPECIAL, CF_UNICODETEXT, 0, nullptr);
    Invalidate();
    return true;
}

// =================================================================
// 字符格式
// =================================================================

void DuiRichEdit::ApplySelCharEffect(DWORD dwMask, DWORD dwEffect, bool bOn)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }

    CHARFORMAT2W cf;
    ::memset(&cf, 0, sizeof(cf));
    cf.cbSize    = sizeof(cf);
    cf.dwMask    = dwMask;
    cf.dwEffects = bOn ? dwEffect : 0;

    // 作用范围是当前选区。没有选区时引擎会把它记成「接下来输入的字符
    // 采用这个格式」，与常见富文本编辑器的行为一致。
    pSvc->TxSendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf, nullptr);
    Invalidate();
}

bool DuiRichEdit::QuerySelCharEffect(DWORD dwMask, DWORD dwEffect, bool& bOn) const
{
    bOn = false;

    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return false;
    }

    CHARFORMAT2W cf;
    ::memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    pSvc->TxSendMessage(EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf, nullptr);

    // 引擎在返回的掩码里只置起「整个选区一致」的那些位。某一位没置起，
    // 说明选区里既有粗体又有非粗体 —— 这时候没有单一答案可给，返回 false，
    // 对应富文本工具栏里粗体按钮呈现「不确定态」的语义。
    if ((cf.dwMask & dwMask) == 0)
    {
        return false;
    }
    bOn = (cf.dwEffects & dwEffect) != 0;
    return true;
}

void DuiRichEdit::SetSelBold(bool b)
{
    ApplySelCharEffect(CFM_BOLD, CFE_BOLD, b);
}

void DuiRichEdit::SetSelItalic(bool b)
{
    ApplySelCharEffect(CFM_ITALIC, CFE_ITALIC, b);
}

void DuiRichEdit::SetSelUnderline(bool b)
{
    ApplySelCharEffect(CFM_UNDERLINE, CFE_UNDERLINE, b);
}

void DuiRichEdit::SetSelTextColor(COLORREF cr)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }
    CHARFORMAT2W cf;
    ::memset(&cf, 0, sizeof(cf));
    cf.cbSize      = sizeof(cf);
    cf.dwMask      = CFM_COLOR;
    cf.crTextColor = cr;
    // 必须清掉「使用自动颜色」这一位，否则引擎会忽略我们给的具体颜色。
    cf.dwEffects   = 0;
    pSvc->TxSendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf, nullptr);
    Invalidate();
}

bool DuiRichEdit::GetSelBold(bool& bBold) const
{
    return QuerySelCharEffect(CFM_BOLD, CFE_BOLD, bBold);
}

bool DuiRichEdit::GetSelItalic(bool& bItalic) const
{
    return QuerySelCharEffect(CFM_ITALIC, CFE_ITALIC, bItalic);
}

bool DuiRichEdit::GetSelUnderline(bool& bUnderline) const
{
    return QuerySelCharEffect(CFM_UNDERLINE, CFE_UNDERLINE, bUnderline);
}

// =================================================================
// 段落格式
// =================================================================

void DuiRichEdit::SetParaAlignment(ParaAlignment align)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }

    WORD wAlign = PFA_LEFT;
    switch (align)
    {
    //居中。
    case kParaCenter:
        wAlign = PFA_CENTER;
        break;
    //右对齐。
    case kParaRight:
        wAlign = PFA_RIGHT;
        break;
    //两端对齐：靠拉伸词间距把每行都撑满，最后一行除外。
    case kParaJustify:
        wAlign = PFA_JUSTIFY;
        break;
    //左对齐（默认）。
    case kParaLeft:
    default:
        wAlign = PFA_LEFT;
        break;
    }

    PARAFORMAT2 pf;
    ::memset(&pf, 0, sizeof(pf));
    pf.cbSize     = sizeof(pf);
    pf.dwMask     = PFM_ALIGNMENT;
    pf.wAlignment = wAlign;
    pSvc->TxSendMessage(EM_SETPARAFORMAT, 0, (LPARAM)&pf, nullptr);
    Invalidate();
}

DuiRichEdit::ParaAlignment DuiRichEdit::GetParaAlignment() const
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return kParaLeft;
    }

    PARAFORMAT2 pf;
    ::memset(&pf, 0, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pSvc->TxSendMessage(EM_GETPARAFORMAT, 0, (LPARAM)&pf, nullptr);

    switch (pf.wAlignment)
    {
    //居中。
    case PFA_CENTER:
        return kParaCenter;
    //右对齐。
    case PFA_RIGHT:
        return kParaRight;
    //两端对齐。
    case PFA_JUSTIFY:
        return kParaJustify;
    //其余（含左对齐与取不到值的情况）一律按左对齐。
    default:
        return kParaLeft;
    }
}

void DuiRichEdit::SetParaLeftIndent(int nPixels)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }
    if (nPixels < 0)
    {
        nPixels = 0;
    }

    // 段落格式里的长度单位是 twip（一英寸的一千四百四十分之一），
    // 不是像素。这里做唯一一处换算，对外接口一律用像素。
    int dpiX = 96;
    int dpiY = 96;
    GetScreenDpi(dpiX, dpiY);
    const int kTwipsPerInch = 1440;

    PARAFORMAT2 pf;
    ::memset(&pf, 0, sizeof(pf));
    pf.cbSize     = sizeof(pf);
    // 同时设「首行缩进」与「后续行相对首行的偏移」，后者给 0 —— 这样整段
    // 每一行都缩进同样的量。只设首行的话，第二行起会顶回左边缘，那是
    // 「悬挂缩进」的效果，不是这个接口想要的。
    pf.dwMask     = PFM_STARTINDENT | PFM_OFFSET;
    pf.dxStartIndent = ::MulDiv(nPixels, kTwipsPerInch, dpiX);
    pf.dxOffset      = 0;
    pSvc->TxSendMessage(EM_SETPARAFORMAT, 0, (LPARAM)&pf, nullptr);
    Invalidate();
}

int DuiRichEdit::GetParaLeftIndent() const
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return 0;
    }

    PARAFORMAT2 pf;
    ::memset(&pf, 0, sizeof(pf));
    pf.cbSize = sizeof(pf);
    pSvc->TxSendMessage(EM_GETPARAFORMAT, 0, (LPARAM)&pf, nullptr);

    int dpiX = 96;
    int dpiY = 96;
    GetScreenDpi(dpiX, dpiY);
    const int kTwipsPerInch = 1440;
    return ::MulDiv((int)pf.dxStartIndent, dpiX, kTwipsPerInch);
}

// =================================================================
// 自动增高
// =================================================================

void DuiRichEdit::SetAutoGrow(bool b)
{
    if (m_bAutoGrow == b)
    {
        return;
    }
    m_bAutoGrow = b;
    // 开关一变，本控件报出的期望尺寸就变了，得让宿主重新排一次版，
    // 否则要等到下一次窗口尺寸变化才生效。
    // 注意不能只调 Invalidate()：那只是重绘，不会重新计算布局。
    if (m_pHost != nullptr)
    {
        m_pHost->RequestRelayout();
    }
    Invalidate();
}

void DuiRichEdit::SetAutoGrowRange(int nMinPixels, int nMaxPixels)
{
    m_nAutoGrowMin = nMinPixels;
    m_nAutoGrowMax = nMaxPixels;
    // 理由同 SetAutoGrow：上下限变了，期望高度也就变了，需要重新排版。
    if (m_pHost != nullptr)
    {
        m_pHost->RequestRelayout();
    }
    Invalidate();
}

int DuiRichEdit::GetLineHeight() const
{
    // 兜底值：取不到字体度量时用它，避免除零或算出零高度。
    const int kFallbackLineHeight = 16;

    HFONT hFont = (m_hDefaultFont != nullptr)
                ? m_hDefaultFont
                : GetDefaultFont();
    if (hFont == nullptr)
    {
        return kFallbackLineHeight;
    }

    HDC hdc = ::GetDC(nullptr);
    if (hdc == nullptr)
    {
        return kFallbackLineHeight;
    }
    HFONT hOld = (HFONT)::SelectObject(hdc, hFont);
    TEXTMETRIC tm;
    ::memset(&tm, 0, sizeof(tm));
    BOOL bOk = ::GetTextMetrics(hdc, &tm);
    ::SelectObject(hdc, hOld);
    ::ReleaseDC(nullptr, hdc);

    if (!bOk || tm.tmHeight <= 0)
    {
        return kFallbackLineHeight;
    }
    // 行高要把行间距算进去，否则连着两行会贴在一起。
    return (int)(tm.tmHeight + tm.tmExternalLeading);
}

void DuiRichEdit::SetAutoGrowLines(int nMinLines, int nMaxLines)
{
    const int nLineH = GetLineHeight();
    const int nMin = (nMinLines > 0) ? (nMinLines * nLineH) : nLineH;
    const int nMax = (nMaxLines > 0) ? (nMaxLines * nLineH) : 0;
    SetAutoGrowRange(nMin, nMax);
}

SIZE DuiRichEdit::GetDesiredSize() const
{
    SIZE sz;
    sz.cx = 0;
    sz.cy = 0;

    if (!m_bAutoGrow)
    {
        // 关掉时返回全零，等于告诉布局体系「高度我不挑，你定」。
        return sz;
    }

    int nContentH = 0;
    if (!MeasureContentHeight(nContentH))
    {
        return sz;
    }

    // 内容高度是**文本区**的高度，而父容器要的是**整个控件**的高度，
    // 中间差着边框与上下内边距，要加回来。漏了这一步的症状是内容底部
    // 被切掉一两个像素，而且内边距设得越大切得越多。
    int nChrome = m_nMarginTop + m_nMarginBottom;
    if (m_bShowBorder)
    {
        nChrome += 2;      // 上下各一像素
    }

    int nWanted = nContentH + nChrome;

    // 夹到上下限之间。下限没设时按一行的高度算 —— 不然空内容会缩成
    // 零高度，界面上看不见这个控件。
    const int nMin = (m_nAutoGrowMin > 0) ? m_nAutoGrowMin
                                          : (GetLineHeight() + nChrome);
    if (nWanted < nMin)
    {
        nWanted = nMin;
    }
    if (m_nAutoGrowMax > 0 && nWanted > m_nAutoGrowMax)
    {
        // 到顶之后不再长高，超出部分交给滚动条。
        nWanted = m_nAutoGrowMax;
    }

    sz.cy = nWanted;
    return sz;
}

void DuiRichEdit::RequestAutoGrowRelayout()
{
    if (!m_bAutoGrow)
    {
        return;
    }
    if (m_pHost == nullptr)
    {
        // 控件还没挂进 DUI 树，没有宿主可请求。挂上去之后会走一次正常排版。
        return;
    }

    SIZE szWanted = GetDesiredSize();
    if (szWanted.cy <= 0)
    {
        // 量不出来（引擎未就绪、宽度还是零等），保持现状，不做无谓的重排。
        return;
    }

    const RECT& rc = GetRect();
    const int nCurrentH = rc.bottom - rc.top;
    const int nCurrentW = rc.right - rc.left;

    if ((int)szWanted.cy == nCurrentH)
    {
        // 当前高度正是期望高度，什么也不用做。同时清掉上一次请求的记录 ——
        // 这说明父容器采纳了本控件的期望高度，下次再有变化应当照常请求。
        m_nLastGrowReqW = 0;
        m_nLastGrowReqH = 0;
        return;
    }

    // 同样的宽度下已经为同一个期望高度请求过一次了，说明父容器不打算采纳
    // （典型是布局提示写的是固定高度而非自动档）。再请求也是同样的结果，
    // 就此打住，否则会在「请求重排 → 重排 → 高度仍不等 → 再请求」之间
    // 无限打转。判定依据的详细说明见 m_nLastGrowReqW / m_nLastGrowReqH 的注释。
    if (nCurrentW == m_nLastGrowReqW && (int)szWanted.cy == m_nLastGrowReqH)
    {
        return;
    }

    m_nLastGrowReqW = nCurrentW;
    m_nLastGrowReqH = (int)szWanted.cy;
    m_pHost->RequestRelayout();
}

#if BUI_FEATURE_IMAGEOLE

// =================================================================
// 内联图片
// =================================================================

IRichEditOle* DuiRichEdit::AcquireRichEditOle() const
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return nullptr;
    }

    // 无窗口模式下取 OLE 接口的唯一途径：把消息交给引擎自己下发。
    // 旧控件是把同一条消息发给它那个真子窗口的 —— 换的只是投递方式，
    // 拿到的是同一个接口。（2026-08-16 实测确认，见 CanObtainOleInterface 用例。）
    IRichEditOle* pOle = nullptr;
    LRESULT lResult = 0;
    if (FAILED(pSvc->TxSendMessage(EM_GETOLEINTERFACE, 0, (LPARAM)&pOle, &lResult)))
    {
        return nullptr;
    }
    return pOle;   // 引用计数已由引擎加好，调用方负责释放
}

bool DuiRichEdit::InsertTaggedImage(LPCTSTR szPath, DWORD_PTR tag, int nSizePx)
{
    if (szPath == nullptr || *szPath == _T('\0'))
    {
        return false;
    }

    IRichEditOle* pOle = AcquireRichEditOle();
    if (pOle == nullptr)
    {
        return false;
    }

    // 按原始分辨率解码、保留 alpha。刻意**不**预先缩小，理由见头文件里
    // nSizePx 的注释。
    SIZE szSrc;
    szSrc.cx = 0;
    szSrc.cy = 0;
    HBITMAP hbm = CDuiImageOle::LoadPremultipliedDibFromFile(szPath, &szSrc);
    if (hbm == nullptr)
    {
        pOle->Release();
        return false;
    }

    // nSizePx 只用来算排版尺寸（等比、只缩不放）。
    const SIZE szDisplay = FitImageSize(szSrc.cx, szSrc.cy, nSizePx, nSizePx);

    // 位图所有权交给图片对象，插入失败时由它负责删除。
    const bool bOk = CDuiImageOle::InsertIntoRichEditOle(
        pOle, hbm, /*ownsHbm=*/true, tag,
        /*hasPremultipliedAlpha=*/true, szDisplay.cx, szDisplay.cy);

    pOle->Release();

    if (bOk)
    {
        // 内容变了，重新量一次高度（自动增高时控件可能要长高一行）并重绘。
        RequestAutoGrowRelayout();
        Invalidate();
    }
    return bOk;
}

bool DuiRichEdit::InsertImageFromFile(LPCTSTR szPath, int nMaxW, int nMaxH)
{
    if (szPath == nullptr || *szPath == _T('\0'))
    {
        return false;
    }

    IRichEditOle* pOle = AcquireRichEditOle();
    if (pOle == nullptr)
    {
        return false;
    }

    SIZE szSrc;
    szSrc.cx = 0;
    szSrc.cy = 0;
    HBITMAP hbm = CDuiImageOle::LoadPremultipliedDibFromFile(szPath, &szSrc);
    if (hbm == nullptr)
    {
        pOle->Release();
        return false;
    }

    const SIZE szDisplay = FitImageSize(szSrc.cx, szSrc.cy, nMaxW, nMaxH);

    // 标记传 0 —— 本方法插入的图不带业务标记。
    const bool bOk = CDuiImageOle::InsertIntoRichEditOle(
        pOle, hbm, /*ownsHbm=*/true, /*dwUser=*/0,
        /*hasPremultipliedAlpha=*/true, szDisplay.cx, szDisplay.cy);

    pOle->Release();

    if (bOk)
    {
        RequestAutoGrowRelayout();
        Invalidate();
    }
    return bOk;
}

int DuiRichEdit::GetEmbeddedImageCount() const
{
    IRichEditOle* pOle = AcquireRichEditOle();
    if (pOle == nullptr)
    {
        return 0;
    }
    const LONG nCount = pOle->GetObjectCount();
    pOle->Release();
    return (int)nCount;
}

CString DuiRichEdit::GetTextRangeChars(long cpMin, long cpMax) const
{
    if (cpMax <= cpMin)
    {
        return CString();
    }
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return CString();
    }

    CString strOut;
    const int nCount = (int)(cpMax - cpMin);
    LPTSTR pBuf = strOut.GetBufferSetLength(nCount + 1);

    TEXTRANGE tr;
    tr.chrg.cpMin = cpMin;
    tr.chrg.cpMax = cpMax;
    tr.lpstrText  = pBuf;

    LRESULT lGot = 0;
    pSvc->TxSendMessage(EM_GETTEXTRANGE, 0, (LPARAM)&tr, &lGot);
    strOut.ReleaseBuffer((int)lGot);
    return strOut;
}

void DuiRichEdit::EnumContent(ContentSegmentFn fn, void* pCtx) const
{
    if (fn == nullptr)
    {
        return;
    }
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return;
    }

    // 文档总字符数。**必须与图片对象记录的字符位置用同一套口径** ——
    // 每个内联图片在这套口径里算一个字符。
    GETTEXTLENGTHEX gtl;
    gtl.flags    = GTL_DEFAULT | GTL_NUMCHARS;
    gtl.codepage = 1200;   // UTF-16
    LRESULT lTotal = 0;
    pSvc->TxSendMessage(EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0, &lTotal);
    long nTotal = (long)lTotal;
    if (nTotal < 0)
    {
        nTotal = 0;
    }

    // 收集所有图片对象的（字符位置，标记），**按位置升序排一遍** ——
    // 枚举出来的次序未必就是文档顺序，不排的话拼出来的内容会乱序。
    std::vector<EmbeddedImagePos> objs;
    IRichEditOle* pOle = AcquireRichEditOle();
    if (pOle != nullptr)
    {
        const int nCount = (int)pOle->GetObjectCount();
        for (int i = 0; i < nCount; ++i)
        {
            REOBJECT reo;
            ::ZeroMemory(&reo, sizeof(reo));
            reo.cbStruct = sizeof(REOBJECT);
            if (SUCCEEDED(pOle->GetObject(i, &reo, REO_GETOBJ_NO_INTERFACES)))
            {
                EmbeddedImagePos item;
                item.m_cp  = (long)reo.cp;
                item.m_tag = reo.dwUser;
                objs.push_back(item);
            }
        }
        pOle->Release();
    }
    std::sort(objs.begin(), objs.end());

    // 按图片位置把文档切成「文本段、图片段、文本段……」依次回调。
    long nPos = 0;
    for (size_t i = 0; i < objs.size(); ++i)
    {
        const long nObjCp = objs[i].m_cp;
        if (nObjCp > nPos)
        {
            CString strText = GetTextRangeChars(nPos, nObjCp);
            if (!strText.IsEmpty())
            {
                fn(false, strText, 0, pCtx);
            }
        }
        fn(true, _T(""), objs[i].m_tag, pCtx);
        nPos = nObjCp + 1;   // 图片对象占一个字符
    }
    if (nPos < nTotal)
    {
        CString strText = GetTextRangeChars(nPos, nTotal);
        if (!strText.IsEmpty())
        {
            fn(false, strText, 0, pCtx);
        }
    }
}

#endif // BUI_FEATURE_IMAGEOLE

// =================================================================
// 右键菜单
// =================================================================

void DuiRichEdit::SetContextMenuEnabled(bool b)
{
    m_bContextMenuEnabled = b;
}

bool DuiRichEdit::AppendContextMenuItem(UINT nId, LPCTSTR szText)
{
    // 编号越界直接挡掉。放行的话会与内置命令撞号，症状是「点了自定义项却
    // 执行了粘贴」—— 不报错、不崩溃，只能靠人发现，所以这里必须挡住并出声。
    if (nId < kRichEditMenuCustomBase)
    {
        ::OutputDebugStringA("DuiRichEdit: custom context menu id is below "
                             "kRichEditMenuCustomBase, item rejected.\n");
        return false;
    }

    m_customMenuItems.push_back(
        MakeDuiRichEditMenuItem(nId, (szText != nullptr) ? szText : _T(""), true));
    return true;
}

void DuiRichEdit::AppendContextMenuSeparator()
{
    m_customMenuItems.push_back(MakeDuiRichEditMenuSeparator());
}

void DuiRichEdit::ClearContextMenuItems()
{
    m_customMenuItems.clear();
}

DuiRichEditMenuState DuiRichEdit::CaptureContextMenuState() const
{
    DuiRichEditMenuState st;
    st.m_readOnly = IsReadOnly();
    st.m_hasText  = (GetTextLength() > 0);

    // 选区：起止字符位置不同即为非空选区。
    long cpMin = 0;
    long cpMax = 0;
    GetSel(cpMin, cpMax);
    st.m_hasSelection = (cpMin != cpMax);

    // 剪贴板：两种文本格式任一可用即可粘贴。检查两种而不是只查宽字符那种，
    // 是因为有些老程序只往剪贴板里放窄字符格式。
    st.m_clipboardHasText =
        (::IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE) ||
        (::IsClipboardFormatAvailable(CF_TEXT) != FALSE);

    st.m_canUndo = CanUndo();
    st.m_canRedo = CanRedo();
    return st;
}

void DuiRichEdit::OnBuildContextMenu(std::vector<DuiRichEditMenuItem>& items)
{
    // 默认项。
    DuiRichEditMenuState st = CaptureContextMenuState();
    BuildDuiRichEditContextMenu(st, items);

    // 调用方登记的自定义项接在末尾，中间隔一条分隔条。
    //
    // 分隔条无条件加：自定义项为空时它会成为结尾的分隔条，随后被规整那一步
    // 去掉，不会在界面上留下一条悬空的横线。这样写省掉一个判空分支。
    items.push_back(MakeDuiRichEditMenuSeparator());
    for (size_t i = 0; i < m_customMenuItems.size(); ++i)
    {
        items.push_back(m_customMenuItems[i]);
    }
}

void DuiRichEdit::BuildContextMenuModel(std::vector<DuiRichEditMenuItem>& items)
{
    items.clear();
    OnBuildContextMenu(items);
    // 子类完全可能在覆写里删项，删完就会留下开头悬空的、或者连续重复的
    // 分隔条。统一在这里规整一次，子类不必自己操心。
    NormalizeDuiRichEditContextMenu(items);
}

bool DuiRichEdit::OnContextMenuCommand(UINT nId)
{
    switch (nId)
    {
    //撤销上一次编辑
    case kRichEditCmdUndo:
        Undo();
        return true;

    //重做刚被撤销的编辑
    case kRichEditCmdRedo:
        Redo();
        return true;

    //剪切：选区进剪贴板并从文档中删除
    case kRichEditCmdCut:
        Cut();
        return true;

    //复制：选区进剪贴板，文档不变
    case kRichEditCmdCopy:
        Copy();
        return true;

    //粘贴：走本控件的公开方法，因此会遵循调用方设定的「粘贴是否保留格式」
    //策略，与用户按 Ctrl+V 的结果保持一致
    case kRichEditCmdPaste:
        Paste();
        return true;

    //粘贴为纯文本：无视上述策略，强制只取文字
    case kRichEditCmdPastePlain:
        PasteAsPlainText();
        return true;

    //删除：删掉选区但不写剪贴板，这是它与剪切的唯一区别
    case kRichEditCmdDelete:
        Clear();
        return true;

    //全选
    case kRichEditCmdSelectAll:
        SelectAll();
        return true;

    //自定义编号：基类不认识，交回给调用方处理
    default:
        return false;
    }
}

#if defined(BUI_FEATURE_MENU)

bool DuiRichEdit::ShowContextMenu(POINT ptScreen)
{
    if (!m_bContextMenuEnabled)
    {
        return false;
    }

    std::vector<DuiRichEditMenuItem> items;
    BuildContextMenuModel(items);
    if (items.empty())
    {
        // 子类可能把菜单清空了。空菜单不弹 —— 弹出来是一个什么都没有的
        // 小方块，比不弹更让人困惑。
        return false;
    }

    DuiMenu menu;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const DuiRichEditMenuItem& item = items[i];
        if (item.m_separator)
        {
            menu.AppendSeparator();
        }
        else if (item.m_enabled)
        {
            menu.AppendItem(item.m_id, (LPCTSTR)item.m_text);
        }
        else
        {
            menu.AppendDisabled(item.m_id, (LPCTSTR)item.m_text);
        }
    }

    // 菜单关闭后焦点回到宿主窗口 —— 本控件没有自己的窗口，宿主窗口就是它
    // 的落脚点。取不到宿主窗口时不弹：没有所有者窗口的弹出菜单在失焦时
    // 无处交还焦点。
    HWND hwndOwner = TxSiteGetHostHwnd();
    if (hwndOwner == nullptr)
    {
        return false;
    }

    // 落点越界不需要在这里处理：DuiMenu 弹出前会自己把菜单限制在锚点所在
    // 显示器的工作区内（右边放不下翻向左、下边放不下翻向上）。
    UINT nChosen = menu.TrackPopup(ptScreen.x, ptScreen.y, hwndOwner);
    if (nChosen == 0)
    {
        // 用户没选任何项（按 Esc 或点到别处）。菜单确实弹过，返回 true。
        return true;
    }

    if (!OnContextMenuCommand(nChosen))
    {
        // 不是内置命令，也没有被子类处理，作为通知交给父窗口。
        NotifyParent(DUIN_RICHTEXT_MENUCOMMAND, (LPARAM)nChosen);
    }
    return true;
}

#else  // !BUI_FEATURE_MENU

bool DuiRichEdit::ShowContextMenu(POINT /*ptScreen*/)
{
    // 菜单特性被裁掉了。返回 false 让调用方按「没有右键菜单」处理。
    return false;
}

#endif // BUI_FEATURE_MENU

bool DuiRichEdit::GetContextMenuAnchorFromCaret(POINT& outScreen) const
{
    outScreen.x = 0;
    outScreen.y = 0;

    HWND hwndHost = TxSiteGetHostHwnd();
    if (hwndHost == nullptr)
    {
        return false;
    }

    // 默认落点：文本区左上角。取不到光标时用它兜底，至少菜单会出现在
    // 控件自己身上，而不是屏幕左上角。
    POINT ptClient;
    ptClient.x = m_rcText.left;
    ptClient.y = m_rcText.top;

    if (m_pTextHost != nullptr && m_pTextHost->GetCaret().IsCreated())
    {
        POINT ptCaret = m_pTextHost->GetCaret().GetPos();
        SIZE  szCaret = m_pTextHost->GetCaret().GetSize();
        ptClient.x = ptCaret.x;
        // 落在光标下沿而不是上沿：菜单从这里向下展开，正好不遮住用户
        // 当前编辑的那一行。
        ptClient.y = ptCaret.y + szCaret.cy;
    }

    outScreen = ptClient;
    ::ClientToScreen(hwndHost, &outScreen);
    return true;
}

bool DuiRichEdit::OnRButtonDown(POINT pt, UINT mkFlags)
{
    // **先把消息转给引擎，再弹菜单**，顺序不能反。
    //
    // 引擎收到右键按下后会按标准行为调整光标与选区：点在选区外时把光标移
    // 过去并清掉原选区，点在选区内则保持选区不变。菜单里「剪切」「复制」
    // 是否可用取决于有没有选区，所以必须等引擎调整完再去读状态。顺序反了
    // 的症状是：右键点在别处，菜单却还对着上一次的选区。
    ForwardMouse(WM_RBUTTONDOWN, pt, mkFlags);

    if (!m_bContextMenuEnabled)
    {
        // 关掉右键菜单时，上面转给引擎那一步照常做，只是不弹菜单。
        // 返回 false 让事件继续在 DUI 树里往上冒泡，业务可以自己接管右键。
        return false;
    }

    // 控件坐标换算成屏幕坐标。取不到宿主窗口说明控件还没挂进树，不弹。
    HWND hwndHost = TxSiteGetHostHwnd();
    if (hwndHost == nullptr)
    {
        return false;
    }
    POINT ptScreen = pt;
    ::ClientToScreen(hwndHost, &ptScreen);

    return ShowContextMenu(ptScreen);
}

// =================================================================
// 拖放
// =================================================================

LRESULT DuiRichEdit::SendMessageToEngine(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return 0;
    }
    LRESULT lResult = 0;
    pSvc->TxSendMessage(uMsg, wParam, lParam, &lResult);
    return lResult;
}

void DuiRichEdit::SetDragDropEnabled(bool b)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }
    // 引擎的这一位是**反的**：置起表示「禁止拖放」。
    m_pTextHost->SetPropertyBits(TXTBIT_DISABLEDRAG, b ? 0 : TXTBIT_DISABLEDRAG);
}

::IDropTarget* DuiRichEdit::GetDropTarget()
{
    if (m_pTextHost == nullptr || !IsDragDropEnabled())
    {
        // 关掉拖放时返回空，宿主的分发器据此跳过本控件 —— 比让引擎收到
        // 拖放再自己拒绝更省事，光标也能正确显示成「不能放」。
        return nullptr;
    }
    if (m_pDropTarget == nullptr)
    {
        // 首次用到时才向引擎索取。引擎交出来时已经加过引用计数，
        // 由本控件持有，析构时释放。
        m_pDropTarget = m_pTextHost->CreateEngineDropTarget();
    }
    return m_pDropTarget;
}

bool DuiRichEdit::IsDragDropEnabled() const
{
    if (m_pTextHost == nullptr)
    {
        return false;
    }
    return (m_pTextHost->GetPropertyBits_() & TXTBIT_DISABLEDRAG) == 0;
}

// =================================================================
// 查找
// =================================================================

bool DuiRichEdit::FindText(LPCTSTR needle, long nStartFrom, bool bForward,
                           bool bMatchCase, bool bWholeWord,
                           long& cpMinOut, long& cpMaxOut) const
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr || needle == nullptr || *needle == _T('\0'))
    {
        return false;
    }

    // 起点没给就按方向取默认值：向前从当前选区末尾续着找，向后从选区起点
    // 往回找 —— 这样连续按「查找下一个」不会原地打转。
    long cpSelMin = 0;
    long cpSelMax = 0;
    GetSel(cpSelMin, cpSelMax);
    long cpStart = (nStartFrom >= 0) ? nStartFrom
                                     : (bForward ? cpSelMax : cpSelMin);

    CStringW wideNeedle(needle);

    FINDTEXTEXW ft;
    ::memset(&ft, 0, sizeof(ft));
    ft.chrg.cpMin = cpStart;
    // 终点：向前找到文档末尾（-1），向后找到开头（0）。
    ft.chrg.cpMax = bForward ? -1 : 0;
    ft.lpstrText  = (LPCWSTR)wideNeedle;

    DWORD dwFlags = 0;
    if (bForward)
    {
        dwFlags |= FR_DOWN;
    }
    if (bMatchCase)
    {
        dwFlags |= FR_MATCHCASE;
    }
    if (bWholeWord)
    {
        dwFlags |= FR_WHOLEWORD;
    }

    LRESULT lResult = -1;
    pSvc->TxSendMessage(EM_FINDTEXTEXW, (WPARAM)dwFlags, (LPARAM)&ft, &lResult);
    if (lResult < 0)
    {
        return false;
    }

    cpMinOut = ft.chrgText.cpMin;
    cpMaxOut = ft.chrgText.cpMax;
    return true;
}

bool DuiRichEdit::FindAndSelect(LPCTSTR needle, long nStartFrom, bool bForward,
                                bool bMatchCase, bool bWholeWord, bool bWrap)
{
    long cpMin = 0;
    long cpMax = 0;
    if (FindText(needle, nStartFrom, bForward, bMatchCase, bWholeWord, cpMin, cpMax))
    {
        SetSel(cpMin, cpMax);
        return true;
    }

    if (!bWrap)
    {
        return false;
    }

    // 绕回再找一轮：向前找就从文档开头重来，向后找就从末尾重来。
    const long cpRestart = bForward ? 0 : GetTextLength();
    if (FindText(needle, cpRestart, bForward, bMatchCase, bWholeWord, cpMin, cpMax))
    {
        SetSel(cpMin, cpMax);
        return true;
    }
    return false;
}

// =================================================================
// 持久化
// =================================================================

namespace {

// 流式读写的中转缓冲。引擎按块回调，读写两个方向共用同一个结构，
// 免得为两个方向各定义一份类型。
struct StreamCookie
{
    const BYTE*        m_pReadPtr;    // 待喂给引擎的剩余数据（读入方向）
    size_t             m_nReadLeft;   // 剩余字节数
    std::vector<BYTE>* m_pWriteBuf;   // 接收引擎输出的缓冲（写出方向）
};

// 读入方向的回调：把中转缓冲里的数据分块喂给引擎。
// 返回 0 且把已读字节数置 0 表示数据已经喂完。
DWORD CALLBACK StreamReadCb(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG* pcb)
{
    StreamCookie* pck = reinterpret_cast<StreamCookie*>(dwCookie);
    if (pck == nullptr || pbBuff == nullptr || pcb == nullptr || cb <= 0)
    {
        if (pcb != nullptr)
        {
            *pcb = 0;
        }
        return 0;
    }
    size_t nWant = (size_t)cb;
    if (nWant > pck->m_nReadLeft)
    {
        nWant = pck->m_nReadLeft;
    }
    if (nWant > 0)
    {
        ::memcpy(pbBuff, pck->m_pReadPtr, nWant);
        pck->m_pReadPtr  += nWant;
        pck->m_nReadLeft -= nWant;
    }
    *pcb = (LONG)nWant;
    return 0;
}

// 写出方向的回调：把引擎交出来的数据块追加到中转缓冲。
// 每次都要把「已写字节数」如实报回去，少报会让引擎以为写失败而中断。
DWORD CALLBACK StreamWriteCb(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG* pcb)
{
    StreamCookie* pck = reinterpret_cast<StreamCookie*>(dwCookie);
    if (pck == nullptr || pck->m_pWriteBuf == nullptr
        || pbBuff == nullptr || pcb == nullptr || cb <= 0)
    {
        if (pcb != nullptr)
        {
            *pcb = 0;
        }
        return 0;
    }
    pck->m_pWriteBuf->insert(pck->m_pWriteBuf->end(), pbBuff, pbBuff + cb);
    *pcb = cb;
    return 0;
}

} // 匿名命名空间

bool DuiRichEdit::SaveRTF(CStringA& out) const
{
    out.Empty();

    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return false;
    }

    std::vector<BYTE> buf;
    StreamCookie ck;
    ck.m_pReadPtr  = nullptr;
    ck.m_nReadLeft = 0;
    ck.m_pWriteBuf = &buf;

    EDITSTREAM es;
    ::memset(&es, 0, sizeof(es));
    es.dwCookie    = (DWORD_PTR)&ck;
    es.pfnCallback = &StreamWriteCb;

    LRESULT lResult = 0;
    pSvc->TxSendMessage(EM_STREAMOUT, SF_RTF, (LPARAM)&es, &lResult);
    if (es.dwError != 0)
    {
        return false;
    }

    if (!buf.empty())
    {
        ::memcpy(out.GetBufferSetLength((int)buf.size()), &buf[0], buf.size());
        out.ReleaseBuffer((int)buf.size());
    }
    return true;
}

bool DuiRichEdit::LoadRTF(const CStringA& in)
{
    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr)
    {
        return false;
    }

    StreamCookie ck;
    ck.m_pReadPtr  = (const BYTE*)(LPCSTR)in;
    ck.m_nReadLeft = (size_t)in.GetLength();
    ck.m_pWriteBuf = nullptr;

    EDITSTREAM es;
    ::memset(&es, 0, sizeof(es));
    es.dwCookie    = (DWORD_PTR)&ck;
    es.pfnCallback = &StreamReadCb;

    LRESULT lResult = 0;
    pSvc->TxSendMessage(EM_STREAMIN, SF_RTF, (LPARAM)&es, &lResult);
    Invalidate();
    return es.dwError == 0;
}

bool DuiRichEdit::SaveText(CString& out) const
{
    // 纯文本方向直接走已有的文本读取，口径与 GetText 一致（末尾不带
    // 引擎的结尾标记）。不另走流式接口，免得两个出口的口径对不上。
    out = GetText();
    return Test_GetTextServices() != nullptr;
}

bool DuiRichEdit::LoadText(const CString& in)
{
    if (Test_GetTextServices() == nullptr)
    {
        return false;
    }
    SetText(in);
    return true;
}

// =================================================================
// 其它属性
// =================================================================

void DuiRichEdit::SetMaxLength(int n)
{
    m_nMaxLength = n;
    if (m_pTextHost != nullptr)
    {
        m_pTextHost->SetMaxLength(n);
    }
}

void DuiRichEdit::SetPasswordMode(bool b)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }
    m_pTextHost->SetPropertyBits(TXTBIT_USEPASSWORD, b ? TXTBIT_USEPASSWORD : 0);
    Invalidate();
}

bool DuiRichEdit::IsPasswordMode() const
{
    if (m_pTextHost == nullptr)
    {
        return false;
    }
    return (m_pTextHost->GetPropertyBits_() & TXTBIT_USEPASSWORD) != 0;
}

void DuiRichEdit::SetPasswordChar(TCHAR ch)
{
    if (m_pTextHost != nullptr)
    {
        m_pTextHost->SetPasswordChar(ch);
        Invalidate();
    }
}

void DuiRichEdit::SetVertical(bool b)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }
    m_pTextHost->SetPropertyBits(TXTBIT_VERTICAL, b ? TXTBIT_VERTICAL : 0);
    m_bScrollDirtyV = true;
    m_bScrollDirtyH = true;
    Invalidate();
}

bool DuiRichEdit::IsVertical() const
{
    if (m_pTextHost == nullptr)
    {
        return false;
    }
    return (m_pTextHost->GetPropertyBits_() & TXTBIT_VERTICAL) != 0;
}

// =================================================================
// 滚动条
// =================================================================

bool DuiRichEdit::ShouldShowScrollBar(ScrollBarPolicy policy, bool bOverflow)
{
    switch (policy)
    {
    //总是显示：不看内容是否溢出。
    case kScrollBarAlways:
        return true;

    //从不显示。注意内容仍可用滚轮和键盘滚动，只是没有可拖动的滑块。
    case kScrollBarNever:
        return false;

    //自动（默认）：内容装不下才显示。
    case kScrollBarAuto:
    default:
        return bOverflow;
    }
}

void DuiRichEdit::SetVScrollPolicy(ScrollBarPolicy policy)
{
    if (m_vScrollPolicy == policy)
    {
        return;
    }
    m_vScrollPolicy = policy;

#if BUI_FEATURE_SCROLLBAR
    if (m_pScrollV != nullptr)
    {
        // 「总是显示」与自动隐藏是矛盾的：既然要求一直看得见，就不能让它
        // 停手几百毫秒之后自己淡出。
        m_pScrollV->SetAutoHide(policy != kScrollBarAlways);
    }
#endif
    m_bScrollDirtyV = true;
    Invalidate();
}

void DuiRichEdit::SetHScrollPolicy(ScrollBarPolicy policy)
{
    if (m_hScrollPolicy == policy)
    {
        return;
    }
    m_hScrollPolicy = policy;

#if BUI_FEATURE_SCROLLBAR
    if (m_pScrollH != nullptr)
    {
        m_pScrollH->SetAutoHide(policy != kScrollBarAlways);
    }
#endif
    m_bScrollDirtyH = true;
    Invalidate();
}

bool DuiRichEdit::GetVScrollMetrics(int& nContentH, int& nPos, int& nViewH) const
{
    nContentH = 0;
    nPos      = 0;
    nViewH    = 0;

    if (m_pTextHost == nullptr)
    {
        return false;
    }

    int nMin = 0;
    int nMax = 0;
    int nPage = 0;
    int nCur = 0;
    bool bEnabled = false;
    if (!m_pTextHost->QueryScrollInfo(true, nMin, nMax, nPage, nCur, bEnabled))
    {
        return false;
    }

    // 口径说明（2026-08-14 实测确认，见 ScrollRangeToMaxPosMapping 用例）：
    // **引擎报的 max 就是内容总高**，不是「最大有效滚动位置」。真正能滚到的
    // 最底部是 max 减去一屏 —— 因为滚到底时最后一屏正好填满可视区，末尾
    // 之后不该露出空白。
    //
    // 这两个口径差着整整一个屏幕高度，搞混的症状是滑块拖不到底：拖过头被
    // 引擎夹回来，回读同步之后滑块自己弹回中间某处。
    nViewH = nPage;
    nPos   = nCur - nMin;

    if (nMax > nMin)
    {
        nContentH = nMax - nMin;
    }
    else
    {
        // 内容装得下时引擎把滚动范围报成零 —— 那只说明「不用滚」，
        // 不代表内容没有高度。这种情况改问引擎「刚好装下内容需要多高」，
        // 否则会得出「非空内容的高度是 0」这种明显错误的答案。
        int nNatural = 0;
        if (MeasureContentHeight(nNatural))
        {
            nContentH = nNatural;
        }
    }
    return true;
}

bool DuiRichEdit::MeasureContentHeight(int& outHeight) const
{
    outHeight = 0;

    ITextServices* pSvc = Test_GetTextServices();
    if (pSvc == nullptr || m_pTextHost == nullptr)
    {
        return false;
    }

    HDC hdc = ::GetDC(nullptr);
    if (hdc == nullptr)
    {
        return false;
    }

    // 测量期间**临时把文本区撑高**，量完还原。宽度保持不变 —— 折行位置由
    // 宽度决定，宽度一动量出来的就不是当前排版的高度了。
    //
    // 为什么非撑不可：引擎量出来的「自然高度」不会超过它当前的排版区域。
    // 照控件现有高度去量，会得到两种错误结果：
    //   · 控件高度为 0 时量出 0。布局分两趟走，第一趟问期望尺寸时控件还没有
    //     矩形、只能报 0，于是第二趟分下来的矩形宽度对、高度为 0；控件要在
    //     这一刻量出真实高度才能自纠，量不出来的话首次显示就是零高度。
    //   · 控件已经按内容排到某个高度之后，量出来永远等于当前高度，内容再多
    //     也不会更大 —— 自动增高长到第一次那个高度就再也不动了。
    //
    // 范围参数随客户区一起变：SetClientRect 内部会按新矩形重算范围，所以这里只动
    // 客户区矩形，范围在撑高之后从 TxGetExtent 取。
    //
    // 代价是每次测量会让引擎多排一次版（撑高一次、还原一次）。自动增高的
    // 编辑器内容都很少，这点开销可以接受。
    const RECT rcSaved = m_pTextHost->GetClientRect_();
    RECT rcTall = rcSaved;
    rcTall.bottom = rcTall.top + kMeasureRoomHeight;
    m_pTextHost->SetClientRect(rcTall);

    // 范围参数必须在撑高之后再取 —— 取的就是与撑高后的客户区相配的那份值。
    // 另外两处必须给对的地方（2026-08-14 实测确认，给错都不会报错）：
    //   · **范围参数不能传全零**。传全零时出参的宽高会一并被写成 0，
    //     什么也量不到，而返回码却是成功。
    //   · **目标设备上下文必须传空**。传一个屏幕上下文进去，调用直接失败。
    SIZEL szExtent;
    szExtent.cx = 0;
    szExtent.cy = 0;
    m_pTextHost->TxGetExtent(&szExtent);

    // 宽度、高度都既是入参又是出参：入参宽度决定按多宽排版；**入参高度必须与范围
    // 参数的高度对应同一个矩形**。引擎把「入参高度 ÷ 范围高度」当作缩放比例，两者
    // 对应同一个矩形时比例为 1。2026-10-01 之前这里传 0，比例失真，量出的高度与宽度
    // 整体偏小：引擎实际行距 17 像素时，单行只量出 3～6 像素、三行只量出 9～18 像素。
    // 同日的对照实验（同一段文字，逐一改变两者）：两者对应同一矩形时四种测量模式都与
    // 引擎行距一致；范围高度改为入参的 2 倍，结果正好减半；范围宽度改变不影响结果。
    // 出参是排完之后的内容宽高，不受入参高度限制（入参 40 像素时三行照样量出 51 像素）。
    LONG lWidth  = rcSaved.right - rcSaved.left;
    LONG lHeight = rcTall.bottom - rcTall.top;
    HRESULT hr = pSvc->TxGetNaturalSize(DVASPECT_CONTENT, hdc, nullptr, nullptr,
                                        TXTNS_FITTOCONTENT, &szExtent,
                                        &lWidth, &lHeight);

    m_pTextHost->SetClientRect(rcSaved);
    ::ReleaseDC(nullptr, hdc);

    BUI_TRACE("MEASURE hr=0x%08X w=%d h=%d extent=(%d,%d) client=(%d,%d,%d,%d) tall=%d",
              (unsigned)hr, (int)lWidth, (int)lHeight,
              (int)szExtent.cx, (int)szExtent.cy,
              (int)rcSaved.left, (int)rcSaved.top, (int)rcSaved.right, (int)rcSaved.bottom,
              (int)(rcTall.bottom - rcTall.top));

    if (FAILED(hr) || lHeight <= 0)
    {
        return false;
    }
    outHeight = (int)lHeight;
    return true;
}

void DuiRichEdit::SetVScrollPos(int nPos)
{
    ApplyScrollPos(/*bVertical=*/true, nPos);
}

void DuiRichEdit::ApplyScrollPos(bool bVertical, int nPos)
{
    if (m_pTextHost == nullptr)
    {
        return;
    }

    m_pTextHost->SetScrollPos(bVertical, nPos);

    // **必须回读**。引擎按行边界对齐，写进去 40 像素可能落在 36 像素上。
    // 不回读的话滑块位置会与内容逐渐错位，滚得越多偏得越远。
    if (bVertical)
    {
        m_bScrollDirtyV = true;
    }
    else
    {
        m_bScrollDirtyH = true;
    }
    SyncScrollBars();
    Invalidate();
}

void DuiRichEdit::SyncScrollBars()
{
#if BUI_FEATURE_SCROLLBAR
    if (m_pTextHost == nullptr)
    {
        return;
    }
    // 没有变化就什么都不做 —— 本方法每次绘制都会被调到，不能每次都去
    // 打扰引擎。
    if (!m_bScrollDirtyV && !m_bScrollDirtyH)
    {
        return;
    }

    if (m_bScrollDirtyV && m_pScrollV != nullptr)
    {
        m_bScrollDirtyV = false;

        int nMin = 0;
        int nMax = 0;
        int nPage = 0;
        int nPos = 0;
        bool bEnabled = false;
        const bool bQueryOk = m_pTextHost->QueryScrollInfo(true, nMin, nMax,
                                                           nPage, nPos, bEnabled);
        BUI_TRACE("RE-SYNCV ok=%d min=%d max=%d page=%d pos=%d enabled=%d policy=%d",
                  bQueryOk ? 1 : 0, nMin, nMax, nPage, nPos,
                  bEnabled ? 1 : 0, (int)m_vScrollPolicy);
        if (bQueryOk)
        {
            // 口径换算：引擎报的 max 是**内容总高**，而滚动条控件要的是
            // **最大有效位置**，两者差一个可视区高度。不换算的话滑块拖不到底
            // ——拖过头被引擎夹回来，回读同步后滑块自己弹回中间。
            // 换算之后滑块比例也自动正确：滚动条按 一屏/(范围+一屏) 算滑块
            // 大小，代进去正好是 一屏/内容总高。
            const int nMaxPos = (nMax - nPage > nMin) ? (nMax - nPage) : nMin;

            m_pScrollV->SetRange(nMin, nMaxPos);
            m_pScrollV->SetPage(nPage);
            // **不通知**：这里是把引擎的状态同步过来，不是用户在拖滑块。
            // 通知了会触发回调再把位置写回引擎，形成来回打转。
            m_pScrollV->SetPos(nPos, /*notify=*/false);

            const bool bShow = ShouldShowScrollBar(m_vScrollPolicy, nMaxPos > nMin);
            BUI_TRACE("RE-SYNCV show=%d wasVisible=%d alpha=%.2f rect=(%d,%d,%d,%d)",
                      bShow ? 1 : 0, m_pScrollV->IsVisible() ? 1 : 0,
                      m_pScrollV->GetAlpha(),
                      (int)m_pScrollV->GetRect().left, (int)m_pScrollV->GetRect().top,
                      (int)m_pScrollV->GetRect().right, (int)m_pScrollV->GetRect().bottom);
            if (m_pScrollV->IsVisible() != bShow)
            {
                m_pScrollV->SetVisible(bShow);
            }
        }
    }

    if (m_bScrollDirtyH && m_pScrollH != nullptr)
    {
        m_bScrollDirtyH = false;

        int nMin = 0;
        int nMax = 0;
        int nPage = 0;
        int nPos = 0;
        bool bEnabled = false;
        if (m_pTextHost->QueryScrollInfo(false, nMin, nMax, nPage, nPos, bEnabled))
        {
            // 与竖直方向同一套口径换算，理由见上。
            const int nMaxPos = (nMax - nPage > nMin) ? (nMax - nPage) : nMin;

            m_pScrollH->SetRange(nMin, nMaxPos);
            m_pScrollH->SetPage(nPage);
            m_pScrollH->SetPos(nPos, /*notify=*/false);

            const bool bShow = ShouldShowScrollBar(m_hScrollPolicy, nMaxPos > nMin);
            if (m_pScrollH->IsVisible() != bShow)
            {
                m_pScrollH->SetVisible(bShow);
            }
        }
    }
#endif
}

void DuiRichEdit::OnVScrollBarMoved(void* user, int newPos)
{
    DuiRichEdit* pSelf = static_cast<DuiRichEdit*>(user);
    if (pSelf == nullptr)
    {
        return;
    }
    pSelf->ApplyScrollPos(/*bVertical=*/true, newPos);
}

void DuiRichEdit::OnHScrollBarMoved(void* user, int newPos)
{
    DuiRichEdit* pSelf = static_cast<DuiRichEdit*>(user);
    if (pSelf == nullptr)
    {
        return;
    }
    pSelf->ApplyScrollPos(/*bVertical=*/false, newPos);
}

bool DuiRichEdit::OnMouseWheel(POINT pt, short zDelta, UINT mkFlags)
{
    // **在转发之前就把「本控件有没有可滚范围」判出来。**
    //
    // 不能用引擎的返回码：它对滚轮消息一律返回成功，不区分「滚动了」和
    // 「已经到头」。照搬会把滚轮无条件吞掉，症状是鼠标停在文本框上时外层
    // 容器再也收不到滚轮，整页滚不动。
    //
    // 也不能用「转发前后比较滚动位置」：**引擎是用它自己的定时器异步滚动
    // 的**，转发返回的那一刻位置还没变，于是永远判成「没滚动」。
    // （2026-08-16 用运行期埋点实测确认。）
    //
    // 判据是**有没有可滚范围**，而不是「这个方向还能不能滚」—— 按库内的
    // 滚轮约定（见 DuiHost::DispatchMouseWheel 上方的说明），能滚的控件
    // 即使已经滚到顶 / 滚到底也要照常消费掉滚轮，不让事件继续上冒。理由是
    // 用户在内层滚到底后往往会再多滚一两下，此时外层整页突然跟着动，视觉
    // 参照一下子全变，比「滚不动」更让人困惑。只有**压根没有可滚范围**
    // （内容装得下）时才如实返回未处理，把机会让给外层容器。
    int nPos = 0;
    int nContentH = 0;
    int nViewH = 0;
    GetVScrollMetrics(nContentH, nPos, nViewH);

    const bool bHasScrollRange = (nContentH > nViewH);

    BUI_TRACE("RE-WHEEL delta=%d pos=%d content=%d view=%d hasRange=%d",
              (int)zDelta, nPos, nContentH, nViewH, bHasScrollRange ? 1 : 0);

    // **滚轮消息的坐标必须是屏幕坐标**，与其它鼠标消息不同。
    //
    // 按 Win32 的消息约定，WM_LBUTTONDOWN 一类的坐标是客户区坐标，而
    // WM_MOUSEWHEEL 的是屏幕坐标 —— 因为滚轮消息发给的是焦点窗口，不是
    // 鼠标下面那个窗口，客户区坐标对收信方没有意义。引擎照这个约定办事，
    // 拿到坐标后自己调宿主的「屏幕转客户区」换算回去。
    //
    // 传成客户区坐标的症状是**滚轮完全不起作用**：换算之后的点落在控件
    // 外面，引擎认为这次滚动与自己无关。而它对滚轮消息一律返回成功，
    // 所以既不报错也没有任何迹象。
    POINT ptScreen = pt;
    HWND hwndHost = TxSiteGetHostHwnd();
    if (hwndHost != nullptr)
    {
        ::ClientToScreen(hwndHost, &ptScreen);
    }

    // 交给引擎处理滚动 —— 它知道行高、知道当前排版结果，滚出来的步长
    // 与键盘翻页一致。自己按固定像素滚会与引擎的行对齐打架。
    LRESULT lResult = 0;
    ForwardToEngine(WM_MOUSEWHEEL,
                    MAKEWPARAM(mkFlags, zDelta),
                    MAKELPARAM(ptScreen.x, ptScreen.y),
                    &lResult);

#if BUI_FEATURE_SCROLLBAR
    if (bHasScrollRange)
    {
        // **先同步、再淡入，顺序不能反。**
        //
        // 滚动条的可见性是在同步那一步按「内容是否溢出」定下来的，而同步平时
        // 只在绘制入口做。若这里直接去触发淡入，第一次滚动时滚动条还处于不可见
        // 状态，淡入就被跳过了；等绘制时它才变成可见，但不透明度停在 0 —— 表现
        // 为**滚得动、却始终看不到滚动条**。
        m_bScrollDirtyV = true;
        SyncScrollBars();

        // 滚动时让滚动条淡入，停手后它自己会淡出 —— 这是自动隐藏形态的标准
        // 交互：只在真正滚动的时候才看得见。
        if (m_pScrollV != nullptr && m_pScrollV->IsVisible())
        {
            m_pScrollV->TriggerShow();
        }
    }
#endif

    return bHasScrollRange;
}

bool DuiRichEdit::OnMouseLeave()
{
    DuiControl::OnMouseLeave();

#if BUI_FEATURE_SCROLLBAR
    // 鼠标离开就开始淡出，不必等空闲计时器走完。
    if (m_pScrollV != nullptr)
    {
        m_pScrollV->StartFadeOut();
    }
    if (m_pScrollH != nullptr)
    {
        m_pScrollH->StartFadeOut();
    }
#endif
    return false;
}

// =================================================================
// IDuiTextHostSite —— 引擎反过来操作本控件的入口
// =================================================================

void DuiRichEdit::TxSiteInvalidate(const RECT* prc)
{
    BUI_TRACE("SITE-INVALIDATE %s host=%s",
                 prc != nullptr ? "rect" : "all",
                 m_pHost != nullptr ? "yes" : "null");

    if (prc != nullptr && m_pHost != nullptr)
    {
        // 局部失效。光标闪烁一次只需要重画很窄的一条，走这条路比整控件
        // 重画省得多。
        m_pHost->InvalidateDuiRect(*prc);
        return;
    }
    Invalidate();
}

void DuiRichEdit::TxSiteSetCapture(bool bCapture)
{
    if (bCapture)
    {
        Capture();
    }
    else
    {
        ReleaseCapture();
    }
}

void DuiRichEdit::TxSiteSetFocus()
{
    if (m_bFocusable)
    {
        SetFocus();
    }
}

void DuiRichEdit::TxSiteScrollInfoChanged(bool bVertical)
{
    // 只置标志，不在这里查询引擎 —— 本方法是在引擎的回调栈里被调用的，
    // 此刻反过来查询引擎属于重入。真正的查询与滚动条同步放在第 7 步，
    // 在下一次布局或绘制这类安全时机进行。
    if (bVertical)
    {
        m_bScrollDirtyV = true;
    }
    else
    {
        m_bScrollDirtyH = true;
    }
}

HRESULT DuiRichEdit::TxSiteNotify(DWORD iNotify, void* /*pv*/)
{
    switch (iNotify)
    {
    //文档内容发生了变化（打字、粘贴、撤销等）。
    case EN_CHANGE:
        NotifyParent(DUIN_VALUECHANGED);
        // 打开自动增高时，内容一变期望高度就可能跟着变，这里请求宿主重新排版。
        // 注意这是**异步**的：本函数是排版引擎在处理按键的中途回调上来的，
        // 当场重排会形成对引擎的重入调用。详见 DuiHost::RequestRelayout 的注释。
        RequestAutoGrowRelayout();
        break;

    //选区发生了变化。目前不对外发通知，留待需要时再加。
    case EN_SELCHANGE:
        break;

    //其余通知暂不处理。返回成功表示不否决引擎的这次操作 ——
    //少数通知的返回值有「宿主否决」的含义，不能一律返回失败。
    default:
        break;
    }
    return S_OK;
}

HWND DuiRichEdit::TxSiteGetHostHwnd() const
{
    // 控件自己没有窗口，光标与输入法都得借宿主窗口的句柄。
    // 控件尚未挂进 DUI 树时返回空，宿主实现那边有判空处理。
    if (m_pHost == nullptr)
    {
        return nullptr;
    }
    return m_pHost->m_hWnd;
}

// =================================================================
// 测试用
// =================================================================

COLORREF DuiRichEdit::Test_GetBorderColor() const
{
    return BorderColor();
}

std::vector<DuiRichEditMenuItem> DuiRichEdit::Test_BuildContextMenu()
{
    std::vector<DuiRichEditMenuItem> items;
    BuildContextMenuModel(items);
    return items;
}

bool DuiRichEdit::Test_InvokeContextMenuCommand(UINT nId)
{
    if (OnContextMenuCommand(nId))
    {
        return true;
    }
    // 与真正弹出菜单时走的是同一条路：基类不认识的编号发通知给父窗口。
    NotifyParent(DUIN_RICHTEXT_MENUCOMMAND, (LPARAM)nId);
    return false;
}

int DuiRichEdit::Test_MeasureContentHeight() const
{
    int nHeight = 0;
    if (!MeasureContentHeight(nHeight))
    {
        return 0;
    }
    return nHeight;
}

bool DuiRichEdit::Test_IsEngineReady() const
{
    return m_pTextHost != nullptr && m_pTextHost->IsReady();
}

bool DuiRichEdit::Test_IsVScrollBarVisible() const
{
#if BUI_FEATURE_SCROLLBAR
    return m_pScrollV != nullptr && m_pScrollV->IsVisible();
#else
    return false;
#endif
}

RECT DuiRichEdit::Test_GetVScrollBarRect() const
{
    RECT rc;
    ::SetRect(&rc, 0, 0, 0, 0);
#if BUI_FEATURE_SCROLLBAR
    if (m_pScrollV != nullptr)
    {
        rc = m_pScrollV->GetRect();
    }
#endif
    return rc;
}

float DuiRichEdit::Test_GetVScrollBarAlpha() const
{
#if BUI_FEATURE_SCROLLBAR
    if (m_pScrollV == nullptr)
    {
        return -1.0f;
    }
    return m_pScrollV->GetAlpha();
#else
    return -1.0f;
#endif
}

ITextServices* DuiRichEdit::Test_GetTextServices() const
{
    if (m_pTextHost == nullptr)
    {
        return nullptr;
    }
    return m_pTextHost->GetTextServices();
}

void DuiRichEdit::Test_SetUiActive(bool b)
{
    m_bFocused = b;
    if (m_pTextHost != nullptr)
    {
        m_pTextHost->SetUiActive(b);
    }
}

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
