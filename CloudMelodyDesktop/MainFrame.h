#pragma once

// =============================================================================
// CloudMelodyMainFrame —— 方Music 主窗（M0 占位版）
//
// 目标：1100×720 四区 dock 布局
//   顶 48px  ：TopBar（M0 占位 ColorPanel；M1 起放后退/前进/搜索/头像/min-max-close）
//   左 220px ：Sidebar（M0 占位；M1 起放品牌 + 主导航 + "+ 创建歌单"）
//   底 80px  ：PlayerBar（M0 占位；M5 起放封面/transport/progress/volume）
//   中 fill  ：ContentHost（M0 占位；M3+ 起切 8 个 page）
//
// XML：main.xml 用 <vbox>/<hbox> + <color-panel>（CustomFactory 注册）拼出
// 4 区。M0 阶段 ContentHost 是空白，纯验证比例。
// =============================================================================

#include "Controls/Window/DuiFrameWindow.h"
#include "DuiXmlBuilder.h"
#include "DuiNotify.h"

#include "App/DesktopLyricsWnd.h"

namespace cloudmelody {

balloonwjui::DuiXmlBuilder::CustomFactory MakeMainXmlFactory();

class CloudMelodyMainFrame : public balloonwjui::DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(CloudMelodyMainFrame)
        MESSAGE_HANDLER(WM_CREATE,     OnCreate_)
        MESSAGE_HANDLER(WM_DESTROY,    OnDestroy_)
        MESSAGE_HANDLER(WM_TIMER,      OnTimer_)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify_)
        CHAIN_MSG_MAP(balloonwjui::DuiFrameWindow)
    END_MSG_MAP()

    void LoadMainXml(LPCTSTR xmlName = nullptr);

private:
    LRESULT OnCreate_   (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnDestroy_  (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnTimer_    (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnDuiNotify_(UINT, WPARAM, LPARAM, BOOL& bHandled);

    // 全屏 / 桌面歌词 helpers
    void ToggleFullscreen_();

    bool             m_isFullscreen = false;
    DesktopLyricsWnd m_lyricsWnd;
};

} // namespace cloudmelody
