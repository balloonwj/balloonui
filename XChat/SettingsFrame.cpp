#include "stdafx.h"
#include "SettingsFrame.h"

#include "DuiResMgr.h"
#include "DuiPaintAA.h"
#include "Controls/Basic/DuiAvatar.h"
#include "Controls/Basic/DuiLabel.h"
#include "Controls/Input/DuiSwitch.h"
#include "Controls/Layout/DuiLayout.h"

using namespace balloonwjui;

namespace xchat {

// 某信品牌绿（与 LoginFrame / MainFrame 保持一致）。
static const COLORREF kBrandGreenRgb = RGB(7, 193, 96);

// ---- 资源 / 文件读取 helper（拷自 MainFrame，重复一份避免互相引用） ----

namespace {

CString ResolveXmlPath(LPCTSTR fileName)
{
    TCHAR exePath[MAX_PATH] = {};
    ::GetModuleFileName(nullptr, exePath, MAX_PATH);
    CString dir = exePath;
    int slash = dir.ReverseFind(_T('\\'));
    if (slash > 0) { dir = dir.Left(slash); }
    CString a = dir + _T("\\") + fileName;
    if (::GetFileAttributes(a) != INVALID_FILE_ATTRIBUTES) { return a; }
    CString b = dir + _T("\\..\\XChat\\") + fileName;
    if (::GetFileAttributes(b) != INVALID_FILE_ATTRIBUTES) { return b; }
    CString c = dir + _T("\\..\\..\\XChat\\") + fileName;
    if (::GetFileAttributes(c) != INVALID_FILE_ATTRIBUTES) { return c; }
    return CString();
}

std::string ReadFileUtf8(LPCTSTR path)
{
    HANDLE h = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { return std::string(); }
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

CString Utf8ToCString(const std::string& utf8)
{
    if (utf8.empty()) { return CString(); }
    int cw = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (cw <= 0) { return CString(); }
    std::wstring w(cw, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], cw);
    while (!w.empty() && w.back() == 0) { w.pop_back(); }
    return CString(w.c_str());
}

// =========================================================================
// SettingsTab —— 左侧 tab 列表（6 项，单选）。
//
// 视觉：每行 44 px 高，左侧 16 px icon 占位 + 名字。选中行换浅灰底色。
// Phase 8 静态选中第 0 项；点击切换不实现（demo 只展示一个 tab 的内容）。
// =========================================================================
class SettingsTab : public DuiControl
{
public:
    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }

        // 6 个 tab 项（纯文字）。原本左侧有 unicode 字符做 icon 占位
        // (U+29BF / U+2699 / U+2191 / U+2699 / U+2708 / U+24D8) 但
        // Microsoft YaHei 没这些 glyph → 渲染成豆腐方块。等以后接上真
        // icon 资源（PNG 或 stroke glyph）再补回来；当前阶段干脆只显文字。
        static LPCTSTR kTabNames[] =
        {
            _T("账号与存储"),
            _T("通用"),
            _T("快捷键"),
            _T("通知"),
            _T("插件"),
            _T("关于"),
        };

        int rowH    = 44;
        int x       = m_rcItem.left;
        int y       = m_rcItem.top + 8;
        int w       = m_rcItem.right - m_rcItem.left;

        HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);

        for (size_t i = 0; i < sizeof(kTabNames) / sizeof(kTabNames[0]); ++i)
        {
            // 选中行底色（仅第 0 项 = 账号与存储）
            if ((int)i == m_selectedIdx)
            {
                RECT rcRow = { x, y, x + w, y + rowH };
                HBRUSH hbr = ::CreateSolidBrush(RGB(232, 232, 232));
                ::FillRect(hdc, &rcRow, hbr);
                ::DeleteObject(hbr);
            }

            // name（左 20px 起，给视觉留点呼吸感）
            COLORREF oldClr = ::SetTextColor(hdc, RGB(40, 40, 40));
            RECT rcName = { x + 20, y, x + w - 8, y + rowH };
            ::DrawText(hdc, kTabNames[i], -1, &rcName,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            ::SetTextColor(hdc, oldClr);
            y += rowH;
        }

        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);
    }

    void SetSelected(int idx) { m_selectedIdx = idx; Invalidate(); }

private:
    int m_selectedIdx = 0;
};

// =========================================================================
// SettingsCard —— 浅灰圆角白底卡，带 padding。容器；children 自己排列。
//
// 视觉同设置详情.png 里两块账号 / 存储分组卡：白底 + 1px 浅灰边框 + 8px
// 圆角。
// =========================================================================
class SettingsCard : public DuiVBox
{
public:
    void OnPaint(HDC hdc, const RECT& rcDirty) override
    {
        if (!m_bVisible) { return; }
        // 白底圆角 + 浅灰描边
        HBRUSH hbr = ::CreateSolidBrush(RGB(255, 255, 255));
        HRGN hrgn = ::CreateRoundRectRgn(m_rcItem.left, m_rcItem.top,
                                         m_rcItem.right + 1, m_rcItem.bottom + 1, 8, 8);
        ::FillRgn(hdc, hrgn, hbr);
        ::DeleteObject(hrgn);
        ::DeleteObject(hbr);

        HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(232, 232, 232));
        HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);
        HBRUSH hOldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
        ::RoundRect(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom, 8, 8);
        ::SelectObject(hdc, hOldBr);
        ::SelectObject(hdc, hOldPen);
        ::DeleteObject(hPen);

        // 让基类继续画 children
        DuiControl::OnPaint(hdc, rcDirty);
    }
};

// =========================================================================
// 简单浅灰按钮（"退出登录" / "管理" / "更改" / "清空全部聊天记录"）。
//
// 仅静态展示；不接 hover / 点击。
// =========================================================================
class SettingsButton : public DuiControl
{
public:
    void SetText(LPCTSTR t) { m_text = t ? t : _T(""); Invalidate(); }

    void OnPaint(HDC hdc, const RECT&) override
    {
        if (!m_bVisible) { return; }

        // 浅灰底 + 圆角 + 浅灰描边
        HBRUSH hbr = ::CreateSolidBrush(RGB(243, 243, 243));
        HRGN hrgn = ::CreateRoundRectRgn(m_rcItem.left, m_rcItem.top,
                                         m_rcItem.right + 1, m_rcItem.bottom + 1, 6, 6);
        ::FillRgn(hdc, hrgn, hbr);
        ::DeleteObject(hrgn);
        ::DeleteObject(hbr);

        HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(220, 220, 220));
        HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);
        HBRUSH hOldBr = (HBRUSH)::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
        ::RoundRect(hdc, m_rcItem.left, m_rcItem.top, m_rcItem.right, m_rcItem.bottom, 6, 6);
        ::SelectObject(hdc, hOldBr);
        ::SelectObject(hdc, hOldPen);
        ::DeleteObject(hPen);

        // 文字
        HFONT hOldFont = (HFONT)::SelectObject(hdc, DuiResMgr::Inst().GetDefaultFont());
        int oldBk = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF oldClr = ::SetTextColor(hdc, RGB(40, 40, 40));
        ::DrawText(hdc, m_text, -1, &m_rcItem,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SetTextColor(hdc, oldClr);
        ::SetBkMode(hdc, oldBk);
        ::SelectObject(hdc, hOldFont);
    }

private:
    CString m_text;
};

// =========================================================================
// CustomFactory —— settings.xml 用的自定义 tag。
// =========================================================================
DuiXmlBuilder::CustomFactory MakeSettingsXmlFactoryImpl()
{
    return [](const DuiXmlBuilder::Node& n) -> std::unique_ptr<DuiControl>
    {
        if (n.tag == "control")
        {
            return std::unique_ptr<DuiControl>(new DuiControl());
        }
        if (n.tag == "settings-bg")
        {
            // 整个右侧内容区灰底 panel（用 ColorPanel 思路：DuiVBox + 自绘
            // 底色）。这里直接用一个简化版本：派生 DuiVBox + OnPaint 涂色。
            class BgPanel : public DuiVBox
            {
            public:
                void OnPaint(HDC hdc, const RECT& d) override {
                    HBRUSH hbr = ::CreateSolidBrush(RGB(245, 245, 245));
                    ::FillRect(hdc, &m_rcItem, hbr);
                    ::DeleteObject(hbr);
                    DuiControl::OnPaint(hdc, d);
                }
            };
            return std::unique_ptr<DuiControl>(new BgPanel());
        }
        if (n.tag == "settings-tab")
        {
            return std::unique_ptr<DuiControl>(new SettingsTab());
        }
        if (n.tag == "settings-card")
        {
            return std::unique_ptr<DuiControl>(new SettingsCard());
        }
        if (n.tag == "settings-btn")
        {
            auto b = std::unique_ptr<SettingsButton>(new SettingsButton());
            auto it = n.attrs.find("text");
            if (it != n.attrs.end())
            {
                b->SetText(Utf8ToCString(it->second));
            }
            return b;
        }
        if (n.tag == "settings-switch")
        {
            // 单纯 DuiSwitch；on 属性控制初始 checked 状态。
            auto sw = std::unique_ptr<DuiSwitch>(new DuiSwitch());
            auto it = n.attrs.find("on");
            bool on = (it != n.attrs.end() && (it->second == "true" || it->second == "1"));
            sw->SetChecked(on, /*animated*/false, /*notify*/false);
            return sw;
        }
        if (n.tag == "settings-user-avatar")
        {
            auto av = std::unique_ptr<DuiAvatar>(new DuiAvatar());
            av->SetShape(DuiAvatar::ShapeRoundRect);
            av->SetCornerRadius(10);
            av->SetName(_T("Balloonwj"));
            av->SetFallbackBgColor(kBrandGreenRgb);
            return av;
        }
        // info-field 跟 MainFrame 同名 tag，但本 frame 用自己的 factory，
        // 所以这里再实现一份（短，复制成本低于把它抽到独立模块）。
        if (n.tag == "info-field")
        {
            auto vb = std::unique_ptr<DuiVBox>(new DuiVBox());
            auto labLab = std::unique_ptr<DuiLabel>(new DuiLabel());
            labLab->SetTextColor(RGB(140, 140, 140));
            auto it = n.attrs.find("label");
            if (it != n.attrs.end())
            {
                labLab->SetText(Utf8ToCString(it->second));
            }
            DuiLayout::Hint hLab; hLab.fixedMain = 18;
            vb->AddChild(std::move(labLab), hLab);

            auto valLab = std::unique_ptr<DuiLabel>(new DuiLabel());
            valLab->SetTextColor(RGB(40, 40, 40));
            it = n.attrs.find("value");
            if (it != n.attrs.end())
            {
                valLab->SetText(Utf8ToCString(it->second));
            }
            DuiLayout::Hint hVal; hVal.fixedMain = 22;
            vb->AddChild(std::move(valLab), hVal);
            return vb;
        }
        return nullptr;
    };
}

const char* kFallbackSettingsXmlUtf8 =
    "<frame-window title=\"\" min-w=\"800\" min-h=\"680\" resizable=\"true\""
    " has-min-button=\"false\" has-max-button=\"false\" has-close-button=\"true\">"
    "  <hbox padding=\"0\" gap=\"0\">"
    "    <settings-tab fixedWidth=\"140\"/>"
    "    <settings-bg weight=\"1\" padding=\"24\" gap=\"16\"/>"
    "  </hbox>"
    "</frame-window>";

} // anonymous

DuiXmlBuilder::CustomFactory MakeSettingsXmlFactory()
{
    return MakeSettingsXmlFactoryImpl();
}

// =============================================================================
// SettingsFrame
// =============================================================================
LRESULT SettingsFrame::OnCreate_(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    bHandled = FALSE;
    return 0;
}

LRESULT SettingsFrame::OnDestroy_(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    // 模态运行时只把 m_modalRunning 置 false 让 RunModal 的 while 自己
    // break 退出。绝<u>不能</u>调 PostQuitMessage(0) —— WM_QUIT 是 thread-
    // wide 的；从 main 弹出的 settings 关闭后，留在 queue 里的 WM_QUIT
    // 会被 main 的 RunMsgLoop 立刻消费，整个 demo 跟着退。
    //
    // 独立 --settings CLI 路径也不依赖 PostQuitMessage：那条路径调
    // RunModal(nullptr) 后直接 return 0，不需要 WM_QUIT 通知任何人。
    if (m_modalRunning)
    {
        m_modalRunning = false;
    }
    bHandled = FALSE;
    return 0;
}

void SettingsFrame::LoadSettingsXml()
{
    std::string xml = ReadFileUtf8(ResolveXmlPath(_T("settings.xml")));
    if (xml.empty())
    {
        xml = kFallbackSettingsXmlUtf8;
    }
    DuiFrameWindowConfig cfg;
    auto root = DuiXmlBuilder::FromFrameXml(xml.c_str(), cfg, MakeSettingsXmlFactory());
    ApplyConfig(cfg);
    if (root)
    {
        SetClientContent(std::move(root));
    }
}

void SettingsFrame::RunModal(HWND ownerHwnd)
{
    m_owner = ownerHwnd;
    if (m_owner)
    {
        ::EnableWindow(m_owner, FALSE);
    }
    m_modalRunning = true;

    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (!m_modalRunning) { break; }
    }

    if (m_owner)
    {
        ::EnableWindow(m_owner, TRUE);
        ::SetForegroundWindow(m_owner);
        m_owner = nullptr;
    }
}

} // namespace xchat
