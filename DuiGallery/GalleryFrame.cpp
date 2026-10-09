/**
 *  画廊主窗口的实现。
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "GalleryFrame.h"
#include "PageKit.h"
#include "PageRegistry.h"
#include "GalleryText.h"
#include "GalleryTests.h"

#include "../balloonui/DuiAnimation.h"
#include "../balloonui/DuiTheme.h"
#include "../balloonui/DuiResMgr.h"
#include "../balloonui/Controls/Basic/DuiLabel.h"
#include "../balloonui/Controls/Basic/DuiButton.h"

#include "../balloonui/Tests/DuiNinePatchTests.h"
#include "../balloonui/Tests/DuiButtonTests.h"
#include "../balloonui/Tests/DuiSwitchTests.h"
#include "../balloonui/Tests/DuiAvatarTests.h"
#include "../balloonui/Tests/DuiSplitterTests.h"
#include "../balloonui/Tests/DuiTabPageTests.h"
#include "../balloonui/Tests/DuiTreeViewTests.h"
#include "../balloonui/Tests/DuiPopupHostTests.h"
#include "../balloonui/Tests/DuiEmojiPanelTests.h"
#include "../balloonui/Tests/DuiFrameWindowTests.h"
#include "../balloonui/Tests/DuiDpiTests.h"
#include "../balloonui/Tests/DuiImageOleTests.h"
#include "../balloonui/Tests/DuiDockTests.h"
#include "../balloonui/Tests/DuiAnimationTests.h"
#include "../balloonui/Tests/DuiAsyncImageTests.h"
#include "../balloonui/Tests/DuiGifTests.h"
#include "../balloonui/Tests/DuiDropTargetTests.h"
#include "../balloonui/Tests/DuiXmlBuilderTests.h"
#include "../balloonui/Tests/DuiTier3Tests.h"
#include "../balloonui/Tests/DuiThemeTests.h"
#include "../balloonui/Tests/DuiListBoxTests.h"
#include "../balloonui/Tests/DuiMenuTests.h"
#include "../balloonui/Tests/DuiMenuBarTests.h"
#include "../balloonui/Tests/DuiTabTests.h"
#include "../balloonui/Tests/DuiMnemonicTests.h"
#include "../balloonui/Tests/DuiTier4Tests.h"
#include "../balloonui/Tests/DuiInspectorGoldenTests.h"
#include "../balloonui/Tests/DuiComboBoxTests.h"
#include "../balloonui/Tests/DuiLabelTests.h"
#include "../balloonui/Tests/DuiEditTests.h"
#include "../balloonui/Tests/DuiSearchBoxTests.h"
#include "../balloonui/Tests/DuiLayoutTests.h"
#include "../balloonui/Tests/DuiHostTests.h"
#include "../balloonui/Tests/DuiCaretTests.h"
#include "../balloonui/Tests/DuiTextHostTests.h"
#include "../balloonui/Tests/DuiRichEditTests.h"
#include "../balloonui/Tests/DuiRichEditCaretTests.h"
#include "../balloonui/Tests/DuiScrollBarTests.h"
#include "../balloonui/Tests/DuiSmallControlsTests.h"
#include "../balloonui/Tests/DuiToolTipTests.h"

using namespace balloonwjui;
using namespace Gallery;

namespace {

// ---- 工具条上各控件的编号 ----
//
// 每个能发通知的控件都要有自己的编号：balloonui 的通知只带 code 与 ctrlId，
// 而按钮发的都是同一个 DUIN_CLICK，不编号就分不出是谁被点了。
enum ToolBarCtrlId
{
    // 语言切换按钮。
    kIdLangButton = 201,
    // 切换到浅色主题。
    kIdThemeLight = 202,
    // 切换到深色主题。
    kIdThemeDark = 203,
    // 切换到高对比度主题。
    kIdThemeHighContrast = 204,
    // 分隔导航栏与内容区的分隔条。
    kIdSplitter = 205,
    // 右侧内容区的滚动视图。
    kIdContentScroll = 206,
};

// ---- 版式常量 ----

// 顶部工具条的高度（像素）。
const int kToolBarHeight = 40;
// 工具条四边的内边距（像素）。
const int kToolBarPadding = 10;
// 工具条内各控件之间的间距（像素）。
const int kToolBarGap = 6;
// 语言切换按钮的宽度（像素）。要装得下"English"这样较长的文字。
const int kLangButtonWidth = 84;
// 主题切换按钮的宽度（像素）。
const int kThemeButtonWidth = 78;
// 左侧导航栏的初始宽度（像素）。
// 取值依据是实测：页面标题最长的那几条（例如「SearchBox / SpinBox　搜索与
// 微调」）在 236 像素下会被截断成省略号，280 像素下能完整显示。
const int kNavInitialWidth = 280;
// 导航栏允许被拖到的最小宽度（像素）。再窄就装不下带缩进的页面标题了。
const int kNavMinWidth = 180;
// 右侧内容区允许被压缩到的最小宽度（像素）。
const int kContentMinWidth = 320;
// 分隔条的厚度（像素）。
const int kSplitterThickness = 4;
// 主窗口标题文字的字号（磅值）。
const int kTitlePointSize = 12;

} // 匿名命名空间

GalleryFrame::GalleryFrame()
    : m_pNav(NULL)
    , m_pContent(NULL)
    , m_pSplitter(NULL)
{
}

GalleryFrame::~GalleryFrame() = default;

// 这里刻意不安装动画用的定时器。DuiAnimMgr 自带一个 16 毫秒的共享定时器，
// 它在活跃动画列表由空变为非空时自动装上、最后一个动画结束后自动撤下，
// 所以开关、浮动提示条、动图、悬停渐变等等都能自己推进，不需要本窗口帮忙。

int GalleryFrame::OnCreate(LPCREATESTRUCT)
{
    CRect rcClient;
    GetClientRect(&rcClient);
    m_host.Create(m_hWnd, rcClient, nullptr,
                  WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0);
    BuildRoot();
    SwitchToPage(GetDefaultPageId());
    RunAllTests();
    return 0;
}

void GalleryFrame::OnDestroy()
{
    // 动画管理器的活跃列表里可能还留着指向控件树内部对象的回调，必须在拆掉
    // 宿主之前清空，否则一次迟到的推进会访问已经析构的内存。清空的同时也会
    // 撤下管理器自己的定时器。
    DuiAnimMgr::Inst().Clear();
    // 当前页面注册的通知钩子持有页面内控件的裸指针，页面即将销毁，一并清掉。
    Gallery::g_pageNotifyHook = NULL;
    if (m_host.IsWindow())
    {
        m_host.DestroyWindow();
    }
    ::PostQuitMessage(0);
}

void GalleryFrame::OnSize(UINT, CSize size)
{
    if (m_host.IsWindow())
    {
        m_host.SetWindowPos(NULL, 0, 0, size.cx, size.cy,
                            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

BOOL GalleryFrame::OnEraseBkgnd(CDCHandle)
{
    return TRUE;
}

std::unique_ptr<DuiControl> GalleryFrame::BuildToolBar()
{
    // 工具条要有自己的底色，才能与下方的内容区分开。底色能力只在竖直布局
    // 容器上有（DuiVBox 的卡片样式），水平布局容器没有，所以外面套一层
    // 只装这一条的竖直容器。
    std::unique_ptr<DuiVBox> barHolder(new DuiVBox());
    barHolder->SetBgColor(DuiTheme::Inst().Get(DuiTheme::SurfaceBg));

    std::unique_ptr<DuiHBox> bar(new DuiHBox());
    bar->SetPadding(kToolBarPadding);
    bar->SetGap(kToolBarGap);

    std::unique_ptr<DuiLabel> title(new DuiLabel());
    title->SetText(Txt(_T("balloonui 控件画廊"), _T("balloonui Control Gallery")));
    title->SetTextColor(DuiTheme::Inst().Get(DuiTheme::TextDefault));
    title->SetFont(DuiResMgr::Inst().GetFontByPointSize(kTitlePointSize, true));
    // 标题占据剩余空间，把后面几个按钮挤到右边。
    bar->AddChild(std::move(title), DuiLayout::Hint().Weight(1));

    // 语言按钮上写的是"点了会切换到哪种语言"，而不是当前是哪种语言 ——
    // 后者会让人不确定点下去是切换还是确认。
    std::unique_ptr<DuiButton> lang(new DuiButton());
    lang->SetCtrlId(kIdLangButton);
    lang->SetButtonType(DuiButton::StylePushButton);
    lang->SetText(CurrentLanguage() == LangChinese ? _T("English") : _T("中文"));
    bar->AddChild(std::move(lang), DuiLayout::Hint().Fixed(kLangButtonWidth));

    std::unique_ptr<DuiButton> light(new DuiButton());
    light->SetCtrlId(kIdThemeLight);
    light->SetText(Txt(_T("浅色"), _T("Light")));
    bar->AddChild(std::move(light), DuiLayout::Hint().Fixed(kThemeButtonWidth));

    std::unique_ptr<DuiButton> dark(new DuiButton());
    dark->SetCtrlId(kIdThemeDark);
    dark->SetText(Txt(_T("深色"), _T("Dark")));
    bar->AddChild(std::move(dark), DuiLayout::Hint().Fixed(kThemeButtonWidth));

    std::unique_ptr<DuiButton> highContrast(new DuiButton());
    highContrast->SetCtrlId(kIdThemeHighContrast);
    highContrast->SetText(Txt(_T("高对比度"), _T("Contrast")));
    bar->AddChild(std::move(highContrast), DuiLayout::Hint().Fixed(kThemeButtonWidth));

    barHolder->AddChild(std::move(bar), DuiLayout::Hint().Weight(1));
    return std::unique_ptr<DuiControl>(barHolder.release());
}

void GalleryFrame::BuildRoot()
{
    std::unique_ptr<DuiVBox> root(new DuiVBox());
    root->SetBgColor(DuiTheme::Inst().Get(DuiTheme::SurfaceAltBg));

    root->AddChild(BuildToolBar(), DuiLayout::Hint().Fixed(kToolBarHeight));

    // 竖向分隔条：分隔条本身纵向跑，两个面板左右并排，与 Win32 的命名一致。
    std::unique_ptr<DuiSplitter> splitter(new DuiSplitter());
    splitter->SetCtrlId(kIdSplitter);
    splitter->SetOrientation(DuiSplitter::Vertical);
    splitter->SetBarThickness(kSplitterThickness);
    splitter->SetMinSizes(kNavMinWidth, kContentMinWidth);
    splitter->SetSplitPx(kNavInitialWidth);

    std::unique_ptr<GalleryNav> nav(new GalleryNav());
    m_pNav = nav.get();
    // 导航栏的内部控件必须在整棵树交给宿主**之前**建好。
    //
    // 反过来的顺序（先 SetRoot、再往导航栏里加控件）行不通：SetRoot 会立刻
    // 排一次版，此后新加的子控件只有再排一次才有矩形，而分隔条给两个面板
    // 设矩形用的是 SetRect —— 它在矩形没有变化时直接返回。切换语言或主题时
    // 重建控件树，导航栏拿到的矩形与上一次一模一样，于是那次返回把重排整个
    // 挡掉了，表现是左侧导航栏一片空白。
    //
    // 先建后挂就没有这个问题：SetRoot 会把宿主指针递归发给每一个控件，并
    // 完成第一次排列。
    m_pNav->BuildContents();
    splitter->SetPane(0, std::unique_ptr<DuiControl>(nav.release()));

    std::unique_ptr<DuiScrollView> content(new DuiScrollView());
    content->SetCtrlId(kIdContentScroll);
    // 页面容器会如实报告自己有多高，所以让滚动视图自动跟随内容高度。
    content->SetAutoContentHeight(true);
    m_pContent = content.get();
    splitter->SetPane(1, std::unique_ptr<DuiControl>(content.release()));

    m_pSplitter = splitter.get();
    root->AddChild(std::move(splitter), DuiLayout::Hint().Weight(1));

    m_host.SetRoot(std::move(root));

    // 切换语言或主题时是就地重建，客户区尺寸没有变化，不会有 WM_SIZE 顺带
    // 把版重排一次，所以这里显式排一次。
    RelayoutRoot();
}

void GalleryFrame::RelayoutRoot()
{
    if (!m_host.IsWindow())
    {
        return;
    }
    DuiControl* root = m_host.GetRoot();
    if (root == NULL)
    {
        return;
    }
    CRect rcClient;
    m_host.GetClientRect(&rcClient);
    if (rcClient.IsRectEmpty())
    {
        return;
    }
    root->ForceLayout(rcClient);
    m_host.Invalidate(FALSE);
}

void GalleryFrame::SwitchToPage(LPCTSTR pageId)
{
    if (pageId == NULL || m_pContent == NULL)
    {
        return;
    }
    const PageEntry* entry = FindPageById(pageId);
    if (entry == NULL || entry->build == NULL)
    {
        return;
    }

    // 旧页面注册的通知钩子里持有旧页面控件的裸指针，而这些控件马上就要随
    // SetContent 一起销毁，所以必须先清掉。需要监听通知的新页面会在自己的
    // 构建函数里重新赋值。
    Gallery::g_pageNotifyHook = NULL;

    // 截图标记同样指向即将销毁的控件。命令行截图模式会在每建完一页之后
    // 立刻取走并清空，交互模式下没人取，这里主动清掉避免越积越多。
    GetCaptureMarks().clear();

    // 清空动画管理器的活跃列表。动画对象持有指向页面内控件的回调，使用者
    // 在动画没跑完时切走页面，下一次脉冲就会写向已经释放的内存。清空必须
    // 排在构建新页面之前 —— 新页面在构建过程中可能自己就启动了动画
    // （动图控件就是一例），先建后清会把新页面的动画一起清掉。
    DuiAnimMgr::Inst().Clear();

    std::unique_ptr<DuiControl> content = entry->build();
    m_pContent->SetContent(std::move(content));
    m_pContent->SetScrollPos(0);
    m_pContent->Invalidate();

    m_currentPageId = pageId;
    if (m_pNav != NULL)
    {
        m_pNav->SelectPage(pageId);
    }
}

void GalleryFrame::RebuildAll()
{
    // 记下当前状态，重建之后原样恢复，免得切一次语言就把导航栏宽度和
    // 当前所在的页面一起丢掉。
    CString pageId = m_currentPageId;
    int splitPx = kNavInitialWidth;
    if (m_pSplitter != NULL)
    {
        splitPx = m_pSplitter->GetSplitPx();
    }

    Gallery::g_pageNotifyHook = NULL;
    GetCaptureMarks().clear();
    m_pNav = NULL;
    m_pContent = NULL;
    m_pSplitter = NULL;
    m_currentPageId.Empty();

    BuildRoot();

    if (m_pSplitter != NULL)
    {
        m_pSplitter->SetSplitPx(splitPx);
    }
    if (pageId.IsEmpty())
    {
        pageId = GetDefaultPageId();
    }
    SwitchToPage(pageId);
    RelayoutRoot();
}

LRESULT GalleryFrame::OnRebuildAllMsg(UINT, WPARAM, LPARAM)
{
    RebuildAll();
    return 0;
}

LRESULT GalleryFrame::OnDuiNotify(UINT, WPARAM, LPARAM lParam)
{
    DuiNotify* pNotify = reinterpret_cast<DuiNotify*>(lParam);
    if (pNotify == NULL)
    {
        return 0;
    }

    // 导航栏优先处理：它内部的树与搜索框发的都是 DUIN_VALUECHANGED，
    // 由它按控件编号自己分辨。返回真表示选中的页面变了。
    if (m_pNav != NULL && m_pNav->HandleNotify(pNotify))
    {
        // 这里就地换页是安全的：通知来自导航树，而被销毁的是右侧内容区，
        // 两者不在同一棵子树上。
        SwitchToPage(m_pNav->GetSelectedPageId());
        return 0;
    }

    // 语言切换。重建要绕一道消息，理由见头文件里 kMsgRebuildAll 的说明。
    if (pNotify->code == DUIN_CLICK && pNotify->ctrlId == kIdLangButton)
    {
        SetCurrentLanguage(CurrentLanguage() == LangChinese ? LangEnglish : LangChinese);
        PostMessage(kMsgRebuildAll, 0, 0);
        return 0;
    }

    // 三档主题预设。切换之后同样要重建 —— 画廊自己的版式颜色是在构建控件时
    // 从主题取的，已经建好的控件不会自己跟着变。
    if (pNotify->code == DUIN_CLICK && pNotify->ctrlId == kIdThemeLight)
    {
        DuiTheme::Inst().ApplyPreset(DuiTheme::Light);
        PostMessage(kMsgRebuildAll, 0, 0);
        return 0;
    }
    if (pNotify->code == DUIN_CLICK && pNotify->ctrlId == kIdThemeDark)
    {
        DuiTheme::Inst().ApplyPreset(DuiTheme::Dark);
        PostMessage(kMsgRebuildAll, 0, 0);
        return 0;
    }
    if (pNotify->code == DUIN_CLICK && pNotify->ctrlId == kIdThemeHighContrast)
    {
        DuiTheme::Inst().ApplyPreset(DuiTheme::HighContrast);
        PostMessage(kMsgRebuildAll, 0, 0);
        return 0;
    }

    // 剩下的通知转给当前页面注册的钩子。没有页面登记时钩子为空，直接忽略。
    if (Gallery::g_pageNotifyHook)
    {
        Gallery::g_pageNotifyHook(pNotify);
    }
    return 0;
}

void GalleryFrame::RunAllTests()
{
    // 把全部单元测试跑一遍，结果同时写到调试输出与临时目录下的日志文件。
    // 有了日志文件，构建机或者命令行环境下不必附加调试器、也不必看界面，
    // 起进程等日志出现即可确认结果。日志内容限定为 ASCII，所以按窄字符写。
    CString report = DuiNinePatchTests::RunAll();
    report += _T("\r\n");
    report += DuiButtonTests::RunAll();
    report += _T("\r\n");
    report += DuiSwitchTests::RunAll();
    report += _T("\r\n");
    report += DuiAvatarTests::RunAll();
    report += _T("\r\n");
    report += DuiSplitterTests::RunAll();
    report += _T("\r\n");
    report += DuiTabPageTests::RunAll();
    report += _T("\r\n");
    report += DuiTreeViewTests::RunAll();
    report += _T("\r\n");
    report += DuiPopupHostTests::RunAll();
    report += _T("\r\n");
    report += DuiEmojiPanelTests::RunAll();
    report += _T("\r\n");
    report += DuiFrameWindowTests::RunAll();
    report += _T("\r\n");
    report += DuiDpiTests::RunAll();
    report += _T("\r\n");
    report += DuiImageOleTests::RunAll();
    report += _T("\r\n");
    report += DuiDockTests::RunAll();
    report += _T("\r\n");
    report += DuiAnimationTests::RunAll();
    report += _T("\r\n");
    report += DuiAsyncImageTests::RunAll();
    report += _T("\r\n");
    report += DuiGifTests::RunAll();
    report += _T("\r\n");
    report += DuiDropTargetTests::RunAll();
    report += _T("\r\n");
    report += DuiXmlBuilderTests::RunAll();
    report += _T("\r\n");
    report += DuiTier3Tests::RunAll();
    report += _T("\r\n");
    report += DuiThemeTests::RunAll();
    report += _T("\r\n");
    report += DuiListBoxTests::RunAll();
    report += _T("\r\n");
    report += DuiMenuTests::RunAll();
    report += _T("\r\n");
    report += DuiMenuBarTests::RunAll();
    report += _T("\r\n");
    report += DuiTabTests::RunAll();
    report += _T("\r\n");
    report += DuiMnemonicTests::RunAll();
    report += _T("\r\n");
    report += DuiTier4Tests::RunAll();
    report += _T("\r\n");
    report += DuiInspectorGoldenTests::RunAll();
    report += _T("\r\n");
    report += DuiComboBoxTests::RunAll();
    report += _T("\r\n");
    report += DuiLabelTests::RunAll();
    report += _T("\r\n");
    report += _T("\r\n");
    // 无窗口普通输入框被裁掉时，这套用例整个不存在，连同它的 RunAll 一起
    // 消失，所以调用点也要跟着裁。
#if BUI_FEATURE_EDIT
    report += DuiEditTests::RunAll();
    report += _T("\r\n");
#endif
    report += DuiSearchBoxTests::RunAll();
    report += _T("\r\n");
    report += DuiLayoutTests::RunAll();
    report += _T("\r\n");
    report += DuiHostTests::RunAll();
    report += _T("\r\n");
    report += DuiCaretTests::RunAll();
    report += _T("\r\n");
    // 无窗口富文本控件被裁掉时，这两套用例整个不存在，连同它们的
    // RunAll 一起消失，所以调用点也要跟着裁。
#if BUI_FEATURE_RICHTEXT
    report += DuiTextHostTests::RunAll();
    report += _T("\r\n");
    report += DuiRichEditTests::RunAll();
    report += _T("\r\n");
    report += DuiRichEditCaretTests::RunAll();
    report += _T("\r\n");
#endif
    report += DuiScrollBarTests::RunAll();
    report += _T("\r\n");
    report += DuiSmallControlsTests::RunAll();
    report += _T("\r\n");
    report += DuiToolTipTests::RunAll();
    report += _T("\r\n");
    // 画廊自己的用例：页面注册表、中英文切换、导航搜索的匹配规则，以及
    // 段落说明按宽度自动换行这一机制。
    report += Gallery::GalleryTests::RunAll();

    int start = 0;
    while (start < report.GetLength())
    {
        int crlf = report.Find(_T("\r\n"), start);
        CString line = (crlf < 0)
            ? report.Mid(start)
            : report.Mid(start, crlf - start);
        ::OutputDebugString(line);
        ::OutputDebugString(_T("\n"));
        if (crlf < 0)
        {
            break;
        }
        start = crlf + 2;
    }

    TCHAR tmp[MAX_PATH] = {};
    if (::GetTempPath(MAX_PATH, tmp))
    {
        CString path = tmp;
        if (!path.IsEmpty() && path[path.GetLength() - 1] != _T('\\'))
        {
            path += _T("\\");
        }
        path += _T("DuiGallery_tests.log");
        HANDLE h = ::CreateFile(path,
                                GENERIC_WRITE,
                                FILE_SHARE_READ,
                                NULL, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE)
        {
            CStringA ansi(report);
            DWORD wrote = 0;
            ::WriteFile(h, (LPCSTR)ansi, ansi.GetLength(), &wrote, NULL);
            ::CloseHandle(h);
        }
    }
}
