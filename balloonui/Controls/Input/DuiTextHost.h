/**
 *  DuiTextHost —— 无窗口文字排版引擎的宿主实现（ITextHost）。
 *  balloonwj@qq.com   2026-08-14
 */

#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHTEXT

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiTextHost —— 排版引擎的宿主端
// =================================================================
//
// 用途：实现系统排版引擎要求的宿主接口，让引擎能在没有任何窗口的情况下
// 工作。使用者是无窗口富文本控件 DuiRichEdit。
//
// ─────────────────────────────────────────────────────────────────
// 一、先理解方向：这个类是被回调的一方
// ─────────────────────────────────────────────────────────────────
//
// 引擎与宿主之间有两个方向相反的接口：
//
//     控件 DuiRichEdit ──调用──▶ ITextServices（引擎实现）
//     控件 DuiRichEdit ◀─回调── ITextHost（本类实现）
//
// 本类是**被回调**的那一侧。它的三十九个方法没有一个是我们主动调用的，
// 全部由引擎在它需要的时候找上门来。因此读这个类的正确方式不是"这个函数
// 什么时候被执行"，而是"引擎在什么情况下会来问这个问题、我们答什么、
// 答错了会怎样"——每个方法的实现处都按这四点写了注释。
//
// ─────────────────────────────────────────────────────────────────
// 二、为什么不直接持有控件指针
// ─────────────────────────────────────────────────────────────────
//
// 参考实现（SOUI）的做法是让宿主实现直接握着控件对象，问什么都转手去问
// 控件。本类没有照搬，而是拆成两半：
//
//   · **数据自己存。** 客户区矩形、内边距、默认字符格式、默认段落格式、
//     属性位、背景样式、长度上限、密码字符、滚动条种类——引擎问的这些，
//     本类用自己的成员变量直接回答。控件通过下面的 SetXxx 系列把值推
//     进来，本类顺带负责通知引擎"这几项变了"。
//   · **动作走回调接口。** 需要反过来操作控件的那几件事（重绘、捕获、
//     焦点、滚动条、事件通知、取宿主窗口句柄）收敛成 IDuiTextHostSite
//     这个只有六个方法的小接口。
//
// 这样拆有三个好处：
//   1. **本类可以完全独立地创建和测试。** 不接控件（回调接口传空）也能
//      把引擎创建出来、灌文本、排版、测量尺寸。无窗口路线最关键的那条
//      性质——引擎不需要任何窗口就能工作——因此可以在控件写出来之前就
//      验证，也让绝大部分文本类测试不必搭建真窗口。
//   2. 引擎生命周期与控件生命周期解耦。本类是 COM 对象、由引擎的引用
//      计数决定何时销毁；控件由 DUI 树决定。控件先走一步时，把回调接口
//      置空即可，不会出现"控件已析构、引擎还在回调"的悬空访问。
//   3. 那六个回调方法一眼就能看全"引擎到底会反过来动控件哪些东西"，
//      比在三十九个方法里逐个找清楚得多。
//
// ─────────────────────────────────────────────────────────────────
// 三、单位与坐标：本类最容易出错的地方
// ─────────────────────────────────────────────────────────────────
//
// 引擎的接口在单位上并不统一，相邻的两个方法可能要求不同的单位：
//
//   · **像素**：绝大多数方法，包括客户区矩形、光标位置、失效矩形、
//     滚动位置。
//   · **百分之毫米**：只有取内边距和取范围这两个方法。
//   · **一英寸的一千四百四十分之一**：字符格式里的字号、段落格式里的
//     制表位与缩进。
//
// 本类的成员变量按"存进来时是什么单位就叫什么名字"来命名，凡是非像素的
// 一律在变量名和注释里点明，换算集中在 SetViewInset / SetExtent 两处发生，
// 别处一律像素。搞混的症状是文字内边距大得离谱或者是负的，而且不崩溃、
// 不报错，只能靠眼睛发现。
//
// 坐标方面只有一套：**宿主窗口的客户区坐标**。balloonui 的控件矩形本来
// 就用这套坐标，我们原样交给引擎，于是引擎回传的失效矩形、光标位置、
// 命中坐标也都是这套，前后一致、无需换算。
//
// ─────────────────────────────────────────────────────────────────
// 四、代码用法
// ─────────────────────────────────────────────────────────────────
//
//     // 控件侧（DuiRichEdit 同时实现 IDuiTextHostSite）：
//     // 构造出来时引用计数就是 1，那一份归创建者所有，不需要再 AddRef。
//     m_pTextHost = new DuiTextHost();
//     if (!m_pTextHost->Init(this))          // 传 nullptr 也能成功，见上文
//     {
//         m_pTextHost->Release();
//         m_pTextHost = nullptr;
//         return false;
//     }
//     ITextServices* pSvc = m_pTextHost->GetTextServices();
//
//     // 控件尺寸变化时：
//     m_pTextHost->SetClientRect(rcText);    // 内部会通知引擎重新排版
//
//     // 控件销毁时：
//     m_pTextHost->Shutdown();               // 断开与控件的联系、反激活引擎
//     m_pTextHost->Release();                // 让出我们持有的那一份引用
//     m_pTextHost = nullptr;                 // 真正的销毁时机由引擎决定
//
// XML 用法：N/A（不是控件）。

#include <windows.h>
#include <ole2.h>
#include <richedit.h>
#include <textserv.h>
#include "../../BalloonUiApi.h"
#include "../../DuiCaret.h"

namespace balloonwjui {

// =================================================================
// IDuiTextHostSite —— 引擎需要反过来操作控件的那几件事
// =================================================================
//
// 由控件（DuiRichEdit）实现。DuiTextHost 只在引擎明确要求时才调用它，
// 且每次调用前都会判空，因此实现方可以放心地在控件析构前把自己摘掉。
//
// 之所以只有六个方法：引擎的三十九个回调里，其余的都是"问数据"，由
// DuiTextHost 自己的成员变量回答，不必惊动控件。真正需要控件动手的只有
// 这六类，把它们单列出来，"引擎会反过来动控件什么"就一目了然。
class BUI_API IDuiTextHostSite
{
public:
    virtual ~IDuiTextHostSite() {}

    // 请求重绘。
    //   prc：需要重绘的矩形（宿主客户区坐标，像素）；为 nullptr 表示整个
    //        控件都要重绘。实现方可据此做局部失效以减少重绘面积。
    virtual void TxSiteInvalidate(const RECT* prc) = 0;

    // 请求获取或释放鼠标捕获。
    //   bCapture：true 表示引擎希望在拖动选区期间持续收到鼠标消息，
    //             false 表示释放。
    virtual void TxSiteSetCapture(bool bCapture) = 0;

    // 请求把键盘焦点交给本控件。引擎在用户点击文本区等场合要求这件事。
    virtual void TxSiteSetFocus() = 0;

    // 某个方向的滚动状态发生了变化，请实现方重新查询并同步自己的滚动条控件。
    //   bVertical：true 为垂直方向、false 为水平方向。
    //
    // 这里刻意**只发信号、不带数据**。因为本方法是在引擎的回调栈里被调用的，
    // 此刻反过来查询引擎的滚动状态属于重入调用，风险不必要；改成让实现方
    // 收到信号后在自己的时机调 DuiTextHost::QueryScrollInfo 去取，调用栈就
    // 干净了。实现方在这里应当只做记录或投递，不要做重活。
    virtual void TxSiteScrollInfoChanged(bool bVertical) = 0;

    // 引擎的事件通知（内容变化、链接点击、选区变化等）。
    //   iNotify：通知码，取值为 richedit.h 里的 EN_ 系列。
    //   pv：随通知码而定的附加数据，可能为 nullptr；所有权归引擎，只读。
    //   返回：多数通知返回 S_OK 即可；少数通知返回非 S_OK 表示宿主否决
    //         这次操作。
    virtual HRESULT TxSiteNotify(DWORD iNotify, void* pv) = 0;

    // 取宿主窗口句柄。光标与输入法都需要一个真窗口才能工作，控件自己
    // 没有窗口，只能借 DUI 宿主窗口的。
    //   返回：宿主窗口句柄；控件尚未挂进 DUI 树时返回 nullptr，此时
    //         DuiTextHost 会跳过所有依赖窗口的操作。
    virtual HWND TxSiteGetHostHwnd() const = 0;
};

// =================================================================
// DuiTextHost —— ITextHost 的实现
// =================================================================
class BUI_API DuiTextHost : public ITextHost
{
public:
    DuiTextHost();

    // 析构由引用计数触发，外部不要直接 delete。
    virtual ~DuiTextHost();

    DuiTextHost(const DuiTextHost&) = delete;
    DuiTextHost& operator=(const DuiTextHost&) = delete;

    // ---- 生命周期 ----

    // 创建引擎对象并就地激活。
    //   pSite：控件侧的回调接口。**允许传 nullptr** —— 此时引擎照样能
    //          创建并完成排版、测量等纯文本工作，只是所有需要操作控件的
    //          回调（重绘、捕获、焦点、滚动条、通知）都被静默丢弃。
    //          单元测试与"控件尚未挂进树"的场景都走这条路。
    //          所有权不转移，调用方须保证其生命周期长于本对象的使用期。
    //   返回：true 创建成功；false 表示引擎库不可用或创建失败。
    bool Init(IDuiTextHostSite* pSite);

    // 断开与控件的联系并反激活引擎。控件析构前必须调用。
    // 调用后本对象仍然存活（引擎可能还持有引用），但所有回调都不再触及
    // 控件，因此不会出现悬空访问。幂等。
    void Shutdown();

    // 取引擎接口。Init 成功前返回 nullptr。所有权归本对象，调用方不要 Release。
    ITextServices* GetTextServices() const { return m_pServices; }

    // 引擎是否已就绪（Init 成功且尚未 Shutdown）。
    bool IsReady() const { return m_pServices != nullptr; }

    // 当前存活的本类实例个数（仅测试与排查用）。
    //
    // 存在的理由是验证「不存在引用环」：本对象把自己交给引擎当宿主，引擎
    // 又被本对象持有。如果引擎在保存宿主指针时增加了引用计数，两者就会互相
    // 拖住谁也释放不掉 —— 那是一种不报错、不崩溃的内存泄漏，只有数实例个数
    // 才看得见。测试的做法是：记下基准值，构造再销毁一个控件，个数应当回到
    // 基准值。
    static int Test_GetLiveInstanceCount();

    // ---- 控件把状态推进来（这些值由本对象保管，供引擎回来问）----

    // 是否显示文本插入光标。默认 true。
    //
    //   b：false 表示不显示那个闪烁的光标。控件的其余行为**完全不变** ——
    //      照常接受焦点、照常能点击定位、拖选、滚动。它管的纯粹是视觉。
    //   副作用：光标的显示状态因此变化时，失效光标所在的矩形并启停闪烁定时器。
    //
    // 关掉之后光标对象仍然创建、位置仍然跟踪，只是不画 —— 输入法候选窗的
    // 落点是按光标位置算的，保留跟踪才能保证在可编辑控件上关掉光标显示时，
    // 输入法不会因此错位。重新打开时，只要引擎仍要求显示光标，立即恢复绘制。
    void SetShowCaret(bool b);
    bool IsShowCaret() const { return m_bShowCaret; }

    // 设置文本区矩形。
    //   rc：宿主客户区坐标、像素。应当是控件矩形去掉边框与内边距之后的部分。
    //   副作用：内部会换算出引擎要的范围值，并通知引擎重新排版。
    void SetClientRect(const RECT& rc);
    RECT GetClientRect_() const { return m_rcClient; }

    // 设置文字与文本区边界之间的内边距。
    //   left / top / right / bottom：像素。内部换算成引擎要的百分之毫米单位。
    //   副作用：通知引擎内边距已变。
    void SetViewInsetPixels(int left, int top, int right, int bottom);

    // 设置默认字符格式（字体、字号、颜色、粗斜体等）。
    //   cf：调用方填好的字符格式；本对象整份拷贝一份保存。
    //   副作用：通知引擎默认字符格式已变，已有文字会按新格式重排。
    void SetDefaultCharFormat(const CHARFORMAT2W& cf);

    // 用一个字体句柄和一种颜色填充默认字符格式，省去调用方自己拼结构。
    //   hFont：字体句柄；所有权归调用方，本对象只读取它的属性。
    //          传 nullptr 时本方法不做任何事。
    //   crText：文字颜色。
    //   副作用：同 SetDefaultCharFormat。
    void SetDefaultFont(HFONT hFont, COLORREF crText);

    // 设置默认段落格式（对齐、缩进、制表位等）。
    //   pf：调用方填好的段落格式；本对象整份拷贝一份保存。
    //   副作用：通知引擎默认段落格式已变。
    void SetDefaultParaFormat(const PARAFORMAT2& pf);

    // 设置属性位（多行、只读、自动换行、密码、竖排等）。
    //   dwMask：本次要修改哪几位。
    //   dwBits：这几位的新值。
    //   副作用：更新内部记录后立即通知引擎，两步缺一不可 —— 只改记录
    //           不通知的话引擎不会主动来问，表现为"改了没反应"。
    void SetPropertyBits(DWORD dwMask, DWORD dwBits);
    DWORD GetPropertyBits_() const { return m_dwPropertyBits; }

    // 设置背景样式。
    //   bTransparent：true 表示引擎不擦背景、由控件自己画（支持圆角、
    //                 半透明、渐变等效果，是本控件相对真窗口控件的一项
    //                 关键优势）；false 表示引擎自己用不透明方式擦背景。
    //   副作用：通知引擎背景样式已变。
    void SetBackTransparent(bool bTransparent);

    // 设置文本长度上限。
    //   nMax：最大字符数；<= 0 表示按引擎默认上限处理。
    //   副作用：通知引擎长度上限已变。
    void SetMaxLength(int nMax);

    // 设置密码字符（属性位里启用密码显示时才生效）。
    void SetPasswordChar(TCHAR ch);

    // 设置引擎应当认为存在哪些滚动条。
    //   dwScrollBars：WS_VSCROLL / WS_HSCROLL / ES_AUTOVSCROLL /
    //                 ES_AUTOHSCROLL / ES_DISABLENOSCROLL 的组合。
    //   副作用：通知引擎滚动条配置已变。
    void SetScrollBars(DWORD dwScrollBars);

    // 设置选区配色。
    //   crBack / crText：选中文字的背景色与前景色。任一为 CLR_INVALID
    //                    时该项退回系统主题色（默认行为）。
    //   说明：默认跟随系统，与系统其它输入框保持一致；需要统一视觉的
    //         调用方可用本方法覆盖。
    void SetSelectionColors(COLORREF crBack, COLORREF crText);

    // ---- 界面激活状态 ----
    //
    // 控件获得 / 失去键盘焦点时调用。这个状态直接决定光标显不显示、
    // 选区高不高亮。**必须如实维护** —— 引擎有时会在控件并无焦点时要求
    // 显示光标（例如它刚收到一条设置文本的命令），本对象靠这个标志把
    // 那类请求挡回去，否则会出现一个没有焦点的输入框在闪光标。
    void SetUiActive(bool bActive);
    bool IsUiActive() const { return m_bUiActive; }

    // 取内部的系统光标对象。控件在需要时（如取光标位置定位右键菜单）可直接
    // 读取它。注意系统光标只用来报告位置，用空白位图创建，在屏幕上不留像素，
    // 界面上的光标由控件自己画，见下方「光标绘制」。
    DuiCaret& GetCaret() { return m_caret; }

    // ---- 光标绘制 ----
    //
    // 界面上看到的光标由控件自己画进后台缓冲，闪烁也由本对象用定时器控制；
    // 系统光标仍然创建并跟随插入点（输入法候选窗、读屏与放大镜一类辅助工具
    // 都靠它定位），但用一张全零的单色位图创建，在屏幕上不留任何像素。
    //
    // 为什么不直接显示系统光标：系统光标靠在屏幕上反色来闪烁，而 balloonui
    // 是先画进后台缓冲、再整块拷上屏。引擎在 TxDraw 期间（宿主 BeginPaint
    // 之后、拷上屏之前）会隐藏再显示一次光标，屏幕上的光标像素于是与系统记录
    // 的亮灭状态错位，表现为光标一会儿只亮上半段、一会儿只亮下半段
    // （2026-09-30 在客户端资料窗的「备注名」文本框上发现，实测 8 次复现 4 次）。
    //
    // 为什么还要用空白位图，而不是「只建不显示」就够了：排版引擎（msftedit.dll）
    // 的导入表里直接有 ShowCaret / HideCaret / CreateCaret 等系统光标接口，它会
    // 绕过 TxShowCaret 自己操作系统光标。实测获得焦点或打字之后的第一次绘制若
    // 发生在宿主的 BeginPaint 期间，EndPaint 之后系统光标就处于显示状态，而宿主
    // 从头到尾没有调用过 ShowCaret。这是引擎内部的行为，宿主拦不住；用空白位图
    // 建光标之后，不管它被谁显示出来，屏幕上都没有变化。引擎自己在有选区时就是
    // 用全零位图让光标「存在但看不见」的，这里是同一个办法。

    // 控件在调用引擎的 TxDraw 之前调用。
    //
    // 引擎是按「光标直接画在屏幕上」的模型写的：绘制文字之前先隐藏光标，画完
    // 再显示，有时还会重设一次位置。对后台缓冲来说这一对操作毫无意义，若照常
    // 处理，每次绘制都会失效光标区域并把闪烁相位重置为亮 —— 既会引发下一次
    // 绘制，又会让光标永远停在亮的相位上不再闪烁。因此在 BeginDraw 与 EndDraw
    // 之间，引擎的光标请求只记录，不触发任何重绘。
    // 不支持嵌套：已处于绘制期间时再调用不产生任何效果。
    void BeginDraw();

    // 控件在引擎的 TxDraw 返回之后调用，与 BeginDraw 配对。
    //   副作用：若绘制期间光标的最终状态（显示与否、位置、尺寸）与开始时不同，
    //           失效新旧两处光标矩形并重新开始闪烁计时；相同则什么都不做。
    void EndDraw();

    // 取此刻应当画出的光标矩形。控件在绘制完文字之后调用，把返回的矩形画出来。
    //   outRc：出参，光标矩形，宿主客户区坐标、像素；返回 false 时置为空矩形。
    //          调用方还应当把它裁剪到文字区内再画。
    //   返回：true 表示此刻应当画光标 —— 控件持有焦点、引擎要求显示光标、
    //         调用方没有关闭光标显示、且闪烁正处于亮的相位；否则返回 false。
    bool GetCaretPaintRect(RECT& outRc) const;

    // 光标的形状位图。
    //   返回：引擎在 TxCreateCaret 里给出位图时，本对象复制下来的那一份；引擎要的
    //         是实心竖线光标时返回 nullptr。所有权归本对象，调用方只读、不得删除，
    //         也不要长期保存（下次重建光标时会被替换）。
    //   用法：非空时控件应当按位图的形状反色 —— 位图里置位的像素取反，未置位的
    //         不动，与系统画位图光标的方式一致。引擎在有选区时给的就是一张全空白
    //         的位图，用来让光标「存在但看不见」，照实心矩形去画就会画出一块色块。
    HBITMAP GetCaretPaintBitmap() const { return m_hCaretPaintBmp; }

    // ---- 拖放 ----

    // 取引擎提供的拖放目标。
    //
    // 引擎自己实现了一整套「把文字拖进文档」的逻辑（落点高亮、插入位置
    // 跟随光标、拖入后保留格式），我们只要把这个对象接到窗口的拖放机制上
    // 即可，不需要自己解析拖放数据。
    //
    //   返回：拖放目标接口；**引用计数已加一，调用方用完要 Release**。
    //         引擎不可用时返回 nullptr。
    IDropTarget* CreateEngineDropTarget();

    // ---- 输入法 ----

    // 把输入法的组字窗（候选条）定位到当前光标处。
    //
    // 为什么需要显式定位：候选条的位置由输入法自己决定，它的依据有两个
    // ——系统光标的位置，以及宿主主动设置的组字窗位置。只靠前者在多数
    // 输入法上够用（本控件建了真的系统光标，见 DuiCaret），但并非全部
    // 输入法都以它为准；显式设置一次是更稳妥的做法，代价也极小。
    //
    // 调用时机：收到输入法的开始组字与组字中这两条消息时各调一次。
    // 不必在每次光标移动时都调 —— 没有组字过程时设了也没有意义。
    //
    //   返回：true 表示已设置；宿主窗口不可用或取不到输入法上下文时
    //         返回 false（此时不影响输入，只是候选条位置可能不准）。
    bool UpdateImeCompositionPos();

    // ---- 滚动 ----

    // 查询某个方向的滚动状态。控件收到 TxSiteScrollInfoChanged 信号后调用。
    //   bVertical：true 查垂直方向、false 查水平方向。
    //   nMin / nMax：出参，滚动范围（像素）。
    //   nPage：出参，可视区大小（像素）。
    //   nPos：出参，当前位置（像素）。
    //   bEnabled：出参，该方向当前是否可滚动（内容是否溢出）。
    //   返回：true 查询成功；引擎尚未就绪时返回 false 且出参全部置零。
    bool QueryScrollInfo(bool bVertical, int& nMin, int& nMax,
                         int& nPage, int& nPos, bool& bEnabled);

    // 把滚动位置写回引擎（用户拖动滚动条时由控件调用）。
    //   bVertical：true 为垂直方向、false 为水平方向。
    //   nPos：目标位置（像素）。
    //   返回：true 已下发；引擎未就绪或正处于滚动通知过程中时返回 false。
    //   说明：引擎会按行边界对齐，实际落点可能与 nPos 有一行以内的偏差，
    //         调用方应当在本方法返回后再 QueryScrollInfo 回读一次真实位置
    //         来同步滚动条，否则滑块会与内容逐渐错位。
    bool SetScrollPos(bool bVertical, int nPos);

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG   STDMETHODCALLTYPE AddRef() override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // ---- ITextHost：设备上下文 ----

    // 引擎需要一个设备上下文来度量文字时调用。返回屏幕设备上下文即可 ——
    // 引擎只拿它查询字体度量，不会往上面画东西。
    HDC  TxGetDC() override;
    // 与上一个方法配对归还。
    INT  TxReleaseDC(HDC hdc) override;

    // ---- ITextHost：滚动条 ----

    // 引擎要求显示 / 隐藏某个方向的滚动条。
    BOOL TxShowScrollBar(INT fnBar, BOOL fShow) override;
    // 引擎要求启用 / 禁用某个方向的滚动条。
    BOOL TxEnableScrollBar(INT fuSBFlags, INT fuArrowflags) override;
    // 引擎告知某个方向的滚动范围（像素）。
    BOOL TxSetScrollRange(INT fnBar, LONG nMinPos, INT nMaxPos, BOOL fRedraw) override;
    // 引擎告知某个方向的当前滚动位置（像素）。
    BOOL TxSetScrollPos(INT fnBar, INT nPos, BOOL fRedraw) override;

    // ---- ITextHost：重绘 ----

    // 引擎要求把指定矩形标记为需要重画。
    void TxInvalidateRect(LPCRECT prc, BOOL fMode) override;
    // 引擎告知视图内容已变，要求重画。
    void TxViewChange(BOOL fUpdate) override;
    // 引擎希望用位块搬移加速滚动。本实现退化为整块重画，正确性不受影响。
    void TxScrollWindowEx(INT dx, INT dy, LPCRECT lprcScroll, LPCRECT lprcClip,
                          HRGN hrgnUpdate, LPRECT lprcUpdate, UINT fuScroll) override;

    // ---- ITextHost：光标 ----

    // 引擎要求创建一个指定尺寸的光标。建出系统光标（不显示），并记下绘制尺寸。
    BOOL TxCreateCaret(HBITMAP hbmp, INT xWidth, INT yHeight) override;
    // 引擎要求显示 / 隐藏光标。未处于界面激活状态时拒绝显示。只记录引擎的要求，
    // 由控件据此自己绘制，本对象不调用 ShowCaret。
    BOOL TxShowCaret(BOOL fShow) override;
    // 引擎告知光标应当移动到的位置（宿主客户区坐标、像素）。系统光标照常移动，
    // 供输入法定位候选窗。
    BOOL TxSetCaretPos(INT x, INT y) override;

    // ---- ITextHost：定时器 ----

    // 引擎要求一个定时器（用于光标闪烁、拖选到边缘时自动滚动等）。
    BOOL TxSetTimer(UINT idTimer, UINT uTimeout) override;
    // 引擎要求撤销之前的定时器。
    void TxKillTimer(UINT idTimer) override;

    // ---- ITextHost：捕获、焦点、鼠标指针、坐标 ----

    // 引擎要求获取 / 释放鼠标捕获（拖选文字期间需要）。
    void TxSetCapture(BOOL fCapture) override;
    // 引擎要求把键盘焦点交给本控件。
    void TxSetFocus() override;
    // 引擎要求改变鼠标指针形状（文本区内显示为竖线形）。
    void TxSetCursor(HCURSOR hcur, BOOL fText) override;
    // 屏幕坐标转宿主客户区坐标。
    BOOL TxScreenToClient(LPPOINT lppt) override;
    // 宿主客户区坐标转屏幕坐标。
    BOOL TxClientToScreen(LPPOINT lppt) override;

    // ---- ITextHost：激活状态 ----

    // 引擎要求宿主进入激活状态。
    HRESULT TxActivate(LONG* plOldState) override;
    // 引擎要求宿主退出激活状态。
    HRESULT TxDeactivate(LONG lNewState) override;

    // ---- ITextHost：几何与格式（引擎回来问数据）----

    // 引擎在每次重新排版前询问可用的排版区域。单位是**像素**。
    HRESULT TxGetClientRect(LPRECT prc) override;
    // 引擎询问文字与排版区域边界之间的内边距。单位是**百分之毫米**。
    HRESULT TxGetViewInset(LPRECT prc) override;
    // 引擎询问默认字符格式。返回内部成员的地址，所有权归本对象。
    HRESULT TxGetCharFormat(const CHARFORMATW** ppCF) override;
    // 引擎询问默认段落格式。返回内部成员的地址，所有权归本对象。
    HRESULT TxGetParaFormat(const PARAFORMAT** ppPF) override;
    // 引擎询问某个系统颜色。选区配色被覆盖时在此拦截。
    COLORREF TxGetSysColor(int nIndex) override;
    // 引擎询问背景应当由谁来擦。
    HRESULT TxGetBackStyle(TXTBACKSTYLE* pstyle) override;
    // 引擎询问文本长度上限。
    HRESULT TxGetMaxLength(DWORD* plength) override;
    // 引擎询问应当认为存在哪些滚动条。
    HRESULT TxGetScrollBars(DWORD* pdwScrollBar) override;
    // 引擎询问密码显示时用哪个字符。
    HRESULT TxGetPasswordChar(TCHAR* pch) override;
    // 引擎询问助记符字符的位置。本控件不使用助记符，恒返回 -1。
    HRESULT TxGetAcceleratorPos(LONG* pcp) override;
    // 引擎询问排版区域的整体范围。单位是**百分之毫米**。
    HRESULT TxGetExtent(LPSIZEL lpExtent) override;
    // 引擎通知默认字符格式已变（由引擎侧发起的变更）。
    HRESULT OnTxCharFormatChange(const CHARFORMATW* pCF) override;
    // 引擎通知默认段落格式已变（由引擎侧发起的变更）。
    HRESULT OnTxParaFormatChange(const PARAFORMAT* pPF) override;
    // 引擎询问一批属性位的当前值。
    HRESULT TxGetPropertyBits(DWORD dwMask, DWORD* pdwBits) override;
    // 引擎发出事件通知（内容变化、链接点击等）。
    HRESULT TxNotify(DWORD iNotify, void* pv) override;
    // 引擎索取输入法上下文。借宿主窗口的。
    HIMC TxImmGetContext() override;
    // 与上一个方法配对归还。
    void TxImmReleaseContext(HIMC himc) override;
    // 引擎询问左侧整行选择条的宽度。本控件不使用该功能，恒返回 0。
    HRESULT TxGetSelectionBarWidth(LONG* lSelBarWidth) override;

private:
    // 把某几个属性位的变化通知给引擎。引擎不会主动来问，必须由我们告知。
    void NotifyPropertyChange(DWORD dwMask, DWORD dwBits);

    // 按当前客户区尺寸重算引擎要的范围值（百分之毫米），并通知引擎重排。
    void RecalcExtent();

    // 取当前屏幕的每英寸点数，用于像素与百分之毫米之间的换算。
    // 取不到时返回系统默认值，保证换算不会除零。
    static void GetDevicePixelsPerInch(int& outX, int& outY);

    // 定时器到期的静态回调。线程定时器不绑窗口，系统通过本函数回调，
    // 再由内部的登记表找回对应的实例。引擎的定时器转交引擎，光标闪烁定时器
    // 由本对象自己处理。
    static void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

    // 光标若处于亮的相位，此刻应占的矩形（不考虑闪烁相位）。
    //   outRc：出参，宿主客户区坐标、像素；光标此刻不该显示（控件没有焦点、
    //          引擎要求隐藏、调用方关闭了光标显示、或系统光标尚未创建）时置为空矩形。
    void GetCaretVisibleRect(RECT& outRc) const;

    // 光标状态可能已经变化之后调用：与变化之前的矩形比较，确有变化时失效新旧
    // 两处矩形，并按新状态重新开始或停止闪烁。
    //   rcBefore：变化之前由 GetCaretVisibleRect 取得的矩形。
    //   说明：处于 BeginDraw / EndDraw 之间时直接返回，由 EndDraw 统一比较。
    void OnCaretMaybeChanged(const RECT& rcBefore);

    // 切到亮的相位并从头开始闪烁计时。光标刚显示或刚移动时调用 —— 插入点一动
    // 光标就要立刻亮出来，这是所有文本框的惯例。用户在系统设置里关闭了闪烁时
    // 只切到亮的相位，不启动定时器。
    void RestartCaretBlink();

    // 停止闪烁定时器，相位复位为亮，以便下次显示时从亮开始。
    void StopCaretBlink();

    // 闪烁定时器到点：翻转相位并失效光标矩形。
    void OnCaretBlinkTimer();

private:
    ULONG              m_cRef;           // COM 引用计数；仅 UI 线程访问，不做原子操作
    ITextServices*     m_pServices;      // 引擎接口；Init 成功后有效，本对象持有一份引用
    IDuiTextHostSite*  m_pSite;          // 控件侧回调接口；可为空，为空时所有动作静默丢弃，不持有所有权

    RECT               m_rcClient;       // 文本区矩形，宿主客户区坐标、**像素**
    RECT               m_rcViewInset;    // 内边距，**百分之毫米**（引擎要求的单位）
    SIZEL              m_sizeExtent;     // 排版范围，**百分之毫米**（引擎要求的单位）

    CHARFORMAT2W       m_cfDefault;      // 默认字符格式；字号单位是一英寸的一千四百四十分之一
    PARAFORMAT2        m_pfDefault;      // 默认段落格式；制表位与缩进同上单位

    DWORD              m_dwPropertyBits; // 属性位集合（多行 / 只读 / 换行 / 密码 / 竖排等）
    DWORD              m_dwScrollBars;   // 引擎应当认为存在哪些滚动条
    DWORD              m_dwMaxLength;    // 文本长度上限（字符数）
    TCHAR              m_chPasswordChar; // 密码显示时使用的字符
    bool               m_bBackTransparent; // 背景是否透明（透明时由控件自己画背景）

    COLORREF           m_crSelBack;      // 选区背景色；CLR_INVALID 表示跟随系统
    COLORREF           m_crSelText;      // 选区文字色；CLR_INVALID 表示跟随系统

    bool               m_bUiActive;      // 是否处于界面激活状态（即控件是否持有键盘焦点）
    //是否显示文本插入光标。false 时不画光标，引擎的显示请求照常记录，
    //其余行为不变。见 SetShowCaret 的注释。
    bool               m_bShowCaret;
    DuiCaret           m_caret;          // 系统光标：只创建、只跟随插入点（供输入法定位），用空白位图创建，屏幕上不留像素

    //引擎最近一次通过 TxShowCaret 提出的要求：true 为显示光标。未处于界面激活
    //状态时的显示请求会被拒绝，不记入本成员；系统光标重建或控件失焦时复位为 false。
    bool               m_bCaretWanted;
    //闪烁相位：true 为亮、false 为灭。停止闪烁时复位为 true，保证下次一显示就是亮的。
    bool               m_bCaretBlinkOn;
    //闪烁定时器的系统编号（线程定时器）；0 表示未启动。在光标可见期间运行，
    //光标隐藏、控件失焦、Shutdown 时撤销。
    UINT_PTR           m_idCaretBlinkTimer;
    //绘制光标用的尺寸（像素）。取自引擎在 TxCreateCaret 里给的宽高；给的是位图时
    //取位图的宽高；宽或高为 0 时按系统 CreateCaret 的规则取边框宽度。
    SIZE               m_szCaretPaint;
    //光标形状位图：引擎在 TxCreateCaret 里给出位图时复制的一份，实心竖线光标时为
    //nullptr。本对象持有，重建光标与 Shutdown 时删除。不直接保存引擎的位图句柄，
    //是因为那张位图归引擎所有，何时释放由引擎决定。
    HBITMAP            m_hCaretPaintBmp;
    //系统光标用的全零单色位图，尺寸与 m_szCaretPaint 相同。系统光标用它创建，
    //因此即使被排版引擎或 BeginPaint / EndPaint 显示出来，也不会改变屏幕像素。
    //本对象持有；系统光标还引用着它，必须在系统光标销毁之后才能删除。
    HBITMAP            m_hSysCaretBlankBmp;
    //是否处于 BeginDraw 与 EndDraw 之间。期间引擎的光标请求只记录、不触发重绘。
    bool               m_bInDraw;
    //BeginDraw 时光标应占的矩形（GetCaretVisibleRect 的结果），EndDraw 用它判断
    //绘制期间光标的最终状态有没有变化。
    RECT               m_rcCaretBeforeDraw;

    // 滚动过程中的防重入标志。引擎与宿主之间的滚动通知是**双向成环**的：
    // 引擎要求设位置 → 我们通知控件 → 控件更新滚动条 → 滚动条回调 →
    // 控件把新位置写回引擎 → 引擎又要求设位置……反方向（用户拖滚动条）
    // 同样成环。本标志在两个入口各判一次把环截断：引擎回调 TxSetScrollPos
    // 时判一次，控件调 SetScrollPos 时判一次。只堵一处堵不住另一个方向。
    bool               m_bInScrollNotify;

    // 引擎自选的定时器编号 → 系统分配的线程定时器编号。
    // 为什么要这张表：引擎给的编号由它自己决定，多个控件实例之间会撞号；
    // 而线程定时器的编号由系统分配、全局唯一。两者必须建立映射，撤销
    // 定时器时才能按引擎给的编号找回正确的系统编号。
    // 单个控件同时存在的定时器极少（通常只有光标闪烁一个），用定长数组
    // 即可，不引入容器依赖。
    enum { kMaxTimers = 8 };
    struct TimerSlot
    {
        bool     m_bUsed;      // 本槽位是否在用。**不能用编号是否为 0 来判断** ——
                               // 引擎完全有可能使用 0 作为定时器编号，那样就会
                               // 把在用的槽位误判成空闲。
        UINT     m_idEngine;   // 引擎给的编号
        UINT_PTR m_idSystem;   // 系统分配的线程定时器编号
    };
    TimerSlot          m_timers[kMaxTimers];
};

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHTEXT
