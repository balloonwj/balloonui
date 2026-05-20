# balloonui

**语言：** 中文 | [English](README_en.md)

一套面向 Windows 桌面应用的无 HWND DUI（DirectUI）控件库，配套若干完整 Demo
和一个全控件展示窗 (DuiGallery)。整套实现用 Visual Studio 2022 + WTL 9.0 +
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
- RichEdit / Edit 这类必须依赖系统 IME 的控件，通过 `HwndHostControl`
  适配器以"飞地"形式嵌入，是显式的例外。

![DUI 控件树结构](docs/images/ctl-host-tree.png)

---

## 控件总览

库内置 30+ 个控件，按职责分七大类，每类在 `balloonui/Controls/` 下有独立
子目录：

| 类别 | 控件 |
|---|---|
| Basic（基础） | `DuiLabel` `DuiButton` `DuiAvatar` `DuiBadge` `DuiSeparator` `DuiGroupBox` |
| Input（输入） | `DuiEditHost` `DuiRichEditHost` `DuiSearchBox` `DuiSpinBox` `DuiComboBox` `DuiSlider` `DuiSwitch` |
| List（列表 / 容器） | `DuiListBox` `DuiTreeView` `DuiTab` `DuiTabPage` `DuiMenu` `DuiMenuBar` |
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

### 输入：原生 IME 支持 + 复合控件

EditHost / RichEditHost 内嵌真实的 Win32 EDIT / RICHEDIT，确保中文输入法
工作正常；SearchBox、SpinBox、ComboBox 都是在 EditHost 之上拼装出来的复合
控件。

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
——业务侧通常一句 `DuiXmlBuilder::FromFrameXml(xml)` 就能生成一个完整的主
窗口。

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
**微软雅黑 9pt**（CHINESE_GB2312），所有 DUI 控件统一从 `DuiResMgr::
GetDefaultFont()` 取字体，可一键替换。

![主题色板](docs/images/ctl-theme-swatches.png)

---

## XML 声明式布局

复杂界面可以用一段 XML 描述，交给 `DuiXmlBuilder` 解析成控件树，省掉手写
长长的 `AddChild` 链：

```xml
<frame-window title="Settings" width="640" height="480">
  <vbox padding="12" spacing="8">
    <label text="账号设置"/>
    <hbox spacing="8">
      <label text="用户名"/>
      <edit id="ed_user" width="240"/>
    </hbox>
    <button id="btn_ok" text="保存" style="pushbutton"/>
  </vbox>
</frame-window>
```

每个内置 tag 都对应一个内置控件类；业务可以通过 `CustomFactory` 扩展自己
的 tag。

---

## 按需裁剪 (Feature Strip)

`BalloonUiFeatures.h` 提供细粒度的 `BUI_DISABLE_XXX` 宏，业务侧可以只编入
用到的控件，把未用控件的 `.cpp` 整体排除出编译，显著减少最终 exe / DLL 体
积。依赖关系（如 `SCROLLBAR` 关掉会连带关 `LISTBOX` / `TREEVIEW`）由头文件
内的 `#if/#error` 强制一致，避免静默编译出半残的库。

详见 `balloonui/BalloonUiFeatures.h` 顶部说明，以及 `docs/guides.html` 的
"按需裁剪 (Feature Strip)" 章节。

---

## 同仓附带的工程

| 工程 | 路径 | 说明 |
|---|---|---|
| balloonui | `balloonui/` | 控件库本体，可以编成静态库或 DLL |
| DuiGallery | `DuiGallery/` | 全控件演示窗，每个控件都有单独的展示页 |
| NewChatDemo | `NewChatDemo/` | 完整的聊天界面 Demo（XML 布局 + 自定义控件） |
| CloudMelodyDesktop | `CloudMelodyDesktop/` | 桌面音乐播放器 Demo（多页面 + 多媒体素材） |
| DemoTaskManager | `DemoTaskManager/` | 任务管理器风格 Demo（多列 TreeView + 多 Tab） |
| DemoChatBubble | `DemoChatBubble/` | 聊天气泡控件 Demo |
| DemoCircularProgress | `DemoCircularProgress/` | 圆形进度环 Demo |
| DemoFileTypeIcon | `DemoFileTypeIcon/` | 文件类型图标 Demo |
| DemoNinePatchBg | `DemoNinePatchBg/` | 9-grid 背景拉伸 Demo |
| DemoTextBadgeTile | `DemoTextBadgeTile/` | 文字徽章瓦片 Demo |
| DemoTreeViewLargeData | `DemoTreeViewLargeData/` | TreeView 大数据量性能 Demo |

打开根目录 `Demos.sln` 即可一次性加载所有工程。

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
- WTL 9.0（仓内 `wtl9.0/` 已含一份）
- 平台：**Win32 (x86/x64)**，Debug / Release 均可
- 运行平台：Windows 7 及以上

---

## 快速开始

```cpp
#include "balloonui/DuiHost.h"
#include "balloonui/DuiXmlBuilder.h"
#include "balloonui/Controls/Window/DuiFrameWindow.h"

using namespace balloonwjui;

// 在 ATL CFrameWindowImpl::OnCreate 里：
const char* xml = R"(
  <frame-window title="Hello" width="320" height="200">
    <vbox padding="16" spacing="8">
      <label text="Hello, balloonui!"/>
      <button id="btn_ok" text="OK" style="pushbutton"/>
    </vbox>
  </frame-window>
)";

auto* host = DuiXmlBuilder::FromFrameXml(xml);
host->Create(/* parent hwnd */);
host->ShowWindow(SW_SHOW);

// 监听按钮点击：在父窗口的 OnDuiNotify 里
// case DUIN_BUTTON_CLICKED:
//   if (id == "btn_ok") { /* ... */ }
//   break;
```

更完整的示例可直接打开 DuiGallery 工程跑一下，每个控件都能在 Gallery 里
单独触发、查看状态机。

---

## 目录结构

```
third_party/
├── balloonui/              控件库源码
│   ├── Controls/           按类别分七大子目录
│   ├── Tests/              单元 / 集成测试
│   ├── BalloonUiApi.h      DLL 导入 / 导出宏
│   ├── BalloonUiFeatures.h 按需裁剪开关
│   ├── DuiHost / DuiControl / DuiLayout / DuiTheme ...   kernel
│   └── balloonui.vcxproj
├── DuiGallery/             全控件演示窗
├── NewChatDemo/            聊天界面 Demo
├── CloudMelodyDesktop/     音乐播放器 Demo
├── Demo*/                  小型单点 Demo
├── docs/
│   ├── guides.html         配套指南
│   └── images/             文档配图
├── wtl9.0/                 仓内 WTL 9.0
├── Goldens/                金图（用于回归对比）
├── Bin/                    输出目录
└── Demos.sln               一次性加载所有工程
```

---

## 授权说明

任何人可以免费使用本库（包括商业用途），无需署名，无需任何形式的费用。

代码、Demo、文档配图均按"原样" (AS IS) 提供，作者不对使用过程中产生的
任何后果承担责任。如发现问题或有改进建议，欢迎反馈。