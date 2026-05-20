#pragma once

// =============================================================================
// ContentRouter —— 中部内容区的"换页"宿主
//
// 职责：根据当前 active nav id 显示对应 page 的子树。caller 通过
//       Switch(navId) 切换；本类负责构建 / 销毁子页面。
//
// 实现策略：每次 Switch 销毁旧 page 树 + 新建目标 page 树。简单粗暴 —
// 缺点：丢失 page 内部 scroll / 选中状态。M5+ 如需保留状态再改成"全部
// 子页常驻 + SetVisible 切换"。
//
// page 工厂查找：本文件内的 switch (navId) — 加新 page 时改这里 + #include
// 对应 Pages/XxxPage.h。
//
// XML：注册成 <content-router id="..."/> 自定义 tag。MainFrame 在
// LoadMainXml 末尾 FindCtrlById 取指针、调用 Switch 设初始页。
// =============================================================================

#include "Controls/Layout/DuiLayout.h"

namespace cloudmelody {

class ContentRouter : public balloonwjui::DuiVBox
{
public:
    ContentRouter() = default;

    // 切到 navId 对应的页面（NavId_Discover .. NavId_Profile / NowPlaying）。
    // 重复调用同一 navId 不重建（短路）。未识别的 navId 走 placeholder。
    //   extra：navId 特定参数。NavId_NowPlaying 时表示 trackIdx；其它 nav
    //          忽略。给 default 值 0 让普通 sidebar 切页 caller 不用关心。
    void Switch(int navId, int extra = 0);

    int  CurrentNav() const { return m_currentNav; }

private:
    balloonwjui::DuiControl* m_currentPage = nullptr;
    int m_currentNav = -1;
};

} // namespace cloudmelody
