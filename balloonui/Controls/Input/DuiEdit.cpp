/**
 *  DuiEdit 的实现 —— 无窗口普通文本输入框。
 *
 *  本文件只负责"普通输入框相对富文本框多出来的那部分"：单行语义、左右内联
 *  图标栏、密码显隐按钮、单行文字垂直居中、旧配色的边框与底色。排版、光标、
 *  选区、输入法、剪贴板、右键菜单等一概由基类 DuiRichEdit 提供。
 *
 *  balloonwj@qq.com   2026-08-17
 */
#include "stdafx.h"
#include "DuiEdit.h"

#if BUI_FEATURE_EDIT

#include "../../DuiPaintAA.h"
#include "../../DuiResMgr.h"
#include "../../DuiTheme.h"   // GetDefaultFontFace：没给字体名时的默认字体

namespace balloonwjui {

namespace {

// ---- 边框与底色（沿用无窗口化之前那套配色）----
//
// 这四个色值取自旧的寄宿窗口版输入框，照搬是为了让两个程序里几百个现存输入
// 框的观感保持不变。基类 DuiRichEdit 用的是另一套偏蓝的配色（聊天输入框、
// 公告正文是那个样子），两套暂时并存，将来若统一，删掉本类的 BorderColor /
// FillColor 覆写即可回到基类配色。
const COLORREF kBorderDisabled = RGB(190, 190, 190);   // 控件被禁用
const COLORREF kBorderFocused  = RGB( 80, 130, 200);   // 持有键盘焦点
const COLORREF kBorderHover    = RGB(100, 140, 200);   // 鼠标悬停其上
const COLORREF kBorderNormal   = RGB(150, 150, 150);   // 常态
const COLORREF kFillDisabled   = RGB(240, 240, 240);   // 禁用时的整体底色

// ---- 几何 ----

// 边框宽度（像素）。图标矩形与文本区都从边框内侧开始算。
const int kBorderPx = 1;

// 文本区相对控件的默认内边距（像素），左右与上下分开。取值与旧输入框一致，
// 换了会让所有输入框里的文字左右位置发生肉眼可见的偏移。
const int kPadH = 4;
const int kPadV = 2;

// 图标矩形相对控件上下边的额外内缩（像素）。旧实现取的是当时的上内边距，
// 与 kPadV 同值，这里直接写成常量，免得上下内边距不等时行为变得难以解释。
const int kIconMarginV = 2;

// ---- 密码显隐按钮（小眼睛）----

// 按钮在文本区右侧保留的宽度（像素）。
const int kEyeBoxW = 22;
// 按钮里可见图标的宽高（像素）。图标在保留区内水平居中、在控件内垂直居中。
const int kEyeIconW = 18;
const int kEyeIconH = 12;
// 眼睛轮廓的线宽（像素）与瞳孔半径（像素）。
const float kEyeStrokeWidth = 1.5f;
const int   kEyePupilRadius = 3;
// 按钮的两种颜色：鼠标悬停时用强调色，其余时候用中性灰。
const COLORREF kEyeColorHover  = RGB( 45, 108, 223);
const COLORREF kEyeColorNormal = RGB(110, 110, 110);

// ---- 字体 ----

// SetCtlFont 的兜底字号：调用方没给字号时用的像素高。与无窗口化之前那份实现一致，不能改 —— 客户端有代码
// 按同一套取值构造 LOGFONT，改了会让输入框里的字与实际渲染出来的字对不上。没给字体名时用 DuiTheme 的
// 默认字体（缺省为微软雅黑，与原先写死的值相同；宿主按界面语言改换时一并跟随）。
const int     kFontSizeFallbackPx = 14;

// ---- 鼠标按下落点的分区标识 ----
//
// 用于匹配"按下"与"抬起"：只有在同一个区域按下并抬起才算一次点击。旧实现
// 只判断抬起位置，因此在别处按下、在图标上抬起也会触发一次点击，属于既有
// 缺陷，本次重写时一并修正。
const int kZoneNone      = 0;   // 没有按在任何可点区域上
const int kZoneLeftIcon  = 1;   // 按在左侧图标上
const int kZoneRightIcon = 2;   // 按在右侧图标上
const int kZoneEye       = 3;   // 按在密码显隐按钮上

// 空矩形，供"没有该区域"时返回。
RECT EmptyRect()
{
    RECT rc = { 0, 0, 0, 0 };
    return rc;
}

}   // namespace

// =================================================================
// IconState
// =================================================================

DuiEdit::IconState::IconState()
    : m_nWidth(0)
    , m_hBitmap(NULL)
    , m_crGlyph(RGB(0, 0, 0))
    , m_bClickable(false)
    , m_bHover(false)
{
}

// =================================================================
// 构造 / 析构
// =================================================================

DuiEdit::DuiEdit()
    : m_bPassword(false)
    , m_bShowEye(false)
    , m_bPwdRevealed(false)
    , m_bEyeHover(false)
    , m_bVCenter(true)
    , m_nPadL(kPadH)
    , m_nPadT(kPadV)
    , m_nPadR(kPadH)
    , m_nPadB(kPadV)
    , m_bSuppressNotify(false)
    , m_bNotifySuppressed(false)
    , m_nDragSlot(-1)
    , m_nPressedZone(kZoneNone)
    , m_hOwnFont(NULL)
{
    // 普通输入框默认单行、不换行；基类的默认值是按富文本场景定的（多行 +
    // 自动换行），这里必须显式改过来。
    SetMultiLine(false);
    SetWordWrap(false);

    // 切为纯文本模式。普通输入框的字体与颜色都是整体设置的，用不到分段格式；
    // 而富文本模式下文档末尾那个看不见的段落结束符可以被选中（光标在末尾时
    // 按 Shift+→、或拖选越过末尾），文字后面会多出一小块高亮，看起来像是末尾
    // 有空格（2026-09-30 登录窗账号框即如此）。纯文本模式下选区不能越过文本
    // 末尾。
    //
    // 必须在写入任何文字之前切换：文档非空时排版引擎拒绝切换。
    SetPlainTextMode(true);

    // 粘贴一律按纯文本处理。普通输入框不接受外来格式 —— 从网页上复制一段
    // 带样式的文字过来，不该把字体与颜色一起带进来。
    SetPasteAsPlainTextDefault(true);

    // 默认不接受拖放，与无窗口化之前的行为保持一致（系统输入框本身不接受
    // 拖放）。这一点不只是习惯问题：控件一旦提供拖放目标，宿主窗口的拖放
    // 目标就会被它占用，窗口自身的拖放功能随之失效 —— 聊天窗口的"把文件
    // 拖进窗口发送"就是这样被挤掉的（用例 CDR3 / CDR4 钉住了这条约束）。
    // 确实需要往输入框里拖文字的场景，由调用方自己打开。
    SetDragDropEnabled(false);

    ApplyTextInsets(0);
}

DuiEdit::~DuiEdit()
{
    if (m_hOwnFont != NULL)
    {
        ::DeleteObject(m_hOwnFont);
        m_hOwnFont = NULL;
    }
}

// =================================================================
// 文本
// =================================================================

void DuiEdit::SetText(LPCTSTR sz)
{
    // 内容没变就整个跳过 —— 不写引擎、不请求重绘、更不发"内容变了"的通知。
    //
    // 这项判断不只是为了节省开销，缺少它会导致界面持续重绘：有的调用方在
    // <u>绘制过程中</u>把数据写回只读展示用的输入框，而写入会请求重绘，重绘
    // 又会执行同一次写回，两者互相触发，界面无法稳定下来。
    //
    // 无窗口化之前的实现同样有这项判断，它比较的是控件自己缓存的那份文本；
    // 这里改为直接读取引擎中的当前内容来比较，因为无窗口实现只保存引擎中的
    // 这一份文本，不存在可能与之不一致的第二份缓存。
    if (GetText() == CString(sz != NULL ? sz : _T("")))
    {
        return;
    }

    // 这里<u>不要</u>再补一次 OnTextChanged 或 DUIN_VALUECHANGED。
    //
    // 排版引擎在内容被程序改写时同样会发出"内容已变"的通知，本控件在
    // TxSiteNotify 里已经把它转成了 OnTextChanged，基类也据此发过一次
    // DUIN_VALUECHANGED。补第二次的症状是内容变化的回调与通知各来两遍。
    //
    // 顺带纠正一处：基类源码里曾写着"程序直接换内容不会走到 EN_CHANGE"，
    // 这条说法与实测不符（用例 DuiEditTests/SetTextNotifiesAndNoNotifyVariantDoesNot
    // 钉住了这一点）。
    DuiRichEdit::SetText(sz);
}

void DuiEdit::SetTextNoNotify(LPCTSTR sz)
{
    // 本方法只阻止发送给宿主窗口的通知，控件内部的 OnTextChanged 回调仍会
    // 照常执行。该回调是控件同步自身状态的地方（例如搜索框据此决定清除按钮
    // 是否显示），一并阻止会让控件的外观停留在变化之前的状态。
    //
    // 标志位在 NotifyParent 中起作用，而不是在本方法中直接判断，因为通知并非
    // 由本方法发出：文本写入引擎之后，由引擎通知本控件，再由基类发送给宿主。
    const bool bSaved = m_bSuppressNotify;
    m_bSuppressNotify = true;
    SetText(sz);
    m_bSuppressNotify = bSaved;
}

void DuiEdit::OnTextChanged()
{
    // 基类实现什么也不做，留给子类覆写。
}

LRESULT DuiEdit::NotifyParent(UINT code, LPARAM extra)
{
    // 有两种情况不向宿主发送通知：一是复合控件（如下拉框）把内嵌的输入框长期
    // 设置为不发送通知；二是调用方本次使用 SetTextNoNotify 设置文本，只在这一
    // 次调用期间不发送通知。
    if (m_bNotifySuppressed || m_bSuppressNotify)
    {
        return 0;
    }
    return DuiRichEdit::NotifyParent(code, extra);
}

HRESULT DuiEdit::TxSiteNotify(DWORD iNotify, void* pv)
{
    HRESULT hr = DuiRichEdit::TxSiteNotify(iNotify, pv);

    // 用户编辑导致的内容变化经由这条通知回来；程序调 SetText 换内容不走这里，
    // 那条路在 SetText 里另行调了 OnTextChanged。两条路合起来才是完整的
    // "内容变了"。
    if (iNotify == EN_CHANGE)
    {
        OnTextChanged();
    }
    return hr;
}

// =================================================================
// 密码模式
// =================================================================

void DuiEdit::SetPassword(bool b)
{
    if (m_bPassword == b)
    {
        return;
    }
    m_bPassword = b;

    // 退出密码模式时把"明文显示"一并复位，否则下次再设成密码框会直接停在
    // 明文状态上。
    if (!m_bPassword)
    {
        m_bPwdRevealed = false;
    }

    SetPasswordMode(m_bPassword && !m_bPwdRevealed);

    // 密码显隐按钮的可见性跟着变，文本区右侧的保留宽度也要重算。
    ApplyTextInsets(0);
    Invalidate();
}

void DuiEdit::SetShowEyeToggle(bool b)
{
    if (m_bShowEye == b)
    {
        return;
    }

    // 关掉按钮之前先把密码恢复成遮蔽 —— 按钮没了就再也没法收回明文，
    // 密码会一直裸露在界面上。
    if (!b && m_bPwdRevealed)
    {
        SetPasswordRevealed(false);
    }

    m_bShowEye = b;
    ApplyTextInsets(0);
    Invalidate();
}

void DuiEdit::SetPasswordRevealed(bool b)
{
    if (!m_bPassword || m_bPwdRevealed == b)
    {
        return;
    }
    m_bPwdRevealed = b;

    // 明文就是"不启用密码遮蔽"。遮蔽字符本身不用动 —— 基类记着它，
    // 下次重新遮蔽时照旧用。
    SetPasswordMode(!m_bPwdRevealed);
    Invalidate();
}

// =================================================================
// 内联图标
// =================================================================

DuiEdit::IconState& DuiEdit::Icon(IconSlot slot)
{
    return (slot == LeftIcon) ? m_iconL : m_iconR;
}

const DuiEdit::IconState& DuiEdit::Icon(IconSlot slot) const
{
    return (slot == LeftIcon) ? m_iconL : m_iconR;
}

RECT DuiEdit::ComputeIconRect(const RECT& rc, IconSlot slot,
                              int gutterWidth, int borderPx, int marginVPx)
{
    if (gutterWidth <= 0)
    {
        return EmptyRect();
    }

    RECT r;
    r.top    = rc.top + borderPx + marginVPx;
    r.bottom = rc.bottom - borderPx - marginVPx;
    if (slot == LeftIcon)
    {
        r.left  = rc.left + borderPx;
        r.right = r.left + gutterWidth;
    }
    else
    {
        r.right = rc.right - borderPx;
        r.left  = r.right - gutterWidth;
    }
    return r;
}

RECT DuiEdit::IconRect(IconSlot slot) const
{
    // 密码显隐按钮占着右侧位置时，右侧图标整体让位。两者只能有一个。
    if (slot == RightIcon && EyeVisible())
    {
        return EmptyRect();
    }
    return ComputeIconRect(m_rcItem, slot, Icon(slot).m_nWidth, kBorderPx, kIconMarginV);
}

void DuiEdit::SetIcon(IconSlot slot, int gutterWidth, IconPainter painter)
{
    IconState& st = Icon(slot);

    // 画法为空即视为清除该图标，宽度一并归零。判断是否需要重新布局时，必须
    // 比较"最终写入的宽度"，不能比较传入的参数：调用方传入与当前相同的宽度、
    // 同时传入空画法时，实际写入的宽度是 0，比较参数会得出"宽度未变"的错误
    // 结论，文本区将一直保持内缩、不再恢复。（旧实现存在此缺陷。）
    const int nNewWidth = (painter != NULL && gutterWidth > 0) ? gutterWidth : 0;
    const bool bWidthChanged = (st.m_nWidth != nNewWidth);

    st.m_nWidth  = nNewWidth;
    st.m_painter = painter;
    st.m_hBitmap = NULL;
    st.m_strGlyph.Empty();

    if (bWidthChanged)
    {
        ApplyTextInsets(0);
    }
    Invalidate();
}

void DuiEdit::SetIconBitmap(IconSlot slot, int gutterWidth, HBITMAP hbm)
{
    IconState& st = Icon(slot);

    const int nNewWidth = (hbm != NULL && gutterWidth > 0) ? gutterWidth : 0;
    const bool bWidthChanged = (st.m_nWidth != nNewWidth);

    st.m_nWidth  = nNewWidth;
    st.m_hBitmap = hbm;
    st.m_painter = NULL;
    st.m_strGlyph.Empty();

    if (bWidthChanged)
    {
        ApplyTextInsets(0);
    }
    Invalidate();
}

void DuiEdit::SetIconGlyph(IconSlot slot, int gutterWidth, LPCTSTR szGlyph, COLORREF crText)
{
    IconState& st = Icon(slot);

    const bool bHasGlyph = (szGlyph != NULL && szGlyph[0] != _T('\0'));
    const int  nNewWidth = (bHasGlyph && gutterWidth > 0) ? gutterWidth : 0;
    const bool bWidthChanged = (st.m_nWidth != nNewWidth);

    st.m_nWidth  = nNewWidth;
    st.m_strGlyph = bHasGlyph ? szGlyph : _T("");
    st.m_crGlyph = crText;
    st.m_painter = NULL;
    st.m_hBitmap = NULL;

    if (bWidthChanged)
    {
        ApplyTextInsets(0);
    }
    Invalidate();
}

void DuiEdit::ClearIcon(IconSlot slot)
{
    SetIcon(slot, 0, NULL);
}

int DuiEdit::GetIconWidth(IconSlot slot) const
{
    return Icon(slot).m_nWidth;
}

void DuiEdit::SetIconClickable(IconSlot slot, bool clickable)
{
    Icon(slot).m_bClickable = clickable;
}

bool DuiEdit::IsIconClickable(IconSlot slot) const
{
    return Icon(slot).m_bClickable;
}

void DuiEdit::SetIconDragHandler(IconSlot slot, IconDragHandler handler)
{
    Icon(slot).m_dragHandler = handler;
}

bool DuiEdit::OnIconClicked(IconSlot /*slot*/)
{
    return false;
}

// =================================================================
// 垂直居中与内边距
// =================================================================

void DuiEdit::SetVerticalCenter(bool b)
{
    if (m_bVCenter == b)
    {
        return;
    }
    m_bVCenter = b;
    ApplyTextInsets(0);
    Invalidate();
}

void DuiEdit::SetMargins(int left, int top, int right, int bottom)
{
    m_nPadL = left;
    m_nPadT = top;
    m_nPadR = right;
    m_nPadB = bottom;
    ApplyTextInsets(0);
}

void DuiEdit::ApplyTextInsets(int nItemHeight)
{
    // 左右：内容内边距之外，再让出图标栏的宽度。右侧的密码显隐按钮与右侧
    // 图标互斥，按钮优先。
    const int nLeft  = m_nPadL + m_iconL.m_nWidth;
    const int nRight = m_nPadR + (EyeVisible() ? kEyeBoxW : m_iconR.m_nWidth);

    int nTop    = m_nPadT;
    int nBottom = m_nPadB;

    const int nHeight = (nItemHeight > 0)
                      ? nItemHeight
                      : (int)(m_rcItem.bottom - m_rcItem.top);

    // 单行居中：把文字所在的那一行摆在控件高度的正中。做法是把上下内边距
    // 算成"剩余空间的一半"，让文本区恰好只有一行高 —— 排版引擎在文本区内
    // 永远从顶部开始排，文本区自身居中了，文字也就居中了。
    if (m_bVCenter && !IsMultiLine() && nHeight > 0)
    {
        const int nInnerH = nHeight - kBorderPx * 2;
        const int nLineH  = GetLineHeight();
        if (nLineH > 0 && nLineH < nInnerH)
        {
            nTop    = (nInnerH - nLineH) / 2;
            nBottom = nInnerH - nLineH - nTop;
        }
    }

    // 这里必须明确调用基类的实现。若调用虚函数，会转回本类的 SetMargins，
    // 把刚算出的"内容内边距加图标栏宽度"当作新的内容内边距记录下来，图标栏
    // 宽度就会在每次排版时被重复累加，文字位置逐次向内偏移。
    DuiRichEdit::SetMargins(nLeft, nTop, nRight, nBottom);
}

void DuiEdit::Layout(const RECT& rcAvail)
{
    // 内边距必须在基类布局之前算好：基类按文本区矩形摆放滚动条，若内边距在
    // 布局之后才生效，滚动条会停留在上一次布局算出的位置上。
    ApplyTextInsets((int)(rcAvail.bottom - rcAvail.top));
    DuiRichEdit::Layout(rcAvail);
}

// =================================================================
// 配色与便捷接口
// =================================================================

COLORREF DuiEdit::BorderColor() const
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
    return kBorderNormal;
}

COLORREF DuiEdit::FillColor() const
{
    return m_bEnabled ? GetBackgroundColor() : kFillDisabled;
}

void DuiEdit::SetBgColor(COLORREF c)
{
    SetBackgroundColor(c);
}

COLORREF DuiEdit::GetBgColor() const
{
    return GetBackgroundColor();
}

void DuiEdit::SetCtlFont(LPCTSTR family, int sizePx,
                         bool bBold, bool bItalic,
                         bool bUnderline, bool bStrikeOut)
{
    // 下面每一项取值都与无窗口化之前那份实现逐字段一致 —— 客户端有代码
    // （聊天窗的字体映射）明确声明按同一套取值构造 LOGFONT，好让"输入框里
    // 看到的字"与"发出去之后渲染出来的字"完全一样。改动其中任何一项都会让
    // 两边错开。
    if (sizePx <= 0)
    {
        sizePx = kFontSizeFallbackPx;
    }
    const CString defaultFace = DuiTheme::Inst().GetDefaultFontFace();
    LPCTSTR face = (family != NULL && family[0] != _T('\0')) ? family : (LPCTSTR)defaultFace;

    HFONT hNew = ::CreateFont(
        -sizePx, 0, 0, 0,
        bBold ? FW_BOLD : FW_NORMAL,
        bItalic ? TRUE : FALSE,
        bUnderline ? TRUE : FALSE,
        bStrikeOut ? TRUE : FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    if (hNew == NULL)
    {
        return;
    }

    SetDefaultFontFromHFONT(hNew);

    // 换掉旧字体要等新字体已经交给基类之后 —— 反过来会有一瞬间引擎手里
    // 握着已经销毁的字体。
    if (m_hOwnFont != NULL)
    {
        ::DeleteObject(m_hOwnFont);
    }
    m_hOwnFont = hNew;

    // 行高变了，单行居中的上下内边距要重算。
    ApplyTextInsets(0);
    Invalidate();
}

// =================================================================
// 绘制
// =================================================================

void DuiEdit::OnPaint(HDC hdc, const RECT& rcDirty)
{
    // 底色、边框、文字、占位文字、滚动条全部由基类画完；基类画文字时已经把
    // 裁剪限制在文本区之内，而图标栏是我们从文本区里让出来的地方，所以随后
    // 画图标不会被覆盖。
    DuiRichEdit::OnPaint(hdc, rcDirty);

    if (!IsVisible())
    {
        return;
    }

    PaintIcon(hdc, LeftIcon);
    if (EyeVisible())
    {
        PaintEyeToggle(hdc);
    }
    else
    {
        PaintIcon(hdc, RightIcon);
    }
}

void DuiEdit::PaintIcon(HDC hdc, IconSlot slot)
{
    const IconState& st = Icon(slot);
    if (st.m_nWidth <= 0)
    {
        return;
    }

    RECT rc = IconRect(slot);
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    // 三种画法互斥，按"自定义 → 位图 → 字形"的优先级取第一个有效的。
    if (st.m_painter != NULL)
    {
        st.m_painter(hdc, rc);
        return;
    }

    if (st.m_hBitmap != NULL)
    {
        BITMAP bi;
        ::memset(&bi, 0, sizeof(bi));
        if (::GetObject(st.m_hBitmap, sizeof(bi), &bi) == 0)
        {
            return;
        }
        HDC hdcMem = ::CreateCompatibleDC(hdc);
        if (hdcMem == NULL)
        {
            return;
        }
        HBITMAP hOld = (HBITMAP)::SelectObject(hdcMem, st.m_hBitmap);
        // 原样拷贝，不缩放也不做透明处理 —— 需要这些效果的调用方应当改用
        // 自定义画法自己画（接口注释里已写明）。
        const int x = (rc.left + rc.right - bi.bmWidth) / 2;
        const int y = (rc.top + rc.bottom - bi.bmHeight) / 2;
        ::BitBlt(hdc, x, y, bi.bmWidth, bi.bmHeight, hdcMem, 0, 0, SRCCOPY);
        ::SelectObject(hdcMem, hOld);
        ::DeleteDC(hdcMem);
        return;
    }

    if (!st.m_strGlyph.IsEmpty())
    {
        HFONT hFont = GetDefaultFont();
        HFONT hOldFont = (hFont != NULL) ? (HFONT)::SelectObject(hdc, hFont) : NULL;
        const int nOldMode = ::SetBkMode(hdc, TRANSPARENT);
        const COLORREF crOld = ::SetTextColor(hdc, st.m_crGlyph);

        ::DrawText(hdc, st.m_strGlyph, -1, &rc,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        ::SetTextColor(hdc, crOld);
        ::SetBkMode(hdc, nOldMode);
        if (hOldFont != NULL)
        {
            ::SelectObject(hdc, hOldFont);
        }
    }
}

RECT DuiEdit::EyeRect() const
{
    if (!EyeVisible())
    {
        return EmptyRect();
    }

    // 保留区贴着右边框内侧，图标在保留区里水平居中；纵向按整个控件居中。
    const int nRight = (int)m_rcItem.right - kBorderPx - kPadH;
    const int nLeft  = nRight - kEyeBoxW + (kEyeBoxW - kEyeIconW) / 2;
    const int nTop   = (int)(m_rcItem.top + m_rcItem.bottom - kEyeIconH) / 2;

    RECT rc;
    rc.left   = nLeft;
    rc.top    = nTop;
    rc.right  = nLeft + kEyeIconW;
    rc.bottom = nTop + kEyeIconH;
    return rc;
}

void DuiEdit::PaintEyeToggle(HDC hdc)
{
    RECT rc = EyeRect();
    if (rc.right <= rc.left)
    {
        return;
    }

    const COLORREF cr = m_bEyeHover ? kEyeColorHover : kEyeColorNormal;

    // 眼睛画成一个椭圆轮廓加居中的实心瞳孔。遮蔽与明文两种状态画的是同一个
    // 图形，只靠位置固定的按钮本身提示"这里可以切换"，不再加斜杠 —— 旧实现
    // 也是这样，换画法会让用户觉得按钮变了。
    DuiAA::FillEllipse(hdc, rc, CLR_INVALID, cr, kEyeStrokeWidth);

    const int cx = (rc.left + rc.right) / 2;
    const int cy = (rc.top + rc.bottom) / 2;
    RECT rcPupil;
    rcPupil.left   = cx - kEyePupilRadius;
    rcPupil.top    = cy - kEyePupilRadius;
    rcPupil.right  = cx + kEyePupilRadius;
    rcPupil.bottom = cy + kEyePupilRadius;
    DuiAA::FillEllipse(hdc, rcPupil, cr, CLR_INVALID, 0.0f);
}

// =================================================================
// 鼠标
// =================================================================

bool DuiEdit::OnLButtonDown(POINT pt, UINT mkFlags)
{
    // 一、图标栏被装了鼠标接管回调时，整条 gutter 归回调所有。
    for (int i = 0; i < 2; ++i)
    {
        const IconSlot slot = (i == 0) ? RightIcon : LeftIcon;
        IconState& st = Icon(slot);
        if (st.m_dragHandler == NULL || st.m_nWidth <= 0)
        {
            continue;
        }
        RECT rc = IconRect(slot);
        if (::PtInRect(&rc, pt))
        {
            m_nDragSlot = (int)slot;
            Capture();
            st.m_dragHandler(WM_LBUTTONDOWN, pt, rc);
            return true;
        }
    }

    // 二、密码显隐按钮。
    if (EyeVisible())
    {
        RECT rcEye = EyeRect();
        if (::PtInRect(&rcEye, pt))
        {
            m_nPressedZone = kZoneEye;
            return true;
        }
    }

    // 三、可点击图标。按下就消费掉，不能交给排版引擎 —— 否则点一下图标，
    // 光标会跟着跳到文本里去。
    if (m_iconL.m_bClickable && m_iconL.m_nWidth > 0)
    {
        RECT rc = IconRect(LeftIcon);
        if (::PtInRect(&rc, pt))
        {
            m_nPressedZone = kZoneLeftIcon;
            return true;
        }
    }
    if (!EyeVisible() && m_iconR.m_bClickable && m_iconR.m_nWidth > 0)
    {
        RECT rc = IconRect(RightIcon);
        if (::PtInRect(&rc, pt))
        {
            m_nPressedZone = kZoneRightIcon;
            return true;
        }
    }

    m_nPressedZone = kZoneNone;
    return DuiRichEdit::OnLButtonDown(pt, mkFlags);
}

bool DuiEdit::OnLButtonUp(POINT pt, UINT mkFlags)
{
    // 一、正处于拖动接管状态：先释放鼠标捕获再调用回调，这样回调内部即便触发
    //     重新布局，本控件的拖动状态也已经是完整的。
    if (m_nDragSlot != -1)
    {
        const IconSlot slot = (IconSlot)m_nDragSlot;
        m_nDragSlot = -1;
        ReleaseCapture();

        IconState& st = Icon(slot);
        if (st.m_dragHandler != NULL)
        {
            st.m_dragHandler(WM_LBUTTONUP, pt, IconRect(slot));
        }
        return true;
    }

    const int nZone = m_nPressedZone;
    m_nPressedZone = kZoneNone;

    // 二、按下与抬起必须落在同一个区域才算点击。
    if (nZone == kZoneEye)
    {
        RECT rcEye = EyeRect();
        if (::PtInRect(&rcEye, pt))
        {
            SetPasswordRevealed(!m_bPwdRevealed);
        }
        return true;
    }
    if (nZone == kZoneLeftIcon || nZone == kZoneRightIcon)
    {
        const IconSlot slot = (nZone == kZoneLeftIcon) ? LeftIcon : RightIcon;
        RECT rc = IconRect(slot);
        if (::PtInRect(&rc, pt))
        {
            // 子类先有机会自己消化这次点击（搜索框的清除叉号就是这么做的），
            // 它不要才发给宿主。
            if (!OnIconClicked(slot))
            {
                NotifyParent((slot == LeftIcon) ? DUIN_EDIT_LEFT_ICON_CLICK
                                                : DUIN_EDIT_RIGHT_ICON_CLICK);
            }
        }
        return true;
    }

    return DuiRichEdit::OnLButtonUp(pt, mkFlags);
}

bool DuiEdit::OnMouseMove(POINT pt, UINT mkFlags)
{
    // 拖动接管期间一律转交回调，并且不判断鼠标是否越出控件范围 —— 鼠标移到
    // 控件之外时仍须继续收到移动消息，否则用户拖动滚动条时只要指针稍微移出
    // 控件，拖动就会中断。
    if (m_nDragSlot != -1)
    {
        IconState& st = Icon((IconSlot)m_nDragSlot);
        if (st.m_dragHandler != NULL)
        {
            st.m_dragHandler(WM_MOUSEMOVE, pt, IconRect((IconSlot)m_nDragSlot));
        }
        return true;
    }

    // 更新悬停状态，变化了才重绘。
    bool bDirty = false;

    if (EyeVisible())
    {
        RECT rcEye = EyeRect();
        const bool bIn = (::PtInRect(&rcEye, pt) != FALSE);
        if (bIn != m_bEyeHover)
        {
            m_bEyeHover = bIn;
            bDirty = true;
        }
    }
    else if (m_bEyeHover)
    {
        m_bEyeHover = false;
        bDirty = true;
    }

    for (int i = 0; i < 2; ++i)
    {
        const IconSlot slot = (i == 0) ? LeftIcon : RightIcon;
        IconState& st = Icon(slot);
        bool bIn = false;
        if (st.m_bClickable && st.m_nWidth > 0)
        {
            RECT rc = IconRect(slot);
            bIn = (::PtInRect(&rc, pt) != FALSE);
        }
        if (bIn != st.m_bHover)
        {
            st.m_bHover = bIn;
            bDirty = true;
        }
    }

    if (bDirty)
    {
        Invalidate();
    }

    return DuiRichEdit::OnMouseMove(pt, mkFlags);
}

bool DuiEdit::OnMouseEnter()
{
    const bool bRet = DuiRichEdit::OnMouseEnter();
    // 基类只置标志、不重绘，而边框的悬停色要靠重绘才看得到。
    Invalidate();
    return bRet;
}

bool DuiEdit::OnMouseLeave()
{
    const bool bRet = DuiRichEdit::OnMouseLeave();

    m_bEyeHover = false;
    m_iconL.m_bHover = false;
    m_iconR.m_bHover = false;
    Invalidate();
    return bRet;
}

bool DuiEdit::OnSetCursor(POINT pt)
{
    if (!m_bEnabled)
    {
        return DuiRichEdit::OnSetCursor(pt);
    }

    // 可点的地方一律手形。
    if (EyeVisible())
    {
        RECT rcEye = EyeRect();
        if (::PtInRect(&rcEye, pt))
        {
            ::SetCursor(::LoadCursor(NULL, IDC_HAND));
            return true;
        }
    }
    for (int i = 0; i < 2; ++i)
    {
        const IconSlot slot = (i == 0) ? LeftIcon : RightIcon;
        const IconState& st = Icon(slot);
        if (st.m_nWidth <= 0)
        {
            continue;
        }
        RECT rc = IconRect(slot);
        if (!::PtInRect(&rc, pt))
        {
            continue;
        }
        if (st.m_bClickable)
        {
            ::SetCursor(::LoadCursor(NULL, IDC_HAND));
            return true;
        }
        if (st.m_dragHandler != NULL)
        {
            // 装了拖动回调的图标栏里多半是自绘滚动条，用箭头而不是文本工字形。
            ::SetCursor(::LoadCursor(NULL, IDC_ARROW));
            return true;
        }
    }

    return DuiRichEdit::OnSetCursor(pt);
}

// =================================================================
// 键盘
// =================================================================

bool DuiEdit::OnKeyDown(UINT vk, UINT flags)
{
    // Esc：单行多行都不该由输入框自己消化，转成通知交给宿主窗口（收起弹出
    // 层、退出内联编辑之类）。
    if (vk == VK_ESCAPE)
    {
        NotifyParent(DUIN_EDIT_ESCAPE);
        return true;
    }

    // 回车：单行输入框里回车是"提交"而不是换行。
    if (vk == VK_RETURN && !IsMultiLine())
    {
        NotifyParent(DUIN_EDIT_ENTER);
        return true;
    }

    return DuiRichEdit::OnKeyDown(vk, flags);
}

bool DuiEdit::OnChar(TCHAR ch)
{
    // 这里必须与上面的按键处理成对出现。宿主派发按键消息时不看返回值，随后
    // 由系统消息泵生成的字符消息照样会送进来 —— 只拦按键、不拦字符的话，
    // 回车仍然会被排版引擎当成换行插进去。
    if ((ch == _T('\r') || ch == _T('\n')) && !IsMultiLine())
    {
        return true;
    }

    // 制表符同理：Tab 键已被宿主截去做焦点遍历，不该再往文本里插一个制表符。
    if (ch == _T('\t'))
    {
        return true;
    }

    // Esc 的字符消息（0x1B）同样丢弃，否则会在文本里留下一个不可见字符。
    if (ch == (TCHAR)VK_ESCAPE)
    {
        return true;
    }

    return DuiRichEdit::OnChar(ch);
}

}   // namespace balloonwjui

#endif  // BUI_FEATURE_EDIT
