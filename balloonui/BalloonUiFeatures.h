#pragma once

// =====================================================================
// BalloonUiFeatures.h — 按需裁剪开关（feature strip）
// =====================================================================
//
// 目的：让业务侧可以"只编译用到的控件"，从而减小最终 exe / DLL 体积。
// 默认全开（不写任何宏 = 全功能）。业务侧在自己工程的预处理器定义里
// 加 BUI_DISABLE_XXX 关掉单个控件，或在自己的 stdafx.h 顶部 #define。
//
// ─────────────────────────────────────────────────────────────────────
// 基本概念
// ─────────────────────────────────────────────────────────────────────
//
// · BUI_DISABLE_XXX —— 业务侧"输入"宏。定义它表示"我不需要 XXX 控件"。
// · BUI_FEATURE_XXX —— 库内"输出"宏，由本文件根据 BUI_DISABLE_XXX 推
//   导得出。所有 .h / .cpp 都用 #if BUI_FEATURE_XXX 判断是否参与编译，
//   <u>不要直接用 BUI_DISABLE_XXX 判断</u>（依赖关系会变得难维护）。
// · 关掉的控件：
//     - 它的 Dui*.h 在 #if 里仍可被 #include，但里头的 class 定义
//       消失（编译期）。任何残留引用 → 编译错误（"未定义的标识符
//       DuiButton"），定位精准
//     - 它的 Dui*.cpp 整体被 #if 包裹 → 不进 .obj → 不进 .lib / .dll
//     - DuiXmlBuilder 的 tag dispatch 表里没有它的入口 → XML 里出现
//       <button> 等 tag 时走 OutputDebugString 警告 + 返回 nullptr
//
// ─────────────────────────────────────────────────────────────────────
// DLL / 静态库 的差异
// ─────────────────────────────────────────────────────────────────────
//
// · 静态库（默认）：BUI_DISABLE_XXX 立竿见影。再配合业务 exe 工程的
//   /OPT:REF + /OPT:ICF，未引用控件代码完全消失。
// · DLL（BUI_USE_DLL）：DLL 是"编一次给多 exe 用"，但本方案允许业务
//   <u>自己重新编 DLL</u>，编译时把 BUI_DISABLE_XXX 加到 balloonui
//   工程的预处理器定义中。注意：业务 exe 和它使用的 DLL 必须用<u>同
//   一份</u> BUI_DISABLE_XXX 集合，否则 exe 端 #include 看到的 class
//   声明 vs DLL 端的导出符号不一致 → 链接错误。
//
// ─────────────────────────────────────────────────────────────────────
// 依赖关系（不写是一回事，<u>会传染</u>是另一回事）
// ─────────────────────────────────────────────────────────────────────
//
// 关掉某个底层 feature 会自动连带关掉依赖它的上层 feature。下面的
// "传染表"说明依赖方向；定义在本文件下半段的 #if/#define 块强制执行。
//
//   DISABLE_LAYOUT     → 不允许（基础容器，整个库都依赖；硬错误）
//   DISABLE_SCROLLBAR  → 也关掉 LISTBOX, TREEVIEW
//   DISABLE_EDIT       → 也关掉 SEARCHBOX, SPINBOX, COMBOBOX, TREEVIEW
//                        （TREEVIEW 用 EditHost 做 inline 单元格编辑）
//   DISABLE_LISTBOX    → 也关掉 COMBOBOX
//   DISABLE_TAB        → 也关掉 TABPAGE
//   DISABLE_IMAGEOLE   → 也关掉 RICHEDIT（RichEdit 用 ImageOle 嵌图片）
//
// 如果业务关掉一个有传染的 feature 但又显式开启一个被传染的下游
// feature，本文件 #error 报错（避免静默编译出半残的库）。
//
// ─────────────────────────────────────────────────────────────────────
// 如何裁剪 —— 业务侧用法
// ─────────────────────────────────────────────────────────────────────
//
// 静态库工程：在 balloonui 子工程 + 业务 exe 工程<u>都</u>加预处理器
// 定义（VS：项目属性 → C/C++ → 预处理器 → 预处理器定义），例如：
//
//     BUI_DISABLE_RICHEDIT;BUI_DISABLE_TREEVIEW;BUI_DISABLE_MENU
//
// DLL 工程：在 balloonui 工程的预处理器定义里加同一份 list，重新编
// DLL；同时业务 exe 也加同一份 list。运行期把新 DLL 替换原 DLL。
//
// ─────────────────────────────────────────────────────────────────────
// 实测体积差异
// ─────────────────────────────────────────────────────────────────────
//
// 见 DemoFeatureStrip / guides.html "按需裁剪 (Feature Strip)" 章节。
//
// =====================================================================


// ---------------------------------------------------------------------
// LAYOUT —— DuiVBox / DuiHBox / DuiGrid（基础容器）
// ---------------------------------------------------------------------
// 作用：所有"自动横排 / 纵排 / 网格排"容器；几乎每棵 DUI 树根都是它。
// 关闭后影响：库无法再做静态布局；业务必须手写每个子控件的坐标 +
// SetItemRect。XML <vbox>/<hbox>/<grid> 标签全部失效。
// 依赖：无（基础）。被几乎所有上层 feature 依赖。
// 是否可关：<u>不允许</u>。本文件下方在 BUI_DISABLE_LAYOUT 出现时
// 直接 #error 拒绝编译。
// 典型场景：登录表单、对话框主体、设置面板、emoji 网格。
#define BUI_FEATURE_LAYOUT 1

#ifdef BUI_DISABLE_LAYOUT
#  error "BUI_DISABLE_LAYOUT is not allowed: layout containers are foundational; nearly every DUI tree root is a vbox/hbox/grid. Disabling them breaks the library."
#endif


// ---------------------------------------------------------------------
// DOCK —— DuiDock（停靠布局：上下左右 + 中间填充）
// ---------------------------------------------------------------------
// 作用：让子控件按 Top/Bottom/Left/Right/Fill 方向停靠，剩下空间留给
// 中间的 Fill 子。比 vbox/hbox 嵌套更接近 IDE 主框架风格的布局。
// 关闭后影响：DuiDock 类消失。XML <dock> 标签失效。需要类似效果的
// 业务必须用 vbox + hbox 嵌套替代。
// 依赖：LAYOUT（DuiDock 是 DuiLayout 子类）。
// 典型场景：IDE 风格主框架（菜单栏在上，状态栏在下，文档区填中间）。
#ifndef BUI_DISABLE_DOCK
#  define BUI_FEATURE_DOCK 1
#endif


// ---------------------------------------------------------------------
// SPLITTER —— DuiSplitter（双窗格可拖动分隔器）
// ---------------------------------------------------------------------
// 作用：把可用区切成两块，中间有可拖动的分隔条。垂直 / 水平两个方向。
// 关闭后影响：DuiSplitter 类消失。XML <splitter> 标签失效。
// 依赖：LAYOUT（基础容器算法）。
// 典型场景：聊天主窗体（左侧好友列表 + 右侧消息区）、任务管理器
// 性能页（左侧硬件列表 + 右侧大折线图）。
#ifndef BUI_DISABLE_SPLITTER
#  define BUI_FEATURE_SPLITTER 1
#endif


// ---------------------------------------------------------------------
// LABEL —— DuiLabel（文字标签 / 超链接）
// ---------------------------------------------------------------------
// 作用：单 / 多行文本标签；可作为超链接（鼠标悬停手形 + 点击通知）。
// 关闭后影响：DuiLabel 类消失。XML <label> 标签失效。最常用的"显示
// 文字"控件没了 —— 多数业务需要它，慎关。
// 依赖：无（仅依赖 kernel）。
// 典型场景：表单字段名、页眉、状态栏文字、链接。
#ifndef BUI_DISABLE_LABEL
#  define BUI_FEATURE_LABEL 1
#endif


// ---------------------------------------------------------------------
// BUTTON —— DuiButton（推送按钮 / 图标按钮 / 复选框 / 单选框）
// ---------------------------------------------------------------------
// 作用：4 种风格按钮（StylePushButton / StyleIcon / StyleCheckbox /
// StyleRadio）。品牌蓝 #2D6CDF 圆角 push 按钮是"默认按钮"。
// 关闭后影响：DuiButton 类消失。XML <button> 标签失效。
// 依赖：无（仅依赖 kernel + DuiNinePatch + DuiMnemonic）。
// 典型场景：表单提交、对话框确定/取消、复选框组、icon-only 工具栏。
#ifndef BUI_DISABLE_BUTTON
#  define BUI_FEATURE_BUTTON 1
#endif


// ---------------------------------------------------------------------
// AVATAR —— DuiAvatar（头像 + 在线状态徽标）
// ---------------------------------------------------------------------
// 作用：圆形 / 圆角矩形头像，无图时 fallback 成"姓名首字母 + 品牌色
// 底"；右下角可叠加 online/away/busy/offline 状态点。
// 关闭后影响：DuiAvatar 类消失。XML <avatar> 标签失效。
// 依赖：无。
// 典型场景：聊天会话条目左侧头像、好友列表、个人资料卡。
#ifndef BUI_DISABLE_AVATAR
#  define BUI_FEATURE_AVATAR 1
#endif


// ---------------------------------------------------------------------
// BADGE —— DuiBadge（小红点 / 数字角标）
// ---------------------------------------------------------------------
// 作用：圆角红底白字小角标，"99+" 自动溢出处理；支持数字 (SetCount)
// 或自定义文字 (SetText)；hide-when-empty=true 时空字符串不绘制。
// 关闭后影响：DuiBadge 类消失。XML <badge> 标签失效。
// 依赖：无。
// 典型场景：菜单项右侧未读数、tab 头条上的"new"标记、按钮右上角通知。
#ifndef BUI_DISABLE_BADGE
#  define BUI_FEATURE_BADGE 1
#endif


// ---------------------------------------------------------------------
// TOAST —— DuiToast（顶层浮出的轻量提示条）
// ---------------------------------------------------------------------
// 作用：窗口顶部居中浮出的"非交互、定时自隐"提示条;支持文本 / 图标 /
// 背景色 / 圆角 / 时长 / 淡入淡出可配。内部走 DuiAnimMgr 自驱, 不需要
// caller 操心定时器。HitTest 永远返回 nullptr —— 点击穿透不抢焦。
// 关闭后影响：DuiToast 类消失。
// 依赖：DuiAnimMgr（淡入淡出 + 定时自驱)。
// 典型场景：成功 / 失败 / 警告 / 信息提示("已保存"、"请先在左侧选择..."
// 之类), 不阻塞用户当前操作。
#ifndef BUI_DISABLE_TOAST
#  define BUI_FEATURE_TOAST 1
#endif


// ---------------------------------------------------------------------
// SEPARATOR —— DuiSeparator（分隔线）
// ---------------------------------------------------------------------
// 作用：水平 / 垂直 1px 分隔线，可设颜色 / 厚度 / inset 边距。
// 关闭后影响：DuiSeparator 类消失。XML <separator> 标签失效。
// 依赖：无。
// 典型场景：菜单项之间的分组分隔、设置页的章节分隔、工具栏分组。
#ifndef BUI_DISABLE_SEPARATOR
#  define BUI_FEATURE_SEPARATOR 1
#endif


// ---------------------------------------------------------------------
// GROUPBOX —— DuiGroupBox（带标题 + 圆角边框的分组容器）
// ---------------------------------------------------------------------
// 作用：为子内容加一个标题 + 圆角边框包裹；标题嵌在边框上沿（Win32
// GroupBox 风格）。一个 GroupBox 持有<u>一个</u>子内容（通常是 vbox）。
// 关闭后影响：DuiGroupBox 类消失。XML <groupbox> 标签失效。
// 依赖：无。
// 典型场景：设置页的"账号设置"/"通知设置"等章节包装。
#ifndef BUI_DISABLE_GROUPBOX
#  define BUI_FEATURE_GROUPBOX 1
#endif


// ---------------------------------------------------------------------
// EDIT —— DuiEditHost（单 / 多行文本输入，HWND-hosted Win32 EDIT）
// ---------------------------------------------------------------------
// 作用：包了一个真的 Win32 EDIT 控件，给 IME / 中文输入正确支持。
// 单行 / 多行 / 密码 / 占位符（placeholder）/ 只读。
// 关闭后影响：DuiEditHost 类消失。XML <edit> 标签失效。<u>传染</u>：
// SearchBox / SpinBox / ComboBox / TreeView 都依赖它，会被一起关掉。
// 依赖：HwndHostControl（kernel）。
// 典型场景：登录用户名 / 密码、搜索输入、聊天输入、表单字段。
#ifndef BUI_DISABLE_EDIT
#  define BUI_FEATURE_EDIT 1
#endif


// ---------------------------------------------------------------------
// IMAGEOLE —— DuiImageOle（RichEdit 内嵌图片的 OLE 适配器）
// ---------------------------------------------------------------------
// 作用：RichEdit 控件能"插一张图片到光标位置"靠的就是 IRichEditOle +
// IDataObject 的 OLE 流；DuiImageOle 把 PNG/JPG/GIF 文件包装成
// IDataObject 喂给 RichEdit。
// 关闭后影响：DuiImageOle 类消失。<u>传染</u>：RICHEDIT 也会被关掉。
// 依赖：无（用 OLE / RichEdit COM 接口，是 Windows 自带）。
// 典型场景：聊天 RichEdit 输入框 / 显示框里的内联图片、emoji 图标
// 显示。
#ifndef BUI_DISABLE_IMAGEOLE
#  define BUI_FEATURE_IMAGEOLE 1
#endif


// ---------------------------------------------------------------------
// RICHEDIT —— DuiRichEditHost（HWND-hosted Win32 RichEdit）
// ---------------------------------------------------------------------
// 作用：包了一个真的 Win32 RICHEDIT 控件；支持富文本（颜色 / 字体 /
// 内嵌图片 / 自动 URL 检测）+ 多行 / word-wrap / 占位符。
// 关闭后影响：DuiRichEditHost 类消失。XML <richedit> 标签失效。聊天
// 输入框这类需要"图文混排 + IME"的场景做不了。
// 依赖：IMAGEOLE（cpp 包含 DuiImageOle.h，用于 InsertImage 插图）+
// HwndHostControl（kernel）。
// 典型场景：聊天 输入框 / 历史显示、富文本编辑器、reading-pane。
#if !defined(BUI_DISABLE_RICHEDIT) && defined(BUI_FEATURE_IMAGEOLE)
#  define BUI_FEATURE_RICHEDIT 1
#endif


// ---------------------------------------------------------------------
// SEARCHBOX —— DuiSearchBox（搜索框：左 🔍 + 中输入区 + 右 × 清除）
// ---------------------------------------------------------------------
// 作用：复合控件 = DuiEditHost（中间真 EDIT）+ 自绘左侧 🔍 + 右侧 ×
// 清除按钮。点 × 清空内容。
// 关闭后影响：DuiSearchBox 类消失。XML <searchbox> 标签失效。
// 依赖：EDIT（DuiSearchBox 嵌入 DuiEditHost*）。
// 典型场景：好友列表搜索框、设置页过滤、文件浏览器搜索。
#if !defined(BUI_DISABLE_SEARCHBOX) && defined(BUI_FEATURE_EDIT)
#  define BUI_FEATURE_SEARCHBOX 1
#endif


// ---------------------------------------------------------------------
// SPINBOX —— DuiSpinBox（数字步进框：EDIT + ▲▼ 上下按钮）
// ---------------------------------------------------------------------
// 作用：复合控件 = DuiEditHost（左侧真 EDIT，强制只接受数字）+ 自绘
// ▲ ▼ 步进按钮条；支持 min/max/step/wrap 设置。
// 关闭后影响：DuiSpinBox 类消失。XML <spinbox> 标签失效。
// 依赖：EDIT。
// 典型场景：设置页的端口号、超时秒数、字号选择。
#if !defined(BUI_DISABLE_SPINBOX) && defined(BUI_FEATURE_EDIT)
#  define BUI_FEATURE_SPINBOX 1
#endif


// ---------------------------------------------------------------------
// SLIDER —— DuiSlider（滑动条：拖动选数值）
// ---------------------------------------------------------------------
// 作用：水平 / 垂直滑动条，min ~ max 区间，可设步长 / 刻度。
// 关闭后影响：DuiSlider 类消失。XML <slider> 标签失效。
// 依赖：无。
// 典型场景：音量调节、亮度调节、缩放比例。
#ifndef BUI_DISABLE_SLIDER
#  define BUI_FEATURE_SLIDER 1
#endif


// ---------------------------------------------------------------------
// SWITCH —— DuiSwitch（iOS 风格圆角胶囊开关）
// ---------------------------------------------------------------------
// 作用：单布尔状态的两态切换器（开 / 关），某信绿胶囊 + 白色滑块，
// 切换走 150ms ease-out-cubic 动画。比 Checkbox 更"开关感"。
// 关闭后影响：DuiSwitch 类消失。XML <switch> 标签失效。
// 依赖：DuiAnimation kernel（用 DuiDoubleAnim 驱动滑块动画 +
//      DuiAnimMgr 调度）—— kernel 总在，不需要单独门控。
// 典型场景：设置页"通知开关"/"自动下载"/"保留聊天记录"等开关项、
//          群聊侧栏的"消息免打扰"。
#ifndef BUI_DISABLE_SWITCH
#  define BUI_FEATURE_SWITCH 1
#endif


// ---------------------------------------------------------------------
// SCROLLBAR —— DuiScrollBar（DUI 自绘滚动条，无 HWND）
// ---------------------------------------------------------------------
// 作用：可滚动容器（ListBox / TreeView / 任何业务自定义滚动区）的
// 通用滚动条组件。垂直 / 水平。
// 关闭后影响：DuiScrollBar 类消失。<u>传染</u>：LISTBOX, TREEVIEW
// 都依赖它，一起关掉。
// 依赖：无。
// 典型场景：所有需要滚动的列表 / 树 / 日志区。
#ifndef BUI_DISABLE_SCROLLBAR
#  define BUI_FEATURE_SCROLLBAR 1
#endif


// ---------------------------------------------------------------------
// LISTBOX —— DuiListBox（列表框：选项列表，可单 / 多选）
// ---------------------------------------------------------------------
// 作用：纵向列表，每项一行；可设单选 / 多选；高度可变。带滚动条。
// 关闭后影响：DuiListBox 类消失。XML <listbox> 标签失效。<u>传染</u>：
// COMBOBOX 依赖它（下拉用 listbox）。
// 依赖：SCROLLBAR。
// 典型场景：好友列表、最近会话、文件列表。
#if !defined(BUI_DISABLE_LISTBOX) && defined(BUI_FEATURE_SCROLLBAR)
#  define BUI_FEATURE_LISTBOX 1
#endif


// ---------------------------------------------------------------------
// COMBOBOX —— DuiComboBox（下拉选择框 = EDIT + 下拉 LISTBOX）
// ---------------------------------------------------------------------
// 作用：复合控件 = DuiEditHost（可输入或纯显示）+ 右侧 ▼ 按钮，点 ▼
// 弹下拉 DuiListBox 让用户选。
// 关闭后影响：DuiComboBox 类消失。XML <combobox> 标签失效。
// 依赖：EDIT + LISTBOX。
// 典型场景：表单的"国家"/"语言"等枚举选择、字体选择、最近搜索历史。
#if !defined(BUI_DISABLE_COMBOBOX) && defined(BUI_FEATURE_EDIT) && defined(BUI_FEATURE_LISTBOX)
#  define BUI_FEATURE_COMBOBOX 1
#endif


// ---------------------------------------------------------------------
// TREEVIEW —— DuiTreeView（树状 / 多列表格视图，4-象限渲染 + 冻结列）
// ---------------------------------------------------------------------
// 作用：可单层（多列表格）或多层（树）展示数据；每列支持 6 种 cell
// 类型（TEXT / ICON / IMAGE / CHECKBOX / PROGRESS / HYPERLINK）；
// 可冻结左侧 N 列 / 顶部 N 行；表头点击排序；inline 单元格编辑。
// 关闭后影响：DuiTreeView 类消失。XML <treeview> + 子 <column> 失效。
// 依赖：SCROLLBAR + EDIT（inline 单元格编辑器是 DuiEditHost）。
// 典型场景：任务管理器（进程 / 服务 / 详细信息）、文件浏览器详情视
// 图、邮件列表。
#if !defined(BUI_DISABLE_TREEVIEW) && defined(BUI_FEATURE_SCROLLBAR) && defined(BUI_FEATURE_EDIT)
#  define BUI_FEATURE_TREEVIEW 1
#endif


// ---------------------------------------------------------------------
// TAB —— DuiTab（自绘 tab 标签条，仅头条不切页）
// ---------------------------------------------------------------------
// 作用：横向 tab 标签条，"当前选中"概念，可选 close 按钮 / dropdown
// 箭头 / 拖拽重排。<u>不</u>管内容区切换。
// 关闭后影响：DuiTab 类消失。<u>传染</u>：TABPAGE 也被关掉。
// 依赖：无。
// 典型场景：聊天主窗顶部的多会话切换、设置页"通用 / 网络 / 隐私"分页栏。
#ifndef BUI_DISABLE_TAB
#  define BUI_FEATURE_TAB 1
#endif


// ---------------------------------------------------------------------
// TABPAGE —— DuiTabPage（Tab 头条 + 自动切页内容区）
// ---------------------------------------------------------------------
// 作用：内置 DuiTab 头条 + 多个内容 page；切 tab 自动 show/hide 对应
// page。比 DuiTab 高一层、业务无需自己管 page 可见性。
// 关闭后影响：DuiTabPage 类消失。XML <tab-page> 标签（CustomFactory
// 路径）失效。
// 依赖：TAB（TabPage 内部就是一个 DuiTab + 多个 page 容器）。
// 典型场景：设置对话框（多分类页面）、个人资料卡的多视图。
#if !defined(BUI_DISABLE_TABPAGE) && defined(BUI_FEATURE_TAB)
#  define BUI_FEATURE_TABPAGE 1
#endif


// ---------------------------------------------------------------------
// MENU —— DuiMenu（同步 TrackPopup 上下文菜单）
// ---------------------------------------------------------------------
// 作用：弹出菜单，支持子菜单 / 助记符 / icon / 分隔符 / 复选 / 禁用。
// 同步 TrackPopup 风格 —— 调用方 block 直到用户选项 / 取消。
// 关闭后影响：DuiMenu 类消失。右键菜单 / 下拉菜单做不了。
// 依赖：无。
// 典型场景：右键上下文菜单、按钮的 dropdown 菜单、菜单栏下拉。
#ifndef BUI_DISABLE_MENU
#  define BUI_FEATURE_MENU 1
#endif


// ---------------------------------------------------------------------
// MENUBAR —— DuiMenuBar（常驻菜单条 / Win10 顶部 File-Edit-View 那条）
// ---------------------------------------------------------------------
// 作用：host 客户区里的横向菜单条，由若干栏目组成（"文件 / 编辑 /
// 查看"）。栏目自身不画下拉项 —— 点击 / Alt 激活时调用关联的 DuiMenu
// 弹下拉。激活态下鼠标移到隔壁栏目时自动切换（关旧弹新），与 Win10
// 真菜单条体感一致。
// 关闭后影响：DuiMenuBar 类消失。XML <menu-bar> 标签失效。需要常驻菜
// 单条的业务对话框得退到"用 hbox + button 拼"。
// 依赖：MENU（栏目下拉就是 DuiMenu，没了 MENU 也就没下拉可弹）。
// 典型场景：桌面应用的主窗口顶部菜单条（IM 主菜单 / 任务管理器顶栏 /
// 设置工具）。
#if !defined(BUI_DISABLE_MENUBAR) && defined(BUI_FEATURE_MENU)
#  define BUI_FEATURE_MENUBAR 1
#endif


// ---------------------------------------------------------------------
// PROGRESSBAR —— DuiProgressBar（进度条）
// ---------------------------------------------------------------------
// 作用：水平 / 垂直进度条；min ~ max 区间；可显示百分比文字 / 自定义
// 文字；marquee（不确定进度）模式。
// 关闭后影响：DuiProgressBar 类消失。XML <progress> 标签失效。
// 依赖：无。
// 典型场景：文件上传 / 下载进度、安装进度、任务管理器内存使用率。
#ifndef BUI_DISABLE_PROGRESSBAR
#  define BUI_FEATURE_PROGRESSBAR 1
#endif


// ---------------------------------------------------------------------
// TOOLTIP —— DuiToolTip（工具提示气泡）
// ---------------------------------------------------------------------
// 作用：鼠标悬停显示一个气泡提示。超时 / 离开自动消失；可手动 Show /
// Hide。
// 关闭后影响：DuiToolTip 类消失。
// 依赖：无（自己管 tip HWND）。
// 典型场景：工具栏按钮的悬浮说明、表单字段的格式提示。
#ifndef BUI_DISABLE_TOOLTIP
#  define BUI_FEATURE_TOOLTIP 1
#endif


// ---------------------------------------------------------------------
// POPUPHOST —— DuiPopupHost（弹窗 host：独立 HWND 顶层弹窗）
// ---------------------------------------------------------------------
// 作用：业务侧自定义弹窗的 HWND host；典型用法是把一个 DUI 子树挂到
// PopupHost 里、显示为没有标题栏的浮窗（比如 emoji 选择器）。
// 关闭后影响：DuiPopupHost 类消失。
// 依赖：DuiHost（kernel）。
// 典型场景：emoji 面板、好友信息悬浮卡、自定义 dropdown。
#ifndef BUI_DISABLE_POPUPHOST
#  define BUI_FEATURE_POPUPHOST 1
#endif


// ---------------------------------------------------------------------
// EMOJIPANEL —— DuiEmojiPanel（emoji 选择面板）
// ---------------------------------------------------------------------
// 作用：网格布局展示 emoji 表情，点击选中并通知。带分类 tab。
// 关闭后影响：DuiEmojiPanel 类消失。
// 依赖：无（独立实现）。
// 典型场景：聊天输入框旁的 emoji 选择按钮弹出面板。
#ifndef BUI_DISABLE_EMOJIPANEL
#  define BUI_FEATURE_EMOJIPANEL 1
#endif


// ---------------------------------------------------------------------
// GIF —— DuiGif（GIF 动画播放）
// ---------------------------------------------------------------------
// 作用：解码并循环播放 GIF 动画；用 DuiAnimation kernel 做帧定时。
// 关闭后影响：DuiGif 类消失。聊天里的动图 emoji 退化成静态首帧。
// 依赖：DuiAnimation（kernel）。
// 典型场景：聊天输入框 / 历史里的动图 emoji、加载 spinner。
#ifndef BUI_DISABLE_GIF
#  define BUI_FEATURE_GIF 1
#endif


// ---------------------------------------------------------------------
// FRAMEWINDOW —— DuiFrameWindow（无边框窗口 + 9-grid bg + 标题栏按钮）
// ---------------------------------------------------------------------
// 作用：完整的无边框顶层窗口实现；9-grid 背景图 + 标题栏 + 自绘
// min/max/close 按钮 + WM_NCHITTEST 边角 resize + 客户区 layout 嵌入。
// 关闭后影响：DuiFrameWindow 类消失。<frame-window> 顶层 XML 失效，
// DuiXmlBuilder::FromFrameXml() 直接返回 nullptr。业务必须自己实现
// 顶层窗口（继承 ATL CFrameWindowImpl 等）。
// 依赖：LAYOUT（客户区是个 layout）。
// 典型场景：所有业务对话框 / 主窗口的 frame。balloonui 的"代表性"
// 控件，多数业务都需要它。
#ifndef BUI_DISABLE_FRAMEWINDOW
#  define BUI_FEATURE_FRAMEWINDOW 1
#endif


// ---------------------------------------------------------------------
// XMLBUILDER —— DuiXmlBuilder（声明式 XML 布局解析器）
// ---------------------------------------------------------------------
// 作用：把 XML 字符串解析为 DUI 控件树。<u>这是 balloonui 里硬引用
// 内置控件类名最集中的代码点</u>。Builder 内部对每个内置 tag 的
// dispatch 分支都用相同的 BUI_FEATURE_XXX 宏门控（与控件 .h/.cpp
// 同步开关），feature 关掉时对应 tag 的 dispatch 分支也消失，遇到
// 该 tag 走 "miss" 路径：OutputDebugString 警告 + 返回 nullptr。
// 关闭后影响：DuiXmlBuilder 类消失，所有 FromString / FromFrameXml
// 调用变成编译错误（需要业务自己用 C++ AddChild 链构造 UI 树）。
// 依赖：无（builder 本身是纯文本 → 控件树的工厂；对每个内置控件类
// 的引用通过 BUI_FEATURE_XXX 宏门控，与控件本身的开关一致）。
// 典型场景：所有"用 XML 写布局"的业务 / demo。
#ifndef BUI_DISABLE_XMLBUILDER
#  define BUI_FEATURE_XMLBUILDER 1
#endif


// ---------------------------------------------------------------------
// GALLERY —— DuiGalleryDlg / DuiGalleryAutoStart（dev-only 测试入口）
// ---------------------------------------------------------------------
// 衍生 feature（不是独立可控开关），仅当 gallery 内部 demo 页面用到
// 的<u>所有</u>底层 feature 都开启时才为 1。任何一个底层 feature 关
// 掉 → gallery 整个跳过编译，避免 GalleryDlg.cpp 内对失踪 RunAll() 等
// 符号的引用导致编译错误。
// 关闭后影响：DuiGalleryDlg / DuiGalleryAutoStart 整个跳过；DEBUG
// 自动弹出的 gallery 测试窗也跟着消失。production 业务方面无影响。
// 依赖：BUTTON, LABEL, EDIT, RICHEDIT, LISTBOX, TAB, TABPAGE, TREEVIEW,
//      MENU, TOOLTIP, POPUPHOST, SCROLLBAR, SPLITTER, DOCK, AVATAR,
//      BADGE, SEPARATOR, GROUPBOX, SLIDER, SWITCH, PROGRESSBAR, COMBOBOX,
//      SEARCHBOX, SPINBOX, GIF, IMAGEOLE, EMOJIPANEL, FRAMEWINDOW,
//      XMLBUILDER —— 几乎全部
// 这是个"自动派生"flag，业务无需手动设置；要单独关 gallery 但保留
// 所有控件，可以在 stdafx.h 里 `#define BUI_DISABLE_GALLERY` 并把下
// 面表达式整体改成手控（暂未做，按需扩展）。
#if defined(BUI_FEATURE_BUTTON) \
 && defined(BUI_FEATURE_LABEL) \
 && defined(BUI_FEATURE_EDIT) \
 && defined(BUI_FEATURE_RICHEDIT) \
 && defined(BUI_FEATURE_LISTBOX) \
 && defined(BUI_FEATURE_TAB) \
 && defined(BUI_FEATURE_TABPAGE) \
 && defined(BUI_FEATURE_TREEVIEW) \
 && defined(BUI_FEATURE_MENU) \
 && defined(BUI_FEATURE_MENUBAR) \
 && defined(BUI_FEATURE_TOOLTIP) \
 && defined(BUI_FEATURE_POPUPHOST) \
 && defined(BUI_FEATURE_SCROLLBAR) \
 && defined(BUI_FEATURE_SPLITTER) \
 && defined(BUI_FEATURE_DOCK) \
 && defined(BUI_FEATURE_AVATAR) \
 && defined(BUI_FEATURE_BADGE) \
 && defined(BUI_FEATURE_SEPARATOR) \
 && defined(BUI_FEATURE_GROUPBOX) \
 && defined(BUI_FEATURE_SLIDER) \
 && defined(BUI_FEATURE_SWITCH) \
 && defined(BUI_FEATURE_PROGRESSBAR) \
 && defined(BUI_FEATURE_COMBOBOX) \
 && defined(BUI_FEATURE_SEARCHBOX) \
 && defined(BUI_FEATURE_SPINBOX) \
 && defined(BUI_FEATURE_GIF) \
 && defined(BUI_FEATURE_IMAGEOLE) \
 && defined(BUI_FEATURE_EMOJIPANEL) \
 && defined(BUI_FEATURE_FRAMEWINDOW) \
 && defined(BUI_FEATURE_XMLBUILDER)
#  define BUI_FEATURE_GALLERY 1
#endif


// =====================================================================
// 依赖一致性检查 —— 业务侧若 BUI_DISABLE 一个 feature 又显式开了下
// 游 feature（罕见，多半是手抖），这里 #error 拦下来，避免静默编译
// 出半残的库。如果业务真的有特殊需求要绕过，删掉对应 #error 即可。
// =====================================================================

#if defined(BUI_DISABLE_EDIT) && defined(BUI_FEATURE_SEARCHBOX)
#  error "BUI_FEATURE_SEARCHBOX requires BUI_FEATURE_EDIT (DuiSearchBox embeds DuiEditHost). Remove BUI_DISABLE_EDIT or also disable SEARCHBOX."
#endif
#if defined(BUI_DISABLE_EDIT) && defined(BUI_FEATURE_SPINBOX)
#  error "BUI_FEATURE_SPINBOX requires BUI_FEATURE_EDIT (DuiSpinBox embeds DuiEditHost). Remove BUI_DISABLE_EDIT or also disable SPINBOX."
#endif
#if defined(BUI_DISABLE_EDIT) && defined(BUI_FEATURE_COMBOBOX)
#  error "BUI_FEATURE_COMBOBOX requires BUI_FEATURE_EDIT (DuiComboBox embeds DuiEditHost). Remove BUI_DISABLE_EDIT or also disable COMBOBOX."
#endif
#if defined(BUI_DISABLE_EDIT) && defined(BUI_FEATURE_TREEVIEW)
#  error "BUI_FEATURE_TREEVIEW requires BUI_FEATURE_EDIT (TreeView uses DuiEditHost for inline cell editing). Remove BUI_DISABLE_EDIT or also disable TREEVIEW."
#endif
#if defined(BUI_DISABLE_SCROLLBAR) && defined(BUI_FEATURE_LISTBOX)
#  error "BUI_FEATURE_LISTBOX requires BUI_FEATURE_SCROLLBAR. Remove BUI_DISABLE_SCROLLBAR or also disable LISTBOX."
#endif
#if defined(BUI_DISABLE_SCROLLBAR) && defined(BUI_FEATURE_TREEVIEW)
#  error "BUI_FEATURE_TREEVIEW requires BUI_FEATURE_SCROLLBAR. Remove BUI_DISABLE_SCROLLBAR or also disable TREEVIEW."
#endif
#if defined(BUI_DISABLE_LISTBOX) && defined(BUI_FEATURE_COMBOBOX)
#  error "BUI_FEATURE_COMBOBOX requires BUI_FEATURE_LISTBOX (combo dropdown is a listbox). Remove BUI_DISABLE_LISTBOX or also disable COMBOBOX."
#endif
#if defined(BUI_DISABLE_TAB) && defined(BUI_FEATURE_TABPAGE)
#  error "BUI_FEATURE_TABPAGE requires BUI_FEATURE_TAB (TabPage embeds DuiTab). Remove BUI_DISABLE_TAB or also disable TABPAGE."
#endif
#if defined(BUI_DISABLE_IMAGEOLE) && defined(BUI_FEATURE_RICHEDIT)
#  error "BUI_FEATURE_RICHEDIT requires BUI_FEATURE_IMAGEOLE (RichEdit uses DuiImageOle for inline images). Remove BUI_DISABLE_IMAGEOLE or also disable RICHEDIT."
#endif
#if defined(BUI_DISABLE_LAYOUT) && defined(BUI_FEATURE_FRAMEWINDOW)
#  error "BUI_FEATURE_FRAMEWINDOW requires BUI_FEATURE_LAYOUT (frame's client area is a layout host). Cannot disable LAYOUT."
#endif
#if defined(BUI_DISABLE_MENU) && defined(BUI_FEATURE_MENUBAR)
#  error "BUI_FEATURE_MENUBAR requires BUI_FEATURE_MENU (menu bar items pop DuiMenu dropdowns). Remove BUI_DISABLE_MENU or also disable MENUBAR."
#endif
