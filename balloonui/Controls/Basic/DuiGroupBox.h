#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_GROUPBOX

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"

namespace balloonwjui {

// =================================================================
// DuiGroupBox —— 带标题的圆角描边容器（无 HWND）
// =================================================================
//
// 用途：把一组相关字段视觉上圈起来 —— 设置页（"账号"、"网络代理"）、
// 个人资料卡（"基本信息"、"联系方式"）、确认对话框的内容区都是典型场景。
// 标题文字画在顶边上、轻微遮住边线，做出"挖出一段标题"的清爽外观。
//
// 工作机制：
//   · 自身 = 一个圆角矩形边框 + 顶部标题条；内容子控件被自动 layout 到
//     (border + 上方标题条 + 4 边 padding) 的内距矩形里。
//   · <u>只能放一个</u>内容子控件（通常是 VBox / Grid 装多行表单）。
//     SetContent 把所有权拿走、销毁旧内容（如果有）。
//   · 不接键盘焦点也不响应鼠标 hit-test —— 标题条上点击会穿透到内容子
//     控件去（GroupBox 自身永远不是 hover/focus 目标）。
//   · Layout 是纯算术（标题条减去 + padding 内距），ComputeContentRect
//     把这套数学暴露成静态 helper 给单测用。
//
// 代码用法：
//
//     auto box = std::unique_ptr<DuiGroupBox>(new DuiGroupBox());
//     box->SetTitle(_T("账号"));
//     box->SetPadding(12);
//     auto inner = std::unique_ptr<DuiVBox>(new DuiVBox());
//     inner->AddChild(std::move(usernameRow));
//     inner->AddChild(std::move(passwordRow));
//     box->SetContent(std::move(inner));
//     parent->AddChild(std::move(box));
//
// XML 用法（详细属性见 guides.html §3.3.3）：
//
//     <groupbox title="账号" padding="12,8,12,12">
//       <vbox gap="6">
//         <hbox gap="6" fixedHeight="28">
//           <label text="用户名" fixedWidth="64"/>
//           <edit  weight="1" placeholder="suwenjia"/>
//         </hbox>
//         <hbox gap="6" fixedHeight="28">
//           <label text="邮箱" fixedWidth="64"/>
//           <edit  weight="1" placeholder="user@example.com"/>
//         </hbox>
//       </vbox>
//     </groupbox>
//
// 事件：无（纯绘制 + 转发 hit-test 到内容；GroupBox 自身不发通知）。
//
// 替代关系：CSkinGroup（老的边框分组容器）。
class BUI_API DuiGroupBox : public DuiControl
{
public:
    DuiGroupBox();

    // 设置 / 读取顶部标题文字。
    //   title：任意文字；nullptr 视为空（标题条仍然占空间）。
    void    SetTitle(LPCTSTR title);
    CString GetTitle() const { return m_title; }

    // 设置 / 读取边框圆角矩形的颜色。默认 RGB(200,200,208)。
    void    SetBorderColor(COLORREF c);
    COLORREF GetBorderColor() const { return m_clrBorder; }

    // 设置 / 读取圆角半径（px）。默认 6。
    //   px：>= 0；< 0 会被 clamp 到 0。
    void    SetCornerRadius(int px);
    int     GetCornerRadius() const { return m_cornerR; }

    // 设置 / 读取标题文字色。默认 RGB(60,60,80)。
    void    SetTitleColor(COLORREF c);
    COLORREF GetTitleColor() const { return m_clrTitle; }

    // 设置内容区相对边框的 4 边 padding。
    //   l/t/r/b：>= 0 的整数。默认每边 12。
    void    SetPadding(int l, int t, int r, int b);

    // 4 边相同 padding 的便捷重载。
    //   all：>= 0 的整数。
    void    SetPadding(int all) { SetPadding(all, all, all, all); }

    // 设置 / 读取顶部标题条高度（包含标题文字本身的那段）。默认 24px。
    //   px：>= 0；< 0 会被 clamp 到 0。
    void    SetTitleStripHeight(int px);
    int     GetTitleStripHeight() const { return m_titleH; }

    // 安装 / 替换内容子控件。旧内容会被销毁。
    //   content：任意 DuiControl 子类；nullptr 表示清空。
    void    SetContent(std::unique_ptr<DuiControl> content);
    DuiControl* GetContent() const { return m_content; }

    // 静态 helper：给定 outer rect + 标题条高 + 4 边 padding，算出
    // 内容子控件的内距矩形。无副作用，便于单测。
    //   outer：GroupBox 自身的 m_rcItem。
    //   titleStripH / padL / padT / padR / padB：见上述 setter。
    //   返回：内容子控件可用的 RECT。
    static RECT ComputeContentRect(const RECT& outer,
                                   int titleStripH,
                                   int padL, int padT, int padR, int padB);

    // ---- DuiControl 覆写 ----
    void  Layout(const RECT& rcAvail) override;
    void  OnPaint(HDC hdc, const RECT& rcDirty) override;
    DuiControl* HitTest(POINT ptHostClient) override;

private:
    DuiControl* m_content   = nullptr;        // 裸指针；所有权在 m_children
    CString     m_title;
    COLORREF    m_clrBorder = RGB(200, 200, 208);
    COLORREF    m_clrTitle  = RGB( 60,  60,  80);
    int         m_cornerR   = 6;
    int         m_titleH    = 24;
    int         m_padL = 12, m_padT = 12, m_padR = 12, m_padB = 12;
};

} // namespace balloonwjui

#endif // BUI_FEATURE_GROUPBOX
