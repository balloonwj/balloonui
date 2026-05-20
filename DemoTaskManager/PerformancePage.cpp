// =============================================================================
// PerformancePage.cpp
// =============================================================================
#include "stdafx.h"
#include "PerformancePage.h"

#include "DuiPaintAA.h"
#include "DuiResMgr.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"

namespace demotaskmgr {

using balloonwjui::DuiControl;
using balloonwjui::DuiVBox;
using balloonwjui::DuiHBox;
using balloonwjui::DuiLabel;
using balloonwjui::DuiLayout;
using balloonwjui::DuiSplitter;
using balloonwjui::DuiResMgr;

namespace DuiAA = balloonwjui::DuiAA;

// =============================================================================
// 视觉常量。Win10 任务管理器的"性能"页是非常成熟的设计；下面的取值大致照
// 着抄一遍，能 visual diff 在像素级别接近就行。
// =============================================================================

// ---- 折线图 ----
// 网格 10×10：X 11 条线 / Y 11 条线（plan 写 10×6 是基础版；用 10×10 更
// 接近 Win10 真任务管理器视觉）。
static const int      kChartGridCols    = 10;
static const int      kChartGridRows    = 10;
static const COLORREF kChartGridColor   = RGB(232, 232, 232);
static const COLORREF kChartBgColor     = RGB(255, 255, 255);
static const COLORREF kChartBorderColor = RGB(204, 204, 204);
static const float    kChartLineWidth   = 1.5f;
// 折线图 4 边 padding，给 Y 轴标注 / 大字当前值留位置
static const int      kChartPadL        = 8;
static const int      kChartPadT        = 8;
static const int      kChartPadR        = 36;   // 右边给 "100%" / "0" Y 轴标注
static const int      kChartPadB        = 8;

// ---- sidebar item ----
// 行高 70：让 14pt 标题 + 18pt 当前值大字 + 副标题三行不挤；与 Win10 真任
// 务管理器 sidebar 行高接近。
static const int      kSidebarItemH         = 70;
static const int      kSidebarValueColW     = 110;   // 右上当前值大字预留宽
static const int      kSidebarSelBarW       = 4;     // 选中态左侧 accent 竖条宽
static const COLORREF kSidebarBgNormal      = RGB(247, 247, 247);
static const COLORREF kSidebarBgSelected    = RGB(225, 233, 247); // 浅蓝
static const COLORREF kSidebarBgHover       = RGB(238, 238, 238);
static const COLORREF kSidebarTitleColor    = RGB(50, 50, 60);
static const COLORREF kSidebarSubtitleColor = RGB(120, 120, 130);
static const COLORREF kSidebarValueColor    = RGB(50, 50, 60);
// sidebar 行内 mini sparkline 的尺寸 / 位置
static const int      kSparklineW           = 90;
static const int      kSparklineH           = 18;

// ---- detail label 颜色 ----
static const COLORREF kDetailTitleColor    = RGB(40, 40, 50);
static const COLORREF kDetailSubtitleColor = RGB(120, 120, 130);
static const COLORREF kDetailStatColor     = RGB(60, 60, 70);

// ---- 字号（pt），实际 px 由 DPI 决定（CreateFont 内自动换算） ----
static const int      kFontSizeBigValue    = 22;   // chart 左上当前值大字
static const int      kFontSizeYAxis       = 9;    // Y 轴 0/50/100 标注
// sidebar 字号：title 14pt / 当前值 18pt 加粗 / 副标题 9pt，对齐 Win10
// 真任务管理器 sidebar 各行的视觉权重（当前值最显眼，标题次之，副标题
// 灰小字）。
static const int      kFontSizeSidebarTitle    = 14;
static const int      kFontSizeSidebarValue    = 18;
static const int      kFontSizeSidebarSubtitle = 9;
static const int      kFontSizeDetailTitle     = 14;
static const int      kFontSizeDetailSubtitle  = 10;

// =============================================================================
// 工具：按 pt 创建 HFONT。caller 负责 DeleteObject。
// =============================================================================

static HFONT MakeFont(int pt, bool bold = false)
{
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(pt, ::GetDeviceCaps(::GetDC(NULL), LOGPIXELSY), 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = GB2312_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, _T("Microsoft YaHei"));
    return ::CreateFontIndirect(&lf);
}

// 在指定矩形里画文字 + 自动颜色 / 字体。drawFlags = DT_*。
// 用完不修改 hdc 的 SelectObject 状态（恢复旧字体 / 文本色 / bg mode）。
static void DrawTextLine(HDC hdc, const RECT& rc, LPCTSTR text,
                         HFONT font, COLORREF clr, UINT drawFlags)
{
    HFONT      oldFont = (HFONT)::SelectObject(hdc, font);
    COLORREF   oldClr  = ::SetTextColor(hdc, clr);
    int        oldBk   = ::SetBkMode(hdc, TRANSPARENT);
    RECT       r = rc;
    ::DrawText(hdc, text, -1, &r, drawFlags);
    ::SetBkMode(hdc, oldBk);
    ::SetTextColor(hdc, oldClr);
    ::SelectObject(hdc, oldFont);
}

// =============================================================================
// TaskMgrLineChart
// =============================================================================

void TaskMgrLineChart::SetSource(const MetricRing* ring, double range)
{
    m_ring  = ring;
    m_range = (range > 0.0) ? range : 1.0;
    Invalidate();
}

void TaskMgrLineChart::SetAccent(COLORREF c)
{
    m_accent = c;
    Invalidate();
}

void TaskMgrLineChart::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    const RECT& rc = m_rcItem;
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    // ---- 背景 ----
    HBRUSH bgBr = ::CreateSolidBrush(kChartBgColor);
    ::FillRect(hdc, &rc, bgBr);
    ::DeleteObject(bgBr);

    // ---- 边框 ----
    HBRUSH borderBr = ::CreateSolidBrush(kChartBorderColor);
    ::FrameRect(hdc, &rc, borderBr);
    ::DeleteObject(borderBr);

    // 折线区域（去 padding）
    RECT inner = rc;
    inner.left   += kChartPadL;
    inner.top    += kChartPadT;
    inner.right  -= kChartPadR;
    inner.bottom -= kChartPadB;
    int chartW = inner.right - inner.left;
    int chartH = inner.bottom - inner.top;
    if (chartW <= 4 || chartH <= 4)
    {
        return;
    }

    // ---- 网格 ----
    HBRUSH gridBr = ::CreateSolidBrush(kChartGridColor);
    for (int i = 1; i < kChartGridCols; ++i)
    {
        int x = inner.left + chartW * i / kChartGridCols;
        RECT line = { x, inner.top, x + 1, inner.bottom };
        ::FillRect(hdc, &line, gridBr);
    }
    for (int i = 1; i < kChartGridRows; ++i)
    {
        int y = inner.top + chartH * i / kChartGridRows;
        RECT line = { inner.left, y, inner.right, y + 1 };
        ::FillRect(hdc, &line, gridBr);
    }
    ::DeleteObject(gridBr);

    // ---- 折线 ----
    if (m_ring && m_ring->Size() >= 2)
    {
        int sz = m_ring->Size();
        // 把数据点对齐到右侧：满 60 个时占满；不满时从右起 sz 个槽位
        int slotBase = MetricRing::kCapacity - sz;

        auto X = [&](int slot)
        {
            return inner.left + chartW * slot / (MetricRing::kCapacity - 1);
        };
        auto Y = [&](double v)
        {
            double norm = v / m_range;
            if (norm < 0.0) norm = 0.0;
            if (norm > 1.0) norm = 1.0;
            return inner.top + (int)((1.0 - norm) * chartH);
        };

        for (int i = 0; i < sz - 1; ++i)
        {
            int s1 = slotBase + i;
            int s2 = slotBase + i + 1;
            DuiAA::DrawLine(hdc, X(s1), Y(m_ring->At(i)),
                                  X(s2), Y(m_ring->At(i + 1)),
                                  m_accent, kChartLineWidth);
        }
    }

    // ---- 当前值大字（左上）----
    HFONT bigFont = MakeFont(kFontSizeBigValue, true);
    if (m_ring)
    {
        TCHAR buf[32];
        // CPU/GPU 用 % 单位，其它直接显数。简化：以量程 == 100 推断百分比；
        // 否则按"数值 + 单位"两格式显示——这里只有 Y 轴量程没有"单位"信息，
        // 简化成统一显示 "%"（CPU/GPU 准确，内存/磁盘/网络示意性）。后续可
        // 改成传 unit 参数。
        if (m_range <= 100.5 && m_range >= 99.5)
        {
            _stprintf_s(buf, _T("%.0f%%"), m_ring->Latest());
        }
        else
        {
            _stprintf_s(buf, _T("%.1f"), m_ring->Latest());
        }
        RECT textRc = { inner.left + 4, inner.top + 2,
                        inner.left + 200, inner.top + 40 };
        DrawTextLine(hdc, textRc, buf, bigFont, m_accent,
                     DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
    ::DeleteObject(bigFont);

    // ---- Y 轴 0 / 50 / 100 标注（右侧外）----
    HFONT axisFont = MakeFont(kFontSizeYAxis);
    {
        TCHAR buf[16];
        // 100%
        if (m_range <= 100.5 && m_range >= 99.5)
        {
            _tcscpy_s(buf, _T("100%"));
        }
        else
        {
            _stprintf_s(buf, _T("%.0f"), m_range);
        }
        RECT r1 = { inner.right + 2, inner.top - 6,
                    rc.right - 2,    inner.top + 12 };
        DrawTextLine(hdc, r1, buf, axisFont, kDetailSubtitleColor,
                     DT_LEFT | DT_TOP | DT_SINGLELINE);
        // 0
        RECT r0 = { inner.right + 2, inner.bottom - 12,
                    rc.right - 2,    inner.bottom + 6 };
        DrawTextLine(hdc, r0, _T("0"), axisFont, kDetailSubtitleColor,
                     DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    }
    ::DeleteObject(axisFont);
}

// =============================================================================
// TaskMgrSidebarItem
// =============================================================================

void TaskMgrSidebarItem::Configure(PerformancePage* parent,
                                   MetricKind kind,
                                   const MetricRing* ring,
                                   double range,
                                   LPCTSTR title,
                                   LPCTSTR subtitle,
                                   COLORREF accent)
{
    m_parent   = parent;
    m_kind     = kind;
    m_ring     = ring;
    m_range    = (range > 0.0) ? range : 1.0;
    m_title    = title;
    m_subtitle = subtitle;
    m_accent   = accent;
    Invalidate();
}

void TaskMgrSidebarItem::SetSelected(bool b)
{
    if (m_selected == b) { return; }
    m_selected = b;
    Invalidate();
}

bool TaskMgrSidebarItem::OnLButtonDown(POINT, UINT)
{
    if (m_parent)
    {
        m_parent->SelectMetric(m_kind);
    }
    return true;       // 消费事件（防止冒泡到 splitter 拖拽路径）
}

bool TaskMgrSidebarItem::OnSetCursor(POINT)
{
    ::SetCursor(::LoadCursor(NULL, IDC_HAND));
    return true;
}

void TaskMgrSidebarItem::OnPaint(HDC hdc, const RECT& /*rcDirty*/)
{
    const RECT& rc = m_rcItem;
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    // ---- 背景 ----
    COLORREF bg = m_selected ? kSidebarBgSelected
                : (IsHover()  ? kSidebarBgHover  : kSidebarBgNormal);
    HBRUSH bgBr = ::CreateSolidBrush(bg);
    ::FillRect(hdc, &rc, bgBr);
    ::DeleteObject(bgBr);

    // ---- 选中态 accent 左侧竖条 ----
    if (m_selected)
    {
        RECT bar = { rc.left, rc.top, rc.left + kSidebarSelBarW, rc.bottom };
        HBRUSH barBr = ::CreateSolidBrush(m_accent);
        ::FillRect(hdc, &bar, barBr);
        ::DeleteObject(barBr);
    }

    // 内容区
    int leftStart = rc.left + (m_selected ? kSidebarSelBarW + 8 : 12);
    int rightStop = rc.right - 8;

    // ---- 上半行：accent 圆点 + 标题（左） + 当前值百分比（右） ----
    int rowMid = rc.top + (rc.bottom - rc.top) / 2;
    int dotSize = 8;
    {
        RECT dot = { leftStart, rowMid - dotSize - 2,
                     leftStart + dotSize, rowMid - 2 };
        DuiAA::FillEllipse(hdc, dot, m_accent);
    }
    // 标题
    HFONT titleFont = MakeFont(kFontSizeSidebarTitle, m_selected);
    {
        RECT tr = { leftStart + dotSize + 6, rc.top + 8,
                    rightStop - kSidebarValueColW, rowMid };
        DrawTextLine(hdc, tr, m_title.GetString(), titleFont,
                     kSidebarTitleColor, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
    ::DeleteObject(titleFont);

    // 当前值（右上）—— 字号 18pt 加粗，按 Win10 sidebar 视觉权重让"当前
    // 值"成为最显眼的字段。
    if (m_ring)
    {
        TCHAR buf[16];
        if (m_range <= 100.5 && m_range >= 99.5)
        {
            _stprintf_s(buf, _T("%.0f%%"), m_ring->Latest());
        }
        else
        {
            _stprintf_s(buf, _T("%.1f"), m_ring->Latest());
        }
        HFONT vf = MakeFont(kFontSizeSidebarValue, /*bold=*/true);
        RECT vr = { rightStop - kSidebarValueColW, rc.top + 6,
                    rightStop, rowMid + 4 };
        DrawTextLine(hdc, vr, buf, vf, kSidebarValueColor,
                     DT_RIGHT | DT_TOP | DT_SINGLELINE);
        ::DeleteObject(vf);
    }

    // ---- 下半行：副标题（左） + mini sparkline（右） ----
    HFONT subFont = MakeFont(kFontSizeSidebarSubtitle);
    {
        RECT sr = { leftStart + dotSize + 6, rowMid + 2,
                    rightStop - kSparklineW - 8, rc.bottom - 6 };
        DrawTextLine(hdc, sr, m_subtitle.GetString(), subFont,
                     kSidebarSubtitleColor,
                     DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    ::DeleteObject(subFont);

    // mini sparkline
    if (m_ring && m_ring->Size() >= 2)
    {
        RECT sp = { rightStop - kSparklineW, rowMid + 4,
                    rightStop, rowMid + 4 + kSparklineH };
        // 浅边框（视觉提示这是一个图）
        HBRUSH bb = ::CreateSolidBrush(kChartGridColor);
        ::FrameRect(hdc, &sp, bb);
        ::DeleteObject(bb);

        int sz = m_ring->Size();
        int slotBase = MetricRing::kCapacity - sz;
        int spW = sp.right - sp.left;
        int spH = sp.bottom - sp.top;

        auto X = [&](int slot)
        {
            return sp.left + spW * slot / (MetricRing::kCapacity - 1);
        };
        auto Y = [&](double v)
        {
            double norm = v / m_range;
            if (norm < 0.0) norm = 0.0;
            if (norm > 1.0) norm = 1.0;
            return sp.top + (int)((1.0 - norm) * spH);
        };
        for (int i = 0; i < sz - 1; ++i)
        {
            int s1 = slotBase + i;
            int s2 = slotBase + i + 1;
            DuiAA::DrawLine(hdc, X(s1), Y(m_ring->At(i)),
                                  X(s2), Y(m_ring->At(i + 1)),
                                  m_accent, 1.0f);
        }
    }
}

// =============================================================================
// PerformancePage
// =============================================================================

PerformancePage::PerformancePage()
{
    SetOrientation(DuiSplitter::Vertical);
    SetSplitPx(220);          // sidebar 默认宽度
    SetMinSizes(160, 320);
    SetBarThickness(2);
}

void PerformancePage::OnPaint(HDC hdc, const RECT& rcDirty)
{
    // detail 区（pane 1）填白底，让 chart / 标题 / stats 叠在白底上 ——
    // sidebar（pane 0）保留 host 浅灰背景，整页"sidebar 灰 + detail 白"
    // 分层与 Win10 任务管理器一致。
    if (DuiControl* p1 = GetPane(1))
    {
        const RECT& r = p1->GetRect();
        if (r.right > r.left && r.bottom > r.top)
        {
            HBRUSH br = ::CreateSolidBrush(RGB(255, 255, 255));
            ::FillRect(hdc, &r, br);
            ::DeleteObject(br);
        }
    }
    DuiSplitter::OnPaint(hdc, rcDirty);
}

// 元数据表。getRing 返回 Metrics 里对应字段的引用，避免在调用点写 switch。
namespace {

const MetricRing& GetRingCpu (const Metrics& m) { return m.cpu;  }
const MetricRing& GetRingMem (const Metrics& m) { return m.mem;  }
const MetricRing& GetRingDisk(const Metrics& m) { return m.disk; }
const MetricRing& GetRingNet (const Metrics& m) { return m.net;  }
const MetricRing& GetRingGpu (const Metrics& m) { return m.gpu;  }

} // anonymous

const PerformancePage::MetricInfo& PerformancePage::GetInfo(MetricKind k)
{
    // 元数据按 enum 顺序排好，O(1) 索引。subtitle 是 mock 设备型号，纯
    // 装饰；后续可改为读 OS 信息（仍不影响 demo 主线）。
    static const MetricInfo kInfos[(int)MetricKind::kMetricCount] = {
        { _T("CPU"),    _T("Intel Core i7-8700K @ 3.70 GHz"),
          RGB(45, 108, 223), 100.0, GetRingCpu  },
        { _T("内存"),   _T("16.0 GB DDR4 @ 2666 MHz"),
          RGB(140, 80, 180),  16.0, GetRingMem  },
        { _T("磁盘 0"), _T("Samsung SSD 970 EVO 1TB"),
          RGB(50, 160, 90),  100.0, GetRingDisk },
        { _T("以太网"), _T("Realtek PCIe GbE Family Controller"),
          RGB(80, 80, 90),   100.0, GetRingNet  },
        { _T("GPU 0"),  _T("NVIDIA GeForce GTX 1660"),
          RGB(220, 160, 30), 100.0, GetRingGpu  },
    };
    return kInfos[(int)k];
}

void PerformancePage::Init()
{
    // ---- pane 0: sidebar ----
    auto sidebar = std::unique_ptr<DuiVBox>(new DuiVBox());
    sidebar->SetPadding(0, 4, 0, 0);
    sidebar->SetGap(6);

    const auto& metrics = GetMetrics();
    for (int i = 0; i < (int)MetricKind::kMetricCount; ++i)
    {
        auto kind = (MetricKind)i;
        const MetricInfo& info = GetInfo(kind);
        auto item = std::unique_ptr<TaskMgrSidebarItem>(new TaskMgrSidebarItem());
        m_items[i] = item.get();
        item->Configure(this, kind, &info.getRing(metrics), info.range,
                        info.title, info.subtitle, info.accent);
        sidebar->AddChild(std::move(item),
                          DuiLayout::Hint().Fixed(kSidebarItemH));
    }
    // 底部填充空白
    sidebar->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
    SetPane(0, std::move(sidebar));

    // ---- pane 1: detail ----
    auto detail = std::unique_ptr<DuiVBox>(new DuiVBox());
    detail->SetPadding(16, 12, 16, 12);
    detail->SetGap(6);

    {
        auto t = std::unique_ptr<DuiLabel>(new DuiLabel());
        t->SetTextColor(kDetailTitleColor);
        m_lblTitle = t.get();
        detail->AddChild(std::move(t), DuiLayout::Hint().Fixed(28));
    }
    {
        auto s = std::unique_ptr<DuiLabel>(new DuiLabel());
        s->SetTextColor(kDetailSubtitleColor);
        m_lblSubtitle = s.get();
        detail->AddChild(std::move(s), DuiLayout::Hint().Fixed(18));
    }
    {
        auto chart = std::unique_ptr<TaskMgrLineChart>(new TaskMgrLineChart());
        m_chart = chart.get();
        detail->AddChild(std::move(chart), DuiLayout::Hint().Weight(1));
    }
    {
        auto stats = std::unique_ptr<DuiHBox>(new DuiHBox());
        stats->SetPadding(0, 8, 0, 0);
        stats->SetGap(20);
        for (int i = 0; i < 4; ++i)
        {
            auto lbl = std::unique_ptr<DuiLabel>(new DuiLabel());
            lbl->SetTextColor(kDetailStatColor);
            m_lblStats[i] = lbl.get();
            stats->AddChild(std::move(lbl), DuiLayout::Hint().Weight(1));
        }
        detail->AddChild(std::move(stats), DuiLayout::Hint().Fixed(56));
    }

    SetPane(1, std::move(detail));

    // 默认选中 CPU
    SelectMetric(MetricKind::CPU);
}

void PerformancePage::SelectMetric(MetricKind k)
{
    m_selected = k;
    for (int i = 0; i < (int)MetricKind::kMetricCount; ++i)
    {
        if (m_items[i])
        {
            m_items[i]->SetSelected((MetricKind)i == k);
        }
    }
    ApplyDetailFor(k);
}

void PerformancePage::ApplyDetailFor(MetricKind k)
{
    const MetricInfo& info = GetInfo(k);
    const auto&       m    = GetMetrics();

    if (m_lblTitle)    { m_lblTitle->SetText(info.title); }
    if (m_lblSubtitle) { m_lblSubtitle->SetText(info.subtitle); }
    if (m_chart)
    {
        m_chart->SetSource(&info.getRing(m), info.range);
        m_chart->SetAccent(info.accent);
    }
    RefreshDetailStats();
}

void PerformancePage::RefreshDetailStats()
{
    const auto& m = GetMetrics();
    TCHAR a[64], b[64], c[64], d[64];

    // 4 列 stats 的内容随 metric 变。Win10 性能页每个 metric 的 4 列指标
    // 都不一样；下表用 mock 但形态与真任务管理器一致。
    switch (m_selected)
    {
    case MetricKind::CPU:
        _stprintf_s(a, _T("利用率\n%.0f%%"),       m.cpuTotalPct);
        _stprintf_s(b, _T("速度\n%s"),             _T("3.70 GHz"));
        _stprintf_s(c, _T("进程\n%d"),             m.procCount);
        _stprintf_s(d, _T("线程\n%s"),             _T("2845"));
        break;

    case MetricKind::Memory:
        _stprintf_s(a, _T("已用\n%.1f GB"),        m.memUsedGB);
        _stprintf_s(b, _T("可用\n%.1f GB"),        m.memTotalGB - m.memUsedGB);
        _stprintf_s(c, _T("已提交\n%.1f / %.1f"),  m.memUsedGB + 0.5,
                                                   m.memTotalGB);
        _stprintf_s(d, _T("已缓存\n%.1f GB"),      0.4 + m.memUsedGB * 0.1);
        break;

    case MetricKind::Disk:
        _stprintf_s(a, _T("活动时间\n%.0f%%"),     m.disk.Latest());
        _stprintf_s(b, _T("读取\n%.1f MB/秒"),     m.disk.Latest() * 0.05);
        _stprintf_s(c, _T("写入\n%.1f MB/秒"),     m.disk.Latest() * 0.03);
        _stprintf_s(d, _T("响应\n%s"),             _T("3 ms"));
        break;

    case MetricKind::Network:
        _stprintf_s(a, _T("发送\n%.2f Mbps"),      m.net.Latest() * 0.4);
        _stprintf_s(b, _T("接收\n%.2f Mbps"),      m.net.Latest() * 0.6);
        _stprintf_s(c, _T("适配器\n%s"),           _T("Ethernet"));
        _stprintf_s(d, _T("连接\n%s"),             _T("1 Gbps"));
        break;

    case MetricKind::GPU:
        _stprintf_s(a, _T("利用率\n%.0f%%"),       m.gpu.Latest());
        _stprintf_s(b, _T("专用内存\n%.1f / 4.0 GB"),
                                                   1.0 + m.gpu.Latest() * 0.02);
        _stprintf_s(c, _T("共享内存\n%.1f / 8.0 GB"),
                                                   0.4 + m.gpu.Latest() * 0.01);
        _stprintf_s(d, _T("驱动版本\n%s"),         _T("27.21.14"));
        break;

    default:
        a[0] = b[0] = c[0] = d[0] = 0;
        break;
    }

    if (m_lblStats[0]) { m_lblStats[0]->SetText(a); }
    if (m_lblStats[1]) { m_lblStats[1]->SetText(b); }
    if (m_lblStats[2]) { m_lblStats[2]->SetText(c); }
    if (m_lblStats[3]) { m_lblStats[3]->SetText(d); }
}

void PerformancePage::OnTick()
{
    // chart：整图重画
    if (m_chart)
    {
        m_chart->Invalidate();
    }
    // 5 个 sidebar item：Latest 在动 + sparkline 在动，全部重画
    for (int i = 0; i < (int)MetricKind::kMetricCount; ++i)
    {
        if (m_items[i])
        {
            m_items[i]->Invalidate();
        }
    }
    // detail 4 列 stats label
    RefreshDetailStats();
}

} // namespace demotaskmgr
