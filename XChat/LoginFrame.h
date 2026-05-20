#pragma once

// =============================================================================
// LoginFrame —— XChat 的登录窗（Phase 1）
//
// 360×500 固定尺寸、不可缩放、只有 ✕ close（无 min/max）。两个互斥模态：
//   Mode::Qr        ——  扫码登录占位 + "扫码登录" + "仅传输文件"
//   Mode::Loading   ——  圆角头像 + 旋转 spinner + "正在进入"
//
// 时序由 main.cpp 串起来 —— 每个模态用一个独立的 LoginFrame 实例（避免
// 同一 frame 二次 SetClientContent 切内容的边缘 bug）：
//   1) LoginFrame(Qr)      show 3s → WM_CLOSE → 退出消息循环
//   2) LoginFrame(Loading) show 3s → WM_CLOSE → 退出消息循环
//   3) XChatMainFrame    主消息循环到进程退出
//
// XML：login.xml / login_loading.xml 通过 DuiXmlBuilder 构造，自定义 tag
// 由 MakeLoginXmlFactory 产生的 CustomFactory 接住：
//   <control/>                              —— invisible spacer
//   <qr-image src="qr_placeholder.png"/>    —— 静态 PNG 显示控件
//   <spinner color="7,193,96" thickness="2"/> —— 旋转圆环 spinner
//   <login-avatar/>                         —— DuiAvatar 圆角矩形 + 缩写 fallback
// =============================================================================

#include "Controls/Window/DuiFrameWindow.h"
#include "DuiXmlBuilder.h"

namespace xchat {

// 创建 login.xml / login_loading.xml 用的 custom factory。Phase 1 只接
// <qr-image>、<spinner>、<login-avatar> 三个自定义 tag；其它走 builder
// 的内置标签。返回的 factory 不持有外部状态，可重复用于多次解析。
balloonwjui::DuiXmlBuilder::CustomFactory MakeLoginXmlFactory();

class LoginFrame : public balloonwjui::DuiFrameWindow
{
public:
    enum Mode { Qr, Loading };

    BEGIN_MSG_MAP(LoginFrame)
        MESSAGE_HANDLER(WM_CREATE,  OnCreate_)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy_)
        MESSAGE_HANDLER(WM_TIMER,   OnTimer_)
        CHAIN_MSG_MAP(balloonwjui::DuiFrameWindow)
    END_MSG_MAP()

    explicit LoginFrame(Mode m);
    ~LoginFrame();

    // 加载 m_mode 对应的 XML 到客户区，并启动 3s timer：
    //   · QR mode：3s 后自动 SwitchMode(Loading) 并重新加载 XML，再起 3s
    //     close timer；
    //   · Loading mode：3s 后 PostMessage(WM_CLOSE) 关窗。
    // Create() 之后立即调用一次。
    void ShowAndStartLoginSequence();

    // 切换 mode 并重新加载对应 XML（同一 frame 内 SetClientContent 二次
    // 调用，依赖 DuiFrameWindow::LayoutSkeleton 走 ForceLayout 路径才能
    // 正确排版）。
    void SwitchMode(Mode m);

private:
    LRESULT OnCreate_ (UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnDestroy_(UINT, WPARAM, LPARAM, BOOL& bHandled);
    LRESULT OnTimer_  (UINT, WPARAM, LPARAM, BOOL& bHandled);

private:
    Mode m_mode;
};

} // namespace xchat
