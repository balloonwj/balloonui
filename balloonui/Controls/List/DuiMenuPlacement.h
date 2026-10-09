/**
 *  DuiMenuPlacement.h —— 弹出菜单"落点"的几何计算
 *
 *  弹出菜单窗口的左上角原本直接放在调用方传入的坐标上，既不翻转，也不会把它拉回屏幕内，
 *  于是锚点靠近屏幕右 / 下边缘时（典型：聊天窗口最大化后输入框贴着屏幕底部、
 *  在其中右键；托盘图标在任务栏上右键），菜单会整个跑到桌面工作区外面，用户
 *  既看不见也点不着。本文件提供把落点拉回工作区内的纯几何运算。
 *
 *  约定：任何菜单 / 下拉 / 浮层在真正定位窗口
 *  之前，都必须先把落点限制在锚点所在显示器的工作区内，不允许把原始光标 / 锚点
 *  坐标直接用于定位。DuiMenuPopup::Open 已经统一调用本文件，库内所有菜单
 *  （右键菜单、菜单栏下拉、子菜单）因此自动受保护，调用方无需自行处理。
 *
 *  本文件<u>刻意不依赖任何控件</u>（只用 <windows.h> 的 POINT / SIZE / RECT），
 *  也<u>不设特性开关</u>，为的是让 balloonui 之外的模块（如客户端的
 *  balloonmain::PopupMenuPlacement，它给托盘浮窗算落点）能直接复用同一份
 *  实现，而不必各写一份必然会走样的副本。全部函数为 inline，无需单独的 .cpp。
 *
 *  典型用法：
 *      SIZE  sz     = menu.MeasureSize();
 *      POINT anchor = ...;                                  // 光标点 / 按钮锚点
 *      RECT  area   = balloonwjui::GetMenuPlacementArea(anchor);
 *      POINT origin = balloonwjui::ClampMenuOriginToRect(anchor, sz, area);
 *      // 或者一步到位：
 *      POINT origin = balloonwjui::ClampMenuOriginToWorkArea(anchor, sz);
 *
 *  balloonwj@qq.com   2026-08-12
 */

#pragma once

#include <windows.h>      // POINT / SIZE / RECT / MonitorFromPoint 等

namespace balloonwjui {

// 菜单与工作区边缘（尤其任务栏一侧）之间预留的间隙，单位：像素。让菜单浮在
// 任务栏 / 屏幕边缘之上留一道缝，观感对齐 Windows 自带菜单，也避开弹出窗口的
// 系统阴影下溢到任务栏造成的"底部被吃掉"错觉。
const int kMenuEdgeGapPx = 4;

/**
 *  把弹出菜单的左上角限制在给定矩形内（通常是显示器工作区），保证整张菜单可见。
 *
 *  策略（与 Windows 自带右键菜单的观感一致）：
 *    · 水平：默认从锚点向右展开；右边放不下则翻到锚点左侧；仍越界则贴右边缘。
 *    · 垂直：默认从锚点向下展开；下边放不下则翻到锚点上方；仍越界则贴下边缘。
 *  两个方向独立处理；最后各限制一次，保证不越过左 / 上边缘（菜单比矩形还大时取边缘）。
 *
 *  本函数幂等：落点已经完全在 area 内时原样返回，所以调用方即便
 *  已经自己算好一次落点，再经 DuiMenuPopup::Open 处理一次也不会被挪动。
 *
 *    anchor：   期望的菜单左上角（屏幕坐标，光标点 / 按钮锚点等）。
 *    menuSize： 菜单尺寸（来自 DuiMenu::MeasureSize()），单位像素。
 *    area：     允许显示的矩形（屏幕坐标，一般为 GetMenuPlacementArea 的产物）。
 *
 *  返回：修正后的菜单左上角屏幕坐标。
 */
inline POINT ClampMenuOriginToRect(POINT anchor, SIZE menuSize, RECT area)
{
    POINT pt = anchor;

    // 水平：默认向右展开；右边放不下则翻到锚点左侧；仍越界则贴右边缘；
    // 最后保证不越过左边缘（菜单比工作区还宽时取左边缘）。
    if (pt.x + menuSize.cx > area.right)
        pt.x = anchor.x - menuSize.cx;
    if (pt.x + menuSize.cx > area.right)
        pt.x = area.right - menuSize.cx;
    if (pt.x < area.left)
        pt.x = area.left;

    // 垂直：默认向下展开；下边放不下则翻到锚点上方；仍越界则贴下边缘；
    // 最后保证不越过上边缘（菜单比工作区还高时取上边缘）。
    if (pt.y + menuSize.cy > area.bottom)
        pt.y = anchor.y - menuSize.cy;
    if (pt.y + menuSize.cy > area.bottom)
        pt.y = area.bottom - menuSize.cy;
    if (pt.y < area.top)
        pt.y = area.top;

    return pt;
}

/**
 *  计算子菜单的左上角落点。
 *
 *  子菜单的展开方向与顶层菜单<u>不同</u>：顶层菜单放不下时可以翻到锚点上方，
 *  子菜单却必须始终贴着父菜单的某一侧，否则会盖住父菜单、或与自己对应的父项
 *  错开很远。策略：
 *    · 水平：默认贴父菜单右缘展开；右边放不下则整体翻到父菜单<u>左</u>侧
 *      （保留与展开时对称的 1 像素重叠）；仍放不下则贴右边缘兜底。
 *    · 垂直：默认与父项对齐向下展开；下边放不下则整体上移到贴住下边缘
 *      （<u>不</u>翻到锚点上方，那会让子菜单跑到离父项很远的地方）。
 *
 *    parentRect： 父菜单窗口的屏幕矩形。
 *    preferred：  期望落点（调用方按"父菜单右缘 + 父项行顶"算好的屏幕坐标）。
 *    subSize：    子菜单尺寸（像素）。
 *    area：       允许显示的矩形（屏幕坐标，一般为 GetMenuPlacementArea 的产物）。
 *
 *  返回：修正后的子菜单左上角屏幕坐标。
 */
inline POINT ClampSubMenuOrigin(RECT parentRect, POINT preferred, SIZE subSize, RECT area)
{
    POINT pt = preferred;

    // 水平：右边放不下 → 翻到父菜单左侧。父菜单展开子菜单时用的是
    // "父菜单右缘 - 1"，这里翻过去同样保留 1 像素重叠，两侧观感对称。
    if (pt.x + subSize.cx > area.right)
        pt.x = parentRect.left - subSize.cx + 1;
    // 翻过去仍放不下（父菜单本身已经贴着左边缘，两侧都塞不进）：贴右边缘兜底。
    if (pt.x + subSize.cx > area.right)
        pt.x = area.right - subSize.cx;
    if (pt.x < area.left)
        pt.x = area.left;

    // 垂直：只做上移，不翻转 —— 子菜单必须留在父项附近。
    if (pt.y + subSize.cy > area.bottom)
        pt.y = area.bottom - subSize.cy;
    if (pt.y < area.top)
        pt.y = area.top;

    return pt;
}

/**
 *  取 anchor 所在显示器、已留出边缘间隙的"菜单可放置矩形"。
 *
 *  = 显示器工作区 rcWork（已排除任务栏，多显示器安全）再四周向内缩
 *  kMenuEdgeGapPx。取不到工作区时依次退化到 SPI_GETWORKAREA、主屏整屏。
 *
 *    anchor：菜单锚点（屏幕坐标，通常为光标点）。
 *
 *  返回：可放置矩形（屏幕坐标）。
 */
inline RECT GetMenuPlacementArea(POINT anchor)
{
    RECT work;

    // 取锚点所在显示器的工作区（排除任务栏），多显示器安全。
    HMONITOR hMonitor = ::MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (hMonitor != NULL && ::GetMonitorInfo(hMonitor, &mi))
    {
        work = mi.rcWork;
    }
    else if (!::SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0))
    {
        // 再退化：用主屏整屏尺寸。
        work.left   = 0;
        work.top    = 0;
        work.right  = ::GetSystemMetrics(SM_CXSCREEN);
        work.bottom = ::GetSystemMetrics(SM_CYSCREEN);
    }

    // 四周向内缩一道缝（缩过头则退化为零宽 / 零高，由后续的落点修正兜底）。
    if (work.right - work.left > 2 * kMenuEdgeGapPx)
    {
        work.left  += kMenuEdgeGapPx;
        work.right -= kMenuEdgeGapPx;
    }
    if (work.bottom - work.top > 2 * kMenuEdgeGapPx)
    {
        work.top    += kMenuEdgeGapPx;
        work.bottom -= kMenuEdgeGapPx;
    }

    return work;
}

/**
 *  ClampMenuOriginToRect 的 Win32 包装：自行取 anchor 所在显示器的可放置矩形，
 *  再做修正。给"只有锚点和尺寸、不关心显示器细节"的调用方用。
 *
 *    anchor / menuSize：含义同 ClampMenuOriginToRect。
 *
 *  返回：修正后的菜单左上角屏幕坐标。
 */
inline POINT ClampMenuOriginToWorkArea(POINT anchor, SIZE menuSize)
{
    return ClampMenuOriginToRect(anchor, menuSize, GetMenuPlacementArea(anchor));
}

} // namespace balloonwjui
