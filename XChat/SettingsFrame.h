#pragma once

// =============================================================================
// SettingsFrame —— XChat 的设置弹窗（Phase 8）。
//
// 独立 DuiFrameWindow，800×680，无 min/max（参考 third_party/wechat/设置详情.png
// 标题栏只有 ✕ close）。布局：左 140px 竖向 tab list（6 项，第 0 项"账号
// 与存储"默认选中）+ 右 fill 内容区（账号信息卡 + 存储卡 + 多个 DuiSwitch）。
//
// 由 MainFrame 的汉堡菜单"设置"项触发；模态行为通过 RunModalLoop()——
// 父 frame 在等待期间被 ::EnableWindow(false) 禁掉，关本窗后恢复，
// 跟 MFC CDialog::DoModal 相同套路。
// =============================================================================

#include "Controls/Window/DuiFrameWindow.h"
#include "DuiXmlBuilder.h"

namespace xchat {

class SettingsFrame : public balloonwjui::DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(SettingsFrame)
        MESSAGE_HANDLER(WM_CREATE,  OnCreate_)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy_)
        CHAIN_MSG_MAP(balloonwjui::DuiFrameWindow)
    END_MSG_MAP()

    // 加载 settings.xml；Create() 之后调一次即可。
    void LoadSettingsXml();

    // 模态运行：禁掉 ownerHwnd，跑自己的消息循环到 Close()，再恢复 owner。
    // 同 MFC CDialog::DoModal —— 但 DoModal 用 OS modal-loop，本类用普通
    // GetMessage 循环 + EnableWindow 切换，行为一致但不依赖 dialog manager。
    void RunModal(HWND ownerHwnd);

private:
    LRESULT OnCreate_ (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnDestroy_(UINT, WPARAM, LPARAM, BOOL& bHandled);

private:
    HWND m_owner = nullptr;
    bool m_modalRunning = false;
};

balloonwjui::DuiXmlBuilder::CustomFactory MakeSettingsXmlFactory();

} // namespace xchat
