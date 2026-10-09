/**
 *  DuiRichEdit —— 无窗口富文本编辑控件。
 *  balloonwj@qq.com   2026-08-14
 */

#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiRichEdit —— 无窗口富文本编辑控件
// =================================================================
//
// 用途：需要富文本编辑或展示的场景 —— 聊天输入框、公告正文、更新说明、
// 富文本编辑器等。它是 DUI 树里的普通一员，**没有自己的窗口**，文字由
// 系统的排版引擎画在宿主的绘制目标上。
//
// ─────────────────────────────────────────────────────────────────
// 为什么走无窗口路线
// ─────────────────────────────────────────────────────────────────
//
// Windows 的富文本引擎有两种取用方式：建一个真的富文本子窗口，让引擎藏在
// 窗口过程后面；或者直接取出无窗口的排版引擎、自己给它当宿主。本控件走的
// 是后者，两条路用的是同一个系统引擎，因此文本行为、RTF 格式、链接识别
// 规则完全一致，差别只在"谁来画、画在哪一层"。
//
// 真子窗口永远画在 DUI 合成层之上：盖不住别的控件、也不会被别的控件盖住，
// 父容器的裁剪对它无效。这一条决定了它进不了滚动容器 / 页签 / 弹出层，也
// 做不出圆角、半透明、渐变背景与跟随布局的动画。无窗口路线没有这些限制 ——
// 文字与 DUI 树里其它控件画在同一层，层叠、裁剪、动画都按普通控件的规则
// 走，随内容自动变高、一个窗口里放很多个实例也都不成问题。多行 / 只读 /
// 换行等属性还能在运行期随时改（真子窗口那边这些是窗口风格位，改一次就得
// 销毁重建）。
//
// ─────────────────────────────────────────────────────────────────
// 构造完即可用，没有创建步骤
// ─────────────────────────────────────────────────────────────────
//
// 排版引擎不依赖任何窗口，所以在构造函数里就建好了，构造完就能设文本、
// 量尺寸。早先内嵌真子窗口的实现必须由调用方在宿主窗口就绪之后手工调一次
// EnsureCreated，漏了控件就是个空壳；**本控件没有这一步。**
//
// 这不只是少一次调用：那种实现因为存在"窗口建好前"和"窗口建好后"两种状态，
// 十几个方法都得写"窗口未建时……"的例外说明，调用方还得各自记得在合适的
// 时机去调。本控件只有一种状态，那些例外全部消失。
//
// ─────────────────────────────────────────────────────────────────
// 绘制上必须知道的两件事
// ─────────────────────────────────────────────────────────────────
//
// 一、**背景由本控件自己画。** 我们告诉引擎"背景透明、你别管"，换来的是
//     可以自己画圆角、渐变、半透明的底。代价是每次绘制都必须把背景铺满，
//     漏了会看到上一帧的残留。
//
// 二、**裁剪也由本控件自己做。** balloonui 的宿主与基类的绘制路径都不设
//     裁剪区，而引擎会按它自己的排版结果画，不裁就会画出控件边界。
//
// ─────────────────────────────────────────────────────────────────
// 代码用法
// ─────────────────────────────────────────────────────────────────
//
//     auto re = std::unique_ptr<DuiRichEdit>(new DuiRichEdit());
//     re->SetMultiLine(true);
//     re->SetWordWrap(true);
//     re->SetPlaceholder(_T("输入消息..."));
//     DuiRichEdit* raw = re.get();
//     m_inputRow->AddChild(std::move(re), DuiLayout::Hint().Weight(1));
//     // 不需要任何"创建"调用，直接就能用：
//     raw->SetText(_T("hello"));
//
// XML 用法（标签是 richtext）：
//
//     <richtext id="100" placeholder="输入消息..." multi-line="true"
//               word-wrap="true" margins="6,4,6,4" fixedHeight="120"/>
//
// 事件：
//   · DUIN_VALUECHANGED  —— 文字改变。
//   · DUIN_SETFOCUS / DUIN_KILLFOCUS —— 焦点进出（由基类发出）。

#include <windows.h>
#include "../../BalloonUiFeatures.h"
#include "../../DuiControl.h"
#include "DuiTextHost.h"
#include "RichEditContextMenu.h"
#include <vector>

// RichEdit 的 OLE 接口，只在插图那一组接口里以指针形式出现。
// 这里只前置声明、不包含 richole.h —— 那个头文件不小，而绝大多数使用本控件
// 的地方并不碰内联图片，没必要让它们都跟着编一遍。
struct IRichEditOle;
#if BUI_FEATURE_SCROLLBAR
#  include "../Window/DuiScrollBar.h"
#endif

namespace balloonwjui {

class BUI_API DuiRichEdit : public DuiControl, public IDuiTextHostSite
{
public:
    // 本控件特有的通知码。
    //
    // 刻意从 DUIN_CUSTOM + 60 起算而不是 +1：库里的自定义通知码是按控件
    // 各自编号的，每个控件都从 DUIN_CUSTOM 起算，于是不同控件的第一个
    // 自定义码数值必然相同，已经有十余个控件挤在 +1 那一档上。另开取值段
    // 能从源头减少撞号。（这只是缓解，派发端该判的控件编号仍然要判 ——
    // 见仓库约定里关于自定义通知码的那一条。）
    enum
    {
        DUIN_RICHTEXT_BASE = DUIN_CUSTOM + 60,
        DUIN_RICHTEXT_LINKCLICK = DUIN_RICHTEXT_BASE + 1,   // 点击了自动识别出的链接
        //用户在右键菜单里选了一个自定义项。extra 里带的是该项的命令编号
        //（即 AppendContextMenuItem 登记时用的那个 nId）。内置命令由控件
        //自己执行，不会走到这条通知。
        DUIN_RICHTEXT_MENUCOMMAND = DUIN_RICHTEXT_BASE + 2,
    };

    // 滚动条显示策略。
    //
    // 取代了旧控件那个「显不显示滚动条槽」的布尔开关 —— 那个开关的名字与
    // 语义都是围绕「真窗口的滚动条会永久占掉约 17 像素宽度」这个现象设计的，
    // 在无窗口控件里根本不存在这回事：本控件的滚动条是**覆盖式**的，浮在
    // 内容之上，任何时候都不占内容宽度。
    enum ScrollBarPolicy
    {
        //内容装不下时才出现，停止滚动后短暂延时自动淡出。默认值。
        kScrollBarAuto = 0,
        //只要控件可见就一直显示，即使内容装得下。
        kScrollBarAlways,
        //从不显示滚动条。注意内容**仍然可以**用滚轮和键盘滚动，
        //只是没有可拖动的滑块。适用于外层容器已经接管滚动的场景。
        kScrollBarNever
    };

    DuiRichEdit();
    ~DuiRichEdit() override;

    // ---- 文本 ----

    // 设置全文文本。会清空原有内容与格式。
    void    SetText(LPCTSTR sz);

    // 读取全文纯文本。
    //   返回：文档内容。**末尾的换行已去掉** —— 引擎会在文档尾部维护一个
    //         结尾标记，直接读回来会比写进去的多一个回车符；本方法统一
    //         归一化，保证「设进去什么就读回什么」。
    CString GetText() const;

    // 文档字符数。口径与 GetText 一致（不含尾部结尾标记）。
    int     GetTextLength() const;

    // ---- 基本属性（全部可在运行期随时修改，不需要重建任何东西）----

    // 只读。只约束用户输入；业务代码调 SetText 等方法照常生效。
    void    SetReadOnly(bool b);
    bool    IsReadOnly() const;

    // 多行。
    void    SetMultiLine(bool b);
    bool    IsMultiLine() const;

    // 自动换行。仅多行模式有意义。
    void    SetWordWrap(bool b);
    bool    IsWordWrap() const;

    // 切换纯文本 / 富文本模式。本类默认富文本；DuiEdit 在构造时切为纯文本。
    //
    // 两种模式的差别：纯文本模式下全文只有一套格式，选区也不能越过文本末尾；
    // 富文本模式下文档末尾那个看不见的段落结束符可以被选中（光标在末尾时按
    // Shift+→，或拖选越过末尾），界面上表现为文字后面多出一小块高亮，像是
    // 末尾有个空格。不需要分段格式的输入框应当用纯文本模式。
    //
    // **只能在文档为空时切换**，这是排版引擎的限制：文档里有内容时引擎拒绝
    // 切换。因此应当在构造之后、写入任何文字之前调用。
    //   bPlain：true 切为纯文本模式，false 切回富文本模式。
    //   返回：true 表示已处于目标模式（本来就是，或本次切换成功）；false 表示
    //         引擎拒绝了切换（通常是文档非空），模式保持不变。
    bool    SetPlainTextMode(bool bPlain);

    // 当前是否为纯文本模式。
    //   返回：true 表示纯文本模式，false 表示富文本模式。
    bool    IsPlainTextMode() const;

    // 是否接受键盘焦点。默认 true。
    //   b：false 表示点击本控件不会获得焦点、也不显示光标。适用于「只读
    //      展示但仍希望能选中复制」之外的纯展示场景 —— 例如更新说明区，
    //      不关掉的话窗口一显示焦点就落在它身上，只读区里闪着光标，
    //      看起来像是可以编辑。
    void    SetFocusable(bool b);
    bool    IsFocusable() const { return m_bFocusable; }

    // ---- 外观 ----

    // 背景色。
    void    SetBackgroundColor(COLORREF cr);
    COLORREF GetBackgroundColor() const { return m_crBackground; }

    // 默认文字颜色。
    void    SetTextColor(COLORREF cr);
    COLORREF GetTextColor() const { return m_crText; }

    // 是否绘制 1 像素外边框。默认 true。
    //   b：false 表示不画边框，让控件与父容器的底色融合（卡片内嵌一段
    //      只读文本时用得上）。
    void    SetShowBorder(bool b);
    bool    IsShowBorder() const { return m_bShowBorder; }

    // 是否显示文本插入光标（那个闪烁的竖条）。默认 true。
    //
    //   b：false 表示不显示光标。控件的其余行为**完全不变** —— 照常接受
    //      焦点、照常能点击定位、拖选、复制、滚动、弹右键菜单。
    //      它管的纯粹是视觉。
    //
    // 用在只读展示区：那种地方要的是「能选中复制、但别看着像能编辑」。
    //
    // **不要改用 SetFocusable(false) 去达到同样效果。** 那样确实没有光标，
    // 但文字会**变得选不中** —— 排版引擎在拖选过程中需要控件持有焦点，
    // 鼠标按下时它会主动来要，控件不给它就不认为自己处在拖选状态。
    // 「不接受焦点」与「能拖选」在这个引擎的模型下是互斥的，而只读展示区
    // 想要的从来只是「不显示光标」这一件事。
    void    SetShowCaret(bool b);
    bool    IsShowCaret() const;

    // 文字与控件边界之间的内边距（像素）。默认 4/2/4/2。
    //
    // 声明为虚函数是因为子类可能要在调用方给的内边距之上再叠加自己的占位
    // 需求 —— 库里的普通输入框就要在两侧让出图标栏的宽度，它把调用方设的
    // 值记为"内容内边距"，实际推给引擎的是内容内边距加图标栏宽度。
    virtual void SetMargins(int left, int top, int right, int bottom);

    // 用调用方持有的字体覆盖默认字体。
    //   font：字体句柄；**所有权归调用方**，本控件不持有、不销毁。
    //         传 nullptr 表示退回库内默认字体。
    void    SetDefaultFontFromHFONT(HFONT font);

    // ---- 占位文字（空文本且未聚焦时显示）----
    void    SetPlaceholder(LPCTSTR sz);
    CString GetPlaceholder() const { return m_strPlaceholder; }
    bool    IsShowingPlaceholder() const;

    // ---- 选区 ----
    //
    // 位置用**字符索引**计量（不是像素）：0 表示文档开头。

    // 设置选区。
    //   cpMin / cpMax：起止字符索引。两者相等表示只把光标放到该处、不选中
    //                  任何内容。cpMax 传 -1 表示一直选到文档末尾。
    //                  超出范围的值由引擎自行夹取，不会出错。
    void    SetSel(long cpMin, long cpMax);

    // 读取当前选区。
    //   cpMin / cpMax：出参，起止字符索引。相等表示没有选中内容。
    void    GetSel(long& cpMin, long& cpMax) const;

    // 选中全部内容。
    void    SelectAll();

    // 把当前选区替换成给定文本；没有选区时相当于在光标处插入。
    //   text：替换用的文本。
    //   bCanUndo：true（默认）表示这次修改可以被撤销。设为 false 时这次
    //             修改不进撤销栈，适用于程序批量填充内容、不希望用户
    //             一路撤销回去的场景。
    void    ReplaceSel(LPCTSTR text, bool bCanUndo = true);

    // 在文档末尾追加文本，不影响当前选区。
    void    AppendText(LPCTSTR text);

    // 文档行数。自动换行时算的是**显示行**（折行后的行数），不是段落数。
    int     LineCount() const;

    // ---- 编辑命令 ----
    //
    // 这几个就是右键菜单和快捷键背后调的东西，业务也可以直接调。

    // 撤销上一次修改。没有可撤销内容时什么也不做。
    void    Undo();

    // 重做被撤销的修改。**这是旧控件没有的能力** —— 无窗口路线下同样是
    // 往引擎转发一条消息，成本为零。
    void    Redo();

    // 当前是否有可撤销 / 可重做的内容。用于给工具栏按钮置灰。
    bool    CanUndo() const;
    bool    CanRedo() const;

    // 剪切 / 复制 / 粘贴 / 清除选中内容。
    void    Cut();
    void    Copy();
    void    Paste();
    void    Clear();

    // 只粘贴纯文本，丢掉源内容的字体、颜色、链接等一切格式。
    //   返回：true 粘贴成功；剪贴板里没有文本时返回 false。
    bool    PasteAsPlainText();

    // 是否把所有粘贴动作都当作纯文本粘贴。默认 false。
    //   b：true 适用于「不希望从网页复制过来一堆花花绿绿格式」的场景，
    //      典型是聊天输入框。
    void    SetPasteAsPlainTextDefault(bool b) { m_bPastePlainText = b; }
    bool    GetPasteAsPlainTextDefault() const { return m_bPastePlainText; }

    // ---- 字符格式（作用于当前选区）----
    //
    // 没有选区时，设置的是「接下来输入的字符」的格式 —— 与常见富文本
    // 编辑器的行为一致。

    void    SetSelBold(bool b);
    void    SetSelItalic(bool b);
    void    SetSelUnderline(bool b);
    void    SetSelTextColor(COLORREF cr);

    // 读取当前选区的格式。选区内格式不一致时返回 false（既不是全粗也不是
    // 全不粗），此时出参无意义 —— 这与富文本编辑器工具栏「粗体按钮呈现
    // 不确定态」的语义一致。
    bool    GetSelBold(bool& bBold) const;
    bool    GetSelItalic(bool& bItalic) const;
    bool    GetSelUnderline(bool& bUnderline) const;

    // ---- 段落格式（作用于当前选区所在的段落）----

    // 段落对齐方式。
    enum ParaAlignment
    {
        kParaLeft = 0,   // 左对齐（默认）
        kParaCenter,     // 居中
        kParaRight,      // 右对齐
        kParaJustify     // 两端对齐
    };

    // 设置 / 读取当前段落的对齐方式。
    void            SetParaAlignment(ParaAlignment align);
    ParaAlignment   GetParaAlignment() const;

    // 设置当前段落的左缩进。
    //   nPixels：缩进量（像素），内部换算成引擎要求的单位。传 0 取消缩进。
    void    SetParaLeftIndent(int nPixels);

    // 读取当前段落的左缩进（像素）。取不到时返回 0。
    // 注意换算到像素会有取整误差，设进去 60 读回来可能是 59 或 60。
    int     GetParaLeftIndent() const;

    // ---- 自动增高 ----
    //
    // 打开后，控件向布局体系报告的期望高度会随内容多少变化 —— 聊天输入框
    // 那种「打到第二行自己变高」的效果就是这么来的。
    //
    // **这是无窗口路线独有的能力。** 寄宿真窗口的控件拿不到引擎内部的排版
    // 结果，只能靠外部估算，估不准；本控件直接问引擎「按当前宽度排版，
    // 刚好装下要多高」。
    //
    // 注意期望高度只是**建议**：最终高度仍由父容器的布局规则决定。父容器
    // 给了固定高度时，自动增高不起作用，这是布局体系的既定行为。

    // 打开 / 关闭自动增高。默认打开。
    void    SetAutoGrow(bool b);
    bool    IsAutoGrow() const { return m_bAutoGrow; }

    // 设置自动增高的上下限（像素）。
    //   nMinPixels：下限；<= 0 时按「一行的高度」处理。
    //   nMaxPixels：上限；<= 0 表示不限。超过上限之后不再长高，改为出滚动条。
    // 像素是准的那个口径：富文本里行高会因字号不同而变化，按行数换算只是近似。
    void    SetAutoGrowRange(int nMinPixels, int nMaxPixels);
    int     GetAutoGrowMin() const { return m_nAutoGrowMin; }
    int     GetAutoGrowMax() const { return m_nAutoGrowMax; }

    // 按行数设置上下限的便捷写法，内部用**当前默认字体的行高**换算成像素。
    //   nMinLines：最少显示几行；<= 0 按 1 行处理。
    //   nMaxLines：最多长到几行；<= 0 表示不限。
    // 适用于聊天输入框这类「最少一行、最多五行」的需求。混排不同字号时
    // 换算会不准，那种场景请直接用像素版本。
    void    SetAutoGrowLines(int nMinLines, int nMaxLines);

    // 一行文字的高度（像素），按当前默认字体量。取不到时返回一个兜底值。
    int     GetLineHeight() const;

    // ---- 内联图片 ----
    //
    // 只在启用了内嵌图片能力时才有这一组（关掉 IMAGEOLE 时整组消失）。

#if BUI_FEATURE_IMAGEOLE
    // 插入一张**带标记**的内联图片，插在当前选区（有选区则替换）。
    //
    // 标记会随图片对象一起存在文档里，日后可由 EnumContent 原样读回 ——
    // 聊天输入框的表情就靠它把内联图还原成表情编号。本控件不解释标记的含义。
    //
    //   szPath：图片磁盘路径（PNG / JPG / BMP 等）。
    //   tag：   附着到该图片上的标记；本控件原样保存、原样回传。
    //   nSizePx：把图片**等比排版**到不超过 nSizePx × nSizePx；0 表示按原始
    //            像素尺寸排版。注意它只决定排版尺寸 —— 位图本身按原始分辨率
    //            保留，绘制时才一次性重采样到实际矩形。刻意不预先缩小位图：
    //            预缩小会先丢一次信息，而最终排出来的矩形未必正好等于
    //            nSizePx（换算受屏幕缩放影响），于是绘制时又要放大回去，
    //            一缩一放正是表情发糊的根源。
    //   返回：  true 插入成功；引擎不可用、文件解不开、插入失败时为 false。
    bool    InsertTaggedImage(LPCTSTR szPath, DWORD_PTR tag, int nSizePx = 0);

    // 插入一张**不带标记**的内联图片，插在当前选区（有选区则替换）。
    //
    // 与上面那个的区别只有一点：不给图片附着业务标记。适用于「插进去只为
    // 显示」的场景（例如把截图贴进输入框预览），调用方另有办法知道插了哪些图。
    //
    //   szPath：图片磁盘路径。
    //   nMaxW / nMaxH：把图片**等比排版**到不超过这个尺寸；0 表示该方向不限。
    //                  与上面那个一样，只决定排版尺寸、不动位图本身的分辨率。
    //   返回：  true 插入成功。
    bool    InsertImageFromFile(LPCTSTR szPath, int nMaxW = 0, int nMaxH = 0);

    // 逐段回调的原型。把图文混排的内容按**文档顺序**拆成「段」，逐段回调一次。
    //   bIsImage：true = 本段是一张内联图片（tag 有效、szText 为空串）；
    //             false = 本段是一串纯文本（szText 有效、tag 恒为 0）。
    //   szText：  文本段的内容。
    //   tag：     图片段的标记，等于插入时传进去的那个值。
    //   pCtx：    EnumContent 传入的上下文，原样透传。
    typedef void (*ContentSegmentFn)(bool bIsImage, LPCTSTR szText,
                                     DWORD_PTR tag, void* pCtx);

    // 按文档顺序枚举内容，纯文本段与图片段穿插回调。
    //
    // 用于把图文混排的内容序列化出去（聊天发送时把输入框内容编码成正文）。
    // **不能改用读全文那条路**：读回来的文本里，每个内联图只是一个占位字符，
    // 既认不出是哪张图、也拿不到它的标记。本方法靠图片对象自己记录的字符
    // 位置来切分文本，位置信息不会丢。
    //
    //   fn：  逐段回调；为空直接返回。
    //   pCtx：透传给回调的上下文。
    void    EnumContent(ContentSegmentFn fn, void* pCtx) const;

    // 文档里当前有多少个内联图片。主要供测试与排查用。
    //   返回：图片个数；引擎不可用时返回 0。
    int     GetEmbeddedImageCount() const;
#endif // BUI_FEATURE_IMAGEOLE

    // ---- 直通排版引擎（高级用法）----

    // 直接给排版引擎下发一条消息，返回它的处理结果。
    //
    // 本控件的公开接口覆盖了绝大多数需求，**优先用那些**：它们语义清楚、
    // 参数有类型检查、行为有单测保障。本方法是留给少数特殊场景的出口 ——
    // 引擎支持的能力远多于控件包装出来的那些，业务偶尔会用到某条冷门消息
    // （例如按范围设置字符格式、按字符位置反查坐标），为此逐个加窄接口不划算。
    //
    // **注意消息的参数约定按富文本控件那一套**，与普通编辑框未必相同。典型的
    // 坑是「按字符位置取坐标」：普通编辑框是把位置当参数传、从返回值里取坐标，
    // 而富文本控件要求传入一个坐标结构体的指针、由它填写。照搬前者会把一个
    // 小整数当指针写，后果严重。下发前请核对该消息在富文本控件下的约定。
    //
    //   uMsg / wParam / lParam：要下发的消息。
    //   返回：引擎的处理结果；引擎不可用时返回 0。
    LRESULT SendMessageToEngine(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // ---- 拖放 ----

    // 允许 / 禁止把文字拖进本控件，以及把选中的文字拖出去。默认允许。
    //
    // 这个开关运行期随时可改 —— 它对应引擎的一个属性位，不像真窗口控件
    // 那样需要重新注册拖放目标。
    void    SetDragDropEnabled(bool b);
    bool    IsDragDropEnabled() const;

    // ---- 右键菜单 ----
    //
    // 默认菜单的内容与各项的可用规则，见 RichEditContextMenu.h 里
    // DuiRichEditMenuCommand 的逐条注释。简单说：读写模式下是撤销 / 重做 /
    // 剪切 / 复制 / 粘贴 / 粘贴为纯文本 / 删除 / 全选，只读模式下只剩
    // 复制与全选。
    //
    // 定制能力分三层，按需要的深度选最省事的那一层：
    //
    //   第一层 —— 只想在默认菜单后面加几项：调 AppendContextMenuItem
    //     登记即可，不必写任何类。用户选中之后控件通过通知
    //     DUIN_RICHTEXT_MENUCOMMAND 把编号发给父窗口（编号在 extra 里）。
    //
    //   第二层 —— 想改默认项（删掉某项、改文案、临时禁用）：子类化本控件、
    //     覆写 OnBuildContextMenu，先调基类拿到默认菜单，再在结果上任意
    //     增删改。分隔条不必自己操心，控件在弹出前会统一规整。
    //
    //   第三层 —— 完全接管：同样覆写 OnBuildContextMenu，但不调基类实现，
    //     自己从空数组填起。或者干脆 SetContextMenuEnabled(false) 关掉，
    //     在自己的 OnRButtonDown 里弹别的东西。

    // 打开 / 关闭右键菜单。默认打开。
    //
    // 关掉之后右键按下不再弹菜单，但**仍然会转给排版引擎**（引擎据此移动
    // 光标、保持或清除选区），所以关掉它不会影响右键点击的其它行为。
    void    SetContextMenuEnabled(bool b);
    bool    IsContextMenuEnabled() const { return m_bContextMenuEnabled; }

    // 在默认菜单末尾追加一个自定义项。
    //
    //   nId：命令编号，**必须不小于 kRichEditMenuCustomBase（1000）**。
    //        低于该值会与内置命令撞号 —— 症状是点了自定义项却执行了粘贴
    //        这类很难查的现象，所以这里直接挡掉。
    //   szText：显示文案，可含 & 助记符。传空指针视为空文案。
    //   返回：登记成功为 true；编号越界为 false（同时记一条日志）。
    //
    // 登记的项与默认菜单之间会自动插入一条分隔条，调用方不必自己加。
    // 追加只支持末尾；需要插到中间请走第二层（覆写 OnBuildContextMenu）。
    bool    AppendContextMenuItem(UINT nId, LPCTSTR szText);

    // 在已登记的自定义项之间追加一条分隔条，用于给自定义项分组。
    // 开头、结尾以及连续多条分隔条会在弹出前被自动去掉，多加无害。
    void    AppendContextMenuSeparator();

    // 清空全部已登记的自定义项。默认菜单不受影响。
    void    ClearContextMenuItems();

    // ---- 滚动条 ----

    // 设置 / 读取垂直滚动条策略。默认 kScrollBarAuto。
    void            SetVScrollPolicy(ScrollBarPolicy policy);
    ScrollBarPolicy GetVScrollPolicy() const { return m_vScrollPolicy; }

    // 设置 / 读取水平滚动条策略。默认 kScrollBarAuto。
    // 注意开启自动换行时内容不会横向溢出，水平滚动条自然永远不出现。
    void            SetHScrollPolicy(ScrollBarPolicy policy);
    ScrollBarPolicy GetHScrollPolicy() const { return m_hScrollPolicy; }

    // 查询垂直滚动状态（像素）。
    //   nContentH：出参，内容总高度。
    //   nPos：出参，当前已向下滚动的偏移。
    //   nViewH：出参，可视区高度。
    //   返回：true 查询成功；引擎不可用时返回 false 且出参置零。
    bool    GetVScrollMetrics(int& nContentH, int& nPos, int& nViewH) const;

    // 把内容垂直滚动到指定像素偏移。
    //   nPos：目标偏移（0 = 顶部），超出范围时由引擎自行夹取。
    //   说明：引擎按行边界对齐，实际落点可能与 nPos 有一行以内的偏差。
    //         本方法内部会回读真实位置并同步滚动条，调用方不必自己补。
    void    SetVScrollPos(int nPos);

    // ---- 查找 ----

    // 在文档里查找下一处匹配。
    //   needle：要找的字符串；空串直接返回 false。
    //   nStartFrom：起始字符索引；传 -1 表示向前找时从当前选区末尾开始、
    //               向后找时从选区起点开始。
    //   bForward：true 向文档末尾方向找，false 向开头方向找。
    //   bMatchCase：是否区分大小写。
    //   bWholeWord：是否只匹配整个单词（"cat" 不会命中 "category"）。
    //   cpMinOut / cpMaxOut：出参，命中区间的起止字符索引；没命中时不改动。
    //   返回：true 命中；false 没找到。
    //   说明：本方法**只查找、不改变选区**。要连带选中请用 FindAndSelect。
    bool    FindText(LPCTSTR needle, long nStartFrom, bool bForward,
                     bool bMatchCase, bool bWholeWord,
                     long& cpMinOut, long& cpMaxOut) const;

    // 同 FindText，命中时顺带把选区移到命中处。
    //   bWrap：true 表示找到头（或尾）之后从另一端接着找一轮。
    //   返回：true 命中并已选中；false 没找到，选区不变。
    bool    FindAndSelect(LPCTSTR needle, long nStartFrom, bool bForward,
                          bool bMatchCase, bool bWholeWord, bool bWrap);

    // ---- 持久化 ----
    //
    // 两组接口对称：RTF 保留全部格式，纯文本只保内容。
    // 调用方可以按「要不要保留格式」在两者之间切换，用法完全一致。

    // 把当前文档序列化成 RTF 字节流。
    //   out：出参，RTF 内容。**按规范 RTF 是单字节编码**，所以是窄字符串，
    //        非英文字符会被写成转义序列，不会乱码。
    //   返回：true 成功；引擎不可用时返回 false 并清空出参。
    bool    SaveRTF(CStringA& out) const;

    // 用 RTF 字节流替换当前文档。
    //   in：RTF 内容。
    //   返回：true 成功；引擎不可用或内容非法时返回 false，原内容不受破坏。
    bool    LoadRTF(const CStringA& in);

    // 把当前文档序列化成纯文本（丢弃全部格式）。
    bool    SaveText(CString& out) const;

    // 用纯文本替换当前文档。
    bool    LoadText(const CString& in);

    // ---- 其它属性 ----

    // 设置 / 读取最大字符数。
    //   n：上限；<= 0 表示不限制。
    //
    // **约束的是用户输入与粘贴，不约束业务代码。** SetText / ReplaceSel /
    // AppendText 这些程序化写入照常生效，不会被截断 —— 与只读的取舍一致：
    // 这类限制是给用户设的，不是给调用方设的。业务需要自己截断时请在调用
    // 之前处理。
    void    SetMaxLength(int n);
    int     GetMaxLength() const { return m_nMaxLength; }

    // 密码模式：把每个字符都显示成同一个替代字符。
    //   b：true 开启。开启后内容读取不受影响，只是显示被遮蔽。
    void    SetPasswordMode(bool b);
    bool    IsPasswordMode() const;

    // 密码模式下用哪个字符遮蔽。默认是星号。
    void    SetPasswordChar(TCHAR ch);

    // 竖排文字。
    //   b：true 表示从上到下、从右到左排版（中日韩传统排版方式）。
    void    SetVertical(bool b);
    bool    IsVertical() const;

    // ---- 选区配色 ----

    // 覆盖选中文字的配色。默认跟随系统，与系统其它输入框保持一致。
    //   crBack / crText：任一传 CLR_INVALID 表示该项退回系统色。
    void    SetSelectionColors(COLORREF crBack, COLORREF crText);

    // ---- DuiControl 覆写 ----
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    void    Layout(const RECT& rcAvail) override;

    // 向布局体系报告期望尺寸。
    //   返回：自动增高关闭时返回 {0,0}（表示「随便，由父容器决定」）；
    //         打开时高度为内容高度加上边框与内边距，再夹到上下限之间，
    //         宽度恒为 0（本控件不主动要求宽度）。
    SIZE    GetDesiredSize() const override;
    bool    OnMouseMove(POINT pt, UINT mkFlags) override;
    bool    OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnLButtonUp(POINT pt, UINT mkFlags) override;
    bool    OnLButtonDblClk(POINT pt, UINT mkFlags) override;
    bool    OnChar(TCHAR ch) override;
    bool    OnKeyDown(UINT vk, UINT flags) override;
    bool    OnSetFocus() override;
    bool    OnKillFocus() override;

    // 本控件需要宿主窗口持有 Win32 键盘焦点 —— 它没有自己的窗口，字符、
    // 按键、输入法消息都得先投递到宿主窗口才能转下来。不声明的话，消息会
    // 一路投递给宿主的父窗口，控件一个字也收不到。
    //   返回：可聚焦时为 true；SetFocusable(false) 之后为 false。
    bool    NeedsWin32Focus() const override { return m_bFocusable; }

    // 本控件的拖放目标 —— 直接用引擎提供的那个，它自带落点高亮、插入位置
    // 跟随光标、保留格式等整套逻辑。
    //   返回：拖放被关掉时返回 nullptr，宿主的分发器据此跳过本控件。
    ::IDropTarget* GetDropTarget() override;
    bool    OnSetCursor(POINT pt) override;
    bool    OnMouseWheel(POINT pt, short zDelta, UINT mkFlags) override;
    bool    OnMouseLeave() override;
    bool    OnRButtonDown(POINT pt, UINT mkFlags) override;
    bool    OnRawMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult) override;

    // 宿主窗口 DPI 变化时调用。用的是库内默认字体时，按新 DPI 重新取一份交给引擎
    // 作默认字符格式；经 SetDefaultFontFromHFONT 设的字体由调用方负责，不替换。
    // 已经单独设过字号的文字保持原格式。
    //   dpi：新的 DPI（本实现经 GetDefaultFont 间接使用，不直接读取）。
    void    OnDpiChanged(int dpi) override;

    // ---- IDuiTextHostSite 实现（引擎反过来操作本控件的入口）----
    void    TxSiteInvalidate(const RECT* prc) override;
    void    TxSiteSetCapture(bool bCapture) override;
    void    TxSiteSetFocus() override;
    void    TxSiteScrollInfoChanged(bool bVertical) override;
    HRESULT TxSiteNotify(DWORD iNotify, void* pv) override;
    HWND    TxSiteGetHostHwnd() const override;

    // ---- 测试用 ----

    // 引擎是否就绪。构造成功后恒为 true；系统缺少引擎库时为 false。
    bool    Test_IsEngineReady() const;

    // 垂直滚动条当前是否可见（仅测试与排查用）。
    bool    Test_IsVScrollBarVisible() const;

    // 垂直滚动条当前的矩形（仅测试与排查用）。滚动条不存在时返回全零。
    RECT    Test_GetVScrollBarRect() const;

    // 文本区矩形（仅测试与排查用）。等于控件矩形去掉边框与内边距。
    RECT    Test_GetTextRect() const { return m_rcText; }

    // 问引擎「按当前宽度排版，全部内容有多高」的原始结果（像素）。
    // 与 GetDesiredSize() 的区别是不加边框内边距、也不夹上下限，用于在
    // 自动增高出问题时区分「测量本身不对」和「夹取或加边框那步不对」。
    // 量不出来时返回 0。
    int     Test_MeasureContentHeight() const;

    // 垂直滚动条当前的不透明度（仅测试与排查用）。
    //   返回：0 = 完全透明（自动隐藏的隐藏态），1 = 完全不透明。
    //         滚动条不存在时返回 -1。
    float   Test_GetVScrollBarAlpha() const;

    // 取引擎接口，供单元测试直接下发编辑命令、读回引擎内部状态。
    //   返回：引擎接口；引擎不可用时返回 nullptr。所有权归本控件，不要 Release。
    // 仅测试使用 —— 业务代码请走本类的公开方法，不要绕过去直接操作引擎，
    // 否则本控件缓存的状态会与引擎脱节。
    ITextServices* Test_GetTextServices() const;

    // 直接读写界面激活状态，用于在没有真窗口的测试里驱动焦点相关逻辑。
    void    Test_SetUiActive(bool b);

    // 当前该用哪种边框颜色（仅测试与排查用）。
    //
    // 颜色本身是内部常量，测试不该照抄一份数值来比对 —— 那样调色时得改两
    // 个地方，且改漏了测试还是绿的。所以用例的写法是「比较不同状态下取到的
    // 颜色互不相同、且优先级正确」，而不是断言具体的 RGB 值。
    COLORREF Test_GetBorderColor() const;

    // 按当前状态算出右键菜单模型，但**不弹出**菜单。
    //
    // 弹菜单那一步是同步阻塞的（要等用户点选或关闭），测试里不能真弹，
    // 所以留这个口子直接拿到「算出来的菜单」来做断言。返回的内容与真正
    // 弹出时看到的完全一致：默认项 + 自定义项，且已经过分隔条规整。
    std::vector<DuiRichEditMenuItem> Test_BuildContextMenu();

    // 直接执行一条右键菜单命令，等同于用户在菜单里点了这一项。
    // 供测试验证「命令分发」这一段，绕开阻塞的弹出流程。
    //   返回：内置命令执行完返回 true；不认识的编号返回 false
    //         （此时控件已经把它作为通知发给了父窗口）。
    bool    Test_InvokeContextMenuCommand(UINT nId);

protected:
    // 菜单弹出前的最后一道加工 —— 子类覆写它来定制菜单。
    //
    // 基类实现按控件当前状态填入默认菜单，再把 AppendContextMenuItem
    // 登记过的自定义项追加到末尾（中间自动插一条分隔条）。
    //
    // 子类想在默认菜单基础上增删改，就先调一次基类实现再改 items；
    // 想完全接管则不调基类、自己从空数组填起。
    //
    // 分隔条不必操心：本方法返回后控件会统一做一次规整，去掉开头、结尾
    // 以及连续重复的分隔条。
    //
    //   items：入参兼出参。传入时为空，返回时是最终要弹出的菜单模型。
    virtual void OnBuildContextMenu(std::vector<DuiRichEditMenuItem>& items);

    // 处理用户选中的菜单命令 —— 子类覆写它来响应自定义项。
    //
    // 基类实现负责全部内置命令（撤销 / 重做 / 剪切 / 复制 / 两种粘贴 /
    // 删除 / 全选），遇到不认识的编号返回 false。
    //
    // 控件在本方法返回 false 时，会把该编号作为通知
    // DUIN_RICHTEXT_MENUCOMMAND 发给父窗口。因此子类如果自己处理了某个
    // 自定义命令，务必返回 true，否则父窗口会再收到一次通知、命令被执行两遍。
    //
    //   nId：被选中项的命令编号。
    //   返回：已处理返回 true；未处理返回 false。
    virtual bool OnContextMenuCommand(UINT nId);

    // 边框颜色，随禁用 / 聚焦 / 悬停三态变化。
    //
    // 声明为虚函数是为了让子类换一套配色 —— 库里的普通输入框沿用的是它无
    // 窗口化之前的那套灰蓝色，与本控件的品牌蓝不同，靠覆写本方法区分，绘制
    // 代码只有一份。
    //
    //   返回：本次绘制边框该用的颜色。
    virtual COLORREF BorderColor() const;

    // 整体底色，禁用时与常态不同。
    //
    // 与 BorderColor 同理，声明为虚函数供子类替换配色。
    //
    //   返回：本次绘制背景该用的颜色。
    virtual COLORREF FillColor() const;

private:
    // 读取控件当前状态，作为构建菜单模型的输入。
    DuiRichEditMenuState CaptureContextMenuState() const;

    // 弹出右键菜单并执行用户选中的命令。同步阻塞到用户点选或关闭菜单。
    //   ptScreen：菜单左上角的期望位置，屏幕坐标。越界处理由 DuiMenu 内部
    //             负责，调用方直接传光标位置即可。
    //   返回：菜单确实弹出过返回 true；右键菜单被关掉、或菜单模型为空时
    //         返回 false（此时调用方应让消息继续走默认处理）。
    bool    ShowContextMenu(POINT ptScreen);

    // 把菜单模型交给 OnBuildContextMenu 加工并规整分隔条，得到最终模型。
    void    BuildContextMenuModel(std::vector<DuiRichEditMenuItem>& items);

#if BUI_FEATURE_IMAGEOLE
    // 取引擎的 OLE 接口。
    //   返回：接口指针，**调用方负责释放**；引擎不可用时返回空。
    IRichEditOle* AcquireRichEditOle() const;

    // 取文档 [cpMin, cpMax) 区间的文本（不含 cpMax）。
    // 只在两个图片对象之间取文本用，区间不会跨越图片占位符。
    CString GetTextRangeChars(long cpMin, long cpMax) const;
#endif // BUI_FEATURE_IMAGEOLE

    // 算出「用键盘唤出右键菜单」时菜单该落在哪里。
    //
    // 键盘唤出时没有鼠标位置可用，落点取当前文本插入光标的正下方 —— 放在
    // 光标正上会把用户正在编辑的那一行盖住。
    //   outScreen：出参，菜单左上角的期望位置，屏幕坐标。
    //   返回：算得出返回 true；控件还没挂进树、取不到宿主窗口时返回 false。
    bool    GetContextMenuAnchorFromCaret(POINT& outScreen) const;

    // 把控件矩形换算成文本区矩形（去掉边框与内边距）并推给引擎。
    void    UpdateTextRect();

    // 把文本插入光标画进后台缓冲。在绘制完文字之后调用。
    //
    // 光标由本控件自己画、闪烁相位由 DuiTextHost 控制，系统光标只用来报告位置，
    // 原因见 DuiTextHost.h「光标绘制」一节。画法是把光标矩形内的像素反色，
    // 与系统光标的观感一致，在任何背景色上都看得清。每次绘制都是在新画的
    // 文字上反色一次，反复重绘不会累积。
    //   hdc：绘制目标（宿主的后台缓冲）。
    //   rcDirty：本次需要重画的区域，宿主客户区坐标；光标只画在其中的部分。
    void    PaintCaret(HDC hdc, const RECT& rcDirty);

    // 把一次鼠标事件转发给引擎。
    //   uMsg：对应的窗口消息号。
    //   pt：宿主客户区坐标 —— 与交给引擎的文本区矩形同一套坐标，直接可用。
    //   mkFlags：按键修饰位。
    //   返回：true 表示引擎已处理（消费该事件）。
    bool    ForwardMouse(UINT uMsg, POINT pt, UINT mkFlags);

    // 把一条消息转发给引擎，并按引擎的返回值判断它有没有处理。
    //   返回：true 表示引擎处理了；false 表示它明确表示不处理，
    //         调用方应当让事件继续在 DUI 树里往上冒泡。
    bool    ForwardToEngine(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);

    // 把引擎当前的滚动状态同步到滚动条控件上，并按策略决定显隐。
    //
    // **只在安全时机调用**（布局、绘制入口），不要在引擎的回调栈里调 ——
    // 那样等于在引擎处理消息的过程中反过来查询引擎，属于不必要的重入。
    // 引擎的回调只负责把「该方向变了」记进标志位，真正的同步延后到这里。
    void    SyncScrollBars();

    // 问引擎「按当前宽度排版，刚好装下全部内容需要多高」。
    //
    // 与「滚动范围」是两回事：滚动范围只在内容溢出时才有值，内容装得下时
    // 恒为零；而本方法任何时候都给得出内容的真实高度。自动增高、以及
    // 「内容装得下时的内容高度」都靠它。
    //
    //   outHeight：出参，内容高度（像素）。
    //   返回：true 测量成功；引擎不可用或测量失败时返回 false 且出参为 0。
    bool    MeasureContentHeight(int& outHeight) const;

    // 内容变化后，判断是否需要让宿主重新排版一次，需要则发出请求。
    //
    // 自动增高关闭时什么也不做。打开时先量一遍新的期望高度，与控件当前高度
    // 相同就不必惊动宿主 —— 打字大多数时候只是往同一行里加字，高度并不变，
    // 每敲一个字都排一次整棵树是不必要的开销。
    void    RequestAutoGrowRelayout();

    // 给当前选区加上或去掉某个字符效果（粗体 / 斜体 / 下划线）。
    //   dwMask：要改动的属性位。
    //   dwEffect：对应的效果位。
    //   bOn：true 加上、false 去掉。
    void    ApplySelCharEffect(DWORD dwMask, DWORD dwEffect, bool bOn);

    // 查询当前选区的某个字符效果。
    //   bOn：出参，该效果是否开启。
    //   返回：true 表示整个选区一致、出参有意义；false 表示选区内不一致
    //         （既有粗体又有非粗体），此时出参无意义。
    bool    QuerySelCharEffect(DWORD dwMask, DWORD dwEffect, bool& bOn) const;

    // 按当前是否自动换行，组合出告诉引擎的滚动条配置并下发。
    // 换行开关变化时必须调用 —— 开着换行时横向不会溢出，不必让引擎维护
    // 水平滚动范围。
    void    UpdateEngineScrollBars();

    // 按策略与内容是否溢出，决定某个方向的滚动条该不该可见。
    //   policy：该方向的策略。
    //   bOverflow：内容是否超出可视区。
    //   返回：true 表示应当显示。
    static bool ShouldShowScrollBar(ScrollBarPolicy policy, bool bOverflow);

    // 滚动条被用户拖动 / 点击后的回调（C 风格，与滚动条控件的接口一致）。
    // 两个方向各一个，这样回调里不必再去分辨是哪一条动了。
    //   user：注册时传入的本控件指针。
    //   newPos：滚动条的新位置（像素）。
    static void OnVScrollBarMoved(void* user, int newPos);
    static void OnHScrollBarMoved(void* user, int newPos);

    // 把某个方向的新位置写回引擎，再回读真实位置同步滚动条。
    //   bVertical：true 为垂直方向。
    //   nPos：目标位置（像素）。
    void    ApplyScrollPos(bool bVertical, int nPos);

private:
    DuiTextHost* m_pTextHost;       // 引擎宿主；构造时创建，引用计数对象，不能直接 delete

    RECT     m_rcText;              // 文本区矩形（控件矩形去掉边框与内边距），宿主客户区坐标、像素

    COLORREF m_crBackground;        // 背景色
    COLORREF m_crText;              // 默认文字色
    bool     m_bShowBorder;         // 是否画 1 像素外边框
    int      m_nMarginLeft;         // 左内边距（像素）
    int      m_nMarginTop;          // 上内边距（像素）
    int      m_nMarginRight;        // 右内边距（像素）
    int      m_nMarginBottom;       // 下内边距（像素）

    CString  m_strPlaceholder;      // 占位文字；空文本且未聚焦时绘制
    bool     m_bFocusable;          // 是否接受键盘焦点
    bool     m_bPastePlainText;     // 是否把所有粘贴都当作纯文本粘贴
    int      m_nMaxLength;          // 最大字符数；<= 0 表示不限制

    // 引擎提供的拖放目标，首次用到时才向引擎索取并缓存。持有一份引用，
    // 析构时释放。
    ::IDropTarget* m_pDropTarget;
    // 是否已经向宿主请求过打开拖放分发。只请求一次，避免每次布局都调。
    bool     m_bDropDispatchRequested;

    bool     m_bAutoGrow;           // 是否随内容自动增高
    int      m_nAutoGrowMin;        // 自动增高下限（像素）；<= 0 表示按一行高度
    int      m_nAutoGrowMax;        // 自动增高上限（像素）；<= 0 表示不限
    HFONT    m_hDefaultFont;        // 调用方注入的默认字体；不持有所有权，可为空

    //是否响应右键弹出菜单。默认 true。
    bool     m_bContextMenuEnabled;
    //调用方通过 AppendContextMenuItem / AppendContextMenuSeparator 登记的
    //自定义菜单项，按登记顺序保存。弹出时追加到默认菜单末尾。
    //生命周期与控件相同，控件析构时随之释放。
    std::vector<DuiRichEditMenuItem> m_customMenuItems;

    // 上一次向宿主请求重排时，本控件的宽度与所要求的高度（像素）。
    //
    // 用来防止死循环。父容器完全可以不采纳本控件报的期望高度 —— 最典型的
    // 情形是布局提示写成了固定高度，而自动增高又是默认打开的。那时每排一次
    // 版，本控件都会发现「实际高度不等于期望高度」，如果无条件再请求一次重排，
    // 就会在「请求重排 → 重排 → 高度仍不等 → 再请求」之间无限打转。
    //
    // 记下上一次请求的宽度与高度之后，就能区分两种情况：期望高度或宽度确实
    // 变了（内容变多、窗口变宽），那是新情况，值得再请求一次；两者都没变，
    // 说明父容器就是不采纳，再请求也是同样的结果，到此为止。
    // 父容器采纳了期望高度时这两个值被清零，回到初始状态。
    int      m_nLastGrowReqW;
    int      m_nLastGrowReqH;

    // 滚动状态是否需要重新同步。引擎在回调里置起，控件在安全时机（下一次
    // 绘制或布局）再去查询并更新滚动条。这样做是为了不在引擎的回调栈里
    // 反过来查询引擎，避免不必要的重入。
    bool     m_bScrollDirtyV;
    bool     m_bScrollDirtyH;

    ScrollBarPolicy m_vScrollPolicy;   // 垂直滚动条策略
    ScrollBarPolicy m_hScrollPolicy;   // 水平滚动条策略

#if BUI_FEATURE_SCROLLBAR
    // 两个方向的滚动条。**作为子控件持有**（所有权在基类的子控件列表里，
    // 这里只是裸指针），因此能自动参与命中测试 —— 拖动滑块时鼠标事件会
    // 先命中滚动条而不是文本区。
    //
    // 摆放上是**覆盖式**：布局时压在文本区之上，不占内容宽度。这与库内
    // 列表等控件的内嵌式摆法不同，是本控件特有的 —— 之所以能这么做，
    // 是因为滚动条隐藏时完全不绘制，显示时又带透明度，浮在文字上不突兀。
    DuiScrollBar* m_pScrollV;
    DuiScrollBar* m_pScrollH;
#endif
};

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
