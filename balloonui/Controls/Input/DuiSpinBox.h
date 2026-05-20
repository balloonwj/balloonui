#pragma once

#include "../../BalloonUiFeatures.h"
#if BUI_FEATURE_SPINBOX

// .cpp 必须先 include stdafx.h（项目 PCH 约定）。
#include "../../DuiControl.h"
#include "DuiEditHost.h"

namespace balloonwjui {

// =================================================================
// DuiSpinBox —— 整数微调输入（HWND-hosted）
// =================================================================
//
// 用途：让用户输入一个有范围的整数（端口号、年龄、缩放百分比、……），
// 既可手输入也可点 ▲ ▼ 增减。
//
// 布局：
//   [ EDIT 区，weight=1 ]  [ ▲ ▼ 按钮条，宽 18px，▲ 上半 / ▼ 下半 ]
//
// 工作机制：
//   · 复合控件 = 一个 DuiEditHost（左侧真 EDIT）+ 自绘的 ▲▼ 按钮条
//     （右侧）。EDIT 是 HWND，必须在 host HWND 就绪后调
//     EnsureCreated(hostHwnd) 一次才会创建出来。
//   · SetValue 自动 clamp 到 [min, max]（或 SetWrap(true) 时回绕），
//     同时把新值文本推到 EDIT。
//   · 用户在 EDIT 里手输入后，<u>不会</u>自动同步到 m_value —— 由 caller
//     在 EDIT 的 EN_KILLFOCUS 时自己 SetValue(_ttoi(GetEdit()->GetText()))
//     提交。
//   · ▲ / ▼ 半区点击 = StepBy(±step) 并发 DUIN_VALUECHANGED。
//   · 自身不参与 tab；内部 EDIT 是 tab stop。hit-test 把 EDIT 区域转给
//     EDIT 子控件（让光标 / 选择 / IME 行为原生）。
//
// 代码用法：
//
//     auto spin = std::make_unique<DuiSpinBox>();
//     spin->SetRange(0, 999);
//     spin->SetStep(5);
//     spin->SetValue(100);
//     spin->SetCtrlId(IDC_PORT);
//     DuiSpinBox* raw = spin.get();
//     parent->AddChild(std::move(spin));
//     raw->EnsureCreated(host->m_hWnd);
//     // 父对话框 OnDuiNotify：
//     //   if (n.code == DUIN_VALUECHANGED && n.ctrlId == IDC_PORT)
//     //       OnPortChanged((int)n.extra);
//
// XML 用法（详细属性见 guides.html §3.3.14）：
//
//     <spinbox id="100"
//              min="0" max="999" value="100" step="5"
//              wrap="false"
//              fixedHeight="28"/>
//
//   注意：跟 <edit> / <searchbox> 一样，builder 不调 EnsureCreated，
//   caller 自己在 host HWND 就绪后调一次。
//
// 事件：
//   · DUIN_VALUECHANGED — ▲▼ 点击触发；extra = 新值。
//                          手输入提交需 caller 在 EDIT 的 KILLFOCUS 里
//                          自己调 SetValue。
class BUI_API DuiSpinBox : public DuiControl
{
public:
    DuiSpinBox();

    // 创建内部真 EDIT 子 HWND。caller 必须在 host HWND 就绪后调一次。
    //   hwndParent：host HWND。
    //   返回：true 表示创建成功。
    bool   EnsureCreated(HWND hwndParent);

    // 设置 / 读取取值范围。SetRange 之后 m_value 会被 clamp 进新区间。
    //   minV / maxV：必须 minV <= maxV。
    void   SetRange(int minV, int maxV);
    int    GetMinValue() const { return m_min; }
    int    GetMaxValue() const { return m_max; }

    // 设置 / 读取每次 ▲ ▼ 点击的步长。
    //   s：> 0；<= 0 会被 clamp 到 1。默认 1。
    void   SetStep(int s);
    int    GetStep() const     { return m_step; }

    // 设置 / 读取当前值。会 clamp（或 wrap），并把新值文本推到 EDIT。
    //   v：目标值。
    //   notify：true 时触发 DUIN_VALUECHANGED；false 抑制（用于初始化）。
    void   SetValue(int v, bool notify = true);
    int    GetValue() const    { return m_value; }

    // 越界处理：clamp（默认）vs wrap（回绕）。
    //   w：true = max+step → min；false = max+step → max。
    void   SetWrap(bool w)     { m_wrap = w; }
    bool   GetWrap() const     { return m_wrap; }

    // 设置 / 读取右侧 ▲ ▼ 按钮条宽度（px）。默认 18。
    //   px：>= 0；过小会让 ▲ ▼ 难点。
    void   SetSpinStripWidth(int px);
    int    GetSpinStripWidth() const { return m_spinW; }

    // 静态 helper：clamp 或 wrap v 到 [minV, maxV]。
    //   v：原始值。
    //   minV / maxV：边界。
    //   wrap：true = wrap，false = clamp。
    //   返回：处理后的值。
    static int ClampOrWrap(int v, int minV, int maxV, bool wrap);

    // ▲ / ▼ 半区在 m_rcItem 坐标系下的矩形。strip 宽 0 时返回空。
    RECT   GetUpRect()   const;
    RECT   GetDownRect() const;

    // 直接拿到内部 EDIT 子控件指针（所有权在 m_children 里）。
    DuiEditHost* GetEdit() const { return m_edit; }

    // ---- DuiControl 覆写 ----
    void   Layout(const RECT& rcAvail) override;
    void   OnPaint(HDC hdc, const RECT& rcDirty) override;
    bool   OnLButtonDown(POINT pt, UINT mkFlags) override;
    bool   OnLButtonUp  (POINT pt, UINT mkFlags) override;
    DuiControl* HitTest(POINT ptHostClient) override;

private:
    void   StepBy(int delta);
    void   PushValueToEdit();

    DuiEditHost* m_edit  = nullptr;
    int          m_min   = 0;
    int          m_max   = 100;
    int          m_step  = 1;
    int          m_value = 0;
    int          m_spinW = 18;
    bool         m_wrap  = false;
    int          m_pressZone = 0;     // 1=上半 2=下半
};

} // namespace balloonwjui

#endif // BUI_FEATURE_SPINBOX
