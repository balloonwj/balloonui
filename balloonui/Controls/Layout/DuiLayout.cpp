#include "stdafx.h"
#include "DuiLayout.h"
#include "../../DuiPaintAA.h"   // DuiVBox 卡片样式走 DuiAA::FillRoundRect

namespace balloonwjui {

// ===== DuiLayout (base) ===================================================

void DuiLayout::SetPadding(int l, int t, int r, int b)
{
    m_padL = l;
    m_padT = t;
    m_padR = r;
    m_padB = b;
    // Layout 体内调到本函数时,m_layouting=true,跳过自我重排;让本次正在
    // 进行的 Layout 自己收尾,避免无限递归(详见 DuiLayout::LayoutGuard 注释)。
    if (m_layouting)
    {
        return;
    }
    Layout(m_rcItem);
    Invalidate();
}

void DuiLayout::SetGap(int gap)
{
    m_gap = gap;
    // 同 SetPadding:Layout 进行中只更新数据,不再二次触发 Layout/Invalidate。
    if (m_layouting)
    {
        return;
    }
    Layout(m_rcItem);
    Invalidate();
}

void DuiLayout::AddChild(std::unique_ptr<DuiControl> child, const Hint& hint)
{
    DuiControl* raw = child.get();
    DuiControl::AddChild(std::move(child));   // base inserts into m_children
    m_hints.push_back({raw, hint});
}

void DuiLayout::OnChildRemoved_(DuiControl* child)
{
    // 同 child 指针只可能 push 过 1 次，但 SetHint 也会 push（在没有匹配
    // 时），所以可能有多条；erase-remove 全清。
    for (auto it = m_hints.begin(); it != m_hints.end(); )
    {
        if (it->first == child)
        {
            it = m_hints.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void DuiLayout::SetHint(DuiControl* child, const Hint& hint)
{
    for (auto& kv : m_hints)
    {
        if (kv.first == child)
        {
            kv.second = hint;
            // Layout 体内常见模式:override Layout 的子类按可用宽度算完
            // titleEdit / bodyEdit 应有高度,SetHint 写回。若此处再调
            // Layout(m_rcItem) 会立刻再次进入子类的 Layout,无限递归。
            // 由 LayoutGuard 在子类 Layout 体首行置 m_layouting=true 拦截。
            if (m_layouting)
            {
                return;
            }
            Layout(m_rcItem);
            Invalidate();
            return;
        }
    }
    m_hints.push_back({child, hint});
    if (m_layouting)
    {
        return;
    }
    Layout(m_rcItem);
    Invalidate();
}

DuiLayout::Hint DuiLayout::GetHint(DuiControl* child) const
{
    return HintFor(child);
}

DuiLayout::Hint DuiLayout::HintFor(DuiControl* c) const
{
    for (const auto& kv : m_hints)
    {
        if (kv.first == c)
        {
            return kv.second;
        }
    }
    return Hint{};
}

// Apply margin + alignment + fixed sizes inside a single cell.
RECT DuiLayout::ApplyHint(const RECT& cell, const Hint& h, bool mainIsHorizontal)
{
    RECT r = { cell.left + h.marginL,
               cell.top  + h.marginT,
               cell.right - h.marginR,
               cell.bottom - h.marginB };

    int availW = r.right - r.left;
    int availH = r.bottom - r.top;
    int wantW  = mainIsHorizontal ? h.fixedMain  : h.fixedCross;
    int wantH  = mainIsHorizontal ? h.fixedCross : h.fixedMain;

    Align alignX = mainIsHorizontal ? h.alignMain  : h.alignCross;
    Align alignY = mainIsHorizontal ? h.alignCross : h.alignMain;

    auto applyAxis = [](int origin, int avail, int want, Align a, int& outOrigin, int& outSize)
    {
        if (want < 0 || want >= avail || a == AlignFill)
        {
            outOrigin = origin;
            outSize   = avail;
            return;
        }
        switch (a)
        {
        case AlignNear:
            outOrigin = origin;
            break;
        case AlignFar:
            outOrigin = origin + (avail - want);
            break;
        case AlignCenter:
        default:
            outOrigin = origin + (avail - want) / 2;
            break;
        }
        outSize = want;
    };

    int x, w, y, hgt;
    applyAxis(r.left, availW, wantW, alignX, x, w);
    applyAxis(r.top,  availH, wantH, alignY, y, hgt);
    return RECT{ x, y, x + w, y + hgt };
}

// ===== DuiHBox ============================================================

void DuiHBox::Layout(const RECT& rcAvail)
{
    // 自递归护栏:期间若有 SetHint / SetPadding / SetGap 调用,只更新数据,
    // 不再二次触发 Layout / Invalidate(见 DuiLayout::LayoutGuard 注释)。
    LayoutGuard layoutGuard(*this);

    m_rcItem = rcAvail;

    RECT inner = { rcAvail.left + m_padL,
                   rcAvail.top  + m_padT,
                   rcAvail.right - m_padR,
                   rcAvail.bottom - m_padB };
    int innerW = inner.right - inner.left;
    int innerH = inner.bottom - inner.top;
    if (innerW <= 0 || innerH <= 0 || m_children.empty())
    {
        return;
    }

    // Pass 1: total fixed width + total weight + visible-child count.
    int fixedSum = 0, weightSum = 0, visN = 0;
    for (auto& up : m_children)
    {
        if (!up->IsVisible())
        {
            continue;
        }
        ++visN;
        Hint h = HintFor(up.get());
        int chunk = (h.fixedMain >= 0) ? h.fixedMain : 0;
        chunk += h.marginL + h.marginR;
        fixedSum += chunk;
        if (h.fixedMain < 0)
        {
            weightSum += (h.weight > 0 ? h.weight : 1);
        }
    }
    if (visN == 0)
    {
        return;
    }

    int gapTotal = m_gap * (visN - 1);
    int flexAvail = innerW - fixedSum - gapTotal;
    if (flexAvail < 0)
    {
        flexAvail = 0;
    }

    // Pass 2: place each child.
    int x = inner.left;
    bool first = true;
    for (auto& up : m_children)
    {
        if (!up->IsVisible())
        {
            continue;
        }
        if (!first)
        {
            x += m_gap;
        }
        first = false;

        Hint h = HintFor(up.get());
        int mainPx;
        if (h.fixedMain >= 0)
        {
            mainPx = h.fixedMain + h.marginL + h.marginR;
        }
        else
        {
            mainPx = h.marginL + h.marginR
                   + (weightSum > 0 ? (flexAvail * (h.weight > 0 ? h.weight : 1)) / weightSum : 0);
        }

        RECT cell = { x, inner.top, x + mainPx, inner.bottom };
        up->SetRect(ApplyHint(cell, h, /*mainIsHorizontal*/true));
        x += mainPx;
    }
}

SIZE DuiHBox::GetDesiredSize() const
{
    int mainSum = 0;     // sum of (max(0, fixedMain) + marginL + marginR)
    int crossMax = 0;    // max of (max(fixedCross, child.cy) + marginT + marginB)
    int visN = 0;
    for (auto& up : m_children)
    {
        if (!up->IsVisible())
        {
            continue;
        }
        ++visN;
        Hint h = HintFor(up.get());

        int mainPx = (h.fixedMain >= 0) ? h.fixedMain : 0;
        mainSum += mainPx + h.marginL + h.marginR;

        // Cross axis (Y for HBox): fixedCross wins if set, otherwise ask the
        // child for its desired cross size. Add per-child cross margins so the
        // container is tall enough to fit the tallest child plus its own margin.
        int childCross = (h.fixedCross >= 0) ? h.fixedCross : up->GetDesiredSize().cy;
        int withMargin = childCross + h.marginT + h.marginB;
        if (withMargin > crossMax)
        {
            crossMax = withMargin;
        }
    }
    int gapTotal = (visN > 1) ? (m_gap * (visN - 1)) : 0;
    SIZE s;
    s.cx = m_padL + mainSum + gapTotal + m_padR;
    s.cy = m_padT + crossMax + m_padB;
    return s;
}

// ===== DuiVBox ============================================================

void DuiVBox::Layout(const RECT& rcAvail)
{
    // 自递归护栏:见 DuiHBox::Layout 同名注释。
    LayoutGuard layoutGuard(*this);

    m_rcItem = rcAvail;

    RECT inner = { rcAvail.left + m_padL,
                   rcAvail.top  + m_padT,
                   rcAvail.right - m_padR,
                   rcAvail.bottom - m_padB };
    int innerW = inner.right - inner.left;
    int innerH = inner.bottom - inner.top;
    if (innerW <= 0 || innerH <= 0 || m_children.empty())
    {
        return;
    }

    int fixedSum = 0, weightSum = 0, visN = 0;
    for (auto& up : m_children)
    {
        if (!up->IsVisible())
        {
            continue;
        }
        ++visN;
        Hint h = HintFor(up.get());
        int chunk = (h.fixedMain >= 0) ? h.fixedMain : 0;
        chunk += h.marginT + h.marginB;
        fixedSum += chunk;
        if (h.fixedMain < 0)
        {
            weightSum += (h.weight > 0 ? h.weight : 1);
        }
    }
    if (visN == 0)
    {
        return;
    }

    int gapTotal = m_gap * (visN - 1);
    int flexAvail = innerH - fixedSum - gapTotal;
    if (flexAvail < 0)
    {
        flexAvail = 0;
    }

    int y = inner.top;
    bool first = true;
    for (auto& up : m_children)
    {
        if (!up->IsVisible())
        {
            continue;
        }
        if (!first)
        {
            y += m_gap;
        }
        first = false;

        Hint h = HintFor(up.get());
        int mainPx;
        if (h.fixedMain >= 0)
        {
            mainPx = h.fixedMain + h.marginT + h.marginB;
        }
        else
        {
            mainPx = h.marginT + h.marginB
                   + (weightSum > 0 ? (flexAvail * (h.weight > 0 ? h.weight : 1)) / weightSum : 0);
        }

        RECT cell = { inner.left, y, inner.right, y + mainPx };
        up->SetRect(ApplyHint(cell, h, /*mainIsHorizontal*/false));
        y += mainPx;
    }
}

SIZE DuiVBox::GetDesiredSize() const
{
    int mainSum = 0;     // sum of (max(0, fixedMain) + marginT + marginB)
    int crossMax = 0;    // max of (max(fixedCross, child.cx) + marginL + marginR)
    int visN = 0;
    for (auto& up : m_children)
    {
        if (!up->IsVisible())
        {
            continue;
        }
        ++visN;
        Hint h = HintFor(up.get());

        int mainPx = (h.fixedMain >= 0) ? h.fixedMain : 0;
        mainSum += mainPx + h.marginT + h.marginB;

        // Cross axis (X for VBox): fixedCross wins if set, otherwise ask the
        // child. Default-control children that don't override GetDesiredSize
        // return {0,0} → contribute only marginL+R.
        int childCross = (h.fixedCross >= 0) ? h.fixedCross : up->GetDesiredSize().cx;
        int withMargin = childCross + h.marginL + h.marginR;
        if (withMargin > crossMax)
        {
            crossMax = withMargin;
        }
    }
    int gapTotal = (visN > 1) ? (m_gap * (visN - 1)) : 0;
    SIZE s;
    s.cx = m_padL + crossMax + m_padR;
    s.cy = m_padT + mainSum + gapTotal + m_padB;
    return s;
}

// ---- 卡片样式 setter / OnPaint / 静态 helper ----

void DuiVBox::SetBgColor(COLORREF c)
{
    if (m_bgColor == c)
    {
        return;
    }
    m_bgColor = c;
    Invalidate();
}

void DuiVBox::SetCornerRadius(int px)
{
    // 防御性钳:负值视作 0(直角)。DuiAA::FillRoundRect 内部还会再夹到 min(w,h)/2。
    if (px < 0)
    {
        px = 0;
    }
    if (m_cornerRadius == px)
    {
        return;
    }
    m_cornerRadius = px;
    Invalidate();
}

void DuiVBox::SetBorderColor(COLORREF c)
{
    if (m_borderColor == c)
    {
        return;
    }
    m_borderColor = c;
    Invalidate();
}

void DuiVBox::SetBorderWidth(float w)
{
    // 防御性钳:负值视作 0(等同不描边,与 DuiAA::FillRoundRect 一致)。
    if (w < 0.0f)
    {
        w = 0.0f;
    }
    if (m_borderWidth == w)
    {
        return;
    }
    m_borderWidth = w;
    Invalidate();
}

void DuiVBox::OnPaint(HDC hdc, const RECT& rcDirty)
{
    if (!m_bVisible)
    {
        return;
    }
    // 卡片样式:只要 bg 或 border 任一非 CLR_INVALID, 就走 PaintBackground。
    // 全 CLR_INVALID(默认)→ 跳过装饰, 与历史行为一致。
    if (m_bgColor != CLR_INVALID || m_borderColor != CLR_INVALID)
    {
        PaintBackground(hdc, m_rcItem, m_bgColor, m_cornerRadius,
                        m_borderColor, m_borderWidth);
    }
    // 装饰画完后, 调基类继续画子控件。
    DuiControl::OnPaint(hdc, rcDirty);
}

void DuiVBox::PaintBackground(HDC hdc, const RECT& rc, COLORREF bg, int radius,
                              COLORREF border, float borderWidth)
{
    // borderWidth <= 0 视作不描边, 防御性传 CLR_INVALID 给 DuiAA。
    // 这样调用方 SetBorderWidth(0) 与 SetBorderColor(CLR_INVALID) 等效。
    if (borderWidth <= 0.0f)
    {
        border = CLR_INVALID;
    }
    DuiAA::FillRoundRect(hdc, rc, bg, radius, border, borderWidth);
}

// ===== DuiGrid ============================================================

void DuiGrid::SetGrid(int rows, int cols)
{
    if (rows < 1)
    {
        rows = 1;
    }
    if (cols < 1)
    {
        cols = 1;
    }
    m_rows = rows;
    m_cols = cols;
    Layout(m_rcItem);
    Invalidate();
}

void DuiGrid::Layout(const RECT& rcAvail)
{
    // 自递归护栏:见 DuiHBox::Layout 同名注释。
    LayoutGuard layoutGuard(*this);

    m_rcItem = rcAvail;

    RECT inner = { rcAvail.left + m_padL,
                   rcAvail.top  + m_padT,
                   rcAvail.right - m_padR,
                   rcAvail.bottom - m_padB };
    int innerW = inner.right - inner.left;
    int innerH = inner.bottom - inner.top;
    if (innerW <= 0 || innerH <= 0 || m_children.empty())
    {
        return;
    }

    int gapsX = m_gap * (m_cols - 1);
    int gapsY = m_gap * (m_rows - 1);
    int cellW = (innerW - gapsX) / m_cols;
    int cellH = (innerH - gapsY) / m_rows;
    if (cellW <= 0 || cellH <= 0)
    {
        return;
    }

    int idx = 0;
    for (auto& up : m_children)
    {
        if (!up->IsVisible())
        {
            continue;
        }
        if (idx >= m_rows * m_cols)
        {
            break;     // overflow children: ignored
        }

        int row = idx / m_cols;
        int col = idx % m_cols;
        int x = inner.left + col * (cellW + m_gap);
        int y = inner.top  + row * (cellH + m_gap);
        RECT cell = { x, y, x + cellW, y + cellH };
        Hint h = HintFor(up.get());
        up->SetRect(ApplyHint(cell, h, /*mainIsHorizontal*/true));
        ++idx;
    }
}

SIZE DuiGrid::GetDesiredSize() const
{
    // cellW = max over visible children of (max(fixedMain, child.cx) + L+R margins)
    // cellH = max over visible children of (max(fixedCross, child.cy) + T+B margins)
    // No visible child → both stay 0; intrinsic = padding only.
    int cellW = 0;
    int cellH = 0;
    for (auto& up : m_children)
    {
        if (!up->IsVisible())
        {
            continue;
        }
        Hint h = HintFor(up.get());
        SIZE child = up->GetDesiredSize();
        int wantW = ((h.fixedMain  >= 0) ? h.fixedMain  : child.cx) + h.marginL + h.marginR;
        int wantH = ((h.fixedCross >= 0) ? h.fixedCross : child.cy) + h.marginT + h.marginB;
        if (wantW > cellW) { cellW = wantW; }
        if (wantH > cellH) { cellH = wantH; }
    }
    int gapsX = (m_cols > 1) ? (m_gap * (m_cols - 1)) : 0;
    int gapsY = (m_rows > 1) ? (m_gap * (m_rows - 1)) : 0;
    SIZE s;
    s.cx = m_padL + (m_cols * cellW) + gapsX + m_padR;
    s.cy = m_padT + (m_rows * cellH) + gapsY + m_padB;
    return s;
}

} // namespace balloonwjui
