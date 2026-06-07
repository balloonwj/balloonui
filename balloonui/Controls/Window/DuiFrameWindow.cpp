#include "stdafx.h"
#include "DuiFrameWindow.h"

#if BUI_FEATURE_FRAMEWINDOW

#include "../../DuiResMgr.h"
#include "../../DuiPaintAA.h"
#if BUI_FEATURE_TOOLTIP
#  include "../Feedback/DuiToolTip.h"      // DuiToolTipMgr —— caption icon 的 tooltip
#endif

namespace balloonwjui {

namespace {

// Geometry & color constants for the caption-button strip (min/max/close).
//
// Why these values:
//   kCaptionBtnW    - 46px matches the Windows convention (Explorer,
//                     Edge, modern WeChat) for caption buttons. Wider
//                     than the previous 36 so 1px stroke glyphs aren't
//                     dwarfed by the surrounding strip.
//   kGlyphPx        - 10px sided glyph; centered in the 46xH cell. Big
//                     enough to read at 100% DPI, small enough that the
//                     red close-hover doesn't dominate the title bar.
//
// Two palettes — picked by CaptionButton::m_transparentCtx, which the
// title bar propagates via SetTransparent. Rationale: a transparent
// title bar sits on top of a colored background (e.g. DemoNinePatchBg
// brand-blue gradient); a light-gray hover block clashes with that bg
// (looks like a dirty smudge), and a saturated Windows red button next
// to a brand-blue gradient feels mismatched. Opaque title bars retain
// the classic Windows-flat look so non-themed dialogs (no bg image)
// look familiar.
//
//   Opaque (default) palette:
//     kHoverBgOpaque       - light gray hover for min/max. Win11 / Edge default.
//     kPressBgOpaque       - one shade darker for press feedback.
//     kCloseHoverBgOpaque  - Windows-standard close red (#E81123).
//     kClosePressBgOpaque  - same hue, darker — visible press on red.
//
//   Transparent (themed-bg) palette — sampled from docs/icons/btn_*_down.png
//   and docs/icons/btn_close_highlight.png:
//     kHoverBgBlue         - hover for min/max, lighter brand blue.
//     kPressBgBlue         - press for min/max, brand blue (#375FC3
//                            from btn_mini_down.png / btn_max_down.png).
//     kCloseHoverBgSoft    - close hover (#CC3333 from btn_close_highlight.png).
//                            Softer than Windows red, sits better next
//                            to brand-blue gradient.
//     kClosePressBgSoft    - same hue, darker.
//
//   kGlyphInk            - rest-state glyph (opaque mode default).
//   kGlyphInv            - white glyph used on any colored bg fill
//                          (close hover, both modes; min/max hover/press
//                          in transparent mode).
const int kCaptionBtnW = 46;
const int kGlyphPx     = 10;

const COLORREF kHoverBgOpaque      = RGB(232, 232, 232);
const COLORREF kPressBgOpaque      = RGB(212, 212, 212);
const COLORREF kCloseHoverBgOpaque = RGB(232,  17,  35);
const COLORREF kClosePressBgOpaque = RGB(180,  10,  25);

const COLORREF kHoverBgBlue        = RGB( 74, 114, 208);   // lighter than #375FC3
const COLORREF kPressBgBlue        = RGB( 55,  95, 195);   // #375FC3 — sampled from btn_*_down.png
const COLORREF kCloseHoverBgSoft   = RGB(204,  51,  51);   // #CC3333 — sampled from btn_close_highlight.png
const COLORREF kClosePressBgSoft   = RGB(166,  40,  40);   // ~10% darker than hover

const COLORREF kGlyphInk           = RGB( 60,  60,  60);
const COLORREF kGlyphInv           = RGB(255, 255, 255);

// Caption button: paint-only DuiControl that draws a flat min / max /
// close glyph and tracks hover + press for color feedback. Fires
// DUIN_CLICK on mouse-up so the existing DuiFrameWindow::OnDuiChildNotify
// dispatch (ctrlId 1/2/3 -> SC_MINIMIZE / SC_MAXIMIZE / SC_RESTORE /
// SC_CLOSE) keeps working unchanged.
class CaptionButton : public DuiControl
{
public:
    enum Kind
    {
        Min,
        Max,
        Close
    };

    explicit CaptionButton(Kind k)
        : m_kind(k)
    {
        SetTabStop(false);
    }

    // 让 caller 覆盖默认 glyph 色（深灰）。用例：标题条透明叠在彩色背景上，
    // 默认深 glyph 看不清 → 改白色。CLR_INVALID 表示用默认。
    void SetGlyphColorOverride(COLORREF c)
    {
        m_glyphOverride = c;
        Invalidate();
    }

    // 透明上下文标志 — 由 DuiFrameTitleBar::SetTransparent 透传下来。
    // true → hover/press 用品牌蓝/柔和红配色，配合彩色 bg 渐变；
    // false → 经典 Windows 浅灰 + 亮红，配合纯色标题栏。
    void SetTransparentContext(bool b)
    {
        if (m_transparentCtx == b)
        {
            return;
        }
        m_transparentCtx = b;
        Invalidate();
    }

    // Max 按钮专用：true → 画 restore（双层方块）glyph；
    // false → 画普通 max（单空心方块）glyph。其它 kind 此标志被忽略。
    // 由 DuiFrameWindow 在窗口收到 SIZE_MAXIMIZED / SIZE_RESTORED 时透传。
    void SetShowAsRestore(bool b)
    {
        if (m_showAsRestore == b)
        {
            return;
        }
        m_showAsRestore = b;
        Invalidate();
    }

    void OnPaint(HDC hdc, const RECT& /*rcDirty*/) override
    {
        if (!m_bVisible)
        {
            return;
        }

        // Background fill — 两套配色按 m_transparentCtx 选。
        COLORREF bg = CLR_INVALID;
        if (m_pressed && m_hover)
        {
            if (m_kind == Close)
            {
                bg = m_transparentCtx ? kClosePressBgSoft : kClosePressBgOpaque;
            }
            else
            {
                bg = m_transparentCtx ? kPressBgBlue : kPressBgOpaque;
            }
        }
        else if (m_hover)
        {
            if (m_kind == Close)
            {
                bg = m_transparentCtx ? kCloseHoverBgSoft : kCloseHoverBgOpaque;
            }
            else
            {
                bg = m_transparentCtx ? kHoverBgBlue : kHoverBgOpaque;
            }
        }
        if (bg != CLR_INVALID)
        {
            HBRUSH br = ::CreateSolidBrush(bg);
            ::FillRect(hdc, &m_rcItem, br);
            ::DeleteObject(br);
        }

        // Glyph color — 优先级：
        //  * 任何"被颜色 bg 填充"的状态都用白 glyph（对比度）：
        //      - close hover/press（红底）
        //      - 透明上下文下的 min/max hover/press（蓝底）
        //  * caller 通过 SetGlyphColorOverride 指定 → 用它
        //  * 否则默认深灰 ink
        bool onColoredBg = (bg != CLR_INVALID);
        COLORREF gc;
        if (onColoredBg)
        {
            gc = kGlyphInv;
        }
        else if (m_glyphOverride != CLR_INVALID)
        {
            gc = m_glyphOverride;
        }
        else
        {
            gc = kGlyphInk;
        }

        // Glyph rect: kGlyphPx square centered in the cell.
        int cx = (m_rcItem.left + m_rcItem.right) / 2;
        int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;
        int half = kGlyphPx / 2;
        RECT g = { cx - half, cy - half, cx + half, cy + half };

        switch (m_kind)
        {
        case Min:
            // Single horizontal line at vertical center.
            DuiAA::DrawLine(hdc, g.left, cy, g.right, cy, gc, 1.0f);
            break;
        case Max:
            if (m_showAsRestore)
            {
                // Restore glyph — two overlapping squares (front bottom-left,
                // back top-right). Matches docs/icons/btn_restore_down.png.
                //
                // Geometry inside kGlyphPx (10) cell:
                //   - Front square: 7×7, top-left at (cx-5, cy-3)
                //   - Back square : 7×7, top-left at (cx-3, cy-5)
                //   - Diagonal offset = 2px so the two squares overlap
                //     in a 5×5 region (visually reads as "windowed inside
                //     window" — Windows-conventional restore icon).
                //
                // We only stroke the back square's top + right edges (its
                // bottom + left would be hidden behind the front square,
                // and partially-drawn lines look noisy at 1px).
                int sq  = 7;
                int off = 2;
                int frontL = cx - sq / 2 - 1;            // -5
                int frontT = cy - sq / 2 + off;          // -3
                int frontR = frontL + sq;                // +2
                int frontB = frontT + sq;                // +4
                int backL  = frontL + off;               // -3
                int backT  = frontT - off - off;         // -5
                int backR  = backL + sq;                 // +4
                int backB  = backT + sq;                 // +2

                // Back square: top edge + right edge only.
                DuiAA::DrawLine(hdc, backL,  backT, backR, backT, gc, 1.0f);
                DuiAA::DrawLine(hdc, backR,  backT, backR, backB, gc, 1.0f);

                // Front square: full 4 edges.
                DuiAA::DrawLine(hdc, frontL, frontT, frontR, frontT, gc, 1.0f);
                DuiAA::DrawLine(hdc, frontR, frontT, frontR, frontB, gc, 1.0f);
                DuiAA::DrawLine(hdc, frontR, frontB, frontL, frontB, gc, 1.0f);
                DuiAA::DrawLine(hdc, frontL, frontB, frontL, frontT, gc, 1.0f);
            }
            else
            {
                // Hollow square (4 strokes). Use fill+inner-fill to avoid
                // GDI Rectangle's non-AA outline. Two outer / two inner
                // rect passes — light enough at 1px to read as a square.
                DuiAA::DrawLine(hdc, g.left,  g.top,    g.right, g.top,    gc, 1.0f);
                DuiAA::DrawLine(hdc, g.right, g.top,    g.right, g.bottom, gc, 1.0f);
                DuiAA::DrawLine(hdc, g.right, g.bottom, g.left,  g.bottom, gc, 1.0f);
                DuiAA::DrawLine(hdc, g.left,  g.bottom, g.left,  g.top,    gc, 1.0f);
            }
            break;
        case Close:
            // Two diagonals forming an X.
            DuiAA::DrawLine(hdc, g.left, g.top,    g.right, g.bottom, gc, 1.0f);
            DuiAA::DrawLine(hdc, g.left, g.bottom, g.right, g.top,    gc, 1.0f);
            break;
        }
    }

    bool OnMouseEnter() override
    {
        m_hover = true;
        Invalidate();
        return false;
    }
    bool OnMouseLeave() override
    {
        m_hover   = false;
        m_pressed = false;
        Invalidate();
        return false;
    }
    bool OnLButtonDown(POINT, UINT) override
    {
        m_pressed = true;
        Capture();
        Invalidate();
        return true;
    }
    bool OnLButtonUp(POINT pt, UINT) override
    {
        bool wasPressed = m_pressed;
        m_pressed = false;
        ReleaseCapture();
        Invalidate();
        if (wasPressed && ::PtInRect(&m_rcItem, pt))
        {
            NotifyParent(DUIN_CLICK);
        }
        return true;
    }
    bool OnSetCursor(POINT) override
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        return true;
    }

private:
    Kind     m_kind;
    bool     m_hover           = false;
    bool     m_pressed         = false;
    bool     m_transparentCtx  = false;
    bool     m_showAsRestore   = false;
    COLORREF m_glyphOverride   = CLR_INVALID;
};

// Caption icon button: paint-only DuiControl that draws a caller-supplied
// 16x16 HBITMAP centered in a standard caption button cell (kCaptionBtnW).
// hover / pressed feedback shares CaptionButton's min/max palette (no
// close-red variant). Click fires DUIFW_CAPTION_ICON_CLICK with
// extra = captionId (assigned by DuiFrameWindow::AddCaptionIcon, monotonic
// across the lifetime of the frame). Optional tooltip is registered with
// DuiToolTipMgr at construction and unregistered on destruction.
class CaptionIconButton : public DuiControl
{
public:
    explicit CaptionIconButton(int captionId, HBITMAP icon)
        : m_captionId(captionId)
        , m_icon(icon)
    {
        SetTabStop(false);
    }

    ~CaptionIconButton() override
    {
#if BUI_FEATURE_TOOLTIP
        if (m_tooltipRegistered)
        {
            DuiToolTipMgr::Inst().Unregister(this);
        }
#endif
    }

    int GetCaptionId() const { return m_captionId; }

    // 透明上下文标志 — 由 DuiFrameTitleBar::SetTransparent 透传。配色规则
    // 与 CaptionButton min/max 相同（不走 close 红）：透明态 → 品牌蓝
    // hover/press；不透明态 → 经典浅灰 hover/press。
    void SetTransparentContext(bool b)
    {
        if (m_transparentCtx == b)
        {
            return;
        }
        m_transparentCtx = b;
        Invalidate();
    }

    // 注册 tooltip。仅在 BUI_FEATURE_TOOLTIP 编译开关启用时生效；text 为
    // nullptr 或空字符串等价于不注册。重复调用先 Unregister 旧的再注册新的。
    void RegisterTooltip(LPCTSTR text)
    {
#if BUI_FEATURE_TOOLTIP
        if (m_tooltipRegistered)
        {
            DuiToolTipMgr::Inst().Unregister(this);
            m_tooltipRegistered = false;
        }
        if (text != nullptr && text[0] != _T('\0'))
        {
            DuiToolTipMgr::Inst().Register(this, text);
            m_tooltipRegistered = true;
        }
#else
        (void)text;
#endif
    }

    void OnPaint(HDC hdc, const RECT& /*rcDirty*/) override
    {
        if (!m_bVisible)
        {
            return;
        }

        //—— hover/press 背景（与 CaptionButton min/max 同款，无 close 红变体）
        COLORREF bg = CLR_INVALID;
        if (m_pressed && m_hover)
        {
            bg = m_transparentCtx ? kPressBgBlue : kPressBgOpaque;
        }
        else if (m_hover)
        {
            bg = m_transparentCtx ? kHoverBgBlue : kHoverBgOpaque;
        }
        if (bg != CLR_INVALID)
        {
            HBRUSH hbr = ::CreateSolidBrush(bg);
            ::FillRect(hdc, &m_rcItem, hbr);
            ::DeleteObject(hbr);
        }

        //—— icon 居中 16x16 绘
        if (m_icon)
        {
            const int kIcon = 16;
            int cx = (m_rcItem.left + m_rcItem.right) / 2;
            int cy = (m_rcItem.top + m_rcItem.bottom) / 2;
            BITMAP bm = {};
            ::GetObject(m_icon, sizeof(bm), &bm);
            if (bm.bmWidth > 0 && bm.bmHeight > 0)
            {
                HDC mem = ::CreateCompatibleDC(hdc);
                HGDIOBJ old = ::SelectObject(mem, m_icon);
                ::SetStretchBltMode(hdc, HALFTONE);
                ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                ::StretchBlt(hdc, cx - kIcon / 2, cy - kIcon / 2,
                             kIcon, kIcon,
                             mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                ::SelectObject(mem, old);
                ::DeleteDC(mem);
            }
        }
    }

    bool OnMouseEnter() override
    {
        m_hover = true;
        Invalidate();
        return false;
    }
    bool OnMouseLeave() override
    {
        m_hover   = false;
        m_pressed = false;
        Invalidate();
        return false;
    }
    bool OnLButtonDown(POINT, UINT) override
    {
        m_pressed = true;
        Capture();
        Invalidate();
        return true;
    }
    bool OnLButtonUp(POINT pt, UINT) override
    {
        bool was = m_pressed;
        m_pressed = false;
        ReleaseCapture();
        Invalidate();
        if (was && ::PtInRect(&m_rcItem, pt))
        {
            //—— 用 DUIFW_CAPTION_ICON_CLICK 直接上冒；不走 DUIN_CLICK 是
            //   为了不被 DuiFrameWindow::OnDuiChildNotify 的 case 1/2/3
            //   误识为 min/max/close（caption icon 的 ctrlId 虽然不是
            //   1/2/3，但 code 用专用类型更清晰、避免后续 ctrlId 冲突）
            NotifyParent((UINT)DuiFrameWindow::DUIFW_CAPTION_ICON_CLICK,
                         (LPARAM)m_captionId);
        }
        return true;
    }
    bool OnSetCursor(POINT) override
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        return true;
    }

private:
    int      m_captionId;
    HBITMAP  m_icon              = nullptr;
    bool     m_hover             = false;
    bool     m_pressed           = false;
    bool     m_transparentCtx    = false;
    bool     m_tooltipRegistered = false;
};

} // anonymous namespace

// =====================================================================
// DuiFrameTitleBar: paint-only title strip with optional left-edge icon,
// title text, and three DuiButton children (min/max/close). Each button
// fires its WM_DUI_NOTIFY DUIN_CLICK event up to the frame, which sends
// a WM_SYSCOMMAND to the OS.
//
// Button ctrlIds are well-known: 1 = min, 2 = max, 3 = close. The frame's
// OnDuiChildNotify intercepts those.
// =====================================================================

class DuiFrameTitleBar : public DuiControl
{
public:
    DuiFrameTitleBar()
    {
        SetTabStop(false);

        auto bMin = std::unique_ptr<CaptionButton>(new CaptionButton(CaptionButton::Min));
        bMin->SetCtrlId(1);
        m_btnMin = bMin.get();
        AddChild(std::move(bMin));

        auto bMax = std::unique_ptr<CaptionButton>(new CaptionButton(CaptionButton::Max));
        bMax->SetCtrlId(2);
        m_btnMax = bMax.get();
        AddChild(std::move(bMax));

        auto bClose = std::unique_ptr<CaptionButton>(new CaptionButton(CaptionButton::Close));
        bClose->SetCtrlId(3);
        m_btnClose = bClose.get();
        AddChild(std::move(bClose));
    }

    void SetTitle(LPCTSTR t)
    {
        m_title = t ? t : _T("");
        Invalidate();
    }
    void SetIcon(HBITMAP h)
    {
        m_icon = h;
        Invalidate();
    }
    void SetButtons(bool mn, bool mx, bool cl)
    {
        m_btnMin->SetVisible(mn);
        m_btnMax->SetVisible(mx);
        m_btnClose->SetVisible(cl);
        Layout(m_rcItem);
        Invalidate();
    }

    DuiControl* GetMinBtn  () const { return m_btnMin;   }
    DuiControl* GetMaxBtn  () const { return m_btnMax;   }
    DuiControl* GetCloseBtn() const { return m_btnClose; }

    // ---- caption icons（标题栏自定义可点击图标）----
    //
    // 视觉顺序（从右到左）：close → max → min → caption icons (按 Add 顺序排)。
    // 即：Add 的图标排在 min 按钮左侧，新加的更靠左。这与"添加进入 layout
    // 末端"的直觉略反，但与"close 永远最右"惯例保持一致。
    void AddCaptionIcon(int captionId, HBITMAP icon, LPCTSTR tooltip)
    {
        if (icon == nullptr)
        {
            return;
        }
        std::unique_ptr<CaptionIconButton> btn(
            new CaptionIconButton(captionId, icon));
        //—— 透传当前透明上下文，保证添加进来的 icon 配色与三按钮一致
        btn->SetTransparentContext(m_transparent);
        if (tooltip != nullptr && tooltip[0] != _T('\0'))
        {
            btn->RegisterTooltip(tooltip);
        }
        m_iconBtns.push_back(btn.get());
        AddChild(std::move(btn));
        Layout(m_rcItem);
        Invalidate();
    }

    // 按 captionId 移除；找不到 id 静默忽略。
    bool RemoveCaptionIcon(int captionId)
    {
        for (auto it = m_iconBtns.begin(); it != m_iconBtns.end(); ++it)
        {
            if ((*it)->GetCaptionId() == captionId)
            {
                CaptionIconButton* doomed = *it;
                m_iconBtns.erase(it);
                //—— RemoveChild 会触发 doomed 的析构（unique_ptr 释放），
                //   doomed 析构里走 DuiToolTipMgr::Unregister。
                RemoveChild(doomed);
                Layout(m_rcItem);
                Invalidate();
                return true;
            }
        }
        return false;
    }

    void ClearCaptionIcons()
    {
        if (m_iconBtns.empty())
        {
            return;
        }
        //—— 复制一份指针清单后逐个 RemoveChild。不能直接遍历 m_iconBtns
        //   清，因为 RemoveChild 触发析构 / 容器变化的 invalidation。
        std::vector<CaptionIconButton*> doomed = m_iconBtns;
        m_iconBtns.clear();
        for (CaptionIconButton* b : doomed)
        {
            RemoveChild(b);
        }
        Layout(m_rcItem);
        Invalidate();
    }

    void Layout(const RECT& rc) override
    {
        m_rcItem = rc;
        // Buttons (right → left): close → max → min → caption icons（按 Add 顺序）
        // 每个按钮固定 kCaptionBtnW 宽，按上面顺序贴右排列。
        const int btnW = kCaptionBtnW;
        int x = rc.right;
        if (m_btnClose->IsVisible())
        {
            RECT br = { x - btnW, rc.top, x, rc.bottom };
            m_btnClose->SetRect(br);
            x -= btnW;
        }
        if (m_btnMax->IsVisible())
        {
            RECT br = { x - btnW, rc.top, x, rc.bottom };
            m_btnMax->SetRect(br);
            x -= btnW;
        }
        if (m_btnMin->IsVisible())
        {
            RECT br = { x - btnW, rc.top, x, rc.bottom };
            m_btnMin->SetRect(br);
            x -= btnW;
        }
        //—— caption icons：按 m_iconBtns 顺序从右到左排（先 Add 的更靠右）
        for (CaptionIconButton* b : m_iconBtns)
        {
            if (!b->IsVisible())
            {
                continue;
            }
            RECT br = { x - btnW, rc.top, x, rc.bottom };
            b->SetRect(br);
            x -= btnW;
        }
        m_buttonStripLeft = x;     // for paint + nc-hittest
    }

    int GetButtonStripLeft() const { return m_buttonStripLeft; }

    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible)
        {
            return;
        }
        // Title strip background.
        // 透明模式跳过 — 让父 host 的 9-grid bg 穿透显示。
        if (!m_transparent)
        {
            HBRUSH hbr = ::CreateSolidBrush(RGB(245, 246, 250));
            ::FillRect(hdc, &m_rcItem, hbr);
            ::DeleteObject(hbr);
        }

        int xText = m_rcItem.left + 12;

        // Optional left-edge icon (16x16 painted vertically centered).
        if (m_icon)
        {
            const int kIcon = 16;
            int y = (m_rcItem.top + m_rcItem.bottom - kIcon) / 2;
            HDC mem = ::CreateCompatibleDC(hdc);
            HGDIOBJ old = ::SelectObject(mem, m_icon);
            BITMAP bm = {};
            ::GetObject(m_icon, sizeof(bm), &bm);
            if (bm.bmWidth > 0 && bm.bmHeight > 0)
            {
                ::SetStretchBltMode(hdc, HALFTONE);
                ::SetBrushOrgEx(hdc, 0, 0, nullptr);
                ::StretchBlt(hdc, xText, y, kIcon, kIcon,
                             mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
            }
            ::SelectObject(mem, old);
            ::DeleteDC(mem);
            xText += kIcon + 8;
        }

        // Title text.
        if (!m_title.IsEmpty())
        {
            HFONT useFont = DuiResMgr::Inst().GetDefaultFont();
            HFONT oldFont = useFont ? (HFONT)::SelectObject(hdc, useFont) : nullptr;
            int oldBk = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF oldClr = ::SetTextColor(hdc, m_titleTextColor);
            RECT rt = m_rcItem;
            rt.left  = xText;
            rt.right = m_buttonStripLeft - 6;
            ::DrawText(hdc, m_title, -1, &rt,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            ::SetTextColor(hdc, oldClr);
            ::SetBkMode(hdc, oldBk);
            if (oldFont)
            {
                ::SelectObject(hdc, oldFont);
            }
        }

        // Bottom 1px separator from content.
        // 透明模式跳过 —— 标题栏要让父 host 的 9-grid 背景整体穿透，再画
        // 一条浅灰线会在渐变标题区与客户区之间留下一道突兀的白线。
        if (!m_transparent)
        {
            HPEN hpen = ::CreatePen(PS_SOLID, 1, RGB(220, 220, 224));
            HPEN op = (HPEN)::SelectObject(hdc, hpen);
            ::MoveToEx(hdc, m_rcItem.left,  m_rcItem.bottom - 1, nullptr);
            ::LineTo  (hdc, m_rcItem.right, m_rcItem.bottom - 1);
            ::SelectObject(hdc, op);
            ::DeleteObject(hpen);
        }

        // Children (the three buttons) paint themselves.
        for (auto& c : m_children)
        {
            if (!c->IsVisible())
            {
                continue;
            }
            RECT inter;
            if (::IntersectRect(&inter, &c->GetRect(), &rcDirty))
            {
                c->OnPaint(hdc, inter);
            }
        }
    }

    // 透明模式：不画自身背景，让父 host 的 9-grid bg 穿透显示。
    // 标题文字 / icon / 三按钮仍按既有逻辑画在 hdc 上。
    // 同时把透明上下文标志传给三个 caption button — 它们据此切换
    // hover/press 配色（透明 → 品牌蓝 / 柔和红；不透明 → 经典浅灰 / 亮红）。
    void SetTransparent(bool b)
    {
        if (m_transparent == b)
        {
            return;
        }
        m_transparent = b;
        if (auto* mn = static_cast<CaptionButton*>(m_btnMin))
        {
            mn->SetTransparentContext(b);
        }
        if (auto* mx = static_cast<CaptionButton*>(m_btnMax))
        {
            mx->SetTransparentContext(b);
        }
        if (auto* cl = static_cast<CaptionButton*>(m_btnClose))
        {
            cl->SetTransparentContext(b);
        }
        //—— 透传给所有 caption icons，保持 hover/press 配色与三按钮一致
        for (CaptionIconButton* b2 : m_iconBtns)
        {
            b2->SetTransparentContext(b);
        }
        Invalidate();
    }
    bool IsTransparent() const { return m_transparent; }

    void SetTitleTextColor(COLORREF c)
    {
        m_titleTextColor = c;
        Invalidate();
    }

    void SetCaptionGlyphOverride(COLORREF c)
    {
        if (auto* mn = static_cast<CaptionButton*>(m_btnMin))
        {
            mn->SetGlyphColorOverride(c);
        }
        if (auto* mx = static_cast<CaptionButton*>(m_btnMax))
        {
            mx->SetGlyphColorOverride(c);
        }
        if (auto* cl = static_cast<CaptionButton*>(m_btnClose))
        {
            cl->SetGlyphColorOverride(c);
        }
    }

    // 由 DuiFrameWindow 在 SIZE_MAXIMIZED / SIZE_RESTORED 时调。
    // true → max 按钮画双层 restore glyph；false → 画普通 max glyph。
    void SetMaximizedState(bool maximized)
    {
        if (auto* mx = static_cast<CaptionButton*>(m_btnMax))
        {
            mx->SetShowAsRestore(maximized);
        }
    }

private:
    DuiControl* m_btnMin   = nullptr;
    DuiControl* m_btnMax   = nullptr;
    DuiControl* m_btnClose = nullptr;
    //—— caption icons：所有权由 DuiControl::m_children 持有；m_iconBtns 仅
    //   存裸指针以便 Layout / RemoveCaptionIcon / SetTransparent 透传。
    std::vector<CaptionIconButton*> m_iconBtns;
    CString     m_title;
    HBITMAP     m_icon = nullptr;
    int         m_buttonStripLeft = 0;
    bool        m_transparent     = false;
    COLORREF    m_titleTextColor  = RGB(40, 40, 40);
};

// =====================================================================
// DuiFrameWindow
// =====================================================================

DuiFrameWindow::DuiFrameWindow()
{
    // 默认开 1px 客户区边框 —— frame 在 WM_NCCALCSIZE 抹掉了系统非客户区，
    // 没有任何 OS 自带的窗口外边线，浅色客户区放在浅色桌面上（白底窗口
    // 在亮色壁纸上）会"看不见边"。颜色取浅灰 RGB(200,200,200)：
    //   · 比典型客户区背景 (245,246,250) 深约 20%，可识别
    //   · 比 Win11 系统边框 RGB(217,217,217) 略深一点，无 chrome 也撑得住
    //   · 设了 SetBgImage 时 DuiHost::OnPaint 会自动跳过这条边，让 9-grid
    //     图本身（圆角 / 阴影 / 装饰）作为视觉边界
    SetClientBorderColor(RGB(200, 200, 200));
}

DuiFrameWindow::~DuiFrameWindow()
{
    if (m_hWnd)
    {
        DestroyWindow();
    }
}

// WM_DESTROY raw handler。
// 时序背景：
//   1) BuildSkeleton 把 m_titleBar / m_skeleton / m_clientContent 三个
//      裸指针指向新建的子控件，并通过 SetRoot(std::move(sk)) 把所有权
//      转交给 DuiHost::m_root。
//   2) 窗口被 DestroyWindow 时，DuiHost::OnDestroy 会 m_root.reset()
//      释放整棵控件树，上述三个裸指针随之变成悬空指针。
//   3) 由于这是裸指针、没人清，本类的 SetTitle / SetIcon / SetButtons /
//      SetTransparent 等带 if(m_titleBar) 守卫的 setter，在窗口对象被
//      复用（如 CMainDlg::m_LoginDlg 反复 DoModal）的第二次 Create 之后、
//      BuildSkeleton 重建之前被调用时，会走到 m_titleBar 真分支并解引用
//      悬空指针，触发 AV 崩溃（SetTitle → CString::operator=(空) →
//      CString::Empty 访问已释放对象内部）。
// 本 handler 的职责仅一条：在基类 OnDestroy 释放 m_root 之前先把三个
// 裸指针置 nullptr。然后置 bHandled=FALSE 让消息继续 chain 到
// DuiHost::OnDestroy。
LRESULT DuiFrameWindow::OnFrameDestroyMsg(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    m_titleBar      = nullptr;
    m_skeleton      = nullptr;
    m_clientContent = nullptr;

    //继续派发到 CHAIN_MSG_MAP(DuiHost) → DuiHost::OnDestroy
    bHandled = FALSE;
    return 0;
}

void DuiFrameWindow::BuildSkeleton()
{
    if (m_skeleton)
    {
        return;
    }

    auto sk = std::unique_ptr<DuiVBox>(new DuiVBox());
    m_skeleton = sk.get();

    auto tb = std::unique_ptr<DuiFrameTitleBar>(new DuiFrameTitleBar());
    m_titleBar = tb.get();
    m_skeleton->AddChild(std::move(tb), DuiLayout::Hint().Fixed(m_titleH));

    // Empty placeholder for client content; replaced in SetClientContent.
    auto placeholder = std::unique_ptr<DuiControl>(new DuiControl());
    m_clientContent = placeholder.get();
    m_skeleton->AddChild(std::move(placeholder), DuiLayout::Hint().Weight(1));

    SetRoot(std::move(sk));

    // Push current title-bar settings into the freshly built control.
    m_titleBar->SetTitle(m_title);
    m_titleBar->SetIcon (m_titleIcon);
    m_titleBar->SetButtons(m_btnMin, m_btnMax, m_btnClose);
    m_titleBar->SetTransparent(m_titleTransparent);
    m_titleBar->SetTitleTextColor(m_titleTextColor);
    m_titleBar->SetCaptionGlyphOverride(m_captionGlyphOverride);
    m_titleBar->SetMaximizedState(m_isMaximized);
}

void DuiFrameWindow::LayoutSkeleton()
{
    if (!m_hWnd || !m_skeleton)
    {
        return;
    }
    RECT rc;
    GetClientRect(&rc);
    // 不能走 SetRect —— 它在 rect 没变时早 return；可我们调本函数的场
    // 景（SetClientContent / SetButtons / SetTitleBarHeight）改的是子
    // 树而非自身 rect，仍然需要 cascading Layout。直接调 Layout 强制
    // 重排，并写回 m_rcItem 保持一致。
    m_skeleton->ForceLayout(rc);
    Invalidate(FALSE);
}

void DuiFrameWindow::SetTitle(LPCTSTR title)
{
    m_title = title ? title : _T("");
    if (m_titleBar)
    {
        m_titleBar->SetTitle(m_title);
    }
    if (m_hWnd)
    {
        ::SetWindowText(m_hWnd, m_title);
    }
}

void DuiFrameWindow::SetTitleBarHeight(int px)
{
    if (px < 18)
    {
        px = 18;
    }
    if (m_titleH == px)
    {
        return;
    }
    m_titleH = px;
    if (m_skeleton)
    {
        // Rebuild the layout hint by re-issuing AddChild ordering — easier
        // path: tear down skeleton and rebuild.
        m_skeleton    = nullptr;
        m_titleBar    = nullptr;
        m_clientContent = nullptr;
        BuildSkeleton();
        LayoutSkeleton();
    }
}

void DuiFrameWindow::SetIcon(HBITMAP icon)
{
    m_titleIcon = icon;
    if (m_titleBar)
    {
        m_titleBar->SetIcon(icon);
    }
}

void DuiFrameWindow::SetButtons(bool minBtn, bool maxBtn, bool closeBtn)
{
    m_btnMin   = minBtn;
    m_btnMax   = maxBtn;
    m_btnClose = closeBtn;
    if (m_titleBar)
    {
        m_titleBar->SetButtons(minBtn, maxBtn, closeBtn);
    }
    LayoutSkeleton();
}

void DuiFrameWindow::SetTitleBarTransparent(bool b)
{
    m_titleTransparent = b;
    if (m_titleBar)
    {
        m_titleBar->SetTransparent(b);
    }
}

void DuiFrameWindow::SetTitleTextColor(COLORREF c)
{
    m_titleTextColor = c;
    if (m_titleBar)
    {
        m_titleBar->SetTitleTextColor(c);
    }
}

void DuiFrameWindow::SetCaptionGlyphColor(COLORREF c)
{
    m_captionGlyphOverride = c;
    if (m_titleBar)
    {
        m_titleBar->SetCaptionGlyphOverride(c);
    }
}

void DuiFrameWindow::SetBorderPx(int px)
{
    if (px < 0)
    {
        px = 0;
    }
    m_borderPx = px;
}

void DuiFrameWindow::SetResizable(bool b)
{
    m_resizable = b;
}

void DuiFrameWindow::SetMinSize(int w, int h)
{
    m_minW = w < 1 ? 1 : w;
    m_minH = h < 1 ? 1 : h;
}

void DuiFrameWindow::SetMaxSize(int w, int h)
{
    //—— 0 = 不限；其它值原样保存（OS 会进一步 clamp 到 work area）。
    //   < 0 视为 0（不限），避免拼写错误传 -1 等导致拖动锁死在 0 像素。
    m_maxW = w < 0 ? 0 : w;
    m_maxH = h < 0 ? 0 : h;
}

int DuiFrameWindow::AddCaptionIcon(HBITMAP icon, LPCTSTR tooltip)
{
    if (icon == nullptr)
    {
        return 0;
    }
    //—— skeleton 还没建好时也允许调（典型：ApplyConfig 阶段）。先 BuildSkeleton。
    if (!m_titleBar)
    {
        BuildSkeleton();
    }
    if (!m_titleBar)
    {
        return 0;
    }
    int captionId = m_nextCaptionIconId++;
    m_titleBar->AddCaptionIcon(captionId, icon, tooltip);
    return captionId;
}

void DuiFrameWindow::RemoveCaptionIcon(int captionId)
{
    if (m_titleBar)
    {
        m_titleBar->RemoveCaptionIcon(captionId);
    }
}

void DuiFrameWindow::ClearCaptionIcons()
{
    if (m_titleBar)
    {
        m_titleBar->ClearCaptionIcons();
    }
}

void DuiFrameWindow::ApplyConfig(const DuiFrameWindowConfig& cfg)
{
    // 顺序按"几乎不互相影响"先后调；SetTitleBarHeight 内部会触发
    // skeleton 重建，所以放在标题栏其它属性之前 —— BuildSkeleton 会从
    // 当前字段重新推一次 transparent / textColor / glyphColor，
    // 即便后面再调一次 setter 也对得上。
    if (cfg.title.has_value())
    {
        SetTitle(cfg.title.value());
    }
    if (cfg.titleBarHeight.has_value())
    {
        SetTitleBarHeight(cfg.titleBarHeight.value());
    }
    if (cfg.titleBarTransparent.has_value())
    {
        SetTitleBarTransparent(cfg.titleBarTransparent.value());
    }
    if (cfg.titleTextColor.has_value())
    {
        SetTitleTextColor(cfg.titleTextColor.value());
    }
    if (cfg.captionGlyphColor.has_value())
    {
        SetCaptionGlyphColor(cfg.captionGlyphColor.value());
    }

    // 三按钮 —— 任一显式给出就调，未给的拿 frame 当前可见性继续
    if (cfg.hasMinButton.has_value()
        || cfg.hasMaxButton.has_value()
        || cfg.hasCloseButton.has_value())
    {
        bool mn = cfg.hasMinButton  .value_or(HasMinButton  ());
        bool mx = cfg.hasMaxButton  .value_or(HasMaxButton  ());
        bool cl = cfg.hasCloseButton.value_or(HasCloseButton());
        SetButtons(mn, mx, cl);
    }

    if (cfg.minW.has_value() || cfg.minH.has_value())
    {
        int w = cfg.minW.value_or(GetMinSize().cx);
        int h = cfg.minH.value_or(GetMinSize().cy);
        SetMinSize(w, h);
    }
    if (cfg.resizable.has_value())
    {
        SetResizable(cfg.resizable.value());
    }
    if (cfg.borderPx.has_value())
    {
        SetBorderPx(cfg.borderPx.value());
    }
    if (cfg.clientBorderColor.has_value())
    {
        SetClientBorderColor(cfg.clientBorderColor.value());
    }

    // bg 图：路径已经 caller 解析过（绝对路径），直接交给 host 加载
    // 并持有。bgDstInsets 未设 → 退化为 src == dst（经典 9-grid，4 角
    // 不缩）。
    if (!cfg.bgImagePath.IsEmpty())
    {
        if (cfg.bgDstInsets.has_value())
        {
            LoadBgImageFromFile(cfg.bgImagePath,
                                cfg.bgSrcInsets, cfg.bgDstInsets.value());
        }
        else
        {
            LoadBgImageFromFile(cfg.bgImagePath, cfg.bgSrcInsets);
        }
    }
}

void DuiFrameWindow::SetClientContent(std::unique_ptr<DuiControl> root)
{
    BuildSkeleton();
    if (!m_skeleton)
    {
        return;
    }

    // Replace the content slot. Skeleton holds: [titleBar, content].
    if (m_clientContent)
    {
        m_skeleton->RemoveChild(m_clientContent);
        m_clientContent = nullptr;
    }
    DuiControl* raw = root.get();
    m_skeleton->AddChild(std::move(root), DuiLayout::Hint().Weight(1));
    m_clientContent = raw;
    LayoutSkeleton();
}

LRESULT DuiFrameWindow::OnNcCalcSize(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (wParam)
    {
        // 普通窗口态：返 0 with wParam==TRUE = 客户区 = 提议窗口矩形（无
        // chrome）。系统仍然按 WS_THICKFRAME 处理 snap / 动画。
        //
        // 最大化态：OS 把窗口往外推 borderX/borderY 像素（让"可见部分 =
        // 工作区"），不补偿的话自绘标题栏顶部会被屏幕边裁掉 ~7-8px。把
        // 客户区四边内缩同样量 = 让 DUI 子树在屏幕工作区内完整可见。
        const int borderX = ::GetSystemMetrics(SM_CXFRAME)
                          + ::GetSystemMetrics(SM_CXPADDEDBORDER);
        const int borderY = ::GetSystemMetrics(SM_CYFRAME)
                          + ::GetSystemMetrics(SM_CXPADDEDBORDER);
        const RECT inset = ComputeMaximizedClientInsets(::IsZoomed(m_hWnd) != 0,
                                                        borderX, borderY);
        auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
        p->rgrc[0].left   += inset.left;
        p->rgrc[0].top    += inset.top;
        p->rgrc[0].right  -= inset.right;
        p->rgrc[0].bottom -= inset.bottom;
        bHandled = TRUE;
        return 0;
    }
    bHandled = FALSE;
    return 0;
}

RECT DuiFrameWindow::ComputeMaximizedClientInsets(bool maximized,
                                                  int borderX,
                                                  int borderY)
{
    RECT r = { 0, 0, 0, 0 };
    if (maximized)
    {
        r.left   = borderX;
        r.top    = borderY;
        r.right  = borderX;
        r.bottom = borderY;
    }
    return r;
}

UINT DuiFrameWindow::ComputeNcHitTest(POINT pt,
                                      const RECT& wr,
                                      int titleH,
                                      int borderPx,
                                      bool resizable,
                                      const RECT btnRcs[3])
{
    if (pt.x < wr.left || pt.x >= wr.right ||
        pt.y < wr.top  || pt.y >= wr.bottom)
    {
        return HTNOWHERE;
    }

    // Resize border zones (only when resizable). Corners take precedence.
    if (resizable && borderPx > 0)
    {
        bool nearL = pt.x <  wr.left   + borderPx;
        bool nearR = pt.x >= wr.right  - borderPx;
        bool nearT = pt.y <  wr.top    + borderPx;
        bool nearB = pt.y >= wr.bottom - borderPx;
        if (nearT && nearL)
        {
            return HTTOPLEFT;
        }
        if (nearT && nearR)
        {
            return HTTOPRIGHT;
        }
        if (nearB && nearL)
        {
            return HTBOTTOMLEFT;
        }
        if (nearB && nearR)
        {
            return HTBOTTOMRIGHT;
        }
        if (nearT)
        {
            return HTTOP;
        }
        if (nearB)
        {
            return HTBOTTOM;
        }
        if (nearL)
        {
            return HTLEFT;
        }
        if (nearR)
        {
            return HTRIGHT;
        }
    }

    // Inside title bar?
    bool inTitle = (pt.y < wr.top + titleH);
    if (inTitle)
    {
        // If over any button rect, defer to DUI client (button handles click).
        for (int i = 0; i < 3; ++i)
        {
            if (::PtInRect(&btnRcs[i], pt))
            {
                return HTCLIENT;
            }
        }
        return HTCAPTION;
    }
    return HTCLIENT;
}

RECT DuiFrameWindow::GetButtonRectScreen(int which) const
{
    RECT empty = { 0, 0, 0, 0 };
    if (!m_titleBar)
    {
        return empty;
    }
    DuiControl* b = nullptr;
    switch (which)
    {
    case 0:
        b = m_titleBar->GetMinBtn();
        break;
    case 1:
        b = m_titleBar->GetMaxBtn();
        break;
    case 2:
        b = m_titleBar->GetCloseBtn();
        break;
    }
    if (!b || !b->IsVisible())
    {
        return empty;
    }

    RECT rc = b->GetRect();
    POINT tl = { rc.left, rc.top };
    POINT br = { rc.right, rc.bottom };
    if (m_hWnd)
    {
        ::ClientToScreen(m_hWnd, &tl);
        ::ClientToScreen(m_hWnd, &br);
    }
    rc.left = tl.x;
    rc.top = tl.y;
    rc.right = br.x;
    rc.bottom = br.y;
    return rc;
}

LRESULT DuiFrameWindow::OnNcHitTestMsg(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    RECT wr;
    GetWindowRect(&wr);
    RECT btnRcs[3] = {
        GetButtonRectScreen(0),
        GetButtonRectScreen(1),
        GetButtonRectScreen(2)
    };
    // borderPx 是 96-dpi 逻辑像素，按当前 monitor DPI 缩放成物理像素。
    // PerMonitorV2 下 hwnd 自身用物理像素，所以 hittest 必须用物理。
    // 这样不管 100% / 125% / 150% / 200% DPI，resize 抓握区都是稳定的
    // 8 个逻辑 px 宽，触摸板上也好抓。
    int dpi = GetDpi();
    if (dpi <= 0)
    {
        dpi = 96;
    }
    int scaledBorder = ::MulDiv(m_borderPx, dpi, 96);
    UINT ht = ComputeNcHitTest(pt, wr, m_titleH, scaledBorder, m_resizable, btnRcs);
    bHandled = TRUE;
    return (LRESULT)ht;
}

void DuiFrameWindow::OnGetMinMaxInfo(LPMINMAXINFO mmi)
{
    if (mmi)
    {
        mmi->ptMinTrackSize.x = m_minW;
        mmi->ptMinTrackSize.y = m_minH;
        //—— SetMaxSize 设了正值（!= 0）才填 ptMaxTrackSize；0 表示
        //   "不限"，让 OS 沿用 work area 默认上限。
        if (m_maxW > 0)
        {
            mmi->ptMaxTrackSize.x = m_maxW;
        }
        if (m_maxH > 0)
        {
            mmi->ptMaxTrackSize.y = m_maxH;
        }
    }
    SetMsgHandled(FALSE);
}

// WM_SIZE — track maximized vs restored so the max button can swap to the
// restore glyph (two overlapping squares) when the window is maximized.
// SIZE_MINIMIZED is ignored (the window is hidden; previous max/restore
// state should persist for when it's un-minimized). bHandled stays FALSE
// so DuiHost still gets the message and re-runs DUI layout.
LRESULT DuiFrameWindow::OnSizeMsg(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
    bHandled = FALSE;
    if (wParam == SIZE_MAXIMIZED)
    {
        if (!m_isMaximized)
        {
            m_isMaximized = true;
            if (m_titleBar)
            {
                m_titleBar->SetMaximizedState(true);
            }
        }
    }
    else if (wParam == SIZE_RESTORED)
    {
        if (m_isMaximized)
        {
            m_isMaximized = false;
            if (m_titleBar)
            {
                m_titleBar->SetMaximizedState(false);
            }
        }
    }
    return 0;
}

LRESULT DuiFrameWindow::OnDuiChildNotify(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
    DuiNotify* n = reinterpret_cast<DuiNotify*>(lParam);
    if (n && n->code == DUIN_CLICK && m_titleBar)
    {
        switch (n->ctrlId)
        {
        case 1:   // minimize
            ::PostMessage(m_hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            bHandled = TRUE;
            return 0;
        case 2:   // maximize / restore toggle
        {
            WINDOWPLACEMENT wp = {};
            wp.length = sizeof(wp);
            GetWindowPlacement(&wp);
            UINT cmd = (wp.showCmd == SW_MAXIMIZE) ? SC_RESTORE : SC_MAXIMIZE;
            ::PostMessage(m_hWnd, WM_SYSCOMMAND, cmd, 0);
            bHandled = TRUE;
            return 0;
        }
        case 3:   // close
            ::PostMessage(m_hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
            bHandled = TRUE;
            return 0;
        default:
            break;
        }
    }
    bHandled = FALSE;
    return 0;
}

} // namespace balloonwjui

#endif // BUI_FEATURE_FRAMEWINDOW
