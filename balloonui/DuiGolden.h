#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiGolden —— DUI 控件渲染的"golden"截图回归测试框架
// =================================================================
//
// 用途：把控件的某种配置渲染到内存 DC，然后跟磁盘上一张"已知正确"
// 的 PNG（golden）做像素级比较。任何渲染回归（缩进改了 1px、颜色
// 变了一档）都会立刻被自动测试 catch 住。
//
// 工作流：
//   · 首次跑新 case 或 set DUI_GOLDEN_UPDATE=1：把实际渲染当 golden 写
//     出去。人工 inspect 那张 PNG 看着合理，commit。
//   · 平常跑：加载 golden，对比 actual。不匹配时把 actual.png +
//     diff.png 写到 %TEMP%\DuiGolden\，case 失败。
//
// 容差默认 ±2 / 通道：GDI+ 抗锯齿在不同 Windows 版本上略有差异，
// 1px halftone 严格匹配会出 false-positive。总不匹配像素数必须 <
// 0.5% 面积。
//
// 纯 helper（CompareHbm / ReadPx / WritePx）单测不需要磁盘 PNG；磁盘
// 流程在 DuiGoldenTests 里有个 in-process round-trip case 覆盖。
//
// 代码用法：
//
//     // 测试 case 里：
//     auto btn = std::make_unique<balloonwjui::DuiButton>();
//     btn->SetText(_T("OK"));
//     bool ok = balloonwjui::DuiGolden::RunGoldenTest(
//         _T("Button_Default"), btn.get(), /*w=*/80, /*h=*/24);
//     EXPECT_TRUE(ok);
//
//     // 开发时更新 golden：
//     //   set DUI_GOLDEN_UPDATE=1
//     //   <跑测试>      -> 写出新 PNG，inspect 后 commit。
//
// XML 用法：N/A（测试框架）。

#include <windows.h>
#include "DuiControl.h"

namespace balloonwjui {

// Usage:
//   // From a test case:
//   auto btn = std::make_unique<balloonwjui::DuiButton>();
//   btn->SetText(_T("OK"));
//   bool ok = balloonwjui::DuiGolden::RunGoldenTest(
//       _T("Button_Default"), btn.get(), /*w=*/80, /*h=*/24);
//   EXPECT_TRUE(ok);
//
//   // Update goldens during dev with environment variable:
//   //   set DUI_GOLDEN_UPDATE=1
//   //   <run tests>     -> writes new PNG, inspect, then commit it.

namespace DuiGolden
{
    struct DiffSummary
    {
        int  diffPixels;       // count of pixels exceeding tolerance
        int  totalPixels;      // bitmap area (sanity)
        int  maxChannelDelta;  // worst-case per-channel diff observed
        bool sizeMismatch;     // true → actual and golden are not same WxH
    };

    // Render a DuiControl into a 32bpp top-down DIBSection. Sets the
    // control's rect to (0, 0, w, h), kicks one OnPaint pass over a
    // white-cleared DC, returns the HBITMAP. Caller owns it
    // (::DeleteObject when done).
    HBITMAP RenderControlToHbm(DuiControl* c, int w, int h);

    // Save a 32bpp DIBSection HBITMAP as a PNG file. Returns true on
    // success. Creates the parent directory if needed.
    bool    SaveHbmAsPng(HBITMAP hbm, LPCTSTR path);

    // Load a PNG into a 32bpp DIBSection HBITMAP. Caller owns it.
    HBITMAP LoadPngAsHbm(LPCTSTR path);

    // Pure pixel comparison. `tolerance` is the max acceptable
    // per-channel delta before a pixel counts as different.
    DiffSummary CompareHbm(HBITMAP actual, HBITMAP golden, int tolerance = 2);

    // Build a red-on-white "diff map" highlighting pixels that differ
    // beyond tolerance. Useful as a build artifact when a test fails.
    HBITMAP MakeDiffHbm(HBITMAP actual, HBITMAP golden, int tolerance = 2);

    // Resolve the path to a golden file based on a test name. Layout:
    //   <exe-dir>\..\Goldens\<name>.png
    // (Bin\..\Goldens\, so the file lives next to the source tree, not
    // inside Bin\ where it's harder to commit.)
    CString ResolveGoldenPath(LPCTSTR name);

    // Where mismatch artifacts get written.
    CString GetArtifactDir();

    // Test driver. Returns true when the rendered control matches the
    // golden (or when DUI_GOLDEN_UPDATE was set, after writing the
    // golden). Returns false on mismatch + writes actual / diff PNGs
    // to GetArtifactDir().
    bool    RunGoldenTest(LPCTSTR testName, DuiControl* c, int w, int h,
                          int tolerance = 2);
}

} // namespace balloonwjui
