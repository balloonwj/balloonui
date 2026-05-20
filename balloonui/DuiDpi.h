#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiDpi —— Per-Monitor DPI 辅助函数
// =================================================================
//
// 用途：高 DPI 显示器（125% / 150% / 200% 缩放）支持。把"逻辑像素"
// （96 dpi 设计基线）和"物理像素"（当前显示器实际像素）之间互转，
// 让控件几何代码写出的 "8 px padding" 在任何 DPI 下都是 8 个<u>逻辑</u>
// 像素 —— 系统按 monitor DPI 自动放大。
//
// 关键术语（详见 guides.html §12 术语表）：
//   · 逻辑 px / DIP = 96 dpi 下的像素值（设计稿尺寸）。
//   · 物理 px / device px = 当前显示器的实际像素。
//   · Scale(v, dpi)   = 逻辑 → 物理（四舍五入）。
//   · Unscale(v, dpi) = 物理 → 逻辑（向 0 取整）。
//
// 所有 helper 都 nothrow + 无副作用，<u>除了</u> OptInPerMonitorV2 —— 它
// 改进程状态，应在 WinMain 早期、任何 HWND 创建前<u>恰好</u>调一次。

#include <windows.h>
#include "BalloonUiApi.h"

namespace balloonwjui {

// 代码用法：
//
//     // WinMain 早期（任何 HWND 之前）调一次：
//     balloonwjui::DuiDpi::OptInPerMonitorV2();
//
//     // 任意控件的 paint / layout 里：
//     int dpi   = balloonwjui::DuiDpi::GetWindowDpi(hwnd);
//     int padPx = balloonwjui::DuiDpi::Scale(8, dpi);   // 8 逻辑 → 物理
//     int sysDpi = balloonwjui::DuiDpi::GetSystemDpi();
namespace DuiDpi
{
    // 96 dpi 是 100% 不缩放基线。
    enum { kDefaultDpi = 96 };

    // 逻辑 px 转物理 px。dpi <= 0 时按 kDefaultDpi 处理（不缩放）。
    inline int Scale(int logical, int dpi)
    {
        if (dpi <= 0)
        {
            dpi = kDefaultDpi;
        }
        return ::MulDiv(logical, dpi, kDefaultDpi);
    }

    // 物理 px 转逻辑 px。dpi <= 0 时按 kDefaultDpi 处理。
    inline int Unscale(int device, int dpi)
    {
        if (dpi <= 0)
        {
            dpi = kDefaultDpi;
        }
        return ::MulDiv(device, kDefaultDpi, dpi);
    }

    // 取主显示器在 X 轴方向的 DPI。GetDC 失败时回退 96。
    BUI_API int GetSystemDpi();

    // 取指定 HWND 当前所在显示器的 DPI（per-monitor v2 感知）。老系统
    // 或 API 缺失时回退到 GetSystemDpi()。
    //   hwnd：要查询的窗口；nullptr 时查主显示器。
    //   返回：DPI 数值（96 / 120 / 144 / 192 等）。
    BUI_API int GetWindowDpi(HWND hwnd);

    // 把进程加入 per-monitor v2 DPI 感知（Windows 10 1703+ 支持）。
    //   返回：true 表示成功，或 API 不存在（caller 不要把"API 缺"当
    //         失败处理 —— 老系统就当 system-aware 即可）。
    // 在 WinMain 任何 HWND 之前调一次。
    BUI_API bool OptInPerMonitorV2();
}

} // namespace balloonwjui
