#pragma once

// =============================================================================
// DesktopLyricsWnd —— 桌面歌词浮窗
//
// 独立 ATL 顶层窗（WS_POPUP），不走 DuiHost。视觉特征：
//   · WS_EX_TOPMOST  ：始终在最上
//   · WS_EX_LAYERED  ：SetLayeredWindowAttributes 给整窗一个 ~90% alpha
//   · WS_EX_NOACTIVATE：点击不抢焦点（避免主窗 EDIT 失焦）
//   · WM_NCHITTEST 全 HTCAPTION：从任意位置都可拖动
//
// 视觉：800×90 横长条，半透明黑底，居中显示当前歌词大字（白 +
// 黑色 1px 描边以保证桌面任何底色下可读）。
//
// 生命周期：MainFrame 持有实例 + 析构。Toggle() 切显隐。MainFrame 的
// 播放 timer 调 SetCurrentLyric() 同步歌词文本。
// =============================================================================

#include <atlbase.h>
#include <atlwin.h>

namespace cloudmelody {

class DesktopLyricsWnd : public ATL::CWindowImpl<DesktopLyricsWnd>
{
public:
    DECLARE_WND_CLASS_EX(_T("CloudMelodyDesktopLyrics"), CS_DBLCLKS, -1)

    BEGIN_MSG_MAP(DesktopLyricsWnd)
        MESSAGE_HANDLER(WM_PAINT,        OnPaint_)
        MESSAGE_HANDLER(WM_ERASEBKGND,   OnEraseBkgnd_)
        MESSAGE_HANDLER(WM_NCHITTEST,    OnNcHitTest_)
    END_MSG_MAP()

    // 首次需要时建窗（避开提前加载，主窗启动更快）。
    void EnsureCreated();

    // 切显 / 隐。第一次调时若未建则建。
    void Toggle();

    bool IsShowingNow() const
    {
        return m_hWnd && ::IsWindowVisible(m_hWnd);
    }

    // 设当前要显示的歌词文本。空字符串等同于"当前没有歌词"。
    void SetCurrentLyric(LPCTSTR text);

private:
    LRESULT OnPaint_     (UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEraseBkgnd_(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnNcHitTest_ (UINT, WPARAM, LPARAM, BOOL&);

    // UpdateLayeredWindow 路径：构造一张 32bpp DIB + GDI+ 画文字 +
    // 兜底 alpha 通道，最后整张 push 给 OS。所有变化（显隐 / 文字变）
    // 都走这条路径，不依赖 WM_PAINT。
    void Repaint_();

    CString m_text;
};

} // namespace cloudmelody
