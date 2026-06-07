#include "stdafx.h"
#include "DuiScrollBar.h"

#if BUI_FEATURE_SCROLLBAR

#include "../../DuiHost.h"
#include "../../DuiAnimation.h"
#include "../../HwndHostControl.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

// =====================================================================
// DuiScrollBar
// =====================================================================

namespace {
    // Minimum thumb size (px) along the main axis. 18 keeps the thumb
    // grabbable even on very long content (otherwise the proportional sizing
    // formula can produce a 1-px thumb that's impossible to click).
    const int MIN_THUMB_PX = 18;

    // ---- Track / chrome colors ----------------------------------------------
    // Track background fill - light gray, common Windows scrollbar look.
    const COLORREF kTrackFill        = RGB(230, 230, 230);
    // Track border - one step darker than the fill for a subtle 1px frame.
    const COLORREF kTrackBorder      = RGB(190, 190, 190);

    // ---- Thumb colors (3 interaction states) --------------------------------
    // Thumb fill while user is actively dragging - deepest blue, "active grip".
    const COLORREF kThumbFillDragging = RGB(110, 150, 210);
    // Thumb fill while hovered - one step lighter than dragging.
    const COLORREF kThumbFillHover    = RGB(150, 180, 220);
    // Thumb fill at rest - lightest blue, visible but unobtrusive.
    const COLORREF kThumbFillIdle     = RGB(170, 190, 215);
    // Thumb border - mid-blue across all states; the fill carries the state.
    const COLORREF kThumbBorder       = RGB(120, 140, 170);

    // ---- Scroll behavior ----------------------------------------------------
    // Mouse-wheel scroll multiplier. Each wheel notch advances 3 line-sizes
    // by Windows convention; matches the standard EM_SETSCROLLPOS step.
    const int kWheelLinesPerNotch = 3;

    // ---- DuiScrollView constants --------------------------------------------
    // Line size (px) used by DuiScrollView for keyboard/wheel scrolling.
    // 16 matches the project default UI font height (9pt YaHei = ~16px).
    const int kScrollViewLineSizePx = 16;
    // Background fill behind the content area - near-white off-white so any
    // empty area below content (when content height < view) doesn't look
    // like a hard edge against the dialog chrome.
    const COLORREF kScrollViewBg      = RGB(252, 252, 252);
}

DuiScrollBar::DuiScrollBar(bool horizontal)
    : m_horizontal(horizontal)
{
    m_fadeToken = std::make_shared<FadeToken>();
    m_fadeToken->owner = this;
    m_fadeToken->alive = true;
}

DuiScrollBar::~DuiScrollBar()
{
    // DuiAnimMgr 仍可能持有 fade/idle anim 的闭包，闭包内部持 token
    // shared_ptr 副本。把 alive 翻 false 让回调下次 fire 走 no-op，
    // 不再访问已死的 this。
    if (m_fadeToken)
    {
        m_fadeToken->alive = false;
        m_fadeToken->owner = nullptr;
    }
}

void DuiScrollBar::SetHorizontal(bool h)
{
    if (m_horizontal == h)
    {
        return;
    }
    m_horizontal = h;
    Invalidate();
}

void DuiScrollBar::SetRange(int nMin, int nMax)
{
    if (nMax < nMin)
    {
        nMax = nMin;
    }
    m_min = nMin;
    m_max = nMax;
    m_pos = ClampPos(m_pos);
    Invalidate();
}

void DuiScrollBar::SetPage(int page)
{
    if (page < 1)
    {
        page = 1;
    }
    m_page = page;
    Invalidate();
}

void DuiScrollBar::SetPos(int pos, bool notify)
{
    int newPos = ClampPos(pos);
    if (newPos == m_pos)
    {
        return;
    }
    m_pos = newPos;
    Invalidate();
    if (notify)
    {
        Notify();
    }
}

int DuiScrollBar::ClampPos(int p) const
{
    if (p < m_min)
    {
        return m_min;
    }
    if (p > m_max)
    {
        return m_max;
    }
    return p;
}

void DuiScrollBar::Notify()
{
    if (m_fn)
    {
        m_fn(m_user, m_pos);
    }
    NotifyParent(DUIN_VALUECHANGED, (LPARAM)m_pos);
}

int DuiScrollBar::TrackPixels() const
{
    return m_horizontal ? (m_rcItem.right - m_rcItem.left)
                        : (m_rcItem.bottom - m_rcItem.top);
}

int DuiScrollBar::ThumbPixels() const
{
    int track = TrackPixels();
    if (track <= MIN_THUMB_PX)
    {
        return track;
    }
    int range = m_max - m_min;
    int total = range + m_page;
    if (total <= 0)
    {
        return track;
    }
    int t = (track * m_page) / total;
    if (t < MIN_THUMB_PX)
    {
        t = MIN_THUMB_PX;
    }
    if (t > track)
    {
        t = track;
    }
    return t;
}

int DuiScrollBar::PixelsPerUnit() const
{
    int range = m_max - m_min;
    if (range <= 0)
    {
        return -1;
    }
    int track = TrackPixels() - ThumbPixels();
    if (track <= 0)
    {
        return -1;
    }
    return track;       // caller does (track * (pos-min)) / range
}

int DuiScrollBar::PixelOriginAlongMain() const
{
    return m_horizontal ? m_rcItem.left : m_rcItem.top;
}

RECT DuiScrollBar::ComputeThumbRect() const
{
    int track = TrackPixels();
    int thumb = ThumbPixels();
    int range = m_max - m_min;
    int origin = PixelOriginAlongMain();
    int offset = 0;
    if (range > 0)
    {
        int trackUsable = track - thumb;
        if (trackUsable > 0)
        {
            offset = (trackUsable * (m_pos - m_min)) / range;
        }
    }
    int start = origin + offset;
    if (m_horizontal)
    {
        return RECT{ start, m_rcItem.top, start + thumb, m_rcItem.bottom };
    }
    else
    {
        return RECT{ m_rcItem.left, start, m_rcItem.right, start + thumb };
    }
}

void DuiScrollBar::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }
    // m_alpha = 0 时整体不可见（auto-hide 隐藏态）—— 直接跳过 paint，
    // 不画 track / 不画 thumb。
    if (m_alpha <= 0.0f)
    {
        return;
    }

    // 走 GDI+ 路径以支持 alpha 通道。把 4 个 RGB 颜色映射成 GDI+ Color
    // (alpha * 255, r, g, b)，整体颜色随 fade 一起变浅 / 变透明。
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    BYTE a = (BYTE)(m_alpha * 255.0f);
    auto toGp = [a](COLORREF c) {
        return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
    };

    Gdiplus::RectF rcGp((Gdiplus::REAL)m_rcItem.left,
                        (Gdiplus::REAL)m_rcItem.top,
                        (Gdiplus::REAL)(m_rcItem.right - m_rcItem.left),
                        (Gdiplus::REAL)(m_rcItem.bottom - m_rcItem.top));

    // Track
    Gdiplus::SolidBrush brTrack(toGp(kTrackFill));
    g.FillRectangle(&brTrack, rcGp);
    Gdiplus::Pen penTrack(toGp(kTrackBorder), 1.0f);
    g.DrawRectangle(&penTrack, rcGp.X, rcGp.Y, rcGp.Width - 1, rcGp.Height - 1);

    // Thumb
    if (m_max <= m_min) { return; }
    RECT rcThumb = ComputeThumbRect();
    COLORREF clr = m_dragging ? kThumbFillDragging
                              : (m_bHover ? kThumbFillHover : kThumbFillIdle);
    Gdiplus::RectF rcThumbGp((Gdiplus::REAL)rcThumb.left,
                             (Gdiplus::REAL)rcThumb.top,
                             (Gdiplus::REAL)(rcThumb.right - rcThumb.left),
                             (Gdiplus::REAL)(rcThumb.bottom - rcThumb.top));
    Gdiplus::SolidBrush brThumb(toGp(clr));
    g.FillRectangle(&brThumb, rcThumbGp);
    Gdiplus::Pen penThumb(toGp(kThumbBorder), 1.0f);
    g.DrawRectangle(&penThumb,
                    rcThumbGp.X, rcThumbGp.Y,
                    rcThumbGp.Width - 1, rcThumbGp.Height - 1);
}

// =====================================================================
// auto-hide / fade alpha 实现
// =====================================================================

void DuiScrollBar::SetAlpha(float a)
{
    if (a < 0.0f) { a = 0.0f; }
    if (a > 1.0f) { a = 1.0f; }
    if (m_alpha == a) { return; }
    m_alpha = a;
    Invalidate();
}

void DuiScrollBar::SetAutoHide(bool b)
{
    if (m_autoHide == b) { return; }
    m_autoHide = b;
    if (b)
    {
        // 进入 auto-hide：默认隐藏（alpha=0），等 caller 调 TriggerShow。
        CancelFadeAnims_();
        SetAlpha(0.0f);
    }
    else
    {
        // 退出 auto-hide：恢复完全可见。
        CancelFadeAnims_();
        SetAlpha(1.0f);
    }
}

void DuiScrollBar::TriggerShow()
{
    if (!m_autoHide) { return; }
    // 取消任何 in-flight 的 fade-out / idle-timer，重置 token。
    CancelFadeAnims_();
    StartFadeAnim_(1.0f, 200);
    StartIdleTimer_(800);
}

void DuiScrollBar::StartFadeOut()
{
    if (!m_autoHide) { return; }
    CancelFadeAnims_();
    StartFadeAnim_(0.0f, 300);
}

void DuiScrollBar::CancelFadeAnims_()
{
    // 经典模式：把老 token 标 dead，老 anim 闭包下次 fire 走 no-op；
    // 新换一个活 token 给后续 anim 用。DuiAnimMgr 没暴露 cancel-by-token
    // 的 API（同 DuiSwitch::CancelAnim 处理）。
    if (m_fadeToken)
    {
        m_fadeToken->alive = false;
    }
    m_fadeToken = std::make_shared<FadeToken>();
    m_fadeToken->owner = this;
    m_fadeToken->alive = true;
}

void DuiScrollBar::StartFadeAnim_(float to, int durMs)
{
    auto token = m_fadeToken;
    float from = m_alpha;
    if (from == to) { return; }
    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        durMs, (double)from, (double)to,
        [token](double v)
        {
            if (token->alive && token->owner)
            {
                token->owner->m_alpha = (float)v;
                token->owner->Invalidate();
            }
        }));
    a->SetEasing(&DuiEase::EaseOutCubic);
    a->SetOnComplete([token, to]()
    {
        if (token->alive && token->owner)
        {
            token->owner->m_alpha = to;
            token->owner->Invalidate();
        }
    });
    DuiAnimMgr::Inst().Add(std::move(a));
}

void DuiScrollBar::StartIdleTimer_(int delayMs)
{
    // 用一段"空动画"作 idle delay：duration = delayMs, from/to 任意（这里
    // 0→1）setter no-op，OnComplete 才发 StartFadeOut。这样 idle anim 跟
    // fade anim 共用 mgr，且共用 m_fadeToken（CancelFadeAnims_ 一次性取消两条）。
    auto token = m_fadeToken;
    auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
        delayMs, 0.0, 1.0,
        [](double) { /* no-op：仅占用时间线 */ }));
    a->SetOnComplete([token]()
    {
        if (token->alive && token->owner)
        {
            token->owner->StartFadeOut();
        }
    });
    DuiAnimMgr::Inst().Add(std::move(a));
}

bool DuiScrollBar::OnLButtonDown(POINT pt, UINT /*mkFlags*/)
{
    if (m_max <= m_min)
    {
        return true;
    }
    RECT rcThumb = ComputeThumbRect();
    if (::PtInRect(&rcThumb, pt))
    {
        m_dragging = true;
        m_dragOffsetPx = m_horizontal ? (pt.x - rcThumb.left) : (pt.y - rcThumb.top);
        m_dragStartPos = m_pos;
        Capture();
        Invalidate();
        return true;
    }
    // Click on track outside thumb -> page up / page down based on side.
    int main = m_horizontal ? pt.x : pt.y;
    int thumbOrigin = m_horizontal ? rcThumb.left : rcThumb.top;
    if (main < thumbOrigin)
    {
        PageUp();
    }
    else
    {
        PageDown();
    }
    return true;
}

bool DuiScrollBar::OnLButtonUp(POINT /*pt*/, UINT /*mkFlags*/)
{
    if (m_dragging)
    {
        m_dragging = false;
        ReleaseCapture();
        Invalidate();
    }
    return true;
}

bool DuiScrollBar::OnMouseMove(POINT pt, UINT /*mkFlags*/)
{
    // 鼠标在 scrollbar 内移动 → keep showing（防 idle timer 隐藏 active 操作时的条）
    TriggerShow();
    if (!m_dragging)
    {
        return false;
    }
    int track = TrackPixels();
    int thumb = ThumbPixels();
    int trackUsable = track - thumb;
    if (trackUsable <= 0)
    {
        return true;
    }
    int main = m_horizontal ? pt.x : pt.y;
    int origin = PixelOriginAlongMain();
    int newThumbStart = main - origin - m_dragOffsetPx;
    if (newThumbStart < 0)
    {
        newThumbStart = 0;
    }
    if (newThumbStart > trackUsable)
    {
        newThumbStart = trackUsable;
    }
    int range = m_max - m_min;
    int newPos = m_min + (range * newThumbStart) / trackUsable;
    SetPos(newPos);
    return true;
}

bool DuiScrollBar::OnMouseWheel(POINT /*pt*/, short zDelta, UINT /*mkFlags*/)
{
    TriggerShow();
    int delta = (zDelta > 0) ? -m_lineSize * kWheelLinesPerNotch : m_lineSize * kWheelLinesPerNotch;
    SetPos(m_pos + delta);
    return true;
}

// =====================================================================
// DuiScrollView
// =====================================================================

DuiScrollView::DuiScrollView()
{
    auto sb = std::unique_ptr<DuiScrollBar>(new DuiScrollBar(/*horizontal=*/false));
    m_sb = sb.get();
    m_sb->SetOnScroll(&DuiScrollView::OnSbScrolled, this);
    DuiControl::AddChild(std::move(sb));
}

void DuiScrollView::SetContent(std::unique_ptr<DuiControl> content)
{
    // Suppress OnCommand routing for the duration of the swap. The OLD
    // content tree may contain HWND-hosted children (DuiEditHost etc.).
    // Their destruction inside RemoveChild's ~vector cascade fires
    // synchronous EN_KILLFOCUS / EN_SETFOCUS to the host as focus migrates
    // among siblings; without suppression, DuiHost::OnCommand walks
    // m_root which is mid-teardown and may dereference already-freed
    // siblings.
    DuiHost* host = m_pHost;
    if (host)
    {
        host->BeginTreeChange();
    }

    if (m_content)
    {
        RemoveChild(m_content);
        m_content = nullptr;
    }
    m_content = content.get();
    if (m_content)
    {
        DuiControl::AddChild(std::move(content));
        // Z-order: scrollbar was added first so it draws first; content
        // last so it paints on top. Their rects don't overlap, so paint
        // order doesn't actually matter visually.
    }
    // 若开启了 auto-height,这里必须按新 content 重算 m_contentH —— 否则
    // 它会停留在上一个 content 算出来的值,导致新 content 比旧的高时
    // ScrollView 滚不到底(SetAutoContentHeight 自身有 m_autoHeight==b 的
    // short-circuit,二次调用不会触发 Layout,所以单靠 SetAutoContentHeight
    // 不能补救)。
    if (m_autoHeight && m_content)
    {
        SIZE want = m_content->GetDesiredSize();
        if (want.cy > 0)
        {
            m_contentH = want.cy;
        }
    }
    DoLayout();
    UpdateRange();

    if (host)
    {
        host->EndTreeChange();
    }
}

void DuiScrollView::SetContentHeight(int h)
{
    if (h < 0)
    {
        h = 0;
    }
    m_contentH = h;
    UpdateRange();
    ApplyScrollToContent();
}

void DuiScrollView::SetScrollBarWidth(int w)
{
    if (w < 0)
    {
        w = 0;
    }
    if (w == m_sbWidth)
    {
        return;
    }
    m_sbWidth = w;
    DoLayout();
}

int DuiScrollView::GetScrollPos() const
{
    return m_sb ? m_sb->GetPos() : 0;
}

void DuiScrollView::SetScrollPos(int p)
{
    if (m_sb)
    {
        m_sb->SetPos(p);
    }
}

void DuiScrollView::SetAutoContentHeight(bool b)
{
    if (m_autoHeight == b)
    {
        return;
    }
    m_autoHeight = b;
    Layout(m_rcItem);
}

void DuiScrollView::Layout(const RECT& rcAvail)
{
    m_rcItem = rcAvail;
    // Auto-height: ask the content for its desired height before laying
    // out — saves callers from having to track changing content size.
    if (m_autoHeight && m_content)
    {
        SIZE want = m_content->GetDesiredSize();
        if (want.cy > 0)
        {
            m_contentH = want.cy;
        }
    }
    DoLayout();
    UpdateRange();
    ApplyScrollToContent();
}

void DuiScrollView::DoLayout()
{
    int viewW = m_rcItem.right - m_rcItem.left;
    int viewH = m_rcItem.bottom - m_rcItem.top;
    if (viewW <= 0 || viewH <= 0)
    {
        return;
    }

    bool sbVisible = m_sbWidth > 0 && m_contentH > viewH;
    int contentW = sbVisible ? (viewW - m_sbWidth) : viewW;
    if (contentW < 0)
    {
        contentW = 0;
    }

    if (m_content)
    {
        m_contentNominalRect = RECT{ m_rcItem.left,
                                     m_rcItem.top,
                                     m_rcItem.left + contentW,
                                     m_rcItem.top  + (m_contentH > 0 ? m_contentH : viewH) };
        m_content->SetRect(m_contentNominalRect);
    }

    if (m_sb)
    {
        m_sb->SetVisible(sbVisible);
        if (sbVisible)
        {
            RECT rcSb = { m_rcItem.left + contentW,
                          m_rcItem.top,
                          m_rcItem.right,
                          m_rcItem.bottom };
            m_sb->SetRect(rcSb);
        }
    }
}

void DuiScrollView::UpdateRange()
{
    if (!m_sb)
    {
        return;
    }
    int viewH = m_rcItem.bottom - m_rcItem.top;
    if (viewH <= 0)
    {
        m_sb->SetRange(0, 0);
        m_sb->SetPage(1);
        return;
    }
    int over = m_contentH - viewH;
    if (over < 0)
    {
        over = 0;
    }
    m_sb->SetRange(0, over);
    m_sb->SetPage(viewH);
    m_sb->SetLineSize(kScrollViewLineSizePx);
}

// Walk a DUI subtree and clip any HwndHostControl descendant's real Win32
// HWND to the intersection of its DUI rect with `viewRect` (also DUI / host-
// client coordinates). Without this, an EDIT/RICHEDIT scrolled partially out
// of the ScrollView still paints at its full window rect; the slice that
// pokes outside the viewport overlaps whatever sibling lives there (e.g. a
// tab strip above the ScrollView), and WS_CLIPCHILDREN on the host blocks
// the host's back-buffer from cleaning up — net effect is the EDIT's white
// background overdraws the sibling's pixels.
//
// Mechanism is SetWindowRgn (not SetWindowPos), so the EDIT keeps its full
// layout dimensions: text doesn't reflow on every scroll pixel, selections
// survive, the EDIT's internal scrollbar position stays put. We just clip
// what part of the HWND the OS lets through to the screen.
//
// The three branches map to the three observable cases the test suite locks
// in (DuiTier3Tests::Test_HwndHostInSV_*):
//   1. Empty intersection → fully out of viewport → empty region (HWND
//      effectively invisible without a hide/show flicker).
//   2. Intersection equals the window rect → fully in viewport → clear
//      region (NULL) so OS uses default whole-HWND clip; cheaper than a
//      redundant explicit-rect region, and required so re-entering the
//      viewport after a scroll-out doesn't leave a stale clipped region.
//   3. Partial → window-region with the visible slice in HWND-local coords
//      (origin at the HWND's top-left, so subtract the window's left/top).
//
// 用寄宿窗口的【真实】窗口矩形（映射到 host 客户区坐标），而非控件 DUI 矩形
// h->GetRect()：两者通常近似（窗口仅在 DUI 矩形内内缩 1px 边框 + margin），
// 但 DuiEditHost 开了垂直居中（单行默认）后窗口会收成一行高并在框内下移，
// 此时只有按真实窗口矩形算 SetWindowRgn 才不会与下移后的窗口错位。
//
// SetWindowRgn takes ownership of the HRGN on success, hence we don't
// DeleteObject in the partial / empty branches.
//
// HwndHostControl exposes no DUI children below itself (HitTest stops there;
// OnPaint is a no-op), so we don't recurse into its m_children.
static void ClipHostedHwndsToView_(DuiControl* node, const RECT& viewRect)
{
    if (!node)
    {
        return;
    }
    HwndHostControl* h = dynamic_cast<HwndHostControl*>(node);
    if (h)
    {
        HWND hw = h->GetHostedHwnd();
        if (hw && ::IsWindow(hw))
        {
            // 真实窗口矩形 → host 客户区坐标（与 viewRect 同一坐标系）。
            RECT wrScreen;
            ::GetWindowRect(hw, &wrScreen);
            HWND parent = ::GetParent(hw);
            POINT lt = { wrScreen.left,  wrScreen.top };
            POINT rb = { wrScreen.right, wrScreen.bottom };
            ::ScreenToClient(parent, &lt);
            ::ScreenToClient(parent, &rb);
            RECT win = { lt.x, lt.y, rb.x, rb.y };
            RECT inter;
            if (!::IntersectRect(&inter, &win, &viewRect))
            {
                HRGN empty = ::CreateRectRgn(0, 0, 0, 0);
                ::SetWindowRgn(hw, empty, TRUE);
            }
            else if (::EqualRect(&inter, &win))
            {
                ::SetWindowRgn(hw, NULL, TRUE);
            }
            else
            {
                RECT local = { inter.left   - win.left,
                               inter.top    - win.top,
                               inter.right  - win.left,
                               inter.bottom - win.top };
                HRGN rgn = ::CreateRectRgnIndirect(&local);
                ::SetWindowRgn(hw, rgn, TRUE);
            }
        }
        return;
    }
    for (auto& c : node->Children())
    {
        ClipHostedHwndsToView_(c.get(), viewRect);
    }
}

void DuiScrollView::ApplyScrollToContent()
{
    if (!m_content)
    {
        return;
    }
    int pos = m_sb ? m_sb->GetPos() : 0;
    RECT r = m_contentNominalRect;
    ::OffsetRect(&r, 0, -pos);
    m_content->SetRect(r);
    // After moving content (which cascaded SetWindowPos for any HwndHost
    // descendants via their Layout), update each Hwnd's window region so
    // the OS only paints the in-viewport slice. See ClipHostedHwndsToView_
    // above for why this is required.
    ClipHostedHwndsToView_(m_content, m_rcItem);
}

void DuiScrollView::OnSbScrolled(void* user, int /*newPos*/)
{
    DuiScrollView* self = static_cast<DuiScrollView*>(user);
    self->ApplyScrollToContent();
    self->Invalidate();
    // Force the queued WM_PAINT to dispatch synchronously before returning,
    // so the host's back buffer + BitBlt cover the EDIT/RICHEDIT's OLD
    // on-screen pixels in the same frame as the SetWindowPos that moved it.
    // Without this, WS_CLIPCHILDREN + suppressed WM_ERASEBKGND on the host
    // leave the old child-HWND area untouched until the next async paint
    // pass — visible as a "ghost" / drag trail of the previous EDIT
    // location during fast scrolls. UpdateWindow bypasses the message
    // queue and dispatches WM_PAINT directly to the WndProc, so by the
    // time we return the host's update region is empty.
    if (self->m_pHost && self->m_pHost->m_hWnd)
    {
        ::UpdateWindow(self->m_pHost->m_hWnd);
    }
}

void DuiScrollView::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    // Paint a light background so out-of-content area is visible.
    HBRUSH hbr = ::CreateSolidBrush(kScrollViewBg);
    ::FillRect(hdc, &m_rcItem, hbr);
    ::DeleteObject(hbr);

    // 把 content 的绘制裁到 view rect(不含滚动条那一列)。这里必须用
    // IntersectClipRect 而不是 SelectClipRgn —— SelectClipRgn 是「替换」
    // 语义,嵌套 ScrollView 时内层一旦 SelectClipRgn 自己的 rect, 外层
    // ScrollView 设的 clip 就被覆盖,内层内容能画到外层 m_rcItem 之外。
    // SaveDC + IntersectClipRect + RestoreDC 才是相交语义且能完整还原
    // dc 状态(clip、坐标变换、SelectObject 都一起存档)。
    int contentW = m_sb && m_sb->IsVisible() ? (m_rcItem.right - m_rcItem.left - m_sbWidth)
                                             : (m_rcItem.right - m_rcItem.left);
    RECT rcClip = { m_rcItem.left, m_rcItem.top,
                    m_rcItem.left + contentW, m_rcItem.bottom };

    int dcSaved = ::SaveDC(hdc);
    ::IntersectClipRect(hdc, rcClip.left, rcClip.top, rcClip.right, rcClip.bottom);

    if (m_content)
    {
        RECT inter;
        if (::IntersectRect(&inter, &m_content->GetRect(), &rcDirty))
        {
            m_content->OnPaint(hdc, inter);
        }
    }

    ::RestoreDC(hdc, dcSaved);

    // 滚动条本身不裁(它在 m_rcItem 内的右侧固定列,本来就在外层 clip 之内)。
    if (m_sb && m_sb->IsVisible())
    {
        RECT inter;
        if (::IntersectRect(&inter, &m_sb->GetRect(), &rcDirty))
        {
            m_sb->OnPaint(hdc, inter);
        }
    }
}

bool DuiScrollView::OnMouseWheel(POINT pt, short zDelta, UINT mkFlags)
{
    if (m_sb)
    {
        return m_sb->OnMouseWheel(pt, zDelta, mkFlags);
    }
    return false;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SCROLLBAR
