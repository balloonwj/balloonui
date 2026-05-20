#pragma once

// =============================================================================
// XChatMainFrame —— XChat 主窗（Phase 2+）
//
// 1100×720 三栏：
//   左 75px  ：深色 nav 栏（avatar + 7 主图标 + 2 底部图标 + 选中绿条）
//   中 290px ：会话列表（Phase 3 起填充；Phase 2 只放搜索行占位 + 空白）
//   右 fill  ：浅灰背景 + 居中"某信" logo 占位（Phase 4 起换聊天区）
//
// XML：main.xml 通过 DuiXmlBuilder + MakeMainXmlFactory 构造，自定义 tag：
//   <color-panel color="r,g,b">...</color-panel>     —— 带背景色的 vbox
//   <nav-icon idx="N" selected="true|false"/>        —— 60×52 nav 单元
//   <logo-image src="wechat_logo_gray.png"/>         —— 静态 PNG（同 LoginFrame::QrImage）
//   <login-avatar/>                                  —— 复用 LoginFrame 的圆角头像
//
// 60Hz pulse：本 frame 暂无动画控件（Phase 2 阶段），但保留 timer 注册，
// Phase 5+ 加 spinner / 切换动画时无需再改这里。
// =============================================================================

#include "Controls/Window/DuiFrameWindow.h"
#include "DuiXmlBuilder.h"
#include "DuiNotify.h"

namespace xchat {

balloonwjui::DuiXmlBuilder::CustomFactory MakeMainXmlFactory();

class XChatMainFrame : public balloonwjui::DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(XChatMainFrame)
        MESSAGE_HANDLER(WM_CREATE,     OnCreate_)
        MESSAGE_HANDLER(WM_DESTROY,    OnDestroy_)
        MESSAGE_HANDLER(WM_TIMER,      OnTimer_)
        MESSAGE_HANDLER(WM_DUI_NOTIFY, OnDuiNotify_)
        CHAIN_MSG_MAP(balloonwjui::DuiFrameWindow)
    END_MSG_MAP()

    // 加载 main.xml 到客户区。Create() + ApplyConfig() 之后调用。
    //   xmlName：xml 文件名（如 "main.xml" / "main_contacts.xml"）。
    //            nullptr 等价于 "main.xml"。
    void LoadMainXml(LPCTSTR xmlName = nullptr);

    // 按当前选中 session 的 pageType 切换右栏 view（id 200/210/202 显隐
    // + chat-title (id=300) 文本）。LoadMainXml 末尾会用初始 idx=0 调一次；
    // 之后每次 session-list 选中变化也调。
    //   sessionIdx >= 0：按对应 session 的 pageType 切 chat / publics 视图
    //   sessionIdx == -1：空 chat 状态，仅显示 watermark pane (id=204)
    void ApplySessionView(int sessionIdx);

    // 切到"空 chat"状态：清掉 session-list 选中 + 显示 watermark。供
    // --empty CLI arg 用，单独走这条路径以便 demo 截图。
    void ShowEmptyView();

private:
    LRESULT OnCreate_   (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnDestroy_  (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnTimer_    (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnDuiNotify_(UINT, WPARAM, LPARAM, BOOL& bHandled);

    void    ShowHamburgerMenu_(balloonwjui::DuiControl* navIcon);
};

} // namespace xchat
