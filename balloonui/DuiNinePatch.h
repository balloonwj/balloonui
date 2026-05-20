#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiNinePatch —— 9-grid（nine-patch）位图绘制器
// =================================================================
//
// 用途：把一张装饰位图按 4 个内距值（left/top/right/bottom）切成 9 块；
// 4 个角原样 1:1 blit、4 条边沿单轴拉伸、中央双轴拉伸 —— 让位图
// 缩放到任意目标尺寸时<u>角不变形</u>。DuiButton 的按状态背景、EditHost
// 的边框、DuiHost 的整窗 bg 都走它。详见 guides.html §10。
//
//      +---+--------+---+
//      | TL|   T    |TR |    <- 顶 height = ins.top
//      +---+--------+---+
//      |   |        |   |
//      | L |   C    | R |    <- 中间行竖向拉伸
//      |   |        |   |
//      +---+--------+---+
//      | BL|   B    |BR |    <- 底 height = ins.bottom
//      +---+--------+---+
//
// 设计为<u>自由函数</u>（不封类）—— 同一张 bitmap 可以喂多个 host
// 重复用、零 per-paint 状态。
//
// 为什么不用老 CImageEx::DrawNinePartImage？
//   · CImageEx 绑死了磁盘 image cache，单测里没法用。本模块直接吃
//     HBITMAP，测试可以 CreateDIBSection 合成 9×9 测试图、Draw 后
//     GetPixel 验。
//   · 老版只支持 stretch；本模块保留 Mode enum 给未来 Tile 模式
//     （纹理背景 / 刷边条）扩展空间，不破坏 API。
//
// 坐标 / bitmap 约定：
//   · srcW / srcH = 位图像素尺寸。
//   · dstRect = HDC 客户区坐标。
//   · ComputeCells 输出顺序：TL, T, TR, L, C, R, BL, B, BR。退化（dst
//     太小） cell 的 src/dst 都置零，caller 用 IsRectEmpty 跳过。
//   · Insets 自动 clamp：left+right > srcW 时按比例缩；负值 clamp 0。
//
// 代码用法：
//
//     balloonwjui::DuiNinePatch::Insets ins{ 8, 8, 8, 8 };
//     balloonwjui::DuiNinePatch::Draw(hdc, hbmSkin, rcButton, ins);
//
//     // 或自己处理 9 个 (src,dst) pair：
//     balloonwjui::DuiNinePatch::Cell cells[9];
//     balloonwjui::DuiNinePatch::ComputeCells(srcW, srcH, rcButton, ins, cells);
//     for (int i = 0; i < 9; ++i)
//     {
//         if (!::IsRectEmpty(&cells[i].dst))
//         {
//             StretchBlit(cells[i].src, cells[i].dst);
//         }
//     }
//
// XML 用法：通过 DuiHost::SetBgImage（详见 guides.html §10 + §11
// frame-window 的 bg-image / bg-src-insets / bg-dst-insets 属性）。

#include <windows.h>
#include "BalloonUiApi.h"

namespace balloonwjui {

namespace DuiNinePatch
{
    struct Insets
    {
        int left   = 0;
        int top    = 0;
        int right  = 0;
        int bottom = 0;
    };

    enum class Mode
    {
        Stretch,  // bilinear stretch (current default, used by LoginDlg / Buttons)
        Tile,     // tile pattern (reserved; not yet implemented in Draw)
    };

    struct Options
    {
        Mode hMode    = Mode::Stretch;  // top/bottom edge + center, horizontal axis
        Mode vMode    = Mode::Stretch;  // left/right edge + center, vertical axis
        bool hasAlpha = true;           // honor 32bpp alpha via GDI+ premultiply path
    };

    struct Cell
    {
        RECT src;
        RECT dst;
    };

    // Saturates insets so left+right <= srcW and top+bottom <= srcH; clamps
    // any negative value to 0. If a pair overflows the source, both values
    // shrink proportionally so corners never overlap.
    BUI_API Insets ClampInsets(int srcW, int srcH, const Insets& ins);

    // Pure math: emit 9 (src,dst) pairs into out[].
    //
    // Output order (matches array indices):
    //   0=TL  1=T   2=TR
    //   3=L   4=C   5=R
    //   6=BL  7=B   8=BR
    //
    // out[] must point to at least 9 cells. ins is clamped internally.
    // Cells whose dst is degenerate (right<=left or bottom<=top) come back
    // with src zero-sized too so the paint loop can skip them via
    // IsRectEmpty.
    BUI_API void ComputeCells(int srcW, int srcH,
                              const RECT& dstRect,
                              const Insets& ins,
                              Cell out[9]);

    // 双 inset 重载：源内距和目标内距独立。
    //
    // 当两者相等时，等价于上面的单 inset 版本（4 角 1:1，4 边一轴拉伸，中央双轴）。
    // 当两者不同时，4 角也参与缩放（src.thickness → dst.thickness）。
    //
    // 典型用法：源图顶部有一条装饰带（如 70px 渐变），但希望它呈现成 40px 高的标题栏 —
    // 让整条装饰带等比压缩进窗口的标题栏，下方业务区不会被装饰带"漏一截"。
    //
    //   srcIns = { L=10, T=70, R=10, B=10 };   // 源里"不缩"区域的厚度
    //   dstIns = { L=10, T=40, R=10, B=10 };   // 目标里对应区域的厚度
    BUI_API void ComputeCells(int srcW, int srcH,
                              const RECT& dstRect,
                              const Insets& srcIns,
                              const Insets& dstIns,
                              Cell out[9]);

    // Paint hbm into hdc using the cell layout above.
    //
    // Returns false if hbm is null, its dimensions can't be queried, or
    // dstRect is empty. Returns true after a no-op when both src and dst
    // have zero size in every cell (still considered a successful paint
    // request). Currently always uses Stretch mode regardless of opts.hMode
    // / opts.vMode (Tile reserved for a future change).
    BUI_API bool Draw(HDC hdc, HBITMAP hbm,
                      const RECT& dstRect,
                      const Insets& insets,
                      const Options& opts = Options());

    // 双 inset Draw 重载（见上面 ComputeCells 双 inset 版本说明）。
    BUI_API bool Draw(HDC hdc, HBITMAP hbm,
                      const RECT& dstRect,
                      const Insets& srcInsets,
                      const Insets& dstInsets,
                      const Options& opts = Options());
}

} // namespace balloonwjui
