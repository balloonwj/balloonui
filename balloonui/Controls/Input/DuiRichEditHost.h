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

    // ---- HwndHostControl 覆写（接 child HWND 通知）----
    void    OnHwndCommand(UINT enCode) override;
    LRESULT OnHwndNotify (NMHDR* pnmh) override;

    // ---- 测试用：pre-HWND 状态访问 ----
    void    Test_SetTextDirect(LPCTSTR sz)  { m_cachedText = sz ? sz : _T(""); }
    CString Test_GetCachedText() const      { return m_cachedText; }
    void    Test_SetFocused(bool b)         { m_bFocused = b; }

    // 老 CSkinRichEdit 的资源 setter 空 stub，迁移期占位用。
    void    SetBgPic     (LPCTSTR /*p*/) {}
    void    SetTransparent(BOOL /*b*/, HDC /*hBgDC*/ = nullptr) {}

private:
    COLORREF BorderColor() const;
    void     RefreshCacheFromHwnd();
    void     ApplyDefaultCharFormat();
    void     ApplySelectionCharFormatFlag(DWORD mask, DWORD effects, bool on);
    void     ApplySelectionTextColor(COLORREF cr);

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
};

} // namespace balloonwjui

#endif // BUI_FEATURE_RICHEDIT
