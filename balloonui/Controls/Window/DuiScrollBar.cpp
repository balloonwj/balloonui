#include "stdafx.h"
#include "DuiScrollBar.h"

#if BUI_FEATURE_SCROLLBAR

#include "../../DuiHost.h"
#include "../../DuiAnimation.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace balloonwjui {

// =====================================================================
// DuiScrollBar
// =====================================================================

namespace {
    // 滑块沿主轴的最短长度（像素）。按比例算出来的滑块在内容很长时可能只有一两个
    // 像素、根本抓不住，故设下限；取值见 DuiScrollBar::kMinThumbPx。
    const int MIN_THUMB_PX = DuiScrollBar::kMinThumbPx;

    // ---- 悬浮式细滑块的颜色（2026-10-04 起，与 Flamingo 聊天窗的滚动条一致）----
    // 滑块是半透明灰：灰度 0x5A，最大不透明度 120/255，再乘以淡入淡出的 alpha。
    // 不画轨道，也不随悬停 / 拖动换色。
    const BYTE kThumbGray     = 0x5A;
    const BYTE kThumbMaxAlpha = 120;
    // 浮点不透明度换算成整数分量时四舍五入用的半个单位
    const float kRoundHalf    = 0.5f;

    // ---- Scroll behavior ----------------------------------------------------
    // Mouse-wheel scroll multiplier. Each wheel notch advances 3 line-sizes
    // by Windows convention; matches the standard EM_SETSCROLLPOS step.
    const int kWheelLinesPerNotch = 3;

    // 滚一行所需的滚轮增量（zDelta 单位）：一整格 WHEEL_DELTA(120) 滚 kWheelLinesPerNotch
    // 行，即每 40 滚一行。精确式触摸板与高精度滚轮按这个量累积，凑满一行才滚一行。
    const int kWheelDeltaPerLine = WHEEL_DELTA / kWheelLinesPerNotch;

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

RECT DuiScrollBar::ComputePaintThumbRect() const
{
    RECT rc = ComputeThumbRect();
    if (m_horizontal)
    {
        // 水平滚动条：沿副轴（纵向）只有 kOverlayThumbPx 高，贴下缘往上 kOverlayThumbMarginPx
        rc.bottom = m_rcItem.bottom - kOverlayThumbMarginPx;
        rc.top = rc.bottom - kOverlayThumbPx;
        if (rc.top < m_rcItem.top)
        {
            rc.top = m_rcItem.top;
            rc.bottom = rc.top + kOverlayThumbPx;
        }
    }
    else
    {
        // 竖直滚动条：沿副轴（横向）只有 kOverlayThumbPx 宽，贴右缘往左 kOverlayThumbMarginPx
        rc.right = m_rcItem.right - kOverlayThumbMarginPx;
        rc.left = rc.right - kOverlayThumbPx;
        if (rc.left < m_rcItem.left)
        {
            rc.left = m_rcItem.left;
            rc.right = rc.left + kOverlayThumbPx;
        }
    }
    return rc;
}

void DuiScrollBar::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    if (!m_bVisible)
    {
        return;
    }
    // m_alpha = 0 时整体不可见（auto-hide 隐藏态），没有可滚范围时也没有滑块可画
    if (m_alpha <= 0.0f || m_max <= m_min)
    {
        return;
    }

    // 只画一根两端为半圆的细滑块，不画轨道：命中带其余部分透出底下的内容。
    // 走 GDI+ 以支持半透明与抗锯齿；不透明度 = 滑块最大不透明度 × 淡入淡出的 alpha。
    const RECT rc = ComputePaintThumbRect();
    const Gdiplus::REAL w = (Gdiplus::REAL)(rc.right - rc.left);
    const Gdiplus::REAL h = (Gdiplus::REAL)(rc.bottom - rc.top);
    if (w <= 0 || h <= 0)
    {
        return;
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    const BYTE a = (BYTE)((float)kThumbMaxAlpha * m_alpha + kRoundHalf);
    Gdiplus::SolidBrush brush(Gdiplus::Color(a, kThumbGray, kThumbGray, kThumbGray));

    // 圆角半径取粗细的一半，两端成半圆
    const Gdiplus::REAL d = (w < h) ? w : h;
    const Gdiplus::REAL x = (Gdiplus::REAL)rc.left;
    const Gdiplus::REAL y = (Gdiplus::REAL)rc.top;
    Gdiplus::GraphicsPath path;
    if (m_horizontal)
    {
        path.AddArc(x, y, d, d, 90.0f, 180.0f);
        path.AddArc(x + w - d, y, d, d, 270.0f, 180.0f);
    }
    else
    {
        path.AddArc(x, y, d, d, 180.0f, 180.0f);
        path.AddArc(x, y + h - d, d, d, 0.0f, 180.0f);
    }
    path.CloseFigure();
    g.FillPath(&brush, &path);
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
    StartFadeAnim_(1.0f, kFadeInMs);
    StartIdleTimer_(kIdleHideMs);
}

void DuiScrollBar::StartFadeOut()
{
    if (!m_autoHide) { return; }
    // 正在拖动时保持可见：拖动中鼠标离开容器（典型是拖出了窗口）不应让滑块消失，
    // 松开后 OnLButtonUp 会重新开始空闲计时
    if (m_dragging) { return; }
    CancelFadeAnims_();
    StartFadeAnim_(0.0f, kFadeOutMs);
}

bool DuiScrollBar::OnMouseEnter()
{
    DuiControl::OnMouseEnter();
    // 鼠标移进命中带：隐藏着的滚动条淡入，用户由此知道这里可以拖
    TriggerShow();
    return false;
}

bool DuiScrollBar::OnMouseLeave()
{
    DuiControl::OnMouseLeave();
    // 离开后重新计时，kIdleHideMs 后淡出；拖动中（capture 在本控件）不会走到这里
    TriggerShow();
    return false;
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
    a->SetOnComplete([token, delayMs]()
    {
        if (token->alive && token->owner)
        {
            // 鼠标还悬停在滚动条上、或正在拖动：保持可见，再等一轮；否则开始淡出
            if (token->owner->IsHover() || token->owner->IsDragging())
            {
                token->owner->StartIdleTimer_(delayMs);
            }
            else
            {
                token->owner->StartFadeOut();
            }
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
    // 按下即显现（滚动条可能还在隐藏态：鼠标刚移进命中带、淡入还没走完）
    TriggerShow();
    RECT rcThumb = ComputeThumbRect();
    if (::PtInRect(&rcThumb, pt))
    {
        // 按在滑块上：记住抓点相对滑块起点的偏移，拖动时滑块不跳
        m_dragOffsetPx = m_horizontal ? (pt.x - rcThumb.left) : (pt.y - rcThumb.top);
    }
    else
    {
        // 按在轨道空白处（2026-10-04 起与聊天窗的滚动条一致，此前是按整页翻）：滑块中心先跳到点击处，
        // 接着按住可以继续拖
        m_dragOffsetPx = ThumbPixels() / 2;
        DragThumbTo(m_horizontal ? pt.x : pt.y);
    }
    m_dragging = true;
    m_dragStartPos = m_pos;
    Capture();
    Invalidate();
    return true;
}

void DuiScrollBar::DragThumbTo(int mainAxisPx)
{
    int track = TrackPixels();
    int thumb = ThumbPixels();
    int trackUsable = track - thumb;
    if (trackUsable <= 0)
    {
        return;
    }
    int origin = PixelOriginAlongMain();
    int newThumbStart = mainAxisPx - origin - m_dragOffsetPx;
    if (newThumbStart < 0)
    {
        newThumbStart = 0;
    }
    if (newThumbStart > trackUsable)
    {
        newThumbStart = trackUsable;
    }
    int range = m_max - m_min;
    SetPos(m_min + (range * newThumbStart) / trackUsable);
}

bool DuiScrollBar::OnLButtonUp(POINT /*pt*/, UINT /*mkFlags*/)
{
    if (m_dragging)
    {
        m_dragging = false;
        ReleaseCapture();
        Invalidate();
        // 松开后重新开始空闲计时；鼠标仍在滚动条上时到期也不会淡出
        TriggerShow();
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
    DragThumbTo(m_horizontal ? pt.x : pt.y);
    return true;
}

bool DuiScrollBar::OnMouseWheel(POINT /*pt*/, short zDelta, UINT /*mkFlags*/)
{
    // 完全没有可滚范围（内容放得下，SetRange 收成了 [min, min]）时如实返回
    // false，让滚轮沿父链继续上冒（见 DuiHost::DispatchMouseWheel）。否则嵌套
    // 场景下——短列表 / 短内容的 DuiScrollView 放在外层滚动容器里——内层会把
    // 滚轮拦下，外层再也滚不动。
    //
    // 注意这与"有可滚范围但已经滚到顶 / 底"是两回事：后者仍要返回 true 消费掉，
    // 那条语义由 DuiHost::DispatchMouseWheel 的注释统一说明，不要一并改掉。
    // SetRange 保证 m_max >= m_min，故此处等价于 m_max == m_min。
    if (m_max <= m_min)
    {
        return false;
    }
    TriggerShow();

    // 滚动量随 zDelta 成比例：普通鼠标一格 120 滚 kWheelLinesPerNotch 行，与原先一致；
    // 精确式触摸板、高精度滚轮一次只发很小的 zDelta，原先每条消息都按整格滚，手势稍
    // 一动列表就飞出去很远。不足一行的余量累积到下一次；方向反转时丢弃上一方向的余量，
    // 免得反向的头一下被它抵消掉、手感发涩。
    if ((m_wheelRemainder > 0 && zDelta < 0) || (m_wheelRemainder < 0 && zDelta > 0))
    {
        m_wheelRemainder = 0;
    }
    m_wheelRemainder += zDelta;
    const int lines = m_wheelRemainder / kWheelDeltaPerLine;   // 向零取整，余量保留同号
    m_wheelRemainder -= lines * kWheelDeltaPerLine;
    if (lines != 0)
    {
        // zDelta 为正表示向上滚，对应滚动位置减小
        SetPos(m_pos - lines * m_lineSize);
    }
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
    // 更换内容的整个过程都标记为「控件树变更中」：旧内容子树在下面的
    // RemoveChild 里就地析构，期间宿主不得对控件树做任何遍历。与
    // DuiHost::SetRoot、DuiFrameWindow::SetClientContent 同一口径。
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

    // 悬浮式：内容始终按视口全宽排版，滚动条浮在右缘之上（2026-10-04 起；此前内容宽要扣掉
    // 滚动条那一列）
    bool sbVisible = m_sbWidth > 0 && m_contentH > viewH;
    int contentW = viewW;

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
            // 命中带贴右缘；视口比命中带还窄时占满视口
            int sbLeft = m_rcItem.right - m_sbWidth;
            if (sbLeft < m_rcItem.left)
            {
                sbLeft = m_rcItem.left;
            }
            RECT rcSb = { sbLeft, m_rcItem.top, m_rcItem.right, m_rcItem.bottom };
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
}

void DuiScrollView::OnSbScrolled(void* user, int /*newPos*/)
{
    DuiScrollView* self = static_cast<DuiScrollView*>(user);
    self->ApplyScrollToContent();
    self->Invalidate();
    // 把这一次重绘同步刷出，不等系统下一次投递 WM_PAINT。上面的 Invalidate
    // 只是把区域标记为待重绘，快速连续滚动时多次滚动会被合并到一次延后的
    // 重绘里，画面跟不上滚动条。UpdateWindow 绕过消息队列，直接把 WM_PAINT
    // 送进窗口过程，本函数返回时宿主的待重绘区域已经为空。
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

    // 把 content 的绘制裁到整个视口(悬浮式滚动条之下的内容照常画出)。这里必须用
    // IntersectClipRect 而不是 SelectClipRgn —— SelectClipRgn 是「替换」
    // 语义,嵌套 ScrollView 时内层一旦 SelectClipRgn 自己的 rect, 外层
    // ScrollView 设的 clip 就被覆盖,内层内容能画到外层 m_rcItem 之外。
    // SaveDC + IntersectClipRect + RestoreDC 才是相交语义且能完整还原
    // dc 状态(clip、坐标变换、SelectObject 都一起存档)。
    RECT rcClip = m_rcItem;

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

    // 滚动条最后画、盖在内容之上(悬浮式);它在 m_rcItem 内的右缘,本来就在外层 clip 之内。
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

DuiControl* DuiScrollView::HitTest(POINT pt)
{
    if (!m_bVisible || !::PtInRect(&m_rcItem, pt))
    {
        return nullptr;
    }
    // 滚动条浮在内容之上：出现时命中带归它（即使正处于自动隐藏的透明态，鼠标移进来
    // 才能把它唤出）。默认实现按子控件加入顺序倒着找，会先命中后加入的内容。
    if (m_sb && m_sb->IsVisible() && ::PtInRect(&m_sb->GetRect(), pt))
    {
        DuiControl* hit = m_sb->HitTest(pt);
        if (hit)
        {
            return hit;
        }
    }
    return DuiControl::HitTest(pt);
}


bool DuiScrollView::GetChildClipRect(RECT& outClip) const
{
    // 取整个视口矩形（含右侧滚动条那一列）而不是只取内容区：滚动条也是本控件的
    // 子控件，若只给内容区，滚动条自己的失效会被整个裁掉、再也不会重画。
    outClip = m_rcItem;
    return true;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_SCROLLBAR
