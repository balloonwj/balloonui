// =============================================================================
// DemoTreeViewLargeData.exe —— DuiTreeView 多列模式大数据量渲染压力 demo
//
// 用途：验证 PaintBody 的 dirty-rect 行裁剪生效后，几千 / 几万行的滚动是否
// 仍然流畅。窗口顶部一排按钮可以一键插入 1k / 10k / 30k 行，下方状态条
// 实时显示行数 + 最近一帧 OnPaint 耗时（毫秒）+ 滚动一段时间内的平均 / 峰值
// 帧时。
//
// 注意：插入耗时本身受 RebuildVisible 的 O(N) 限制，"插 30k 行"按一次大约
// 卡 8~12 秒（这是另一个待优化点，跟本 demo 要测的渲染开销无关）。本 demo
// 测的是<u>插完之后用滚轮 / 拖滚动条时</u>的每帧重绘耗时。
// =============================================================================

#include "stdafx.h"

#include "DuiDpi.h"
#include "DuiHost.h"
#include "DuiNotify.h"
#include "Controls/Layout/DuiLayout.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Basic/DuiButton.h"
#include "Controls/List/DuiTreeView.h"
#include "Controls/Window/DuiFrameWindow.h"

#include <atlcrack.h>

using namespace balloonwjui;

CAppModule _Module;

// -----------------------------------------------------------------------------
// 控件 ctrl id：DuiButton / DuiTreeView / DuiLabel 的 id 不能与 DuiFrameWindow
// 标题栏三按钮 (1/2/3) 冲突，所以从 1001 起。
// -----------------------------------------------------------------------------
static const UINT kIdBtnInsert1k    = 1001;
static const UINT kIdBtnInsert10k   = 1002;
static const UINT kIdBtnInsert30k   = 1003;
static const UINT kIdBtnClear       = 1004;
static const UINT kIdBtnExpandAll   = 1005;
static const UINT kIdBtnCollapseAll = 1006;
static const UINT kIdTree           = 2000;
static const UINT kIdLblStatus      = 3000;

// 200ms 刷一次状态行；快到能看出滚动节奏，慢到不打扰主线程。
static const UINT kTimerRefreshStatus    = 1;
static const UINT kRefreshStatusPeriodMs = 200;

// 状态行环形缓冲，记录最近 60 帧 OnPaint 耗时；多于这个数旧值被覆盖。
static const int  kPaintRingSize         = 60;

// 一次性插入 30k 行的上限：再多的话 RebuildVisible O(N) 会让插入端等更久；
// demo 只是测渲染，不必把插入也压到极限。
static const int  kMaxInsertChunk        = 30000;

// -----------------------------------------------------------------------------
// TimedTreeView：在 DuiTreeView::OnPaint 外面包一圈高精度计时，方便读出
// 每帧 PaintBody 的开销。
// -----------------------------------------------------------------------------
class TimedTreeView : public DuiTreeView
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        LARGE_INTEGER t0, t1, freq;
        ::QueryPerformanceFrequency(&freq);
        ::QueryPerformanceCounter(&t0);
        DuiTreeView::OnPaint(hdc, rcDirty);
        ::QueryPerformanceCounter(&t1);

        double ms = static_cast<double>(t1.QuadPart - t0.QuadPart)
                    * 1000.0 / static_cast<double>(freq.QuadPart);

        m_paintMs[m_paintHead % kPaintRingSize] = ms;
        ++m_paintHead;
        m_lastPaintMs   = ms;
        m_lastDirtyH    = rcDirty.bottom - rcDirty.top;
    }

    // 平均 / 峰值（取环形缓冲里所有有效样本）。无样本时返回 0。
    void GetStats(double& avgMs, double& peakMs, int& sampleCount) const
    {
        int n = (m_paintHead < kPaintRingSize) ? m_paintHead : kPaintRingSize;
        sampleCount = n;
        if (n == 0)
        {
            avgMs  = 0.0;
            peakMs = 0.0;
            return;
        }
        double sum  = 0.0;
        double peak = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double v = m_paintMs[i];
            sum += v;
            if (v > peak) { peak = v; }
        }
        avgMs  = sum / n;
        peakMs = peak;
    }

    double m_lastPaintMs = 0.0;
    int    m_lastDirtyH  = 0;

private:
    double m_paintMs[kPaintRingSize] = {};
    int    m_paintHead               = 0;
};

// -----------------------------------------------------------------------------
// 主窗口：顶部按钮条 + 状态行 + 占满剩余的多列 DuiTreeView。
// -----------------------------------------------------------------------------
class MainFrame : public DuiFrameWindow
{
public:
    BEGIN_MSG_MAP(MainFrame)
        MESSAGE_HANDLER(WM_DESTROY,     OnDestroy_)
        MESSAGE_HANDLER(WM_TIMER,       OnTimer_)
        MESSAGE_HANDLER(WM_DUI_NOTIFY,  OnDuiNotify_)
        CHAIN_MSG_MAP(DuiFrameWindow)
    END_MSG_MAP()

    // 库里 DuiFrameWindow 不主动 PostQuitMessage（一个进程可能多 frame，
    // 关一个不等于退出），单 frame 进程必须自己把"主 frame 关闭"翻译成
    // "消息循环退出"。
    LRESULT OnDestroy_(UINT, WPARAM, LPARAM, BOOL& bHandled)
    {
        ::PostQuitMessage(0);
        bHandled = FALSE;
        return 0;
    }

    void BuildContent()
    {
        auto page = std::unique_ptr<DuiVBox>(new DuiVBox());
        page->SetPadding(8);
        page->SetGap(6);

        // ---- 按钮条 ----
        auto bar = std::unique_ptr<DuiHBox>(new DuiHBox());
        bar->SetGap(8);

        auto addBtn = [&](UINT id, LPCTSTR text, int widthPx) {
            auto b = std::unique_ptr<DuiButton>(new DuiButton());
            b->SetText(text);
            b->SetCtrlId(id);
            bar->AddChild(std::move(b), DuiLayout::Hint().Fixed(widthPx));
        };
        addBtn(kIdBtnInsert1k,    _T("插入 1k 行"),   100);
        addBtn(kIdBtnInsert10k,   _T("插入 10k 行"),  100);
        addBtn(kIdBtnInsert30k,   _T("插入 30k 行"),  100);
        addBtn(kIdBtnClear,       _T("清空"),          70);
        addBtn(kIdBtnExpandAll,   _T("全展开"),        80);
        addBtn(kIdBtnCollapseAll, _T("全折叠"),        80);
        // 右侧空白占位，按钮靠左
        bar->AddChild(std::unique_ptr<DuiControl>(new DuiControl()),
                      DuiLayout::Hint().Weight(1));
        page->AddChild(std::move(bar), DuiLayout::Hint().Fixed(32));

        // ---- 状态行 ----
        auto lblStatus = std::unique_ptr<DuiLabel>(new DuiLabel());
        lblStatus->SetText(_T("行数: 0  |  上次 OnPaint: -- ms  |  最近 60 帧平均/峰值: -- / -- ms"));
        lblStatus->SetTextColor(RGB(60, 60, 70));
        lblStatus->SetCtrlId(kIdLblStatus);
        m_status = lblStatus.get();
        page->AddChild(std::move(lblStatus), DuiLayout::Hint().Fixed(22));

        // ---- 多列 TreeView ----
        auto tree = std::unique_ptr<TimedTreeView>(new TimedTreeView());
        tree->SetCtrlId(kIdTree);
        tree->AddColumn(_T("名称"), 280);
        tree->AddColumn(_T("大小"),  90, 40, DT_RIGHT);
        tree->AddColumn(_T("完成"),  70, 40, DT_CENTER);
        tree->AddColumn(_T("进度"), 140);
        tree->AddColumn(_T("链接"), 200);
        tree->SetFrozenColumns(1);
        tree->SetZebra(true);
        m_tree = tree.get();
        page->AddChild(std::move(tree), DuiLayout::Hint().Weight(1));

        SetClientContent(std::move(page));
    }

    LRESULT OnDuiNotify_(UINT, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        const DuiNotify* n = reinterpret_cast<const DuiNotify*>(lParam);
        UINT ctrlId = static_cast<UINT>(wParam);
        if (n && n->code == DUIN_CLICK && m_tree)
        {
            switch (ctrlId)
            {
            case kIdBtnInsert1k:    DoInsert(1000);                 break;
            case kIdBtnInsert10k:   DoInsert(10000);                break;
            case kIdBtnInsert30k:   DoInsert(kMaxInsertChunk);      break;
            case kIdBtnClear:       m_tree->Clear();                break;
            case kIdBtnExpandAll:   m_tree->ExpandAll();            break;
            case kIdBtnCollapseAll: m_tree->CollapseAll();          break;
            default:                                                break;
            }
            UpdateStatus(_T(""));
        }
        bHandled = FALSE;       // 让 DuiFrameWindow::OnDuiChildNotify 继续处理 caption 钮
        return 0;
    }

    LRESULT OnTimer_(UINT, WPARAM wp, LPARAM, BOOL& bHandled)
    {
        if (wp == kTimerRefreshStatus)
        {
            UpdateStatus(_T(""));
        }
        bHandled = FALSE;
        return 0;
    }

private:
    // 一次插入 count 行：分两个根节点 "Group A" "Group B"，子节点按 i 平摊。
    // 用 5 列把所有 cell 类型都覆盖到（Text / 进度条 / 复选框 / 超链接）。
    void DoInsert(int count)
    {
        if (count <= 0 || !m_tree) { return; }

        // 找一个新的根，避免和已存在的根重名混淆
        TCHAR rootName[64];
        _stprintf_s(rootName, _T("批次 #%d (%d 行)"), m_batchIndex++, count);
        int rootId = m_tree->AddRoot(rootName);

        // 5 列含义：col0 = 名称 (col0)；col1 = 大小 (Text)；col2 = 完成 (CheckBox)；
        //           col3 = 进度 (ProgressBar)；col4 = 链接 (Hyperlink)
        const int kColSize     = 1;
        const int kColDone     = 2;
        const int kColProgress = 3;
        const int kColLink     = 4;

        // 每 200 行打一个子组，方便手动展开 / 折叠看效果
        const int kPerGroup = 200;
        int groupId = -1;

        for (int i = 0; i < count; ++i)
        {
            if (i % kPerGroup == 0)
            {
                TCHAR g[64];
                _stprintf_s(g, _T("子组 %d-%d"),
                            i, (i + kPerGroup - 1 < count ? i + kPerGroup - 1 : count - 1));
                groupId = m_tree->AddChild(rootId, g);
            }

            TCHAR name[64];
            _stprintf_s(name, _T("item-%07d.dat"), i);
            int nodeId = m_tree->AddChild(groupId, name);

            TCHAR sz[32];
            _stprintf_s(sz, _T("%d KB"), (i % 9999) + 1);
            m_tree->SetCellText(nodeId, kColSize, sz);

            m_tree->SetCellChecked(nodeId, kColDone, (i & 1) == 0);
            m_tree->SetCellValue  (nodeId, kColProgress, i % 101);
            m_tree->SetCellLink   (nodeId, kColLink,
                                   _T("打开"), _T("https://example.com/x"));
        }
    }

    // 拼状态行；prefix 非空时贴在最前面。
    void UpdateStatus(LPCTSTR prefix)
    {
        if (!m_status || !m_tree) { return; }
        double avg = 0.0, peak = 0.0;
        int n = 0;
        m_tree->GetStats(avg, peak, n);

        TCHAR buf[256];
        _stprintf_s(buf,
                    _T("%s行数: %d  |  上次 OnPaint: %.2f ms (脏区高 %d px)")
                    _T("  |  最近 %d 帧平均/峰值: %.2f / %.2f ms"),
                    prefix ? prefix : _T(""),
                    m_tree->GetVisibleCount(),
                    m_tree->m_lastPaintMs,
                    m_tree->m_lastDirtyH,
                    n, avg, peak);
        m_status->SetText(buf);
    }

    TimedTreeView* m_tree   = nullptr;
    DuiLabel*      m_status = nullptr;
    int            m_batchIndex = 1;
};

// -----------------------------------------------------------------------------
// 入口
// -----------------------------------------------------------------------------
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR, int nCmdShow)
{
    DuiDpi::OptInPerMonitorV2();
    ::OleInitialize(NULL);
    AtlInitCommonControls(ICC_BAR_CLASSES);
    _Module.Init(NULL, hInst);

    int ret = 0;
    {
        MainFrame frame;
        frame.SetTitle(_T("DemoTreeViewLargeData — 多列 DuiTreeView 大数据渲染压测"));
        frame.SetButtons(true, true, true);
        if (!frame.Create(NULL, CWindow::rcDefault,
                          _T("DemoTreeViewLargeData"),
                          WS_OVERLAPPEDWINDOW, 0))
        {
            _Module.Term();
            ::OleUninitialize();
            return 0;
        }
        // 加载 app.ico (IDI_APP=100，棕色 "L")
        if (HICON hI = (HICON)::LoadImage(_Module.GetModuleInstance(),
                MAKEINTRESOURCE(100), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR))
        {
            ::SendMessage(frame.m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hI);
            ::SendMessage(frame.m_hWnd, WM_SETICON, ICON_BIG,   (LPARAM)hI);
            ICONINFO ii = {};
            if (::GetIconInfo(hI, &ii))
            {
                if (ii.hbmMask) { ::DeleteObject(ii.hbmMask); }
                frame.SetIcon(ii.hbmColor);
            }
        }
        frame.BuildContent();
        frame.ResizeClient(960, 600);
        frame.CenterWindow();
        frame.ShowWindow(nCmdShow);
        frame.UpdateWindow();
        frame.SetTimer(kTimerRefreshStatus, kRefreshStatusPeriodMs);

        MSG msg;
        while (::GetMessage(&msg, NULL, 0, 0))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        ret = static_cast<int>(msg.wParam);
    }

    _Module.Term();
    ::OleUninitialize();
    return ret;
}
