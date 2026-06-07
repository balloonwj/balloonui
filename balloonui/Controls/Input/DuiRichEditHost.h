#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_RICHEDIT

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../HwndHostControl.h"

namespace balloonwjui {

// =================================================================
// DuiRichEditHost —— 富文本编辑控件（HWND-hosted，包 RICHEDIT50W）
// =================================================================
//
// 用途：聊天输入框 / 消息区里需要图文混排（emoji、内嵌图片、链接、字体
// 颜色 / 字重 / 下划线、IME 候选条）的场景。这是 balloonui 里两个明确的
// "HWND 逃生口"之一（另一个是 DuiEditHost），因为 windowless ITextServices
// 的接入太重，而标准 RICHEDIT50W 类把这些已经免费做好了。
//
// 工作机制：
//   · 包一个真 RICHEDIT50W 子 HWND（msftedit.dll，进程级懒加载，第一次
//     EnsureCreated 时加）。窗口风格根据 m_multiLine / m_wordWrap /
//     m_readonly 在 EnsureCreated 时拼出来；这些标志在 HWND 创建之后
//     再改 → 销毁内部 HWND 重建（因 ES_MULTILINE / ES_AUTOHSCROLL 是
//     baked-in 风格位）。
//   · 对外 API 跟 DuiEditHost 在重叠部分保持一致（文本读写、只读、
//     最大长度、占位、多行、margins、边框绘制）；rich-only 扩展：
//     字符格式、默认字色 / 背景色、bold / italic / underline 切换、
//     URL 自动识别、undo / cut / copy / paste、图片 / 引用块 / 文件卡
//     在选区插入、RTF 文档保存 / 加载。
//   · 图片插入用 CF_BITMAP 剪贴板 + EM_PASTESPECIAL trick；为防把用户
//     剪贴板搞乱，仅保存 / 恢复 CF_UNICODETEXT，<u>不</u>保留其它格式。
//   · auto-URL 检测后，EN_LINK 转发为 DUIN_RE_LINKCLICK，extra 是
//     原始 ENLINK NMHDR* 指针。
//   · 状态缓存（m_cachedText、m_placeholder、m_showPlaceholder...）
//     在 HWND 还未创建前是<u>真值源</u>；HWND 创建后每次变化都同步镜像。
//   · 仅在 host 线程访问；RichEdit IME 要求真 HWND focus。
//
// 代码用法（聊天输入框，开启 auto-URL + 引用块插入）：
//
//     auto re = std::unique_ptr<DuiRichEditHost>(new DuiRichEditHost());
//     re->SetMultiLine(true);
//     re->SetWordWrap(true);
//     re->SetMaxLength(8192);
//     re->SetPlaceholder(_T("输入消息..."));
//     re->SetAutoUrlDetect(true);
//     DuiRichEditHost* raw = re.get();
//     m_inputRow->AddChild(std::move(re));
//     raw->EnsureCreated(host->m_hWnd);
//     // 引用某用户的话：
//     raw->InsertQuoteBlock(_T("alice"), _T("你看到 build report 了吗？"));
//     // 父对话框 OnDuiNotify：
//     //   case DUIN_RE_LINKCLICK: ShellExecute(...); break;
//
// XML 用法（详细属性见 guides.html §3.3.12）：
//
//     <richedit id="100"
//               placeholder="输入消息..."
//               max-length="8192"
//               multi-line="true" word-wrap="true"
//               auto-url-detect="true"
//               margins="6,4,6,4"
//               fixedHeight="120"/>
//
//   注意：跟 <edit> 一样，builder 不调 EnsureCreated；caller 在 host
//   HWND 就绪后自己调一次。
//
// 事件：
//   · DUIN_VALUECHANGED   — 文字改变（EN_CHANGE）。
//   · DUIN_SETFOCUS       — EN_SETFOCUS。
//   · DUIN_KILLFOCUS      — EN_KILLFOCUS。
//   · DUIN_RE_LINKCLICK   — auto-URL 检测下点击 URL；extra = ENLINK*。
//
// 替代关系：CSkinRichEdit。SetBgPic / SetTransparent 保留为空 stub，
// 让从 CSkinRichEdit 迁移过来的代码继续编过。
class BUI_API DuiRichEditHost : public HwndHostControl
{
public:
    // 控件特有的通知码（在 DUIN_CUSTOM 之上扩展）。
    enum { DUIN_RE_LINKCLICK = DUIN_CUSTOM + 1 };

    DuiRichEditHost();
    ~DuiRichEditHost() override;

    // ---- Pre-Create 配置（必须在 EnsureCreated 之前调）----

    // 竖直滚动位置变化回调原型。当用户用滚轮 / 键盘等使内容滚动、首个可见
    // 像素发生变化时,本控件回调它(供外部「悬浮滚动条」据此同步显示)。
    //   ctx：SetOnVScrollChanged 传入的上下文(通常为宿主面板 this)。
    typedef void (*VScrollChangedFn)(void* ctx);

    // 多行模式。<u>HWND 创建后再改会触发内部 HWND 销毁重建</u>。
    //   b：true（默认） = 多行；false = 单行。
    void    SetMultiLine(bool b);

    // 自动换行。仅多行模式有意义。HWND 创建后改也是销毁重建。
    //   b：true（默认）= 自动换行（关闭 ES_AUTOHSCROLL）；false = 横向滚动。
    void    SetWordWrap (bool b);
    bool    IsMultiLine() const { return m_multiLine; }
    bool    IsWordWrap () const { return m_wordWrap;  }

    // 创建 / 销毁 RICHEDIT50W 子 HWND（msftedit.dll 懒加载）。
    //   hwndParent：host HWND。
    //   返回：true 创建成功；false 失败（msftedit 加载失败 / CreateWindow 失败）。
    bool    EnsureCreated(HWND hwndParent);

    // ---- 文本读写 ----

    // 设置 / 读取整个文档的文本。HWND 未建时只更新缓存；建好后既更新
    // 缓存又同步给 HWND。
    void    SetText(LPCTSTR sz);
    CString GetText() const;

    // 当前文档字符数。HWND 未建时返回缓存长度。
    int     GetTextLength() const;

    // 设置 / 读取只读模式。
    void    SetReadOnly(bool b);
    bool    IsReadOnly() const { return m_readonly; }

    // 设置 / 读取最大字符数限制。0 = 无限制（默认；内部仍会下发一个很大
    // 的 EM_EXLIMITTEXT 值给 RichEdit 防止它默认上限太低）。
    void    SetMaxLength(int n);
    int     GetMaxLength() const { return m_maxLen; }

    // 设置内边距（EDIT 文字到边框的间距）。
    //   left/top/right/bottom：>= 0 的整数。默认 4/2/4/2。
    void    SetMargins(int left, int top, int right, int bottom);

    // ---- 占位文字（EDIT 空 + 未聚焦时显示）----

    void    SetPlaceholder(LPCTSTR sz);
    CString GetPlaceholder() const { return m_placeholder; }

    // 是否显示 placeholder。默认 true。
    void    SetShowPlaceholder(bool b);
    bool    IsShowingPlaceholder() const;

    // ---- 颜色 ----

    // 整个 EDIT 的背景色。默认白。
    void    SetBackgroundColor(COLORREF cr);
    COLORREF GetBackgroundColor() const { return m_bgColor; }

    // 默认字符色（应用在 default CHARFORMAT2 fg）。默认 RGB(30,30,30)。
    void    SetTextColor(COLORREF cr);
    COLORREF GetTextColor() const { return m_textColor; }

    // ---- 选区操作 ----（HWND 未建时全部 no-op）

    // 设置 / 读取选区起止字符位置。
    //   cpMin / cpMax：>= 0 字符索引（cpMin 可 == cpMax = 光标位置）。
    void    SetSel(long cpMin, long cpMax);
    void    GetSel(long& cpMin, long& cpMax) const;

    // 选中全部文本。
    void    SelectAll();

    // 把当前选区替换为 text。
    //   text：替换文本。
    //   canUndo：true 表示这次操作可撤销（push undo stack）。
    void    ReplaceSel(LPCTSTR text, bool canUndo = true);

    // 在文档末尾追加文本（不影响当前选区）。
    void    AppendText(LPCTSTR text);

    // 文档行数（含换行后的最后一行）。
    int     LineCount() const;

    // ---- 标准编辑命令（菜单 / 快捷键背后调的就是这些）----

    void    Undo();
    void    Cut();
    void    Copy();
    void    Paste();
    void    Clear();

    // ---- 右键菜单 ----
    //
    // 默认：右键 / 键盘菜单键 弹 balloonui 自绘菜单（DuiMenu）取代 RichEdit
    // 原生菜单：
    //   · 读写：剪切 / 复制 / 粘贴 /（分隔条）/ 全选；
    //   · 只读：复制 /（分隔条）/ 全选。
    // 各项按选区 / 剪贴板 / 文本量自动灰显（规则见 EditContextMenu.h）。粘贴
    // 沿用 SetPasteAsPlainTextDefault 的设定。实现复用已有的子 HWND 子类过程
    // （ScrollSubclassProc）拦 WM_CONTEXTMENU。
    //   b：true（默认）= 自绘菜单；false = 退回原生菜单。
    void    SetContextMenuEnabled(bool b);
    bool    IsContextMenuEnabled() const { return m_contextMenuEnabled; }

    // ---- 字符格式（操作当前选区）----

    void    SetSelBold     (bool b);
    void    SetSelItalic   (bool b);
    void    SetSelUnderline(bool b);
    void    SetSelTextColor(COLORREF cr);

    // 启用 / 关闭 URL 自动识别。识别后蓝色下划线呈现，点击发
    // DUIN_RE_LINKCLICK。
    void    SetAutoUrlDetect(bool b);
    bool    IsAutoUrlDetect() const { return m_autoUrl; }

    // ---- 富文本内容插入 ----
    //
    // 三个方法都在<u>当前选区</u>插入（替换选区），插入后恢复 default
    // CHARFORMAT 让后续输入回到正常字体。

    // 插入图片。先按 maxW / maxH 缩放（0 = 不限）再插入。
    //   path：图片磁盘路径（PNG / JPG / BMP / ICO 等 RichEdit 支持的格式）。
    //   maxW / maxH：>= 0；0 = 该方向不限。
    //   返回：true 表示成功；false：HWND 未建 / 文件加载失败 / 剪贴板
    //         往返失败。
    bool    InsertImageFromFile  (LPCTSTR path,  int maxW = 0, int maxH = 0);

    // 同上但源是已加载的 HBITMAP。
    bool    InsertImageFromBitmap(HBITMAP hbm,   int maxW = 0, int maxH = 0);

    // 插入引用块，呈现为：
    //     │  <senderName>
    //     │  <body>
    // 灰色字、"│" 是 U+2503（HEAVY VERTICAL）；body 里的换行被保留并
    // 加同样的 │ 前缀。
    void    InsertQuoteBlock(LPCTSTR senderName, LPCTSTR body);

    // 插入文件卡片（聊天里"发送了一个文件"的视觉占位）：
    //     [📎]  <fileName>   23.4 MB
    // 深灰字 + 轻微下划线；sizeBytes 走 FormatHumanSize 自动转单位。
    void    InsertFileCard(LPCTSTR fileName, ULONGLONG sizeBytes);

    // 静态 helper：把字节数变成人类可读字符串（B/KB/MB/GB/TB）。
    static CString FormatHumanSize(ULONGLONG bytes);

    // ---- RTF / 纯文本 持久化 ----

    // 把当前文档（文本 + 格式 + 嵌入图片 OLE）序列化成 RTF 字节流
    // （EM_STREAMOUT(SF_RTF)）。RTF 按规范是 ANSI，所以输出 CStringA。
    // HWND 未建时返回 false 并清 out。
    bool    SaveRTF (CStringA& out)            const;

    // 把传入的 RTF 字节流加载成当前文档（EM_STREAMIN(SF_RTF)）。
    // HWND 未建时返回 false（RTF 解析逻辑在 RICHEDIT50W 里，纯缓存模式
    // 没法做）。成功时刷新 m_cachedText。
    bool    LoadRTF (const CStringA& in);

    // 同 Save/LoadRTF 但走 SF_TEXT | SF_UNICODE —— 无格式、无 OLE，
    // 等价于 GetText/SetText 但用统一的流式 API；好处是 caller 可在
    // "lossy plain"和"lossless RTF"两路之间用对称 API 切换。
    bool    SaveText(CString&  out)            const;
    bool    LoadText(const CString&  in);

    // ---- Paste filter + 查找 ----

    // 仅粘贴 CF_UNICODETEXT / CF_TEXT，丢掉所有格式 / 内嵌对象。粘贴
    // 在当前选区。HWND 未建或剪贴板没文字时返回 false。等价于"粘贴专用
    // - 无格式文本"（EM_PASTESPECIAL + CF_UNICODETEXT）。
    bool    PasteAsPlainText();

    // 把所有 WM_PASTE / Ctrl-V 路由到 PasteAsPlainText —— 适用于聊天
    // 输入框这种"不要带颜色 / 字体 / 链接来"的场景。默认 false。
    void    SetPasteAsPlainTextDefault(bool b)     { m_pastePlain = b; }
    bool    GetPasteAsPlainTextDefault() const     { return m_pastePlain; }

    // 在文档里查找下一处 needle。
    //   needle：查找字符串。
    //   searchFrom：起始 cpMin；-1 = 从当前选区结尾（forward）/ 起点（backward）开始。
    //   forward：true = 向前；false = 向后（FR_DOWN 清掉）。
    //   matchCase：true = 区分大小写（FR_MATCHCASE）。
    //   matchWord：true = 匹配整词（FR_WHOLEWORD）。
    //   outRange：命中区间；miss 时不动。
    //   返回：true 命中、false miss / HWND 未建。
    bool    FindText(LPCTSTR needle,
                     long searchFrom,
                     bool forward,
                     bool matchCase,
                     bool matchWord,
                     CHARRANGE& outRange) const;

    // 同 FindText，命中时同时把选区移到命中处。wrap=true 时 miss 后从
    // 文档头（forward）/ 尾（backward）再试一次。
    bool    FindAndSelect(LPCTSTR needle,
                          long searchFrom,
                          bool forward,
                          bool matchCase,
                          bool matchWord,
                          bool wrap);

    // ---- DuiControl 覆写 ----
    void    Layout(const RECT& rcAvail) override;
    void    OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool    OnSetCursor(POINT pt) override;

    // 滚轮:多行模式下滚动内容。DUI 宿主按鼠标位置把滚轮派发给最顶层命中的
    // 控件(不冒泡),正文这类寄宿 HWND 命中的就是本控件,故必须由本控件自己
    // 处理滚轮才能滚动。滚动后若注册了 VScrollChangedFn 则回调通知(供外部
    // 悬浮滚动条同步)。
    //   pt：鼠标位置(host-client 坐标,本控件未使用);
    //   zDelta：滚轮增量(WHEEL_DELTA 的整数倍,正值向上滚);
    //   mkFlags：按键修饰(本控件未使用)。
    //   返回:true = 已消费(多行且内容溢出);false = 未消费(单行 / 内容放得下,
    //         交由宿主另作处理)。
    bool    OnMouseWheel(POINT pt, short zDelta, UINT mkFlags) override;

    // ---- HwndHostControl 覆写（接 child HWND 通知）----
    void    OnHwndCommand(UINT enCode) override;
    LRESULT OnHwndNotify (NMHDR* pnmh) override;

    // ---- 测试用：pre-HWND 状态访问 ----
    void    Test_SetTextDirect(LPCTSTR sz)  { m_cachedText = sz ? sz : _T(""); }
    CString Test_GetCachedText() const      { return m_cachedText; }
    void    Test_SetFocused(bool b)         { m_bFocused = b; }
    bool    Test_IsContextMenuEnabled() const { return m_contextMenuEnabled; }

    // 老 CSkinRichEdit 的资源 setter 空 stub，迁移期占位用。
    void    SetBgPic     (LPCTSTR /*p*/) {}
    void    SetTransparent(BOOL /*b*/, HDC /*hBgDC*/ = nullptr) {}

    // ---- 边框开关 ----
    //
    // 设置 / 读取是否在 OnPaint 里绘制 1px 外边框(BorderColor() 决定颜色)。
    //   b: true(默认,与历史一致) = 画边框,RichEdit 表现为典型"输入框"
    //      外观;false = 跳过 Rectangle 绘制,让 RichEdit 与父容器卡片底完全
    //      融合,适用于"卡片内嵌一段只读多行可选文本"这类不希望出现输入框
    //      外观的场景(如 flamingoAdmin 公告详情的标题 / 正文)。
    //   只影响视觉外边框,RichEdit 自身行为(滚动 / 选区 / 输入)不变。
    void    SetShowBorder(bool b);
    bool    IsShowBorder() const { return m_showBorder; }

    // ---- 垂直滚动条槽位开关 ----
    //
    // 设置 / 读取是否在多行模式下保留垂直滚动条槽位(WS_VSCROLL 风格)。
    //   b: true(默认,与历史一致) = 加 WS_VSCROLL 风格,RichEdit 永远预留
    //      ~17px 的右侧滚动条空间,内容溢出时自动出现可拖动滚动条;
    //      false = 不加 WS_VSCROLL,RichEdit 没有滚动条槽,内容铺满全宽。
    //      适用于"外层容器已经管滚动 + 自己高度被父布局算到足够装下全部
    //      内容"的场景(如公告详情把 RichEdit 嵌进 OverlayScrollbar 接管的
    //      预览区),不希望右侧出现内嵌滚动条占空间。
    //   ES_AUTOVSCROLL 行为标志不动 —— RichEdit 内部仍可滚动(键盘 / 滚轮),
    //   只是不显示滚动条。
    //   注意:HWND 创建后再改会触发内部 HWND 销毁重建(WS_VSCROLL 是 baked-in
    //   风格位,跟 SetMultiLine / SetWordWrap 同性质)。
    void    SetShowVScroll(bool b);
    bool    IsShowVScroll() const { return m_showVScroll; }

    // ---- 外部悬浮滚动条接入(竖直)----
    //
    // 用途:当调用方关掉原生滚动条(SetShowVScroll(false))、改用自绘的悬浮
    // 滚动条(如 flamingoAdmin 的 OverlayScrollbar)时,需要:① 在内容因滚轮 /
    // 键盘滚动而位置变化时收到通知,② 查询滚动度量以驱动悬浮条,③ 反向把
    // 悬浮条的拖动结果写回本控件。下面三个接口正是为此提供。
    //
    // 注册竖直滚动位置变化回调。HWND 未建时仅缓存,建好后由内部子类化的窗口
    // 过程在滚轮 / 键盘 / 翻页导致首个可见位置变化时回调。
    //   fn：回调函数指针(nullptr 注销);ctx：回调上下文(本控件不持有所有权)。
    void    SetOnVScrollChanged(VScrollChangedFn fn, void* ctx);

    // 查询竖直滚动度量(均为像素;HWND 未建时三者全置 0)。
    //   contentH：内容总高度(所有行铺开的像素高);
    //   scrollPos：当前已向下滚动的像素偏移(0 = 顶部);
    //   viewH：可视区像素高度(约等于控件客户区高)。
    //   三个参数均为出参(引用)。
    void    GetVScrollMetrics(int& contentH, int& scrollPos, int& viewH) const;

    // 把内容竖直滚动到指定像素偏移(自动夹取到 [0, contentH-viewH])。
    //   pixelPos：目标像素偏移(0 = 顶部)。HWND 未建时为空操作。
    void    SetVScrollPos(int pixelPos);

    // ---- 默认字体覆盖 ----
    //
    // 用调用方持有的 HFONT 覆盖 RichEdit 的默认字体。
    //   font: 调用方持有所有权的 HFONT(本控件不持有、不 Delete);
    //         nullptr = 退回到 DuiResMgr::Inst().GetDefaultFont() 默认值,
    //         与 SetDefaultFontFromHFONT 未被调过的初始状态一致。
    //   行为:HWND 已建则立即 WM_SETFONT(redraw=true) 让现有文字与未来输入
    //   都按新字体重排;未建则只缓存 m_defaultFont,EnsureCreated 时一并应用。
    //   典型用法:让公告详情的标题用 17pt 半粗、正文用 10pt 普通字体
    //   (admin 业务字号),而不是 RichEdit 系统默认。
    void    SetDefaultFontFromHFONT(HFONT font);
    HFONT   GetDefaultFontOverride() const { return m_defaultFont; }

private:
    COLORREF BorderColor() const;
    void     RefreshCacheFromHwnd();
    void     ApplyDefaultCharFormat();
    void     ApplySelectionCharFormatFlag(DWORD mask, DWORD effects, bool on);
    void     ApplySelectionTextColor(COLORREF cr);

    int      LineHeightPx() const;      // 单行像素高(取默认字体 TEXTMETRIC)
    int      FirstVisibleLine() const;  // EM_GETFIRSTVISIBLELINE(无 HWND 返回 0)

    // 按滚轮增量滚动内容(多行且溢出时按行滚动并回调 m_onVScroll)。被 DUI 层
    // OnMouseWheel 与子窗口子类过程的 WM_MOUSEWHEEL 分支共用。
    //   zDelta：WHEEL_DELTA 的整数倍,正值向上滚。
    //   返回:true = 已滚动(消费);false = 单行 / 内容放得下 / 无 HWND(未消费)。
    bool     ScrollByWheelDelta(short zDelta);

    // 子窗口子类过程:滚轮消息默认发给鼠标下的窗口 —— 正文是真子窗口,滚轮
    // 多数情况下直达它而非 DUI 宿主,故必须在子窗口层拦 WM_MOUSEWHEEL 主动滚。
    // 在 EnsureCreated 里挂、析构与重建前摘。
    static LRESULT CALLBACK ScrollSubclassProc(HWND hwnd, UINT msg,
                                               WPARAM wParam, LPARAM lParam,
                                               UINT_PTR uIdSubclass,
                                               DWORD_PTR dwRefData);
    void     InstallScrollSubclass();   // 给当前子 HWND 挂滚动子类过程(幂等)
    void     RemoveScrollSubclass();    // 摘掉滚动子类过程(幂等)

    // 收到 WM_CONTEXTMENU 时(由 ScrollSubclassProc 分发):读自身状态 →
    // BuildEditContextMenu → 弹 DuiMenu → 执行被选项。
    //   screenPt：菜单弹出位置(屏幕坐标;键盘触发时由 ScrollSubclassProc 算好)。
    //   返回:true = 已弹自绘菜单(消费消息、压掉原生菜单);false = 未处理
    //         (交回默认过程,例如 HWND 无效 / 菜单特性被裁掉)。
    bool     ShowContextMenu(POINT screenPt);

private:
    bool        m_multiLine     = true;     // RichEdit 默认就是 multi-line 用法
    bool        m_wordWrap      = true;
    bool        m_readonly      = false;
    int         m_maxLen        = 0;        // 0 = 无限制（仍下发大默认 EM_EXLIMITTEXT）
    bool        m_autoUrl       = false;
    COLORREF    m_bgColor       = RGB(255, 255, 255);
    COLORREF    m_textColor     = RGB( 30,  30,  30);

    CString     m_cachedText;
    CString     m_placeholder;
    bool        m_showPlaceholder = true;

    int         m_marginL = 4, m_marginT = 2, m_marginR = 4, m_marginB = 2;
    UINT        m_ctrlIdHwnd = 0;
    bool        m_pastePlain = false;     // 把 WM_PASTE 路由到 PasteAsPlainText
    bool        m_showBorder = true;      // 是否画 1px 外边框;默认 true 维持历史外观
    bool        m_showVScroll = true;     // 多行下是否预留 WS_VSCROLL 槽;默认 true 维持历史外观
    HFONT       m_defaultFont = nullptr;  // 覆盖 RichEdit 默认字体;nullptr = 走 DuiResMgr 默认;不持有所有权

    VScrollChangedFn m_onVScroll    = nullptr;  // 竖直滚动位置变化回调;nullptr = 不通知
    void*            m_onVScrollCtx = nullptr;  // 上述回调上下文(本控件不持有所有权)
    bool             m_scrollSubclassed = false;// 是否已给子 HWND 挂滚动子类过程
    bool             m_contextMenuEnabled = true;// 是否用 balloonui 自绘右键菜单(默认开)
};

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHEDIT
