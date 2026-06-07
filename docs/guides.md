# balloonui 使用文档

**语言：** 中文 | [English](guides_en.md)

<a id="overview"></a>

## 1. 概述

<a id="overview-purpose"></a>

### 1.1 balloonui 是什么 / 用来做什么

balloonui 是一个面向 Windows 桌面客户端的 **DUI（DirectUI）UI 库**。"DirectUI"意指：整个界面只用一个真 HWND，所有控件都是<u>逻辑对象</u>（`DuiControl`），由库直接画到宿主窗口的 HDC，而不是 Win32 的"每控件一个 HWND"模型。这套架构最早由微软在 Windows XP / Office / Live Messenger 内部使用，后被腾讯（QQ / 某信 PC）、网易、迅雷等国内桌面厂商以及多家开源项目（DuiLib、SOUI、Sciter）广泛复用。

典型使用场景（本仓库自带 demo 全部基于 balloonui）：

- **消息流 / 即时通讯类**（XChat 复刻某信、NewChatDemo、DemoChatBubble）：左导航 + 中列表 + 右内容三段式，自定义气泡 / 文件卡 / emoji 面板，richedit 输入框带表情和图片插入。
- **消费型多页面应用**（CloudMelodyDesktop 音乐 App "FangMusic"）：卡片网格、媒体播放器、Now-Playing、桌面歌词逐像素 alpha 浮窗、全屏沉浸模式、播放计时驱动。
- **工具型 / 管理型 UI**（DemoTaskManager 任务管理器复刻、DuiGallery 控件展示库）：菜单 / Tab / 表格 / 树形 / 列表 / 设置项等"信息密集型"控件密集页面。
- **视觉演示 / 单控件**（DemoCircularProgress、DemoFileTypeIcon、DemoNinePatchBg、DemoTextBadgeTile、DemoTreeViewLargeData）：每个 demo 聚焦一个控件或一种自绘技巧（GDI+ 抗锯齿、9-grid 拉伸、虚拟列表、文件类型图标等）。

不适合的场景：3D / 视频高吞吐渲染（请用 D3D / Media Foundation）、需要原生平台外观完全一致的"系统设置"类窗口（请用 WinUI / WPF）、跨平台桌面（请用 Qt / Flutter / Tauri）。

<a id="overview-advantages"></a>

### 1.2 为什么选 balloonui — 与同类方案对比

| 对比点 | balloonui | 原生 Win32 / WTL / MFC | Qt / WPF / WinUI | Web Hybrid（CEF / Electron） |
| --- | --- | --- | --- | --- |
| **HWND 数量** | 1（宿主），其余全是逻辑节点 | 每控件一个 HWND，复杂窗口几百到上千个 | WPF/WinUI 1 个，Qt 视类型而定 | 1（embedded WebView） |
| **启动 / 内存开销** | 极低（DLL ~1MB），一个 frame 启动 < 100ms | 低 | WPF 中等；WinUI 偏重 | 高（Chromium 内核 ~100MB） |
| **UI 自由度** | **极高** — 任何控件都可派生改 `OnPaint_`，自绘抗锯齿气泡 / 圆形按钮 / 渐变封面都直接写 | 受系统主题约束，自绘要走 OWNERDRAW + 子类化，工程代价高 | 样式表 / 模板灵活，但也较厚重 | CSS 万能，但渲染走 GPU 合成，硬件占用高 |
| **布局描述** | XML 声明式（`<hbox>`/`<vbox>`/`<grid>` 等），可注册自绘标签工厂 | 纯代码（CreateWindow + 手算位置）或 .rc 静态对话框，难以响应式 | XAML / QML，能力强但学习曲线陡 | HTML + CSS |
| **IME / 中文输入** | 原生（HWND-hosted EDIT / RichEdit 例外，`DuiEditHost`） | 原生 | 原生（成熟） | 原生 |
| **DPI 感知** | Per-Monitor V2，一行 opt-in | 能做但要手动处理 `WM_DPICHANGED` 全链路 | WPF/WinUI 内置；Qt 视版本 | WebView 自带 |
| **外部依赖** | 仅 ATL / WTL 9.0 + GDI / GDI+ / comctl32 / richedit（系统自带） | 无 | 需要 .NET 运行时 / Qt 共享库 | 嵌入完整 Chromium |
| **跨平台** | 否（仅 Windows） | 否 | 是 | 是 |

**选 balloonui 的典型理由**：业务在 Windows 桌面，UI 需要高度自定义视觉（QQ / 某信 / 网易云风格的非系统样式控件），又不想拉一整套 Qt / Chromium / .NET，希望保持原生 C++ 工程链 + 极小启动包 + 低内存占用。同时项目愿意用 ATL/WTL 风格的 C++ 组合界面，并接受"仅 Windows"的覆盖范围。

<a id="overview-features"></a>

### 1.3 技术特点速览

提供 30+ 控件，关键设计：

- **单 HWND 模型**：整个 UI 树共用一个真窗口（`DuiHost`），子控件全部是逻辑节点（`DuiControl`），不创建 HWND，因此控件数量不受 GDI 句柄上限影响。
- **HWND-Hosted 例外**：需要 IME 输入法的文本控件（`DuiEditHost` / `DuiRichEditHost`）通过 `HwndHostControl` 适配器嵌入真 EDIT/RichEdit。
- **双缓冲绘制**：`DuiHost` 内置离屏 DC，控件直接画到 HDC，无闪烁。
- **XML 声明式布局**：`DuiXmlBuilder` 把 XML 解析成控件树，业务可注册自定义标签工厂插入自绘节点。
- **事件冒泡**：子控件通过 `NotifyParent(DUIN_*, extra)` 向上发 `WM_DUI_NOTIFY`。
- **Per-Monitor V2 DPI 感知**：`DuiDpi::OptInPerMonitorV2()` 一行启用。
- **GDI+ 抗锯齿统一封装**：`DuiPaintAA::FillEllipse / FillPolygon / FillRoundRect / DrawLine`，所有非轴对齐图形（圆 / 三角 / 斜线 / 圆角矩形）走同一接口。
- **无依赖编译**：仅依赖 ATL/WTL 9.0、GDI、GDI+、comctl32、richedit。

<a id="how-to-use"></a>

## 2. 程序如何使用 balloonui

### 2.1 链接方式

balloonui 编译为 `balloonui.dll` + `balloonui.lib`（导入库）。消费方有两种用法：

| 方式 | 用法 | 适用场景 |
| --- | --- | --- |
| **动态链接** | 定义宏 `BUI_USE_DLL`，链 `balloonui.lib`，发布时一并打包 `balloonui.dll`。 | 常规业务程序、demo（如 NewChatDemo） |
| **静态嵌入** | 不定义宏，把 `balloonui/*.cpp`（含 `Controls/`）直接 ClCompile 进 exe。 | 测试 harness（如 DuiGallery，需访问内部状态） |

> **原理：** `BalloonUiApi.h` 定义三态宏 `BUI_API`：
>
> - 构建 DLL 时：`BUI_BUILD_DLL` → `__declspec(dllexport)`
> - 消费 DLL 时：`BUI_USE_DLL` → `__declspec(dllimport)`
> - 静态嵌入时：两个都不定义 → 宏展开为空

### 2.2 工程配置（VS 2022 vcxproj）

```
<ClCompile>
  <AdditionalIncludeDirectories>
    ..\balloonui;..\Source\wtl9.0;%(AdditionalIncludeDirectories)
  </AdditionalIncludeDirectories>
  <PreprocessorDefinitions>BUI_USE_DLL;%(PreprocessorDefinitions)</PreprocessorDefinitions>
</ClCompile>
<Link>
  <AdditionalLibraryDirectories>..\Bin;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
  <AdditionalDependencies>balloonui.lib;gdiplus.lib;comctl32.lib;%(AdditionalDependencies)</AdditionalDependencies>
</Link>
```

### 2.3 最小可运行程序

```
#include <atlbase.h>
#include <atlapp.h>
extern CAppModule _Module;
#include <atlwin.h>

#include "DuiDpi.h"
#include "DuiXmlBuilder.h"
#include "Controls/DuiFrameWindow.h"

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR, int nCmdShow)
{
    balloonwjui::DuiDpi::OptInPerMonitorV2();
    ::OleInitialize(NULL);
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);

    balloonwjui::DuiFrameWindow frame;
    frame.SetTitle(_T("Hello balloonui"));
    frame.SetButtons(true, true, true);
    frame.Create(NULL, CWindow::rcDefault, _T("Hello"), WS_OVERLAPPEDWINDOW, 0);

    // 客户区：从 XML 加载，或代码构建。
    auto root = balloonwjui::DuiXmlBuilder::FromString(
        "<vbox padding=\"16\" gap=\"8\">"
        "  <label text=\"Hello, balloonui\" fixedHeight=\"22\"/>"
        "  <button id=\"100\" text=\"OK\" fixedHeight=\"32\"/>"
        "</vbox>");
    frame.SetClientContent(std::move(root));

    frame.ResizeClient(400, 200);
    frame.CenterWindow();
    frame.ShowWindow(nCmdShow);

    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    _Module.Term();
    ::OleUninitialize();
    return (int)msg.wParam;
}
```

### 2.4 接收子控件事件

每个控件触发交互（如按钮点击）时发 `WM_DUI_NOTIFY`：消息的 `lParam` 是 `DuiNotify*`，含 `code`（`DUIN_CLICK` 等）+ `ctrlId`。`DuiHost` 默认把通知 **发给 HWND 父窗口**；当宿主自身就是顶层窗口（`DuiFrameWindow`）时，`SendNotify` 自动回路到自己的 WM_DUI_NOTIFY 处理函数。

```
BEGIN_MSG_MAP(MyDialog)
    MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)
END_MSG_MAP()

LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lParam, BOOL&)
{
    auto* n = reinterpret_cast<balloonwjui::DuiNotify*>(lParam);
    if (n->code == DUIN_CLICK && n->ctrlId == 100) {
        ::MessageBox(m_hWnd, _T("OK pressed"), _T("Demo"), MB_OK);
    }
    return 0;
}
```

<a id="xml-layout"></a>

## 3. XML 布局系统

`DuiXmlBuilder::FromString(xml [, customFactory])` 把 XML 字符串解析成 `DuiControl` 树（不含顶层 frame；顶层 frame 走 [§11 `FromFrameXml`](#frame-window-xml)）。本节是**所有原生标签 + 通用属性 + 自定义工厂**的完整参考。

<a id="xml-tag-overview"></a>

### 3.1 内置标签一览（17 个）

| 标签 | 对应类 | 类别 | 用途简述 |
| --- | --- | --- | --- |
| `<vbox>` | DuiVBox | 布局 | 纵向排子节点 |
| `<hbox>` | DuiHBox | 布局 | 横向排子节点 |
| `<grid>` | DuiGrid | 布局 | 固定 rows × cols 网格 |
| `<splitter>` | DuiSplitter | 布局 | 2 pane 可拖动分隔（前两个子元素 = pane 0/1） |
| `<groupbox>` | DuiGroupBox | 布局 | 带标题圆角描边容器（第一个子元素 = 内容） |
| `<label>` | DuiLabel | 静态 | 纯文本标签 |
| `<separator>` | DuiSeparator | 静态 | 横/竖分隔线 |
| `<avatar>` | DuiAvatar | 静态 | 头像 / 圆形 / 缩写 fallback / 状态点 |
| `<badge>` | DuiBadge | 静态 | 红色未读计数 / 短标签 |
| `<progress>` | DuiProgressBar | 静态 | 确定 / marquee 进度条 |
| `<button>` | DuiButton | 交互 | push / icon / checkbox / radio 四态 |
| `<slider>` | DuiSlider | 交互 | 数值滑块 |
| `<switch>` | DuiSwitch | 交互 | iOS 风格圆角胶囊开关（150ms 动画） |
| `<edit>` | DuiEditHost | HWND | HWND-hosted 单/多行 EDIT |
| `<richedit>` | DuiRichEditHost | HWND | HWND-hosted 富文本（图文 / 链接 / IME） |
| `<searchbox>` | DuiSearchBox | HWND | 带放大镜 + 清除按钮的搜索框（含一个 EDIT） |
| `<spinbox>` | DuiSpinBox | HWND | 整数微调（含一个 EDIT） |
| `<treeview>` | DuiTreeView | 列表 | 层级树 / 多列表格混合（XML 仅配 `<column>`，节点 C++ 添加） |

**HWND 类** 控件内部寄宿一个真 HWND（OS 原生 EDIT / RICHEDIT），构造完后 caller 必须在 host HWND 就绪后调用 `EnsureCreated(hostHwnd)` 一次（参见 [§3.4](#xml-ensure-created)）。其它类别都是无 HWND 的纯 DUI 控件，构造即可用。

<a id="xml-common-attrs"></a>

### 3.2 通用属性（所有标签都支持）

| 属性 | 类型 | 含义 |
| --- | --- | --- |
| `id` | UINT > 0 | 调 `SetCtrlId`，作为子节点触发 `WM_DUI_NOTIFY` 时的 `n.ctrlId`，业务在父窗口的处理函数里靠这个 id 区分是哪个控件发的事件。 |
| `fixedWidth` | int (px) | 父若是 `<hbox>`：固定主轴宽；其它情况：交叉轴宽。 |
| `fixedHeight` | int (px) | 父若是 `<vbox>`：固定主轴高；其它情况：交叉轴高。 |
| `weight` | int | 主轴 flex 权重，剩余空间按所有子项 weight 之和按比例分。`fixedWidth/Height` 与 `weight` 互斥（前者优先）。 |
| `margin` | int (px) | 四边外边距（同一值；要分开请走父容器的 `gap` + `padding`）。 |
| `alignCross` | "fill" \\| "center" \\| "near" \\| "far"（别名 "start"/"end"/"top"/"bottom"/"left"/"right"） | 交叉轴对齐。默认 `fill`（撑满交叉轴；此时 `fixedWidth/Height` 在交叉轴方向上<u>被忽略</u>）。要让控件按 `fixedCross`（hbox 子的 fixedHeight、vbox 子的 fixedWidth）实际收缩并居中显示，必须显式写 `alignCross="center"`（或 `near`/`far`）。 |

**fixedCross 与 alignCross 的搭配**：
单写 `fixedHeight="22"` 在 hbox 里<u>不会</u>把控件压成 22 高 —— 因为 alignCross 默认 `fill`，会忽略 fixedCross 把控件拉到 hbox 满高。要真正限制交叉轴尺寸 + 居中，必须配 `alignCross="center"`：

```
<hbox fixedHeight="32">
  <label text="保留聊天记录" weight="1"/>
  <!-- switch 56×22，在 32 高的 hbox 内居中显示，上下各留 5px 空白 -->
  <switch fixedWidth="56" fixedHeight="22" alignCross="center"/>
</hbox>
```

**属性约定**：颜色一律 `"r,g,b"` 三段（0–255）；矩形 / 内距 / 外距支持 `"l,t,r,b"` 四段或单值"n"（4 边相同）；布尔 `"true"/"1"/"yes"` 视为 true，其它 false；任何<u>缺省</u> = 不调对应 setter，控件保留构造时默认。

<a id="xml-tag-detail"></a>

### 3.3 各标签属性详细参考

<a id="xml-vbox-hbox-grid"></a>

#### 3.3.1 `<vbox>` / `<hbox>` / `<grid>` — 布局容器

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `padding` | int 或 "l,t,r,b" | 容器内 4 边内距。默认 0。 |
| `gap` | int (px) | 子元素之间的间距（vbox 是行间距，hbox 是列间距，grid 是行列共用）。默认 0。 |
| `rows` | int | **仅 grid**：行数。默认 1。 |
| `cols` | int | **仅 grid**：列数。默认 1。 |

子节点按声明顺序填入；`fixedWidth/Height/weight` 通用属性决定每项主轴尺寸。

```
<vbox padding="20" gap="10">
  <label text="姓名" fixedHeight="22"/>
  <edit  placeholder="请输入" fixedHeight="32"/>
  <hbox gap="8" fixedHeight="36">
    <control weight="1"/>            <!-- 弹性占位 -->
    <button text="取消" fixedWidth="80"/>
    <button text="确定" fixedWidth="80" id="100"/>
  </hbox>
</vbox>
```

<a id="xml-splitter"></a>

#### 3.3.2 `<splitter>` — DuiSplitter（2-pane 分隔）

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `orientation` | "vertical" \\| "horizontal" | 分隔条方向。**"vertical"**（默认）= 条<u>纵向</u>跑、两 pane 左右排；**"horizontal"** = 条横向跑、两 pane 上下排。 |
| `bar-thickness` | int (px) | 分隔条粗细。默认 4。 |
| `min-size-0` | int (px) | pane 0 主轴最小尺寸（拖到此值后挡住）。默认 40。 |
| `min-size-1` | int (px) | pane 1 主轴最小尺寸。默认 40。 |
| `split-px` | int (px) | pane 0 初始尺寸（绝对像素）。优先于 split-fraction。 |
| `split-fraction` | float 0..1 | pane 0 初始尺寸占容器主轴比例（如 "0.3"）。仅在没写 `split-px` 时生效。 |

**子节点**：前两个子元素自动作为 pane 0 / pane 1，<u>多余的忽略</u>。每个 pane 一般是 `<vbox>`/`<hbox>`/具体业务控件。

**事件**：用户拖完分隔条松手时，发 `DUIN_VALUECHANGED`（`extra` = pane 0 当前像素尺寸）。

```
<splitter id="200" orientation="vertical" bar-thickness="6" split-px="240">
  <vbox padding="8">...左 pane 内容...</vbox>
  <vbox padding="8">...右 pane 内容...</vbox>
</splitter>
```

<a id="xml-groupbox"></a>

#### 3.3.3 `<groupbox>` — DuiGroupBox（带标题边框容器）

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `title` | string | 顶部标题文字。 |
| `title-color` | "r,g,b" | 标题文字色。默认 RGB(60,60,80)。 |
| `border-color` | "r,g,b" | 圆角边框色。默认 RGB(200,200,208)。 |
| `corner-radius` | int (px) | 圆角半径。默认 6。 |
| `title-strip-height` | int (px) | 顶部标题条高度（内容从这里之下开始）。默认 24。 |
| `padding` | int 或 "l,t,r,b" | 内容区相对边框的 4 边内距。默认 12。 |

**子节点**：第一个子元素 = 内容（通过 `SetContent` 安装），多余的忽略。一般是 `<vbox>` 或 `<grid>` 装多个表单行。

```
<groupbox title="账号" padding="12,8,12,12">
  <vbox gap="6">
    <hbox gap="6" fixedHeight="28">
      <label text="用户名" fixedWidth="64"/>
      <edit  weight="1" placeholder="suwenjia"/>
    </hbox>
    <hbox gap="6" fixedHeight="28">
      <label text="邮箱"   fixedWidth="64"/>
      <edit  weight="1" placeholder="user@example.com"/>
    </hbox>
  </vbox>
</groupbox>
```

<a id="xml-label"></a>

#### 3.3.4 `<label>` — DuiLabel

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `text` | string | 显示文本（支持 `&` 助记符）。 |
| `textColor` | "r,g,b" | 字体色。默认黑色。 |

无原生事件（纯绘制）。文字字体、对齐由 `SetTextAlign / SetFont` 在 C++ 侧调，XML 暂不暴露。

<a id="xml-separator"></a>

#### 3.3.5 `<separator>` — DuiSeparator

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `orientation` | "horizontal" \\| "vertical" | 线方向。默认 horizontal。 |
| `color` | "r,g,b" | 线色。默认 RGB(220,220,224)。 |
| `thickness` | int (px) | 线粗。默认 1。 |
| `inset` | int (px) | 沿线方向两端内缩多少 px。默认 0。 |

无事件。典型用法：在 hbox/vbox 里隔开两组 toolbar 按钮（带 inset 让短一截更精致）。

<a id="xml-avatar"></a>

#### 3.3.6 `<avatar>` — DuiAvatar

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `name` | string | 姓名 → 自动取最多 2 个汉字 / 大写字母作 fallback 缩写（"苏文嘉" → "苏文"，"Alice Smith" → "AS"）。 |
| `fallback-bg-color` | "r,g,b" | 无图时的圆 / 圆角背景色。默认品牌蓝 RGB(45,108,223)。 |
| `initials-color` | "r,g,b" | fallback 缩写文字色。默认白。 |
| `shape` | "circle" \\| "round-rect" | 形状。默认 circle。 |
| `corner-radius` | int (px) | 仅 round-rect 时生效。默认 8。 |
| `status` | "none" \\| "online" \\| "away" \\| "busy" \\| "offline" | 右下角状态点（绿/黄/红/灰）。默认 none。 |

**位图加载不在 XML 范围**：Avatar 接受 caller-owned `HBITMAP`，XML 不支持 `image="path"`（避免引入隐式 GDI+ 加载逻辑）。caller 拿到 builder 返回的根，找到该 avatar 节点，`av->SetBitmap(hbm)` 自己挂图。常用于"先用 fallback 占位，async 拉到头像后回填"。

无事件（纯绘制）。

<a id="xml-badge"></a>

#### 3.3.7 `<badge>` — DuiBadge

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `count` | int | 未读数。0 → 隐藏（在 `hide-when-empty=true` 时）；1–99 → 显示数字；100+ → 显示 "99+"。<u>优先于 text</u>。 |
| `text` | string | 原始文本（最多 4 字符）。仅在 `count` 不写时生效。 |
| `bg-color` | "r,g,b" | 底色。默认红 RGB(220,60,60)。 |
| `text-color` | "r,g,b" | 字色。默认白。 |
| `hide-when-empty` | bool | 空文本/0 是否完全不绘制。默认 true。 |

无事件。

<a id="xml-progress"></a>

#### 3.3.8 `<progress>` — DuiProgressBar

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `min` | int | 下限。默认 0。 |
| `max` | int | 上限。默认 100。 |
| `value` | int | 当前值（被 clamp 进 [min,max]）。XML 阶段不发 notify。默认 0。 |
| `text` | string | 覆盖文字（写死）。 |
| `show-percent` | bool | text 为空时是否显示 "NN%"。默认 true。 |
| `vertical` | bool | true = 上下方向。默认 false（左右）。 |
| `marquee` | bool | true = 不确定进度（流光条）；caller 需自己定时调 `SetMarqueePhase()`。默认 false。 |
| `bg-color` | "r,g,b" | 背景色。默认 RGB(232,232,236)。 |
| `fill-color` | "r,g,b" | 填充色（已完成部分）。默认品牌蓝。 |
| `border-color` | "r,g,b" | 边框色。默认 RGB(180,180,188)。 |
| `text-color` | "r,g,b" | 覆盖文字色。默认 RGB(30,30,30)。 |

**事件**：`DUIN_VALUECHANGED`（`extra` = 新值）—— 仅由 `SetPos(v, true)` 触发；XML 解析阶段调的都是 `SetPos(v, false)`（`notify=false`）所以不会"启动时就发一次"。

<a id="xml-button"></a>

#### 3.3.9 `<button>` — DuiButton

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `text` | string | 按钮文字（支持 `&` 助记符）。 |
| `buttonType` | "push" \\| "icon" \\| "checkbox" \\| "radio" | 按钮风格。默认 push（品牌蓝实心圆角）。 · icon = 紧凑带图标； · checkbox = 左侧勾选框； · radio = 单选框（同父节点同 group 互斥，需在 C++ 侧 `SetRadioGroup(gid)`）。 |

**事件**：

| code | 触发时机 | extra |
| --- | --- | --- |
| `DUIN_CLICK` | 所有按钮风格 — mouse-up 在按钮内时 | 0 |
| `DUIN_VALUECHANGED` | checkbox / radio 的勾选状态改变时 | 新状态（0=未选 / 1=选中） |

<a id="xml-slider"></a>

#### 3.3.10 `<slider>` — DuiSlider

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `min` / `max` | int | 取值范围。默认 0 / 100。 |
| `value` | int | 初始值。XML 阶段 notify=false。默认 0。 |
| `line-size` | int | 键盘左右键 / 鼠标滚轮 / 轨道点击的步长。默认 1。 |
| `vertical` | bool | true = 竖滑（上→下值递增）。默认 false。 |
| `track-height` | int (px) | 轨道粗细。默认 4。 |
| `thumb-size` | int (px) | 滑块直径。默认约 14（半径 7）。 |
| `track-color` | "r,g,b" | 轨道未填充段颜色。默认 RGB(220,220,226)。 |
| `fill-color` | "r,g,b" | 轨道已填充段颜色。默认品牌蓝。 |
| `thumb-color` | "r,g,b" | 滑块色。默认白。 |
| `tick-frequency` | int | 刻度间隔（值-单位为间）。0 = 不画刻度。默认 0。 |
| `tick-color` | "r,g,b" | 刻度色。默认 RGB(150,150,160)。 |

**事件**：`DUIN_VALUECHANGED`（`extra` = 新值）—— 拖动 / 轨道点击 / 滚轮 / 键盘调整都触发。

<a id="xml-switch"></a>

#### 3.3.11 `<switch>` — DuiSwitch

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `checked` | bool | 初始勾选态。XML 阶段不发 notify、不动画。默认 false（off）。 |
| `animated` | bool | 鼠标 / 键盘翻转时是否走 150ms 动画。默认 true。<u>不影响</u> `SetChecked(_, animated)` 的显式参数。 |
| `on-color` | "r,g,b" | on 态胶囊底色。默认 RGB(7,193,96)（某信绿）。 |
| `off-color` | "r,g,b" | off 态胶囊底色。默认 RGB(229,229,229)（浅灰）。 |
| `knob-color` | "r,g,b" | 滑块色。默认 RGB(255,255,255)（白）。 |

**动画依赖：**切换走 `DuiAnimMgr` 驱动 —— host 必须周期性调 `balloonwjui::DuiAnimMgr::Inst().TickAll(::GetTickCount())`（典型 16ms `WM_TIMER` pulse）才能看到中间帧。`DuiGallery` 的 `GalleryFrame` 已经接通这个 pulse，业务侧的主对话框 / 顶层 frame 也要补上一份；不接通的话最终态仍然正确，只是没有动画过渡。

**事件**：`DUIN_VALUECHANGED`（`extra` = 1（on）/ 0（off））—— 鼠标点击 / Space / Enter 翻转时触发；`SetChecked()` 编程调用<u>不</u>触发，匹配 `DuiButton` checkbox 行为。

<a id="xml-edit"></a>

#### 3.3.12 `<edit>` — DuiEditHost

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `placeholder` | string | 空值占位提示文字。 |
| `password` | bool | 密码模式（显示 ●）。默认 false。 |
| `multiline` | bool | 多行编辑。默认 false。 |

**EnsureCreated 约束**：`<edit>` 内部寄宿一个真 EDIT HWND，必须在 host HWND 就绪后调用 `edit->EnsureCreated(hostHwnd)` 才会真正创建出来。详见 [§3.4](#xml-ensure-created)。

**事件**：

| code | 触发时机 |
| --- | --- |
| `DUIN_VALUECHANGED` | EN_CHANGE — 文字变化（每输入一字符） |
| `DUIN_SETFOCUS` | EN_SETFOCUS |
| `DUIN_KILLFOCUS` | EN_KILLFOCUS |

<a id="xml-richedit"></a>

#### 3.3.13 `<richedit>` — DuiRichEditHost

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `placeholder` | string | 空值占位文字。 |
| `read-only` | bool | 只读。默认 false。 |
| `max-length` | int | 最大字符数。0 = 无限制（默认）。 |
| `multi-line` | bool | 多行（必须在 EnsureCreated 前设；默认 true 即可——RichEdit 多行场景为主）。 |
| `word-wrap` | bool | 自动换行。同样 pre-Create。默认 true。 |
| `text-color` | "r,g,b" | 默认字色。默认 RGB(30,30,30)。 |
| `bg-color` | "r,g,b" | 背景色。默认白。 |
| `auto-url-detect` | bool | 自动识别 URL 并蓝色下划线。默认 false。识别后点击发 `DUIN_RE_LINKCLICK`。 |
| `margins` | "l,t,r,b" | EDIT 内边距。默认 4,2,4,2。 |

**事件**：同 `<edit>` 的 3 个 + `DUIN_RE_LINKCLICK`（`auto-url-detect=true` 时点击 URL，`extra` 是 `ENLINK*`）。

富文本 / RTF / 图片 / 引用块插入需 C++ 侧调具体方法，XML 不暴露。EnsureCreated 同 `<edit>`。

<a id="xml-searchbox"></a>

#### 3.3.14 `<searchbox>` — DuiSearchBox

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `placeholder` | string | 空值占位文字。 |
| `read-only` | bool | 只读。默认 false。 |
| `max-length` | int | 最大字符数。默认 0（无限）。 |
| `glyph-strip-width` | int (px) | 左侧放大镜区域宽。默认 24。 |
| `clear-strip-width` | int (px) | 右侧 "×" 清除按钮区宽。默认 22。 |

**事件**：`DUIN_VALUECHANGED` —— 文字变化或点击 × 清空时。

EnsureCreated 同 `<edit>`（searchbox 内部含一个 `DuiEditHost` 子节点）。

<a id="xml-spinbox"></a>

#### 3.3.15 `<spinbox>` — DuiSpinBox

| 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `min` / `max` | int | 取值范围。默认 0 / 100。 |
| `value` | int | 初始值。XML 阶段 notify=false。默认 0。 |
| `step` | int > 0 | 每次 ▲▼ 点击变化量。默认 1。 |
| `wrap` | bool | true = 越界回绕（max → min），false = clamp。默认 false。 |
| `spin-strip-width` | int (px) | 右侧 ▲▼ 按钮条宽度。默认 18。 |

**事件**：`DUIN_VALUECHANGED`（`extra` = 新值）—— ▲▼ 点击触发；用户在 EDIT 里手输入后的提交需 caller 在 `DUIN_KILLFOCUS` 里 `SetValue(_ttoi(GetEdit()->GetText()))`。

EnsureCreated 同 `<edit>`（spinbox 内部含一个 `DuiEditHost` 子节点）。

<a id="xml-treeview"></a>

#### 3.3.16 `<treeview>` — DuiTreeView（仅多列模式 XML 配列）

XML 仅配列定义；节点（`AddRoot`/`AddChild`）必须由 C++ 端添加，因为节点常运行时变化用 XML 表达不划算（详见 [§7 DuiTreeView](#DuiTreeView)）。<u>不</u>调任何 `<column>` 时控件保持单列模式（无 header）。

| <treeview> 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `row-height` | int (px) | 行高。默认 28。 |
| `header-height` | int (px) | 多列模式 header 条高度。默认 26。 |
| `indent` | int (px) | 每层缩进。默认 18。 |
| `frozen-cols` | int | 前 N 列贴左不水平滚。默认 0。 |
| `frozen-rows` | int | 前 N 个可见行贴顶不垂直滚。默认 0。 |
| `editable` | bool | 编辑闸全局开关。默认 false（只读）。 |
| `zebra` | bool | 偶数行斑马纹。默认 false。 |

| <column> 属性 | 类型 | 含义 / 默认值 |
| --- | --- | --- |
| `title` | string | 列名（header 显示）。 |
| `width` | int (px) | 初始列宽。默认 120。 |
| `min-width` | int (px) | 最小列宽（拖拽 / auto-fit 都不会更窄）。默认 40。 |
| `align` | left / center / right | cell 文本对齐。默认 left。 |
| `sortable` | bool | true 时点击列名发 `DUITVN_COLUMNCLICK`。默认 true。 |
| `editable` | bool | per-column 编辑闸（须配合 `<treeview editable="true">` 才生效）。默认 true。 |

```
<treeview id="100" frozen-cols="1" editable="true">
    <column title="Name"     width="200" min-width="80"  align="left"/>
    <column title="Done"     width="70"  min-width="50"  align="center"/>
    <column title="Progress" width="160" min-width="90"/>
    <column title="Size"     width="90"  min-width="60"  align="right"/>
    <column title="Link"     width="180" min-width="80"  editable="false"/>
</treeview>
```

**事件 / cell 类型 / 编辑闸**详见 [§7 DuiTreeView](#DuiTreeView)。

<a id="xml-ensure-created"></a>

### 3.4 HWND-hosted 控件的 EnsureCreated 约定

4 个 HWND 类标签（`<edit>` / `<richedit>` / `<searchbox>` / `<spinbox>`）内部寄宿一个真 EDIT/RICHEDIT 子 HWND。这些 HWND 必须有<u>已创建的父 HWND</u> 才能 `CreateWindow` 出来；XML 解析阶段控件还没挂上 host，所以 builder **不会**主动创建这些子 HWND。

正确的 wiring 顺序：

1. `FromString(xml)` 拿到 root（控件树构造好，但 HWND 类还都是<u>裸 DUI 控件</u>）。
2. `host.SetRoot(std::move(root))` 或 `frame.SetClientContent(...)` —— 控件树挂到 host 上。
3. host 必须已经 `Create(...)` 过，确保 `m_hWnd` 有效。
4. 遍历 root 找到所有 HWND 类控件，对每个调 `EnsureCreated(host.m_hWnd)`。可以 `FindChildById(id)` 拿到指针，也可以自己写 visitor 走树。

```
auto root = balloonwjui::DuiXmlBuilder::FromString(xml);
host.SetRoot(std::move(root));               // host 必须已 Create

// 找到 id=100 的 edit，让它真正建出 HWND
auto* edit = static_cast<balloonwjui::DuiEditHost*>(
    host.GetRoot()->FindControlById(100));
if (edit)
{
    edit->EnsureCreated(host.m_hWnd);
}
```

没调 `EnsureCreated` 的 HWND 控件不会崩，但<u>不显示真 EDIT 控件</u>（仍是裸 DUI 节点 + 占位绘制），用户也无法输入。

<a id="xml-comments"></a>

### 3.5 XML 解析的边界条件

| 支持 | 不支持 |
| --- | --- |
| 注释 `<!-- ... -->`（prolog 与子元素之间均可，体内可含 `>`） | DOCTYPE / 实体引用 / CDATA |
| processing instruction `<?xml ... ?>` | XML 命名空间 `xmlns:foo`（解析按裸 tag 处理） |
| UTF-8 / UTF-16（LPCWSTR 重载内部转 UTF-8） | GBK / Big5 等其它编码（请先转 UTF-8） |
| UTF-8 BOM（caller 端去掉，参见 DemoNinePatchBg/main.cpp 的 LoadXmlFromExeDir） | — |
| 空白字符容忍（任意 \r\n \t 在 tag 之间） | 属性值里的 `&amp;` / `&quot;` 实体（按字面处理） |

<a id="xml-custom-factory"></a>

### 3.6 自定义标签 / 自绘布局（CustomFactory）

未识别的标签（包括 §3.1 的 16 个内置标签之外的任何 tag）会派发到调用方注册的 `CustomFactory`。业务可注入任意自绘控件，且可包含子节点（在自绘控件 `Layout()` 里自管位置 — 即"自绘布局"）：

```
auto fac = [](const balloonwjui::DuiXmlBuilder::Node& n)
              -> std::unique_ptr<balloonwjui::DuiControl>
{
    if (n.tag == "my-bubble")
    {
        auto b = std::make_unique<MyBubble>();
        auto it = n.attrs.find("text");
        if (it != n.attrs.end())
        {
            b->SetText(ToCString(it->second));
        }
        return b;
    }
    return nullptr;        // 让 builder 跳过未知标签（不阻断后续兄弟节点）
};
auto root = balloonwjui::DuiXmlBuilder::FromString(xml.c_str(), fac);
```

典型场景：

- **带 model 的复杂控件**（ListBox / TreeView / ComboBox / Tab+TabPage / Menu / ToolTip / Gif / ImageOle / Dock / EmojiPanel / PopupHost）—— 这些不在原生 16 个标签里，业务通过 factory 注册自己的标签。例如 `<contact-list>` 解析成自家 `DuiListBox` + 业务的 model 桥。
- **自绘业务卡片**（聊天气泡、文件传输条、会话行）—— factory 创建自家 `DuiControl` 子类，子元素是子节点（builder 会自动 recurse build 后 AddChild）。

详见 [§8 自绘控件](#custom-controls) 给 4 个完整示例。

<a id="styles"></a>

## 4. 主题与样式

每个控件构造时已设置默认风格 — Microsoft YaHei 9pt 字体、品牌蓝（PushButton）、统一圆角等。常见样式覆盖入口：

| 层级 | API | 影响 |
| --- | --- | --- |
| 全局字体 | `DuiResMgr::Inst().GetDefaultFont()` | 所有控件默认 UI 字体 |
| 主题色 | `DuiTheme` 命名空间常量 | 品牌色、状态色（online/away/red） |
| per-控件 | 各 Set* 接口（见下） | 该实例 |

**默认 UI 风格**：

- 字体：**Microsoft YaHei 9pt**（GB2312）— 由 `DuiResMgr` 提供
- 圆角：button 8px、bubble 8px、chip 9-11px
- 品牌色：`#2D6CDF`（蓝）、`#4CC7A1`（绿，DuiAvatar/DuiBadge 默认）
- I-beam 光标在 EditHost / RichEditHost 上自动出现
- 所有非轴向图形（圆、对角线、多边形）通过 `DuiPaintAA` 抗锯齿绘制

<a id="class-hierarchy"></a>

## 5. 类继承结构

balloonui 内部分两条独立的类继承链，加上若干"独立服务/管理器"类。理解这两条链能帮助回答"我要不要 `SetRoot`？"、"为什么 `DuiMenu` 不能 `AddChild`？"这种常见疑惑。

### 5.1 控件链 — `DuiControl` 体系

一切"逻辑控件"（无 HWND，仅参与 paint / hit-test / event）都派生自 `DuiControl`。它们由父 `DuiHost` 共用一个真窗口绘制。

```
DuiControl                      // 逻辑节点基类（无 HWND）
│
├── HwndHostControl            // 嵌入真 HWND 的适配器
│   ├── DuiEditHost              // 真 EDIT，IME 友好
│   └── DuiRichEditHost          // 真 RICHEDIT_CLASS
│
├── DuiLayout                  // 容器抽象基类
│   ├── DuiVBox                  // 纵向盒
│   ├── DuiHBox                  // 横向盒
│   └── DuiGrid                  // 行 × 列 网格
│
├── DuiDock                      // 上下左右 + 中央填充
├── DuiSplitter                  // 可拖动分隔条
├── DuiTabPage                   // 多页容器
│
├── // — 基础控件 —
├── DuiLabel                     // 文本 / 链接
├── DuiButton                    // PushButton/Checkbox/Radio/Icon
├── DuiBadge                     // 红点 / 数字徽章
├── DuiAvatar                    // 头像 + 状态点
├── DuiSeparator                 // 横/竖 1px 分隔线
├── DuiGroupBox                  // 带标题分组框
│
├── // — 输入 —
├── DuiSearchBox                 // M7+ 改为继承 DuiEditHost，thin preset
├── DuiSpinBox                   // 内嵌 EDIT（聚合，非继承）
├── DuiSlider                    // 横/竖 滑块
├── DuiSwitch                    // iOS 风格圆角胶囊开关（150ms 动画）
├── DuiProgressBar               // 进度条
├── DuiComboBox                  // 下拉选择
│
├── // — 列表与导航 —
├── DuiListBox                   // 单/多选列表
├── DuiVirtualList               // 回调驱动的虚拟列表
├── DuiTreeView                  // 层级树
├── DuiTab                       // 水平 tab 栏
│
├── // — 反馈与浮层（非弹窗的部分）—
├── DuiEmojiPanel                // 表情面板（嵌入 popup 内）
│
├── // — 媒体 —
├── DuiGifControl                // GIF 播放
│
└── // — 滚动 —
    ├── DuiScrollBar             // 独立滚动条
    └── DuiScrollView            // 内置滚动容器
```

**注意**：`DuiSpinBox` 内部嵌了真 EDIT 但用<u>聚合</u>方式（不继承 HwndHostControl），DUI 部分（上下三角）用聚合更清爽。`DuiSearchBox` 早期同款，M7+ 重构成<u>继承 DuiEditHost</u>（thin preset）—— DuiEditHost 加了[原生左/右内联图标 API](#DuiEditHost-IconApi) 后，把 magnifier / × 直接装成 icon，比重写 paint / Layout / HitTest 更精简。

### 5.2 真窗口链 — `DuiHost` 体系

每一个"真 Win32 窗口"都派生自 `DuiHost`。`DuiHost` 自己又派生自 ATL 的 `CWindowImpl`，所以可以直接用 `Create` / `SubclassWindow` / `m_hWnd` 这些 ATL 套路。

```
ATL CWindowImpl<DuiHost, CWindow>
│
└── DuiHost                    // 一个真 HWND，承载 DuiControl 树
    │
    ├── DuiFrameWindow           // 顶层无边框 + 自绘标题栏 + 三按钮
    └── DuiPopupHost             // 浮层窗（下拉、菜单宿主、emoji 面板宿主）
```

关键事实：

- **`DuiHost` 是 `DuiControl` 的<u>容器</u>，不是它的子类。**把控件树挂上 host 用 `SetRoot(unique_ptr<DuiControl>)`。
- **每个真 HWND 一个 `DuiHost`**。最常见情况：主对话框 1 个 host；任何弹层（菜单、下拉、tooltip）各自再 1 个 host。
- `DuiFrameWindow` 是 `DuiHost` 的子类，**不要直接 `SetRoot`**，要用 `SetClientContent(...)`，因为它内部已经把 root 占了来摆标题栏 + 三按钮。

### 5.3 独立服务类（不在以上两条链中）

| 类 | 角色 | 关系 |
| --- | --- | --- |
| `DuiMenu` | 右键 / 命令菜单 | 内部维护一个独立的 `DuiMenuPopup`（弹层 HWND），`PopupAt(pt, owner)` 显示。**不是** `DuiControl`，不能 `AddChild` 到任何父控件。 |
| `DuiToolTipMgr` | 悬停提示管理器 | 单例。`Register(ctrl, text)` 把 `DuiControl*` 与提示文本关联，hover 自动浮出。 |
| `DuiAnim` / `DuiDoubleAnim` | 动画值生成器 | 定时插值器，输出值由 caller 应用到任意属性。 |
| `CDuiImageOle` | RichEdit 内嵌图 | 实现 `IOleObject` 系列接口，由 RichEdit 通过 OLE 装载。 |
| `CImageEx` | 图片资源 | 派生 ATL `CImage`，加缓存 / 加载辅助。由 `DuiResMgr` 管理。 |
| `DuiResMgr` | 资源管理器 | 单例。统一字体、图片缓存、DPI 通知。 |

### 5.4 namespace

整个库放在 `balloonwjui` 命名空间下。`Dui` 前缀是 grep 锚点（与遗留的 `CSkin*` 树视觉区分），即便已在命名空间内也保留。子命名空间：`balloonwjui::DuiAA`（抗锯齿绘制）、`balloonwjui::DuiDpi`（DPI 工具）、`balloonwjui::DuiTheme`（颜色 token）、`balloonwjui::DuiMnemonic`（& 助记键）等。

<a id="event-routing"></a>

## 6. 事件处理与路由

本节回答最常被问到的两个问题：

1. **事件代码写在哪里？** — 写在<u>包含 `DuiHost` 的那个真窗口类</u>的 WTL/ATL 消息映射里，处理 `WM_DUI_NOTIFY`。
2. **路由到底怎么走的？** — 控件调 `NotifyParent(code, extra)` → host 把它包成 `DuiNotify` 结构 → `SendMessage(parentHWND, WM_DUI_NOTIFY, ctrlId, &notify)`。

<a id="event-routing-flow"></a>

### 6.1 路由路径

```
┌─────────────────────────────────────────────────────────────────┐
│  控件代码                                                       │
│  void DuiButton::OnLButtonUp(...) {                             │
│      ...                                                        │
│      NotifyParent(DUIN_CLICK);     // 在 DuiControl 基类      │
│  }                                                              │
└──────┬──────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│  DuiControl::NotifyParent(code, extra)                          │
│   → DuiHost::SendNotify(this, code, extra)                      │
│   → 组装 DuiNotify { code, ctrlId, pCtrl, extra }                │
│   → ::SendMessage(targetHWND, WM_DUI_NOTIFY,                       │
│                    ctrlId, (LPARAM)&notify);                     │
│                                                                 │
│   targetHWND = ::GetParent(host) ? : host (回路到自己)          │
└──────┬──────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│  你的窗口类                                                     │
│  class MyDlg : public CDialogImpl<MyDlg> {                       │
│      BEGIN_MSG_MAP(MyDlg)                                       │
│          MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)             │
│      END_MSG_MAP()                                              │
│      LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lp, BOOL&) {       │
│          auto* n = (balloonwjui::DuiNotify*)lp;                 │
│          // ↓↓↓ 你的派发代码写在这里 ↓↓↓                       │
│      }                                                          │
│  };                                                             │
└─────────────────────────────────────────────────────────────────┘
```

`DuiNotify` 结构（在 `DuiNotify.h`）：

```
struct DuiNotify {
    UINT       code;     // DUIN_CLICK / DUIN_VALUECHANGED / DUITN_CLOSE / ...
    UINT       ctrlId;   // SetCtrlId 设置的 id（XML 里 id="..." 属性）
    DuiControl*pCtrl;    // 触发的控件指针（可空）
    LPARAM     extra;    // 控件特定附加值（看每控件 "事件" 表）
};
```

<a id="event-routing-where"></a>

### 6.2 处理函数写在哪里

看你用 `DuiHost` 的方式：

| 使用模式 | 处理函数所在类 | 事件路由到的 HWND |
| --- | --- | --- |
| **子窗口 host** `m_host.Create(m_hWnd, ...)` | 包含 `m_host` 的对话框 / 框架窗类 | `::GetParent(host)` = 你的对话框 HWND |
| **SubclassWindow** `m_host.SubclassWindow(m_hWnd)` | 同上 — host 与对话框共用一个 HWND | 对话框自身 HWND |
| **顶层 DuiFrameWindow** （无父） | `DuiFrameWindow` 的子类（你扩展的那个） | `GetParent` 为空时，**路由回到 host 自身** — 你在子类里加 `WM_DUI_NOTIFY` handler 即可 |
| **弹层 DuiPopupHost** | 调 `popup->SetOwner(ownerHwnd)` 指定的 HWND 所属类 | popup 不挂到任何 parent，事件去 owner |
| **DuiMenu** | `PopupAt(pt, ownerHwnd)` 中的 owner 所属类 | 同上 |

**三个常见误解**：

① 不需要在每个控件上"绑定回调"。整个 host 树共享一条 `WM_DUI_NOTIFY` 通道，靠 `n->ctrlId` 区分来源。

② `SetCtrlId`（或 XML `id` 属性）必须设；否则所有事件 `ctrlId=0`，无法区分。

③ 控件树本身没有 HWND，所以你<u>不能</u>给某个 `DuiButton` 写 `BEGIN_MSG_MAP` —— 消息映射只在<u>真窗口类</u>（`DuiHost` 的派生 / 嵌主对话框）里写。

<a id="event-routing-dispatch"></a>

### 6.3 推荐的派发模式

事件多的对话框用"先按 `ctrlId` 分组、再按 `code` 分支"的两层 switch，可读性最好：

```
// 控件 id 集中在一个 enum，避免散落魔数
enum : UINT {
    IDC_NICKNAME      = 100,
    IDC_PASSWORD      = 101,
    IDC_REMEMBER      = 102,
    IDC_LOGIN         = 103,
    IDC_FORGOT        = 104,
    IDC_FRIENDLIST    = 200,
    IDC_TAB_MAIN      = 300,
};

class LoginDlg : public CDialogImpl<LoginDlg>
{
public:
    enum { IDD = IDD_LOGIN };

    BEGIN_MSG_MAP(LoginDlg)
        MESSAGE_HANDLER(WM_INITDIALOG,    OnInitDialog)
        MESSAGE_HANDLER(WM_DUI_NOTIFY,    OnDuiNotify)
    END_MSG_MAP()

private:
    balloonwjui::DuiHost m_host;

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
    {
        m_host.SubclassWindow(m_hWnd);
        m_host.SetRoot(BuildLoginUi());     // 内部各控件 SetCtrlId 用上面的 enum
        return 0;
    }

    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lp, BOOL&)
    {
        auto* n = reinterpret_cast<balloonwjui::DuiNotify*>(lp);

        switch (n->ctrlId)
        {
        case IDC_NICKNAME:
        case IDC_PASSWORD:
            // 任一文本框输入变化 → 重新评估"登录"按钮可用性
            if (n->code == DUIN_VALUECHANGED) UpdateLoginEnabled();
            break;

        case IDC_REMEMBER:
            // 复选框翻转
            if (n->code == DUIN_VALUECHANGED) m_remember = (n->extra != 0);
            break;

        case IDC_LOGIN:
            if (n->code == DUIN_CLICK) DoLogin();
            break;

        case IDC_FORGOT:
            if (n->code == DUIN_CLICK) ShowForgotDialog();
            break;

        case IDC_FRIENDLIST:
            if (n->code == DUIN_VALUECHANGED) OnFriendSelected((int)n->extra);
            else if (n->code == DUIN_DBLCLK)  OpenChatWith((int)n->extra);
            else if (n->code == balloonwjui::DuiListBox::DUITN_CHECKED)
                                              OnFriendChecked((int)n->extra);
            break;

        case IDC_TAB_MAIN:
            if (n->code == DUIN_VALUECHANGED) m_tabPage->SetCurSel((int)n->extra, /*notify*/false);
            else if (n->code == balloonwjui::DuiTab::DUITN_CLOSE)
                                              CloseTabAt((int)n->extra);
            break;
        }
        return 0;
    }

    void DoLogin()        { /* ... */ }
    void UpdateLoginEnabled()  { /* ... */ }
    void ShowForgotDialog()    { /* ... */ }
    void OnFriendSelected(int) { /* ... */ }
    void OpenChatWith(int)     { /* ... */ }
    void OnFriendChecked(int)  { /* ... */ }
    void CloseTabAt(int)       { /* ... */ }
    bool m_remember = false;
};
```

**要点**：

- `n->extra` 含义随 code 变化（看每控件的「事件」表）。`DUIN_CLICK` 通常 0；`DUIN_VALUECHANGED` 通常是新值或新索引；`DUITN_CLOSE` 是被请求关闭的索引；等等。
- 派发可以反过来按 `n->code` 先分支，再按 `n->ctrlId` 区分 — 适合"全局所有 click 都打日志"这类场景。
- 处理函数<u>不要</u>同步删除触发事件的控件（`n->pCtrl`），会回到正在执行的控件代码栈，崩。要异步删用 `PostMessage(WM_USER+...)` 自己。

<a id="event-routing-frame"></a>

### 6.4 顶层 DuiFrameWindow 的事件回路

`DuiFrameWindow` 是顶层窗口，没有 HWND 父。`DuiHost::SendNotify` 检测到 `::GetParent(m_hWnd) == NULL` 时，**会把 `WM_DUI_NOTIFY` 发给 host 自己**，所以你在 `DuiFrameWindow` 子类里加消息映射即可：

```
class MyMainWindow : public balloonwjui::DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(MyMainWindow)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)
        // DuiFrameWindow 自己也处理一些消息（标题栏 hit-test、caption
        // 按钮等），用 CHAIN_MSG_MAP 把它们接上：
        CHAIN_MSG_MAP(balloonwjui::DuiFrameWindow)
    END_MSG_MAP()

    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lp, BOOL&)
    {
        auto* n = (balloonwjui::DuiNotify*)lp;
        // 处理你的客户区控件事件
        return 0;
    }
};
```

**min/max/close 三按钮的事件不需要你处理** — `DuiFrameWindow` 内部已把它们的 `DUIN_CLICK` 转成 `WM_SYSCOMMAND`(`SC_MINIMIZE / SC_MAXIMIZE / SC_RESTORE / SC_CLOSE`)。如果想阻止关闭，处理 `WM_CLOSE` 即可。

<a id="event-routing-popup"></a>

### 6.5 弹层 / 菜单的事件路由

弹层窗口（`DuiPopupHost` / `DuiMenu`）不在主窗口的子树里 — 它们是<u>独立顶层窗口</u>。事件需要显式指定接收者：

```
// DuiPopupHost：自定义弹层（下拉、表情面板等）
auto popup = std::make_unique<balloonwjui::DuiPopupHost>();
popup->SetContent(BuildEmojiPanel());
popup->SetOwner(m_hWnd);          // 事件回这个 HWND
popup->SetAnchor(button->GetRect(), balloonwjui::DuiPopupHost::AnchorBelow);
popup->Show();
// 你的对话框 OnDuiNotify 会收到 popup 内子控件的事件，
// n->ctrlId 是子控件的 SetCtrlId。

// DuiMenu：右键 / 命令菜单
balloonwjui::DuiMenu menu;
menu.AddItem(IDM_OPEN,  _T("Open"),   _T("Ctrl+O"));
menu.AddItem(IDM_SAVE,  _T("Save"),   _T("Ctrl+S"));
menu.AddSeparator();
menu.AddItem(IDM_DEL,   _T("Delete"), _T("Del"), /*danger*/ true);
menu.PopupAt(pt, m_hWnd);          // 事件回 m_hWnd
// 你的 OnDuiNotify 收到 DUIN_CLICK，n->ctrlId = IDM_OPEN / IDM_SAVE / IDM_DEL
```

<a id="event-routing-cheatsheet"></a>

### 6.6 速查：每个控件去哪里看自己的事件清单

每个控件的「事件」小节都列了：

- **code**（基础码 `DUIN_*` 或控件特定的 `DUITN_* / DUITVN_* / DUIN_RE_*`）
- **触发条件**（用户操作 + 程序写入是否触发）
- **extra (LPARAM)** 的含义

下表列出哪些控件会发事件、哪些只是渲染：

| 会发事件的控件 | 不发事件（仅渲染 / 容器 / 引擎） |
| --- | --- |
| DuiButton, DuiLabel(链接模式), DuiEditHost, DuiRichEditHost, DuiSearchBox, DuiSpinBox, DuiSlider, DuiProgressBar, DuiComboBox, DuiListBox, DuiTreeView, DuiTab, DuiTabPage, DuiSplitter, DuiScrollBar, DuiEmojiPanel, DuiMenu | DuiVBox / DuiHBox / DuiGrid / DuiDock / DuiTabPage*容器, DuiBadge, DuiAvatar, DuiSeparator, DuiGroupBox, DuiGifControl, DuiImageOle, DuiHost, DuiFrameWindow*shell, DuiPopupHost*shell, DuiToolTipMgr, DuiResMgr, DuiDpi, DuiPaintAA, DuiTheme, DuiNotify |

<a id="control-catalog"></a>

## 7. 控件清单

DuiVBox

DuiHBox

DuiGrid

DuiSplitter

DuiDock

DuiLabel

DuiButton

DuiBadge

DuiAvatar

DuiSeparator

DuiGroupBox

DuiEditHost

DuiRichEditHost

DuiSearchBox

DuiSpinBox

DuiSlider

DuiComboBox

DuiListBox

DuiVirtualList

DuiTreeView

DuiTab

DuiTabPage

DuiMenu

DuiPopupHost

DuiToolTip

DuiProgressBar

DuiEmojiPanel

DuiGif

DuiImageOle

DuiHost

DuiFrameWindow

DuiScrollBar

<a id="control-hosting"></a>

### 7.0 渲染入口与父容器

每一个 DuiControl <u>必须</u>挂在某个 `DuiHost` 子树里才能被渲染 —— DuiHost 持有<u>唯一一个真 HWND</u>，整棵 DUI 子树都画在它的客户区。下面看到每个控件段都有"**典型父：xxx**"一行，回答的就是"这个控件应该挂在什么样的父容器下"，最终所有 leaf 都顶到 `host.GetRoot()`。

```
HWND（顶层窗口 / 老对话框某区域）
└─ DuiHost ──────────────  ← 唯一一个真 HWND
   │  GetRoot() / SetRoot()
   │
   └─ Layout 容器（DuiVBox / DuiHBox / DuiGrid / DuiSplitter / DuiGroupBox / DuiDock）
      ├─ Leaf 控件（DuiButton / DuiLabel / DuiAvatar / DuiSlider / ...）
      ├─ HWND-寄宿控件（DuiEditHost / DuiRichEditHost / DuiSearchBox / DuiSpinBox） + EnsureCreated
      ├─ 列表 / Tab（DuiListBox / DuiTreeView / DuiTab + DuiTabPage）
      └─ 嵌套 Layout 容器（VBox 套 HBox 套 Grid ...）
```

共有 3 种把 DuiHost 挂到操作系统的方式 —— 各控件段的 snippet<u>不再重复</u> host 创建代码，统一参考下面 3 个最简骨架：

#### ① 普通 HWND 寄宿（嵌入老对话框某区域）

给 `DuiHost` 一个 parent HWND，它就在那个 HWND 的客户区里画。常见场景：把一块 DUI 区域嵌入既有的 WTL / MFC 对话框。

```
balloonwjui::DuiHost host;
host.Create(parentHwnd, CWindow::rcDefault, _T("Panel"), WS_CHILD | WS_VISIBLE);

auto vbox = std::unique_ptr<balloonwjui::DuiVBox>(new balloonwjui::DuiVBox());
// vbox->AddChild(...);  ← 这里加你的 leaf 控件（参见各段 snippet）
host.SetRoot(std::move(vbox));
```

#### ② 顶层窗口（DuiFrameWindow）

独立顶层窗口，自带标题栏 + 边框 + min/max/close 三按钮。client area 用 `SetClientContent` 装。

```
balloonwjui::DuiFrameWindow frame;
frame.Create(NULL, CWindow::rcDefault, _T("MyApp"), WS_OVERLAPPEDWINDOW);
frame.SetTitle(_T("我的窗口"));

auto vbox = std::unique_ptr<balloonwjui::DuiVBox>(new balloonwjui::DuiVBox());
// vbox->AddChild(...);
frame.SetClientContent(std::move(vbox));

frame.ShowWindow(SW_SHOW);
```

#### ③ 临时浮层（DuiPopupHost）

菜单 / EmojiPanel / 自定义 popup 的载体。`Show(anchorRect, ownerHwnd)` 首次调用时懒创建 popup HWND；anchorRect 是<u>屏幕坐标</u>，popup 位置按 anchorRect 自动算。

```
balloonwjui::DuiPopupHost popup;
auto vbox = std::unique_ptr<balloonwjui::DuiVBox>(new balloonwjui::DuiVBox());
// vbox->AddChild(...);
popup.SetContent(std::move(vbox));

RECT anchor = ...;  // 例如父按钮的屏幕坐标 rect
popup.Show(anchor, ownerHwnd);
```

#### XML 路径

caller 用 `balloonwjui::DuiXmlBuilder().FromString(xml)` 把 XML 串解析成 `std::unique_ptr<DuiControl>` root，然后按上面 3 种入口之一挂入：

```
const TCHAR* xml = _T(R"xml(
<vbox padding="12" gap="8">
    <label text="用户名" />
    <edit  id="100" placeholder="请输入" />
    <button id="101" text="登录" buttonType="push" />
</vbox>
)xml");

auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));            // 入口 ① 用 host.SetRoot
// 或 frame.SetClientContent(std::move(root));   // 入口 ② 用 frame.SetClientContent
// 或 popup.SetContent(std::move(root));         // 入口 ③ 用 popup.SetContent
```

**HWND-寄宿控件的 EnsureCreated**：`DuiEditHost` / `DuiRichEditHost` / `DuiSearchBox` / `DuiSpinBox` 内部寄宿一个真 EDIT/RICHEDIT 子 HWND，必须在 `host.Create`（已经有真 HWND）<u>之后</u>对每个寄宿控件调一次 `EnsureCreated(host.m_hWnd)`。详见 [§3.4](#xml-ensure-created)。

## 7.1 布局容器

<a id="DuiVBox"></a>

### DuiVBox  `[layout]`

![DuiVBox 纵向盒示意](images/ctl-layout-vbox.png)

*纵向盒：3 个子节点，gap=8，padding=12。*

纵向盒。子节点按声明顺序自上而下排列，每个子节点要么 `fixedMain`（指定高度），要么按 `weight` 分配剩余空间。

**典型父：**任意 DuiControl 容器（含其它 layout 容器），最外层顶到 `host.GetRoot()`。

#### 代码创建

```
auto col = std::unique_ptr<balloonwjui::DuiVBox>(new balloonwjui::DuiVBox());
col->SetPadding(8);
col->SetGap(4);
col->AddChild(std::move(title),   balloonwjui::DuiLayout::Hint().Fixed(28));
col->AddChild(std::move(content), balloonwjui::DuiLayout::Hint().Weight(1));
col->AddChild(std::move(footer),  balloonwjui::DuiLayout::Hint().Fixed(32));
host.SetRoot(std::move(col));     // col 当顶层 root；嵌套时改为 outer->AddChild(std::move(col), ...)
```

#### XML 创建

```
<vbox padding="8" gap="4">
  <label text="Title" fixedHeight="28"/>
  <label text="Body"  weight="1"/>
  <button text="OK"   fixedHeight="32"/>
</vbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### 关键 API

| 方法 | 说明 |
| --- | --- |
| `SetPadding(int)` / `SetPadding(l,t,r,b)` | 容器内边距 |
| `SetGap(int)` | 子节点间距 |
| `AddChild(child, Hint)` | 追加子节点 + 布局提示 |
| `SetHint(child, Hint)` | 修改已存在子节点的布局提示 |

#### Hint 链式构建

`Hint().Fixed(main, cross=-1).Weight(w).Margin(all).AlignM(...).AlignC(...)`

#### 卡片样式（可选；默认全关闭、向后兼容）

设任意一项后，`OnPaint` 先在 `m_rcItem` 上绘"底色 + 圆角 + 描边"，再调基类绘制子控件 —— 让 `DuiVBox` 直接担当卡片容器（替代业务自封装的 `CardBox`）。底色 / 描边走 `DuiAA::FillRoundRect`，自带抗锯齿。

| 方法 | 说明 |
| --- | --- |
| `SetBgColor(COLORREF)` / `GetBgColor()` | 卡片底色。`CLR_INVALID`（默认）= 不画底 |
| `SetCornerRadius(int px)` / `GetCornerRadius()` | 圆角半径（像素）。默认 `0` = 直角；`>= 0` = 圆角；负值钳到 0；最终半径会被 `DuiAA::FillRoundRect` 二次夹到 `min(w,h)/2` |
| `SetBorderColor(COLORREF)` / `GetBorderColor()` | 描边色。`CLR_INVALID`（默认）= 不描边 |
| `SetBorderWidth(float w)` / `GetBorderWidth()` | 描边宽度（像素，浮点）。默认 `1.0`；`<= 0` 视作不描边；负值钳到 0 |
| `PaintBackground(hdc, rc, bg, radius, border=CLR_INVALID, width=1.0)` [static] | 纯函数：不构造 `DuiVBox` 也能在任意 HDC 上画卡片底。供 owner-draw 流程与 `OnPaint` 共用同一段绘制逻辑 —— 替代业务侧的 `DrawCard` / `FillRoundRect + StrokeRoundRect` 双调用 |

```
// 子控件容器写法:
auto card = std::unique_ptr<balloonwjui::DuiVBox>(new balloonwjui::DuiVBox());
card->SetBgColor    (RGB(255, 255, 255));
card->SetCornerRadius(8);
card->SetBorderColor(RGB(232, 236, 240));
card->SetBorderWidth(1.0f);
card->SetPadding(12);
card->AddChild(std::move(title), balloonwjui::DuiLayout::Hint().Fixed(24));
card->AddChild(std::move(body),  balloonwjui::DuiLayout::Hint().Weight(1));

// owner-draw 流程写法(不构造控件):
balloonwjui::DuiVBox::PaintBackground(hdc, rcCard,
    RGB(255,255,255), 8, RGB(232,236,240), 1.0f);
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<vbox padding="..." gap="...">...</vbox>` |
| 详细属性参考 | [§3.3.1 vbox / hbox / grid](#xml-vbox-hbox-grid) |
| 事件 | 无（容器对 WM_DUI_NOTIFY 链透明，子控件事件正常上冒） |

<a id="DuiHBox"></a>

### DuiHBox  `[layout]`

![DuiHBox 横向盒示意](images/ctl-layout-hbox.png)

*横向盒：4 个子节点，gap=8，padding=12。*

横向盒。语义同 `DuiVBox`，主轴变 X。常用于工具栏、按钮排。

**典型父：**任意 DuiControl 容器（含其它 layout 容器），最外层顶到 `host.GetRoot()`。最常见用法是嵌在 VBox 行内做"label + edit + button"工具行（见下例）。

#### 代码创建

```
auto row = std::unique_ptr<balloonwjui::DuiHBox>(new balloonwjui::DuiHBox());
row->SetGap(6);
row->AddChild(std::move(label), balloonwjui::DuiLayout::Hint().Fixed(60));
row->AddChild(std::move(edit),  balloonwjui::DuiLayout::Hint().Weight(1));
row->AddChild(std::move(go),    balloonwjui::DuiLayout::Hint().Fixed(60));
vbox->AddChild(std::move(row),  balloonwjui::DuiLayout::Hint().Fixed(32));
```

#### XML 创建

```
<hbox gap="6" fixedHeight="32">
  <label text="Name:" fixedWidth="60"/>
  <edit  id="100"      weight="1"/>
  <button text="Go"    fixedWidth="60"/>
</hbox>
```

```
// caller 侧（hbox 通常嵌在 vbox 里，所以解析后挂到 outer vbox 的子；
// 直接挂 host root 也合法）：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### 关键 API

|   |   |
| --- | --- |
| `SetPadding / SetGap` | 同 DuiVBox |
| `AddChild(child, Hint)` | 追加子节点 + 布局提示。`Hint.fixedMain` 此时是<u>固定宽度</u>，`Hint.weight` 分剩余宽。 |

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<hbox padding="..." gap="...">...</hbox>` |
| 详细属性参考 | [§3.3.1 vbox / hbox / grid](#xml-vbox-hbox-grid) |
| 事件 | 无（容器透明） |

<a id="DuiGrid"></a>

### DuiGrid  `[layout]`

![DuiGrid 等宽网格 2×3](images/ctl-layout-grid-equal.png)

![DuiGrid 4 列彩色单元](images/ctl-layout-grid-uneven.png)

*上：2×3 等宽网格；下：4×2 彩色网格示意。*

规则网格 (rows × cols)。子节点按声明顺序行优先填充。

**典型父：**任意 DuiControl 容器（含其它 layout 容器），最外层顶到 `host.GetRoot()`。

#### 代码 / XML

```
auto grid = std::make_unique<balloonwjui::DuiGrid>();
grid->SetGrid(2, 3);   // 2 行 3 列
grid->SetGap(4);
for (auto& cell : cells) grid->AddChild(std::move(cell));
host.SetRoot(std::move(grid));        // grid 当顶层；嵌套时改 outer->AddChild(std::move(grid), ...)
```

```
<grid rows="2" cols="3" gap="4">
  <label text="A"/><label text="B"/><label text="C"/>
  <label text="D"/><label text="E"/><label text="F"/>
</grid>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### 关键 API

|   |   |
| --- | --- |
| `SetGrid(rows, cols)` | 必须在 AddChild 前调；缺省按 1×1 排 |
| `SetGap(int)` | 行 / 列之间统一间距 |
| `AddChild(...)` | 子按行优先填入；超过 rows×cols 个的<u>不显示也不溢出滚动</u> |

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<grid rows="..." cols="..." gap="..." padding="...">...</grid>` |
| 详细属性参考 | [§3.3.1 vbox / hbox / grid](#xml-vbox-hbox-grid) |
| 事件 | 无（容器透明） |

<a id="DuiSplitter"></a>

### DuiSplitter  `[DUI]`

![水平 DuiSplitter](images/ctl-splitter-horizontal.png)

![垂直 DuiSplitter](images/ctl-splitter-vertical.png)

*上：水平分隔（左右两栏）；下：垂直分隔（上下两栏）。中间分隔条可拖动。*

横向 / 纵向可拖动分隔条。鼠标拖动改变两侧子区域比例，发 `DUIN_VALUECHANGED`。常用于聊天窗左右栏宽度调节。

**典型父：**任意 DuiControl 容器（含其它 layout 容器），最外层顶到 `host.GetRoot()`。<u>前两个子作 pane 0/1</u>，由 `SetPane(idx, child)` 装入而非通用 AddChild。

#### 代码创建

```
auto sp = std::make_unique<balloonwjui::DuiSplitter>();
sp->SetOrientation(balloonwjui::DuiSplitter::Vertical);   // 纵向条 / 拖动改 X 比例
sp->SetRatio(0.3f);   // 左 30% / 右 70%
sp->SetMinChild(120, 200);   // 左/右最小宽度
sp->SetPane(0, std::move(leftPane));
sp->SetPane(1, std::move(rightPane));
host.SetRoot(std::move(sp));     // 整窗 splitter；嵌套时改 outer->AddChild(std::move(sp), ...)
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 用户拖动分隔条结束（释放鼠标）；拖动过程中实时也发，频率受刷新限制 | 当前 `splitPx`（左 / 上侧主轴像素值） |

业务通常不需要听这个事件（分隔已自动调整子区域），只有当需要持久化用户拖出的比例时才订阅。

```
// 父对话框 OnDuiNotify：保存用户拖出的比例
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_MAIN_SPLITTER && n->code == DUIN_VALUECHANGED) {
    int splitPx = (int)n->extra;
    g_settings.SetInt(_T("MainSplitPx"), splitPx);   // 下次启动恢复
}
```

#### 关键 API

|   |   |
| --- | --- |
| `SetOrientation(Vertical\|Horizontal)` | 分隔条方向。Vertical = 条纵向跑、两 pane 左右排；Horizontal = 条横向跑、两 pane 上下叠 |
| `SetPane(idx, child)` | 装 pane 0 或 1。<u>不要</u>用通用 AddChild —— splitter 要求明确的 slot 语义 |
| `SetSplitPx(px) / SetSplitFraction(f)` | 初始分割位置（绝对 px 或 0..1 比例） |
| `SetMinSizes(min0, min1)` | 每 pane 沿主轴最小尺寸（拖到此值挡住） |
| `SetBarThickness(px)` | 分隔条粗细，默认 4 |

#### XML 创建

```
<splitter orientation="vertical" split-px="240">
  <vbox padding="8"> ... </vbox>     <!-- pane 0 -->
  <vbox padding="8"> ... </vbox>     <!-- pane 1 -->
</splitter>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<splitter orientation="vertical" split-px="240"> <vbox/> <vbox/></splitter>` |
| 详细属性参考 | [§3.3.2 splitter](#xml-splitter) |
| 事件 | `DUIN_VALUECHANGED` — 拖动松手后；`extra` = pane 0 当前像素尺寸 |

<a id="DuiDock"></a>

### DuiDock  `[layout]`

![DuiDock 五区停靠示意](images/ctl-dock-five-zones.png)

*上 / 下 / 左 / 右 / 中央填充五区，颜色块标示停靠方向。*

WPF 风格的停靠布局：每个子节点指定 `Top / Bottom / Left / Right / Fill`。常用于"顶部 toolbar + 底部 statusbar + 中间 client"的窗体框架。

**典型父：**任意 DuiControl 容器（含其它 layout 容器），最外层顶到 `host.GetRoot()`。<u>无 dock-edge 标记的子作中心区</u>（Fill）；同一 edge 多个子按声明顺序由外向内堆叠。

#### 代码创建

```
auto dock = std::make_unique<balloonwjui::DuiDock>();
dock->AddChildDocked(std::move(toolbar), balloonwjui::DuiDock::Top, 32);
dock->AddChildDocked(std::move(status),  balloonwjui::DuiDock::Bottom, 22);
dock->AddChildDocked(std::move(content), balloonwjui::DuiDock::Fill);
host.SetRoot(std::move(dock));    // dock 当顶层；嵌套时改 outer->AddChild(std::move(dock), ...)
```

## 7.2 基础控件

<a id="DuiLabel"></a>

### DuiLabel  `[DUI]`

![单行 DuiLabel](images/ctl-label-single.png)

![多行换行 DuiLabel](images/ctl-label-multiline.png)

![省略号截断 DuiLabel](images/ctl-label-ellipsis.png)

![链接 hover 态 DuiLabel](images/ctl-label-link-hover.png)

*从上到下：单行 / 多行 wrap / 末尾省略 / 链接默认 vs hover 态。*

静态文本 + 超链接。两种模式（与**可选中**能力正交）：

- `ModeText`（默认）：纯文本
- `ModeLink`：下划线 + hover 高亮 + IDC_HAND 光标 + 点击发 `DUIN_CLICK` 或自动 ShellExecute（`SetAutoNavigate`）

支持 **多行 wrap**（`SetWordWrap(true)`）+ **测高**（`MeasureHeight(width)`）— 这是聊天气泡 / 流式列表所必需。

支持 **可选中**（`SetSelectable(true)`）：鼠标拖选高亮 + `Ctrl+C` 复制 + `Ctrl+A` 全选；空选区下 `Ctrl+C` 复制整段。**仅单行模式有效**（与 `SetWordWrap(true)` 共存时本能力自动失效）。典型用途：详情页 / 个人资料的只读值字段，让用户能直接复制不必"先编辑再粘贴"。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。

#### 代码创建

```
auto lbl = std::make_unique<balloonwjui::DuiLabel>();
lbl->SetText(_T("欢迎使用 balloonui"));
lbl->SetTextColor(RGB(40, 40, 40));
lbl->SetWordWrap(true);
int h = lbl->MeasureHeight(300);   // 宽度=300 时所需高度
vbox->AddChild(std::move(lbl), balloonwjui::DuiLayout::Hint().Fixed(h));

// 超链接
auto link = std::make_unique<balloonwjui::DuiLabel>();
link->SetMode(balloonwjui::DuiLabel::ModeLink);
link->SetText(_T("打开网站"));
link->SetUrl (_T("https://example.com"));
link->SetAutoNavigate(true);
vbox->AddChild(std::move(link), balloonwjui::DuiLayout::Hint().Fixed(22));

// 可选中的只读值字段（详情页 / 个人资料常用）
auto val = std::make_unique<balloonwjui::DuiLabel>();
val->SetText(_T("EMP100086"));
val->SetSelectable(true);              // 拖选 + Ctrl+C / Ctrl+A
val->SetSelectionColor(RGB(217, 232, 252));  // 可选；默认就是这个淡蓝
vbox->AddChild(std::move(val), balloonwjui::DuiLayout::Hint().Fixed(22));
```

#### XML 创建

```
<vbox padding="12" gap="6">
    <label text="Hello world" textColor="40,40,40" fixedHeight="22"/>
</vbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### 关键 API

| 方法 | 说明 |
| --- | --- |
| `SetText(LPCTSTR)` / `GetText()` | 文本内容 |
| `SetMode(ModeText\|ModeLink)` | 切换文本 / 链接 |
| `SetTextColor(COLORREF)` | 非链接文本色 |
| `SetLinkColor / SetHoverColor / SetVisitedColor` | 链接配色 |
| `SetWordWrap(bool)` | 开启 DT_WORDBREAK 自动换行 |
| `MeasureHeight(width)` | 返回给定宽度下渲染所需高度（DT_CALCRECT） |
| `SetUrl(LPCTSTR)` + `SetAutoNavigate(true)` | 链接点击自动 ShellExecute |
| `GetMnemonicChar()` | `"打开(&O)"` → `'o'` |
| `SetSelectable(bool)` / `IsSelectable()` | 开启可选中模式（拖选 + Ctrl+C 复制 + Ctrl+A 全选）；默认关。开启后控件可获焦点。**仅单行模式有效**，与 `SetWordWrap(true)` 共存时本能力自动失效 |
| `SetSelectionColor(COLORREF)` / `GetSelectionColor()` | 选区高亮背景色；默认 `RGB(217, 232, 252)` 淡蓝 |

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_CLICK` | 仅 `ModeLink` 模式：用户左键点击文本。`ModeText` 不发任何事件。`SetAutoNavigate(true)` 时点击不再发 `DUIN_CLICK` 而直接 `ShellExecute` 打开 URL | 0 |

```
// 父对话框 OnDuiNotify：
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_FORGOT_LINK && n->code == DUIN_CLICK) {
    ShowForgotPasswordDialog();
}
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<label text="..." textColor="..."/>` |
| 详细属性参考 | [§3.3.4 label](#xml-label) |
| 事件 | `ModeText` 无；`ModeLink` + `SetAutoNavigate(false)` 时发 `DUIN_CLICK` |
| 说明 | XML 暂不暴露 ModeLink / 字体 / 对齐 等高级属性，需要时拿到 builder 返回的 root 后用 FindControlById + SetXxx 自己设 |

<a id="DuiButton"></a>

### DuiButton  `[DUI]`

![DuiButton 四种风格](images/ctl-button-styles-overview.png)

![PushButton 4 状态](images/ctl-button-pushbutton-states.png)

*上：4 种风格（PushButton / Checkbox / Radio / Icon）；下：PushButton 的 Normal / Hover / Pressed / Disabled。*

四种风格：

| Style | 外观 / 用途 |
| --- | --- |
| `StylePushButton` | 实心品牌蓝 + 8px 圆角，最常用 |
| `StyleCheckbox` | 左侧方形 tick + 文本，`SetCheck` 切换 |
| `StyleRadio` | 同 Checkbox 但同 group 互斥（`SetRadioGroup(gid)`） |
| `StyleIcon` | 左侧图标 + 文本，紧凑（如工具按钮） |

支持 **per-state 背景位图**（`SetBgBitmap(normal, hover, pressed, disabled)`）+ **9-grid 拉伸**（`SetBgInsets`）。位图调用方拥有，按钮不拷贝不释放。

另外支持 **视觉变体 `Variant`**（与 `Style` 正交，**仅对 `StylePushButton` 生效**）—— 一组语义化的"按钮皮肤"：

| Variant | 视觉 / 语义 | 典型用途 |
| --- | --- | --- |
| `Primary` | 品牌主色实心 + 白字（默认；与历史 PushButton 一致） | 主操作（Save / 登录 / 创建） |
| `Default` | 白底 + 1px 灰边 + 深字 | 次操作（取消 / 导出） |
| `Outlined` | 透明底 + 1px 品牌色边 + 品牌色字 | 强次操作 / 表单内的辅助主操作 |
| `Ghost` | 透明 + 深字，hover 才有浅底，无边 | 弱操作 / 工具栏按钮 |
| `Danger` | 危险红实心 + 白字 | 不可逆操作（删除 / 重置 / 停用） |
| `Text` | 纯文字、无底无边、hover 才浮出浅底 | 链接式按钮（"忘记密码？"） |

`StyleIcon` 忽略 Variant，始终按既有 kLight 色板渲染。`SetBgBitmap` 设了位图皮肤时位图优先于 Variant —— 不要同时使用两者。各 Variant 在 4 个状态（Normal / Hover / Pressed / Disabled）下的具体颜色由 `PaletteFor(v, s)` 静态纯函数返回，可单测、无 HDC 依赖。

**`StyleCheckbox` / `StyleRadio` 的特例：**三个透明 Variant（`Ghost` / `Outlined` / `Text`）会接管<u>外框</u>色板，让 Checkbox / Radio 也能呈现「无边框无背景」样式 —— 适合放在列表行 / 紧凑表单里。其余 Variant（`Primary` / `Default` / `Danger`）在 Checkbox / Radio 上会兜底回 kLight 色板，与历史视觉一致。内部 glyph（小方框 / 圆环）始终走 kLight 色板以保证可见；透明 Variant 下 glyph 内部填充也跟着透明（呈"空心"形态）。

**抗锯齿：**外框圆角与 Checkbox 方框 glyph 默认走 `DuiAA::FillRoundRect`（GDI+ 抗锯齿），8px 圆角无可见阶梯像素。可通过 `SetAntiAlias(false)` 退回 GDI `::RoundRect` 兜底路径（用于极致性能或对比测试）。Radio 的圆形 glyph 始终 AA（走 `DuiAA::FillEllipse`），与该开关无关。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。

#### 代码创建

```
// 默认 PushButton + Primary（最常见的主操作按钮）
auto btn = std::make_unique<balloonwjui::DuiButton>();
btn->SetButtonType(balloonwjui::DuiButton::StylePushButton);
btn->SetText(_T("&Save"));        // & 标记快捷键 'S'
btn->SetCtrlId(IDC_SAVE);
vbox->AddChild(std::move(btn), balloonwjui::DuiLayout::Hint().Fixed(32));

// 改成 Danger 变体（不可逆操作）：
btn->SetVariant(balloonwjui::DuiButton::Variant::Danger);

// 改成 Ghost 变体（弱操作，工具栏按钮）：
btn->SetVariant(balloonwjui::DuiButton::Variant::Ghost);

// 父对话框 WM_DUI_NOTIFY:
//   if (n->code == DUIN_CLICK && n->ctrlId == IDC_SAVE) Save();
```

#### XML 创建

```
<vbox padding="12" gap="8">
    <button id="100" text="Save"          buttonType="push"     fixedHeight="32"/>
    <button id="101" text="Show password" buttonType="checkbox"/>
    <button id="102" text="Light"         buttonType="radio"/>
    <button id="103" text="Dark"          buttonType="radio"/>
</vbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### 关键 API

| 方法 | 说明 |
| --- | --- |
| `SetButtonType / GetButtonType` | 切换四种 Style |
| `SetText / GetText` | `"&X"` 中的字符自动作为快捷键 |
| `SetCheck(bool, notify=true)` / `IsChecked()` | Checkbox/Radio 状态 |
| `SetRadioGroup(int gid)` | 非 0 group id 的同父 Radio 组成互斥组 |
| `SetBgBitmap(n,h,p,d)` | per-state 背景位图（caller-owned） |
| `SetBgInsets(l,t,r,b)` | 9-grid 切边 |
| `SetTextColor / SetTextAlign` | 文字色与 DT_* 对齐（StylePushButton 文字色被 Variant palette 覆盖） |
| `GetMnemonicChar()` | 取出 `&` 后的字符 |
| `SetVariant(Variant) / GetVariant()` | 视觉变体 Primary / Default / Outlined / Ghost / Danger / Text。`StylePushButton` 全部 6 个 Variant 都生效；`StyleCheckbox` / `StyleRadio` 仅 Ghost / Outlined / Text 三个透明 Variant 接管外框色板，其余 Variant 兜底回 kLight；`StyleIcon` 忽略。与 `SetBgBitmap` 冲突时位图优先 |
| `PaletteFor(Variant, ButtonState)` [static] | 纯函数：返回 `ButtonPalette{ bg, text, border }`。`bg` / `border` 取 `CLR_INVALID` 表示透明 / 不描边 |
| `SetAntiAlias(bool) / IsAntiAlias()` | 外框 / Checkbox 方框 glyph 是否走抗锯齿绘制（默认 `true`）。关时退回 GDI `::RoundRect` 兜底（8px 圆角可见锯齿）；开时走 `DuiAA::FillRoundRect`。Radio 圆形 glyph 始终 AA，与该开关无关 |
| `SetFont(HFONT) / GetFont()` | 设置自定义文字字体。HFONT caller-owned，控件不释放。`nullptr`（默认）走 `DuiResMgr::GetDefaultFont()`（YaHei 9pt） |
| `SetTextPointSize(int pt, bool bold = false)` | 便捷 setter：按磅值 + 粗细从 `DuiResMgr::GetFontByPointSize` 拿缓存字体并 `SetFont`。`pt <= 0` 退化为默认字体 |
| `SetLeadingIcon(HBITMAP) / GetLeadingIcon()` | 设置文字左侧图标位图。**仅 `StylePushButton` 生效**；其它 Style 忽略。HBITMAP caller-owned。图标走 `::AlphaBlend`，支持 32bpp 预乘 alpha。整组（图标 + gap + 文字）按 `m_dtFlags` 对齐（默认水平居中） |
| `SetLeadingIconSize(int px)` / `SetLeadingIconGap(int px)` | 图标绘制边长（默认 `16`，`<= 0` 钳到 1）/ 图标与文字间距（默认 `6`，`< 0` 钳到 0） |

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_CLICK` | 任何 Style 在按钮内按下并在按钮内释放（鼠标移出再释放<u>不</u>触发） | 0 |
| `DUIN_VALUECHANGED` | Checkbox / Radio 状态翻转。`SetCheck(_, true)` 也会触发；`SetCheck(_, false)` 不触发 | 新选中状态：`1`=checked / `0`=unchecked |

```
// 父对话框：
LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lp, BOOL&) {
    auto* n = (balloonwjui::DuiNotify*)lp;
    if (n->ctrlId == IDC_SAVE   && n->code == DUIN_CLICK)        DoSave();
    if (n->ctrlId == IDC_REMEMBER && n->code == DUIN_VALUECHANGED) m_remember = (n->extra != 0);
    return 0;
}
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<button id="..." text="..." buttonType="push\|icon\|checkbox\|radio"/>` |
| 详细属性参考 | [§3.3.9 button](#xml-button) |
| 事件 | `DUIN_CLICK`（所有 Style）；`DUIN_VALUECHANGED`（Checkbox / Radio 切换，extra = 0/1） |
| 说明 | XML 暂不暴露 SetRadioGroup / SetBgBitmap / SetTextColor 等高级属性，需要时拿到 root 后用 FindControlById 自己设 |

<a id="DuiBadge"></a>

### DuiBadge  `[DUI]`

![DuiBadge 几种形态](images/ctl-badge-types.png)

*左→右：红点 / 数字 / "99+" / 自定义品牌色。*

未读消息红点 / 数字徽章，常叠在头像或图标的右上角。`SetCount(0)` 隐藏；`SetCount(-1)` 显示纯红圆点；`>= 1` 显示数字（>99 显示 "99+"）。

另支持 **前导小圆点**（`SetLeadingDot(color)`）：在文字左侧画一个独立色的小圆点 + 间距，整组在徽章内居中。典型用于状态指示如「● 运行中」「● 停机」「● 警告」。圆点 + gap 自动计入徽章宽度，与既有的红色圆形 / 数字胶囊正交。

**形状参数化（chip 扩展）：**`DuiBadge` 现可覆盖「短计数 / 红点」与「长文字状态标签」两种形态：

- `SetCornerRadius(int r)` —— 圆角半径。`-1`（默认）= 自动胶囊形（圆角 = 高/2，与历史一致）；`>= 0` = 固定半径（chip 形态，推荐 4 或 6）；`0` = 直角矩形。最终半径会被 `DuiAA::FillRoundRect` 二次夹到 `min(w,h)/2`，设过大也安全。
- `SetMaxDisplayChars(int n)` —— 显示字符数上限。`4`（默认，与历史一致，"99+" 链路天然不超 4）；`0` = 不截（长文字标签必用）；`> 0` = 截到 n 字（直接砍，<u>不</u>加省略号）。<u>截断逻辑搬到 `OnPaint`，`SetText` 保存的是原始文本</u> —— `GetText()` 返回原始，不是显示版本（与历史的"`SetText` 时截"语义不同，但默认 4 仍能保持显示一致）。

合并后，`DuiBadge` 既能担任原"未读红点"，也能担任 `flamingoAdmin::ui::DrawChip` / `DrawStatusChip` 那种「圆角矩形 + 浅灰底 + 深字 + 语义圆点」的状态徽章。OnPaint 背景统一走 `DuiAA::FillRoundRect`（GDI+ 抗锯齿），不再分"圆 / 胶囊"两条绘制路径。

**典型 chip 用法（与 ServicesPanel 状态徽章一致）：**

```
auto chip = std::make_unique<balloonwjui::DuiBadge>();
chip->SetText(_T("已启用"));
chip->SetCornerRadius(4);             // chip 形态(默认胶囊)
chip->SetMaxDisplayChars(0);          // 不截(默认 4)
chip->SetLeadingDot(RGB(60, 200, 120)); // 绿点 = OK
chip->SetBgColor   (RGB(245, 246, 248)); // 浅灰底
chip->SetTextColor (RGB( 80,  88, 102)); // 深字
```

**典型父：**两种用法 ——

  ① 普通 leaf：嵌任意 layout 容器（VBox/HBox/Grid 等），独立显示数字徽章；

  ② <u>角标 overlay</u>：<u>父控件</u>（DuiAvatar / DuiTab / DuiButton 等）持有 DuiBadge 子并在自己 OnPaint / Layout 里<u>显式定位</u> badge 的 rect 到右上角 —— 不靠 layout 容器自动放，而是父按"主体右上角"的几何手动算坐标。

#### 代码创建

```
// 用法 ①：嵌 layout 容器作普通 leaf
auto badge = std::make_unique<balloonwjui::DuiBadge>();
badge->SetCount(unread);             // 0 隐藏，1-99 数字，100+ 显示 "99+"
badge->SetBgColor(RGB(220, 60, 60));
badge->SetTextColor(RGB(255, 255, 255));
hbox->AddChild(std::move(badge), balloonwjui::DuiLayout::Hint().Fixed(20));

// 用法 ②：作头像角标（父 = DuiAvatar 子类，自家 Layout() 里定位）
//   m_badge 是 DuiAvatar 的成员；override Layout() 把 badge rect 设到右上角：
//     RECT av = GetRect();
//     m_badge->SetRect(RECT{ av.right - 18, av.top - 4, av.right + 4, av.top + 14 });

// 用法 ③：带前导小圆点的状态徽章（如「● 运行中」「● 停机」）
auto status = std::make_unique<balloonwjui::DuiBadge>();
status->SetText(_T("运行中"));
status->SetTextColor(RGB(80, 80, 80));
status->SetBgColor  (RGB(245, 246, 248));
status->SetLeadingDot(RGB(60, 200, 120));   // 绿点 = OK
// 可选：SetLeadingDotRadius(4)，SetLeadingGap(6)
hbox->AddChild(std::move(status), balloonwjui::DuiLayout::Hint().Fixed(20));
```

#### 关键 API

|   |   |
| --- | --- |
| `SetText(LPCTSTR)` / `GetText()` | **保存原始文本**（不在此处截断；显示时按 `MaxDisplayChars` 截）。`GetText()` 返回的是原始值，<u>不</u>是显示版本 |
| `SetCount(int)` | 整数计数自动转文字：0 → 隐藏；1-99 → 数字；100+ → "99+" |
| `SetBgColor / SetTextColor` | 底色 / 字色（默认红底白字） |
| `SetHideWhenEmpty(bool)` | 空文本是否完全跳过绘制（默认 true） |
| `IsShowing()` | 当前是否会画出 |
| `FormatCount(int)` [static] | 纯函数：把 N 转成 "" / "99+" / "" |
| `SetLeadingDot(COLORREF)` / `GetLeadingDotColor()` / `HasLeadingDot()` | 开 / 关前导小圆点；传 `CLR_INVALID` 清除。圆点 + gap 自动计入徽章宽度 |
| `SetLeadingDotRadius(int)` | 圆点半径（像素）；`<= 0`（默认）按字体高度自适配 = `max(3, fontHeight/4)` |
| `SetLeadingGap(int)` | 圆点与文字间距（像素）；默认 4 |
| `SetCornerRadius(int r)` / `GetCornerRadius()` | 圆角半径。`-1`（默认）= 胶囊形（高/2）；`>= 0` = 固定半径（chip 形态，推荐 4 或 6）；`0` = 直角；`< -1`（误用）兜底为 0 |
| `SetMaxDisplayChars(int n)` / `GetMaxDisplayChars()` | 显示字符数上限。`0` = 不截（chip 用法必用）；`> 0` = 截到 n 字（直接砍，不加省略号）；默认 `4`，与历史一致；`< 0` 兜底为不截 |
| `ContentWidth(textW, r, gap, hasDot)` [static] | 纯函数：算徽章内容宽度（不含 padding） |
| `AutoDotRadius(fontHeight)` [static] | 纯函数：自适配圆点半径规则 |
| `EffectiveCornerRadius(rawRadius, height)` [static] | 纯函数：算实际生效圆角。`-1` → `height/2`；`>= 0` → 原值；`< -1` → 0 |
| `ApplyMaxChars(text, maxChars)` [static] | 纯函数：应用截断规则。`maxChars <= 0` → 不截；否则截到前 `n` 字（按 `CString::GetLength()`，中文 1 字算 1 个） |

#### XML 创建

```
<hbox padding="8" gap="4">
    <badge count="3"   bg-color="220,60,60"/>
    <badge text="NEW"  bg-color="40,140,80"/>
</hbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<badge count="3" bg-color="220,60,60"/>` 或 `<badge text="NEW" bg-color="40,140,80"/>` |
| 详细属性参考 | [§3.3.7 badge](#xml-badge) |
| 事件 | 无（纯绘制） |

<a id="DuiToast"></a>

### DuiToast  `[DUI]`

顶层浮出的轻量提示控件。典型形态:窗口顶部水平居中、深色圆角底 + 左侧图标 + 文字, 持续约 3 秒后自动渐隐消失。**非交互**(HitTest 永远返回 nullptr) —— 点击穿透到下层控件, 不会抢焦。

**典型父:**任意承载 page 的容器(如 admin 的 RootContainer / DuiGallery 的 page VBox)。<u>把 DuiToast 加在父 m_children 末尾</u> —— 基类 OnPaint 按子顺序绘制, 末尾即最上层, 自动顶层显示。

#### 代码用法

```
auto toast = std::make_unique<balloonwjui::DuiToast>();
toast->SetBgColor(RGB(245, 158, 11));      // 警告橙
toast->SetTextColor(RGB(255, 255, 255));   // 白字
toast->SetIcon(myWarningBitmap);           // caller-owned 32bpp PARGB
toast->SetDurationMs(3000);                // 3 秒
m_root->AddChild(std::move(toast));        // 末尾 = 最上层

// 业务事件触发:
m_toast->Show(_T("请先在左侧选择一个 agent"));
```

#### 工作机制

- **动画链路**: Show → 渐入(默认 200ms, alpha 0→1) → 显示等待(默认 3000ms) → 渐出(200ms, alpha 1→0) → SetVisible(false)。全部走 `DuiAnimMgr` 自驱, caller 不需要 SetTimer。
- **多次 Show 不堆积**: 内部代际号 `m_animGen` 让旧链路 callback 自废, 新文本立刻从当前 alpha 起渐入, 视觉无跳变。
- **渲染走 PARGB Bitmap + ::AlphaBlend**: **背景圆角与文字**都在 GDI+ `PixelFormat32bppPARGB` Bitmap 上画(背景走 `FillPath`、文字走 `DrawString`),alpha 通道由 GDI+ 统一管理;LockBits 复制到 BI_RGB DIB 后,**图标**走 GDI `::AlphaBlend`(图标本身是 32bpp 预乘 alpha HBITMAP);最后整张 DIB 用 `AC_SRC_ALPHA + SourceConstantAlpha=255×m_alpha` 合成到 host —— 既支持圆角外透明、又支持整体渐入渐出。<u>注</u>:文字必须走 GDI+ 而非 GDI ::DrawText —— 后者在 BI_RGB 32bpp DIB 上会污染 alpha 通道,产生"重影"(已实证)。
- **位置自定位**: `Layout` 忽略父分配的 rcAvail, 始终用 `GetParent()->GetRect()` 算"顶居中 + topOffset"。`Show()` 时主动调一次 `Layout(parent rect)` 立刻刷 m_rcItem,避免首次显示时父 layout 还没传 rect 导致 0×0 看不见。

#### 关键 API

| 方法 | 说明 |
| --- | --- |
| `Show(LPCTSTR text)` | 显示一条提示;多次 Show 替换前一条 + 重置计时。空文本退化为 HideNow。 |
| `HideNow()` | 立即隐藏, 取消未完成动画, m_alpha 归 0。 |
| `IsActive()` | 当前是否处于显示态(含渐入 / 显示 / 渐出)。 |
| `SetDurationMs(int ms) / GetDurationMs()` | 显示时长(不含淡入淡出);默认 3000。<= 0 钳到 1。 |
| `SetFadeMs(int ms) / GetFadeMs()` | 渐入 / 渐出动画时长;默认 200。0 = 硬切;负值钳到 0。 |
| `SetTextColor(COLORREF) / GetTextColor()` | 文字色;默认白 `RGB(255,255,255)`。 |
| `SetBgColor(COLORREF) / GetBgColor()` | 背景色;默认深灰 `RGB(50,50,50)`。 |
| `SetCornerRadius(int) / GetCornerRadius()` | 圆角半径(像素);默认 16;负值钳 0。走 `DuiAA::FillRoundRect` 抗锯齿。 |
| `SetIcon(HBITMAP) / GetIcon()` | 左侧图标位图。HBITMAP caller-owned, 控件不释放;nullptr(默认) = 无图标。走 `::AlphaBlend` 支持 32bpp 预乘 alpha。 |
| `SetIconSize(int) / GetIconSize()` | 图标边长(像素, 正方形);默认 16;<= 0 钳到 1。 |
| `SetIconGap(int) / GetIconGap()` | 图标与文字间距;默认 8;<0 钳 0。 |
| `SetFont(HFONT) / GetFont()` | 自定义字体。HFONT caller-owned, 控件不释放;nullptr(默认) 走 toast 内部默认字体(YaHei 9pt + ANTIALIASED_QUALITY)。<u>注</u>:由于 toast 走 PARGB + AlphaBlend 合成, ClearType 字体会出现子像素错位"重影",caller 应传 `lfQuality=ANTIALIASED_QUALITY` 的 HFONT;或直接走 SetTextPointSize 内部自动用 AA 字体。 |
| `SetTextPointSize(int pt, bool bold=false)` | 便捷 setter:按磅值 + 粗细从 `DuiResMgr::GetAntiAliasedFontByPointSize` 拿 AA 缓存字体并 `SetFont`。pt <= 0 退化为默认字体(等同 SetFont(nullptr))。 |
| `SetTopOffset(int) / GetTopOffset()` | 距父客户区顶的偏移(像素);默认 40。 |
| `SetMaxWidth(int) / GetMaxWidth()` | 最大宽度(像素);默认 0 = 不限;>0 时文字超出 (maxWidth - icon - gap - 2×padding) 后截断加 "…"。 |
| `MeasureWidth(textPx, hasIcon, iconSize, iconGap)` [static] | 纯函数:量算 toast 总宽。 |
| `ApplyEllipsis(text, maxChars)` [static] | 纯函数:按 maxChars 截断 + 加 "…"。<=0 不截;<3 退化硬截。 |

#### XML 创建

暂未原生支持 `<toast>` 标签 —— toast 是运行时浮出, 不在静态布局描述里。需要静态注册请走 `DuiXmlBuilder::CustomFactory`。

事件:无(纯展示, 不参与 WM_DUI_NOTIFY)。

<a id="DuiAvatar"></a>

### DuiAvatar  `[DUI]`

![DuiAvatar 多种形态](images/ctl-avatar-grid.png)

*从左到右：圆形位图 / 圆角位图 / 首字母回退（英文 + 中文）/ 4 种状态点（在线 / 离开 / 忙碌 / 离线）。*

头像：圆形 / 圆角矩形 + 可选状态点（在线 / 离开 / 忙碌 / 离线）。两种模式：

- 有 `HBITMAP`：直接绘制源图（caller-owned，不拷贝）
- 无图：纯色背景 + 自动生成的 1-2 字符首字母（"Alice Smith" → "AS"）

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。最常见用法是嵌在联系人行 HBox（avatar + name + status badge）。

#### 代码创建

```
auto av = std::make_unique<balloonwjui::DuiAvatar>();
av->SetName(_T("Alice Smith"));        // 无图时显示 "AS"
av->SetBitmap(hBmp);                   // 有图则用图
av->SetShape(balloonwjui::DuiAvatar::ShapeCircle);
av->SetStatus(balloonwjui::DuiAvatar::StatusOnline);
hbox->AddChild(std::move(av), balloonwjui::DuiLayout::Hint().Fixed(48));   // 联系人行
```

#### 关键 API

|   |   |
| --- | --- |
| `SetBitmap(HBITMAP)` | 源图（nullptr 回退首字母） |
| `SetName(LPCTSTR)` | 首字母回退用名字 |
| `SetShape(ShapeCircle\|ShapeRoundRect)` | 形状 |
| `SetCornerRadius(int)` | 圆角矩形圆角 |
| `SetStatus(StatusNone\|Online\|Away\|Busy\|Offline)` | 右下角状态点 |
| `SetFallbackBgColor / SetInitialsColor` | 无图模式配色 |
| `ComputeInitials(name)` [static] | 纯函数：取首字母 |

#### XML 创建

```
<hbox padding="8" gap="8">
    <avatar id="200" name="Alice Smith" shape="circle" status="online" fixedWidth="48"/>
    <label  text="Alice Smith" weight="1"/>
</hbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
// 解析后取 avatar 节点挂图（XML 不支持 image="path"）：
if (auto* av = static_cast<balloonwjui::DuiAvatar*>(root->FindControlById(200))) {
    av->SetBitmap(LoadAvatarBitmap(...));
}
host.SetRoot(std::move(root));
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<avatar name="..." shape="circle" status="online"/>` |
| 详细属性参考 | [§3.3.6 avatar](#xml-avatar) |
| 事件 | 无（纯绘制） |
| 说明 | XML <u>不</u>支持 `image="path"`（HBITMAP 加载交业务侧）；caller 拿到 builder 返回的 root 后调 `av->SetBitmap(hbm)` 自己挂图 |

<a id="DuiSeparator"></a>

### DuiSeparator  `[DUI]`

![水平 DuiSeparator](images/ctl-separator-horizontal.png)

![DuiSeparator 配标签](images/ctl-separator-labeled.png)

*上：水平 1px 分隔线；下：垂直分隔与左右标签搭配。*

1px 横线 / 竖线，常用于菜单项分隔、面板分组。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。横向 separator 通常在 VBox 中两个内容块之间；纵向 separator 通常在 HBox 中两个工具组之间。

#### 代码创建

```
auto sep = std::make_unique<balloonwjui::DuiSeparator>();
sep->SetOrientation(balloonwjui::DuiSeparator::Vertical);
sep->SetColor(RGB(220, 220, 224));
sep->SetThickness(1);
sep->SetInset(4);                  // 两端各内缩 4px
hbox->AddChild(std::move(sep), balloonwjui::DuiLayout::Hint().Fixed(1));   // 1px 宽，行高填满
```

#### 关键 API

|   |   |
| --- | --- |
| `SetOrientation(Horizontal\|Vertical)` | 线方向。Horizontal = 横线（默认）/ Vertical = 竖线 |
| `SetColor(COLORREF)` | 线色。默认 RGB(220,220,224) |
| `SetThickness(int)` | 线粗（垂直于线方向）。默认 1，clamp >= 1 |
| `SetInset(int)` | 沿线方向两端各内缩多少 px（短一截的精致效果）。默认 0 |

#### XML 创建

```
<hbox padding="8" gap="8" fixedHeight="32">
    <button text="Cut"  fixedWidth="60"/>
    <button text="Copy" fixedWidth="60"/>
    <separator orientation="vertical" inset="4" fixedWidth="1"/>
    <button text="Paste" fixedWidth="60"/>
</hbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<separator orientation="vertical" color="..." thickness="1" inset="4"/>` |
| 详细属性参考 | [§3.3.5 separator](#xml-separator) |
| 事件 | 无（纯绘制 + 鼠标透过） |

<a id="DuiGroupBox"></a>

### DuiGroupBox  `[DUI]`

![DuiGroupBox 带标题分组框](images/ctl-groupbox-sample.png)

*带标题条的分组框，可嵌套任意子控件。*

带标题 + 描边的分组框。子节点是其内部任意 DuiControl。

**典型父：**任意 DuiControl 容器（含其它 layout 容器），最外层顶到 `host.GetRoot()`。<u>第一个子作内容区</u>，由 `SetContent(std::move(inner))` 装入而非通用 AddChild —— groupbox 只承一个内容子，要多个控件请先嵌一层 VBox/HBox。

#### 代码创建

```
auto gb = std::make_unique<balloonwjui::DuiGroupBox>();
gb->SetTitle(_T("用户偏好"));
gb->SetTitleColor(RGB(80, 80, 80));
gb->SetPadding(12);
auto inner = std::make_unique<balloonwjui::DuiVBox>();
// ... 装表单行到 inner ...
gb->SetContent(std::move(inner));    // ★ 单内容子，不用 AddChild
vbox->AddChild(std::move(gb), balloonwjui::DuiLayout::Hint().Weight(1));
```

#### 关键 API

|   |   |
| --- | --- |
| `SetTitle / SetTitleColor` | 标题文字 + 颜色 |
| `SetBorderColor / SetCornerRadius` | 边框色 + 圆角半径（默认 6 px） |
| `SetTitleStripHeight(px)` | 顶部标题条高度（默认 24，内容从此条之下开始） |
| `SetPadding(int) / SetPadding(l,t,r,b)` | 内容区相对边框的 4 边内距（默认 12） |
| `SetContent(unique_ptr<DuiControl>)` | 装 / 替换<u>唯一</u>内容子（旧的销毁） |

#### XML 创建

```
<groupbox title="用户偏好" padding="12">
    <vbox gap="8">
        <label  text="昵称"/>
        <edit   id="200" placeholder="必填"/>
        <label  text="邮箱"/>
        <edit   id="201" placeholder="user@example.com"/>
    </vbox>
</groupbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<groupbox title="..." padding="..."> <vbox/></groupbox>`（第一个子元素自动作为内容） |
| 详细属性参考 | [§3.3.3 groupbox](#xml-groupbox) |
| 事件 | 无（纯绘制 + 转发 hit-test 到内容） |

## 7.3 输入

<a id="DuiEditHost"></a>

### DuiEditHost  `[HWND-hosted]`

![DuiEditHost 4 状态](images/ctl-edit-states.png)

*左→右：占位符（空）/ 已填值 / focus 高亮 / disabled。EDIT 内文字在屏外快照中可能被裁掉，实际运行时正常。*

纯文本单行 / 多行编辑控件。**因为需要 IME 输入法**，内部嵌入真 EDIT HWND（通过 `HwndHostControl` 适配器）。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。<u>挂入后还须在 host HWND 就绪时调一次</u> `edit->EnsureCreated(host.m_hWnd)` 才能真正建出 EDIT 子 HWND（详见 [§3.4](#xml-ensure-created)）。

#### 代码创建

```
auto edit = std::make_unique<balloonwjui::DuiEditHost>();
edit->SetCtrlId(100);
edit->SetPlaceholder(_T("输入用户名"));
edit->SetMultiLine(false);
edit->SetPassword(false);
balloonwjui::DuiEditHost* editRaw = edit.get();
vbox->AddChild(std::move(edit), balloonwjui::DuiLayout::Hint().Fixed(28));

// host.Create(...) 之后调一次：
editRaw->EnsureCreated(host.m_hWnd);
```

#### XML 创建

```
<vbox padding="12" gap="6">
    <edit id="100" placeholder="输入用户名" fixedHeight="28"/>
    <edit id="101" password="true"          fixedHeight="28"/>
    <edit id="102" multiline="true"         weight="1"/>
</vbox>
```

```
// caller 侧（注意 EnsureCreated）：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
// host HWND 就绪后，对每个 <edit> 节点调一次 EnsureCreated：
for (UINT id : { 100u, 101u, 102u }) {
    if (auto* e = static_cast<balloonwjui::DuiEditHost*>(host.GetRoot()->FindControlById(id)))
        e->EnsureCreated(host.m_hWnd);
}
```

|   |   |
| --- | --- |
| `SetText / GetText` | 文本内容（同步 EDIT） |
| `SetPlaceholder` | 空文本时显示的灰色提示 |
| `SetPassword(bool)` | 密码模式（显示 *） |
| `SetMultiLine(bool)` | 多行 |
| `SetReadOnly / SetMaxLength` | 只读 / 长度限制 |

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | EDIT 内容变化（响应每次 `EN_CHANGE`，包括 IME 上屏每个汉字、剪切板粘贴、用户键入）。可在 handler 里调 `GetText()` 拿最新值。 | 0 |
| `DUIN_SETFOCUS` | EDIT 拿到键盘焦点（`EN_SETFOCUS`） | 0 |
| `DUIN_KILLFOCUS` | EDIT 失焦（`EN_KILLFOCUS`），适合做"提交 / 校验 / 清空 placeholder 状态" | 0 |

表单常见模式：在 `DUIN_VALUECHANGED` 里实时校验并禁用提交按钮；在 `DUIN_KILLFOCUS` 里把临时输入持久化到模型。

```
// 父对话框 OnDuiNotify：
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_NICKNAME) {
    if (n->code == DUIN_VALUECHANGED) {
        // 实时校验
        CString s = m_editNickname->GetText();
        m_btnLogin->SetEnabled(!s.IsEmpty() && s.GetLength() <= 32);
    }
    else if (n->code == DUIN_KILLFOCUS) {
        // 持久化
        m_model.nickname = m_editNickname->GetText();
    }
}
```

**EnsureCreated 约定**：DuiEditHost 内嵌真 EDIT HWND，必须在 host HWND 就绪后调 `edit->EnsureCreated(host->m_hWnd)` 一次才会真正建出来。XML 路径下 builder<u>不会</u>替你调；caller 自己用 `FindControlById` 找到节点调一次。详见 [§3.4 EnsureCreated 约定](#xml-ensure-created)。

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<edit id="..." placeholder="..." password="false" multiline="false"/>` |
| 详细属性参考 | [§3.3.12 edit](#xml-edit) |
| 事件 | `DUIN_VALUECHANGED` / `DUIN_SETFOCUS` / `DUIN_KILLFOCUS` |

<a id="DuiEditHost-IconApi"></a>

#### 内联图标 + 底色 + 边框（默认全部关闭，零回归）

EDIT 框的左 / 右 gutter 可以塞图标，常见用途：搜索框左侧放大镜、邮箱框左侧 `@`、密码框右侧 👁。EDIT 文字区<u>自动避让 gutter</u>（Layout 把 EDIT HWND 内缩对应宽度），不会盖到图标。配合 `SetBgColor` 让 EDIT 底色融进容器圆角灰底，配合 `SetShowBorder(false)` 去掉默认 1px 边框，实现"图标浮在 pill 灰底里、看不到 EDIT 边界"的效果。

**样式示意**（左：默认 / 中：左侧放大镜 + 灰底无边框 / 右：右侧 × 清除）：

<svg width="660" height="42" viewbox="0 0 660 42" xmlns="http://www.w3.org/2000/svg" font-family="Microsoft YaHei,Segoe UI,sans-serif">

      <rect x="0" y="5" width="200" height="32" fill="#FFFFFF" stroke="#D0D0D0"></rect>
      <text x="8" y="25" font-size="12" fill="#999">输入用户名</text>


      <rect x="220" y="5" width="200" height="32" rx="16" ry="16" fill="#F3F3F4"></rect>
      <circle cx="236" cy="21" r="6" fill="none" stroke="#8C8C8C" stroke-width="1.5"></circle>
      <line x1="240" y1="25" x2="244" y2="29" stroke="#8C8C8C" stroke-width="1.5" stroke-linecap="round"></line>
      <text x="256" y="25" font-size="12" fill="#999">搜索音乐、视频、播客...</text>


      <rect x="440" y="5" width="200" height="32" rx="16" ry="16" fill="#F3F3F4"></rect>
      <text x="452" y="25" font-size="12" fill="#333">已输入的查询</text>
      <line x1="618" y1="15" x2="628" y2="25" stroke="#8C8C8C" stroke-width="1.5" stroke-linecap="round"></line>
      <line x1="628" y1="15" x2="618" y2="25" stroke="#8C8C8C" stroke-width="1.5" stroke-linecap="round"></line>
    </svg>

*EDIT 内联图标三种典型用法。中 / 右两图都是同一个 DuiEditHost 实例，分别用 `SetIcon(LeftIcon, ...)` 和 `SetIcon(RightIcon, ...)` 配置。*

##### API

|   |   |
| --- | --- |
| `SetIcon(slot, gutterW, painter)` | 核心 API。`slot` = `LeftIcon` / `RightIcon`；`gutterW` 是图标占的 px 宽（0 = 移除）；`painter` 是 `std::function<void(HDC, const RECT&)>`，caller 在里面用任意 GDI / GDI+ API 把图标画到指定 RECT 内。 |
| `SetIconBitmap(slot, gutterW, hbm)` | 简便重载：用 HBITMAP 当图标。caller 持有 HBITMAP 所有权。内部 painter 在 RECT 内居中 BitBlt（不缩放）。 |
| `SetIconGlyph(slot, gutterW, glyph, color)` | 简便重载：用 Unicode 字符当图标，用默认字体（Microsoft YaHei 9pt）居中 DrawText。常用 `_T("🔍")` / `_T("@")`。 |
| `ClearIcon(slot)` | 等价于 `SetIcon(slot, 0, nullptr)` —— gutter 收回，文字区扩回。 |
| `SetIconClickable(slot, b)` | 默认 `false`（图标是装饰，鼠标穿透到 EDIT 设 caret）。`true` 时 gutter 内点击被本控件消费，发 `DUIEN_LEFT_ICON_CLICK` / `DUIEN_RIGHT_ICON_CLICK` 通知给父 host。 |
| `SetBgColor(c)` | 同时控制 (1) DUI 侧 OnPaint 在 EDIT 周围 margin 区填的底色；(2) Win32 EDIT 控件本身的 bg（通过 WM_CTLCOLOREDIT propagate 到 `HwndHostControl::SetCtlBgColor`）。默认 `RGB(255,255,255)`。 |
| `SetShowBorder(b)` | 是否画 1px 边框。默认 `true`。把 EDIT 嵌进自带圆角的容器（如 pill 形 SearchBox）时关掉，避免 EDIT 的方框边压在容器圆角上。 |

##### 事件

| code | 触发 | extra |
| --- | --- | --- |
| `DUIEN_LEFT_ICON_CLICK` <small>(= `DUIN_CUSTOM + 1`)</small> | 左侧 `SetIconClickable(LeftIcon, true)` 后，gutter 内左键点击 | 0 |
| `DUIEN_RIGHT_ICON_CLICK` <small>(= `DUIN_CUSTOM + 2`)</small> | 右侧同理 | 0 |

**与小眼睛按钮（`SetShowEyeToggle`）的关系**：右侧图标和密码小眼睛按钮共用右 gutter。当 `password=true` 且 `SetShowEyeToggle(true)` 时，`SetIcon(RightIcon, ...)` 在右侧的绘制 / hit-test 被忽略 —— 小眼睛优先。左侧图标不受影响，可与小眼睛共存。

##### 用法：搜索 pill

```
// 不需要继承 DuiEditHost，普通构造即可：
auto edit = std::make_unique<balloonwjui::DuiEditHost>();
edit->SetPlaceholder(_T("搜索音乐、视频、播客..."));
edit->SetBgColor(RGB(0xF3, 0xF3, 0xF4));   // 与外层 pill 灰底一致
edit->SetShowBorder(false);                // 关掉 1px 边框
edit->SetIcon(balloonwjui::DuiEditHost::LeftIcon, 32,
    [](HDC hdc, const RECT& rc) {
        int gx = (rc.left + rc.right) / 2;
        int gy = (rc.top + rc.bottom) / 2;
        const COLORREF c = RGB(140, 140, 140);
        const int r = 6;
        RECT circle = { gx - r, gy - r - 1, gx + r, gy + r - 1 };
        balloonwjui::DuiAA::FillEllipse(hdc, circle, CLR_INVALID, c, 1.5f);
        balloonwjui::DuiAA::DrawLine(hdc, gx + 3, gy + 3, gx + 7, gy + 7,
                                      c, 1.5f);
    });
// 外层容器自己 paint 圆角 pill 灰底（参见 PaintHelpers::FillRoundedRect）
```

##### 用法：可点击 × 清除

```
edit->SetIconGlyph(balloonwjui::DuiEditHost::RightIcon, 24,
                    _T("✕"), RGB(140, 140, 140));
edit->SetIconClickable(balloonwjui::DuiEditHost::RightIcon, true);

// 父对话框 OnDuiNotify 路由：
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_SEARCH
    && n->code == balloonwjui::DuiEditHost::DUIEN_RIGHT_ICON_CLICK)
{
    edit->SetText(_T(""));    // 清空
}
```

<a id="DuiRichEditHost"></a>

### DuiRichEditHost  `[HWND-hosted]`

![DuiRichEditHost 边框示意](images/ctl-richedit-styles.png)

*RichEdit 框体（带边框）。富文本运行时显示效果请参考 RTF 演示页。*

富文本编辑（基于 `RICHEDIT_CLASS`）。支持：

- RTF 序列化：`SaveRTF/LoadRTF` + 纯文本 `SaveText/LoadText`
- 粘贴过滤：`SetPasteAsPlainTextDefault(true)` 或 `PasteAsPlainText()`
- 查找：`FindText / FindAndSelect`（支持向前/向后、大小写敏感、整词、wrap-around）
- 插入图片 OLE 对象（配合 `DuiImageOle`）

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。<u>挂入后还须在 host HWND 就绪时调一次</u> `re->EnsureCreated(host.m_hWnd)`。`multi-line` / `word-wrap` / `password` 必须在 EnsureCreated 之前设（baked-in 风格），切换需销毁重建 HWND。

#### 代码创建

```
auto re = std::make_unique<balloonwjui::DuiRichEditHost>();
re->SetMultiLine(true);
re->SetPasteAsPlainTextDefault(true);   // Ctrl+V 自动只粘文本
re->LoadText(_T("默认内容..."));
balloonwjui::DuiRichEditHost* reRaw = re.get();
vbox->AddChild(std::move(re), balloonwjui::DuiLayout::Hint().Weight(1));

// host.Create(...) 之后调一次：
reRaw->EnsureCreated(host.m_hWnd);

// 查找
CHARRANGE r;
if (reRaw->FindAndSelect(_T("foo"), 0, /*forward*/true,
                          /*caseSensitive*/false, /*wholeWord*/false,
                          /*wrap*/true)) { /* 选中了 */ }

// 持久化
CStringA rtf;
reRaw->SaveRTF(rtf);    // 含格式
CString plain;
reRaw->SaveText(plain); // 纯文本
```

#### XML 创建

```
<vbox padding="8">
    <richedit id="300" multi-line="true" word-wrap="true" auto-url-detect="true" weight="1"/>
</vbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
// host HWND 就绪后调一次 EnsureCreated：
if (auto* re = static_cast<balloonwjui::DuiRichEditHost*>(host.GetRoot()->FindControlById(300)))
    re->EnsureCreated(host.m_hWnd);
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | RichEdit 内容变化（每次 `EN_CHANGE`） | 0 |
| `DUIN_SETFOCUS` / `DUIN_KILLFOCUS` | 焦点进出 | 0 |
| `DUIN_RE_LINKCLICK` | 用户点击自动检测到的 URL（需先 `SetAutoUrlDetect(true)`）。`EN_LINK` 自动转发为本通知 | 原始 `EN_LINK` 的 `lParam`（指向 `ENLINK`，可解析出位置 / 文本） |

```
// 父对话框：
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_RE && n->code == DUIN_RE_LINKCLICK) {
    auto* el = (ENLINK*)n->extra;
    CString url = re->GetTextRange(el->chrg);
    ::ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}
```

**EnsureCreated**：同 DuiEditHost，须在 host HWND 就绪后调 `re->EnsureCreated(hostHwnd)`。`multi-line` / `word-wrap` 必须在 EnsureCreated 之前设（baked-in 风格），切换需销毁重建 HWND。

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<richedit id="..." placeholder="..." multi-line="true" word-wrap="true" auto-url-detect="true"/>` |
| 详细属性参考 | [§3.3.13 richedit](#xml-richedit) |
| 事件 | `DUIN_VALUECHANGED` / `DUIN_SETFOCUS` / `DUIN_KILLFOCUS` / `DUIN_RE_LINKCLICK` |
| 说明 | RTF 序列化 / 图片插入 / 引用块 / 查找等高级功能仅 C++ 暴露，XML 不涉及 |

<a id="DuiSearchBox"></a>

### DuiSearchBox  `[HWND-hosted]`

![DuiSearchBox 状态](images/ctl-searchbox-states.png)

*左：空状态显示占位符；右：键入后右端出现 × 清除按钮。*

带搜索图标 + 占位符的紧凑搜索框。键入后发 `DUIN_VALUECHANGED`。

**实现：M7+ 重构为 `DuiEditHost` 的 thin preset** —— DuiSearchBox 现在<u>继承</u> DuiEditHost，ctor 通过 DuiEditHost 的[内联图标 API](#DuiEditHost-IconApi)装好左侧 magnifier；监听 EN_CHANGE 在文字非空时显右侧 ×、空时藏；拦 OnIconClicked_(RightIcon) 自吞 click + SetText("")。重构前是聚合 + 自绘 ~250 行，重构后 ~80 行。<u>对 caller 的 API 表面零变化</u>，`SetGlyphStripWidth` / `SetClearStripWidth` / `IsClearShowing` / `GetClearRect` / `GetEdit` 全保留语义（GetEdit 现在返回 `this`，因为本类<u>就是</u> EDIT）。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。<u>挂入后还须在 host HWND 就绪时调一次</u> `sb->EnsureCreated(host.m_hWnd)`（继承自 DuiEditHost）。常见用法是放在联系人 / 文件列表的顶部。

#### 代码创建

```
auto sb = std::make_unique<balloonwjui::DuiSearchBox>();
sb->SetCtrlId(IDC_SEARCH_CONTACTS);
sb->SetPlaceholder(_T("搜索联系人..."));
sb->SetDebounceMs(200);    // 输入防抖（200ms 内多次 keystroke 合并发一次）
balloonwjui::DuiSearchBox* sbRaw = sb.get();
vbox->AddChild(std::move(sb), balloonwjui::DuiLayout::Hint().Fixed(28));

// host.Create(...) 之后调一次：
sbRaw->EnsureCreated(host.m_hWnd);
```

#### XML 创建

```
<vbox padding="6">
    <searchbox id="400" placeholder="搜索联系人..." max-length="64" fixedHeight="28"/>
</vbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
if (auto* sb = static_cast<balloonwjui::DuiSearchBox*>(host.GetRoot()->FindControlById(400)))
    sb->EnsureCreated(host.m_hWnd);
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | EDIT 内容变化（已经过 `SetDebounceMs` 防抖合并）。点击右侧 × 清除按钮也会触发一次（值变为空串） | 0（用 `GetText()` 拿最新值） |

```
// 父对话框 OnDuiNotify：增量过滤联系人列表
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_SEARCH_CONTACTS && n->code == DUIN_VALUECHANGED) {
    CString q = m_searchBox->GetText();
    m_friendList->ApplyFilter(q);   // 业务方法：按文本过滤列表项
}
```

**EnsureCreated**：内嵌一个 DuiEditHost，须在 host HWND 就绪后调 `sb->EnsureCreated(hostHwnd)`。

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<searchbox id="..." placeholder="..." max-length="64"/>` |
| 详细属性参考 | [§3.3.14 searchbox](#xml-searchbox) |
| 事件 | `DUIN_VALUECHANGED` — 文字变化（含 × 清空）；ctrlId 是 searchbox 的 id（已自动从内部 EDIT 重打） |

<a id="DuiSpinBox"></a>

### DuiSpinBox  `[HWND-hosted]`

![DuiSpinBox 默认形态](images/ctl-spinbox-default.png)

*左侧 EDIT + 右侧 ↑↓ 增减按钮。*

数字输入框 + 上下增减小按钮。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。<u>挂入后还须在 host HWND 就绪时调一次</u> `spin->EnsureCreated(host.m_hWnd)`（内嵌一个 DuiEditHost）。

#### 代码创建

```
auto spin = std::make_unique<balloonwjui::DuiSpinBox>();
spin->SetCtrlId(IDC_FONT_SIZE);
spin->SetRange(0, 100);
spin->SetValue(50);
spin->SetStep(5);
balloonwjui::DuiSpinBox* spinRaw = spin.get();
vbox->AddChild(std::move(spin), balloonwjui::DuiLayout::Hint().Fixed(28));

// host.Create(...) 之后调一次：
spinRaw->EnsureCreated(host.m_hWnd);
```

#### XML 创建

```
<vbox padding="8">
    <spinbox id="500" min="0" max="999" value="100" step="5" fixedHeight="28"/>
</vbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
if (auto* sp = static_cast<balloonwjui::DuiSpinBox*>(host.GetRoot()->FindControlById(500)))
    sp->EnsureCreated(host.m_hWnd);
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 用户点 ↑↓ / 在 EDIT 里键入合法数字 / 滚轮滚动；`SetValue(_, true)` 也触发一次（程序写入），`SetValue(_, false)` 不触发 | 新数值（`(int)n->extra`） |

```
// 父对话框 OnDuiNotify：调整字号
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_FONT_SIZE && n->code == DUIN_VALUECHANGED) {
    int newSize = (int)n->extra;
    m_chatList->SetFontSize(newSize);
}
```

**EnsureCreated**：内嵌一个 DuiEditHost，须在 host HWND 就绪后调 `sp->EnsureCreated(hostHwnd)`。<u>用户在 EDIT 里手输入</u>不会自动同步到 m_value —— caller 需在 EDIT 的 `DUIN_KILLFOCUS` 里调 `SetValue(_ttoi(GetEdit()->GetText()))` 提交。

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<spinbox id="..." min="0" max="999" value="100" step="5"/>` |
| 详细属性参考 | [§3.3.15 spinbox](#xml-spinbox) |
| 事件 | `DUIN_VALUECHANGED` — ▲▼ 点击触发；extra = 新值。手输入提交需 caller 自己处理 |

<a id="DuiSlider"></a>

### DuiSlider  `[DUI]`

![水平 DuiSlider 不同进度](images/ctl-slider-horizontal.png)

![垂直 DuiSlider](images/ctl-slider-vertical.png)

*上：水平滑块 0% / 50% / 100% / disabled；下：垂直方向 3 个进度。*

横向 / 纵向滑动条。拖动 thumb 实时发 `DUIN_VALUECHANGED`。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。

#### 代码创建

```
auto sl = std::make_unique<balloonwjui::DuiSlider>();
sl->SetCtrlId(IDC_VOLUME);
sl->SetOrientation(balloonwjui::DuiSlider::Horizontal);
sl->SetRange(0, 100);
sl->SetValue(60);
sl->SetTickInterval(10);   // 刻度
hbox->AddChild(std::move(sl), balloonwjui::DuiLayout::Hint().Weight(1));
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 拖动 thumb（实时连续触发） / 点击 track / ←→↑↓ / Home/End / PageUp/Down / 滚轮；`SetPos(_, true)` 程序写入也触发 | 新位置 `(int)n->extra` |

```
// 父对话框 OnDuiNotify：音量条
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_VOLUME && n->code == DUIN_VALUECHANGED) {
    int v = (int)n->extra;          // 0..100
    m_audio.SetVolume(v / 100.0f);
    m_lblVolume->SetText(CString().Format(_T("%d%%"), v));
}
```

#### 关键 API

|   |   |
| --- | --- |
| `SetRange(min, max) / SetPos(v, notify)` | 取值范围 + 当前值；自动 clamp |
| `SetLineSize(int)` | 键盘 / 滚轮 / 轨道点击的步长（默认 1） |
| `SetVertical(bool)` | 竖向 = 上→下值递增（与 Win32 ScrollBar 一致） |
| `SetTrackHeight / SetThumbSize` | 轨道粗细 / thumb 直径 |
| `SetTrackColor / SetFillColor / SetThumbColor` | 颜色覆盖（默认浅灰 / 品牌蓝 / 白） |
| `SetTickFrequency(int)` | 每 n 个值-单位一根刻度；0 = 不画 |

#### XML 创建

```
<hbox padding="8" gap="8" fixedHeight="32">
    <label  text="音量" fixedWidth="60"/>
    <slider id="600" min="0" max="100" value="40" line-size="5" tick-frequency="10" weight="1"/>
</hbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<slider id="..." min="0" max="100" value="40" line-size="5" tick-frequency="10"/>` |
| 详细属性参考 | [§3.3.10 slider](#xml-slider) |
| 事件 | `DUIN_VALUECHANGED` — 拖动 / 点轨道 / 滚轮 / 键盘 / SetPos(_, true) 触发；extra = 新值（int） |

<a id="DuiSwitch"></a>

### DuiSwitch  `[DUI]`

![DuiSwitch 四态：off / on / disabled-off / disabled-on](images/ctl-switch-states.png)

*从左到右：off（浅灰底）/ on（某信绿底）/ disabled-off / disabled-on。*

iOS 风格圆角胶囊开关，单个布尔状态的两态切换器，比 Checkbox 更"开关感"，色彩对比明确。切换走 150ms ease-out-cubic 动画 —— 滑块从一侧滑到另一侧的同时，底色从 off 色 fade 到 on 色。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。常出现在设置页的"通知开关 / 自动下载 / 保留聊天记录"行尾、群聊侧栏的"消息免打扰"等位置。

#### 代码创建

```
auto sw = std::make_unique<balloonwjui::DuiSwitch>();
sw->SetCtrlId(IDC_MUTE_NOTIF);
sw->SetChecked(true, /*animated=*/false, /*notify=*/false);   // 初始化不动画 / 不通知
hbox->AddChild(std::move(sw), balloonwjui::DuiLayout::Hint().Fixed(46));
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 鼠标点击 / Space / Enter 翻转状态时；<u>不</u>包括 `SetChecked()` 编程调用（与 DuiButton checkbox 一致） | 1（on）/ 0（off） |

```
// 父对话框 OnDuiNotify：消息免打扰
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_MUTE_NOTIF && n->code == DUIN_VALUECHANGED) {
    bool on = (n->extra != 0);
    m_settings.SetMute(on);
}
```

#### 关键 API

|   |   |
| --- | --- |
| `SetChecked(bool, bool animated=true, bool notify=false)` | 设勾选态；`animated` 显式参数永远赢（不受 SetAnimated 全局开关影响），方便"现在就瞬间设值"的初始化路径 |
| `IsChecked() / GetProgress()` | 当前状态 + 当前 0..1 动画进度（off → on 时从 0 爬到 1） |
| `SetAnimated(bool)` | 鼠标 / 键盘翻转的默认动画开关，默认 true。关闭后翻转瞬间跳变，省 CPU |
| `SetOnColor / SetOffColor / SetKnobColor` | 颜色覆盖（默认 某信绿 RGB(7,193,96) / 浅灰 RGB(229,229,229) / 白） |

#### 动画驱动

DuiSwitch 通过 `balloonwjui::DuiAnimMgr` + `DuiDoubleAnim` 驱动滑块动画。host 必须周期性调 `DuiAnimMgr::Inst().TickAll(::GetTickCount())` 才能看到中间帧 —— 典型做法是顶层 frame 在 `OnCreate` 里 `SetTimer(id, 16)`、`OnTimer` 里调 `TickAll`，`OnDestroy` 里 `KillTimer` + `DuiAnimMgr::Inst().Clear()`。`DuiGallery` 的 `GalleryFrame` 已经接通这个 pulse 作为参考实现。

没接通 pulse 的业务对话框：`SetChecked` 仍然能立即把状态设到端点（`animated=false` 路径），只是没有动画过渡。

#### XML 创建

```
<hbox padding="8" gap="8" fixedHeight="32">
    <label  text="消息免打扰" weight="1"/>
    <switch id="700" checked="false" fixedWidth="46" fixedHeight="24"/>
</hbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<switch id="..." checked="false" animated="true" on-color="7,193,96"/>` |
| 详细属性参考 | [§3.3.11 switch](#xml-switch) |
| 事件 | `DUIN_VALUECHANGED` — 鼠标 / Space / Enter 翻转触发；extra = 1（on） / 0（off）；`SetChecked()` 不触发 |

<a id="DuiComboBox"></a>

### DuiComboBox  `[DUI]`

![DuiComboBox 收起态](images/ctl-combobox-collapsed.png)

*收起态显示当前选中项 + 右侧三角。*

下拉选择框。两种风格：只读（点击弹下拉列表）/ 可输入（同时键入过滤 — 需 `SetIncrementalSearch(true)`）。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。可输入风格内含 EDIT 子，挂入后须在 host HWND 就绪时调一次 `cb->EnsureCreated(host.m_hWnd)`（仅可输入模式必须，只读模式可省）。

**下拉箭头：**右侧三角形走 `DuiAA::FillPolygon` 抗锯齿绘制（对角斜边在屏上不会出现阶梯像素）。颜色可通过 `SetArrowColor(COLORREF)` 配置，默认 `RGB(80,100,140)` 蓝灰色；仅覆盖 enabled 态，disabled 态沿用内部 `kArrowDisabled = RGB(160,160,160)` 不变（业务一般不需要单独换 disabled 色）。

#### 代码创建

```
auto cb = std::make_unique<balloonwjui::DuiComboBox>();
cb->SetCtrlId(IDC_FRUIT);
cb->AddItem(_T("Apple"));
cb->AddItem(_T("Banana"));
cb->AddItem(_T("Cherry"));
cb->SetEditable(true);
cb->SetIncrementalSearch(true);
cb->SetIncrementalSearchSubstring(true);   // 子串匹配（默认 false 仅前缀）
cb->SetIncrementalSearchCaseSensitive(false);
balloonwjui::DuiComboBox* cbRaw = cb.get();
hbox->AddChild(std::move(cb), balloonwjui::DuiLayout::Hint().Fixed(160));

// 可输入模式：host.Create(...) 之后调一次：
cbRaw->EnsureCreated(host.m_hWnd);
```

|   |   |
| --- | --- |
| `AddItem / RemoveAt / Clear` | 项目操作 |
| `GetCount / GetItemText(idx)` | 查询 |
| `SetCurSel(idx) / GetCurSel()` | 当前选中 |
| `SetEditable(bool)` | 是否允许输入 |
| `SetBgColor(COLORREF)` / `SetShowBorder(bool)` / `SetShowArrow(bool)` | 主体底色 / 边框 / 右侧箭头开关 |
| `SetArrowColor(COLORREF)` / `GetArrowColor()` | 下拉箭头颜色（默认 RGB(80,100,140)）。仅覆盖 enabled 态，disabled 态沿用内部灰色不变。三角形走 `DuiAA::FillPolygon` AA 绘制 |
| `SetIncrementalSearch(bool)` | 键入过滤下拉项 |
| `ComputeFilteredIndices(query)` | 纯函数：返回过滤后真实索引列表 |

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 用户从下拉列表选了某项；`SetCurSel(_, true)` 程序选择也触发，`SetCurSel(_, false)` 不触发 | 新选中索引 `(int)n->extra` |

可输入模式（`SetEditable(true)`）下，键入过滤是内部对下拉项的隐藏 / 显示，<u>不会</u>额外发事件 — 只有最终选中某项才发 `DUIN_VALUECHANGED`。需要监听键入过程，请改用 `DuiSearchBox`。

```
// 父对话框 OnDuiNotify：语言切换
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_LANGUAGE && n->code == DUIN_VALUECHANGED) {
    int idx = (int)n->extra;
    static const LPCTSTR codes[] = { _T("en"), _T("zh-CN"), _T("ja") };
    SetUiLocale(codes[idx]);
}
```

XML 暂未原生支持 `<combobox>`（项目列表 + editable 模式涉及内嵌 EDIT 节点）；若需 XML 描述请走 `DuiXmlBuilder::CustomFactory`。

## 7.4 列表与导航

<a id="DuiListBox"></a>

### DuiListBox  `[DUI]`

![DuiListBox 单选](images/ctl-listbox-single.png)

![DuiListBox 多选](images/ctl-listbox-multi.png)

*左：单选模式（"Drafts" 选中）；右：多选模式（"Apple" 与 "Cherry" 选中）。*

简单滚动列表。支持单选 / 多选、自定义行高、可选 checkbox / 拖动重排。<u>所有 item 都存在控件内部（`std::vector<Item>`）</u>—— 这意味着 N 行就有 N 份字符串副本，N 万行起就该考虑下面的 [DuiVirtualList](#DuiVirtualList)。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。<u>长列表通常套一层 `DuiScrollView`</u> 让超出可视区时滚动 —— DuiListBox 自身不内置滚动条。

**什么时候不用 DuiListBox 而用 DuiVirtualList：**

      行数 ≥ 1 万 — 维护 vector 的内存 + 插入 / 删除的 O(N) 位移开始有感。
      行内容来自外部数据源（数据库 / 业务 model），不想在 UI 控件里再复制一份。
      每行需要复杂自定义绘制（聊天气泡 / 多行卡片），`SetItemText` 这种纯字符串接口表达不了。

#### 代码创建

```
auto lb = std::make_unique<balloonwjui::DuiListBox>();
lb->SetCtrlId(IDC_SESSION_LIST);
for (auto& s : items) lb->AddItem(s);
lb->SetSelectMode(balloonwjui::DuiListBox::Multi);
lb->SetItemHeight(28);
lb->SetCurSel(0);

// 套 DuiScrollView 让超长列表能滚：
auto sv = std::make_unique<balloonwjui::DuiScrollView>();
balloonwjui::DuiListBox* lbRaw = lb.get();
sv->SetContent(std::move(lb));
sv->SetContentHeight(lbRaw->GetCount() * lbRaw->GetItemHeight());
vbox->AddChild(std::move(sv), balloonwjui::DuiLayout::Hint().Weight(1));
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 当前选中变化（单选 / 多选模式都会发） | 新选中索引（多选模式下为最近一次操作的索引） |
| `DUIN_DBLCLK` | 双击某项（典型用作"打开会话"等回车等价行为） | 双击的项索引 |
| `DuiListBox::DUITN_CHECKED` | 多选 + checkbox 模式（`SetShowCheckboxes(true)`）下，用户翻转某项 checkbox | 项索引；调 `IsItemChecked(idx)` 拿新状态 |
| `DuiListBox::DUITN_REORDERED` | 启用 `SetDragReorderEnabled(true)` 后，拖动重排完成 | 新索引（被拖动项移动到的位置） |

```
// 父对话框 OnDuiNotify：会话列表
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_SESSION_LIST) {
    int idx = (int)n->extra;
    switch (n->code) {
    case DUIN_VALUECHANGED:
        // 选中切换 - 加载对应会话的聊天记录
        OpenSession(m_sessionList->GetItemParam(idx));
        break;
    case DUIN_DBLCLK:
        // 双击 - 弹会话详情
        ShowSessionDetail(idx);
        break;
    case balloonwjui::DuiListBox::DUITN_CHECKED:
        // 多选 checkbox 翻转 - 用于"批量标记已读"
        UpdateBatchSelection();
        break;
    }
}
```

<a id="DuiVirtualList"></a>

### DuiVirtualList  `[DUI]`

真正意义上的虚拟列表：<u>不持有任何行数据</u>，由 caller 给一个"行数 + 每行怎么画"的回调，控件按需调。鼠标 / 键盘 / 选中 / 滚动行为跟 [DuiListBox](#DuiListBox) 完全一样，但内存占用 = O(可见行 + 控件本身的固定开销)，跟数据集大小<u>无关</u>。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。<u>自管滚动条</u>（内置 `DuiScrollBar`），不需要也不该再套 `DuiScrollView`。

#### 用途

- **聊天记录 / 消息历史**：单会话 10 万条消息也常见，每条都要自己的气泡 / 头像 / 时间戳布局。DuiListBox 的纯文本接口表达不了，复制 10 万份 `CString` 也不划算。
- **大数据量后台列表**：日志查看器、监控指标列表、最近会话列表。数据本来就在业务的 `std::vector` / `std::deque` 里，没必要再丢一份给控件。
- **每行需要复杂自绘**：行不止有文字 — 还有头像、徽章、状态点、未读数等。DuiVirtualList 的 paint 回调把整行 HDC 让给 caller，想画啥都行。

#### 实现原理

三句话讲完：

1. **不存数据**。控件内只存：行数 `m_rowCount`、行高 `m_rowH`、当前选中 `m_curSel`、hover 行 `m_hoverIdx`、滚动条。零行内容缓存。
2. **每帧只画视口可见行**。`OnPaint` 里算： `int firstVisible = scrollPos / m_rowH; int lastVisible = firstVisible + RowsVisible() + 1; // +1 给最后半行 for (int i = firstVisible; i < lastVisible; ++i) { RECT rcRow = { ..., top - scrollPos + i * m_rowH, ... }; m_paint(m_paintUser, hdc, i, rcRow, isSelected, isHover); // 回调 } ` 整个视口 ~30 行，每帧调 30 次回调，跟 `m_rowCount` 是 1 万还是 1 千万<u>毫无关系</u>。
3. **选中 / hover 状态由控件管，行内容由 caller 管**。回调签名 `(hdc, rowIndex, rowRect, selected, hover)` —— caller 收到这 5 个参数后，按 rowIndex 去自家数据源拿第 i 条记录，按 selected/hover 决定背景色 / 高亮，然后把内容画到 rowRect 范围内。

#### 跟 DuiListBox 的对比

|  | DuiListBox | DuiVirtualList |
| --- | --- | --- |
| 数据持有 | `std::vector<Item>` 控件内 | <u>不持有</u>，caller 提供回调 |
| 典型上限 | ~5 千行（再多内存 / 插入开销有感） | ~千万行（瓶颈在滚动条精度，不在控件） |
| 每行内容接口 | 纯文本 + LPARAM 业务 ID + 可选 checkbox | caller 自绘整行（HDC + RECT），想画啥画啥 |
| 插入 / 删除一行 | O(N) 拷贝（vector 中间删 / 插） | caller 自管业务数据；控件只须 `SetRowCount(n)` |
| 滚动条 | 需要外层套 `DuiScrollView` | 内置自管 |
| 多选 | 支持（`SetSelectMode(Multi)` + 可选 checkbox） | <u>仅单选</u>（`m_curSel`，没有多选 API） |
| 拖动重排 | 支持（`SetDragReorderEnabled`） | 不支持（控件不知道行的真实身份，没法在 UI 层重排） |

选择路径：<u>≤ 几千行的简单文本列表</u> → DuiListBox；<u>万行起 / 行需要自绘 / 数据本来就在 model 里</u> → DuiVirtualList。

#### API 速览

| 调用 | 含义 |
| --- | --- |
| `SetRowCount(n)` | 声明数据集行数；变化时刷新滚动范围 |
| `SetRowHeight(h)` | 单行像素高度（默认 28） |
| `SetPaintRowCallback(fn, user)` | **必填**。每行调一次的绘制回调 |
| `SetRowClickCallback(fn, user)` | 可选。单击 / 双击触发；`isDoubleClick` 区分 |
| `SetCurSel(idx, notify=true)` | 程序设置选中行；通常 caller 不必直接调，控件会响应鼠标 / 键盘 |
| `EnsureVisible(idx)` | 滚动到让 idx 行可见（用于"跳到最新消息"等） |
| `GetCurSel() / GetRowCount() / GetScrollPos()` | 状态查询 |

#### 代码示例 1：聊天历史（典型场景）

10 万条聊天消息存在业务的 `std::deque<ChatMsg>` 里。每条消息要画头像 + 气泡 + 时间戳。

```
struct ChatMsg
{
    CString  senderName;
    CString  content;
    HBITMAP  avatarBmp;
    SYSTEMTIME timestamp;
    bool     isOutgoing;     // 自己发的画右边，对方发的画左边
};

class ChatHistoryView
{
public:
    void Build(balloonwjui::DuiVBox* parent, std::deque<ChatMsg>* model)
    {
        m_model = model;

        auto vlist = std::unique_ptr<balloonwjui::DuiVirtualList>(new balloonwjui::DuiVirtualList());
        vlist->SetCtrlId(IDC_CHAT_HISTORY);
        vlist->SetRowHeight(64);                           // 头像 48 + 上下 padding
        vlist->SetRowCount((int)model->size());

        // user 指针指回业务 model；回调里强转回来取数据
        vlist->SetPaintRowCallback(&ChatHistoryView::PaintRow, this);
        vlist->SetRowClickCallback(&ChatHistoryView::OnRowClick, this);

        m_vlist = vlist.get();
        parent->AddChild(std::move(vlist), balloonwjui::DuiLayout::Hint().Weight(1));

        // 默认滚到最底（最新消息）
        if (!model->empty())
        {
            m_vlist->EnsureVisible((int)model->size() - 1);
        }
    }

    // 业务追加新消息：只需更新 model + 刷新行数 + 滚到底
    void OnNewMessage(const ChatMsg& msg)
    {
        m_model->push_back(msg);
        m_vlist->SetRowCount((int)m_model->size());
        m_vlist->EnsureVisible((int)m_model->size() - 1);
    }

private:
    // ---- 静态 trampoline，转发到成员函数 ----
    static void PaintRow(void* user, HDC hdc, int rowIndex,
                         const RECT& rcRow, bool selected, bool hover)
    {
        static_cast<ChatHistoryView*>(user)->PaintMsg(hdc, rowIndex, rcRow, selected, hover);
    }
    static void OnRowClick(void* user, int rowIndex, bool isDoubleClick)
    {
        static_cast<ChatHistoryView*>(user)->HandleClick(rowIndex, isDoubleClick);
    }

    void PaintMsg(HDC hdc, int rowIndex, const RECT& rcRow,
                  bool selected, bool hover)
    {
        if (rowIndex < 0 || rowIndex >= (int)m_model->size()) { return; }
        const ChatMsg& m = (*m_model)[rowIndex];

        // 1. 行背景（控件不画，由 caller 决定）
        COLORREF bg = RGB(255, 255, 255);
        if (selected)   { bg = RGB(180, 210, 245); }
        else if (hover) { bg = RGB(232, 240, 252); }
        ::SetBkColor(hdc, bg);
        ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rcRow, _T(""), 0, nullptr);

        // 2. 头像
        RECT rcAvatar = { rcRow.left + 8, rcRow.top + 8,
                          rcRow.left + 8 + 48, rcRow.top + 8 + 48 };
        DrawBitmapInto(hdc, rcAvatar, m.avatarBmp);

        // 3. 气泡 + 文字
        RECT rcBubble = { rcAvatar.right + 8, rcRow.top + 8,
                          rcRow.right - 8, rcRow.bottom - 8 };
        DrawBubble(hdc, rcBubble, m.content, m.isOutgoing);

        // 4. 时间戳（右上角）
        TCHAR tsBuf[32];
        FormatTime(m.timestamp, tsBuf);
        DrawTimestamp(hdc, rcBubble, tsBuf);
    }

    void HandleClick(int rowIndex, bool isDoubleClick)
    {
        if (isDoubleClick)
        {
            ShowMessageDetail((*m_model)[rowIndex]);
        }
    }

    balloonwjui::DuiVirtualList* m_vlist = nullptr;
    std::deque<ChatMsg>*         m_model = nullptr;
};
```

**关键点**：

- **append 一条消息**不需要 `InsertItem`—— 业务 `push_back` 后调 `SetRowCount(n)` 就够了。控件下次 OnPaint 会自然画出新行。
- **静态 trampoline**是因为回调签名是 C 函数指针（不是 `std::function`，避免 DLL 边界 ABI 问题）。`user` 参数就是绕 C 函数指针不能捕获 `this` 的标准做法。
- **行背景 caller 自画**：`selected`/`hover` 状态控件给你，但具体颜色 / 选中样式控件不强加。如果要做"自己发的消息高亮金色 + 选中变橙"这种业务样式，自由度都在你手里。

#### 代码示例 2：百万行只读日志查看器

极端场景。日志文件 mmap 进内存，每行偏移记在一个数组里。

```
class LogViewer
{
public:
    void Open(LPCTSTR path, balloonwjui::DuiVBox* parent)
    {
        m_lines = MmapAndIndexLines(path);   // 业务侧：返回 vector<LineRef>{offset, len}

        auto v = std::unique_ptr<balloonwjui::DuiVirtualList>(new balloonwjui::DuiVirtualList());
        v->SetRowHeight(20);
        v->SetRowCount((int)m_lines.size());     // 哪怕 1000 万行，这里就是设个数
        v->SetPaintRowCallback(&LogViewer::Paint, this);
        m_v = v.get();
        parent->AddChild(std::move(v), balloonwjui::DuiLayout::Hint().Weight(1));
    }

    void JumpToLine(int n) { m_v->EnsureVisible(n); m_v->SetCurSel(n); }

private:
    static void Paint(void* user, HDC hdc, int row, const RECT& rc, bool sel, bool hover)
    {
        auto* self = static_cast<LogViewer*>(user);
        if (row < 0 || row >= (int)self->m_lines.size()) { return; }

        // 行背景
        COLORREF bg = sel ? RGB(255, 235, 130) : (hover ? RGB(245, 245, 245) : RGB(255, 255, 255));
        HBRUSH hbr = ::CreateSolidBrush(bg);
        ::FillRect(hdc, &rc, hbr);
        ::DeleteObject(hbr);

        // 直接从 mmap 视图取这一行的字节（零拷贝），DrawText
        const auto& line = self->m_lines[row];
        ::SetTextColor(hdc, RGB(40, 40, 40));
        ::SetBkMode(hdc, TRANSPARENT);
        RECT rcText = { rc.left + 4, rc.top, rc.right - 4, rc.bottom };
        ::DrawTextA(hdc, self->m_mapped + line.offset, line.len, &rcText,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    balloonwjui::DuiVirtualList* m_v = nullptr;
    const char*                  m_mapped = nullptr;
    std::vector<LineRef>         m_lines;
};
```

这个 viewer 打开 1 GB 日志（约 1500 万行）的内存占用 ≈ 1 GB（mmap 自己）+ `vector<LineRef>` 的 12 字节 × 1500 万 ≈ 180 MB。控件本身只有 ~1 KB 状态。<u>如果换成 DuiListBox</u>，仅 `m_items` 里的 `CString` 就要再吃一份 1 GB 内存（且每条 1 ~ 2 次堆分配），多半直接 OOM。

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 当前选中行变化（鼠标点 / 键盘 Up-Down 都会发；`SetCurSel(_, true)` 程序设置也发） | 新选中行索引；-1 = 清空 |

注意：双击 / 单击在 `SetRowClickCallback` 那条回调里发，<u>不</u>走 `WM_DUI_NOTIFY` —— 因为 click 跟"行"绑得太紧（rowIndex 必须传），用回调更直接。如果业务还是想统一走 notify，自己在回调里 `NotifyParent` 也行。

#### 陷阱 / 注意事项

- **数据 / UI 同步是 caller 的责任**。业务删除第 5 行后必须调 `SetRowCount` 通知控件；否则 `m_curSel` 和回调里的 rowIndex 会跟实际数据错位。建议封装一个"业务侧 Add/Remove → 自动同步"的 helper（参见聊天示例的 `OnNewMessage`）。
- **回调里访问 model 要线程安全**。OnPaint 在 UI 线程，但如果 caller 有别的线程往 model 里写（IM 后台收消息），需要业务侧锁住 model 的读访问 —— 控件本身不知道有锁。
- **不要在回调里搞耗时操作**。每帧 ~30 次回调，单次回调超过 1ms 滚动就会卡。复杂行布局应该提前在业务 model 里预计算好（行高、文字 wrap 位置等），回调里只做 GDI 调用。
- **没有"行 LPARAM"概念**。caller 永远只拿到 `rowIndex`，业务身份要自己用 `m_lines[rowIndex].id` 这种方式映射 —— 这就是"虚拟"的代价。

<a id="DuiTreeView"></a>

### DuiTreeView  `[DUI]`

![DuiTreeView 展开/折叠](images/ctl-treeview-states.png)

*"Friends" 展开（显示子节点）；"Groups" 折叠。*

带展开 / 折叠的层级列表。两种模式：<u>单列</u>（默认）= 经典树视图；<u>多列</u>（调过 `AddColumn` 后）= 树形 + 表格混合，含 header 拖拽 / 排序 / 冻结面板 / 6 种 cell 类型 / 单元格级选中 / Text in-place 编辑。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。<u>单列模式</u>需要外层 `DuiScrollView` 才能在长内容下滚动；<u>多列模式</u>自管水平 + 垂直滚动条，直接挂 layout 容器即可。

#### 单列模式

不调 `AddColumn`。每行 `[▶ glyph][indent][icon][label][...][status dot]`，无 header，无水平滚动，靠外层 `DuiScrollView` 滚。

```
auto tv = std::make_unique<balloonwjui::DuiTreeView>();
int gFriends = tv->AddRoot(_T("Friends"));
int idAlice  = tv->AddChild(gFriends, _T("Alice"));
tv->SetItemStatusColor(idAlice, RGB(64, 200, 96));    // 在线绿
tv->Expand(gFriends);
tv->SetCurSel(idAlice);

// 套 DuiScrollView 让超长树能滚：
auto sv = std::make_unique<balloonwjui::DuiScrollView>();
balloonwjui::DuiTreeView* tvRaw = tv.get();
sv->SetContent(std::move(tv));
sv->SetContentHeight(tvRaw->GetContentHeight());
vbox->AddChild(std::move(sv), balloonwjui::DuiLayout::Hint().Weight(1));
```

#### 多层级嵌套（任意深度）

`AddRoot/AddChild` 支持<u>任意层级嵌套</u> —— 每次 `AddChild(parentId, ...)` 都可以挂在任何已存在节点下（不限根节点）。`Node` 结构内部用 `parentIdx` + `depth` 字段记录关系，PaintRow 按 `depth * m_indent`（默认 18 px）算缩进；折叠任意中间层时 `RebuildVisible` 用 `hideUntilDepth` 整体跳过子树（不进入可见行列表）。

```
auto tv = std::make_unique<balloonwjui::DuiTreeView>();

// Depth 0
int gCompany = tv->AddRoot(_T("CloudCorp"));

// Depth 1
int gRD  = tv->AddChild(gCompany, _T("R&D Center"));
int gOps = tv->AddChild(gCompany, _T("Operations Center"));

// Depth 2
int gFE  = tv->AddChild(gRD, _T("Frontend"));
int gBE  = tv->AddChild(gRD, _T("Backend"));

// Depth 3
int gWeb    = tv->AddChild(gFE, _T("Web Team"));
int gMobile = tv->AddChild(gFE, _T("Mobile Team"));

// Depth 4（叶节点）
tv->AddChild(gWeb,    _T("Project Aurora"));
tv->AddChild(gMobile, _T("Project Comet"));
// ... 也可以继续往下挂 depth 5+, 没有上限
```

DuiGallery 演示：TreeView tab → **"Multi-level nesting (arbitrary depth)"** section（5 层深的组织架构 + Expand all / Collapse all 按钮）。

#### 节点 icon

每个节点可挂一份独立的 HBITMAP 作 icon，绘制在 ▶ glyph 与 label 之间的 18×18 槽位（`kIconSizePx`）。位图所有权由 caller 持有（控件只存裸指针），相同位图可被多个节点共享。

```
// 头像 / 文件夹 / 文件 等小图，建议提前生成或加载，控件不复制位图字节。
HBITMAP hDir  = LoadProjectIconBitmap(IconKind::Folder);
HBITMAP hDoc  = LoadProjectIconBitmap(IconKind::Document);

int gProj = tv->AddRoot(_T("project-alpha"), hDir);
tv->AddChild(gProj, _T("README.md"),  hDoc);
tv->AddChild(gProj, _T("design.pdf"), hDoc);
tv->AddChild(gProj, _T("(no icon, label only)"));    // icon 参数省略 = 无 icon
```

也支持运行期改：`SetItemIcon(id, hbm)`、`GetItemIcon(id)`。

进一步视觉变体：`SetItemIconGrayed(id, true)` 把 icon 按 NTSC luma `(R*299 + G*587 + B*114) / 1000` 转灰输出，原 HBITMAP 不动 —— 典型用例是"离线好友 / 已禁用项"的视觉表达。

DuiGallery 演示：TreeView tab → **"Per-node icons (HBITMAP via SetItemIcon)"** section（两个根节点 + 多个有 / 无 icon 子节点混排）。

#### 多列模式

![DuiTreeView 多列模式 — 5 列 + 冻结 + 6 种 cell](images/ctl-treeview-multicol.png)

*5 列布局：Name（tree 列，frozen-cols=1，初始按升序排序，▲ 三角）/ Done（CheckBox cell）/ Progress（ProgressBar cell + 百分比）/ Size（右对齐 Text）/ Link（Hyperlink，蓝色下划线）。两个根节点 project-alpha / project-beta 各自展开子节点。*

调 `AddColumn` 后切换到多列模式：顶部固定 header（拖列宽 / 单击排序 / 双击 auto-fit / 右键 `DUITVN_HEADER_RCLICK`），下方 4 象限布局支持 `SetFrozenColumns/SetFrozenRows`（Excel 风格）。控件自管水平 + 垂直<u>两条</u>滚动条 —— 即使外面没套 DuiScrollView 也能滚。

**Cell 类型：**

| 类型 | 调用 | 显示 | 默认交互 |
| --- | --- | --- | --- |
| `CELL_TEXT` | `SetCellText(id, col, text)` | 单行文字 | F2 / 双击 → in-place EDIT* |
| `CELL_ICON` | `SetCellIcon(id, col, hbm)` | 居中 18×18 小图标 | 无 |
| `CELL_IMAGE` | `SetCellImage(id, col, hbm)` | 缩放到列宽的大图 | 无 |
| `CELL_CHECKBOX` | `SetCellChecked(id, col, b)` | 方框 + ✓ | 单击切换* → `DUITVN_CHECKED` |
| `CELL_PROGRESS` | `SetCellValue(id, col, 0..100)` | 进度条 + 百分比文字 | 单击 / 拖动改值* → `DUITVN_VALUECHANGED_CELL` |
| `CELL_HYPERLINK` | `SetCellLink(id, col, text, url)` | 蓝色下划线文字 | 单击发 `DUITVN_LINKCLICK`（始终） |

* 受<u>编辑闸</u>控制：`SetEditable(true)` 全局开关 + `SetColumnEditable(col, b)` 单列开关。<u>默认 false</u>（只读）。Hyperlink 不受闸控制。

**选中模型：**多列模式下选中粒度 = 单元格。`GetCurSelCell()` 返回 `{itemId, col}`。Ctrl+click 多选 / Shift+click 区域选 → `GetSelectedCellCount()`+`GetSelectedCell(n)` 遍历。当前活动 cell 画 1px 品牌蓝边 + 1px 内白（cell focus rect）。

**列宽行为：**`SetColumnMinWidth` 给最小（默认 40px）；最后列<u>不</u>自动 stretch（留 scrollbar 空间）；header 列分隔线双击触发 <u>auto-fit</u>（按可见 cell 实际文字宽度）；<u>不</u>支持拖动重排列序。

**视觉细节：**`SetZebra(bool)` 默认关；hover 整行高亮；cell focus rect 1px 蓝 + 1px 内白；文本溢出末尾省略号。

#### 代码用法（多列）

下面这段就是上图截图所对应的 DuiGallery `Build_TreeView` 多列段实现（`DuiGallery/Pages.cpp`），可以直接拷贝改造。

```
auto tree = std::unique_ptr<DuiTreeView>(new DuiTreeView());
tree->SetCtrlId(901);

// Column layout.
int colName = tree->AddColumn(_T("Name"),     200, 80, DT_LEFT);
int colDone = tree->AddColumn(_T("Done"),      70, 50, DT_CENTER);
int colProg = tree->AddColumn(_T("Progress"), 160, 90, DT_LEFT);
int colSize = tree->AddColumn(_T("Size"),      90, 60, DT_RIGHT);
int colLink = tree->AddColumn(_T("Link"),     180, 80, DT_LEFT);

tree->SetFrozenColumns(1);              // Name 列贴左不水平滚
tree->SetEditable(true);                // 允许 Text 双击编辑
tree->SetSortIndicator(colName, +1);    // 初始按 Name 升序（仅画三角）

struct Row {
    LPCTSTR name;  bool done;  int progress;
    LPCTSTR size;  LPCTSTR link;  LPCTSTR url;
};
const Row rows[] = {
    { _T("README.md"),  true,  100, _T("4.2 KB"), _T("open"),     _T("https://example.com/readme") },
    { _T("design.pdf"), false,  60, _T("1.1 MB"), _T("preview"),  _T("https://example.com/design") },
    { _T("logo.png"),   true,  100, _T("87 KB"),  _T("download"), _T("https://example.com/logo")   },
    { _T("notes.txt"),  false,  20, _T("612 B"),  _T("edit"),     _T("https://example.com/notes")  },
    { _T("schema.sql"), false,  85, _T("32 KB"),  _T("run"),      _T("https://example.com/sql")    },
};

int gProj = tree->AddRoot(_T("project-alpha"));
for (auto& r : rows) {
    int id = tree->AddChild(gProj, r.name);    // col 0 = Name
    tree->SetCellChecked(id, colDone, r.done);
    tree->SetCellValue  (id, colProg, r.progress);
    tree->SetCellText   (id, colSize, r.size);
    tree->SetCellLink   (id, colLink, r.link, r.url);
}
int gAnother = tree->AddRoot(_T("project-beta"));
int idDoc = tree->AddChild(gAnother, _T("docs.md"));
tree->SetCellChecked(idDoc, colDone, false);
tree->SetCellValue  (idDoc, colProg, 45);
tree->SetCellText   (idDoc, colSize, _T("8.7 KB"));
tree->SetCellLink   (idDoc, colLink, _T("open"), _T("https://example.com/docs"));

vbox->AddChild(std::move(tree), balloonwjui::DuiLayout::Hint().Weight(1));
```

#### XML 用法（仅列定义）

节点（`AddRoot/AddChild`）必须 C++ 端添加；XML 只能配列定义，因为节点常运行时变化用 XML 表达不划算。

```
<vbox padding="0">
    <treeview id="100" row-height="28" header-height="26"
              frozen-cols="1" editable="true" zebra="false">
        <column title="Name"     width="200" min-width="80"  align="left"   sortable="true"  editable="true"/>
        <column title="Done"     width="70"  min-width="50"  align="center"/>
        <column title="Progress" width="160" min-width="90"  align="left"/>
        <column title="Size"     width="90"  min-width="60"  align="right"/>
        <column title="Link"     width="180" min-width="80"  align="left"   editable="false"/>
    </treeview>
</vbox>
```

```
// caller 侧（节点仍由 C++ 添加，XML 只是装好了列）：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
auto* tree = static_cast<balloonwjui::DuiTreeView*>(host.GetRoot()->FindControlById(100));
int gProj = tree->AddRoot(_T("project-alpha"));
// ... 继续 AddChild + SetCellXxx，跟代码用法段一致
```

#### 事件

事件载体分两类：<u>行级事件</u> `extra = itemId int`；<u>单元格 / header 级事件</u> `extra = DuiTreeView::DuiTreeCellNotify*`（生命期仅在 `SendMessage` 调用栈期间，父若需要存值要 deep-copy `text`）。

| code | 触发 | extra |
| --- | --- | --- |
| `DUIN_CLICK` | 左键点击节点 | itemId |
| `DUIN_DBLCLK` | 双击节点（CELL_TEXT 双击且 editable=on 时进 in-place EDIT 不冒此事件） | itemId |
| `DUIN_VALUECHANGED` | 当前选中变化 | 新 itemId / -1 |
| `DUITVN_EXPAND_TOGGLED` | 展开 / 折叠完成 | itemId |
| `DUITVN_RCLICK` | 行右键（不改选中） | itemId |
| `DUITVN_HEADER_RCLICK` | 多列 header 列名右键 | `DuiTreeCellNotify*`（col） |
| `DUITVN_BLANK_RCLICK` | 多列表体空白区右键 | `DuiTreeCellNotify*`（itemId=-1, col=-1） |
| `DUITVN_COLUMNCLICK` | 多列 header 列名单击 = 排序意图（库不排数据，业务排完调 `SetSortIndicator`） | `DuiTreeCellNotify*`（col, ascending） |
| `DUITVN_CHECKED` | CheckBox cell 切换 | `DuiTreeCellNotify*`（itemId, col, checked） |
| `DUITVN_VALUECHANGED_CELL` | ProgressBar cell 拖动改值 | `DuiTreeCellNotify*`（value） |
| `DUITVN_LINKCLICK` | Hyperlink cell 单击（始终触发，不受 editable 控制） | `DuiTreeCellNotify*`（text=URL） |
| `DUITVN_CELLEDITED` | Text cell in-place EDIT 提交（Enter / 失焦） | `DuiTreeCellNotify*`（text=新文本） |

```
// 多列模式 OnDuiNotify
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId != IDC_PROJ_TREE) return 0;
using TV = balloonwjui::DuiTreeView;

switch (n->code) {
case TV::DUITVN_COLUMNCLICK: {
    auto* c = (TV::DuiTreeCellNotify*)n->extra;
    SortRowsByColumn(c->col, c->ascending);     // 业务自排
    m_tree->SetSortIndicator(c->col, c->ascending ? +1 : -1);
    break;
}
case TV::DUITVN_LINKCLICK: {
    auto* c = (TV::DuiTreeCellNotify*)n->extra;
    ::ShellExecute(nullptr, _T("open"), c->text, nullptr, nullptr, SW_SHOW);
    break;
}
case TV::DUITVN_CHECKED: {
    auto* c = (TV::DuiTreeCellNotify*)n->extra;
    PersistDoneState(c->itemId, c->checked);
    break;
}
case TV::DUITVN_CELLEDITED: {
    auto* c = (TV::DuiTreeCellNotify*)n->extra;
    SaveCellText(c->itemId, c->col, c->text);
    break;
}
}
```

#### 扩展 API（单列模式增强）

下面几组 API 在单列模式生效，配合"分组好友 / 联系人 / 文件树"等场景常用；多列模式忽略（多列请用 `SetCellText` / `SetCellIcon` 等等价 API）。

**节点副标签（两行节点）**

`SetItemSubLabel(id, text)` 给节点加第二行小字（在主 label 下方、字号更小、颜色更淡）。空字符串 = 回退单行垂直居中绘（向后兼容）。两行排版需要 caller 自己抬高行高（建议 `SetRowHeight(40)`），全局字号 / 颜色用 `SetSubLabelPointSize(pt)` / `SetSubLabelTextColor(c)` 调整（默认 8pt + `RGB(140,140,140)`）。

**节点右侧辅助文字**

`SetItemRightText(id, text)` 在行右端 status dot 内侧右对齐绘一小段辅助文字 —— 典型用例：分组节点的"在线数 / 总数"、单元格的尺寸 / 时间。绘制时按文字实际宽度收缩 textRight，主 label 自动 ellipsis。字号 / 颜色用 `SetRightTextPointSize(pt)`（0 = 与主 label 同字号 9pt）/ `SetRightTextColor(c)`（默认浅灰）调整。

**Icon 灰显**

`SetItemIconGrayed(id, true)` 把节点 icon 按 NTSC luma 系数转灰度后绘出，原 HBITMAP 不动。纯 GDI 实现（无 GDI+ 依赖），逐像素遍历 + 保留 alpha 通道。

**展开状态快照（救命 API）**

```
// "存 → 改数据 → 还" 模式：刷新前后保留用户的展开 / 折叠选择
std::vector<int> snap = tv->GetExpandedSnapshot();
tv->CollapseAll();
// ... 业务从数据源全量重建节点 ...
tv->RestoreExpanded(snap);     // 增量恢复：snap 内 id 调 Expand，
                               // 不在 snap 里的节点保持当前状态不变
```

`RestoreExpanded` 对 snapshot 内已不存在的节点 id 静默跳过；一次性 RebuildVisible，避免 N 次 SetExpanded 触发 N 次 Invalidate。

**节点可见性 / 全局过滤**

```
// 单节点开关：连同后代一起隐藏，数据保留
tv->SetItemVisible(idBob, false);

// 全局过滤器：predicate 返回 true 表示保留可见，false 隐藏
tv->SetFilter([&tv](int id) -> bool {
    CString lbl = tv->GetItemLabel(id);
    return lbl.Find(_T("Eng")) == 0;       // 只留 "Eng*" 开头的
});
tv->ClearFilter();                         // 移除过滤器
```

最终可见 = `SetItemVisible` 标志为 true <u>且</u>（无 filter 或 filter 返回 true）。父节点被任一条件隐藏时，后代也一起隐藏（按 `depth` 范围跳过）。

**Hover 事件**

| code | 触发 | extra |
| --- | --- | --- |
| `DUITVN_HOVER_ENTER` | 鼠标进入新节点（m_hoverId 变化） | 新节点 itemId |
| `DUITVN_HOVER_LEAVE` | 鼠标离开之前节点 / 鼠标离开控件 | 旧节点 itemId |

库本身<u>无延时</u>：每次 m_hoverId 变化都立即 fire。业务侧若要"悬停 ~500ms 才弹卡片"语义，自己在 ENTER 时启 `SetTimer`、LEAVE 时 `KillTimer` 实现。屏幕坐标用 `::GetCursorPos()` 现场取。

DuiGallery 演示：TreeView tab → **"Hover notify (DUITVN_HOVER_ENTER/LEAVE) & auto-hide scrollbar"** section（含 hover label 实时更新 + 滚动条 auto-hide 行为）。

#### 性能特征

**什么是 dirty-rect 行裁剪？**

"dirty rect"（脏矩形）是 Windows 在 `WM_PAINT` 时告诉控件的"<u>这次只有这个矩形真的需要重画</u>"的提示。窗口被遮挡又露出来 / 滚动条滚一行 / `InvalidateRect` 主动标脏，每种场景都会让 dirty rect 只包含真正变化的那一小块（例如滚一行只标新进来的那一行带）。Windows 还会在底层用 `BitBlt` / `ScrollWindowEx` 把已经画好的内容平移到新位置，控件只需补画"新进来的那条"。

行控件如果不利用这个信息，就会在每次 `WM_PAINT` 把全部 N 行（哪怕只有 30 行能在屏幕上看见）都跑一遍 paint 循环，即便 GDI 自己会把视口外的画图调用裁掉，C++ 那层循环开销已经付出了。<u>dirty-rect 行裁剪</u>就是把 dirty rect 的 `top` / `bottom` 反推成第几行 ~ 第几行：

```
// 假设第 r 行的 y 范围是 [yOrigin + r * rowH, yOrigin + (r+1) * rowH)
// 那么需要画的行号区间就是
int rowFrom = max(0,        (rcDirty.top    - yOrigin) / rowH);
int rowTo   = min(rowCount, (rcDirty.bottom - yOrigin + rowH - 1) / rowH);
for (int r = rowFrom; r < rowTo; ++r) { ... }
```

这样无论列表有 100 行还是 100 万行，每帧只循环视口里实际可见的那 **~30 行**。对应的渲染耗时跟"行数"<u>解耦</u>，只跟视口大小有关。

**注意**：dirty-rect 行裁剪解决的是"<u>渲染</u>"成本（每帧 OnPaint），并<u>不等于</u>"虚拟列表"。它仍然要求所有行的<u>数据</u>都加载进了内存（DuiTreeView 的 `m_nodes` 是 `std::vector<Node>`，全量持有）。如果连"持有所有行"都吃不消，要用真正的虚拟列表 —— 见下面 [DuiVirtualList](#DuiVirtualList)。

**DuiTreeView 的具体策略：**节点存成扁平 `std::vector<Node>` + 一份 `m_visible`（被展开的可见行索引）。每帧重绘只走 `m_visible`；折叠后的子树压根不进遍历。两种模式都做了 dirty-rect 行裁剪：把 Windows 给的 `rcDirty` 反推成需要画的行号区间，视口外的行直接跳过。多列模式还会按 4 象限（TL/TR/BL/BR — 见多列模式段的图）分别裁剪，冻结面板和滚动区互不打扰。

**没有 DuiVirtualTree。**balloonui 暂时只有<u>扁平</u>列表的虚拟版（DuiVirtualList），没有<u>树形</u>的虚拟版。原因是树形数据要算父子可见性 + 缩进，回调式接口不容易表达干净。如果你的树有几十万节点，目前推荐的做法是分批加载（业务侧按 lazy-expand 策略，节点首次展开才 `AddChild`）+ 必要时折叠掉不感兴趣的子树（`Collapse` 后的子节点不参与 dirty-rect 循环）。

**实测档位（2.4 GHz CPU，Debug Win32，参考值；Release 优化更好）：**

| 节点数 | 单列模式 | 多列模式 |
| --- | --- | --- |
| ≤ 1k | 流畅 | 流畅 |
| 1k – 5k | 流畅 | 流畅 |
| 5k – 20k | 流畅（dirty-rect 裁剪兜底） | 流畅（dirty-rect 裁剪兜底） |
| 20k – 50k | 滚动可能掉帧 | 滚动可能掉帧；选中大量 cell 时画选中态会明显变慢 |
| > 50k | 不建议直接用 | 不建议直接用 |

**渲染开销：每帧只画视口可见行**。`PaintBody` 的核心裁剪逻辑（`balloonui/Controls/List/DuiTreeView.cpp` 内 `paintQuadrant` lambda 入口）：

```
// 每象限按 dirty-rect 收窄行号：只画行 y 区间与 (rcDirty ∩ rcQuad) 相交
// 的那几行。不做这步的话，BR 象限每帧都会完整循环 m_visible × m_columns，
// 上万行时这是主要瓶颈。
//
// yOriginAbs = 该象限坐标系下"第 0 行"的绝对 y 值
//   · 冻结行象限 TL / TR：= body.top；
//   · 滚动行象限 BL / BR：= body.top - m_scrollY。
// 行 r 占据 y ∈ [yOriginAbs + r*m_rowH, yOriginAbs + (r+1)*m_rowH)。
int yLo = (rcDirty.top    > rcQuad.top)    ? rcDirty.top    : rcQuad.top;
int yHi = (rcDirty.bottom < rcQuad.bottom) ? rcDirty.bottom : rcQuad.bottom;
if (yHi <= yLo) { return; }

int relLo = yLo - yOriginAbs;
int relHi = yHi - yOriginAbs;
int visLo = (relLo >= 0) ? (relLo / m_rowH) : 0;
int visHi = (relHi >= 0) ? ((relHi + m_rowH - 1) / m_rowH) : 0;
if (visLo > rowFrom) { rowFrom = visLo; }
if (visHi < rowTo)   { rowTo   = visHi; }
if (rowFrom >= rowTo) { return; }
```

实测在 10k 行多列树滚动时，单帧 OnPaint ≈ 1–4 ms，峰值 ≤ 10 ms，跟视口能容下的 ~30 行一致。脏区高度（滚一行时通常只有几十 px）也会被 Windows 自身的 `ScrollWindowEx` 进一步压缩，多数滚动只重绘一条窄带。

**已知瓶颈 / 不在 dirty-rect 裁剪管辖范围内：**

- **插入 / 删除 / 展开折叠是 O(N)**：`AddRoot`/`AddChild`/`Remove`/`SetExpanded` 内部都会调一次 `RebuildVisible`（全表扫一遍 `m_nodes`）。批量灌 N 行就是 O(N²)，10k 行 ≈ 1 秒、30k 行 ≈ 10 秒。日常一次性建树没问题；如果业务要做"实时追加日志"那种持续插入，需要在调用侧攒一批再统一 `Clear`+rebuild。
- **cell-level 多选是线性扫描**：每画一个 cell 都会遍历一次 `m_selCells` 看自己是否在选中里。视口里 30 行 × 5 列 × 选中 100 cell ≈ 1.5 万次比较 / 帧仍能扛住；但若用户全选几千 cell，会开始掉帧。需要时考虑把 `m_selCells` 换成哈希表。
- **节点存扁平 `vector<Node>`**：删子树要从中间删掉一段，会触发后续元素位移；频繁删 / 移动节点的场景会有 O(N) 抖动。

#### 压测 demo：DemoTreeViewLargeData

仓库内自带的多列大数据量压测 demo。位置 `flamingoclient/DemoTreeViewLargeData/`，编出来在 `Bin/DemoTreeViewLargeData.exe`。打开后顶部有"插入 1k / 10k / 30k 行 / 清空 / 全展开 / 全折叠"6 个按钮，下方状态条实时显示：

```
行数: N  |  上次 OnPaint: X.XX ms (脏区高 H px)
       |  最近 60 帧平均/峰值: A.AA / P.PP ms
```

状态行靠 200ms 一次的 `WM_TIMER` 刷新；`OnPaint` 耗时由一个子类化的 `TimedTreeView` 用 `QueryPerformanceCounter` 在 `DuiTreeView::OnPaint` 外面包计时落到 60 槽环形缓冲。完整源码见 `DemoTreeViewLargeData/main.cpp`。

**验证 dirty-rect 裁剪是否真的生效**（A/B 对照）：

1. 跑一次 demo，按"插入 10k 行"，记下滚动时的"平均/峰值"（参考值：~1–3 ms / ~5–10 ms）。
2. 把 `DuiTreeView.cpp` 里 `paintQuadrant` lambda 入口的"在 y 轴上把 rcDirty 和 rcQuad 求交"那段（`yLo / yHi / relLo / relHi / visLo / visHi` 6 个变量加 4 个 if）注释掉，单独重编 `balloonui.dll`（不必动 demo）。
3. 再跑一次 demo，同样滚 10k 行 — "平均/峰值"会涨到 30–80 ms / 80+ ms 区间，肉眼能感到滞后。

<a id="DuiTab"></a>

### DuiTab  `[DUI]`

![水平 DuiTab](images/ctl-tab-horizontal.png)

![垂直 nav (DuiListBox)](images/ctl-tab-vertical.png)

*上：水平 tab 栏（Friends/Groups/Recent/Archive，第二个选中）；下：设置页风格的垂直导航（用 DuiListBox 实现）。*

水平 tab 栏。`AddTab(title)` 加项，点击切换 + 发 `DUIN_VALUECHANGED`（extra=新选中索引）。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。<u>通常在 `DuiVBox` 顶部</u>，下方配 `DuiTabPage` 切换内容（两者通过同 ctrlId 通知联动）。

#### 代码创建

```
auto tab = std::make_unique<balloonwjui::DuiTab>();
tab->SetCtrlId(IDC_MAIN_TAB);
tab->AddTab(_T("Files"));
tab->AddTab(_T("Edit"));
tab->AddTab(_T("View"));
tab->SetCurSel(0, /*notify*/false);
vbox->AddChild(std::move(tab), balloonwjui::DuiLayout::Hint().Fixed(28));
```

#### 图标 + 自适应宽度

每个 tab 可在文字左侧显示一个 16x16 PARGB 图标（`HBITMAP`，caller-owned，控件不拷贝也不释放，传 `nullptr` 清除）。tab 宽度默认 clamp 到 `[60, 200]`，也可整体切换为**自适应文字**，跳过 clamp 让短 tab 紧贴 padding、长 tab 不再被省略号截断。

```
// 图标：随 AddTab 一起传，或事后用 SetTabIcon 设。
tab->AddTab(_T("Inbox"), /*closeable*/false, /*dropdown*/false, /*lParam*/0, hIconRed);
tab->SetTabIcon(1, hIconGreen);

// 调整图标尺寸与图标-文字间距（所有 tab 共用，默认 16 / 6）。
tab->SetIconSize(18);
tab->SetIconGap(8);

// 宽度自适应：打开后 tab 宽严格贴合内容（text + padding + 可选 icon / close / dropdown 增量），
// 跳过 [m_minTabW, m_maxTabW] clamp。默认 false。
tab->SetAutoFitTabWidth(true);
```

图标渲染走 `::AlphaBlend`，与 `DuiButton::SetLeadingIcon` 完全一致；HBITMAP 用 `CreateDIBSection` 造 32 位 PARGB 位图即可。

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 用户切 tab；`SetCurSel(_, true)` 程序切换也触发 | 新选中索引 |
| `DuiTab::DUITN_CLOSE` | 用户点击 closeable tab 上的 × 按钮。**本通知<u>不</u>移除 tab**，由父决定是否真的删 — 例如可能要弹"确认关闭未保存" | 请求关闭的 tab 索引 |
| `DuiTab::DUITN_DROPDOWN` | 用户点击 dropdown tab 的下拉箭头 | tab 索引（父弹 `DuiMenu`） |
| `DuiTab::DUITN_REORDERED` | `SetReorderEnabled(true)` 启用后，拖动 tab 重排完成 | 新索引 |

```
// 父对话框 OnDuiNotify：tab 全套行为
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_MAIN_TAB) {
    int idx = (int)n->extra;
    switch (n->code) {
    case DUIN_VALUECHANGED:
        m_tabPage->SetCurSel(idx, /*notify*/false);
        break;
    case balloonwjui::DuiTab::DUITN_CLOSE:
        if (CanCloseSession(idx)) m_tab->RemoveTab(idx);
        break;
    case balloonwjui::DuiTab::DUITN_DROPDOWN:
        ShowTabDropdownMenu(idx);   // 业务弹自己的菜单
        break;
    case balloonwjui::DuiTab::DUITN_REORDERED:
        m_sessionOrder.MoveItem(/*from=*/m_lastDragIdx, /*to=*/idx);
        break;
    }
}
```

<a id="DuiTabPage"></a>

### DuiTabPage  `[layout]`

![DuiTabPage 与 DuiTab 联动](images/ctl-tabpage-content.png)

*顶部 DuiTab + 下方内容区组合，切 tab 换内容。*

复合控件：内部 = 一个 tab 头条（私有 `DuiTab`） + N 个 page 子树（任意 `DuiControl`）。`AddPage(title, page)` 加一页，`SetCurSel(i, notify)` 切换显示。<u>DuiTabPage 自带 tab 头条，无需外面再挂一个独立的 `DuiTab`。</u>

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。本身像一个"按 SetCurSel 显示指定子的容器"。

#### 代码创建

```
auto tp = std::make_unique<balloonwjui::DuiTabPage>();
tp->SetCtrlId(IDC_TABPAGE);
tp->AddPage(_T("Files"), BuildFilesPage());
tp->AddPage(_T("Edit"),  BuildEditPage());
tp->SetCurSel(0, /*notify*/false);
vbox->AddChild(std::move(tp), balloonwjui::DuiLayout::Hint().Weight(1));
```

内部头条可用 `GetHeader()` 取到 `DuiTab*`，调它的样式 setter（色彩 / closeable 等）；<u>但不要</u>通过 GetHeader() 直接 AddTab / RemoveTab，会与 page 列表失同步。

#### 图标 + 自适应宽度（透传内部 DuiTab）

DuiTabPage 把图标 / 自适应宽度等 tab 头条相关 API 透传到内部 `DuiTab`，行为与上文 [DuiTab](#DuiTab) 一致；`AddPage` 末位可直接传 icon。

```
tp->AddPage(_T("Inbox"),   BuildInboxPage(),   hIconRed);
tp->AddPage(_T("Sent"),    BuildSentPage(),    hIconGreen);
tp->SetPageIcon(1, nullptr);    // 清除已设图标

tp->SetIconSize(20);
tp->SetIconGap(8);
tp->SetAutoFitTabWidth(true);   // 标题长短一目了然（适合分类条 / 过滤条）
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 当前激活页变化（用户点 tab 头切换，或程序调 `SetCurSel(_, /*notify*/true)`）；`ctrlId = DuiTabPage 的 id`，内部 DuiTab 的切换事件已被重定向。 | 新激活页索引 |

```
// 业务只盯 DuiTabPage 自己的 ctrlId 即可；内部头条对外不可见。
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_TABPAGE && n->code == DUIN_VALUECHANGED) {
    int idx = (int)n->extra;
    // 例：把用户最近用的 page 索引持久化
    SaveLastTabIndex(idx);
}
```

<a id="DuiMenu"></a>

### DuiMenu  `[popup]`

右键 / 命令菜单。支持快捷键文本列、checked 状态、子菜单（hover 自动展开）、danger 项（红色文字）、分隔线。

**典型父：**<u>不挂</u>父 DuiControl —— 调 `PopupAt(pt, ownerHwnd)` 在屏幕坐标弹出，自己创建一个 popup HWND；选完 / 失焦自动销毁。owner HWND 是事件接收者（通常就是当前对话框）。

#### 代码用法

```
// 在某个右键事件 handler 里调用：
balloonwjui::DuiMenu m;
m.AddItem(IDM_OPEN,   _T("Open"),       _T("Ctrl+O"));
m.AddItem(IDM_SAVE,   _T("Save"),       _T("Ctrl+S"));
m.AddSeparator();
m.AddItem(IDM_DELETE, _T("Delete"),     _T("Del"), /*danger*/ true);

POINT pt; ::GetCursorPos(&pt);
m.PopupAt(pt, m_hWnd);    // m 是栈对象；菜单关闭后局部变量析构
// owner（m_hWnd）收 WM_DUI_NOTIFY: code=DUIN_CLICK, ctrlId=IDM_OPEN
```

**无 XML 路径**：DuiMenu 通过命令式 API 弹出，没有"挂入控件树"这一步，因此不参与 XML builder。如要从配置文件描述菜单项，业务自己写解析代码。

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_CLICK` | 用户选了某个菜单项；菜单自动关闭。disabled 项不发；分隔线不可点 | 0（项 id 由 `n->ctrlId` 给出，即 `AddItem` 的第一个参数） |

事件目标是 `PopupAt(pt, owner)` 传入的 `owner` HWND（不一定是触发右键的控件所在窗口）— 通常就是主对话框。

```
// 弹菜单（在某个右键事件里）
POINT pt; ::GetCursorPos(&pt);
balloonwjui::DuiMenu m;
m.AddItem(IDM_OPEN, _T("Open"), _T("Ctrl+O"));
m.AddItem(IDM_SAVE, _T("Save"), _T("Ctrl+S"));
m.PopupAt(pt, m_hWnd);

// 父对话框 OnDuiNotify 收菜单事件：
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->code == DUIN_CLICK) {
    switch (n->ctrlId) {
    case IDM_OPEN: DoOpen(); break;
    case IDM_SAVE: DoSave(); break;
    }
}
```

<a id="DuiMenuBar"></a>

### DuiMenuBar  `[list]`

常驻菜单条（host 客户区里的横向 File / Edit / View 那条），由若干**栏目**组成。每个栏目关联一个 `DuiMenu` 作下拉。点击 / Alt-激活 时本控件调 `DuiMenu::TrackPopupEx` 把下拉弹出来。

**典型父：**`DuiVBox`（窗口顶部一行，`fixedHeight=24`）。可被 `<menu-bar>` XML 标签直接构造。

#### 视觉

- 透明底；hover 一格 `#E5E5E5`；active（下拉打开中）一格 `#D7E3F4` 浅蓝。
- 默认行高 24px；栏目宽度 = 文字宽 + 左右 8px，不固定栏宽。
- 助记符（`&F` 中的 F）下划线<u>仅 Alt 激活态显示</u>。

#### 激活态下 mouse-move 切换栏目

菜单条在 Win10 风格下有"下拉打开后，鼠标移到隔壁栏目自动切（关旧弹新）"的招牌交互。本控件靠 `DuiMenu::SetSwitchZones` + `TrackPopupEx` 实现：注册"其它栏目矩形"为 popup 的切换区域，popup 内部以 30ms 间隔轮询鼠标位置，命中即关闭并报告 zone idx；菜单条本地循环根据 idx 立即在新栏弹下拉。同步阻塞，跟普通 `DuiMenu::TrackPopup` 一致。

#### 代码用法

```
auto bar = std::make_unique<balloonwjui::DuiMenuBar>();
bar->SetCtrlId(IDC_MENUBAR);
bar->AppendItem(IDM_FILE,    _T("文件(&F)"), &m_fileMenu);
bar->AppendItem(IDM_OPTIONS, _T("选项(&O)"), &m_optionsMenu);
bar->AppendItem(IDM_VIEW,    _T("查看(&V)"), &m_viewMenu);
parent->AddChild(std::move(bar), DuiLayout::Hint().Fixed(24));

// 父 OnDuiNotify:
//   case DUIN_VALUECHANGED:                  // 菜单某项被选中
//       HandleMenuChosen((UINT)n.extra);     // extra = chosen menu item id
//       break;
//   case DuiMenuBar::DUIMBN_DROPDOWN_OPEN:   // (调试 / 埋点用)
//       Log("dropdown opened on bar idx %d", (int)n.extra);
//       break;
```

#### XML 用法

```
<menu-bar id="100" item-height="24">
  <menu-item id="101" text="文件(&amp;F)"/>
  <menu-item id="102" text="选项(&amp;O)"/>
  <menu-item id="103" text="查看(&amp;V)"/>
</menu-bar>
```

XML 不支持声明<u>菜单项内容</u>（`DuiMenu` 是事件触发的瞬时弹层）。业务侧 C++ 自己 `new DuiMenu` + `AppendItem` 配出菜单实例，再 `bar->SetDropdown(idx, &menu)` 绑定到对应栏目。

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 用户在某下拉里选中了一项；dismissed 不发 | 被选项的菜单 id（`DuiMenu::AppendItem` 的 `nID`） |
| `DUIMBN_DROPDOWN_OPEN` | 即将弹某栏的下拉 | 栏目 index |
| `DUIMBN_DROPDOWN_CLOSE` | 下拉刚关闭（无论是否选中、是否切换中） | 栏目 index |

#### 键盘

- `Alt` + 助记符字母 → 直接打开匹配栏的下拉（host 须在 `WM_SYSKEYDOWN` 转发到 `bar->ProcessAltKey(vk)`）。
- 下拉打开期间，`Esc` / `Enter` / 选项点击由 popup 自身处理（属 `DuiMenu` 行为）。

## 7.5 反馈与浮层

<a id="DuiPopupHost"></a>

### DuiPopupHost  `[popup]`

自动定位的浮层窗口，内部还是一棵 DuiControl 子树。常用作下拉、表情面板、自定义 tooltip。

**典型父：**<u>顶层 HWND 容器</u>（临时浮层），本身不嵌在父 DuiControl 里 —— 由父控件按需弹出，菜单 / EmojiPanel / 自定义 popup 都基于它。caller 调 `popup.SetContent(std::move(content))` 装入内容，然后 `popup.Show(anchorRect, ownerHwnd)` 显示（首次 Show 懒创建 HWND；anchorRect 是屏幕坐标）。

#### 代码用法

```
// emoji 按钮 OnClick handler 里：
RECT anchorScreen;
m_emojiBtn->GetRect(anchorScreen);
::ClientToScreen(m_hWnd, (LPPOINT)&anchorScreen);
::ClientToScreen(m_hWnd, ((LPPOINT)&anchorScreen) + 1);

m_pop.SetContent(BuildEmojiPanel());
m_pop.Show(anchorScreen, m_hWnd);     // owner = m_hWnd，事件冒到对话框
```

**无 XML 路径**：popup 自身（边缘 / 阴影 / anchor）走 C++；其内部 content 子树<u>可以</u>用 `builder.FromString(...)` 解析后传给 `SetContent`。

#### 事件

本身不发事件 — 它只是个浮层壳。<u>其内部 `Content` 子控件</u>的事件按常规通过 `WM_DUI_NOTIFY` 冒到 popup 的 owner HWND（即调 `SetOwner(hwnd)` 的窗口）。所以业务收到的还是 `DUIN_CLICK` / `DUIN_VALUECHANGED` 等 — 路由透明。

<a id="DuiToolTip"></a>

### DuiToolTip  `[popup]`

悬停提示文本。通过单例 `DuiToolTipMgr` 注册控件 + 文本，hover 一定时间后自动浮现，移开自动消失。

**典型父：**<u>不挂</u>父 DuiControl —— 单例 `DuiToolTipMgr` 持有，父控件只是"被注册"的 target；hover 计时和 popup 显隐由管理器自管，业务无须接收事件。

#### 代码用法

```
// 在 button 已经挂入 layout 后注册：
vbox->AddChild(std::move(button), balloonwjui::DuiLayout::Hint().Fixed(32));
balloonwjui::DuiToolTipMgr::Inst().Register(buttonRaw, _T("Save the file"));

// 不需要时：
balloonwjui::DuiToolTipMgr::Inst().Unregister(buttonRaw);
```

**无 XML 路径**：tooltip 通过命令式 API 注册，没有"挂入控件树"这一步，因此不参与 XML builder。

#### 事件

不发事件。tooltip 是单向呈现：注册后由管理器自管 hover 计时与显隐，业务无须接收任何通知。

<a id="DuiProgressBar"></a>

### DuiProgressBar  `[DUI]`

![DuiProgressBar 0/35/100](images/ctl-progressbar-states.png)

*左→右：0% / 35% / 100%。可叠加百分比文字。*

进度条。横 / 纵向、可显示百分比文字、不定模式（marquee）。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。常见用法是放在文件传输 / 后台任务 row 中。

#### 代码创建

```
auto pb = std::make_unique<balloonwjui::DuiProgressBar>();
pb->SetCtrlId(IDC_DOWNLOAD_PROGRESS);
pb->SetRange(0, 100);
pb->SetValue(35);
pb->SetShowText(true);
pb->SetMarquee(false);   // true 时不定进度
hbox->AddChild(std::move(pb), balloonwjui::DuiLayout::Hint().Weight(1));
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | `SetPos(_, true)` 程序写入；`SetPos(_, false)` 不发。无用户交互来源（进度条不可拖） | 新位置 |

```
// 一般直接 SetPos 写入即可，不需要 OnDuiNotify。仅当需要被
// 第三方代码（如下载线程）通过 NotifyParent 转写到 UI 时用：
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_DOWNLOAD_PROGRESS && n->code == DUIN_VALUECHANGED) {
    int pct = (int)n->extra;
    m_lblDownload->SetText(CString().Format(_T("下载中 %d%%"), pct));
}
```

#### 关键 API

|   |   |
| --- | --- |
| `SetRange(min, max) / SetPos(v, notify)` | 取值范围 + 当前进度。clamp 进区间 |
| `SetText(LPCTSTR) / SetShowPercent(bool)` | 覆盖文字优先级：m_text 非空 → 显 text；否则 showPercent=true → 显 "NN%"；否则不显 |
| `SetVertical(bool)` | true = 上下方向；false = 左右（默认） |
| `SetMarquee(bool) / SetMarqueePhase(int)` | 不定进度（流光条）。<u>需 caller 用 timer 驱动 phase</u>，本控件不自跳 |
| `SetBgColor / SetFillColor / SetBorderColor / SetTextColor` | 颜色覆盖 |

#### XML 创建

```
<hbox padding="8" gap="6" fixedHeight="24">
    <label    text="下载中" fixedWidth="60"/>
    <progress id="700" min="0" max="100" value="40" show-percent="true" weight="1"/>
</hbox>
```

```
// caller 侧：
auto root = balloonwjui::DuiXmlBuilder().FromString(xml);
host.SetRoot(std::move(root));
```

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<progress min="0" max="100" value="40" show-percent="true"/>` |
| 详细属性参考 | [§3.3.8 progress](#xml-progress) |
| 事件 | `DUIN_VALUECHANGED` — `SetPos(_, true)` 改变值时；XML 阶段调的都是 notify=false 不通知 |

<a id="DuiEmojiPanel"></a>

### DuiEmojiPanel  `[popup]`

![DuiEmojiPanel 默认面板](images/ctl-emojipanel-default.png)

*面板默认 8 列网格 + 上排 tab（截图为屏外快照，实际表情字符运行时可见）。*

表情面板：上排 tab 切分类、下方 8 列网格。点击表情发 `DUIN_CLICK`，extra 是表情索引。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）作为常驻面板使用；最常见用法是嵌在 `DuiPopupHost` 里 —— emoji 按钮单击时由父弹出 popup 显示 panel。

#### 代码用法（弹出式，最常见）

```
// 在父对话框成员里持有 popup：
balloonwjui::DuiPopupHost m_emojiPopup;

// emoji 按钮 OnClick：
auto ep = std::make_unique<balloonwjui::DuiEmojiPanel>();
ep->SetCtrlId(IDC_EMOJI_PANEL);
ep->AddCategory(_T("😀"), GetSmileyCodepoints());
ep->AddCategory(_T("🐾"), GetAnimalCodepoints());
m_emojiPopup.SetContent(std::move(ep));

RECT anchor;
m_emojiBtn->GetRect(anchor);
::ClientToScreen(m_hWnd, (LPPOINT)&anchor);
::ClientToScreen(m_hWnd, ((LPPOINT)&anchor) + 1);
m_emojiPopup.Show(anchor, m_hWnd);
```

#### 代码用法（常驻面板）

```
// 也可以直接挂到 layout 容器作为长开面板：
auto ep = std::make_unique<balloonwjui::DuiEmojiPanel>();
ep->AddCategory(_T("😀"), GetSmileyCodepoints());
vbox->AddChild(std::move(ep), balloonwjui::DuiLayout::Hint().Fixed(280));
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 用户点击格子里某个表情 | 表情索引（在当前激活分类内） |

```
// 父对话框 OnDuiNotify：把表情插入到聊天输入框
auto* n = (balloonwjui::DuiNotify*)lp;
if (n->ctrlId == IDC_EMOJI_PANEL && n->code == DUIN_VALUECHANGED) {
    int emojiIdx = (int)n->extra;
    CString tag;
    tag.Format(_T("[emoji:%d]"), emojiIdx);   // 业务的占位串
    m_inputBox->ReplaceSel(tag);
    m_emojiPopup->Hide();
}
```

## 7.6 媒体

<a id="DuiGif"></a>

### DuiGif  `[DUI]`

GIF 动图播放。支持本地路径与内存帧，自动按帧延迟刷新。

**典型父：**任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区），跟普通 leaf 一致。常见用法是聊天气泡内的小动图、加载中动画。

#### 代码创建

```
auto gif = std::make_unique<balloonwjui::DuiGif>();
gif->LoadFromFile(_T("emoji_dance.gif"));
gif->Play();      // / Pause / Stop
hbox->AddChild(std::move(gif), balloonwjui::DuiLayout::Hint().Fixed(48));
```

<a id="DuiImageOle"></a>

### DuiImageOle  `[DUI]`

RichEdit 内嵌图片对象。把它插入到 `DuiRichEditHost` 后，图片随文字一起被序列化 / 滚动。常用于聊天窗"插入表情"。

**典型父：**<u>不</u>是 layout 子控件 —— 它是 RichEdit OLE 对象，由业务调 `richEdit->InsertImage(...)` 内联插入到 `DuiRichEditHost` 文本流中，跟普通字符并排参与滚动 / 选区 / 复制粘贴。

#### 代码用法

```
// richEdit 已经挂入 layout 并 EnsureCreated 之后：
HBITMAP hBmp = LoadEmojiBitmap(_T("smiley.png"));
richEdit->InsertImage(hBmp);     // 内联插入到当前光标位置
```

## 7.7 窗口与宿主

<a id="DuiHost"></a>

### DuiHost

![DuiHost 嵌套子树](images/ctl-host-tree.png)

*一个简单的子树：根 VBox 包含标题 + 一行按钮。*

整棵 DUI 树的真窗口宿主。两种使用模式：

- **子窗口模式**：`m_host.Create(hParent, rcClient, ..., WS_CHILD | WS_VISIBLE, 0)` 然后 `SetRoot(...)`。常用于把 DUI 嵌进对话框中。
- **SubclassWindow 模式**：`m_host.SubclassWindow(m_hWnd)` 把已存在的 dialog 整个当 DuiHost。

**典型父：**<u>顶层 HWND 容器</u>，本身不嵌在父 DuiControl 里。caller 调 `host.Create(parentHwnd, rcDefault, name, dwStyle)` 创建（`parentHwnd = NULL` = 顶层窗口；`= HWND` = 嵌入老对话框某区域）；DUI 子树通过 `host.GetRoot()->AddChild(...)` 或 `host.SetRoot(std::move(root))` 挂入。

#### 代码用法

```
class MyDlg : public CDialogImpl<MyDlg> {
    balloonwjui::DuiHost m_host;
    LRESULT OnInitDialog(...) {
        m_host.SubclassWindow(m_hWnd);
        m_host.SetRoot(BuildUi());
        return 0;
    }
};
```

**关键 API**：`SetRoot` / `SendNotify` / `SetDuiFocus` / `InvalidateDuiRect` / `GetDpi` / `SetBgImage` / `SetClientBorderColor`。

#### 客户区 1px 边框 — `SetClientBorderColor(COLORREF)`

给整个客户区描一圈 1px 外框。专门解决"无 OS chrome、客户区浅色、桌面也浅色，看不见窗口边"的场景（典型 = `DuiFrameWindow`）。

| 取值 | 行为 |
| --- | --- |
| `CLR_INVALID`（默认） | **不画**。`DuiHost` 默认就是这个值 — subclass 现有 dialog 时不会在客户区里凭空多一圈线。 |
| 正常 COLORREF | OnPaint 末尾 `FrameRect(rcClient, color)` 画 1px 外框，覆盖在所有 DUI 控件之上。 |
| 任意值 + `SetBgImage` 已设 | **自动跳过不画**。原因：9-grid 图自身已经表达了边界（典型带圆角 / 阴影 / 装饰），再叠 1px 直线会把圆角切方。 |

**谁需要主动调它**：

- `DuiFrameWindow`：**构造时已自动设**为 `RGB(200,200,200)` — 一般不用管。想换色调 `SetClientBorderColor(...)`；不想要边调 `SetClientBorderColor(CLR_INVALID)`。
- 子窗口模式 / SubclassWindow 模式的 `DuiHost`：默认 `CLR_INVALID`，外围已有宿主 dialog 的 chrome，**不要**开。

**颜色取舍**：浅一档（`RGB(217,217,217)` 接近 Win11 系统边）安静；中等（`RGB(200,200,200)` 当前默认）平衡；深一档（`RGB(150,150,150)`）醒目但视觉重；纯黑或品牌色一般不推荐 — 边框太醒目反而像"加了个 outline 的标签"。

#### 事件路由约定

不同输入按"应该感受到反馈的目标"分别走不同路由：

| 输入 | 路由策略 | 说明 |
| --- | --- | --- |
| 键盘（`WM_CHAR` / `WM_KEYDOWN`） | **focus 优先** | 当前键盘焦点控件（`m_pFocus`）接收。Tab 切焦后输入字符落到新焦点。 |
| 鼠标点击 / 移动 / 进入 / 离开 | **HitTest(鼠标位置)** | 命中最深的 visible+enabled 子控件接收。 |
| 鼠标滚轮（`WM_MOUSEWHEEL`） | **HitTest(鼠标位置)** | 跟点击同路径。<u>不</u>走 focus —— 鼠标在哪个滚动容器，滚轮就滚那个。这是 Web / macOS / Win Explorer / 某信 PC 都一致的标准行为。 |

**历史遗留 bug 修复**：早期 `OnMouseWheel` 用 `m_pFocus ? m_pFocus : HitTopMost(pt)` 的回退路径，导致"某个 list 被点过获得 focus 后，鼠标移到别处滚轮仍滚原 list"的典型 bug。当前实现去掉了 focus 优先；caller 不需要为此额外处理什么。如果你确实想"焦点容器接管所有滚轮"（例如某 keyboard-driven 内容编辑器），自己在 host 子类 override `OnMouseWheel` 可以强制走 `m_pFocus` 分支。

<a id="DuiFrameWindow"></a>

### DuiFrameWindow

无边框顶层窗口，自带：

- 自绘 **36 px 默认** 标题栏 + 居中标题文字（可调）
- 右上角 3 个 caption 按钮（最小化 / 最大化 / 关闭）— 平底；不透明态浅灰 hover、Windows 红 close hover；透明态品牌蓝 hover、柔和红 close hover
- 最大化时中间按钮 glyph **自动切换为 restore（双层方块）** — 由 `WM_SIZE`（`SIZE_MAXIMIZED/RESTORED`）驱动
- 客户区四周 1px 浅灰边框（默认开，`RGB(200,200,200)`）— 让没有 OS chrome 的 frame 在浅色桌面上仍能看见窗口范围；调用 `SetBgImage(...)` 后**自动跳过**，让 9-grid 图自身的圆角/装饰当边界
- WM_NCCALCSIZE 返回 0 去掉系统边框，但保留 WS_THICKFRAME 让 Aero snap / 最小化动画正常工作
- WM_NCHITTEST 自动处理 8 向 resize 边（默认 8 px，按 monitor DPI 缩放）、HTCAPTION 拖动区
- WM_GETMINMAXINFO 限制最大化不覆盖任务栏

![DuiFrameWindow 自绘标题栏](images/demo_titlebar_full.png)

**典型父：**<u>顶层 HWND 容器</u>，本身不嵌在父 DuiControl 里 —— 内嵌一个 `DuiHost`。caller 调 `frame.Create(NULL, rcDefault, _T("title"), WS_OVERLAPPEDWINDOW)` + `frame.ShowWindow(...)`；client area 内容通过 `frame.SetClientContent(std::move(root))` 挂入（root 是任意 DuiControl，最常见是 `DuiVBox`）。

#### 用法

```
balloonwjui::DuiFrameWindow frame;
frame.SetTitle(_T("My App"));
frame.SetButtons(true, true, true);   // min, max, close 三按钮
frame.SetTitleBarHeight(36);          // 默认 36（最小 18）
frame.SetMinSize(720, 480);
frame.SetResizable(true);
frame.Create(NULL, CWindow::rcDefault, _T("My App"),
             WS_OVERLAPPEDWINDOW, 0);
frame.SetClientContent(BuildClientUi());   // 注意：不要直接 SetRoot
frame.ResizeClient(1080, 720);
frame.CenterWindow();
frame.ShowWindow(nCmdShow);
```

**事件路由**：caption 按钮通过 `OnDuiChildNotify` 自动转 `SC_MINIMIZE/SC_MAXIMIZE/SC_RESTORE/SC_CLOSE` 给 OS — 业务一行不写。

#### 关键 API

|   |   |
| --- | --- |
| `SetTitle / SetIcon` | 标题文本 / 左侧图标 |
| `SetButtons(min, max, close)` | 哪几个 caption 按钮可见 |
| `SetTitleBarHeight(int)` | **标题栏高，默认 36 px**，最小 18。 典型值：32 偏紧凑（旧默认）、36 平衡（当前默认）、40 配 9-grid 渐变标题栏。 |
| `SetBorderPx(int)` | **resize 抓握区宽度，默认 8 px**（96-dpi 逻辑像素，运行时按 monitor DPI 缩放：125% → 10 物理 px、150% → 12 物理 px）。设 0 = 不允许拖边 resize。 |
| `SetResizable(bool)` | 是否允许拖边 resize |
| `SetMinSize(w, h)` | 最小窗口跟踪尺寸（拖动 resize 缩小到此值后挡住）。默认 200×150。 |
| `SetMaxSize(w, h)` | 最大窗口跟踪尺寸（拖动 resize 放大到此值后挡住）。`w` / `h` = 0 表示不限（默认）。可与 `SetMinSize` 同值实现"固定尺寸但保留 resize 命中"。 |
| `AddCaptionIcon(HBITMAP, tooltip)` | 在标题栏 close 按钮左侧追加一个可点击图标。返回 captionId（>= 1 单调递增）。点击发 `DUIFW_CAPTION_ICON_CLICK` 通知（`extra` = captionId）。tooltip 非空时自动 register 到 `DuiToolTipMgr`。位图所有权 caller 持有（库只存裸指针）。 |
| `RemoveCaptionIcon(captionId)` | 按 captionId 移除单个图标；找不到 id 静默忽略。tooltip 自动 unregister。 |
| `ClearCaptionIcons()` | 清空所有 caption icons。 |
| `SetClientContent(unique_ptr<DuiControl>)` | 客户区根控件 — **不要** 用 SetRoot |
| `SetTitleBarTransparent(bool)` | 透明标题栏（让 9-grid bg 顶部装饰带穿透显示）。配合 `SetTitleTextColor` + `SetCaptionGlyphColor` 改成白字白 glyph 用于深色渐变。详见 [§10 9-grid 背景图](#nine-patch-bg)。 |
| `SetTitleTextColor(COLORREF)` | 标题文字颜色，默认 `RGB(40,40,40)` 深灰。透明标题栏 + 彩色背景下需手动改成对比色（一般白）。 |
| `SetCaptionGlyphColor(COLORREF)` | min/max/close glyph 默认色（rest 状态）。`CLR_INVALID` = 用默认深灰。hover/press 状态下"被颜色 bg 填充"的格子会自动用白 glyph，不受此影响。 |
| `SetClientBorderColor(COLORREF)` | **客户区四周 1px 边框颜色**。继承自 `DuiHost`。`DuiFrameWindow` 构造时默认设为 `RGB(200,200,200)`。 规则：颜色 `CLR_INVALID` → 不画；**设置了 `SetBgImage(...)` → 自动跳过**（9-grid 图自身已表达边界，叠 1px 直线会切方圆角）。 常用值：`RGB(200,200,200)` 默认浅灰，`RGB(180,180,180)` 深一档可强调，`CLR_INVALID` 完全关掉。 |

#### 事件

caption 三按钮（min / max / close）发的 `DUIN_CLICK` 由 `DuiFrameWindow` 内部 **自动转** 成 `WM_SYSCOMMAND`（`SC_MINIMIZE / SC_MAXIMIZE / SC_RESTORE / SC_CLOSE`），业务<u>不需要</u>手动监听。如果想阻止关闭，按常规处理 `WM_CLOSE` 即可。

客户区子控件的事件按常规通过 `WM_DUI_NOTIFY` 路由到 frame 窗自身（即 `m_hWnd`）。

| code | 触发 | extra |
| --- | --- | --- |
| `DUIFW_CAPTION_ICON_CLICK` | 标题栏自定义图标（由 `AddCaptionIcon` 添加）被点击 | captionId（`AddCaptionIcon` 返回值） |

#### 标题栏自定义图标按钮（caption icons）

在标题栏 close 按钮左侧依次插入可点击图标，每个图标占一个标准按钮宽（`kCaptionBtnW` = 46 px），点击发 `DUIFW_CAPTION_ICON_CLICK` 通知。典型用例：VS Code / Chrome 右上角 "设置 / 通知 / 帮助" 风格的标题栏图标按钮。

```
// 业务侧的 frame 子类拦截 caption icon click
class MyFrame : public balloonwjui::DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(MyFrame)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)
        CHAIN_MSG_MAP(balloonwjui::DuiFrameWindow)
    END_MSG_MAP()
private:
    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lp, BOOL& bHandled) {
        auto* n = reinterpret_cast<balloonwjui::DuiNotify*>(lp);
        if (n && n->code == balloonwjui::DuiFrameWindow::DUIFW_CAPTION_ICON_CLICK) {
            int captionId = (int)n->extra;
            // ... 业务处理 ...
            bHandled = TRUE;
            return 0;
        }
        bHandled = FALSE;
        return 0;
    }
};

// 设置
HBITMAP hSettings = LoadCaptionIcon(...);
HBITMAP hBell     = LoadCaptionIcon(...);
int idSettings = frame.AddCaptionIcon(hSettings, _T("Settings"));    // 自动 register tooltip
int idBell     = frame.AddCaptionIcon(hBell,     _T("Notifications"));
// ... 业务 OnDuiNotify 里按 captionId == idSettings/idBell 分支处理
```

HBITMAP 所有权由 caller 持有（控件只存裸指针）。位图源建议至少 16×16，`StretchBlt` HALFTONE 缩到 16×16 居中绘。hover/press 配色随 `SetTitleBarTransparent` 切换：不透明态浅灰 hover（与 min/max 同款）；透明态品牌蓝 hover（不走 close 红变体）。

DuiGallery 演示：FrameWindow tab → "DuiFrameWindow" section 的 demo frame 默认带 3 个 caption icons，点击弹 MessageBox 显示 captionId；"Caption icons — add / remove dynamically" section 演示 `AddCaptionIcon` / `ClearCaptionIcons` 运行期增删。

#### 拖动尺寸限制（min / max）

`SetMinSize(w, h)` / `SetMaxSize(w, h)` 把 ptMinTrackSize / ptMaxTrackSize 喂给 `WM_GETMINMAXINFO`，OS 在拖边 resize 时强制 clamp。`SetMaxSize(0, 0)` = 不限（默认），让 OS 沿用 work area 上限。

- **普通可缩放窗口：**`SetMinSize(360, 240)` + 不调 SetMaxSize 即可。
- **给定上下限：**`SetMinSize(360, 240); SetMaxSize(800, 600);` 拖动只能在 360×240 ~ 800×600 之间。
- **"固定尺寸但保留 resize 命中"：**`SetMinSize(480, 320); SetMaxSize(480, 320);` ── 与 `SetResizable(false)` 区别：本方式仍激活 8 向 resize cursor、只是 OS 不让真正改变尺寸；`SetResizable(false)` 直接关掉 NCHITTEST resize 命中。

DuiGallery 演示：FrameWindow tab → "Min / Max drag size limits" section 三个按钮（默认 / 不限 / 固定）切换后拖窗口边缘验证。

#### 默认视觉对比

同样的客户区内容（`BuildBuddyInfoContent` 一份）以四种配置同时显示 —— 行为模式（上：纯 C++ / 下：XML 驱动），列为样式（左：默认 / 右：9-grid bg）：

![代码 vs XML × 默认 vs 9-grid bg 共 4 窗对比](images/frame-4-windows-2x2.png)

*左列：默认 frame（36px 标题栏 + 1px 浅灰客户区边框 + 浅灰客户区背景）；右列：透明标题栏 + 9-grid bg（边框自动跳过让圆角图当边界）。同列两窗除标题文字外像素级一致。*

**常见调整组合**：

- **"我就用默认"**：什么都不调，得到 36px 标题栏 + 1px 浅灰边框 + 浅灰客户区。适合大多数业务对话框。
- **"想要更紧凑"**：`SetTitleBarHeight(30)`。最小 18，再小标题栏文字会撞到分隔线。
- **"想要更醒目的边"**：`SetClientBorderColor(RGB(150,150,150))`。注意太深会让窗口看起来"重"。
- **"完全不要边"**：`SetClientBorderColor(CLR_INVALID)`。常见于即将叠 bg image 之前。
- **"我有 9-grid bg"**：直接调 `SetBgImage(...)` 即可，边框无需手动关 — 库自动跳过。

#### XML 速查

|   |   |
| --- | --- |
| 标签 | `<frame-window title="..." title-bar-height="40" title-bar-transparent="true" bg-image="..." bg-src-insets="..." bg-dst-insets="..."> <vbox/> <!-- 客户区 --></frame-window>` |
| 详细属性参考 | [§11 DuiFrameWindow 的 XML 配置](#frame-window-xml)（独立大章节，含完整 schema、API、端到端 walkthrough、常见坑） |
| 解析入口 | `DuiXmlBuilder::FromFrameXml(xml, cfg)` 同时返回客户区 root + 填 cfg；落地：`frame.Create(...) → frame.ApplyConfig(cfg) → SetClientContent(...)` |
| 事件 | 三 caption 按钮自动转 `WM_SYSCOMMAND`，业务无需监听；客户区子控件的 WM_DUI_NOTIFY 路由到 frame 自身 m_hWnd。`WM_SIZE` 内部驱动 max → restore glyph 切换。 |

<a id="DuiScrollBar"></a>

### DuiScrollBar / DuiScrollView

![DuiScrollBar 垂直 + 水平](images/ctl-scrollbar-states.png)

*左侧：垂直滚动条；右侧：水平滚动条。thumb 位置随 SetPos 改变。*

独立滚动条 + 内置滚动容器。`DuiScrollView::SetContent(child)` 装内容，`SetContentHeight(h)`（或 `SetAutoContentHeight(true)` 自动量高）告知滚动范围。

**典型父：**`DuiScrollBar` 通常<u>不直接</u>用 —— `DuiScrollView` 内部自动创建并管理。如确需独立用（自定义 view 自管滚动条 / 内置 scrollbar 不能套 DuiScrollView 的场景），典型父：任意 layout 容器。

  `DuiScrollView` 自己的典型父：任意 layout 容器（VBox / HBox / Grid / GroupBox 内容区 / Splitter pane / Dock 子区）。

#### 代码创建

```
auto sv = std::make_unique<balloonwjui::DuiScrollView>();
sv->SetContent(BuildLongVBox());
sv->SetContentHeight(2400);   // 内容总高
sv->SetScrollBarWidth(12);
vbox->AddChild(std::move(sv), balloonwjui::DuiLayout::Hint().Weight(1));
```

#### 事件

| code | 触发 | extra (LPARAM) |
| --- | --- | --- |
| `DUIN_VALUECHANGED` | 用户拖 thumb / 点击轨道 / 滚轮 / 键盘（PgUp/PgDn/Home/End）；`SetPos(_, true)` 程序写入也触发 | 新位置（像素） |

嵌入 `DuiScrollView` 时通常不需要订阅 — 滚动条与内容偏移已联动。仅当独立使用 `DuiScrollBar`（不通过 ScrollView）需要自己处理偏移时才监听。

#### auto-hide / 渐隐

`DuiScrollBar` 内置一套 auto-hide 状态机：默认 alpha=0 不可见但仍占控件位置；caller 主动触发 `TriggerShow()` 后渐入到 alpha=1，800ms 内没新事件就自动渐出回 0。常见用法：list 控件在 OnMouseMove / OnMouseWheel 调 `TriggerShow()`，OnMouseLeave 调 `StartFadeOut()` —— 鼠标在 list 区显示，移开短延迟后隐藏。`DuiListBox` / `DuiVirtualList` 内嵌的 sb 已经默认开启 auto-hide，不需 caller 额外配置。

| 方法 | 含义 |
| --- | --- |
| `SetAutoHide(bool)` | 开启 / 关闭 auto-hide。开启时立即把 alpha 拉到 0；关闭时拉到 1（保持完全可见）。 |
| `IsAutoHide()` | 查询当前是否处于 auto-hide 模式。 |
| `TriggerShow()` | 立即启动 fade-in（200ms 渐入到 1.0）+ 重置 800ms idle 计时器。caller 在 hover / 滚轮事件里调。auto-hide 关闭时是 no-op。 |
| `StartFadeOut()` | 取消 idle 计时器，启动 fade-out（300ms 渐出到 0）。caller 在 OnMouseLeave 里调。auto-hide 关闭时是 no-op。 |
| `SetAlpha(float)` / `GetAlpha()` | 直接设 / 查询 alpha，跳过动画。0 完全透明（OnPaint 不画），1 完全不透明。一般不用，让 fade 函数管理。 |

**挂 60Hz pulse**：auto-hide 的 fade 走 `DuiAnimMgr`，需要 host 所在 frame 在 WM_TIMER 里调 `DuiAnimMgr::Inst().TickAll(GetTickCount())`。XChat 的 `XChatMainFrame` 已挂 16ms timer 当 60Hz pulse，业务沿用即可。如果 caller 自己的 frame 没挂 pulse，scrollbar 会一直停在初始 alpha 不动（既不渐入也不渐出）。

```
// 自定义 list 控件启用 auto-hide：
ctor() {
    auto sb = std::make_unique<balloonwjui::DuiScrollBar>(/*horizontal=*/false);
    sb->SetAutoHide(true);
    m_sb = sb.get();
    AddChild(std::move(sb));
}
bool OnMouseMove(POINT, UINT) override { if (m_sb) m_sb->TriggerShow(); ... }
bool OnMouseLeave()           override { if (m_sb) m_sb->StartFadeOut(); ... }
bool OnMouseWheel(POINT pt, short z, UINT mk) override {
    if (m_sb) {
        m_sb->TriggerShow();
        return m_sb->OnMouseWheel(pt, z, mk);
    }
    return false;
}
```

## 7.8 引擎与工具

<a id="DuiControl"></a>

### DuiControl — 控件基类

所有 DUI 控件的基类。逻辑节点（无 HWND），由 `DuiHost` 托管。继承它即可写自定义控件。

#### 核心虚函数（覆写以自定义行为）

|   |   |
| --- | --- |
| `OnPaint(HDC, RECT)` | 绘制本控件 |
| `Layout(RECT)` | 给定可用 rect 排子节点（默认 = m_rcItem = rcAvail） |
| `HitTest(POINT)` | 命中测试，返回最深可见+启用的子节点 |
| `OnMouseEnter/Leave/Move/Down/Up/DblClk` | 鼠标事件，返回 true 消费 |
| `OnChar / OnKeyDown` | 键盘事件 |
| `OnSetCursor(POINT)` | 返回 true 表示已 SetCursor |

#### 常用调用

|   |   |
| --- | --- |
| `AddChild / RemoveChild` | 子树管理 |
| `SetRect(RECT)` | 设置位置 — 自动 Invalidate + 调 Layout |
| `SetVisible / SetEnabled / SetTabStop` | 状态 |
| `Invalidate()` | 请求重绘自身 rect |
| `Capture / ReleaseCapture / SetFocus` | DUI 内部捕获/焦点（非 Win32） |
| `NotifyParent(code, extra=0)` | 向 HWND 父发 WM_DUI_NOTIFY |

<a id="DuiResMgr"></a>

### DuiResMgr — 资源管理器

单例。包装 `CSkinManager`（图片）+ 进程级默认 UI 字体（Microsoft YaHei 9pt GB2312，惰性创建，DPI 变化时重建）。

```
HFONT f = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
::SelectObject(hdc, f);

// WM_DPICHANGED 时由 DuiHost 自动调用：
balloonwjui::DuiResMgr::Inst().SetDpi(newDpi);

CImageEx* img = balloonwjui::DuiResMgr::Inst().AcquireImage(_T("button_normal.png"));
```

<a id="DuiDpi"></a>

### DuiDpi — 高 DPI 支持

命名空间，三个函数：

|   |   |
| --- | --- |
| `OptInPerMonitorV2()` | WinMain 早期调用一次，启用 per-monitor v2 DPI 感知 |
| `GetSystemDpi()` | 主显示器 DPI（默认 96） |
| `GetWindowDpi(HWND)` | 该窗口当前 DPI（per-monitor v2 感知） |
| `Scale(logical, dpi)` | 逻辑 px → 设备 px |
| `Unscale(device, dpi)` | 设备 px → 逻辑 px |

<a id="DuiPaintAA"></a>

### DuiPaintAA — 抗锯齿绘制助手

![GDI vs DuiPaintAA 对比](images/ctl-paintaa-comparison.png)

*上排：原生 GDI Polygon / Ellipse / LineTo（对角线肉眼可见锯齿）。下排：DuiPaintAA::FillPolygon / FillEllipse / DrawLine（平滑）。*

命名空间，4 个函数。所有非轴对齐图形（圆、对角线、多边形、圆角矩形）必须走这套，否则会有锯齿。

```
POINT tri[3] = { {10, 2}, {18, 14}, {2, 14} };
balloonwjui::DuiAA::FillPolygon  (hdc, tri, 3, RGB(45,108,223));
balloonwjui::DuiAA::FillEllipse  (hdc, RECT{0,0,16,16}, RGB(220,40,40));
balloonwjui::DuiAA::FillRoundRect(hdc, RECT{0,0,80,32}, RGB(45,108,223), /*radius*/8,
                                  /*outline*/RGB(30,74,153), /*outlineWidth*/1.0f);
balloonwjui::DuiAA::DrawLine     (hdc, 0, 0, 100, 50, RGB(0,0,0), 1.5f);
```

GDI+ 在第一次调用时惰性初始化（进程生命期 token）。轴对齐矩形仍用 `::Rectangle / FillRect` 即可（无对角无锯齿）。`::RoundRect` 现在已被 `DuiAA::FillRoundRect` 替代（GDI `::RoundRect` 在 8px 圆角下仍有可见阶梯，`DuiButton` 默认走 AA 路径，调 `SetAntiAlias(false)` 可退回兜底）。`FillRoundRect` 的 `radius` 自动夹紧到 `min(w,h)/2`，超出则退化成胶囊形；`radius<=0` 退化为普通矩形。`fill` / `outline` 任一取 `CLR_INVALID` 表示不填充 / 不描边。

<a id="DuiTheme"></a>

### DuiTheme — 主题颜色 token

![DuiTheme 配色样板](images/ctl-theme-swatches.png)

*左→右：brand / deep / online / away / busy / off。控件默认配色直接读这些常量。*

命名空间常量集合：品牌色、状态色（online/away/red）、墨色 (ink-1..4)、表面色等。控件内部默认使用，亦可被业务直接引用。

<a id="DuiNotify"></a>

### DuiNotify — 事件通知

`WM_DUI_NOTIFY` 的 lParam 是 `DuiNotify*`：

```
struct DuiNotify {
    UINT       code;     // DUIN_CLICK / DUIN_VALUECHANGED / ... 等
    UINT       ctrlId;   // 触发控件的 SetCtrlId
    DuiControl*pCtrl;    // 触发控件指针
    LPARAM     extra;    // 控件特定额外数据
};

enum {
    DUIN_CLICK         = 1,
    DUIN_VALUECHANGED  = 2,
    DUIN_TEXTCHANGED   = 3,
    DUIN_TEXTCOMMIT    = 4,
    DUIN_SELCHANGED    = 5,
    DUIN_HOVER         = 6,
    DUIN_RBUTTONUP     = 7,
    // ... 其它通知码见 DuiNotify.h
};
```

子控件触发：`NotifyParent(DUIN_CLICK, 0);` — 自动经由 `DuiHost::SendNotify` 把消息送给 HWND 父窗口。当宿主自身就是顶层窗口（如 `DuiFrameWindow`），通知自路由到自己的 WM_DUI_NOTIFY 处理函数。

---

<a id="custom-controls"></a>

## 8. 自绘控件

balloonui 的现成控件覆盖了 90% 的常见 UI 需求；剩下 10% — 比如**聊天气泡 / 文件类型徽章 / 进度环 / 业务自定义的彩色卡片** — 设计稿外观特殊、交互单纯，最适合自绘。本章手把手教你写自绘控件，配 4 个独立可运行的 demo（在 `Demos.sln` 里，每个 demo 一个 `.exe`）。

<a id="custom-when"></a>

### 8.1 何时自绘 vs 用现成

| 场景 | 建议 |
| --- | --- |
| 设计稿是品牌特有形态（异形、特殊配色、渐变 …） | **自绘** |
| 需要批量渲染（如聊天列表 1000 条消息） | **自绘**（避免 HWND 句柄爆掉） |
| 动画密度高 / 帧率敏感 | **自绘**（直接控制 OnPaint） |
| 只是普通按钮、列表、tab、复选框 | **用现成**（DuiButton / DuiListBox / DuiTab …） |
| 需要 IME 输入 | **用 DuiEditHost / DuiRichEditHost**（HWND-hosted，自绘搞不定 IME） |

<a id="custom-steps"></a>

### 8.2 写一个自绘 DuiControl 的最小步骤

1. **继承 `DuiControl`**，可选定义属性 + setter。
2. **覆写 `OnPaint(HDC hdc, const RECT& rcDirty)`** — 在 `m_rcItem` 矩形内绘图。`hdc` 是 host 双缓冲的内存 DC（已被父控件清成 COLOR_BTNFACE 或父绘内容）。
3. **每个 setter 内调 `Invalidate()`** 触发重绘 — 但<u>仅当值变化时</u>（避免无谓 paint）。
4. **（可选）覆写鼠标事件** — `OnLButtonDown` / `OnLButtonUp` / `OnMouseMove` 等返回 `true` 表示消费事件。需要触发业务行为时调 `NotifyParent(DUIN_CLICK, extra);`。
5. **（可选）覆写 `HitTest(POINT)`** 当控件外形非矩形时（如圆形 / 异形）告诉 host 哪些坐标算"命中"。默认实现是矩形测试。
6. **（可选）实现 `MeasureHeight(width)` 等测量函数** — 父布局排自适应高度时调用。

骨架长这样：

```
// MyControl.h
#pragma once
#include "DuiControl.h"

class MyControl : public balloonwjui::DuiControl
{
public:
    MyControl() = default;

    void SetText(LPCTSTR sz);
    void SetBgColor(COLORREF c);

    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnLButtonUp(POINT pt, UINT mkFlags) override;

private:
    CString  m_text;
    COLORREF m_bgColor = RGB(45, 108, 223);
};

// MyControl.cpp
void MyControl::SetText(LPCTSTR sz)
{
    CString s = sz ? sz : _T("");
    if (m_text == s) return;          // 值未变 → 不 Invalidate
    m_text = s;
    Invalidate();                     // 触发 host 安排 WM_PAINT
}

void MyControl::OnPaint(HDC hdc, const RECT&)
{
    HBRUSH br = ::CreateSolidBrush(m_bgColor);
    ::FillRect(hdc, &m_rcItem, br);
    ::DeleteObject(br);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, RGB(255, 255, 255));
    HFONT f = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
    HGDIOBJ oldF = ::SelectObject(hdc, f);
    RECT rc = m_rcItem;
    ::DrawText(hdc, m_text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ::SelectObject(hdc, oldF);
}

bool MyControl::OnLButtonUp(POINT, UINT)
{
    NotifyParent(DUIN_CLICK);
    return true;
}
```

**关键约定**：

① `m_rcItem` 是父布局已经算好的本控件矩形（host 客户区坐标），所有绘制都在它内部进行 — 你不需要自己处理坐标偏移。

② 默认字体一律走 `DuiResMgr::Inst().GetDefaultFont()`（Microsoft YaHei 9pt GB2312），不要硬编码 LOGFONT。

③ 非轴向几何（圆 / 三角 / 对角线）必须走 `DuiPaintAA` 或直接 `Gdiplus::Graphics + SetSmoothingMode(AntiAlias)`，否则有锯齿。

④ 颜色用常量化 `static const COLORREF kXxx = RGB(...)`，方便后续抽到 `DuiTheme`。

<a id="custom-xml"></a>

### 8.3 让自绘控件支持 XML 描述 — DuiXmlBuilder + CustomFactory

`DuiXmlBuilder` 把一段 XML 字符串解析成 **DuiControl 树**。它对 7 种内置标签（`vbox` / `hbox` / `grid` / `dock` / `splitter` / `label` / `button` / `edit` 等）有原生支持；遇到不认识的标签就把决定权让给 caller 通过 `CustomFactory` 注册的回调 — 这是把自绘控件接入 XML 的**唯一入口**。

#### 8.3.1 DuiXmlBuilder API 一览

| 方法 | 说明 |
| --- | --- |
| `static unique_ptr<DuiControl> FromString(LPCSTR xmlUtf8, const CustomFactory& = {})` | 主入口。XML 必须 UTF-8 编码（无 BOM 或带 BOM 都行）。返回根控件 / 解析失败时 `nullptr`。 |
| `static unique_ptr<DuiControl> FromString(LPCWSTR xmlW, const CustomFactory& = {})` | 便捷重载：宽串内部转 UTF-8 后调上面那个。 |
| `static bool Parse(LPCSTR xmlUtf8, Node& outRoot)` | 低层 — 只跑 XML 词法/语法分析，不构造控件。当你需要做"先 inspect AST 再决定建不建"的高级场景时用。 |
| `static unique_ptr<DuiControl> Build(const Node& root, const CustomFactory& = {})` | 低层 — 把已有 `Node` AST 构造成控件树。`FromString` = `Parse` + `Build`。 |

#### 8.3.2 Node AST 结构

```
struct DuiXmlBuilder::Node
{
    std::string                         tag;       // 小写标签名，如 "vbox" / "my-tile"
    std::map<std::string, std::string>  attrs;    // 属性键值对（值是原始 UTF-8 字符串）
    std::vector<Node>                   children; // 子节点（递归）
};
```

关键事实：**属性值始终是 UTF-8 字节序列**（即使 XML 源是 UTF-16）。中文 / 日文 / emoji 都安全；但要给 `CString`（Unicode build 是宽串）时必须 `CA2W(s.c_str(), CP_UTF8)` 转码。

#### 8.3.3 CustomFactory 签名与调用契约

```
using CustomFactory =
    std::function<std::unique_ptr<DuiControl>(const Node&)>;
```

| 契约 | 说明 |
| --- | --- |
| **调用时机** | builder 遇到一个标签时，先查内置（vbox/hbox/...）；不在内置列表 → **调你的 factory**。 |
| **返回 `nullptr`** | 表示"我也不识别" — builder 跳过该节点（连同它的子节点）。 |
| **返回非空 `unique_ptr`** | builder 把它当本节点的产物。<u>之后会自动 `Build()` 每个 `Node.children`，并 `AddChild()` 到这个返回值上</u> — 也就是说自绘控件可以包含子控件，而不需要 factory 内部手动递归解析。 |
| **通用属性** | `id` / `fixedWidth` / `fixedHeight` / `weight` / `margin` 由 builder 在父容器 `AddChild` 时统一读取（写到 `DuiLayout::Hint`）。**factory 不要自己解析这几个**，否则会和 builder 冲突。 |
| **线程** | 始终在 caller 调 `FromString` 的那个线程同步执行。 |
| **异常** | 从 factory 抛异常会穿过 builder。生产代码里建议 factory 只 try-noexcept，解析失败就返回 `nullptr`。 |

#### 8.3.4 标签命名约定

避免和未来的内置标签撞名，建议自定义标签：

- 用 **kebab-case**（带连字符），如 `chat-bubble` / `file-icon` / `my-search-input`。内置标签都是单词，无连字符 — 撞名概率低。
- 属性也用 kebab-case 或 lowerCamelCase（保持文档一致）。
- tag 比较时用**区分大小写的精确比较**（builder 内部已 lowercase 化，所以你 factory 收到的 `n.tag` 始终是小写）。

#### 8.3.5 推荐写法 — factory 模板

```
// === 1) 标签设计 ===
//   <my-tile text="OK" bg-color="45,108,223" radius="6"/>
//
// === 2) factory ===
static std::unique_ptr<balloonwjui::DuiControl>
MyTileFactory(const balloonwjui::DuiXmlBuilder::Node& n)
{
    // 不是我管的标签 → 立即让出
    if (n.tag != "my-tile")
    {
        return nullptr;
    }

    auto t = std::unique_ptr<MyTile>(new MyTile());

    // 取属性的小工具（找不到返回 nullptr）
    auto attr = [&](const char* k) -> const std::string* {
        auto it = n.attrs.find(k);
        return (it != n.attrs.end()) ? &it->second : nullptr;
    };

    // 字符串属性 — UTF-8 → CString
    if (auto* s = attr("text"))
    {
        t->SetText(CString(CA2W(s->c_str(), CP_UTF8)));
    }

    // 数值属性
    if (auto* s = attr("radius"))
    {
        t->SetCornerRadius(atoi(s->c_str()));
    }

    // RGB 三元组 "r,g,b"
    if (auto* s = attr("bg-color"))
    {
        int r, g, b;
        if (sscanf(s->c_str(), "%d,%d,%d", &r, &g, &b) == 3)
        {
            t->SetBgColor(RGB(r, g, b));
        }
    }

    // bool 属性 — 推荐接受 "true" / "1" 都算真
    if (auto* s = attr("disabled"))
    {
        bool b = (*s == "true" || *s == "1" || *s == "yes");
        t->SetEnabled(!b);
    }

    return t;
}
```

#### 8.3.6 常用属性解析助手（建议抽到工具头）

5 类属性几乎覆盖所有自绘控件的需要 — 抽成 helper 减少 factory 里的重复样板：

```
namespace XmlAttr {

inline const std::string*
Find(const balloonwjui::DuiXmlBuilder::Node& n, const char* key)
{
    auto it = n.attrs.find(key);
    return (it != n.attrs.end()) ? &it->second : nullptr;
}

inline CString GetCString(const balloonwjui::DuiXmlBuilder::Node& n,
                          const char* key,
                          LPCTSTR defaultValue = _T(""))
{
    if (auto* s = Find(n, key))
    {
        return CString(CA2W(s->c_str(), CP_UTF8));
    }
    return defaultValue;
}

inline int GetInt(const balloonwjui::DuiXmlBuilder::Node& n,
                  const char* key, int defaultValue = 0)
{
    if (auto* s = Find(n, key))
    {
        return atoi(s->c_str());
    }
    return defaultValue;
}

inline bool GetBool(const balloonwjui::DuiXmlBuilder::Node& n,
                    const char* key, bool defaultValue = false)
{
    if (auto* s = Find(n, key))
    {
        return (*s == "true" || *s == "1" || *s == "yes");
    }
    return defaultValue;
}

inline COLORREF GetColor(const balloonwjui::DuiXmlBuilder::Node& n,
                         const char* key, COLORREF defaultValue = 0)
{
    if (auto* s = Find(n, key))
    {
        int r, g, b;
        if (sscanf(s->c_str(), "%d,%d,%d", &r, &g, &b) == 3)
        {
            return RGB(r, g, b);
        }
    }
    return defaultValue;
}

} // namespace XmlAttr

// === factory 因此变得很短 ===
static std::unique_ptr<balloonwjui::DuiControl>
MyTileFactory(const balloonwjui::DuiXmlBuilder::Node& n)
{
    if (n.tag != "my-tile") return nullptr;
    auto t = std::unique_ptr<MyTile>(new MyTile());
    t->SetText        (XmlAttr::GetCString(n, "text"));
    t->SetBgColor     (XmlAttr::GetColor  (n, "bg-color", RGB(45,108,223)));
    t->SetCornerRadius(XmlAttr::GetInt    (n, "radius",   6));
    return t;
}
```

#### 8.3.7 注册多个工厂 — 链式 / 组合模式

`FromString` 只接 1 个 `CustomFactory`。要支持多套自绘控件，把它们包成一个"分发 factory"，按 tag 路由：

```
auto fac = [](const balloonwjui::DuiXmlBuilder::Node& n)
    -> std::unique_ptr<balloonwjui::DuiControl>
{
    // 按 tag 派发到具体工厂
    if (n.tag == "my-tile")     return MyTileFactory(n);
    if (n.tag == "chat-bubble") return ChatBubbleFactory(n);
    if (n.tag == "file-icon")   return FileTypeIconFactory(n);
    return nullptr;             // 其它未知 tag → builder 跳过
};

auto root = balloonwjui::DuiXmlBuilder::FromString(xmlUtf8, fac);
```

NewChatDemo / 后续业务里会把所有业务控件的 factory 集中在一个 `MakeChatFactory()` 这种返回 `CustomFactory` 的工厂方法里 — 单文件单一来源，便于维护。

#### 8.3.8 子节点自动递归 — 自绘容器

因为 builder 会对你 factory 返回的控件自动 `AddChild()` 子节点，自绘控件天然可以做**容器**。常见用法：

```
// 标签设计：<chat-list weight="1"> ... 子节点是若干消息项 ... </chat-list>
class MyChatList : public balloonwjui::DuiControl
{
public:
    void Layout(const RECT& rcAvail) override
    {
        // 自定义排子节点 — 例如垂直堆叠 + 自动量高
        m_rcItem = rcAvail;
        int y = rcAvail.top;
        for (auto& child : m_children)
        {
            int h = child->GetDesiredSize().cy;     // 或自定义 MeasureHeight
            RECT r = { rcAvail.left, y,
                       rcAvail.right, y + h };
            child->SetRect(r);
            y += h + 6;
        }
    }
};

static auto ChatListFactory =
    [](const balloonwjui::DuiXmlBuilder::Node& n)
    -> std::unique_ptr<balloonwjui::DuiControl>
{
    if (n.tag != "chat-list") return nullptr;
    return std::unique_ptr<MyChatList>(new MyChatList());
    // 子节点（每个 <chat-bubble/> / <system-msg/> 等）由 builder 自动 AddChild
    // 进我们的 MyChatList，Layout() 自动接管排版
};
```

**关键**：`Layout(rcAvail)` 是 `DuiControl` 提供的虚函数 — 父容器在自身 layout 阶段调用每个子的 `Layout()`，子可以覆写它来自己排子节点位置。这就是 "自绘布局" 的本质。

#### 8.3.9 与内置标签混排

自绘控件可以放进任何内置容器：

```
const char* kXml =
    "<vbox padding=\"16\" gap=\"8\">"
    "  <label text=\"操作面板\" fixedHeight=\"24\"/>"
    "  <hbox gap=\"8\" fixedHeight=\"36\">"
    "    <my-tile text=\"开始\" bg-color=\"45,108,223\" weight=\"1\"/>"
    "    <my-tile text=\"暂停\" bg-color=\"220,170,60\"  weight=\"1\"/>"
    "    <my-tile text=\"停止\" bg-color=\"220,60,60\"   weight=\"1\"/>"
    "  </hbox>"
    "  <my-chat-list weight=\"1\"/>"
    "</vbox>";

auto fac = [](auto& n) -> std::unique_ptr<balloonwjui::DuiControl> {
    if (n.tag == "my-tile")      return MyTileFactory(n);
    if (n.tag == "my-chat-list") return MyChatListFactory(n);
    return nullptr;
};
auto root = balloonwjui::DuiXmlBuilder::FromString(kXml, fac);
host.SetRoot(std::move(root));
```

#### 8.3.10 常见坑

| 症状 | 原因 / 解决 |
| --- | --- |
| FromString 返回 nullptr | ① XML 语法错（标签未闭合 / 引号不配对）。② root 标签是未识别的且 factory 没认。先用 `Parse(xml, &node)` 跑一遍确认 AST 正常。 |
| 中文文本变乱码 | 属性值是 UTF-8 字节，`CString(s.c_str())` 在 Unicode build 走的是 ANSI→Wide 转换，会乱。必须用 `CString(CA2W(s.c_str(), CP_UTF8))`。 |
| fixedWidth / weight 没生效 | ① 父不是 vbox/hbox/grid 这类布局容器；② factory 自己解析了 fixedWidth 但没设到任何地方 — 不要碰这几个属性，让 builder 处理。 |
| 子节点没 AddChild 过来 | factory 返回了 `nullptr`。返回 nullptr 后 builder 不会再下钻该节点的 children。 |
| 子节点 AddChild 顺序错乱 | builder 是按 XML 出现顺序顺次 AddChild 的，不会重排。如果你的 Layout 依赖另一种顺序，要么改 XML 顺序，要么自己维护一份 index→child 映射。 |
| 同一份 XML 复用多次失败 | FromString 每次都新建一棵控件树，是安全的。但如果你缓存了某个 `DuiControl*` 然后 `SetRoot` 第二棵树，旧指针失效（树被销毁）。 |

<a id="custom-examples"></a>

### 8.4 4 个完整示例

以下 4 个示例都是 `Demos.sln` 里独立可运行的工程：

| 工程 | 演示重点 | 源代码 |
| --- | --- | --- |
| `DemoTextBadgeTile.exe` | 最简自绘 — 圆角矩形 + 居中文字 | `flamingoclient/DemoTextBadgeTile/` |
| `DemoCircularProgress.exe` | GDI+ 抗锯齿圆环 + clamp setter | `flamingoclient/DemoCircularProgress/` |
| `DemoChatBubble.exe` | 异形（带尾巴）+ MeasureHeight 测高 + 左右对齐 | `flamingoclient/DemoChatBubble/` |
| `DemoFileTypeIcon.exe` | 数据驱动配色 + 折角 + hover 反馈 | `flamingoclient/DemoFileTypeIcon/` |

每个 demo 都同时演示 **代码方式** 与 **XML 方式** 两条创建路径，并自带 `--capture-all <dir>` 截图模式（本节里的 PNG 都是从这些 demo 真截出来的）。

<a id="ex-textbadge"></a>

### 示例 #1 — DemoTextBadgeTile

圆角彩色矩形 + 居中文字。最简单的自绘示例，覆盖：构造 setter、`OnPaint` 用 GDI `RoundRect` + `DrawText`、`OnLButtonUp` 通过 `NotifyParent` 抛 click。

![默认](images/demo-textbadge-default.png)

*默认（品牌蓝）*

![成功](images/demo-textbadge-success.png)

*SetBgColor=绿*

![警告](images/demo-textbadge-warning.png)

*SetBgColor=琥珀*

![危险](images/demo-textbadge-danger.png)

*SetBgColor=红*

![代码方式 4 状态](images/demo-textbadge-code-row.png)

*代码方式整行：4 个不同色板 tile 横向并排*

![XML 方式 3 变体](images/demo-textbadge-xml-row.png)

*XML 方式：`<text-badge text=\"...\" bgColor=\"r,g,b\" radius=\"N\"/>` 描述，3 个不同 radius / 配色*

#### 关键代码片段

```
// DemoTextBadgeTile.h
class DemoTextBadgeTile : public balloonwjui::DuiControl
{
public:
    void SetText(LPCTSTR sz);
    void SetBgColor(COLORREF c);
    void SetFgColor(COLORREF c);
    void SetCornerRadius(int r);
    void OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool OnLButtonUp(POINT pt, UINT mkFlags) override;
private:
    CString  m_text;
    COLORREF m_bgColor = RGB( 45, 108, 223);
    COLORREF m_fgColor = RGB(255, 255, 255);
    int      m_radius  = 6;
};

// OnPaint 主体（DemoTextBadgeTile.cpp）
void DemoTextBadgeTile::OnPaint(HDC hdc, const RECT&)
{
    // 1) 圆角矩形背景：用 NULL_PEN 不画边框，仅填 brush
    HBRUSH brBg  = ::CreateSolidBrush(m_bgColor);
    HGDIOBJ oldBr = ::SelectObject(hdc, brBg);
    HGDIOBJ oldPn = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
    ::RoundRect(hdc, m_rcItem.left, m_rcItem.top,
                m_rcItem.right, m_rcItem.bottom,
                m_radius * 2, m_radius * 2);
    ::SelectObject(hdc, oldBr);
    ::SelectObject(hdc, oldPn);
    ::DeleteObject(brBg);

    // 2) 居中文字 — 用库默认字体
    if (m_text.IsEmpty()) return;
    HFONT hFont = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
    HGDIOBJ oldFont = ::SelectObject(hdc, hFont);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, m_fgColor);
    RECT rc = m_rcItem;
    ::DrawText(hdc, m_text, -1, &rc,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ::SelectObject(hdc, oldFont);
    // 字体由 DuiResMgr 管理 — 不要 DeleteObject
}
```

#### XML 方式

```
<text-badge text="Save"   bgColor="45,108,223" radius="6"  fixedWidth="110"/>
<text-badge text="Cancel" bgColor="180,180,180" radius="2" fixedWidth="110"/>
```

<details class="full-code">
  <summary>展开完整代码（.h + .cpp + main.cpp 的 XML 工厂）</summary>
  <pre><code>// 完整源代码见 flamingoclient/DemoTextBadgeTile/
//   stdafx.h               — ATL/WTL 通用前向
//   DemoTextBadgeTile.h    — 类声明
//   DemoTextBadgeTile.cpp  — setter + OnPaint + OnLButtonUp
//   CaptureMode.h/.cpp     — --capture-all 模式（mark 注册 + 离屏截 PNG）
//   main.cpp               — 创建 DuiFrameWindow + 多状态布局 +
//                            CustomFactory + CLI 解析

// === DemoTextBadgeTile.cpp（完整 setter 群）===
void DemoTextBadgeTile::SetText(LPCTSTR sz) {
    CString s = sz ? sz : _T(""); if (m_text == s) return;
    m_text = s; Invalidate();
}
void DemoTextBadgeTile::SetBgColor(COLORREF c) {
    if (m_bgColor == c) return; m_bgColor = c; Invalidate();
}
void DemoTextBadgeTile::SetFgColor(COLORREF c) {
    if (m_fgColor == c) return; m_fgColor = c; Invalidate();
}
void DemoTextBadgeTile::SetCornerRadius(int r) {
    if (r &lt; 0) r = 0; if (m_radius == r) return;
    m_radius = r; Invalidate();
}

// === main.cpp 的 XML 工厂 ===
static std::unique_ptr&lt;DuiControl&gt;
TextBadgeFactory(const DuiXmlBuilder::Node&amp; n) {
    if (n.tag != "text-badge") return nullptr;
    auto t = std::unique_ptr&lt;DemoTextBadgeTile&gt;(new DemoTextBadgeTile());
    auto get = [&amp;](const char* k) -&gt; const std::string* {
        auto it = n.attrs.find(k);
        return it == n.attrs.end() ? nullptr : &amp;it-&gt;second;
    };
    if (auto* s = get("text"))    t-&gt;SetText(CString(CA2W(s-&gt;c_str(), CP_UTF8)));
    if (auto* s = get("bgColor")) { int r,g,b;
        if (sscanf(s-&gt;c_str(),"%d,%d,%d",&amp;r,&amp;g,&amp;b)==3) t-&gt;SetBgColor(RGB(r,g,b)); }
    if (auto* s = get("fgColor")) { int r,g,b;
        if (sscanf(s-&gt;c_str(),"%d,%d,%d",&amp;r,&amp;g,&amp;b)==3) t-&gt;SetFgColor(RGB(r,g,b)); }
    if (auto* s = get("radius"))  t-&gt;SetCornerRadius(atoi(s-&gt;c_str()));
    return t;
}
</code></pre>
</details>

<a id="ex-circprog"></a>

### 示例 #2 — DemoCircularProgress

圆形进度环，0..100%。新增的学习点：用 `Gdiplus::Graphics` 直接绘画 + `SetSmoothingMode(AntiAlias)`，覆盖 `DuiPaintAA` 没暴露的 Arc 操作；`SetPercent` 演示 clamp + 仅在变化时 Invalidate。

![0%](images/demo-circprog-p0.png)

*0%*

![33%](images/demo-circprog-p33.png)

*33%*

![66%](images/demo-circprog-p66.png)

*66%*

![100%](images/demo-circprog-p100.png)

*100%*

![代码 4 进度](images/demo-circprog-code-row.png)

*代码方式：4 种进度（0/33/66/100%）*

![XML 4 配色](images/demo-circprog-xml-row.png)

*XML 方式：自定义 fillColor + thickness + showText*

#### 关键代码片段

```
// OnPaint 主体（DemoCircularProgress.cpp）
void DemoCircularProgress::OnPaint(HDC hdc, const RECT&)
{
    int w = m_rcItem.right - m_rcItem.left;
    int h = m_rcItem.bottom - m_rcItem.top;
    int diameter = (w < h ? w : h) - 2;
    int cx = (m_rcItem.left + m_rcItem.right) / 2;
    int cy = (m_rcItem.top + m_rcItem.bottom) / 2;

    float halfPen = m_thickness / 2.0f;
    float left = cx - diameter / 2.0f + halfPen;
    float top  = cy - diameter / 2.0f + halfPen;
    float side = diameter - m_thickness;

    // GDI+ 启 AA → 弧无锯齿
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // 1) 灰色完整轨道
    Gdiplus::Pen trackPen(Gdiplus::Color(255,
        GetRValue(m_trackColor), GetGValue(m_trackColor),
        GetBValue(m_trackColor)),
        (Gdiplus::REAL)m_thickness);
    g.DrawEllipse(&trackPen, left, top, side, side);

    // 2) 进度填充弧 — 从 12 点钟方向顺时针
    if (m_percent > 0) {
        Gdiplus::Pen fillPen(Gdiplus::Color(255,
            GetRValue(m_fillColor), GetGValue(m_fillColor),
            GetBValue(m_fillColor)),
            (Gdiplus::REAL)m_thickness);
        fillPen.SetStartCap(Gdiplus::LineCapRound);
        fillPen.SetEndCap  (Gdiplus::LineCapRound);
        Gdiplus::REAL sweep = 3.6f * (Gdiplus::REAL)m_percent;
        g.DrawArc(&fillPen, left, top, side, side, -90.0f, sweep);
    }

    // 3) 中心 "%" 文字（GDI 走默认字体）
    if (m_showText) {
        HFONT f = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
        HGDIOBJ oldF = ::SelectObject(hdc, f);
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, m_textColor);
        CString s; s.Format(_T("%d%%"), m_percent);
        RECT rc = m_rcItem;
        ::DrawText(hdc, s, -1, &rc,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ::SelectObject(hdc, oldF);
    }
}
```

#### XML 方式

```
<circular-progress percent="75" fillColor="50,160,110"  fixedWidth="80"/>
<circular-progress percent="50" fillColor="220,170,60" thickness="10" fixedWidth="80"/>
<circular-progress percent="90" fillColor="100,80,200" showText="false" fixedWidth="80"/>
```

<details class="full-code">
  <summary>展开完整代码 setter 群（DemoCircularProgress.cpp）</summary>
  <pre><code>// 全部 setter 都遵循同一个模式：边界检查 → 值未变直接返回 → 否则更新 + Invalidate
void DemoCircularProgress::SetPercent(int p) {
    if (p &lt; 0) p = 0; if (p &gt; 100) p = 100;
    if (m_percent == p) return;
    m_percent = p; Invalidate();
}
void DemoCircularProgress::SetThickness(int t) {
    if (t &lt; 0) t = 0; if (m_thickness == t) return;
    m_thickness = t; Invalidate();
}
void DemoCircularProgress::SetTrackColor(COLORREF c) {
    if (m_trackColor == c) return; m_trackColor = c; Invalidate();
}
void DemoCircularProgress::SetFillColor(COLORREF c) {
    if (m_fillColor == c) return; m_fillColor = c; Invalidate();
}
// SetTextColor / SetShowText 同理
</code></pre>
</details>

<a id="ex-chatbubble"></a>

### 示例 #3 — DemoChatBubble

聊天气泡：圆角矩形 + 三角尾巴（异形）+ 自动换行 + 时间戳。新增的学习点：**异形自绘**（GDI `Polygon` 画三角）+ **测高函数**（`MeasureHeight(width)`，给定宽度返回所需高度）+ **左右对齐切换**（属性变化引起几何反转）。

![对方短消息](images/demo-chatbubble-in-short.png)

*side=in，短文本 + ts*

![我方长消息](images/demo-chatbubble-out-long.png)

*side=out，长文本换行 + ts*

![对方无时间戳](images/demo-chatbubble-in-noTs.png)

*side=in，无 ts*

![我方极短](images/demo-chatbubble-out-tiny.png)

*side=out，极短文本*

![XML 一对气泡](images/demo-chatbubble-xml-pair.png)

*XML 方式：`<chat-bubble side=\"in|out\" text=\"...\" ts=\"15:00\"/>` 一对*

#### 关键代码片段

```
// PaintBubble: 画气泡身（圆角矩形）+ 尾巴（三角），返回内容矩形
RECT DemoChatBubble::PaintBubble(HDC hdc) const
{
    // 气泡矩形：扣掉一侧的尾巴宽
    RECT bubbleRc = m_rcItem;
    if (m_side == SideIn)  bubbleRc.left  += kTailWidth;
    else                   bubbleRc.right -= kTailWidth;

    // 配色：对方白底，我方品牌绿
    COLORREF bg     = (m_side == SideOut) ? RGB( 90, 200, 150) : RGB(255, 255, 255);
    COLORREF border = (m_side == SideOut) ? RGB( 60, 170, 120) : RGB(220, 222, 226);

    // 画气泡身
    HBRUSH brBg = ::CreateSolidBrush(bg);
    HPEN   pen  = ::CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldB = ::SelectObject(hdc, brBg);
    HGDIOBJ oldP = ::SelectObject(hdc, pen);
    ::RoundRect(hdc, bubbleRc.left, bubbleRc.top,
                bubbleRc.right, bubbleRc.bottom,
                kRadius * 2, kRadius * 2);

    // 画尾巴：根据 side 决定指向
    POINT tri[3];
    if (m_side == SideIn) {
        tri[0] = { bubbleRc.left,                bubbleRc.top + kTailOffsetY };
        tri[1] = { bubbleRc.left - kTailWidth,   bubbleRc.top + kTailOffsetY + kTailHeight / 2 };
        tri[2] = { bubbleRc.left,                bubbleRc.top + kTailOffsetY + kTailHeight };
    } else {
        tri[0] = { bubbleRc.right,               bubbleRc.top + kTailOffsetY };
        tri[1] = { bubbleRc.right + kTailWidth,  bubbleRc.top + kTailOffsetY + kTailHeight / 2 };
        tri[2] = { bubbleRc.right,               bubbleRc.top + kTailOffsetY + kTailHeight };
    }
    ::Polygon(hdc, tri, 3);

    ::SelectObject(hdc, oldB);
    ::SelectObject(hdc, oldP);
    ::DeleteObject(brBg);
    ::DeleteObject(pen);

    RECT inner = bubbleRc;
    ::InflateRect(&inner, -kPadX, -kPadY);
    return inner;
}

// MeasureHeight: 给定可用宽度，算文字 WORDBREAK 后的总占用高度
int DemoChatBubble::MeasureHeight(int availWidth) const
{
    int textW = availWidth - kTailWidth - kPadX * 2;
    if (textW < 1) textW = 1;

    HDC hdc = ::GetDC(NULL);
    HFONT f = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
    HGDIOBJ oldF = ::SelectObject(hdc, f);
    RECT rc = { 0, 0, textW, 1 };
    ::DrawText(hdc, m_text, -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_LEFT);
    int textH = rc.bottom - rc.top;
    ::SelectObject(hdc, oldF);
    ::ReleaseDC(NULL, hdc);

    int total = textH + kPadY * 2;
    if (!m_ts.IsEmpty()) total += kMetaGap + kMetaH;
    int minH = kTailOffsetY + kTailHeight + kPadY;
    return total > minH ? total : minH;
}
```

#### XML 方式

```
<vbox gap="6">
  <chat-bubble side="in"  text="from XML — incoming" ts="15:00" fixedHeight="50"/>
  <chat-bubble side="out" text="from XML — outgoing" ts="15:01" fixedHeight="50"/>
</vbox>
```

**注意**：XML 描述里气泡高度需要预先指定（`fixedHeight`）— 因为 builder 不知道文字换行后的真实高度。代码方式可以先算 `MeasureHeight(width)` 再 `AddChild(_, Hint().Fixed(h))`，更精确。聊天列表场景里通常用代码方式。

<details class="full-code">
  <summary>展开完整 OnPaint（含文字 + 时间戳 + 配色）</summary>
  <pre><code>void DemoChatBubble::OnPaint(HDC hdc, const RECT&amp;)
{
    RECT inner = PaintBubble(hdc);

    COLORREF textColor = (m_side == SideOut)
        ? RGB( 20,  60,  40)
        : RGB( 30,  30,  30);

    HFONT f = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
    HGDIOBJ oldF = ::SelectObject(hdc, f);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, textColor);

    // 主文字区：扣掉时间戳行
    RECT textRc = inner;
    if (!m_ts.IsEmpty()) textRc.bottom -= (kMetaGap + kMetaH);
    ::DrawText(hdc, m_text, -1, &amp;textRc,
               DT_LEFT | DT_TOP | DT_WORDBREAK);

    // 时间戳：右下角灰小字
    if (!m_ts.IsEmpty()) {
        ::SetTextColor(hdc, RGB(140, 140, 140));
        RECT metaRc = { inner.left, inner.bottom - kMetaH,
                        inner.right, inner.bottom };
        ::DrawText(hdc, m_ts, -1, &amp;metaRc,
                   DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);
    }
    ::SelectObject(hdc, oldF);
}
</code></pre>
</details>

<a id="ex-fileicon"></a>

### 示例 #4 — DemoFileTypeIcon

文件后缀彩色徽章（PDF / DOC / MP4 / ZIP …）。新增的学习点：**数据驱动配色**（ext → 三色板查表）+ **右上角折角**（Polygon 画三角阴影）+ **hover 视觉反馈**（OnPaint 直接读 `IsHover()`）。

![PDF](images/demo-fileicon-pdf.png)

*pdf*

![DOC](images/demo-fileicon-doc.png)

*doc*

![XLS](images/demo-fileicon-xls.png)

*xls*

![PPT](images/demo-fileicon-ppt.png)

*ppt*

![MP4](images/demo-fileicon-mp4.png)

*mp4*

![PNG](images/demo-fileicon-png.png)

*png*

![ZIP](images/demo-fileicon-zip.png)

*zip*

![未知](images/demo-fileicon-unknown.png)

*unknown → 灰*

![代码 8 类型](images/demo-fileicon-code-row.png)

*代码方式 — 8 种后缀并排*

![XML 4 类型](images/demo-fileicon-xml-row.png)

*XML 方式 — 设计软件类后缀（fig/psd/sketch）+ 自定义 label + radius*

#### 关键代码片段

```
// 单一来源色板表（DemoFileTypeIcon.cpp）
struct PaletteEntry { LPCTSTR ext; COLORREF bg; COLORREF fg; COLORREF fold; };
const PaletteEntry kPalettes[] = {
    { _T("pdf"),  RGB(232,  90,  90), RGB(255, 255, 255), RGB(180,  60,  60) },
    { _T("doc"),  RGB( 60, 130, 220), RGB(255, 255, 255), RGB( 30, 100, 180) },
    { _T("xls"),  RGB( 50, 160, 110), RGB(255, 255, 255), RGB( 30, 130,  90) },
    { _T("mp4"),  RGB(120,  80, 200), RGB(255, 255, 255), RGB( 90,  60, 170) },
    // ... 见源文件
};

DemoFileTypeIcon::Palette DemoFileTypeIcon::LookupPalette(LPCTSTR ext)
{
    if (!ext || !*ext) return kDefault;
    CString needle = ext; needle.MakeLower();
    for (auto& e : kPalettes)
        if (needle == e.ext) return { e.bg, e.fg, e.fold };
    return kDefault;
}

// OnPaint：圆角背景 + 折角三角 + hover 提亮 + 文字
void DemoFileTypeIcon::OnPaint(HDC hdc, const RECT&)
{
    Palette p = LookupPalette(m_ext);

    // hover 时把所有色拉浅一档（IsHover() 由 host 在 mouse-enter/leave 自动维护）
    if (IsHover()) {
        auto lift = [](COLORREF c) {
            int r = GetRValue(c) + 20, g = GetGValue(c) + 20, b = GetBValue(c) + 20;
            if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
            return RGB(r, g, b);
        };
        p.bg = lift(p.bg); p.fold = lift(p.fold);
    }

    // 1) 圆角背景
    HBRUSH brBg = ::CreateSolidBrush(p.bg);
    HGDIOBJ oldBr = ::SelectObject(hdc, brBg);
    HGDIOBJ oldPn = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
    ::RoundRect(hdc, m_rcItem.left, m_rcItem.top,
                m_rcItem.right, m_rcItem.bottom,
                m_radius * 2, m_radius * 2);
    ::SelectObject(hdc, oldBr); ::SelectObject(hdc, oldPn);
    ::DeleteObject(brBg);

    // 2) 右上角折角三角
    int sz = (m_rcItem.right - m_rcItem.left) / 4;
    if (sz > 14) sz = 14;
    if (sz >= 6) {
        POINT tri[3] = {
            { m_rcItem.right - sz, m_rcItem.top },
            { m_rcItem.right,      m_rcItem.top + sz },
            { m_rcItem.right,      m_rcItem.top },
        };
        HBRUSH brFold = ::CreateSolidBrush(p.fold);
        HGDIOBJ ob = ::SelectObject(hdc, brFold);
        HGDIOBJ op = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
        ::Polygon(hdc, tri, 3);
        ::SelectObject(hdc, ob); ::SelectObject(hdc, op);
        ::DeleteObject(brFold);
    }

    // 3) 居中文字
    if (!m_label.IsEmpty()) {
        HFONT f = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
        HGDIOBJ oldF = ::SelectObject(hdc, f);
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, p.fg);
        RECT rc = m_rcItem;
        ::DrawText(hdc, m_label, -1, &rc,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ::SelectObject(hdc, oldF);
    }
}
```

#### XML 方式

```
<file-icon ext="fig"    fixedWidth="48"/>
<file-icon ext="sketch" label="SK" fixedWidth="48"/>       <!-- 自定义 label 覆盖默认 -->
<file-icon ext="mp3"    radius="14" fixedWidth="48"/>     <!-- 圆角更大 → 视觉接近圆形 -->
```

<details class="full-code">
  <summary>展开 setter 与查表逻辑</summary>
  <pre><code>void DemoFileTypeIcon::SetExt(LPCTSTR ext) {
    CString s = ext ? ext : _T("");
    s.MakeLower();
    if (m_ext == s) return;
    m_ext = s;
    // 自动同步 label 为大写（业务可在之后再 SetLabel 覆盖）
    m_label = s; m_label.MakeUpper();
    Invalidate();
}

void DemoFileTypeIcon::SetLabel(LPCTSTR t) {
    CString s = t ? t : _T("");
    if (m_label == s) return;
    m_label = s; Invalidate();
}

bool DemoFileTypeIcon::OnLButtonUp(POINT, UINT) {
    NotifyParent(DUIN_CLICK);   // 父对话框处理"打开文件"
    return true;
}

// 静态助手：暴露给 tests 和文档；ext 大小写不敏感；nullptr/"" → kDefault
struct Palette { COLORREF bg; COLORREF fg; COLORREF fold; };
static Palette LookupPalette(LPCTSTR ext);
</code></pre>
</details>

<a id="custom-build"></a>

### 8.5 编译运行

4 个 demo 都在 `flamingoclient/Demos.sln` 里。命令行编译：

```
msbuild Demos.sln /p:Configuration=Debug /p:Platform=Win32 ^
        /t:DemoTextBadgeTile;DemoCircularProgress;DemoChatBubble;DemoFileTypeIcon

# 交互运行（看效果）
Bin\DemoTextBadgeTile.exe
Bin\DemoCircularProgress.exe
Bin\DemoChatBubble.exe
Bin\DemoFileTypeIcon.exe

# 截图模式（重新生成本节用图）
Bin\DemoTextBadgeTile.exe   --capture-all flamingoclient\docs\images
Bin\DemoCircularProgress.exe --capture-all flamingoclient\docs\images
Bin\DemoChatBubble.exe      --capture-all flamingoclient\docs\images
Bin\DemoFileTypeIcon.exe    --capture-all flamingoclient\docs\images
```

---

<a id="layouts"></a>

## 9. 完整布局示例

本章给 5 个常见 UI 布局的完整 demo — **每个都是 balloonui 真控件渲染**（不是 mock），并附等价 XML 描述。截图来自 DuiGallery 的 `Layouts` tab，运行 `DuiGallery.exe --capture-all flamingoclient\docs\images` 可重新生成。

本章的目标是**"看完就会拼一个真窗口"** — 重点演示 `DuiVBox/HBox/Dock/Splitter` 的 Hint 用法（`Fixed`/`Weight`）+ 现成控件（`DuiLabel`/`DuiEditHost`/`DuiButton`/`DuiListBox`/`DuiSearchBox`/`DuiAvatar`/`DuiComboBox`）的组合方式。

<a id="layout-skeleton-app"></a>

### 9.0 通用程序结构（5 个示例共用脚手架）

5 个布局示例只是<u>客户区控件树</u>不同；外面的"创建 frame、显示窗口、跑消息循环、接事件"流程都一样。本节给完整可拷贝的 WinMain 模板，把 frame 创建 / 客户区装载 / EDIT 子 HWND 创建 / 事件接入 / 退出清理一次写全，下面 9.1–9.5 只列各自的 `BuildXxxRoot()` 函数 + 接事件片段，主程序结构都引这里。

#### 9.0.1 项目设置

- **解决方案 / 工程**：`flamingoclient/Demos.sln` 里加一个 Application 工程（参 `DemoNinePatchBg.vcxproj` 模板），`OutDir = ..\Bin\`。
- **预处理器**：`WIN32;_WINDOWS;BUI_USE_DLL;_CRT_SECURE_NO_WARNINGS`（`BUI_USE_DLL` 让 `BUI_API` 解析为 `__declspec(dllimport)`，对接 balloonui.dll）。
- **包含目录**：`..\balloonui;..\Source\wtl9.0`
- **链接库**：`balloonui.lib;gdiplus.lib;comctl32.lib`（`AdditionalLibraryDirectories=..\Bin`）
- **子系统**：`Windows`（`SubSystem=Windows`，入口 `WinMain`）
- **UTF-8 源**：`AdditionalOptions=/utf-8 /FS`（让中文 _T(...) 字面量编码一致）
- **字符集**：`Unicode`（`UNICODE` + `_UNICODE` 自动定义）

#### 9.0.2 完整 WinMain（脚手架）

```
// main.cpp — 9.X 任意一个布局都用这份骨架

#include "stdafx.h"
#include "DuiDpi.h"
#include "DuiHost.h"
#include "DuiNotify.h"
#include "Controls/DuiFrameWindow.h"
#include "Controls/DuiLayout.h"
#include "Controls/DuiLabel.h"
#include "Controls/DuiButton.h"
#include "Controls/DuiEditHost.h"
// ...按本示例需要 include 其它控件头...

using namespace balloonwjui;

CAppModule _Module;     // WTL 全局；CWindowImpl 等需要

// === 1) 占位：本示例对应的"建客户区树"函数。
//     9.1 是 BuildLoginRoot()，9.2 是 BuildFormRoot()，依此类推。
//     函数体在下面 9.X 各自小节给。
extern std::unique_ptr<DuiControl> BuildLoginRoot();

// === 2) 顶层窗口子类，挂消息映射 + 事件分发。
class MainFrame : public DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(MainFrame)
        MSG_WM_DESTROY(OnDestroy)              // 关窗时退出消息循环
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify)
        CHAIN_MSG_MAP(DuiFrameWindow)
    END_MSG_MAP()

    // WM_DESTROY：单 frame 进程关窗 = 退出。
    void OnDestroy() { ::PostQuitMessage(0); }

    // 子控件事件入口 —— 业务在这里按 ctrlId 派发到具体处理函数。
    // 见 9.X.3 "事件处理"小节给的具体处理代码。
    LRESULT OnDuiNotify(UINT, WPARAM, LPARAM lp, BOOL& bHandled)
    {
        DuiNotify* n = (DuiNotify*)lp;
        // 例：登录按钮 IDC_LOGIN 的处理（9.1.3 给）：
        //   if (n->ctrlId == IDC_LOGIN && n->code == DUIN_CLICK)
        //       OnLoginClicked();
        bHandled = FALSE;     // 没匹配的事件让 DuiFrameWindow 默认处理（caption 按钮等）
        return 0;
    }
};

// === 3) WinMain：进程入口。一次性做 DPI 感知、OLE 启动、frame 创建、
//     客户区装载、HWND-hosted 控件 EnsureCreated、显示、消息循环。
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR, int nCmdShow)
{
    // a) Per-Monitor V2 DPI 感知 —— 必须在任何 HWND 创建之前。
    DuiDpi::OptInPerMonitorV2();

    // b) OLE / COM 初始化（OLE 拖拽、RichEdit、剪贴板都需要）。
    ::OleInitialize(NULL);

    // c) WTL ATL 公共控件 + 全局 module。
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);

    // d) 顶层 frame。MainFrame 是 DuiFrameWindow 子类。
    MainFrame frame;
    frame.SetButtons(true, true, true);    // min / max / close 都显示
    frame.SetMinSize(480, 320);            // 最小窗口
    frame.SetResizable(true);

    // 实际创建 HWND（这一行之前任何 SetTitle 都不会显示在任务栏，
    // 因为 m_hWnd 还没建立 —— 见 §7.7 DuiFrameWindow 的 SetTitle 注释）。
    if (!frame.Create(NULL, CWindow::rcDefault, _T("MyApp"),
                      WS_OVERLAPPEDWINDOW, 0))
    {
        _Module.Term();
        ::OleUninitialize();
        return 1;
    }
    frame.SetTitle(_T("登录"));            // 必须 Create 之后才设标题

    // e) 装载客户区控件树（本示例对应的 BuildXxxRoot()）。
    auto root = BuildLoginRoot();
    frame.SetClientContent(std::move(root));

    // f) EnsureCreated —— 如果客户区有 DuiEditHost / DuiRichEditHost /
    //    DuiSearchBox / DuiSpinBox，必须在 host HWND 就绪后调一次创建
    //    它们的内部 EDIT 子 HWND。详见 §3.4。
    //    简单遍历法：拿 root，按 id / 类型找出来一个个调。
    auto* clientRoot = frame.GetClientContent();
    if (auto* edit = (DuiEditHost*)clientRoot->FindControlById(100 /*用户名*/))
    {
        edit->EnsureCreated(frame.m_hWnd);
    }
    if (auto* edit = (DuiEditHost*)clientRoot->FindControlById(101 /*密码*/))
    {
        edit->EnsureCreated(frame.m_hWnd);
    }

    // g) 显示窗口 + 消息循环。
    frame.ResizeClient(800, 600);          // 客户区目标尺寸
    frame.CenterWindow();                  // 屏幕居中
    frame.ShowWindow(nCmdShow);
    frame.UpdateWindow();

    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    // h) 退出清理。frame 在栈上，析构时如果 HWND 还在会自动 DestroyWindow。
    _Module.Term();
    ::OleUninitialize();
    return (int)msg.wParam;
}
```

#### 9.0.3 构建 + 运行

```
# 在 VS 2022 里：
# 1. 打开 flamingoclient\Demos.sln
# 2. 把新工程加进去（或克隆 DemoNinePatchBg.vcxproj 改名）
# 3. 选 Debug | Win32 → 生成解决方案
# 4. 跑 Bin\MyApp.exe

# 命令行（在 flamingoclient\ 下）：
"%MSBuild%" Demos.sln /t:MyApp /p:Configuration=Debug /p:Platform=Win32
.\Bin\MyApp.exe
```

**下面 9.1–9.5 各示例只列<u>本示例独有</u>的内容**：客户区树构造函数 `BuildXxxRoot()`、对应 XML、事件处理片段。WinMain 不重复贴。

<a id="layout-login"></a>

### 9.1 登录对话框

典型"垂直居中卡片"模式：外层 HBox 用 `Weight(1) | Fixed(W) | Weight(1)` 实现水平居中，内层 VBox 自上而下排版。

![登录布局](images/ctl-layout-login.png)

*Logo + 标题 + 副标题 + 用户名/密码 + 复选+链接 + 登录按钮 + 版本号*

#### 代码方式

```
using namespace balloonwjui;

// === 内层卡片 ===
auto card = std::make_unique<DuiVBox>();
card->SetPadding(20);
card->SetGap(10);

// Logo（这里用占位 — 真业务用 DuiAvatar 或 DuiAsyncImage）
auto logo = std::make_unique<LogoTile>(_T("F"), RGB(50, 160, 110));
card->AddChild(std::move(logo), DuiLayout::Hint().Fixed(48));

// 标题 + 副标题
auto title = std::make_unique<DuiLabel>();
title->SetText(_T("FlamingoNewUI"));
title->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
card->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

auto sub = std::make_unique<DuiLabel>();
sub->SetText(_T("登录账号"));
sub->SetTextColor(RGB(120, 120, 120));
sub->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
card->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

// 用户名 / 密码
auto eUser = std::make_unique<DuiEditHost>();
eUser->SetPlaceholder(_T("用户名 / 邮箱"));
card->AddChild(std::move(eUser), DuiLayout::Hint().Fixed(32));

auto ePwd = std::make_unique<DuiEditHost>();
ePwd->SetPlaceholder(_T("密码"));
ePwd->SetPassword(true);
card->AddChild(std::move(ePwd), DuiLayout::Hint().Fixed(32));

// 一行 "记住我" + "忘记密码" 链接
auto opts = std::make_unique<DuiHBox>();
auto cb = std::make_unique<DuiButton>();
cb->SetButtonType(DuiButton::StyleCheckbox);
cb->SetText(_T("记住我"));
auto link = std::make_unique<DuiLabel>();
link->SetText(_T("忘记密码？"));
link->SetMode(DuiLabel::ModeLink);
link->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
opts->AddChild(std::move(cb),   DuiLayout::Hint().Weight(1));
opts->AddChild(std::move(link), DuiLayout::Hint().Fixed(80));
card->AddChild(std::move(opts), DuiLayout::Hint().Fixed(24));

// 登录按钮
auto bLogin = std::make_unique<DuiButton>();
bLogin->SetCtrlId(IDC_LOGIN);   // 父 OnDuiNotify 收 DUIN_CLICK
bLogin->SetText(_T("登录"));
card->AddChild(std::move(bLogin), DuiLayout::Hint().Fixed(36));

// === 外层水平居中 ===
auto root = std::make_unique<DuiHBox>();
root->AddChild(std::make_unique<DuiControl>(),
               DuiLayout::Hint().Weight(1));    // 左占位
root->AddChild(std::move(card), DuiLayout::Hint().Fixed(320));
root->AddChild(std::make_unique<DuiControl>(),
               DuiLayout::Hint().Weight(1));    // 右占位

frame.SetClientContent(std::move(root));
```

#### XML 方式

```
<hbox>
  <control weight="1"/>          <!-- 左占位实现居中 -->
  <vbox padding="20" gap="10" fixedWidth="320">
    <logo  fixedHeight="48"/>     <!-- 业务自绘标签 -->
    <label text="FlamingoNewUI" textAlign="center" fixedHeight="28"/>
    <label text="登录账号" textColor="120,120,120" textAlign="center" fixedHeight="20"/>
    <edit  id="100" placeholder="用户名 / 邮箱" fixedHeight="32"/>
    <edit  id="101" placeholder="密码" password="true" fixedHeight="32"/>
    <hbox fixedHeight="24">
      <button id="102" buttonType="checkbox" text="记住我" weight="1"/>
      <label  id="103" text="忘记密码？" mode="link"
              textAlign="right" fixedWidth="80"/>
    </hbox>
    <button id="104" text="登录" fixedHeight="36"/>
  </vbox>
  <control weight="1"/>
</hbox>
```

#### 9.1.1 完整 Build 函数

把上面的 C++ 代码包装成一个返回 `unique_ptr<DuiControl>` 的函数，给 [§9.0](#layout-skeleton-app) 的 `WinMain` 调用：

```
// 控件 id（与 OnDuiNotify 派发对应）
enum { IDC_USER = 100, IDC_PWD = 101, IDC_REMEMBER = 102,
       IDC_FORGOT = 103, IDC_LOGIN = 104 };

std::unique_ptr<DuiControl> BuildLoginRoot()
{
    using namespace balloonwjui;

    // 内层卡片（VBox）
    auto card = std::make_unique<DuiVBox>();
    card->SetPadding(20);
    card->SetGap(10);

    auto logo = std::make_unique<LogoTile>(_T("F"), RGB(50, 160, 110));
    card->AddChild(std::move(logo), DuiLayout::Hint().Fixed(48));

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("FlamingoNewUI"));
    title->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    card->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    auto sub = std::make_unique<DuiLabel>();
    sub->SetText(_T("登录账号"));
    sub->SetTextColor(RGB(120, 120, 120));
    sub->SetTextAlign(DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    card->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    auto eUser = std::make_unique<DuiEditHost>();
    eUser->SetCtrlId(IDC_USER);
    eUser->SetPlaceholder(_T("用户名 / 邮箱"));
    card->AddChild(std::move(eUser), DuiLayout::Hint().Fixed(32));

    auto ePwd = std::make_unique<DuiEditHost>();
    ePwd->SetCtrlId(IDC_PWD);
    ePwd->SetPlaceholder(_T("密码"));
    ePwd->SetPassword(true);
    card->AddChild(std::move(ePwd), DuiLayout::Hint().Fixed(32));

    auto opts = std::make_unique<DuiHBox>();
    auto cb = std::make_unique<DuiButton>();
    cb->SetCtrlId(IDC_REMEMBER);
    cb->SetButtonType(DuiButton::StyleCheckbox);
    cb->SetText(_T("记住我"));
    auto link = std::make_unique<DuiLabel>();
    link->SetCtrlId(IDC_FORGOT);
    link->SetText(_T("忘记密码？"));
    link->SetMode(DuiLabel::ModeLink);
    link->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    opts->AddChild(std::move(cb),   DuiLayout::Hint().Weight(1));
    opts->AddChild(std::move(link), DuiLayout::Hint().Fixed(80));
    card->AddChild(std::move(opts), DuiLayout::Hint().Fixed(24));

    auto bLogin = std::make_unique<DuiButton>();
    bLogin->SetCtrlId(IDC_LOGIN);
    bLogin->SetText(_T("登录"));
    card->AddChild(std::move(bLogin), DuiLayout::Hint().Fixed(36));

    // 外层水平居中 (HBox)
    auto root = std::make_unique<DuiHBox>();
    root->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(card), DuiLayout::Hint().Fixed(320));
    root->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Weight(1));
    return root;
}
```

#### 9.1.2 事件处理

在 [§9.0](#layout-skeleton-app) 的 `MainFrame::OnDuiNotify` 里加：

```
LRESULT MainFrame::OnDuiNotify(UINT, WPARAM, LPARAM lp, BOOL& bHandled)
{
    DuiNotify* n = (DuiNotify*)lp;
    bHandled = TRUE;

    switch (n->ctrlId)
    {
    case IDC_LOGIN:
        if (n->code == DUIN_CLICK)
        {
            // 拿用户名 / 密码 EDIT 当前值
            auto* user = (DuiEditHost*)GetClientContent()->FindControlById(IDC_USER);
            auto* pwd  = (DuiEditHost*)GetClientContent()->FindControlById(IDC_PWD);
            DoLogin(user->GetText(), pwd->GetText());
        }
        return 0;

    case IDC_FORGOT:
        if (n->code == DUIN_CLICK)
        {
            ShowForgotPasswordDialog();
        }
        return 0;

    case IDC_REMEMBER:
        if (n->code == DUIN_VALUECHANGED)
        {
            g_settings.SetBool(_T("RememberMe"), n->extra != 0);
        }
        return 0;
    }
    bHandled = FALSE;
    return 0;
}
```

#### 9.1.3 构建 + 运行

WinMain 直接用 [§9.0.2](#layout-skeleton-app) 的脚手架，`extern` 声明里换成 `BuildLoginRoot()`；EnsureCreated 段已经覆盖 IDC_USER / IDC_PWD 两个 EDIT。`frame.SetTitle(_T("登录"))` + `ResizeClient(800, 600)` 让卡片真正居中显示。生成 / 运行同 §9.0.3。

<a id="layout-form"></a>

### 9.2 表单

"标签右对齐 + 编辑框弹性" 多行模式 + 底部按钮组（弹性占位推到右）。

![表单布局](images/ctl-layout-form.png)

*4 个字段 + 取消 / 保存按钮*

#### 代码方式

```
// helper: 一行 "label + edit"
auto makeRow = [](LPCTSTR labelText, LPCTSTR placeholder) {
    auto row = std::make_unique<DuiHBox>();
    row->SetGap(8);
    auto l = std::make_unique<DuiLabel>();
    l->SetText(labelText);
    l->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    auto e = std::make_unique<DuiEditHost>();
    if (placeholder) e->SetPlaceholder(placeholder);
    row->AddChild(std::move(l), DuiLayout::Hint().Fixed(80));   // 标签固定宽
    row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));   // 编辑框弹性
    return row;
};

auto col = std::make_unique<DuiVBox>();
col->SetPadding(20);
col->SetGap(12);

auto title = std::make_unique<DuiLabel>();
title->SetText(_T("个人资料"));
col->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

col->AddChild(makeRow(_T("姓名"), _T("请输入姓名")),
              DuiLayout::Hint().Fixed(28));
col->AddChild(makeRow(_T("昵称"), _T("可选")),
              DuiLayout::Hint().Fixed(28));
col->AddChild(makeRow(_T("邮箱"), _T("name@example.com")),
              DuiLayout::Hint().Fixed(28));
col->AddChild(makeRow(_T("电话"), nullptr),
              DuiLayout::Hint().Fixed(28));

// 弹性占位 — 把按钮推到底部
col->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Weight(1));

// 底部按钮组：左占位 weight=1 把按钮推到右
auto buttons = std::make_unique<DuiHBox>();
buttons->SetGap(8);
buttons->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Weight(1));
auto bCancel = std::make_unique<DuiButton>();
bCancel->SetButtonType(DuiButton::StyleIcon);
bCancel->SetText(_T("取消"));
auto bOk = std::make_unique<DuiButton>();
bOk->SetText(_T("保存"));
buttons->AddChild(std::move(bCancel), DuiLayout::Hint().Fixed(80));
buttons->AddChild(std::move(bOk),     DuiLayout::Hint().Fixed(80));
col->AddChild(std::move(buttons), DuiLayout::Hint().Fixed(32));
```

#### XML 方式

```
<vbox padding="20" gap="12">
  <label text="个人资料" fixedHeight="28"/>

  <hbox fixedHeight="28" gap="8">
    <label text="姓名" textAlign="right" fixedWidth="80"/>
    <edit id="100" placeholder="请输入姓名" weight="1"/>
  </hbox>
  <hbox fixedHeight="28" gap="8">
    <label text="昵称" textAlign="right" fixedWidth="80"/>
    <edit id="101" placeholder="可选" weight="1"/>
  </hbox>
  <hbox fixedHeight="28" gap="8">
    <label text="邮箱" textAlign="right" fixedWidth="80"/>
    <edit id="102" placeholder="name@example.com" weight="1"/>
  </hbox>
  <hbox fixedHeight="28" gap="8">
    <label text="电话" textAlign="right" fixedWidth="80"/>
    <edit id="103" weight="1"/>
  </hbox>

  <control weight="1"/>            <!-- 弹性占位推按钮到底 -->

  <hbox fixedHeight="32" gap="8">
    <control weight="1"/>         <!-- 弹性占位推按钮到右 -->
    <button id="200" buttonType="icon" text="取消" fixedWidth="80"/>
    <button id="201" text="保存" fixedWidth="80"/>
  </hbox>
</vbox>
```

**核心模式**：`Weight(1)` 的 `DuiControl` 占位 = "把后面的子控件推到容器末尾"。这是 balloonui 实现"右对齐 / 底部对齐 / 居中"的统一手段，比写 absolute position 干净得多。

#### 9.2.1 完整 Build 函数

```
enum { IDC_NAME = 100, IDC_NICK = 101, IDC_EMAIL = 102, IDC_PHONE = 103,
       IDC_CANCEL = 200, IDC_SAVE = 201 };

std::unique_ptr<DuiControl> BuildFormRoot()
{
    using namespace balloonwjui;

    auto makeRow = [](LPCTSTR labelText, LPCTSTR placeholder, UINT editId) {
        auto row = std::make_unique<DuiHBox>();
        row->SetGap(8);
        auto l = std::make_unique<DuiLabel>();
        l->SetText(labelText);
        l->SetTextAlign(DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        auto e = std::make_unique<DuiEditHost>();
        e->SetCtrlId(editId);
        if (placeholder) e->SetPlaceholder(placeholder);
        row->AddChild(std::move(l), DuiLayout::Hint().Fixed(80));
        row->AddChild(std::move(e), DuiLayout::Hint().Weight(1));
        return row;
    };

    auto col = std::make_unique<DuiVBox>();
    col->SetPadding(20);
    col->SetGap(12);

    auto title = std::make_unique<DuiLabel>();
    title->SetText(_T("个人资料"));
    col->AddChild(std::move(title), DuiLayout::Hint().Fixed(28));

    col->AddChild(makeRow(_T("姓名"), _T("请输入姓名"), IDC_NAME),  DuiLayout::Hint().Fixed(28));
    col->AddChild(makeRow(_T("昵称"), _T("可选"),       IDC_NICK),  DuiLayout::Hint().Fixed(28));
    col->AddChild(makeRow(_T("邮箱"), _T("name@example.com"), IDC_EMAIL), DuiLayout::Hint().Fixed(28));
    col->AddChild(makeRow(_T("电话"), nullptr,          IDC_PHONE), DuiLayout::Hint().Fixed(28));

    col->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Weight(1));

    auto buttons = std::make_unique<DuiHBox>();
    buttons->SetGap(8);
    buttons->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Weight(1));
    auto bCancel = std::make_unique<DuiButton>();
    bCancel->SetCtrlId(IDC_CANCEL);
    bCancel->SetButtonType(DuiButton::StyleIcon);
    bCancel->SetText(_T("取消"));
    auto bOk = std::make_unique<DuiButton>();
    bOk->SetCtrlId(IDC_SAVE);
    bOk->SetText(_T("保存"));
    buttons->AddChild(std::move(bCancel), DuiLayout::Hint().Fixed(80));
    buttons->AddChild(std::move(bOk),     DuiLayout::Hint().Fixed(80));
    col->AddChild(std::move(buttons), DuiLayout::Hint().Fixed(32));
    return col;
}
```

#### 9.2.2 事件处理

```
// MainFrame::OnDuiNotify 派发：
case IDC_SAVE:
    if (n->code == DUIN_CLICK)
    {
        auto* root = GetClientContent();
        m_model.name  = ((DuiEditHost*)root->FindControlById(IDC_NAME ))->GetText();
        m_model.nick  = ((DuiEditHost*)root->FindControlById(IDC_NICK ))->GetText();
        m_model.email = ((DuiEditHost*)root->FindControlById(IDC_EMAIL))->GetText();
        m_model.phone = ((DuiEditHost*)root->FindControlById(IDC_PHONE))->GetText();
        SaveProfile(m_model);
        ::PostMessage(m_hWnd, WM_CLOSE, 0, 0);
    }
    return 0;
case IDC_CANCEL:
    if (n->code == DUIN_CLICK) ::PostMessage(m_hWnd, WM_CLOSE, 0, 0);
    return 0;

// 实时校验邮箱（任一 EDIT 输入触发）：
case IDC_EMAIL:
    if (n->code == DUIN_VALUECHANGED)
    {
        auto* edit = (DuiEditHost*)GetClientContent()->FindControlById(IDC_EMAIL);
        bool valid = ValidateEmail(edit->GetText());
        ((DuiButton*)GetClientContent()->FindControlById(IDC_SAVE))->SetEnabled(valid);
    }
    return 0;
```

#### 9.2.3 构建 + 运行

WinMain 用 [§9.0.2](#layout-skeleton-app) 脚手架；`extern` 改 `BuildFormRoot()`，EnsureCreated 段把 4 个 EDIT id（IDC_NAME / NICK / EMAIL / PHONE）都 Create 一次。`SetTitle(_T("个人资料"))`。生成命令同 §9.0.3。

<a id="layout-three-pane"></a>

### 9.3 三列主窗

桌面 IM 经典布局：60px 左导航条 + 240px 中列表 + 弹性右内容。某信、Slack、Discord 都是这个三段式。

![三列主窗布局](images/ctl-layout-three-pane.png)

*左 60px Avatar + 图标导航 / 中 240px DuiSearchBox + DuiListBox / 右 flex 内容*

#### 代码方式

```
auto root = std::make_unique<DuiHBox>();

// === 左：60px 导航条 ===
auto rail = std::make_unique<DuiVBox>();
rail->SetPadding(8);
rail->SetGap(8);

auto avatar = std::make_unique<DuiAvatar>();
avatar->SetName(_T("陆"));
avatar->SetFallbackBgColor(RGB(50, 160, 110));
rail->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(40));

for (int i = 0; i < 4; ++i) {
    // 业务里这是 DuiButton(StyleIcon) 或自绘的导航项
    auto btn = std::make_unique<DuiButton>();
    btn->SetButtonType(DuiButton::StyleIcon);
    btn->SetCtrlId(IDC_RAIL_BASE + i);
    rail->AddChild(std::move(btn), DuiLayout::Hint().Fixed(36));
}
root->AddChild(std::move(rail), DuiLayout::Hint().Fixed(60));

// === 中：240px 搜索 + 列表 ===
auto mid = std::make_unique<DuiVBox>();
auto search = std::make_unique<DuiSearchBox>();
search->SetCtrlId(IDC_SEARCH);
search->SetPlaceholder(_T("搜索"));
mid->AddChild(std::move(search), DuiLayout::Hint().Fixed(36));

auto list = std::make_unique<DuiListBox>();
list->SetCtrlId(IDC_SESSION_LIST);
list->SetItemHeight(48);
list->AddItem(_T("苏文嘉"));
list->AddItem(_T("# 设计周会"));
list->AddItem(_T("李静"));
list->SetCurSel(0, false);
mid->AddChild(std::move(list), DuiLayout::Hint().Weight(1));
root->AddChild(std::move(mid), DuiLayout::Hint().Fixed(240));

// === 右：flex 内容（标题栏 + 内容 + 输入栏）===
auto right = std::make_unique<DuiVBox>();
right->AddChild(BuildHeaderBar(),  DuiLayout::Hint().Fixed(48));
right->AddChild(BuildChatThread(), DuiLayout::Hint().Weight(1));
right->AddChild(BuildComposer(),   DuiLayout::Hint().Fixed(80));
root->AddChild(std::move(right), DuiLayout::Hint().Weight(1));
```

#### XML 方式

```
<hbox>
  <!-- 左 rail -->
  <vbox fixedWidth="60" padding="8" gap="8">
    <avatar name="陆" fallbackColor="50,160,110" fixedHeight="40"/>
    <button id="200" buttonType="icon" fixedHeight="36"/>
    <button id="201" buttonType="icon" fixedHeight="36"/>
    <button id="202" buttonType="icon" fixedHeight="36"/>
    <button id="203" buttonType="icon" fixedHeight="36"/>
    <control weight="1"/>
  </vbox>

  <!-- 中 list -->
  <vbox fixedWidth="240">
    <search-box id="100" placeholder="搜索" fixedHeight="36"/>
    <listbox    id="101" itemHeight="48" weight="1"/>
  </vbox>

  <!-- 右 content -->
  <vbox weight="1">
    <header-bar  fixedHeight="48"/>       <!-- 业务自绘 -->
    <chat-thread weight="1"/>
    <composer    fixedHeight="80"/>
  </vbox>
</hbox>
```

**升级路径**：当用户需要"拖动调整中列表宽度"时，把中右两段包进 `DuiSplitter`（`orientation=horizontal`），`SetRatio` 给初始比例 — 一行替换。

#### 9.3.1 完整 Build 函数

```
enum { IDC_RAIL_BASE = 200, IDC_SEARCH = 100, IDC_SESSION_LIST = 101 };

std::unique_ptr<DuiControl> BuildThreePaneRoot()
{
    using namespace balloonwjui;
    auto root = std::make_unique<DuiHBox>();

    // 左：60px 导航条
    auto rail = std::make_unique<DuiVBox>();
    rail->SetPadding(8); rail->SetGap(8);
    auto avatar = std::make_unique<DuiAvatar>();
    avatar->SetName(_T("陆"));
    avatar->SetFallbackBgColor(RGB(50, 160, 110));
    rail->AddChild(std::move(avatar), DuiLayout::Hint().Fixed(40));
    for (int i = 0; i < 4; ++i)
    {
        auto btn = std::make_unique<DuiButton>();
        btn->SetButtonType(DuiButton::StyleIcon);
        btn->SetCtrlId(IDC_RAIL_BASE + i);
        rail->AddChild(std::move(btn), DuiLayout::Hint().Fixed(36));
    }
    rail->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(rail), DuiLayout::Hint().Fixed(60));

    // 中：240px 搜索 + 列表
    auto mid = std::make_unique<DuiVBox>();
    auto search = std::make_unique<DuiSearchBox>();
    search->SetCtrlId(IDC_SEARCH);
    search->SetPlaceholder(_T("搜索"));
    mid->AddChild(std::move(search), DuiLayout::Hint().Fixed(36));

    auto list = std::make_unique<DuiListBox>();
    list->SetCtrlId(IDC_SESSION_LIST);
    list->SetItemHeight(48);
    list->AddItem(_T("苏文嘉"));
    list->AddItem(_T("# 设计周会"));
    list->AddItem(_T("李静"));
    list->SetCurSel(0, false);
    mid->AddChild(std::move(list), DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(mid), DuiLayout::Hint().Fixed(240));

    // 右：flex 内容（业务侧自家 BuildHeaderBar / Thread / Composer）
    auto right = std::make_unique<DuiVBox>();
    right->AddChild(BuildHeaderBar(),  DuiLayout::Hint().Fixed(48));
    right->AddChild(BuildChatThread(), DuiLayout::Hint().Weight(1));
    right->AddChild(BuildComposer(),   DuiLayout::Hint().Fixed(80));
    root->AddChild(std::move(right), DuiLayout::Hint().Weight(1));
    return root;
}
```

#### 9.3.2 事件处理

```
// MainFrame::OnDuiNotify 派发：
case IDC_SEARCH:
    if (n->code == DUIN_VALUECHANGED)
    {
        auto* sb = (DuiSearchBox*)GetClientContent()->FindControlById(IDC_SEARCH);
        FilterContacts(sb->GetText());     // 业务方法：按 q 过滤会话列表
    }
    return 0;

case IDC_SESSION_LIST:
    if (n->code == DUIN_VALUECHANGED)
    {
        // n->extra 是新选中行的 index；用 LPARAM 取业务 sessionId
        auto* lb = (DuiListBox*)GetClientContent()->FindControlById(IDC_SESSION_LIST);
        LPARAM sessionId = lb->GetItemParam((int)n->extra);
        SwitchToSession((int)sessionId);
    }
    return 0;

// 4 个导航 rail 按钮：IDC_RAIL_BASE + 0..3
default:
    if (n->ctrlId >= IDC_RAIL_BASE && n->ctrlId < IDC_RAIL_BASE + 4
        && n->code == DUIN_CLICK)
    {
        SwitchTab(n->ctrlId - IDC_RAIL_BASE);    // 0=会话 / 1=联系人 / 2=收藏 / 3=设置
    }
    return 0;
```

#### 9.3.3 构建 + 运行

WinMain 用 [§9.0.2](#layout-skeleton-app)；`extern` 改 `BuildThreePaneRoot()`。EnsureCreated 段补一行 `FindControlById(IDC_SEARCH)->EnsureCreated(frame.m_hWnd)`（SearchBox 内嵌一个 EDIT）。`SetTitle(_T("Flamingo IM"))` + `ResizeClient(1100, 720)` 给三栏足够空间。

<a id="layout-settings"></a>

### 9.4 设置页

左侧 vertical nav（用 `DuiListBox` 实现，比 `DuiTab` 更适合长列表）+ 右侧"标签 + 控件"配置项。

![设置页布局](images/ctl-layout-settings.png)

*8 项左导航 + 通用页：开机自启 / 静默运行 / 关闭主窗口时*

#### 代码方式

```
auto root = std::make_unique<DuiHBox>();

// === 左：导航列表 ===
auto nav = std::make_unique<DuiListBox>();
nav->SetCtrlId(IDC_SETTINGS_NAV);
nav->SetItemHeight(32);
nav->AddItem(_T("通用"));
nav->AddItem(_T("账号与安全"));
nav->AddItem(_T("消息与通知"));
nav->AddItem(_T("隐私"));
nav->AddItem(_T("聊天偏好"));
nav->AddItem(_T("音频与视频"));
nav->AddItem(_T("文件与存储"));
nav->AddItem(_T("外观"));
nav->SetCurSel(0, false);
root->AddChild(std::move(nav), DuiLayout::Hint().Fixed(160));

// === 右：vbox 内容 ===
auto content = std::make_unique<DuiVBox>();
content->SetPadding(20);
content->SetGap(8);

auto h = std::make_unique<DuiLabel>();
h->SetText(_T("通用"));
content->AddChild(std::move(h), DuiLayout::Hint().Fixed(28));

auto sub = std::make_unique<DuiLabel>();
sub->SetText(_T("启动行为"));
sub->SetTextColor(RGB(120, 120, 120));
content->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

// helper: "label + 右侧控件" 一行
auto makeOption = [](LPCTSTR label, std::unique_ptr<DuiControl> right) {
    auto row = std::make_unique<DuiHBox>();
    auto l = std::make_unique<DuiLabel>();
    l->SetText(label);
    row->AddChild(std::move(l), DuiLayout::Hint().Weight(1));
    row->AddChild(std::move(right), DuiLayout::Hint().Fixed(120));
    return row;
};

auto cb1 = std::make_unique<DuiButton>();
cb1->SetButtonType(DuiButton::StyleCheckbox);
cb1->SetText(_T("启用"));
cb1->SetCheck(true, false);
content->AddChild(makeOption(_T("开机自动启动"), std::move(cb1)),
                  DuiLayout::Hint().Fixed(28));

auto combo = std::make_unique<DuiComboBox>();
combo->AddString(_T("最小化到托盘"));
combo->AddString(_T("退出程序"));
combo->SetCurSel(0, false);
content->AddChild(makeOption(_T("关闭主窗口时"), std::move(combo)),
                  DuiLayout::Hint().Fixed(28));

content->AddChild(std::make_unique<DuiControl>(),
                  DuiLayout::Hint().Weight(1));     // 内容自适应填充

root->AddChild(std::move(content), DuiLayout::Hint().Weight(1));
```

#### XML 方式

```
<hbox>
  <!-- 左 nav -->
  <listbox id="100" fixedWidth="160" itemHeight="32">
    <!-- 注：listbox 项目目前需要代码 AddItem，XML 不支持
         <item> 子节点 — 这是后续可补的功能 -->
  </listbox>

  <!-- 右 content -->
  <vbox weight="1" padding="20" gap="8">
    <label text="通用" fixedHeight="28"/>
    <label text="启动行为" textColor="120,120,120" fixedHeight="20"/>

    <hbox fixedHeight="28">
      <label text="开机自动启动" weight="1"/>
      <button id="200" buttonType="checkbox" text="启用" fixedWidth="120"/>
    </hbox>
    <hbox fixedHeight="28">
      <label text="关闭主窗口时" weight="1"/>
      <combo-box id="201" fixedWidth="120"/>
    </hbox>

    <control weight="1"/>
  </vbox>
</hbox>
```

#### 9.4.1 完整 Build 函数

```
enum { IDC_SETTINGS_NAV = 100, IDC_AUTOSTART = 200, IDC_CLOSE_BEHAVIOR = 201 };

std::unique_ptr<DuiControl> BuildSettingsRoot()
{
    using namespace balloonwjui;
    auto root = std::make_unique<DuiHBox>();

    // 左：导航 ListBox
    auto nav = std::make_unique<DuiListBox>();
    nav->SetCtrlId(IDC_SETTINGS_NAV);
    nav->SetItemHeight(32);
    nav->AddItem(_T("通用"));
    nav->AddItem(_T("账号与安全"));
    nav->AddItem(_T("消息与通知"));
    nav->AddItem(_T("隐私"));
    nav->AddItem(_T("聊天偏好"));
    nav->AddItem(_T("音频与视频"));
    nav->AddItem(_T("文件与存储"));
    nav->AddItem(_T("外观"));
    nav->SetCurSel(0, false);
    root->AddChild(std::move(nav), DuiLayout::Hint().Fixed(160));

    // 右：内容 VBox
    auto content = std::make_unique<DuiVBox>();
    content->SetPadding(20); content->SetGap(8);

    auto h = std::make_unique<DuiLabel>();
    h->SetText(_T("通用"));
    content->AddChild(std::move(h), DuiLayout::Hint().Fixed(28));

    auto sub = std::make_unique<DuiLabel>();
    sub->SetText(_T("启动行为"));
    sub->SetTextColor(RGB(120, 120, 120));
    content->AddChild(std::move(sub), DuiLayout::Hint().Fixed(20));

    auto makeOption = [](LPCTSTR label, std::unique_ptr<DuiControl> right) {
        auto row = std::make_unique<DuiHBox>();
        auto l = std::make_unique<DuiLabel>();
        l->SetText(label);
        row->AddChild(std::move(l), DuiLayout::Hint().Weight(1));
        row->AddChild(std::move(right), DuiLayout::Hint().Fixed(120));
        return row;
    };

    auto cb1 = std::make_unique<DuiButton>();
    cb1->SetCtrlId(IDC_AUTOSTART);
    cb1->SetButtonType(DuiButton::StyleCheckbox);
    cb1->SetText(_T("启用"));
    cb1->SetCheck(true, false);
    content->AddChild(makeOption(_T("开机自动启动"), std::move(cb1)),
                      DuiLayout::Hint().Fixed(28));

    auto combo = std::make_unique<DuiComboBox>();
    combo->SetCtrlId(IDC_CLOSE_BEHAVIOR);
    combo->AddString(_T("最小化到托盘"));
    combo->AddString(_T("退出程序"));
    combo->SetCurSel(0, false);
    content->AddChild(makeOption(_T("关闭主窗口时"), std::move(combo)),
                      DuiLayout::Hint().Fixed(28));

    content->AddChild(std::make_unique<DuiControl>(), DuiLayout::Hint().Weight(1));
    root->AddChild(std::move(content), DuiLayout::Hint().Weight(1));
    return root;
}
```

#### 9.4.2 事件处理

```
// MainFrame::OnDuiNotify 派发：
case IDC_SETTINGS_NAV:
    if (n->code == DUIN_VALUECHANGED)
    {
        // 真实业务里这里会用 SetClientContent 切换右侧面板
        // 简单示例：只切右侧标题
        int sel = (int)n->extra;
        SwitchSettingsPage(sel);
    }
    return 0;

case IDC_AUTOSTART:
    if (n->code == DUIN_VALUECHANGED)
    {
        bool enabled = (n->extra != 0);
        ConfigureAutoStart(enabled);
        g_settings.SetBool(_T("AutoStart"), enabled);
    }
    return 0;

case IDC_CLOSE_BEHAVIOR:
    if (n->code == DUIN_VALUECHANGED)
    {
        // n->extra >= 0：popup 选了一项；< 0：editable 输入未匹配
        if ((int)n->extra >= 0)
        {
            g_settings.SetInt(_T("CloseBehavior"), (int)n->extra);
        }
    }
    return 0;
```

#### 9.4.3 构建 + 运行

WinMain 用 [§9.0.2](#layout-skeleton-app)；`extern` 改 `BuildSettingsRoot()`。无 EDIT，所以 EnsureCreated 段可以全删。`SetTitle(_T("设置"))`。生成命令同 §9.0.3。

<a id="layout-skeleton"></a>

### 9.5 窗口骨架（Dock）

WPF / WinForm 的经典 dock 布局：上 toolbar + 下 statusbar + 左 nav + 中央 fill。`DuiDock` 就是为这个场景设计的，比手动嵌套 VBox/HBox 直观。

![窗口骨架布局](images/ctl-layout-skeleton.png)

*顶部 toolbar (32px) + 底部 statusbar (24px) + 左 nav (140px) + center 自适应填充*

#### 代码方式

```
auto dock = std::make_unique<DuiDock>();

// 顶部 32px toolbar
dock->AddDocked(BuildToolbar(),  DuiDock::DockTop,    32);

// 底部 24px statusbar
dock->AddDocked(BuildStatusbar(),DuiDock::DockBottom, 24);

// 左侧 140px nav
dock->AddDocked(BuildLeftNav(),  DuiDock::DockLeft,  140);

// 中央填充（必须最后加，且只能一个 DockFill）
dock->AddDocked(BuildContent(),  DuiDock::DockFill);

frame.SetClientContent(std::move(dock));
```

#### XML 方式

```
<dock>
  <toolbar    dockSide="top"    dockSize="32"/>
  <statusbar  dockSide="bottom" dockSize="24"/>
  <left-nav   dockSide="left"   dockSize="140"/>
  <content    dockSide="fill"/>
</dock>
```

DuiDock 的子节点添加顺序<u>很重要</u>：先 add 的占外圈，后 add 的占内圈。最后一个 `DockFill` 拿剩下的所有空间。如果你想让左 nav 一直顶到顶 / 底（而不是停在 toolbar / statusbar 之间），调换"先 left 后 top/bottom"的顺序即可。

#### 9.5.1 完整 Build 函数

```
std::unique_ptr<DuiControl> BuildSkeletonRoot()
{
    using namespace balloonwjui;
    auto dock = std::make_unique<DuiDock>();

    // 顶部 toolbar：从外向内 dock，先 add 的占外圈
    dock->AddDocked(BuildToolbar(),   DuiDock::DockTop,    32);

    // 底部 statusbar
    dock->AddDocked(BuildStatusbar(), DuiDock::DockBottom, 24);

    // 左侧 nav（在 top/bottom 之后 add，所以 nav 不会撑到顶 / 底
    //   —— 它停在 toolbar 下沿到 statusbar 上沿之间。想让 nav 从顶到底
    //   通跑就把这一行挪到 top / bottom 之前。）
    dock->AddDocked(BuildLeftNav(),   DuiDock::DockLeft,  140);

    // 中央填充（最后 add；一个 dock 只挂一个 DockFill）
    dock->AddDocked(BuildContent(),   DuiDock::DockFill);

    return dock;
}

// BuildToolbar / BuildStatusbar / BuildLeftNav / BuildContent 的实现示例：
std::unique_ptr<balloonwjui::DuiControl> BuildToolbar()
{
    using namespace balloonwjui;
    auto bar = std::make_unique<DuiHBox>();
    bar->SetPadding(4); bar->SetGap(4);
    for (int i = 0; i < 5; ++i)
    {
        auto b = std::make_unique<DuiButton>();
        b->SetButtonType(DuiButton::StyleIcon);
        b->SetCtrlId(2000 + i);
        bar->AddChild(std::move(b), DuiLayout::Hint().Fixed(28));
    }
    return bar;
}

std::unique_ptr<balloonwjui::DuiControl> BuildStatusbar()
{
    auto bar = std::make_unique<balloonwjui::DuiLabel>();
    bar->SetText(_T("Ready"));
    bar->SetTextColor(RGB(120, 120, 120));
    return bar;
}

std::unique_ptr<balloonwjui::DuiControl> BuildLeftNav()
{
    auto nav = std::make_unique<balloonwjui::DuiListBox>();
    nav->SetCtrlId(3000);
    nav->AddItem(_T("收件箱"));
    nav->AddItem(_T("已发送"));
    nav->AddItem(_T("草稿"));
    nav->SetCurSel(0, false);
    return nav;
}

std::unique_ptr<balloonwjui::DuiControl> BuildContent()
{
    return std::make_unique<balloonwjui::DuiLabel>();    // 业务自定义
}
```

#### 9.5.2 事件处理

```
// MainFrame::OnDuiNotify 派发：
case 3000:    // 左侧 nav listbox
    if (n->code == DUIN_VALUECHANGED)
    {
        SwitchView((int)n->extra);   // 0=收件箱 / 1=已发送 / 2=草稿
    }
    return 0;

default:
    // toolbar 5 个按钮 IDC = 2000..2004
    if (n->ctrlId >= 2000 && n->ctrlId < 2005 && n->code == DUIN_CLICK)
    {
        OnToolbarCmd(n->ctrlId - 2000);
    }
    return 0;
```

#### 9.5.3 构建 + 运行

WinMain 用 [§9.0.2](#layout-skeleton-app)；`extern` 改 `BuildSkeletonRoot()`。无 EDIT，所以 EnsureCreated 段可以全删。`SetTitle(_T("文档浏览器"))` + `ResizeClient(1024, 720)`。

---

<a id="nine-patch-bg"></a>

## 10. 9-grid 背景图（DuiHost::SetBgImage）

很多桌面对话框 / 窗口需要一张**带装饰**的背景图（圆角、顶部渐变条、阴影、边框纹理等）。这种图如果直接用 `StretchBlt` 整张缩放，圆角会被压扁、渐变条被拉成大胖子、装饰线变形。9-grid（也叫 nine-patch / nine-slice）解决了这个问题：把图按 4 个内距切成 9 块，**四角不缩、四边只一轴拉伸、中央双轴拉伸**。

balloonui 通过 `DuiHost::SetBgImage(HBITMAP, RECT insets)` 把这能力给整个 host 客户区。任何继承自 `DuiHost` 的窗口（`DuiFrameWindow` / `DuiPopupHost` / 业务自己的子类）都能用一行调用拿到。

<a id="bg-concept"></a>

### 10.1 概念图

4 个内距把源位图切 9 块。source（左侧 580×520）经 9-grid 渲染到任意尺寸的 dst（右侧）：

```
         source (580×520)                       dst (任意尺寸)
       +----+--------+----+               +----+----------------+----+
       | TL |   T    | TR |               | TL |        T       | TR |     ← 顶部 40px 渐变
       +----+--------+----+               +----+----------------+----+
       | L  |   C    | R  |               | L  |                | R  |
       |    |        |    |    →          |    |        C       |    |     ← 中间双轴拉
       |    |        |    |               |    |                |    |
       +----+--------+----+               +----+----------------+----+
       | BL |   B    | BR |               | BL |        B       | BR |     ← 底部 10px 描边
       +----+--------+----+               +----+----------------+----+
        ← 10px →   ← 10px →
        ← left →   ← right →

   每块的渲染规则：
     TL / TR / BL / BR  →  1:1 BitBlt（不缩放）
     T  / B             →  仅横向 StretchBlt（高度不变）
     L  / R             →  仅纵向 StretchBlt（宽度不变）
     C                  →  双轴 StretchBlt
```

<a id="bg-api"></a>

### 10.2 API

两个重载，根据需求选：

```
// DuiHost.h
class BUI_API DuiHost : public CWindowImpl<DuiHost, CWindow>
{
public:
    // 单 inset 重载 — 源 == 目标，4 角 1:1 复制（经典 9-grid 行为）。
    // 适合源图四角带圆角 / 阴影 / 装饰线 — 它们在任意 dst 尺寸下保持原始像素。
    //
    // hbm     : 源位图（caller-owned；host 不拷贝不释放，bitmap 必须比 host 活久）
    //           nullptr → 清除背景，回退 COLOR_BTNFACE 默认填充。
    // insets  : 源像素 left/top/right/bottom，定义 4 角不缩区域厚度。
    void    SetBgImage(HBITMAP hbm, const RECT& insets);

    // 双 inset 重载 — 源 / 目标内距独立指定。4 角参与缩放
    // (src.thickness → dst.thickness)。
    //
    // 典型用法：源里有一条装饰带（如 70px 渐变）想压缩到 40px 标题栏：
    //   srcInsets = { 10, 70, 10, 10 };
    //   dstInsets = { 10, 40, 10, 10 };
    //   host.SetBgImage(hbm, srcInsets, dstInsets);
    //
    // 当 src == dst 时退化为单 inset 模式。
    void    SetBgImage(HBITMAP hbm,
                       const RECT& srcInsets,
                       const RECT& dstInsets);

    HBITMAP GetBgImage() const;
};
```

| 场景 | 用哪个 |
| --- | --- |
| 源图四角带圆角 / 硬边装饰，要保住像素级清晰 | 单 inset |
| 源里装饰带高度 ≠ 想要的渲染高度（"渐变压缩到标题栏"） | 双 inset |
| 源是纯色边框（4 角缩放无视觉损失） | 都行；双 inset 更灵活 |

<a id="bg-usage"></a>

### 10.3 用法

```
// === main.cpp ===
balloonwjui::DuiFrameWindow frame;
frame.SetTitle(_T("BuddyInfo"));
frame.Create(NULL, CWindow::rcDefault, _T("BuddyInfo"),
             WS_OVERLAPPEDWINDOW, 0);

// 加载背景 PNG（caller 持有 HBITMAP）
HBITMAP hbm = LoadBgPng(_T("BuddyInfoDlgBg.png"));

// 4 个内距：粉蓝渐变条罩 40px、左右底各 10px 边框
RECT insets = { 10 /*L*/, 40 /*T*/, 10 /*R*/, 10 /*B*/ };
frame.SetBgImage(hbm, insets);

frame.SetClientContent(BuildContent());
frame.ResizeClient(560, 480);   // 用户拖窗角缩放都不失真
frame.ShowWindow(SW_SHOW);

// 退出前再 SetBgImage(nullptr, {}) 解关联，再 DeleteObject(hbm)。
```

<a id="bg-comparison"></a>

### 10.4 9-grid vs 朴素 StretchBlt — 一图说尽

以下两图均把同一张 580×520 的源图渲染到 ~970×280 的目标矩形。**区别就是：9-grid 的顶部渐变条保持 40px 高、左右描边保持 10px、四角圆润；朴素 StretchBlt 把顶部渐变条按比例压成 ~22px、四角随之缩小，整体变形。**

![9-grid medium](images/bg-9grid-medium.png)

*9-grid (DuiHost::SetBgImage)*

![naive medium](images/bg-naive-medium.png)

*朴素 StretchBlt*

<a id="bg-resize"></a>

### 10.5 三种尺寸 — 9-grid 都不变形

同一张源图、9-grid 渲染到三种目标尺寸。注意顶部渐变条**始终是 40px 高**（不被横向放大也不被纵向压扁），左右底细描边**始终是 10px**。中央白色面板自由伸缩，正好承载业务控件。

![small](images/bg-9grid-small.png)

*小（~970×220）*

![medium](images/bg-9grid-medium.png)

*中（~970×280）*

![large](images/bg-9grid-large.png)

*大（~970×360）*

<a id="bg-impl"></a>

### 10.6 实现原理

balloonui 内部分两层：

```
DuiHost::OnPaint                       // 真窗口 paint 入口
  ├─ EnsureBackBuffer(W, H)             // 准备 32bpp DIB 离屏 DC
  ├─ if (m_hBgImage):
  │     DuiNinePatch::Draw(memDC, hbm,
  │                       rcClient, insets);      // 9-grid 填整客户区
  ├─ else:
  │     FillRect(memDC, COLOR_BTNFACE);  // 默认灰底
  ├─ m_root->OnPaint(memDC, rcClient);   // 业务控件画在 bg 之上
  ├─ if (m_clientBorderColor != CLR_INVALID
  │     && !m_hBgImage):
  │     FrameRect(memDC, rcClient,
  │               m_clientBorderColor);  // 1px 外框，bg 图存在时跳过
  └─ BitBlt 离屏 → 窗口 DC               // 一次性 swap，无闪烁
```

注意倒数第二步的"**且 `!m_hBgImage`**"判断 — 这是 `SetBgImage` 与 `SetClientBorderColor` 的关键耦合：一旦设了 bg 图，`DuiFrameWindow` 默认开的 1px 浅灰外框 **自动失效**，让 9-grid 图自身的圆角 / 阴影 / 装饰当边界。<u>不需要</u> caller 在 `SetBgImage` 之前手动 `SetClientBorderColor(CLR_INVALID)`。

**`DuiNinePatch::Draw`**（`balloonui/DuiNinePatch.cpp`）做的事：

1. **ClampInsets** — 把 caller 给的 insets 截到合法范围：`left + right ≤ srcW` 且 `top + bottom ≤ srcH`。如果溢出，按比例缩小（不让 4 角重叠）。负值钳到 0。
2. **ComputeCells** — 数学计算，生成 9 对 (srcRect, dstRect)： `// 源切 3 列：[0, L), [L, srcW-R), [srcW-R, srcW) // 源切 3 行：[0, T), [T, srcH-B), [srcH-B, srcH) // dst 同样切 3 列 / 3 行，列高 = max(0, dstH - T - B) 给中间行， // 左列宽 = L、右列宽 = R 不变。 // 9 个组合 = 9 个 cell。` 某 cell 在 dst 退化（dstW < L+R 或 dstH < T+B）时该方向 src/dst 都置零，由调用方 IsRectEmpty 跳过。
3. **Blit 9 个 cell** — 单 cell 高/宽不变（4 角）→ `BitBlt`；高变宽不变（左右边）/ 高不变宽变（上下边）/ 双轴变（中央）→ `StretchBlt(SRCCOPY)` 或 `AlphaBlend`（如果源是 32bpp 带 premultiplied alpha）。

**关键不变量**：4 个角的 src 宽 = dst 宽 = `L`/`R`，src 高 = dst 高 = `T`/`B`。所以 BitBlt 是<u>像素对像素</u>复制，永远不失真。变形只发生在边/中央，且方向受限。

<a id="bg-pitfalls"></a>

### 10.7 常见坑

| 症状 | 原因 / 解决 |
| --- | --- |
| 整张图都被拉伸（看着像没生效） | insets 全是 0 → 没有"不缩"区域。检查传入的 RECT，左/上/右/下应是源像素值。 |
| 4 角变小或重叠 | insets 之和大于源尺寸（`L+R > srcW` 或 `T+B > srcH`）。`DuiNinePatch::ClampInsets` 会按比例缩小，但视觉上仍可能不是你想要的 — 改源图或调小 insets。 |
| 背景画不出来 / 黑屏 | HBITMAP 是 32bpp 但 alpha=0。`StretchBlt(SRCCOPY)` 复制 alpha 后被宿主 BitBlt 当透明处理 → 黑。两条解决路：①加载时强制 alpha=255；②走 `Gdiplus::Bitmap::GetHBITMAP(背景色, &hbm)` 把 alpha 预合成进 RGB（demo 走的就是这条）。 |
| HBITMAP 何时释放 | caller 拥有，比 host 活久。host 析构不会动它。建议生命期内只 `SetBgImage(nullptr, {})` 解关联，进程退出由 OS 回收。 |
| 插入子控件后看不到背景 | 子控件覆盖了背景。子控件通常不画自身背景（透明），但有些（DuiListBox / DuiEditHost）用纯色 brush 填底 — 给它们 `SetBgColor(transparent_marker)` 或在它们之间留 padding。 |

<a id="bg-titlebar"></a>

### 10.8 把渐变区当作窗口标题栏（透明标题条）

9-grid 背景图常见的设计是顶部留一条彩色装饰条 / 渐变 / 阴影，它视觉上 = 窗口的标题栏。 balloonui 提供 `DuiFrameWindow::SetTitleBarTransparent` + `SetTitleTextColor` + `SetCaptionGlyphColor` 三个 API 让"渐变区直接当标题栏"零样板代码。

**关键技巧 — 源 inset ≠ 目标 inset。**

源图里"装饰带"高度（如 70px 渐变）通常和你想要的标题栏视觉高度（如 40px）不一致。如果用单 inset（src=dst=40）：源前 40 行渐变 1:1 复制到目标，剩下 30 行渐变残尾被推到中间区，会在标题栏下方漏出一道脏边。

正确做法：用 `SetBgImage` 双 inset 重载 — `srcInsets.top=70`（罩住整条装饰带），`dstInsets.top=40`（你要的标题栏高度）。9-grid 把整条装饰带<u>等比压缩</u>到 40px，渐变完整呈现，下方业务区干净。

![渐变作为标题栏的 DuiFrameWindow](images/bg-frame-titlebar.png)

*BuddyInfoDlg 形态：粉蓝渐变条本身就是窗口标题栏 — 标题"好友资料"白字 + 三按钮白色 glyph 叠在渐变上；下面是头像 / 表单 / 关闭按钮。窗口可拖角缩放。*

#### API

```
// DuiFrameWindow.h（节选）
class BUI_API DuiFrameWindow : public DuiHost
{
public:
    // 标题栏透明 — 不画自身背景，让 host 的 SetBgImage(9-grid) 顶部
    // 那条装饰区直接当标题条用。
    void    SetTitleBarTransparent(bool b);
    bool    IsTitleBarTransparent() const;

    // 标题文字颜色（仅在 SetTitleBarTransparent(true) + 彩色背景时
    // 通常需要改）。默认 ink-1 (40,40,40)。
    void    SetTitleTextColor(COLORREF c);

    // min / max / close 三按钮 glyph 颜色覆盖。CLR_INVALID = 用默认深灰。
    // 关闭按钮 hover 红底时的白 glyph 不受此影响（仍是白）。
    void    SetCaptionGlyphColor(COLORREF c);
};
```

#### 用法

```
balloonwjui::DuiFrameWindow frame;
frame.SetTitle(_T("好友资料"));

// 关键尺寸
const int kSrcGradientH = 69;   // 源图里渐变带的真实像素高度（量出来的）
const int kDstTitleBarH = 40;   // 目标里标题栏的高度（你希望的最终视觉）

frame.SetTitleBarHeight   (kDstTitleBarH);   // 标题栏高度 = 目标顶 inset
frame.SetTitleBarTransparent(true);          // 不画自身背景
frame.SetTitleTextColor   (RGB(255, 255, 255));   // 白字配渐变
frame.SetCaptionGlyphColor(RGB(255, 255, 255));   // 白色 glyph
frame.SetButtons(true, true, true);
frame.SetMinSize(320, 240);
frame.SetResizable(true);

frame.Create(NULL, CWindow::rcDefault, _T("MyApp"), WS_OVERLAPPEDWINDOW, 0);

// 9-grid 背景：源 69px 渐变带 → 目标 40px 标题栏（等比压缩，渐变完整呈现）
HBITMAP hbm = LoadBgPng(_T("BuddyInfoDlgBg.png"));
RECT srcInsets = { 10, kSrcGradientH, 10, 10 };
RECT dstInsets = { 10, kDstTitleBarH, 10, 10 };
frame.SetBgImage(hbm, srcInsets, dstInsets);

frame.SetClientContent(BuildBuddyInfoForm());   // 表单从 y=titleBarH 开始
frame.ResizeClient(560, 520);
frame.CenterWindow();
frame.ShowWindow(SW_SHOW);
```

#### 关键点

| 问题 | 处理 |
| --- | --- |
| 渐变区如何成为"可拖动" | 无需特殊设置 — DuiFrameWindow 默认把整条标题栏（不管 m_titleH 是多大）都映射为 `HTCAPTION`，按下拖即拖动窗口。 |
| min/max/close 按钮位置 | 始终位于标题栏右上角，`kCaptionBtnW = 46` 宽 × 标题栏高。即使 SetTitleBarHeight(80)，按钮也是 46×80 — glyph 居中显示。 |
| 按钮 glyph 颜色对比 | 用 `SetCaptionGlyphColor(RGB(255,255,255))`。关闭按钮 hover 红底时强制白色 glyph，不受 override 影响（保证红底上仍可见）。 |
| 客户区内容应该从哪开始 | 客户区 root 的<u>顶部 padding</u> 不需要再留出标题栏高度 — DuiFrameWindow 已经把客户区 = 标题栏 + 内容 两段，content 从 y=titleBarH 开始。 |
| 窗口可拖角缩放吗 | 是。`WS_OVERLAPPEDWINDOW` 包含 `WS_THICKFRAME`，配合 `SetResizable(true)` + `SetMinSize`。9-grid 拖动缩放时<u>四角不失真</u>就是它的卖点。 |

<a id="bg-demo"></a>

### 10.9 完整 Demo

独立可运行 demo 在 `flamingoclient/DemoNinePatchBg/`：

| 文件 | 作用 |
| --- | --- |
| `main.cpp` | WinMain + 加载 PNG（GDI+ `GetHBITMAP` 路径，规避 alpha 坑）+ 构造 BuddyInfo 形态客户区 + `SetBgImage` 一行调用。 |
| `BgPaintTile.{h,cpp}` | 截图 helper：在自身 m_rcItem 内画 9-grid 或朴素 StretchBlt 之一。<u>仅供 capture 模式用</u> — 真业务直接调 `DuiHost::SetBgImage` 即可。 |
| `CaptureMode.{h,cpp}` | `--capture-all` 屏外渲染 + 出 PNG，与其它 4 个 demo 同模式。 |
| `BuddyInfoDlgBg.png` | 背景源图（580×520）。 |

```
# 编译
msbuild Demos.sln /p:Configuration=Debug /p:Platform=Win32 /t:DemoNinePatchBg

# 交互模式 — 拖窗角缩放看 9-grid 不失真
Bin\DemoNinePatchBg.exe

# 截图模式 — 重新生成本节图
Bin\DemoNinePatchBg.exe --capture-all flamingoclient\docs\images
```

---

<a id="frame-window-xml"></a>

## 11. DuiFrameWindow 的 XML 配置（<frame-window>）

`DuiFrameWindow` 自身的属性 —— 标题 / 标题栏高度 / 透明性 / 字色 / glyph 色 / 三按钮 / 客户区边框 / **9-grid bg 图（含双 inset）** —— 都可以走 `DuiXmlBuilder::FromFrameXml` 一份 XML 配置。<u>客户区控件树</u>仍按 §3 / §6 那套用 `FromString` 或直接 `BuildBuddyInfoContent()` 风格的 C++ 拼。

两种模式对比：

|  | 纯 C++（旧路径） | XML 驱动（新路径） |
| --- | --- | --- |
| frame 配置 | 一串 `frame.SetXxx(...)` | `FromFrameXml(xml, cfg)` + `frame.ApplyConfig(cfg)` |
| bg 图加载 | caller 自己 `Gdiplus::Bitmap::GetHBITMAP` + `SetBgImage(hbm, src, dst)`，自己释放 | `bg-image="..."` 属性，`LoadBgImageFromFile` 自动加载 + `frame` 持有 + 析构释放 |
| 路径基 | caller 自己 `GetModuleFileName` 拼 | 相对路径自动相对 exe 目录（`ResolveAssetPath`）；绝对路径直用 |
| 客户区 root | 同 — `SetClientContent(BuildXxx())` | 同 — `FromFrameXml` 也能从 `<frame-window>` 内的子元素 build 出 root 返回 |

<a id="frame-xml-schema"></a>

### 11.1 完整 schema

```
<frame-window
    title="好友资料"

    title-bar-height="40"                 // int，默认 36，最小 18
    title-bar-transparent="true"          // bool，默认 false（标题栏画浅灰底）
    title-text-color="255,255,255"        // "r,g,b"，默认 RGB(40,40,40)
    caption-glyph-color="255,255,255"     // "r,g,b"，rest 状态下 min/max/close glyph 色

    has-min-button="true"                 // bool，默认 true
    has-max-button="true"                 // bool，默认 true
    has-close-button="true"               // bool，默认 true

    min-w="320" min-h="240"               // int，默认 200 × 150
    resizable="true"                      // bool，默认 true
    border-px="8"                         // int，96-dpi 逻辑 px，默认 8
    client-border-color="200,200,200"     // "r,g,b" 或 "none"；DuiFrameWindow 默认开 RGB(200,200,200)

    bg-image="BuddyInfoDlgBg.png"         // 路径（绝对 / 相对 exe 目录），空 = 不设
    bg-src-insets="10,69,10,10"           // "l,t,r,b" 或单值"n"（4 边相同）
    bg-dst-insets="10,40,10,10">          // 同上；省略 → 退化为 src == dst（经典 9-grid）

  <vbox padding="24,16,24,16" gap="10">  // 客户区 root（可选）
    ...                                    // vbox/hbox/label/button/edit + 自定义 factory
  </vbox>
</frame-window>
```

#### 属性约定

- **颜色**：`"r,g,b"` 三段（0–255）。`client-border-color` 还接受 `"none"` = `CLR_INVALID`，显式禁用边框（<u>有 bg 图时其实已自动跳过，不必写 none</u>）。
- **insets / margin（RECT）**：`"l,t,r,b"` 四段，或单值 `"n"`（4 边相同）。
- **布尔**：`"true"` / `"1"` / `"yes"` 视为 true，其它为 false。
- **缺省**：任何属性<u>不写</u> = "未设" — `ApplyConfig` 不调对应 setter，frame 保留默认。例：只写 `title`，其它全用默认 → 跟 §10 之前的"默认样式"窗等价。
- **bg-image 路径**：以 `C:\` / `D:/` / `\\server\` / `//server/` 开头视为绝对，直接用；其它视为相对路径，由 `ResolveAssetPath` 拼到 `GetModuleFileName(NULL)` 所在目录后。<u>不会</u>解析 `~`、环境变量或当前 cwd（防止 cwd 漂移导致皮肤丢失）。
- **bg-dst-insets 省略**：等价于 `dst == src`（经典 9-grid，4 角 1:1 不缩）。要"源装饰带压缩到目标更小高度"那种用法（如本 demo 把 69px 渐变压成 40px 标题栏），必须显式写 `bg-dst-insets`。

<a id="frame-xml-api"></a>

### 11.2 API

```
// DuiXmlBuilder.h
struct DuiFrameWindowConfig
{
    Optional<CString>   title;
    Optional<int>       titleBarHeight;
    Optional<bool>      titleBarTransparent;
    Optional<COLORREF>  titleTextColor;
    Optional<COLORREF>  captionGlyphColor;
    Optional<bool>      hasMinButton;
    Optional<bool>      hasMaxButton;
    Optional<bool>      hasCloseButton;
    Optional<int>       minW;
    Optional<int>       minH;
    Optional<bool>      resizable;
    Optional<int>       borderPx;
    Optional<COLORREF>  clientBorderColor;   // CLR_INVALID = 显式禁用边框
    CString             bgImagePath;
    RECT                bgSrcInsets = { 0, 0, 0, 0 };
    Optional<RECT>      bgDstInsets;          // 不设 → 退化为 src == dst
};

class BUI_API DuiXmlBuilder
{
public:
    // 解析 <frame-window> 顶层元素，同时填 cfg + 返回客户区控件树
    // （<frame-window> 的第一个子元素 build 出来的）；没有子元素 → 返回 nullptr。
    // bg-image 路径在这里就转成了绝对路径写回 cfg.bgImagePath。
    static std::unique_ptr<DuiControl> FromFrameXml(LPCSTR  xmlUtf8,
                                                    DuiFrameWindowConfig& outConfig,
                                                    const CustomFactory& factory = {});
    static std::unique_ptr<DuiControl> FromFrameXml(LPCWSTR xmlW,
                                                    DuiFrameWindowConfig& outConfig,
                                                    const CustomFactory& factory = {});

    // 资源路径解析。绝对路径 → 直返；相对 → 拼 exe 目录；空 → 空。
    // XML 内部用，也对外暴露给业务自己用（如自定义 factory 里加载 icon）。
    static CString ResolveAssetPath(LPCTSTR userPath);
};

// Controls/DuiFrameWindow.h
class BUI_API DuiFrameWindow : public DuiHost
{
    ...
    // 应用 cfg 到 frame —— 任何 has_value() 的字段都调对应 setter，
    // 未设字段保留 frame 当前值。bg image 走 LoadBgImageFromFile（host
    // 持有 bitmap、析构释放）。一般在 Create 之后、ShowWindow 之前调。
    void ApplyConfig(const DuiFrameWindowConfig& cfg);
    ...
};

// DuiHost.h（新加）
// 文件路径加载便捷重载 —— GDI+ 加载 + host 持有 + 析构释放。
// path 由 caller 决定（绝对 / 相对自己处理；XML 走 ResolveAssetPath 后再传）。
bool LoadBgImageFromFile(LPCTSTR path, const RECT& insets);
bool LoadBgImageFromFile(LPCTSTR path, const RECT& srcInsets, const RECT& dstInsets);
```

<a id="frame-xml-walkthrough"></a>

### 11.3 端到端走查 — DemoNinePatchBg 右窗

本 demo 的右窗已切到 XML 驱动。下面是 `DemoNinePatchBg/main.cpp` 节选：

```
// 1) 字面量 XML（也可以从文件 / 资源里加载）
const char* kFrameXml =
    "<frame-window"
    "  title=\"好友资料（XML 驱动）\""
    "  title-bar-height=\"40\""
    "  title-bar-transparent=\"true\""
    "  title-text-color=\"255,255,255\""
    "  caption-glyph-color=\"255,255,255\""
    "  bg-image=\"BuddyInfoDlgBg.png\""             // 相对 exe 目录
    "  bg-src-insets=\"10,69,10,10\""
    "  bg-dst-insets=\"10,40,10,10\""
    "  client-border-color=\"none\""                // 这里其实可以省（有 bg 图自动跳过）
    "  min-w=\"320\" min-h=\"240\""
    "  resizable=\"true\"/>";

// 2) 解析 — cfg 拿到所有属性，bg-image 路径已经 ResolveAssetPath 转成绝对
DuiFrameWindowConfig cfg;
DuiXmlBuilder::FromFrameXml(kFrameXml, cfg);

// 3) 标准 frame 创建流程
MainFrame frame;
frame.Create(NULL, CWindow::rcDefault, _T("DemoFrameThemed"),
             WS_OVERLAPPEDWINDOW, 0);
frame.ApplyConfig(cfg);                          // ★ 一行落地所有属性 + 加载 bg 图
frame.SetClientContent(BuildBuddyInfoContent());     // 客户区还是 C++ 拼 —— 也可以 FromString
frame.ResizeClient(560, 520);
frame.ShowWindow(nCmdShow);
```

四窗 2×2 同时启动对比效果（DemoNinePatchBg.exe 直接跑就是这个布局）：

![代码 vs XML 驱动 × 默认 vs 9-grid bg 共 4 窗对比](images/frame-4-windows-2x2.png)

*行 = 配置模式（上：纯 C++ / 下：XML 驱动）；列 = 样式（左：默认 / 右：9-grid bg）。<u>同列两窗</u>除标题文字"/ 代码" vs "/ XML"后缀外像素级一致 —— 证明 XML 驱动与等价 C++ 调用产出的窗口完全等同。*

<a id="frame-xml-pitfalls"></a>

### 11.4 常见坑 / 设计取舍

| 问题 | 处理 |
| --- | --- |
| 路径含中文 / 空格 | UTF-8 编码的 XML 里直接写。`ResolveAssetPath` 不做 URL 编码，按字面拼到 exe 目录后；只要文件系统能找到就 OK。 |
| bg-image 相对路径找不到 | `LoadBgImageFromFile` 静默返回 false，frame 不挂 bg 图，等价于"默认样式"。检查 exe 输出目录是否拷了 png。 |
| 同时有 bg-image 和 client-border-color | OnPaint 看 `!m_hBgImage` 跳过画框；`client-border-color` 留着不影响（写 `"none"` 是为了"将来去掉 bg 图也别画框"的语义清晰）。 |
| 已经 ShowWindow 后再 ApplyConfig | 合法 — 各 setter 自己 Invalidate / 重建 skeleton（`SetTitleBarHeight` 会重建）。可用于"换皮"实时切换。 |
| 客户区也想从 XML 来 | `<frame-window>` 内放一个 `<vbox>` 子元素，`FromFrameXml` 直接 build 出来当返回值给 `SetClientContent`。复杂客户区建议另起一个 XML 文件 + `FromString`，主 frame XML 只配框架，可读性更好。 |
| bgImagePath 是 CString 不是 Optional | 有意 — 空字符串就是"未设 bg 图"，跟"已设但路径空"无法区分（也无意义），所以省一层包装。 |

---

<a id="feature-strip"></a>

## 12. 按需裁剪 (Feature Strip)

balloonui 提供 28 个控件 + 一个 XML builder。但实际项目里业务多半只用其中一部分（聊天客户端可能不需要 `DuiTreeView`，工具类应用不需要 `DuiRichEditHost`，等等）。本节介绍如何用<u>预处理器宏</u>把没用到的控件从编译过程中剔除，从而让最终 `balloonui.dll` / `.lib` 体积变小。

### 12.1 工作原理

每个控件的 `.h` 和 `.cpp` 整体被 `#if BUI_FEATURE_XXX` 包起来；feature 关掉 → 该控件源码不参与编译 → `.obj` 不生成 → 不进 lib / DLL。同时 `DuiXmlBuilder` 的 tag dispatch 也用同样的宏门控，feature 关掉时 dispatch 分支消失，遇到对应 tag 走"miss"路径：`OutputDebugString` 警告 + 返回 `nullptr`。

所有开关定义在 `balloonui/BalloonUiFeatures.h`。<u>默认全开</u>，业务什么都不写 = 全功能。要关某 feature：在 balloonui 工程的预处理器定义里加 `BUI_DISABLE_XXX`，重新编 balloonui.dll；同时业务 exe 工程也加同一份 list（让 `#include` 的头文件 class 声明跟 DLL 导出符号保持一致）。

### 12.2 开关一览表

共 28 个独立开关 + 1 个衍生开关（GALLERY）。每个 `BUI_DISABLE_XXX` 关掉对应控件；下表把控件按目录分组列出。

| 开关 | 控件 | 关闭后影响 | 依赖（被传染） |
| --- | --- | --- | --- |
| `BUI_DISABLE_LAYOUT` | DuiVBox / DuiHBox / DuiGrid | **不允许**（基础容器，文件中 `#error` 拦截） |  |
| `BUI_DISABLE_DOCK` | DuiDock | `<dock>` XML 失效 | — |
| `BUI_DISABLE_SPLITTER` | DuiSplitter | `<splitter>` XML 失效 | — |
| `BUI_DISABLE_LABEL` | DuiLabel | `<label>` XML 失效 | — |
| `BUI_DISABLE_BUTTON` | DuiButton | `<button>` XML 失效 | — |
| `BUI_DISABLE_AVATAR` | DuiAvatar | `<avatar>` XML 失效 | — |
| `BUI_DISABLE_BADGE` | DuiBadge | `<badge>` XML 失效 | — |
| `BUI_DISABLE_SEPARATOR` | DuiSeparator | `<separator>` XML 失效 | — |
| `BUI_DISABLE_GROUPBOX` | DuiGroupBox | `<groupbox>` XML 失效 | — |
| `BUI_DISABLE_EDIT` | DuiEditHost | `<edit>` XML 失效 | SEARCHBOX, SPINBOX, COMBOBOX, TREEVIEW |
| `BUI_DISABLE_IMAGEOLE` | CDuiImageOle | RichEdit 内嵌图片不可用 | RICHEDIT |
| `BUI_DISABLE_RICHEDIT` | DuiRichEditHost | `<richedit>` XML 失效 | — |
| `BUI_DISABLE_SEARCHBOX` | DuiSearchBox | `<searchbox>` XML 失效 | — |
| `BUI_DISABLE_SPINBOX` | DuiSpinBox | `<spinbox>` XML 失效 | — |
| `BUI_DISABLE_SLIDER` | DuiSlider | `<slider>` XML 失效 | — |
| `BUI_DISABLE_SWITCH` | DuiSwitch | `<switch>` XML 失效 | — |
| `BUI_DISABLE_SCROLLBAR` | DuiScrollBar | — | LISTBOX, TREEVIEW |
| `BUI_DISABLE_LISTBOX` | DuiListBox | `<listbox>` XML 失效 | COMBOBOX |
| `BUI_DISABLE_COMBOBOX` | DuiComboBox | `<combobox>` XML 失效 | — |
| `BUI_DISABLE_TREEVIEW` | DuiTreeView | `<treeview>` XML 失效 | — |
| `BUI_DISABLE_TAB` | DuiTab | — | TABPAGE |
| `BUI_DISABLE_TABPAGE` | DuiTabPage | `<tab-page>` CustomFactory 失效 | — |
| `BUI_DISABLE_MENU` | DuiMenu | 右键菜单 / dropdown 不可用 | MENUBAR |
| `BUI_DISABLE_MENUBAR` | DuiMenuBar | `<menu-bar>` XML 失效；常驻菜单条不可用 | — |
| `BUI_DISABLE_PROGRESSBAR` | DuiProgressBar | `<progress>` XML 失效 | — |
| `BUI_DISABLE_TOOLTIP` | DuiToolTipMgr | 悬浮提示气泡不可用 | — |
| `BUI_DISABLE_POPUPHOST` | DuiPopupHost | — | — |
| `BUI_DISABLE_EMOJIPANEL` | DuiEmojiPanel | — | — |
| `BUI_DISABLE_GIF` | DuiGif | 动图退化为静态首帧 | — |
| `BUI_DISABLE_FRAMEWINDOW` | DuiFrameWindow | `<frame-window>` + `FromFrameXml` 失效 | — |
| `BUI_DISABLE_XMLBUILDER` | DuiXmlBuilder | `FromString` / `FromFrameXml` 失效（需手写 C++ AddChild 链） | — |

"被传染"列表示：关掉本行 feature 自动连带关掉这些上层 feature。例如 `BUI_DISABLE_EDIT` 会让 SEARCHBOX / SPINBOX / COMBOBOX / TREEVIEW 一起消失（因为它们内部嵌入 `DuiEditHost`）。文件中用 `#if !defined(BUI_DISABLE_X) && defined(BUI_FEATURE_DEP)` 链强制执行。

### 12.3 衍生开关 BUI_FEATURE_GALLERY

`DuiGalleryDlg` + `DuiGalleryAutoStart` 是 dev-only 测试入口（Debug 启动时自动弹一个浏览所有控件 demo 的窗口）。它<u>使用了几乎所有控件</u>，所以 BalloonUiFeatures.h 自动派生一个 `BUI_FEATURE_GALLERY` 开关：仅当 28 个底层 feature 全开时才为 1。任何一个底层 feature 关掉 → gallery 整个跳过编译。这避免了 GalleryDlg.cpp 内对失踪 RunAll() 等符号的引用。

### 12.4 实测体积差

测试方法：保持默认编译 balloonui.dll（feature 全开）记下尺寸；在 `balloonui.vcxproj` 的预处理器定义里加 23 个 `BUI_DISABLE_*`（保留 LAYOUT + LABEL + BUTTON + EDIT + XMLBUILDER），重新编译记下尺寸。VS 2022 + Win32。

| 配置 | balloonui.dll 尺寸 | 剩余 |
| --- | --- | --- |
| Debug 全开 | 2,581,504 bytes (2.46 MB) | — |
| Debug 极简（仅 LAYOUT + LABEL + BUTTON + EDIT + XMLBUILDER） | 2,004,992 bytes (1.91 MB) | **↓ 22.3%** |
| Release 全开 | 295,424 bytes (288 KB) | — |
| Release 极简 | 151,552 bytes (148 KB) | **↓ 48.7%** |

Release 模式收益最大（48.7%）—— 因为 Release 没有 Debug 信息这种与 feature 关系不大的"水分"，feature 之间的<u>纯代码占比</u>更高，剔除收益更明显。Debug 模式收益相对小（22.3%），因为 PDB / ASan / no-inline 等本来就占了大量字节。

### 12.5 业务侧操作步骤

1. 在 `balloonui/balloonui.vcxproj` 的"项目属性 → C/C++ → 预处理器 → 预处理器定义"里加上你要关的 feature，分号分隔。例： BUI_DISABLE_TREEVIEW;BUI_DISABLE_RICHEDIT;BUI_DISABLE_MENU;BUI_DISABLE_GIF
2. 重新编译 balloonui（Release 用 `Bin\balloonui.dll` + `balloonui.lib`；Debug 同）。
3. **关键**：在你的业务 exe 工程里加<u>同一份</u>预处理器定义。否则业务 exe 端 `#include "Controls/.../DuiXxx.h"` 看到的 class 声明跟 DLL 导出符号不一致 → 链接失败。
4. 重新编业务 exe，把新生成的 `balloonui.dll` 部署到运行目录。

如果你是<u>静态库</u>方式链接 balloonui（`BUI_USE_DLL` 没定义），上述流程一样，区别只是 .lib 直接链进 exe，不需要部署 DLL。

### 12.6 Tag miss 排查指引

关掉某 feature 后，业务侧 XML 里若还残留对应 tag（例如关了 TREEVIEW 但 XML 仍写 `<treeview>`），`DuiXmlBuilder` 会：

1. 先尝试 caller 注册的 `CustomFactory`，如果业务自己注册了同名 tag 就用业务版本；
2. 否则在 VS Output 窗口（或 DebugView）打印一行： [balloonui] DuiXmlBuilder: unknown tag <treeview> (feature disabled or factory missing)
3. 返回 `nullptr` 给该节点（其父容器 AddChild 跳过）。<u>不抛异常 / 不 abort</u>，Release 也能跑、只是 UI 缺一块。

排查时把 OutputDebugString 流引到 DebugView（SysInternals）查看，定位是哪段 XML 里写了被裁掉的 tag。

### 12.7 限制 / 已知问题

- **DLL 模式重编**：`BUI_USE_DLL` 模式下 balloonui 是"编一次给多 exe 用"，但本方案要求业务<u>自己</u>用 BUI_DISABLE_* 重编 DLL（一份 DLL 对一组业务 exe）。如果同一台机器上多个业务用不同的 disable list，需要给每个业务编一份独立 DLL（或选静态库）。
- **多 exe 共享一个 balloonui.dll**：要求所有共享业务用同一份 disable list。否则 exe 端 `#include` 看到的 class 声明跟 DLL 导出符号不一致。
- **新增控件**：库内未来新增控件时，作者要在 BalloonUiFeatures.h 加对应 BUI_FEATURE_XXX 开关 + 在控件 .h/.cpp 包 `#if`，并把 DuiXmlBuilder.cpp 里对应 dispatch 分支门控住。本节 §12.2 的开关表也得相应更新。
- **没用到的"被传染"feature 也消失**：例如关掉 EDIT 会自动连关 TREEVIEW；如果业务以为"我不用 EditHost 但要用 TreeView"那就行不通（TreeView 内嵌 EditHost 做 inline cell editing）。BalloonUiFeatures.h 在矛盾配置下会 `#error` 拦截。

---

<a id="demo-taskmgr"></a>

## 13. 综合案例：DemoTaskManager

`third_party/DemoTaskManager/` 用 balloonui 复刻 Win10 任务管理器界面，是把库里多个控件、XML 驱动、自绘控件、事件路由的整套用法贯穿起来的最完整 demo。本章详述其结构 —— **重点是布局**，因为这是新加入开发者最需要"对照着抄"的部分。

![DemoTaskManager 进程页全貌](images/demo-taskmgr-processes.png)

*整体布局：**frame-window**（自绘标题栏）→ **menu-bar**（文件 / 选项 / 查看）→ **tab-page**（7 个 tab）→ 内容区 → **status-bar**（进程数 / CPU / 内存）。*

<a id="demo-taskmgr-stack"></a>

### 13.1 涉及的库特性

| 类别 | 用法 |
| --- | --- |
| 容器 | `DuiVBox` / `DuiHBox` / `DuiSplitter` |
| 顶层窗口 | `DuiFrameWindow`（XML 驱动，含 `title-bar-height` / `has-min/max/close-button` / `min-w/min-h` / `resizable` 全配） |
| 菜单 | `DuiMenuBar` 常驻菜单条 + `DuiMenu` 弹出下拉（含 mouse-move 切换） |
| 多 tab | `DuiTabPage` 7 页（自定义 `<tab-page> / <tab-page-item>` tag 由 `CustomFactory` 接住） |
| 多列表格 | `DuiTreeView` 多列模式：6 个 tab 都用它（树结构 / 单层 / 双层 / 14 列冻结首列） |
| 表格 cell 类型 | `CELL_TEXT`（默认）/ `CELL_CHECKBOX`（启动 tab 状态列）；首列 `SetItemIcon` 显示字母 logo |
| 表头排序 | `DUITVN_COLUMNCLICK` + 业务侧 `SortByColumn(col, asc)` + `SetSortIndicator` |
| 右键菜单 | 进程 tab 节点右键 → `DuiMenu::TrackPopup` 弹"结束任务 / 转到详细信息 / ..." |
| 分隔条 | 性能 tab 用 `DuiSplitter`（左 sidebar / 右 detail，可拖动） |
| 自绘控件 | `TaskMgrLineChart`（折线图）+ `TaskMgrSidebarItem`（sidebar 行）—— 派生 `DuiControl`，**留在 demo 里不进库** |
| 主题 / 字体 | 所有控件走 `DuiResMgr::GetDefaultFont()`（微软雅黑 9pt）；字母 logo 走 `MakeLetterBitmap` GDI 内存生成 |
| 数据驱动 | `WM_TIMER` 200ms → `demotaskmgr::Tick()` 推进 mock 数据 → 状态栏 / 进程数值 / 折线图 / sidebar 全部刷新 |
| 事件路由 | `WM_DUI_NOTIFY` 接 menu bar / treeview 排序 / 双击 / 右键；`WM_SYSKEYDOWN` 转发 Alt+letter 给 menu bar |

<a id="demo-taskmgr-files"></a>

### 13.2 文件清单

| 文件 | 职责 |
| --- | --- |
| `main.cpp` | 入口、`TaskManagerFrame` 主 frame、200ms timer、所有事件路由（menu / tree 排序 / 右键 / 双击 / Alt 助记符）；`--screenshot <dir>` 自动化截图模式（生成本章所有 PNG） |
| `taskmgr.xml` | 主框架 XML：`frame-window` + `vbox` + `menu-bar` + `tab-page`（含 7 个 `tab-page-item`）+ 状态栏 `hbox` + `status-label` ×3 |
| `XmlFactory.h/.cpp` | `CustomFactory` 入口：接 `<tab-page>` / `<tab-page-item>` / `<status-label>` 三个自定义 tag；`TaskMgrXmlOuts` 把构造完的关键控件指针带回 main |
| `MockData.h/.cpp` | 200 进程 / 30 启动项 / 150 服务 / 2 用户的固定 seed 生成；5 路 ring buffer（CPU / 内存 / 磁盘 / 网络 / GPU 各 60 点）；`MakeLetterBitmap` + `MakeLetterIcon` GDI 内存图标生成器 |
| `ProcessesPage.h/.cpp` | 进程 tab：`DuiTreeView` 7 列树结构（应用 / 后台 / Windows 三 root），首列字母 logo，组内排序保留分组 |
| `PerformancePage.h/.cpp` | 性能 tab：`DuiSplitter` + 5 个 sidebar item + 自绘 line chart + 4 列 stats；含 `TaskMgrLineChart` + `TaskMgrSidebarItem` 两个自绘子类 |
| `OtherPages.h/.cpp` | 剩 5 个 tab：应用历史 / 启动 / 用户 / 详细信息 / 服务（每个一个 `DuiTreeView` 子类，单层或双层） |

<a id="demo-taskmgr-layout"></a>

### 13.3 整体布局（taskmgr.xml）

整个主窗口的骨架由一个 XML 文件描述。`main.cpp` 用 `DuiXmlBuilder::FromFrameXml(xml, cfg, factory)` 一次解析完，frame 配置（标题 / min-size / 三按钮 / resizable）和客户区子树同步落地。

```
frame-window  title="任务管理器"  title-bar-height="36"
              has-min/max/close-button="true"
              min-w="800"  min-h="600"  resizable="true"
└── vbox  padding="8,4,8,4"  gap="2"
    │
    ├── <menu-bar>  id=100  fixedHeight=24
    │   ├── <menu-item id=110 text="文件(&F)"/>     # dropdown 由 main.cpp 后置 SetDropdown
    │   ├── <menu-item id=120 text="选项(&O)"/>
    │   └── <menu-item id=140 text="查看(&V)"/>
    │
    ├── <tab-page>  id=200  weight=1                # CustomFactory 接住
    │   ├── <tab-page-item title="进程"           page-tag="processes"/>
    │   ├── <tab-page-item title="性能"           page-tag="performance"/>
    │   ├── <tab-page-item title="应用历史记录"   page-tag="appHistory"/>
    │   ├── <tab-page-item title="启动"           page-tag="startup"/>
    │   ├── <tab-page-item title="用户"           page-tag="users"/>
    │   ├── <tab-page-item title="详细信息"       page-tag="details"/>
    │   └── <tab-page-item title="服务"           page-tag="services"/>
    │
    └── <hbox>  id=300  fixedHeight=22  gap=16  padding="0,2,0,2"   # 状态栏
        ├── <status-label which="proc" text="进程数: --"     fixedWidth=100/>
        ├── <status-label which="cpu"  text="CPU 使用率: --" fixedWidth=140/>
        └── <status-label which="mem"  text="内存: --"        fixedWidth=180/>
```

三个自定义 tag 由 `demotaskmgr::MakeTaskMgrFactory(&outs)` 返回的 lambda 接住：

| 自定义 tag | 构造 | 带回 outs |
| --- | --- | --- |
| `<tab-page>` | `new DuiTabPage`，遍历子节点 `<tab-page-item>`，每个调 `AddPage(title, MakePageByTag(page-tag, outs))` | `outs.tabPage` + 各 page 指针 |
| `<tab-page-item>` | 0 尺寸占位 `DuiControl`（已在 `<tab-page>` 工厂里消费过；这里只是为了不触发 kernel 的 unknown-tag 警告） | — |
| `<status-label>` | `new DuiLabel` + 设初始文字 + 按 `which` 写指针到 `outs.lblProc / lblCpu / lblMem` | 3 个 label 指针 |

main.cpp 解析完 XML 拿到 `outs`，`frame.BindOuts(outs)` 把指针拷到 frame；之后 timer / 事件路由都靠这些指针快速定位控件。`menu-bar` 不在 `outs` 里 —— 用 `DuiHost::GetRoot()->FindCtrlById(100)` 拿到（XML 里它是库内置 tag，不走 CustomFactory）。

<a id="demo-taskmgr-pages"></a>

### 13.4 各 tab 的内部布局

#### 13.4.1 进程 (`ProcessesPage`)

![进程 tab：3 root 树形 + 7 列 + 字母 logo](images/demo-taskmgr-processes.png)

*进程 tab：`DuiTreeView` 多列模式 7 列。3 个 group root 不带 icon；进程节点首列带字母 logo（A 蓝 = 应用 / B 灰 = 后台 / W 深灰 = Windows）。"应用"组里若进程有 helper 子进程，子进程缩进显示。*

```
DuiTreeView  cols=7  rowH=28
├── root "应用 (16)"           [no icon]
│   ├── chrome.exe              [A blue]   正在运行  8.1%  850.1 MB  2.5 MB/秒  1.3 Mbps  2.7%
│   │   ├── chrome.exe (Helper) [A blue]   ...
│   │   └── chrome.exe (Helper) [A blue]   ...
│   ├── Code.exe                [A blue]   ...
│   ...
├── root "后台进程 (45)"        [no icon]
│   └── 80 进程节点             [B gray]   ...
└── root "Windows 进程 (38)"    [no icon]
    └── 40 进程节点             [W darker] ...

列：名称(240) / 状态(80) / CPU(70,右) / 内存(90,右) / 磁盘(90,右) / 网络(90,右) / GPU(70,右)
```

**关键 API：**`AddRoot` / `AddChild`（树结构）；`SetItemIcon`（字母 logo）；`SetCellText`（每帧 timer 刷 5 数值列）；`ExpandAll`（默认全展开）。

**事件：**双击节点 `DUIN_DBLCLK` → `MessageBox` 显示 mock 属性框；右键节点 `DUITVN_RCLICK` → `DuiMenu::TrackPopup` 弹 "结束任务 / 转到详细信息 / 打开文件位置 / 属性"；表头列名单击 `DUITVN_COLUMNCLICK` → `SortByColumn(col, asc)` 三 root 内分别 stable_sort + 重建树（应用组的 helper 子进程跟父走，不参与排序）。

#### 13.4.2 性能 (`PerformancePage`)

![性能 tab：splitter + sidebar + 折线图 + stats](images/demo-taskmgr-performance.png)

*性能 tab：`DuiSplitter` 把页面分成左 sidebar（5 个 metric）+ 右 detail（折线图主体）。Sidebar item 选中时左侧 4px accent 竖条 + 浅蓝底；行尾带 60 点 mini sparkline。Detail 区主图 60 点 ring buffer，`DuiAA::DrawLine` AA 折线 + 10×10 网格 + 当前值大字 + 100% / 0 Y 轴标注。*

```
PerformancePage   :  DuiSplitter (vertical, split=220, min=160/320)
│
├── pane 0: DuiVBox (sidebar)        gap=2
│   ├── TaskMgrSidebarItem CPU      [60 px]   选中态：4px 竖条 + 浅蓝底
│   ├── TaskMgrSidebarItem 内存      [60 px]
│   ├── TaskMgrSidebarItem 磁盘 0    [60 px]
│   ├── TaskMgrSidebarItem 以太网    [60 px]
│   ├── TaskMgrSidebarItem GPU 0    [60 px]
│   └── DuiControl (spacer)         weight=1
│
└── pane 1: DuiVBox (detail)         padding="16,12,16,12"  gap=6
    ├── DuiLabel  title              fixed=28      "CPU"
    ├── DuiLabel  subtitle           fixed=18      "Intel Core i7-8700K @ 3.70 GHz"
    ├── TaskMgrLineChart              weight=1     ← 自绘
    └── DuiHBox  stats               fixed=56  gap=20
        ├── DuiLabel "利用率\n18%"    weight=1
        ├── DuiLabel "速度\n3.70 GHz" weight=1
        ├── DuiLabel "进程\n200"      weight=1
        └── DuiLabel "线程\n2845"     weight=1
```

**两个自绘 DuiControl 子类（<u>不进库，留在 demo</u>）：**

- **`TaskMgrLineChart`**：60 点 ring buffer 数据源 + 0–`range` Y 轴量程；`OnPaint` 画白底 + 10×10 浅灰网格 + accent 色 1.5px AA 折线（`DuiAA::DrawLine`）+ 左上当前值 22pt 加粗大字 + 右 Y 轴 100% / 0 标注。最新点固定在右端，启动 12s 内曲线从右侧逐渐"长出"。
- **`TaskMgrSidebarItem`**（60px 行）：选中态 4px accent 竖条 + 浅蓝底；上半行 accent 圆点 + 标题 + 当前值（右对齐）；下半行副标题 + 90×18 mini sparkline。`OnLButtonDown` 不走 `WM_DUI_NOTIFY`，直接调持有的父 `m_parent->SelectMetric(kind)`（父子关系稳定，裸指针安全）。

切换 metric 时 `PerformancePage::SelectMetric(k)` 重置 5 个 sidebar item 的选中态、把 chart 数据源 / accent 切到新 metric 的 ring buffer、更新 detail 区 title / subtitle / 4 个 stats label。

#### 13.4.3 应用历史记录 (`AppHistoryPage`)

![应用历史 tab：5 列单层](images/demo-taskmgr-appHistory.png)

*5 列单层。数据：`g_processes` 中 `kind == App && parentIndex < 0` 的"父"应用进程；CPU 时间 / 网络 / 流量 / 磁贴 是 mock 累计值（按 procIdx 派生 deterministic 数字）。*

```
DuiTreeView  cols=5
├── 名称(220)  CPU 时间(90,右)  网络(90,右)  按流量计费(100,右)  磁贴更新(100,右)
└── 16 个应用顶层节点（仅父进程，不含 helper）
```

#### 13.4.4 启动 (`StartupPage`)

![启动 tab：4 列 + checkbox](images/demo-taskmgr-startup.png)

*4 列单层。**状态列用 `CELL_CHECKBOX`**（勾选 = 启用，取消 = 禁用），`SetCellChecked` + `SetCellText` 双显（图形 + 文字）。"启动影响"列文字标签：无 / 低 / 中 / 高。*

```
DuiTreeView  cols=4
└── 名称(220)  发布者(220)  状态(90,CHECKBOX)  启动影响(100)
```

#### 13.4.5 用户 (`UsersPage`)

![用户 tab：两层 user → 进程](images/demo-taskmgr-users.png)

*2 列 / 两层结构。每个 user 作为 root（"`balloonwj (N)`" / "`admin (N)`"），下挂该用户的应用进程。CPU 列 root 显示组合计、子节点显示单进程值。Sort 仅对每个 root 内的子节点排，user root 顺序固定不动。*

#### 13.4.6 详细信息 (`DetailsPage`)

![详细信息 tab：14 列 + 首列冻结](images/demo-taskmgr-details.png)

*14 列单层 + `SetFrozenColumns(1)`。横向滚动时首列名称贴左不动，方便对照行。所有 200 进程一次性铺平显示。*

```
DuiTreeView  cols=14  frozen=1
列：名称(200,冻结)  PID(70)  状态(80)  用户名(120)  CPU(60)
    内存(专用工作集)(130)  内存(提交大小)(130)  UAC虚拟化(100)
    命令行(280)  平台(60)  描述(220)  公司(180)  操作(80)  启动时间(100)
```

#### 13.4.7 服务 (`ServicesPage`)

![服务 tab：6 列 + 首列绿 S logo](images/demo-taskmgr-services.png)

*6 列单层。每行首列带绿底 'S' 字母 logo（与进程页字母 logo 同机制，`MakeLetterBitmap` 生成）。150 行服务，`pid == 0` 时 PID 列显示 "—"。*

```
DuiTreeView  cols=6
└── 名称(150,有 icon)  PID(70,右)  描述(280)  状态(90)  组(180)  备注(120)
```

<a id="demo-taskmgr-data"></a>

### 13.5 数据驱动 + 200ms timer

所有动态数字都靠一个 200ms 的 `WM_TIMER` 推进。`main.cpp::TaskManagerFrame::OnTimer_` 每 tick 做 4 件事：

1. `demotaskmgr::Tick()` —— mock 数据自身推进（5 路 ring buffer 各 push 一个新采样点；200 个进程的 cpu/mem/disk/net/gpu 数值各自抖动一格）
2. `UpdateStatusBar()` —— 状态栏 3 个 `DuiLabel` 文字重写（进程数 / CPU% / 内存）
3. `m_outs.processesPage->RefreshNumbers()` —— 进程 tab 表格 5 数值列文字重写（200 进程 × 5 = 1000 次 `SetCellText`，热路径都用栈上 `TCHAR[32]` + `_stprintf_s`）
4. `m_outs.performancePage->OnTick()` —— 性能 tab 主图 + 5 个 sidebar item 全 `Invalidate`，下一帧 `OnPaint` 直接读最新 ring buffer 重画

其余 5 个 tab（应用历史 / 启动 / 用户 / 详细信息 / 服务）数据相对静态，timer 不刷它们；用户切到这些 tab 也不会卡，因为它们的内容是 `Init()` 一次性写死的。

<a id="demo-taskmgr-events"></a>

### 13.6 事件路由汇总

| 事件 | 触发 | 处理 |
| --- | --- | --- |
| `WM_TIMER` | 200ms 周期 | 见 §13.5 4 步 |
| `DUIN_VALUECHANGED` (ctrlId == menuBar) | 用户在某 menu bar 下拉里选中一项 | `HandleMenuBarChosen(chosenId)`：File→退出 走 `SC_CLOSE`；其它 mock 提示 |
| `WM_SYSKEYDOWN` | Alt + 字母 | 转给 `m_menuBar->ProcessAltKey(vk)`，命中助记符则直接弹对应栏下拉 |
| `DUIN_DBLCLK` (ctrlId == 进程 tree) | 双击进程节点 | `ShowProcessProperties()` mock 属性框（PID / 用户 / CPU / 内存 / ...） |
| `DUITVN_RCLICK` (ctrlId == 进程 tree) | 右键进程节点 | `DuiMenu::TrackPopup`："结束任务 / 转到详细信息 / 打开文件位置 / 属性" |
| `DUITVN_COLUMNCLICK` (任意表格 tab) | 表头列名单击 | `DispatchColumnSort(ctrlId, col, asc)` 路由到对应 page 的 `SortByColumn`，业务侧 `stable_sort` 数据顺序数组 + `Clear()` 重建节点 + `SetSortIndicator` |
| `WM_DESTROY` | 关闭按钮 → SC_CLOSE → ... | `PostQuitMessage(0)`（库不主动投，业务 frame 必须自己挂；同 `guides.html` §[11](#frame-window-xml) 模板） |

<a id="demo-taskmgr-screenshot"></a>

### 13.7 命令行截图模式

本章所有 PNG 都由 `DemoTaskManager.exe --screenshot <outdir>` 自动生成。机制（`main.cpp::RunScreenshotMode`）：

1. 正常启动 frame、加载 XML、SetTimer，进入 `ShowWindow`；
2. 消息泵 1500ms 让 mock 数据填到 5 个采样点 + layout 稳定；
3. 对 7 个 tab 依次 `tab->SetCurSel(idx, true)` + `InvalidateRect` + 消息泵 500ms；
4. 每个 tab 用 `PrintWindow(hwnd, memDC, 0)` 抓整窗到 32bpp DIB，GDI+ 编码 PNG 存为 `demo-taskmgr-<tag>.png`；
5. 全部完成后立即退出（不进入主消息循环）。

实现复用了 `DuiGallery/CaptureMode.cpp` 的 GDI+ encoder 逻辑，简化了"capture mark 走 BitBlt 子矩形"那条路径 —— 这里只截整窗。

<a id="demo-taskmgr-vsdiff"></a>

### 13.8 与 Win10 真任务管理器的已知视觉差异

本 demo 不追求像素级 1:1 复刻；下面列出主要可见差异及当前选择，便于后续维护者判断是否要进一步打磨。"已贴近"指当前 demo 跟 Win10 视觉接近；"差异保留"指刻意未追平的取舍。

| 项 | Win10 真任务管理器 | 当前 demo | 状态 |
| --- | --- | --- | --- |
| 标题栏 | OS 自绘窗口标题栏 + 系统按钮 | `DuiFrameWindow` 自绘标题栏（与库其它 demo 一致） | **差异保留**（自绘是 demo 演示重点） |
| menu bar | 极扁平文字 + hover 一格浅灰 | `DuiMenuBar` hover `#E5E5E5` + active `#D7E3F4` 浅蓝（与 hover 区分） | 已贴近 |
| Alt+letter | 支持 | 支持（`WM_SYSKEYDOWN` 转发到 `ProcessAltKey`） | 已贴近 |
| menu bar 下拉切栏 | 下拉打开后鼠标移到隔壁栏自动切（同步 close + open） | 支持，依靠 `DuiMenu::SetSwitchZones` + popup 30ms 鼠标位置轮询 | 已贴近 |
| tab 头条 | 极扁平 + 蓝色 active 下划线（无白底） | 普通 tab 与底色一致 + 选中白底（无下划线） | **差异保留**（白底 active 视觉冲突小，库默认；加下划线需改 `DuiTab` paint 逻辑） |
| tab 头条字号 / 行高 | 9-10pt / 32-36px | 默认 9pt / 32px | 已贴近 |
| 表格 header | 几乎透明 + 1px 底线分隔 | `m_clrHeaderBg` 默认 `#FCFCFD`（接近白）+ 1px 底线（默认就有） | 已贴近 |
| 表格行高 | ≈ 28px | 默认 28px | 已贴近 |
| 列分隔线 | 极淡（几乎不画） | 列右侧 1px 浅灰（`m_clrGrid`） | 已贴近 |
| 选中行 | 浅蓝底 + 黑字 | 品牌蓝底 + 白字（`m_clrRowSel` = #2D6CDF） | **差异保留**（库默认全控件统一品牌色） |
| 进程图标 | 真实 exe icon | 字母 logo（A 蓝 / B 灰 / W 深灰） | **差异保留**（mock demo 不接 OS exe icon） |
| 性能页 sidebar 字号 | 标题 14pt / 当前值 18pt 加粗 | 同左（`kFontSizeSidebarTitle` = 14, `kFontSizeSidebarValue` = 18 加粗） | 已贴近 |
| 性能页 sidebar 行高 | ≈ 70px | 70px（`kSidebarItemH`） | 已贴近 |
| 性能页 detail 区背景 | 白底（与 sidebar 浅灰分层） | `PerformancePage::OnPaint` 先填白再让 splitter 子控件叠画 | 已贴近 |
| 性能页 mini sparkline | 无边框，更细 | 有 1px 浅灰边框（视觉提示这是图） | **差异保留**（小修，未做） |
| 性能页 chart 折线 | 下方半透明填充 | 仅折线，无填充 | **差异保留**（GDI 无 alpha；GDI+ 路径未实现） |
| 性能页 chart 网格 | 10×10 浅灰 | 10×10 浅灰（`kChartGridCols` / `kChartGridRows`） | 已贴近 |
| 状态栏 | 底部一行：进程数 / CPU% / 内存 | 同左 | 已贴近 |
| 窗口图标 (任务栏 / Alt-Tab) | 真实 taskmgr.ico | 字母 logo（蓝底 'T'，`WM_SETICON`） | 已贴近 |

整体观感约 80% 接近 Win10 真任务管理器；剩 20% 主要是"刻意保留的差异"（自绘标题栏 / 品牌色选中 / 字母 logo）和"未做的小修"（半透明填充 / sparkline 边框 / tab 蓝下划线）。后续若要 100% 追平，最需要做的是：

- `DuiTab` active 态加蓝色顶部下划线（要改库 paint 逻辑）
- `TaskMgrLineChart` 折线下方加 GDI+ 半透明填充
- `TaskMgrSidebarItem` 去掉 sparkline 边框

<a id="demo-taskmgr-gotchas"></a>

### 13.9 实战中踩过的坑

| 现象 | 根因 | 修复 |
| --- | --- | --- |
| 关闭按钮关不掉进程 | `DuiFrameWindow` 不主动 `PostQuitMessage`（一个进程可多 frame）。业务 frame 必须自己挂 `WM_DESTROY → PostQuitMessage(0)` | `main.cpp::OnDestroy_` |
| `DuiTabPage` / `DuiMenu` / `DuiSplitter` 链接缺导出 | 类声明少 `BUI_API`。DLL 模式（`BUI_USE_DLL`）下 ctor / 方法符号没导出 → `LNK2019` | 各类 .h 加 `BUI_API` |
| 水平滚动条不需要时还占 gutter | 库历史行为"永远预留" gutter 避循环依赖 | `DuiTreeView::EnsureScrollRanges` 加 `m_needHScroll` 二遍 layout，`BodyRect` 按需收 gutter |
| 截图模式下 outs 全空、tab 不显示 | incremental build 没把 `XmlFactory.h` 改动重新编译给 main.cpp 各 TU，struct 大小不一致 | 修改 `TaskMgrXmlOuts` 字段后做一次 `Rebuild`（不要靠 incremental） |

---

<a id="demo-xchat"></a>

## 14. 综合案例：XChat（某信 PC 复刻）

`third_party/XChat/` 用 balloonui 复刻某信 Windows PC 客户端界面，是除 DemoTaskManager 之外另一个完整的"参考贴近"案例。重点演示：<u>多视图主面板（聊天 / 联系人 / 公众号 / 空 chat 水印）的切换</u>、<u>session-list 驱动的右栏 view</u>、<u>HWND-hosted EDIT（搜索 / 输入框）</u>、<u>ChatScrollBar auto-hide</u> 以及大量<u>自绘 stroke icon / chat 气泡 / 文件卡 / 程序化"假图片"</u>。和 DemoTaskManager 互补 —— 后者偏 menu/tab/list 这种"工具型 UI"，本 demo 偏"消息流 + 弹层 + 大量自定义视觉元素"的"消费型 UI"。

![XChat 主面板聊天 view](images/xchat/main_chat.png)

*XChat 主面板（默认 chat view，1100×720）。三栏 hbox：左 75 nav / 中 290 session-list / 右 fill 多视图。所有 icon、气泡、文件卡、二维码消息全部 GDI+ stroke + GDI+ AA 自绘，<u>无 PNG 资源</u>。*

<a id="xchat-stack"></a>

### 14.1 涉及的库特性

| 特性 / 控件 | 用在哪里 |
| --- | --- |
| `DuiFrameWindow` | 登录窗 (360×500) + 主窗 (1100×720) + 设置窗 (800×680) + 汉堡 popup 三个独立顶层 frame |
| `DuiXmlBuilder` | 所有窗口都用 XML 描述：`login.xml` / `login_loading.xml` / `main.xml` / `main_contacts.xml` / `main_publics.xml` / `settings.xml` |
| `CustomFactory` | 每个 frame 都注册自家的 factory 把<u>自定义 tag</u>（`<color-panel>` / `<nav-icon>` / `<icon-button>` / `<chat-message-list>` / `<group-members>` ...）解析成对应自绘 DuiControl 子类 |
| `DuiVBox / DuiHBox / DuiGrid` | 所有 XML 容器；主面板三栏靠最外层 `<hbox>` |
| `DuiVirtualList` | 主面板中栏 session-list（21 个会话；rowH 68；自管 scrollbar + auto-hide） |
| `DuiScrollBar` | session-list 自带；ChatMessageList 内嵌；ContactCategoryList 内嵌。**都启用了 auto-hide**（默认隐藏，hover/wheel 渐入） |
| `DuiEditHost` | 左侧搜索栏（id=60）+ 右侧聊天输入区（id=400）；后者 multiline。两者通过 `EM_SETCUEBANNER` 走 placeholder（caller demo exe 必须挂 ComCtl32 v6 manifest） |
| `DuiSwitch` | 群信息栏"消息免打扰" + 设置页"保留聊天记录""自动下载"。<u>用 `alignCross="center"` 让 fixedHeight 真生效</u>，让 switch 在 32 高 hbox 里居中显示 |
| `DuiAvatar` | 登录窗 loading view 的 110×110 圆角矩形头像；主面板左 nav 顶部 38×38 用户头像 |
| `DuiLabel` | chat-title（id=300）；登录窗"扫码登录"/"仅传输文件"等文字（手动 SetTextAlign 居中）；info-field 的 label/value 两行 |
| `DuiMenu` | 左下汉堡 nav-icon 点击弹出 5 项菜单（聊天文件 / 聊天记录管理 / 锁定 / 意见反馈 / 设置） |
| `DuiAA / GDI+` | 所有非轴对齐路径（圆角矩形头像 / unread badge 胶囊 / mute bell 圆环 / chevron 三角 / chat 圆头像 / DuiSwitch 胶囊 / 8 种"假图片" / 文件 ext-icon / 水印双气泡）走 GDI+ AntiAlias |
| `DuiAnim` | 登录 spinner（旋转 240° 弧线）+ DuiSwitch 拨动 + DuiScrollBar fade in/out 都走 DuiAnimMgr。XChatMainFrame 在 WM_TIMER 16ms pulse 里调 `TickAll` |

<a id="xchat-files"></a>

### 14.2 文件清单

| 文件 | 作用 |
| --- | --- |
| `main.cpp` | WinMain：DPI v2 + GDI+ startup + ComCtl32 v6 manifest pragma；CLI arg 路由（`--settings` / `--contacts` / `--publics` / `--empty`）；串两个 GetMessage 循环：先跑 LoginFrame 直到关闭，再跑 XChatMainFrame |
| `LoginFrame.{h,cpp}` | 登录窗（QR + Loading 两 mode）。Mode::Qr 显二维码 + "扫码登录" + "仅传输文件"；3s 后 SwitchMode → Mode::Loading 显头像 + spinner + "正在进入"；再 3s 关窗 PostQuit |
| `MainFrame.{h,cpp}` | XChatMainFrame：主面板 frame，所有自定义控件 + factory 都<u>定义在本文件</u>（约 2200 行 —— 是 demo 最大文件）。包含：`ColorPanel` / `NavIcon` / `IconButton` / `SendButton` / `WatermarkPanel` / `WxIcons` / `LogoImage` / `FrequentContactsRow` / `ArticleList` / `ContactMgmtButton` / `ContactCategoryList` / `MemberCell` / `GroupMembersGrid` + 4 个 XML factory tag 解析器 + `ApplySessionView` / `ShowEmptyView` / `ShowHamburgerMenu_` |
| `SettingsFrame.{h,cpp}` | 设置弹窗（独立 DuiFrameWindow，800×680）。`RunModal()` 用 `EnableWindow(parent, FALSE)` + 自家 GetMessage 循环实现模态。<u>不能</u> PostQuitMessage —— WM_QUIT 是 thread-wide，会让 main 也退出（曾经踩过的坑） |
| `ChatMessageList.{h,cpp}` | 单聊消息流自绘控件 —— 不用 DuiVirtualList 因为消息变高（文本气泡/图片/转账卡/文章卡/日期分隔/文件卡 6 种 kind 高度差异大）。自管 per-message layout 缓存 + 内嵌 DuiScrollBar (auto-hide) |
| `SessionListItem.{h,cpp}` | session-list 单行 paint 函数（圆角矩形头像 + name + time + preview + bell/badge）。注册到 DuiVirtualList 的 PaintRowFn 回调 |
| `MockData.{h,cpp}` | 21 条会话的 mock + 8 个群的成员 mock。`Sessions()` lazy init 后从 `MessagesForSession()` 倒推每个 session 的 preview |
| `MockChat.{h,cpp}` | 17 段对话 mock（按 sessionIdx 索引）+ 6 种 ChatItemKind enum + ImageKind enum；helper：Self/Peer/Divider/SelfImage/PeerImage/SelfTransfer/PeerTransfer/PeerKnowledgeCard/SelfFile/PeerFile |
| `main.xml` / `main_contacts.xml` / `main_publics.xml` | 主面板三种顶层布局变体。`main.xml` 是默认（聊天 view）；其它两个是 CLI arg 入口或 nav 切换目标 |
| `login.xml` / `login_loading.xml` | 登录窗两个 mode 各自的布局 |
| `settings.xml` | 设置窗布局：左 140 tab + 右 fill 卡片区 |
| `res/` | 仅 `qr_placeholder.png` + `wechat_logo_gray.png`。其它视觉元素（icon / chat 图片 / 文件 ext icon）**全部程序化绘制**，不依赖外部资源 |

<a id="xchat-startup"></a>

### 14.3 启动流程

`main.cpp` 走"两个 GetMessage 循环串行"模式：

```
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR cmdLine, int nCmdShow)
{
    DuiDpi::OptInPerMonitorV2();
    OleInitialize(NULL);
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);
    Gdiplus::GdiplusStartup(&gpToken, &gsi, nullptr);

    // --settings 早路径：跳过 login + main，直接弹设置 frame（截图用）
    if (cmdLine 含 "--settings") { SettingsFrame.RunModal(nullptr); return 0; }

    // ── Phase 1：登录窗（QR → Loading → 关闭，PostQuit 退出第一个 GetMessage 循环） ──
    {
        LoginFrame login(LoginFrame::Qr);
        login.Create(...); login.ShowAndStartLoginSequence();
        login.ResizeClient(360, 500); login.CenterWindow(); login.ShowWindow(...);
        RunMsgLoop();   // 循环 1：到 LoginFrame OnDestroy_::PostQuitMessage
    }

    // ── Phase 2+：主窗 ──
    LPCTSTR initialXml = "main.xml";
    if (--contacts) initialXml = "main_contacts.xml";
    else if (--publics) initialXml = "main_publics.xml";

    XChatMainFrame frame;
    frame.Create(...);
    frame.LoadMainXml(initialXml);
    if (--empty) frame.ShowEmptyView();
    frame.ResizeClient(1100, 720); frame.CenterWindow(); frame.ShowWindow(...);
    RunMsgLoop();   // 循环 2：到 XChatMainFrame OnDestroy_::PostQuitMessage

    Gdiplus::GdiplusShutdown(gpToken);
    _Module.Term(); OleUninitialize();
}
```

两段 GetMessage 循环串行 比"一个循环 + 多 frame 共存"简单 —— 每个 frame OnDestroy_ 各自 PostQuit。缺点是 LoginFrame 关到 MainFrame 起之间约 1 帧视觉空档；做精致转场要换"双 frame 共存 + 显式信号"方案，本 demo 不必要。
**另一个 PostQuit 关键点**：SettingsFrame 这种"被主窗弹起的 modal" <u>绝不能</u> PostQuitMessage —— WM_QUIT 是 thread-wide，会让 main 的 RunMsgLoop 一并退出。SettingsFrame::OnDestroy_ 只翻 `m_modalRunning=false`，让自己的 RunModal while 循环 break 即可。

<a id="xchat-frames"></a>

### 14.4 三个独立 frame 总览

| frame | 尺寸 | 主 XML | visual |
| --- | --- | --- | --- |
| `LoginFrame(Qr)` | 360×500 | login.xml | vbox：占位二维码 + "扫码登录" 绿字（id=11） + spacer + "仅传输文件" 蓝字（id=12） |
| `LoginFrame(Loading)` | 360×500 | login_loading.xml | vbox：DuiAvatar 圆角头像 + 旋转 spinner + "正在进入" 绿字 |
| `XChatMainFrame` | 1100×720 | main.xml / main_contacts.xml / main_publics.xml | 三栏 hbox：75 nav / 290 中栏 / fill 右栏（多视图） |
| `SettingsFrame` | 800×680 | settings.xml | hbox：140 左 tab + fill 右内容（卡片） |

![XChat LoginFrame QR view](images/xchat/login.png)

*LoginFrame(Qr)：占位二维码 + 绿色"扫码登录" + 蓝色"仅传输文件"。3s 后自动 SwitchMode 到 Loading view。*

![XChat SettingsFrame](images/xchat/settings.png)

*SettingsFrame：左 140 tab（账号与存储 / 通用 / 快捷键 / 通知 / 插件 / 关于）+ 右卡片区。`RunModal` 用 `EnableWindow(parent, FALSE)` 实现模态。注意右上 DuiSwitch 用某信绿。*

<a id="xchat-main-layout"></a>

### 14.5 主面板布局（main.xml）

主面板是整个 demo 最复杂的布局。最外层是水平三栏：

```
<frame-window min-w="800" min-h="540" resizable="true" ...>
  <hbox padding="0" gap="0">

    <!-- ① 左 nav 栏（75 宽） -->
    <color-panel color="46,46,46" fixedWidth="75" padding="0,15,0,15" gap="6">
      <hbox fixedHeight="48"> <control weight="1"/><user-avatar fixedWidth="38" fixedHeight="38"/><control weight="1"/> </hbox>
      <nav-icon id="90" selected="true" glyph="chat-bubble"/>       <!-- 聊天 -->
      <nav-icon id="92" glyph="people"/>                            <!-- 联系人 -->
      <nav-icon glyph="box"/>                                       <!-- 收藏 -->
      <nav-icon glyph="aperture"/>                                  <!-- 朋友圈 -->
      <nav-icon glyph="video-bookmark"/>                            <!-- 视频号 -->
      <nav-icon glyph="eye-look"/>                                  <!-- 看一看 -->
      <nav-icon glyph="mini-prog"/>                                 <!-- 小程序 -->
      <control weight="1"/>                                         <!-- 弹性 spacer -->
      <nav-icon glyph="phone"/>                                     <!-- 手机端 -->
      <nav-icon id="91" glyph="hamburger"/>                         <!-- ≡ 弹菜单 -->
    </color-panel>

    <!-- ② 中栏（290 宽） -->
    <color-panel color="245,245,245" fixedWidth="290" padding="0,16,0,0" gap="0">
      <hbox fixedHeight="32" gap="8" padding="12,0,12,0">
        <edit id="60" placeholder="搜索" weight="1"/>
        <icon-button glyph="plus" fixedWidth="32"/>
      </hbox>
      <control fixedHeight="12"/>          <!-- spacer，避免选中行紧贴搜索框 -->
      <session-list weight="1" id="50"/>   <!-- DuiVirtualList，21 行 -->
    </color-panel>

    <!-- ③ 右栏（fill）—— 容器装 4 个互斥 pane -->
    <vbox weight="1" padding="0" gap="0">
      <hbox id="200" weight="1"> ... 聊天 view ... </hbox>
      <color-panel id="202" weight="1"> ... 公众号 view ... </color-panel>
      <watermark id="204" weight="1"/>   <!-- 空 chat 双气泡水印 -->
    </vbox>

  </hbox>
</frame-window>
```

4 个 pane 由 `XChatMainFrame::ApplySessionView(int sessionIdx)` 互斥切换：

| 触发 | 显示 pane |
| --- | --- |
| session 是 `Page_Single` | 200（chat） + 隐 210/202/204 |
| session 是 `Page_Group` | 200（chat） + 210（群信息侧栏） + 隐 202/204 |
| session 是 `Page_Publics` | 202（公众号） + 隐 200/204 |
| `ShowEmptyView()` / `--empty` | 204（水印） + 隐 200/202；session-list 也清选中 |

四种 view 的实际渲染效果：

![XChat 空 chat 水印 view](images/xchat/main_empty.png)

***空 chat（pane 204 / `--empty`）**：右栏只剩 `WatermarkPanel` 程序化绘制的"双气泡 + 6 个小点"灰色水印。session-list 选中态也清空。*

![XChat 联系人 view](images/xchat/main_contacts.png)

***联系人 view（`main_contacts.xml` / `--contacts`）**：中栏从 session-list 换成 `ContactCategoryList`（7 大分类，"联系人"分类 lazy 生成 3071 个水浒主题虚构联系人）。*

![XChat 公众号 view](images/xchat/main_publics.png)

***公众号 view（pane 202 / `main_publics.xml` / `--publics`）**：上方 `FrequentContactsRow`（6 圆头像 + "更多 >"）+ 下方 `ArticleList`（3 张文章卡）。*

![XChat 聊天 view](images/xchat/main_chat.png)

***聊天 view（pane 200 / `main.xml`）**：右栏 chat 区显示 `ChatMessageList` 6 种 ChatItemKind（文本气泡 / 假图 / 文件卡 / 二维码图 / 转账卡 / 日期分隔），底部 toolbar + 真 EDIT 输入框 + 发送按钮。*

**SetVisible 的 layout 陷阱**：DUI 的 `SetRect` 在 `EqualRect` 时会 short-circuit 跳过 Layout —— 切 pane 后右栏 vbox rect 没变，子项就不会重新算位置，旧 pane 残留。
解决 = 显式调 `ForceLayout(rect)` 跳过 EqualRect 短路。本 demo `ApplySessionView` 末尾对右栏 vbox 和 paneChat 各 force 一次。这条经验同时让 library 在 DuiControl 加了 `ForceLayout` 接口供别处复用。

<a id="xchat-chat-pane"></a>

### 14.6 聊天 pane (id=200) 内部布局

左半 chat 内容 + 右半 270 群信息栏（id=210，仅群聊显示）：

```
<hbox id="200" weight="1">

  <!-- 左半：chat 内容 -->
  <color-panel color="255,255,255" weight="1">

    <!-- 6.1 chat header -->
    <hbox fixedHeight="60" gap="4" padding="24,0,16,0">
      <chat-title id="300" weight="1"/>             <!-- DuiLabel，按 session 名动态 -->
      <icon-button glyph="emoji"      fixedWidth="32"/>
      <icon-button glyph="phone-call" fixedWidth="32"/>
      <icon-button glyph="dots"       fixedWidth="32"/>
    </hbox>
    <color-panel color="232,232,232" fixedHeight="1"/>     <!-- 1px 灰分隔线 -->

    <!-- 6.2 消息流 -->
    <chat-message-list weight="1"/>                       <!-- 自绘，含 auto-hide scrollbar -->

    <!-- 6.3 输入区上方 toolbar -->
    <hbox fixedHeight="38" gap="4" padding="10,4,10,4">
      <icon-button glyph="emoji"/><icon-button glyph="folder"/>
      <icon-button glyph="scissors"/><icon-button glyph="mic"/><icon-button glyph="phone-call"/>
      <control weight="1"/>
    </hbox>

    <!-- 6.4 输入区（真 EDIT） -->
    <edit id="400" multiline="true" fixedHeight="92"/>

    <!-- 6.5 底部 send 行 -->
    <hbox fixedHeight="48" padding="14,4,14,12">
      <icon-button glyph="phone" fixedWidth="32"/>
      <control weight="1"/>
      <send-button text="发送" fixedWidth="64" fixedHeight="30"/>
    </hbox>
  </color-panel>

  <!-- 右半：群信息侧栏（id=210，Page_Group 才显） -->
  <hbox id="210" fixedWidth="271">
    <color-panel color="232,232,232" fixedWidth="1"/>     <!-- 左侧 1px 分隔 -->
    <color-panel color="255,255,255" fixedWidth="270" padding="14,18,14,14" gap="14">
      <color-panel color="240,240,240" fixedHeight="30"/>        <!-- avatar 占位 -->
      <group-members id="220" fixedHeight="152"/>                <!-- 2×4 grid 动态 -->
      <info-field label="群聊名称" value="..." fixedHeight="46"/>
      <info-field label="群公告"   value="..." fixedHeight="46"/>
      <info-field label="备注"     value="..." fixedHeight="46"/>
      <info-field label="我在本群的昵称" value="..." fixedHeight="46"/>
      <info-field label="查找聊天内容"  value=""    fixedHeight="46"/>
      <info-mute-row label="消息免打扰" fixedHeight="32"/>        <!-- DuiSwitch + label -->
      <control weight="1"/>
    </color-panel>
  </hbox>

</hbox>
```

<a id="xchat-customs"></a>

### 14.7 自定义控件清单（按职责）

| 控件 | 替代的 XML tag | 实现方式 | 说明 |
| --- | --- | --- | --- |
| `ColorPanel` | `<color-panel color="r,g,b">` | 派生 `DuiVBox`，OnPaint 先 FillRect 自身 m_rcItem 再走基类 | 给 vbox/hbox 容器加纯色背景 —— 三栏底色 / 群信息栏白底 / 聊天底 |
| `NavIcon` | `<nav-icon glyph="...">` | 派生 `DuiControl`，自绘 stroke icon + 选中绿条 | 左 nav 9 个图标（聊天/联系人/收藏/朋友圈/.../汉堡）；glyph 字符串映射到 `WxIconKind` 枚举 |
| `WxIcons::Paint(hdc, rc, kind, color)` | —（helper） | 17 种 GDI+ stroke 路径，按 24×24 viewport 缩放到任意 rect | chat-bubble / people / box / aperture / video-bookmark / eye-look / mini-prog / phone / hamburger / emoji / folder / scissors / mic / plus / phone-call / dots —— **全 stroke 画法，无 PNG 资源** |
| `IconButton` | `<icon-button glyph="..."/>` | 派生 `DuiControl`，hover 加浅灰圆角底 + 中央 WxIcon | chat header 右上 3 icon、底部 toolbar 5 icon、搜索栏 + 号 |
| `SendButton` | `<send-button text="发送"/>` | 圆角浅灰底 + 居中文字 | 聊天底部右下"发送"按钮 |
| `WatermarkPanel` | `<watermark/>` | GDI+ FillPath 画两个互叠的圆角对话气泡 + 6 个小点 | 空 chat 状态（pane 204）的"某信" 双气泡灰水印 |
| `SessionListItem::PaintSessionRow()` | —（DuiVirtualList paint 回调） | 圆角矩形头像（GDI+ AA path）+ name + time + preview + bell 圆环 / 红 badge 胶囊 | session-list 单行 paint；选中态浅灰 #E4E4E4 / hover 浅灰 #EE |
| `ChatMessageList` | `<chat-message-list/>` | 派生 `DuiControl`，per-message 高度缓存 + 内嵌 DuiScrollBar(auto-hide) | 支持 6 种 ChatItemKind（Text 气泡 / Image 假图 / TransferCard / KnowledgeCard / DateDivider / File） |
| `WxIcons + GroupMembersGrid` | `<group-members id="220"/>` | 派生 `DuiControl`，按 sessionIdx 取 Sessions().members 画 2×4 grid | 群信息侧栏成员 grid；前 6 真成员 + "添加" / "移除" 占位 |
| `ContactCategoryList` | `<contact-tree/>` | 派生 `DuiControl`，7 个分类各自 mock children；可点击展开/折叠 + 内嵌 auto-hide scrollbar | 联系人页中栏。"联系人"分类 lazy 生成 3071 个水浒主题虚构联系人 |
| `ContactMgmtButton` | `<contact-mgmt-button/>` | 白底圆角 + 1px 边 + 中央"两个圆 + 通讯录管理" | 联系人页顶部大按钮 |
| `FrequentContactsRow` | `<freq-contacts/>` | 横排 6 个 38×38 圆头像 + 名字 + 右上"更多 >" | 公众号 view 的"常看的号"行 |
| `ArticleList` | `<article-list/>` | 3 张文章卡片纵排，每卡 ~150 高（avatar + 公众号名 + 时间 + 标题 + 预览 + 缩图占位 + footer） | 公众号 view 文章列表 |
| `MemberCell` | `<member-cell letter="X" bg="..." name="..."/>` | 圆角矩形头像（GDI Rgn） + 单字 + 下方名字。<u>已被 GroupMembersGrid 替代</u> 但 factory 仍保留兼容 | 群信息栏老结构成员 cell；`placeholder="+/-"` 走虚线框 + glyph 路径 |
| `chat-title` / `info-field` / `info-mute-row` | 同名 tag | factory 直接组合 DuiLabel / DuiVBox / DuiHBox / DuiSwitch | 没有独立类，是"在 factory 里临时拼装的复合控件" |
| `SettingsTab` / `SettingsCard` / `SettingsButton` / `BgPanel` | 同名 tag（在 SettingsFrame 内） | 独立各 30-50 行的 DuiControl 子类 | 设置弹窗专用，不在 MainFrame.cpp 而在 SettingsFrame.cpp |

<a id="xchat-painted-vs-xml"></a>

### 14.8 自绘 vs XML+代码 速查

| 区域 | 怎么实现 |
| --- | --- |
| 整体三栏布局 | **纯 XML**：hbox + 三个带 weight/fixedWidth 的 color-panel |
| 左 nav 9 个 icon + 选中绿条 | **XML 占位 + 代码自绘**：XML 给 9 个 `<nav-icon glyph="...">`；NavIcon::OnPaint 走 WxIcons::Paint 用 GDI+ stroke 画 |
| session-list（21 行会话） | **library + 自绘 row**：DuiVirtualList 给 row 容器；SessionListItem::PaintSessionRow 自绘单行（头像 + name + time + preview + bell/badge） |
| chat-title 文本 | **纯 library**：DuiLabel |
| chat header 右上 3 个 icon | **XML + 代码自绘**：3 个 `<icon-button glyph="..."/>`；IconButton 用 WxIcons 画 |
| 消息流（气泡 / 卡 / 图片 / 文件） | **纯代码自绘**：ChatMessageList 自管 layout + 6 种 paint 函数（PaintTextBubble / PaintImage / PaintTransferCard / PaintKnowledgeCard / PaintDateDivider / PaintFile） |
| chat 中假图片（火锅/风景/操场/二维码/...） | **纯代码自绘**：8 种 ImageKind 各对应一段 30-50 行 GDI+ 路径，<u>不依赖 PNG 资源</u> |
| 文件卡 ext-icon（DOCX/PDF/...） | **纯代码自绘**：FileExtColor(ext) 按后缀返色 → PaintFileExtIcon 画圆角矩形 + header band + 居中白色 ext 大字 |
| 聊天输入框 / 搜索框 | **library + caller wire**：<edit id="60/400"/>；MainFrame.LoadMainXml 末尾 `FindCtrlById` + `EnsureCreated(m_hWnd)` 真创建 EDIT HWND；placeholder 由 BuildEdit 透到 SetPlaceholder → SyncPlaceholderToHwnd → EM_SETCUEBANNER |
| 群信息栏 info-field（label + value 两行） | **XML 复合 + factory**：<info-field> 在 factory 里组装 DuiVBox + 2 个 DuiLabel |
| 群信息栏成员 grid | **纯代码自绘**：GroupMembersGrid 按 sessionIdx 取 Sessions().members，2×4 grid（前 6 + 添加/移除占位） |
| 群信息栏"消息免打扰" | **XML 复合 + library**：<info-mute-row> factory 组装 DuiHBox + DuiLabel + DuiSwitch（带 alignCross="center"） |
| 设置卡片 / 按钮 / Tab | **XML + 自绘控件**：SettingsFrame 自家 factory 接 5 个 tag (settings-tab / settings-card / settings-btn / settings-switch / settings-bg)，每个对应一个 30-50 行的 DuiControl 子类 |
| 汉堡菜单弹层 | **library**：DuiMenu::TrackPopup 同步阻塞，AppendItem 加 5 项 |
| 设置 frame 模态 | **caller 实现 modal**：EnableWindow(parent, FALSE) + 自家 GetMessage 循环 + OnDestroy 翻 m_modalRunning（<u>不</u>能 PostQuitMessage） |
| 登录 spinner | **纯代码自绘**：旋转 240° 弧线，DuiAnim 1 小时长 anim 当 60Hz pulse 用，OnTick 把 GetTickCount 折成相位 |
| 空 chat 水印 | **纯代码自绘**：WatermarkPanel 用 GDI+ FillPath 画两个互叠对话气泡 |

<a id="xchat-events"></a>

### 14.9 事件路由与切换流程

| 用户操作 | 事件路径 | 结果 |
| --- | --- | --- |
| 点 session-list 某行 | DuiVirtualList row 选中 → DUIN_VALUECHANGED 给父 → XChatMainFrame::OnDuiNotify_ → ApplySessionView(idx) | 右栏切到对应 pane（chat/group/publics）+ 切 ChatMessageList 数据源 + 切 GroupMembersGrid 数据源 |
| 点左 nav "聊天" (id=90) | NavIcon::OnLButtonUp → NotifyParent(DUIN_CLICK) → OnDuiNotify_(wp=90) → LoadMainXml("main.xml") | 整个客户区树重建为 main.xml（默认主面板） |
| 点左 nav "联系人" (id=92) | 同上，wp=92 → LoadMainXml("main_contacts.xml") | 客户区重建为联系人页（中栏分类 + 右栏空白） |
| 点左 nav "汉堡" (id=91) | 同上，wp=91 → ShowHamburgerMenu_() | DuiMenu::TrackPopup 5 项；选"设置"(1006) 弹 SettingsFrame.RunModal(m_hWnd) |
| chat 区滚滚轮 | WM_MOUSEWHEEL → DuiHost::OnMouseWheel <u>按鼠标位置</u> HitTopMost → ChatMessageList::OnMouseWheel → m_sb->SetPos | chat 区滚动；session-list 不动（修复前是 focus 优先 → bug） |
| session-list 滚滚轮 | 同上，命中目标变成 DuiVirtualList → 它的 m_sb 滚动 + TriggerShow（auto-hide 渐入） | session-list 滚动；scrollbar 渐入 800ms 后渐出 |
| 点击 ContactCategoryList 主分类行 | OnLButtonUp → toggle m_expanded[i] → UpdateScrollRange_ + Invalidate | 分类下方展开/折叠 children；scrollbar range 同步更新 |

<a id="xchat-mock"></a>

### 14.10 Mock 数据

所有数据写死在 cpp，进程级 static lazy init，永远拿 const&：

| 数据 | 位置 | 规模 |
| --- | --- | --- |
| 会话列表（21 条） | `MockData.cpp Sessions()` | name/preview/timeText/avatar/isGroup/muted/unread/pageType/members |
| 群成员（8 个群每群 8 名） | 同上 v[i].members | 各群独立身份贴合（家长群、技术群、PM/UI/QA、教练 + 会员 ...） |
| session preview 自动生成 | Sessions() 末尾循环 | 取 MessagesForSession(i) 倒序首条非 DateDivider，按 kind 生成 preview（"[图片]" / "[转账] ..." / "[文章] ..." / "[文件] ..."），群里 self 加 "我: " 前缀 |
| 对话流（17 段） | `MockChat.cpp MessagesForSession(idx)` | 每段 6-25 条；6 种 kind 混用；publics 三个空 vector |
| 联系人列表（3071） | `MainFrame.cpp ContactCategoryList::Cats()` | 水浒主题：前 108 真名（宋江/卢俊义/...）+ 30 个绰号 × 99 姓 = 2970 后续 |
| 公众号"常看的号" / 文章卡 | `FrequentContactsRow / ArticleList` | 各 6 个 / 3 张写死 |

---

<a id="demo-cloudmelody"></a>

## 15. 综合案例：CloudMelodyDesktop（音乐 App demo）

`third_party/CloudMelodyDesktop/` 用 balloonui 复刻一款音乐 App（"FangMusic"），设计稿来源 `third_party/cloud_melody_desktop/stitch_cloud_melody_desktop/music_*/` 共 8 张界面。和 DemoTaskManager / XChat 互补 —— 后两者偏"信息密集型 UI"（菜单 / 列表 / 表格），本 demo 偏"卡片 / 媒体内容 + 动效"的<u>消费型 UI</u>，演示：<u>多页面路由切换（ContentRouter）</u>、<u>真实 mock 播放计时</u>、<u>GDI+ 抗锯齿自绘控件（旋转黑胶 / 圆形播放按钮 / 色板渐变封面）</u>、<u>UpdateLayeredWindow 逐像素 alpha 桌面浮窗（桌面歌词）</u>、<u>全屏沉浸模式</u>、<u>DuiEditHost 内联图标 API 应用（搜索 pill）</u>。

**注**：以下截图来自<u>设计稿</u>（`cloud_melody_desktop/stitch_cloud_melody_desktop/music_*/screen.png`）。实际运行效果（`third_party/Bin/CloudMelodyDesktop.exe`）与设计稿基本一致 —— 同样的色板、同样的布局、同样的字号梯度；细节（卡片 hover / 按钮 active 态等）按设计稿语义实现。

<a id="cmd-stack"></a>

### 15.1 涉及的库特性

| 特性 / 控件 | 用在哪里 |
| --- | --- |
| `DuiFrameWindow` | 1100×720 主窗，Win11 Mica 系统背景（`DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE)`）+ 圆角（`DWMWCP_ROUND`） |
| `DuiXmlBuilder` | 主框架走 `main.xml`：`<frame-window> <vbox>` + 自定义 tag `<top-bar> <sidebar> <content-router> <player-bar>` |
| `CustomFactory` | 4 个自定义 tag 在 `MakeMainXmlFactory()` 注册，每个返回一个对应的复合控件根 |
| `DuiVBox / DuiHBox` | 所有页面内部 layout、卡片网格、播放栏 transport 行 / 进度行 |
| `DuiLabel` | 所有静态文字（标题 / 副标 / 链接 / section header / 时长 label）。配合 `SetFont` 用 `CloudMelodyFonts.h` 6 档梯度（DisplayLg/HeadlineMd/TitleSm/BodyMd/BodySm/LabelXs） |
| `DuiAvatar` | TopBar 用户头像（圆形 fallback "张"）+ ProfilePage 大头像（96px） |
| `DuiEditHost` | TopBar 搜索 pill 内嵌真 EDIT —— <u>用本期新加的内联图标 API</u> 装左侧放大镜（`SetIcon(LeftIcon, 32, painter)`）+ `SetBgColor` 让 EDIT 底色与 pill 灰底融合 + `SetShowBorder(false)` 关 EDIT 默认 1px 边框 |
| `DuiTab` | LocalMusic 的 "全部歌曲 / 歌手 / 专辑 / 正在下载"；Favorites 的 "全部 / 歌单 / 艺人"；NowPlaying 的 "歌词 / 视频" |
| `DuiSeparator` | 各页 section 分隔线、单曲列表表头下方 |
| `DuiSlider` | PlayerBar 音量条（关掉 `SetTabStop(false)`，让点击边缘不画虚线焦点框） |
| `DuiToolTip` | 所有 IconButton 按钮（PlayerBar transport 5 个、TopBar back/forward/⚙）通过 `DuiToolTipMgr::Inst().Register()` 挂中文提示 |
| `DuiPaintAA` | 所有非轴对齐图形（PlayCircleButton 红圆+白三角、RotatingDisc 黑胶纹路、HoursBarChart 柱图圆角、CoverArt 渐变 path）走 GDI+ 抗锯齿 —— 沿用 `DuiAA::FillEllipse / FillPolygon / DrawLine` |

<a id="cmd-files"></a>

### 15.2 文件结构

| 路径 | 作用 |
| --- | --- |
| `main.cpp` | 入口：DPI v2、GDI+ startup、ComCtl32 v6 manifest、生成 16×16 红方块"F" icon、`frame.Create + SetTitle("FangMusic · Your Sound, Your Way") + SetIcon` + 单 GetMessage 循环 |
| `MainFrame.{h,cpp}` | `CloudMelodyMainFrame : DuiFrameWindow`。msg map：`WM_CREATE` 起 4Hz 播放 timer + 30Hz 动画 timer + Win11 Mica/圆角；`WM_DUI_NOTIFY` 路由 sidebar nav / playerbar transport / fullscreen / desktop-lyrics / nowplay-back；`WM_TIMER` 推 PlayerBar.Tick + 桌面歌词同步。`ToggleFullscreen_()`：隐藏 TopBar/Sidebar + 切 NowPlay + SW_MAXIMIZE |
| `main.xml` | 主框架顶层布局 —— 见 §15.3 |
| `App/CloudMelodyTheme.h` | 设计令牌集中：所有色（kColor*）、尺寸（kSize*）、圆角（kRadius*）、字体名 kFontFaceCJK = Microsoft YaHei |
| `App/CloudMelodyFonts.{h,cpp}` | 6 档进程级 HFONT 单例缓存：DisplayLg(30/700) / HeadlineMd(22/600) / TitleSm(16/600) / BodyMd(14/400) / BodySm(13/400) / LabelXs(11/500)。`Fonts::Get(kind)` 懒构造 + cache。`MakeLabel(text, color, kind)` 便捷 helper |
| `App/Sidebar.{h,cpp}` | 左 220px 导航栏。private 类 `SidebarNavItem : DuiControl` 自绘 active 4px 红条 + tint 底；自绘 8 种 nav 图标（NavIconKind 枚举：Discover/Podcast/Music/Favorites/Recent/Profile/Settings/Help，全 GDI+ 1.5px stroke）。`BuildSidebar(initialNav)` 工厂返根 |
| `App/TopBar.{h,cpp}` | 顶 48px 工具条。`SearchPill : DuiVBox` 自绘圆角 pill 灰底 + 1px 描边，内嵌一个 `DuiEditHost` 用[新内联图标 API](#DuiEditHost-IconApi) 装左 magnifier。`BuildTopBar()` 工厂 |
| `App/PlayerBar.{h,cpp}` | 底 80px 播放栏。`PlayerBar : DuiHBox` 自身就是控件类，缓存 6 个子指针（cover / title / sub / 时长 left/right / progress / playBtn / shuffleBtn）。public 方法：`Tick(deltaMs) / OnPlayClicked / OnPrevClicked / OnNextClicked / OnSeek / OnShuffleClicked`。状态：m_trackIdx / m_durationSec / m_posMs / m_playing / m_playMode（4 模式循环） |
| `App/ContentRouter.{h,cpp}` | 中部页面路由器 `: DuiVBox`。`Switch(navId, extra)`：销毁旧 page → 调对应 `Build*Page()` 建新 page → AddChild + ForceLayout。NavId 枚举见 Sidebar.h（100..120） |
| `App/DesktopLyricsWnd.{h,cpp}` | 独立 ATL `CWindowImpl` 顶层浮窗。`WS_POPUP + WS_EX_TOPMOST + WS_EX_LAYERED + WS_EX_NOACTIVATE`。<u>UpdateLayeredWindow 逐像素 alpha</u> 模式：背景全透 + 文字白带黑 1px 描边 + 左 60% 绿色"已唱"。从任意区域可拖（HTCAPTION 全捕） |
| `App/Mock/MockMusic.{h,cpp}` | 5 个精选歌单 + 6 首 "最新音乐"（含 durationSec 字段）+ 8 行歌词（kMockLyrics）+ 当前高亮行下标 |
| `Pages/DiscoverPage.{h,cpp}` | music_2 复刻：banner（红底圆角 + 标题副标 + 立即播放/了解更多 PillButton）+ 5 张精选歌单 MusicCard + 6 行最新音乐 |
| `Pages/LocalMusicPage.{h,cpp}` | music_8：标题"本地音乐" + "共 1,248 首歌曲" + 播放/随机播放 + DuiTab + 单曲表（label 行模拟 6 列） |
| `Pages/FavoritesPage.{h,cpp}` | music_3：大封面 + PLAYLIST tag + "我喜欢的音乐" + 副标 + 播放/分享 + DuiTab + 6 张艺人卡 + 单曲列表 |
| `Pages/RecentPlayPage.{h,cpp}` | music_6：标题 + 红 hero "这就是我"专辑卡 + 灰最近播客 chip + 清除全部/播放全部 + 6 行单曲 |
| `Pages/ProfilePage.{h,cpp}` | music_5：头像 + 张小方 + LEVEL 18 chip（自绘小胶囊，不用 DuiBadge —— 后者硬截 4 字符）+ 关注/粉丝/听歌 stats + 红色"最爱风格"卡 + 24 列 HoursBarChart + 4 张歌单 + "+ 创建歌单"占位 |
| `Pages/PodcastPage.{h,cpp}` | music_4：红 banner "声动早咖啡" + 2 块 chip 卡 + 6 个分类胶囊（PillButton Primary / Secondary）+ 5 张热门节目网格 |
| `Pages/NowPlayingPage.{h,cpp}` | music_7：← 返回 + 居中 RotatingDisc 360px + 歌名 / 歌手 + ♡ ☆ ｜ DuiTab + 8 行歌词（current 行红+大） |
| `Controls/PillButton.{h,cpp}` | 三 style 胶囊按钮：Primary（品牌红）/ Secondary（灰描边）/ OnDark（透明 + 白描边，用在红底 banner） |
| `Controls/IconButton.{h,cpp}` | 32×32 透明底 icon 按钮。glyph 是 Unicode 字符（"‹" "›" "♡" "⏭" "⚙"...）。hover 浅灰底（圆形 / 圆角矩形可选）。SetTooltip 透传 DuiToolTipMgr |
| `Controls/PlayCircleButton.{h,cpp}` | 40px 红圆 + 白三角（▶）/ 双白圆角矩形（⏸）。GDI+ 抗锯齿。click 切 m_playing。toggle 模式 |
| `Controls/PlayerProgressBar.{h,cpp}` | 派生 `DuiControl`（不用 DuiSlider 因为不要 thumb）。track 高 idle 2px / hover 4px；elapsed 红、剩余浅灰；拖动发 DUIN_VALUECHANGED |
| `Controls/MusicCard.{h,cpp}` | 方卡：CoverArt 程序化渐变封面（按 hueSeed 选 6 套色板）+ 标题 + 副标。hover 时右下角浮 GDI+ 抗锯齿 ▶ 圆形 badge |
| `Controls/CoverArt.{h,cpp}` | 程序化封面 painter helper。`CoverArt::Paint(hdc, rc, title, hueSeed, cornerRadius)`：6 套预设色板（红/紫/橙/青/绿/粉），LinearGradientBrush + GraphicsPath 圆角 + 黑色阴影白字标题 |
| `Controls/RotatingDisc.{h,cpp}` | 黑胶旋转：33⅓ RPM = 200°/s。OnPaint 用 GDI+ `RotateTransform` 把内圆 + 中央封面方块跟着 m_angle 转；外环 + 同心纹路 + 中央主轴洞不旋转 |
| `Controls/HoursBarChart.{h,cpp}` | 24 列听歌时段柱图。OnPaint 直接 `FillRect`（柱子轴对齐、不需 AA）。柱顶 3px 圆角走 PaintHelpers::FillRoundedRect |
| `Controls/PaintHelpers.{h,cpp}` | app 自带的 `FillRoundedRect / DrawRoundedRectBorder / FillEllipse` 抗锯齿 helper（用 GDI+ GraphicsPath，替代 GDI `CreateRoundRectRgn + FillRgn`，避免楼梯锯齿） |

<a id="cmd-arch"></a>

### 15.3 整体架构

主窗顶层布局（`main.xml`）：

```
<frame-window min-w="900" min-h="640" resizable="true"
              has-min-button="true" has-max-button="true" has-close-button="true">
  <vbox padding="0" gap="0">
    <top-bar      id="10" fixedHeight="48"/>          <!-- 自定义 tag -->
    <hbox weight="1" padding="0" gap="0">
      <sidebar        id="20" fixedWidth="220"/>       <!-- 自定义 tag -->
      <content-router id="30" weight="1"/>             <!-- 自定义 tag -->
    </hbox>
    <player-bar   id="40" fixedHeight="80"/>          <!-- 自定义 tag -->
  </vbox>
</frame-window>
```

整张 XML 不到 20 行 —— 4 个自定义 tag 把所有重活托给 C++ factory：

```
// MainFrame.cpp
DuiXmlBuilder::CustomFactory MakeMainXmlFactory()
{
    return [](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
    {
        if (n.tag == "top-bar")       return BuildTopBar();
        if (n.tag == "sidebar")       return BuildSidebar(NavId_Discover);
        if (n.tag == "content-router")return std::make_unique<ContentRouter>();
        if (n.tag == "player-bar")    return BuildPlayerBar();
        return nullptr;
    };
}
```

路由：用户点 Sidebar nav item → bubble `DUIN_CLICK` 给 host →
`MainFrame::OnDuiNotify_` 按 ctrl id 区分（NavId_Discover..Profile）→ 调
`ContentRouter::Switch(navId)`。Router 内部销毁旧 page 子树（`RemoveChild`）+ 调 `BuildXxxPage()` 建新树 + AddChild + ForceLayout。

<a id="cmd-layout-strategy"></a>

### 15.4 布局策略：XML / 代码 / 自绘 三档分工

| 方式 | 用在哪 | 原因 |
| --- | --- | --- |
| **XML** | 仅顶层 `main.xml`（4 自定义 tag）+ 4 大区的"<u>有/无</u>"声明 | 结构固定、与设计稿语义一一对应、看 XML 就能知道窗框形态 |
| **代码 layout** | 所有页面内部（DiscoverPage / NowPlayingPage 等 8 个 page）+ Sidebar / TopBar / PlayerBar 这 3 个区 | 内容数量来自 mock 数据（5 张 vs 6 张卡），用循环 build 比 XML 列举清晰；条件分支（active/inactive nav 项）也是代码更直观 |
| **自绘控件** | 10 个业务专用控件（PillButton / IconButton / MusicCard / RotatingDisc / HoursBarChart / SidebarNavItem / SearchPill / PlayerProgressBar / PlayCircleButton / DesktopLyricsWnd） | balloonui 没有现成的"音乐 App pill 风按钮 / 播放圆按钮 / 旋转黑胶 / 桌面浮歌词"控件 —— 这些是业务专用的<u>新</u>视觉/交互形态，最适合直接派生 DuiControl 或 ATL CWindowImpl 自绘 |

除了上述三档，还有第 4 类介于 XML 和代码之间：<u>自定义 tag + 工厂构建子树</u> —— top-bar / sidebar / player-bar 等"复合控件"用这种方式，让 XML 顶层语义干净（一个 tag 表达一个区域），构建细节藏在 C++ 工厂函数里。这是 balloonui CustomFactory 的典型用法。

<a id="cmd-pages"></a>

### 15.5 各页面拆解

<a id="cmd-page-frame"></a>

#### 15.5.0 主框架（TopBar / Sidebar / PlayerBar 常驻）

![主框架 + DiscoverPage](images/cloudmelody/discover.png)

*主框架 = 顶 48px TopBar（搜索 pill / 头像 / ⚙）+ 左 220px Sidebar（品牌已挪到标题栏）+ 中部 ContentRouter（Discover 默认页）+ 底 80px PlayerBar（封面/transport/进度/音量/歌词等）。所有页面切换都只发生在 ContentRouter 区。*

<a id="cmd-page-discover"></a>

#### 15.5.1 DiscoverPage（发现）

![Discover 设计稿](images/cloudmelody/discover.png)

*Banner (红底 / 圆角 / [精选] 标 / 大标题 + 副标 + 立即播放 + 了解更多) + "精选歌单" section 标题 + 5 张 MusicCard 横排 + "最新音乐" section + 6 行单曲。*

| 区域 | 用什么 | 布局方式 |
| --- | --- | --- |
| Banner 红底圆角 | 自绘 HeroBanner（DuiVBox 派生，`OnPaint` 用 `PaintHelpers::FillRoundedRect` 画 kColorPrimary 圆角矩形） | 代码（mock 标题文字） |
| Banner 标题 + 副标 + tag | 3 个 DuiLabel | 代码（VBox 串） |
| Banner CTA 按钮 | 2 个 PillButton（Primary 立即播放 / OnDark 了解更多 透明 + 白边） | 代码 |
| Section 标题 + 查看全部链接 | HBox(DuiLabel + spacer + DuiLabel) | 代码（每页都有此 helper） |
| 5 张精选歌单卡 | 5 个 MusicCard（自绘 CoverArt 渐变 + 标题 + 副标 + hover ▶ badge） | 代码循环（按 mock idx setHueSeed 给不同色） |
| 最新音乐 6 行 | 每行一个 DuiLabel 显示拼接字符串（"01 晴天 周杰伦 04:29"） | 代码循环 |

<a id="cmd-page-local"></a>

#### 15.5.2 LocalMusicPage（音乐）

![本地音乐设计稿](images/cloudmelody/local-music.png)

*页头大标题 "本地音乐" + 副标 "共 1,248 首歌曲" + ▶ 播放全部（Primary）+ ✕ 随机播放（Secondary 灰描边）+ DuiTab + 单曲表（# / 标题 / 歌手 / 专辑 / 时长 5 列）。*

| 区域 | 用什么 | 布局方式 |
| --- | --- | --- |
| 页头标题 + 计数 | 2 个 DuiLabel（DisplayLg + BodySm） | 代码 VBox |
| CTA 按钮 | PillButton ×2（Primary / Secondary） | 代码 |
| Tab 头条 | DuiTab（balloonui 内置） | 代码（AddTab × 4） |
| 单曲表表头 + 行 | HBox of 5 DuiLabel（fixedWidth 列宽） | 代码循环（mock 6 行） |

<a id="cmd-page-favorites"></a>

#### 15.5.3 FavoritesPage（收藏）

![收藏设计稿](images/cloudmelody/favorites.png)

*大封面 180×180 + PLAYLIST tag + "我喜欢的音乐" 大标题 + 副标 + ▶ 播放全部（Primary）+ ⤴ 分享（Secondary）+ "收藏的歌单与艺人" + DuiTab + 6 张 MusicCard 艺人网格 + 单曲列表。*

| 区域 | 用什么 | 布局方式 |
| --- | --- | --- |
| 180×180 大封面 | BigCoverPlaceholder（DuiControl 自绘，`CoverArt::Paint(hue=0, "我喜欢的音乐")` = 红色渐变 + 白字） | 代码 |
| 右侧文字块 | 4 行 DuiLabel + 按钮行 HBox | 代码 |
| 艺人卡网格 | 6 张 MusicCard（hueSeed=i+2 错开 Discover 起色） | 代码循环 |
| 单曲列表 | 同 LocalMusicPage | 代码循环 |

<a id="cmd-page-recent"></a>

#### 15.5.4 RecentPlayPage（最近播放）

![最近播放设计稿](images/cloudmelody/recent.png)

*页头 "最近播放" + 副标 + ✕ 清除全部 / ▶ 播放全部 + 红色 hero "正在播放的专辑 / 这就是我" + 灰小卡 "最近的播客" + 单曲表（标题 / 播放时间 / 时长 / 操作）。*

| 区域 | 用什么 | 布局方式 |
| --- | --- | --- |
| 红 hero "这就是我" 卡 | NowPlayingHero（自绘红底圆角）+ CoverArt 紫色封面（hue=1）+ 文字 + ▶ + + PillButton | 代码 |
| 灰播客 chip | PodcastChip（自绘灰底圆角）+ CoverArt 橙封面（hue=2）+ 2 行 label | 代码 |
| 单曲列表（4 列） | HBox(DuiLabel × 4) | 代码循环 |

<a id="cmd-page-profile"></a>

#### 15.5.5 ProfilePage（个人中心）

![个人中心设计稿](images/cloudmelody/profile.png)

*96px 圆头像 + 张小方 + LEVEL 18 chip + 关注/粉丝/听歌 stats + 编辑资料 / 分享 + 红色"最爱风格"卡 + "听歌时段分析" 24 列柱图 + "我的歌单" 4 张卡 + 创建占位卡。*

| 区域 | 用什么 | 布局方式 |
| --- | --- | --- |
| 圆头像 | DuiAvatar（balloonui 内置，圆形 + fallback "张" + 红 fallback bg） | 代码 |
| LEVEL 18 chip | LevelChip（自绘小胶囊，<u>不</u>用 DuiBadge —— 后者 `SetText` 硬截 4 字符，"LEVEL 18" 会被截成 "LEVE"） | 代码 |
| 关注 / 粉丝 / 听歌 stats | 3 个 VBox(DuiLabel + DuiLabel) | 代码 + lambda |
| 红色"最爱风格"卡 | GenreCard（DuiVBox 派生，`OnPaint` 画红圆角矩形）+ 3 行 label | 代码 |
| 24 列柱图 | HoursBarChart（自绘）—— 写死 24 个归一化值，柱顶 3px 圆角，红/浅红交替 | 纯自绘 |
| 我的歌单 4 张 | MusicCard × 4（hueSeed=i+3 错开）+ CreatePlaylistCard（灰底虚线占位） | 代码 |

<a id="cmd-page-podcast"></a>

#### 15.5.6 PodcastPage（播客）

![播客设计稿](images/cloudmelody/podcast.png)

*红 banner "声动早咖啡" + 右两张 chip 卡 + "全部分类" 6 个胶囊（"音乐"主红 active / 其它灰 Secondary）+ "热门节目" 5 张 MusicCard。*

<a id="cmd-page-nowplay"></a>

#### 15.5.7 NowPlayingPage（正在播放）

![正在播放设计稿](images/cloudmelody/now-playing.png)

*左半区：← 返回 + 居中 360px 旋转黑胶（中央封面跟着转）+ 歌名 / 歌手 + ♡ ☆。右半区：歌词/视频 DuiTab + 8 行歌词，当前行（kMockCurrentLyricIdx）红色加大（HeadlineMd 22px）。*

| 区域 | 用什么 | 布局方式 |
| --- | --- | --- |
| 360px 旋转黑胶 | RotatingDisc（自绘）—— host 30Hz timer 推 angle，OnPaint 用 GDI+ `RotateTransform` 应用旋转 | 纯自绘 + 动画 |
| 歌词列表 | 8 个 LyricLine（DuiControl 派生，自绘）—— current 行用 HeadlineMd + 红色，非 current 用 BodyMd + 灰 | 代码循环 |

<a id="cmd-page-search"></a>

#### 15.5.8 SearchResultsPage（设计稿，未实装入路由）

![搜索设计稿](images/cloudmelody/search.png)

*顶 tab 单曲/歌手/专辑/歌单/歌词 + 最佳匹配卡（圆头像 + 名 + chip）+ 单曲列表 + 相关专辑卡片行。<u>本页设计稿已绘制但本期未接入路由</u> —— TopBar 搜索 pill 点击事件还没接 dropdown。*

<a id="cmd-custom-controls"></a>

### 15.6 关键自定义控件深入

#### PlayCircleButton（红圆 + 白三角 / 双白竖条）

```
void PlayCircleButton::OnPaint(HDC hdc, const RECT&)
{
    // 红圆（hover 切到 primary-container）
    DuiAA::FillEllipse(hdc, rcCircle,
        m_hover ? kColorPrimaryHover : kColorPrimaryAccent);
    if (!m_playing) {
        // 三角：3 顶点 GDI+ 抗锯齿
        POINT tri[3] = {...};
        DuiAA::FillPolygon(hdc, tri, 3, kColorOnPrimary);
    } else {
        // 暂停 ⏸：两根白色圆角矩形（轴对齐，普通 RoundRect）
        ::RoundRect(hdc, leftX, top, leftX + barW, bot, 3, 3);
        ::RoundRect(hdc, rightX, top, rightX + barW, bot, 3, 3);
    }
}
```

#### RotatingDisc（旋转黑胶）

```
void RotatingDisc::OnPaint(HDC hdc, const RECT&)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // 1) 黑色外圆（不旋转）
    g.FillEllipse(&blackBrush, cx-outerR, cy-outerR, outerR*2, outerR*2);
    // 2) 5 道同心环纹（不旋转）
    for (int i=1;i<=5;++i) g.DrawEllipse(&ringPen, ...);
    // 3) 内圆 + 封面方块 → 跟着旋转
    g.TranslateTransform(cx, cy);
    g.RotateTransform(m_angle);   // 33⅓ RPM = 200°/s 累加
    g.FillEllipse(&labelBrush, -innerR, -innerR, innerR*2, innerR*2);
    g.FillRectangle(&coverGrad, -coverHalf, -coverHalf, ...);
    g.ResetTransform();
    // 4) 中央主轴黑点（不转）
    g.FillEllipse(&dotBrush, cx-dotR, cy-dotR, dotR*2, dotR*2);
}

void RotatingDisc::Tick(int deltaMs)
{
    m_angle += 200.0f * (deltaMs / 1000.0f);
    while (m_angle >= 360.0f) m_angle -= 360.0f;
    Invalidate();
}
```

#### SearchPill（用 DuiEditHost 内联图标 API）

```
class SearchPill : public DuiVBox {
public:
    SearchPill() {
        auto edit = std::make_unique<DuiEditHost>();
        edit->SetPlaceholder(_T("搜索音乐、视频、播客..."));
        edit->SetBgColor(kPillBg);                  // 与 pill 灰底融合
        edit->SetShowBorder(false);                  // 关 EDIT 默认 1px 边
        // 左 magnifier：用本期新加的内联图标 API
        edit->SetIcon(DuiEditHost::LeftIcon, 32, [](HDC hdc, const RECT& rc) {
            // GDI+ 抗锯齿圆环 + 右下斜把手
            DuiAA::FillEllipse(hdc, ...);
            DuiAA::DrawLine(hdc, ...);
        });
        AddChild(std::move(edit), DuiLayout::Hint().Weight(1));
    }
    void OnPaint(HDC hdc, const RECT& rcDirty) override {
        // 圆角 pill 灰底 + 1px 描边（外层容器画，内层 EDIT 同色融合）
        int radius = (m_rcItem.bottom - m_rcItem.top) / 2;
        PaintHelpers::FillRoundedRect(hdc, m_rcItem, kPillBg, radius);
        PaintHelpers::DrawRoundedRectBorder(hdc, m_rcItem, kPillBorder, radius, 1.0f);
        DuiControl::OnPaint(hdc, rcDirty);
    }
};
```

#### DesktopLyricsWnd（桌面浮窗 + 逐像素 alpha）

用 `UpdateLayeredWindow` 而非 `SetLayeredWindowAttributes(LWA_ALPHA)` —— 后者整窗一致 alpha，做不到"背景全透 + 文字全不透"。流程：

```
void DesktopLyricsWnd::Repaint_()
{
    // 1) 创建 32bpp top-down DIB（GDI 用）
    HBITMAP hbm = CreateDIBSection(...);
    void* pBits;   // BGRA premultiplied
    ZeroMemory(pBits, w*h*4);  // 全 alpha=0（透）

    // 2) GDI+ 在 DIB 上画字
    Gdiplus::Graphics g(hdcMem);
    // 8 邻接黑底描边
    for (dx=-1;dx<=1;++dx) for (dy=-1;dy<=1;++dy)
        g.DrawString(text, &font, off, &blackBrush);
    // 全宽白字
    g.DrawString(text, &font, rcF, &whiteBrush);
    // 测量后 clip 左 60% 画绿（覆盖白色那部分）
    g.MeasureString(...);  g.SetClip(left60%);
    g.DrawString(text, &font, rcF, &greenBrush);

    // 3) 兜底 alpha：扫描 DIB，RGB 非零像素强制 alpha=255
    for (i=0;i<w*h;++i) if (rgb_nonzero) p[3]=255;

    // 4) ULW 整图 push
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(m_hWnd, hdcScreen, &ptDst, &sz,
                        hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
}
```

<a id="cmd-key-interactions"></a>

### 15.7 关键交互行为

| 行为 | 实现 |
| --- | --- |
| **4 模式播放循环**（顺序 → 随机 → 列表循环 → 单曲循环） | PlayerBar 持有 m_playMode 枚举 + 缓存 m_shuffleBtn 指针。`OnShuffleClicked()` 切下一模式 + 更新 IconButton glyph + tooltip（"⇉" / "⇄" / "⟳" / "⥁"） |
| **全屏沉浸模式** | `ToggleFullscreen_()`：保存 m_isFullscreen 状态 + `FindCtrlById(10/20).SetVisible(false)` 隐藏 TopBar/Sidebar + Router 切到 NowPlay + `SW_MAXIMIZE`。<u>本期</u>修了一个伴生 bug：`DuiControl::SetVisible` 现在递归同步 HWND 后代显隐（否则 EDIT 子窗仍透出来）。退出走对称还原 |
| **桌面歌词浮窗** | 独立 `WS_POPUP + WS_EX_TOPMOST + WS_EX_LAYERED + WS_EX_NOACTIVATE`。`WM_NCHITTEST` 全 `HTCAPTION` 让任意位置可拖。播放 timer 每 tick 把 mock 当前歌词行 push 进去 |
| **NowPlay 返回按钮 全屏感知** | 原来的 bug：全屏态点返回 → 切回 Discover 但 m_isFullscreen 不重置 + TopBar/Sidebar 没还原。修法：返回路径里 if (m_isFullscreen) 走 ToggleFullscreen_，对称还原一次性完成 |
| **Win11 Mica 系统背景** | OnCreate_ 调 `DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE, DWMSBT_MAINWINDOW)` + `DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND`。Win10 / 早期 Win11 build 上 DwmSetWindowAttribute 返 E_INVALIDARG，无副作用 |

---

<a id="glossary"></a>

## 16. 术语表

文档中频繁出现的 Win32 / DUI 术语速查。按字母 / 拼音排序。每条给一个"一句话解释"+ 必要的 balloonui 上下文。

<a id="g-allman"></a>

### Allman 大括号

C/C++ 代码风格的一种 —— 函数 / 类 / `if` / `for` 等的<u>左大括号 `{` 单独占一行</u>，不跟在控制语句后面（与之对应的是 K&R 风格 `if (x) {`）。balloonui 库内代码强制 Allman；业务侧不强制。

<a id="g-back-buffer"></a>

### back buffer（背缓冲）

窗口绘制时先画到一张<u>离屏</u>位图（"背缓冲"），全部画完一次性 BitBlt 到窗口 DC。避免边画边显示导致的闪烁、撕裂。`DuiHost::OnPaint` 内部用 32bpp DIB 做背缓冲，每次 `WM_SIZE` 重建尺寸。

<a id="g-bsr"></a>

### BSTR

COM 用的<u>带长度前缀</u>宽字符串，由 `::SysAllocString` 分配、`::SysFreeString` 释放。balloonui 里只在 `XmlDocument`（msxml）/ RichEdit COM 调用里出现。

<a id="g-bui-api"></a>

### BUI_API

balloonui 自定义的 DLL 导出宏 —— 库内编译时展开成 `__declspec(dllexport)`，业务工程链接时展开成 `__declspec(dllimport)`。在<u>跨 DLL 边界</u>暴露的 class / function 上加这个宏；纯值类型 struct（如 `DuiFrameWindowConfig`）不需要。

<a id="g-com"></a>

### COM (Component Object Model)

Windows 的"二进制对象接口"标准。一组带 `QueryInterface / AddRef / Release` 的虚函数表。balloonui 直接接触 COM 的地方只有：`OleInitialize`（OLE 服务）、`::CoCreateInstance` 创建 MSXML2.DOMDocument、RichEdit 的 OLE 对象 / IRichEditOleCallback 等。

<a id="g-dpi"></a>

### DPI / Per-Monitor V2

**DPI**（dots per inch）= 屏幕每英寸像素数。Windows 默认 96 DPI 即"100% 缩放"；用户可以在显示设置里把缩放调到 125%（120 DPI）/ 150%（144 DPI）/ 200%（192 DPI）等，让 UI 在高分辨率屏上不至于太小。

**Per-Monitor V2** 是 Windows 10 1703 起的"DPI 感知"模式，告诉 OS 程序<u>每个显示器各算各的 DPI</u>，主屏 100% 副屏 200% 跨屏拖动时 UI 自动变大。balloonui 在 `DuiDpi::OptInPerMonitorV2()` 一次性开启，所有 `DuiHost` 自动接收 `WM_DPICHANGED` 并通过 `GetDpi()` 暴露当前 DPI 给控件做缩放。

在 balloonui 里"96-dpi 逻辑像素"指<u>DPI=96 时的物理像素值</u>，运行时按 `actual = MulDiv(logical, GetDpi(), 96)` 缩放（如 `DuiFrameWindow` 的 `m_borderPx`）。

<a id="g-defwindowproc"></a>

### DefWindowProc

Windows 的"默认窗口过程"。任何 `WM_*` 消息你的 wndproc 不处理时调它，OS 给一个标准行为（如 `WM_NCLBUTTONDOWN(HTCAPTION)` 触发拖动、`WM_NCHITTEST` 返 HTCLIENT 等）。`DuiFrameWindow` 故意只处理 `WM_NCHITTEST` 决定 hit-test 结果，`WM_NCLBUTTONDOWN` 留给 DefWindowProc 处理标准 caption 行为。

<a id="g-factory"></a>

### factory（工厂） / CustomFactory

设计模式术语 —— 一个<u>函数</u>，给它一份描述（如 `DuiXmlBuilder::Node`），它返回构造好的对象（`DuiControl`）。balloonui 的 `DuiXmlBuilder::CustomFactory = std::function<unique_ptr<DuiControl>(const Node&)>` 就是 factory 类型。业务通过它注册自家标签的解析逻辑。

<a id="g-gdi"></a>

### GDI / GDI+

Windows 的 2D 图形 API。**GDI** 是老的（1985 起）—— `BitBlt` / `FillRect` / `TextOut`，整数坐标，无 alpha 通道。**GDI+** 是 2001 起的现代加强版，浮点坐标 / 抗锯齿 / alpha / PNG 加载。balloonui 主体走 GDI（性能 + 双缓冲），抗锯齿圆 / 多边形走 GDI+（`DuiPaintAA` helper），PNG 加载也走 GDI+（`Gdiplus::Bitmap`）。

<a id="g-hbitmap"></a>

### HBITMAP / HDC / HWND

Windows 的<u>不透明句柄</u>类型。

- **HWND** — 一个窗口的 handle。balloonui 强调"单 HWND 宿主"，意思是整个 DUI 控件树<u>只对应一个 HWND</u>（=`DuiHost`），子控件全是无 HWND 的逻辑节点。
- **HDC** — Device Context。绘图操作的"画笔工具箱"句柄，绑定到一张位图或一个窗口客户区。`OnPaint(HDC, RECT)` 给的就是背缓冲 DC。
- **HBITMAP** — 位图 handle。可以通过 `::CreateCompatibleBitmap` / `::CreateDIBSection` / `Gdiplus::Bitmap::GetHBITMAP` 获得。生命周期由 caller 管，`::DeleteObject(hbm)` 释放。

<a id="g-hit-test"></a>

### hit-test（命中测试）

给定一个鼠标坐标，判断它落在哪个 UI 元素上。

- **非客户区 hit-test** = OS 派 `WM_NCHITTEST`，wndproc 返回 HTLEFT / HTRIGHT / HTTOP / HTBOTTOM / HTTOPLEFT / ... / HTCAPTION / HTCLIENT 中的一个，OS 据此决定光标 / 拖动 / resize 行为。
- **DUI 内部 hit-test** = `DuiControl::HitTest(POINT)` —— 给 host-客户区坐标，递归找最深可见 + 启用的子节点。鼠标事件路由按这个结果分发给具体控件。

<a id="g-htcaption"></a>

### HTCAPTION / HTCLIENT / HTLEFT...

WM_NCHITTEST 的标准返回值集合。

| 返回值 | OS 行为 |
| --- | --- |
| HTCAPTION | 左键按下 → 拖动整个窗口；双击 → 最大化/还原 |
| HTCLIENT | 消息以 `WM_LBUTTONDOWN` 等"客户区版本"派给 wndproc |
| HTTOP / HTBOTTOM / HTLEFT / HTRIGHT | 鼠标变上下/左右双向箭头，拖动 = 改单边 |
| HTTOPLEFT / HTBOTTOMRIGHT | 鼠标变 ↘↖ 双向箭头，拖动 = 改两边 |
| HTTOPRIGHT / HTBOTTOMLEFT | 鼠标变 ↙↗ 双向箭头 |
| HTNOWHERE | 点击不在窗口任何区域（罕见） |

<a id="g-model"></a>

### model（数据模型）

UI 业内术语，特指控件背后那份"<u>数据集合</u>" —— 通常是动态长度、每项有自己的结构。

- **无 model**：状态 = 一组固定属性。如 Slider（一个 int value）、Avatar（name + image + 颜色）、ProgressBar（一个 int progress）。XML 静态描述刚好。
- **有 model**：状态 = 一个<u>列表 / 树</u>。如 ListBox（item 列表）、TreeView（节点树）、ComboBox（下拉选项列表）、Tab（页签列表）、Menu（菜单项列表）。XML 表达需要嵌套 `<item>` / `<node>` 子元素，且业务多半还要运行时动态填，所以这些控件的 XML 落地复杂度更高，balloonui 暂未原生支持，需 caller 自己写 `CustomFactory`。

<a id="g-msxml"></a>

### MSXML / IXMLDOM*

Windows 自带的 XML DOM 解析器（COM 组件）。balloonui 的 `XmlDocument.{h,cpp}` 是它的薄封装，但<u>仅</u>给 `SkinManager` 加载老式 SkinLib 配置文件用；`DuiXmlBuilder` 自带的极简 XML 解析器是手写的，不依赖 MSXML（避免业务依赖系统注册的 COM 组件）。

<a id="g-nccalcsize"></a>

### WM_NCCALCSIZE

"计算非客户区"消息。OS 在窗口尺寸变化前发，wndproc 决定客户区相对窗口的位置 / 大小。`DuiFrameWindow` 处理它返回 0 + bHandled=TRUE → 客户区 = 整窗，没有任何系统 chrome（标题栏 / 边框）；窗口 chrome 由 DUI 自绘补回来。

<a id="g-nine-patch"></a>

### 9-grid（nine-patch）

把一张装饰位图按 4 个内距切成 9 块（4 角 + 4 边 + 中央），让位图<u>缩放到任意目标尺寸</u>时角不变形、边沿单轴拉伸、中央双轴拉伸。Android UI 历史上的标准做法，balloonui 通过 `DuiHost::SetBgImage(HBITMAP, srcInsets, dstInsets)` 暴露 —— 详见 [§10](#nine-patch-bg)。

<a id="g-overlay"></a>

### overlay / 叠加

多张位图按透明度合成。balloonui 的 caption 按钮在 hover 时画一个有色背景 + 白色 glyph 叠在标题栏渐变上，就是 overlay 的一种。

<a id="g-premul-alpha"></a>

### premultiplied alpha（预乘 alpha）

32bpp BGRA 位图的一种存储约定 —— 每个像素的 RGB 分量已经<u>乘以 alpha</u>（如完全透明的红 = (0,0,0,0) 而不是 (255,0,0,0)）。`AlphaBlend`、`UpdateLayeredWindow` 都要求 premultiplied。balloonui 在 `DuiNinePatch::Draw` 内自动处理；caller 用 GDI+ 加载 PNG 时通过 `Gdiplus::Bitmap::GetHBITMAP(白底, &hbm)` 把 alpha 预合成进 RGB 也能规避此坑。

<a id="g-richedit"></a>

### RICHEDIT50W

Windows 自带的富文本控件类（msftedit.dll）。能渲染图文混排 / RTF / IME 输入，所以 balloonui 在 `DuiRichEditHost` 直接寄宿一个真 RICHEDIT50W HWND，<u>不</u>尝试用纯 DUI 重写。

<a id="g-subclass"></a>

### SubclassWindow（子类化）

WTL 术语 —— 把某个已存在 HWND 的 wndproc 替换成自己的 `CWindowImpl`，<u>原始 wndproc 仍在</u>，未处理的消息走它。balloonui 用法：`DuiHost::SubclassWindow(existingDialogHwnd)` 把已有 CDialog 接管成 DUI 容器。

<a id="g-thickframe"></a>

### WS_THICKFRAME / WS_OVERLAPPEDWINDOW

窗口风格位。**WS_THICKFRAME** = 可调尺寸的边框（即使 chrome 被 NCCALCSIZE 抹掉，OS 仍据此参与 Aero snap / 最大化动画）。**WS_OVERLAPPEDWINDOW** = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX 的组合（顶层窗口的常用集合）。

<a id="g-wm-dui-notify"></a>

### WM_DUI_NOTIFY

balloonui 自定义消息常量（`WM_USER + 0x0100` 附近）。控件触发交互（click、value change、focus...）时把一个 `DuiNotify*` 装进 `lParam` 发给 host 的 HWND 父。`DuiNotify::code` 是 `DUIN_CLICK` 等常量；`ctrlId` 是发起控件的 id；`extra` 是控件特有的载荷。详见 [§6 事件处理与路由](#event-routing)。

<a id="g-wtl-atl"></a>

### WTL / ATL / CWindowImpl / CDialogImpl

**ATL** = Active Template Library（微软的 C++ 模板库，1996+）；**WTL** = Windows Template Library（ATL 的窗口框架扩展）。balloonui 用 ATL 的 `CWindow` 操作 HWND，用 WTL 的 `CWindowImpl<T>` 写自己的 wndproc 类（如 `DuiHost`），用 `CDialogImpl<T>` 接老式 CDialog（业务对话框）。两者都是 header-only，无 lib 依赖。

<a id="g-z-order"></a>

### Z-order（层叠顺序）

Windows 多窗口的"前后顺序"。`SetWindowPos` 时 `HWND_TOPMOST` / `HWND_TOP` / `HWND_BOTTOM` 等控制；`BringWindowToTop` 把窗口置顶。在 DUI 内部还有"DUI Z-order" —— 控件树<u>子节点声明顺序</u>从下往上画，后面的盖前面的。

---

## 附录

### A. 完整工程示例

参见 `flamingoclient/NewChatDemo/`：完全基于 balloonui，演示 13 种业务自绘控件 + XML 描述、聊天气泡、自绘布局（`chat-thread` 自管子节点位置）。

![NewChatDemo 截图](images/NewChatDemo_final.png)

### B. 编译与打包

所有工程在 `flamingoclient/Demos.sln`：

- `balloonui` → `Bin/balloonui.dll` + `Bin/balloonui.lib`（Win32）/ `Bin/x64/...`（x64）
- `NewChatDemo` → `Bin/NewChatDemo.exe` 链 `balloonui.lib`
- `DuiGallery` → `Bin/DuiGallery.exe` 静态嵌入内核（test harness）

支持 4 配置：Debug/Release × Win32/x64。

### C. 默认 caption 按钮风格示例

参考某信桌面版的扁平 caption 按钮，hover 时 close 变红：

![参考设计：扁平 caption 按钮](images/QQ20260505-194743.png)

### D. 项目编码与命名约定

- 所有库代码命名空间 `balloonwjui`（保留 `Dui` 类名前缀作 grep 锚点）
- Allman 大括号风格 + 总是大括号控制流
- 头文件须有 usage block 注释（包括 Behavior、Key API、Usage 三段）
- 无 magic number — 所有数字常量须有 `kCamelCase` 命名 + 注释说明用途与单位
