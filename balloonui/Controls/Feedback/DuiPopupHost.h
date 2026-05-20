#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_POPUPHOST

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiHost.h"

namespace balloonwjui {

// =================================================================
// DuiPopupHost —— 浮层窗口宿主（顶层 WS_POPUP 容纳 DUI 树）
// =================================================================
//
// 用途：所有"飘在上面"的 DUI 内容的统一基座 —— combo 下拉、菜单、
// tooltip、emoji 选择面板。给业务一个一致的"定位 / 显示 / 自动消失 /
// 通知 dismiss"流程。
//
// 工作机制：
//   · IS-A DuiHost（单 HWND、双缓冲绘制、完整 DUI 消息路由）。在
//     DuiHost 之上加顶层窗口 concerns：WS_POPUP + WS_EX_TOOLWINDOW +
//     WS_EX_TOPMOST 创建；CS_DROPSHADOW + CS_SAVEBITS 类风格。
//   · 自动消失：VK_ESCAPE → Hide(ReasonEscape)；WM_ACTIVATE
//     WA_INACTIVE → Hide(ReasonLostFocus)。失焦 dismiss 发生在新目标
//     接收 click <u>之前</u>，所以"点 popup 外面"= dismiss + 点击仍然
//     到达真实目标（菜单 / 下拉的标准行为）。
//   · Show(anchor, owner) 第一次调时懒创建 HWND，按 ComputePositionScreen
//     算 screen rect（边缘越界自动翻边、最后兜底 clamp 到工作区），
//     然后 SetWindowPos + SW_SHOW。Hide() 只 SW_HIDE，HWND + 内容保活
//     给下次 Show() 用。
//   · 生命期：实例<u>不</u>挂在父 host 的控件树里（自己持有 HWND）。
//     一般是对话框 / 控件的成员变量（如未来某 DuiComboBox 持有自家
//     dropdown）。析构销毁 HWND。
//   · 仅 host 线程访问。
//
// 代码用法（按钮 dropdown）：
//
//     class MyDlg : public CDialogImpl<MyDlg> {
//         balloonwjui::DuiPopupHost m_pop;
//         static void OnDismiss(void* ud, balloonwjui::DuiPopupHost::Reason r)
//         {
//             ((MyDlg*)ud)->m_btnDropdown.SetCheck(false);
//         }
//         void OpenDropdown(HWND anchor) {
//             RECT rc; ::GetWindowRect(anchor, &rc);
//             m_pop.SetContent(BuildList());
//             m_pop.SetSize(220, 240);
//             m_pop.SetEdge(balloonwjui::DuiPopupHost::EdgeBelow);
//             m_pop.SetDismissCallback(&OnDismiss, this);
//             m_pop.Show(rc, m_hWnd);
//         }
//     };
//
// XML 用法：<u>暂未原生支持</u>。Popup 是事件触发的瞬时弹层，不在静态
// 布局范畴；内容部分（Show 之前 SetContent 的子树）<u>可以</u>用 XML
// 描述（把 FromString 拿到的 root 给 SetContent），但 popup 自身（边缘
// 偏好 / 大小 / dismiss 行为）业务自管。
//
// 事件：通过 SetDismissCallback 回调（Reason 标识为何关闭）。<u>不</u>
// 通过 WM_DUI_NOTIFY。
class BUI_API DuiPopupHost : public DuiHost
{
public:
    DECLARE_WND_CLASS_EX(_T("__DuiPopupHost__"), CS_DROPSHADOW | CS_SAVEBITS, COLOR_WINDOW)

    // 优先放置在 anchor 哪一边（实际越界时自动翻边到对面）。
    enum Edge
    {
        EdgeBelow = 0,    // popup 顶 = anchor.bottom（默认）
        EdgeAbove = 1,    // popup 底 = anchor.top
        EdgeRight = 2,    // popup 左 = anchor.right
        EdgeLeft  = 3,    // popup 右 = anchor.left
    };

    // dismiss 原因。
    enum Reason
    {
        ReasonProgrammatic = 0,   // 显式 Hide() 调用
        ReasonOutsideClick = 1,   // 预留；多数平台走 lost-focus
        ReasonEscape       = 2,   // ESC 键
        ReasonLostFocus    = 3,   // 窗口失活
    };

    typedef void (*DismissCallback)(void* userdata, Reason r);

    DuiPopupHost();
    ~DuiPopupHost();

    // 设置 / 替换 popup 内容（DUI 子树）。可在 Show 前或显示中调；
    // 转给 DuiHost::SetRoot。
    void   SetContent(std::unique_ptr<DuiControl> root);

    // 设置 popup 外部尺寸（px）。默认 200×200。
    //   w / h：> 0；<= 0 会被忽略。
    void   SetSize(int w, int h);
    int    GetWidth () const { return m_width;  }
    int    GetHeight() const { return m_height; }

    // 设置 / 读取边缘偏好。
    void   SetEdge(Edge e);
    Edge   GetEdge() const   { return m_edge;   }

    // 注册 dismiss 回调；nullptr 取消。每次 Hide 都会调一次。
    void   SetDismissCallback(DismissCallback cb, void* userdata);

    // 显示 popup。
    //   anchorScreen：屏幕坐标的"锚"矩形（典型 = host 控件的 window rect）。
    //   hOwner：owner HWND。让 popup 不出现在任务栏、随父 app 销毁；
    //           nullptr 表示无 owner（顶层 floating window）。
    void   Show(const RECT& anchorScreen, HWND hOwner = nullptr);

    // 关闭 popup。HWND + 内容保活，可再 Show。
    void   Hide(Reason r = ReasonProgrammatic);
    bool   IsShowing() const;

    // 静态纯 helper：给定 anchor / 想要的尺寸 / 边缘偏好 / 工作区，
    // 算出 popup 应放的屏幕矩形。越界自动翻边、最后 clamp。无副作用，
    // 测试可以脱离 HWND 调。
    static RECT ComputePositionScreen(const RECT& anchor,
                                      int popupW, int popupH,
                                      Edge prefer,
                                      const RECT& workArea);

    BEGIN_MSG_MAP(DuiPopupHost)
        MSG_WM_KEYDOWN(OnPopupKeyDown)
        MSG_WM_ACTIVATE(OnActivate)
        CHAIN_MSG_MAP(DuiHost)
    END_MSG_MAP()

protected:
    void  OnPopupKeyDown(TCHAR vk, UINT nRepCnt, UINT nFlags);
    void  OnActivate    (UINT nState, BOOL bMin, CWindow other);

private:
    void  EnsureCreated(HWND hOwner);
    void  FireDismiss(Reason r);

private:
    int             m_width      = 200;
    int             m_height     = 200;
    Edge            m_edge       = EdgeBelow;
    DismissCallback m_dismissCb  = nullptr;
    void*           m_dismissUd  = nullptr;
    bool            m_inDismiss  = false;     // 重入保护
};

} // namespace balloonwjui

#endif // BUI_FEATURE_POPUPHOST
