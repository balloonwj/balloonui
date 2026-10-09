#pragma once

/**
 *  DuiEdit（无窗口普通输入框）的单元测试。
 *  balloonwj@qq.com   2026-08-17
 */

#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_EDIT
#include "../Controls/Input/DuiEdit.h"

namespace balloonwjui {

// DuiEdit 的自包含单元测试。
//
// 本文件只覆盖 DuiEdit 相对基类 DuiRichEdit **多出来的那部分语义**：单行时
// 回车与 Esc 的处理、左右内联图标栏对文本区的影响、密码显隐按钮与右侧图标的
// 互斥关系、单行文字垂直居中、SetText / SetTextNoNotify 的通知差异，以及
// 纯文本模式（选区不越过文本末尾，文字后面不会多出一块选不掉的高亮）。
// 文本读写、选区、撤销重做、剪贴板、滚动条等基类能力已由 DuiRichEditTests
// 覆盖，这里不重复。
//
// 绝大多数用例**不创建任何窗口**，也不挂进 DUI 树 —— 排版引擎本身不依赖窗口，
// 属性开关与布局换算都能直接验证。只有「观察控件发给宿主的通知」这一类必须
// 有真窗口：通知是经 DuiHost 同步 SendMessage 给宿主窗口的父窗口的，没有父
// 窗口就无处可观察。那几条用例自建一个挪到屏幕外的顶层窗口来接收。
//
// 返回多行报告，最后一行是汇总。
namespace DuiEditTests {
    CString RunAll();
}

} // namespace balloonwjui

#endif // BUI_FEATURE_EDIT
