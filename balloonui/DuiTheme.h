#pragma once

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。

// =================================================================
// DuiTheme —— 进程级主题（命名色板 + 默认字体）
// =================================================================
//
// 用途：让品牌蓝 / hover 浅灰 / 选中行底色等样式有一份"命名"的中心
// 来源，停止在每个控件里重复 RGB 数字字面量。控件 OnPaint 时读
// DuiTheme::Inst().Get(slot) 拿色。
//
// V1 保持最小：
//   · Singleton DuiTheme::Inst() 持有一组固定的色槽（Slot enum）+ 字体
//     face + 字号。
//   · Get(slot) 取色；Set(slot, color) 改色（递增 version 号）。
//   · ApplyPreset(Light/Dark/HighContrast) 一键换整套。
//   · SubscribeChange(cb, ud) / Unsubscribe(token) 监听变化（典型
//     从设置对话框切主题时所有打开窗口刷重绘）。
//
// 线程安全：<u>不</u>安全，仅 UI 线程访问。控件 OnPaint 里调 Get() 是
// vector 索引，开销可忽略。
//
// 迁移计划：现有控件仍硬编码颜色；新控件<u>建议</u>用 DuiTheme。本期
// 不批量改写所有 paint 路径；那是 Sprint A 替换真实对话框时的任务。
//
// 代码用法：
//
//     COLORREF c = balloonwjui::DuiTheme::Inst().Get(
//         balloonwjui::DuiTheme::BrandPrimary);
//     balloonwjui::DuiTheme::Inst().ApplyPreset(balloonwjui::DuiTheme::Dark);
//
//     // 监听主题变化（如设置对话框切主题）：
//     int token = balloonwjui::DuiTheme::Inst().SubscribeChange(
//         [](void* ud) { ((MyControl*)ud)->Invalidate(); }, this);
//     // ...
//     balloonwjui::DuiTheme::Inst().Unsubscribe(token);
//
// XML 用法：N/A（manager，不是控件）。

#include <windows.h>
#include <vector>
#include <functional>

namespace balloonwjui {

// Usage:
//   COLORREF c = balloonwjui::DuiTheme::Inst().Get(balloonwjui::DuiTheme::BrandPrimary);
//   balloonwjui::DuiTheme::Inst().ApplyPreset(balloonwjui::DuiTheme::Dark);
//
//   // Listen for theme changes (e.g. preset switch from a settings dialog):
//   int token = balloonwjui::DuiTheme::Inst().SubscribeChange(
//       [](void* ud) { ((MyControl*)ud)->Invalidate(); }, this);
//   // ...
//   balloonwjui::DuiTheme::Inst().Unsubscribe(token);
class DuiTheme
{
public:
    // Slot ids. Add at the END to keep existing values stable.
    enum Slot
    {
        BrandPrimary = 0,    // primary action / focus stroke
        BrandHover,
        BrandPressed,
        BrandBorder,
        TextOnPrimary,       // text drawn on top of BrandPrimary fills
        TextDefault,
        TextSubtle,          // gray helper text
        TextDisabled,
        TextLink,
        SurfaceBg,           // window / dialog background
        SurfaceAltBg,        // secondary surface (cards / popups)
        BorderLight,
        BorderHeavy,
        RowHover,
        RowSel,              // selected row fill
        TextOnRowSel,
        StatusOnline,        // online dot color
        StatusAway,
        StatusBusy,
        StatusOffline,

        SlotCount            // sentinel; keep last
    };

    enum Preset { Light = 0, Dark = 1, HighContrast = 2 };

    typedef void (*ChangeCallback)(void* userdata);

    static DuiTheme& Inst();

    COLORREF Get(Slot s) const;
    void     Set(Slot s, COLORREF c);

    void     ApplyPreset(Preset p);

    // Monotonic version. Bumps on every Set / ApplyPreset.
    unsigned GetVersion() const { return m_version; }

    // Optional listener registry. Fires after the palette mutates.
    int      SubscribeChange(ChangeCallback cb, void* userdata = nullptr);
    void     Unsubscribe(int token);

    // Default font face / point size hooks. The actual HFONT comes from
    // DuiResMgr; setting these here rebuilds it.
    void     SetDefaultFontFace(LPCTSTR face);
    CString  GetDefaultFontFace() const { return m_fontFace; }
    void     SetDefaultFontPt(int pt);
    int      GetDefaultFontPt() const { return m_fontPt; }

    // Pure-data accessors for tests.
    int      GetSubscriberCount() const { return (int)m_subs.size(); }

private:
    DuiTheme();
    ~DuiTheme() = default;
    DuiTheme(const DuiTheme&) = delete;
    DuiTheme& operator=(const DuiTheme&) = delete;

    void     FillLight();
    void     FillDark();
    void     FillHighContrast();
    void     Bump();

    struct Sub { int token; ChangeCallback cb; void* ud; };

    COLORREF          m_colors[SlotCount];
    unsigned          m_version = 1;
    int               m_nextToken = 1;
    std::vector<Sub>  m_subs;
    CString           m_fontFace;
    int               m_fontPt = 9;
};

} // namespace balloonwjui
