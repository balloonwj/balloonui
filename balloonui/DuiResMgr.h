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
//      (GB2312)，懒构造、跟随 DPI 变化重建。
//   3) 按 (pt, bold) 缓存的 UI 字体：GetFontByPointSize 给控件
//      （DuiButton::SetTextPointSize 等）提供"指定字号"字体，避免每次
//      ::CreateFont 句柄泄漏；缓存按 DPI 变化整体清空、惰性重建。
//
// 工作机制：
//   · 单例，Inst() 拿。
//   · LoadImage / GetImage / ReleaseImage 三件套透传到 CSkinManager。
//   · GetDefaultFont 懒构造一次（按当前 DPI），返回的 HFONT<u>不要</u>
//     DeleteObject。Per-monitor DPI 切换时 host 调 SetDpi(newDpi)，
//     manager 重建字体后续 GetDefaultFont 返回缩放过的 handle。
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
    // 同一 (pt, bold) 多次调用返回同一 HFONT；缓存随 SetDpi 整体清空、
    // 下次访问惰性重建。<u>不要</u> DeleteObject —— 所有权在 manager。
    // 用途：业务控件需要"非 9pt"字体时调，避免业务侧自管 ::CreateFont
    // 句柄泄漏；典型 caller：DuiButton::SetTextPointSize 内部。
    HFONT       GetFontByPointSize(int pt, bool bold = false);

    // 取按 (pt, bold) 缓存的字体，但走 ANTIALIASED_QUALITY(灰度 AA)
    // 而非 ClearType。用于走 PARGB + AlphaBlend 合成的控件(如 DuiToast)
    // —— ClearType 字体在透明合成时会有"子像素错位重影"。
    // 同 GetFontByPointSize:DPI-aware, 缓存, SetDpi 清空, 不要 DeleteObject。
    HFONT       GetAntiAliasedFontByPointSize(int pt, bool bold = false);

    // 设置当前 DPI。manager 据此重建默认字体。host 在 WM_DPICHANGED
    // 时调；caller 业务一般不调。
    void        SetDpi(int dpi);
    int         GetDpi() const { return m_dpi; }

private:
    DuiResMgr() = default;
    ~DuiResMgr();
    DuiResMgr(const DuiResMgr&) = delete;
    DuiResMgr& operator=(const DuiResMgr&) = delete;

private:
    HFONT       m_hDefaultFont = nullptr;
    int         m_dpi          = 0;       // 0 = 首次构建时取系统 DPI

    // (pt, bold) → HFONT 缓存。key = (pt << 1) | (bold ? 1 : 0);
    // SetDpi 时整张表清空、惰性重建,与 m_hDefaultFont 同生命周期管理。
    std::map<int, HFONT> m_fontCache;
    // 同 m_fontCache 但走 ANTIALIASED_QUALITY, 服务 PARGB 合成场景。
    std::map<int, HFONT> m_aaFontCache;
};

} // namespace balloonwjui
