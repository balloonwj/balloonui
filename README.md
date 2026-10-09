# balloonui

**语言：** 中文 | [English](README_en.md)

一套面向 Windows 桌面应用的无 HWND DUI（DirectUI）控件库，配套若干完整 Demo
和一个全控件展示窗 (DuiGallery)。整套实现用 Visual Studio 2022 + WTL 10.1 +
Win32 API，无外部依赖，开箱即用。

![NewChatDemo 主界面](docs/images/NewChatDemo_final.png)

---

## 这个库解决什么问题

传统 Win32/WTL 应用每个控件都是一个独立 HWND，导致：

- 复杂界面 HWND 数量爆炸，重绘和事件路由开销大；
- 半透明、动画、自绘特效难做（HWND 子窗口互相遮挡 DC）；
- 高 DPI 适配要逐控件处理。

balloonui 走 **"宿主一个 HWND，子控件全部 DUI"** 的路线：

- 一个 `DuiHost` 持有唯一的 HWND，整棵控件树挂在它的客户区里；
- 子控件由 `DuiControl` 派生，无 HWND，事件通过 `WM_DUI_NOTIFY` 冒泡给父
  窗口；
- 依赖系统输入法的控件也**没有例外**：普通输入框 `DuiEdit` 与富文本
  `DuiRichEdit` 都走无窗口路线，直接驱动系统的 RichEdit 排版引擎，输入法
  上下文与光标由控件自己提供，因此它们都是 DUI 树里的普通一员。库内不再
  提供把真窗口接入控件树的设施，所有控件都由库直接绘制、都受父容器裁剪、
  也都按同一套绘制层级参与绘制。

![DUI 控件树结构](docs/images/ctl-host-tree.png)

---

## 控件总览

库内置 30+ 个控件，按职责分七大类，每类在 `balloonui/Controls/` 下有独立
子目录：

| 类别 | 控件 |
|---|---|
| Basic（基础） | `DuiLabel` `DuiButton` `DuiAvatar` `DuiBadge` `DuiSeparator` `DuiGroupBox` `DuiToast` |
| Input（输入） | `DuiEdit` `DuiRichEdit` `DuiSearchBox` `DuiSpinBox` `DuiComboBox` `DuiSlider` `DuiSwitch` |
| List（列表 / 容器） | `DuiListBox` `DuiVirtualList` `DuiTreeView` `DuiTab` `DuiTabPage` `DuiMenu` `DuiMenuBar` |
| Layout（布局） | `DuiLayout` (`DuiVBox` / `DuiHBox` / `DuiGrid`) `DuiDock` `DuiSplitter` |
| Feedback（反馈） | `DuiProgressBar` `DuiToolTip` `DuiPopupHost` `DuiEmojiPanel` |
| Media（多媒体） | `DuiGif` `DuiImageOle` |
| Window（窗口 / 系统件） | `DuiFrameWindow` `DuiScrollBar` |

### 按钮：品牌色 + 多形态

PushButton 默认走品牌蓝 `#2D6CDF` + 8px 圆角，hover / pressed / disabled
状态自动派生；同一 `DuiButton` 类还能切到 Checkbox / Radio / Icon 三种形
态，共用一套点击、聚焦、热区逻辑。

![按钮风格总览](docs/images/ctl-button-styles-overview.png)
![PushButton 状态](docs/images/ctl-button-pushbutton-states.png)

### 输入：无窗口实现 + 原生 IME 支持 + 复合控件

普通输入框 `DuiEdit` 是富文本控件 `DuiRichEdit` 的子类，两者都不建子窗口，
直接驱动系统的 RichEdit 排版引擎，输入法上下文、光标与滚动条都由控件自己
接管，因此它们能被其它控件遮挡、能放进滚动容器、能用透明背景、也能随内容
自动增高；密码、多行、只读这些属性还可以在运行期随时切换，不需要销毁重建
控件。`DuiEdit` 在基类之上补齐了普通输入框特有的部分：单行回车不换行、
左右内联图标栏、密码显隐按钮、单行文字垂直居中。

SearchBox、SpinBox、ComboBox 都是在输入框之上拼装出来的复合控件。

![Edit 状态](docs/images/ctl-edit-states.png)
![SearchBox 状态](docs/images/ctl-searchbox-states.png)
![Switch 状态](docs/images/ctl-switch-states.png)

### 列表 / 树：四象限渲染 + 冻结列

`DuiTreeView` 同时支持单层多列表格和多层树，每列可选 6 种 cell 类型
（TEXT / ICON / IMAGE / CHECKBOX / PROGRESS / HYPERLINK），并支持冻结左侧
N 列 / 顶部 N 行、表头点击排序、单元格 inline 编辑——足以撑起任务管理
器级别的数据视图。

![TreeView 状态](docs/images/ctl-treeview-states.png)
![TreeView 多列](docs/images/ctl-treeview-multicol.png)
![ListBox 多选](docs/images/ctl-listbox-multi.png)

### 布局：声明式 + 多形态

VBox / HBox / Grid 三件套覆盖常规线性 / 网格布局；Dock 覆盖"上下左右 +
中间填充"的 IDE 风格；Splitter 提供可拖动分隔条。布局既能用 C++ 链式
`AddChild` 构造，也能用 XML 声明式描述并交给 `DuiXmlBuilder` 解析。

![登录页布局](docs/images/ctl-layout-login.png)
![三栏布局](docs/images/ctl-layout-three-pane.png)
![Dock 五区](docs/images/ctl-dock-five-zones.png)

### Tab：横版 + 竖版 + 自动切页

`DuiTab` 是纯标签条（只管"选中"），`DuiTabPage` 在它的基础上接管多个内容
页的 show/hide。

![横版 Tab](docs/images/ctl-tab-horizontal.png)
![竖版 Tab](docs/images/ctl-tab-vertical.png)

### 反馈：进度条 / 弹窗 / Emoji 面板

![ProgressBar 状态](docs/images/ctl-progressbar-states.png)
![EmojiPanel 默认](docs/images/ctl-emojipanel-default.png)
![Avatar 网格](docs/images/ctl-avatar-grid.png)
![Badge 类型](docs/images/ctl-badge-types.png)

---

## 顶层窗口：DuiFrameWindow

`DuiFrameWindow` 提供完整的无边框顶层窗口：9-grid 拉伸背景、自绘标题栏、
min / max / close 按钮、`WM_NCHITTEST` 边角拖拽 resize、客户区 layout 嵌入
——业务侧用 `DuiXmlBuilder::FromFrameXml` 解析一段 `<frame-window>` XML，
就能同时得到窗口配置与客户区控件树（用法见下文「快速开始」）。

![标题栏全貌](docs/images/demo_titlebar_full.png)
![9-grid 背景](docs/images/bg-9grid-medium.png)
![多窗口示例](docs/images/frame-4-windows-2x2.png)

---

## GDI+ 抗锯齿

非轴对齐图形（三角形、菱形、箭头、圆形、对角线）必须走
`DuiPaintAA` 提供的辅助函数（`DuiAA::FillPolygon` / `DuiAA::FillEllipse` /
`DuiAA::DrawLine`），内部用 GDI+ 抗锯齿处理。普通 GDI `Polygon`/`Ellipse`/
`LineTo` 在斜线上有可见锯齿，库内已禁用。

![GDI+ 抗锯齿 对比](docs/images/ctl-paintaa-comparison.png)

---

## 主题与配色

`DuiTheme` / `DuiResMgr` 把颜色、字体、间距集中托管。默认 UI 字体为
**微软雅黑 9pt**，字体名由 `DuiTheme` 管理，宿主可以在启动时用
`DuiTheme::Inst().SetDefaultFontFace()` 按界面语言改换（须在第一次取字体之前
调用）。字体按 DPI 缓存在 `DuiResMgr` 里，控件经 `DuiControl::GetDefaultFont()`
按自己所在窗口的 DPI 取用，窗口移到缩放比例不同的显示器后字号随之变化。

![主题色板](docs/images/ctl-theme-swatches.png)

---

## XML 声明式布局

复杂界面可以用一段 XML 描述，交给 `DuiXmlBuilder` 解析成控件树，省掉手写
长长的 `AddChild` 链：

```xml
<frame-window title="Settings" min-w="480" min-h="240">
  <vbox padding="12" gap="8">
    <label text="账号设置" fixedHeight="24"/>
    <hbox gap="8" fixedHeight="28">
      <label text="用户名" fixedWidth="60"/>
      <edit id="101" fixedWidth="240"/>
    </hbox>
    <button id="102" text="保存" fixedWidth="96" fixedHeight="32" alignCross="near"/>
  </vbox>
</frame-window>
```

几条容易写错的约定：`id` 是数字（事件里按它区分控件，1~3 留给标题栏的
最小化 / 最大化 / 关闭按钮）；子控件间距用 `gap`；尺寸用 `fixedWidth` /
`fixedHeight`，竖直布局里设宽度（水平布局里设高度）要同时写 `alignCross`，
否则会被拉满；窗口本身的宽高在代码里用 `ResizeClient` 设置。

每个内置 tag 都对应一个内置控件类；业务可以通过 `CustomFactory` 扩展自己
的 tag。

---

## 按需裁剪 (Feature Strip)

`BalloonUiFeatures.h` 提供细粒度的 `BUI_DISABLE_XXX` 宏，业务侧可以只编入
用到的控件，把未用控件的 `.cpp` 整体排除出编译，显著减少最终 exe / DLL 体
积。依赖关系（如 `SCROLLBAR` 关掉会连带关 `LISTBOX` / `TREEVIEW`；`EDIT`
依赖 `RICHTEXT`，因为 `DuiEdit` 派生自 `DuiRichEdit`）由头文件内的
`#if/#error` 强制一致，避免静默编译出半残的库。

详见 `balloonui/BalloonUiFeatures.h` 顶部说明，以及 `docs/guides.html` 的
"按需裁剪 (Feature Strip)" 章节。

---

## 同仓附带的工程

| 工程 | 路径 | 说明 |
|---|---|---|
| balloonui | `balloonui/` | 控件库本体，`balloonui.vcxproj` 编成 DLL，`balloonui_static.vcxproj` 编成静态库 |
| DuiGallery | `DuiGallery/` | 全控件演示窗，每个控件都有单独的展示页 |
| NewChatDemo | `NewChatDemo/` | 完整的聊天界面 Demo（XML 布局 + 自定义控件） |
| CloudMelodyDesktop | `CloudMelodyDesktop/` | 桌面音乐播放器 Demo（多页面 + 多媒体素材） |
| XChat | `XChat/` | 仿某信 PC 客户端界面的 Demo（登录、多视图主面板、设置页） |
| DemoTaskManager | `DemoTaskManager/` | 任务管理器风格 Demo（多列 TreeView + 多 Tab） |
| DemoChatBubble | `DemoChatBubble/` | 聊天气泡控件 Demo |
| DemoCircularProgress | `DemoCircularProgress/` | 圆形进度环 Demo |
| DemoFileTypeIcon | `DemoFileTypeIcon/` | 文件类型图标 Demo |
| DemoNinePatchBg | `DemoNinePatchBg/` | 9-grid 背景拉伸 Demo |
| DemoTextBadgeTile | `DemoTextBadgeTile/` | 文字徽章瓦片 Demo |
| DemoTreeViewLargeData | `DemoTreeViewLargeData/` | TreeView 大数据量性能 Demo |

打开根目录 `Demos.sln` 即可一次性加载 DLL 版控件库与上表全部 Demo。静态库工程
`balloonui/balloonui_static.vcxproj` 不在其中，由使用方自己的解决方案引用。

### Demo 截图

NewChatDemo / CloudMelodyDesktop / 任务管理器演示了"完整应用"级别的能力：

![NewChat 聊天界面](docs/images/NewChatDemo_final.png)
![CloudMelody 正在播放](docs/images/cloudmelody/now-playing.png)
![CloudMelody 发现页](docs/images/cloudmelody/discover.png)
![任务管理器 进程](docs/images/demo-taskmgr-processes.png)
![任务管理器 性能](docs/images/demo-taskmgr-performance.png)

小型 Demo 演示了单个控件 / 单类元素：

![聊天气泡](docs/images/demo-chatbubble-out-long.png)
![圆形进度](docs/images/demo-circprog-p66.png)
![文件图标 PDF](docs/images/demo-fileicon-pdf.png)
![文字徽章](docs/images/demo-textbadge-success.png)

---

## 构建要求

- Visual Studio 2022（也支持 2019，需自行降级 toolset）
- WTL 10.1（仓内 `wtl10.1/` 已含一份）
- 平台：**Win32 (x86/x64)**，Debug / Release 均可
- 运行平台：Windows 7 及以上

---

## 快速开始

```cpp
#include "balloonui/DuiXmlBuilder.h"
#include "balloonui/Controls/Window/DuiFrameWindow.h"

using namespace balloonwjui;

// XML 里按钮的 id。1~3 被标题栏的最小化 / 最大化 / 关闭按钮占用，业务控件不要用。
const UINT kIdOk = 101;

// 主窗口。控件的通知以 WM_DUI_NOTIFY 发给窗口自己：wParam 是控件 id，
// lParam 是 DuiNotify*。
class HelloFrame : public DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(HelloFrame)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)
        CHAIN_MSG_MAP(DuiFrameWindow)
    END_MSG_MAP()

    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
    {
        const DuiNotify* n = reinterpret_cast<const DuiNotify*>(lParam);
        if (n->code == DUIN_CLICK && n->ctrlId == kIdOk)
        {
            PostMessage(WM_CLOSE);
            return 0;
        }
        // 其余通知交回基类：标题栏三个按钮的点击由 DuiFrameWindow 处理
        bHandled = FALSE;
        return 0;
    }
};

// 建窗（例如在 WinMain 里，WTL 的 _Module 已初始化之后）：
const char* kXml =
    "<frame-window title=\"Hello\" min-w=\"320\" min-h=\"200\">"
    "  <vbox padding=\"16\" gap=\"8\">"
    "    <label text=\"Hello, balloonui!\" fixedHeight=\"24\"/>"
    "    <button id=\"101\" text=\"OK\" fixedWidth=\"88\" fixedHeight=\"32\" alignCross=\"near\"/>"
    "  </vbox>"
    "</frame-window>";

DuiFrameWindowConfig cfg;
std::unique_ptr<DuiControl> root = DuiXmlBuilder::FromFrameXml(kXml, cfg);

HelloFrame frame;
frame.Create(NULL, CWindow::rcDefault, _T("Hello"), WS_OVERLAPPEDWINDOW);
frame.ApplyConfig(cfg);                    // 窗口属性要在 Create 之后应用
frame.SetClientContent(std::move(root));   // <frame-window> 里的子树作为客户区
frame.ResizeClient(320, 200);              // 整窗尺寸，标题栏也在其内
frame.ShowWindow(SW_SHOW);
```

更完整的示例可直接打开 DuiGallery 工程跑一下，每个控件都能在 Gallery 里
单独触发、查看状态机。

---

## 测试

控件库的单元测试在 `balloonui/Tests/` 下，编进 DuiGallery 工程（`Debug|Win32`）。
Debug 版 `Bin/DuiGallery.exe` 启动时会跑完全部用例，并把结果写到
`%TEMP%\DuiGallery_tests.log`，每组用例一行 `[summary] ... passed=N failed=M`，
失败的用例以 `[FAIL]` 开头。所以不必看窗口：启动它，等日志出现，再结束进程即可。
`DuiRichEditCaretTests` 的 `SystemCaretLeavesNoPixelsOnScreen` 要从真实屏幕截图，
在隐藏桌面等看不到屏幕的环境里运行时会报 `screen capture failed`。

---

## 目录结构

```
balloonui/（仓库根目录）
├── balloonui/                    控件库源码
│   ├── Controls/                 按类别分七大子目录
│   ├── Tests/                    单元 / 集成测试（编进 DuiGallery）
│   ├── BalloonUiApi.h            DLL 导入 / 导出宏
│   ├── BalloonUiFeatures.h       按需裁剪开关
│   ├── DuiHost / DuiControl / DuiLayout / DuiTheme ...   kernel
│   ├── balloonui.vcxproj         编成 DLL
│   └── balloonui_static.vcxproj  编成静态库
├── DuiGallery/                   全控件演示窗
├── NewChatDemo/                  聊天界面 Demo
├── CloudMelodyDesktop/           音乐播放器 Demo
├── XChat/                        仿某信 PC 客户端 Demo
├── Demo*/                        小型单点 Demo
├── docs/
│   ├── guides.html / guides_en.html          配套指南（源文件）
│   ├── guides.md / guides_en.md              同上的 Markdown 版，由 html_to_md.py 生成
│   ├── windowless-richedit*.md               无窗口富文本控件的设计与测试记录
│   └── images/                               文档配图
├── wtl10.1/                      仓内 WTL 10.1（含其 ReadMe.html）
├── Bin/                          输出目录
├── clear.bat                     清理编译产物（git clean -fdX）
└── Demos.sln                     加载 DLL 版控件库与全部 Demo
```

---

## 完整使用文档

更详细的使用手册（16 章 + 附录，覆盖控件参考、XML 布局、事件路由、自绘控件、
完整布局示例、9-grid 背景、Feature Strip、3 个综合案例与术语表）：

- Markdown 版本（GitHub 等平台可直接渲染）
  - 中文：[`docs/guides.md`](docs/guides.md)
  - English: [`docs/guides_en.md`](docs/guides_en.md)
- HTML 版本（带左侧导航 + scrollspy，本地浏览器打开体验最好）
  - 中文：[`docs/guides.html`](docs/guides.html)
  - English: [`docs/guides_en.html`](docs/guides_en.html)

---

## 授权说明

任何人可以免费使用本库（包括商业用途），无需署名，无需任何形式的费用。

代码、Demo、文档配图均按"原样" (AS IS) 提供，作者不对使用过程中产生的
任何后果承担责任。如发现问题或有改进建议，欢迎反馈。