#pragma once

#include "SkinManager.h"
#include "BalloonUiApi.h"
#include <map>

class CImageEx;

namespace balloonwjui {

// =================================================================
// DuiResMgr —— DUI 资源管理器（皮肤图 + 共享字体）
// =================================================================
//
// 用途：DUI 控件需要的两样东西的统一入口：
//   1) 皮肤图（CImageEx*）：包装老 CSkinManager，避免控件代码直接接老
//      单例，方便测试时 mock，也让迁移期"refcount 在 DUI 这一侧管控"。
//   2) 默认 UI 字体：进程级共享的 HFONT —— Microsoft YaHei 9pt
//      (GB2312)，按当前 DPI 懒构造。
//   3) 按 (pt, bold) 缓存的 UI 字体：GetFontByPointSize 给控件
//      （DuiButton::SetTextPointSize 等）提供"指定字号"字体，避免每次
//      ::CreateFont 句柄泄漏。
//
// 工作机制：
//   · 单例，Inst() 拿。
//   · LoadImage / GetImage / ReleaseImage 三件套透传到 CSkinManager。
//   · 字体按 (DPI, 磅值, 是否加粗) 分别缓存、懒构造，返回的 HFONT<u>不要</u>
//     DeleteObject。Per-monitor DPI 切换时 host 调 SetDpi(newDpi)，之后取到的
//     是按新 DPI 缩放的字体。
//   · SetDpi <u>不销毁</u>任何已创建的字体：控件会把取到的句柄长期保存
//    （DuiButton::SetTextPointSize、DuiLabel::SetFont 等），销毁后它们会用已失效的
//     句柄绘制。切回用过的 DPI 时直接复用当时的字体，所以字体总数只取决于出现过的
//     DPI 档数与用到的字号种类数，不随切换次数增长。全部字体在进程退出时统一释放。
//
// 代码用法：
//
//     HFONT hFont = balloonwjui::DuiResMgr::Inst().GetDefaultFont();
//     ::SelectObject(hdc, hFont);
//
//     // host 收到 WM_DPICHANGED 时：
//     balloonwjui::DuiResMgr::Inst().SetDpi(newDpi);
//
// XML 用法：N/A（不是控件，是资源 manager）。
class BUI_API DuiResMgr
{
public:
    // 取单例。线程不安全，仅 host 线程调。
    static DuiResMgr& Inst();

    // 按文件名加载皮肤图（透传 CSkinManager）。
    //   返回：CImageEx* 或 nullptr（找不到 / 解码失败）。
    CImageEx*   LoadImage(LPCTSTR lpszFileName);

    // 按文件名查已加载的图（不存在返 nullptr，<u>不</u>触发加载）。
    CImageEx*   GetImage (LPCTSTR lpszFileName);

    // Release 一张图（refcount-1）。会清空 caller 持有的指针。
    void        ReleaseImage(CImageEx*& lpImg);

    // 便捷：load-or-get 一次性。优先 GetImage 命中已加载，未命中再
    // LoadImage。
    CImageEx*   AcquireImage(LPCTSTR lpszFileName);

    // 取共享默认字体（Microsoft YaHei 9pt GB2312）。懒构造在首次调用，
    // 进程退出时统一释放。<u>不要</u> DeleteObject。
    HFONT       GetDefaultFont();

    // 取按 (pt, bold) 缓存的字体（Microsoft YaHei，DPI-aware）。
    //   pt：磅值（点字号），如 9 / 11 / 14。pt <= 0 → 返回默认字体。
    //   bold：true 用 FW_BOLD；false（默认）用 FW_NORMAL。
    // 同一 DPI 下同一 (pt, bold) 多次调用返回同一 HFONT；DPI 变化后返回按新 DPI
    // 缩放的另一份，旧句柄仍然有效。<u>不要</u> DeleteObject —— 所有权在 manager。
    // 用途：业务控件需要"非 9pt"字体时调，避免业务侧自管 ::CreateFont
    // 句柄泄漏；典型 caller：DuiButton::SetTextPointSize 内部。
    HFONT       GetFontByPointSize(int pt, bool bold = false);

    // 取按 (pt, bold) 缓存的字体，但走 ANTIALIASED_QUALITY(灰度 AA)
    // 而非 ClearType。用于走 PARGB + AlphaBlend 合成的控件(如 DuiToast)
    // —— ClearType 字体在透明合成时会有"子像素错位重影"。
    // 同 GetFontByPointSize:DPI-aware, 缓存, SetDpi 不销毁旧句柄, 不要 DeleteObject。
    HFONT       GetAntiAliasedFontByPointSize(int pt, bool bold = false);

    // 以下三个与上面三个一一对应，区别在于按参数 dpi 取字体，而不是按当前全局 DPI。
    // 控件应优先经 DuiControl::GetDefaultFont 等便捷函数间接调用它们 —— 那些函数
    // 传入的是控件所在窗口的 DPI，不同缩放比例显示器上的窗口因此各用各的字号。
    // 上面三个按全局 DPI 取的接口只留给不属于任何窗口的代码使用。
    //   dpi：字体对应的 DPI；<= 0 时按当前全局 DPI 处理。不改变全局 DPI。
    //   pt / bold：同上面对应的接口。
    //   返回：缓存中的字体，所有权在 manager，<u>不要</u> DeleteObject。
    HFONT       GetDefaultFontForDpi(int dpi);
    HFONT       GetFontByPointSizeForDpi(int pt, bool bold, int dpi);
    HFONT       GetAntiAliasedFontByPointSizeForDpi(int pt, bool bold, int dpi);

    // 设置当前 DPI，此后取字体都按这个 DPI 缩放。只切换当前值，不销毁任何已创建的
    // 字体。host 在建窗与 WM_DPICHANGED 时调；caller 业务一般不调。
    //   dpi：新的 DPI；<= 0 时按 96 处理。
    void        SetDpi(int dpi);
    int         GetDpi() const { return m_dpi; }

private:
    DuiResMgr() = default;
    ~DuiResMgr();
    DuiResMgr(const DuiResMgr&) = delete;
    DuiResMgr& operator=(const DuiResMgr&) = delete;

    // 字体缓存表。键由 DuiResMgr.cpp 的 MakeFontKey(dpi, pt, bold) 生成。
    typedef std::map<unsigned long long, HFONT> FontCache;

    // 返回当前 DPI；尚未设置过时取系统 DPI 并记下。
    int         EnsureDpi();

    // 把调用方给的 DPI 规整为可用值：> 0 原样返回，否则返回当前全局 DPI。
    int         ResolveDpi(int dpi);

    // 三个取字体接口的共用实现：在 cache 里按 (dpi, pt, bold) 查找，命中直接返回，
    // 未命中按参数新建并存入。
    //   cache：要查找 / 写入的缓存表。
    //   dpi：  字体对应的 DPI，决定字高 lfHeight = -MulDiv(pt, dpi, 72)。
    //   pt：   磅值，须 > 0。
    //   bold： true 用 FW_BOLD，false 用 FW_NORMAL。
    //   quality：LOGFONT::lfQuality（CLEARTYPE_QUALITY 或 ANTIALIASED_QUALITY）。
    //   返回：缓存中的字体；创建失败返回 nullptr。所有权在 manager。
    HFONT       GetCachedFont(FontCache& cache, int dpi, int pt, bool bold, BYTE quality);

    // 释放一张缓存表里的全部字体并清空该表，仅在析构时调用。
    static void ReleaseCache(FontCache& cache);

private:
    int         m_dpi = 0;      // 当前 DPI；0 = 尚未设置，首次取字体时取系统 DPI

    // 以下三张表都按 (DPI, 磅值, 是否加粗) 分别缓存，SetDpi 不清空，析构时统一释放。
    FontCache   m_defaultFontCache;  // 默认字体（9 磅），ClearType
    FontCache   m_fontCache;         // GetFontByPointSize 的字体，ClearType
    FontCache   m_aaFontCache;       // 同 m_fontCache 但走 ANTIALIASED_QUALITY，服务 PARGB 合成场景
};

} // namespace balloonwjui
