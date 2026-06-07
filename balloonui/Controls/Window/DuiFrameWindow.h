#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_FRAMEWINDOW

// .cpp must include stdafx.h first.
#include "../../DuiHost.h"
#include "../Layout/DuiLayout.h"
#include "../../BalloonUiApi.h"

namespace balloonwjui {

// 极简 Optional<T> —— 只用 (bool m_set + T m_value)。
// 为什么不直接用 std::optional：
//   · std::optional 是 C++17 标准（balloonui 工程跑 /std:c++14 默认；
//     旧的 WTL 9.0 在 /std:c++17 下会因 noexcept 规范不匹配编不过）
//   · 这里只需要"有/没有 + 取值"语义，不需要 std::optional 的全部功能
//     （monadic ops、reference 重载、in_place 构造等）
//
// 用法：
//   Optional<int> x;            // 未设
//   x = 5;                      // 等价 x.set(5)
//   if (x.has_value()) { x.value(); }
template <typename T>
class Optional
{
public:
    Optional() : m_set(false), m_value() {}
    Optional(const T& v) : m_set(true), m_value(v) {}

    Optional& operator=(const T& v)
    {
        m_set   = true;
        m_value = v;
        return *this;
    }

    bool has_value() const { return m_set; }
    const T& value() const { return m_value; }
    T value_or(const T& def) const { return m_set ? m_value : def; }

private:
    bool m_set;
    T    m_value;
};

// 声明式 frame 配置 —— 由 DuiXmlBuilder::FromFrameXml 从 <frame-window>
// 元素的属性解析得到，由 DuiFrameWindow::ApplyConfig 一次性落地为各 setter
// 调用。
//
// 字段大部分用 Optional<T>，"未设" vs "显式设值" 语义清晰：
//   · 未设 → ApplyConfig 不调对应 setter，frame 保留默认行为
//   · 已设 → ApplyConfig 调用对应 setter（即使值等于默认也调，
//            因为 caller 显式表达了意图）
//
// COLORREF 字段：Optional 为空表示"用 frame 默认"；Optional 含
// CLR_INVALID 表示"显式禁用"（如 client-border-color="none" 关闭边框）。
//
// bgImagePath 是 CString 而非 Optional —— 空字符串就是"未设 bg 图"，
// 和"已设但是空路径"无法区分（也无意义），所以省一层包装。
//
// 业务直接 new + 填字段也行（无需经过 XML），ApplyConfig 接受任何来源。
struct DuiFrameWindowConfig
{
    // 标题栏
    Optional<CString>  title;
    Optional<int>      titleBarHeight;
    Optional<bool>     titleBarTransparent;
    Optional<COLORREF> titleTextColor;
    Optional<COLORREF> captionGlyphColor;

    // 三按钮可见性 —— 任一显式给出就调 SetButtons，未给的拿当前值
    Optional<bool>     hasMinButton;
    Optional<bool>     hasMaxButton;
    Optional<bool>     hasCloseButton;

    // 尺寸 / 边
    Optional<int>      minW;
    Optional<int>      minH;
    Optional<bool>     resizable;
    Optional<int>      borderPx;
    Optional<COLORREF> clientBorderColor;   // CLR_INVALID 显式禁用边框

    // 9-grid 背景图
    // bgImagePath 已经过 caller 端的路径解析（绝对 / 或经过
    // DuiXmlBuilder::ResolveAssetPath 转成绝对）。空字符串 = 不设 bg。
    CString            bgImagePath;
    RECT               bgSrcInsets = { 0, 0, 0, 0 };
    Optional<RECT>     bgDstInsets;         // 不设 → 退化为 src == dst
};

// =================================================================
// DuiFrameWindow —— 自绘顶层窗口（标题栏 + 三按钮 + 9-grid bg）
// =================================================================
//
// 用途：业务侧的对话框 / 主窗都用它做顶层窗口。把系统非客户区
// （标题栏 + 厚边框 chrome）整个抹掉，让应用自己绘制标题栏，可挂 9-grid
// 背景图把皮肤画进客户区。8 向 resize 和拖动还是 OS 负责（靠
// WM_NCHITTEST 返 HTLEFT/HTRIGHT/HTTOP/HTBOTTOM/HTCAPTION 等）；
// min/max/close 三按钮是<u>内部</u>的自绘 CaptionButton（藏在 .cpp 里），
// 点击转 WM_SYSCOMMAND 给 OS。
//
// 架构：继承自 DuiHost（单 HWND、完整 DUI 消息路由、双缓冲绘制）。
// 内部 DUI root 自动组合为：
//
//      [ DuiFrameTitleBar（固定高度）]
//      [ caller 提供的客户区（weight 1）]
//
// host 的客户区 = 整个窗口。caller 通过 SetClientContent 换内容；标题栏
// 属性（文字 / icon / 按钮可见性 / resize / 透明 / 颜色）通过本类的
// SetXxx 配置；标题栏控件本身是<u>私有</u>的，业务不能直接拿到。
//
// 工作机制：
//   · 顶层 CWindowImpl，subclass DuiHost；DUI 树里<u>唯一</u>的真 HWND
//     就是它（与"无 HWND DUI"的不变量一致）。
//   · 标题栏自绘（icon + 文字 + min/max/close）：默认不透明态浅灰填充
//     + Win 红 close hover；透明态（SetTitleBarTransparent）让 host 的
//     bg 图直接穿透。OS 仍管 8 向 resize + 拖动（WM_NCHITTEST 覆写）。
//   · WS_THICKFRAME 保留 → Aero snap / 最大化动画正常；只是用
//     WM_NCCALCSIZE 返 0 把 chrome 抠掉。
//   · 默认 m_borderPx = 8（96-dpi 逻辑像素），运行时按 GetDpi()/96 缩放
//     成物理像素；m_titleH = 36。
//   · 默认开 1px 客户区描边（RGB(200,200,200)），但<u>当 SetBgImage 设了
//     bg 图时自动跳过</u>，让 9-grid 图自身的圆角 / 装饰当边界。
//   · Per-monitor DPI 通过 DuiHost 继承到本类。
//
// 代码用法（建一个顶层 DUI 窗口）：
//
//     DuiFrameWindow wnd;
//     wnd.SetButtons(true, false, true);            // 没有 max 按钮
//     wnd.SetMinSize(400, 300);
//     wnd.Create(nullptr, CWindow::rcDefault, _T("Settings"),
//                WS_OVERLAPPEDWINDOW | WS_VISIBLE);
//     wnd.SetTitle(_T("设置"));                      // 必须 Create 之后调
//     wnd.SetClientContent(BuildSettingsUi());      // unique_ptr<DuiControl>
//
// XML 用法（顶层 <frame-window>，详细见 guides.html §11）：
//
//     <frame-window
//         title="好友资料"
//         title-bar-height="40"
//         title-bar-transparent="true"
//         title-text-color="255,255,255"
//         caption-glyph-color="255,255,255"
//         bg-image="BuddyInfoDlgBg.png"
//         bg-src-insets="10,69,10,10"
//         bg-dst-insets="10,40,10,10"
//         min-w="320" min-h="240"
//         resizable="true">
//       <vbox padding="24,16,24,16" gap="10">
//         ...客户区子树...
//       </vbox>
//     </frame-window>
//
//   解析入口：DuiXmlBuilder::FromFrameXml(xml, cfg)；落地：
//   frame.Create(...) → frame.ApplyConfig(cfg) → SetClientContent。
//
// 事件：
//   · 三按钮 min/max/close 点击 → 内部转 WM_SYSCOMMAND（SC_MINIMIZE /
//     SC_MAXIMIZE / SC_RESTORE / SC_CLOSE），业务<u>不需要</u>手动监听。
//     想阻止关闭按 WM_CLOSE 处理。
//   · 客户区子控件的 WM_DUI_NOTIFY 自动路由到本 frame 自身（即 m_hWnd）。
//   · WM_SIZE 内部更新 m_isMaximized 并把状态推给 max button —— 最大化
//     时按钮 glyph 自动切到 restore（双方块）样式。
//
// 替代关系：CSkinDialog（顶层皮肤窗口；模态对话框场景仍用 CDialogImpl
// + DuiHost::SubclassWindow 重叠）。
class DuiFrameTitleBar;

class BUI_API DuiFrameWindow : public DuiHost
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiFrameWindow__"), 0, COLOR_WINDOW)

    DuiFrameWindow();
    ~DuiFrameWindow();

    // ---- 标题栏属性 ----

    // 设置 / 读取标题文字。注意：必须在 Create 之后调，否则 OS 标题
    // （任务栏 / Alt-Tab 显示）保留窗口类名，业务标题没生效。
    //   title：任意文字。nullptr 视为空。
    void    SetTitle(LPCTSTR title);
    CString GetTitle() const { return m_title; }

    // 设置 / 读取标题栏高度。
    //   px：>= 18；< 18 会被 clamp 到 18。默认 36。
    void    SetTitleBarHeight(int px);
    int     GetTitleBarHeight() const { return m_titleH; }

    // 设置 / 读取标题左侧 icon 位图（16–24px 推荐）。caller 持有所有权。
    //   icon：HBITMAP 或 nullptr 表示不画 icon。
    void    SetIcon(HBITMAP icon);
    HBITMAP GetIcon() const { return m_titleIcon; }

    // 标题栏透明 — 不画自身背景，让 host 的 SetBgImage(9-grid) 顶部那条
    // 装饰区直接当标题条用。常用搭配：
    //   frame.SetBgImage(hbm, RECT{...,80,...,...});  // 9-grid 顶 80px 不缩
    //   frame.SetTitleBarHeight(80);                  // 标题条高度 = 装饰区高度
    //   frame.SetTitleBarTransparent(true);
    //   frame.SetTitleTextColor(RGB(255,255,255));    // 白字配深色装饰
    //   frame.SetCaptionGlyphColor(RGB(255,255,255)); // 白色 min/max/close glyph
    void    SetTitleBarTransparent(bool b);
    bool    IsTitleBarTransparent() const { return m_titleTransparent; }

    // 标题文字颜色（仅在 SetTitleBarTransparent(true) 配合彩色背景时
    // 通常需要改）。默认 ink-1 (40,40,40)。
    void    SetTitleTextColor(COLORREF c);

    // min/max/close 的 glyph 颜色覆盖。CLR_INVALID = 用默认深灰。
    // 关闭按钮 hover 红底时的白 glyph 不受此影响（仍是白）。
    void    SetCaptionGlyphColor(COLORREF c);

    // 设置 / 读取三按钮可见性。默认全部 true。
    //   minBtn / maxBtn / closeBtn：true = 显示该按钮；false = 隐藏。
    void    SetButtons(bool minBtn, bool maxBtn, bool closeBtn);
    bool    HasMinButton  () const { return m_btnMin;   }
    bool    HasMaxButton  () const { return m_btnMax;   }
    bool    HasCloseButton() const { return m_btnClose; }

    // 设置 / 读取 resize 边缘抓握区宽度（96-dpi 逻辑像素，4 边各算一份）。
    // 运行时按 monitor DPI 缩放成物理像素，125% → 10、150% → 12。
    // 默认 8。
    //
    // 为什么是 8（不是 Win32 frame 默认的 6）：
    //   DuiFrameWindow 用 WM_NCCALCSIZE 把整个非客户区都抹掉了，所以
    //   用户看不到任何窗口外缘 / 阴影 / 边框 hint —— 只能瞄"里面这 N px"
    //   去拖。Win32 默认 6px 适合带原生 chrome 的窗口（还有外缘 DWM
    //   幽灵区），但 chromeless 窗口里 6px 真的很难抓。8 是触摸板 + HiDPI
    //   下既好抓、又不让"客户区点击区域"实质变小的最小值。
    //
    //   px：>= 0；0 = 完全关闭 resize 边缘 hit-test（等价 SetResizable(false)
    //       但不影响其它）。
    void    SetBorderPx(int px);
    int     GetBorderPx() const { return m_borderPx; }

    // 设置 / 读取整窗 resize 开关（含拖角拖边）。
    //   b：true（默认）= 允许 resize；false = 固定尺寸。
    void    SetResizable(bool b);
    bool    IsResizable() const { return m_resizable; }

    // 设置 / 读取最小窗口跟踪尺寸（px）。OS 拖小到此值后挡住。
    //   w / h：>= 1；< 1 会被 clamp 到 1。默认 200 × 150。
    void    SetMinSize(int w, int h);
    SIZE    GetMinSize() const { return SIZE{ m_minW, m_minH }; }

    // 设置 / 读取最大窗口跟踪尺寸（px）。OS 拖大到此值后挡住。
    //   w / h：0 = 不限（默认）；>= 1 会被 OS clamp 到屏幕 work area。
    //   若同时 SetMaxSize 与 SetMinSize 设了相同值，等同于"固定尺寸"
    //   （但 resize 边缘 hit-test 仍激活；要彻底禁拖再 SetResizable(false)）。
    void    SetMaxSize(int w, int h);
    SIZE    GetMaxSize() const { return SIZE{ m_maxW, m_maxH }; }

    // ---- 标题栏自定义图标按钮 ----
    //
    // 在标题栏 close 按钮左侧依次插入可点击图标（VS Code / Chrome 右上角
    // "设置 / 帮助 / 通知"风格）。每个 caption icon 占一个标准按钮宽
    // （kCaptionBtnW），hover 同三按钮浅灰风格，点击发
    // DUIFW_CAPTION_ICON_CLICK 通知（extra = 添加时返回的 captionId）。
    // 业务侧识别用 code + extra，与 ctrlId 无关。
    //
    // HBITMAP 所有权由 caller 持有（控件只存裸指针），相同位图可被多个
    // caption icon 共享。建议位图源至少 16x16，PaintBlt HALFTONE 缩放到 16x16
    // 居中绘。
    //
    //   icon：HBITMAP，不可为 nullptr（nullptr 则不添加、返回 0）。
    //   tooltip：可选，非空时自动 DuiToolTipMgr::Register 该图标；析构 / 移除时 Unregister。
    //   返回：本图标的 captionId（>= 1 单调递增）；后续 Remove 用它，
    //         click notify 的 extra 也是它。
    int     AddCaptionIcon(HBITMAP icon, LPCTSTR tooltip = nullptr);
    // 按 AddCaptionIcon 返回的 captionId 移除该图标；找不到 id 静默忽略。
    void    RemoveCaptionIcon(int captionId);
    // 清空所有 caption icons。
    void    ClearCaptionIcons();

    // ---- 客户区 ----

    // 安装 / 替换客户区根控件。frame 内部已经预留了标题栏 slot，
    // <u>不要</u>直接调 DuiHost::SetRoot —— 用这个方法。
    //   root：任意 DuiControl 子类作为客户区 root（一般 vbox/hbox/grid）。
    //         nullptr 表示清空。
    void    SetClientContent(std::unique_ptr<DuiControl> root);
    DuiControl* GetClientContent() const { return m_clientContent; }

    // 应用一份声明式配置 —— 任何 cfg.field.has_value() 的字段都会调到
    // 对应 setter，未设字段保留 frame 当前值。bg image 走
    // LoadBgImageFromFile（host 持有 bitmap、析构释放）。
    //
    // 一般在 Create 之后、ShowWindow 之前调用，与手动调一串 SetXxx 等价。
    // 也可在窗口已显示后再调用 —— setter 各自负责 Invalidate / 重建
    // skeleton（如 SetTitleBarHeight）。
    //
    // 典型流程（XML 驱动）：
    //   DuiFrameWindowConfig cfg;
    //   auto content = DuiXmlBuilder::FromFrameXml(xml, cfg);
    //   frame.Create(...);
    //   frame.ApplyConfig(cfg);
    //   frame.SetClientContent(std::move(content));
    //   frame.ShowWindow(...);
    void    ApplyConfig(const DuiFrameWindowConfig& cfg);

    // ---- 单测用纯函数 helper ----
    //
    // 给定屏幕坐标点 + 窗口矩形 + 标题栏 / 按钮 / 边缘配置，计算
    // WM_NCHITTEST 应返的值。无副作用（不依赖 m_hWnd / GDI），便于单测。
    //   ptScreen：鼠标屏幕坐标。
    //   windowRcScreen：窗口屏幕矩形（GetWindowRect 拿到的）。
    //   titleBarH：标题栏高度（px）。
    //   borderPx：resize 边缘宽度（已 DPI 缩放后的物理像素）。
    //   resizable：是否允许 resize。
    //   buttonRectsScreen：三按钮的屏幕矩形数组。不存在的按钮传 {0,0,0,0}。
    //   返回：HTLEFT / HTRIGHT / HTTOP / HTBOTTOM / HT*角 / HTCAPTION /
    //         HTCLIENT / HTNOWHERE 之一。
    static UINT ComputeNcHitTest(POINT ptScreen,
                                 const RECT& windowRcScreen,
                                 int titleBarH,
                                 int borderPx,
                                 bool resizable,
                                 const RECT buttonRectsScreen[3]);

    // 最大化客户区内缩量 —— WS_THICKFRAME 窗口最大化时 OS 会把窗口
    // 往四周外推 borderX / borderY 像素（让"窗口可见部分"恰好等于
    // 显示器工作区），如果 OnNcCalcSize 不补偿，自绘标题栏顶部 ~7-8px
    // 会被屏幕边裁掉，视觉上"最大化时标题栏比正常态变矮了"。
    //
    //   maximized：当前是否最大化（IsZoomed(hwnd) != 0）。
    //   borderX：SM_CXFRAME + SM_CXPADDEDBORDER（物理像素）。
    //   borderY：SM_CYFRAME + SM_CXPADDEDBORDER（物理像素）。
    // 返回：四边各自要内缩多少 px。非最大化时全 0；最大化时为
    //       {borderX, borderY, borderX, borderY}。
    static RECT ComputeMaximizedClientInsets(bool maximized,
                                             int borderX,
                                             int borderY);

    // ---- notify codes ----
    enum NotifyCode
    {
        // 标题栏自定义图标按钮被点击；extra = AddCaptionIcon 返回的 captionId。
        // ctrlId 是 frame 内部 caption button 的 ctrlId（业务一般不关心，
        // 用 code + extra 完成识别即可）。
        DUIFW_CAPTION_ICON_CLICK = DUIN_CUSTOM + 1,
    };

    BEGIN_MSG_MAP(DuiFrameWindow)
        // WM_DESTROY 必须放在 CHAIN_MSG_MAP(DuiHost) 之前 —— 先清掉本类
        // 持有的 m_titleBar / m_skeleton / m_clientContent 三个裸指针
        // （它们指向 DuiHost 的 m_root 所拥有的子控件），再让 chain 把消息
        // 交给 DuiHost::OnDestroy 释放 m_root，避免下一次 Create + SetTitle
        // 时这几个裸指针变成悬空指针被解引用（参 OnFrameDestroyMsg 注释）。
        MESSAGE_HANDLER(WM_DESTROY,    OnFrameDestroyMsg)
        MESSAGE_HANDLER(WM_NCCALCSIZE, OnNcCalcSize)
        MESSAGE_HANDLER(WM_NCHITTEST,  OnNcHitTestMsg)
        MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_SIZE,       OnSizeMsg)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiChildNotify)
        CHAIN_MSG_MAP(DuiHost)
    END_MSG_MAP()

protected:
    LRESULT OnNcCalcSize  (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnNcHitTestMsg(UINT, WPARAM, LPARAM, BOOL& bHandled);
    void    OnGetMinMaxInfo(LPMINMAXINFO mmi);
    LRESULT OnSizeMsg     (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnDuiChildNotify(UINT, WPARAM, LPARAM, BOOL& bHandled);
    // WM_DESTROY raw handler：把派生类持有的、指向 DuiHost 子控件的
    // 三个裸指针置为 nullptr，然后 bHandled=FALSE 让消息继续 chain 到
    // DuiHost::OnDestroy 释放 m_root。这保证窗口对象在 DestroyWindow
    // 之后被复用（如 CMainDlg::m_LoginDlg 反复 DoModal）时再调
    // SetTitle / SetIcon / SetButtons 等不会触碰悬空指针。
    LRESULT OnFrameDestroyMsg(UINT, WPARAM, LPARAM, BOOL& bHandled);

private:
    void    BuildSkeleton();      // builds title bar + content slot (one-time)
    void    LayoutSkeleton();     // re-runs DUI layout against current client
    RECT    GetButtonRectScreen(int which) const;   // 0=min 1=max 2=close

private:
    DuiFrameTitleBar* m_titleBar = nullptr;     // owned by DuiHost root
    DuiVBox*          m_skeleton = nullptr;     // root layout
    DuiControl*       m_clientContent = nullptr;

    CString  m_title;
    int      m_titleH    = 36;
    HBITMAP  m_titleIcon = nullptr;
    bool     m_btnMin    = true;
    bool     m_btnMax    = true;
    bool     m_btnClose  = true;
    int      m_borderPx  = 8;
    bool     m_resizable = true;
    int      m_minW      = 200;
    int      m_minH      = 150;
    int      m_maxW      = 0;          // 0 = 不限
    int      m_maxH      = 0;          // 0 = 不限
    int      m_nextCaptionIconId = 1;  // 单调递增的 caption icon id
    bool     m_titleTransparent = false;
    COLORREF m_titleTextColor   = RGB(40, 40, 40);
    COLORREF m_captionGlyphOverride = CLR_INVALID;
    bool     m_isMaximized      = false;   // tracked from WM_SIZE; propagated to max button glyph
};

} // namespace balloonwjui

#endif // BUI_FEATURE_FRAMEWINDOW
