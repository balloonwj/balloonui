#pragma once

// =============================================================================
// CaptureMode — DemoTextBadgeTile 自带的截图基础设施。
//
// Demo 文档要求每个示例自带 mark 机制 + --capture-all 模式（决策 4 = B）。
// 这套代码与 DuiGallery/CaptureMode.{h,cpp} 在思路上完全一致：
//
//   1) 各 demo 在构建窗口内容时，对想截的子控件调
//      CaptureMode::Mark(name, anchor) 注册一个截图位。
//   2) main.cpp 在 --capture-all <dir> 模式下：
//        - 创建一个屏外的 CaptureOwner 窗口 + DuiHost
//        - 调用业务的 BuildContent() 把内容塞进 host
//        - pump 几轮消息让 HWND 子窗口（如有）完成 layout / paint
//        - 取 host 的 back-buffer DC + iterate 所有可见 HWND 子窗口做合成
//        - 对每个 mark：BitBlt 出 anchor 的矩形保存为
//          <outDir>\demo-textbadge-<name>.png
//   3) demo 退出。
//
// 用法（main.cpp）：
//   if (lpCmdLine starts with "--capture-all") {
//       int saved = CaptureMode::RunCaptureAll(outDir, &BuildContent);
//   }
// =============================================================================

#include "DuiControl.h"
#include <atlstr.h>
#include <vector>
#include <memory>
#include <functional>

namespace CaptureMode {

// 截图位（mark）— 把 control 与一个名字关联，capture 时按名字命名 PNG。
// anchor 不持有 control（control 由 host 树持有），这里只是个 raw ptr。
struct Mark
{
    CString                  name;
    balloonwjui::DuiControl* anchor;
};

// 注册一个 mark。仅在 BuildContent() 内部调用 — 在 capture 流程之外调用
// 没意义（registry 在每次 RunCaptureAll 开头被清空）。
void RegisterMark(LPCTSTR name, balloonwjui::DuiControl* anchor);

// 头跑捕捉流程。
//
// outDir       : PNG 输出目录（不存在则自动创建）
// buildContent : demo 提供的工厂函数 — 构造一棵 DuiControl 树作为 host root，
//                构造过程中调 RegisterMark 标记需要截的子控件。
// filePrefix   : PNG 文件名前缀（不含 .png），如 "demo-textbadge"
//                每个 mark 生成 "<prefix>-<mark.name>.png"。
//
// 返回成功保存的 PNG 数；负数表示初始化失败。
int RunCaptureAll(LPCTSTR outDir,
                  LPCTSTR filePrefix,
                  std::function<std::unique_ptr<balloonwjui::DuiControl>()> buildContent);

} // namespace CaptureMode
