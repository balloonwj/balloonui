#include "stdafx.h"
#include "LoginFrame.h"

#include "DuiAnimation.h"
#include "DuiHost.h"
#include "DuiPaintAA.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Layout/DuiLayout.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace balloonwjui;

namespace xchat {

// ---- 常量（颜色 / 时序 / 字号） ----------------------------------------------

// 某信品牌绿。设置页"消息免打扰"开关、登录页"扫码登录" / spinner / "正在
// 进入"全用同一个色，保持品牌一致。
static const COLORREF kBrandGreenRgb = RGB(7, 193, 96);

// "仅传输文件" 链接色 —— 取某信 Web 标准蓝；不是品牌主色，所以单独命名。
static const COLORREF kLinkBlueRgb = RGB(57, 97, 222);

// 状态文字（"扫码登录" / "正在进入"）字号。比标题栏（默认 9pt）大一点
// 以引导视线，参照截图实测 ~14pt。
static const int kStatusFontSize = 14;

// "仅传输文件"小字 —— 截图里比状态文字明显小，约 10pt。
static const int kLinkFontSize = 10;

// 每个 mode 显示后多久关掉自己（ms）。3 秒分别模拟"用户在手机上点确认"
// 和"server 下发用户资料 + 好友列表"的过渡时间。同一份时长两个模式共
// 用，main.cpp 串行起 2 个 LoginFrame。
static const UINT_PTR kTimerCloseSelf = 0x1A;
static const UINT     kCloseSelfMs   = 3000;

// =============================================================================
// 工具：用 GDI+ 加载 PNG 文件成 HBITMAP（32bpp ARGB）。
// 失败返 nullptr，调用方 Caller 自己 ::DeleteObject。
// =============================================================================
namespace {

HBITMAP LoadPngAsHBitmap(LPCTSTR path)
{
    Gdiplus::Bitmap bmp(path);
    if (bmp.GetLastStatus() != Gdiplus::Ok)
    {
        return nullptr;
    }
    HBITMAP hbm = nullptr;
    bmp.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbm);
    return hbm;
}

// 把"相对 exe 所在目录"的资源路径解析成绝对路径。和 DuiXmlBuilder::
// ResolveAssetPath 同语义，但本 demo 里部分场景在 main 启动前 / XML 解
// 析外面也要用，所以保留独立 helper。
CString ResolveResPath(LPCTSTR rel)
{
    TCHAR exePath[MAX_PATH] = {};
    ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    CString dir = exePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0)
    {
        dir = dir.Left(slash);
    }
    // exe 在 third_party/Bin/，资源在 third_party/XChat/res/
    CString a = dir + _T("\\res\\") + rel;
    if (::GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES)
    {
        return a;
    }
    CString b = dir + _T("\\..\\XChat\\res\\") + rel;
    if (::GetFileAttributes(b) != INVALID_FILE_ATTRIBUTES)
    {
        return b;
    }
    CString c = dir + _T("\\..\\..\\XChat\\res\\") + rel;
    if (::GetFileAttributes(c) != INVALID_FILE_ATTRIBUTES)
    {
        return c;
    }
    return CString();
}

// 把 "r,g,b" 字符串解析成 COLORREF；fallback 是默认色。本地 helper
// 避免和 DuiXmlBuilder 内的 ParseColor 名字冲突（那边在匿名 namespace）。
COLORREF ParseRgbAttr(const std::string& s, COLORREF def)
{
    int r = 0, g = 0, b = 0;
    if (sscanf_s(s.c_str(), "%d,%d,%d", &r, &g, &b) == 3)
    {
        return RGB(r, g, b);
    }
    return def;
}

// =============================================================================
// QrImage —— 自定义 DUI 控件，绘制一张静态 PNG 到 m_rcItem。
//
// 用途：登录窗 QR view 中央的占位二维码图。所有权约定：HBITMAP 由控件
// 析构时 DeleteObject。
// =============================================================================
class QrImage : public DuiControl
{
public:
    explicit QrImage(HBITMAP hbm) : m_hbm(hbm) {}
    ~QrImage() override
    {
        if (m_hbm)
        {
            ::DeleteObject(m_hbm);
        }
    }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_hbm)
        {
            return;
        }
        BITMAP bm = {};
        ::GetObject(m_hbm, sizeof(bm), &bm);
        if (bm.bmWidth <= 0 || bm.bmHeight <= 0)
        {
            return;
        }
        HDC mem = ::CreateCompatibleDC(hdc);
        HGDIOBJ old = ::SelectObject(mem, m_hbm);
        int w = m_rcItem.right - m_rcItem.left;
        int h = m_rcItem.bottom - m_rcItem.top;
        ::SetStretchBltMode(hdc, HALFTONE);
        ::SetBrushOrgEx(hdc, 0, 0, nullptr);
        ::StretchBlt(hdc, m_rcItem.left, m_rcItem.top, w, h,
                     mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        ::SelectObject(mem, old);
        ::DeleteDC(mem);
    }

private:
    HBITMAP m_hbm = nullptr;
};

// =============================================================================
// Spinner —— 旋转圆环 spinner 控件。
//
// 视觉：在 m_rcItem 中央画一个 240° 的开环圆弧，端点圆角；起始角度 = 当前
// 进程时间 / 4 mod 360（约每秒转 90°）。配合品牌绿色，匹配某信"正在进入"
// 截图。
//
// 驱动：DuiAnimMgr 的进程级 60Hz pulse —— 注册一个超长 anim（1 小时），
// OnTick 里只读 GetTickCount 计算相位，不在乎 anim 的 from/to 值；mgr
// 在 frame 析构（Clear）时一次性扔掉，spinner 不会泄露 anim。
// =============================================================================
class Spinner : public DuiControl
{
public:
    Spinner()
    {
        m_token = std::make_shared<Token>();
        m_token->alive = true;
        m_token->owner = this;

        auto token = m_token;
        // 1 小时长 anim 当 60Hz heartbeat 用，参考 DuiGifControl 的
        // GifTickAnim 思路（balloonui/Controls/Media/DuiGif.cpp:226 附近）。
        // OnTick 不用 from/to 插值，只把进程 tick 折算成相位。
        auto a = std::unique_ptr<DuiDoubleAnim>(new DuiDoubleAnim(
            60 * 60 * 1000, 0.0, 1.0,
            [token](double /*v*/)
            {
                if (token->alive && token->owner)
                {
                    token->owner->m_phaseDeg = (int)((::GetTickCount() / 4) % 360);
                    token->owner->Invalidate();
                }
            }));
        DuiAnimMgr::Inst().Add(std::move(a));
    }

    ~Spinner() override
    {
        if (m_token)
        {
            m_token->alive = false;
            m_token->owner = nullptr;
        }
    }

    void SetColor (COLORREF c) { m_color = c; Invalidate(); }
    void SetThickness(int px)  { m_thicknessPx = px > 1 ? px : 1; Invalidate(); }

    void OnPaint(HDC hdc, const RECT&) override
    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        int w = m_rcItem.right - m_rcItem.left;
        int h = m_rcItem.bottom - m_rcItem.top;
        int side = (w < h) ? w : h;
        if (side < 8) { return; }
        // 留 1px 内缩，避免抗锯齿描边被裁切。
        int cx = (m_rcItem.left + m_rcItem.right) / 2;
        int cy = (m_rcItem.top  + m_rcItem.bottom) / 2;
        int r  = (side / 2) - 1;
        if (r < 4) { r = 4; }

        Gdiplus::Pen pen(
            Gdiplus::Color(255,
                           GetRValue(m_color),
                           GetGValue(m_color),
                           GetBValue(m_color)),
            (Gdiplus::REAL)m_thicknessPx);
        // 端点圆角 —— 让 240° 弧的两端看起来更柔和。
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap  (Gdiplus::LineCapRound);

        // GDI+ DrawArc 的 0° 是 3 点钟方向，正方向是顺时针。
        g.DrawArc(&pen,
                  Gdiplus::REAL(cx - r), Gdiplus::REAL(cy - r),
                  Gdiplus::REAL(r * 2),  Gdiplus::REAL(r * 2),
                  (Gdiplus::REAL)m_phaseDeg,
                  240.0f);
    }

private:
    struct Token { Spinner* owner = nullptr; bool alive = false; };
    std::shared_ptr<Token> m_token;

    int      m_phaseDeg    = 0;
    int      m_thicknessPx = 2;
    COLORREF m_color       = kBrandGreenRgb;
};

// =============================================================================
// Custom factory：把 XML 里 3 个自定义 tag 翻译成对应控件实例。
// =============================================================================
DuiXmlBuilder::CustomFactory MakeLoginXmlFactoryImpl()
{
    return [](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
    {
        // <control> = invisible spacer。balloonui builder 没有内置 spacer
        // tag（vbox/hbox 的 weight 是布局机制，但需要一个挂载点；空 label
        // 占位会浪费 paint 时间）。给一个裸 DuiControl 实例做占位，weight
        // / fixedWidth 由 ApplyCommon 接住。
        if (n.tag == "control")
        {
            return std::unique_ptr<DuiControl>(new DuiControl());
        }
        if (n.tag == "qr-image")
        {
            // src 属性指资源相对路径；缺省走 qr_placeholder.png。
            std::string src = "qr_placeholder.png";
            auto it = n.attrs.find("src");
            if (it != n.attrs.end() && !it->second.empty())
            {
                src = it->second;
            }
            CString relW(src.c_str());
            CString abs = ResolveResPath(relW);
            HBITMAP hbm = abs.IsEmpty() ? nullptr : LoadPngAsHBitmap(abs);
            return std::unique_ptr<DuiControl>(new QrImage(hbm));
        }
        if (n.tag == "spinner")
        {
            auto sp = std::unique_ptr<Spinner>(new Spinner());
            auto it = n.attrs.find("color");
            if (it != n.attrs.end())
            {
                sp->SetColor(ParseRgbAttr(it->second, kBrandGreenRgb));
            }
            it = n.attrs.find("thickness");
            if (it != n.attrs.end())
            {
                sp->SetThickness(atoi(it->second.c_str()));
            }
            return sp;
        }
        if (n.tag == "login-avatar")
        {
            // 用 DuiAvatar 走"圆角矩形 + 缩写 fallback"路径，免去再加一张
            // PNG 资源（截图里头像是用户照片，demo 里用品牌绿首字母占位
            // 也能看出"已登录态"的语义）。
            auto av = std::unique_ptr<DuiAvatar>(new DuiAvatar());
            av->SetShape(DuiAvatar::ShapeRoundRect);
            av->SetCornerRadius(10);
            av->SetName(_T("Balloonwj"));
            av->SetFallbackBgColor(kBrandGreenRgb);
            return av;
        }
        return nullptr;     // 未知 tag → 让 builder 走默认 miss 路径
    };
}

} // anonymous

DuiXmlBuilder::CustomFactory MakeLoginXmlFactory()
{
    return MakeLoginXmlFactoryImpl();
}

// =============================================================================
// XML 兜底（找不到 login.xml / login_loading.xml 时用，保证窗口至少能起来）。
// /utf-8 编译开关让源码里中文按 UTF-8 落盘，无需 \xXX 转义。
// =============================================================================
static const char* kFallbackQrXmlUtf8 =
    "<frame-window title=\"XChat\""
    " min-w=\"360\" min-h=\"500\""
    " has-min-button=\"false\" has-max-button=\"false\" has-close-button=\"true\""
    " resizable=\"false\">"
    "  <vbox padding=\"0,30,0,30\" gap=\"0\">"
    "    <hbox weight=\"1\">"
    "      <control weight=\"1\"/>"
    "      <qr-image fixedWidth=\"210\" fixedHeight=\"210\"/>"
    "      <control weight=\"1\"/>"
    "    </hbox>"
    "    <label id=\"11\" text=\"\xe6\x89\xab\xe7\xa0\x81\xe7\x99\xbb\xe5\xbd\x95\""
    "           textColor=\"7,193,96\" fixedHeight=\"40\"/>"
    "    <control weight=\"1\"/>"
    "    <label id=\"12\" text=\"\xe4\xbb\x85\xe4\xbc\xa0\xe8\xbe\x93\xe6\x96\x87\xe4\xbb\xb6\""
    "           textColor=\"57,97,222\" fixedHeight=\"30\"/>"
    "  </vbox>"
    "</frame-window>";

static const char* kFallbackLoadingXmlUtf8 =
    "<frame-window title=\"XChat\""
    " min-w=\"360\" min-h=\"500\""
    " has-min-button=\"false\" has-max-button=\"false\" has-close-button=\"true\""
    " resizable=\"false\">"
    "  <vbox padding=\"0,60,0,60\" gap=\"24\">"
    "    <hbox fixedHeight=\"110\">"
    "      <control weight=\"1\"/>"
    "      <login-avatar fixedWidth=\"110\"/>"
    "      <control weight=\"1\"/>"
    "    </hbox>"
    "    <hbox fixedHeight=\"32\" gap=\"10\">"
    "      <control weight=\"1\"/>"
    "      <spinner fixedWidth=\"22\" fixedHeight=\"22\"/>"
    "      <label text=\"\xe6\xad\xa3\xe5\x9c\xa8\xe8\xbf\x9b\xe5\x85\xa5\""
    "             textColor=\"7,193,96\" fixedWidth=\"100\"/>"
    "      <control weight=\"1\"/>"
    "    </hbox>"
    "    <control weight=\"1\"/>"
    "  </vbox>"
    "</frame-window>";

// 读取 UTF-8 文件内容（最多 1MB，BOM 自动剥）。失败返空字符串。
static std::string ReadFileUtf8(LPCTSTR path)
{
    HANDLE h = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        return std::string();
    }
    DWORD sz = ::GetFileSize(h, nullptr);
    std::string out;
    if (sz > 0 && sz < (1 << 20))
    {
        out.resize(sz);
        DWORD got = 0;
        ::ReadFile(h, &out[0], sz, &got, nullptr);
        out.resize(got);
        if (out.size() >= 3
            && (unsigned char)out[0] == 0xEF
            && (unsigned char)out[1] == 0xBB
            && (unsigned char)out[2] == 0xBF)
        {
            out.erase(0, 3);
        }
    }
    ::CloseHandle(h);
    return out;
}

namespace {
// 按文件名解析 XML 路径；找不到时返回空。和 ResolveResPath 类似但找的
// 是 .xml 文件而非 res/ 子目录。
CString ResolveXmlPath(LPCTSTR fileName)
{
    TCHAR exePath[MAX_PATH] = {};
    ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    CString dir = exePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0)
    {
        dir = dir.Left(slash);
    }
    CString a = dir + _T("\\") + fileName;
    if (::GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES) { return a; }
    CString b = dir + _T("\\..\\XChat\\") + fileName;
    if (::GetFileAttributes(b) != INVALID_FILE_ATTRIBUTES) { return b; }
    CString c = dir + _T("\\..\\..\\XChat\\") + fileName;
    if (::GetFileAttributes(c) != INVALID_FILE_ATTRIBUTES) { return c; }
    return CString();
}
}

// =============================================================================
// LoginFrame
// =============================================================================
LoginFrame::LoginFrame(Mode m) : m_mode(m) {}
LoginFrame::~LoginFrame() = default;

LRESULT LoginFrame::OnCreate_(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    // 60Hz 动画 pulse —— spinner 走的 DuiAnimMgr 需要 host 周期性 TickAll。
    // 与 XChatMainFrame 用同一档（main.cpp 那边定义的 kAnimPulseTimerId
    // 不在 .h 里暴露，所以这里另起一个本地 id；只要不和业务 timer 撞就行）。
    SetTimer(0xA10, 16);
    bHandled = FALSE;
    return 0;
}

LRESULT LoginFrame::OnDestroy_(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    KillTimer(0xA10);
    KillTimer(kTimerCloseSelf);
    DuiAnimMgr::Inst().Clear();
    ::PostQuitMessage(0);   // 退出当前登录消息循环，main.cpp 接着起下一段
    bHandled = FALSE;
    return 0;
}

LRESULT LoginFrame::OnTimer_(UINT, WPARAM wp, LPARAM, BOOL& bHandled)
{
    if (wp == 0xA10)
    {
        DuiAnimMgr::Inst().TickAll(::GetTickCount());
    }
    else if (wp == kTimerCloseSelf)
    {
        KillTimer(kTimerCloseSelf);
        if (m_mode == Qr)
        {
            // QR 阶段结束 —— 切到 loading view，再起 3s close timer。
            SwitchMode(Loading);
            SetTimer(kTimerCloseSelf, kCloseSelfMs);
        }
        else
        {
            // Loading 阶段结束 —— 关掉本 frame；OnDestroy_ 里 PostQuit
            // 退出登录消息循环，main.cpp 接着起 main frame。
            ::PostMessage(m_hWnd, WM_CLOSE, 0, 0);
        }
    }
    bHandled = FALSE;
    return 0;
}

// 公共加载逻辑：按当前 m_mode 拉对应 XML，应用 frame 配置，塞进客户区。
static void LoadAndApplyForMode(LoginFrame::Mode mode, balloonwjui::DuiFrameWindow& wnd)
{
    LPCTSTR fileName = (mode == LoginFrame::Loading)
        ? _T("login_loading.xml")
        : _T("login.xml");
    const char* fallback = (mode == LoginFrame::Loading)
        ? kFallbackLoadingXmlUtf8
        : kFallbackQrXmlUtf8;

    std::string xml = ReadFileUtf8(ResolveXmlPath(fileName));
    if (xml.empty())
    {
        xml = fallback;
    }
    DuiFrameWindowConfig cfg;
    auto root = DuiXmlBuilder::FromFrameXml(xml.c_str(), cfg, MakeLoginXmlFactory());
    wnd.ApplyConfig(cfg);
    if (root)
    {
        wnd.SetClientContent(std::move(root));
    }

    // QR view 里两个状态文字 label 默认 DT_LEFT 看起来偏左，居中对齐才符
    // 合截图。XML BuildLabel 不暴露 textAlign 属性，所以只能 caller 加载
    // 完 XML 后用 FindCtrlById + SetTextAlign 设。loading view 的"正在
    // 进入" label 在 hbox+spinner 里整体居中，不需要单独改对齐。
    if (mode == LoginFrame::Qr)
    {
        DuiControl* content = wnd.GetClientContent();
        if (content)
        {
            const DWORD kCenter = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
            if (auto* status = dynamic_cast<DuiLabel*>(content->FindCtrlById(11)))
            {
                status->SetTextAlign(kCenter);
            }
            if (auto* link = dynamic_cast<DuiLabel*>(content->FindCtrlById(12)))
            {
                link->SetTextAlign(kCenter);
            }
        }
    }
}

void LoginFrame::ShowAndStartLoginSequence()
{
    LoadAndApplyForMode(m_mode, *this);
    SetTimer(kTimerCloseSelf, kCloseSelfMs);
}

void LoginFrame::SwitchMode(Mode m)
{
    m_mode = m;
    LoadAndApplyForMode(m_mode, *this);
}

} // namespace xchat
